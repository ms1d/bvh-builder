#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <unistd.h>
#include "build_bvh.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>



#define BUFFER_IN_SIZE 100'000'000
#define BUFFER_OUT_SIZE 100'000'000



std::filesystem::path path = std::filesystem::current_path();



int main(void) {
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

	char *buffer = new char[BUFFER_IN_SIZE], *output_buffer = new char[BUFFER_OUT_SIZE];

	printf("server ready\n");
	while (1) {
		int client_fd = accept(fd, NULL, NULL);

		char size_buffer[4]; int read_bytes = 0;

		while (read_bytes < 4) {
			auto tmp = read(client_fd, size_buffer + read_bytes, 4 - read_bytes);
			if (tmp <= 0) { close(client_fd); continue; }
			read_bytes += tmp;
		}

		uint32_t size_in, size_out; memcpy(&size_in, size_buffer, 4);

		// Need at least:
		// - 1 verts_len (1 uint32_t = 4 bytes)
		// - 1 vertex (1 vec<3> = 4 * 3 = 12 bytes)
		// - 1 tris_len (1 uint32_t = 4 bytes)
		// - 1 triangle (3 uint32_t = 12 bytes)
		if (size_in > BUFFER_IN_SIZE || size_in < 32) { close(client_fd); continue; };

		read_bytes = 0;
		while (read_bytes < size_in) {
			auto tmp = read(client_fd, buffer + read_bytes, size_in - read_bytes);
			if (tmp <= 0) { printf("here\n"); perror("read"); close(client_fd); continue; }

			read_bytes += tmp;
		}

		build_bvh(buffer, output_buffer, size_in, &size_out);

		int send_bytes = 0;
		while (send_bytes < 4) {
			auto tmp = send(client_fd, &size_out + send_bytes, 4 - send_bytes, 0);
			if (tmp <= 0) { perror("send"); close(client_fd); continue; }
			send_bytes += tmp;
		}

		send_bytes = 0;
		while (send_bytes < size_out) {
            auto tmp = send(client_fd, output_buffer + send_bytes, size_out - send_bytes, 0);
            if (tmp <= 0) { perror("send"); close(client_fd); continue; }
			send_bytes += tmp;
		}
	}

	delete[] buffer;
	delete[] output_buffer;

	close(fd);

	return 2;
}
