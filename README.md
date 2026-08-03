<div align="center">

# 🧶 ArtakhaV2 🧶
<sub><i>/arr-TOCK-ah/</i></sub>
### Real-Time Yarn-Level Knit Simulation
<img width="1292" height="827" alt="Denoised" src="https://github.com/user-attachments/assets/2adca42b-b22b-4ff5-9963-50608883127b" />

</div>

<div align="left">

**ArtakhaV2** is a real-time yarn-level knit simulation and rendering system. It constructs stitch meshes, relaxes them under knit-mechanics energy models,
and generates procedural yarn geometry by sweeping Catmull-Rom splines into
tubular strands using parallel-transport frames.

Relaxed swatches can be exported as `.smobj` and `.bcc` files for use in
other yarn-level cloth tools.

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL-4.1%20core-5586A4?logo=opengl&logoColor=white)
![GLSL](https://img.shields.io/badge/GLSL-410-5586A4)
![CMake](https://img.shields.io/badge/CMake-3.26%2B-064F8C?logo=cmake&logoColor=white)
![Eigen](https://img.shields.io/badge/Eigen-5.0-8E44AD)
![ImGui](https://img.shields.io/badge/Dear%20ImGui-panel-F26430)
![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows-lightgrey)

</div>

**Contents** — [Features](#features) · [Gallery](#gallery) · [Dependencies](#dependencies) · [Building](#building) · [Command line](#command-line) · [Controls](#controls) · [Relaxation Solvers](#the-relaxation-solvers) · [Export](#export-smobj--bcc) · [Project layout](#project-layout) · [Acknowledgments](#acknowledgments) · [References](#references)

---

## Features

| Feature | Description |
| :--- | :--- |
| **Procedural stitch mesh**<br>![mesh](https://img.shields.io/badge/mesh-27AE60?style=for-the-badge) | Configurable rows/columns, course/wale rest lengths, built on a quad grid with a dual graph for neighbor-aware forces. |
| **Two relaxation solvers**<br>![solver](https://img.shields.io/badge/solver-8E44AD?style=for-the-badge) | Swappable at runtime:<br>• *Original* — Per-vertex Newton step (stretch, shear, wale-bend springs).<br>• *Neighbor-Aware* — Global sparse Newton solve over the dual graph (kernel/boundary springs, shear, bend, slide energies) via `Eigen::ConjugateGradient`. |
| **Procedural yarn geometry**<br>![yarn](https://img.shields.io/badge/yarn-BE0000?style=for-the-badge) | Per-stitch template curves (knit, purl, cast-on, bind-off, left/right selvage) sampled with Catmull-Rom splines and swept into tubes with parallel-transport frames. |
| **Mesh morphing**<br>![morph](https://img.shields.io/badge/morph-F39C12?style=for-the-badge) | Bends, twists, stretches, shears, or wraps the swatch onto a sphere for garment-shaping previews before relaxation. |
| **One-click export**<br>![i/o](https://img.shields.io/badge/i%2Fo-546E7A?style=for-the-badge) | relax to convergence and write out `.smobj` + `.bcc` for the current swatch. |

## Gallery

<div align="center">

| Sheared | Solved |
|:---:|:---:|
| <img src="https://github.com/user-attachments/assets/c9c0eebe-151c-4b52-8408-e0debe7028a2" width="480" alt="App screenshot"> | <img src="https://github.com/user-attachments/assets/6988093c-62b1-4767-ad84-9d1658cad503" width="480" alt="App screenshot"> |

</div>

## Dependencies

| Library | | Purpose |
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

The build copies `res/` next to the executable, so it runs correctly regardless of
the working directory it is launched from.

> [!NOTE]
> Targets **OpenGL 4.1 core / GLSL 410** on both platforms — macOS caps out there, so
> it's one shader set and one GLAD loader rather than a per-platform split.

## Headless Export & Command Line

For Windows users, `buildRun.bat` can also be used to configure, build, and launch the Release executable automatically.

```sh
./buildRun.bat
```

Every tuning parameter in the ImGui panel can also be set from the command line, so a
swatch can be configured — or exported — without touching the UI.

```sh
./build/ArtakhaV2 --rows 16 --cols 16 --rest-course 0.6    # viewer, preconfigured
./build/ArtakhaV2 --rows 16 --cols 16 --export             # relax, write both files, quit
```

| Flag | | Default |
| :--- | :---: | :--- |
| `--export` | ![action](https://img.shields.io/badge/action-BE0000) | relax to convergence, write both files, exit |
| `--rows` `--cols` | ![mesh](https://img.shields.io/badge/mesh-27AE60) | `8` `8` — a 2-stitch border is added, as in the viewer |
| `--stitch-width` `--stitch-height` | ![mesh](https://img.shields.io/badge/mesh-27AE60) | `1.0` `1.0` |
| `--rest-course` `--rest-wale` | ![mesh](https://img.shields.io/badge/mesh-27AE60) | `0.75` `0.75` |
| `--solver` | ![solver](https://img.shields.io/badge/solver-8E44AD) | `neighbor`, or `original` for the per-vertex solve |
| `--k-stretch` `--k-shear` `--k-wale` | ![solver](https://img.shields.io/badge/solver-8E44AD) | `5.0` `0.2` `2.0` — original solver |
| `--kernel-spring` `--bound-spring` | ![solver](https://img.shields.io/badge/solver-8E44AD) | `2.0` `1.0` — neighbor-aware |
| `--e-shear` `--e-bend` `--e-slide` | ![solver](https://img.shields.io/badge/solver-8E44AD) | `2.0` `30.0` `5.0` — neighbor-aware |
| `--time-step` `--iters` `--tol` | ![converge](https://img.shields.io/badge/converge-F39C12) | `0.1` `60000` `1e-6` — `--iters` is a ceiling; `--tol` stops it early |
| `--out-smobj` `--out-bcc` | ![i/o](https://img.shields.io/badge/i%2Fo-546E7A) | `output/relaxed_stitch.smobj` `output/relaxed_yarn.bcc` |

`--export` exits `0` when the relaxation converged and `1` when it hit `--iters` first, so
sweeps are just shell loops:

```sh
printf '%s\n' 0.6 0.7 0.8 | xargs -P 8 -I{} \
  ./build/ArtakhaV2 --export --rows 32 --cols 32 --rest-course {} \
    --out-smobj output/rc{}.smobj --out-bcc output/rc{}.bcc
```

> [!NOTE]
> `--export` opens no window and reads nothing from `res/` — the whole relax-and-export
> path is CPU only, so it runs over SSH and parallelises freely.

## Controls

| Input | Action |
| :---: | :--- |
| ![WASD](https://img.shields.io/badge/W%20A%20S%20D-30363D) | Fly camera (move / up / down) |
| ![Scroll](https://img.shields.io/badge/Scroll-1E88E5) ![QE](https://img.shields.io/badge/Q%20E-30363D)| Move forward/back along view direction |
| ![Mouse](https://img.shields.io/badge/Mouse-1E88E5) | Look around | 
| ![Alt](https://img.shields.io/badge/Alt-30363D) | Toggle quad-preview mode + UI cursor mode |
| ![Ctrl + mouse drag](https://img.shields.io/badge/Ctrl%20%2B%20mouse%20drag-1E88E5) | Orbit the light around the mesh |
| ![F5](https://img.shields.io/badge/F5-30363D) | Save a screenshot to `screenshots/` |
| ![Esc](https://img.shields.io/badge/Esc-BE0000) | Quit | | |

## The Relaxation Solvers

The stitch mesh starts as a flat grid and is relaxed towards a physically
plausible knit shape:

| Solver | Method |
| :--- | :--- |
| **Original** (`Relax`)<br>![local](https://img.shields.io/badge/local-1E88E5?style=for-the-badge) | Solves a local per-vertex Newton step using stretch springs (course, wale, and both diagonals) and shear terms across each quad face, plus a wale-bending term across three vertically-stacked rows. |
| **Neighbor-Aware** (`RelaxNeighbor`)<br>![global](https://img.shields.io/badge/global-8E44AD?style=for-the-badge) | Builds a full sparse Hessian over the *dual graph* (one node per stitch face) with kernel/boundary springs, bilinear shear, a cosine bending energy, and a slide energy. The sparse system is solved at once with a conjugate-gradient solver. Vertex positions are recovered from the relaxed dual nodes via a weighted average of the four (or fewer, at boundaries) surrounding stitch centers and phantom nodes. |

Both are exposed live in the ImGui panel so you can compare convergence
behavior and tune spring/energy constants per run.

## Export: .smobj / .bcc

Clicking **Export Relaxed (smobj + bcc)** relaxes a *copy* of the current
mesh to convergence (max per-vertex delta below tolerance, or a max
iteration cap), then writes the following into the output folder:

| *File* | Contents |
| :--- | :--- |
| [`relaxed_stitch.smobj`](output/relaxed_stitch.smobj)<br>![topology](https://img.shields.io/badge/topology-27AE60?style=for-the-badge) | The stitch-mesh topology in the [augmented stitch mesh format](https://github.com/textiles-lab/smobj) (vertices, quad faces, a typed face library distinguishing cast-on, bind-off, left/right selvage and per-course knit direction, and edge-to-edge connectivity derived from the grid's shared vertices). |
| [`relaxed_yarn.bcc`](output/relaxed_yarn.bcc)<br>![curves](https://img.shields.io/badge/curves-BE0000?style=for-the-badge) | Yarn centerlines in Cem Yuksel's [Binary Curve Collection format](https://www.cemyuksel.com/research/yarnmodels/), sampled per stitch template (knit/purl/cast-on/bind-off/selvage) as open Catmull-Rom curves, up-axis Y. |

> [!NOTE]
> `.smobj` is topology, `.bcc` is geometry — so no `.sf` library is needed to use them.
> Knitting runs top-to-bottom here (cast-on is the highest row), which inverts loop
> edges relative to `faces/knitout.sf`.

## Project layout

<details>
<summary><b>📂 Project structure</b> — click to collapse</summary>

<pre>
ArtakhaV2/
├── <a href="README.md">README.md</a>
├── <a href="buildRun.bat">buildRun.bat</a>
├── <a href="CMakeLists.txt">CMakeLists.txt</a>          # macOS + Windows build, fetches GLFW + Eigen
├── <a href=".gitignore">.gitignore</a>
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
