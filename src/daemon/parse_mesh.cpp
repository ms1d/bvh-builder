#include "vec3.cuh"
#include <cstring>



bool parse_mesh(const char *buffer, uint32_t size, // number of BYTES in data
		uint32_t *&tris, uint32_t &tris_len,
		vec<3> *&verts, uint32_t &verts_len,
		vec<3> &max, vec<3> &min) {


	const char *__restrict ptr = buffer;

	memcpy(&verts_len, ptr, 4);

	// Need at least 1 triangle (3 vertices)
	// Need verts_len to not be too big (allow space for tris_len)
	if (verts_len < 3 || verts_len * 12 + 4 >= size - 4) return false;

	verts = new vec<3>[verts_len];
	ptr += 4;

	memcpy(verts, ptr, verts_len * 12);

	float maxx,maxy,maxz; maxx=maxy=maxz=-1e10;
	float minx,miny,minz; minx=miny=minz=1e10;

	for (uint32_t i = 0; i < verts_len; i++) {
		auto v = verts[i];

		maxx = std::max(maxx, v.x);
		maxy = std::max(maxy, v.y);
		maxz = std::max(maxz, v.z);
		
		minx = std::min(minx, v.x);
		miny = std::min(miny, v.y);
		minz = std::min(minz, v.z);


		// stride length = 3 4-byte floats = 12 bytes
		ptr += 12;
	}

	max = { maxx, maxy, maxz };
    min = { minx, miny, minz };

	memcpy(&tris_len, ptr, 4);

	ptr += 4;

	if ((verts_len + tris_len / 3) * 12 + 8 != size) return false;

	// tris_len = number of elements in tris. each triangle is 3 ints
	tris = new uint32_t[tris_len];
	memcpy(tris, ptr, tris_len * 4);
	return true;
}
