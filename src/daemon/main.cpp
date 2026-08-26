#include <cstring>
#include <filesystem>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "build_bvh.hpp"
#include "codes.hpp"



#define BUFFER_IN_SIZE 100'000'000
#define BUFFER_OUT_SIZE 100'000'000



std::filesystem::path path = std::filesystem::current_path();



void send_err(int client_fd, uint32_t code) {
	int sent_bytes = 0;
	uint32_t len = 4;

	while (sent_bytes < 4) {
		int tmp = send(client_fd, (char*)&len + sent_bytes, 4 - sent_bytes, 0);
		if (tmp <= 0) { perror("send"); return; }
		sent_bytes += tmp;
	}
	
	sent_bytes = 0;
	while (sent_bytes < 4) {
		int tmp = send(client_fd, (char*)&code + sent_bytes, 4 - sent_bytes, 0);
		if (tmp <= 0) { perror("send"); return; }
		sent_bytes += tmp;
	}

	close(client_fd);
}



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

	while (1) {
		int client_fd = accept(fd, NULL, NULL);

#ifndef NDEBUG
		printf("server: accepted\n");
#endif

		char size_buffer[4]; int read_bytes = 0;

		{
			bool success = true;
			while (read_bytes < 4) {
				auto tmp = read(client_fd, size_buffer + read_bytes, 4 - read_bytes);
				if (tmp <= 0) { perror("read"); success = false; break; }
				read_bytes += tmp;
			}

			if (!success) { send_err(client_fd, ERR_READ); continue; }
		}

#ifndef NDEBUG
		printf("server: client size_in recieved\n");
#endif

		uint32_t size_in, size_out; memcpy(&size_in, size_buffer, 4);

		// Need at least:
		// - 1 verts_len (1 uint32_t = 4 bytes)
		// - 1 vertex (1 vec<3> = 4 * 3 = 12 bytes)
		// - 1 tris_len (1 uint32_t = 4 bytes)
		// - 1 triangle (3 uint32_t = 12 bytes)
		if (size_in > BUFFER_IN_SIZE || size_in < 32) { send_err(client_fd, ERR_INVALID_SIZE); continue; };

#ifndef NDEBUG
		printf("server: client size_in correct\n");
#endif

		read_bytes = 0;

		{
			bool success = true;
			while (read_bytes < size_in) {
				auto tmp = read(client_fd, buffer + read_bytes, size_in - read_bytes);
				if (tmp <= 0) { perror("read"); success = false; break; }

				read_bytes += tmp;
			}

			if (!success) { send_err(client_fd, ERR_READ); continue; }
		}

#ifndef NDEBUG
		printf("server: client data read\n");
#endif

		int bvh_res = build_bvh(buffer, output_buffer, size_in, &size_out);
		if (bvh_res < 0) { send_err(client_fd, static_cast<uint32_t>(-bvh_res)); continue; }

		int send_bytes = 0;
		{
			bool success = true;
			while (send_bytes < 4) {
				auto tmp = send(client_fd, (char*)&size_out + send_bytes, 4 - send_bytes, 0);
				if (tmp <= 0) { perror("send"); success = false; break; }
				send_bytes += tmp;
			}

			if (!success) { close(client_fd); continue; }
		}

#ifndef NDEBUG
		printf("server: size_out sent\n");
#endif

		send_bytes = 0;
		{
			bool success = true;
			while (send_bytes < size_out) {
				auto tmp = send(client_fd, output_buffer + send_bytes, size_out - send_bytes, 0);
				if (tmp <= 0) { perror("send"); success = false; break; }
				send_bytes += tmp;
			}

			if (!success) { close(client_fd); continue; }
		}

#ifndef NDEBUG
		printf("server: data out sent\n");
#endif

		close(client_fd);
	}

	delete[] buffer;
	delete[] output_buffer;

	close(fd);

	return 2;
}
