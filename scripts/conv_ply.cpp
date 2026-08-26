// ---------------------------------
// AI USAGE DISCLOSURE - see README.md
// ---------------------------------



#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>

struct Vec3 {
    float x, y, z;
};

struct Tri {
    uint32_t a, b, c;
};

void writeMesh(
    const std::string& filename,
    const std::vector<Vec3>& vertices,
    const std::vector<Tri>& triangles)
{
    std::ofstream out(filename, std::ios::binary);

    uint32_t nv = vertices.size();
    uint32_t nt = triangles.size();

    out.write((char*)&nv, sizeof(uint32_t));
    out.write((char*)vertices.data(), vertices.size() * sizeof(Vec3));

    out.write((char*)&nt, sizeof(uint32_t));
    out.write((char*)triangles.data(), triangles.size() * sizeof(Tri));
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cout << "usage: ply_to_mesh input.ply output.mesh\n";
        return 1;
    }

    std::ifstream in(argv[1]);

    std::string line;

    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;

    // Read header
    while (std::getline(in, line)) {

        if (line.find("element vertex") == 0) {
            std::stringstream ss(line);
            std::string a, b;
            ss >> a >> b >> vertexCount;
        }

        if (line.find("element face") == 0) {
            std::stringstream ss(line);
            std::string a, b;
            ss >> a >> b >> faceCount;
        }

        if (line == "end_header")
            break;
    }


    std::vector<Vec3> vertices(vertexCount);

    for (uint32_t i = 0; i < vertexCount; i++) {
        in >> vertices[i].x
           >> vertices[i].y
           >> vertices[i].z;
    }


    std::vector<Tri> triangles;

    for (uint32_t i = 0; i < faceCount; i++) {

        uint32_t n;
        in >> n;

        std::vector<uint32_t> indices(n);

        for (uint32_t j = 0; j < n; j++)
            in >> indices[j];

        // triangulate polygons
        for (uint32_t j = 1; j + 1 < n; j++) {
            triangles.push_back({
                indices[0],
                indices[j],
                indices[j+1]
            });
        }
    }


    writeMesh(argv[2], vertices, triangles);

    std::cout
        << "vertices: " << vertices.size()
        << "\ntriangles: " << triangles.size()
        << "\n";

    return 0;
}
