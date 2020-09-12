#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "macros.h"
#include "sx/math.h"
#include "sx/allocator.h"
#include "sx/hash.h"
#include "device/types.h"

#if SX_PLATFORM_ANDROID
#	define KHR_SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#elif SX_PLATFORM_LINUX
#	define KHR_SURFACE_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
#elif SX_PLATFORM_WINDOWS
#	define KHR_SURFACE_EXTENSION_NAME  VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#else
#	define KHR_SURFACE_EXTENSION_NAME ""
#endif // BX_PLATFORM_*

#define VK_NO_STDINT_H
#define VK_NO_PROTOTYPES
#include "volk/volk.h"
//#include "vulkan/vulkan.h"

#define DEBUG_RENDERER


typedef struct DeviceVk {
    VkInstance vk_instance;
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    VkSurfaceKHR presentation_surface;
} DeviceVk;

typedef struct DescriptorPoolInfo {
    VkDescriptorPoolSize* pool_sizes;
    uint32_t pool_size_count;
    uint32_t max_sets;
} DescriptorPoolInfo;

typedef struct DescriptorLayoutInfo {
    VkDescriptorSetLayoutBinding* bindings;
    uint32_t num_bindings;
} DescriptorLayoutInfo;


typedef struct Buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    uint32_t size;
} Buffer;

typedef struct ImageBuffer {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView image_view;
    VkFormat format;
} ImageBuffer;

typedef struct Texture {
    ImageBuffer image_buffer;
    VkSampler sampler;
} Texture;

typedef struct DescriptorSetUpdateInfo {
    VkDescriptorBufferInfo* buffer_infos;
    VkDescriptorType* buffer_descriptor_types;
    uint32_t* buffer_bindings;
    uint32_t num_buffer_bindings;

    VkDescriptorImageInfo* images_infos;
    VkDescriptorType* image_descriptor_types;
    uint32_t* image_bindings;
    uint32_t num_image_bindings;

    VkBufferView* texel_buffer_views;
    VkDescriptorType* texel_descriptor_types;
    uint32_t* texel_bindings;
    uint32_t num_texel_bindings;

    VkDescriptorSet descriptor_set;
} DescriptorSetUpdateInfo;


typedef struct Swapchain {
    VkSwapchainKHR swapchain;
    VkSurfaceFormatKHR format;
    VkImage images[4];
    VkImageView image_views[4];
} Swapchain;

typedef enum QueueType {
    PRESENT,
    GRAPHICS,
    COMPUTE,
    TRANSFER
} QueueType;

typedef struct ComputePipelineInfo {
    VkPipelineLayout* layouts;
    VkPipelineShaderStageCreateInfo* shader;
    uint32_t num_pipelines;
} ComputePipelineInfo;

typedef struct PipelineLayoutInfo {
    VkDescriptorSetLayout* descriptor_layouts;
    uint32_t descriptor_layout_count;
    VkPushConstantRange* push_constant_ranges;
    uint32_t push_constant_count;
} PipelineLayoutInfo;

typedef struct CommandSubmitInfo {
    QueueType queue;
    uint32_t command_buffers_count;
    VkCommandBuffer* command_buffers;
    uint32_t wait_semaphores_count;
    VkSemaphore* wait_semaphores;
    VkPipelineStageFlags wait_dst_stage_mask;
    uint32_t signal_semaphores_count;
    VkSemaphore* signal_semaphores;
    VkFence fence;
} CommandSubmitInfo;


bool vk_renderer_init(DeviceWindow win);
bool vk_resize(uint32_t width, uint32_t height);
void vk_renderer_cleanup();


Swapchain create_swapchain(uint32_t width, uint32_t height);

VkResult create_command_buffer(QueueType type, VkCommandBufferLevel level, uint32_t count, VkCommandBuffer* cmdbuffer);

VkResult create_buffer(Buffer* buffer, VkBufferUsageFlags usage, 
                       VkMemoryPropertyFlags memory_properties_flags, VkDeviceSize size);

VkResult copy_buffer(Buffer* dst_buffer, void* data, VkDeviceSize size);

VkResult copy_buffer_staged(Buffer* dst_buffer, void* data, VkDeviceSize size);

VkResult create_texture(Texture* texture, VkSamplerAddressMode sampler_address_mode, const sx_alloc* alloc, const char* filepath);
VkResult create_texture_from_data(Texture* texture, VkSamplerAddressMode sampler_address_mode, const sx_alloc* alloc, 
        const void* data, uint32_t width, uint32_t height);

VkPipelineShaderStageCreateInfo load_shader(VkDevice logical_device, const char* filnename, VkShaderStageFlagBits stage);

void clear_buffer(DeviceVk* device, Buffer* buffer);

void clear_texture(DeviceVk* device, Texture* texture);

VkPipelineShaderStageCreateInfo load_shader(VkDevice logical_device, const char* filnename, VkShaderStageFlagBits stage);

VkPipelineShaderStageCreateInfo create_shader_module(const char* filename, VkShaderStageFlagBits stage);

VkResult create_descriptor_pool(DescriptorPoolInfo* info, VkDescriptorPool* pool);

VkResult create_descriptor_layout(DescriptorLayoutInfo* info, VkDescriptorSetLayout* layout);

VkResult create_descriptor_sets(VkDescriptorPool pool, VkDescriptorSetLayout* layouts, uint32_t num_layouts, VkDescriptorSet* sets);

void update_descriptor_set(DescriptorSetUpdateInfo* info);

VkResult create_compute_pipeline(ComputePipelineInfo* info, VkPipeline* compute_pipeline);

VkResult create_pipeline_layout(PipelineLayoutInfo* info, VkPipelineLayout* pipeline_layout);

VkResult create_fence(VkFence* fence, bool begin_signaled);

VkResult wait_fences(uint32_t num_fences, VkFence* fence, bool wait_all, uint64_t timeout);

void destroy_fence(VkFence fence);

VkResult submit_commands(CommandSubmitInfo* info);

void* map_buffer_memory(Buffer* buffer, VkDeviceSize offset);

void unmap_buffer_memory(Buffer* buffer);
