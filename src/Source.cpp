#define _CRT_SECURE_NO_WARNINGS
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstring>

#include "VertexBuffer.h"
#include "VertexBufferLayout.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Texture.h"
#include "FrameBuffer.h"


#include "cyCore.h"
#include "cyVector.h"
#include "cyTriMesh.h"
#include <eigen-5.0.0/Eigen/Sparse>
#include <eigen-5.0.0/Eigen/IterativeLinearSolvers>
#include <eigen-5.0.0/Eigen/SparseCholesky>

#define EIGEN_VECTORIZE_AVX2
#define PI 3.1415926f
#define SPLINESAMPLES 8

float BIAS = 0.001f;
const unsigned int width = 1080;
const unsigned int height = 1920;

bool firstMouse = true;
float yaw = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
float pitch = 0.0f;
float lastX = height / 2.0;
float lastY = width / 2.0;
float fov = 45.0f;
float targetFov = 45.0f;

float nearPlane = 0.1f;
float farPlane = 300.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

float quadScale = 15.0f;
float quadRot = 0.0f;
float quadYaw = -90.0f;
float quadPitch = 0.0f;
float quadfov = 45.0f;
float targetQuadfov = 45.0f;

float EPSILON = 0.01f;
float GAMMA = 2.2f;
float GRAVITY = 9.8f;
float MASS = 0.00f;

bool takeScreenshot = false;
bool QUAD = false;	
bool UImode = false;
bool showMesh = false;
bool showDual = false;
bool colorByRow = false;

float yarnRadius = 0.075f; //radius of thread
int sides = 8;
int iRows = 8;
int iCols = 8;
float stitchHeight = 1.0f;
float stitchWidth = 1.0f;
float restLengthCourse = 0.75f;
float restLengthWale = 0.75f;

glm::vec3 quadPos = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 quadCameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 quadCameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 quadCameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

glm::mat4 i4 = glm::mat4(1.0f); //The identity matrix
glm::vec3 playerPos = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 50.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

glm::vec3 targetCameraPos = cameraPos;
glm::vec3 targetQuadCameraPos = cameraPos;

glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 lightPos;
glm::vec3 meshCenter;

float lightTheta = 0.0f;
float lightPhi = 45.0f;
float lightRadius = 50.0f;

float shadowNearPlane = 0.1f;
float shadowFarPlane = 300.0f;

int   morphType = 0;      
float morphAmount = 0.0f;

void TakeScreenshot(const std::string& filename, int width, int height)
{
	std::vector<unsigned char> pixels(width * height * 3);
	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

	//Reminder that OpenGL Reads bottom to top. 
	//STB writed TOP to bottom though
	std::vector<unsigned char> flipped(pixels.size());
	int rowSize = width * 3;
	
	for (int y = 0; y < height; y++)
	{
		memcpy(flipped.data() + y * rowSize, pixels.data() + (height - 1 - y) * rowSize, rowSize);
	}
	
	stbi_write_png(filename.c_str(), width, height, 3, flipped.data(), rowSize);
	std::cout << "Screenshot saved: " << filename << std::endl;
}

std::string GetScreenshotFilename()
{
	static int count = -1;

	// First call: scan for the highest existing screenshot number
	if (count == -1)
	{
		count = 0;
		while (std::filesystem::exists("screenshots/screenshot_" + std::to_string(count) + ".png"))
			count++;
	}

	std::string filename = "screenshots/screenshot_" + std::to_string(count) + ".png";
	count++;
	return filename;
}

//*********************************************************************************************************//
//*************************// ALL RELEVANT STITCH MESH RELATED RELAXATION STUFF //*************************//
//*********************************************************************************************************//

//This is Kui's knot struct construction. Haven't used them yet but here it is future me.
struct StitchKnots
{
	glm::vec3 pos; //x_k
	glm::vec3 c0_rest, c1_rest; //These will hold the true resting points as the points get deformed.
	glm::vec3 p0_rest, p1_rest;

	glm::vec3 c0; //TOP contact point
	glm::vec3 c1; //LOWER contact Points

	glm::vec3 p0; //INNER control point
	glm::vec3 p1; //OUTER control Points


	glm::vec3 c0_plus, c0_minus; //c0 lifted along the normal
	glm::vec3 c1_plus, c1_minus; //c1 lifted along the normal
	//basically c0 & c1 are the "center points" right in between where the yarns would intersect. Adding and subtracting will cover the distance both above and below
	//those points to "touch" or "reach" the yarn of the two loops.

	glm::vec3 d_dir; //direction from c0 to c1
	glm::vec3 b_dir; //perpendicular to d. AKA the direction from p0 to p1
	glm::vec3 normal; //normal of the sticth face

};

struct StitchMesh
{
	std::vector<glm::vec3> vertices;

	struct Face
	{
		int bl, br, tl, tr; //bottom left, etc/
	};

	std::vector<Face> faces;

	int nRows, nCols;
	float stitchHeight;
	float stitchWidth;
};

struct DualGraph
{
	std::vector<glm::vec3> nodes;
	int nRows, nCols;

	struct Neighbor
	{
		int top, right, bot, left;
	};

	std::vector<Neighbor> neighbors;

};

//ok deal with this initialization later looks liek this is the annoying bit, BUUUT I THiNK I WE CAN FIND CLOSEST POINTS IN QUADRANTS.
//I mean it LOOKS like they're distinctly in the four "corners"
void InitKnots(const StitchMesh& sm, int faceIdx, float yarnRadius)
{
	StitchKnots k;
	auto& face = sm.faces[faceIdx];
	glm::vec3 bl = sm.vertices[face.bl];
	glm::vec3 br = sm.vertices[face.br];
	glm::vec3 tl = sm.vertices[face.tl];
	glm::vec3 tr = sm.vertices[face.tr];

}

struct TubeFrame
{
	glm::vec3 tangent;
	glm::vec3 normal;
	glm::vec3 binormal;
};

//Creates and returns a tangent, norma, and binormal to create the centerply
TubeFrame InitFrame(glm::vec3 tangent)
{
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

	if (abs(glm::dot(tangent, up)) >= 0.99f)
		up = glm::vec3(1.0f, 0.0f, 0.0f);

	glm::vec3 binormal = normalize(cross(tangent, up)); //(basically the "right" vector)
	glm::vec3 normal = normalize(cross(binormal, tangent));

	return { tangent, normal, binormal };
}

//Parallel Transport Frame. Created so I have have a continuous tangent normal and binornal throughout the generated tubes.
TubeFrame TransportFrame(TubeFrame prev, glm::vec3 newTangent)
{
	//This will be perpendicular to both tangents and create a direction outwards from the centerply to rotate around. Think unit circle
	glm::vec3 axis = glm::cross(prev.tangent, newTangent); 
	
	if (glm::length(axis) < 0.0001f)
		return { newTangent, prev.normal, prev.binormal };
	
	axis = glm::normalize(axis);
	float angle = acos(glm::clamp(glm::dot(prev.tangent, newTangent), -1.0f, 1.0f));
	glm::mat4 rot = glm::rotate(glm::mat4(1.0f), angle, axis);
	glm::vec3 newNormal = glm::normalize(glm::vec3(rot * glm::vec4(prev.normal, 0.0f)));
	glm::vec3 newBinormal = glm::normalize(glm::cross(newTangent, newNormal));
	
	return { newTangent, newNormal, newBinormal };
}

void GenerateTube(const std::vector<glm::vec3>& points, const std::vector<glm::vec3> tangents, float radius, int sides, std::vector<float>& vertices, std::vector<unsigned int>& indices, float row, float fixRotation)
{

	unsigned int baseVertex = vertices.size() / 8;
	TubeFrame frame = InitFrame(tangents[0]);

	float fix = glm::radians(fixRotation / (sides * 2.0f)); //This is to align the center loop with the ones below VERTICALLY. Horizontals are perf fine.

	for (int i = 0; i < (int)points.size(); i++)
	{
		if (i > 0)
			frame = TransportFrame(frame, tangents[i]);

		for (int s = 0; s < sides; s++)
		{
			float angle = (2.0f * PI * float(s) + fix) / float(sides);
			glm::vec3 circle = radius * (cos(angle) * frame.normal + sin(angle) * frame.binormal);
			glm::vec3 pos = points[i] + circle;
			glm::vec3 normal = glm::normalize(circle);

			vertices.push_back(pos.x);
			vertices.push_back(pos.y);
			vertices.push_back(pos.z);
			vertices.push_back(normal.x);
			vertices.push_back(normal.y);
			vertices.push_back(normal.z);
			vertices.push_back(row);
			vertices.push_back(0.0f);
		}
	}

	for (int i = 0; i < (int)points.size() - 1; i++)
	{
		for (int s = 0; s < sides; s++)
		{
			unsigned int a = baseVertex + i * sides + s;
			unsigned int b = baseVertex + i * sides + (s + 1) % sides;
			unsigned int c = baseVertex + (i + 1) * sides + s;
			unsigned int d = baseVertex + (i + 1) * sides + (s + 1) % sides;

			indices.push_back(a); 
			indices.push_back(c); 
			indices.push_back(b);
			indices.push_back(b); 
			indices.push_back(c); 
			indices.push_back(d);
		}
	}
}

//Ya boi Edwin and Raphael
glm::vec3 CatmullRom(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t)
{
	float t2 = t * t;
	float t3 = t * t * t;
	return 0.5f * ((2.0f * p1) + 
		(p2 - p0) * t + 
		(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + 
		(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

glm::vec3 CatmullRomTangent(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t)
{
	float t2 = t * t;
	glm::vec3 CRtan = 0.5f * 
		((p2 - p0)
		+ 2.0f * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t
		+ 3.0f * (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t2);

	return normalize(CRtan);
}

void BuildStitchMesh(StitchMesh& sm, int nRows, int nCols, float stitchWidth, float stitchHeight)
{
	sm.nRows = nRows;
	sm.nCols = nCols;
	sm.stitchWidth = stitchWidth;
	sm.stitchHeight = stitchHeight;

	sm.vertices.clear();
	sm.faces.clear();

	//pushing back a bunch of vertices for each quad
	for (int r = 0; r <= nRows; r++)
	{
		for (int c = 0; c <= nCols; c++)
		{
			sm.vertices.push_back({ c * stitchWidth, r * stitchHeight, 0.0f });
		}
	}

	for (int r = 0; r < nRows; r++)
	{
		for (int c = 0; c < nCols; c++)
		{
			StitchMesh::Face f;
			f.bl = r * (nCols + 1) +  c;
			f.br = r * (nCols + 1) + (c + 1);
			f.tl = (r + 1) * (nCols + 1) + c;
			f.tr = (r + 1) * (nCols + 1) + (c + 1);
			sm.faces.push_back(f);
		}
	}
}

void BuildDualGraph(StitchMesh& sm, DualGraph& dg)
{
	dg.nRows = sm.nRows;
	dg.nCols = sm.nCols;

	dg.nodes.clear();
	dg.neighbors.clear();

	for (auto& face : sm.faces)
	{
		glm::vec3 center = (sm.vertices[face.bl] + sm.vertices[face.br] + sm.vertices[face.tl] + sm.vertices[face.tr]) * 0.25f;
		dg.nodes.push_back(center);
	}

	for (int r = 0; r < dg.nRows; r++)
	{
		for (int c = 0; c < dg.nCols; c++)
		{
			int idx = r * dg.nCols + c;

			DualGraph::Neighbor n;

			n.top = (r > 0) ? (idx - dg.nCols) : -1;
			n.bot = (r < dg.nRows - 1) ? (idx + dg.nCols) : -1;
			n.left = (c > 0) ? (idx - 1) : -1;
			n.right = (c < dg.nCols - 1) ? (idx + 1) : -1;

			dg.neighbors.push_back(n);
		}
	}
}


void RelaxNeighbor(StitchMesh& sm, DualGraph& dg, float timeStep, float kernelSpring, float boundSpring, float eShear, float eBend, float eSlide, float rCourse, float rWale)
{
	int N = dg.nodes.size();
	std::vector<glm::vec3> gradient(N);
	std::vector<glm::mat3> hessian(N);
	std::vector<Eigen::Triplet<float>> triplets; //will use this to store hessian values
	Eigen::VectorXf grad(3 * N); //new global gradient
	grad.setZero();

	//Makes it a lot easier to set the blocks in the sparse matrix. Each entry will fill out a 3x3 matrix inside the larger sparse matrix and then we'll solve the sparse system simultaneously.
	//it's like the local one I did before, but I'm putting them into one giant sparse matrix and then using Conjugate to solve through it.
	auto AddBlock = [&](int a, int b, const glm::mat3& H)
	{
		for (int r = 0; r < 3; r++)
		{
			for (int c = 0; c < 3; c++)
			{
				triplets.emplace_back(3*a + r, 3*b + c, H[c][r]); //REminder GLM IS COLUMN FIRST. horrid
			}
		}
	};

	gradient.assign(N, glm::vec3(0.0f));
	hessian.assign(N, glm::mat3(0.0f));

	for (int idx = 0; idx < (int)dg.neighbors.size(); idx++)
	{
		auto& neighbor = dg.neighbors[idx];

		//i = index of one node
		//j = index of second node
		//L = restLength of each singular course and wale quad
		//eSpring = spring constant. Will be different depending if it's a boundary spring or a kernel spring
		auto Spring = [&](int i, int j, float L, float eSpring)
		{
			glm::vec3 xi = dg.nodes[i];
			glm::vec3 xj = dg.nodes[j];
			glm::vec3 d = xi - xj;
			float len = glm::length(d); //cursive L
			glm::vec3 n = d / len; //normalized

			glm::vec3 g = eSpring * (len - L) * n;

			gradient[i] += g;
			gradient[j] -= g;

			glm::mat3 nnT = glm::outerProduct(n, n);
			glm::mat3 H = eSpring * ((1.0f - (L / len)) * glm::mat3(1.0f) + (L / len) * nnT);

			hessian[i] += H;
			hessian[j] += H;

			AddBlock(i, i, H);
			AddBlock(j, j, H);
			AddBlock(i, j, -H);
			AddBlock(j, i, -H);
		};

		//Kernel Spring - For anything with 4 neighbors
		if (neighbor.top >= 0 && neighbor.bot >= 0)
			Spring(neighbor.top, neighbor.bot, rWale * 2.0f, kernelSpring);

		if (neighbor.left >= 0 && neighbor.right >= 0)
			Spring(neighbor.left, neighbor.right, rCourse * 2.0f, kernelSpring);

		//Boundary Springs - for anything that does not have all 4 neighbors. Will check to see which direction is "empty" and proceed to calculate the boundary spring forces
		if (neighbor.left < 0)
			Spring(idx, neighbor.right, rCourse, boundSpring);

		if (neighbor.right < 0)
			Spring(neighbor.left, idx, rCourse, boundSpring);

		if (neighbor.top < 0)
			Spring(idx, neighbor.bot, rWale, boundSpring);

		if (neighbor.bot < 0)
			Spring(neighbor.top, idx, rWale, boundSpring);

		auto Shear = [&](int t, int r, int b, int l)
		{
			glm::vec3 vc = dg.nodes[r] - dg.nodes[l];
			glm::vec3 vw = dg.nodes[t] - dg.nodes[b];
			float dot = glm::dot(vc, vw);

			glm::vec3 xr = eShear * dot * vw;
			glm::vec3 xl = -eShear * dot * vw;
			glm::vec3 xt = eShear * dot * vc;
			glm::vec3 xb = -eShear * dot * vc;

			gradient[t] += xt;
			gradient[r] += xr;
			gradient[b] += xb;
			gradient[l] += xl;

			//Intuitively the dot product is bilinear so it's derivative will basically be a constant and its 2nd derivative becomes 0. Just left with the outerproduct and k
			glm::mat3 Hcourse = eShear * glm::outerProduct(vc, vc);
			glm::mat3 Hwale = eShear * glm::outerProduct(vw, vw);
			//glm::mat3 Hcross = eShear * glm::outerProduct(vc, vw);
			glm::mat3 Hcross = eShear * (glm::outerProduct(vc, vw) + dot * glm::mat3(1.0f));

			hessian[t] += Hcourse;
			hessian[b] += Hcourse;
			hessian[r] += Hwale;
			hessian[l] += Hwale;

			AddBlock(t, t, Hcourse);
			AddBlock(b, b, Hcourse);
			AddBlock(r, r, Hwale);
			AddBlock(l, l, Hwale);

			AddBlock(t, b, -Hcourse);
			AddBlock(b, t, -Hcourse);
			AddBlock(r, l, -Hwale);
			AddBlock(l, r, -Hwale);


			glm::mat3 HcrossT = glm::transpose(Hcross);
			AddBlock(t, r, Hcross);
			AddBlock(r, t, HcrossT);
			AddBlock(t, l, -Hcross);
			AddBlock(l, t, -HcrossT);
			AddBlock(b, r, -Hcross);
			AddBlock(r, b, -HcrossT);
			AddBlock(b, l, Hcross);
			AddBlock(l, b, HcrossT);
		};

		//Should only run for full kernels
		if (neighbor.top >= 0 && neighbor.right >= 0 && neighbor.bot >= 0 && neighbor.left >= 0)
		{
			Shear(neighbor.top, neighbor.right, neighbor.bot, neighbor.left);
		}

		//eeeeuuurgh
		auto Bend = [&](int i0, int i1, int i2)
		{
			glm::vec3 x0 = dg.nodes[i0];
			glm::vec3 x1 = dg.nodes[i1];
			glm::vec3 x2 = dg.nodes[i2];

			glm::vec3 v0 = x1 - x0;
			glm::vec3 v1 = x2 - x1;
			float l0 = glm::length(v0);
			float l1 = glm::length(v1);
			if (l0 < 1e-8f || l1 < 1e-8f) return;

			glm::vec3 v0n = v0 / l0;
			glm::vec3 v1n = v1 / l1;
			float c = glm::dot(v0n, v1n);

			// dE/dc
			float dEdc = -eBend * (1.0f - c);

			glm::vec3 dc_dv0 = (v1n - c * v0n) / l0;
			glm::vec3 dc_dv1 = (v0n - c * v1n) / l1;

			glm::vec3 g0 = -dEdc * dc_dv0;
			glm::vec3 g2 = dEdc * dc_dv1;
			glm::vec3 g1 = dEdc * (dc_dv0 - dc_dv1);

			gradient[i0] += g0;
			gradient[i1] += g1;
			gradient[i2] += g2;

			glm::vec3 u = dc_dv0 - dc_dv1;
			hessian[i0] += eBend * glm::outerProduct(dc_dv0, dc_dv0);
			hessian[i1] += eBend * glm::outerProduct(u, u);
			hessian[i2] += eBend * glm::outerProduct(dc_dv1, dc_dv1);

			AddBlock(i0, i0, eBend * glm::outerProduct(dc_dv0, dc_dv0));
			AddBlock(i1, i1, eBend * glm::outerProduct(u, u));
			AddBlock(i2, i2, eBend * glm::outerProduct(dc_dv1, dc_dv1));

			glm::mat3 H01 = eBend * glm::outerProduct(-dc_dv0, u);
			glm::mat3 H02 = eBend * glm::outerProduct(-dc_dv0, dc_dv1);
			glm::mat3 H12 = eBend * glm::outerProduct(u, dc_dv1);

			AddBlock(i0, i1, H01);
			AddBlock(i1, i0, glm::transpose(H01));
			AddBlock(i0, i2, H02);
			AddBlock(i2, i0, glm::transpose(H02));
			AddBlock(i1, i2, H12);
			AddBlock(i2, i1, glm::transpose(H12));
		};

		auto Slide = [&](int i0, int i1, int i2)
		{
			glm::vec3 x0 = dg.nodes[i0], x1 = dg.nodes[i1], x2 = dg.nodes[i2];
			glm::vec3 v0 = x1 - x0;
			glm::vec3 v1 = x2 - x1;
			glm::vec3 v2 = x2 - x0;

			float l0sq = glm::dot(v0, v0), l1sq = glm::dot(v1, v1), l2sq = glm::dot(v2, v2);
			if (l2sq < 1e-12f) return;

			float s = (l1sq - l0sq) / l2sq;
			float dEds = eSlide * s;
			float inv = 1.0f / l2sq;

			glm::vec3 ds_dx0 = 2.0f * inv * (v0 + s * v2);
			glm::vec3 ds_dx1 = -2.0f * inv * v2;
			glm::vec3 ds_dx2 = 2.0f * inv * (v1 - s * v2);   // this one was already correct

			gradient[i0] += dEds * ds_dx0;
			gradient[i1] += dEds * ds_dx1;
			gradient[i2] += dEds * ds_dx2;

			hessian[i0] += eSlide * glm::outerProduct(ds_dx0, ds_dx0);
			hessian[i1] += eSlide * glm::outerProduct(ds_dx1, ds_dx1);
			hessian[i2] += eSlide * glm::outerProduct(ds_dx2, ds_dx2);

			AddBlock(i0, i0, eSlide* glm::outerProduct(ds_dx0, ds_dx0));
			AddBlock(i1, i1, eSlide* glm::outerProduct(ds_dx1, ds_dx1));
			AddBlock(i2, i2, eSlide* glm::outerProduct(ds_dx2, ds_dx2));

			glm::mat3 H01 = eSlide * glm::outerProduct(ds_dx0, ds_dx1);
			glm::mat3 H02 = eSlide * glm::outerProduct(ds_dx0, ds_dx2);
			glm::mat3 H12 = eSlide * glm::outerProduct(ds_dx1, ds_dx2);

			AddBlock(i0, i1, H01); AddBlock(i1, i0, glm::transpose(H01));
			AddBlock(i0, i2, H02); AddBlock(i2, i0, glm::transpose(H02));
			AddBlock(i1, i2, H12); AddBlock(i2, i1, glm::transpose(H12));
		};

		// wale direction: top, current node, bot (three vertically stacked dual nodes)
		if (neighbor.top >= 0 && neighbor.bot >= 0)
		{
			Bend(neighbor.top, idx, neighbor.bot);
			Slide(neighbor.top, idx, neighbor.bot);
		}

		// course direction: left, current node, right
		if (neighbor.left >= 0 && neighbor.right >= 0)
		{
			Bend(neighbor.left, idx, neighbor.right);
			Slide(neighbor.left, idx, neighbor.right);
		}
	}

	for (int i = 0; i < N; i++)
	{
		gradient[i].z += MASS * GRAVITY;
	}

	for (int i = 0; i < 3 * N; i++)
	{
		triplets.emplace_back(i, i, EPSILON); //adding a small epsilon as a triplet
	}

	Eigen::SparseMatrix<float> H(3 * N, 3 * N);
	H.setFromTriplets(triplets.begin(), triplets.end());

	Eigen::ConjugateGradient<Eigen::SparseMatrix<float>, Eigen::Lower | Eigen::Upper, Eigen::DiagonalPreconditioner<float>> cg;
	cg.compute(H);

	for (int i = 0; i < N; i++)
	{
		grad[3 * i + 0] = gradient[i].x;
		grad[3 * i + 1] = gradient[i].y;
		grad[3 * i + 2] = gradient[i].z;
	}
	Eigen::VectorXf dx = cg.solve(-grad);

	for (int i = 0; i < N; i++)
	{
		dg.nodes[i].x += timeStep * dx[3 * i + 0];
		dg.nodes[i].y += timeStep * dx[3 * i + 1];
		dg.nodes[i].z += timeStep * dx[3 * i + 2];
	}

	//I'm stupid. I forgot that I made sm.nCols and sm.nRows represent the number of FACES. This means the sm dimension is REALLY sm.nCols + 1 and sm.nRows + 1. lol so stupid.
	for (int r = 0; r <= sm.nRows; r++)
	{
		for (int c = 0; c <= sm.nCols; c++)
		{
			glm::vec3 total = glm::vec3(0.0f);
			int idx = r * (sm.nCols + 1) + c;
			int br = r * dg.nCols + c;
			int bl = r * dg.nCols + (c - 1);
			int tr = (r - 1) * dg.nCols + c;
			int tl = (r - 1) * dg.nCols + (c - 1);

			float corner = 0.25f;

			if (((r == 0) || (r == sm.nRows)) && ((c == 0) || (c == sm.nCols)))
			{
				corner = 0.5f;
				if (r == 0)
				{
					if (c == 0) //upper left
					{
						total += 2.0f * dg.nodes[br] - dg.nodes[br + 1];
						total += 2.0f * dg.nodes[br] - dg.nodes[br + dg.nCols];
						//total += 2.0f * sm.vertices[idx] - dg.nodes[br];
					}

					else //upper right
					{
						total += 2.0f * dg.nodes[bl] - dg.nodes[bl - 1];
						total += 2.0f * dg.nodes[bl] - dg.nodes[bl + dg.nCols];
						//total += 2.0f * sm.vertices[idx] - dg.nodes[bl];
					}
				}

				else
				{
					if (c == 0) //lower left
					{
						total += 2.0f * dg.nodes[tr] - dg.nodes[tr + 1];
						total += 2.0f * dg.nodes[tr] - dg.nodes[tr - dg.nCols];
						//total += 2.0f * sm.vertices[idx] - dg.nodes[tr];

					}

					else //lower right
					{
						total += 2.0f * dg.nodes[tl] - dg.nodes[tl - 1];
						total += 2.0f * dg.nodes[tl] - dg.nodes[tl - dg.nCols];
						//total += 2.0f * sm.vertices[idx] - dg.nodes[tl];
					}
				}
			}

			//corners are already checked so we can just check the edges without worrying about corner cases
			else
			{
				if (r == 0)
				{
					total += dg.nodes[bl];
					total += dg.nodes[br];
					total += 2.0f * dg.nodes[bl] - dg.nodes[bl + dg.nCols];
					total += 2.0f * dg.nodes[br] - dg.nodes[br + dg.nCols];
				}

				else if (r == sm.nRows)
				{
					total += dg.nodes[tl];
					total += dg.nodes[tr];
					total += 2.0f * dg.nodes[tl] - dg.nodes[tl - dg.nCols];
					total += 2.0f * dg.nodes[tr] - dg.nodes[tr - dg.nCols];
				}

				else if (c == 0)
				{
					total += dg.nodes[tr];
					total += dg.nodes[br];
					total += 2.0f * dg.nodes[tr] - dg.nodes[tr + 1];
					total += 2.0f * dg.nodes[br] - dg.nodes[br + 1];
				}

				else if (c == sm.nCols)
				{
					total += dg.nodes[tl];
					total += dg.nodes[bl];
					total += 2.0f * dg.nodes[tl] - dg.nodes[tl - 1];
					total += 2.0f * dg.nodes[bl] - dg.nodes[bl - 1];
				}

				else
				{
					total += dg.nodes[tl] + dg.nodes[tr] + dg.nodes[bl] + dg.nodes[br];
				}
			}

			sm.vertices[idx] = total * corner;

		}
	}

}

void Relax(StitchMesh& sm, DualGraph& dg, float timeStep, float kStretch, float kShear, float kWale, float rCourse, float rWale)
{
	//Newton Solver gradients and Hessian matrix
	int N = sm.vertices.size();
	std::vector<glm::vec3> gradient(N);
	std::vector<glm::mat3> hessian(N);

	for (int i = 0; i < N; i++)
	{
		gradient[i] = glm::vec3(0.0f);
		hessian[i] = glm::mat3(0.0f);
	}

	for (auto& face : sm.faces)
	{
		auto Stretch = [&](int i, int j, float restLength)
		{
			glm::vec3 d = sm.vertices[i] - sm.vertices[j];
			float length = glm::length(d);
			glm::vec3 dn = d / length;
			float r = restLength / length;
			glm::vec3 f = kStretch * (1.0f - r) * dn;

			gradient[i] += f;
			gradient[j] -= f;

			glm::mat3 H;

			H[0][0] = kStretch * ((1.0f - r) + r * dn.x * dn.x); //xx

			H[0][1] = kStretch * (r * dn.x * dn.y); //xy
			H[1][0] = H[0][1];

			H[0][2] = kStretch * (r * dn.x * dn.z); //xz
			H[2][0] = H[0][2];

			H[1][1] = kStretch * ((1.0f - r) + r * dn.y * dn.y); //yy

			H[1][2] = kStretch * (r * dn.y * dn.z); //yz
			H[2][1] = H[1][2];

			H[2][2] = kStretch * ((1.0f - r) + r * dn.z * dn.z); //zz

			hessian[i] += H;   // same block contributes to both endpoints
			hessian[j] += H;
		};

		Stretch(face.bl, face.br, rCourse);
		Stretch(face.tl, face.tr, rCourse);
		Stretch(face.bl, face.tl, rWale);
		Stretch(face.br, face.tr, rWale);

		//Paper says to calculate the diagonal stretch as well like this
		float diagRest = sqrt(rCourse * rCourse + rWale * rWale);
		Stretch(face.bl, face.tr, diagRest);
		Stretch(face.br, face.tl, diagRest);

		auto Shear = [&](int i, int j, int k)
		{
			glm::vec3& xi = sm.vertices[i];
			glm::vec3& xj = sm.vertices[j];
			glm::vec3& xk = sm.vertices[k];

			glm::vec3 a = xi - xj;
			glm::vec3 b = xk - xj;
			float dot = glm::dot(a, b);

			glm::vec3 gi = kShear * dot * b;
			glm::vec3 gk = kShear * dot * a;
			glm::vec3 gj = -kShear * dot * (a + b);

			gradient[i] += gi;
			gradient[k] += gk;
			gradient[j] += gj;

			hessian[i] += kShear * glm::outerProduct(b, b);
			hessian[k] += kShear * glm::outerProduct(a, a);
			hessian[j] += kShear * glm::outerProduct(a + b, a + b);
		};

		Shear(face.bl, face.br, face.tr);
		Shear(face.br, face.tr, face.tl);
		Shear(face.tr, face.tl, face.bl);
		Shear(face.tl, face.bl, face.br);
	}

	for (int r = 0; r < sm.nRows - 1; r++)
	{
		for (int c = 0; c <= sm.nCols; c++)
		{
			int i = r * (sm.nCols + 1) + c;
			int j = (r + 1) * (sm.nCols + 1) + c;
			int k = (r + 2) * (sm.nCols + 1) + c;

			if (k >= sm.vertices.size())
				continue;

			glm::vec3 xi = sm.vertices[i];
			glm::vec3 xj = sm.vertices[j];
			glm::vec3 xk = sm.vertices[k];

			float rijk = glm::max(rWale, glm::length(xi - xj) + glm::length(xk - xj));
			float ikLength = glm::length(xi - xk);

			glm::vec3 f = -kWale * ((ikLength / rijk) - 1.0f) * (xi - xk) / ikLength;

			gradient[i] += f;
			gradient[k] -= f;
		}
	}

	for (int i = 0; i < N; i++)
	{
		gradient[i].z -= MASS * GRAVITY;
	}


	for (int i = 0; i < sm.vertices.size(); i++)
	{
		glm::vec3 dx = glm::inverse(hessian[i]) * (-gradient[i]);
		sm.vertices[i] += timeStep * dx;
	}

	for (int fIdx = 0; fIdx < (int)sm.faces.size(); fIdx++)
	{
		auto& face = sm.faces[fIdx];
		dg.nodes[fIdx] = (sm.vertices[face.bl] + sm.vertices[face.br] + sm.vertices[face.tl] + sm.vertices[face.tr]) * 0.25f;
	}

}

//*********************************************************************************************************//
//*********************************************************************************************************//
//*********************************************************************************************************//


struct ShaderProgramSource
{
	std::string VertexSource;
	std::string TessellationControlSource;
	std::string TessellationEvalSource;
	std::string GeometrySource;
	std::string FragmentSource;
};

struct MeshData
{
	std::vector<float> vertices;
	std::vector<unsigned int> indices;
	unsigned int vertexCount;
	unsigned int indexCount;
};

std::vector<glm::vec3> CastOnTemplateRight()
{
	return
	{
		{0.375f, -0.375f, yarnRadius},
		{0.3125f, 0.0f, yarnRadius},
		{0.25f, 0.25f, yarnRadius},
		{0.0f, 0.5f, -yarnRadius},
		{-0.5f, 0.35f, -yarnRadius},

	};
};

std::vector<glm::vec3> CastOnTemplateLeft()
{
	return
	{
		{0.585f, -0.375f, yarnRadius},
		{0.695f, 0.0f, yarnRadius},
		{0.72f, 0.25f, yarnRadius},
		{0.5f, 0.5f, yarnRadius},
		{0.0f, 0.5f, yarnRadius * 2.0f},
		{-0.5f, 0.5f, yarnRadius * 2.0f},

	};
};

std::vector<glm::vec3> CastOnTemplateLoop()
{
	return
	{
		{1.5f, 0.4f, yarnRadius * 2.0f},
		{1.0f, 0.5f, yarnRadius * 2.0f},
		{0.55f, 0.4f, yarnRadius * 3.0f},
		{0.25f, 0.25f, yarnRadius * 3.0f},
		{0.1f, 0.1f, yarnRadius * 2.5f},

		{0.1f, 0.1f, -yarnRadius * 1.5f},
		{0.25f, 0.25f, -yarnRadius * 2.0f},
		{0.55f, 0.5f, -yarnRadius * 1.5f},
		{1.0f, 0.5f, -yarnRadius},
		{1.5f, 0.35f, -yarnRadius},
	};
};

std::vector<glm::vec3> CastOnTemplateStart()
{
	return
	{
		{1.5f, 0.5f, yarnRadius * 2.0f},
		{1.0f, 0.5f, yarnRadius * 2.0f},
		{0.65f, 0.4f, yarnRadius * 2.0f},
		{0.6f, 0.25f, yarnRadius * 2.0f},
		{0.6f, 0.0f, yarnRadius},

		{0.6f, 0.0f, -yarnRadius},


		{0.6f, 0.0f, -yarnRadius * 2.0f},
		{0.6f, 0.25f, -yarnRadius * 2.0f},
		{0.65f, 0.5f, -yarnRadius * 2.0f},
		{1.0f, 0.5f, -yarnRadius},
		{1.5f, 0.35f, -yarnRadius},
	};
};

std::vector<glm::vec3> BindOffTemplateOver()
{
	return
	{
		{0.30f, 1.25f, -yarnRadius},
		{0.3125f, 1.0f, yarnRadius},
		{0.5f, 0.6f, -yarnRadius},
		{1.0f, 0.5f, 0.0f},
		{1.5f, 0.6f, 0.0f},
	};
};

//Tying it off when it's at the end
std::vector<glm::vec3> BindOffTemplateOverTied()
{
	return
	{
		{0.30f, 1.25f, -yarnRadius},
		{0.3125f, 1.0f, yarnRadius},
		{0.5f, 0.6f, yarnRadius},
		{1.0f, 0.5f, 0.0f},
	};
};

std::vector<glm::vec3> BindOffTemplateUnder()
{
	return
	{
		{0.70f, 1.25f, yarnRadius},
		{0.6875f, 1.0f, yarnRadius},
		{0.5f, 0.6f, yarnRadius},
		{0.0f, 0.5f, 0.0f},
		{-0.5f, 0.6f, 0.0f},
	};
};


//Tying it off when it's at the end
std::vector<glm::vec3> BindOffTemplateUnderTied()
{
	return
	{
		{0.70f, 1.25f, yarnRadius},
		{0.6875f, 1.0f, yarnRadius},
		{0.5f, 0.6f, -yarnRadius},
		{0.0f, 0.5f, 0.0f},
	};
};

std::vector<glm::vec3> LeftSelvageTemplateTop()
{
	return
	{

		{1.1875f, 0.3f, -yarnRadius},
		{1.0f, 0.4f, -yarnRadius},
		{0.7f, 0.3f, -yarnRadius},
		{0.6f, 0.0f, -yarnRadius},
		{0.7f, -0.4f, -yarnRadius},
	};
};

std::vector<glm::vec3> LeftSelvageTemplateBot()
{
	return
	{
		{0.7f, 1.4f, -yarnRadius},
		{0.6f, 1.0f, -yarnRadius},
		{0.7f, 0.5f, -yarnRadius},
		{1.0f, 0.4f, -yarnRadius},
		{1.1875f, 0.5f, -yarnRadius},
	};
};

std::vector<glm::vec3> LeftSelvageTemplateBotFinal()
{
	return
	{
		{0.7f, 1.4f, -yarnRadius},
		{0.6f, 1.0f, -yarnRadius},
		{0.7f, 0.6f, -yarnRadius},
		{1.0f, 0.5f, 0.0f},
		{1.1875f, 0.6f, 0.0f},
	};
};


std::vector<glm::vec3> RightSelvageTemplateTop()
{
	return
	{
		{-0.1875f, 0.3f, -yarnRadius},
		{0.0f, 0.4f, -yarnRadius},
		{0.3f, 0.3f, -yarnRadius},
		{0.4f, 0.0f, -yarnRadius},
		{0.3f, -0.4f, -yarnRadius},
	};
};

std::vector<glm::vec3> RightSelvageTemplateBot()
{
	return
	{
		{0.3f, 1.4f, -yarnRadius},
		{0.4f, 1.0f, -yarnRadius},
		{0.3f, 0.5f, -yarnRadius},
		{0.0f, 0.4f, -yarnRadius},
		{-0.1875f, 0.5f, -yarnRadius},
	};
};

std::vector<glm::vec3> RightSelvageTemplateBotFinal()
{
	return
	{
		{0.3f, 1.4f, -yarnRadius},
		{0.4f, 1.0f, -yarnRadius},
		{0.3f, 0.6f, -yarnRadius},
		{0.0f, 0.5f, 0.0f},
		{-0.1875f, 0.6f, 0.0f},
	};
};

std::vector<glm::vec3> KnitTemplate()
{
	float close = 0.001f;
	glm::vec3 c1left = { 0.1875f,  0.4375f, yarnRadius }; //c1 from the knot based paper
	glm::vec3 c0left = { 0.3375f, 0.855f, -yarnRadius }; //c0 from the same paper

	glm::vec3 kleft = (c1left + c0left) / 2.0f; //the k from the same paper currently unused

	glm::vec3 L1 = { 0.125f, 0.625f, 0.0f };
	glm::vec3 L1dir = glm::normalize(kleft - L1);

	glm::vec3 L2 = { 0.1875f, 0.75f, -yarnRadius };
	glm::vec3 L2dir = glm::normalize(kleft - L2);

	if (glm::distance(L1, kleft) > yarnRadius)
	{
		L1 += L1dir * close;
	}

	if (glm::distance(L2, kleft) > yarnRadius)
	{
		L2 += L2dir * close;
	}

	glm::vec3 c0right = { 0.6625f, 0.855f, -yarnRadius };
	glm::vec3 c1right = { 0.8125f, 0.4375, yarnRadius };

	glm::vec3 kright = (c0right + c1right) / 2.0f; //the k from the same paper currently unused

	glm::vec3 R1 = { 0.8125f, 0.75, -yarnRadius };
	glm::vec3 R1dir = glm::normalize(kright - R1);

	glm::vec3 R2 = { 0.875f,  0.625f, 0.0f };
	glm::vec3 R2dir = glm::normalize(kright - R2);

	if (glm::length(R1 - kright) > yarnRadius)
	{
		R1 += R1dir * close;
	}

	if (glm::length(R2 - kright) > yarnRadius)
	{
		R2 += R2dir * close;
	}

	return
	{
		{0.375f, -0.375f, yarnRadius},

		{0.3125f, 0.0f, yarnRadius},
		{0.25f, 0.25f, yarnRadius},
		c1left, //c1 LEFT
		L1,
		L2,
		c0left, //c0 LEFT
		{0.5f,  0.875f, -yarnRadius},
		c0right, //c0 RIGHT
		R1,
		R2,
		c1right, //c1 RIGHT
		{0.75f, 0.25f,  yarnRadius},
		{0.6875f, 0.0f, yarnRadius},

		{0.625f, -0.375f, yarnRadius},
	};
};

std::vector<glm::vec3> PurlTemplate1() //LEFT couldn't be bothered to change the names so there ya go loool
{
	float close = 0.001f;

	glm::vec3 c0 = { 0.3375f, 0.855f, yarnRadius };
	glm::vec3 c1 = { 0.1875f, 0.4375f, -yarnRadius };
	glm::vec3 k = (c1 + c0) / 2.0f;

	glm::vec3 L = { 0.375f, 0.625f, 0.0f };
	glm::vec3 Ldir = glm::normalize(k - L);

	if (glm::length(L - k) > yarnRadius)
	{
		L += Ldir * close;
	}

	return
	{
		{0.25f, 1.25f, yarnRadius}, //from the top
		{0.3125f, 1.0f, yarnRadius},
		c0,
		L,
		c1,
		{0.0f, 0.4f, -yarnRadius}, //to the left
		{-0.1875f, 0.4375f, -yarnRadius},

	};
};

std::vector<glm::vec3> PurlTemplate2() //RIGHT ngk couldn't be bothered to swtich that #2 to just right
{
	float close = 0.001f;

	glm::vec3 c0 = { 0.6625f, 0.855f, yarnRadius };
	glm::vec3 c1 = { 0.8125f, 0.4375f, -yarnRadius };
	glm::vec3 k = (c1 + c0) / 2.0f;

	glm::vec3 R = { 0.625f, 0.625f, 0.0f };
	glm::vec3 Rdir = glm::normalize(k - R);


	if (glm::length(R - k) > yarnRadius)
	{
		R += Rdir * close;
	}

	return
	{
		{0.75f, 1.25f, yarnRadius}, //from the top
		{0.6875f, 1.0f, yarnRadius},
		c0,
		R,
		c1,
		{1.0f, 0.4f, -yarnRadius}, //to the right
		{1.1875f, 0.4375f, -yarnRadius},
	};
};

MeshData ConvertMeshToVertexData(const cyTriMesh& mesh)
{
	MeshData data;

	bool hasNormals = mesh.HasNormals();
	bool hasTexCoords = mesh.HasTextureVertices();

	unsigned int nFaces = mesh.NF();

	//layout
	data.vertices.reserve(nFaces * 3 * 8);

	//build vertex data per face
	for (unsigned int i = 0; i < nFaces; i++)
	{
		cyTriMesh::TriFace face = mesh.F(i);

		//process all 3 vertices of this triangle
		for (int j = 0; j < 3; j++)
		{
			unsigned int vertexIndex = face.v[j];

			//position
			cy::Vec3f pos = mesh.V(vertexIndex);
			data.vertices.push_back(pos.x);
			data.vertices.push_back(pos.y);
			data.vertices.push_back(pos.z);

			//normals
			if (hasNormals)
			{
				unsigned int normalIndex;

				if (mesh.HasNormals() && mesh.FN(i).v[j] < mesh.NVN())
				{
					normalIndex = mesh.FN(i).v[j];
				}
				else
				{
					normalIndex = vertexIndex;
				}

				if (normalIndex < mesh.NVN())
				{
					cy::Vec3f normal = mesh.VN(normalIndex);
					data.vertices.push_back(normal.x);
					data.vertices.push_back(normal.y);
					data.vertices.push_back(normal.z);
				}
				else
				{
					data.vertices.push_back(0.0f);
					data.vertices.push_back(1.0f);
					data.vertices.push_back(0.0f);
				}
			}
			else
			{
				data.vertices.push_back(0.0f);
				data.vertices.push_back(1.0f);
				data.vertices.push_back(0.0f);
			}

			// textures
			if (hasTexCoords)
			{
				unsigned int texIndex;

				if (mesh.HasTextureVertices() && mesh.FT(i).v[j] < mesh.NVT())
				{
					texIndex = mesh.FT(i).v[j];
				}

				else
				{
					texIndex = vertexIndex;
				}

				if (texIndex < mesh.NVT())
				{
					cy::Vec3f texCoord = mesh.VT(texIndex);
					data.vertices.push_back(texCoord.x);
					data.vertices.push_back(texCoord.y);
				}
				else
				{
					data.vertices.push_back(0.0f);
					data.vertices.push_back(0.0f);
				}
			}
			else
			{
				data.vertices.push_back(0.0f);
				data.vertices.push_back(0.0f);
			}
		}
	}

	data.vertexCount = nFaces * 3;
	return data;
}


unsigned int LoadCubeMap(std::vector<std::string> faces)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int w, h, nrChannels;

	for (unsigned int i = 0; i < faces.size(); i++)
	{
		unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &nrChannels, 0);

		if (data)
		{
			GLenum format;

			if (nrChannels == 3)
				format = GL_RGB;

			else if (nrChannels == 4)
				format = GL_RGBA;

			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}

		else
		{
			std::cout << "Cubemap tex failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}

	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	return textureID;
}

static ShaderProgramSource ParseShader(const std::string& filepath)
{
	std::ifstream stream(filepath);

	enum class ShaderType
	{
		NONE = -1, VERTEX = 0, TESSCONTROL = 1, TESSEVAL = 2, GEOMETRY = 3, FRAGMENT = 4,
	};

	std::string line;
	std::stringstream ss[5];
	ShaderType type = ShaderType::NONE;

	while (getline(stream, line))
	{
		if (line.find("#shader") != std::string::npos)
		{
			if (line.find("vertex") != std::string::npos)
				type = ShaderType::VERTEX;

			else if (line.find("geometry") != std::string::npos)
				type = ShaderType::GEOMETRY;

			else if (line.find("fragment") != std::string::npos)
				type = ShaderType::FRAGMENT;

			else if (line.find("tessellation control") != std::string::npos)
				type = ShaderType::TESSCONTROL;

			else if (line.find("tessellation eval") != std::string::npos)
				type = ShaderType::TESSEVAL;
		}

		else
			ss[(int)type] << line << "\n";
	}

	return { ss[0].str(), ss[1].str(), ss[2].str(), ss[3].str(), ss[4].str() };
}

bool LEFTMOUSE = false;
bool CONTROL = false;

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	float speed = 5.0f;

	if (QUAD)
		targetQuadCameraPos += speed * (float)yoffset * cameraFront;

	else
		targetCameraPos += speed * (float)yoffset * cameraFront;
}

void mouseCallback(GLFWwindow* window, double xposin, double yposin)
{
	float xpos = static_cast<float>(xposin);
	float ypos = static_cast<float>(yposin);

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; //backwards for vert
	lastX = xpos;
	lastY = ypos;

	float sensitivity = 0.01f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	if (QUAD)
	{
		quadYaw += xoffset;
		quadPitch += yoffset;
		quadPitch = glm::clamp(quadPitch, -89.0f, 89.0f);

		glm::vec3 front;
		front.x = cos(glm::radians(quadYaw)) * cos(glm::radians(quadPitch));
		front.y = sin(glm::radians(quadPitch));
		front.z = sin(glm::radians(quadYaw)) * cos(glm::radians(quadPitch));
		quadCameraFront = glm::normalize(front);
	}

	else if (CONTROL)
	{
		float lightSensitivity = 20.0f;
		lightTheta += xoffset * lightSensitivity;
		lightPhi += yoffset * lightSensitivity;

		if (lightPhi > 360.0f)
			lightPhi -= 360.0f;

		if (lightPhi < 0.0f)
			lightPhi += 360.0f;

		if (lightTheta > 360.0f)
			lightTheta -= 360.0f;

		if (lightTheta < 0.0f)
			lightTheta += 360.0f;

		lightPos.x = meshCenter.x + lightRadius * sin(glm::radians(lightPhi)) * cos(glm::radians(lightTheta));
		lightPos.y = meshCenter.y + lightRadius * cos(glm::radians(lightPhi));
		lightPos.z = meshCenter.z + lightRadius * sin(glm::radians(lightPhi)) * sin(glm::radians(lightTheta));
	}

	else
	{
		yaw += xoffset;
		pitch += yoffset;
		pitch = glm::clamp(pitch, -89.0f, 89.0f);

		glm::vec3 front;
		front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		front.y = sin(glm::radians(pitch));
		front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		cameraFront = glm::normalize(front);
	}
}


void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		std::cout << "PRESS" << std::endl;
		LEFTMOUSE = true;
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	{
		std::cout << "RELEASE" << std::endl;
		LEFTMOUSE = false;
	}
}

void processInput(GLFWwindow* window)
{
	static bool ALT = false;
	if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS)
	{
		if (!ALT)
		{
			QUAD = !QUAD;
			UImode = !UImode;

			if (UImode)
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

			else if (!UImode)
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

			ALT = true;

		}
	}

	else
	{
		ALT = false;
	}

	static bool F5pressed = false;
	if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS)
	{
		if (!F5pressed)
		{
			takeScreenshot = true;
			F5pressed = true;
		}
	}

	else
	{
		F5pressed = false;
	}

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, true);
	}

	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
	{
		CONTROL = true;
	}

	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_RELEASE)
	{
		CONTROL = false;
	}

	if (QUAD)
	{
		float speed = 2.0f  * deltaTime;

		if (glfwGetKey(window, GLFW_KEY_E))
			quadCameraPos += speed * quadCameraFront;

		if (glfwGetKey(window, GLFW_KEY_Q))
			quadCameraPos -= speed * quadCameraFront;

		if (glfwGetKey(window, GLFW_KEY_W))
			quadCameraPos += glm::normalize(quadCameraUp) * speed;

		if (glfwGetKey(window, GLFW_KEY_S))
			quadCameraPos -= glm::normalize(quadCameraUp) * speed;

		if (glfwGetKey(window, GLFW_KEY_A))
			quadCameraPos -= glm::normalize(glm::cross(quadCameraFront, quadCameraUp)) * speed;

		if (glfwGetKey(window, GLFW_KEY_D))
			quadCameraPos += glm::normalize(glm::cross(quadCameraFront, quadCameraUp)) * speed;
	}

	else
	{
		float speed = 10.0f * deltaTime;
		if (glfwGetKey(window, GLFW_KEY_E))
			targetCameraPos += speed * cameraFront;

		if (glfwGetKey(window, GLFW_KEY_Q))
			targetCameraPos -= speed * cameraFront;

		if (glfwGetKey(window, GLFW_KEY_W))
			targetCameraPos += glm::normalize(cameraUp) * speed;

		if (glfwGetKey(window, GLFW_KEY_S))
			targetCameraPos -= glm::normalize(cameraUp) * speed;

		if (glfwGetKey(window, GLFW_KEY_A))
			targetCameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;

		if (glfwGetKey(window, GLFW_KEY_D))
			targetCameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
	}
}

enum class MorphType { None = 0, BendX, BendY, Stretch, Twist, Sphere, Shear };

void MorphStitchMesh(StitchMesh& sm, DualGraph& dg, MorphType type, float amount)
{
	// Find the mesh's Bounding box or just max it out
	glm::vec3 bboxMin(FLT_MAX), bboxMax(-FLT_MAX);
	glm::vec3 DGbboxMin(FLT_MAX), DGbboxMax(-FLT_MAX);

	for (auto& v : sm.vertices)
	{
		bboxMin = glm::min(bboxMin, v);
		bboxMax = glm::max(bboxMax, v);
	}

	for (auto& u : dg.nodes)
	{
		DGbboxMin = glm::min(DGbboxMin, u);
		DGbboxMax = glm::max(DGbboxMax, u);
	}

	glm::vec3 center = (bboxMin + bboxMax) * 0.5f;
	glm::vec3 size = bboxMax - bboxMin;

	glm::vec3 DGcenter = (DGbboxMin + DGbboxMax) * 0.5f;
	glm::vec3 DGsize = DGbboxMax - DGbboxMin;

	for (auto& v : sm.vertices)
	{
		// Normalized [0,1] coordinates within the mesh
		float nx = (size.x > 0.f) ? (v.x - bboxMin.x) / size.x : 0.f;
		float ny = (size.y > 0.f) ? (v.y - bboxMin.y) / size.y : 0.f;

		switch (type)
		{
		case MorphType::BendX:
		{
			// turn into a can
			float angle = (ny - 0.5f) * amount; // RADIANS
			float radius = (size.y / glm::max(glm::abs(amount), 0.0001f));
			float newY = radius * sin(angle);
			float newZ = v.z + radius * (cos(angle) - 1.0f);
			v.y = newY;
			v.z = newZ;
			break;
		}

		case MorphType::BendY:
		{
			// turn into a can
			float angle = (nx - 0.5f) * amount; // RADIANS
			float radius = (size.x / glm::max(glm::abs(amount), 0.0001f));
			float newX = radius * sin(angle);
			float newZ = v.z + radius * (cos(angle) - 1.0f);
			v.x = newX;
			v.z = newZ;
			break;
		}

		case MorphType::Stretch:
		{
			//Stretch horizontally
			v.x = center.x + (v.x - center.x) * amount;
			break;
		}

		case MorphType::Twist:
		{
			// Twist
			float angle = ny * amount;
			float cosA = cos(angle), sinA = sin(angle);
			float cx = center.x, cz = center.z;
			float dx = v.x - cx, dz = v.z - cz;
			v.x = cx + dx * cosA - dz * sinA;
			v.z = cz + dx * sinA + dz * cosA;
			break;
		}

		case MorphType::Sphere:
		{
			// Wrap the flat mesh onto a ball
			float theta = (nx - 0.5f) * amount; // horizontal angle
			float phi = (ny - 0.5f) * amount;  // vertical angle
			float r = size.x / glm::max(glm::abs(amount), 0.0001f);
			v.x = r * sin(theta) * cos(phi);
			v.y = r * sin(phi);
			v.z = r * (cos(theta) * cos(phi) - 1.0f);
			break;
		}

		case MorphType::Shear:
		{
			// Bottom moves left, top moves right.
			float offset = (ny - 0.5f) * amount;

			v.x += offset;

			break;
		}

		break;

		}
	}

	for (auto& u : dg.nodes)
	{
		// Normalized [0,1] coordinates within the mesh
		float nx = (DGsize.x > 0.f) ? (u.x - DGbboxMin.x) / DGsize.x : 0.f;
		float ny = (DGsize.y > 0.f) ? (u.y - DGbboxMin.y) / DGsize.y : 0.f;

		switch (type)
		{
		case MorphType::BendX:
		{
			// turn into a can
			float angle = (ny - 0.5f) * amount; // RADIANS
			float radius = (DGsize.y / glm::max(glm::abs(amount), 0.0001f));
			float newY = radius * sin(angle);
			float newZ = u.z + radius * (cos(angle) - 1.0f);
			u.y = newY;
			u.z = newZ;
			break;
		}

		case MorphType::BendY:
		{
			// turn into a can
			float angle = (nx - 0.5f) * amount; // RADIANS
			float radius = (size.x / glm::max(glm::abs(amount), 0.0001f));
			float newX = radius * sin(angle);
			float newZ = u.z + radius * (cos(angle) - 1.0f);
			u.x = newX;
			u.z = newZ;
			break;
		}

		case MorphType::Stretch:
		{
			//Stretch horizontally
			u.x = DGcenter.x + (u.x - DGcenter.x) * amount;
			break;
		}

		case MorphType::Twist:
		{
			// Twist
			float angle = ny * amount;
			float cosA = cos(angle), sinA = sin(angle);
			float cx = DGcenter.x, cz = DGcenter.z;
			float dx = u.x - cx, dz = u.z - cz;
			u.x = cx + dx * cosA - dz * sinA;
			u.z = cz + dx * sinA + dz * cosA;
			break;
		}

		case MorphType::Sphere:
		{
			// Wrap the flat mesh onto a ball
			float theta = (nx - 0.5f) * amount; // horizontal angle
			float phi = (ny - 0.5f) * amount;  // vertical angle
			float r = size.x / glm::max(glm::abs(amount), 0.0001f);
			u.x = r * sin(theta) * cos(phi);
			u.y = r * sin(phi);
			u.z = r * (cos(theta) * cos(phi) - 1.0f);
			break;
		}

		case MorphType::Shear:
		{
			// Bottom moves left, top moves right.
			float offset = (ny - 0.5f) * amount;

			u.x += offset;

			break;
		}

		break;

		}
	}
}

// ============================================================================================================================ //
// ============================================== COLLECT YARN CENTERLINE CURVES ============================================== //

void CollectYarnCurves(StitchMesh& sm, std::vector<std::vector<glm::vec3>>& yarnCurves)
{
	yarnCurves.clear();

	auto SampleTemplateCurve = [&](const std::vector<glm::vec3>& templ, glm::vec3 bl, glm::vec3 br, glm::vec3 tl, glm::vec3 tr)
	{
		glm::vec3 faceNormal = glm::normalize(glm::cross(tr - bl, tl - br));
		std::vector<glm::vec3> ctrl;
		for (auto& tp : templ)
		{
			float u = tp.x, v = 1.0f - tp.y; // needed v to be flipped (1 -tp.y) for the correct orientation of the yarn curves
			glm::vec3 pos = (1 - u) * (1 - v) * tl + u * (1 - v) * tr + u * v * br + (1 - u) * v * bl;
			pos += faceNormal * tp.z;
			ctrl.push_back(pos);
		}
		std::vector<glm::vec3> pts;
		for (int i = 1; i < (int)ctrl.size() - 2; i++)
		{
			glm::vec3 p0 = ctrl[i - 1], p1 = ctrl[i], p2 = ctrl[i + 1], p3 = ctrl[i + 2];
			bool isLast = (i == (int)ctrl.size() - 3);
			int end = isLast ? SPLINESAMPLES : SPLINESAMPLES - 1;
			for (int s = 0; s < end; s++)
			{
				float t = float(s) / float(SPLINESAMPLES - 1);
				pts.push_back(CatmullRom(p0, p1, p2, p3, t));
			}
		}
		if (pts.size() >= 2)
			yarnCurves.push_back(pts);
	};

	auto knitTemp = KnitTemplate();
	auto purlTemp1 = PurlTemplate1();
	auto purlTemp2 = PurlTemplate2();

	auto castOnLeft = CastOnTemplateLeft();
	auto castOnRight = CastOnTemplateRight();
	auto castOnLoop = CastOnTemplateLoop();
	auto castOnStart = CastOnTemplateStart();

	auto bindOffOver = BindOffTemplateOver();
	auto bindOffUnder = BindOffTemplateUnder();
	auto bindOffOverTied = BindOffTemplateOverTied();
	auto bindOffUnderTied = BindOffTemplateUnderTied();

	auto selvageLeftTop = LeftSelvageTemplateTop();
	auto selvageLeftBot = LeftSelvageTemplateBot();
	auto selvageRightTop = RightSelvageTemplateTop();
	auto selvageRightBot = RightSelvageTemplateBot();
	auto selvageCloseLeft = LeftSelvageTemplateBotFinal();
	auto selvageCloseRight = RightSelvageTemplateBotFinal();

	for (int idx = 0; idx < (int)sm.faces.size(); idx++)
	{
		auto& face = sm.faces[idx];
		glm::vec3 bl = sm.vertices[face.bl];
		glm::vec3 br = sm.vertices[face.br];
		glm::vec3 tl = sm.vertices[face.tl];
		glm::vec3 tr = sm.vertices[face.tr];

		// slight change so borders match mirror
		int col = idx % sm.nCols;
		int row = idx / sm.nCols;
		int nRow = (int)sm.faces.size() / sm.nCols;
		int rrow = (nRow - 1) - row;
		int ridx = rrow * sm.nCols + col;
		bool evenRow = (rrow % 2 == 0);

		// updated to change idx -> ridx for the selvage curves to match the mirror
		if (ridx < sm.nCols - 1)
		{
			if (ridx == 0)
				SampleTemplateCurve(castOnStart, bl, br, tl, tr);
			else
			{
				SampleTemplateCurve(castOnLeft, bl, br, tl, tr);
				SampleTemplateCurve(castOnRight, bl, br, tl, tr);
				if (ridx != sm.nCols - 2)
					SampleTemplateCurve(castOnLoop, bl, br, tl, tr);
			}
		}
		else if (ridx == (int)sm.faces.size() - 1 && evenRow)
		{
			SampleTemplateCurve(selvageCloseRight, bl, br, tl, tr);
		}
		else if (ridx == (int)sm.faces.size() - sm.nCols)
		{
			if (!evenRow)
				SampleTemplateCurve(selvageCloseLeft, bl, br, tl, tr);
			else
				continue;
		}
		else if (ridx == (int)sm.faces.size() - 1 || ridx == sm.nCols - 1)
		{
			continue;
		}
		else if (ridx > (int)sm.faces.size() - sm.nCols)
		{
			if (ridx == (int)sm.faces.size() - 2 && !evenRow)
				SampleTemplateCurve(bindOffOverTied, bl, br, tl, tr);
			else
				SampleTemplateCurve(bindOffOver, bl, br, tl, tr);

			if (ridx == (int)sm.faces.size() - sm.nCols + 1 && evenRow)
				SampleTemplateCurve(bindOffUnderTied, bl, br, tl, tr);
			else
				SampleTemplateCurve(bindOffUnder, bl, br, tl, tr);
		}
		else if (col == 0)
		{
			SampleTemplateCurve(evenRow ? selvageLeftTop : selvageLeftBot, bl, br, tl, tr);
		}
		else if (col == sm.nCols - 1)
		{
			SampleTemplateCurve(!evenRow ? selvageRightTop : selvageRightBot, bl, br, tl, tr);
		}
		else
		{
			SampleTemplateCurve(knitTemp, bl, br, tl, tr);
			SampleTemplateCurve(purlTemp1, bl, br, tl, tr);
			SampleTemplateCurve(purlTemp2, bl, br, tl, tr);
		}
	}
}
// ============================================== EXPORT: SMOBJ ============================================== //
void ExportSMOBJ(const StitchMesh& sm, const std::string& path)
{
	std::ofstream out(path);

	if (!out)
	{
		std::cout << "ExportSMOBJ: failed to open " << path << std::endl;
		return;
	}

	out << "# exported from Artakha knit sim (fully relaxed state)\n";

	//I think this was noting out the counter clockwise ordering and/or the "library" of faces we are making.
	out << "L knit -l +y +l -y\n";

	//literally just v and the vertex coordinates of my relaxed nodes
	for (auto& v : sm.vertices)
	{
		out << "v " << v.x << " " << v.y << " " << v.z << "\n";
	}

	//reordering faces b/c smobj uses a counter clockwise winding. Literally just rewriting since my stichmesh does Z (bl,br,tl,tr)
	//count 1, 2, 3, 4 and indexing originally started at 0 soooo +1 it is
	for (auto& f : sm.faces)
	{
		out << "f " << (f.bl + 1) << " " << (f.br + 1) << " " << (f.tr + 1) << " " << (f.tl + 1) << "\n";
	}

	//Not sure but we'll see.  A type from the library but I think they're in the order listed at the top???
	for (size_t i = 0; i < sm.faces.size(); i++)
	{
		out << "T 1\n";
	}

	//N is optional so I'm skipping that

	//was annoying so just did this to retrieve face indices
	auto FaceIndex = [&](int r, int c) 
	{ 
		return r * sm.nCols + c; 
	};

	for (int r = 0; r < sm.nRows; r++)
	{
		for (int c = 0; c < sm.nCols; c++)
		{
			int idx = FaceIndex(r, c);

			if (r + 1 < sm.nRows)
			{
				int up = FaceIndex(r + 1, c);
				out << "e " << (idx + 1) << "/3 " << (up + 1) << "/-1\n";
			}

			if (c + 1 < sm.nCols)
			{
				int right = FaceIndex(r, c + 1);
				out << "e " << (idx + 1) << "/2 " << (right + 1) << "/-4\n";
			}
		}
	}

	out.close();

	std::cout << "Wrote " << path << " (" << sm.vertices.size() << " verts, " << sm.faces.size() << " faces)" << std::endl;
}


// ============================================== EXPORT: BCC (yarn curves) ============================================== //
//   bytes 0-2   "BCC"
//   byte  3     0x44  (4-byte int, 4-byte float)
//   bytes 4-5   "C0"  (Catmull-Rom, uniform parameterization)
//   byte  6     Number of dimensions(dimensions)
//   byte  7     up-axis index (0=x,1=y,2=z)
//   bytes 8-15  curve count      (uint64_t)
//   bytes 16-23 total point count (uint64_t)
//   bytes 24-63 40-byte ascii info string
//
// then per curve: int32 pointCount (positive = open), followed by that many (x,y,z) float32 triples.

void ExportBCC(const std::vector<std::vector<glm::vec3>>& yarnCurves, const std::string& path)
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
	{
		std::cout << "ExportBCC: failed to open " << path << std::endl;
		return;
	}

	uint64_t curveCount = 0;
	uint64_t pointCount = 0;

	for (auto& c : yarnCurves)
	{
		if (c.size() < 2) continue; // skip degenerate curves
		curveCount++;
		pointCount += c.size();
	}

	char header[64];
	memset(header, 0, sizeof(header));
	header[0] = 'B';
	header[1] = 'C';
	header[2] = 'C';
	header[3] = (char)0x44;// 4-byte int / 4-byte float precision
	header[4] = 'C';
	header[5] = '0'; // Catmull-Rom, uniform parameterization
	header[6] = 3; // 3 dimensions
	header[7] = 1; // up = y. If for some reason y and z are swapped, make this 2.

	memcpy(header + 8, &curveCount, sizeof(uint64_t));
	memcpy(header + 16, &pointCount, sizeof(uint64_t));

	const char* info = "Artakha knit sim export";
	strncpy(header + 24, info, 40);

	out.write(header, 64);

	for (auto& c : yarnCurves)
	{
		if (c.size() < 2) continue;

		int32_t n = (int32_t)c.size(); // positive => open curve (not a closed loop)
		out.write(reinterpret_cast<const char*>(&n), sizeof(int32_t));

		for (auto& p : c)
		{
			float xyz[3] = { p.x, p.y, p.z };
			out.write(reinterpret_cast<const char*>(xyz), sizeof(xyz));
		}
	}

	out.close();
	std::cout << "Wrote " << path << " (" << curveCount << " curves, " << pointCount << " points)" << std::endl;
}

// ============================================== EXPORT: SMOBJ (stitch mesh OBJ) ============================================== //
void WriteSMobj(const StitchMesh& sm, const std::string& path)
	{
		std::ofstream out(path);
		if (!out)
		{
			std::cout << "WriteSMobj: failed to open " << path << std::endl;
			return;
		}

		// classify face idx
		auto faceType = [&](int idx) -> int
			{
				int col = idx % sm.nCols;
				int row = idx / sm.nCols;
				bool evenRow = (row % 2 == 0);
				int nF = (int)sm.faces.size();
				if (idx < sm.nCols - 1) return 1;                   // bottom row: cast-on -> tuck-twist-
				if (idx == nF - 1 && evenRow) return 4;             // selvage close right -> edge)
				if (idx == nF - sm.nCols) return 2;                 // selvage close left  -> edge(
				if (idx == nF - 1 || idx == sm.nCols - 1) return 2; // skipped corners     -> edge(
				if (idx > nF - sm.nCols) return 6;                  // top row: bind-off   -> drop
				if (col == 0) return 2;                             // left selvage        -> edge(
				if (col == sm.nCols - 1) return 4;                  // right selvage       -> edge)
				return evenRow ? 3 : 5;                             // interior knit+ / knit-
			};

		out << "# exported from Artakha knit sim (stitch mesh, relaxed state)\n";
		// type library
		out << "L tuck-twist- -lX -yX +lX +yX\n";   // 1  cast-on (bottom row)
		out << "L edge( x -y1 +y1 x x\n";           // 2  left selvage / corners
		out << "L knit+ -lX +yX +lX -yX\n";         // 3  interior, even row
		out << "L edge) x x x +y1 -y1\n";           // 4  right selvage
		out << "L knit- -lX -yX +lX +yX\n";         // 5  interior, odd row
		out << "L drop -l1 +y0 +l0 -y0\n";          // 6  bind-off (top row)

		for (const auto& p : sm.vertices)
			out << "v " << p.x << " " << p.y << " " << p.z << "\n";

		// winding bl -> br -> tr -> tl
		for (const auto& f : sm.faces)
			out << "f " << (f.bl + 1) << " " << (f.br + 1) << " " << (f.tr + 1) << " " << (f.tl + 1) << "\n";

		for (int i = 0; i < (int)sm.faces.size(); i++)
			out << "T " << faceType(i) << "\n";

		out.close();
		std::cout << "Wrote " << path << " (" << sm.vertices.size() << " verts, " << sm.faces.size() << " faces, typed)" << std::endl;
	}
// ============================================= EXPORT DONE ==============================================

// ============================================== RELAX TO CONVERGE + EXPORTING ============================================== //
// Runs relaxation on a *copy* of sm/dg so it doesn't disturb whatever you're looking at interactively, iterates until the largest per-vertex position
// change between steps drops below `tol` (or maxIters is hit), then writes.

void ExportFullyRelaxedKnit(StitchMesh sm, DualGraph dg, float timeStep, float kStretch, float kShear, float kWale, float kernelSpring, float boundSpring, float eShear, float eBend, float eSlide, float rCourse, float rWale, bool useNeighborAware, const std::string& smobjPath, const std::string& bccPath, int maxIters = 2000, float tol = 1e-5f)
{
	for (int iter = 0; iter < maxIters; iter++)
	{
		std::vector<glm::vec3> prev = sm.vertices;

		if (useNeighborAware)
			RelaxNeighbor(sm, dg, timeStep, kernelSpring, boundSpring, eShear, eBend, eSlide, rCourse, rWale);

		else
			Relax(sm, dg, timeStep, kStretch, kShear, kWale, rCourse, rWale);

		float maxDelta = 0.0f;
		for (size_t v = 0; v < sm.vertices.size(); v++)
		{
			maxDelta = glm::max(maxDelta, glm::length(sm.vertices[v] - prev[v]));
		}

		if (maxDelta < tol)
		{
			std::cout << "ExportFullyRelaxedKnit: converged after " << iter
				<< " iterations (max vertex delta " << maxDelta << ")" << std::endl;
			break;
		}

		if (iter == maxIters - 1)
			std::cout << "ExportFullyRelaxedKnit: hit maxIters (" << maxIters << ") without reaching tol=" << tol << " -- result may not be fully settled." << std::endl;
	}

	std::filesystem::create_directories("output");
	WriteSMobj(sm, smobjPath);
	std::vector<std::vector<glm::vec3>> yarnCurves;
	CollectYarnCurves(sm, yarnCurves);
	ExportSMOBJ(sm, smobjPath);
	ExportBCC(yarnCurves, bccPath);
}
// ============================================== EXPORT DONE ============================================== //


int main(int argc, char* argv[])
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4); //Very simple Antialiasing. 
	
	GLFWwindow* window = glfwCreateWindow(height, width, "Artakha Renderer", NULL, NULL);

	if (window == NULL)
	{
		std::cout << "FAILED IDIOT" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);

	if (UImode)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	else
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	
	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Guess you aren't GLAD" << std::endl;
		return -1;
	}

	float cubemapVertices[] = {
		//positions          //normals              //tex coords
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
		-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,

		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
		-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,

		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
		 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
		 1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
		 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
		 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
		-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
		 1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
		 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
		 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
		-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f
	};

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);
	//glEnable(GL_FRAMEBUFFER_SRGB);

	//==============================// LOAD OBJ FILE //==============================//
	cyTriMesh mesh;
	const char* objFilePath;
	
	if (argc > 1)
	{
		objFilePath = argv[1];
	}
	
	else
	{
		objFilePath = "res/models/teapot.obj";
	}


	if (!mesh.LoadFromFileObj(objFilePath, true))
	{
		std::cout << "Failed to load mesh: " << objFilePath << std::endl;
		return -1;
	}

	// Compute normals if doesn't exist
	if (!mesh.HasNormals())
	{
		mesh.ComputeNormals();
	}

	mesh.ComputeBoundingBox();

	cy::Vec3f bbMin = mesh.GetBoundMin();
	cy::Vec3f bbMax = mesh.GetBoundMax();
	cy::Vec3f bbCenter = (bbMin + bbMax) * 0.5f;
	meshCenter = glm::vec3(bbCenter.x, bbCenter.y, bbCenter.z);
	lightPos = glm::vec3(meshCenter.x, meshCenter.y - 10.0f, meshCenter.z + 25.0f);

	std::cout << "Loaded mesh with " << mesh.NV() << " vertices and " << mesh.NF() << " faces" << std::endl;

	//Self-explanatory
	MeshData meshData = ConvertMeshToVertexData(mesh);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//++++++++++++++++++++++++++++++ STITCH MESH INITIALIZATION ++++++++++++++++++++++++++++++//

	std::vector<float> yarnVertices;
	std::vector<unsigned int> yarnIndices;
	glm::vec3 yarnColor = glm::vec3(0.0f, 0.545f, 0.255f);

	//The Stitch Mesh initialization
	float stepSize = 0.1f;
	float kStretch = 5.0f;
	float kShear = 0.2f;
	float kWale = 2.0f;
	StitchMesh sm;
	DualGraph dg;

	//Neighbor-Aware parameters initialization
	float kernelSpring = 2.0f;
	float boundSpring = 1.0f;
	float eShear = 2.0f;
	float eBend = 30.0f;
	float eSlide = 5.0f;

	VertexArray yarnVAO;
	VertexBuffer yarnVBO(yarnVertices.data(), yarnVertices.size() * sizeof(float));
	IndexBuffer yarnEBO(yarnIndices.data(), yarnIndices.size() * sizeof(unsigned int));

	VertexBufferLayout yarnLayout;
	std::string yarnPath = "res/shaders/yarn.shader";
	ShaderProgramSource yarnSource = ParseShader(yarnPath);
	Shader yarnShader(yarnSource.VertexSource, yarnSource.TessellationControlSource, yarnSource.TessellationEvalSource, yarnSource.GeometrySource, yarnSource.FragmentSource);
	yarnShader.Bind();
	yarnLayout.Push<float>(3);
	yarnLayout.Push<float>(3);
	yarnLayout.Push<float>(2);
	yarnVAO.AddBuffer(yarnLayout);

	yarnVAO.Bind();
	yarnEBO.Bind();

	yarnVAO.Unbind();
	yarnEBO.Unbind();
	yarnVBO.Unbind();
	yarnShader.Unbind();

	float yarnWidth = 7.0f;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//

//=====================================// SKY BOX //====================================//
	const char* skyboxNames[] = { "Sea", "Yokohama", "Yokohama Hotel", "Arsta Bridge", "Studio"};
	std::vector<std::vector<std::string>> skyboxFaces =
	{
		{"res/textures/sea/right.jpg",
		"res/textures/sea/left.jpg",
		"res/textures/sea/top.jpg",
		"res/textures/sea/bottom.jpg",
		"res/textures/sea/front.jpg",
		"res/textures/sea/back.jpg"},

		{"res/textures/yokohama/right.jpg",
		"res/textures/yokohama/left.jpg",
		"res/textures/yokohama/top.jpg",
		"res/textures/yokohama/bottom.jpg",
		"res/textures/yokohama/front.jpg",
		"res/textures/yokohama/back.jpg"},

		{"res/textures/Yokohama Hotel/right.jpg",
		"res/textures/Yokohama Hotel/left.jpg",
		"res/textures/Yokohama Hotel/top.jpg",
		"res/textures/Yokohama Hotel/bottom.jpg",
		"res/textures/Yokohama Hotel/front.jpg",
		"res/textures/Yokohama Hotel/back.jpg"},

		{"res/textures/ArstaBridge/right.jpg",
		"res/textures/ArstaBridge/left.jpg",
		"res/textures/ArstaBridge/top.jpg",
		"res/textures/ArstaBridge/bottom.jpg",
		"res/textures/ArstaBridge/front.jpg",
		"res/textures/ArstaBridge/back.jpg"},

		{"res/textures/Studio/right.png",
		"res/textures/Studio/left.png",
		"res/textures/Studio/top.png",
		"res/textures/Studio/bottom.png",
		"res/textures/Studio/front.png",
		"res/textures/Studio/back.png"}
	};

	std::vector<unsigned int> skyboxTextures;
	for (auto& faces : skyboxFaces)
		skyboxTextures.push_back(LoadCubeMap(faces));

	int currentCubemap = 0;
	unsigned int cubemapTexture = skyboxTextures[currentCubemap];
	VertexArray cubemapVAO;
	VertexBuffer cubemapVBO(cubemapVertices, sizeof(cubemapVertices));
	VertexBufferLayout cubemapLayout;
	std::string cubemapPath = "res/shaders/cubemap.shader";
	ShaderProgramSource cubemapSource = ParseShader(cubemapPath);
	Shader cubemapShader(cubemapSource.VertexSource, cubemapSource.TessellationControlSource, cubemapSource.TessellationEvalSource, cubemapSource.GeometrySource, cubemapSource.FragmentSource);
	cubemapShader.Bind();
	cubemapLayout.Push<float>(3);
	cubemapLayout.Push<float>(3);
	cubemapLayout.Push<float>(2);
	cubemapVAO.AddBuffer(cubemapLayout);
	cubemapVBO.Unbind();
	cubemapVAO.Unbind();
	cubemapShader.Unbind();

	//==============================// SHADOWMAPPING STUFF (FBO from light's perspective) //==============================//
	const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);

	unsigned int depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL); //THIRTY Two bits
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	std::string shadowPath = "res/shaders/shadow.shader";
	ShaderProgramSource shadowSource = ParseShader(shadowPath);
	Shader shadowShader(shadowSource.VertexSource, shadowSource.TessellationControlSource, shadowSource.TessellationEvalSource, shadowSource.GeometrySource, shadowSource.FragmentSource);
	shadowShader.Bind();

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return false;

	//=========================================// LIGHT BLOCK //=========================================//
	VertexArray lightVAO;
	//VertexBuffer lightVBO(meshData.vertices.data(), meshData.vertices.size() * sizeof(float));
	VertexBuffer lightVBO(cubemapVertices, sizeof(cubemapVertices));

	VertexBufferLayout lightLayout;
	std::string lightShaderPath = "res/shaders/light.shader";
	ShaderProgramSource lightSource = ParseShader(lightShaderPath);
	Shader lightShader(lightSource.VertexSource, lightSource.TessellationControlSource, lightSource.TessellationEvalSource, lightSource.GeometrySource, lightSource.FragmentSource);
	lightShader.Bind();

	lightLayout.Push<float>(3);
	lightLayout.Push<float>(3);
	lightLayout.Push<float>(2);
	lightVAO.AddBuffer(lightLayout);
	lightVBO.Unbind();
	lightVAO.Unbind();
	lightShader.Unbind();

	//------------------------------INITIALIZING IMGUI---------------------------------//
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 460");

	auto RegenerateKnit = [&]()
	{
		yarnVertices.clear();
		yarnIndices.clear();

		int nRows = iRows + 2;
		int nCols = iCols + 2;

		BuildStitchMesh(sm, nRows, nCols, stitchHeight, stitchWidth);
		BuildDualGraph(sm, dg);

		float rCourse = restLengthCourse;
		float rWale = restLengthWale;

		MorphStitchMesh(sm, dg, (MorphType)morphType, morphAmount);

		auto SampleTemplate = [&](const std::vector<glm::vec3>& templ, glm::vec3 bl, glm::vec3 br, glm::vec3 tl, glm::vec3 tr, float row, float fixRotation)
		{
			glm::vec3 faceNormal = glm::normalize(glm::cross(tr - bl, tl - br));
			std::vector<glm::vec3> ctrl;

			for (auto& tp : templ)
			{
				float u = tp.x, v = tp.y;
				glm::vec3 pos = (1 - u) * (1 - v) * tl + u * (1 - v) * tr + u * v * br + (1 - u) * v * bl;
				pos += faceNormal * tp.z;
				ctrl.push_back(pos);
			}

			std::vector<glm::vec3> pts, tans;
			for (int i = 1; i < (int)ctrl.size() - 2; i++)
			{
				glm::vec3 p0 = ctrl[i - 1], p1 = ctrl[i], p2 = ctrl[i + 1], p3 = ctrl[i + 2];
				bool isLast = (i == (int)ctrl.size() - 3);
				int end = isLast ? SPLINESAMPLES : SPLINESAMPLES - 1;
				for (int s = 0; s < end; s++)
				{
					float t = float(s) / float(SPLINESAMPLES - 1);
					pts.push_back(CatmullRom(p0, p1, p2, p3, t));
					tans.push_back(CatmullRomTangent(p0, p1, p2, p3, t));
				}
			}

			GenerateTube(pts, tans, yarnRadius, sides, yarnVertices, yarnIndices, row, fixRotation);
		};

		auto knitTemp = KnitTemplate();
		auto purlTemp1 = PurlTemplate1();
		auto purlTemp2 = PurlTemplate2();

		auto castOnLeft = CastOnTemplateLeft();
		auto castOnRight = CastOnTemplateRight();
		auto castOnLoop = CastOnTemplateLoop();
		auto castOnStart = CastOnTemplateStart();

		auto bindOffOver = BindOffTemplateOver();
		auto bindOffUnder = BindOffTemplateUnder();
		auto bindOffOverTied = BindOffTemplateOverTied();
		auto bindOffUnderTied = BindOffTemplateUnderTied();

		auto selvageLeftTop = LeftSelvageTemplateTop();
		auto selvageLeftBot = LeftSelvageTemplateBot();
		auto selvageRightTop = RightSelvageTemplateTop();
		auto selvageRightBot = RightSelvageTemplateBot();

		auto selvageCloseLeft = LeftSelvageTemplateBotFinal();
		auto selvageCloseRight = RightSelvageTemplateBotFinal();


		for (int idx = 0; idx < (int)sm.faces.size(); idx++)
		{
			auto& face = sm.faces[idx];

			glm::vec3 bl = sm.vertices[face.bl], br = sm.vertices[face.br];
			glm::vec3 tl = sm.vertices[face.tl], tr = sm.vertices[face.tr];

			int col = idx % sm.nCols;
			int row = idx / sm.nCols;
			bool evenRow = (row % 2 == 0);

			float rowColoring = row;

			//Cast Ons on the first row
			if (idx < sm.nCols - 1)
			{
				if (idx == 0)
					SampleTemplate(castOnStart, bl, br, tl, tr, rowColoring, 360.0f);

				else
				{
					SampleTemplate(castOnLeft, bl, br, tl, tr, rowColoring, 360.0f);
					SampleTemplate(castOnRight, bl, br, tl, tr, rowColoring, 360.0f);

					if (idx != sm.nCols - 2)
						SampleTemplate(castOnLoop, bl, br, tl, tr, rowColoring, 360.0f);
				}

			}



			//Covers the FINAL selvages on both the right and left sides
			else if (idx == (int)sm.faces.size() - 1 && evenRow)
			{
				SampleTemplate(selvageCloseRight, bl, br, tl, tr, rowColoring, 360.0f);
			}

			else if (idx == (int)sm.faces.size() - sm.nCols)
			{
				if (!evenRow)
					SampleTemplate(selvageCloseLeft, bl, br, tl, tr, rowColoring, 360.0f);

				else
					continue;
			}

			//skips two corners since they won't be holding anything (for now)
			else if (idx == (int)sm.faces.size() - 1 || idx == sm.nCols - 1)
			{
				continue;
			}


			//Bind Offs
			else if (idx > (int)sm.faces.size() - sm.nCols)
			{
				if (idx == (int)sm.faces.size() - 2 && !evenRow)
					SampleTemplate(bindOffOverTied, bl, br, tl, tr, rowColoring, 0.0f);

				else
					SampleTemplate(bindOffOver, bl, br, tl, tr, rowColoring, 0.0f);

				if (idx == (int)sm.faces.size() - sm.nCols + 1 && evenRow)
					SampleTemplate(bindOffUnderTied, bl, br, tl, tr, rowColoring, 0.0f);

				else
					SampleTemplate(bindOffUnder, bl, br, tl, tr, rowColoring, 0.0f);
			}

			//Selvage Left
			else if (col == 0)
			{
				if (evenRow)
					SampleTemplate(selvageLeftTop, bl, br, tl, tr, rowColoring, 360.0f);
				else
					SampleTemplate(selvageLeftBot, bl, br, tl, tr, rowColoring, 360.0f);
			}

			//Selvage Right
			else if (col == sm.nCols - 1)
			{
				if (!evenRow)
					SampleTemplate(selvageRightTop, bl, br, tl, tr, rowColoring, 360.0f);


				else
					SampleTemplate(selvageRightBot, bl, br, tl, tr, rowColoring, 360.0f);
			}

			//The actual stitch mesh
			else
			{
				SampleTemplate(knitTemp, bl, br, tl, tr, rowColoring, 360.0f);
				SampleTemplate(purlTemp1, bl, br, tl, tr, rowColoring, 0.0f);
				SampleTemplate(purlTemp2, bl, br, tl, tr, rowColoring, 0.0f);
			}
		}

		yarnVBO.Bind();
		glBufferData(GL_ARRAY_BUFFER, yarnVertices.size() * sizeof(float), yarnVertices.data(), GL_DYNAMIC_DRAW);
		yarnVBO.Unbind();

		yarnEBO.Bind();
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, yarnIndices.size() * sizeof(unsigned int), yarnIndices.data(), GL_DYNAMIC_DRAW);
		yarnEBO.Unbind();
	};

	RegenerateKnit();

	//WIREFRAME
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	
	while (!glfwWindowShouldClose(window))
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		float fps = 1.0f / deltaTime;
		float lerpSpeed = 10.0f;
		fov = glm::mix(fov, targetFov, deltaTime * lerpSpeed);
		quadfov = glm::mix(quadfov, targetQuadfov, deltaTime * lerpSpeed);

		cameraPos = glm::mix(cameraPos, targetCameraPos, deltaTime * lerpSpeed);
		quadCameraPos = glm::mix(quadCameraPos, targetQuadCameraPos, deltaTime * lerpSpeed);

		lightPos.x = meshCenter.x + lightRadius * sin(glm::radians(lightPhi)) * cos(glm::radians(lightTheta));
		lightPos.y = meshCenter.y + lightRadius * cos(glm::radians(lightPhi));
		lightPos.z = meshCenter.z + lightRadius * sin(glm::radians(lightPhi)) * sin(glm::radians(lightTheta));

		processInput(window);

		//=========================== SETUP FOR SHADOWS FROM LIGHT POV ===========================//
		glm::mat4 lightProjection = glm::ortho(-35.0f, 35.0f, -35.0f, 35.0f, shadowNearPlane, shadowFarPlane);
		glm::mat4 lightView = glm::lookAt(lightPos, meshCenter, cameraUp);
		glm::mat4 lightSpaceMatrix = lightProjection * lightView;

		
		//================================= ORIGINAL TPOT RENDER =================================//
		glViewport(0, 0, height, width);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);

		glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		glm::mat4 projection = glm::perspective(glm::radians(fov), float(height) / float(width), nearPlane, farPlane);

		//==================================== RENDER SKYBOX ====================================//
		glDepthMask(GL_FALSE);
		glDepthFunc(GL_LEQUAL);
		cubemapShader.Bind();

		glm::mat4 cubemapView = glm::mat4(glm::mat3(view));
		glm::mat4 cubemapProjection = glm::perspective(glm::radians(45.0f), float(height) / float(width), nearPlane, farPlane);;
		cubemapShader.SetUniformMat4f("view", cubemapView);
		cubemapShader.SetUniformMat4f("projection", cubemapProjection);
		cubemapVAO.Bind();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
		cubemapShader.SetUniform1i("skybox", 0);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);

		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, meshCenter);
		model = glm::translate(model, glm::vec3(0.0f, 100.0f, 0.0f));
		//model = glm::rotate(model, glm::radians(60.0f), glm::vec3(-1.0f, 0.0f, -0.5f));
		model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
		glm::mat4 normalMat = transpose(inverse(model));

	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	//+++++++++++++++++++++++++++++ YEARN FOR THE YARN +++++++++++++++++++++++++++++++//
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//

		yarnShader.Bind();
		yarnVAO.Bind();

		float rCourse = restLengthCourse;
		float rWale = restLengthWale;

		yarnVertices.clear();
		yarnIndices.clear();
		
		auto SampleTemplate = [&](const std::vector<glm::vec3>& templ, glm::vec3 bl, glm::vec3 br, glm::vec3 tl, glm::vec3 tr, float row, float fixRotation)
		{
			glm::vec3 faceNormal = glm::normalize(glm::cross(tr - bl, tl - br));

			std::vector<glm::vec3> ctrl;
			for (auto& tp : templ)
			{
				float u = tp.x, v = tp.y;
				glm::vec3 pos = (1 - u) * (1 - v) * tl + u * (1 - v) * tr + u * v * br + (1 - u) * v * bl;
				pos += faceNormal * tp.z;
				ctrl.push_back(pos);
			}

			std::vector<glm::vec3> pts, tans;
			for (int i = 1; i < (int)ctrl.size() - 2; i++)
			{
				glm::vec3 p0 = ctrl[i - 1], p1 = ctrl[i], p2 = ctrl[i + 1], p3 = ctrl[i + 2];
				bool isLast = (i == (int)ctrl.size() - 3);
				int end = isLast ? SPLINESAMPLES : SPLINESAMPLES - 1;
				for (int s = 0; s < end; s++)
				{
					float t = float(s) / float(SPLINESAMPLES - 1);
					pts.push_back(CatmullRom(p0, p1, p2, p3, t));
					tans.push_back(CatmullRomTangent(p0, p1, p2, p3, t));
				}
			}

			GenerateTube(pts, tans, yarnRadius, sides, yarnVertices, yarnIndices, row, fixRotation);
		};

		if (showMesh)
		{
			// push flat quads directly from stitch mesh vertices
			for (auto& face : sm.faces)
			{
				glm::vec3 bl = sm.vertices[face.bl];
				glm::vec3 br = sm.vertices[face.br];
				glm::vec3 tl = sm.vertices[face.tl];
				glm::vec3 tr = sm.vertices[face.tr];

				glm::vec3 n = glm::normalize(glm::cross(tr - bl, tl - br));

				unsigned int base = yarnVertices.size() / 8;
				auto pushVert = [&](glm::vec3 p, glm::vec2 uv)
				{
					yarnVertices.push_back(p.x);
					yarnVertices.push_back(p.y);
					yarnVertices.push_back(p.z);
					yarnVertices.push_back(n.x);
					yarnVertices.push_back(n.y);
					yarnVertices.push_back(n.z);
					yarnVertices.push_back(uv.y);
					yarnVertices.push_back(uv.x);
				};

				pushVert(bl, { 0,0 });
				pushVert(br, { 1,0 });
				pushVert(tl, { 0,1 });
				pushVert(tr, { 1,1 });

				// two triangles per quad
				yarnIndices.push_back(base + 0);
				yarnIndices.push_back(base + 1);
				yarnIndices.push_back(base + 2);
				yarnIndices.push_back(base + 1);
				yarnIndices.push_back(base + 3);
				yarnIndices.push_back(base + 2);
			}
		}

		else
		{
			auto knitTemp = KnitTemplate();
			auto purlTemp1 = PurlTemplate1();
			auto purlTemp2 = PurlTemplate2();

			auto selvageLeftTop = LeftSelvageTemplateTop();
			auto selvageLeftBot = LeftSelvageTemplateBot();
			auto selvageRightTop = RightSelvageTemplateTop();
			auto selvageRightBot = RightSelvageTemplateBot();

			auto selvageCloseLeft = LeftSelvageTemplateBotFinal();
			auto selvageCloseRight = RightSelvageTemplateBotFinal();

			auto castOnLeft = CastOnTemplateLeft();
			auto castOnRight = CastOnTemplateRight();
			auto castOnLoop = CastOnTemplateLoop();
			auto castOnStart = CastOnTemplateStart();

			auto bindOffOver = BindOffTemplateOver();
			auto bindOffUnder = BindOffTemplateUnder();
			auto bindOffOverTied = BindOffTemplateOverTied();
			auto bindOffUnderTied = BindOffTemplateUnderTied();

			for (int idx = 0; idx < (int)sm.faces.size(); idx++)
			{
				auto& face = sm.faces[idx];

				glm::vec3 bl = sm.vertices[face.bl], br = sm.vertices[face.br];
				glm::vec3 tl = sm.vertices[face.tl], tr = sm.vertices[face.tr];

				int col = idx % sm.nCols;
				int row = idx / sm.nCols;
				bool evenRow = (row % 2 == 0);
				float rowColoring = row / (sm.nRows - 1.0f);

				//Cast Ons on the first row
				if (idx < sm.nCols - 1)
				{
					if (idx == 0)
						SampleTemplate(castOnStart, bl, br, tl, tr, rowColoring, 360.0f);

					else
					{
						SampleTemplate(castOnLeft, bl, br, tl, tr, rowColoring, 360.0f);
						SampleTemplate(castOnRight, bl, br, tl, tr, rowColoring, 360.0f);

						if (idx != sm.nCols - 2)
							SampleTemplate(castOnLoop, bl, br, tl, tr, rowColoring, 360.0f);
					}

				}

				//Covers the FINAL selvages on both the right and left sides
				else if (idx == (int)sm.faces.size() - 1 && evenRow)
				{
					SampleTemplate(selvageCloseRight, bl, br, tl, tr, rowColoring, 360.0f);
				}

				else if (idx == (int)sm.faces.size() - sm.nCols)
				{
					if (!evenRow)
						SampleTemplate(selvageCloseLeft, bl, br, tl, tr, rowColoring, 360.0f);

					else
						continue;
				}

				//skips two corners since they won't be holding anything (for now)
				else if (idx == (int)sm.faces.size() - 1 || idx == sm.nCols - 1)
				{
					continue;
				}


				//Bind Offs
				else if (idx > (int)sm.faces.size() - sm.nCols)
				{
					if (idx == (int)sm.faces.size() - 2 && !evenRow)
						SampleTemplate(bindOffOverTied, bl, br, tl, tr, rowColoring, 0.0f);

					else
						SampleTemplate(bindOffOver, bl, br, tl, tr, rowColoring, 0.0f);
					
					if (idx == (int)sm.faces.size() - sm.nCols + 1 && evenRow)
						SampleTemplate(bindOffUnderTied, bl, br, tl, tr, rowColoring, 0.0f);

					else
						SampleTemplate(bindOffUnder, bl, br, tl, tr, rowColoring, 0.0f);
				}

				//Selvage Left
				else if (col == 0)
				{
					if (evenRow)
						SampleTemplate(selvageLeftTop, bl, br, tl, tr, rowColoring, 360.0f);
					else
						SampleTemplate(selvageLeftBot, bl, br, tl, tr, rowColoring, 360.0f);
				}

				//Selvage Right
				else if (col == sm.nCols - 1)
				{
					if (!evenRow)
						SampleTemplate(selvageRightTop, bl, br, tl, tr, rowColoring, 360.0f);


					else
						SampleTemplate(selvageRightBot, bl, br, tl, tr, rowColoring, 360.0f);
				}
				
				//The actual stitch mesh
				else
				{
					SampleTemplate(knitTemp, bl, br, tl, tr, rowColoring, 360.0f);
					SampleTemplate(purlTemp1, bl, br, tl, tr, rowColoring, 0.0f);
					SampleTemplate(purlTemp2, bl, br, tl, tr, rowColoring, 0.0f);
				}


			}
		}

		if (showDual)
		{
			for (auto& node : dg.nodes)
			{
				float s = 0.1f;  // cross size
				unsigned int base = yarnVertices.size() / 8;

				auto pushPoint = [&](glm::vec3 p)
				{
					yarnVertices.push_back(p.x); yarnVertices.push_back(p.y);
					yarnVertices.push_back(p.z);
					yarnVertices.push_back(0); yarnVertices.push_back(1);
					yarnVertices.push_back(0); // normal up
					yarnVertices.push_back(0.5f); yarnVertices.push_back(0.5f);
				};

				pushPoint(node + glm::vec3(-s, -s, 0));
				pushPoint(node + glm::vec3(s, -s, 0));
				pushPoint(node + glm::vec3(-s, s, 0));
				pushPoint(node + glm::vec3(s, s, 0));

				yarnIndices.push_back(base + 0); yarnIndices.push_back(base + 1);
				yarnIndices.push_back(base + 2);
				yarnIndices.push_back(base + 1); yarnIndices.push_back(base + 3);
				yarnIndices.push_back(base + 2);
			}
		}


		yarnVBO.Bind();
		glBufferData(GL_ARRAY_BUFFER, yarnVertices.size() * sizeof(float), yarnVertices.data(), GL_DYNAMIC_DRAW);
		yarnVBO.Unbind();

		yarnEBO.Bind();
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, yarnIndices.size() * sizeof(unsigned int), yarnIndices.data(), GL_DYNAMIC_DRAW);
		//yarnEBO.Unbind();

		glm::mat4 yarnModel = glm::mat4(1.0f);
		yarnModel = glm::translate(yarnModel, meshCenter);
		yarnModel = glm::rotate(yarnModel, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)); //180 to have it on the "upright"
		//yarnModel = glm::rotate(yarnModel, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); //-90 to have it on the "floor"

		glm::mat4 yarnNormalMat = transpose(inverse(yarnModel));

		yarnShader.SetUniform1f("GAMMA", GAMMA);
		yarnShader.SetUniformMat4f("normalMat", yarnNormalMat);
		yarnShader.SetUniformMat4f("model", yarnModel);
		yarnShader.SetUniformMat4f("view", view);
		yarnShader.SetUniformMat4f("projection", projection);
		yarnShader.SetUniform3v("yarnColor", yarnColor);

		yarnShader.SetUniform3v("cameraPos", cameraPos);
		yarnShader.SetUniform3v("lightPos", lightPos);
		yarnShader.SetUniform3v("lightColor", lightColor);
		yarnShader.SetUniformMat4f("lightSpaceMatrix", lightSpaceMatrix);

		yarnShader.SetUniform1i("colorByRow", colorByRow);

		//When I eventually add the Fiber level rendering portion. These should be the parameters according to the paper minus the LoD part.
		//yarnShader.SetUniform1i("nFiberMax", 30);
		//yarnShader.SetUniform1i("subdivisions", SPLINESAMPLES);
		//yarnShader.SetUniform1f("alpha", 1.0f);
		//yarnShader.SetUniform1f("Rmin", yarnRadius);
		//yarnShader.SetUniform1f("Rmax", yarnRadius + 0.05f);
		//yarnShader.SetUniform1f("RmaxLoop", yarnRadius + 0.1);
		//yarnShader.SetUniform1i("nPly", 8);
		//yarnShader.SetUniform1f("eN", 1.0f);
		//yarnShader.SetUniform1f("eB", 1.0f);
		//yarnShader.SetUniform1f("yarnFiberRadius", 0.01f);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		yarnShader.SetUniform1i("depthMap", 2);

		glDrawElements(GL_TRIANGLES, (GLsizei)yarnIndices.size(), GL_UNSIGNED_INT, 0);

	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//

		//================================ Rendering Light Sources ================================//
		lightShader.Bind();
		lightVAO.Bind();
		glm::mat4 lightModel = glm::translate(i4, lightPos);
		lightModel = glm::rotate(lightModel, glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		//lightModel = glm::scale(lightModel, glm::vec3(0.1f, 0.1f, 0.1f));
		lightModel = glm::scale(lightModel, glm::vec3(1.0f, 1.0f, 1.0f));

		lightShader.SetUniformMat4f("view", view);
		lightShader.SetUniformMat4f("projection", projection);
		lightShader.SetUniform3v("lightColor", lightColor);

		glm::mat4 lightNormalMat = transpose(inverse(lightModel));
		lightShader.SetUniformMat4f("model", lightModel);

		//glDrawArrays(GL_TRIANGLES, 0, meshData.vertexCount);
		glDrawArrays(GL_TRIANGLES, 0, 36);

		//===================================== SHADOW MAPPING ======================================//
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, depthMapFBO);
		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glClear(GL_DEPTH_BUFFER_BIT);
		shadowShader.Bind();
		shadowShader.SetUniformMat4f("lightSpaceMatrix", lightSpaceMatrix);
		shadowShader.SetUniformMat4f("model", model);
		glDrawArrays(GL_TRIANGLES, 0, meshData.vertexCount);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		//====================================== GUI STUFF ========================================//
		bool p_open = false;
		ImGui::Begin("I know where you live", &p_open);// ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::Text("FPS: %.1f", fps);

		if (ImGui::Combo("Skybox", &currentCubemap, skyboxNames, 5))
		{
			cubemapTexture = skyboxTextures[currentCubemap];
		}

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		ImGui::SliderFloat("Gamma Correction", &GAMMA, 0.0f, 3.0f);

		if (ImGui::CollapsingHeader("Lighting"))
		{
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
			ImGui::InputFloat3("Light Position", glm::value_ptr(lightPos), "%.2f", ImGuiInputTextFlags_ReadOnly);
			ImGui::PopStyleColor();
			ImGui::ColorEdit3("Light Color", glm::value_ptr(lightColor));
			ImGui::SliderFloat("Theta", &lightTheta, 0.0f, 360.0f, NULL);
			ImGui::SliderFloat("Phi", &lightPhi, 0.0f, 360.0f, NULL);
			ImGui::NewLine();
		}

		bool yarnHack = false;

		if (ImGui::Button("Regenerate Knit"))
		{
			RegenerateKnit();
		}

		static int currentMethod = 0;

		if (ImGui::Button("Export Relaxed (smobj + bcc)"))
		{
			ExportFullyRelaxedKnit(sm, dg,stepSize, kStretch, kShear, kWale, kernelSpring, boundSpring, eShear, eBend, eSlide, rCourse, rWale, currentMethod == 1, "output/relaxed_stitch.smobj", "output/relaxed_yarn.bcc");
		}

		ImGui::Checkbox("Show Mesh", &showMesh);
		ImGui::Checkbox("Show Dual Graph", &showDual);

		ImGui::Checkbox("Color by Row", &colorByRow);

		const char* methods[] = { "Original", "Neighbor-Aware", "Pause"};
		ImGui::Combo("Relaxation Methods", &currentMethod, methods, IM_ARRAYSIZE(methods));
		switch (currentMethod)
		{
		case 0:
			Relax(sm, dg, stepSize, kStretch, kShear, kWale, rCourse, rWale);
			break;

		case 1:
			RelaxNeighbor(sm, dg, stepSize, kernelSpring, boundSpring, eShear, eBend, eSlide, rCourse, rWale);
			//Relax(sm, dg, stepSize, kStretch, kShear, kWale, rCourse, rWale);
			break;

		case 2:
			break;
		}

		if (ImGui::SliderFloat("Yarn Radius", &yarnRadius, 0.01f, 0.1f))
			yarnHack = true;

		if (ImGui::SliderInt("Rows", &iRows, 1, 64))
			yarnHack = true;

		if (ImGui::SliderInt("Columns", &iCols, 1, 64))
			yarnHack = true;


		if (ImGui::SliderFloat("Stitch Width", &stitchWidth, 0.1f, 2.0f))
			yarnHack = true;
		if (ImGui::SliderFloat("Stitch Height", &stitchHeight, 0.1f, 2.0f))
			yarnHack = true;

		ImGui::NewLine();
		ImGui::SliderFloat("Rest Length (Course)", &restLengthCourse, 0.05f, 2.0f);
		ImGui::SliderFloat("Rest Length (Wale)", &restLengthWale, 0.05f, 2.0f);


		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Original Parameters"))
		{
			if (ImGui::SliderFloat("shear", &kShear, 0.0f, 5.0f))
				yarnHack = true;

			if (ImGui::SliderFloat("stretch", &kStretch, 0.0f, 10.0f))
				yarnHack = true;
		}

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Neighbor-Aware Parameters"))
		{
			if (ImGui::SliderFloat("Kernel Spring", &kernelSpring, 0.0f, 5.0f))
				yarnHack = true;

			if (ImGui::SliderFloat("Boundary Spring", &boundSpring, 0.0f, 5.0f))
				yarnHack = true;

			if (ImGui::SliderFloat("Shear", &eShear, 0.0f, 5.0f))
				yarnHack = true;

			if (ImGui::SliderFloat("Bend", &eBend, 0.0f, 50.0f))
				yarnHack = true;

			if (ImGui::SliderFloat("Slide", &eSlide, 0.0f, 20.0f))
				yarnHack = true;
		}

		ImGui::ColorEdit3("Yarn Color", glm::value_ptr(yarnColor));
		//ImGui::SliderFloat("Yarn Width", &yarnWidth, 10.0f, 25.0f, NULL);

		ImGui::NewLine();

		const char* morphNames[] = { "None", "BendX", "BendY", "Stretch", "Twist", "Sphere", "Shear"};

		ImGui::Separator();
		ImGui::Text("Morph");

		if (ImGui::Combo("Morph Type", &morphType, morphNames, IM_ARRAYSIZE(morphNames)))
			yarnHack = true;

		if (morphType != 0)
		{
			if (ImGui::SliderFloat("Morph Amount", &morphAmount, -10.0f, 10.0f))
				yarnHack = true;
		}

		if (yarnHack)
		{
			yarnHack = false;
			//RegenerateKnit();
		}

		ImGui::End();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (takeScreenshot)
		{
			TakeScreenshot(GetScreenshotFilename(), height, width);
			takeScreenshot = false;
		}

		glfwSwapBuffers(window); //GLFW swap buffers and take i/o events
		glfwPollEvents();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}

