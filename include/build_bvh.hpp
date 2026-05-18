#pragma once



#include <atomic>
#include <sys/types.h>
#include <filesystem>
#include "structs.hpp"
#include "thread_pool.hpp"



// Defaults - overriden in main.cpp
inline uint masters_count = 5;
inline uint workers_per_master_count = 32;
inline bool enable_concurrency = true;



struct master_resource;



void build_bvh(const std::filesystem::path &file_path, master_resource &res);

void build_bvh_node(bvh_node *node, vec<3> *verts, std::atomic<uint16_t> &nodes_len, master_resource &res);

void output_bvh_node(bvh_node *curr_node, uint32_t *root_tris, char *output_buffer, std::atomic<uint16_t> &next_pos, uint16_t curr_bvh_pos, master_resource &res);



struct master_resource {
	thread_pool<build_bvh_node, bvh_node*, vec<3>*, std::atomic<uint16_t>&, master_resource&> *build_pool = nullptr;
	thread_pool<output_bvh_node, bvh_node*, uint32_t*, char*, std::atomic<uint16_t>&, uint16_t, master_resource&> *output_pool = nullptr;
	std::atomic<bool> busy = false;

	master_resource() {}
	master_resource(const master_resource& other) {
		build_pool = other.build_pool;
		output_pool = other.output_pool;
		busy = false;
	}
};
