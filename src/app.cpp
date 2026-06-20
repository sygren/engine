#include "app.hpp"
#include "GLFW/glfw3.h"
#include "renderer.hpp"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <iostream>

#include <stb_image.h>

#include <glm/gtc/matrix_transform.hpp>

#define CHECK(x)                                                                                                                                     \
  do {                                                                                                                                               \
    if (!x) {                                                                                                                                        \
      std::cerr << "CHECK failed: "                                                                                                                  \
                << " in " << __func__ << " ( " << __FILE__ << ":" << __LINE__ << ")" << std::endl;                                                   \
      abort();                                                                                                                                       \
    }                                                                                                                                                \
  } while (0)

static std::vector<Vertex> vertices = {
    // Face avant (Z+) - normal (0, 0, 1)
    {{-0.5f, -0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f}},
    {{0.5f, -0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, 1.f, 0.f}, {1.f, 0.f}},
    {{0.5f, 0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, 1.f, 0.f}, {1.f, 1.f}},
    {{0.5f, 0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, 1.f, 0.f}, {1.f, 1.f}},
    {{-0.5f, 0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 1.f}},
    {{-0.5f, -0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f}},

    // Face arrière (Z-) - normal (0, 0, -1)
    {{0.5f, -0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, -1.f, 0.f}, {0.f, 0.f}},
    {{-0.5f, -0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, -1.f, 0.f}, {1.f, 0.f}},
    {{-0.5f, 0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, -1.f, 0.f}, {1.f, 1.f}},
    {{-0.5f, 0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, -1.f, 0.f}, {1.f, 1.f}},
    {{0.5f, 0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, -1.f, 0.f}, {0.f, 1.f}},
    {{0.5f, -0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, -1.f, 0.f}, {0.f, 0.f}},

    // Face gauche (X-) - normal (-1, 0, 0)
    {{-0.5f, -0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {-1.f, 0.f, 0.f, 0.f}, {0.f, 0.f}},
    {{-0.5f, -0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {-1.f, 0.f, 0.f, 0.f}, {1.f, 0.f}},
    {{-0.5f, 0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {-1.f, 0.f, 0.f, 0.f}, {1.f, 1.f}},
    {{-0.5f, 0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {-1.f, 0.f, 0.f, 0.f}, {1.f, 1.f}},
    {{-0.5f, 0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {-1.f, 0.f, 0.f, 0.f}, {0.f, 1.f}},
    {{-0.5f, -0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {-1.f, 0.f, 0.f, 0.f}, {0.f, 0.f}},

    // Face droite (X+) - normal (1, 0, 0)
    {{0.5f, -0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 0.f, 0.f, 0.f}, {0.f, 0.f}},
    {{0.5f, -0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 0.f, 0.f, 0.f}, {1.f, 0.f}},
    {{0.5f, 0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 0.f, 0.f, 0.f}, {1.f, 1.f}},
    {{0.5f, 0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 0.f, 0.f, 0.f}, {1.f, 1.f}},
    {{0.5f, 0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 0.f, 0.f, 0.f}, {0.f, 1.f}},
    {{0.5f, -0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 0.f, 0.f, 0.f}, {0.f, 0.f}},

    // Face bas (Y-) - normal (0, -1, 0)
    {{-0.5f, -0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, -1.f, 0.f, 0.f}, {0.f, 0.f}},
    {{0.5f, -0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, -1.f, 0.f, 0.f}, {1.f, 0.f}},
    {{0.5f, -0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, -1.f, 0.f, 0.f}, {1.f, 1.f}},
    {{0.5f, -0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, -1.f, 0.f, 0.f}, {1.f, 1.f}},
    {{-0.5f, -0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, -1.f, 0.f, 0.f}, {0.f, 1.f}},
    {{-0.5f, -0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, -1.f, 0.f, 0.f}, {0.f, 0.f}},

    // Face haut (Y+) - normal (0, 1, 0)
    {{-0.5f, 0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f}},
    {{0.5f, 0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 1.f, 0.f, 0.f}, {1.f, 0.f}},
    {{0.5f, 0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 1.f, 0.f, 0.f}, {1.f, 1.f}},
    {{0.5f, 0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 1.f, 0.f, 0.f}, {1.f, 1.f}},
    {{-0.5f, 0.5f, -0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 1.f}},
    {{-0.5f, 0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f}},
};

static std::vector<uint32_t> indices = {
    0, 1, 2, 2, 3, 0,
};

static bool   gCursorLocked  = true;
static bool   gCursorWasFree = false;
static double gLastXPos      = 0.f;
static double gLastYPos      = 0.f;

void Camera::LookAt(const glm::vec3 &target) {
  auto dir    = target - pos;
  auto xz_len = glm::length(glm::vec2(dir.x, dir.z));
  yaw         = glm::degrees(atan2(dir.x, dir.z));
  pitch       = std::clamp<float>(glm::degrees(atan2(dir.y, xz_len)), -89.f, 89.f);
}

glm::mat4 Camera::GetViewProj(float ratio) const {
  auto forward = GetFront();
  auto view    = glm::lookAt(pos, pos + forward, glm::vec3(0.f, 1.f, 0.f));
  auto proj    = glm::perspective(glm::radians(fov), ratio, far, near);
  proj[1][1] *= -1;
  return proj * view;
}

glm::vec3 Camera::GetFront() const {
  auto p = glm::radians(pitch);
  auto y = glm::radians(yaw);
  return glm::normalize(glm::vec3({cos(p) * sin(y), sin(p), cos(p) * cos(y)}));
}

glm::vec3 Camera::GetRight() const { return glm::normalize(glm::cross(GetFront(), glm::vec3(0, 1, 0))); }

static void KeyboardInputWindowCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    if (gCursorLocked) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      gCursorWasFree = true;
    }
    gCursorLocked = !gCursorLocked;
  }
}

static void KeyboardInputWindowUpdate(GLFWwindow *window) {
  auto app   = (App *)glfwGetWindowUserPointer(window);
  auto front = app->mCamera.GetFront();
  auto right = app->mCamera.GetRight();

  auto camera_speed = 2.0f;

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    app->mCamera.pos += camera_speed * app->dt * front;
  }
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    app->mCamera.pos -= camera_speed * app->dt * front;
  }
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    app->mCamera.pos += camera_speed * app->dt * right;
  }
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    app->mCamera.pos -= camera_speed * app->dt * right;
  }
}

static void MouseInputWindowCallback(GLFWwindow *window, double xpos, double ypos) {
  if (!gCursorLocked) {
    return;
  }

  auto app     = (App *)glfwGetWindowUserPointer(window);
  auto xoffset = xpos - gLastXPos;
  auto yoffset = ypos - gLastYPos;
  gLastXPos    = xpos;
  gLastYPos    = ypos;

  if (gCursorWasFree) {
    gCursorWasFree = false;
    return;
  }

  auto sensitivity = 0.005f;

  app->mCamera.yaw -= xoffset * sensitivity;
  app->mCamera.pitch -= yoffset * sensitivity;

  app->mCamera.yaw   = std::fmod(app->mCamera.yaw, 360.f);
  app->mCamera.pitch = std::clamp(app->mCamera.pitch, -89.f, 89.f);
}

static void MouseInputWindowUpdate(GLFWwindow *window) {
  if (!gCursorLocked) {
    return;
  }
  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

  auto app     = (App *)glfwGetWindowUserPointer(window);
  auto xoffset = xpos - gLastXPos;
  auto yoffset = ypos - gLastYPos;
  gLastXPos    = xpos;
  gLastYPos    = ypos;

  if (gCursorWasFree) {
    gCursorWasFree = false;
    return;
  }

  auto sensitivity = 0.005f;

  app->mCamera.yaw -= xoffset * sensitivity;
  app->mCamera.pitch -= yoffset * sensitivity;

  app->mCamera.yaw   = std::fmod(app->mCamera.yaw, 360.f);
  app->mCamera.pitch = std::clamp(app->mCamera.pitch, -89.f, 89.f);
}

static void MouseScrollbackInputWindowCallback(GLFWwindow *window, double xoffset, double yoffset) {
  auto app = (App *)glfwGetWindowUserPointer(window);

  app->mCamera.fov -= (float)yoffset;
  app->mCamera.fov = std::clamp(app->mCamera.fov, 1.f, 45.f);
}

static void ResizeWindowCallback(GLFWwindow *window, int width, int height) {
  auto app           = (App *)glfwGetWindowUserPointer(window);
  app->mScreenWidth  = width;
  app->mScreenHeight = height;
  app->mRenderer.RequestResize(window);
}

void App::Init() {
  mScreenWidth  = 1920;
  mScreenHeight = 1080;
  CHECK(glfwInit());
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  pWindowHandle = glfwCreateWindow(mScreenWidth, mScreenHeight, "Application", nullptr, nullptr);
  CHECK(pWindowHandle);
  glfwSetInputMode(pWindowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  if (glfwRawMouseMotionSupported()) {
    std::cout << "RAW MOUSE INPUT ENABLED !!" << std::endl;
    glfwSetInputMode(pWindowHandle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
  }
  glfwSetWindowUserPointer(pWindowHandle, this);
  glfwSetFramebufferSizeCallback(pWindowHandle, ResizeWindowCallback);
  glfwSetKeyCallback(pWindowHandle, KeyboardInputWindowCallback);
  // glfwSetCursorPosCallback(pWindowHandle, MouseInputWindowCallback);
  glfwSetScrollCallback(pWindowHandle, MouseScrollbackInputWindowCallback);
  mRenderer.Init(pWindowHandle);

  mCamera.pos  = glm::vec3(0, 0, 8);
  mCamera.far  = 100.f;
  mCamera.near = 0.1f;
  mCamera.fov  = 45.f;
  mCamera.LookAt({0, 0, 0});
}

struct CameraData {
  glm::mat4 mProjView;
  glm::vec4 mPos;
  glm::vec4 mFront;
};

struct LightData {
  PointLight       pointLights[64];
  DirectionalLight directionalLights[64];
  SpotLight        spotLights[64];
  uint32_t         lightsCount;
  uint32_t         directionalLightsCount;
  uint32_t         spotLightsCount;
};

void App::Run() {
  int width, height, alpha_chan;
  stbi_set_flip_vertically_on_load(true);
  uint8_t *data = stbi_load("C:\\Users\\mikep\\Desktop\\KaosEngine\\assets\\container2.png", &width, &height, &alpha_chan, STBI_rgb_alpha);

  ImageDesc image_desc{};
  image_desc.rate         = UPDATE_RATE_NEVER;
  image_desc.name         = "texture cube";
  image_desc.type         = IMAGE_TYPE_TEXTURE2D;
  image_desc.format       = VK_FORMAT_R8G8B8A8_SRGB;
  image_desc.width        = width;
  image_desc.height       = height;
  image_desc.mipmapped    = true;
  image_desc.pData        = data;
  auto container2_texture = mRenderer.CreateTexture(image_desc);
  stbi_image_free(data);

  data = stbi_load("C:\\Users\\mikep\\Desktop\\KaosEngine\\assets\\container2_specular.png", &width, &height, &alpha_chan, STBI_rgb_alpha);

  image_desc.name                  = "specular map cube";
  image_desc.width                 = width;
  image_desc.height                = height;
  image_desc.pData                 = data;
  auto container2_specular_texture = mRenderer.CreateImage(image_desc);
  stbi_image_free(data);

  BufferDesc buffer_desc{};
  buffer_desc.rate = UPDATE_RATE_NEVER;

  buffer_desc.name     = "vertices";
  buffer_desc.size     = vertices.size() * sizeof(Vertex);
  buffer_desc.pData    = vertices.data();
  buffer_desc.type     = BUFFER_TYPE_VERTEX;
  auto vertices_buffer = mRenderer.CreateBuffer(buffer_desc);

  buffer_desc.name    = "indices";
  buffer_desc.size    = indices.size() * sizeof(uint32_t);
  buffer_desc.pData   = indices.data();
  buffer_desc.type    = BUFFER_TYPE_INDEX;
  auto indices_buffer = mRenderer.CreateBuffer(buffer_desc);

  buffer_desc.rate   = UPDATE_RATE_PER_FRAME;
  buffer_desc.name   = "gCamera";
  buffer_desc.size   = sizeof(CameraData);
  buffer_desc.pData  = nullptr;
  buffer_desc.type   = BUFFER_TYPE_UNKNOWN;
  auto camera_buffer = mRenderer.CreateBuffer(buffer_desc);

  buffer_desc.rate   = UPDATE_RATE_PER_FRAME;
  buffer_desc.name   = "gLightsBuffer";
  buffer_desc.size   = sizeof(LightData);
  buffer_desc.pData  = nullptr;
  buffer_desc.type   = BUFFER_TYPE_UNKNOWN;
  auto lights_buffer = mRenderer.CreateBuffer(buffer_desc);

  DescriptorDesc descriptor_desc{};
  descriptor_desc.shader_stages = VK_SHADER_STAGE_ALL_GRAPHICS;
  descriptor_desc.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .AddBinding(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
      .AddBinding(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
      .AddBinding(4, VK_DESCRIPTOR_TYPE_SAMPLER);
  auto set = mRenderer.CreateDescriptorSet(descriptor_desc);

  for (auto &frame : mRenderer.mFrames) {
    auto &frame_set = frame.mDescriptorSet;

    auto &camera_frame_buffer = frame.mUniformBuffers["gCamera"];
    mRenderer.WriteBufferDescriptorSet(0, camera_frame_buffer, frame_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    auto &lights_frame_buffer = frame.mUniformBuffers["gLightsBuffer"];
    mRenderer.WriteBufferDescriptorSet(1, lights_frame_buffer, frame_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    mRenderer.WriteImageDescriptorSet(2, container2_texture.mImage, frame_set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    mRenderer.WriteImageDescriptorSet(3, container2_specular_texture, frame_set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    mRenderer.WriteSamplerDescriptorSet(4, container2_texture.mSampler, frame_set, VK_DESCRIPTOR_TYPE_SAMPLER);
  }

  VkPushConstantRange range{};
  range.size       = sizeof(PushConstants);
  range.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;

  auto vertex_shader         = mRenderer.CreateShader("C:\\Users\\mikep\\Desktop\\KaosEngine\\shaders\\phong.slang", "vertMain");
  auto fragment_shader       = mRenderer.CreateShader("C:\\Users\\mikep\\Desktop\\KaosEngine\\shaders\\phong.slang", "fragMain");
  auto light_fragment_shader = mRenderer.CreateShader("C:\\Users\\mikep\\Desktop\\KaosEngine\\shaders\\phong.slang", "fragLight");

  std::vector<VkPushConstantRange>   ranges      = {range};
  std::vector<VkDescriptorSetLayout> set_layouts = {set.pDescriptorSetLayout};

  GraphicPipelineDesc pipeline_desc{};
  pipeline_desc.pipeline_layout_desc.layouts = set_layouts;
  pipeline_desc.pipeline_layout_desc.ranges  = ranges;

  pipeline_desc.SetShaders(vertex_shader.pShader, fragment_shader.pShader)
      .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
      .SetPolygonMode(VK_POLYGON_MODE_FILL)
      .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
      .SetMultiSamplingNone()
      .DisableBlending()
      .EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

  auto object_pipeline = mRenderer.CreateGraphicsPipeline(pipeline_desc);
  pipeline_desc.SetShaders(vertex_shader.pShader, light_fragment_shader.pShader);

  auto light_pipeline = mRenderer.CreateGraphicsPipeline(pipeline_desc);
  mRenderer.DestroyShader(light_fragment_shader);
  mRenderer.DestroyShader(fragment_shader);
  mRenderer.DestroyShader(vertex_shader);

  RenderObject object{};
  object.useIndexBuffer    = false;
  object.indexBuffer       = indices_buffer;
  object.indexCount        = indices.size();
  object.vertexCount       = vertices.size();
  object.pushConstantsSize = 0;

  std::vector<RenderObject> objects{};

  RenderContext context{};
  context.objects = objects;

  // std::vector<SpotLight> spot_lights{};
  std::vector<SpotLight> spot_lights{{{glm::vec4(mCamera.pos, 1.f)},
                                      {glm::vec4(mCamera.GetFront(), 0.f)},
                                      {0.f, 0.f, 0.f, 0.f},
                                      {1.f, 1.f, 1.f, 0.f},
                                      {1.f, 1.f, 1.f, 0.f},
                                      cos(glm::radians(12.5f)),
                                      cos(glm::radians(6.25f))}};
  // std::vector<PointLight> lights{};
  std::vector<PointLight> lights{
      {{1.2f, 1.0, 2.0f, 1.f}, {0.01f, 0.01f, 0.01f, 0.f}, {1.f, 1.f, 1.f, 0.f}, {1.f, 1.f, 1.f, 0.f}, 1.f, 0.09f, 0.032f}};
  std::vector<DirectionalLight> dir_lights{};
  // std::vector<DirectionalLight>         dir_lights{{{-0.2f, -1.0f, -0.3f, 0.f}, {0.f, 0.f, 0.f, 0.f}, {0.5f, 0.5f, 0.5f, 0.f}, {1.f, 1.f, 1.f,
  // 0.f}}};
  std::vector<glm::vec3>     object_positions{glm::vec3(0.0f, 0.0f, 0.0f),     glm::vec3(2.0f, 5.0f, -15.0f), glm::vec3(-1.5f, -2.2f, -2.5f),
                                              glm::vec3(-3.8f, -2.0f, -12.3f), glm::vec3(2.4f, -0.4f, -3.5f), glm::vec3(-1.7f, 3.0f, -7.5f),
                                              glm::vec3(1.3f, -2.0f, -2.5f),   glm::vec3(1.5f, 2.0f, -2.5f),  glm::vec3(1.5f, 0.2f, -1.5f),
                                              glm::vec3(-1.3f, 1.0f, -1.5f)};
  std::vector<PushConstants> pcs(lights.size() + object_positions.size());

  auto frame     = 0;
  auto last_time = (float)glfwGetTime();
  while (!glfwWindowShouldClose(pWindowHandle)) {
    auto now  = (float)glfwGetTime();
    dt        = now - last_time;
    last_time = now;
    if (frame++ % 100 == 1) {
      std::cout << "FPS : " << 1 / dt << "\n";
    }

    glfwPollEvents();

    if (mScreenWidth == 0 || mScreenHeight == 0) {
      continue;
    }

    MouseInputWindowUpdate(pWindowHandle);
    KeyboardInputWindowUpdate(pWindowHandle);

    // lights[0].position = glm::rotate(glm::mat4(1.f), (float)dt, {0, 1, 0}) * lights[0].position;
    spot_lights[0].position  = glm::vec4(mCamera.pos, 1);
    spot_lights[0].direction = glm::vec4(mCamera.GetFront(), 0);

    context.objects.clear();

    CameraData cam_data;
    cam_data.mProjView = mCamera.GetViewProj((float)mScreenWidth / (float)mScreenHeight);
    cam_data.mPos      = glm::vec4(mCamera.pos, 0);
    cam_data.mFront    = glm::vec4(mCamera.GetFront(), 0);
    camera_buffer      = mRenderer.GetPerFrameBuffer("gCamera");
    std::memcpy(camera_buffer.mVmaAllocationInfo.pMappedData, &cam_data, sizeof(CameraData));

    LightData light_data;
    light_data.lightsCount            = lights.size();
    light_data.directionalLightsCount = dir_lights.size();
    light_data.spotLightsCount        = spot_lights.size();
    std::memcpy(light_data.pointLights, lights.data(), lights.size() * sizeof(PointLight));
    std::memcpy(light_data.directionalLights, dir_lights.data(), dir_lights.size() * sizeof(DirectionalLight));
    std::memcpy(light_data.spotLights, spot_lights.data(), spot_lights.size() * sizeof(SpotLight));
    lights_buffer = mRenderer.GetPerFrameBuffer("gLightsBuffer");
    std::memcpy(lights_buffer.mVmaAllocationInfo.pMappedData, &light_data, sizeof(LightData));

    auto current_set = mRenderer.GetPerFrameDescriptorSet();

    VkBufferDeviceAddressInfo addr_info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addr_info.buffer = vertices_buffer.pBuffer;

    for (size_t i = 0; i < lights.size(); i++) {
      auto &pos = lights[i].position;
      auto &pc  = pcs[i];

      pc.pVertexBufferAddress = vkGetBufferDeviceAddress(mRenderer.pDevice, &addr_info);
      pc.mMaterial            = {32};
      pc.mModel               = glm::scale(glm::translate(glm::mat4(1.f), glm::vec3(pos)), glm::vec3(0.2f));
      pc.mTransformedModel    = glm::transpose(glm::inverse(pc.mModel));

      RenderObject o      = object;
      o.pipeline          = light_pipeline;
      o.usePushConstants  = true;
      o.pPushConstants    = &pc;
      o.pushConstantsSize = sizeof(pc);
      o.useDescriptorSet  = true;
      o.descriptorSet     = current_set;

      context.objects.emplace_back(o);
    }

    for (size_t i = lights.size(); i < lights.size() + object_positions.size(); i++) {
      auto &pos = object_positions[i - lights.size()];
      auto &pc  = pcs[i];

      pc.pVertexBufferAddress = vkGetBufferDeviceAddress(mRenderer.pDevice, &addr_info);
      pc.mMaterial            = {32};
      pc.mModel               = glm::rotate(glm::translate(glm::mat4(1.f), pos), glm::radians(20.0f * i), {1.0f, 0.3f, 0.5f});
      pc.mTransformedModel    = glm::transpose(glm::inverse(pc.mModel));

      RenderObject o      = object;
      o.pipeline          = object_pipeline;
      o.usePushConstants  = true;
      o.pPushConstants    = &pc;
      o.pushConstantsSize = sizeof(pc);
      o.useDescriptorSet  = true;
      o.descriptorSet     = current_set;

      context.objects.emplace_back(o);
    }

    mRenderer.Draw(context);
  }

  vkDeviceWaitIdle(mRenderer.pDevice);
  // mRenderer.DestroyBuffer(lights_buffer); no need it is destroyed by the renderer since there is a buffer in each frame (MAX_IN_FLIGHT or whatever)
  // mRenderer.DestroyBuffer(camera_buffer); no need it is destroyed by the renderer since there is a buffer in each frame (MAX_IN_FLIGHT or whatever)
  mRenderer.DestroyImage(container2_specular_texture);
  mRenderer.DestroyTexture(container2_texture);
  mRenderer.DestroyBuffer(indices_buffer);
  mRenderer.DestroyBuffer(vertices_buffer);
  mRenderer.DestroyPipeline(light_pipeline);
  mRenderer.DestroyPipeline(object_pipeline);
  mRenderer.DestroyDescriptorSet(set);
}

App::~App() {
  glfwDestroyWindow(pWindowHandle);
  glfwTerminate();
}
