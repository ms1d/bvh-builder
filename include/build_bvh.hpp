#pragma once



#include <atomic>
#include <sys/types.h>
#include <filesystem>



void build_bvh(const std::filesystem::path &file_path, std::atomic<uint> &curr_thread_count);
