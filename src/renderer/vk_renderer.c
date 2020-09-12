#include "renderer/vk_renderer.h"

#include <stdio.h>
/*#include "vulkan/vulkan_core.h"*/
#include "sx/os.h"
#include "sx/allocator.h"
#include "sx/math.h"
#include "sx/string.h"
#include "device/device.h"
#include "vulkan/vulkan_core.h"
#include "sx/io.h"

#define VOLK_IMPLEMENTATION
#include "volk/volk.h"

#define MAX_NKTEXTURES 8

#define DDSKTX_IMPLEMENT
#define DDSKTX_API static
#define ddsktx_memcpy(_dst, _src, _size)    sx_memcpy((_dst), (_src), (_size))
#define ddsktx_memset(_dst, _v, _size)      sx_memset((_dst), (_v), (_size))
#define ddsktx_assert(_a)                   sx_assert((_a))
#define ddsktx_strcpy(_dst, _src)           sx_strcpy((_dst), sizeof(_dst), (_src))
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
#include "dds-ktx/dds-ktx.h"
SX_PRAGMA_DIAGNOSTIC_POP()

/*#define VALIDATION_LAYERS*/

int RENDERING_RESOURCES_SIZE = 2;
int swapchain_image_count = 0;

/*typedef struct RendererContext {{{*/
typedef struct RendererContext {
    VkDebugUtilsMessengerEXT vk_debugmessenger;

    DeviceVk device;

    uint32_t graphic_queue_index;
    uint32_t present_queue_index;
    uint32_t transfer_queue_index;
    uint32_t compute_queue_index;

    VkQueue  vk_graphic_queue;
    VkQueue  vk_present_queue;
    VkQueue  vk_transfer_queue;
    VkQueue  vk_compute_queue;

    VkCommandPool present_queue_cmdpool;
    VkCommandPool graphic_queue_cmdpool;
    VkCommandPool transfer_queue_cmdpool;
    VkCommandPool compute_queue_cmdpool;

    VkCommandBuffer* present_cmdbuffer;
    VkCommandBuffer* graphic_cmdbuffer;
    VkCommandBuffer transfer_cmdbuffer;
    VkCommandBuffer compute_cmdbuffer;
    VkCommandBuffer copy_cmdbuffer;

    VkSemaphore *image_available_semaphore;
    VkSemaphore *rendering_finished_semaphore;
    VkFence *fences;

    Swapchain swapchain;
    ImageBuffer depth_image;

    VkRenderPass render_pass;

    VkFramebuffer* framebuffer;


    uint32_t width;
    uint32_t height;
    uint32_t attchments_width;
    uint32_t attchments_height;

} RendererContext;
/*}}}*/

static RendererContext vk_context;


/* {{{debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, */
static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
        void* p_user_data) {
    printf("Validation layer: %s\n\n", p_callback_data->pMessage);

    return VK_FALSE;
}
/* }}}*/

uint32_t get_memory_type(VkMemoryRequirements image_memory_requirements, VkMemoryPropertyFlags properties);

VkCommandPool find_pool(QueueType type);

VkResult create_image(DeviceVk* device, ImageBuffer* image, 
                      uint32_t width, uint32_t height, 
                      VkImageUsageFlags usage, VkImageAspectFlags aspect, uint32_t mip_levels);

void clear_image(DeviceVk* device, ImageBuffer* image);

VkResult create_attachments(RendererContext* context);


/*bool vk_renderer_init(DeviceWindow win) {{{*/
bool vk_renderer_init(DeviceWindow win) {
    vk_context.width = win.width;
    vk_context.height = win.height;
    volkInitialize();

    const sx_alloc* alloc = sx_alloc_malloc();

    uint32_t num_layers = 0;
    uint32_t num_extensions = 2;
    const char *layers[10];

#ifdef VALIDATION_LAYERS
    /*const char *validation_layers[] = {*/
        /*"VK_LAYER_KHRONOS_validation"*/
    /*};*/
    layers[num_layers] = "VK_LAYER_KHRONOS_validation";
    num_layers++;
    num_extensions++;
#endif

    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        KHR_SURFACE_EXTENSION_NAME,
#ifdef VALIDATION_LAYERS
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
    };


    VkResult result;
    /* Instance Creation {{{ */
    {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, NULL);
        VkLayerProperties *layer_properties;
        layer_properties = sx_malloc(alloc, (layer_count * sizeof(*layer_properties)));
        vkEnumerateInstanceLayerProperties(&layer_count, layer_properties);
        for (uint32_t i = 0; i < num_layers; i++) {
            bool has_layer = false;
            for (uint32_t j = 0; j < layer_count; j++) {
                if (strcmp(layers[i], layer_properties[j].layerName) == 0) {
                    has_layer = true;
                    break;
                }
            }
            if (!has_layer) {
                return false;
            }
        }
        sx_free(alloc, layer_properties);


        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
        VkExtensionProperties *extension_properties;
        extension_properties = sx_malloc(alloc, extension_count *
                sizeof(*extension_properties));
        vkEnumerateInstanceExtensionProperties(NULL, &extension_count, extension_properties);

        for (uint32_t i = 0; i < num_extensions; i++)
        {
            bool has_extension = false;
            for(uint32_t j = 0; j < extension_count; j++)
            {
                if (strcmp(extensions[i], extension_properties[j].extensionName) == 0)
                {
                    has_extension = true;
                    break;
                }
            }
            if (!has_extension)
            {
                return false;
            }
        }
        sx_free(alloc, extension_properties);

#ifdef VALIDATION_LAYERS

        VkDebugUtilsMessengerCreateInfoEXT dbg_create_info;
        dbg_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbg_create_info.flags = 0;
        dbg_create_info.pNext = NULL;
        dbg_create_info.pUserData = NULL;
        dbg_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbg_create_info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbg_create_info.pfnUserCallback = debug_callback;
#endif


        VkApplicationInfo app_info;
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pNext = NULL;
        app_info.pApplicationName = "Gabibits Wingsuit";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "Gabibits Wingsuit";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo inst_create_info;
        inst_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        inst_create_info.pNext = NULL;
#ifdef VALIDATION_LAYERS
        inst_create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&dbg_create_info;
#endif
        inst_create_info.flags = 0;
        inst_create_info.pApplicationInfo = &app_info;
        inst_create_info.enabledLayerCount = num_layers;
        inst_create_info.ppEnabledLayerNames = layers;
        inst_create_info.enabledExtensionCount = num_extensions;
        inst_create_info.ppEnabledExtensionNames = extensions;

        result = vkCreateInstance(&inst_create_info, NULL, &vk_context.device.vk_instance);
        sx_assert_rel(result == VK_SUCCESS && "Could not create instance");
        volkLoadInstance(vk_context.device.vk_instance);


#ifdef VALIDATION_LAYERS
        result = vkCreateDebugUtilsMessengerEXT(vk_context.device.vk_instance, &dbg_create_info, NULL, &vk_context.vk_debugmessenger);
        sx_assert_rel(result == VK_SUCCESS && "Could not create Debug Utils Messenger");
#endif
    }
    /* }}} */

    /* Initiate swap chain {{{*/
    {
        VkXcbSurfaceCreateInfoKHR surface_create_info;
        surface_create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        surface_create_info.pNext = NULL;
        surface_create_info.flags = 0;
        surface_create_info.connection = win.connection;
        surface_create_info.window = win.window;

        result = vkCreateXcbSurfaceKHR( vk_context.device.vk_instance, &surface_create_info, NULL,
                &vk_context.device.presentation_surface);
        sx_assert_rel(result == VK_SUCCESS && "Could not create swap chain");
    }
    /*}}}*/

    /* Logical Device creation {{{*/
    {
        uint32_t physical_device_count;
        result = vkEnumeratePhysicalDevices(vk_context.device.vk_instance, &physical_device_count, NULL);
        sx_assert_rel(result == VK_SUCCESS && "Could not create physical device");

        VkPhysicalDevice *physical_devices = sx_malloc(alloc, physical_device_count *
                sizeof(*physical_devices));
        result = vkEnumeratePhysicalDevices(vk_context.device.vk_instance, 
                &physical_device_count, physical_devices);

        vk_context.device.physical_device = physical_devices[0];
        for (uint32_t i = 0; i < physical_device_count; i++)
        {
            VkPhysicalDeviceProperties device_props;
            vkGetPhysicalDeviceProperties(physical_devices[i], &device_props);
            if (device_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                vk_context.device.physical_device = physical_devices[i];
            }
        }
        sx_free(alloc, physical_devices);

        uint32_t extension_count = 0;

        vkEnumerateDeviceExtensionProperties(vk_context.device.physical_device, NULL, &extension_count,
                NULL);
        VkExtensionProperties *available_extensions;
        available_extensions = sx_malloc(alloc, extension_count *
                sizeof(*available_extensions));
        vkEnumerateDeviceExtensionProperties(vk_context.device.physical_device, NULL, &extension_count,
                available_extensions);

        const char *device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        uint32_t num_device_extensions = 1;

        for (uint32_t i = 0; i < num_device_extensions; i++) {
            bool has_extension = false;
            for(uint32_t j = 0; j < extension_count; j++) {
                if (strcmp(device_extensions[i], available_extensions[j].extensionName) == 0) {
                    has_extension = true;
                    break;
                }
            }
            sx_assert_rel(has_extension);
        }
        sx_free(alloc, available_extensions);

        /* Choose queues */
        uint32_t queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(vk_context.device.physical_device, &queue_family_count, NULL);
        VkQueueFamilyProperties *queue_family_properties = sx_malloc(alloc,
                queue_family_count * sizeof(*queue_family_properties));
        vkGetPhysicalDeviceQueueFamilyProperties(vk_context.device.physical_device, &queue_family_count,
                queue_family_properties);
        for (uint32_t i = 0; i < queue_family_count; i++) {
            VkBool32 queue_present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(vk_context.device.physical_device, i,
                    vk_context.device.presentation_surface, &queue_present_support);
            if(queue_present_support) {
                vk_context.present_queue_index = i;
            }
            if (queue_family_properties[i].queueCount >0 &&
                    queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                vk_context.graphic_queue_index = i;
                if(queue_present_support) break;
            }

        }
        for (uint32_t i = 0; i < queue_family_count; i++) {
            if (queue_family_count > 0 &&
                    queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
                vk_context.transfer_queue_index = i;
            }
            if (queue_family_count > 0 &&
                    !(queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                break;
            }
        }

        for (uint32_t i = 0; i < queue_family_count; i++) {
            if (queue_family_count > 0 &&
                    queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                vk_context.compute_queue_index = i;
            }
            if (queue_family_count > 0 &&
                    queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                break;
            }
        }
        sx_free(alloc, queue_family_properties);

        VkDeviceQueueCreateInfo queue_create_info[4];
        float queue_priority = 1.0;
        uint32_t number_of_queues = 1;
        uint32_t idx = 1;
        queue_create_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info[0].pNext = NULL;
        queue_create_info[0].flags = 0;
        queue_create_info[0].queueFamilyIndex = vk_context.graphic_queue_index;
        queue_create_info[0].queueCount = 1;
        queue_create_info[0].pQueuePriorities = &queue_priority;

        if (vk_context.graphic_queue_index != vk_context.transfer_queue_index) {
            number_of_queues++;
            queue_create_info[idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info[idx].pNext = NULL;
            queue_create_info[idx].flags = 0;
            queue_create_info[idx].queueFamilyIndex = vk_context.transfer_queue_index;
            queue_create_info[idx].queueCount = 1;
            queue_create_info[idx].pQueuePriorities = &queue_priority;
            idx++;
        }

        if (vk_context.graphic_queue_index != vk_context.present_queue_index) {
            number_of_queues++;
            queue_create_info[idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info[idx].pNext = NULL;
            queue_create_info[idx].flags = 0;
            queue_create_info[idx].queueFamilyIndex = vk_context.present_queue_index;
            queue_create_info[idx].queueCount = 1;
            queue_create_info[idx].pQueuePriorities = &queue_priority;
        }

        if ((vk_context.graphic_queue_index != vk_context.compute_queue_index) &&
            (vk_context.compute_queue_index != vk_context.transfer_queue_index)) {
            number_of_queues++;
            queue_create_info[idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info[idx].pNext = NULL;
            queue_create_info[idx].flags = 0;
            queue_create_info[idx].queueFamilyIndex = vk_context.compute_queue_index;
            queue_create_info[idx].queueCount = 1;
            queue_create_info[idx].pQueuePriorities = &queue_priority;
        }
        
        VkPhysicalDeviceFeatures device_features = {};
        device_features.tessellationShader = VK_TRUE;
        device_features.samplerAnisotropy = VK_TRUE;
        device_features.fillModeNonSolid = VK_TRUE;


        VkDeviceCreateInfo device_create_info;
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.pNext = NULL;
        device_create_info.flags = 0;
        device_create_info.queueCreateInfoCount = number_of_queues;
        device_create_info.pQueueCreateInfos = &queue_create_info[0];
        device_create_info.enabledExtensionCount = 1;
        device_create_info.ppEnabledExtensionNames = device_extensions;
        device_create_info.enabledLayerCount = 0;
        device_create_info.ppEnabledLayerNames = NULL;
#ifdef VALIDATION_LAYERS
        device_create_info.enabledLayerCount = num_layers;
        device_create_info.ppEnabledLayerNames = layers;
#endif
        device_create_info.enabledLayerCount = 0;
        device_create_info.ppEnabledLayerNames = NULL;
        device_create_info.pEnabledFeatures = &device_features;

        result = vkCreateDevice(vk_context.device.physical_device, &device_create_info, NULL, &vk_context.device.logical_device);
        sx_assert_rel(result == VK_SUCCESS && "Could not create logical_device");
        volkLoadDevice(vk_context.device.logical_device);
    }
    /*}}}*/

    /*Request queue handles {{{*/
    vkGetDeviceQueue(vk_context.device.logical_device, vk_context.graphic_queue_index, 0,
            &vk_context.vk_graphic_queue);
    vkGetDeviceQueue(vk_context.device.logical_device, vk_context.present_queue_index, 0,
            &vk_context.vk_present_queue);
    vkGetDeviceQueue(vk_context.device.logical_device, vk_context.transfer_queue_index, 0,
            &vk_context.vk_transfer_queue);
    vkGetDeviceQueue(vk_context.device.logical_device, vk_context.compute_queue_index, 0, 
            &vk_context.vk_compute_queue);
    /*}}}*/

    vk_context.swapchain = create_swapchain(win.width, win.height);
    result = create_attachments(&vk_context);

    /* Command pool creation and command buffer allocation {{{*/
	{
		VkCommandPoolCreateInfo cmd_pool_create_info;
		cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmd_pool_create_info.pNext = NULL;
		cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
									 VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		cmd_pool_create_info.queueFamilyIndex = vk_context.graphic_queue_index;
		result = vkCreateCommandPool(vk_context.device.logical_device, &cmd_pool_create_info, NULL,
				&vk_context.graphic_queue_cmdpool);
        sx_assert_rel(result == VK_SUCCESS && "Could not create command pool!");

        cmd_pool_create_info.queueFamilyIndex = vk_context.compute_queue_index;
        result = vkCreateCommandPool(vk_context.device.logical_device, &cmd_pool_create_info, NULL,
                &vk_context.compute_queue_cmdpool);
        sx_assert_rel(result == VK_SUCCESS && "Could not create command pool!");
        create_command_buffer(COMPUTE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1,
                &vk_context.compute_cmdbuffer);

        vk_context.graphic_cmdbuffer = sx_malloc(sx_alloc_malloc(), RENDERING_RESOURCES_SIZE *
                sizeof(*(vk_context.graphic_cmdbuffer)));
        create_command_buffer(GRAPHICS, VK_COMMAND_BUFFER_LEVEL_PRIMARY, RENDERING_RESOURCES_SIZE, 
                vk_context.graphic_cmdbuffer);
    }
    /*}}}*/

	vk_context.framebuffer = sx_malloc(alloc, RENDERING_RESOURCES_SIZE *
			sizeof(*(vk_context.framebuffer)));
	for (uint32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {
		vk_context.framebuffer[i] = VK_NULL_HANDLE;
	}

    return true;
}
/*}}}*/


/* create_swapchain( uint32_t width, uint32_t height) {{{*/
Swapchain create_swapchain( uint32_t width, uint32_t height) {
    Swapchain swapchain;
    swapchain.swapchain = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < 4; i++) {
        swapchain.images[i] = VK_NULL_HANDLE;
        swapchain.image_views[i] = VK_NULL_HANDLE;
    }
    const sx_alloc* alloc = sx_alloc_malloc();
    VkSurfaceCapabilitiesKHR surface_capabilities;
    VkResult result;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_context.device.physical_device,
            vk_context.device.presentation_surface, &surface_capabilities);
    sx_assert_rel(result == VK_SUCCESS);

    uint32_t formats_count;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(vk_context.device.physical_device,
            vk_context.device.presentation_surface, &formats_count, NULL);
    VkSurfaceFormatKHR *surface_formats = sx_malloc(alloc, formats_count *
            sizeof(*surface_formats));
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(vk_context.device.physical_device,
            vk_context.device.presentation_surface, &formats_count, surface_formats);
    sx_assert_rel(result == VK_SUCCESS);

    uint32_t present_modes_count;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(vk_context.device.physical_device,
            vk_context.device.presentation_surface, &present_modes_count, NULL);
    VkPresentModeKHR *present_modes = sx_malloc(alloc, present_modes_count *
            sizeof(*present_modes));
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(vk_context.device.physical_device,
            vk_context.device.presentation_surface, &present_modes_count, present_modes);
    sx_assert_rel(result == VK_SUCCESS);


    uint32_t image_count = surface_capabilities.minImageCount + 1;
    if( (surface_capabilities.maxImageCount > 0) && (image_count >
                surface_capabilities.maxImageCount) ) {
        image_count = surface_capabilities.maxImageCount;
    }

    VkSurfaceFormatKHR surface_format = surface_formats[0];
    if( (formats_count == 1) && (surface_formats[0].format = VK_FORMAT_UNDEFINED) ) {
        surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
        surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }

    for (uint32_t i = 0; i < formats_count; i++) {
        if(surface_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            surface_format = surface_formats[i];
            break;
        }
    }
    sx_free(alloc, surface_formats);

    /* Clamp width and height */
    VkExtent2D swap_chain_extent = { width, height };
    if (surface_capabilities.currentExtent.width == -1) {
        if (swap_chain_extent.width < surface_capabilities.minImageExtent.width )
            swap_chain_extent.width = surface_capabilities.minImageExtent.width;
        if (swap_chain_extent.height < surface_capabilities.minImageExtent.height)
            swap_chain_extent.height = surface_capabilities.minImageExtent.height;
        if (swap_chain_extent.width > surface_capabilities.maxImageExtent.width)
            swap_chain_extent.width = surface_capabilities.maxImageExtent.width;
        if (swap_chain_extent.height > surface_capabilities.maxImageExtent.height)
            swap_chain_extent.height = surface_capabilities.maxImageExtent.height;
    } else {
        swap_chain_extent = surface_capabilities.currentExtent;
    }

    VkImageUsageFlagBits swap_chain_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        swap_chain_usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        swap_chain_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    VkSurfaceTransformFlagBitsKHR swap_chain_transform_flags;
    if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        swap_chain_transform_flags = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        swap_chain_transform_flags = surface_capabilities.currentTransform;
    }

    VkPresentModeKHR present_mode;
    for(uint32_t i = 0; i < present_modes_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_FIFO_KHR)
            present_mode = present_modes[i];
    }
    for(uint32_t i = 0; i < present_modes_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            present_mode = present_modes[i];
    }
    present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    sx_free(alloc, present_modes);

    VkSwapchainCreateInfoKHR swap_chain_create_info;
    swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_create_info.pNext = NULL;
    swap_chain_create_info.flags = 0;
    swap_chain_create_info.surface = vk_context.device.presentation_surface;
    swap_chain_create_info.minImageCount = image_count;
    swap_chain_create_info.imageFormat = surface_format.format;
    swap_chain_create_info.imageColorSpace = surface_format.colorSpace;
    swap_chain_create_info.imageExtent = swap_chain_extent;
    swap_chain_create_info.imageArrayLayers = 1;
    swap_chain_create_info.imageUsage = swap_chain_usage_flags;
    swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_chain_create_info.queueFamilyIndexCount = 0;
    swap_chain_create_info.pQueueFamilyIndices = NULL;
    swap_chain_create_info.preTransform = swap_chain_transform_flags;
    swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_create_info.presentMode = present_mode;
    swap_chain_create_info.clipped = VK_TRUE;
    swap_chain_create_info.oldSwapchain = vk_context.swapchain.swapchain;
    VkSwapchainKHR old_swapchain = vk_context.swapchain.swapchain;

    result = vkCreateSwapchainKHR(vk_context.device.logical_device, &swap_chain_create_info, NULL,
            &swapchain.swapchain);
    sx_assert_rel(result == VK_SUCCESS);

    if(old_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(vk_context.device.logical_device, old_swapchain, NULL);
    }

    vkGetSwapchainImagesKHR(vk_context.device.logical_device, swapchain.swapchain, &image_count, NULL);
    vkGetSwapchainImagesKHR(vk_context.device.logical_device, swapchain.swapchain, &image_count,
            swapchain.images);
    swapchain_image_count = image_count;

    VkImageSubresourceRange image_subresource_range;
    image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_subresource_range.baseMipLevel = 0;
    image_subresource_range.levelCount = 1;
    image_subresource_range.baseArrayLayer = 0;
    image_subresource_range.layerCount = 1;

    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo image_create_info;
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_create_info.pNext = NULL;
        image_create_info.flags = 0;
        image_create_info.image = swapchain.images[i];
        image_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_create_info.format = surface_format.format;
        image_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_create_info.subresourceRange = image_subresource_range;

        if (&swapchain.image_views[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(vk_context.device.logical_device, swapchain.image_views[i], NULL);
        }
        result = vkCreateImageView(vk_context.device.logical_device, &image_create_info, NULL,
                &swapchain.image_views[i]);
        sx_assert_rel(result == VK_SUCCESS);
    }
    swapchain.format = surface_format;
    /*RENDERING_RESOURCES_SIZE = image_count;*/
    return swapchain;
}
/*}}}*/

/* get_memory_type(VkMemoryRequirements image_memory_requirements, VkMemoryPropertyFlags properties) {{{*/
uint32_t get_memory_type(VkMemoryRequirements image_memory_requirements, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(vk_context.device.physical_device, &memory_properties);
    uint32_t index = UINT32_MAX;
    for(uint32_t k = 0; k < memory_properties.memoryTypeCount; k++) {
        if((image_memory_requirements.memoryTypeBits & (1<<k)) &&
                (memory_properties.memoryTypes[k].propertyFlags &
                 properties)) {
            index = k;
        }
    }
    return index;
}
/*}}}*/

/* create_image(DeviceVk* device, ImageBuffer* image_buffer, {{{*/
VkResult create_image(DeviceVk* device, ImageBuffer* image_buffer, 
        uint32_t width, uint32_t height, 
        VkImageUsageFlags usage, VkImageAspectFlags aspect, uint32_t mip_levels) {
    if (image_buffer->image != VK_NULL_HANDLE) {
        clear_image(device, image_buffer);
    }
    image_buffer->image = VK_NULL_HANDLE;
    image_buffer->image_view = VK_NULL_HANDLE;
    image_buffer->memory = VK_NULL_HANDLE;

    VkResult result;
    VkExtent3D extent;
    extent.width = width;
    extent.height = height;
    extent.depth = 1;
    VkImageCreateInfo image_create_info;
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.flags = 0;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = image_buffer->format;
    image_create_info.extent = extent;
    image_create_info.mipLevels = mip_levels;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = usage;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    result = vkCreateImage(device->logical_device, &image_create_info, NULL,
            &image_buffer->image);
    if(result != VK_SUCCESS) {
        printf("Could not create depht image!\n");
        return result;
    }

    VkMemoryRequirements image_memory_requirements;
    vkGetImageMemoryRequirements(device->logical_device, image_buffer->image,
            &image_memory_requirements);
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(device->physical_device, &memory_properties);

    for(uint32_t k = 0; k < memory_properties.memoryTypeCount; k++) {
        if((image_memory_requirements.memoryTypeBits & (1<<k)) &&
                (memory_properties.memoryTypes[k].propertyFlags &
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {

            VkMemoryAllocateInfo memory_allocate_info;
            memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memory_allocate_info.pNext = NULL;
            memory_allocate_info.allocationSize = image_memory_requirements.size;
            memory_allocate_info.memoryTypeIndex = k;

            result = vkAllocateMemory(device->logical_device, &memory_allocate_info, NULL,
                    &image_buffer->memory);
            if(result != VK_SUCCESS) {
                printf("Could not allocate memory!\n");
                return result;
            }
        }
    }
    result = vkBindImageMemory(device->logical_device, image_buffer->image,
            image_buffer->memory, 0);
    if(result != VK_SUCCESS) {
        printf("Could not bind memory image!\n");
        return result;
    }
    VkImageSubresourceRange image_subresource_range;
    image_subresource_range.aspectMask = aspect;
    image_subresource_range.baseMipLevel = 0;
    image_subresource_range.levelCount = 1;
    image_subresource_range.baseArrayLayer = 0;
    image_subresource_range.layerCount = 1;

    VkImageViewCreateInfo image_view_create_info;
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.pNext = NULL;
    image_view_create_info.flags = 0;
    image_view_create_info.image = image_buffer->image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = image_buffer->format;
    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.subresourceRange = image_subresource_range;

    result = vkCreateImageView(device->logical_device, &image_view_create_info, NULL,
            &image_buffer->image_view);
    if(result != VK_SUCCESS) {
        printf("Could not depth image view!\n");
        return result;
    }
    return result;
}
/*}}}*/

/* clear_image(DeviceVk* device, ImageBuffer* image) {{{*/
void clear_image(DeviceVk* device, ImageBuffer* image) {
    vkDestroyImageView(device->logical_device, image->image_view, NULL);
    vkFreeMemory(device->logical_device, image->memory, NULL);
    vkDestroyImage(device->logical_device, image->image, NULL);
}
/*}}}*/

/*{{{void clear_buffer(DeviceVk* device, Buffer* buffer) { */
void clear_buffer(DeviceVk* device, Buffer* buffer) {
    vkFreeMemory(device->logical_device, buffer->memory, NULL);
    vkDestroyBuffer(device->logical_device, buffer->buffer, NULL);
}
/*}}}*/

/*{{{void clear_texture(DeviceVk* device, Texture* texture) {*/
void clear_texture(DeviceVk* device, Texture* texture) {
    vkDestroySampler(device->logical_device, texture->sampler, NULL);
    clear_image(device, &texture->image_buffer);
}
/*}}}*/

/* {{{ VkResult create_texture_from_data(Texture* texture, VkSamplerAddressMode sampler_address_mode, const sx_alloc* alloc, */
VkResult create_texture_from_data(Texture* texture, VkSamplerAddressMode sampler_address_mode, const sx_alloc* alloc, 
        const void* data, uint32_t width, uint32_t height) {
    clear_texture(&vk_context.device, texture);
    VkResult result;
    Buffer staging;
    uint32_t size = width * height * 4;
    result = create_buffer(&staging, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, size);
    sx_assert_rel(result == VK_SUCCESS && "Could not create staging buffer!");

    void *staging_buffer_memory_pointer;
    result = vkMapMemory(vk_context.device.logical_device, staging.memory, 0,
            size, 0, &staging_buffer_memory_pointer);
    sx_assert_rel(result == VK_SUCCESS && "Could not map memory and upload data to staging buffer!");
    sx_memcpy(staging_buffer_memory_pointer, data, size);
    vkUnmapMemory(vk_context.device.logical_device, staging.memory);
    uint32_t offset = 0;
    VkExtent3D extent;
    extent.width = width;
    extent.height = height;
    extent.depth = 1;
    VkImageCreateInfo image_create_info;
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.flags = 0;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_create_info.extent = extent;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = vkCreateImage(vk_context.device.logical_device, &image_create_info, NULL,
            &texture->image_buffer.image);
    sx_assert_rel(result == VK_SUCCESS && "Could not create texture image!");

    VkMemoryRequirements image_memory_requirements;
    vkGetImageMemoryRequirements(vk_context.device.logical_device, texture->image_buffer.image, &image_memory_requirements);

    VkMemoryAllocateInfo memory_allocate_info;
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = image_memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = get_memory_type(image_memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    result = vkAllocateMemory(vk_context.device.logical_device, &memory_allocate_info, NULL,
            &texture->image_buffer.memory);

    sx_assert_rel(result == VK_SUCCESS && "Could not allocate memory for texture!");

    result = vkBindImageMemory(vk_context.device.logical_device, texture->image_buffer.image, texture->image_buffer.memory, 0);
    sx_assert_rel(result == VK_SUCCESS && "Could not bind memory image!");

    VkImageSubresourceRange image_subresource_range;
    image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_subresource_range.baseMipLevel = 0;
    image_subresource_range.levelCount = 1;
    image_subresource_range.baseArrayLayer = 0;
    image_subresource_range.layerCount = 1;
    VkCommandBuffer cmdbuffer = vk_context.copy_cmdbuffer;;

    VkCommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.pNext = NULL;
    command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    command_buffer_begin_info.pInheritanceInfo = NULL;

    VkBufferImageCopy buffer_image_copy_info = {};
    buffer_image_copy_info.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    buffer_image_copy_info.imageSubresource.mipLevel = 0;
    buffer_image_copy_info.imageSubresource.baseArrayLayer = 0;
    buffer_image_copy_info.imageSubresource.layerCount = 1;
    buffer_image_copy_info.imageExtent.width = width;
    buffer_image_copy_info.imageExtent.height = height;
    buffer_image_copy_info.imageExtent.depth = 1;
    buffer_image_copy_info.bufferOffset = offset;

    vkBeginCommandBuffer(cmdbuffer, &command_buffer_begin_info);

    {
        VkImageMemoryBarrier image_memory_barrier = {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.pNext = NULL;
        image_memory_barrier.srcAccessMask = 0;
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_memory_barrier.image = texture->image_buffer.image;
        image_memory_barrier.subresourceRange = image_subresource_range;
        vkCmdPipelineBarrier(cmdbuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0,
                NULL, 1, &image_memory_barrier);
    }

    vkCmdCopyBufferToImage(cmdbuffer, staging.buffer,
            texture->image_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
            &buffer_image_copy_info);
    {
        VkImageMemoryBarrier image_memory_barrier = {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.pNext = NULL;
        image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_memory_barrier.image = texture->image_buffer.image;
        image_memory_barrier.subresourceRange = image_subresource_range;
        vkCmdPipelineBarrier( cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
                &image_memory_barrier);
    }

    vkEndCommandBuffer(cmdbuffer);

    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmdbuffer;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = NULL;

    VkFenceCreateInfo fence_create_info;
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = 0;
    fence_create_info.pNext = NULL;
    VkFence fence;
    vkCreateFence(vk_context.device.logical_device, &fence_create_info, NULL, &fence);

    result = vkQueueSubmit(vk_context.vk_graphic_queue, 1, &submit_info, fence);
    sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
    result = vkWaitForFences(vk_context.device.logical_device, 1, &fence, VK_TRUE, 1000000000);
    sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
    vkDestroyFence(vk_context.device.logical_device, fence, NULL);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = sampler_address_mode;
    samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
    samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
    samplerCreateInfo.mipLodBias = 0.0;
    samplerCreateInfo.maxAnisotropy = 1.0;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.minLod = 0.0;
    samplerCreateInfo.maxLod = 0.0;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    result = vkCreateSampler(vk_context.device.logical_device, &samplerCreateInfo, NULL, &texture->sampler);
    sx_assert_rel(result == VK_SUCCESS && "Could not create texture sampler!");

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.subresourceRange = image_subresource_range;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.image = texture->image_buffer.image;
    result = vkCreateImageView(vk_context.device.logical_device, &viewCreateInfo, NULL, &texture->image_buffer.image_view);
    sx_assert_rel(result == VK_SUCCESS && "Could not create texture image view!");

    // Clean up staging resources
    vkFreeMemory(vk_context.device.logical_device, staging.memory, NULL);
    vkDestroyBuffer(vk_context.device.logical_device, staging.buffer, NULL);

    return result;
}
/*}}}*/

/* create_texture(DeviceVk* device, Texture* texture, const sx_alloc* alloc, const char* filepath) {{{*/
VkResult create_texture(Texture* texture, VkSamplerAddressMode sampler_address_mode, const sx_alloc* alloc, const char* filepath) {
    texture->sampler = VK_NULL_HANDLE;
    texture->image_buffer.image = VK_NULL_HANDLE;
    texture->image_buffer.image_view = VK_NULL_HANDLE;
    texture->image_buffer.memory = VK_NULL_HANDLE;

    VkResult result;
    sx_mem_block* mem = sx_file_load_bin(alloc, filepath);
    ddsktx_texture_info tc = {0};
    ddsktx_error err;
    bool parse = ddsktx_parse(&tc, mem->data, mem->size, &err);
    /*printf("%s\n", filepath);*/
    sx_assert(parse == true && "Could not parse ktx file");
    if(parse) {
        Buffer staging;
        result = create_buffer(&staging, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tc.size_bytes);
        sx_assert_rel(result == VK_SUCCESS && "Could not create staging buffer!");

		void *staging_buffer_memory_pointer;
		VkResult result = vkMapMemory(vk_context.device.logical_device, staging.memory, 0,
				tc.size_bytes, 0, &staging_buffer_memory_pointer);
        sx_assert_rel(result == VK_SUCCESS && "Could not map memory and upload data to staging buffer!");
        uint32_t offset = 0;
        int num_faces = 1;
        if (tc.flags & DDSKTX_TEXTURE_FLAG_CUBEMAP) {
            num_faces = DDSKTX_CUBE_FACE_COUNT;
        }
        ddsktx_sub_data sub_data;
        int num_regions = tc.num_mips * tc.num_layers * num_faces;
        VkBufferImageCopy* buffer_copy_regions = sx_malloc(alloc, num_regions * sizeof(*buffer_copy_regions));
        int idx = 0;
        int layer_face = 0;
        for (int mip = 0; mip < tc.num_mips; mip++) {
            layer_face = 0;
            for (int layer = 0; layer < tc.num_layers; layer++) {
                for (int face = 0; face < num_faces; face++) {
                    ddsktx_get_sub(&tc, &sub_data, mem->data, mem->size, layer, face, mip);
                    sx_memcpy(staging_buffer_memory_pointer + offset, sub_data.buff, sub_data.size_bytes);
                    VkBufferImageCopy buffer_image_copy_info = {};
                    buffer_image_copy_info.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    buffer_image_copy_info.imageSubresource.mipLevel = mip;
                    buffer_image_copy_info.imageSubresource.baseArrayLayer = layer_face;
                    buffer_image_copy_info.imageSubresource.layerCount = 1;
                    buffer_image_copy_info.imageExtent.width = sub_data.width;
                    buffer_image_copy_info.imageExtent.height = sub_data.height;
                    buffer_image_copy_info.imageExtent.depth = 1;
                    buffer_image_copy_info.bufferOffset = offset;
                    buffer_copy_regions[idx] = buffer_image_copy_info;

                    offset += sub_data.size_bytes;
                    idx++;
                    layer_face++;
                }
            }
        }
		vkUnmapMemory(vk_context.device.logical_device, staging.memory);
        VkExtent3D extent;
		extent.width = tc.width;
		extent.height = tc.height;
		extent.depth = 1;
		VkImageCreateInfo image_create_info;
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.pNext = NULL;
        image_create_info.flags = 0;
        if (tc.flags & DDSKTX_TEXTURE_FLAG_CUBEMAP) {
            image_create_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }
		image_create_info.imageType = VK_IMAGE_TYPE_2D;
		image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
		image_create_info.extent = extent;
		image_create_info.mipLevels = tc.num_mips;
		image_create_info.arrayLayers = layer_face;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_create_info.queueFamilyIndexCount = 0;
		image_create_info.pQueueFamilyIndices = NULL;
		image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		result = vkCreateImage(vk_context.device.logical_device, &image_create_info, NULL,
				&texture->image_buffer.image);
        sx_assert_rel(result == VK_SUCCESS && "Could not create texture image!");

		VkMemoryRequirements image_memory_requirements;
		vkGetImageMemoryRequirements(vk_context.device.logical_device, texture->image_buffer.image, &image_memory_requirements);

        VkMemoryAllocateInfo memory_allocate_info;
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.pNext = NULL;
        memory_allocate_info.allocationSize = image_memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = get_memory_type(image_memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        result = vkAllocateMemory(vk_context.device.logical_device, &memory_allocate_info, NULL,
                &texture->image_buffer.memory);

        sx_assert_rel(result == VK_SUCCESS && "Could not allocate memory for texture!");

		result = vkBindImageMemory(vk_context.device.logical_device, texture->image_buffer.image, texture->image_buffer.memory, 0);
        sx_assert_rel(result == VK_SUCCESS && "Could not bind memory image!");

		VkImageSubresourceRange image_subresource_range;
        image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_subresource_range.baseMipLevel = 0;
		image_subresource_range.levelCount = tc.num_mips;
		image_subresource_range.baseArrayLayer = 0;
		image_subresource_range.layerCount = layer_face;
        /*printf("num_layers: %d\n\n", layer_face);*/
        /*printf("num_mips: %d\n\n", tc.num_mips);*/
        VkCommandBuffer cmdbuffer = vk_context.copy_cmdbuffer;;

		VkCommandBufferBeginInfo command_buffer_begin_info;
		command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_buffer_begin_info.pNext = NULL;
		command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		command_buffer_begin_info.pInheritanceInfo = NULL;

		vkBeginCommandBuffer(cmdbuffer, &command_buffer_begin_info);

        {
            VkImageMemoryBarrier image_memory_barrier = {};
            image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_memory_barrier.pNext = NULL;
            image_memory_barrier.srcAccessMask = 0;
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            image_memory_barrier.image = texture->image_buffer.image;
            image_memory_barrier.subresourceRange = image_subresource_range;
            vkCmdPipelineBarrier(cmdbuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0,
                    NULL, 1, &image_memory_barrier);
        }

		vkCmdCopyBufferToImage(cmdbuffer, staging.buffer,
				texture->image_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions,
				buffer_copy_regions);
        {
            VkImageMemoryBarrier image_memory_barrier = {};
            image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_memory_barrier.pNext = NULL;
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_memory_barrier.image = texture->image_buffer.image;
            image_memory_barrier.subresourceRange = image_subresource_range;
            vkCmdPipelineBarrier( cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
                    &image_memory_barrier);
        }
        sx_free(alloc, buffer_copy_regions);

		vkEndCommandBuffer(cmdbuffer);

		VkSubmitInfo submit_info;
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = NULL;
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmdbuffer;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;

        VkFenceCreateInfo fence_create_info;
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = 0;
        fence_create_info.pNext = NULL;
        VkFence fence;
        vkCreateFence(vk_context.device.logical_device, &fence_create_info, NULL, &fence);

        result = vkQueueSubmit(vk_context.vk_graphic_queue, 1, &submit_info, fence);
        sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
        result = vkWaitForFences(vk_context.device.logical_device, 1, &fence, VK_TRUE, 1000000000);
        sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
        vkDestroyFence(vk_context.device.logical_device, fence, NULL);

        // Create sampler
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = sampler_address_mode;
        samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
        samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
        samplerCreateInfo.mipLodBias = 0.0;
        samplerCreateInfo.maxAnisotropy = 4.0;
        samplerCreateInfo.anisotropyEnable = VK_TRUE;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = (float)tc.num_mips;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        result = vkCreateSampler(vk_context.device.logical_device, &samplerCreateInfo, NULL, &texture->sampler);
        sx_assert_rel(result == VK_SUCCESS && "Could not create texture sampler!");

        // Create image view
        VkImageViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        if (tc.num_layers > 1) {
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        } else if (tc.flags & DDSKTX_TEXTURE_FLAG_CUBEMAP) {
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        }
        viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.subresourceRange = image_subresource_range;
        viewCreateInfo.subresourceRange.layerCount = layer_face;
        viewCreateInfo.image = texture->image_buffer.image;
        result = vkCreateImageView(vk_context.device.logical_device, &viewCreateInfo, NULL, &texture->image_buffer.image_view);
        sx_assert_rel(result == VK_SUCCESS && "Could not create texture image view!");

        // Clean up staging resources
        vkFreeMemory(vk_context.device.logical_device, staging.memory, NULL);
        vkDestroyBuffer(vk_context.device.logical_device, staging.buffer, NULL);


    }
    sx_mem_destroy_block(mem);

    return result;
}
/*}}}*/

/* create_attachments(RendererContext* context){{{*/
VkResult create_attachments(RendererContext* context) {
    uint32_t width = context->width;
    uint32_t height = context->height;
    VkResult result;

    /* Depth stencil creation {{{ */
    {
        VkFormat depth_formats[5] = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM
        };

        for (uint32_t i = 0; i < 5; i++) {
            VkFormatProperties format_props;
            vkGetPhysicalDeviceFormatProperties(vk_context.device.physical_device,
                    depth_formats[i], &format_props);
            if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                vk_context.depth_image.format = depth_formats[i];
            }
        }
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (vk_context.depth_image.format >= VK_FORMAT_D16_UNORM_S8_UINT) {
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        result = create_image(&vk_context.device, &vk_context.depth_image,
                width, height, usage, aspect, 1);
        sx_assert_rel(result == VK_SUCCESS && "Could not create depth image");

    }
    /* }}} */

    return result;
}
/*}}}*/

/* create_buffer(DeviceVk* device, Buffer* buffer, VkBufferUsageFlags usage...) {{{*/
VkResult create_buffer(Buffer* buffer, VkBufferUsageFlags usage, 
                       VkMemoryPropertyFlags memory_properties_flags, VkDeviceSize size) {
        buffer->buffer = VK_NULL_HANDLE;
        buffer->memory = VK_NULL_HANDLE;
		buffer->size = size;
		VkBufferCreateInfo buffer_create_info;
		buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_create_info.pNext = NULL;
		buffer_create_info.flags = 0;
		buffer_create_info.size = buffer->size;
		buffer_create_info.usage = usage;
		buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer_create_info.queueFamilyIndexCount = 0;
		buffer_create_info.pQueueFamilyIndices = NULL;

		VkResult result = vkCreateBuffer(vk_context.device.logical_device, &buffer_create_info, NULL,
				&buffer->buffer);
        sx_assert_rel(result == VK_SUCCESS && "Could not create buffer");

		VkMemoryRequirements buffer_memory_requirements;
		vkGetBufferMemoryRequirements(vk_context.device.logical_device, buffer->buffer,
				&buffer_memory_requirements);
        VkMemoryAllocateInfo memory_allocate_info;
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.pNext = NULL;
        memory_allocate_info.allocationSize = buffer_memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = get_memory_type(buffer_memory_requirements, memory_properties_flags);
        result = vkAllocateMemory(vk_context.device.logical_device, &memory_allocate_info, NULL,
                &buffer->memory);

		result = vkBindBufferMemory(vk_context.device.logical_device, buffer->buffer,
				buffer->memory, 0);
        sx_assert_rel(result == VK_SUCCESS && "Could not bind memory for buffer");
        return result;
}
/*}}}*/

/*VkResult copy_buffer(DeviceVk* device, Buffer* dst_buffer, void* data, VkDeviceSize size) {{{*/
VkResult copy_buffer(Buffer* dst_buffer, void* data, VkDeviceSize size) {
    void *buffer_memory_pointer;
    VkResult result;
    result = vkMapMemory(vk_context.device.logical_device, dst_buffer->memory, 0, size, 0, &buffer_memory_pointer);
    sx_assert_rel(result == VK_SUCCESS && "Could not map memory and upload data to buffer!");
    sx_memcpy(buffer_memory_pointer, data, size);
    
    vkUnmapMemory(vk_context.device.logical_device, dst_buffer->memory);

    return result;
}
/*}}}*/

/* VkResult copy_buffer_staged(Buffer* dst_buffer, void* data, VkDeviceSize size) {{{*/
VkResult copy_buffer_staged(Buffer* dst_buffer, void* data, VkDeviceSize size) {
    Buffer staging;
    VkResult result = create_buffer(&staging, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, size);
    sx_assert_rel(result == VK_SUCCESS && "Could not create staging buffer!");

    void *staging_buffer_memory_pointer;
    result = vkMapMemory(vk_context.device.logical_device, staging.memory, 0,
            staging.size, 0, &staging_buffer_memory_pointer);
    sx_assert_rel(result == VK_SUCCESS && "Could not map memory and upload data to buffer!");
    sx_memcpy(staging_buffer_memory_pointer, data, size);
    
    vkUnmapMemory(vk_context.device.logical_device, staging.memory);
    VkCommandBuffer cmdbuffer = vk_context.copy_cmdbuffer;

    VkCommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.pNext = NULL;
    command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    command_buffer_begin_info.pInheritanceInfo = NULL;

    vkBeginCommandBuffer(cmdbuffer, &command_buffer_begin_info);
    VkBufferCopy buffer_copy_info;
    buffer_copy_info.srcOffset = 0;
    buffer_copy_info.dstOffset = 0;
    buffer_copy_info.size = size;
    vkCmdCopyBuffer(cmdbuffer, staging.buffer,
            dst_buffer->buffer, 1, &buffer_copy_info);

    vkEndCommandBuffer(cmdbuffer);

    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmdbuffer;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = NULL;

    VkFenceCreateInfo fence_create_info;
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = 0;
    fence_create_info.pNext = NULL;
    VkFence fence;
    vkCreateFence(vk_context.device.logical_device, &fence_create_info, NULL, &fence);

    result = vkQueueSubmit(vk_context.vk_graphic_queue, 1, &submit_info, fence);
    sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
    result = vkWaitForFences(vk_context.device.logical_device, 1, &fence, VK_TRUE, 1000000000);
    sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
    vkDestroyFence(vk_context.device.logical_device, fence, NULL);

    // Clean up staging resources
    vkFreeMemory(vk_context.device.logical_device, staging.memory, NULL);
    vkDestroyBuffer(vk_context.device.logical_device, staging.buffer, NULL);
    return result;
}
/*}}}*/

/* VkPipelineShaderStageCreateInfo load_shader(VkDevice logical_device, const char* filnename, VkShaderStageFlagBits stage) {{{*/
VkPipelineShaderStageCreateInfo load_shader(VkDevice logical_device, const char* filnename, VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo shaderstage_create_info;
    shaderstage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderstage_create_info.pNext = NULL;
    shaderstage_create_info.flags = 0;
    shaderstage_create_info.stage = stage;
    shaderstage_create_info.pName = "main";
    shaderstage_create_info.pSpecializationInfo = NULL;
    sx_mem_block* mem = sx_file_load_bin(sx_alloc_malloc(), filnename);
    /*FILE* shader = fopen(filnename, "rb");*/
    /*fseek(shader, 0, SEEK_END);*/
    /*uint64_t size = ftell(shader);*/
    /*uint8_t* data = (uint8_t*)aligned_alloc(sizeof(uint32_t), size * sizeof(uint8_t));*/
    /*rewind(shader);*/
    /*fread(data, sizeof(uint8_t), size, shader);*/
    VkShaderModuleCreateInfo module_create_info;
    module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_create_info.pNext = NULL;
    module_create_info.flags = 0;
    module_create_info.codeSize = mem->size;
    module_create_info.pCode = (uint32_t*)mem->data;
    VkResult result = vkCreateShaderModule(logical_device, &module_create_info, NULL,
            &shaderstage_create_info.module);
    if(result != VK_SUCCESS) {
        printf("Could not create vert shader module!\n");
    }
    sx_mem_destroy_block(mem);
    /*free(data);*/
    return shaderstage_create_info;
}
/*}}}*/

/* {{{ VkCommandPool find_pool(QueueType type) { */
VkCommandPool find_pool(QueueType type) {
    if (type == PRESENT) {
        return vk_context.present_queue_cmdpool;
    } else if (type == GRAPHICS) {
        return vk_context.graphic_queue_cmdpool;
    } else if (type == TRANSFER) {
        return vk_context.transfer_queue_cmdpool;
    } else if (type == COMPUTE) {
        return vk_context.compute_queue_cmdpool;
    }
    return vk_context.graphic_queue_cmdpool;
}
/*}}}*/

/* {{{ VkCommandPool find_queue(QueueType type) { */
VkQueue find_queue(QueueType type) {
    if (type == PRESENT) {
        return vk_context.vk_present_queue;
    } else if (type == GRAPHICS) {
        return vk_context.vk_graphic_queue;
    } else if (type == TRANSFER) {
        return vk_context.vk_transfer_queue;
    } else if (type == COMPUTE) {
        return vk_context.vk_compute_queue;
    }
    return vk_context.vk_graphic_queue;
}
/*}}}*/

/*{{{ VkResult create_command_buffer(VkCommandPool pool, VkCommandBufferLevel level, uint32_t count, VkCommandBuffer* cmdbuffer)*/
VkResult create_command_buffer(QueueType type, VkCommandBufferLevel level, uint32_t count, VkCommandBuffer* cmdbuffer) {
    VkCommandPool pool = find_pool(type);
    VkCommandBufferAllocateInfo cmd_buffer_allocate_info;
    cmd_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_allocate_info.pNext = NULL;
    cmd_buffer_allocate_info.commandPool = pool;
    cmd_buffer_allocate_info.level = level;
    cmd_buffer_allocate_info.commandBufferCount = count;

    VkResult result;
    result = vkAllocateCommandBuffers(vk_context.device.logical_device, &cmd_buffer_allocate_info, cmdbuffer);
    sx_assert_rel(result == VK_SUCCESS && "Could no create command buffer!");

    return result;
}
/*}}}*/

/* {{{VkResult create_descriptor_pool(DescriptorPoolInfo* info, VkDescriptorPool* pool) { */
VkResult create_descriptor_pool(DescriptorPoolInfo* info, VkDescriptorPool* pool) {
    VkDescriptorPoolCreateInfo descriptor_pool_create_info;
    descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_create_info.pNext = NULL;
    descriptor_pool_create_info.flags = 0;
    descriptor_pool_create_info.maxSets = info->max_sets;
    descriptor_pool_create_info.poolSizeCount = info->pool_size_count;
    descriptor_pool_create_info.pPoolSizes = info->pool_sizes;
    return vkCreateDescriptorPool(vk_context.device.logical_device, &descriptor_pool_create_info, NULL, pool);
}
/*}}}*/

/* {{{ VkResult create_descriptor_layout(DescriptorLayoutInfo* info, VkDescriptorSetLayout* layout) */
VkResult create_descriptor_layout(DescriptorLayoutInfo* info, VkDescriptorSetLayout* layout) {
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
    descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.pNext = NULL;
    descriptor_set_layout_create_info.flags = 0;
    descriptor_set_layout_create_info.bindingCount = info->num_bindings;
    descriptor_set_layout_create_info.pBindings = info->bindings;

    return vkCreateDescriptorSetLayout(vk_context.device.logical_device, &descriptor_set_layout_create_info, NULL,
                                        layout);
}
/* }}} */

/* {{{VkResult create_descriptor_sets(VkDescriptorPool pool, VkDescriptorSetLayout* layouts, uint32_t num_layouts, ...) */
VkResult create_descriptor_sets(VkDescriptorPool pool, VkDescriptorSetLayout* layouts, uint32_t num_layouts, VkDescriptorSet* sets) {
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
    descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.pNext = NULL;
    descriptor_set_allocate_info.descriptorPool = pool;
    descriptor_set_allocate_info.descriptorSetCount = num_layouts;
    descriptor_set_allocate_info.pSetLayouts = layouts;

    return vkAllocateDescriptorSets(vk_context.device.logical_device, &descriptor_set_allocate_info, sets);
}
/* }}} */

/*{{{void update_descriptor_set(DescriptorSetUpdateInfo* info)*/
void update_descriptor_set(DescriptorSetUpdateInfo* info) {
    uint32_t num_writes = info->num_buffer_bindings + info->num_image_bindings + info->num_texel_bindings;
    VkWriteDescriptorSet* descriptor_writes = sx_malloc(sx_alloc_malloc(), num_writes * sizeof(*descriptor_writes));

    uint32_t c = 0;
    for (uint32_t i = 0; i < info->num_buffer_bindings; i++) {
        descriptor_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[i].pNext = NULL;
        descriptor_writes[i].dstSet = info->descriptor_set;
        descriptor_writes[i].dstBinding = info->buffer_bindings[i];
        descriptor_writes[i].dstArrayElement = 0;
        descriptor_writes[i].descriptorCount = 1;
        descriptor_writes[i].descriptorType = info->buffer_descriptor_types[i];
        descriptor_writes[i].pBufferInfo = &info->buffer_infos[i];
        descriptor_writes[i].pImageInfo = NULL;
        descriptor_writes[i].pTexelBufferView = NULL;
        c++;
    }

    for (uint32_t i = c; i < info->num_image_bindings + c; i++) {
        descriptor_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[i].pNext = NULL;
        descriptor_writes[i].dstSet = info->descriptor_set;
        descriptor_writes[i].dstBinding = info->image_bindings[i];
        descriptor_writes[i].dstArrayElement = 0;
        descriptor_writes[i].descriptorCount = 1;
        descriptor_writes[i].descriptorType = info->image_descriptor_types[i];
        descriptor_writes[i].pImageInfo = &info->images_infos[i];
        descriptor_writes[i].pBufferInfo = NULL;
        descriptor_writes[i].pTexelBufferView = NULL;
        c++;
    }

    for (uint32_t i = c; i < info->num_texel_bindings + c; i++) {
        descriptor_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[i].pNext = NULL;
        descriptor_writes[i].dstSet = info->descriptor_set;
        descriptor_writes[i].dstBinding = info->texel_bindings[i];
        descriptor_writes[i].dstArrayElement = 0;
        descriptor_writes[i].descriptorCount = 1;
        descriptor_writes[i].descriptorType = info->texel_descriptor_types[i];
        descriptor_writes[i].pTexelBufferView = &info->texel_buffer_views[i];
        descriptor_writes[i].pBufferInfo = NULL;
        descriptor_writes[i].pImageInfo = NULL;
    }

    vkUpdateDescriptorSets(vk_context.device.logical_device, num_writes, descriptor_writes, 0, NULL);
    sx_free(sx_alloc_malloc(), descriptor_writes);
}
/*}}}*/

/* {{{VkPipelineShaderStageCreateInfo create_shader_module(const char* filnename, VkShaderStageFlagBits stage)*/
VkPipelineShaderStageCreateInfo create_shader_module(const char* filename, VkShaderStageFlagBits stage) {
    return load_shader(vk_context.device.logical_device, filename, stage);
}
/* }}}*/

/* {{{ VkResult create_pipeline_layout(PipelineLayoutInfo* info, VkPipeline* pipeline_layout);*/
VkResult create_pipeline_layout(PipelineLayoutInfo* info, VkPipelineLayout* pipeline_layout) {
    VkPipelineLayoutCreateInfo layout_create_info;
    layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_create_info.pNext = NULL;
    layout_create_info.flags = 0;
    layout_create_info.setLayoutCount = info->descriptor_layout_count;
    layout_create_info.pSetLayouts = info->descriptor_layouts;
    layout_create_info.pushConstantRangeCount = info->push_constant_count;
    layout_create_info.pPushConstantRanges = info->push_constant_ranges;

    return vkCreatePipelineLayout(vk_context.device.logical_device, &layout_create_info, NULL, pipeline_layout);
}
/*}}}*/

/*{{{ VkResult create_compute_pipeline(ComputePipelineInfo* info, VkPipeline* compute_pipeline) */
VkResult create_compute_pipeline(ComputePipelineInfo* info, VkPipeline* compute_pipeline) {
    const sx_alloc* alloc = sx_alloc_malloc();
    VkComputePipelineCreateInfo* create_info = sx_malloc(alloc, info->num_pipelines * sizeof(*create_info));
    
    for (uint32_t i = 0; i < info->num_pipelines; i++) {
        create_info[i].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        create_info[i].pNext = NULL;
        create_info[i].flags = 0;
        create_info[i].stage = info->shader[i];
        create_info[i].layout = info->layouts[i];
        create_info[i].basePipelineHandle = VK_NULL_HANDLE;
        create_info[i].basePipelineIndex = -1;
    }
    VkResult result =  vkCreateComputePipelines(vk_context.device.logical_device, VK_NULL_HANDLE, info->num_pipelines,
                                    create_info, NULL, compute_pipeline);
    sx_free(alloc, create_info);
    return result;
}
/*}}}*/

/*{{{VkResult create_fence(VkFence* fence) */
VkResult create_fence(VkFence* fence, bool begin_signaled) {
    VkFenceCreateInfo fence_create_info;
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.pNext = NULL;
    fence_create_info.flags = 0;
    if (begin_signaled)
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    return vkCreateFence(vk_context.device.logical_device, &fence_create_info, NULL, fence);
}
/*}}}*/

/*{{{VkResult wait_fences(uint32_t num_fences, VkFence* fence, bool wait_all, uint64_t timeout)*/
VkResult wait_fences(uint32_t num_fences, VkFence* fence, bool wait_all, uint64_t timeout) {
    VkBool32 wait = wait_all ? VK_TRUE : VK_FALSE;

    return vkWaitForFences(vk_context.device.logical_device, num_fences, fence, wait, timeout);
}
/*}}}*/

/*{{{VkResult submit_commands(CommandSubmitInfo* info) */
VkResult submit_commands(CommandSubmitInfo* info) {
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = info->wait_semaphores_count;
    submit_info.pWaitSemaphores = info->wait_semaphores;
    submit_info.pWaitDstStageMask = &info->wait_dst_stage_mask;
    submit_info.signalSemaphoreCount = info->signal_semaphores_count;
    submit_info.pSignalSemaphores = info->signal_semaphores;
    submit_info.commandBufferCount = info->command_buffers_count;
    submit_info.pCommandBuffers = info->command_buffers;
    return vkQueueSubmit(find_queue(info->queue), 1, &submit_info, info->fence);
}
/*}}}*/

/*{{{void destroy_fence(VkFence* fence)*/
void destroy_fence(VkFence fence) {
    vkDestroyFence(vk_context.device.logical_device, fence, NULL);
}
/*}}}*/

/*{{{void* map_buffer_memory(Buffer* buffer, VkDeviceSize offset)*/
void* map_buffer_memory(Buffer* buffer, VkDeviceSize offset) {
    void* mem = NULL;
    vkMapMemory(vk_context.device.logical_device, buffer->memory, offset, buffer->size, 0, &mem);
    return mem;
}
/*}}}*/

/*{{{void unmap_buffer_memory(Buffer* buffer)*/
void unmap_buffer_memory(Buffer* buffer) {
    vkUnmapMemory(vk_context.device.logical_device, buffer->memory);
}
/*}}}*/
