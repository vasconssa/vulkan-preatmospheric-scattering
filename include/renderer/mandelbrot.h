#pragma once

#include "renderer/vk_renderer.h"

typedef struct MandelbrotInfo {
    Buffer buffer;

    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorSet descriptor_set;

    VkCommandBuffer command_buffer;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkFence fence;

} MandelbrotInfo;

void compute_mandelbrot(MandelbrotInfo *info);
void save_image(MandelbrotInfo* info);
