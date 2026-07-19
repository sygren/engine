#pragma once

#include "renderer/renderer.hpp"
#include <GLFW/glfw3.h>

#include "asset_manager.hpp"

struct Camera {
  glm::vec3 pos;
  float     yaw;
  float     pitch;
  float     fov;
  float     near;
  float     far;

  void      LookAt(const glm::vec3 &target);
  glm::mat4 GetViewProj(float ratio) const;
  glm::vec3 GetFront() const;
  glm::vec3 GetRight() const;
};

struct PointLight {
  glm::vec4 position;
  glm::vec4 ambient;
  glm::vec4 diffuse;
  glm::vec4 specular;

  float constant;
  float linear;
  float quadratic;
};

struct DirectionalLight {
  glm::vec4 direction;
  glm::vec4 ambient;
  glm::vec4 diffuse;
  glm::vec4 specular;
};

struct SpotLight {
  glm::vec4 position;
  glm::vec4 direction;
  glm::vec4 ambient;
  glm::vec4 diffuse;
  glm::vec4 specular;
  float     OuterCutOff;
  float     InnerCutOff;
};

class App {
public:
  GLFWwindow *pWindowHandle;
  uint32_t    mScreenWidth;
  uint32_t    mScreenHeight;

  Renderer mRenderer;
  AssetManager mAssetManager;

  Camera mCamera;

  float dt;

  void Init();
  void Run();

  App() = default;
  ~App();
};
