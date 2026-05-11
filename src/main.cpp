#include <cstring>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <unistd.h>
#include "build_bvh.hpp"



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

	thread_pool<std::filesystem::path, master_resource&> master_pool(MAX_WORKERS, build_bvh);
	master_resource resources[MAX_WORKERS];
	for (int i = 0; i < MAX_WORKERS; i++) {
		// Never freed since program should run indefinitely
		resources[i].build_pool = new thread_pool<bvh_node*, vec<3>*, std::atomic<uint16_t>&, master_resource&>(THREADS_PER_WORKER, build_bvh_node);
		resources[i].output_pool = new thread_pool<bvh_node*, uint32_t*, char*, std::atomic<uint16_t>&, uint16_t, master_resource&>(THREADS_PER_WORKER, output_bvh_node);
	}

	while (true) {
		for (const auto &entry : std::filesystem::directory_iterator(path)) {
			if (entry.is_regular_file() && entry.path().extension() == ".mesh") {
				int index = -1;
				for (int i = 0; i < MAX_WORKERS && index == -1; i++) {
					if (!resources[i].busy.load()) {
						resources[i].busy = true;
						index = i;
					}
				}

				const auto &dst = std::filesystem::path(path) / "baking" / entry.path().filename();
				std::filesystem::copy_file(entry.path(), dst);

				if (index != -1 && master_pool.try_emplace_task(dst, resources[index])) std::filesystem::remove(entry.path());
				else std::filesystem::remove(dst);
			}
		}

		sleep(SLEEP_PERIOD);
	}

	return 1;
}
