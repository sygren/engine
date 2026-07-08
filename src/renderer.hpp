#pragma once

#include <unordered_map>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <slang/slang-com-ptr.h>
#include <slang/slang.h>
#include <span>
#include <vector>

#include "core.hpp"

struct SlangContext {
  Slang::ComPtr<slang::IGlobalSession> pGlobalSession;
  Slang::ComPtr<slang::ISession>       pSession;
};

struct GpuStat {
  uint32_t                    score;
  uint32_t                    graphicQueueIndex;
  uint32_t                    presentQueueIndex;
  uint32_t                    transferQueueIndex;
  uint32_t                    computeQueueIndex;
  VkPhysicalDeviceProperties2 properties;
};

struct PhysicalDevice {
  VkPhysicalDevice pPhysicalDevice;
  GpuStat          stat;
};

struct Swapchain {
  VkSwapchainKHR           pSwapchain;
  VkPresentModeKHR         mode;
  VkSurfaceFormatKHR       surfaceFormat;
  VkExtent2D               extent;
  std::vector<VkImage>     images;
  std::vector<VkImageView> views;
  std::vector<VkSemaphore> semaphores;
};

struct DescriptorDesc {
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  VkShaderStageFlags                        shader_stages;

  DescriptorDesc &AddBinding(uint32_t binding, VkDescriptorType type);
};

struct StencilDesc {
    VkCompareOp compare_op = VK_COMPARE_OP_ALWAYS;
    uint32_t reference = 0x0;
    uint32_t compare_mask = 0xFF;
    VkStencilOp fail_op = VK_STENCIL_OP_KEEP;
    VkStencilOp depth_fail_op = VK_STENCIL_OP_KEEP;
    VkStencilOp pass_op = VK_STENCIL_OP_KEEP;
    uint32_t write_mask = 0xFF;
};

struct PipelineDesc {
  std::span<VkPushConstantRange>   ranges;
  std::span<VkDescriptorSetLayout> layouts;
};

struct GraphicPipelineDesc {
  PipelineDesc pipeline_layout_desc;

  std::vector<VkPipelineShaderStageCreateInfo> stages;
  VkPipelineInputAssemblyStateCreateInfo       input_assembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  VkPipelineRasterizationStateCreateInfo       rasterizer     = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  VkPipelineColorBlendAttachmentState          color_blend_attachment;
  VkPipelineMultisampleStateCreateInfo         multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  VkPipelineDepthStencilStateCreateInfo        depth_stencil = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

  GraphicPipelineDesc &SetShaders(VkShaderModule vertex, VkShaderModule fragment);
  GraphicPipelineDesc &SetInputTopology(VkPrimitiveTopology topology);
  GraphicPipelineDesc &SetPolygonMode(VkPolygonMode mode);
  GraphicPipelineDesc &SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
  GraphicPipelineDesc &SetMultiSamplingNone();
  GraphicPipelineDesc &DisableBlending();
  GraphicPipelineDesc &DisableDepthTest();
  GraphicPipelineDesc &DisableStencilTest();
  GraphicPipelineDesc &EnableBlendingAdditive();
  GraphicPipelineDesc &EnableBlendingAlphaBend();
  GraphicPipelineDesc &EnableDepthTest(bool write_enable, VkCompareOp op);
  GraphicPipelineDesc &EnableStencilTest(const StencilDesc& desc);
};

struct Pipeline {
  VkPipeline       pPipeline;
  VkPipelineLayout pLayout;
};

enum UpdateRate {
  UPDATE_RATE_NEVER     = 0,
  UPDATE_RATE_PER_FRAME = 1,
};

enum ImageType {
  IMAGE_TYPE_TEXTURE2D  = 0,
  IMAGE_TYPE_DRAW_COLOR = 1,
  IMAGE_TYPE_DRAW_DEPTH = 2,
};

struct ImageDesc {
  std::string_view name;
  void            *pData;
  uint32_t         width;
  uint32_t         height;
  bool             mipmapped;
  VkFormat         format;
  UpdateRate       rate;
  ImageType        type;
};

struct Image {
  VkImage           pImage;
  VkImageView       pView;
  VkFormat          mFormat;
  VkExtent3D        mExtent;
  VmaAllocation     pVmaAllocation;
  VmaAllocationInfo mVmaAllocationInfo;
};

struct Sampler {
  VkSampler pSampler;
};

enum BufferType {
  BUFFER_TYPE_SSBO = 0,
  BUFFER_TYPE_UBO  = 1,
};

enum BufferUsage {
  BUFFER_USAGE_UNKNOWN = 0,
  BUFFER_USAGE_VERTEX  = 1,
  BUFFER_USAGE_INDEX   = 2,
};

struct BufferDesc {
  std::string_view name;
  void            *pData;
  size_t           size;
  UpdateRate       rate;
  BufferType type;
  BufferUsage      usage;
};

struct Buffer {
  VkBuffer          pBuffer;
  VmaAllocation     pVmaAllocation;
  VmaAllocationInfo mVmaAllocationInfo;
};

struct TextureDesc {
  std::string_view name;
  void            *pData;
  uint32_t         width;
  uint32_t         height;
  bool             mipmapped;
};

struct Texture {
  Image   mImage;
  Sampler mSampler;
};

struct Shader {
  VkShaderModule pShader;
  VkDevice       pDevice;
};

struct DescriptorSet {
  VkDescriptorSet       pDescriptorSet;
  VkDescriptorSetLayout pDescriptorSetLayout;
};

struct Frame {
  VkCommandPool   pCmdPool;
  VkCommandBuffer pCmdBuffer;

  VkFence     pFence;
  VkSemaphore pSemaphore;

  DescriptorSet mDescriptorSet;

  std::unordered_map<std::string, BufferId> mUniformBuffers;
};

struct ImmediateSubmit {
  VkCommandPool   pCmdPool;
  VkCommandBuffer pCmdBuffer;
  VkFence         pFence;
};

struct ResizeRequest {
  bool        requested = false;
  GLFWwindow *pWindow   = nullptr;
};

struct RenderObject {
  Pipeline pipeline;

  bool          useDescriptorSet;
  DescriptorSet descriptorSet;

  bool     usePushConstants;
  void    *pPushConstants;
  uint32_t pushConstantsSize;

  bool     useIndexBuffer;
  Buffer   indexBuffer;
  uint32_t indexCount;
  uint32_t indexOffset;

  uint32_t vertexCount;
  uint32_t vertexOffset;
};

struct RenderContext {
  std::vector<RenderObject> objects;
};

class Renderer {
public:
  SlangContext mSlangContext;

  VkInstance pInstance;
#ifndef NDEBUG
  VkDebugUtilsMessengerEXT pDebugMessenger;
#endif

  VmaAllocator pVmaAllocator;

  VkPhysicalDevice pPhysicalDevice;
  VkDevice         pDevice;
  VkQueue          pGraphicQueue;
  VkQueue          pPresentQueue;
  VkQueue          pTransferQueue;
  VkQueue          pComputeQueue;
  PhysicalDevice   mGpu;

  VkSurfaceKHR pSurface;
  Swapchain    mSwapchain;

  VkDescriptorPool pMainDescriptorPool;

  std::vector<Frame> mFrames;
  uint32_t           mCurrentFrame = 0;

  ImmediateSubmit mImmediateSubmit;

  Image mDrawColorImage;
  Image mDrawDepthImage;

  ResizeRequest mResizeRequest = {};

  std::unordered_map<BufferId, Buffer> mBuffers;
  uint32_t                             mBufferIdCount = 0;

  DescriptorSet                          mTexturesSet;
  std::unordered_map<TextureId, Texture> mTextures;
  uint32_t                               mTextureIdCount = 0;

  void Init(GLFWwindow *window);
  void Draw(const RenderContext &context);

  Renderer() = default;
  ~Renderer();

  void RequestResize(GLFWwindow *window);

  BufferId      GetPerFrameBuffer(std::string_view key);
  DescriptorSet GetPerFrameDescriptorSet();

  void WriteBufferDescriptorSet(uint32_t binding, Buffer &buffer, DescriptorSet &set, VkDescriptorType type);
  void WriteImageDescriptorSet(uint32_t binding, Image &image, DescriptorSet &set, VkDescriptorType type);
  void WriteSamplerDescriptorSet(uint32_t binding, Sampler &sampler, DescriptorSet &set, VkDescriptorType type);
  void WriteTextureDescriptorSet(uint32_t binding, Texture &texture, DescriptorSet &set, VkDescriptorType type);

  Shader   CreateShader(std::filesystem::path path, std::string_view entry_point = "main");
  BufferId CreateBuffer(BufferDesc &desc);
  void     DestroyBuffer(BufferId id);
  Image    CreateImage(ImageDesc &desc);

  TextureId CreateTexture(TextureDesc &texture_desc);
  void      DestroyTexture(TextureId id);

  Pipeline      CreateGraphicsPipeline(const GraphicPipelineDesc &builder);
  DescriptorSet CreateDescriptorSet(DescriptorDesc &desc);

  void DestroyDescriptorSet(DescriptorSet &set);
  void DestroyTexture(Texture &texture);
  void DestroyShader(Shader &shader);
  void DestroyBuffer(Buffer &buffer);
  void DestroyImage(Image &image);
  void DestroyPipeline(Pipeline &pipeline);

private:
  void RegisterTexture(VkImageView view, uint32_t index);
  void RegisterSampler(VkSampler sampler, uint32_t index);

  Buffer CreateBufferInternal(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage mem_usage, VmaAllocationCreateFlags flags);
  Image  CreateImageInternal(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped);

  void DrawColor(const RenderContext &context, VkCommandBuffer cmd, VkExtent2D draw_extent);

  Frame   &GetCurrentFrame();
  void     StartFrame(const Frame &frame);
  VkResult AcquireNextSwapchainImage(const Frame &frame, uint32_t &out_index);
  VkResult PresentImage(uint32_t image_index);

  void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function);
  void ResizeSwapchain();

  void PickPhysicalDevice();
  void CreateInstance();
  void CreateSurface(GLFWwindow *window);
  void CreateDevice();
  void CreateVmaAllocator();
  void CreateSwapchain(GLFWwindow *window, uint32_t width, uint32_t height, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);
  void CreateSlangSession();
  void CreateMainDescriptorPool();
  void CreateMainTexturesSet();
  void CreateFrames();
  void CreateImmediateSubmitTools();

  void DestroyImmediateSubmitTools();
  void DestroyFrames();
  void DestroyMainTexturesSet();
  void DestroyMainDescriptorPool();
  void DestroySlangSession();
  void DestroySwapchain(Swapchain &swapchain);
  void DestroyVmaAllocator();
  void DestroyDevice();
  void DestroySurface();
  void DestroyInstance();

  // Pipeline CreateComputePipeline();

#ifndef NDEBUG
  void CreateDebugMessenger();
  void DestroyDebugMessenger();
#endif
};
