#pragma
#include <glm/glm.hpp>
#include <vector>

struct StitchVertex
{
	glm::vec3 position;
	glm::vec3 restPosition;
};

struct StitchFace
{
	int v[4];
};

struct StitchMesh
{
	std::vector<StitchVertex> vertices;
	std::vector<StitchFace> faces;
	int rows;
	int cols;
};
