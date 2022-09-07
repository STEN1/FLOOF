#include "Utils.h"

#include "Floof.h"

#include <fstream>

static void SubDivide(glm::vec3 &a, glm::vec3 &b, glm::vec3 &c, int recursions, std::vector<FLOOF::Vertex> &vertexData);

namespace FLOOF {
	namespace Utils {
		std::pair<std::vector<Vertex>, std::vector<uint32_t>> GetVisimVertexData(const std::string& path) {
			std::vector<Vertex> vertexData;
			std::vector<uint32_t> indexData;

			std::ifstream file(path);
			if (!file.is_open()) {
				std::cout << "Cant open file: " << path << std::endl;
				return { vertexData, indexData };
			}

			uint32_t vertexCount{};
			file >> vertexCount;
			uint32_t indexCount{};
			file >> indexCount;

			vertexData.resize(vertexCount);
			for (Vertex& vertex : vertexData) {
				file >> vertex.pos.x;
				file >> vertex.pos.y;
				file >> vertex.pos.z;
			}

			indexData.resize(indexCount);
			for (uint32_t& i : indexData) {
				file >> i;
			}

			for (uint32_t i = 2; i < vertexData.size(); i += 3) {
				Vertex& a = vertexData[i - 2];
				Vertex& b = vertexData[i - 1];
				Vertex& c = vertexData[i - 0];

				glm::vec3 ab = b.pos - a.pos;
				glm::vec3 ac = c.pos - a.pos;

				glm::vec3 normal = glm::normalize(glm::cross(ab, ac));

				a.normal = normal;
				b.normal = normal;
				c.normal = normal;

				a.uv.x = a.pos.x;
				a.uv.y = a.pos.y;
				b.uv.x = b.pos.x;
				b.uv.y = b.pos.y;
				c.uv.x = c.pos.x;
				c.uv.y = c.pos.y;
			}

			return { vertexData, indexData };
		}

        std::vector<Vertex> MakeBall(int recursions, int radius) {
            int r = radius;
            std::vector<Vertex> vertexData;

            glm::vec3 v0{0,0,r};
            glm::vec3 v1{r,0,0};
            glm::vec3 v2{0,r,0};
            glm::vec3 v3{-r,0,0};
            glm::vec3 v4{0,-r,0};
            glm::vec3 v5{0,0,-r};

            SubDivide(v0, v1, v2, recursions,vertexData);
            SubDivide(v0, v2, v3, recursions,vertexData);
            SubDivide(v0, v3, v4, recursions,vertexData);
            SubDivide(v0, v4, v1, recursions,vertexData);
            SubDivide(v5, v2, v1, recursions,vertexData);
            SubDivide(v5, v3, v2, recursions,vertexData);
            SubDivide(v5, v4, v3, recursions,vertexData);
            SubDivide(v5, v1, v4, recursions,vertexData);

            return { vertexData };
        }
    }
}

static void SubDivide(glm::vec3 &a, glm::vec3 &b, glm::vec3 &c, int recursions, std::vector<FLOOF::Vertex> &vertexData) {
    if (recursions > 0) {
        glm::vec3 v1 = glm::normalize(a + b);
        glm::vec3 v2 = glm::normalize(a + c);
        glm::vec3 v3 = glm::normalize(c + b);

        SubDivide(a, v1, v2, recursions - 1, vertexData);
        SubDivide(c, v2, v3, recursions - 1, vertexData);
        SubDivide(b, v3, v1, recursions - 1, vertexData);
        SubDivide(v3, v2, v1, recursions - 1, vertexData);
    } else {
        FLOOF::Vertex v{};
        // TODO: Calculate normals
        v.pos.x = a.x;
        v.pos.y = a.y;
        v.pos.z = a.z;
        vertexData.push_back(v);
        v.pos.x = b.x;
        v.pos.y = b.y;
        v.pos.z = b.z;
        vertexData.push_back(v);
        v.pos.x = c.x;
        v.pos.y = c.y;
        v.pos.z = c.z;
        vertexData.push_back(v);
    }
}

