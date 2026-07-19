cmake_minimum_required (VERSION 4.1.1)
project(KaosEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------
# Build speed: ccache (transformative on clean rebuilds / branch switches)
# ------------------------------------------------------------------
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# ------------------------------------------------------------------
# Build speed: faster linker (lld/mold). Link time dominates on big
# C++ projects pulling in Vulkan + Assimp + GLFW.
# ------------------------------------------------------------------
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT MSVC)
    find_program(LLD_LINK_PROGRAM lld-link)
    if(LLD_LINK_PROGRAM)
        add_link_options(-fuse-ld=lld-link)
    endif()
elseif(NOT MSVC)
    find_program(MOLD_PROGRAM mold)
    find_program(LLD_PROGRAM ld.lld)
    if(MOLD_PROGRAM)
        add_link_options(-fuse-ld=mold)
    elseif(LLD_PROGRAM)
        add_link_options(-fuse-ld=lld)
    endif()
endif()

#file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
  PRIVATE
    src/main.cpp
    src/impl.cpp
    src/app.cpp
    src/renderer/renderer.cpp
    src/asset_manager.cpp
)

find_package(Vulkan REQUIRED)

get_filename_component(VULKAN_SDK_ROOT "${Vulkan_INCLUDE_DIRS}" DIRECTORY)
add_library(slang SHARED IMPORTED)
set_target_properties(slang PROPERTIES
    IMPORTED_IMPLIB   "${VULKAN_SDK_ROOT}/Lib/slang.lib"
    IMPORTED_LOCATION "${VULKAN_SDK_ROOT}/Bin/slang.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${Vulkan_INCLUDE_DIRS}"
)

include(FetchContent)

# Optional: pin a shared cache dir so a `rm -rf build` doesn't
# re-clone everything from git each time.
# set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.fc-cache" CACHE PATH "")

# ------------------------------------------------------------------
# GLFW config — skip docs/tests/examples (ON by default upstream)
# ------------------------------------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  fastgltf
  GIT_REPOSITORY https://github.com/spnda/fastgltf.git
  GIT_TAG        a31be25
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  VulkanMemoryAllocator
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG        v3.3.0
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        31c1ad3
  GIT_SHALLOW    TRUE
)

# Declaring everything first, then fetching once, lets CMake overlap
# the git clones instead of doing them strictly sequentially.
FetchContent_MakeAvailable(glfw fastgltf VulkanMemoryAllocator stb)

set_target_properties(VulkanMemoryAllocator PROPERTIES SYSTEM TRUE)
#set_target_properties(fastgltf PROPERTIES SYSTEM TRUE)
set_target_properties(glfw PROPERTIES SYSTEM TRUE)

add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})

target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_link_libraries(${PROJECT_NAME}
    Vulkan::Vulkan
    glfw
    fastgltf
    VulkanMemoryAllocator
    slang
    stb
)
