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

	delete[] buffer;

    return 0;
}
