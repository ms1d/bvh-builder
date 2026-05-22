#include <cstdint>
#include <cstring>
#include <filesystem>
#include <atomic>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include "thread_pool.hpp"
#include "vec3.cuh"
#include "parse_mesh.hpp"
#include "structs.hpp"
#include "build_bvh.hpp"



// Max number of elements in tris per child INCLUSIVE
#define MAX_TRIS 150


void find_min_max_verts(vec<3> *verts, uint32_t *tris, uint32_t len, vec<3> &out_min, vec<3> &out_max) {
	out_min = verts[tris[0]]; out_max = out_min;

	for (uint32_t i = 1; i < len; i++) {
		auto curr = verts[tris[i]];
		out_min.x = std::min(curr.x, out_min.x); out_max.x = std::max(curr.x, out_max.x);
		out_min.y = std::min(curr.y, out_min.y); out_max.y = std::max(curr.y, out_max.y);
		out_min.z = std::min(curr.z, out_min.z); out_max.z = std::max(curr.z, out_max.z);
	}
}



void build_bvh_node(bvh_node *node, vec<3> *verts, std::atomic<uint16_t> &nodes_len, master_resource &res) {

	// If node has few tris, do not recurse; return to caller
	if (node->tris_len <= MAX_TRIS) { nodes_len.fetch_add(1); return; }

	// 1 - Split BVH by longest axis
	vec<3> offset(node->max - node->min);
	int longest_axis = 0;
	for (int i = 1; i < 3; i++) longest_axis = offset.data[i] > offset.data[longest_axis] ? i : longest_axis;
	offset.data[longest_axis] /= 2; offset.data[(longest_axis + 1) % 3] = 0; offset.data[(longest_axis + 2) % 3] = 0;

	// 2 - Create new children
	node->left = new bvh_node; node->right = new bvh_node;
	// Convention: left[i] > mid, right[i] <= mid
	node->left->max = node->max; node->left->min = node->min + offset;
	node->right->max = node->max-offset; node->right->min = node->min;

	node->left->left = node->left->right = node->right->left = node->right->right = nullptr;

	// Sort tris in place and produce pointers for children
	// Front is for left, right is for back
	uint *front = node->tris, *back = node->tris + node->tris_len - 1;
	uint32_t tmp;
	while (front < back) {
		bool left_correct = verts[(*front)].data[longest_axis] > offset.data[longest_axis] + node->min.data[longest_axis],
			 right_correct = verts[*(back-2)].data[longest_axis] <= node->min.data[longest_axis] + offset.data[longest_axis];

		if (!left_correct && !right_correct) {
			// hot path!! fix
			tmp = *front;
			*front = *(back-2);
			*(back-2) = tmp;

			tmp = *(front+1);
			*(front+1) = *(back-1);
			*(back-1) = tmp;

			tmp = *(front+2);
			*(front+2) = *back;
			*back = tmp;
		}
		else {
			left_correct ? front += 3 : 0;
			right_correct ? back -= 3 : 0;
		}
	}

	// front now points to the start of right's nodes
	node->right->tris = front; node->right->tris_len = node->tris_len - (front - node->tris);
	node->left->tris = node->tris; node->left->tris_len = node->tris_len - node->right->tris_len;

	// 3 - Recurse with 2 new threads, await results

	std::atomic<bool> is_ready = false;
	auto success = enable_concurrency && res.build_pool->try_emplace_task(&is_ready, node->left, verts, nodes_len, res);
	if (success) {
		build_bvh_node(node->right, verts, nodes_len, res);
		is_ready.wait(false);
    }
	else {
		build_bvh_node(node->left, verts, nodes_len, res);
		build_bvh_node(node->right, verts, nodes_len, res);
	}

	// This node has children so set its tris to nullptr.
	// Ignore tris_len in this case
	node->tris = nullptr;
	nodes_len.fetch_add(1);
}



void output_bvh_node(bvh_node *curr_node, uint32_t *root_tris, char *output_buffer, std::atomic<uint16_t> &next_pos, uint16_t curr_bvh_pos, master_resource &res) {
	if (curr_node == nullptr) return;

	bvh_node_serialised curr_node_out;
	curr_node_out.max = curr_node->max; curr_node_out.min = curr_node->min;

	if (curr_node->tris != nullptr) { 
		curr_node_out.tris_i = curr_node->tris - root_tris;
		curr_node_out.tris_len = curr_node->tris_len;
		curr_node_out.left_i = curr_node_out.right_i = 0;
	}
	else {
		uint16_t left_index = next_pos++, right_index = next_pos++;
		curr_node_out.left_i = left_index;
		curr_node_out.right_i = right_index;

		std::atomic<bool> is_ready = false;
		auto success = enable_concurrency && res.output_pool->try_emplace_task(&is_ready, curr_node->left, root_tris, output_buffer, next_pos, left_index, res);
		if (success) {
			output_bvh_node(curr_node->right, root_tris, output_buffer, next_pos, right_index, res);
			is_ready.wait(false);
		} else {
			output_bvh_node(curr_node->left, root_tris, output_buffer, next_pos, left_index, res);
			output_bvh_node(curr_node->right, root_tris, output_buffer, next_pos, right_index, res);
		}
	}

	memcpy(output_buffer + curr_bvh_pos * sizeof(bvh_node_serialised), &curr_node_out, sizeof(bvh_node_serialised));
}



void free_bvh_children(bvh_node *node) {
	if (node == nullptr) return;
	free_bvh_children(node->left);
	delete node->left;

	free_bvh_children(node->right);
	delete node->right;
}

void build_bvh(const std::filesystem::path &file_path, master_resource &res) {
	uint32_t *tris, verts_len, tris_len;
	vec<3> *verts, max, min;

	parse_mesh(file_path, tris, tris_len, verts, verts_len, max, min);

	bvh_node root; root.tris = tris; root.tris_len = tris_len; root.max = max; root.min = min;
	std::atomic<uint16_t> nodes_len = 0;

	build_bvh_node(&root, verts, nodes_len, res);

	const auto &dst = file_path.parent_path().parent_path() / "baked" / file_path.filename();

	char *output_buffer = new char[
		+ sizeof(nodes_len)
		+ nodes_len * sizeof(bvh_node_serialised) // bvh_nodes
	];

	std::filesystem::copy(file_path, dst);
	std::filesystem::remove(file_path);

	std::ofstream output(dst, std::ios::binary | std::ios::app);

	memcpy(output_buffer, &nodes_len, sizeof(nodes_len));
	std::atomic<uint16_t> next_pos = 1;
	output_bvh_node(&root, tris, output_buffer + sizeof(nodes_len), next_pos, 0, res);

	output.write(output_buffer + sizeof(nodes_len), nodes_len * sizeof(bvh_node_serialised));
	output.close();

	delete[] verts;
	delete[] tris;

	free_bvh_children(&root);

	res.busy = false;
}
