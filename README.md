# bvh-builder

A concurrent BVH builder for path tracing applications
in c++ to run on little-endian machines.

## Usage (WIP)

- Run the daemon executable (for example using systemd)

- Run the client executable to make requests to the daemon

## Input Format

- `uint32_t verts_len` (number of elements in `verts`)

- `vec<3> *verts` (see [ms1d/vec](https://github.com/ms1d/vec))

- `uint32_t tris_len` (number of triangles in `tris`)

- `vec<3, uint32_t> *tris` (each tri is 3 indices into `verts`).

Please ignore any and all references to `tris_len` being the number
of elements in `tris` rather than the number of tris!
Refactor in progress...

## Output Format

- `uint32_t verts_len` (number of elements in `verts`)

- `vec<3> *verts` (see [ms1d/vec](https://github.com/ms1d/vec))

- `uint32_t tris_len` (number of triangles in `tris`)

- `vec<3, uint32_t> *tris` (each tri is 3 indices into `verts`), now sorted

- `uint16_t nodes_len` (number of nodes in `nodes`)

- `bvh_node_serialised *nodes` (see `include/structs.hpp`)
