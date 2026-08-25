#pragma once

#include "thread_pool.hpp"
#include "bump_pool.hpp"
#include "structs.hpp"
#include "build_bvh.hpp"
#include "parse_mesh.hpp"


#define WRAPPER_TYPE_BUILD 0
#define WRAPPER_TYPE_OUTPUT 1
#define WRAPPER_TYPE_FIND 2



struct build_bvh_node_args {
	bvh_node *node;
	vec<3> *verts;
	std::atomic<uint16_t> *nodes_len;
};

struct output_bvh_node_args {
	bvh_node *curr_node;
	vec<3, uint32_t> *root_tris;
	char *output_buffer;
	std::atomic<uint32_t> *curr_bvh_pos;
	uint32_t curr_node_index;
};

// The function of `parse_mesh.cpp`, not "build_bvh.cpp"
struct find_min_max_verts_args {
	vec<3> *verts;
	uint32_t len;
	vec<3> *max_out;
	vec<3> *min_out;
};

struct output_bvh_node_args;
inline int wrapper(int type, void *data) {
	switch (type) {
		case WRAPPER_TYPE_BUILD: {
			auto args = static_cast<build_bvh_node_args*>(data);
			return build_bvh_node(args->node, args->verts, args->nodes_len);
			break;
		}
		case WRAPPER_TYPE_OUTPUT: {
			auto args = static_cast<output_bvh_node_args*>(data);
			return output_bvh_node(args->curr_node, args->root_tris, args->output_buffer, args->curr_bvh_pos, args->curr_node_index);
			break;
		}
		case WRAPPER_TYPE_FIND: {
			auto args = static_cast<find_min_max_verts_args*>(data);
			find_min_max_verts(args->verts, args->len, args->max_out, args->min_out);
			return 0;
			break;
		}
		default:
			assert(false && "Invalid wrapper type");
	}
}



#define NUM_THREADS 4
#define NUM_TASKS 16
inline thread_pool<wrapper, NUM_THREADS, NUM_TASKS, pool_type::vyukov_buffer_spin> worker_pool{};

#define POOL_SIZE 100'000'000
inline bump_pool<char, POOL_SIZE, mp_type::thread_safe> memory_pool{};
