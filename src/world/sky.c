#include "world/sky.h"
#include "renderer/vk_renderer.h"
#include "sx/math.h"
#include "vulkan/vulkan_core.h"

void sky_draw(VkCommandBuffer cmdbuffer);

Sky* global_sky;

Sky* sky_create(const sx_alloc* alloc, Renderer *rd) {
    VkResult result;
    Sky* sky = sx_malloc(alloc, sizeof(*sky));
    global_sky = sky;
    sky->alloc = alloc;
    sky->rd = rd;
    sky->atmosphere.bottom_radius = 6371.f;
    sky->atmosphere.top_radius = 6471.f;
    sky->rayleigh_scattering_scale = 0.1f;
    sky->mie_scattering_scale = 0.01f;
    sky->mie_absorption_scale = 0.01f;
    sky->atmosphere.rayleigh_scattering = sx_vec4_mulf((sx_vec4){ .x = 0.05802f, .y = 0.13558f, .z = 0.33100f }, sky->rayleigh_scattering_scale);
    sky->atmosphere.mie_scattering = sx_vec4_mulf((sx_vec4){ .x = 0.3996f, .y = 0.3996f, .z = 0.3996f }, sky->mie_scattering_scale);
    sky->atmosphere.mie_absorption = sx_vec4_mulf((sx_vec4){ .x = 0.4440f, .y = 0.4440f, .z = 0.4440f }, sky->mie_absorption_scale);
    sx_quat sun_rotation = { { 0.7071068f, 0.0f, 0.0f, -0.7071068f } };
    const sx_vec3 forward = { { 0.0, 0.0, -1.0 } };
    sx_vec3 sun_dir = sx_vec3_mul_quat(forward, sun_rotation);
    sky->atmosphere.sun_direction = (sx_vec4){ { sun_dir.x, sun_dir.y, sun_dir.z, 0.f } };
    sun_dir = sx_vec3_norm( (sx_vec3){ { 0.f, 0.5f, -1.f } } );
    sky->atmosphere.sun_direction = (sx_vec4){ { sun_dir.x, sun_dir.y, sun_dir.z, 0.f } };

    uint8_t empty_data[256 * 64 * 4] = { 0 };
    uint8_t empty_data2[32 * 32 * 4] = { 0 };

    result = create_buffer(&sky->atmospher_ubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(Atmosphere));
    VK_CHECK_RESULT(result);

    result = create_texture_from_data(&sky->transmittance_tex, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, alloc, empty_data, 256, 64);
    VK_CHECK_RESULT(result);

    result = create_texture_from_data(&sky->multi_scat_tex, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, alloc, empty_data2, 32, 32);
    VK_CHECK_RESULT(result);

    /* Descriptor Set Creation */
    {
        VkDescriptorPoolSize pool_sizes[2];
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = 1;
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = 1;

        DescriptorPoolInfo pool_info = {};
        pool_info.pool_sizes = pool_sizes;
        pool_info.pool_size_count = 2;
        pool_info.max_sets = 1;

        result = create_descriptor_pool(&pool_info, &sky->descriptor_pool);
        VK_CHECK_RESULT(result);

        VkDescriptorSetLayoutBinding layout_bindings[3];
        layout_bindings[0].binding = 0;
        layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layout_bindings[0].descriptorCount = 1;
        layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        layout_bindings[0].pImmutableSamplers = NULL;

        layout_bindings[1].binding = 1;
        layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layout_bindings[1].descriptorCount = 1;
        layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        layout_bindings[1].pImmutableSamplers = NULL;

        layout_bindings[2].binding = 2;
        layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layout_bindings[2].descriptorCount = 1;
        layout_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        layout_bindings[2].pImmutableSamplers = NULL;

        DescriptorLayoutInfo layout_info;
        layout_info.bindings = layout_bindings;
        layout_info.num_bindings = 3;
        result = create_descriptor_layout(&layout_info, &sky->descriptor_layout);
        VK_CHECK_RESULT(result);

        result = create_descriptor_sets(sky->descriptor_pool, &sky->descriptor_layout, 1, &sky->descriptor_set);
        VK_CHECK_RESULT(result);

        VkDescriptorBufferInfo buffer_info = {};
        buffer_info.buffer = sky->atmospher_ubo.buffer;
        buffer_info.offset = 0;
        buffer_info.range = sky->atmospher_ubo.size;
        uint32_t buffer_binding = 0;
        uint32_t buffer_descriptor_count = 1;
        VkDescriptorType buffer_descriptor_types = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        VkDescriptorImageInfo image_info[2];
        image_info[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_info[0].imageView = sky->transmittance_tex.image_buffer.image_view;
        image_info[0].sampler = sky->transmittance_tex.sampler;
        image_info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_info[1].imageView = sky->multi_scat_tex.image_buffer.image_view;
        image_info[1].sampler = sky->multi_scat_tex.sampler;
        uint32_t image_bindings[2] = {1, 2};
        uint32_t image_descriptor_count[2] = {1, 1};
        VkDescriptorType image_descriptor_types[2] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};

        DescriptorSetUpdateInfo update_info = {0};
        update_info.descriptor_set = sky->descriptor_set;

        update_info.buffer_infos = &buffer_info;
        update_info.buffer_bindings = &buffer_binding;
        update_info.buffer_descriptor_types = &buffer_descriptor_types;
        update_info.num_buffer_bindings = 1;
        update_info.buffer_descriptor_count = &buffer_descriptor_count;

        update_info.images_infos = image_info;
        update_info.image_descriptor_types = image_descriptor_types;
        update_info.image_bindings = image_bindings;
        update_info.num_image_bindings = 2;
        update_info.image_descriptor_count = image_descriptor_count;

        update_descriptor_set(&update_info);
    }

    /* Pipeline creation */
    {
        VertexInputStateInfo vertex_input_state_info = { 0 };

        InputAssemblyStateInfo input_assembly_state_info;
        input_assembly_state_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state_info.restart_enabled = VK_FALSE;

        DepthStencilStateInfo depth_stencil_state_info;
        depth_stencil_state_info.depth_test_enable = VK_FALSE;
        depth_stencil_state_info.depth_write_enable = VK_FALSE;
        depth_stencil_state_info.depht_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
        
        bool blend_enables[1] = {true};
        ColorBlendStateInfo color_blend_state_info;
        color_blend_state_info.blend_enables = blend_enables;
        color_blend_state_info.attachment_count = 1;

        RasterizationStateInfo rasterization_state_info;
        rasterization_state_info.polygon_mode = VK_POLYGON_MODE_FILL;
        rasterization_state_info.cull_mode = VK_CULL_MODE_NONE;
        rasterization_state_info.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        MultisampleStateInfo multisample_state_info;
        multisample_state_info.samples = VK_SAMPLE_COUNT_1_BIT;
        multisample_state_info.shadingenable = VK_FALSE;
        multisample_state_info.min_shading = 1.0;

        VkDescriptorSetLayout descriptor_set_layouts[2] = {
            rd->global_descriptor_layout, 
            sky->descriptor_layout,
        };

        PipelineLayoutInfo pipeline_layout_info = {0};
        pipeline_layout_info.descriptor_layouts = descriptor_set_layouts;
        pipeline_layout_info.descriptor_layout_count = 2;

        result = create_pipeline_layout(&pipeline_layout_info, &sky->pipeline_layout);
        VK_CHECK_RESULT(result);



        VkPipelineShaderStageCreateInfo shader_stages[2];
        shader_stages[0] = create_shader_module("shaders/render_sky.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shader_stages[1] = create_shader_module("shaders/render_sky.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

        GraphicPipelineInfo graphic_pipeline_info;
        graphic_pipeline_info.vertex_input = &vertex_input_state_info;
        graphic_pipeline_info.input_assembly = &input_assembly_state_info;
        graphic_pipeline_info.rasterization = &rasterization_state_info;
        graphic_pipeline_info.multisample = &multisample_state_info;
        graphic_pipeline_info.depth_stencil = &depth_stencil_state_info;
        graphic_pipeline_info.color_blend = &color_blend_state_info;
        graphic_pipeline_info.render_pass = rd->render_pass;
        graphic_pipeline_info.subpass = 2;
        graphic_pipeline_info.layout = &sky->pipeline_layout;
        graphic_pipeline_info.shader_stages = shader_stages;
        graphic_pipeline_info.shader_stages_count = 2;

        result = create_graphic_pipeline(&graphic_pipeline_info, &sky->pipeline);
        VK_CHECK_RESULT(result);
    }
    VkImageSubresourceRange image_subresource_range;
    image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_subresource_range.baseMipLevel = 0;
    image_subresource_range.levelCount = 1;
    image_subresource_range.baseArrayLayer = 0;
    image_subresource_range.layerCount = 1;

    /* transmittance compute */
    {
        {
            VkDescriptorPoolSize pool_sizes[2];
            pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            pool_sizes[0].descriptorCount = 1;
            pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            pool_sizes[1].descriptorCount = 1;

            DescriptorPoolInfo pool_info = {};
            pool_info.pool_sizes = pool_sizes;
            pool_info.pool_size_count = 2;
            pool_info.max_sets = 1;

            result = create_descriptor_pool(&pool_info, &sky->transmittance_descriptor_pool);
            VK_CHECK_RESULT(result);

            VkDescriptorSetLayoutBinding layout_bindings[2];
            layout_bindings[0].binding = 0;
            layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            layout_bindings[0].descriptorCount = 1;
            layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
            layout_bindings[0].pImmutableSamplers = NULL;

            layout_bindings[1].binding = 1;
            layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            layout_bindings[1].descriptorCount = 1;
            layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
            layout_bindings[1].pImmutableSamplers = NULL;

            DescriptorLayoutInfo layout_info;
            layout_info.bindings = layout_bindings;
            layout_info.num_bindings = 2;
            result = create_descriptor_layout(&layout_info, &sky->transmittance_descriptor_layout);
            VK_CHECK_RESULT(result);

            result = create_descriptor_sets(sky->transmittance_descriptor_pool, &sky->transmittance_descriptor_layout, 1, &sky->transmittance_descriptor_set);
            VK_CHECK_RESULT(result);

            VkDescriptorBufferInfo buffer_info = {};
            buffer_info.buffer = sky->atmospher_ubo.buffer;
            buffer_info.offset = 0;
            buffer_info.range = sky->atmospher_ubo.size;
            uint32_t buffer_binding = 0;
            uint32_t buffer_descriptor_count = 1;
            VkDescriptorType buffer_descriptor_types = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            VkDescriptorImageInfo image_info[1];
            image_info[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            image_info[0].imageView = sky->transmittance_tex.image_buffer.image_view;
            image_info[0].sampler = sky->transmittance_tex.sampler;
            uint32_t image_bindings[1] = {1};
            uint32_t image_descriptor_count[1] = {1};
            VkDescriptorType image_descriptor_types[1] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};

            DescriptorSetUpdateInfo update_info = {0};
            update_info.descriptor_set = sky->transmittance_descriptor_set;

            update_info.buffer_infos = &buffer_info;
            update_info.buffer_bindings = &buffer_binding;
            update_info.buffer_descriptor_types = &buffer_descriptor_types;
            update_info.num_buffer_bindings = 1;
            update_info.buffer_descriptor_count = &buffer_descriptor_count;

            update_info.images_infos = image_info;
            update_info.image_descriptor_types = image_descriptor_types;
            update_info.image_bindings = image_bindings;
            update_info.num_image_bindings = 1;
            update_info.image_descriptor_count = image_descriptor_count;

            update_descriptor_set(&update_info);
        }
        VkPipelineShaderStageCreateInfo shader_stage_info = create_shader_module("shaders/compute_transmittance.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

        PipelineLayoutInfo pipeline_layout_info;
        pipeline_layout_info.descriptor_layout_count = 1;
        pipeline_layout_info.descriptor_layouts = &sky->transmittance_descriptor_layout;
        pipeline_layout_info.push_constant_count = 0;
        pipeline_layout_info.push_constant_ranges = NULL;

        result = create_pipeline_layout(&pipeline_layout_info, &sky->transmittance_pipeline_layout);
        sx_assert_rel(result == VK_SUCCESS && "Could not create pipeline layout");

        ComputePipelineInfo pipeline_info;
        pipeline_info.num_pipelines = 1;
        pipeline_info.layouts = &sky->transmittance_pipeline_layout;
        pipeline_info.shader = &shader_stage_info;

        result = create_compute_pipeline(&pipeline_info, &sky->transmittance_pipeline);
        sx_assert_rel(result == VK_SUCCESS && "Could not create compute pipeline");
        VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 
                                                .flags = 0, .pInheritanceInfo = NULL};
        result = vkBeginCommandBuffer(rd->compute_cmdbuffer, &begin_info);
        sx_assert_rel(result == VK_SUCCESS && "Could not begin command buffer");
        {
            VkImageMemoryBarrier image_memory_barrier = {};
            image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_memory_barrier.pNext = NULL;
            image_memory_barrier.srcAccessMask = 0;
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            image_memory_barrier.image = sky->transmittance_tex.image_buffer.image;
            image_memory_barrier.subresourceRange = image_subresource_range;
            vkCmdPipelineBarrier( rd->compute_cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
                    &image_memory_barrier);
        }

        vkCmdBindPipeline(rd->compute_cmdbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, sky->transmittance_pipeline);
        vkCmdBindDescriptorSets(rd->compute_cmdbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, sky->transmittance_pipeline_layout, 0, 1, 
                                &sky->transmittance_descriptor_set, 0, NULL);

        vkCmdDispatch(rd->compute_cmdbuffer, (uint32_t)sx_ceil(256 / (float)8),
                                            (uint32_t)sx_ceil(64 / (float)8), 1);

        result = vkEndCommandBuffer(rd->compute_cmdbuffer);
        sx_assert_rel(result == VK_SUCCESS && "Could not finish command buffer record");

        VkFence fence;
        result = create_fence(&fence, false);
        sx_assert_rel(result == VK_SUCCESS && "Could not create fence");

        CommandSubmitInfo submit_info = {0};
        submit_info.command_buffers_count = 1;
        submit_info.command_buffers = &rd->compute_cmdbuffer;
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

    /* multi scattering compute */
    {
        {
            VkDescriptorPoolSize pool_sizes[3];
            pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            pool_sizes[0].descriptorCount = 1;
            pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            pool_sizes[1].descriptorCount = 1;
            pool_sizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pool_sizes[2].descriptorCount = 1;

            DescriptorPoolInfo pool_info = {};
            pool_info.pool_sizes = pool_sizes;
            pool_info.pool_size_count = 3;
            pool_info.max_sets = 1;

            result = create_descriptor_pool(&pool_info, &sky->multi_scat_descriptor_pool);
            VK_CHECK_RESULT(result);

            VkDescriptorSetLayoutBinding layout_bindings[3];
            layout_bindings[0].binding = 0;
            layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            layout_bindings[0].descriptorCount = 1;
            layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
            layout_bindings[0].pImmutableSamplers = NULL;

            layout_bindings[1].binding = 1;
            layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            layout_bindings[1].descriptorCount = 1;
            layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
            layout_bindings[1].pImmutableSamplers = NULL;

            layout_bindings[2].binding = 2;
            layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layout_bindings[2].descriptorCount = 1;
            layout_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
            layout_bindings[2].pImmutableSamplers = NULL;

            DescriptorLayoutInfo layout_info;
            layout_info.bindings = layout_bindings;
            layout_info.num_bindings = 3;
            result = create_descriptor_layout(&layout_info, &sky->multi_scat_descriptor_layout);
            VK_CHECK_RESULT(result);

            result = create_descriptor_sets(sky->multi_scat_descriptor_pool, &sky->multi_scat_descriptor_layout, 1, &sky->multi_scat_descriptor_set);
            VK_CHECK_RESULT(result);

            VkDescriptorBufferInfo buffer_info = {};
            buffer_info.buffer = sky->atmospher_ubo.buffer;
            buffer_info.offset = 0;
            buffer_info.range = sky->atmospher_ubo.size;
            uint32_t buffer_binding = 0;
            uint32_t buffer_descriptor_count = 1;
            VkDescriptorType buffer_descriptor_types = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            VkDescriptorImageInfo image_info[2];
            image_info[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            image_info[0].imageView = sky->multi_scat_tex.image_buffer.image_view;
            image_info[0].sampler = sky->multi_scat_tex.sampler;
            image_info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            image_info[1].imageView = sky->transmittance_tex.image_buffer.image_view;
            image_info[1].sampler = sky->transmittance_tex.sampler;
            uint32_t image_bindings[2] = {1, 2};
            uint32_t image_descriptor_count[2] = {1, 1};
            VkDescriptorType image_descriptor_types[2] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};

            DescriptorSetUpdateInfo update_info = {0};
            update_info.descriptor_set = sky->multi_scat_descriptor_set;

            update_info.buffer_infos = &buffer_info;
            update_info.buffer_bindings = &buffer_binding;
            update_info.buffer_descriptor_types = &buffer_descriptor_types;
            update_info.num_buffer_bindings = 1;
            update_info.buffer_descriptor_count = &buffer_descriptor_count;

            update_info.images_infos = image_info;
            update_info.image_descriptor_types = image_descriptor_types;
            update_info.image_bindings = image_bindings;
            update_info.num_image_bindings = 2;
            update_info.image_descriptor_count = image_descriptor_count;

            update_descriptor_set(&update_info);
        }
        VkPipelineShaderStageCreateInfo shader_stage_info = create_shader_module("shaders/compute_multi_scattering.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

        PipelineLayoutInfo pipeline_layout_info;
        pipeline_layout_info.descriptor_layout_count = 1;
        pipeline_layout_info.descriptor_layouts = &sky->multi_scat_descriptor_layout;
        pipeline_layout_info.push_constant_count = 0;
        pipeline_layout_info.push_constant_ranges = NULL;

        result = create_pipeline_layout(&pipeline_layout_info, &sky->multi_scat_pipeline_layout);
        sx_assert_rel(result == VK_SUCCESS && "Could not create pipeline layout");

        ComputePipelineInfo pipeline_info;
        pipeline_info.num_pipelines = 1;
        pipeline_info.layouts = &sky->multi_scat_pipeline_layout;
        pipeline_info.shader = &shader_stage_info;

        result = create_compute_pipeline(&pipeline_info, &sky->multi_scat_pipeline);
        sx_assert_rel(result == VK_SUCCESS && "Could not create compute pipeline");
        VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 
                                                .flags = 0, .pInheritanceInfo = NULL};
        result = vkBeginCommandBuffer(rd->compute_cmdbuffer, &begin_info);
        sx_assert_rel(result == VK_SUCCESS && "Could not begin command buffer");
        {
            VkImageMemoryBarrier image_memory_barrier = {};
            image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_memory_barrier.pNext = NULL;
            image_memory_barrier.srcAccessMask = 0;
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            image_memory_barrier.image = sky->multi_scat_tex.image_buffer.image;
            image_memory_barrier.subresourceRange = image_subresource_range;
            vkCmdPipelineBarrier( rd->compute_cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
                    &image_memory_barrier);
        }

        vkCmdBindPipeline(rd->compute_cmdbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, sky->multi_scat_pipeline);
        vkCmdBindDescriptorSets(rd->compute_cmdbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, sky->multi_scat_pipeline_layout, 0, 1, 
                                &sky->multi_scat_descriptor_set, 0, NULL);

        vkCmdDispatch(rd->compute_cmdbuffer, 32, 32, 1);

        result = vkEndCommandBuffer(rd->compute_cmdbuffer);
        sx_assert_rel(result == VK_SUCCESS && "Could not finish command buffer record");

        VkFence fence;
        result = create_fence(&fence, false);
        sx_assert_rel(result == VK_SUCCESS && "Could not create fence");

        CommandSubmitInfo submit_info = {0};
        submit_info.command_buffers_count = 1;
        submit_info.command_buffers = &rd->compute_cmdbuffer;
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
    result = create_fence(&sky->fence, false);
    sx_assert_rel(result == VK_SUCCESS && "Could not create fence");
    renderer_register_callback(sky->rd, sky_draw, 2);

    return sky;

}

void update_atmosphere_buffer(Sky* sky) {
    copy_buffer(&sky->atmospher_ubo, &sky->atmosphere, sizeof(Atmosphere));
}

void sky_draw(VkCommandBuffer cmdbuffer) {
    update_atmosphere_buffer(global_sky);
    Renderer *rd = global_sky->rd;
    Sky *sky = global_sky;
    VkResult result;
    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 
                                            .flags = 0, .pInheritanceInfo = NULL};
    result = vkBeginCommandBuffer(rd->compute_cmdbuffer, &begin_info);
    sx_assert_rel(result == VK_SUCCESS && "Could not begin command buffer");

    vkCmdBindPipeline(rd->compute_cmdbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, sky->transmittance_pipeline);
    vkCmdBindDescriptorSets(rd->compute_cmdbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, sky->transmittance_pipeline_layout, 0, 1, 
                            &sky->transmittance_descriptor_set, 0, NULL);

    vkCmdDispatch(rd->compute_cmdbuffer, (uint32_t)sx_ceil(256 / (float)8),
                                        (uint32_t)sx_ceil(64 / (float)8), 1);

    result = vkEndCommandBuffer(rd->compute_cmdbuffer);
    sx_assert_rel(result == VK_SUCCESS && "Could not finish command buffer record");

    reset_fences(1, &sky->fence);

    CommandSubmitInfo submit_info = {0};
    submit_info.command_buffers_count = 1;
    submit_info.command_buffers = &rd->compute_cmdbuffer;
    submit_info.signal_semaphores_count = 0;
    submit_info.signal_semaphores = NULL;
    submit_info.wait_semaphores_count = 0;
    submit_info.wait_semaphores = NULL;
    submit_info.wait_dst_stage_mask = 0;
    submit_info.fence = sky->fence;
    submit_info.queue = COMPUTE;
    result = submit_commands(&submit_info);
    sx_assert_rel(result == VK_SUCCESS && "Failed to submit commands");

    result = wait_fences(1, &sky->fence, true, 100000000000);
    sx_assert_rel(result == VK_SUCCESS && "Failed to wait fence");
    reset_fences(1, &sky->fence);

    result = vkBeginCommandBuffer(rd->compute_cmdbuffer, &begin_info);
    sx_assert_rel(result == VK_SUCCESS && "Could not begin command buffer");

    vkCmdBindPipeline(rd->compute_cmdbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, sky->multi_scat_pipeline);
    vkCmdBindDescriptorSets(rd->compute_cmdbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, sky->multi_scat_pipeline_layout, 0, 1, 
                            &sky->multi_scat_descriptor_set, 0, NULL);

    vkCmdDispatch(rd->compute_cmdbuffer, 32, 32, 1);

    result = vkEndCommandBuffer(rd->compute_cmdbuffer);
    sx_assert_rel(result == VK_SUCCESS && "Could not finish command buffer record");

    submit_info.command_buffers_count = 1;
    submit_info.command_buffers = &rd->compute_cmdbuffer;
    submit_info.signal_semaphores_count = 0;
    submit_info.signal_semaphores = NULL;
    submit_info.wait_semaphores_count = 0;
    submit_info.wait_semaphores = NULL;
    submit_info.wait_dst_stage_mask = 0;
    submit_info.fence = sky->fence;
    submit_info.queue = COMPUTE;
    result = submit_commands(&submit_info);
    sx_assert_rel(result == VK_SUCCESS && "Failed to submit commands");

    result = wait_fences(1, &sky->fence, true, 100000000000);
    sx_assert_rel(result == VK_SUCCESS && "Failed to wait fence");

    vkCmdBindDescriptorSets(cmdbuffer, 
            VK_PIPELINE_BIND_POINT_GRAPHICS, global_sky->pipeline_layout,
            0, 1, &global_sky->rd->global_descriptorset, 0, NULL);
    vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, global_sky->pipeline_layout,
            1, 1, &global_sky->descriptor_set, 0, NULL);
    vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, global_sky->pipeline);
    vkCmdDraw(cmdbuffer, 4, 1, 0, 0);
}
