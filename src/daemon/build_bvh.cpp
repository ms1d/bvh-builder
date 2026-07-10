#include <chrono>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <iostream>
#include <unistd.h>
#include "thread_pool.hpp"
#include "vec3.cuh"
#include "parse_mesh.hpp"
#include "structs.hpp"
#include "build_bvh.hpp"
#include <immintrin.h>



// Max number of elements in tris per child INCLUSIVE
#define MAX_TRIS 150



thread_pool<build_bvh_node> build_pool{worker_count};
thread_pool<output_bvh_node> output_pool{worker_count};



void find_min_max_verts(vec<3> *verts, uint32_t *tris, uint32_t len, vec<3> &out_min, vec<3> &out_max) {
	out_min = verts[tris[0]]; out_max = out_min;

	for (uint32_t i = 1; i < len; i++) {
		auto curr = verts[tris[i]];
		out_min.x = std::min(curr.x, out_min.x); out_max.x = std::max(curr.x, out_max.x);
		out_min.y = std::min(curr.y, out_min.y); out_max.y = std::max(curr.y, out_max.y);
		out_min.z = std::min(curr.z, out_min.z); out_max.z = std::max(curr.z, out_max.z);
	}
}



void build_bvh_node(bvh_node *node, vec<3> *verts, std::atomic<uint16_t> &nodes_len) {

	// If node has few tris, do not recurse; return to caller
	if (node->tris_len <= MAX_TRIS) { nodes_len.fetch_add(1); return; }

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
	uint *front = node->tris, *back = node->tris + node->tris_len - 3;
	const float mid = offset.data[longest_axis] + node->min.data[longest_axis];
	
	while (front < back) {
		bool lc = verts[(*front)].data[longest_axis] > mid,
			 rc = verts[*(back)].data[longest_axis] <= mid;

		if (lc) front += 3;
		if (rc) back  -= 3;
		if (!(lc | rc)) {
			uint f0 = front[0], f1 = front[1], f2 = front[2];
			uint b0 = back[0],  b1 = back[1],  b2 = back[2];

			front[0]=b0; front[1]=b1; front[2]=b2;
			back[0]=f0;  back[1]=f1;  back[2]=f2;}
	}

	// front now points to the start of right's nodes
	node->right->tris = front; node->right->tris_len = static_cast<uint32_t>(node->tris_len - (front - node->tris));
	node->left->tris = node->tris; node->left->tris_len = node->tris_len - node->right->tris_len;

	// 3 - Recurse with 2 new threads, await results

	std::atomic<bool> is_ready = false;
	auto success = enable_concurrency && build_pool.try_emplace_task(&is_ready, node->left, verts, nodes_len);
	if (success) {
		build_bvh_node(node->right, verts, nodes_len);
		is_ready.wait(false);
    }
	else {
		build_bvh_node(node->left, verts, nodes_len);
		build_bvh_node(node->right, verts, nodes_len);
	}

	// This node has children so set its tris to nullptr.
	// Ignore tris_len in this case
	node->tris = nullptr;
	nodes_len.fetch_add(1);
}



void output_bvh_node(bvh_node *curr_node, uint32_t *root_tris, char *bvh_output_buffer, uint16_t curr_bvh_pos) {
	// TODO - assert that all these indices fit in the ranges. For now I assume they do
	if (curr_node == nullptr) return;

	bvh_node_serialised curr_node_out;

	curr_node_out.max.x = _cvtss_sh(curr_node->max.x, 0);
	curr_node_out.max.y = _cvtss_sh(curr_node->max.y, 0);
	curr_node_out.max.z = _cvtss_sh(curr_node->max.z, 0);
	curr_node_out.min.x = _cvtss_sh(curr_node->min.x, 0);
	curr_node_out.min.y = _cvtss_sh(curr_node->min.y, 0);
	curr_node_out.min.z = _cvtss_sh(curr_node->min.z, 0);

	if (curr_node->tris != nullptr) { // MSB = is_leaf = 1
		// Assuming that tris index fits in 31 bits (i.e. MSB = 1)
		curr_node_out.payload = static_cast<uint32_t>(curr_node->tris - root_tris);
		curr_node_out.payload |= (1u << 31);
	}
	else { // MSB = is_leaf = 0
		uint16_t left_index = 2 * curr_bvh_pos, right_index = left_index + 1;
		auto li = static_cast<uint32_t>(left_index);
		// Assuming that left_index fits in 15 bits (i.e. MSB = 0)
		curr_node_out.payload = (li << 16) | right_index;

		std::atomic<bool> is_ready = false;
		auto success = enable_concurrency && output_pool.try_emplace_task(&is_ready, curr_node->left, root_tris, bvh_output_buffer, left_index);
		if (success) {
			output_bvh_node(curr_node->right, root_tris, bvh_output_buffer, right_index);
			is_ready.wait(false);
		} else {
			output_bvh_node(curr_node->left, root_tris, bvh_output_buffer, left_index);
			output_bvh_node(curr_node->right, root_tris, bvh_output_buffer, right_index);
		}
	}

	memcpy(bvh_output_buffer + curr_bvh_pos * sizeof(bvh_node_serialised), &curr_node_out, sizeof(bvh_node_serialised));
}



void free_bvh_children(bvh_node *node) {
	if (node == nullptr) return;
	free_bvh_children(node->left);
	delete node->left;

	free_bvh_children(node->right);
	delete node->right;
}



void build_bvh(const char *buffer, char *output_buffer) {
	auto s = std::chrono::high_resolution_clock::now();

	uint32_t *tris, verts_len, tris_len;
	vec<3> *verts, max, min;

	auto s_mesh = std::chrono::high_resolution_clock::now();
	parse_mesh(buffer, tris, tris_len, verts, verts_len, max, min);
	auto e_mesh = std::chrono::high_resolution_clock::now();

	bvh_node root; root.tris = tris; root.tris_len = tris_len; root.max = max; root.min = min;
	std::atomic<uint16_t> nodes_len = 0;

	auto s_bvh = std::chrono::high_resolution_clock::now();
	build_bvh_node(&root, verts, nodes_len);
	auto e_bvh = std::chrono::high_resolution_clock::now();

	auto s_file = std::chrono::high_resolution_clock::now();

	auto t3 = std::chrono::high_resolution_clock::now();

	auto m_file = std::chrono::high_resolution_clock::now();

	// copy verts len, verts, tris len and tris in that order
	// HOTSPOT!	
	auto verts_space = verts_len * sizeof(vec<3>),
		   tris_space = tris_len * sizeof(uint32_t);

	char *ptr = output_buffer;
	memcpy(ptr, &verts_len, sizeof(verts_len)); ptr += sizeof(verts_len);
	memcpy(ptr, verts, verts_space); ptr += verts_space;
	auto t5 = std::chrono::high_resolution_clock::now();
	memcpy(ptr, &tris_len, sizeof(tris_len)); ptr += sizeof(tris_len);
	memcpy(ptr, tris, tris_space); ptr += tris_space;

	auto e_file = std::chrono::high_resolution_clock::now();

	// Last use of ptr
	memcpy(ptr += sizeof(nodes_len), &nodes_len, sizeof(nodes_len));
	auto s_out = std::chrono::high_resolution_clock::now();
	output_bvh_node(&root, tris, ptr + sizeof(nodes_len), 0);
	auto e_out = std::chrono::high_resolution_clock::now();


	auto s_file2 = std::chrono::high_resolution_clock::now();
	auto e_file2 = std::chrono::high_resolution_clock::now();

	delete[] verts;
	delete[] tris;

	free_bvh_children(&root);

	auto e = std::chrono::high_resolution_clock::now();

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

	std::cout << "File copy + remove: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - s_file).count()
			  << "ms\n";

	std::cout << "File open: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(m_file - t3).count()
			  << "ms\n";

	std::cout << "CPU staging (verts memcpy + tris_len prep): "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(t5 - m_file).count()
			  << "ms\n";

	std::cout << "First disk write (verts + tris): "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(e_file - t5).count()
			  << "ms\n";

	std::cout << "BVH serialization: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(e_out - s_out).count()
			  << "ms\n";

	std::cout << "Final disk write + close: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(e_file2 - s_file2).count()
			  << "ms\n";

	std::cout << "======================================\n";
}
