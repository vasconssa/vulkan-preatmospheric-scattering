#pragma once

#include "renderer/vk_renderer.h"

typedef void(*draw_callback)(VkCommandBuffer);

typedef struct Renderer {
    const sx_alloc* alloc;

    Swapchain swapchain;
    VkFramebuffer* framebuffer;

    VkCommandBuffer* present_cmdbuffer;
    VkCommandBuffer* graphic_cmdbuffer;
    VkCommandBuffer transfer_cmdbuffer;
    VkCommandBuffer compute_cmdbuffer;
    VkCommandBuffer copy_cmdbuffer;

    VkSemaphore *image_available_semaphore;
    VkSemaphore *rendering_finished_semaphore;
    VkFence *fences;

    VkRenderPass render_pass;
    draw_callback subpass_callbacks[3][20];
    uint32_t subpass_callbacks_count[3];

    VkDescriptorPool global_descriptor_pool;
    VkDescriptorPool composition_descriptor_pool;

    VkDescriptorSetLayout global_descriptor_layout;
    VkDescriptorSetLayout composition_descriptorset_layout;

    VkDescriptorSet global_descriptorset;
    VkDescriptorSet composition_descriptorset;

    VkPipelineLayout composition_pipeline_layout;
    VkPipeline composition_pipeline;

    ImageBuffer depth_image;
    /* G-Buffer */
    ImageBuffer position_image;
    ImageBuffer normal_image;
    ImageBuffer albedo_image;
    ImageBuffer metallic_roughness_image;

    Texture lut_brdf;
    Texture irradiance_cube;
    Texture prefiltered_cube;
    Buffer global_uniform_buffer;

    uint32_t width;
    uint32_t height;

} Renderer;

typedef struct GlobaUBO {
    sx_mat4 projection;
    sx_mat4 view;
    sx_mat4 projection_view;
    sx_vec4 light_position[4];
    sx_vec4 camera_position;
    sx_vec4 exposure_gama;
} GlobalUBO;


Renderer* create_renderer(const sx_alloc* alloc, uint32_t width, uint32_t height);

bool renderer_draw(Renderer* rd);

void renderer_render(Renderer* rd);

void renderer_resize(Renderer* rd, uint32_t width, uint32_t height);

void renderer_register_callback(Renderer* rd, draw_callback callback, uint32_t subpass_index);

