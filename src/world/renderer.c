#include "world/renderer.h"
#include "sx/math.h"
#include "vulkan/vulkan_core.h"
#include "world/camera.h"
#include "device/device.h"

void update_composition_descriptors(Renderer* rd);
VkResult renderer_frame(Renderer* rd, uint32_t resource_index, uint32_t image_index);
VkResult create_attachments(Renderer* rd);

/*{{{Renderer* create_renderer(const sx_alloc* alloc, uint32_t width, uint32_t height)*/
Renderer* create_renderer(const sx_alloc* alloc, uint32_t width, uint32_t height) {
    VkResult result;

    Renderer* rd = sx_malloc(alloc, sizeof(*rd));
    rd->width = width;
    rd->height = height;
    rd->exposure = 0.8f;
    rd->subpass_callbacks_count[0] = 0;
    rd->subpass_callbacks_count[1] = 0;
    rd->subpass_callbacks_count[2] = 0;

    rd->position_image.image = VK_NULL_HANDLE;
    rd->position_image.image_view = VK_NULL_HANDLE;
    rd->position_image.memory = VK_NULL_HANDLE;
    rd->albedo_image.image = VK_NULL_HANDLE;
    rd->albedo_image.image_view = VK_NULL_HANDLE;
    rd->albedo_image.memory = VK_NULL_HANDLE;
    rd->metallic_roughness_image.image = VK_NULL_HANDLE;
    rd->metallic_roughness_image.image_view = VK_NULL_HANDLE;
    rd->metallic_roughness_image.memory = VK_NULL_HANDLE;
    rd->normal_image.image = VK_NULL_HANDLE;
    rd->normal_image.image_view = VK_NULL_HANDLE;
    rd->normal_image.memory = VK_NULL_HANDLE;
    rd->depth_image.image = VK_NULL_HANDLE;
    rd->depth_image.image_view = VK_NULL_HANDLE;
    rd->depth_image.image_view = VK_NULL_HANDLE;

    rd->swapchain = create_swapchain(width, height);
    result = create_attachments(rd);
    VK_CHECK_RESULT(result);

    rd->graphic_cmdbuffer = sx_malloc(sx_alloc_malloc(), RENDERING_RESOURCES_SIZE *
            sizeof(*(rd->graphic_cmdbuffer)));
    result = create_command_buffer(GRAPHICS, VK_COMMAND_BUFFER_LEVEL_PRIMARY, RENDERING_RESOURCES_SIZE, 
            rd->graphic_cmdbuffer);
    VK_CHECK_RESULT(result);
    result = create_command_buffer(COMPUTE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, &rd->compute_cmdbuffer);
    VK_CHECK_RESULT(result);


    /* Global Buffer Creation {{{*/ 
    {
        result = create_buffer(&rd->global_uniform_buffer,
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(GlobalUBO));
        sx_assert_rel(result == VK_SUCCESS && "Could not create global uniform buffer!");

        create_texture(&rd->lut_brdf, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, alloc, "misc/empty.ktx");
        create_texture(&rd->irradiance_cube, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, alloc, "misc/empty.ktx");
        create_texture(&rd->prefiltered_cube, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, alloc, "misc/empty.ktx");
    }
    /*}}}*/


    /* Render pass creation {{{ */
    {
        VkAttachmentDescription attachment_descriptions[6];
        /* Color attchment */
		attachment_descriptions[0].flags = 0;
        attachment_descriptions[0].format = rd->swapchain.format.format;
		attachment_descriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        /* Deferred attachments */
        /* position */
		attachment_descriptions[1].flags = 0;
        attachment_descriptions[1].format = rd->position_image.format;
		attachment_descriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        /* normal */
		attachment_descriptions[2].flags = 0;
        attachment_descriptions[2].format = rd->normal_image.format;
		attachment_descriptions[2].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        /* albedo */
		attachment_descriptions[3].flags = 0;
        attachment_descriptions[3].format = rd->albedo_image.format;
		attachment_descriptions[3].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[3].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        /* metallic roughness */
		attachment_descriptions[4].flags = 0;
        attachment_descriptions[4].format = rd->metallic_roughness_image.format;
		attachment_descriptions[4].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[4].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[4].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        /* Depth attachment */
		attachment_descriptions[5].flags = 0;
		attachment_descriptions[5].format = rd->depth_image.format;
		attachment_descriptions[5].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[5].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[5].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descriptions[5].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[5].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[5].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[5].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;;


        /* Three subpasses */
        VkSubpassDescription subpass_descriptions[3];

        /* First subpass: Fill G-Buffer components */
        VkAttachmentReference color_references[5];
        color_references[0].attachment = 0;
        color_references[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_references[1].attachment = 1;
        color_references[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_references[2].attachment = 2;
        color_references[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_references[3].attachment = 3;
        color_references[3].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_references[4].attachment = 4;
        color_references[4].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_reference[1];
        depth_reference[0].attachment = 5;
        depth_reference[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        subpass_descriptions[0].flags = 0;
        subpass_descriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_descriptions[0].inputAttachmentCount = 0;
        subpass_descriptions[0].pInputAttachments = NULL;
        subpass_descriptions[0].colorAttachmentCount = 5;
        subpass_descriptions[0].pColorAttachments = color_references;
        subpass_descriptions[0].pDepthStencilAttachment = depth_reference;
        subpass_descriptions[0].pResolveAttachments = NULL;
        subpass_descriptions[0].preserveAttachmentCount = 0;
        subpass_descriptions[0].pPreserveAttachments = NULL;

        /* Second subpass: Final composition (using G-Buffer components */
        VkAttachmentReference color_reference[1];
        color_reference[0].attachment = 0;
        color_reference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference input_references[4];
        input_references[0].attachment = 1;
        input_references[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        input_references[1].attachment = 2;
        input_references[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        input_references[2].attachment = 3;
        input_references[2].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        input_references[3].attachment = 4;
        input_references[3].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        uint32_t preserve_attachment_index = 1;
        subpass_descriptions[1].flags = 0;
        subpass_descriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_descriptions[1].inputAttachmentCount = 4;
        subpass_descriptions[1].pInputAttachments = input_references;
        subpass_descriptions[1].colorAttachmentCount = 1;
        subpass_descriptions[1].pColorAttachments = color_reference;
        subpass_descriptions[1].pDepthStencilAttachment = depth_reference;
        subpass_descriptions[1].pResolveAttachments = NULL;
        subpass_descriptions[1].preserveAttachmentCount = 0;
        subpass_descriptions[1].pPreserveAttachments = NULL;
        
        /* Third subpass: Forward transparency */
        color_reference[0].attachment = 0;
        color_reference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        input_references[0].attachment = 1;
        input_references[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        subpass_descriptions[2].flags = 0;
        subpass_descriptions[2].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_descriptions[2].inputAttachmentCount = 1;
        subpass_descriptions[2].pInputAttachments = input_references;
        subpass_descriptions[2].colorAttachmentCount = 1;
        subpass_descriptions[2].pColorAttachments = color_reference;
        subpass_descriptions[2].pDepthStencilAttachment = depth_reference;
        subpass_descriptions[2].pResolveAttachments = NULL;
        subpass_descriptions[2].preserveAttachmentCount = 0;
        subpass_descriptions[2].pPreserveAttachments = NULL;

        /* Subpass dependencies for layout transitions */
        VkSubpassDependency dependencies[4];
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT 
                                        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = 1;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[2].srcSubpass = 1;
        dependencies[2].dstSubpass = 2;
        dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[3].srcSubpass = 0;
        dependencies[3].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[3].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT 
                                        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[3].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[3].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo render_pass_create_info;
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.pNext = NULL;
        render_pass_create_info.flags = 0;
        render_pass_create_info.attachmentCount = 6;
        render_pass_create_info.pAttachments = attachment_descriptions;
        render_pass_create_info.subpassCount = 3;
        render_pass_create_info.pSubpasses = subpass_descriptions;
        render_pass_create_info.dependencyCount = 4;
        render_pass_create_info.pDependencies = dependencies;

        RenderPassInfo render_pass_info;
        render_pass_info.attachment_descriptions = attachment_descriptions;
        render_pass_info.attachment_count = 6;
        render_pass_info.subpass_description = subpass_descriptions;
        render_pass_info.subpass_count = 3;
        render_pass_info.supass_dependencies = dependencies;
        render_pass_info.supass_dependencies_count = 4;

        result = create_renderpass(&render_pass_info, &rd->render_pass);
        sx_assert_rel(result == VK_SUCCESS && "Could not create render pass");
    }
    /* }}} */

    /* Descriptor Sets Creation {{{*/
    {
        /* Descriptor Pools creation {{{*/
        /* Global Pool */
        {
            VkDescriptorPoolSize pool_sizes[2];
            pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            pool_sizes[0].descriptorCount = 1;
            pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pool_sizes[1].descriptorCount = 3;

            DescriptorPoolInfo pool_info;
            pool_info.pool_sizes = pool_sizes;
            pool_info.pool_size_count = 2;
            pool_info.max_sets = 1;

            result = create_descriptor_pool(&pool_info, &rd->global_descriptor_pool);
            VK_CHECK_RESULT(result);
        }
        /* Composition pool */
        {
            VkDescriptorPoolSize pool_sizes[1];
            pool_sizes[0].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            pool_sizes[0].descriptorCount = 4;

            DescriptorPoolInfo pool_info;
            pool_info.pool_sizes = pool_sizes;
            pool_info.pool_size_count = 2;
            pool_info.max_sets = 1;

            result = create_descriptor_pool(&pool_info, &rd->composition_descriptor_pool);
            VK_CHECK_RESULT(result);
        }
        /*}}}*/

        /*{{{ Descriptor Set Layout Creation */
        /* Global Layout */
        {
            VkDescriptorSetLayoutBinding layout_bindings[4];
            layout_bindings[0].binding = 0;
            layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            layout_bindings[0].descriptorCount = 1;
            layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT 
                                            | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[0].pImmutableSamplers = NULL;
            layout_bindings[1].binding = 1;
            layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layout_bindings[1].descriptorCount = 1;
            layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[1].pImmutableSamplers = NULL;
            layout_bindings[2].binding = 2;
            layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layout_bindings[2].descriptorCount = 1;
            layout_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[2].pImmutableSamplers = NULL;
            layout_bindings[3].binding = 3;
            layout_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layout_bindings[3].descriptorCount = 1;
            layout_bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[3].pImmutableSamplers = NULL;

            DescriptorLayoutInfo layout_info;
            layout_info.bindings = layout_bindings;
            layout_info.num_bindings = 4;
            result = create_descriptor_layout(&layout_info, &rd->global_descriptor_layout);
            VK_CHECK_RESULT(result);
        }
        /* Composition Layout */
        {
            VkDescriptorSetLayoutBinding layout_bindings[4];
            layout_bindings[0].binding = 0;
            layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            layout_bindings[0].descriptorCount = 1;
            layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[0].pImmutableSamplers = NULL;
            layout_bindings[1].binding = 1;
            layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            layout_bindings[1].descriptorCount = 1;
            layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[1].pImmutableSamplers = NULL;
            layout_bindings[2].binding = 2;
            layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            layout_bindings[2].descriptorCount = 1;
            layout_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[2].pImmutableSamplers = NULL;
            layout_bindings[3].binding = 3;
            layout_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            layout_bindings[3].descriptorCount = 1;
            layout_bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[3].pImmutableSamplers = NULL;

            DescriptorLayoutInfo layout_info;
            layout_info.bindings = layout_bindings;
            layout_info.num_bindings = 4;
            result = create_descriptor_layout(&layout_info, &rd->composition_descriptorset_layout);
            VK_CHECK_RESULT(result);
        }
        /*}}}*/

        /*{{{ Descriptor set creation */
        result = create_descriptor_sets(rd->global_descriptor_pool, &rd->global_descriptor_layout, 
                1, &rd->global_descriptorset);
        VK_CHECK_RESULT(result);

        result = create_descriptor_sets(rd->composition_descriptor_pool, &rd->composition_descriptorset_layout, 
                1, &rd->composition_descriptorset);
        VK_CHECK_RESULT(result);
        /*}}}*/
    }
    /*}}}*/

    /* Update Descriptor Sets {{{*/
    /*Global Descriptor update*/
    {
        VkDescriptorBufferInfo buffer_info;
        buffer_info.buffer = rd->global_uniform_buffer.buffer;
        buffer_info.offset = 0;
        buffer_info.range = rd->global_uniform_buffer.size;
        uint32_t buffer_binding = 0;
        uint32_t buffer_descriptor_count = 1;
        VkDescriptorType buffer_descriptor_types = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        VkDescriptorImageInfo image_info[3];
        image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[0].imageView = rd->lut_brdf.image_buffer.image_view;
        image_info[0].sampler = rd->lut_brdf.sampler;
        image_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[1].imageView = rd->irradiance_cube.image_buffer.image_view;
        image_info[1].sampler = rd->irradiance_cube.sampler;
        image_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[2].imageView = rd->prefiltered_cube.image_buffer.image_view;
        image_info[2].sampler = rd->prefiltered_cube.sampler;
        uint32_t image_bindings[3] = {1, 2, 3};
        uint32_t image_descriptor_count[3] = {1, 1, 1};
        VkDescriptorType image_descriptor_types[3] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
                                                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
                                                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};

        DescriptorSetUpdateInfo update_info = {0};
        update_info.descriptor_set = rd->global_descriptorset;

        update_info.buffer_infos = &buffer_info;
        update_info.buffer_bindings = &buffer_binding;
        update_info.buffer_descriptor_types = &buffer_descriptor_types;
        update_info.num_buffer_bindings = 1;
        update_info.buffer_descriptor_count = &buffer_descriptor_count;

        update_info.images_infos = image_info;
        update_info.image_descriptor_types = image_descriptor_types;
        update_info.image_bindings = image_bindings;
        update_info.num_image_bindings = 3;
        update_info.image_descriptor_count = image_descriptor_count;
        update_descriptor_set(&update_info);

    }
    update_composition_descriptors(rd);
    /*}}}*/

    /* Pipelines creation {{{*/
    {
        /* Common pipeline state {{{*/ 
		VkVertexInputBindingDescription vertex_binding_descriptions[1] = {
            {
                .binding = 0,
                .stride  = 8 * sizeof(float),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
        };
        //Position, Normal, uv
		VkVertexInputAttributeDescription vertex_attribute_descriptions[3] = {
            {
                .location = 0,
                .binding = vertex_binding_descriptions[0].binding,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0
            },
            {
                .location = 1,
                .binding = vertex_binding_descriptions[0].binding,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 3*sizeof(float)
            },
            {
                .location = 2,
                .binding = vertex_binding_descriptions[0].binding,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = 6*sizeof(float)
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
        rasterization_state_info.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        MultisampleStateInfo multisample_state_info;
        multisample_state_info.samples = VK_SAMPLE_COUNT_1_BIT;
        multisample_state_info.shadingenable = VK_FALSE;
        multisample_state_info.min_shading = 1.0;

        DepthStencilStateInfo depth_stencil_state_info;
        depth_stencil_state_info.depth_test_enable = VK_TRUE;
        depth_stencil_state_info.depth_write_enable = VK_TRUE;
        depth_stencil_state_info.depht_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;

        bool blend_enables[5] = {false, false, false, false, false};
        ColorBlendStateInfo color_blend_state_info;
        color_blend_state_info.blend_enables = blend_enables;
        color_blend_state_info.attachment_count = 5;


        /*}}}*/
        /* Composition pipeline {{{*/
        {
            VertexInputStateInfo vertex_input_state_info = {0};

            DepthStencilStateInfo depth_stencil_state_info;
            depth_stencil_state_info.depth_test_enable = VK_FALSE;
            depth_stencil_state_info.depth_write_enable = VK_FALSE;
            depth_stencil_state_info.depht_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
            
            ColorBlendStateInfo color_blend_state_info;
            color_blend_state_info.blend_enables = blend_enables;
            color_blend_state_info.attachment_count = 1;

            RasterizationStateInfo rasterization_state_info;
            rasterization_state_info.polygon_mode = VK_POLYGON_MODE_FILL;
            rasterization_state_info.cull_mode = VK_CULL_MODE_NONE;
            rasterization_state_info.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;

            VkDescriptorSetLayout descriptor_set_layouts[2] = {
                rd->global_descriptor_layout, 
                rd->composition_descriptorset_layout
            };

            PipelineLayoutInfo pipeline_layout_info = {0};
            pipeline_layout_info.descriptor_layouts = descriptor_set_layouts;
            pipeline_layout_info.descriptor_layout_count = 2;

            result = create_pipeline_layout(&pipeline_layout_info, &rd->composition_pipeline_layout);
            VK_CHECK_RESULT(result);



            VkPipelineShaderStageCreateInfo shader_stages[2];
            shader_stages[0] = create_shader_module("shaders/composition.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
            shader_stages[1] = create_shader_module("shaders/composition.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

            GraphicPipelineInfo graphic_pipeline_info;
            graphic_pipeline_info.vertex_input = &vertex_input_state_info;
            graphic_pipeline_info.input_assembly = &input_assembly_state_info;
            graphic_pipeline_info.rasterization = &rasterization_state_info;
            graphic_pipeline_info.multisample = &multisample_state_info;
            graphic_pipeline_info.depth_stencil = &depth_stencil_state_info;
            graphic_pipeline_info.color_blend = &color_blend_state_info;
            graphic_pipeline_info.render_pass = rd->render_pass;
            graphic_pipeline_info.subpass = 1;
            graphic_pipeline_info.layout = &rd->composition_pipeline_layout;
            graphic_pipeline_info.shader_stages = shader_stages;
            graphic_pipeline_info.shader_stages_count = 2;

            result = create_graphic_pipeline(&graphic_pipeline_info, &rd->composition_pipeline);
            VK_CHECK_RESULT(result);

        }
        /*}}}*/

    }
    /*}}}*/

	/* Semaphore creation {{{*/
	{
		rd->image_available_semaphore = sx_malloc(alloc, RENDERING_RESOURCES_SIZE *
				sizeof(*(rd->image_available_semaphore)));
		rd->rendering_finished_semaphore = sx_malloc(alloc, RENDERING_RESOURCES_SIZE *
				sizeof(*(rd->rendering_finished_semaphore)));

		for(uint32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {
			result = create_semaphore(&rd->image_available_semaphore[i]);
            VK_CHECK_RESULT(result);
			result = create_semaphore(&rd->rendering_finished_semaphore[i]);
            VK_CHECK_RESULT(result);
		}
	}
    /*}}}*/

	/* fences creation {{{*/
	{
		rd->fences = sx_malloc(alloc, RENDERING_RESOURCES_SIZE *
				sizeof(*(rd->fences)));

		for(uint32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {

			result = create_fence(&rd->fences[i], true);
            VK_CHECK_RESULT(result);
		}
	}
    /*}}}*/

	rd->framebuffer = sx_malloc(alloc, RENDERING_RESOURCES_SIZE *
			sizeof(*(rd->framebuffer)));
	for (uint32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {
		rd->framebuffer[i] = VK_NULL_HANDLE;
	}
    return rd;
}
/*}}}*/

bool update_uniform_buffer(Renderer* rd) {
        GlobalUBO ubo;
        ubo.projection = perspective_mat((Camera*)&device()->camera);
        ubo.inverse_projection = sx_mat4_inv(&ubo.projection);
        ubo.view = view_mat((Camera*)&device()->camera);
        ubo.inverse_view = sx_mat4_inv(&ubo.view);
        ubo.projection_view = sx_mat4_mul(&ubo.projection, &ubo.view);
        sx_vec3 pos = device()->camera.cam.pos;
        ubo.camera_position = sx_vec4f(pos.x, pos.y, pos.z, 1.0);
        ubo.exposure_gama = sx_vec4f(rd->exposure, 0.5, 0.5, 0.5);
        ubo.light_position[0] = sx_vec4f(2000.0, 2000.0, 2000.0, 0.0);
        ubo.light_position[1] = sx_vec4f(0.0, 10000.0, 0.0, 0.0);
        ubo.light_position[2] = sx_vec4f(0.0, 10000.0, 0.0, 0.0);
        ubo.light_position[3] = sx_vec4f(0.0, 10000.0, 0.0, 0.0);
        /*ubo.view.col4 = SX_VEC4_ZERO;*/
        /*memcpy(&ubo.view, &vk_context.camera->view, 16 * sizeof(float));*/
        /*mat4_transpose(res_matrix, m);*/
        copy_buffer(&rd->global_uniform_buffer,  &ubo, sizeof(ubo));

        return true;
}

/*update_composition_descriptors(Renderer* rd){{{*/
void update_composition_descriptors(Renderer* rd) {
    VkDescriptorImageInfo image_info[4];
    image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info[0].imageView = rd->position_image.image_view;
    image_info[0].sampler = VK_NULL_HANDLE;
    image_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info[1].imageView = rd->normal_image.image_view;
    image_info[1].sampler = VK_NULL_HANDLE;
    image_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info[2].imageView = rd->albedo_image.image_view;
    image_info[2].sampler = VK_NULL_HANDLE;
    image_info[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info[3].imageView = rd->metallic_roughness_image.image_view;
    image_info[3].sampler = VK_NULL_HANDLE;
    uint32_t image_bindings[4] = {0, 1, 2, 3};
    uint32_t image_descriptor_count[4] = {1, 1, 1, 1};
    VkDescriptorType descriptor_types[4] = {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT};
    DescriptorSetUpdateInfo update_info = {0};
    update_info.descriptor_set = rd->composition_descriptorset;

    update_info.images_infos = image_info;
    update_info.image_bindings = image_bindings;
    update_info.image_descriptor_types = descriptor_types;
    update_info.image_descriptor_count = image_descriptor_count;
    update_info.num_image_bindings = 4;

    update_descriptor_set(&update_info);
}
/*}}}*/

/*{{{VkResult renderer_frame(Renderer* rd, uint32_t resource_index, uint32_t image_index)*/
VkResult renderer_frame(Renderer* rd, uint32_t resource_index, uint32_t image_index) {
    VkResult result;
    if (rd->framebuffer[resource_index] != VK_NULL_HANDLE) {
        destroy_framebuffer(rd->framebuffer[resource_index]);
    }

    VkImageView attachments[6];
    attachments[0] = rd->swapchain.image_views[image_index];
    attachments[1] = rd->position_image.image_view;
    attachments[2] = rd->normal_image.image_view;
    attachments[3] = rd->albedo_image.image_view;
    attachments[4] = rd->metallic_roughness_image.image_view;
    attachments[5] = rd->depth_image.image_view;

    FramebufferInfo framebuffer_info;
    framebuffer_info.render_pass = rd->render_pass;
    framebuffer_info.attachments = attachments;
    framebuffer_info.attachment_count = 6;
    framebuffer_info.width = rd->width;
    framebuffer_info.height = rd->height;
    framebuffer_info.layers = 1;

    result = create_framebuffer(&framebuffer_info, &rd->framebuffer[resource_index]);
    VK_CHECK_RESULT(result);

	VkCommandBufferBeginInfo command_buffer_begin_info;
	command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	command_buffer_begin_info.pNext = NULL;
	command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	command_buffer_begin_info.pInheritanceInfo = NULL;

    VkClearValue clear_value[6];
    VkClearColorValue color = { {0.0f, 0.0f, 0.0f, 0.0f} };
    VkClearDepthStencilValue depth = {1.0, 0.0};
    clear_value[0].color = color;
    clear_value[1].color = color;
    clear_value[2].color = color;
    clear_value[3].color = color;
    clear_value[4].color = color;
    clear_value[5].depthStencil = depth;

    VkRect2D render_area;
    render_area.offset.x = 0;
    render_area.offset.y = 0;
    render_area.extent.width = rd->width;
    render_area.extent.height = rd->height;

    VkRenderPassBeginInfo render_pass_begin_info;
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.pNext = NULL;
    render_pass_begin_info.renderPass = rd->render_pass;
    render_pass_begin_info.renderArea = render_area;
    render_pass_begin_info.clearValueCount = 6;
    render_pass_begin_info.pClearValues = clear_value;

    render_pass_begin_info.framebuffer = rd->framebuffer[resource_index];
    vkBeginCommandBuffer(rd->graphic_cmdbuffer[resource_index],
            &command_buffer_begin_info);
    VkImageSubresourceRange image_subresource_range;
    image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_subresource_range.baseMipLevel = 0;
    image_subresource_range.levelCount = 1;
    image_subresource_range.baseArrayLayer = 0;
    image_subresource_range.layerCount = 1;


    if (get_queue(GRAPHICS) != get_queue(PRESENT)) {
        VkImageMemoryBarrier barrier_from_present_to_draw;
        barrier_from_present_to_draw.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier_from_present_to_draw.pNext = NULL;
        barrier_from_present_to_draw.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrier_from_present_to_draw.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier_from_present_to_draw.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier_from_present_to_draw.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier_from_present_to_draw.srcQueueFamilyIndex = get_queue_index(PRESENT);
        barrier_from_present_to_draw.dstQueueFamilyIndex = get_queue_index(GRAPHICS);
        barrier_from_present_to_draw.image = rd->swapchain.images[image_index];
        barrier_from_present_to_draw.subresourceRange = image_subresource_range;
        vkCmdPipelineBarrier(rd->graphic_cmdbuffer[resource_index],
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
                &barrier_from_present_to_draw);
    }


    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = rd->width;
    viewport.height = rd->height;
    viewport.minDepth = 0.0;
    viewport.maxDepth = 1.0;

    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = rd->width;
    scissor.extent.height = rd->height;

    vkCmdBeginRenderPass(rd->graphic_cmdbuffer[resource_index], &render_pass_begin_info,
            VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(rd->graphic_cmdbuffer[resource_index], 0, 1, &viewport);
    vkCmdSetScissor(rd->graphic_cmdbuffer[resource_index], 0, 1, &scissor);
    VkDeviceSize offset = 0;

    VkDescriptorSet descriptor_sets[2];
    descriptor_sets[0] = rd->global_descriptorset;

    vkCmdBindDescriptorSets(rd->graphic_cmdbuffer[resource_index], 
            VK_PIPELINE_BIND_POINT_GRAPHICS, rd->composition_pipeline_layout,
            0, 1, &rd->global_descriptorset, 0, NULL);
    // First Sub pass
    {

        for (uint32_t i = 0; i < rd->subpass_callbacks_count[0]; i++) {
            rd->subpass_callbacks[0][i](rd->graphic_cmdbuffer[resource_index]);
        }


    }
    // Second subpass
    {
        vkCmdNextSubpass(rd->graphic_cmdbuffer[resource_index], VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindDescriptorSets(rd->graphic_cmdbuffer[resource_index], 
                VK_PIPELINE_BIND_POINT_GRAPHICS, rd->composition_pipeline_layout,
                1, 1, &rd->composition_descriptorset, 0, NULL);
        vkCmdBindPipeline(rd->graphic_cmdbuffer[resource_index], 
                VK_PIPELINE_BIND_POINT_GRAPHICS, rd->composition_pipeline);
        vkCmdDraw(rd->graphic_cmdbuffer[resource_index], 4, 1, 0, 0);
    }
    //Third subpass
    {
        vkCmdNextSubpass(rd->graphic_cmdbuffer[resource_index], VK_SUBPASS_CONTENTS_INLINE);
        for (uint32_t i = 0; i < rd->subpass_callbacks_count[2]; i++) {
            rd->subpass_callbacks[2][i](rd->graphic_cmdbuffer[resource_index]);
        }
    }

    vkCmdEndRenderPass(rd->graphic_cmdbuffer[resource_index]);

    if (get_queue(GRAPHICS) != get_queue(PRESENT)) {
        VkImageMemoryBarrier barrier_from_draw_to_present;
        barrier_from_draw_to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier_from_draw_to_present.pNext = NULL;
        barrier_from_draw_to_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier_from_draw_to_present.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrier_from_draw_to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier_from_draw_to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier_from_draw_to_present.srcQueueFamilyIndex = get_queue_index(GRAPHICS);
        barrier_from_draw_to_present.dstQueueFamilyIndex = get_queue_index(PRESENT);
        barrier_from_draw_to_present.image = rd->swapchain.images[image_index];
        barrier_from_draw_to_present.subresourceRange = image_subresource_range;
        vkCmdPipelineBarrier(rd->graphic_cmdbuffer[resource_index],
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
                &barrier_from_draw_to_present);
    }

     result = vkEndCommandBuffer(rd->graphic_cmdbuffer[resource_index]);
     sx_assert_rel(result == VK_SUCCESS && "Could not record command buffers!");
    return result;
}
/*}}}*/

/*{{{bool renderer_draw(Renderer* rd)*/
bool renderer_draw(Renderer* rd) {
    static uint32_t resource_index = 0;
	uint32_t next_resource_index = (resource_index + 1) % RENDERING_RESOURCES_SIZE;
	uint32_t image_index;
    VkResult result = wait_fences(1, &rd->fences[resource_index], VK_FALSE, 1000000000);
    VK_CHECK_RESULT(result);
    reset_fences(1, &rd->fences[resource_index]);

    result = acquire_next_image(rd->swapchain.swapchain, UINT64_MAX, rd->image_available_semaphore[resource_index], &image_index);
	switch(result) {
		case VK_SUCCESS:
			break;
		case VK_SUBOPTIMAL_KHR:
		case VK_ERROR_OUT_OF_DATE_KHR:
            printf("acquire subotimo\n");
			break;
            /*renderer_resize(rd, rd->width, rd->height);*/
		default:
			printf("Problem occurred during swap chain image acquisition!\n");
			return false;
	}

    renderer_frame(rd, resource_index, image_index);

	VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    CommandSubmitInfo submit_info;
    submit_info.command_buffers = &rd->graphic_cmdbuffer[resource_index];
    submit_info.command_buffers_count = 1;
    submit_info.wait_semaphores_count = 1;
    submit_info.wait_semaphores = &rd->image_available_semaphore[resource_index];
    submit_info.wait_dst_stage_mask = wait_dst_stage_mask;
    submit_info.signal_semaphores_count = 1;
    submit_info.signal_semaphores = &rd->rendering_finished_semaphore[resource_index];
    submit_info.fence = rd->fences[resource_index];

    result = submit_commands(&submit_info);
    VK_CHECK_RESULT(result);

    PresentInfo present_info;
    present_info.wait_semaphores_count = 1;
    present_info.wait_semaphores = &rd->rendering_finished_semaphore[resource_index];
    present_info.swapchain_count = 1;
    present_info.swapchains = &rd->swapchain.swapchain;
    present_info.image_index = &image_index;

    result = present_image(&present_info);
	switch(result) {
		case VK_SUCCESS:
			break;
		case VK_SUBOPTIMAL_KHR:
		case VK_ERROR_OUT_OF_DATE_KHR:
            printf("present subotimo\n");
            break;
            /*renderer_resize(rd, rd->width, rd->height);*/
		default:
			printf("Problem occurred during image presentation!\n");
			return false;
	}
	resource_index = next_resource_index;

    /*vkDeviceWaitIdle(rd->device.logical_device);*/
	return true;

}
/*}}}*/

/*{{{void renderer_render(Renderer* rd)*/
void renderer_render(Renderer* rd) {
    update_uniform_buffer(rd);
    renderer_draw(rd);
}
/*}}}*/

/*{{{void renderer_resize(Renderer* rd, uint32_t width, uint32_t height)*/
void renderer_resize(Renderer* rd, uint32_t width, uint32_t height) {
    rd->width = width;
    rd->height = height;
    device_wait_idle();
    rd->swapchain = create_swapchain(rd->width, rd->width);
    /*setup_framebuffer(&vk_context, true);*/
    create_attachments(rd);
    update_composition_descriptors(rd);
    device_wait_idle();
}
/*}}}*/

/*{{{VkResult create_attachments(Renderer* rd)*/
VkResult create_attachments(Renderer* rd) {
    uint32_t width = rd->width;
    uint32_t height = rd->height;
    VkResult result;

    /* GBuffer creation {{{*/
    {
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        rd->position_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        rd->normal_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        rd->albedo_image.format = VK_FORMAT_R8G8B8A8_UNORM;
        rd->metallic_roughness_image.format = VK_FORMAT_R8G8B8A8_UNORM;
        result = create_image(width, height,usage, aspect, 1, &rd->position_image);
        VK_CHECK_RESULT(result);
        result = create_image(width, height, usage, aspect, 1, &rd->normal_image);
        VK_CHECK_RESULT(result);
        result = create_image(width, height, usage, aspect, 1, &rd->albedo_image);
        VK_CHECK_RESULT(result);
        result = create_image(width, height, usage, aspect, 1, &rd->metallic_roughness_image);
        VK_CHECK_RESULT(result);
    }
    /*}}}*/

    /*{{{ Depth stencil creation*/
    VkFormat depth_formats[5] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM
    };
    for (uint32_t i = 0; i < 5; i++) {
        VkFormatProperties format_props;
        get_physical_device_format_properties(depth_formats[i], &format_props);
        if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            rd->depth_image.format = depth_formats[i];
        }
    }

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (rd->depth_image.format >= VK_FORMAT_D16_UNORM_S8_UINT) {
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    result = create_image(width, height, usage, aspect, 1, &rd->depth_image);
    VK_CHECK_RESULT(result);
    /*}}}*/

    return result;
}
/*}}}*/

/*{{{void renderer_register_callback(Renderer* rd, draw_callback callback, uint32_t subpass_index)*/
void renderer_register_callback(Renderer* rd, draw_callback callback, uint32_t subpass_index) {
    rd->subpass_callbacks[subpass_index][rd->subpass_callbacks_count[subpass_index]] = callback;
    rd->subpass_callbacks_count[subpass_index]++;
}
/*}}}*/
