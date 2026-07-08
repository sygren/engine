#include "app.hpp"
#include "GLFW/glfw3.h"
#include "core.hpp"
#include "renderer.hpp"
#include <filesystem>
#include <iostream>
#include <print>

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
  std::println("Renderer successfully initialized!");
  mAssetManager.Init(&mRenderer);
  std::println("AssetManager successfully initialized!");

  mCamera.pos  = glm::vec3(0, 0, 8);
  mCamera.far  = 100.f;
  mCamera.near = 0.1f;
  mCamera.fov  = 45.f;
  mCamera.LookAt({0, 0, 0});
}

struct Transform {
  glm::mat4 modelMatrix;
  glm::mat4 normalMatrix;
};

struct PushConstantsData {
  uint64_t pVertexBufferAddress;
  uint32_t transformIndex;
  uint32_t materialIndex;
};

struct HighlightPushConstantsData {
      uint64_t pVertexBufferAddress ; // 8 bytes
      uint32_t     transformIndex;                               // 4 bytes
      glm::vec4   color;
      float    scale;
};

struct CameraData {
  glm::mat4 projView;
  glm::vec4 pos;
  glm::vec4 front;
};

struct LightCounts {
  uint32_t lightsCount;
  uint32_t directionalLightsCount;
  uint32_t spotLightsCount;
};

constexpr uint32_t MAX_TRANSFORMS   = 1000;
constexpr uint32_t MAX_POINT_LIGHTS = 32;
constexpr uint32_t MAX_SPOT_LIGHTS  = 32;
constexpr uint32_t MAX_DIR_LIGHTS   = 32;
constexpr uint32_t MAX_MATERIALS    = 1000;

void App::Run() {
  const auto &model_handle_opt = mAssetManager.GetGltfModel("C:\\Users\\mikep\\Desktop\\KaosEngine\\assets\\backpack\\scene.gltf");
  const auto &model_handle     = model_handle_opt.value();

  std::println("RUN: Successfully created all models");

  BufferDesc camera_desc{.name = "gCamera", .pData = nullptr, .size = sizeof(CameraData), .rate = UPDATE_RATE_PER_FRAME, .type = BUFFER_TYPE_UBO};
  auto       camera_data_id = mRenderer.CreateBuffer(camera_desc);

  BufferDesc light_count_desc{
      .name = "gLightsCount", .pData = nullptr, .size = sizeof(LightCounts), .rate = UPDATE_RATE_PER_FRAME, .type = BUFFER_TYPE_UBO};
  auto light_count_data_id = mRenderer.CreateBuffer(light_count_desc);

  BufferDesc transform_desc{.name = "gTransforms", .pData = nullptr, .size = sizeof(Transform) * MAX_TRANSFORMS, .rate = UPDATE_RATE_PER_FRAME, .type = BUFFER_TYPE_SSBO};
  auto       transform_data_id = mRenderer.CreateBuffer(transform_desc);

  BufferDesc point_lights_desc{.name  = "gPointLights",
                               .pData = nullptr,
                               .size  = sizeof(PointLight) * MAX_POINT_LIGHTS,
                               .rate  = UPDATE_RATE_PER_FRAME,
                               .type  = BUFFER_TYPE_SSBO};
  auto       _p = mRenderer.CreateBuffer(point_lights_desc);
  BufferDesc directional_light_desc{.name  = "gDirectionnalLights",
                                    .pData = nullptr,
                                    .size  = sizeof(DirectionalLight) * MAX_DIR_LIGHTS,
                                    .rate  = UPDATE_RATE_PER_FRAME,
                                    .type  = BUFFER_TYPE_SSBO};
  auto       _d = mRenderer.CreateBuffer(directional_light_desc);
  BufferDesc spotlights_desc{
      .name = "gSpotLights", .pData = nullptr, .size = sizeof(SpotLight) * MAX_SPOT_LIGHTS, .rate = UPDATE_RATE_PER_FRAME, .type = BUFFER_TYPE_SSBO};
  auto       _s = mRenderer.CreateBuffer(spotlights_desc);
  BufferDesc material_desc{
      .name = "gMaterials", .pData = nullptr, .size = sizeof(Material) * MAX_MATERIALS, .rate = UPDATE_RATE_PER_FRAME, .type = BUFFER_TYPE_SSBO};
  auto _m = mRenderer.CreateBuffer(material_desc);

  std::println("RUN: Successfully created all buffers");

  DescriptorDesc descriptor_desc{};
  descriptor_desc.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
      .AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
      .AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
      .AddBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
      .AddBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  descriptor_desc.shader_stages = VK_SHADER_STAGE_ALL_GRAPHICS;
  auto set                      = mRenderer.CreateDescriptorSet(descriptor_desc);

  std::println("RUN: Successfully created descriptor set engine side");

  for (auto &frame : mRenderer.mFrames) {
    mRenderer.WriteBufferDescriptorSet(0, mRenderer.mBuffers.at(frame.mUniformBuffers.at("gTransforms")), frame.mDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    mRenderer.WriteBufferDescriptorSet(1, mRenderer.mBuffers.at(frame.mUniformBuffers.at("gCamera")), frame.mDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    mRenderer.WriteBufferDescriptorSet(2, mRenderer.mBuffers.at(frame.mUniformBuffers.at("gLightsCount")), frame.mDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    mRenderer.WriteBufferDescriptorSet(3, mRenderer.mBuffers.at(frame.mUniformBuffers.at("gPointLights")), frame.mDescriptorSet , VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    mRenderer.WriteBufferDescriptorSet(4, mRenderer.mBuffers.at(frame.mUniformBuffers.at("gDirectionnalLights")), frame.mDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    mRenderer.WriteBufferDescriptorSet(5, mRenderer.mBuffers.at(frame.mUniformBuffers.at("gSpotLights")), frame.mDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    mRenderer.WriteBufferDescriptorSet(6, mRenderer.mBuffers.at(frame.mUniformBuffers.at("gMaterials")), frame.mDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  }

  auto shader_vertex   = mRenderer.CreateShader("C:\\Users\\mikep\\Desktop\\KaosEngine\\shaders\\phong.slang", "vertMain");
  auto shader_fragment = mRenderer.CreateShader("C:\\Users\\mikep\\Desktop\\KaosEngine\\shaders\\phong.slang", "fragMain");
  auto shader_depth_fragment = mRenderer.CreateShader("C:\\Users\\mikep\\Desktop\\KaosEngine\\shaders\\phong.slang", "fragDepth");

  auto shader_highlight_vertex = mRenderer.CreateShader("C:\\Users\\mikep\\Desktop\\KaosEngine\\shaders\\highlight.slang", "vertMain");
  auto shader_highlight_fragment = mRenderer.CreateShader("C:\\Users\\mikep\\Desktop\\KaosEngine\\shaders\\highlight.slang", "fragStencil");

  StencilDesc stencil_desc{};
  stencil_desc.compare_op = VK_COMPARE_OP_ALWAYS;
  stencil_desc.reference = 0xFF;
  stencil_desc.compare_mask = 0xFF;
  stencil_desc.write_mask = 0xFF;
  stencil_desc.fail_op = VK_STENCIL_OP_KEEP;
  stencil_desc.depth_fail_op = VK_STENCIL_OP_KEEP;
  stencil_desc.pass_op = VK_STENCIL_OP_REPLACE;

  PipelineDesc pipeline_desc{};

  std::vector<VkPushConstantRange>   range{{.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS, .size = sizeof(PushConstantsData)}};
  std::vector<VkDescriptorSetLayout> layouts{mRenderer.mTexturesSet.pDescriptorSetLayout, set.pDescriptorSetLayout};
  pipeline_desc.ranges  = range;
  pipeline_desc.layouts = layouts;

  GraphicPipelineDesc graphic_pipeline_desc{};
  graphic_pipeline_desc.pipeline_layout_desc = pipeline_desc;
  graphic_pipeline_desc.SetShaders(shader_vertex.pShader, shader_fragment.pShader)
      .SetPolygonMode(VK_POLYGON_MODE_FILL)
      .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
      .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
      .EnableDepthTest(VK_TRUE, VK_COMPARE_OP_GREATER)
      .EnableStencilTest(stencil_desc)
      .DisableBlending()
      .SetMultiSamplingNone();
  auto pipeline = mRenderer.CreateGraphicsPipeline(graphic_pipeline_desc);

  graphic_pipeline_desc.SetShaders(shader_vertex.pShader, shader_depth_fragment.pShader).DisableStencilTest();
  auto depth_pipeline = mRenderer.CreateGraphicsPipeline(graphic_pipeline_desc);

  stencil_desc.compare_op = VK_COMPARE_OP_NOT_EQUAL;
  stencil_desc.reference = 0xFF;
  stencil_desc.pass_op = VK_STENCIL_OP_KEEP;
  stencil_desc.write_mask = 0x00;

  range[0] = {.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS, .size=sizeof(HighlightPushConstantsData)};
  graphic_pipeline_desc.SetShaders(shader_highlight_vertex.pShader, shader_highlight_fragment.pShader).DisableDepthTest().EnableStencilTest(stencil_desc);
  auto highlight_pipeline = mRenderer.CreateGraphicsPipeline(graphic_pipeline_desc);
  std::println("RUN: Graphic pipelines successfully created!");

  mRenderer.DestroyShader(shader_highlight_fragment);
  mRenderer.DestroyShader(shader_highlight_vertex);
  mRenderer.DestroyShader(shader_depth_fragment);
  mRenderer.DestroyShader(shader_fragment);
  mRenderer.DestroyShader(shader_vertex);

  RenderContext context{};

  auto                                       *model = model_handle.Get();
  std::vector<std::vector<PushConstantsData>> pcs(model->meshes.size());
  std::vector<std::vector<HighlightPushConstantsData>> highlight_pcs(model->meshes.size());
  std::vector<Transform>                      transforms;
  std::vector<Material>                       materials;

  std::vector<PointLight>       point_lights;
  std::vector<DirectionalLight> directional_lights ;
  std::vector<SpotLight>        spot_lights {{glm::vec4(0), glm::vec4(0), glm::vec4(0.f), glm::vec4(0.5f), glm::vec4(0.5f), cos(glm::radians(12.5f)), cos(glm::radians(6.25f))}};

  std::println("There is {} meshes in the model", model->meshes.size());
  for (size_t i = 0; i < model->meshes.size(); i++) {
    std::println("Mesh {} with {} submeshes", i, model->meshes[i].submeshes.size());
    pcs[i].resize(model->meshes[i].submeshes.size());
    highlight_pcs[i].resize(model->meshes[i].submeshes.size());
  }

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

    context.objects.clear();
    transforms.clear();
    materials.clear();

    spot_lights[0].position = glm::vec4(mCamera.pos, 1.f);
    spot_lights[0].direction = glm::vec4(glm::normalize(mCamera.GetFront()), 0.f);

    for (size_t i = 0; i < model->meshes.size(); i++) {
      const auto               &mesh = model->meshes[i];
      VkBufferDeviceAddressInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = mRenderer.mBuffers[mesh.vertexBuffer].pBuffer};
      auto                      scale_matrix = glm::scale(glm::mat4(1.f), glm::vec3(0.01f));
      auto model = scale_matrix * mesh.transform;
      auto normal = glm::transpose(glm::inverse(glm::mat3(model)));
      transforms.push_back({model, glm::mat4(normal)});

      for (size_t j = 0; j < mesh.submeshes.size(); j++) {
        auto &pc      = pcs[i][j];
        auto &highlight_pc = highlight_pcs[i][j];
        auto &submesh = mesh.submeshes[j];

        pc.pVertexBufferAddress = vkGetBufferDeviceAddress(mRenderer.pDevice, &info);
        pc.transformIndex       = transforms.size() - 1;

        highlight_pc.pVertexBufferAddress = pc.pVertexBufferAddress;
        highlight_pc.transformIndex = pc.transformIndex;

        if (submesh.material.baseColorTexture == INVALID_ID) {
            std::println("Meshes [{}][{}] has invalid baseColorTexture", i, j);
        }
        if (submesh.material.metallicRoughnessTexture == INVALID_ID) {
                    std::println("Meshes [{}][{}] has invalid metallicRoughnessTexture", i, j);
        }

        materials.push_back(submesh.material);
        pc.materialIndex = materials.size() - 1;

        RenderObject object{};

        object.pipeline = pipeline;

        object.usePushConstants  = true;
        object.pPushConstants    = &pc;
        object.pushConstantsSize = sizeof(PushConstantsData);

        object.useDescriptorSet = true;
        object.descriptorSet    = mRenderer.GetPerFrameDescriptorSet();

        object.useIndexBuffer = true;
        object.indexBuffer    = mRenderer.mBuffers[mesh.indexBuffer];
        object.indexCount     = submesh.indexCount;
        object.indexOffset    = submesh.indexOffset;

        object.vertexCount  = mRenderer.mBuffers[mesh.vertexBuffer].mVmaAllocationInfo.size / sizeof(Vertex);
        object.vertexOffset = submesh.vertexOffset;

        context.objects.push_back(object);
      }
    }

    for (size_t i = 0; i < model->meshes.size(); i++) {
      const auto               &mesh = model->meshes[i];
      for (size_t j = 0; j < mesh.submeshes.size(); j++) {
        auto &highlight_pc = highlight_pcs[i][j];
        auto &submesh = mesh.submeshes[j];
        highlight_pc.scale = 1.03f;
        highlight_pc.color = glm::vec4(1.0f, 0.f,0.f, 1.f);
        RenderObject object{};

        object.pipeline = highlight_pipeline;

        object.usePushConstants  = true;
        object.pPushConstants    = &highlight_pc;
        object.pushConstantsSize = sizeof(HighlightPushConstantsData);

        object.useDescriptorSet = true;
        object.descriptorSet    = mRenderer.GetPerFrameDescriptorSet();

        object.useIndexBuffer = true;
        object.indexBuffer    = mRenderer.mBuffers[mesh.indexBuffer];
        object.indexCount     = submesh.indexCount;
        object.indexOffset    = submesh.indexOffset;

        object.vertexCount  = mRenderer.mBuffers[mesh.vertexBuffer].mVmaAllocationInfo.size / sizeof(Vertex);
        object.vertexOffset = submesh.vertexOffset;

        context.objects.push_back(object);
      }
    }

    CameraData cam_data{};
    cam_data.projView = mCamera.GetViewProj((float)mScreenWidth / mScreenHeight);
    cam_data.pos      = glm::vec4(mCamera.pos, 1.f);
    cam_data.front    = glm::vec4(mCamera.GetFront(), 0.f);
    LightCounts light_counts{.lightsCount            = (uint32_t)point_lights.size(),
                             .directionalLightsCount = (uint32_t)directional_lights.size(),
                             .spotLightsCount        = (uint32_t)spot_lights.size()};

    std::memcpy(mRenderer.mBuffers[mRenderer.GetPerFrameBuffer("gLightsCount")].mVmaAllocationInfo.pMappedData, &light_counts, sizeof(LightCounts));
    std::memcpy(mRenderer.mBuffers[mRenderer.GetPerFrameBuffer("gPointLights")].mVmaAllocationInfo.pMappedData, point_lights.data(), point_lights.size() * sizeof(PointLight));
    std::memcpy(mRenderer.mBuffers[mRenderer.GetPerFrameBuffer("gDirectionnalLights")].mVmaAllocationInfo.pMappedData, directional_lights.data(), directional_lights.size() * sizeof(DirectionalLight));
    std::memcpy(mRenderer.mBuffers[mRenderer.GetPerFrameBuffer("gSpotLights")].mVmaAllocationInfo.pMappedData, spot_lights.data(), spot_lights.size() * sizeof(SpotLight));
    std::memcpy(mRenderer.mBuffers[mRenderer.GetPerFrameBuffer("gTransforms")].mVmaAllocationInfo.pMappedData, transforms.data(), sizeof(Transform) * transforms.size());
    std::memcpy(mRenderer.mBuffers[mRenderer.GetPerFrameBuffer("gCamera")].mVmaAllocationInfo.pMappedData, &cam_data, sizeof(CameraData));
    std::memcpy(mRenderer.mBuffers[mRenderer.GetPerFrameBuffer("gMaterials")].mVmaAllocationInfo.pMappedData, materials.data(), materials.size() * sizeof(Material));

    mRenderer.Draw(context);
  }

  vkDeviceWaitIdle(mRenderer.pDevice);

  mRenderer.DestroyPipeline(highlight_pipeline);
  mRenderer.DestroyPipeline(depth_pipeline);
  mRenderer.DestroyPipeline(pipeline);
  mRenderer.DestroyDescriptorSet(set);
  mRenderer.DestroyBuffer(camera_data_id);
}

App::~App() {
  glfwDestroyWindow(pWindowHandle);
  glfwTerminate();
}
