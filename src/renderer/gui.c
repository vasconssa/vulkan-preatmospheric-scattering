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

#include "renderer/gui.h"

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER  128 * 1024

/*{{{void gui_create(GuiData* data)*/
void gui_create(GuiData* data) {
    VkResult result;
    nk_buffer_init_default(&data->cmds);
    nk_init_default(&data->context, 0);
    data->fb_scale.x = 1;
    data->fb_scale.y = 1;

    result = create_buffer(&data->vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, MAX_VERTEX_BUFFER);
    VK_CHECK_RESULT(result)
    result = create_buffer(&data->index_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, MAX_INDEX_BUFFER);
    VK_CHECK_RESULT(result)
    //Descriptor pool
    VkDescriptorPoolSize pool_size[1];
    pool_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size[0].descriptorCount = MAX_NKTEXTURES;
    DescriptorPoolInfo pool_info;
    pool_info.pool_size_count = 1;
    pool_info.pool_sizes = pool_size;
    pool_info.max_sets = 1;
    result = create_descriptor_pool(&pool_info, &data->descriptor_pool);
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
    result = create_descriptor_layout(&layout_info, &data->descriptor_layout);
    VK_CHECK_RESULT(result);

    result = create_descriptor_sets(data->descriptor_pool, &data->descriptor_layout, 1, &data->descriptor_set);
    VK_CHECK_RESULT(result);

}
/*}}}*/

/*{{{void gui_update(GuiData* data) */
void gui_update(GuiData* data) {
}
/*}}}*/

/*{{{void gui_draw(GuiData* data, VkCommandBuffer command_buffer) */
void gui_draw(GuiData* data, VkCommandBuffer command_buffer) {
}
/*}}}*/

/*{{{NK_API void nk_font_stash_begin(GuiData* gui, struct nk_font_atlas** atlas) */
NK_API void nk_font_stash_begin(GuiData* gui, struct nk_font_atlas** atlas) {
}
/*}}}*/

/*{{{NK_API void nk_font_stash_end(GuiData* gui) */
NK_API void nk_font_stash_end(GuiData* gui) {
}
/*}}}*/
