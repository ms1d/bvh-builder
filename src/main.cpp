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

		// workers per master
		else if (std::strcmp(arg, "-w") == 0) {
			try {
				worker_count = static_cast<uint>(std::stoi(argv[++i]));
			} catch (const std::exception &e) {
				throw std::runtime_error("Number of workers per master must be a uint!");
			}
		}

		// sleep period
		else if (std::strcmp(arg, "-s") == 0) {
			try {
				auto tmp = std::stoi(argv[++i]);
				if (tmp <= 0) tmp = 1;
				sleep_period = static_cast<uint>(tmp);
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

	while (true) {
		for (const auto &entry : std::filesystem::directory_iterator(path)) {
			if (entry.is_regular_file() && entry.path().extension() == ".mesh") {
				const auto &dst = std::filesystem::path(path) / "baking" / entry.path().filename();
				std::filesystem::copy_file(entry.path(), dst);
				build_bvh(dst);
				std::filesystem::remove(entry.path());
			}
		}

		sleep(sleep_period);
	}

	return 1;
}
