# ============================================================================
#  ArtakhaV2 - CMake build  (macOS + Windows)
# ============================================================================
#  Everything needed is either vendored in this repo (GLAD, ImGui, GLM, stb)
#  or fetched automatically on first configure (GLFW, Eigen).  There is no
#  per-developer setup -- a fresh clone builds as-is.
#  (First configure needs an internet connection for GLFW + Eigen.)
#
#  macOS / command line:
#      cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#      cmake --build build -j
#      ./build/ArtakhaV2
#
#  Windows / Visual Studio 2022:
#      File -> Open -> Folder...  (pick this folder)
#      VS configures automatically, then Build -> Build All.
#
#  Targets OpenGL 4.1 core / GLSL 410 on both platforms -- that is the
#  ceiling on macOS, and nothing here needs anything newer.
# ============================================================================

cmake_minimum_required(VERSION 3.26)   # copy_directory_if_different (below) needs 3.26
project(ArtakhaV2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Fail early with a helpful message if the GLAD headers are missing --------
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/include/glad/glad.h")
    message(FATAL_ERROR
        "\n GLAD headers not found.\n"
        " Expected: ${CMAKE_CURRENT_SOURCE_DIR}/external/include/glad/glad.h\n"
        " Generate them once at https://glad.dav1d.de (gl 3.3, Core, C/C++,\n"
        " 'Generate a loader'), then copy the zip's include/glad and\n"
        " include/KHR folders into external/include/.  See top of this file.\n")
endif()

include(FetchContent)

# --- GLFW (downloaded + built automatically) ---------------------------------
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# --- Eigen (header-only; downloaded, not built) ------------------------------
#  The source includes it as  <eigen-5.0.0/Eigen/Sparse>, so we force the
#  clone into a folder literally named "eigen-5.0.0" and put its PARENT on the
#  include path.  Change EIGEN_GIT_TAG below if 5.0.0 is unavailable (e.g. 3.4.0).
set(EIGEN_GIT_TAG "5.0.0" CACHE STRING "Eigen git tag to fetch")
set(EIGEN_ROOT "${CMAKE_BINARY_DIR}/eigen-root")
#  SOURCE_SUBDIR points at a path that does not exist on purpose: that makes
#  FetchContent download the headers without running Eigen's own CMake build.
FetchContent_Declare(eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG        ${EIGEN_GIT_TAG}
    SOURCE_DIR     "${EIGEN_ROOT}/eigen-5.0.0"
    SOURCE_SUBDIR  "do-not-configure-eigen"
)
FetchContent_MakeAvailable(eigen)

# --- ImGui sources (vendored in this repo) -----------------------------------
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/imgui")
set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/imgui_impl_opengl3.cpp"
)

# --- Project sources ---------------------------------------------------------
set(PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/external/src/glad.c"  # matched pair, generated with the headers
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Source.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/FrameBuffer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IndexBuffer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Shader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Texture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/VertexArray.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/VertexBuffer.cpp"
)

add_executable(ArtakhaV2 ${PROJECT_SOURCES} ${IMGUI_SOURCES})

target_include_directories(ArtakhaV2 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor"
    "${IMGUI_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/include"   # glad/ and KHR/
    "${EIGEN_ROOT}"                                  # makes <eigen-5.0.0/Eigen/...> resolve
)

# GLFW's glfw3.h comes from the glfw target's own interface include dirs.
# The glfw target also carries Cocoa/IOKit/CoreFoundation on macOS and gdi32 on
# Windows through its interface link libraries, so only GL itself is needed here.
set(OpenGL_GL_PREFERENCE GLVND)   # no-op on Win/Mac, picks the modern lib on Linux
find_package(OpenGL REQUIRED)
target_link_libraries(ArtakhaV2 PRIVATE glfw OpenGL::GL)

# Stop GLFW from pulling in the system OpenGL header (glad provides it).
# Needed because Source.cpp includes <GLFW/glfw3.h> before <glad/glad.h>.
target_compile_definitions(ArtakhaV2 PRIVATE GLFW_INCLUDE_NONE)

if(MSVC)
    target_compile_definitions(ArtakhaV2 PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    target_compile_options(ArtakhaV2 PRIVATE /arch:AVX2)   # AVX2 for Eigen
endif()

# Apple deprecated OpenGL in 10.14; it still works fine, we just don't want the
# warning on every single GL call in the build log.
if(APPLE)
    target_compile_options(ArtakhaV2 PRIVATE -Wno-deprecated-declarations)
endif()

# cyCore.h unconditionally includes <immintrin.h> (x86 SSE/AVX intrinsics) unless
# this is defined. That header does not exist on ARM -- e.g. Apple Silicon.
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
    target_compile_definitions(ArtakhaV2 PRIVATE CY_NO_INTRIN_H)
endif()

# Run from the project root so it finds res\shaders, res\textures and imgui.ini
set_target_properties(ArtakhaV2 PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Also copy res\ and imgui.ini next to the exe, so it runs correctly no matter
# what working directory Visual Studio (or a double-click) launches it from.
add_custom_command(TARGET ArtakhaV2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:ArtakhaV2>/res"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/imgui.ini" "$<TARGET_FILE_DIR:ArtakhaV2>/imgui.ini"
    COMMENT "Copying res/ and imgui.ini next to the executable")
