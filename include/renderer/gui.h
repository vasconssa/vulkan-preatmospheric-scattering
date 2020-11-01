#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "sx/allocator.h"
#include "sx/math.h"
#include "renderer/vk_renderer.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "nuklear/nuklear.h"

#define MAX_NKTEXTURES 8

typedef struct nk_vertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
} nk_vertex;

typedef struct GuiPushConstantBlock {
    sx_vec2 scale;
    sx_vec2 translate;
} GuiPushConstantBlock;

typedef struct GuiData {
    struct nk_buffer cmds;
    struct nk_draw_null_texture null;
    struct nk_context context;
    struct nk_font_atlas atlas;
    struct nk_vec2 fb_scale;

    GuiPushConstantBlock push_constants_block;

    Buffer vertex_buffer;
    Buffer index_buffer;

    Texture textures[MAX_NKTEXTURES];

    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorSet descriptor_set;

    VkCommandBuffer command_buffer;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkFence fence;

} GuiData;

void gui_create(GuiData* data);
void gui_update(GuiData* data);
void gui_draw(GuiData* data, VkCommandBuffer command_buffer);

NK_API void nk_font_stash_begin(GuiData* gui, struct nk_font_atlas** atlas);
NK_API void nk_font_stash_end(GuiData* gui);
