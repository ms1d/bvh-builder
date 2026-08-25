#include <chrono>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <iostream>
#include <tuple>
#include <unistd.h>
#include <immintrin.h>
#include "vec3.cuh"
#include "parse_mesh.hpp"
#include "structs.hpp"
#include "build_bvh.hpp"
#include "pools.hpp"
#include "codes.hpp"



// Max number of triangles per leaf INCLUSIVE
#define MAX_TRIS 50

// (MACRO) Checks that uin32_t x fits in y bits for 0 < y <= 32
#define ui32_FITS(x, y) if(x != (x << (32 - y)) >> (32 - y)) { return -ERR_BUILD_OVERFLOW; }



int build_bvh_node(bvh_node *node, vec<3> *verts, std::atomic<uint16_t> *nodes_len) {
	// If node has few tris, do not recurse; return to caller
	if (node->tris_len <= MAX_TRIS) { nodes_len->fetch_add(1, std::memory_order_relaxed); return 0; }

	// 1 - Split BVH by longest axis
	vec<3> offset(node->max - node->min);
	int longest_axis = 0;
	for (int i = 1; i < 3; i++) longest_axis = offset.data[i] > offset.data[longest_axis] ? i : longest_axis;
	offset.data[longest_axis] /= 2; offset.data[(longest_axis + 1) % 3] = 0; offset.data[(longest_axis + 2) % 3] = 0;

	// 2 - Create new children
	node->left = new bvh_node; node->right = new bvh_node;
	// Convention: left[i] > mid, right[i] <= mid
	node->left->max = node->max; node->left->min = node->min + offset;
	node->right->max = node->max-offset; node->right->min = node->min;

	node->left->left = node->left->right = node->right->left = node->right->right = nullptr;

	// Sort tris in place and produce pointers for children
	// Front is for left, right is for back
	vec<3, uint32_t> *front = node->tris, *back = node->tris + node->tris_len - 3;
	const float mid = offset.data[longest_axis] + node->min.data[longest_axis];
	
	while (front < back) {
		// Arbritary dimensions picked here
		bool lc = verts[(front->x)].data[longest_axis] > mid,
			 rc = verts[back->x].data[longest_axis] <= mid;

		if (lc) front += 1;
		else if (rc) back  -= 1;
		else {
			auto tmp = *front;
			*front = *back;
			*back = tmp;
		}
	}

	// front now points to the start of right's nodes
	node->right->tris = front; node->right->tris_len = static_cast<uint32_t>(node->tris_len - (front - node->tris));
	node->left->tris = node->tris; node->left->tris_len = node->tris_len - node->right->tris_len;

	// 3 - Recurse + await results

	// Due to stack re-use during recursion, it is not safe to stack allocate task
	auto task = reinterpret_cast<tp_task<wrapper>*>(memory_pool.alloc(sizeof(tp_task<wrapper>), alignof(tp_task<wrapper>)));
	auto args = reinterpret_cast<build_bvh_node_args*>(memory_pool.alloc(sizeof(build_bvh_node_args), alignof(build_bvh_node_args)));
	if (task == nullptr || args == nullptr) return -ERR_BUILD_BAD_ALLOC;
	*args = build_bvh_node_args{ node->left, verts, nodes_len };

	task->args = std::make_tuple(WRAPPER_TYPE_BUILD, static_cast<void*>(args));
	task->is_result_ready = false;

	auto res = worker_pool.try_submit(task);
	auto err = build_bvh_node(node->right, verts, nodes_len);
	if (!res && err < 0) return err;
	if (res) {
		while (!task->is_result_ready.load(std::memory_order_acquire)) {
			if (!worker_pool.try_claim()) task->is_result_ready.wait(false, std::memory_order_acquire);
		}

		if (err) return err;
		if (task->result < 0) return task->result;
	} else {
		err = build_bvh_node(node->left, verts, nodes_len);
		if (err < 0) return err;
	}

	// This node has children so set its tris to nullptr.
	// Ignore tris_len in this case
	node->tris = nullptr;
	nodes_len->fetch_add(1);
	return 0;
}


int output_bvh_node(bvh_node *curr_node, vec<3, uint32_t> *root_tris, char *bvh_output_buffer, std::atomic<uint32_t> *curr_bvh_pos, uint32_t curr_node_index) {
	if (curr_node == nullptr) return 0;

	bvh_node_serialised curr_node_out;

	// Convert to half float
	curr_node_out.max.x = _cvtss_sh(curr_node->max.x, 0);
	curr_node_out.max.y = _cvtss_sh(curr_node->max.y, 0);
	curr_node_out.max.z = _cvtss_sh(curr_node->max.z, 0);

	curr_node_out.min.x = _cvtss_sh(curr_node->min.x, 0);
	curr_node_out.min.y = _cvtss_sh(curr_node->min.y, 0);
	curr_node_out.min.z = _cvtss_sh(curr_node->min.z, 0);

	if (curr_node->tris != nullptr) { // LSB = is_leaf = 1
		auto index = static_cast<uint32_t>(curr_node->tris_len);
	    ui32_FITS(index, 31);	// TODO: replace with better error handling
		curr_node_out.payload = (index << 1) + 1;
	}
	else { // LSB = is_leaf = 0
		const auto curr_bvh_pos_old = curr_bvh_pos->fetch_add(2);
		uint32_t left_index = curr_bvh_pos_old + 1, right_index = curr_bvh_pos_old + 2;

		ui32_FITS(left_index, 16); // TODO: replace with better error handling
		ui32_FITS(right_index, 15); // TODO: replace with better error handling
		curr_node_out.payload = (left_index << 16) | (right_index << 1);

		// Due to stack re-use during recursion, it is not safe to stack allocate task
		auto task = reinterpret_cast<tp_task<wrapper>*>(memory_pool.alloc(sizeof(tp_task<wrapper>), alignof(tp_task<wrapper>)));
		auto args = reinterpret_cast<output_bvh_node_args*>(memory_pool.alloc(sizeof(output_bvh_node_args), alignof(output_bvh_node_args)));
		if (task == nullptr || args == nullptr) return -ERR_BUILD_BAD_ALLOC;
		*args = output_bvh_node_args{ curr_node->left, root_tris, bvh_output_buffer, curr_bvh_pos, left_index };
		task->args = std::make_tuple(WRAPPER_TYPE_OUTPUT, static_cast<void*>(args));
		task->is_result_ready = false;

		auto res = worker_pool.try_submit(task);
		
		auto err = output_bvh_node(curr_node->right, root_tris, bvh_output_buffer, curr_bvh_pos, right_index);
		if (!res && err < 0) return err;
		if (res) {
			while (!task->is_result_ready.load(std::memory_order_acquire)) {
				if (!worker_pool.try_claim()) task->is_result_ready.wait(false, std::memory_order_acquire);
			}

			if (err < 0) return err;
			if (task->result < 0) return task->result;
		}
		else {
			err = output_bvh_node(curr_node->left, root_tris, bvh_output_buffer, curr_bvh_pos, left_index);
			if (err < 0) return err;
		}
	}

	memcpy(bvh_output_buffer + (curr_node_index) * sizeof(bvh_node_serialised), &curr_node_out, sizeof(bvh_node_serialised));
	return 0;
}



void free_bvh_children(bvh_node *node) {
	if (node == nullptr) return;
	free_bvh_children(node->left);
	delete node->left;

	free_bvh_children(node->right);
	delete node->right;
}



void cleanup(bvh_node *root) {
	free_bvh_children(root);
	memory_pool.free();
}



int build_bvh(const char *buffer, char *output_buffer, const uint32_t size_in, uint32_t *size_out) {
#ifndef NDEBUG
	auto s = std::chrono::high_resolution_clock::now();
#endif

	vec<3, uint32_t> *tris;
	uint32_t verts_len, tris_len;
	vec<3> *verts, max, min;

#ifndef NDEBUG
	auto s_mesh = std::chrono::high_resolution_clock::now();
#endif

	auto success = parse_mesh(buffer, size_in, tris, tris_len, verts, verts_len, max, min);
	if (success < 0) { cleanup(nullptr); return success; }

#ifndef NDEBUG
	auto e_mesh = std::chrono::high_resolution_clock::now();
#endif

	bvh_node *root = reinterpret_cast<bvh_node*>(memory_pool.alloc(sizeof(bvh_node), alignof(bvh_node)));
	root->tris = tris; root->tris_len = tris_len; root->max = max; root->min = min;
	std::atomic<uint16_t> nodes_len = 0;

#ifndef NDEBUG
	auto s_bvh = std::chrono::high_resolution_clock::now();
#endif

	success = build_bvh_node(root, verts, &nodes_len);
	if (success < 0) { cleanup(root); return success; }

#ifndef NDEBUG
	auto e_bvh = std::chrono::high_resolution_clock::now();
#endif

	char *ptr = output_buffer;

	// Last use of ptr
	memcpy(ptr, &nodes_len, sizeof(nodes_len));

#ifndef NDEBUG
	auto s_out = std::chrono::high_resolution_clock::now();
#endif

	std::atomic<uint32_t> curr_bvh_pos = 0;
	success = output_bvh_node(root, tris, ptr + sizeof(nodes_len), &curr_bvh_pos, 0);
	if (success < 0) { cleanup(root); return success; }

#ifndef NDEBUG
	auto e_out = std::chrono::high_resolution_clock::now();
#endif

	*size_out = sizeof(nodes_len) + nodes_len * sizeof(bvh_node_serialised);

	cleanup(root);

#ifndef NDEBUG
	auto e = std::chrono::high_resolution_clock::now();
#endif

#ifndef NDEBUG
	std::cout << "\n========== BVH BUILD PROFILE ==========\n";

	std::cout << "Total time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count()
			  << "ms\n";

	std::cout << "Mesh parse: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(e_mesh - s_mesh).count()
			  << "ms\n";

	std::cout << "BVH build: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(e_bvh - s_bvh).count()
			  << "ms\n";

	std::cout << "BVH serialization: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(e_out - s_out).count()
			  << "ms\n";

	double seconds =
		std::chrono::duration<double>(e_bvh - s_bvh).count();

	double tris_per_second = (tris_len / 3.0) / seconds / 1e6;

	std::cout << "BVH Builder throughput: "
			  << tris_per_second << " Million tris/s\n";

	std::cout << "======================================\n";
#endif
	return 0;
}
