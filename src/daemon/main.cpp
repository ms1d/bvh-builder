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



int main() {
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
