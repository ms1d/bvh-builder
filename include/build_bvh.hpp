#pragma once



#include <atomic>
#include <sys/types.h>
#include <filesystem>
#include "structs.hpp"
#include "thread_pool.hpp"



void build_bvh(const std::filesystem::path &file_path, std::atomic<uint> &curr_thread_count, thread_pool &pool);

void build_bvh_node(bvh_node *node, vec<3> *verts, std::atomic<uint16_t> &nodes_len);
