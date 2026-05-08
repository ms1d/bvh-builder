#include "vec.cuh"
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>



void parse_mesh(const std::filesystem::path &file_path,
		uint32_t *&tris, uint32_t &tris_len,
		vec<3> *&verts, uint32_t &verts_len,
		vec<3> &max, vec<3> &min) {

	std::ifstream mesh(file_path, std::ios::binary);
	if (!mesh || !mesh.is_open()) {
		auto msg = std::format("Mesh not found at {}", file_path.c_str());
		throw std::runtime_error(msg);
	}
	
	mesh.seekg(0, std::ios::end);
	size_t len = mesh.tellg();
	mesh.seekg(0, std::ios::beg);

	char *base = new char[len];
	mesh.read(base, len);
	char *ptr = base;

	memcpy(&verts_len, ptr, 4);

	verts = new vec<3>[verts_len];

	ptr += 4;
	for (uint32_t i = 0; i < verts_len; i++) {
		float x,y,z;

		memcpy(&x, ptr, 4);
        memcpy(&y, ptr+4, 4);
        memcpy(&z, ptr+8, 4);

		auto curr_vec = vec<3>(x,y,z);
		verts[i] = curr_vec;

		max.x = std::max(max.x, curr_vec.x);
		max.y = std::max(max.y, curr_vec.y);
		max.z = std::max(max.z, curr_vec.z);
		
		min.x = std::min(min.x, curr_vec.x);
		min.y = std::min(min.y, curr_vec.y);
		min.z = std::min(min.z, curr_vec.z);


		// stride length = 3 4-byte floats = 12 bytes
		ptr += 12;
	}

	memcpy(&tris_len, ptr, 4);

	ptr += 4;

	// tris_len = number of elements in tris. each triangle is 3 ints
	tris = new uint32_t[tris_len];

	for (uint32_t i = 0; i < tris_len; i++) {
		memcpy(&tris[i], ptr, 4);

		// stride length = 3 4-byte uints = 12 bytes
		ptr += 4;
	}

	// base is a temporary: tris and verts are owned by caller
	delete[] base;
}
