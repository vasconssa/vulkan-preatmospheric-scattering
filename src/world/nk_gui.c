#include <stdarg.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear/nuklear.h"
#undef NK_IMPLEMENTATION

#include "world/nk_gui.h"

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER  128 * 1024

void update_gui_descriptors(NkGui* gui);
void nkgui_draw(VkCommandBuffer cmdbuffer);

NkGui* nk_gui;

/*{{{NkGui* nkgui_create(const sx_alloc* alloc, Renderer* rd) */
NkGui* nkgui_create(const sx_alloc* alloc, Renderer* rd) {
    VkResult result;
    NkGui* gui = sx_malloc(alloc, sizeof(*gui));
    nk_gui = gui;
    gui->alloc = alloc;
    gui->rd = rd;
    nk_buffer_init_default(&gui->cmds);
    nk_init_default(&gui->context, 0);
    gui->fb_scale.x = 1;
    gui->fb_scale.y = 1;

    result = create_buffer(&gui->vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, MAX_VERTEX_BUFFER);
    VK_CHECK_RESULT(result)
    result = create_buffer(&gui->index_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, MAX_INDEX_BUFFER);
    VK_CHECK_RESULT(result)

    for (int32_t i = 0; i < MAX_NKTEXTURES; i++) {
        result = create_texture(&gui->textures[i], VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, gui->alloc, "misc/empty.ktx");
        VK_CHECK_RESULT(result)
    }
    //Descriptor pool
    VkDescriptorPoolSize pool_size[1];
    pool_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size[0].descriptorCount = MAX_NKTEXTURES;
    DescriptorPoolInfo pool_info;
    pool_info.pool_size_count = 1;
    pool_info.pool_sizes = pool_size;
    pool_info.max_sets = 1;
    result = create_descriptor_pool(&pool_info, &gui->descriptor_pool);
    VK_CHECK_RESULT(result);

    VkDescriptorSetLayoutBinding layout_bindings[1];
    layout_bindings[0].binding = 0;
    layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layout_bindings[0].descriptorCount = MAX_NKTEXTURES;
    layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layout_bindings[0].pImmutableSamplers = NULL;
    DescriptorLayoutInfo layout_info;
    layout_info.bindings =  layout_bindings;
    layout_info.num_bindings = 1;
    result = create_descriptor_layout(&layout_info, &gui->descriptor_layout);
    VK_CHECK_RESULT(result);

    result = create_descriptor_sets(gui->descriptor_pool, &gui->descriptor_layout, 1, &gui->descriptor_set);
    VK_CHECK_RESULT(result);


    /* Pipelines creation {{{*/
    {
        /* Common pipeline state {{{*/ 
		VkVertexInputBindingDescription vertex_binding_descriptions[1] = {
            {
                .binding = 0,
                .stride  = sizeof(nk_vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
        };
        //Position, Normal, uv
		VkVertexInputAttributeDescription vertex_attribute_descriptions[3] = {
            {
                .location = 0,
                .binding = vertex_binding_descriptions[0].binding,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(nk_vertex, position)
            },
            {
                .location = 1,
                .binding = vertex_binding_descriptions[0].binding,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(nk_vertex, uv)
            },
            {
                .location = 2,
                .binding = vertex_binding_descriptions[0].binding,
                .format = VK_FORMAT_R8G8B8A8_UINT,
                .offset = offsetof(nk_vertex, col)
            }
        };
        
        VertexInputStateInfo vertex_input_state_info;
        vertex_input_state_info.binding_descriptions = vertex_binding_descriptions;
        vertex_input_state_info.binding_count = 1;
        vertex_input_state_info.attribute_descriptions = vertex_attribute_descriptions;
        vertex_input_state_info.attribute_count = 3;


        InputAssemblyStateInfo input_assembly_state_info;
        input_assembly_state_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state_info.restart_enabled = VK_FALSE;

        RasterizationStateInfo rasterization_state_info;
        rasterization_state_info.polygon_mode = VK_POLYGON_MODE_FILL;
        rasterization_state_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        rasterization_state_info.front_face = VK_FRONT_FACE_CLOCKWISE;

        MultisampleStateInfo multisample_state_info;
        multisample_state_info.samples = VK_SAMPLE_COUNT_1_BIT;
        multisample_state_info.shadingenable = VK_FALSE;
        multisample_state_info.min_shading = 1.0;

        DepthStencilStateInfo depth_stencil_state_info;
        depth_stencil_state_info.depth_test_enable = VK_TRUE;
        depth_stencil_state_info.depth_write_enable = VK_TRUE;
        depth_stencil_state_info.depht_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;

        bool blend_enables[1] = {true};
        
        ColorBlendStateInfo color_blend_state_info;
        color_blend_state_info.blend_enables = blend_enables;
        color_blend_state_info.attachment_count = 1;

        VkDescriptorSetLayout descriptor_set_layouts[2] = {
            rd->global_descriptor_layout,
            gui->descriptor_layout
        };

        VkPushConstantRange push_constants_range = {
            .offset = 0,
            .size = sizeof(NkGuiPushConstantBlock),
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        };

        PipelineLayoutInfo pipeline_layout_info = {0};
        pipeline_layout_info.descriptor_layouts = descriptor_set_layouts;
        pipeline_layout_info.descriptor_layout_count = 2;
        pipeline_layout_info.push_constant_ranges = &push_constants_range;
        pipeline_layout_info.push_constant_count = 1;

        result = create_pipeline_layout(&pipeline_layout_info, &gui->pipeline_layout);
        VK_CHECK_RESULT(result);



        VkPipelineShaderStageCreateInfo shader_stages[2];
        shader_stages[0] = create_shader_module("shaders/nk_gui.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shader_stages[1] = create_shader_module("shaders/nk_gui.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

        GraphicPipelineInfo graphic_pipeline_info;
        graphic_pipeline_info.vertex_input = &vertex_input_state_info;
        graphic_pipeline_info.input_assembly = &input_assembly_state_info;
        graphic_pipeline_info.rasterization = &rasterization_state_info;
        graphic_pipeline_info.multisample = &multisample_state_info;
        graphic_pipeline_info.depth_stencil = &depth_stencil_state_info;
        graphic_pipeline_info.color_blend = &color_blend_state_info;
        graphic_pipeline_info.render_pass = rd->render_pass;
        graphic_pipeline_info.subpass = 2;
        graphic_pipeline_info.layout = &gui->pipeline_layout;
        graphic_pipeline_info.shader_stages = shader_stages;
        graphic_pipeline_info.shader_stages_count = 2;

        result = create_graphic_pipeline(&graphic_pipeline_info, &gui->pipeline);
        VK_CHECK_RESULT(result);

        /*}}}*/

    }
    /*}}}*/
    renderer_register_callback(gui->rd, nkgui_draw, 2);

    return gui;
}
/*}}}*/

/*{{{NK_API void nk_font_stash_begin(NkGui* gui, struct nk_font_atlas** atlas) */
NK_API void nk_font_stash_begin(NkGui* gui, struct nk_font_atlas** atlas) {
    nk_font_atlas_init_default(&gui->atlas);
    nk_font_atlas_begin(&gui->atlas);
    *atlas = &gui->atlas;
}
/*}}}*/

/*{{{NK_API void nk_font_stash_end(NkGui* gui) */
NK_API void nk_font_stash_end(NkGui* gui) {
    VkResult result;
    const void* image;
    int w, h;
    image = nk_font_atlas_bake(&gui->atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    result = create_texture_from_data(&gui->textures[0], VK_SAMPLER_ADDRESS_MODE_REPEAT, gui->alloc, (uint8_t*) image, w, h);
    VK_CHECK_RESULT(result);
    update_gui_descriptors(gui);
    nk_font_atlas_end(&gui->atlas, nk_handle_id(0), &gui->null);
    if (gui->atlas.default_font) {
        nk_style_set_font(&gui->context, &gui->atlas.default_font->handle);
    }
}
/*}}}*/

/*{{{void update_gui_descriptors(NkGui* gui)*/
void update_gui_descriptors(NkGui* gui) {
    VkDescriptorImageInfo image_info[MAX_NKTEXTURES];
    for (int32_t i = 0; i < MAX_NKTEXTURES; i++) {
        image_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[i].imageView = gui->textures[i].image_buffer.image_view;
        image_info[i].sampler = gui->textures[i].sampler;
    }
    uint32_t image_bindings = 0;
    uint32_t descriptor_count = MAX_NKTEXTURES;
    VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    DescriptorSetUpdateInfo update_info = {0};
    update_info.images_infos = image_info;
    update_info.image_bindings = &image_bindings;
    update_info.num_image_bindings = 1;
    update_info.image_descriptor_count = &descriptor_count;
    update_info.image_descriptor_types = &descriptor_type;
    update_info.descriptor_set = gui->descriptor_set;

    update_descriptor_set(&update_info);
}
/*}}}*/

/*{{{void nkgui_update(NkGui* gui) */
void nkgui_update(NkGui* gui) {
    struct nk_buffer vbuf, ibuf;
    uint8_t vertices[MAX_VERTEX_BUFFER];
    uint8_t indices[MAX_INDEX_BUFFER];
    
    /* fill convert configuration */
    struct nk_convert_config config;
    static const struct nk_draw_vertex_layout_element vertex_layout[] = {
        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(nk_vertex, position)},
        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(nk_vertex, uv)},
        {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(nk_vertex, col)},
        {NK_VERTEX_LAYOUT_END}
    };
    sx_memset(&config, 0, sizeof(config));
    config.vertex_layout = vertex_layout;
    config.vertex_size = sizeof(nk_vertex);
    config.vertex_alignment = NK_ALIGNOF(nk_vertex);
    config.null = gui->null;
    config.circle_segment_count = 22;
    config.curve_segment_count = 22;
    config.arc_segment_count = 22;
    config.global_alpha = 1.0f;
    config.shape_AA = NK_ANTI_ALIASING_ON;
    config.line_AA = NK_ANTI_ALIASING_ON;

    /* setup buffers to load vertices and elements */
    nk_buffer_init_fixed(&vbuf, vertices, (size_t)MAX_VERTEX_BUFFER);
    nk_buffer_init_fixed(&ibuf, indices, (size_t)MAX_INDEX_BUFFER);
    nk_convert(&gui->context, &gui->cmds, &vbuf, &ibuf, &config);
    copy_buffer(&gui->vertex_buffer, vertices, sizeof(vertices));
    copy_buffer(&gui->index_buffer, indices, sizeof(indices));
}
/*}}}*/

/*{{{void nkgui_draw(VkCommandBuffer cmdbuffer) */
void nkgui_draw(VkCommandBuffer cmdbuffer) {
    vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, nk_gui->pipeline);
    vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, nk_gui->pipeline_layout, 1, 1, &nk_gui->descriptor_set, 0, NULL);
    
    nk_gui->push_constants_block.scale.x = 2.0 / nk_gui->rd->width;
    nk_gui->push_constants_block.scale.y = 2.0 / nk_gui->rd->height;
    nk_gui->push_constants_block.translate.x = -1.0;
    nk_gui->push_constants_block.translate.y = -1.0;

    vkCmdPushConstants(cmdbuffer, nk_gui->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(NkGuiPushConstantBlock), &nk_gui->push_constants_block);

    const struct nk_draw_command* cmd;

    VkDeviceSize doffset = 0;
    vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &nk_gui->vertex_buffer.buffer, &doffset);
    vkCmdBindIndexBuffer(cmdbuffer, nk_gui->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT16);

    uint32_t index_offset = 0;
    nk_draw_foreach(cmd, &nk_gui->context, &nk_gui->cmds) {
        if (!cmd->elem_count) continue;

        VkRect2D scissor = {
            .extent = {
                .width = cmd->clip_rect.w * nk_gui->fb_scale.x,
                .height = cmd->clip_rect.h * nk_gui->fb_scale.y
            },
            .offset = {
                .x = sx_max(cmd->clip_rect.x * nk_gui->fb_scale.x, (float)0.0),
                .y = sx_max(cmd->clip_rect.y * nk_gui->fb_scale.y, (float)0.0)
            }
        };

        vkCmdSetScissor(cmdbuffer, 0, 1, &scissor);
        vkCmdDrawIndexed(cmdbuffer, cmd->elem_count, 1, index_offset, 0, 0);
        index_offset += cmd->elem_count;
    }

}
/*}}}*/
