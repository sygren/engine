#include "app.hpp"
#include "GLFW/glfw3.h"
#include "core.hpp"
#include "renderer/render_pass.hpp"
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
    std::vector<Vertex> vertices = {
        {.position = glm::vec4(-0.5f, -0.5f, 0.0f, 1.f), .uv = glm::vec2(0.0f, 0.0f)},
        {.position = glm::vec4(-0.5f,  0.5f, 0.0f, 1.f), .uv = glm::vec2(0.0f, 1.0f)},
        {.position = glm::vec4( 0.5f,  0.5f, 0.0f, 1.f), .uv = glm::vec2(1.0f, 1.0f)},
        {.position = glm::vec4( 0.5f, -0.5f, 0.0f, 1.f), .uv = glm::vec2(1.0f, 0.0f)}
    };

    std::vector<uint32_t> indices = {
        0, 2, 1,
        2, 0, 3
    };

  auto grass_tex_handle = mAssetManager.GetTexture("C:\\Users\\mikep\\Desktop\\KaosEngine\\assets\\grass.png", true);
  auto backpack_model_handle = mAssetManager.GetGltfModel("C:\\Users\\mikep\\Desktop\\KaosEngine\\assets\\backpack\\scene.gltf");

  if (!grass_tex_handle.has_value() || !backpack_model_handle.has_value()) {
      std::println("ERROR COULDTR LOAD GFRASS TEXTURE OR BACKPACK MODEL!!");
      abort();
  }

  BufferDesc camera_desc{.name = "gCamera", .pData = nullptr, .size = sizeof(CameraData), .rate = UPDATE_RATE_PER_FRAME, .type = BUFFER_TYPE_UBO};
  mRenderer.CreateBuffer(camera_desc);
  BufferDesc transform_desc{.name = "gTransforms", .pData = nullptr, .size = sizeof(Transform) * MAX_TRANSFORMS, .rate = UPDATE_RATE_PER_FRAME, .type = BUFFER_TYPE_SSBO};
  mRenderer.CreateBuffer(transform_desc);
  BufferDesc material_desc{.name = "gMaterials", .pData = nullptr, .size = sizeof(Material) * MAX_MATERIALS, .rate = UPDATE_RATE_PER_FRAME, .type = BUFFER_TYPE_SSBO};
  mRenderer.CreateBuffer(material_desc);
  std::println("RUN: Successfully created all buffers");

  BufferDesc desc = {.pData = vertices.data(), .size = vertices.size() * sizeof(Vertex), .rate = UPDATE_RATE_NEVER, .type = BUFFER_TYPE_SSBO, .usage = BUFFER_USAGE_VERTEX};
  auto square_vertices_buf = mRenderer.CreateBuffer(desc);

  desc.pData = indices.data();
  desc.size = indices.size() * sizeof(uint32_t);
  desc.usage = BUFFER_USAGE_INDEX;
  auto square_indices_buf = mRenderer.CreateBuffer(desc);

  DescriptorDesc descriptor_desc{};
  descriptor_desc.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
      .AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  descriptor_desc.shader_stages = VK_SHADER_STAGE_ALL_GRAPHICS;
  auto set                      = mRenderer.CreateDescriptorSet(descriptor_desc);
  std::println("RUN: Successfully created descriptor set engine side");

  for (auto &frame : mRenderer.mFrames) {
    mRenderer.WriteBufferDescriptorSet(0, mRenderer.mBuffers.at(frame.mUniformBuffers.at("gTransforms")), frame.mDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    mRenderer.WriteBufferDescriptorSet(1, mRenderer.mBuffers.at(frame.mUniformBuffers.at("gCamera")), frame.mDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    mRenderer.WriteBufferDescriptorSet(2, mRenderer.mBuffers.at(frame.mUniformBuffers.at("gMaterials")), frame.mDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  }

  auto shader_vertex   = mRenderer.CreateShader("C:\\Users\\mikep\\Desktop\\KaosEngine\\shaders\\basic.slang", "vertMain");
  auto shader_fragment = mRenderer.CreateShader("C:\\Users\\mikep\\Desktop\\KaosEngine\\shaders\\basic.slang", "fragMain");

  // StencilDesc stencil_desc{};
  // stencil_desc.compare_op = VK_COMPARE_OP_ALWAYS;
  // stencil_desc.reference = 0xFF;
  // stencil_desc.compare_mask = 0xFF;
  // stencil_desc.write_mask = 0xFF;
  // stencil_desc.fail_op = VK_STENCIL_OP_KEEP;
  // stencil_desc.depth_fail_op = VK_STENCIL_OP_KEEP;
  // stencil_desc.pass_op = VK_STENCIL_OP_REPLACE;

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
      .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
      .EnableDepthTest(VK_TRUE, VK_COMPARE_OP_GREATER)
      .DisableStencilTest()
      .DisableBlending()
      .SetMultiSamplingNone();
  auto pipeline = mRenderer.CreateGraphicsPipeline(graphic_pipeline_desc);
  std::println("RUN: Graphic pipelines successfully created!");

  mRenderer.DestroyShader(shader_fragment);
  mRenderer.DestroyShader(shader_vertex);

  auto *backpack = backpack_model_handle->Get();

  RenderContext context{};
  std::vector<Transform>                      transforms;
  std::vector<Material>                       materials;
  std::vector<glm::vec3> grass_transforms{{2.5,0,0}};

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

    context.passes.clear();
    context.passes.push_back(RenderPass{});

    transforms.clear();
    materials.clear();


    for (size_t i = 0; i < backpack->meshes.size(); i ++) {
        auto& mesh = backpack->meshes[i];

        VkBufferDeviceAddressInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = mRenderer.mBuffers[mesh.vertexBuffer].pBuffer};
        VkDeviceAddress vertex_addr = vkGetBufferDeviceAddress(mRenderer.pDevice, &info);

        auto scale = glm::scale(glm::mat4(1), glm::vec3(0.01));
        auto transform = scale * mesh.transform;
        transforms.push_back({transform, glm::transpose(glm::inverse(transform))});

        for (size_t j = 0; j < mesh.submeshes.size(); j++) {
            PushConstantsData pc;

            materials.push_back(mesh.submeshes[j].material);
            pc.materialIndex = materials.size() - 1;
            pc.transformIndex = transforms.size() - 1;
            pc.pVertexBufferAddress = vertex_addr;

            RenderObject object{};

            object.pipeline = pipeline;

            object.descriptor_set = mRenderer.GetPerFrameDescriptorSet();

            PushConstants pc_data;
            pc_data.write(pc);
            object.push_constants = pc_data;

            object.vertices.count = mRenderer.mBuffers[mesh.vertexBuffer].mVmaAllocationInfo.size / sizeof(Vertex);
            object.vertices.offset = mesh.submeshes[j].vertexOffset;

            IndicesDesc indices_desc{};
            indices_desc.count = mesh.submeshes[j].indexCount;
            indices_desc.offset = mesh.submeshes[j].indexOffset;
            indices_desc.buffer = mesh.indexBuffer;
            object.indices = indices_desc;

            context.passes[0].objects.push_back(object);
        }
    }

    for (size_t i = 0; i < grass_transforms.size(); i++) {
        auto& grass_p = grass_transforms[i];
        PushConstantsData pc;

        auto model = glm::translate(glm::mat4(1.f), grass_p);
        transforms.push_back({.modelMatrix = model, .normalMatrix = glm::transpose(glm::inverse(model))});
        materials.push_back({.baseColorTexture = grass_tex_handle->Id()});
        pc.materialIndex = materials.size() - 1;
        pc.transformIndex = transforms.size() - 1;
        VkBufferDeviceAddressInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = mRenderer.mBuffers[square_vertices_buf].pBuffer};
        pc.pVertexBufferAddress = vkGetBufferDeviceAddress(mRenderer.pDevice, &info);

        RenderObject object{};

        object.pipeline = pipeline;

        object.descriptor_set = mRenderer.GetPerFrameDescriptorSet();

        PushConstants pc_data;
        pc_data.write(pc);
        object.push_constants = pc_data;

        object.vertices.count = vertices.size();
        object.vertices.offset = 0;

        IndicesDesc indices_desc{};
        indices_desc.count = indices.size();
        indices_desc.offset = 0;
        indices_desc.buffer = square_indices_buf;
        object.indices = indices_desc;

        context.passes[0].objects.push_back(object);
    }

    CameraData cam_data{};
    cam_data.projView = mCamera.GetViewProj((float)mScreenWidth / mScreenHeight);
    cam_data.pos      = glm::vec4(mCamera.pos, 1.f);
    cam_data.front    = glm::vec4(mCamera.GetFront(), 0.f);

    std::memcpy(mRenderer.mBuffers[mRenderer.GetPerFrameBuffer("gTransforms")].mVmaAllocationInfo.pMappedData, transforms.data(), sizeof(Transform) * transforms.size());
    std::memcpy(mRenderer.mBuffers[mRenderer.GetPerFrameBuffer("gCamera")].mVmaAllocationInfo.pMappedData, &cam_data, sizeof(CameraData));
    std::memcpy(mRenderer.mBuffers[mRenderer.GetPerFrameBuffer("gMaterials")].mVmaAllocationInfo.pMappedData, materials.data(), materials.size() * sizeof(Material));

    mRenderer.Draw(context);
  }

  vkDeviceWaitIdle(mRenderer.pDevice);

  mRenderer.DestroyBuffer(square_vertices_buf);
  mRenderer.DestroyBuffer(square_indices_buf);
  mRenderer.DestroyPipeline(pipeline);
  mRenderer.DestroyDescriptorSet(set);
}

App::~App() {
  glfwDestroyWindow(pWindowHandle);
  glfwTerminate();
}
