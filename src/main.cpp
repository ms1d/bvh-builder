#include <atomic>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include "vec.cuh"



constexpr uint max_thread_count = 5;
constexpr uint sleep_period = 5;



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

	// tris_len = number of triangles. each triangle is 3 ints
	tris = new uint32_t[tris_len * 3];

	for (uint32_t i = 0; i < tris_len*3; i++) {
		memcpy(&tris[i], ptr, 4);

		// stride length = 3 4-byte uints = 12 bytes
		ptr += 4;
	}

	// base is a temporary: tris and verts are owned by caller
	delete[] base;
}



void build_bvh(const std::filesystem::path &file_path, std::atomic<uint> &curr_thread_count) {
	curr_thread_count++;



	uint32_t *tris, verts_len, tris_len;
	vec<3> *verts, max, min;

	parse_mesh(file_path, tris, tris_len, verts, verts_len, max, min);
	
	// TODO: work with the data to create bvh data & save
	
	delete[] verts;
	delete[] tris;



	sleep(10);

	const auto &dst = file_path.parent_path().parent_path() / "baked" / file_path.filename();

	std::filesystem::copy(file_path, dst);
	std::filesystem::remove(file_path);
	curr_thread_count--;
}



int main(int argc, char *argv[]) {
	if (argc != 3) throw std::runtime_error("You can only pass in 1 arg (-p: path to dir)");
	if (strcmp(argv[1], "-p") != 0) throw std::runtime_error("Flag can only be -p");

	std::filesystem::path path(argv[2]);
	std::filesystem::create_directories(path / "baked");
	std::filesystem::create_directories(path / "baking");

	// In the event of a crash, move all unbaked files back to be baked
	// Assumes none have been modified
	auto path_str = path.c_str();
	const auto &cmd = std::format("mv {}/baking/* {}/", path_str, path_str);
	std::system(cmd.c_str());

	std::atomic<uint> curr_thread_count = 0;

	while (true) {
		for (const auto &entry : std::filesystem::directory_iterator(path)) {
			if (entry.is_regular_file() && entry.path().extension() == ".mesh" && curr_thread_count.load() < max_thread_count) {
				const auto &dst = std::filesystem::path(path) / "baking" / entry.path().filename();
				std::filesystem::copy_file(entry.path(), dst);
				std::filesystem::remove(entry.path());
				std::thread(build_bvh, std::filesystem::absolute(dst), std::ref(curr_thread_count)).detach();
			}
		}

		sleep(sleep_period);
	}

	return 1;
}
