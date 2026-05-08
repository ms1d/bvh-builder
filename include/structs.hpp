#pragma once



#include "vec.cuh"
#include <cstdint>



struct bvh_node {
	vec<3> min, max;
	bvh_node *left, *right;
	uint32_t *tris, tris_len; // tris_len = number of elements in tris
};

struct bvh_node_serialised {
	vec<3> min, max;
	uint32_t tris_i, tris_len;
	uint8_t left_i, right_i;
};
