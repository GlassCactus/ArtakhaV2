# ============================================================================
#  ArtakhaV2 - CMake build
# ============================================================================
#  Auto-fetches GLFW and Eigen from the internet the first time you configure.
#  Reuses the glad.c already in this repo.
#
#  YOU MUST STILL PROVIDE THE GLAD HEADERS ONCE (they are code-generated):
#    1. Go to https://glad.dav1d.de
#       Language: C/C++ | API gl: 3.3 (or higher) | Profile: Core
#       tick "Generate a loader" -> Generate -> download the zip
#    2. From the zip's "include" folder, copy these into this project so you get:
#         external/include/glad/glad.h
#         external/include/KHR/khrplatform.h
#
#  Then in Visual Studio 2022:  File -> Open -> Folder...  (pick this folder)
#  VS will configure automatically, then Build -> Build All.
#  (First configure needs an internet connection for GLFW + Eigen.)
# ============================================================================

cmake_minimum_required(VERSION 3.16)
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
FetchContent_Declare(eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG        ${EIGEN_GIT_TAG}
    SOURCE_DIR     "${EIGEN_ROOT}/eigen-5.0.0"
)
FetchContent_GetProperties(eigen)
if(NOT eigen_POPULATED)
    FetchContent_Populate(eigen)   # download headers only, do not run Eigen's CMake
endif()

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
target_link_libraries(ArtakhaV2 PRIVATE glfw opengl32)

# Stop GLFW from pulling in the system OpenGL header (glad provides it).
# Needed because Source.cpp includes <GLFW/glfw3.h> before <glad/glad.h>.
target_compile_definitions(ArtakhaV2 PRIVATE GLFW_INCLUDE_NONE)

if(MSVC)
    target_compile_definitions(ArtakhaV2 PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    target_compile_options(ArtakhaV2 PRIVATE /arch:AVX2)   # matches the .vcxproj (AVX2 for Eigen)
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
