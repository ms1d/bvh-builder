#pragma once



#include <atomic>
#include <sys/types.h>
#include <filesystem>
#include "structs.hpp"
#include "thread_pool.hpp"



#define MAX_WORKERS 5
#define THREADS_PER_WORKER 32



void build_bvh(const std::filesystem::path &file_path);

void build_bvh_node(bvh_node *node, vec<3> *verts, std::atomic<uint16_t> &nodes_len);

void output_bvh_node(bvh_node *curr_node, uint32_t *root_tris, char *output_buffer, std::atomic<uint16_t> &next_pos, uint16_t curr_bvh_pos);

inline thread_pool<bvh_node*, vec<3>*, std::atomic<uint16_t>&> build_bvh_node_pool(MAX_WORKERS * THREADS_PER_WORKER, build_bvh_node);
inline thread_pool<bvh_node*, uint32_t*, char*, std::atomic<uint16_t>&, uint16_t> output_bvh_node_pool(MAX_WORKERS * THREADS_PER_WORKER, output_bvh_node);
