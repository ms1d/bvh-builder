#include "vec3.cuh"
#include "parse_mesh.hpp"
#include "pools.hpp"
#include <algorithm>
#include <cstring>
#include <tuple>

#define SMALL_VERTS_LEN 2'500'000 // TODO: fine tune



bool parse_mesh(const char *buffer, uint32_t size, // number of BYTES in data
		uint32_t *&tris, uint32_t &tris_len,
		vec<3> *&verts, uint32_t &verts_len,
		vec<3> &max, vec<3> &min) {


	const char *__restrict ptr = buffer;

	memcpy(&verts_len, ptr, sizeof(verts_len));

	// Need at least 1 triangle (3 vertices)
	// Need verts_len to not be too big (allow space for tris_len)
	if (verts_len < 3 || verts_len * sizeof(vec<3>) + sizeof(tris_len) >= size - sizeof(verts_len)) return false;

	verts = new vec<3>[verts_len];
	ptr += sizeof(verts_len);

	memcpy(verts, ptr, sizeof(vec<3>) * verts_len);

	ptr += sizeof(vec<3>) * verts_len;

	if (verts_len < SMALL_VERTS_LEN) {
#define TOTAL_WORKERS (NUM_THREADS + 1)
	vec<3> extreme_verts[2 * TOTAL_WORKERS];
	vec<3> *maxes = extreme_verts, *mins = maxes + TOTAL_WORKERS;
	
	tp_task<wrapper> tasks[NUM_THREADS];
	find_min_max_verts_args args[NUM_THREADS];
	const uint32_t thread_len = verts_len / (TOTAL_WORKERS);

	for (uint32_t i = 0; i < NUM_THREADS; i++) {
		args[i] = find_min_max_verts_args(verts + i * thread_len, thread_len, maxes + i, mins + i);
		tasks[i].args = std::make_tuple(WRAPPER_TYPE_FIND, static_cast<void*>(&args));
		worker_pool.submit(tasks + i);
	}

	find_min_max_verts(verts + NUM_THREADS * thread_len, thread_len + verts_len % (TOTAL_WORKERS), maxes + NUM_THREADS, mins + NUM_THREADS);

	for (uint32_t i = 0; i < NUM_THREADS; i++)
		tasks[i].is_result_ready.wait(false);

	find_min_max_verts(extreme_verts, 2 * TOTAL_WORKERS, &max, &min);
	
	} else {
		find_min_max_verts(verts, verts_len, &max, &min);
	}

	memcpy(&tris_len, ptr, 4);

	ptr += 4;

	if ((verts_len + tris_len / 3) * 12 + 8 != size) return false;

	// tris_len = number of elements in tris. each triangle is 3 ints
	tris = new uint32_t[tris_len];
	memcpy(tris, ptr, tris_len * 4);
	return true;
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
