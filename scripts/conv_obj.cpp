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
        std::cout << "usage: obj_to_mesh input.obj output.mesh\n";
        return 1;
    }

    std::ifstream in(argv[1]);

    std::vector<Vec3> vertices;
    std::vector<Tri> triangles;

    std::string line;

    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") {
            Vec3 v;
            ss >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        }

        else if (type == "f") {
            std::vector<uint32_t> indices;
            std::string token;

            while (ss >> token) {
                size_t slash = token.find('/');
                if (slash != std::string::npos)
                    token = token.substr(0, slash);

                indices.push_back(std::stoi(token) - 1);
            }

            // fan triangulation
            for (size_t i = 1; i + 1 < indices.size(); i++) {
                triangles.push_back({
                    indices[0],
                    indices[i],
                    indices[i+1]
                });
            }
        }
    }

    writeMesh(argv[2], vertices, triangles);

    std::cout
        << "vertices: " << vertices.size()
        << "\ntriangles: " << triangles.size()
        << "\n";

    return 0;
}
