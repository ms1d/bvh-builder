#include "vec3.cuh"
#include "parse_mesh.hpp"
#include "pools.hpp"
#include "codes.hpp"
#include <cstring>



int parse_mesh(const char *buffer, const uint32_t size, // number of BYTES in data
		vec<3, uint32_t> *&tris, uint32_t &tris_len,
		vec<3> *&verts, uint32_t &verts_len,
		vec<3> &max, vec<3> &min) {


	const char *__restrict ptr = buffer;

	memcpy(&verts_len, ptr, sizeof(verts_len));

	// Need at least 1 triangle (3 vertices)
	// Need verts_len to not be too big (allow space for tris_len)
	if (verts_len < 3 || sizeof(verts_len) + verts_len * sizeof(vec<3>) + sizeof(tris_len) >= size) return -ERR_PARSE_VERTS_LEN;

	verts = reinterpret_cast<vec<3>*>(memory_pool.alloc(verts_len * sizeof(vec<3>), alignof(vec<3>)));
	if (verts == nullptr) return -ERR_PARSE_BAD_ALLOC;
	ptr += sizeof(verts_len);

	memcpy(verts, ptr, sizeof(vec<3>) * verts_len);

	ptr += sizeof(vec<3>) * verts_len;

	find_min_max_verts(verts, verts_len, &max, &min);

	memcpy(&tris_len, ptr, 4);

	ptr += 4;

	if (verts_len * sizeof(vec<3>) + tris_len * sizeof(vec<3, uint32_t>) + sizeof(verts_len) + sizeof(tris_len) != size) return -ERR_PARSE_SIZE;

	// tris_len = number of triangles in tris. each triangle is 3 ints
	tris = reinterpret_cast<vec<3, uint32_t>*>(memory_pool.alloc(tris_len * sizeof(vec<3, uint32_t>), alignof(vec<3, uint32_t>)));
	if (tris == nullptr) return -ERR_PARSE_BAD_ALLOC;
	memcpy(tris, ptr, tris_len * sizeof(vec<3, uint32_t>));
	return 0;
}


void find_min_max_verts(
		vec<3> *verts, uint32_t len,
		vec<3> *max_out, vec<3> *min_out
		) {

	*min_out = vec<3>{  1e10,  1e10,  1e10 };
	*max_out = vec<3>{ -1e10, -1e10, -1e10 };
	
	for (uint32_t i = 0; i < len; i++) {
		auto &v = verts[i];

		max_out->x = std::max(max_out->x, v.x);
		max_out->y = std::max(max_out->y, v.y);
		max_out->z = std::max(max_out->z, v.z);
		min_out->x = std::min(min_out->x, v.x);
		min_out->y = std::min(min_out->y, v.y);
		min_out->z = std::min(min_out->z, v.z);
	}
}
