#include <cstdint>
#include <cstring>
#include <iostream>
#include <print>
#include <stb_image.h>
#include <string_view>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <variant>
#include <vector>

#include "asset_manager.hpp"
#include "core.hpp"
#include "fastgltf/types.hpp"
#include "fastgltf/util.hpp"
#include "renderer/renderer.hpp"

void AssetManager::Init(Renderer *renderer) { p_renderer = renderer; }

std::optional<TextureHandle> AssetManager::GetTexture(std::filesystem::path path, bool flip_y) {
    auto key = path.string();

  auto it = m_textures.find(key);
  if (it != m_textures.end()) {
    auto &entry = it->second;
    if (auto control = entry.control.lock()) {
      return TextureHandle(entry.id, control);
    }
    m_textures.erase(it);
  }

  int width, height, alphaCh;
  stbi_set_flip_vertically_on_load(flip_y);
  uint8_t *data = stbi_load(key.c_str(), &width, &height, &alphaCh, STBI_rgb_alpha);

  if (!data) {
    return std::nullopt;
  }

  auto handle = GetTexture(std::move(key), data, width, height);

  stbi_image_free(data);
  return handle;
}

std::optional<TextureHandle> AssetManager::GetTexture(std::string key, void *data, uint32_t width, uint32_t height) {
    if (data == nullptr || width == 0 || height == 0) {
        return std::nullopt;
    }

    TextureDesc desc;
    desc.name      = key;
    desc.pData     = data;
    desc.width     = width;
    desc.height    = height;
    desc.mipmapped = true;

    auto texture_id = p_renderer->CreateTexture(desc);
    auto control = std::shared_ptr<void>(nullptr, [this, key, texture_id](void *) { DestroyTexture(key, texture_id); });

    m_textures.emplace(std::move(key), TextureEntry{texture_id, control});
    return TextureHandle(texture_id, control);
}

void ProcessNodeGltf(AssetManager *asset_manager, std::string_view path, fastgltf::Asset &gltf, size_t node_index, const glm::mat4 &parent_transform,
                     const std::span<TextureId> texture_ids, Model &out) {
  fastgltf::Node &node = gltf.nodes[node_index];

  glm::mat4 node_transform;
  auto      buffer = fastgltf::getLocalTransformMatrix(node);
  std::memcpy(&node_transform, buffer.data(), sizeof(glm::mat4));

  glm::mat4 transform = parent_transform * node_transform;

  if (node.meshIndex.has_value()) {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;

    auto &gltf_mesh = gltf.meshes[node.meshIndex.value()];
    Mesh  mesh{};
    mesh.transform = transform;

    mesh.submeshes.reserve(gltf_mesh.primitives.size());
    for (const auto &gltf_submesh : gltf_mesh.primitives) {
      SubMesh submesh;
      submesh.vertexOffset = vertices.size();
      submesh.indexOffset  = indices.size();

      {
        const auto &index_accessor = gltf.accessors[gltf_submesh.indicesAccessor.value()];
        submesh.indexCount         = index_accessor.count;
        indices.reserve(indices.size() + index_accessor.count);
        fastgltf::iterateAccessor<uint32_t>(gltf, index_accessor, [&](uint32_t index) { indices.push_back(index); });
      }

      {
        if (!(gltf_submesh.findAttribute("POSITION") && gltf_submesh.findAttribute("NORMAL") && gltf_submesh.findAttribute("TEXCOORD_0"))) {
          std::println("Error while loading model : {}, either no position,normal or texcoord_0", path);
          abort();
        }
        const auto &vertex_accessor_position = gltf.accessors[gltf_submesh.findAttribute("POSITION")->accessorIndex];
        const auto &vertex_accessor_normal   = gltf.accessors[gltf_submesh.findAttribute("NORMAL")->accessorIndex];
        const auto &vertex_accessor_uv       = gltf.accessors[gltf_submesh.findAttribute("TEXCOORD_0")->accessorIndex];
        vertices.resize(vertices.size() + vertex_accessor_position.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, vertex_accessor_position, [&](glm::vec3 position, size_t index) {
          vertices[submesh.vertexOffset + index].position = glm::vec4(position, 1.f);
        });
        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, vertex_accessor_normal, [&](glm::vec3 normal, size_t index) {
          vertices[submesh.vertexOffset + index].normal = glm::vec4(normal, 0.f);
        });
        fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, vertex_accessor_uv,
                                                      [&](glm::vec2 uv, size_t index) { vertices[submesh.vertexOffset + index].uv = uv; });
      }

      if (gltf_submesh.materialIndex.has_value()) {
        fastgltf::Material &submesh_material = gltf.materials[gltf_submesh.materialIndex.value()];
        auto               &pbr              = submesh_material.pbrData;

        submesh.material.baseColorFactor = glm::make_vec4(pbr.baseColorFactor.data());
        submesh.material.emissiveFactor  = glm::vec4(glm::make_vec3(submesh_material.emissiveFactor.data()), 0.f);
        submesh.material.metallicFactor  = pbr.metallicFactor;
        submesh.material.roughnessFactor = pbr.roughnessFactor;

        auto resolve = [&](auto &texInfo) -> TextureId {
          if (!texInfo.has_value())
            return INVALID_ID;
          size_t idx = texInfo->textureIndex;
          return idx < texture_ids.size() ? texture_ids[idx] : INVALID_ID;
        };

        submesh.material.baseColorTexture         = resolve(pbr.baseColorTexture);
        submesh.material.metallicRoughnessTexture = resolve(pbr.metallicRoughnessTexture);
        submesh.material.normalTexture            = resolve(submesh_material.normalTexture);
        submesh.material.occlusionTexture         = resolve(submesh_material.occlusionTexture);
        submesh.material.emissiveTexture          = resolve(submesh_material.emissiveTexture);
      };

      mesh.submeshes.push_back(submesh);
    }

    BufferDesc buffer_desc{};
    buffer_desc.rate = UPDATE_RATE_NEVER;

    buffer_desc.name  = "";
    buffer_desc.pData = vertices.data();
    buffer_desc.size  = vertices.size() * sizeof(Vertex);
    buffer_desc.usage = BUFFER_USAGE_VERTEX;
    buffer_desc.type =  BUFFER_TYPE_SSBO;

    mesh.vertexBuffer = asset_manager->p_renderer->CreateBuffer(buffer_desc);

    buffer_desc.name  = "";
    buffer_desc.pData = indices.data();
    buffer_desc.size  = indices.size() * sizeof(uint32_t);
    buffer_desc.usage = BUFFER_USAGE_INDEX;
    buffer_desc.type =  BUFFER_TYPE_SSBO;
    mesh.indexBuffer  = asset_manager->p_renderer->CreateBuffer(buffer_desc);
    out.meshes.push_back(mesh);
  }

  for (size_t child_node : node.children) {
    ProcessNodeGltf(asset_manager, path, gltf, child_node, transform, texture_ids, out);
  }
};

std::optional<ModelHandle> AssetManager::GetGltfModel(std::filesystem::path path) {
  std::string key(std::move(path.string()));

  auto it = m_models.find(key);
  if (it != m_models.end()) {
    auto &entry = it->second;

    if (auto control = entry.model.lock()) {
      return ModelHandle(entry.id, control);
    }

    auto &key = it->first;
    m_models.erase(key);
  }

  auto gltfFile = fastgltf::GltfDataBuffer::FromPath(path);
  if (gltfFile.error() != fastgltf::Error::None) {
    return std::nullopt;
  }

  fastgltf::Parser parser;
  auto             asset = parser.loadGltf(gltfFile.get(), path.parent_path(), fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::LoadExternalImages | fastgltf::Options::LoadExternalBuffers);
  if (asset.error() != fastgltf::Error::None) {
    return std::nullopt;
  }

  fastgltf::Asset &gltf  = asset.get();
  auto            &scene = gltf.scenes[gltf.defaultScene.value_or(0)];

  Model model{};

  std::vector<TextureId> texture_ids(gltf.textures.size(), INVALID_ID);
  for (size_t i = 0; i < gltf.textures.size(); i++) {
    auto &gltf_tex = gltf.textures[i];
    if (!gltf_tex.imageIndex.has_value())
      continue;

    auto &image = gltf.images[gltf_tex.imageIndex.value()];

    std::optional<TextureHandle> handle = std::nullopt;

    std::visit(fastgltf::visitor {
       [](auto& arg) {},
      [&](fastgltf::sources::URI& file_path) {
          handle = GetTexture(path.parent_path() / file_path.uri.path(), false);
      },
      [&](fastgltf::sources::Array& array) {
          int width,height,nrChannels;
          stbi_set_flip_vertically_on_load(false);
          uint8_t *data = stbi_load_from_memory((const stbi_uc*)array.bytes.data(), array.bytes.size(), &width, &height, &nrChannels, STBI_rgb_alpha);
          handle = GetTexture(key, data, width, height);
          stbi_image_free(data);
      },
      [&](fastgltf::sources::BufferView& view) {
          auto& buffer_view = gltf.bufferViews[view.bufferViewIndex];
          auto& buffer = gltf.buffers[buffer_view.bufferIndex];

          if (auto *array = std::get_if<fastgltf::sources::Array>(&buffer.data)) {
              int width,height,nrChannels;
              stbi_set_flip_vertically_on_load(false);
              uint8_t *data = stbi_load_from_memory((const stbi_uc*)array->bytes.data(), array->bytes.size(), &width, &height, &nrChannels, STBI_rgb_alpha);
              handle = GetTexture(key, data, width, height);
              stbi_image_free(data);
          } else {
              std::println("Error while loading {} BufferView isnt in an Array already!", key);
              abort();
          }
      }
    }, image.data);

    if (!handle.has_value()) {
          std::println("WARNING: failed to load texture {} (image {}) for model {}", i, gltf_tex.imageIndex.value(), key);
      continue;
    }

    texture_ids[i] = handle->Id();
    model.owned_textures.push_back(std::move(*handle));
  }

  for (size_t index : scene.nodeIndices) {
    ProcessNodeGltf(this, key, gltf, index, glm::mat4(1.f), texture_ids, model);
  }

  auto model_id = m_model_id_count++;
  auto control  = std::shared_ptr<Model>(new Model(model), [this, key](Model *model) {
    DestroyModel(key, model);
    delete model;
  });

  m_models[key] = ModelEntry{model_id, control};

  return ModelHandle(model_id, control);
}

void AssetManager::DestroyTexture(const std::string &path, TextureId id) {
  std::cout << "Freeing Texture: " << path << std::endl;
  p_renderer->DestroyTexture(id);
  m_textures.erase(path);
}

void AssetManager::DestroyModel(const std::string &path, Model *model) {
  assert(model);
  std::cout << "Freeing Model: " << path << std::endl;
  for (auto &mesh : model->meshes) {
    p_renderer->DestroyBuffer(mesh.vertexBuffer);
    p_renderer->DestroyBuffer(mesh.indexBuffer);
  }
  m_models.erase(path);
}
