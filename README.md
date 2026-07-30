<div align="center">

# 🧶 ArtakhaV2 🧶

### Real-Time Yarn-Level Knit Simulation

**A real-time OpenGL renderer that builds a stitch mesh, relaxes it under a
knit-mechanics energy model, and generates procedural yarn geometry
(cast-on, knit, purl, selvage, bind-off) using Catmull-Rom splines swept
into tubes. Relaxed swatches can be exported as `.smobj` (augmented
stitch mesh) and `.bcc` (binary curve collection) for use in other
yarn-level cloth tools.**

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL-4.1%20core-5586A4?logo=opengl&logoColor=white)
![GLSL](https://img.shields.io/badge/GLSL-410-5586A4)
![CMake](https://img.shields.io/badge/CMake-3.26%2B-064F8C?logo=cmake&logoColor=white)
![Eigen](https://img.shields.io/badge/Eigen-5.0-8E44AD)
![ImGui](https://img.shields.io/badge/Dear%20ImGui-panel-F26430)
![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows-lightgrey)

</div>

**Contents** — [Features](#features) · [Gallery](#gallery) · [Dependencies](#dependencies) · [Building](#building) · [Controls](#controls) · [Relaxation Solvers](#the-relaxation-solvers) · [Export](#export-smobj--bcc) · [Project layout](#project-layout) · [Acknowledgments](#acknowledgments) · [References](#references)

---

## Features

| *Feature* | Description |
| :--- | :--- |
| **Procedural stitch mesh**<br>![mesh](https://img.shields.io/badge/mesh-27AE60?style=for-the-badge) | configurable rows/columns, course/wale rest lengths, built on a quad grid with a dual graph for neighbor-aware forces. |
| **Two relaxation solvers**<br>![solver](https://img.shields.io/badge/solver-8E44AD?style=for-the-badge) | swappable at runtime:<br>• *Original* — per-vertex Newton step (stretch, shear, wale-bend springs).<br>• *Neighbor-Aware* — global sparse Newton solve over the dual graph (kernel/boundary springs, shear, bend, slide energies) via `Eigen::ConjugateGradient`. |
| **Procedural yarn geometry**<br>![yarn](https://img.shields.io/badge/yarn-BE0000?style=for-the-badge) | per-stitch template curves (knit, purl, cast-on, bind-off, left/right selvage) sampled with Catmull-Rom splines and swept into tubes with parallel-transport frames. |
| **Mesh morphing**<br>![morph](https://img.shields.io/badge/morph-F39C12?style=for-the-badge) | bend, twist, stretch, shear, or wrap the relaxed swatch onto a sphere for garment-shaping previews. |
| **Full render pipeline**<br>![render](https://img.shields.io/badge/render-F26430?style=for-the-badge) | shadow-mapped directional light, 5 selectable skyboxes, gamma correction, live ImGui control panel. |
| **One-click export**<br>![i/o](https://img.shields.io/badge/i%2Fo-546E7A?style=for-the-badge) | relax to convergence and write out `.smobj` + `.bcc` for the current swatch. |

## Gallery

<div align="center">

| Sheared | Solved |
|:---:|:---:|
| <img src="https://github.com/user-attachments/assets/c9c0eebe-151c-4b52-8408-e0debe7028a2" width="480" alt="App screenshot"> | <img src="https://github.com/user-attachments/assets/6988093c-62b1-4767-ad84-9d1658cad503" width="480" alt="App screenshot"> |

</div>

## Dependencies

| *Library* | | Purpose |
| :--- | :---: | :--- |
| [GLFW](https://www.glfw.org/) | ![window](https://img.shields.io/badge/window-1E88E5) | Windowing, input |
| [GLAD](https://glad.dav1d.de/) | ![loader](https://img.shields.io/badge/loader-5586A4) | OpenGL core loader (generated for 3.3, vendored in `external/`) |
| [Dear ImGui](https://github.com/ocornut/imgui) | ![ui](https://img.shields.io/badge/ui-F26430) | Control panel UI |
| [stb_image / stb_image_write](https://github.com/nothings/stb) | ![i/o](https://img.shields.io/badge/i%2Fo-546E7A) | Texture loading, screenshot PNG export |
| [GLM](https://github.com/g-truc/glm) | ![math](https://img.shields.io/badge/math-27AE60) | Math (vectors, matrices, transforms) |
| [Eigen](https://eigen.tuxfamily.org/) 5.0 | ![solve](https://img.shields.io/badge/solve-8E44AD) | Sparse linear solves for the neighbor-aware relaxer |
| [cyCodeBase](https://www.cemyuksel.com/cyCodeBase/) (`cyTriMesh`) | ![mesh](https://img.shields.io/badge/mesh-9E9E9E) | OBJ mesh loading *(unused for now)* |

## Building

Builds on **macOS** and **Windows** from the same `CMakeLists.txt`. Everything is
either vendored in this repo (GLAD, ImGui, GLM, stb) or fetched automatically on
the first configure (GLFW 3.4, Eigen 5.0.0) — there is no per-developer setup, so
a fresh clone builds as-is. The first configure needs an internet connection.

**macOS** (requires CMake 3.26+ and the Xcode command line tools):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/ArtakhaV2
```

**Windows** (Visual Studio 2022): `File → Open → Folder…` and pick this folder.
VS configures from `CMakeLists.txt` automatically, then `Build → Build All`.
Or from the command line:

```sh
cmake -S . -B build
cmake --build build --config Release
```

The build copies `res/` and `imgui.ini` next to the executable, so it runs
correctly regardless of the working directory it is launched from.

> [!NOTE]
> **OpenGL version.** Both platforms target **OpenGL 4.1 core / GLSL 410**. macOS
> caps out at 4.1, and nothing here needs anything newer — so there is one shader
> set and one GLAD loader rather than a per-platform split. Note that Apple has
> deprecated OpenGL; it still works, but will not advance past 4.1.

## Controls

| *Input* | Action | *Input* | Action |
| :---: | :--- | :---: | :--- |
| ![WASD](https://img.shields.io/badge/W%20A%20S%20D-30363D) ![QE](https://img.shields.io/badge/Q%20E-30363D) | Fly camera (move / up / down) | ![Alt](https://img.shields.io/badge/Alt-30363D) | Toggle quad-preview mode + UI cursor mode |
| ![Mouse](https://img.shields.io/badge/Mouse-1E88E5) | Look around | ![Ctrl + mouse drag](https://img.shields.io/badge/Ctrl%20%2B%20mouse%20drag-1E88E5) | Orbit the light around the mesh |
| ![Scroll](https://img.shields.io/badge/Scroll-1E88E5) | Move forward/back along view direction | ![F5](https://img.shields.io/badge/F5-30363D) | Save a screenshot to `screenshots/` |
| ![Esc](https://img.shields.io/badge/Esc-BE0000) | Quit | | |

## The Relaxation Solvers

The stitch mesh starts as a flat grid and is relaxed toward a physically
plausible knit shape:

| *Solver* | Method |
| :--- | :--- |
| **Original** (`Relax`)<br>![local](https://img.shields.io/badge/local-1E88E5?style=for-the-badge) | solves a local per-vertex Newton step using stretch springs (course, wale, and both diagonals) and shear terms across each quad face, plus a wale-bending term across three vertically-stacked rows. |
| **Neighbor-Aware** (`RelaxNeighbor`)<br>![global](https://img.shields.io/badge/global-8E44AD?style=for-the-badge) | builds a full sparse Hessian over the *dual graph* (one node per stitch) with kernel/boundary springs, bilinear shear, a cosine bending energy, and a slide energy, then solves the whole system at once with a conjugate-gradient solver. Vertex positions are recovered from the relaxed dual nodes via a weighted average of the four (or fewer, at boundaries) surrounding stitch centers. |

Both are exposed live in the ImGui panel so you can compare convergence
behavior and tune spring/energy constants per run.

## Export: .smobj / .bcc

Clicking **Export Relaxed (smobj + bcc)** relaxes a *copy* of the current
mesh to convergence (max per-vertex delta below tolerance, or a max
iteration cap), then writes:

| *File* | Contents |
| :--- | :--- |
| [`output/relaxed_stitch.smobj`](output/relaxed_stitch.smobj)<br>![topology](https://img.shields.io/badge/topology-27AE60?style=for-the-badge) | the stitch-mesh topology in the [augmented stitch mesh format](https://github.com/textiles-lab/smobj) (vertices, quad faces, a typed face library distinguishing cast-on, bind-off, left/right selvage and per-course knit direction, and edge-to-edge connectivity derived from the grid's shared vertices). |
| [`output/relaxed_yarn.bcc`](output/relaxed_yarn.bcc)<br>![curves](https://img.shields.io/badge/curves-BE0000?style=for-the-badge) | yarn centerlines in Cem Yuksel's [Binary Curve Collection format](https://www.cemyuksel.com/research/yarnmodels/), sampled per stitch template (knit/purl/cast-on/bind-off/selvage) as open Catmull-Rom curves, up-axis Y. |

> [!NOTE]
> The two files split the work: the `.smobj` carries **topology** (faces, stitch types,
> edge connectivity) and the `.bcc` carries **geometry** (yarn centerlines). Because the
> yarn paths are exported directly, no `.sf` face library is needed to make use of them.
> Note that knitting runs top-to-bottom in these coordinates — cast-on is the highest
> row — so loop edges are inverted relative to `faces/knitout.sf`.

## Project layout

<details open>
<summary><b>📂 Project structure</b> — click to collapse</summary>

<pre>
ArtakhaV2/
├── <a href="README.md">README.md</a>
├── <a href="CMakeLists.txt">CMakeLists.txt</a>          # macOS + Windows build, fetches GLFW + Eigen
├── <a href=".gitignore">.gitignore</a>
├── <a href="imgui.ini">imgui.ini</a>               # ImGui panel layout
├── <a href="external">external/</a>               # vendored GLAD
│   ├── <a href="external/include/glad/glad.h">include/glad/glad.h</a>
│   ├── <a href="external/include/KHR/khrplatform.h">include/KHR/khrplatform.h</a>
│   └── <a href="external/src/glad.c">src/glad.c</a>
├── <a href="res">res/</a>
│   ├── <a href="res/shaders">shaders/</a>            # GLSL 410
│   │   ├── <a href="res/shaders/yarn.shader">yarn.shader</a>
│   │   ├── <a href="res/shaders/light.shader">light.shader</a>
│   │   ├── <a href="res/shaders/shadow.shader">shadow.shader</a>
│   │   └── <a href="res/shaders/cubemap.shader">cubemap.shader</a>
│   ├── <a href="res/models/teapot.obj">models/teapot.obj</a>
│   └── <a href="res/textures">textures/</a>           # 5 skybox cubemaps
├── <a href="output">output/</a>                 # written by Export Relaxed
│   ├── <a href="output/relaxed_stitch.smobj">relaxed_stitch.smobj</a>
│   └── <a href="output/relaxed_yarn.bcc">relaxed_yarn.bcc</a>
└── <a href="src">src/</a>
    ├── <a href="src/Source.cpp">Source.cpp</a>          # app, render loop, solvers, morphing, export
    ├── <a href="src/StitchMesh.h">StitchMesh.h</a>        # stitch mesh + dual graph
    ├── <a href="src/Mesh.h">Mesh.h</a>
    ├── <a href="src/Shader.h">Shader.h</a> / <a href="src/Shader.cpp">.cpp</a>     # shader parse, compile, uniforms
    ├── <a href="src/Texture.h">Texture.h</a> / <a href="src/Texture.cpp">.cpp</a>
    ├── <a href="src/FrameBuffer.h">FrameBuffer.h</a> / <a href="src/FrameBuffer.cpp">.cpp</a> # shadow map target
    ├── <a href="src/VertexArray.h">VertexArray.h</a> / <a href="src/VertexArray.cpp">.cpp</a>
    ├── <a href="src/VertexBuffer.h">VertexBuffer.h</a> / <a href="src/VertexBuffer.cpp">.cpp</a>
    ├── <a href="src/VertexBufferLayout.h">VertexBufferLayout.h</a>
    ├── <a href="src/IndexBuffer.h">IndexBuffer.h</a> / <a href="src/IndexBuffer.cpp">.cpp</a>
    ├── <a href="src/cyCore.h">cyCore.h</a> · <a href="src/cyVector.h">cyVector.h</a> · <a href="src/cyTriMesh.h">cyTriMesh.h</a>
    └── vendor/             # Dear ImGui, GLM, stb
</pre>

</details>

## Acknowledgments

- Yarn-level cloth modeling and the BCC file format:
  [Cem Yuksel — Yarn-Level Cloth Models](https://www.cemyuksel.com/research/yarnmodels/)
- Stitch mesh topology format: [textiles-lab/smobj](https://github.com/textiles-lab/smobj)

## References

1. C. Yuksel et al. (2012).  
   [*Stitch Meshes for Modeling Knitted Clothing with Yarn-Level Detail*](https://doi.org/10.1145/2185520.2185533).  
   ACM Transactions on Graphics, 31(4).

2. Y. Hwang et al. (2025).  
   [*Neighbor-Aware Data-Driven Relaxation of Stitch Mesh Models for Knits*](https://doi.org/10.1145/3757377.3763890).  
   SIGGRAPH Asia 2025 Conference Papers.
