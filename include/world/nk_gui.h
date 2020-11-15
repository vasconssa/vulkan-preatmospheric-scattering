#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "sx/allocator.h"
#include "sx/math.h"
#include "world/renderer.h"

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

typedef struct NkGuiPushConstantBlock {
    sx_vec2 scale;
    sx_vec2 translate;
} NkGuiPushConstantBlock;

typedef struct NkGui {
    const sx_alloc* alloc;
    struct nk_buffer cmds;
    struct nk_draw_null_texture null;
    struct nk_context context;
    struct nk_font_atlas atlas;
    struct nk_vec2 fb_scale;

    NkGuiPushConstantBlock push_constants_block;

    Buffer vertex_buffer;
    Buffer index_buffer;

    Texture textures[MAX_NKTEXTURES];

    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorSet descriptor_set;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkFence fence;

    Renderer* rd;
} NkGui;

NkGui* nkgui_create(const sx_alloc* alloc, Renderer* rd);
void nkgui_update(NkGui* gui);

NK_API void nk_font_stash_begin(NkGui* gui, struct nk_font_atlas** atlas);
NK_API void nk_font_stash_end(NkGui* gui);
