#include "device/input_manager.h"
#include "sx/string.h"
#include "macros.h"


InputDevice* create_device(const sx_alloc* alloc, const char* name, uint8_t num_buttons, uint8_t num_axes) {
    uint32_t size = 0
        + sizeof(InputDevice) + alignof(InputDevice)
        + sizeof(uint8_t)*num_buttons*2 + alignof(uint8_t)
        + sizeof(sx_vec3)*num_axes + alignof(sx_vec3)
        + sizeof(uint32_t)*num_axes + alignof(uint32_t)
        + sizeof(float)*num_axes + alignof(float)
        + sx_strlen(name) + 1 + alignof(char)
        ;

    InputDevice* id = (InputDevice*)sx_malloc(alloc, size);

    id->connected       = false;
    id->num_buttons     = num_buttons;
    id->num_axes        = num_axes;
    id->first_button[0] = UINT8_MAX;
    id->first_button[1] = UINT8_MAX;

    id->last_state    = (uint8_t*   )&id[1];
    id->state         = (uint8_t*   )sx_align_ptr((void*)(id->last_state + num_buttons), 0, alignof(uint8_t  ));
    id->axis          = (sx_vec3*   )sx_align_ptr((void*)(id->state + num_buttons),      0, alignof(sx_vec3  ));
    id->deadzone_mode = (uint32_t*  )sx_align_ptr((void*)(id->axis + num_axes),          0, alignof(uint32_t ));
    id->deadzone_size = (float*     )sx_align_ptr((void*)(id->deadzone_mode + num_axes), 0, alignof(float    ));
    id->name          = (char*      )sx_align_ptr((void*)(id->deadzone_size + num_axes), 0, alignof(char     ));

    memset(id->last_state, 0, sizeof(uint8_t)*num_buttons);
    memset(id->state, 0, sizeof(uint8_t)*num_buttons);
    memset(id->axis, 0, sizeof(sx_vec3)*num_axes);
    memset(id->deadzone_mode, 0, sizeof(*id->deadzone_mode)*num_axes);
    memset(id->deadzone_size, 0, sizeof(*id->deadzone_size)*num_axes);


    strcpy(id->name, name);

    return id;
}
void destroy_device(const sx_alloc* alloc, InputDevice* device) {
    sx_free(alloc, device);
}

void set_button(InputDevice* device, uint8_t id, uint8_t state) {
    sx_assert(id < device->num_buttons);
    device->state[id] = state;
    if (device->first_button[state % 2] == UINT8_MAX) {
        device->first_button[state % 2] = id;
    }
}

void set_axis(InputDevice* device, uint8_t id, float x, float y, float z) {
    sx_assert(id < device->num_axes);
    device->axis[id].x = x;
    device->axis[id].y = y;
    device->axis[id].z = z;

}
void set_deadzone(InputDevice* device, uint8_t id, DeadzoneMode mode, float deadzone_size) {
    if (id < device->num_axes) {
        device->deadzone_mode[id] = mode;
        device->deadzone_size[id] = deadzone_size;
    }
}

bool pressed(InputDevice* device, uint8_t id) {
    bool pressed = id < device->num_buttons ? (~device->last_state[id] & device->state[id]) != 0 : false;
    return pressed;
}

bool released(InputDevice* device, uint8_t id) {
    bool pressed = id < device->num_buttons ? (device->last_state[id] & ~device->state[id]) != 0 : false;
    return pressed;
}

float button(InputDevice* device, uint8_t id) {
    float value = id < device->num_buttons ? (float)(device->state[id]) : 0;
    return value;
}

sx_vec3 axis(InputDevice* device, uint8_t id) {
    if (id >= device->num_axes)
        return SX_VEC3_ZERO;

    sx_vec3 axis = device->axis[id];
    switch (device->deadzone_mode[id])
    {
        case DZM_RAW:
            // No deadzone
            break;

        case DZM_INDEPENDENT:
            axis.x = sx_abs(axis.x) < device->deadzone_size[id] ? 0.0f : axis.x;
            axis.y = sx_abs(axis.y) < device->deadzone_size[id] ? 0.0f : axis.y;
            axis.z = sx_abs(axis.z) < device->deadzone_size[id] ? 0.0f : axis.z;
            break;

        case DZM_CIRCULAR:
            if (sx_vec3_len(axis) < device->deadzone_size[id])
            {
                axis = SX_VEC3_ZERO;
            }
            else
            {
                const float size = 1.0f - device->deadzone_size[id];
                const float size_inv = 1.0f / size;
                const float axis_len = sx_vec3_len(axis);
                axis = sx_vec3_mulf(sx_vec3_norm(axis), (axis_len - device->deadzone_size[id]) * size_inv);
            }
            break;

        default:
            /*CE_FATAL("Unknown deadzone mode");*/
            break;
    }

    return axis;
}


void update(InputDevice* input_device) {
	sx_memcpy(input_device->last_state, input_device->state, sizeof(u8)*input_device->num_buttons);
	input_device->first_button[0] = UINT8_MAX;
	input_device->first_button[1] = UINT8_MAX;
}

InputManager* create_input_manager(const sx_alloc* allocator) {
    InputManager* input_manager = sx_malloc(allocator, sizeof(InputManager));
    input_manager->mouse_last_x = INT16_MAX;
    input_manager->mouse_last_y = INT16_MAX;
    input_manager->has_delta_axis_event = false;

    const char keyboard[] = "Keyboard";
    const char mouse[] = "Mouse";
    const char touch[] = "Touch";
    input_manager->keyboard = create_device(allocator
            , keyboard
            , KB_COUNT
            , 0
            );
    input_manager->mouse = create_device(allocator
            , mouse
            , MB_COUNT
            , MA_COUNT
            );
    input_manager->touch = create_device(allocator
            , touch
            , TB_COUNT
            , TA_COUNT
            );

    input_manager->keyboard->connected = true;
    input_manager->mouse->connected = true;
    input_manager->touch->connected = true;

    return input_manager;
}

void input_manager_read(InputManager* input_manager, OsEvent* event) {
    switch (event->type)
    {
        case OST_BUTTON:
            {
                ButtonEvent ev = event->button;
                switch (ev.device_id)
                {
                    case IDT_KEYBOARD:
                        set_button(input_manager->keyboard, ev.button_num, ev.pressed);
                        break;

                    case IDT_MOUSE:
                        set_button(input_manager->mouse, ev.button_num, ev.pressed);
                        break;

                    case IDT_TOUCHSCREEN:
                        set_button(input_manager->touch, ev.button_num, ev.pressed);
                        break;

                        /*case IDT_JOYPAD:*/
                        /*set_button(input_manager->joypad, ev.button_num, ev.pressed);*/
                        /*break;*/
                }
            }
            break;

        case OST_AXIS:
            {
                AxisEvent ev = event->axis;
                switch (ev.device_id)
                {
                    case IDT_MOUSE:
                        if (ev.axis_num == MA_CURSOR_DELTA)
                        {
                            const sx_vec3 delta = input_manager->has_delta_axis_event ? axis(input_manager->mouse, MA_CURSOR_DELTA) : SX_VEC3_ZERO;
                            set_axis(input_manager->mouse
                                    , MA_CURSOR_DELTA
                                    , delta.x + ev.axis_x
                                    , delta.y + ev.axis_y
                                    , 0
                                    );
                            input_manager->has_delta_axis_event = true;
                        }
                        else
                            set_axis(input_manager->mouse, ev.axis_num, ev.axis_x, ev.axis_y, ev.axis_z);

                        break;

                        /*case InputDeviceType::JOYPAD:*/
                        /*_joypad[ev.device_num]->set_axis(ev.axis_num*/
                        /*, (f32)ev.axis_x / (f32)INT16_MAX*/
                        /*, (f32)ev.axis_y / (f32)INT16_MAX*/
                        /*, (f32)ev.axis_z / (f32)INT16_MAX*/
                        /*);*/
                        /*break;*/
                }
            }
            break;

            /*case OsEventType::STATUS:*/
            /*{*/
            /*const StatusEvent ev = event->status;*/
            /*switch (ev.device_id)*/
            /*{*/
            /*case InputDeviceType::JOYPAD:*/
            /*_joypad[ev.device_num]->_connected = ev.connected;*/
            /*break;*/
            /*}*/
            /*}*/
            /*break;*/

        default:
            /*CE_FATAL("Unknown input event type");*/
            break;
    }
}

void input_manager_update(InputManager* input_manager) {

    update(input_manager->keyboard);

	if (!input_manager->has_delta_axis_event) {
        set_axis(input_manager->mouse, MA_CURSOR_DELTA, 0, 0, 0);
	}
	input_manager->has_delta_axis_event = false;
    update(input_manager->mouse);

    update(input_manager->touch);

	/*for (u8 i = 0; i < CROWN_MAX_JOYPADS; ++i)*/
        /*update(input_manager->keyboard[i]);*/
}

bool keyboard_button_pressed(InputManager* input_manager, uint8_t id) {
    return pressed(input_manager->keyboard, id);
}

bool keyboard_button_released(InputManager* input_manager, uint8_t id) {
    return released(input_manager->keyboard, id);
}

float keyboard_button(InputManager* input_manager, uint8_t id) {
    return button(input_manager->keyboard, id);
}

bool mouse_button_pressed(InputManager* input_manager, uint8_t id) {
    return pressed(input_manager->mouse, id);
}

bool mouse_button_released(InputManager* input_manager, uint8_t id) {
    return released(input_manager->mouse, id);
}

float mouse_button(InputManager* input_manager, uint8_t id) {
    return button(input_manager->mouse, id);
}

sx_vec3 mouse_axis(InputManager* input_manager, uint8_t id) {
    return axis(input_manager->mouse, id);
}

bool touch_button_pressed(InputManager* input_manager, uint8_t id) {
    return pressed(input_manager->touch, id);
}

bool touch_button_released(InputManager* input_manager, uint8_t id) {
    return released(input_manager->touch, id);
}

float touch_button(InputManager* input_manager, uint8_t id) {
    return button(input_manager->touch, id);
}

sx_vec3 touch_axis(InputManager* input_manager, uint8_t id) {
    return axis(input_manager->touch, id);
}
