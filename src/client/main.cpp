#include <cstdint>
#include <filesystem>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cstdio>



inline int send_bytes(int fd, const char *bytes, uint32_t len) {
	if (fd < 0) return -1;

	uint32_t sent = 0;
	while (sent < len) {
		auto tmp = send(fd, bytes + sent, len - sent, MSG_NOSIGNAL);
		if (tmp <= 0) { perror("send"); return -2; }
        sent += tmp;
	}

	return 0;
}



int main() {
	const auto in_file_path = std::filesystem::absolute("./test.mesh").string();
	const uint32_t in_file_path_len = static_cast<uint32_t>(in_file_path.size());
	
	const auto out_file_path = std::filesystem::absolute("./test.mesh.out").string();
	const uint32_t out_file_path_len = static_cast<uint32_t>(out_file_path.size());

	static_assert(sizeof(in_file_path_len) == 4);
	static_assert(sizeof(out_file_path_len) == 4);

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);

	struct sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = '\0';
	strcpy(addr.sun_path + 1, "bvh_builderd");

	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("connect"); return 1;
	}

#ifndef NDEBUG
	printf("client: connect succeeded\n");
#endif

	uint32_t buffer_len = sizeof(in_file_path_len) + in_file_path_len + sizeof(out_file_path_len) + out_file_path_len;
	char *buffer = static_cast<char*>(malloc(buffer_len));
	memcpy(buffer, &in_file_path_len, sizeof(in_file_path_len));
	memcpy(buffer + sizeof(in_file_path_len), in_file_path.c_str(), in_file_path_len);
	memcpy(buffer + sizeof(in_file_path_len) + in_file_path_len, &out_file_path_len, sizeof(out_file_path_len));
	memcpy(buffer + sizeof(in_file_path_len) + in_file_path_len + sizeof(out_file_path_len), out_file_path.c_str(), out_file_path_len);

	auto res = send_bytes(fd, reinterpret_cast<const char*>(buffer), buffer_len);
	if (res < 0) return res;
#ifndef NDEBUG
	printf("client: data sent successfully\n");
#endif

	int read_bytes = 0;

    while (read_bytes < 4) {
        auto tmp = read(fd, (char*)&res + read_bytes, 4 - read_bytes);
        if (tmp < 0) { perror("read"); close(fd); return 4; }
        read_bytes += tmp;
    }

#ifndef NDEBUG
	printf("client: response received (%d)\n", res);
#endif

	close(fd);
	free(buffer);
    
	return res;
}
