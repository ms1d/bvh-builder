#include <cstdint>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cstdio>



int main() {
	int file_fd = open("./test.mesh", O_RDONLY);
	if (file_fd < 0) { printf("Couldn't find file\n"); return -1; }

	struct stat s;

	fstat(file_fd, &s);
	long len = s.st_size;
	if (len > 100'000'000) { printf("Length of file too long\n"); return 1; }

	char *buffer = (char*)mmap(0, len, PROT_READ, MAP_PRIVATE, file_fd, 0);
#ifndef NDEBUG
	printf("client: len valid\n");
#endif

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);

	struct sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = '\0';
	strcpy(addr.sun_path + 1, "bvh_builderd");

	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("connect"); return 2;
	}

#ifndef NDEBUG
	printf("client: connect succeeded\n");
#endif

	char size_buff[4]; memcpy(size_buff, &len, 4);

	int sent = 0;
	while (sent < 4) {
		auto tmp = send(fd, size_buff + sent, 4 - sent, 0);
		if (tmp < 0) { perror("send"); close(fd); return 3; }
		sent += tmp;
	}

#ifndef NDEBUG
	printf("client: size sent succeeded\n");
#endif

	sent = 0;
	while (sent < len) {
		auto tmp = send(fd, buffer + sent, len - sent, 0);
		if (tmp < 0) { perror("send"); close(fd); return 4; }
		sent += tmp;
	}

#ifndef NDEBUG
	printf("client: data sent succeeded\n");
#endif

	int read_bytes = 0;
    uint32_t size_in;
    while (read_bytes < 4) {
        auto tmp = read(fd, (char*)&size_in + read_bytes, 4 - read_bytes);
        if (tmp < 0) { perror("read"); close(fd); return 5; }
        read_bytes += tmp;
    }

#ifndef NDEBUG
	printf("client: response size received\n");
#endif

	read_bytes = 0;
	char *output_buffer = new char[size_in];
	while (read_bytes < size_in) {
		auto tmp = read(fd, output_buffer + read_bytes, size_in - read_bytes);
		if (tmp < 0) { perror("read"); close(fd); return 6; }
        read_bytes += tmp;
    }

#ifndef NDEBUG
	printf("client: response data received\n");
#endif

	uint32_t err = 0;
	if (size_in == 4)
		err = *(uint32_t*)output_buffer;
	
	delete[] output_buffer;

	close(fd);
    
	return err;
}
