#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "device/types.h"
#include "sx/math.h"
#include "sx/allocator.h"

typedef struct {
    bool connected;
    uint8_t num_buttons;
    uint8_t num_axes;
    uint8_t first_button[2];

    uint8_t* last_state;          // num_buttons
    uint8_t* state;               // num_buttons
    sx_vec3* axis;                // num_axes
    uint32_t* deadzone_mode;      // num_axes
    float* deadzone_size;      // num_axes
    char* name;                   // strlen32(name) + 1
} InputDevice;

typedef struct {
    InputDevice* keyboard;
    InputDevice* mouse;
    InputDevice* touch;
    int16_t mouse_last_x;
    int16_t mouse_last_y;
    bool has_delta_axis_event;
}InputManager;

extern InputManager* create_input_manager(const sx_alloc* allocator);

extern void    input_manager_read(InputManager* input_manager, OsEvent* ev);
extern void    input_manager_update(InputManager* input_manager);
extern bool    keyboard_button_pressed(InputManager* input_manager, uint8_t id);
extern bool    keyboard_button_released(InputManager* input_manager, uint8_t id);
extern float   keyboard_button(InputManager* input_manager, uint8_t id);
extern bool    mouse_button_pressed(InputManager* input_manager, uint8_t id);
extern bool    mouse_button_released(InputManager* input_manager, uint8_t id);
extern float   mouse_button(InputManager* input_manager, uint8_t id);
extern sx_vec3 mouse_axis(InputManager* input_manager, uint8_t id);
extern bool    touch_button_pressed(InputManager* input_manager, uint8_t id);
extern bool    touch_button_released(InputManager* input_manager, uint8_t id);
extern float   touch_button(InputManager* input_manager, uint8_t id);
extern sx_vec3 touch_axis(InputManager* input_manager, uint8_t id);

