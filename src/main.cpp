#include <atomic>
#include <cstring>
#include <filesystem>
#include <format>
#include <thread>
#include <unistd.h>



constexpr uint max_thread_count = 5;
constexpr uint sleep_period = 5;



void build_bvh(const std::filesystem::path &file_path, std::atomic<uint> &curr_thread_count) {
	curr_thread_count++;
	// work. IMPORTANT - do NOT modify original file
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
