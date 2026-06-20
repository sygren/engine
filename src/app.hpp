#pragma once

#include "renderer.hpp"
#include <GLFW/glfw3.h>

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

struct Vertex {
  glm::vec4 pos;
  glm::vec4 color;
  glm::vec4 normal;
  glm::vec2 uv;
};

struct Material {
  float shininess;
  float _pad[3] = {0};
};

struct PointLight {
  glm::vec4 position;
  glm::vec4 ambient;
  glm::vec4 diffuse;
  glm::vec4 specular;

  float constant;
  float linear;
  float quadratic;
  float _padding;
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
  float     _padding[2] = {0};
};

struct PushConstants {
  glm::mat4 mModel;
  glm::mat4 mTransformedModel; // transpose(inverse(mModel));
  Material  mMaterial;
  uint64_t  pVertexBufferAddress;
};
static_assert(sizeof(PushConstants) <= 256, "PushConstants must be 256 bytes or less.");

class App {
public:
  GLFWwindow *pWindowHandle;
  uint32_t    mScreenWidth;
  uint32_t    mScreenHeight;

  Renderer mRenderer;

  Camera mCamera;

  float dt;

  void Init();
  void Run();

  App() = default;
  ~App();
};
