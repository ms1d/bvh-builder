#pragma once



#include <atomic>
#include <sys/types.h>
#include "structs.hpp"



// Defaults - overriden in main.cpp
inline uint worker_count = 32;
inline bool enable_concurrency = true;



void build_bvh(const char *buffer, char *output_buffer);

void build_bvh_node(bvh_node *node, vec<3> *verts, std::atomic<uint16_t> &nodes_len);

void output_bvh_node(bvh_node *curr_node, uint32_t *root_tris, char *output_buffer, uint16_t curr_bvh_pos);
