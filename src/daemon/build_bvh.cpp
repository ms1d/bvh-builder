#include <chrono>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <iostream>
#include <sys/mman.h>
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
#define MAX_TRIS   50
#define SMALL_TRIS 1

// (MACRO) Checks that uint32_t x fits in y bits for 0 < y <= 32
#define ui32_FITS(x, y) if(x != (x << (32 - y)) >> (32 - y)) { return -ERR_BUILD_OVERFLOW; }

int end_bvh_recursion(bvh_node *node, std::atomic<uint32_t> *nodes_len) {
	node->tris = nullptr;
	nodes_len->fetch_add(1 + CHILDREN_PER_NODE, std::memory_order_relaxed);

	// Merge bounds
	for (uint32_t i = 0; i < CHILDREN_PER_NODE; i++) {
		node->max.x = std::max(node->max.x, node->children[i].max.x);
		node->max.y = std::max(node->max.y, node->children[i].max.y);
		node->max.z = std::max(node->max.z, node->children[i].max.z);

		node->min.x = std::min(node->min.x, node->children[i].min.x);
		node->min.y = std::min(node->min.y, node->children[i].min.y);
		node->min.z = std::min(node->min.z, node->children[i].min.z);
	}

	return 0;
}

int build_bvh_node(bvh_node *node, vec<3> *verts, std::atomic<uint32_t> *nodes_len) {
	// Base case
	if (node->tris_len <= MAX_TRIS) {
		nodes_len->fetch_add(1, std::memory_order_relaxed);
		for (uint32_t i = 0; i < node->tris_len; i++) {
			auto &tri = node->tris[i];
			for (int j = 0; j < 3; j++) {
				auto &vert = verts[tri.data[j]];

				node->max.x = std::max(node->max.x, vert.x);
				node->max.y = std::max(node->max.y, vert.y);
				node->max.z = std::max(node->max.z, vert.z);

				node->min.x = std::min(node->min.x, vert.x);
				node->min.y = std::min(node->min.y, vert.y);
				node->min.z = std::min(node->min.z, vert.z);
			}
		}
		return 0;
	}

	// 1 - Split BVH by longest axis
	vec<3> offset = vec<3>(node->max - node->min);
	int longest_axis = 0;
	for (int i = 1; i < 3; i++) longest_axis = offset.data[i] > offset.data[longest_axis] ? i : longest_axis;
	offset.data[longest_axis] /= CHILDREN_PER_NODE; offset.data[(longest_axis + 1) % 3] = 0; offset.data[(longest_axis + 2) % 3] = 0;

	// 2 - Create new children
#if MP_SCOPE != MP_SCOPE_MALLOC
	node->children = reinterpret_cast<bvh_node*>(memory_pool.alloc(CHILDREN_PER_NODE * sizeof(bvh_node), alignof(bvh_node)));
	if (node->children == nullptr) return -ERR_BUILD_BAD_ALLOC;
#else
	node->children = new bvh_node[CHILDREN_PER_NODE];
#endif
	vec<3, uint32_t> *front = node->tris, *back = node->tris + node->tris_len - 1;

	for (uint32_t i = 0; i < CHILDREN_PER_NODE; i++) {
		auto &child = node->children[i];
		child.min = node->min + offset * i;
		child.max = node->max - offset * (CHILDREN_PER_NODE - i - 1);

		if (i == CHILDREN_PER_NODE - 1) {
			child.tris = front;
			child.tris_len = static_cast<uint32_t>(back - front + 1);
			break;
		}

		const float boundary = child.max.data[longest_axis];

		// TODO - experiment with allocating more memory to reduce compute!
		while (front < back) {
			// Arbritary dimensions picked here
			bool lc = verts[front->x].data[longest_axis] <= boundary,
				 rc = verts[back->x].data[longest_axis] > boundary;

			if (lc) front++;
			if (rc) back--;
			else if (!lc) std::swap(*front, *back);
		}

		auto old_front = i > 0 ? node->children[i - 1].tris + node->children[i - 1].tris_len : node->tris;
		child.tris = old_front;
		child.tris_len = static_cast<uint32_t>(front - old_front);
		back = node->tris + node->tris_len - 1;
	}

	int empty_count = 0;
	for (uint32_t i = 0; i < CHILDREN_PER_NODE; i++)
		if (node->children[i].tris_len < SMALL_TRIS) empty_count++;

	// Most tris went into 1 child - no point in trying to keep going
	// Take the L and move on
	if (empty_count == CHILDREN_PER_NODE - 1) return end_bvh_recursion(node, nodes_len);

	// 3 - Recurse + await results

	// Due to stack re-use during recursion, it is not safe to stack allocate task
#if MP_SCOPE != MP_SCOPE_MALLOC
	auto tasks = reinterpret_cast<tp_task<wrapper>*>(memory_pool.alloc((CHILDREN_PER_NODE-1) * sizeof(tp_task<wrapper>), alignof(tp_task<wrapper>)));
	auto args = reinterpret_cast<build_bvh_node_args*>(memory_pool.alloc((CHILDREN_PER_NODE-1) * sizeof(build_bvh_node_args), alignof(build_bvh_node_args)));
#else
	auto tasks = new tp_task<wrapper>[CHILDREN_PER_NODE-1];
	auto args = new build_bvh_node_args[CHILDREN_PER_NODE-1];
#endif

	bool results[CHILDREN_PER_NODE-1];
	int errs[CHILDREN_PER_NODE];

	if (tasks == nullptr || args == nullptr) return -ERR_BUILD_BAD_ALLOC;
	for (uint32_t i = 0; i < CHILDREN_PER_NODE-1; i++) {
		args   [i] = build_bvh_node_args{ node->children + i, verts, nodes_len };
		tasks  [i].args = std::make_tuple(WRAPPER_TYPE_BUILD, static_cast<void*>(args+i));
#if MP_SCOPE != MP_SCOPE_MALLOC
		tasks  [i].is_result_ready = false;
		tasks  [i].result = 0;
		tasks  [i].reset_flag();
#endif
		results[i] = worker_pool.try_submit(tasks + i);
	}

	errs[CHILDREN_PER_NODE-1] = build_bvh_node(node->children + CHILDREN_PER_NODE-1, verts, nodes_len);

	
	for (uint32_t i = 0; i < CHILDREN_PER_NODE-1; i++) {
		if (results[i]) {
			while (!tasks[i].is_result_ready.load(std::memory_order_acquire))
				if (!worker_pool.try_claim())
					tasks[i].is_result_ready.wait(false, std::memory_order_acquire);

			errs[i] = tasks[i].result;
		} else {
			errs[i] = build_bvh_node(node->children + i, verts, nodes_len);
		}
	}

#if MP_SCOPE == MP_SCOPE_MALLOC
	delete[] args;
	delete[] tasks;
#endif

	// ISSUE - if errors differ, the first one will be returned.
	for (int i = 0; i < CHILDREN_PER_NODE; i++) if (errs[i] < 0) return errs[i];

	// This node has children so set its tris to nullptr.
	// Ignore tris_len in this case
	node->tris = nullptr;
	nodes_len->fetch_add(1, std::memory_order_relaxed);

	// Merge bounds
	for (uint32_t i = 0; i < CHILDREN_PER_NODE; i++) {
		node->max.x = std::max(node->max.x, node->children[i].max.x);
		node->max.y = std::max(node->max.y, node->children[i].max.y);
		node->max.z = std::max(node->max.z, node->children[i].max.z);

		node->min.x = std::min(node->min.x, node->children[i].min.x);
		node->min.y = std::min(node->min.y, node->children[i].min.y);
		node->min.z = std::min(node->min.z, node->children[i].min.z);
	}

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
	    ui32_FITS(index, 31);
		curr_node_out.payload = (index << 1) + 1;
	}
	else { // LSB = is_leaf = 0
		const auto curr_bvh_pos_old = curr_bvh_pos->fetch_add(CHILDREN_PER_NODE, std::memory_order_relaxed);
		uint32_t first_index = curr_bvh_pos_old + 1;

		ui32_FITS((first_index + CHILDREN_PER_NODE - 1), 31);
		curr_node_out.payload = first_index << 1;

		// Due to stack re-use during recursion, it is not safe to stack allocate tasks
#if MP_SCOPE != MP_SCOPE_MALLOC
		auto tasks = reinterpret_cast<tp_task<wrapper>*>(memory_pool.alloc((CHILDREN_PER_NODE-1) * sizeof(tp_task<wrapper>), alignof(tp_task<wrapper>)));
		auto args = reinterpret_cast<output_bvh_node_args*>(memory_pool.alloc((CHILDREN_PER_NODE-1) * sizeof(output_bvh_node_args), alignof(output_bvh_node_args)));
#else
		auto tasks = new tp_task<wrapper>[CHILDREN_PER_NODE-1];
		auto args = new output_bvh_node_args[CHILDREN_PER_NODE-1];
#endif

		bool results[CHILDREN_PER_NODE-1];
		int errs[CHILDREN_PER_NODE];

		if (tasks == nullptr) return -ERR_BUILD_BAD_ALLOC;
		if (args == nullptr) return -ERR_BUILD_BAD_ALLOC;
		for (uint32_t i = 0; i < CHILDREN_PER_NODE-1; i++) {
			args   [i] = output_bvh_node_args{ curr_node->children + i, root_tris, bvh_output_buffer, curr_bvh_pos, first_index + i };
			tasks  [i].args = std::make_tuple(WRAPPER_TYPE_OUTPUT, static_cast<void*>(args+i));
#if MP_SCOPE != MP_SCOPE_MALLOC // new keyword will construct these values for us
			tasks  [i].is_result_ready = false;
			tasks  [i].result = 0;
			tasks  [i].reset_flag();
#endif
			results[i] = worker_pool.try_submit(tasks + i);
		}

		errs[CHILDREN_PER_NODE-1] = output_bvh_node(
			curr_node->children + CHILDREN_PER_NODE-1,
			root_tris,
			bvh_output_buffer,
			curr_bvh_pos,
			first_index + CHILDREN_PER_NODE-1
		);
	
		for (int i = 0; i < CHILDREN_PER_NODE-1; i++) {
			if (results[i]) {
				while (!tasks[i].is_result_ready.load(std::memory_order_acquire))
					if (!worker_pool.try_claim())
						tasks[i].is_result_ready.wait(false, std::memory_order_acquire);

				errs[i] = tasks[i].result;
			} else {
				errs[i] = output_bvh_node(curr_node->children + i, root_tris, bvh_output_buffer, curr_bvh_pos, first_index + i);
			}
		}

#if MP_SCOPE == MP_SCOPE_MALLOC
		delete[] tasks;
		delete[] args;
#endif

		// ISSUE - if errors differ, the first one will be returned.
		for (int i = 0; i < CHILDREN_PER_NODE; i++) if (errs[i] < 0) return errs[i];
	}

	memcpy(bvh_output_buffer + (curr_node_index) * sizeof(bvh_node_serialised), &curr_node_out, sizeof(bvh_node_serialised));
	return 0;
}



#if MP_SCOPE != MP_SCOPE_MALLOC
void free_memory() {
#else
void free_memory(bvh_node *node = nullptr, vec<3> *verts = nullptr, vec<3, uint32_t> *tris = nullptr) {
#endif
#if MP_SCOPE == MP_SCOPE_LOCAL
	tp_task<wrapper> tasks[NUM_THREADS];
	for (uint32_t i = 0; i < NUM_THREADS; i++) {
		tasks[i].args = std::make_tuple(WRAPPER_TYPE_FREE, nullptr);
		tasks[i].is_result_ready = false;
		tasks[i].reset_flag();
	}

	for (uint32_t i = 0; i < NUM_THREADS; i++) {
		worker_pool.submit(tasks + i);
	}

	memory_pool.free();

	for (uint32_t i = 0; i < NUM_THREADS; i++) {
		tasks[i].is_result_ready.wait(false, std::memory_order_relaxed);
	}
#elif MP_SCOPE == MP_SCOPE_MALLOC
	if (verts) delete[] verts;
	if (tris) delete[] tris;

	if (!node->children) return;

	for (uint32_t i = 0; i < CHILDREN_PER_NODE; i++) {
		free_memory(node->children + i);
	}

	delete[] node->children;
#endif
}



// Important - do NOT close out_file_fd here - the caller is responsible for it
int build_bvh(const char *input_buffer, const uint32_t size_in, int out_file_fd) {
#ifndef NDEBUG
	auto s = std::chrono::high_resolution_clock::now();
#endif


	vec<3, uint32_t> *tris;
	uint32_t verts_len, tris_len;
	vec<3> *verts, max, min;


#ifndef NDEBUG
	auto s_mesh = std::chrono::high_resolution_clock::now();
#endif

	auto success = parse_mesh(input_buffer, size_in, tris, tris_len, verts, verts_len, max, min);
	// if new/delete is used, parse_mesh will free its own memory on failure
	if (success < 0) { free_memory(); return success; }
#ifndef NDEBUG
	std::cout << "Parse mesh succeeded" << std::endl;
	auto e_mesh = std::chrono::high_resolution_clock::now();
#endif


#if MP_SCOPE != MP_SCOPE_MALLOC
	bvh_node *root = reinterpret_cast<bvh_node*>(memory_pool.alloc(sizeof(bvh_node), alignof(bvh_node)));
	if (root == nullptr) return -ERR_BUILD_BAD_ALLOC;
#else
	bvh_node *root = new bvh_node{};
#endif
	root->tris = tris; root->tris_len = tris_len; root->max = max; root->min = min;
	std::atomic<uint32_t> nodes_len = 0;


#ifndef NDEBUG
	auto s_bvh = std::chrono::high_resolution_clock::now();
#endif


	success = build_bvh_node(root, verts, &nodes_len);
	if (success < 0) {
#if MP_SCOPE == MP_SCOPE_MALLOC
		free_memory(root, verts, tris); delete root; 
#else
		free_memory();
#endif
		return success;
	}


#ifndef NDEBUG
	std::cout << "Build bvh succeeded" << std::endl;
	auto e_bvh = std::chrono::high_resolution_clock::now();
#endif



	uint32_t size_out = tris_len * sizeof(vec<3, uint32_t>) + sizeof(nodes_len) + nodes_len * sizeof(bvh_node_serialised);

	char *output_buffer = static_cast<char*>(mmap(0, size_out, PROT_WRITE, MAP_PRIVATE, out_file_fd, 0));
	if (output_buffer == MAP_FAILED) return -ERR_OUT_MAP;
	
	if (ftruncate(out_file_fd, size_out) < 0) { return -ERR_OUT_MAP; }

	memcpy(output_buffer, tris, tris_len * sizeof(vec<3, uint32_t>));
	memcpy(output_buffer + tris_len * sizeof(vec<3, uint32_t>), &nodes_len, sizeof(nodes_len));


#ifndef NDEBUG
	auto s_out = std::chrono::high_resolution_clock::now();
#endif


	std::atomic<uint32_t> curr_bvh_pos = 0;
	success = output_bvh_node(root, tris, output_buffer + tris_len * sizeof(vec<3, uint32_t>) + sizeof(nodes_len), &curr_bvh_pos, 0);
	if (success < 0) {
		munmap(output_buffer, size_out);
#if MP_SCOPE == MP_SCOPE_MALLOC
		free_memory(root, verts, tris); delete root;
#else
		free_memory();
#endif
		return success;
	}


#ifndef NDEBUG
	std::cout << "Output bvh succeeded" << std::endl;
	auto e_out = std::chrono::high_resolution_clock::now();
#endif

#if MP_SCOPE == MP_SCOPE_MALLOC
	free_memory(root, verts, tris);
	delete root;
#else
	free_memory();
#endif
	munmap(output_buffer, size_out);

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

	double tris_per_second = tris_len / seconds / 1e6;

	std::cout << "Number of tris: "
              << tris_len << "\n";

	std::cout << "BVH Builder throughput: "
			  << tris_per_second << " Million tris/s\n";

	std::cout << "======================================\n";
#endif
	return 0;
}
