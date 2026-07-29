# ArtakhaV2 — Real-Time Yarn-Level Knit Simulation

A real-time OpenGL renderer that builds a stitch mesh, relaxes it under a
knit-mechanics energy model, and generates procedural yarn geometry
(cast-on, knit, purl, selvage, bind-off) using Catmull-Rom splines swept
into tubes. Relaxed swatches can be exported as **`.smobj`** (augmented
stitch mesh) and **`.bcc`** (binary curve collection) for use in other
yarn-level cloth tools.

---

## Table of Contents

- [Features](#features)
- [Gallery](#gallery)
- [Dependencies](#dependencies)
- [Building](#building)
- [Controls](#controls)
- [The Relaxation Solvers](#the-relaxation-solvers)
- [Export: .smobj / .bcc](#export-smobj--bcc)
- [Acknowledgments](#acknowledgments)

---

## Features

- **Procedural stitch mesh** — configurable rows/columns, course/wale rest
  lengths, built on a quad grid with a dual graph for neighbor-aware forces.
- **Two relaxation solvers**, swappable at runtime:
  - *Original* — per-vertex Newton step (stretch, shear, wale-bend springs).
  - *Neighbor-Aware* — global sparse Newton solve over the dual graph
    (kernel/boundary springs, shear, bend, slide energies) via
    `Eigen::ConjugateGradient`.
- **Procedural yarn geometry** — per-stitch template curves (knit, purl,
  cast-on, bind-off, left/right selvage) sampled with Catmull-Rom splines
  and swept into tubes with parallel-transport frames.
- **Mesh morphing** — bend, twist, stretch, shear, or wrap the relaxed
  swatch onto a sphere for garment-shaping previews.
- **Full render pipeline** — shadow-mapped directional light, 5 selectable
  skyboxes, gamma correction, live ImGui control panel.
- **One-click export** — relax to convergence and write out `.smobj` +
  `.bcc` for the current swatch.

## Gallery

| Sheared | Solved |
|:---:|:---:|
| <img src="https://github.com/user-attachments/assets/c9c0eebe-151c-4b52-8408-e0debe7028a2" width="480" alt="App screenshot"> | <img src="https://github.com/user-attachments/assets/6988093c-62b1-4767-ad84-9d1658cad503" width="480" alt="App screenshot" />
"> |


## Dependencies

| Library | Purpose |
|---|---|
| [GLFW](https://www.glfw.org/) | Windowing, input |
| [GLAD](https://glad.dav1d.de/) | OpenGL core loader (generated for 3.3, vendored in `external/`) |
| [Dear ImGui](https://github.com/ocornut/imgui) | Control panel UI |
| [stb_image / stb_image_write](https://github.com/nothings/stb) | Texture loading, screenshot PNG export |
| [GLM](https://github.com/g-truc/glm) | Math (vectors, matrices, transforms) |
| [Eigen](https://eigen.tuxfamily.org/) 5.0 | Sparse linear solves for the neighbor-aware relaxer |
| [cyCodeBase](https://www.cemyuksel.com/cyCodeBase/) (`cyTriMesh`) | OBJ mesh loading | (unused for now)

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

> **OpenGL version.** Both platforms target **OpenGL 4.1 core / GLSL 410**. macOS
> caps out at 4.1, and nothing here needs anything newer — so there is one shader
> set and one GLAD loader rather than a per-platform split. Note that Apple has
> deprecated OpenGL; it still works, but will not advance past 4.1.

## Controls

| Input | Action |
|---|---|
| `W A S D` / `Q E` | Fly camera (move / up / down) |
| Mouse | Look around |
| Scroll | Move forward/back along view direction |
| `Alt` | Toggle quad-preview mode + UI cursor mode |
| `Ctrl` + mouse drag | Orbit the light around the mesh |
| `F5` | Save a screenshot to `screenshots/` |
| `Esc` | Quit |

## The Relaxation Solvers

The stitch mesh starts as a flat grid and is relaxed toward a physically
plausible knit shape:

- **Original (`Relax`)** solves a local per-vertex Newton step using
  stretch springs (course, wale, and both diagonals) and shear terms
  across each quad face, plus a wale-bending term across three
  vertically-stacked rows.
- **Neighbor-Aware (`RelaxNeighbor`)** builds a full sparse Hessian over
  the *dual graph* (one node per stitch) with kernel/boundary springs,
  bilinear shear, a cosine bending energy, and a slide energy, then
  solves the whole system at once with a conjugate-gradient solver.
  Vertex positions are recovered from the relaxed dual nodes via a
  weighted average of the four (or fewer, at boundaries) surrounding
  stitch centers.

Both are exposed live in the ImGui panel so you can compare convergence
behavior and tune spring/energy constants per run.

## Export: .smobj / .bcc

Clicking **Export Relaxed (smobj + bcc)** relaxes a *copy* of the current
mesh to convergence (max per-vertex delta below tolerance, or a max
iteration cap), then writes:

- **`output/relaxed_stitch.smobj`** — the stitch-mesh topology in the
  [augmented stitch mesh format](https://github.com/textiles-lab/smobj)
  (vertices, quad faces, a single generic `knit` face type, and
  edge-to-edge connectivity derived from the grid's shared vertices).
- **`output/relaxed_yarn.bcc`** — yarn centerlines in Cem Yuksel's
  [Binary Curve Collection format](https://www.cemyuksel.com/research/yarnmodels/),
  sampled per stitch template (knit/purl/cast-on/bind-off/selvage) as
  open Catmull-Rom curves, up-axis Y.

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
