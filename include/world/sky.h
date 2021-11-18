#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "sx/allocator.h"
#include "sx/math.h"
#include "world/renderer.h"

#include "vulkan/vulkan_core.h"

typedef struct Atmosphere {
    sx_vec4 rayleigh_scattering;
    sx_vec4 mie_scattering;
    sx_vec4 mie_absorption;
    sx_vec4 sun_direction;
    float bottom_radius;
    float top_radius;
} Atmosphere;

typedef struct Sky {
    const sx_alloc* alloc;
    Atmosphere atmosphere;
    Buffer atmospher_ubo;
    float rayleigh_scattering_scale;
    float mie_scattering_scale;
    float mie_absorption_scale;


    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorSet descriptor_set;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    Texture transmittance_tex;

    VkCommandBuffer transmittance_cmd_buffer;
    VkDescriptorPool transmittance_descriptor_pool;
    VkDescriptorSetLayout transmittance_descriptor_layout;
    VkDescriptorSet transmittance_descriptor_set;

    VkPipelineLayout transmittance_pipeline_layout;
    VkPipeline transmittance_pipeline;

    Texture multi_scat_tex;

    VkCommandBuffer multi_scat_cmd_buffer;
    VkDescriptorPool multi_scat_descriptor_pool;
    VkDescriptorSetLayout multi_scat_descriptor_layout;
    VkDescriptorSet multi_scat_descriptor_set;

    VkPipelineLayout multi_scat_pipeline_layout;
    VkPipeline multi_scat_pipeline;

    VkFence fence;

    Renderer* rd;
} Sky;

Sky* sky_create(const sx_alloc* alloc, Renderer* rd);
void sky_update(Sky* sky);
