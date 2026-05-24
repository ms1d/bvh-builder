#include <cstring>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <unistd.h>
#include "build_bvh.hpp"



uint sleep_period = 1;



std::filesystem::path path;



void parse_args(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		auto arg = argv[i];

		// path
		if (std::strcmp(arg, "-p") == 0) {
			try {
				path = std::filesystem::path(argv[++i]);
				std::filesystem::create_directories(path / "baked");
				std::filesystem::create_directories(path / "baking");
			} catch (const std::exception &e) {
				throw std::runtime_error("Error finding file at path! You probably got it wrong. Details:\n"+std::string(e.what()));
			}
		}

		// masters
		else if (std::strcmp(arg, "-m") == 0){ 
			try {
				masters_count = std::stoi(argv[++i]);
			} catch (const std::exception &e) {
				throw std::runtime_error("Number of masters must be a uint!");
			}
		}

		// workers per master
		else if (std::strcmp(arg, "-w") == 0) {
			try {
				workers_per_master_count = std::stoi(argv[++i]);
			} catch (const std::exception &e) {
				throw std::runtime_error("Number of workers per master must be a uint!");
			}
		}

		// sleep period
		else if (std::strcmp(arg, "-s") == 0) {
			try {
				sleep_period = std::stoi(argv[++i]); if (sleep_period == 0) sleep_period = 1;
			} catch (const std::exception &e) {
				throw std::runtime_error("Sleep period must be a uint!");
			}
		}

		// concurrency
		else if (std::strcmp(arg, "-c") == 0) {
			enable_concurrency = false;
		}

		else throw std::runtime_error("Unrecognized argument! " + std::string(arg));
	}
}



int main(int argc, char *argv[]) {
	try {
		parse_args(argc, argv);
	} catch (const std::exception &e) {
		throw std::runtime_error(e.what());
		return 1;
	}


	// Move all unbaked files back to be baked (may occur in e.g. crashes)
	// Assumes none of these files have been modified!
	auto path_str = path.c_str();
	const auto &cmd = std::format("mv {}/baking/* {}/", path_str, path_str);
	std::system(cmd.c_str());

	thread_pool<build_bvh, std::filesystem::path, master_resource&> master_pool(masters_count);
	master_resource *resources = new master_resource[masters_count];
	for (uint i = 0; i < masters_count; i++) {
		// Never freed since program should run indefinitely
		resources[i].build_pool = new thread_pool<build_bvh_node, bvh_node*, vec<3>*, std::atomic<uint16_t>&, master_resource&>(workers_per_master_count);
		resources[i].output_pool = new thread_pool<output_bvh_node, bvh_node*, uint32_t*, char*, uint16_t, master_resource&>(workers_per_master_count);
	}

	while (true) {
		for (const auto &entry : std::filesystem::directory_iterator(path)) {
			if (entry.is_regular_file() && entry.path().extension() == ".mesh") {
				int index = -1;
				for (uint i = 0; i < masters_count && index == -1; i++) {
					if (!resources[i].busy.load()) {
						resources[i].busy = true;
						index = i;
					}
				}

				const auto &dst = std::filesystem::path(path) / "baking" / entry.path().filename();
				std::filesystem::copy_file(entry.path(), dst);

				if (index != -1 && master_pool.try_emplace_task(nullptr, dst, resources[index])) std::filesystem::remove(entry.path());
				else std::filesystem::remove(dst);
			}
		}

		sleep(sleep_period);
	}

	return 1;
}
