#pragma once



#include <atomic>
#include <sys/types.h>
#include "structs.hpp"



int build_bvh(const char *input_buffer, const uint32_t size_in, int out_file_fd);

int build_bvh_node(bvh_node *node, vec<3> *verts, std::atomic<uint32_t> *nodes_len);

int output_bvh_node(bvh_node *curr_node, vec<3, uint32_t> *root_tris, char *output_buffer, std::atomic<uint32_t> *curr_bvh_pos, uint32_t curr_node_index);
