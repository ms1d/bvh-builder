#include <atomic>
#include <cstring>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include "build_bvh.hpp"
#include "thread_pool.hpp"



#define MAX_WORKERS 5
#define THREADS_PER_WORKER 32
#define SLEEP_PERIOD 5



int main(int argc, char *argv[]) {
	if (argc != 3) throw std::runtime_error("You can only pass in 1 arg (-p: path to dir)");
	if (strcmp(argv[1], "-p") != 0) throw std::runtime_error("Flag can only be -p");

	std::filesystem::path path;

	try {
		path = std::filesystem::path(argv[2]);
		std::filesystem::create_directories(path / "baked");
		std::filesystem::create_directories(path / "baking");
	} catch (const std::exception &e) {
		throw std::runtime_error("Error finding file at path! You probably got it wrong. Details:\n"+std::string(e.what()));
		return 1;
	}

	// Move all unbaked files back to be baked (may occur in e.g. crashes)
	// Assumes none of these files have been modified!
	auto path_str = path.c_str();
	const auto &cmd = std::format("mv {}/baking/* {}/", path_str, path_str);
	std::system(cmd.c_str());

	std::atomic<uint> curr_thread_count = 0;

	thread_pool<bvh_node*, vec<3>*, std::atomic<uint16_t>&> build_bvh_node_pool (MAX_WORKERS * THREADS_PER_WORKER, build_bvh_node);
	thread_pool<bvh_node*, uint32_t*, char*, std::atomic<uint16_t>&, uint16_t> output_bvh_node_pool(MAX_WORKERS * THREADS_PER_WORKER, output_bvh_node);

	while (true) {
		for (const auto &entry : std::filesystem::directory_iterator(path)) {
			if (entry.is_regular_file() && entry.path().extension() == ".mesh" && curr_thread_count.load() < MAX_WORKERS) {
				const auto &dst = std::filesystem::path(path) / "baking" / entry.path().filename();
				std::filesystem::copy_file(entry.path(), dst);
				std::filesystem::remove(entry.path());
				std::thread(build_bvh, std::filesystem::absolute(dst), std::ref(curr_thread_count), &build_bvh_node_pool, &output_bvh_node_pool).detach();
			}
		}

		sleep(SLEEP_PERIOD);
	}

	return 1;
}
