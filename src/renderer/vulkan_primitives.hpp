#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>

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

struct Swapchain {
  VkSwapchainKHR           pSwapchain;
  VkPresentModeKHR         mode;
  VkSurfaceFormatKHR       surfaceFormat;
  VkExtent2D               extent;
  std::vector<VkImage>     images;
  std::vector<VkImageView> views;
  std::vector<VkSemaphore> semaphores;
};

struct Pipeline {
  VkPipeline       pPipeline;
  VkPipelineLayout pLayout;
};

struct Buffer {
  VkBuffer          pBuffer;
  VmaAllocation     pVmaAllocation;
  VmaAllocationInfo mVmaAllocationInfo;
};

struct Shader {
  VkShaderModule pShader;
  VkDevice       pDevice;
};

struct DescriptorSet {
  VkDescriptorSet       pDescriptorSet;
  VkDescriptorSetLayout pDescriptorSetLayout;
};

struct Texture {
  Image   mImage;
  Sampler mSampler;
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

enum BufferType {
  BUFFER_TYPE_SSBO = 0,
  BUFFER_TYPE_UBO  = 1,
};

enum BufferUsage {
  BUFFER_USAGE_UNKNOWN = 0,
  BUFFER_USAGE_VERTEX  = 1,
  BUFFER_USAGE_INDEX   = 2,
};
