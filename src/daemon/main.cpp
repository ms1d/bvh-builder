#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <unistd.h>
#include "build_bvh.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>



uint sleep_period = 1;
#define BUFFER_SIZE 50'000'000



std::filesystem::path path = std::filesystem::current_path();



void parse_args(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		auto arg = argv[i];

		// number of workers
		if (std::strcmp(arg, "-w") == 0) {
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

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));

	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = '\0';
	strcpy(addr.sun_path + 1, "bvh_builderd");

	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("bind"); return 1;
	}

	if (listen(fd, 5) == -1) {
		perror("listen"); return 1;
	}

	char *buffer = new char[BUFFER_SIZE], *output_buffer = new char[BUFFER_SIZE];

	perror("bind/listen/accept");
	printf("server ready\n");
	while (1) {
		int client_fd = accept(fd, NULL, NULL);

		char size_buffer[4]; int i = 0;

		while (i < 4) {
			i += read(client_fd, size_buffer + i, 1);
		}

		size_t size; memcpy(&size, size_buffer, 4);

		i = 0;
		while (i < size-4) {
			i += read(client_fd, buffer + i, size-i-4);
		}

		std::cout << "starting..." << std::endl;
		build_bvh(buffer, output_buffer);

		close(client_fd);
	}

	delete[] buffer;
	delete[] output_buffer;

	close(fd);

	return 2;
}
