#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

using BufferId  = uint32_t;
using TextureId = uint32_t;
using ModelId   = uint32_t;

#define INVALID_ID 0xFFFFFFFF

struct Vertex {
  glm::vec4 position;
  glm::vec4 normal;
  glm::vec2 uv;
};

struct Material {
  TextureId baseColorTexture         = INVALID_ID;
  TextureId metallicRoughnessTexture = INVALID_ID;
  TextureId normalTexture            = INVALID_ID;
  TextureId occlusionTexture         = INVALID_ID;
  TextureId emissiveTexture          = INVALID_ID;
  glm::vec4 baseColorFactor = glm::vec4(1.f);
  glm::vec4 emissiveFactor  = glm::vec4(0.f);
  float     metallicFactor  = 1.f;
  float     roughnessFactor = 1.f;
};

struct SubMesh {
  uint32_t vertexOffset;
  uint32_t indexOffset;
  uint32_t indexCount;
  Material material;
};

struct Mesh {
  std::vector<SubMesh> submeshes;
  BufferId             vertexBuffer = INVALID_ID;
  BufferId             indexBuffer  = INVALID_ID;
  glm::mat4            transform    = glm::mat4(1.f);
};
