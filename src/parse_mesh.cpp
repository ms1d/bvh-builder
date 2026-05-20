#include "vec3.cuh"
#include <cstring>
#include <filesystem>
#include <format>



void parse_mesh(const std::filesystem::path &file_path,
		uint32_t *&tris, uint32_t &tris_len,
		vec<3> *&verts, uint32_t &verts_len,
		vec<3> &max, vec<3> &min) {


	FILE* f = fopen(file_path.c_str(), "rb");
	if (!f) {
		auto msg = std::format("Mesh not found at {}", file_path.c_str());
		throw std::runtime_error(msg);
	}

	fseek(f, 0, SEEK_END);
	unsigned long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *base = (char*)malloc(len);

	size_t read_bytes = fread(base, 1, len, f);
	if (read_bytes != (size_t)len) {
		fclose(f);
		auto msg = std::format("Partial read of mesh at {}", file_path.c_str());
		throw std::runtime_error(msg);
		free(base);
	}

	fclose(f);

	char* __restrict ptr = base;

	memcpy(&verts_len, ptr, 4);

	verts = new vec<3>[verts_len];
	ptr += 4;

	float axis[3];
	float maxx,maxy,maxz; maxx=maxy=maxz=-1e99;
	float minx,miny,minz; minx=miny=minz=1e99;

	for (uint32_t i = 0; i < verts_len; i++) {
		memcpy(axis, ptr, 12);

		verts[i].x = axis[0];
		verts[i].y = axis[1];
		verts[i].z = axis[2];

		maxx = std::max(maxx, axis[0]);
		maxy = std::max(maxy, axis[1]);
		maxz = std::max(maxz, axis[2]);
		
		minx = std::min(minx, axis[0]);
		miny = std::min(miny, axis[1]);
		minz = std::min(minz, axis[2]);


		// stride length = 3 4-byte floats = 12 bytes
		ptr += 12;
	}

	max = { maxx, maxy, maxz };
    min = { minx, miny, minz };

	memcpy(&tris_len, ptr, 4);

	ptr += 4;

	// tris_len = number of elements in tris. each triangle is 3 ints
	tris = new uint32_t[tris_len];
	memcpy(tris, ptr, tris_len * 4);

	// base is a temporary: tris and verts are owned by caller
	free(base);
}
