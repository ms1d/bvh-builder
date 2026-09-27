#pragma once

#include "thread_pool.hpp"
#include "bump_pool.hpp"
#include "structs.hpp"
#include "build_bvh.hpp"
#include "parse_mesh.hpp"
#include <memory>


#define WRAPPER_TYPE_BUILD 0
#define WRAPPER_TYPE_OUTPUT 1
#define WRAPPER_TYPE_FIND 2
#define WRAPPER_TYPE_FREE 3



struct build_bvh_node_args {
	bvh_node *node;
	vec<3> *verts;
	std::atomic<uint32_t> *nodes_len;
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

struct free_args {
	std::atomic<bool> *flag;
};

inline int wrapper(int type, void *data);

#ifndef NUM_THREADS
#define NUM_THREADS 16
#endif

#ifndef NUM_TASKS
#define NUM_TASKS 8'192
#endif

#ifndef TP_TYPE
#define TP_TYPE work_stealing
#endif

inline thread_pool<wrapper, NUM_THREADS, NUM_TASKS, pool_type::TP_TYPE> worker_pool{};

#ifndef POOL_SIZE
#define POOL_SIZE 20'000'000
#endif

#ifndef MP_TYPE
#define MP_TYPE bump_pool
#endif

#ifndef MP_SCOPE
#define MP_SCOPE 0 // 0 = local, 1 = global
#endif

#if MP_SCOPE == 0
thread_local inline MP_TYPE<char, POOL_SIZE, mp_type::thread_unsafe> memory_pool{};
#elif MP_SCOPE == 1
inline MP_TYPE<char, POOL_SIZE, mp_type::thread_safe> memory_pool{};
#else
static_assert(false, "INVALID MP_SCOPE - '0' (local) OR '1' (global) ONLY");
#endif

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
#if MP_SCOPE == 0
		case WRAPPER_TYPE_FREE: {
			auto args = static_cast<free_args*>(data);
			//args->flag->wait(false, std::memory_order_relaxed);
			while (!args->flag->load(std::memory_order_relaxed));
			memory_pool.free();
			return 0;
			break;
		}
#endif
		default:
			assert(false && "Invalid wrapper type");
	}
}
