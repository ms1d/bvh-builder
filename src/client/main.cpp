#include <fstream>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>



int main() {
	std::ifstream input("test.mesh", std::ios::binary);
	char *buffer = new char[50'000'000];

	input.read(buffer, 50'000'000);
	long len = input.gcount();
	if (len > 50'000'000) return 1;

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);

	struct sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = '\0';
	strcpy(addr.sun_path + 1, "bvh_builderd");

	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("connect"); return 1;
	}

	char size_buff[4]; memcpy(size_buff, &len, 4);
	send(fd, size_buff, 4, 0);

	int sent = 0;
	while (sent < len) {
		auto curr_send = send(fd, buffer + sent, len - sent, 0);
		if (curr_send < 0) {
			perror("send");
		}
		sent += curr_send;
	}

	int read_bytes = 0;
    uint32_t size_in;
    while (read_bytes < 4) {
        auto tmp = read(fd, &size_in + read_bytes, 4 - read_bytes);
        if (tmp < 0) { close(fd); return 1; }
        read_bytes += tmp;
    }

	read_bytes = 0;
	char *output_buffer = new char[size_in];
	while (read_bytes < size_in) {
		auto tmp = read(fd, output_buffer + read_bytes, size_in - read_bytes);
		if (read_bytes < 0) { close(fd); return 1; }
        read_bytes += tmp;
    }

	delete[] output_buffer;
	delete[] buffer;

    return 0;
}
