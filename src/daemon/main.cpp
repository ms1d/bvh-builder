#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "build_bvh.hpp"
#include "codes.hpp"



#define BUFFER_SIZE 4'096



std::filesystem::path path = std::filesystem::current_path();



inline int send_code(int client_fd, uint32_t code, const char *perror_name = nullptr) {
#ifndef NDEBUG
	if (perror_name != nullptr) perror(perror_name);
#endif
	if (client_fd < 0) return -1;

	int sent_bytes = 0;
	while (sent_bytes < 4) {
		int tmp = send(client_fd, (char*)&code + sent_bytes, 4 - sent_bytes, 0);
		if (tmp <= 0) { perror("send"); return -2; }
		sent_bytes += tmp;
	}

#ifndef NDEBUG
	printf("sent code %d\n", code);
#endif
	return 0;
}



inline int read_bytes(int client_fd, char *out, uint32_t len) {
	if (client_fd < 0) return -1;

	uint32_t read_bytes = 0;
	while (read_bytes < len) {
		auto tmp = read(client_fd, out + read_bytes, len - read_bytes);
		if (tmp <= 0) return -2;
		read_bytes += tmp;
	}

	return 0;
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

	char in_file_path[BUFFER_SIZE], out_file_path[BUFFER_SIZE];

	while (1) {
		for (int i = 0; i < BUFFER_SIZE; i++) {
			in_file_path[i] = '\0';
			out_file_path[i] = '\0';
		}

		int client_fd = accept(fd, NULL, NULL);
		if (client_fd < 0) break;

#ifndef NDEBUG
		printf("server: accepted\n");
#endif

		uint32_t in_file_path_len, out_file_path_len;
		auto send_err_read = [client_fd]() {
			send_code(client_fd, ERR_READ, "read");
			close(client_fd);
		};

		if (read_bytes(client_fd, reinterpret_cast<char*>(&in_file_path_len), 4) < 0) { send_err_read(); continue; }
		if (read_bytes(client_fd, in_file_path, in_file_path_len) < 0) { send_err_read(); continue; }
		if (read_bytes(client_fd, reinterpret_cast<char*>(&out_file_path_len), 4) < 0) { send_err_read(); continue; }
		if (read_bytes(client_fd, out_file_path, out_file_path_len) < 0) { send_err_read(); continue; }

#ifndef NDEBUG
		printf("server: client data recieved\n");
#endif

		if (in_file_path_len >= BUFFER_SIZE) { send_code(client_fd, ERR_INVALID_SIZEIN); close(client_fd); continue; };
		if (out_file_path_len >= BUFFER_SIZE) { send_code(client_fd, ERR_INVALID_SIZEOUT); close(client_fd); continue; };

#ifndef NDEBUG
		printf("server: client in and out file_path_lens correct\n");
#endif

		int in_file_fd = open(in_file_path, O_RDONLY);
		if (in_file_fd < 0) {
			send_code(client_fd, ERR_IN_OPEN, "open");
			close(client_fd);
			continue;
		}
		int out_file_fd = open(out_file_path, O_CREAT | O_EXCL | O_RDWR, 0644);
		if (out_file_fd < 0) {
			close(in_file_fd);
			send_code(client_fd, ERR_OUT_OPEN, "open");
			close(client_fd);
			continue;
		}

		struct stat in_s;
		fstat(in_file_fd, &in_s);
		unsigned long in_file_size = static_cast<unsigned long>(in_s.st_size);
		if (in_file_size != static_cast<uint32_t>(in_file_size)) {
			close(in_file_fd); close(out_file_fd);
			send_code(client_fd, ERR_IN_TOO_LONG);
			close(client_fd);
			continue;
		}

		char *input_buffer = static_cast<char*>(mmap(0, in_file_size, PROT_READ, MAP_SHARED, in_file_fd, 0));
		if (input_buffer == MAP_FAILED) {
			close(in_file_fd); close(out_file_fd);
			send_code(client_fd, ERR_IN_MAP, "mmap");
			close(client_fd);
			continue;
		}

		int bvh_res = build_bvh(input_buffer, static_cast<uint32_t>(in_file_size), out_file_fd);
		if (bvh_res < 0) {
			close(in_file_fd); close(out_file_fd);
			send_code(client_fd, static_cast<uint32_t>(-bvh_res));
			close(client_fd);
			continue;
		}

		send_code(client_fd, bvh_res);

#ifndef NDEBUG
		printf("server: bvh_res sent\n");
#endif

		close(client_fd);
		close(in_file_fd);
		close(out_file_fd);
	}


	close(fd);

	return 2;
}
