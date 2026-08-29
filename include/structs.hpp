#pragma once



#include "vec3.cuh"
#include <cstdint>

#define CHILDREN_PER_NODE 4


struct bvh_node {
	vec<3> min, max;
	bvh_node *children = nullptr;
	vec<3, uint32_t> *tris;
	uint32_t tris_len; // tris_len = number of triangles
};

struct bvh_node_serialised {
	vec<3, uint16_t> min, max;

	// LSB - is_leaf
	// if is_leaf - 31 bits for tris_index
	// else - 31 upper bits for left index, right index = left + 1
	uint32_t payload;
};
