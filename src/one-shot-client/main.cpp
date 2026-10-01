#include "build_bvh.hpp"
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

	if (len < 32) { printf("Length of file too small\n"); return 2; }
	
	char *buffer = (char*)mmap(0, len, PROT_READ, MAP_PRIVATE, file_fd, 0);
#ifndef NDEBUG
	printf("oneshot: len valid\n");
#endif


	// DO WORK HERE
	int out_fd = open("./test.mesh.out", O_RDWR | O_CREAT | O_EXCL, 0644);
	if (out_fd < 0) { printf("Couldn't create file\n"); return -1; }

	auto res = build_bvh(buffer, len, out_fd);
#ifndef NDEBUG
	printf("oneshot: all done!\n");
#endif

	return res;
}
