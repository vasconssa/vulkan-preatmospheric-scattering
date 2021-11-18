#pragma once

#include <stdint.h>

#define VK_NO_STDINT_H
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#include "device/input_manager.h"
#include "world/camera.h"

typedef struct {
    InputManager* input_manager;
    FpsCamera camera;
    sx_rect viewport;
} Device;

int device_run(void* win, void* data);

Device* device();
