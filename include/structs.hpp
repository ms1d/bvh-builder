#pragma once



#include "vec3.cuh"
#include <cstdint>



struct bvh_node {
	vec<3> min, max;
	bvh_node *left, *right;
	uint32_t *tris, tris_len; // tris_len = number of elements in tris
};

struct bvh_node_serialised {
	vec<3, uint16_t> min, max;

	// MSB - is_leaf
	// if is_leaf - 31 bits for tris_index
	// else - 15 bits for left, 16 bits for right
	uint32_t payload;
};
