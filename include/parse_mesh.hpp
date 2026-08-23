#pragma once



#include "vec3.cuh"



int parse_mesh(const char *buffer, const uint32_t size,
		uint32_t *&tris, uint32_t &tris_len,
		vec<3> *&verts, uint32_t &verts_len,
		vec<3> &max, vec<3> &min);

void find_min_max_verts(
		vec<3> *verts, uint32_t len,
		vec<3> *max_out, vec<3> *min_out
		);
