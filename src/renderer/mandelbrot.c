#include "renderer/mandelbrot.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define WIDTH 512
#define HEIGHT 512
#define WORKGROUP_SIZE 32

void compute_mandelbrot(MandelbrotInfo *info) {
    uint32_t size = 4 * sizeof(float) * WIDTH * HEIGHT;
    create_buffer(&info->buffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size);

    // Descriptor pool
    VkDescriptorPoolSize pool_size[1];
    pool_size[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size[0].descriptorCount = 1;
    DescriptorPoolInfo pool_info = { .pool_sizes = pool_size, .pool_size_count = 1, .max_sets = 1};
    VkResult result = create_descriptor_pool(&pool_info, &info->descriptor_pool);
    sx_assert_rel(result == VK_SUCCESS && "Could not create descriptor pool");

    // Descriptor Set layout
    VkDescriptorSetLayoutBinding bindings[1];
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = NULL;
    DescriptorLayoutInfo layout_info = { .bindings = bindings, .num_bindings = 1};
    result = create_descriptor_layout(&layout_info, &info->descriptor_layout);
    VK_CHECK_RESULT(result);

    // Descriptor set
    result = create_descriptor_sets(info->descriptor_pool, &info->descriptor_layout, 1, &info->descriptor_set);
    sx_assert_rel(result == VK_SUCCESS && "Could not create descriptor set");

    // Update Descriptors
    VkDescriptorBufferInfo buffer_info;
    buffer_info.buffer = info->buffer.buffer;
    buffer_info.offset = 0;
    buffer_info.range = info->buffer.size;
    uint32_t buffer_binding = 0;
    VkDescriptorType desc_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    DescriptorSetUpdateInfo update_info = {0};
    update_info.buffer_infos = &buffer_info;
    update_info.buffer_descriptor_types = &desc_type;
    update_info.buffer_bindings = &buffer_binding;
    update_info.descriptor_set = info->descriptor_set;
    update_info.num_buffer_bindings = 1;
    update_info.num_image_bindings = 0;
    update_info.num_texel_bindings = 0;
    update_descriptor_set(&update_info);

    result = create_command_buffer(COMPUTE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, &info->command_buffer);
    sx_assert_rel(result == VK_SUCCESS && "Could not create command buffer");

    // Shader module creation
    VkPipelineShaderStageCreateInfo shaderstage_create_info =
        create_shader_module("shaders/mandelbrot.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

    PipelineLayoutInfo pipeline_layout_info;
    pipeline_layout_info.descriptor_layout_count = 1;
    pipeline_layout_info.descriptor_layouts = &info->descriptor_layout;
    pipeline_layout_info.push_constant_count = 0;
    pipeline_layout_info.push_constant_ranges = NULL;

    result = create_pipeline_layout(&pipeline_layout_info, &info->pipeline_layout);
    sx_assert_rel(result == VK_SUCCESS && "Could not create pipeline layout");

    ComputePipelineInfo pipeline_info;
    pipeline_info.num_pipelines = 1;
    pipeline_info.layouts = &info->pipeline_layout;
    pipeline_info.shader = &shaderstage_create_info;

    result = create_compute_pipeline(&pipeline_info, &info->pipeline);
    sx_assert_rel(result == VK_SUCCESS && "Could not create compute pipeline");
    
    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 
                                            .flags = 0, .pInheritanceInfo = NULL};
    result = vkBeginCommandBuffer(info->command_buffer, &begin_info);
    sx_assert_rel(result == VK_SUCCESS && "Could not begin command buffer");

    vkCmdBindPipeline(info->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, info->pipeline);
    vkCmdBindDescriptorSets(info->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, info->pipeline_layout, 0, 1, 
                            &info->descriptor_set, 0, NULL);

    vkCmdDispatch(info->command_buffer, (uint32_t)sx_ceil(WIDTH / (float)WORKGROUP_SIZE),
                                        (uint32_t)sx_ceil(HEIGHT / (float)WORKGROUP_SIZE), 1);

    result = vkEndCommandBuffer(info->command_buffer);
    sx_assert_rel(result == VK_SUCCESS && "Could not finish command buffer record");

    VkFence fence;
    result = create_fence(&fence, false);
    sx_assert_rel(result == VK_SUCCESS && "Could not create fence");

    CommandSubmitInfo submit_info = {0};
    submit_info.command_buffers_count = 1;
    submit_info.command_buffers = &info->command_buffer;
    submit_info.signal_semaphores_count = 0;
    submit_info.signal_semaphores = NULL;
    submit_info.wait_semaphores_count = 0;
    submit_info.wait_semaphores = NULL;
    submit_info.wait_dst_stage_mask = 0;
    submit_info.fence = fence;
    submit_info.queue = COMPUTE;
    result = submit_commands(&submit_info);
    sx_assert_rel(result == VK_SUCCESS && "Failed to submit commands");

    result = wait_fences(1, &fence, true, 100000000000);
    sx_assert_rel(result == VK_SUCCESS && "Failed to wait fence");

    destroy_fence(fence);

}


void save_image(MandelbrotInfo* info) {
    uint8_t *pixels = sx_malloc(sx_alloc_malloc(), WIDTH * HEIGHT * 4);
    struct Pixel { float r, g, b, a;};
    struct Pixel* data = (struct Pixel*)map_buffer_memory(&info->buffer, 0);
    uint64_t index = 0;
    for (uint32_t i = 0; i < WIDTH * HEIGHT; i++) {
        /*printf("pr: %d\n", data[i].r);*/
        pixels[index++] = (uint8_t)(255.0 * data[i].r);
        pixels[index++] = (uint8_t)(255.0 * data[i].g);
        pixels[index++] = (uint8_t)(255.0 * data[i].b);
        pixels[index++] = (uint8_t)(255.0 * data[i].a);
    }

    stbi_write_png("mandelbrot.png", WIDTH, HEIGHT, 4, pixels, WIDTH * 4);

    unmap_buffer_memory(&info->buffer);
}
