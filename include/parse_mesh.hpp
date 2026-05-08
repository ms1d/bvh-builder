#pragma once



#include "vec.cuh"
#include <filesystem>



void parse_mesh(const std::filesystem::path &file_path,
		uint32_t *&tris, uint32_t &tris_len,
		vec<3> *&verts, uint32_t &verts_len,
		vec<3> &max, vec<3> &min);
