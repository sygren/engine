#pragma once

#include <string>
#include <optional>
#include <type_traits>

#include "vulkan_primitives.hpp"
#include "../core.hpp"

#define MAX_PUSH_CONSTANTS_SIZE 128

// Description of the amount of vertices.
// Also describe the offset in the buffer.
// It is used to fill the draw commands
struct VerticesDesc {
    uint32_t count = 0;
    uint32_t offset = 0;
};

// Description of the amount of indices.
// Also describe the offset in the buffer.
// It is used to fill the draw commands
struct IndicesDesc {
    BufferId buffer = INVALID_ID;
    uint32_t count  = 0;
    uint32_t offset = 0;
};

// Opaque storage for Vulkan push constants.
// The renderer never interprets the contents.
// It simply uploads the raw bytes to the command buffer.
struct PushConstants {
    uint8_t buffer[MAX_PUSH_CONSTANTS_SIZE];
    size_t size;

    template<typename T>
    void write(const T &input) {
        static_assert(sizeof(T) <= MAX_PUSH_CONSTANTS_SIZE);
        static_assert(std::is_trivially_copyable_v<T>);
        static_assert(std::is_trivially_destructible_v<T>);

        std::memcpy(buffer, &input, sizeof(T));
        size = sizeof(T);
    }
};

// An object to be rendered, it might be geomtric (if any vertex data)
// Or a generic call to execute a post-processing pipeline.
struct RenderObject {
    Pipeline pipeline;

    VerticesDesc vertices = {};
    std::optional<DescriptorSet> descriptor_set = std::nullopt;
    std::optional<PushConstants> push_constants = std::nullopt;
    std::optional<IndicesDesc> indices = std::nullopt;
};

struct RenderPass {
  std::string name;

  std::vector<RenderObject> objects;
};

struct RenderContext {
  std::vector<RenderPass> passes;
};
