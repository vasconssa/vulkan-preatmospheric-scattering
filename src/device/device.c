#include "device/device.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "macros.h"
#include "device/types.h"
#include "device/input_manager.h"
#include "sx/timer.h"
#include <time.h>
#include <limits.h>
#include <math.h>
#include "renderer/vk_renderer.h"
#include "renderer/mandelbrot.h"


Device game_device;

extern bool next_event(OsEvent* ev);
extern void create_window(uint16_t width, uint16_t height);
/*extern void set_fullscreen(bool full);*/
/*extern void set_title (const char* title);*/

bool process_events(InputManager* input_manager, bool vsync) {
    bool exit = false;
    bool reset = false;
    OsEvent event;
	while (next_event(&event))
	{
		switch (event.type)
		{
		case OST_BUTTON:
		case OST_AXIS:
		case OST_STATUS:
            input_manager_read(input_manager, &event);
			break;

		case OST_RESOLUTION:
			reset   = true;
            /*game_device.viewport.xmax = event.resolution.width;*/
            /*game_device.viewport.ymax = event.resolution.height;*/
            /*game_device.camera.cam.viewport = game_device.viewport;*/
            /*renderer_resize(world->renderer, event.resolution.width, event.resolution.height);*/
			break;

		case OST_EXIT:
			exit = true;
			break;

		case OST_PAUSE:
			/*pause();*/
			break;

		case OST_RESUME:
			/*unpause();*/
			break;

		case OST_TEXT:
			break;

		default:
			break;
		}
	}

    return exit;
}

int device_run(void* win, void* data) {
    sx_tm_init();
    uint64_t last_time = 0;
    DeviceWindow* window = (DeviceWindow*)win;
    game_device.viewport.xmin = 0.0;
    game_device.viewport.xmax = window->width;
    game_device.viewport.ymin = 0.0;
    game_device.viewport.ymax = window->height;

    const sx_alloc* alloc = sx_alloc_malloc();

    /*World* world = create_world(alloc);*/
    game_device.input_manager = create_input_manager(alloc);

    /*game_device.camera.cam.pos.x = 15.2;*/
    /*game_device.camera.cam.pos.y = 16.5;*/
    /*game_device.camera.cam.pos.z = 17.4;*/

    float dt = 0;
    /*game_init(world);*/
    vk_renderer_init(*window);
    MandelbrotInfo info;
    compute_mandelbrot(&info);
    printf("add: %p\n", &info);
    save_image(&info);
    printf("thread finished\n");
    fflush(stdout);
    /*while(!process_events(game_device.input_manager, false)) {*/
        /*sx_vec3 m = mouse_axis(game_device.input_manager, MA_CURSOR);*/
        /*dt = (float)sx_tm_sec(sx_tm_laptime(&last_time));*/
        /*fps_camera_update(&game_device.camera, dt);*/
        /*world_update(world, dt);*/
        /*input_manager_update(game_device.input_manager);*/
        /*printf("fps: %lf\n", 1.0/dt);*/
        /*[>dt2 = sx_tm_sec(sx_tm_diff(t2, t));<]*/
        /*printf("cam.x: %f, cam.y: %f, cam.z: %f\n", game_device.camera.cam.pos.x, game_device.camera.cam.pos.y, game_device.camera.cam.pos.z);*/
    /*}*/
    /*world_destroy(world);*/
    sx_free(alloc, game_device.input_manager->keyboard);
    sx_free(alloc, game_device.input_manager->mouse);
    sx_free(alloc, game_device.input_manager->touch);
    sx_free(alloc, game_device.input_manager);


    return 0;
}

Device* device() {
    return &game_device;
}
