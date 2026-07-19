#include "renderer.hpp"
#include "GLFW/glfw3.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <map>
#include <print>
#include <set>
#include <span>

#define VK_CHECK(x)                                                                                                                                  \
  do {                                                                                                                                               \
    VkResult vk_check_result_ = x;                                                                                                                   \
    if (vk_check_result_ != VK_SUCCESS) {                                                                                                            \
      std::cerr << "VK_CHECK failed: " << vk_check_result_ << " in " << __func__ << " ( " << __FILE__ << ":" << __LINE__ << ")" << std::endl;        \
      abort();                                                                                                                                       \
    }                                                                                                                                                \
  } while (0)

#define VK_CHECK_FOR_SWAPCHAIN_RESIZE(x)                                                                                                             \
  do {                                                                                                                                               \
    VkResult vk_check_result_resize = x;                                                                                                             \
    if (vk_check_result_resize == VK_SUBOPTIMAL_KHR || vk_check_result_resize == VK_ERROR_OUT_OF_DATE_KHR) {                                         \
      mResizeRequest.requested = true;                                                                                                               \
      return;                                                                                                                                        \
    }                                                                                                                                                \
    VK_CHECK(vk_check_result_resize);                                                                                                                \
  } while (0)

#define VK_ENUMERATE(func, vec, ...)                                                                                                                 \
  do {                                                                                                                                               \
    uint32_t _count = 0;                                                                                                                             \
    func(__VA_ARGS__, &_count, nullptr);                                                                                                             \
    vec.resize(_count);                                                                                                                              \
    func(__VA_ARGS__, &_count, vec.data());                                                                                                          \
  } while (0)

#define SLANG_CHECK(x)                                                                                                                               \
  do {                                                                                                                                               \
    SlangResult slang_check_result_ = x;                                                                                                             \
    if (slang_check_result_ != SLANG_OK) {                                                                                                           \
      std::cerr << "SLANG_CHECK failed: " << slang_check_result_ << " in " << __func__ << " ( " << __FILE__ << ":" << __LINE__ << ")" << std::endl;  \
      abort();                                                                                                                                       \
    }                                                                                                                                                \
  } while (0)

#define CHECK(x)                                                                                                                                     \
  do {                                                                                                                                               \
    if (!(x)) {                                                                                                                                      \
      std::cerr << "CHECK failed: "                                                                                                                  \
                << " in " << __func__ << " ( " << __FILE__ << ":" << __LINE__ << ")" << std::endl;                                                   \
      abort();                                                                                                                                       \
    }                                                                                                                                                \
  } while (0)

constexpr static auto FRAME_IN_FLIGHT   = 2;
constexpr static auto MAX_TEXTURES      = 4096;

static auto VK_VALIDATION_LAYERS = std::vector<const char *>({
#ifndef NDEBUG
    "VK_LAYER_KHRONOS_validation"
#endif
});

static auto VK_EXTENSIONS = std::vector<const char *>({
#ifndef NDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
});

static auto VK_DEVICE_EXTENSIONS = std::vector<const char *>({
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
});

static VkRenderingAttachmentInfo InitColorAttachmentInfo(VkImageView view, VkImageLayout layout) {
  VkRenderingAttachmentInfo colorAttachment{};
  colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAttachment.pNext = nullptr;

  colorAttachment.imageView   = view;
  colorAttachment.imageLayout = layout;
  colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

  return colorAttachment;
}

static VkRenderingAttachmentInfo InitDepthColorAttachmentInfo(VkImageView view, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/) {
  VkRenderingAttachmentInfo depthAttachment{};
  depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depthAttachment.pNext = nullptr;

  depthAttachment.imageView                     = view;
  depthAttachment.imageLayout                   = layout;
  depthAttachment.loadOp                        = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp                       = VK_ATTACHMENT_STORE_OP_STORE;
  depthAttachment.clearValue.depthStencil.depth = 0.f;

  return depthAttachment;
}

static VkRenderingInfo InitRenderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo *colorAttachment,
                                         VkRenderingAttachmentInfo *depthAttachment) {
  VkRenderingInfo renderInfo{};
  renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderInfo.pNext = nullptr;

  renderInfo.renderArea           = VkRect2D{VkOffset2D{0, 0}, renderExtent};
  renderInfo.layerCount           = 1;
  renderInfo.colorAttachmentCount = 1;
  renderInfo.pColorAttachments    = colorAttachment;
  renderInfo.pDepthAttachment     = depthAttachment;
  renderInfo.pStencilAttachment   = depthAttachment;

  return renderInfo;
}

VkImageSubresourceRange InitImageSubresourceRange(VkImageAspectFlags aspectMask) {
  VkImageSubresourceRange subImage{};
  subImage.aspectMask     = aspectMask;
  subImage.baseMipLevel   = 0;
  subImage.levelCount     = VK_REMAINING_MIP_LEVELS;
  subImage.baseArrayLayer = 0;
  subImage.layerCount     = VK_REMAINING_ARRAY_LAYERS;

  return subImage;
}

static VkSubmitInfo2 InitSubmitInfo(VkCommandBufferSubmitInfo *cmd, VkSemaphoreSubmitInfo *signalSemaphoreInfo,
                                    VkSemaphoreSubmitInfo *waitSemaphoreInfo) {
  VkSubmitInfo2 info = {};
  info.sType         = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  info.pNext         = nullptr;

  info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
  info.pWaitSemaphoreInfos    = waitSemaphoreInfo;

  info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
  info.pSignalSemaphoreInfos    = signalSemaphoreInfo;

  info.commandBufferInfoCount = 1;
  info.pCommandBufferInfos    = cmd;

  return info;
}

static VkSemaphoreSubmitInfo InitSemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore) {
  VkSemaphoreSubmitInfo submitInfo{};
  submitInfo.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  submitInfo.pNext       = nullptr;
  submitInfo.semaphore   = semaphore;
  submitInfo.stageMask   = stageMask;
  submitInfo.deviceIndex = 0;
  submitInfo.value       = 1;

  return submitInfo;
}

static VkCommandBufferBeginInfo InitCommandBufferBeginInfo(VkCommandBufferUsageFlags flags) {
  VkCommandBufferBeginInfo info = {};
  info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.pNext                    = nullptr;

  info.pInheritanceInfo = nullptr;
  info.flags            = flags;
  return info;
}

static VkCommandBufferSubmitInfo InitCommandBufferSubmitInfo(VkCommandBuffer cmd) {
  VkCommandBufferSubmitInfo info{};
  info.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  info.pNext         = nullptr;
  info.commandBuffer = cmd;
  info.deviceMask    = 0;

  return info;
}

static VkCommandBufferAllocateInfo InitCommandBufferAllocateInfo(VkCommandPool pool, size_t count) {
  VkCommandBufferAllocateInfo info = {};
  info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.pNext                       = nullptr;
  info.commandPool                 = pool;
  info.commandBufferCount          = 1;
  info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  return info;
}

static VkPipelineShaderStageCreateInfo InitPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule,
                                                                         const char *entry) {
  VkPipelineShaderStageCreateInfo info{};
  info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.pNext  = nullptr;
  info.stage  = stage;
  info.module = shaderModule;
  info.pName  = entry;
  return info;
}

static VkImageCreateInfo InitImageCreateInfo(VkExtent3D extent, VkFormat format, VkImageUsageFlags usageFlags) {
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext = nullptr;

  info.imageType = VK_IMAGE_TYPE_2D;

  info.format = format;
  info.extent = extent;

  info.mipLevels   = 1;
  info.arrayLayers = 1;

  // for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
  info.samples = VK_SAMPLE_COUNT_1_BIT;

  // optimal tiling, which means the image is stored on the best gpu format
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage  = usageFlags;

  return info;
}

static VkImageViewCreateInfo InitImageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags) {
  // build a image-view for the depth image to use for rendering
  VkImageViewCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.pNext = nullptr;

  info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
  info.image                           = image;
  info.format                          = format;
  info.subresourceRange.baseMipLevel   = 0;
  info.subresourceRange.levelCount     = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount     = 1;
  info.subresourceRange.aspectMask     = aspectFlags;

  return info;
}

static bool CheckDeviceExtensionsSupport(std::span<const char *> requested, std::span<VkExtensionProperties> available) {
  for (auto request : requested) {
    bool found = false;
    for (const auto &extension : available) {
      if (std::strcmp(request, extension.extensionName) == 0)
        found = true;
    }
    if (!found)
      return false;
  }
  return true;
}

static void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout) {
  VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  imageBarrier.pNext = nullptr;

  imageBarrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  imageBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  imageBarrier.oldLayout = currentLayout;
  imageBarrier.newLayout = newLayout;

  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  } else if (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  imageBarrier.subresourceRange = InitImageSubresourceRange(aspectMask);
  imageBarrier.image            = image;

  VkDependencyInfo depInfo{};
  depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.pNext = nullptr;

  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers    = &imageBarrier;

  vkCmdPipelineBarrier2(cmd, &depInfo);
}

static void TransitionImageMip(VkCommandBuffer cmd, VkImage image, uint32_t mipLevel, VkImageLayout currentLayout, VkImageLayout newLayout) {
  VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  imageBarrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  imageBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
  imageBarrier.oldLayout     = currentLayout;
  imageBarrier.newLayout     = newLayout;
  imageBarrier.image         = image;

  imageBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  imageBarrier.subresourceRange.baseMipLevel   = mipLevel;
  imageBarrier.subresourceRange.levelCount     = 1;
  imageBarrier.subresourceRange.baseArrayLayer = 0;
  imageBarrier.subresourceRange.layerCount     = 1;

  VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers    = &imageBarrier;
  vkCmdPipelineBarrier2(cmd, &depInfo);
}

static void GenerateMimaps(VkCommandBuffer cmd, VkImage image, uint32_t width, uint32_t height, uint32_t miplevels) {
  for (uint32_t i = 1; i < miplevels; i++) {
    TransitionImageMip(cmd, image, i - 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkImageBlit blit{};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel   = i - 1;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[1]             = {(int32_t)(width >> (i - 1)), (int32_t)(height >> (i - 1)), 1};

    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel   = i;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[1]             = {(int32_t)(width >> i), (int32_t)(height >> i), 1};

    vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
    TransitionImageMip(cmd, image, i - 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  TransitionImageMip(cmd, image, miplevels - 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize) {
  VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

  blitRegion.srcOffsets[1].x = srcSize.width;
  blitRegion.srcOffsets[1].y = srcSize.height;
  blitRegion.srcOffsets[1].z = 1;

  blitRegion.dstOffsets[1].x = dstSize.width;
  blitRegion.dstOffsets[1].y = dstSize.height;
  blitRegion.dstOffsets[1].z = 1;

  blitRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  blitRegion.srcSubresource.baseArrayLayer = 0;
  blitRegion.srcSubresource.layerCount     = 1;
  blitRegion.srcSubresource.mipLevel       = 0;

  blitRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  blitRegion.dstSubresource.baseArrayLayer = 0;
  blitRegion.dstSubresource.layerCount     = 1;
  blitRegion.dstSubresource.mipLevel       = 0;

  VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
  blitInfo.dstImage       = destination;
  blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  blitInfo.srcImage       = source;
  blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  blitInfo.filter         = VK_FILTER_LINEAR;
  blitInfo.regionCount    = 1;
  blitInfo.pRegions       = &blitRegion;

  vkCmdBlitImage2(cmd, &blitInfo);
}

DescriptorDesc &DescriptorDesc::AddBinding(uint32_t binding, VkDescriptorType type) {
  VkDescriptorSetLayoutBinding bind = {};
  bind.binding                      = binding;
  bind.descriptorType               = type;
  bind.descriptorCount              = 1;
  bindings.push_back(bind);
  return *this;
}

GraphicPipelineDesc &GraphicPipelineDesc::SetShaders(VkShaderModule vertex, VkShaderModule fragment) {
  stages.clear();
  stages.push_back(InitPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertex, "main"));
  stages.push_back(InitPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragment, "main"));
  return *this;
}

GraphicPipelineDesc &GraphicPipelineDesc::SetInputTopology(VkPrimitiveTopology topology) {
  input_assembly.topology = topology;
  return *this;
}

GraphicPipelineDesc &GraphicPipelineDesc::SetPolygonMode(VkPolygonMode mode) {
  rasterizer.polygonMode = mode;
  rasterizer.lineWidth   = 1.f;
  return *this;
}

GraphicPipelineDesc &GraphicPipelineDesc::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) {
  rasterizer.cullMode  = cullMode;
  rasterizer.frontFace = frontFace;
  return *this;
}

GraphicPipelineDesc &GraphicPipelineDesc::SetMultiSamplingNone() {
  multisampling.sampleShadingEnable   = VK_FALSE;
  multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
  multisampling.minSampleShading      = 1.0f;
  multisampling.pSampleMask           = nullptr;
  multisampling.alphaToCoverageEnable = VK_FALSE;
  multisampling.alphaToOneEnable      = VK_FALSE;
  return *this;
}

GraphicPipelineDesc &GraphicPipelineDesc::DisableBlending() {
  color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable    = VK_FALSE;
  return *this;
}

GraphicPipelineDesc &GraphicPipelineDesc::DisableDepthTest() {
  depth_stencil.depthTestEnable       = VK_FALSE;
  return *this;
}
GraphicPipelineDesc &GraphicPipelineDesc::DisableStencilTest() {
    depth_stencil.stencilTestEnable = VK_FALSE;
    return *this;
}

GraphicPipelineDesc &GraphicPipelineDesc::EnableBlendingAdditive() {
  color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable    = VK_TRUE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
  return *this;
}

GraphicPipelineDesc &GraphicPipelineDesc::EnableBlendingAlphaBend() {
  color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable    = VK_TRUE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
  return *this;
}

GraphicPipelineDesc &GraphicPipelineDesc::EnableDepthTest(bool write_enable, VkCompareOp op) {
  depth_stencil.depthTestEnable       = VK_TRUE;
  depth_stencil.depthWriteEnable      = write_enable;
  depth_stencil.depthCompareOp        = op;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable     = VK_FALSE;
  depth_stencil.front                 = {};
  depth_stencil.back                  = {};
  depth_stencil.minDepthBounds        = 0.f;
  depth_stencil.maxDepthBounds        = 1.f;
  return *this;
}

GraphicPipelineDesc &GraphicPipelineDesc::EnableStencilTest(const StencilDesc& desc) {
    depth_stencil.stencilTestEnable = VK_TRUE;
    depth_stencil.front.compareOp = desc.compare_op;
    depth_stencil.front.reference = desc.reference;
    depth_stencil.front.compareMask = desc.compare_mask;
    depth_stencil.front.failOp = desc.fail_op;
    depth_stencil.front.depthFailOp = desc.depth_fail_op;
    depth_stencil.front.passOp = desc.pass_op;
    depth_stencil.front.writeMask = desc.write_mask;

    depth_stencil.back.compareOp = desc.compare_op;
    depth_stencil.back.reference = desc.reference;
    depth_stencil.back.compareMask = desc.compare_mask;
    depth_stencil.back.failOp = desc.fail_op;
    depth_stencil.back.depthFailOp = desc.depth_fail_op;
    depth_stencil.back.passOp = desc.pass_op;
    depth_stencil.back.writeMask = desc.write_mask;
    return *this;
}

// GraphicPipelineDesc &GraphicPipelineDesc::SetColorAttachmentFormat(VkFormat format) {
//   color_attachment_format = format;
//   return *this;
// }

// GraphicPipelineDesc &GraphicPipelineDesc::SetDepthFormat(VkFormat format) {
//   render_info.depthAttachmentFormat = format;
//   return *this;
// }

void Renderer::Init(GLFWwindow *window) {
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  CreateInstance();
#ifndef NDEBUG
  CreateDebugMessenger();
#endif
  CreateSurface(window);
  PickPhysicalDevice();
  CreateDevice();
  CreateVmaAllocator();
  CreateSwapchain(window, width, height, VK_NULL_HANDLE);
  CreateSlangSession();
  CreateMainDescriptorPool();
  CreateMainTexturesSet();
  CreateFrames();
  CreateImmediateSubmitTools();

  ImageDesc image_desc{};
  image_desc.width     = 3840;
  image_desc.height    = 2160;
  image_desc.rate      = UPDATE_RATE_PER_FRAME;
  image_desc.type      = IMAGE_TYPE_DRAW_COLOR;
  image_desc.format    = VK_FORMAT_R16G16B16A16_SFLOAT;
  image_desc.mipmapped = false;
  mDrawColorImage      = CreateImage(image_desc);

  image_desc.type   = IMAGE_TYPE_DRAW_DEPTH;
  image_desc.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
  mDrawDepthImage   = CreateImage(image_desc);
}

void Renderer::Draw(const RenderContext &context) {
  if (mResizeRequest.requested) {
    ResizeSwapchain();
    return;
  }

  auto &frame = GetCurrentFrame();
  StartFrame(frame);
  uint32_t swapchain_image_index = 0;
  VK_CHECK_FOR_SWAPCHAIN_RESIZE(AcquireNextSwapchainImage(frame, swapchain_image_index));

  auto cmd_begin_info = InitCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK(vkBeginCommandBuffer(frame.pCmdBuffer, &cmd_begin_info));

  VkExtent2D draw_extent{};
  draw_extent.width  = std::min(mDrawColorImage.mExtent.width, mSwapchain.extent.width);
  draw_extent.height = std::min(mDrawColorImage.mExtent.height, mSwapchain.extent.height);

  TransitionImage(frame.pCmdBuffer, mDrawColorImage.pImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  TransitionImage(frame.pCmdBuffer, mDrawDepthImage.pImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  DrawColor(context, frame.pCmdBuffer, draw_extent);
  TransitionImage(frame.pCmdBuffer, mDrawColorImage.pImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  TransitionImage(frame.pCmdBuffer, mSwapchain.images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  CopyImageToImage(frame.pCmdBuffer, mDrawColorImage.pImage, mSwapchain.images[swapchain_image_index], draw_extent, mSwapchain.extent);
  TransitionImage(frame.pCmdBuffer, mSwapchain.images[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  VK_CHECK(vkEndCommandBuffer(frame.pCmdBuffer));

  VkCommandBufferSubmitInfo cmdinfo    = InitCommandBufferSubmitInfo(frame.pCmdBuffer);
  VkSemaphoreSubmitInfo     waitInfo   = InitSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame.pSemaphore);
  VkSemaphoreSubmitInfo     signalInfo = InitSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, mSwapchain.semaphores[swapchain_image_index]);
  VkSubmitInfo2             submit     = InitSubmitInfo(&cmdinfo, &signalInfo, &waitInfo);
  VK_CHECK_FOR_SWAPCHAIN_RESIZE(vkQueueSubmit2(pGraphicQueue, 1, &submit, frame.pFence));
  VK_CHECK_FOR_SWAPCHAIN_RESIZE(PresentImage(swapchain_image_index));

  mCurrentFrame++;
}

Renderer::~Renderer() {
  vkDeviceWaitIdle(pDevice);
  DestroyImage(mDrawDepthImage);
  DestroyImage(mDrawColorImage);
  for (auto &texture : mTextures) {
    DestroyTexture(texture.second);
  }

  DestroyImmediateSubmitTools();
  DestroyFrames();
  DestroyDescriptorSet(mTexturesSet);
  DestroyMainDescriptorPool();
  DestroySlangSession();
  DestroySwapchain(mSwapchain);
  DestroyVmaAllocator();
  DestroyDevice();
  DestroySurface();
#ifndef NDEBUG
  DestroyDebugMessenger();
#endif
  DestroyInstance();
}

void Renderer::RequestResize(GLFWwindow *window) {
  mResizeRequest.requested = true;
  mResizeRequest.pWindow   = window;
}

BufferId Renderer::GetPerFrameBuffer(std::string_view key) {
  auto &frame = GetCurrentFrame();
  return frame.mUniformBuffers.at(key.data());
}

DescriptorSet Renderer::GetPerFrameDescriptorSet() {
  auto &frame = GetCurrentFrame();
  return frame.mDescriptorSet;
}

void Renderer::WriteBufferDescriptorSet(uint32_t binding, Buffer &buffer, DescriptorSet &set, VkDescriptorType type) {
  VkDescriptorBufferInfo info{};
  info.buffer = buffer.pBuffer;
  info.offset = 0;
  info.range  = VK_WHOLE_SIZE;

  VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.descriptorType  = type;
  write.dstBinding      = binding;
  write.descriptorCount = 1;
  write.dstSet          = set.pDescriptorSet;
  write.pBufferInfo     = &info;

  vkUpdateDescriptorSets(pDevice, 1, &write, 0, nullptr);
}

void Renderer::WriteImageDescriptorSet(uint32_t binding, Image &image, DescriptorSet &set, VkDescriptorType type) {
  VkDescriptorImageInfo info{};
  info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  info.imageView   = image.pView;
  info.sampler     = VK_NULL_HANDLE;

  VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.descriptorType  = type;
  write.dstBinding      = binding;
  write.descriptorCount = 1;
  write.dstSet          = set.pDescriptorSet;
  write.pImageInfo      = &info;

  vkUpdateDescriptorSets(pDevice, 1, &write, 0, nullptr);
}

void Renderer::WriteSamplerDescriptorSet(uint32_t binding, Sampler &sampler, DescriptorSet &set, VkDescriptorType type) {
  VkDescriptorImageInfo info{};
  info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  info.imageView   = VK_NULL_HANDLE;
  info.sampler     = sampler.pSampler;

  VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.descriptorType  = type;
  write.dstBinding      = binding;
  write.descriptorCount = 1;
  write.dstSet          = set.pDescriptorSet;
  write.pImageInfo      = &info;

  vkUpdateDescriptorSets(pDevice, 1, &write, 0, nullptr);
}

void Renderer::WriteTextureDescriptorSet(uint32_t binding, Texture &texture, DescriptorSet &set, VkDescriptorType type) {
  VkDescriptorImageInfo info{};
  info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  info.imageView   = texture.mImage.pView;
  info.sampler     = texture.mSampler.pSampler;

  VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.descriptorType  = type;
  write.dstBinding      = binding;
  write.descriptorCount = 1;
  write.dstSet          = set.pDescriptorSet;
  write.pImageInfo      = &info;

  vkUpdateDescriptorSets(pDevice, 1, &write, 0, nullptr);
}

Shader Renderer::CreateShader(std::filesystem::path path, std::string_view entry_point) {
  std::string path_str = path.string();

  Slang::ComPtr<slang::IBlob> diagnostics;

  auto check = [&](SlangResult res, const char *step) {
    if (diagnostics) {
      auto *msg = (const char *)diagnostics->getBufferPointer();
      if (SLANG_FAILED(res)) {
        std::cerr << "[SLANG] ERROR at " << step << " (" << path << "):\n" << msg << "\n";
        abort();
      } else {
        std::cerr << "[SLANG] WARNING at " << step << " (" << path << "):\n" << msg << "\n";
      }
    }
    diagnostics.setNull();
  };

  Slang::ComPtr<slang::IModule> module(mSlangContext.pSession->loadModule(path_str.c_str(), diagnostics.writeRef()));

  Slang::ComPtr<slang::IEntryPoint> shader_entry_point;
  check(module->findEntryPointByName(entry_point.data(), shader_entry_point.writeRef()), "findEntryPointByName");

  slang::IComponentType               *components[] = {module, shader_entry_point};
  Slang::ComPtr<slang::IComponentType> program;
  check(mSlangContext.pSession->createCompositeComponentType(components, 2, program.writeRef()), "createCompositeComponentType");

  Slang::ComPtr<slang::IComponentType> linked_program;
  check(program->link(linked_program.writeRef(), diagnostics.writeRef()), "link");

  Slang::ComPtr<slang::IBlob> kernel_blob;
  check(linked_program->getEntryPointCode(0, 0, kernel_blob.writeRef(), diagnostics.writeRef()), "getEntryPointCode");

  auto buffer = (uint32_t *)kernel_blob->getBufferPointer();
  auto size   = kernel_blob->getBufferSize();

  Shader                   shader{};
  VkShaderModuleCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  create_info.codeSize = size;
  create_info.pCode    = buffer;
  VK_CHECK(vkCreateShaderModule(pDevice, &create_info, nullptr, &shader.pShader));

  return shader;
}

void Renderer::RegisterTexture(VkImageView view, uint32_t index) {
  VkDescriptorImageInfo image_info{.imageView = view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

  VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.dstSet          = mTexturesSet.pDescriptorSet;
  write.dstBinding      = 0;
  write.dstArrayElement = index;
  write.descriptorCount = 1;
  write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  write.pImageInfo      = &image_info;

  vkUpdateDescriptorSets(pDevice, 1, &write, 0, nullptr);
}

void Renderer::RegisterSampler(VkSampler sampler, uint32_t index) {
  VkDescriptorImageInfo sampler_info{.sampler = sampler};

  VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.dstSet          = mTexturesSet.pDescriptorSet;
  write.dstBinding      = 1;
  write.dstArrayElement = index;
  write.descriptorCount = 1;
  write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
  write.pImageInfo      = &sampler_info;

  vkUpdateDescriptorSets(pDevice, 1, &write, 0, nullptr);
}

Buffer Renderer::CreateBufferInternal(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage mem_usage, VmaAllocationCreateFlags flags) {
  VkBufferCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  info.pNext = nullptr;
  info.size  = size;
  info.usage = usage;

  VmaAllocationCreateInfo vma_info = {};
  vma_info.usage                   = mem_usage;
  vma_info.flags                   = flags;

  Buffer buffer{};
  VK_CHECK(vmaCreateBuffer(pVmaAllocator, &info, &vma_info, &buffer.pBuffer, &buffer.pVmaAllocation, &buffer.mVmaAllocationInfo));

  return buffer;
}

BufferId Renderer::CreateBuffer(BufferDesc &desc) {
  Buffer buffer;
  if (desc.rate == UPDATE_RATE_NEVER) { // ON GPU
    VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (desc.type == BUFFER_TYPE_SSBO) {
      usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    } else if (desc.type == BUFFER_TYPE_UBO) {
      std::println("BUFFER CREATION WARNING: trying to create UBO on GPU side only ({})", desc.name);
      usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    if (desc.usage == BUFFER_USAGE_VERTEX) {
      usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    } else if (desc.usage == BUFFER_USAGE_INDEX) {
      usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    buffer         = CreateBufferInternal(desc.size, usage_flags, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
    Buffer staging = CreateBufferInternal(desc.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO,
                                          VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    memcpy(staging.mVmaAllocationInfo.pMappedData, desc.pData, desc.size);

    ImmediateSubmit([&](VkCommandBuffer cmd) {
      VkBufferCopy copy{};
      copy.size = desc.size;

      vkCmdCopyBuffer(cmd, staging.pBuffer, buffer.pBuffer, 1, &copy);
    });

    DestroyBuffer(staging);
    mBuffers[mBufferIdCount++] = buffer;
    return mBufferIdCount - 1;
  }

  if (desc.rate == UPDATE_RATE_PER_FRAME) { // ON CPU
    VkBufferUsageFlags usage_flags = 0;
    if (desc.type == BUFFER_TYPE_SSBO) {
      usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    } else if (desc.type == BUFFER_TYPE_UBO) {
      usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    for (auto &frame : mFrames) {
      buffer = CreateBufferInternal(desc.size, usage_flags, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
      if (desc.pData) {
        std::memcpy(buffer.mVmaAllocationInfo.pMappedData, desc.pData, desc.size);
      }
      frame.mUniformBuffers.emplace(desc.name, mBufferIdCount);
      mBuffers[mBufferIdCount++] = buffer;
    }
    return GetPerFrameBuffer(desc.name);
  }

  std::cerr << "BUFFER CREATION: DESC.RATE DOESNT MAKE SENSE (" << desc.rate << ")" << std::endl;
  abort();
}

void Renderer::DestroyBuffer(BufferId id) {
  auto it = mBuffers.find(id);
  if (it != mBuffers.end()) {
    DestroyBuffer(it->second);
    mBuffers.erase(it->first);
  }
}

Image Renderer::CreateImage(ImageDesc &desc) {
  Image image;

  if (desc.rate == UPDATE_RATE_NEVER) {
    if (desc.type == IMAGE_TYPE_TEXTURE2D) {
      auto size           = desc.width * desc.height * 4;
      auto mipmapped_flag = desc.mipmapped ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;
      image               = CreateImageInternal({desc.width, desc.height, 1}, desc.format,
                                                mipmapped_flag | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, desc.mipmapped);
      Buffer staging      = CreateBufferInternal(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
                                                 VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
      std::memcpy(staging.mVmaAllocationInfo.pMappedData, desc.pData, size);

      ImmediateSubmit([&](VkCommandBuffer cmd) {
        TransitionImage(cmd, image.pImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent                 = {desc.width, desc.height, 1};

        vkCmdCopyBufferToImage(cmd, staging.pBuffer, image.pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        GenerateMimaps(cmd, image.pImage, desc.width, desc.height, std::floor(std::log2(std::max(desc.width, desc.height))) + 1);
      });

      DestroyBuffer(staging);
      return image;
    }
  }

  if (desc.rate == UPDATE_RATE_PER_FRAME) {
    if (desc.type == IMAGE_TYPE_DRAW_COLOR) {
      image = CreateImageInternal({desc.width, desc.height, 1}, desc.format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                  false);
      return image;
    }
    if (desc.type == IMAGE_TYPE_DRAW_DEPTH) {
      image = CreateImageInternal({desc.width, desc.height, 1}, desc.format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, false);
      return image;
    }
  }

  std::cerr << "BUFFER CREATION: DESC.RATE DOESNT MAKE SENSE (" << desc.rate << ")" << std::endl;
  abort();
}

Image Renderer::CreateImageInternal(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped) {
  Image image{};
  image.mFormat = format;
  image.mExtent = extent;

  auto info      = InitImageCreateInfo(extent, format, usage);
  info.mipLevels = mipmapped ? std::floor(std::log2(std::max(extent.width, extent.height))) + 1 : 1;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage         = VMA_MEMORY_USAGE_GPU_ONLY;
  alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VK_CHECK(vmaCreateImage(pVmaAllocator, &info, &alloc_info, &image.pImage, &image.pVmaAllocation, nullptr));
  VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
  if (format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
      aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  }

  auto view_info = InitImageViewCreateInfo(format, image.pImage, aspect_flags);

  if (mipmapped) {
    view_info.subresourceRange.levelCount = info.mipLevels;
  }

  VK_CHECK(vkCreateImageView(pDevice, &view_info, nullptr, &image.pView));

  return image;
}

TextureId Renderer::CreateTexture(TextureDesc &texture_desc) {
  Texture texture{};

  ImageDesc image_desc;
  image_desc.name      = texture_desc.name;
  image_desc.width     = texture_desc.width;
  image_desc.height    = texture_desc.height;
  image_desc.pData     = texture_desc.pData;
  image_desc.rate      = UPDATE_RATE_NEVER;
  image_desc.type      = IMAGE_TYPE_TEXTURE2D;
  image_desc.format    = VK_FORMAT_R8G8B8A8_SRGB;
  image_desc.mipmapped = texture_desc.mipmapped;

  texture.mImage = CreateImage(image_desc);
  VkSamplerCreateInfo sampler_info{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sampler_info.minFilter    = VK_FILTER_LINEAR;
  sampler_info.magFilter    = VK_FILTER_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.minLod = 0.f;
  sampler_info.maxLod = VK_LOD_CLAMP_NONE;
  VK_CHECK(vkCreateSampler(pDevice, &sampler_info, nullptr, &texture.mSampler.pSampler));

  CHECK(mTextureIdCount != INVALID_ID);
  auto texture_id = mTextureIdCount++;

  RegisterTexture(texture.mImage.pView, texture_id);
  RegisterSampler(texture.mSampler.pSampler, texture_id);

  mTextures[texture_id] = texture;
  return texture_id;
}

void Renderer::DestroyTexture(TextureId id) {
  auto it = mTextures.find(id);
  if (it != mTextures.end()) {
    DestroyTexture(it->second);
    mTextures.erase(it->first);
  }
}

Pipeline Renderer::CreateGraphicsPipeline(const GraphicPipelineDesc &desc) {
  Pipeline pipeline{};

  VkPipelineLayoutCreateInfo layout_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layout_info.setLayoutCount         = (uint32_t)desc.pipeline_layout_desc.layouts.size();
  layout_info.pSetLayouts            = desc.pipeline_layout_desc.layouts.data();
  layout_info.pushConstantRangeCount = (uint32_t)desc.pipeline_layout_desc.ranges.size();
  layout_info.pPushConstantRanges    = desc.pipeline_layout_desc.ranges.data();

  VK_CHECK(vkCreatePipelineLayout(pDevice, &layout_info, nullptr, &pipeline.pLayout));

  VkPipelineRenderingCreateInfo render_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  render_info.depthAttachmentFormat   = mDrawDepthImage.mFormat;
  render_info.pColorAttachmentFormats = &mDrawColorImage.mFormat;
  render_info.colorAttachmentCount    = 1;
  render_info.stencilAttachmentFormat = mDrawDepthImage.mFormat;

  VkPipelineViewportStateCreateInfo viewport_state{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount  = 1;

  VkPipelineColorBlendStateCreateInfo color_blending{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  color_blending.logicOpEnable   = VK_FALSE;
  color_blending.logicOp         = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments    = &desc.color_blend_attachment;

  VkPipelineVertexInputStateCreateInfo vertex_input_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

  VkGraphicsPipelineCreateInfo info{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  info.pNext               = &render_info;
  info.stageCount          = (uint32_t)desc.stages.size();
  info.pStages             = desc.stages.data();
  info.pVertexInputState   = &vertex_input_info;
  info.pInputAssemblyState = &desc.input_assembly;
  info.pViewportState      = &viewport_state;
  info.pRasterizationState = &desc.rasterizer;
  info.pMultisampleState   = &desc.multisampling;
  info.pColorBlendState    = &color_blending;
  info.pDepthStencilState  = &desc.depth_stencil;
  info.layout              = pipeline.pLayout;

  auto states = std::to_array<VkDynamicState>({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  VkPipelineDynamicStateCreateInfo dynamic_info = {};
  dynamic_info.sType                            = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_info.dynamicStateCount                = states.size();
  dynamic_info.pDynamicStates                   = states.data();
  info.pDynamicState                            = &dynamic_info;

  VK_CHECK(vkCreateGraphicsPipelines(pDevice, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline.pPipeline));
  return pipeline;
}

DescriptorSet Renderer::CreateDescriptorSet(DescriptorDesc &desc) {
  DescriptorSet                   set{};
  VkDescriptorSetLayoutCreateInfo desc_layout_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  for (auto &binding : desc.bindings) {
    binding.stageFlags = desc.shader_stages;
  }
  desc_layout_info.bindingCount = (uint32_t)desc.bindings.size();
  desc_layout_info.pBindings    = desc.bindings.data();
  VK_CHECK(vkCreateDescriptorSetLayout(pDevice, &desc_layout_info, nullptr, &set.pDescriptorSetLayout));

  for (auto &frame : mFrames) {
    VkDescriptorSetAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.descriptorPool     = pMainDescriptorPool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts        = &set.pDescriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(pDevice, &alloc_info, &frame.mDescriptorSet.pDescriptorSet));
    frame.mDescriptorSet.pDescriptorSetLayout = set.pDescriptorSetLayout;
  }

  return GetPerFrameDescriptorSet();
}

void Renderer::DestroyDescriptorSet(DescriptorSet &set) { vkDestroyDescriptorSetLayout(pDevice, set.pDescriptorSetLayout, nullptr); }

void Renderer::DestroyTexture(Texture &texture) {
  DestroyImage(texture.mImage);
  vkDestroySampler(pDevice, texture.mSampler.pSampler, nullptr);
}

void Renderer::DestroyShader(Shader &shader) { vkDestroyShaderModule(pDevice, shader.pShader, nullptr); }
void Renderer::DestroyBuffer(Buffer &buffer) { vmaDestroyBuffer(pVmaAllocator, buffer.pBuffer, buffer.pVmaAllocation); }
void Renderer::DestroyImage(Image &image) {
  vkDestroyImageView(pDevice, image.pView, nullptr);
  vmaDestroyImage(pVmaAllocator, image.pImage, image.pVmaAllocation);
}

void Renderer::DestroyPipeline(Pipeline &pipeline) {
  vkDestroyPipelineLayout(pDevice, pipeline.pLayout, nullptr);
  vkDestroyPipeline(pDevice, pipeline.pPipeline, nullptr);
}

void Renderer::DrawColor(const RenderContext &context, VkCommandBuffer cmd, VkExtent2D draw_extent) {
  auto color_attachment = InitColorAttachmentInfo(mDrawColorImage.pView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  auto depth_attachment = InitDepthColorAttachmentInfo(mDrawDepthImage.pView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  auto render_info      = InitRenderingInfo(draw_extent, &color_attachment, &depth_attachment);
  for (const auto& pass : context.passes) {
      vkCmdBeginRendering(cmd, &render_info);
      VkViewport viewport{.width = (float)draw_extent.width, .height = (float)draw_extent.height, .minDepth = 0.f, .maxDepth = 1.f};
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      VkRect2D scissor{.extent = draw_extent};
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      for (const auto& object: pass.objects) {
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.pipeline.pPipeline);
          if (object.descriptor_set.has_value()) {
            std::array<VkDescriptorSet, 2> sets {mTexturesSet.pDescriptorSet, object.descriptor_set->pDescriptorSet};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.pipeline.pLayout, 0, sets.size(), sets.data(), 0, nullptr);
          } else {
              vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.pipeline.pLayout, 0, 1, &mTexturesSet.pDescriptorSet, 0, nullptr);
          }

          if (object.push_constants.has_value()) {
            vkCmdPushConstants(cmd, object.pipeline.pLayout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, object.push_constants->size, object.push_constants->buffer);
          }

          if (object.indices.has_value()) {
            auto &buffer = mBuffers[object.indices->buffer];
            vkCmdBindIndexBuffer(cmd, buffer.pBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, object.indices->count, 1, object.indices->offset, 0, 0);
          } else {
            vkCmdDraw(cmd, object.vertices.count, 1, object.vertices.offset, 0);
          }
        }
        vkCmdEndRendering(cmd);
      }
}

Frame &Renderer::GetCurrentFrame() { return mFrames[mCurrentFrame % mFrames.size()]; }

void Renderer::StartFrame(const Frame &frame) {
  VK_CHECK(vkWaitForFences(pDevice, 1, &frame.pFence, VK_TRUE, 1000000000));
  VK_CHECK(vkResetFences(pDevice, 1, &frame.pFence));
  VK_CHECK(vkResetCommandBuffer(frame.pCmdBuffer, 0));
}

VkResult Renderer::AcquireNextSwapchainImage(const Frame &frame, uint32_t &out_index) {
  return vkAcquireNextImageKHR(pDevice, mSwapchain.pSwapchain, 1000000000, frame.pSemaphore, VK_NULL_HANDLE, &out_index);
}

VkResult Renderer::PresentImage(uint32_t image_index) {
  VkPresentInfoKHR present_info   = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present_info.pSwapchains        = &mSwapchain.pSwapchain;
  present_info.swapchainCount     = 1;
  present_info.pWaitSemaphores    = &mSwapchain.semaphores[image_index];
  present_info.waitSemaphoreCount = 1;
  present_info.pImageIndices      = &image_index;

  return vkQueuePresentKHR(pPresentQueue, &present_info);
}

void Renderer::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function) {
  VK_CHECK(vkResetFences(pDevice, 1, &mImmediateSubmit.pFence));
  VK_CHECK(vkResetCommandBuffer(mImmediateSubmit.pCmdBuffer, 0));

  VkCommandBuffer cmd = mImmediateSubmit.pCmdBuffer;

  VkCommandBufferBeginInfo cmdBeginInfo = InitCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  function(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdinfo = InitCommandBufferSubmitInfo(cmd);
  VkSubmitInfo2             submit  = InitSubmitInfo(&cmdinfo, nullptr, nullptr);

  // submit command buffer to the queue and execute it.
  //  _renderFence will now block until the graphic commands finish execution
  VK_CHECK(vkQueueSubmit2(pGraphicQueue, 1, &submit, mImmediateSubmit.pFence));
  VK_CHECK(vkWaitForFences(pDevice, 1, &mImmediateSubmit.pFence, true, 9999999999));
}

void Renderer::ResizeSwapchain() {
  CHECK(mResizeRequest.requested);
  CHECK(mResizeRequest.pWindow);
  int width = 0, height = 0;
  glfwGetFramebufferSize(mResizeRequest.pWindow, &width, &height);

  if (width == 0 || height == 0) {
    return;
  }

  vkDeviceWaitIdle(pDevice);

  auto old_swapchain = mSwapchain;
  mSwapchain.images.clear();
  mSwapchain.views.clear();
  mSwapchain.semaphores.clear();
  CreateSwapchain(mResizeRequest.pWindow, width, height, old_swapchain.pSwapchain);
  DestroySwapchain(old_swapchain);

  mResizeRequest.requested = false;
  mResizeRequest.pWindow   = nullptr;
}

void Renderer::PickPhysicalDevice() {
  std::vector<VkPhysicalDevice> physical_devices;
  VK_ENUMERATE(vkEnumeratePhysicalDevices, physical_devices, pInstance);
  CHECK(!physical_devices.empty());

  std::multimap<uint32_t, PhysicalDevice> candidates;
  for (const auto &physical_device : physical_devices) {
    uint32_t score = 0;

    VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceFeatures2   features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    vkGetPhysicalDeviceProperties2(physical_device, &properties);
    vkGetPhysicalDeviceFeatures2(physical_device, &features);

    std::cout << "Checking: " << properties.properties.deviceName;

    if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      score += 1000;
    }
    score += properties.properties.limits.maxImageDimension2D;

    std::vector<VkExtensionProperties> extension_properties;
    VK_ENUMERATE(vkEnumerateDeviceExtensionProperties, extension_properties, physical_device, nullptr);

    uint32_t format_count, present_mode_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, pSurface, &format_count, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, pSurface, &present_mode_count, nullptr);

    if (format_count == 0 || present_mode_count == 0 || properties.properties.apiVersion < VK_API_VERSION_1_4 ||
        features.features.geometryShader == VK_FALSE || features.features.samplerAnisotropy == VK_FALSE ||
        CheckDeviceExtensionsSupport(VK_DEVICE_EXTENSIONS, extension_properties) == false) {
      std::cout << " [FAILED]" << std::endl;
      continue;
    }

    GpuStat stat{};

    std::vector<VkQueueFamilyProperties> queue_families_properties;
    VK_ENUMERATE(vkGetPhysicalDeviceQueueFamilyProperties, queue_families_properties, physical_device);

    bool foundGraphicQueue  = false;
    bool foundPresentQueue  = false;
    bool foundTransferQueue = false;
    bool foundComputeQueue  = false;

    for (size_t i = 0; i < queue_families_properties.size(); i++) {
      auto flags = queue_families_properties[i].queueFlags;

      VkBool32 presentSupport = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, pSurface, &presentSupport);

      bool graphicSupport  = flags & VK_QUEUE_GRAPHICS_BIT;
      bool transferSupport = flags & VK_QUEUE_TRANSFER_BIT;
      bool computeSupport  = flags & VK_QUEUE_COMPUTE_BIT;

      bool dedicated_present_queue  = presentSupport && !graphicSupport && !transferSupport && !computeSupport;
      bool dedicated_transfer_queue = (flags & VK_QUEUE_TRANSFER_BIT) == flags;
      bool dedicated_compute_queue  = (flags & VK_QUEUE_COMPUTE_BIT) == flags;

      if (!foundGraphicQueue && graphicSupport) {
        stat.graphicQueueIndex = i;
        foundGraphicQueue      = true;
      }

      if (!foundPresentQueue && presentSupport)
        stat.presentQueueIndex = i;
      if (!foundTransferQueue && transferSupport)
        stat.transferQueueIndex = i;
      if (!foundComputeQueue && computeSupport)
        stat.computeQueueIndex = i;

      if (dedicated_present_queue) {
        stat.presentQueueIndex = i;
        foundPresentQueue      = true;
      }
      if (dedicated_transfer_queue) {
        stat.transferQueueIndex = i;
        foundTransferQueue      = true;
      }
      if (dedicated_compute_queue) {
        stat.computeQueueIndex = i;
        foundComputeQueue      = true;
      }

      if (foundGraphicQueue && foundTransferQueue && foundComputeQueue)
        break;
    }

    stat.score          = score;
    stat.properties     = properties;
    PhysicalDevice info = {.pPhysicalDevice = physical_device, .stat = stat};

    std::cout << " [OK] (" << score << ")" << std::endl;
    candidates.insert(std::make_pair(score, info));
  }

  CHECK(!candidates.empty());

  mGpu = candidates.rbegin()->second;
}

void Renderer::CreateInstance() {
  VkApplicationInfo app_info{};
  app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName   = "Renderer";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName        = "";
  app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion         = VK_API_VERSION_1_4;

  uint32_t glfw_extension_count = 0;
  auto     glfw_extensions      = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

  std::vector<const char *> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

  for (auto ext : VK_EXTENSIONS) {
    extensions.push_back(ext);
  }

  VkInstanceCreateInfo create_info{};
  create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo        = &app_info;
  create_info.enabledLayerCount       = (uint32_t)VK_VALIDATION_LAYERS.size();
  create_info.ppEnabledLayerNames     = VK_VALIDATION_LAYERS.data();
  create_info.enabledExtensionCount   = (uint32_t)extensions.size();
  create_info.ppEnabledExtensionNames = extensions.data();

  VK_CHECK(vkCreateInstance(&create_info, nullptr, &pInstance));
}

void Renderer::CreateSurface(GLFWwindow *window) { VK_CHECK(glfwCreateWindowSurface(pInstance, window, nullptr, &pSurface)); }

void Renderer::CreateDevice() {
  VkPhysicalDeviceExtendedDynamicState2FeaturesEXT extended_dynamic_state_features{};
  extended_dynamic_state_features.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;
  extended_dynamic_state_features.extendedDynamicState2 = VK_TRUE;

  VkPhysicalDeviceVulkan13Features features13{};
  features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features13.dynamicRendering = VK_TRUE;
  features13.synchronization2 = VK_TRUE;
  features13.pNext            = &extended_dynamic_state_features;

  VkPhysicalDeviceVulkan12Features features12{};
  features12.sType                                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  features12.bufferDeviceAddress                          = VK_TRUE;
  features12.descriptorIndexing                           = VK_TRUE;
  features12.descriptorBindingPartiallyBound              = VK_TRUE;
  features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
  features12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
  features12.runtimeDescriptorArray                       = VK_TRUE;
  features12.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
  features12.scalarBlockLayout = VK_TRUE;
  features12.pNext                                        = &features13;

  VkPhysicalDeviceVulkan11Features features11{};
  features11.sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  features11.shaderDrawParameters = VK_TRUE;
  features11.pNext                = &features12;

  VkPhysicalDeviceFeatures2 features2{};
  features2.sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features2.features.shaderInt64 = VK_TRUE;
  features2.pNext                = &features11;

  const auto &stat = mGpu.stat;
  std::cout << "Max push constants size: " << stat.properties.properties.limits.maxPushConstantsSize << std::endl;

  std::set<uint32_t> unique_families = {stat.graphicQueueIndex, stat.transferQueueIndex, stat.computeQueueIndex};

  std::vector<VkDeviceQueueCreateInfo> queues_infos;
  float                                queue_priority = 1.f;

  for (auto family : unique_families) {
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = family;
    queue_create_info.queueCount       = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    queues_infos.push_back(queue_create_info);
  }

  VkDeviceCreateInfo create_info{};
  create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pNext                   = &features2;
  create_info.queueCreateInfoCount    = (uint32_t)queues_infos.size();
  create_info.pQueueCreateInfos       = queues_infos.data();
  create_info.enabledExtensionCount   = (uint32_t)VK_DEVICE_EXTENSIONS.size();
  create_info.ppEnabledExtensionNames = VK_DEVICE_EXTENSIONS.data();

  VK_CHECK(vkCreateDevice(mGpu.pPhysicalDevice, &create_info, nullptr, &pDevice));

  std::cout << "Queue indices = [Graphic: " << mGpu.stat.graphicQueueIndex << ", "
            << "Present: " << mGpu.stat.presentQueueIndex << ", "
            << "Transfer: " << mGpu.stat.transferQueueIndex << ", "
            << "Compute: " << mGpu.stat.computeQueueIndex << "]" << std::endl;

  vkGetDeviceQueue(pDevice, mGpu.stat.graphicQueueIndex, 0, &pGraphicQueue);
  vkGetDeviceQueue(pDevice, mGpu.stat.presentQueueIndex, 0, &pPresentQueue);
  vkGetDeviceQueue(pDevice, mGpu.stat.transferQueueIndex, 0, &pTransferQueue);
  vkGetDeviceQueue(pDevice, mGpu.stat.computeQueueIndex, 0, &pComputeQueue);
}

void Renderer::CreateVmaAllocator() {
  VmaAllocatorCreateInfo allocator_info{
      .flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
      .physicalDevice = mGpu.pPhysicalDevice,
      .device         = pDevice,
      .instance       = pInstance,
  };
  VK_CHECK(vmaCreateAllocator(&allocator_info, &pVmaAllocator));
}

void Renderer::CreateSwapchain(GLFWwindow *window, uint32_t width, uint32_t height, VkSwapchainKHR old_swapchain) {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mGpu.pPhysicalDevice, pSurface, &capabilities);

  std::vector<VkSurfaceFormatKHR> surface_formats_available;
  std::vector<VkPresentModeKHR>   present_modes_available;
  VK_ENUMERATE(vkGetPhysicalDeviceSurfaceFormatsKHR, surface_formats_available, mGpu.pPhysicalDevice, pSurface);
  VK_ENUMERATE(vkGetPhysicalDeviceSurfacePresentModesKHR, present_modes_available, mGpu.pPhysicalDevice, pSurface);
  CHECK(!surface_formats_available.empty());
  CHECK(!present_modes_available.empty());

  auto format       = surface_formats_available.begin()->format;
  auto color_space  = surface_formats_available.begin()->colorSpace;
  auto present_mode = *present_modes_available.begin();

  for (const auto &surface_format : surface_formats_available) {
    if (surface_format.format == VK_FORMAT_B8G8R8A8_SRGB && surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      format      = surface_format.format;
      color_space = surface_format.colorSpace;
      break;
    }
  }
  for (const auto &p : present_modes_available) {
    if (p == VK_PRESENT_MODE_MAILBOX_KHR) {
      present_mode = p;
    }
  }
  VkExtent2D extent = capabilities.currentExtent;
  if (capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
    extent.width  = std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
  }

  auto min_image_count = std::max<uint32_t>(3, capabilities.minImageCount + 1);
  if (capabilities.maxImageCount > 0) {
    min_image_count = capabilities.maxImageCount;
  }

  bool                  multiple_queues = mGpu.stat.graphicQueueIndex != mGpu.stat.presentQueueIndex;
  std::vector<uint32_t> unique_queues{mGpu.stat.graphicQueueIndex, mGpu.stat.presentQueueIndex};

  VkSwapchainCreateInfoKHR create_info{.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                       .surface               = pSurface,
                                       .minImageCount         = min_image_count,
                                       .imageFormat           = format,
                                       .imageColorSpace       = color_space,
                                       .imageExtent           = extent,
                                       .imageArrayLayers      = 1,
                                       .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                       .imageSharingMode      = multiple_queues ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
                                       .queueFamilyIndexCount = multiple_queues ? (uint32_t)unique_queues.size() : 0,
                                       .pQueueFamilyIndices   = multiple_queues ? unique_queues.data() : nullptr,
                                       .preTransform          = capabilities.currentTransform,
                                       .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                       .presentMode           = present_mode,
                                       .clipped               = true,
                                       .oldSwapchain          = old_swapchain};

  VK_CHECK(vkCreateSwapchainKHR(pDevice, &create_info, nullptr, &mSwapchain.pSwapchain));
  mSwapchain.extent        = extent;
  mSwapchain.mode          = present_mode;
  mSwapchain.surfaceFormat = VkSurfaceFormatKHR{.format = format, .colorSpace = color_space};
  VK_ENUMERATE(vkGetSwapchainImagesKHR, mSwapchain.images, pDevice, mSwapchain.pSwapchain);

  std::cout << "Swapchain settings : ["
            << "Count: " << mSwapchain.images.size() << ", "
            << "SharingMode: " << create_info.imageSharingMode << ", "
            << "Extent: " << extent.width << "x" << extent.height << ", "
            << "Mode: " << present_mode << ", "
            << "Format: " << format << ", "
            << "ColorSpace: " << color_space << "]" << std::endl;

  VkImageViewCreateInfo view_info{
      .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType         = VK_IMAGE_VIEW_TYPE_2D,
      .format           = format,
      .components       = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

  for (const auto &image : mSwapchain.images) {
    view_info.image = image;
    VkImageView view;
    VK_CHECK(vkCreateImageView(pDevice, &view_info, nullptr, &view));
    mSwapchain.views.emplace_back(view);
  }

  VkSemaphoreCreateInfo sem_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  for (const auto &image : mSwapchain.images) {
    VkSemaphore semaphore;
    VK_CHECK(vkCreateSemaphore(pDevice, &sem_info, nullptr, &semaphore));
    mSwapchain.semaphores.emplace_back(semaphore);
  }
}

void Renderer::CreateSlangSession() {
  SlangGlobalSessionDesc desc{};
  SLANG_CHECK(createGlobalSession(&desc, mSlangContext.pGlobalSession.writeRef()));

  slang::TargetDesc target_desc{};
  target_desc.format  = SLANG_SPIRV;
  target_desc.profile = mSlangContext.pGlobalSession->findProfile("glsl_450");

  slang::CompilerOptionEntry scalar_layout{};
  scalar_layout.name = slang::CompilerOptionName::ForceCLayout;
  scalar_layout.value.kind = slang::CompilerOptionValueKind::Int;
  scalar_layout.value.intValue0 = 1;

  slang::SessionDesc session_desc{};
  session_desc.targetCount = 1;
  session_desc.targets     = &target_desc;
  session_desc.compilerOptionEntryCount = 1;
  session_desc.compilerOptionEntries = &scalar_layout;
  std::vector<const char *> search_paths{};
  session_desc.searchPathCount = (SlangInt)search_paths.size();
  session_desc.searchPaths     = search_paths.data();
  SLANG_CHECK(mSlangContext.pGlobalSession->createSession(session_desc, mSlangContext.pSession.writeRef()));
}

void Renderer::CreateMainDescriptorPool() {
  auto                              max_sets = 100;
  std::vector<VkDescriptorPoolSize> pool_sizes{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100},
                                               {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
                                               {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURES}, {VK_DESCRIPTOR_TYPE_SAMPLER, 32}};

  VkDescriptorPoolCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  create_info.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  create_info.maxSets       = max_sets;
  create_info.poolSizeCount = (uint32_t)pool_sizes.size();
  create_info.pPoolSizes    = pool_sizes.data();

  VK_CHECK(vkCreateDescriptorPool(pDevice, &create_info, nullptr, &pMainDescriptorPool));
}

void Renderer::CreateMainTexturesSet() {
  VkDescriptorSetLayoutBinding bindings[] = {
      {
          .binding         = 0,
          .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .descriptorCount = MAX_TEXTURES,
          .stageFlags      = VK_SHADER_STAGE_ALL_GRAPHICS,
      },
      {
          .binding         = 1,
          .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
          .descriptorCount = 32,
          .stageFlags      = VK_SHADER_STAGE_ALL_GRAPHICS,
      },
  };

  VkDescriptorBindingFlags binding_flags[] = {
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
  };

  VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
  flags_info.bindingCount  = 2;
  flags_info.pBindingFlags = binding_flags;

  VkDescriptorSetLayoutCreateInfo layout_create_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layout_create_info.bindingCount = 2;
  layout_create_info.pBindings    = bindings;
  layout_create_info.pNext        = &flags_info;
  layout_create_info.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

  VK_CHECK(vkCreateDescriptorSetLayout(pDevice, &layout_create_info, nullptr, &mTexturesSet.pDescriptorSetLayout));

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool     = pMainDescriptorPool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts        = &mTexturesSet.pDescriptorSetLayout;

  VK_CHECK(vkAllocateDescriptorSets(pDevice, &alloc_info, &mTexturesSet.pDescriptorSet));
}

void Renderer::CreateFrames() {
  VkCommandPoolCreateInfo cmd_pool_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  cmd_pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  cmd_pool_info.queueFamilyIndex = mGpu.stat.graphicQueueIndex;

  VkFenceCreateInfo fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkSemaphoreCreateInfo semaphore_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

  mFrames.resize(FRAME_IN_FLIGHT);
  for (auto &frame : mFrames) {
    VK_CHECK(vkCreateCommandPool(pDevice, &cmd_pool_info, nullptr, &frame.pCmdPool));
    auto cmd_buffer_info = InitCommandBufferAllocateInfo(frame.pCmdPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(pDevice, &cmd_buffer_info, &frame.pCmdBuffer));
    VK_CHECK(vkCreateFence(pDevice, &fence_info, nullptr, &frame.pFence));
    VK_CHECK(vkCreateSemaphore(pDevice, &semaphore_info, nullptr, &frame.pSemaphore));
  }
}

void Renderer::CreateImmediateSubmitTools() {
  VkCommandPoolCreateInfo cmd_pool_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  cmd_pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  cmd_pool_info.queueFamilyIndex = mGpu.stat.graphicQueueIndex;

  VkFenceCreateInfo fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VK_CHECK(vkCreateFence(pDevice, &fence_info, nullptr, &mImmediateSubmit.pFence));
  VK_CHECK(vkCreateCommandPool(pDevice, &cmd_pool_info, nullptr, &mImmediateSubmit.pCmdPool));
  auto cmd_info = InitCommandBufferAllocateInfo(mImmediateSubmit.pCmdPool, 1);
  VK_CHECK(vkAllocateCommandBuffers(pDevice, &cmd_info, &mImmediateSubmit.pCmdBuffer));
}

void Renderer::DestroyImmediateSubmitTools() {
  vkDestroyCommandPool(pDevice, mImmediateSubmit.pCmdPool, nullptr);
  vkDestroyFence(pDevice, mImmediateSubmit.pFence, nullptr);
}

void Renderer::DestroyFrames() {
  for (auto &frame : mFrames) {
    vkDestroySemaphore(pDevice, frame.pSemaphore, nullptr);
    vkDestroyFence(pDevice, frame.pFence, nullptr);
    vkDestroyCommandPool(pDevice, frame.pCmdPool, nullptr);
    for (auto &pair : frame.mUniformBuffers) {
      DestroyBuffer(pair.second);
    }
  }
}

void Renderer::DestroyMainDescriptorPool() { vkDestroyDescriptorPool(pDevice, pMainDescriptorPool, nullptr); }

void Renderer::DestroySlangSession() {}

void Renderer::DestroySwapchain(Swapchain &swapchain) {
  CHECK(swapchain.semaphores.size() == swapchain.images.size());
  CHECK(swapchain.views.size() == swapchain.images.size());

  for (size_t i = 0; i < swapchain.images.size(); i++) {
    vkDestroySemaphore(pDevice, swapchain.semaphores[i], nullptr);
    vkDestroyImageView(pDevice, swapchain.views[i], nullptr);
  }

  vkDestroySwapchainKHR(pDevice, swapchain.pSwapchain, nullptr);
}
void Renderer::DestroyVmaAllocator() { vmaDestroyAllocator(pVmaAllocator); }
void Renderer::DestroyDevice() { vkDestroyDevice(pDevice, nullptr); }
void Renderer::DestroySurface() { vkDestroySurfaceKHR(pInstance, pSurface, nullptr); }
void Renderer::DestroyInstance() { vkDestroyInstance(pInstance, nullptr); }

#ifndef NDEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT,
                                                            const VkDebugUtilsMessengerCallbackDataEXT *p_data, void *) {
  switch (message_severity) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    std::cout << "[VULKAN][ERROR]: ";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    std::cout << "[VULKAN][WARNING] ";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    std::cout << "[VULKAN][INFO] ";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    std::cout << "[VULKAN][VERBOSE] ";
    break;
  default:
    std::cout << "[VULKAN][UNKN]: ";
  };
  std::cout << p_data->pMessage << "\n";
  return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT InitDebugMessengerInfo() {
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info{};
  debug_messenger_info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug_messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
      // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
      //   VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
      ;
  debug_messenger_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
  debug_messenger_info.pfnUserCallback = vulkan_debug_callback;

  return debug_messenger_info;
}

void Renderer::CreateDebugMessenger() {
  auto debug_messenger_info           = InitDebugMessengerInfo();
  auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(pInstance, "vkCreateDebugUtilsMessengerEXT");
  CHECK(vkCreateDebugUtilsMessengerEXT);
  vkCreateDebugUtilsMessengerEXT(pInstance, &debug_messenger_info, nullptr, &pDebugMessenger);
}

void Renderer::DestroyDebugMessenger() {
  auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(pInstance, "vkDestroyDebugUtilsMessengerEXT");
  CHECK(vkDestroyDebugUtilsMessengerEXT);
  vkDestroyDebugUtilsMessengerEXT(pInstance, pDebugMessenger, nullptr);
}
#endif
