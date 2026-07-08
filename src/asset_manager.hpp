#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core.hpp"

class Renderer;

class TextureHandle {
public:
  TextureHandle() = default;
  TextureHandle(TextureId id, std::shared_ptr<void> ptr) : m_id(id), m_control(std::move(ptr)) {}

  TextureId Id() const noexcept { return m_id; };
  bool      IsValid() const noexcept { return static_cast<bool>(m_control); }

private:
  TextureId             m_id;
  std::shared_ptr<void> m_control;
};

struct Model {
  std::vector<Mesh>          meshes;
  std::vector<TextureHandle> owned_textures;
};

class ModelHandle {
public:
  ModelHandle() = default;
  ModelHandle(ModelId id, std::shared_ptr<Model> ptr) : m_id(id), m_control(std::move(ptr)) {}

  ModelId Id() const noexcept { return m_id; };
  bool    IsValid() const noexcept { return static_cast<bool>(m_control); }
  Model  *Get() const noexcept { return m_control.get(); }

private:
  ModelId                m_id;
  std::shared_ptr<Model> m_control;
};

class AssetManager {
public:
  AssetManager()  = default;
  ~AssetManager() = default;

  AssetManager(AssetManager &)              = delete;
  AssetManager &operator=(AssetManager &)   = delete;
  AssetManager(AssetManager &&)             = delete;
  AssetManager &&operator=(AssetManager &&) = delete;

  void Init(Renderer *renderer);

  std::optional<TextureHandle> GetTexture(std::filesystem::path path, bool flip_y = false);
  std::optional<TextureHandle> GetTexture(std::string key, void *data, uint32_t width, uint32_t height);
  std::optional<ModelHandle>   GetGltfModel(std::filesystem::path model);

  Renderer *p_renderer;
private:

  struct TextureEntry {
    TextureId           id;
    std::weak_ptr<void> control;
  };

  struct ModelEntry {
    ModelId              id;
    std::weak_ptr<Model> model;
  };

  std::unordered_map<std::string, TextureEntry> m_textures;
  std::unordered_map<std::string, ModelEntry>   m_models;
  uint32_t                                      m_model_id_count = 0;

  void DestroyTexture(const std::string &path, TextureId id);
  void DestroyModel(const std::string &path, Model *model);
};
