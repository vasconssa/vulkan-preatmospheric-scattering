#pragma once

#include <stdint.h>
#include "sx/allocator.h"
#include "world/renderer.h"
#include "world/nk_gui.h"
#include "world/sky.h"

typedef struct World {
    const sx_alloc* alloc;
    Renderer* renderer;
    NkGui* gui;
    Sky* sky;
    float cam_translation_speed;
} World;

World* create_world(const sx_alloc* alloc, uint32_t width, uint32_t height);
void world_init(World* world);
void world_update(World* world, float dt);
void world_destroy(World* world);
