#include "sx/platform.h"

#if SX_PLATFORM_LINUX && GB_USE_XCB
#include "macros.h"
#include "sx/sx.h"
#include "sx/os.h"
#include "sx/io.h"
#include "sx/allocator.h"
#include "sx/atomic.h"
#include "sx/threads.h"
#include "sx/string.h"
#include "device/types.h"
#include "device/device.h"
/*#include "world/renderer.h"*/
#include "renderer/vk_renderer.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_event.h>
#include <X11/keysym.h>
#include <time.h>

#define WIDTH 1280
#define HEIGHT 960
static KeyboardButton x11_translate_key(xcb_keysym_t x11_key)
{
	switch (x11_key)
	{
	case XK_BackSpace:    return KB_BACKSPACE;
	case XK_Tab:          return KB_TAB;
	case XK_space:        return KB_SPACE;
	case XK_Escape:       return KB_ESCAPE;
	case XK_Return:       return KB_ENTER;
	case XK_F1:           return KB_F1;
	case XK_F2:           return KB_F2;
	case XK_F3:           return KB_F3;
	case XK_F4:           return KB_F4;
	case XK_F5:           return KB_F5;
	case XK_F6:           return KB_F6;
	case XK_F7:           return KB_F7;
	case XK_F8:           return KB_F8;
	case XK_F9:           return KB_F9;
	case XK_F10:          return KB_F10;
	case XK_F11:          return KB_F11;
	case XK_F12:          return KB_F12;
	case XK_Home:         return KB_HOME;
	case XK_Left:         return KB_LEFT;
	case XK_Up:           return KB_UP;
	case XK_Right:        return KB_RIGHT;
	case XK_Down:         return KB_DOWN;
	case XK_Page_Up:      return KB_PAGE_UP;
	case XK_Page_Down:    return KB_PAGE_DOWN;
	case XK_Insert:       return KB_INS;
	case XK_Delete:       return KB_DEL;
	case XK_End:          return KB_END;
	case XK_Shift_L:      return KB_SHIFT_LEFT;
	case XK_Shift_R:      return KB_SHIFT_RIGHT;
	case XK_Control_L:    return KB_CTRL_LEFT;
	case XK_Control_R:    return KB_CTRL_RIGHT;
	case XK_Caps_Lock:    return KB_CAPS_LOCK;
	case XK_Alt_L:        return KB_ALT_LEFT;
	case XK_Alt_R:        return KB_ALT_RIGHT;
	case XK_Super_L:      return KB_SUPER_LEFT;
	case XK_Super_R:      return KB_SUPER_RIGHT;
	case XK_Num_Lock:     return KB_NUM_LOCK;
	case XK_KP_Enter:     return KB_NUMPAD_ENTER;
	case XK_KP_Delete:    return KB_NUMPAD_DELETE;
	case XK_KP_Multiply:  return KB_NUMPAD_MULTIPLY;
	case XK_KP_Add:       return KB_NUMPAD_ADD;
	case XK_KP_Subtract:  return KB_NUMPAD_SUBTRACT;
	case XK_KP_Divide:    return KB_NUMPAD_DIVIDE;
	case XK_KP_Insert:
	case XK_KP_0:         return KB_NUMPAD_0;
	case XK_KP_End:
	case XK_KP_1:         return KB_NUMPAD_1;
	case XK_KP_Down:
	case XK_KP_2:         return KB_NUMPAD_2;
	case XK_KP_Page_Down: // or XK_KP_Next
	case XK_KP_3:         return KB_NUMPAD_3;
	case XK_KP_Left:
	case XK_KP_4:         return KB_NUMPAD_4;
	case XK_KP_Begin:
	case XK_KP_5:         return KB_NUMPAD_5;
	case XK_KP_Right:
	case XK_KP_6:         return KB_NUMPAD_6;
	case XK_KP_Home:
	case XK_KP_7:         return KB_NUMPAD_7;
	case XK_KP_Up:
	case XK_KP_8:         return KB_NUMPAD_8;
	case XK_KP_Page_Up:   // or XK_KP_Prior
	case XK_KP_9:         return KB_NUMPAD_9;
	case '0':             return KB_NUMBER_0;
	case '1':             return KB_NUMBER_1;
	case '2':             return KB_NUMBER_2;
	case '3':             return KB_NUMBER_3;
	case '4':             return KB_NUMBER_4;
	case '5':             return KB_NUMBER_5;
	case '6':             return KB_NUMBER_6;
	case '7':             return KB_NUMBER_7;
	case '8':             return KB_NUMBER_8;
	case '9':             return KB_NUMBER_9;
	case 'a':             return KB_A;
	case 'b':             return KB_B;
	case 'c':             return KB_C;
	case 'd':             return KB_D;
	case 'e':             return KB_E;
	case 'f':             return KB_F;
	case 'g':             return KB_G;
	case 'h':             return KB_H;
	case 'i':             return KB_I;
	case 'j':             return KB_J;
	case 'k':             return KB_K;
	case 'l':             return KB_L;
	case 'm':             return KB_M;
	case 'n':             return KB_N;
	case 'o':             return KB_O;
	case 'p':             return KB_P;
	case 'q':             return KB_Q;
	case 'r':             return KB_R;
	case 's':             return KB_S;
	case 't':             return KB_T;
	case 'u':             return KB_U;
	case 'v':             return KB_V;
	case 'w':             return KB_W;
	case 'x':             return KB_X;
	case 'y':             return KB_Y;
	case 'z':             return KB_Z;
	default:              return KB_COUNT;
	}
}

typedef struct LinuxDevice {
    DeviceWindow window;
	xcb_atom_t wm_protocols;
	xcb_atom_t wm_delete_window;
	xcb_atom_t wm_state;
	xcb_atom_t net_wm_suported;
	xcb_atom_t net_wm_name;
	xcb_atom_t net_wm_state;
	xcb_atom_t net_wm_state_fullscreen;
    xcb_atom_t net_wm_state_maximized_horz;
    xcb_atom_t net_wm_state_maximized_vert;
    xcb_key_symbols_t* syms;
    /*Cursor x11_hidden_cursor;*/
    bool x11_detectable_autorepeat;
    /*XRRScreenConfiguration* screen_config;*/
    /*DeviceEventQueue _queue;*/
    /*Joypad _joypad;*/
    s16 mouse_last_x;
    s16 mouse_last_y;
    CursorMode cursor_mode;
} LinuxDevice;

static LinuxDevice linux_device;
static DeviceWindow win;
static int mouse_x;
static int mouse_y;
xcb_generic_error_t* err;

void _testerr(const char* file, const int line)
{
	if(err)
	{
		fprintf(stderr, "%s:%d - request returned error %i, \"%s\"\n", file, line,
			(int)err->error_code, xcb_event_get_error_label(err->error_code));
		free(err);
		err = NULL;
		assert(0);
	}
}

void _testcookie(DeviceWindow win, xcb_void_cookie_t cookie, const char* file, const int line)
{
	err = xcb_request_check(win.connection, cookie);
	_testerr(file, line);
}

#define testerr() _testerr(__FILE__, __LINE__);
#define testcookie(cookie) _testcookie(win, cookie, __FILE__, __LINE__);

static inline xcb_intern_atom_reply_t* intern_atom_helper(xcb_connection_t *conn, bool only_if_exists, const char *str)
{
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, only_if_exists, strlen(str), str);
	return xcb_intern_atom_reply(conn, cookie, NULL);
}

xcb_atom_t setup_atom(const char* name) {
	xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(win.connection, 0, strlen(name), name);
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(win.connection, atom_cookie, &err);
	xcb_atom_t atom;

	if(reply) {
		atom = reply->atom;
		free(reply);
	} else {
		testerr();
	}

	return atom;
}

// Initialize XCB connection
int init_xcb_connection(DeviceWindow* win) {
	const xcb_setup_t* setup;
	xcb_screen_iterator_t iter;
	int scr;

	win->connection = xcb_connect(NULL, &scr);
	if (win->connection == NULL) {
		printf("Could not find a compatible Vulkan ICD!\n");
		fflush(stdout);
		return 0;
	}

	setup = xcb_get_setup(win->connection);
	iter = xcb_setup_roots_iterator(setup);
	while (scr-- > 0)
		xcb_screen_next(&iter);
	win->screen = iter.data;
    return 1;
}

void setup_atoms() {
	linux_device.wm_protocols = setup_atom("WM_PROTOCOLS");
	linux_device.wm_delete_window = setup_atom("WM_DELETE_WINDOW");
	linux_device.wm_state = setup_atom("WM_STATE");
	linux_device.net_wm_suported = setup_atom("_NET_SUPPORTED");
	linux_device.net_wm_name = setup_atom("_NET_WM_NAME");
	linux_device.net_wm_state = setup_atom("_NET_WM_STATE");
	linux_device.net_wm_state_fullscreen = setup_atom("_NET_WM_STATE_FULLSCREEN");
}
// Set up a window using XCB and request event types
void setup_window(DeviceWindow* win, bool fullscreen) {
	uint32_t value_mask, value_list[32];

	win->window = xcb_generate_id(win->connection);

	value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	value_list[0] = win->screen->black_pixel;
	value_list[1] =
		XCB_EVENT_MASK_KEY_RELEASE |
		XCB_EVENT_MASK_KEY_PRESS |
		XCB_EVENT_MASK_EXPOSURE |
		XCB_EVENT_MASK_STRUCTURE_NOTIFY |
		XCB_EVENT_MASK_POINTER_MOTION |
		XCB_EVENT_MASK_BUTTON_PRESS |
		XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_ENTER_WINDOW;

	if (fullscreen)
	{
		win->width = win->screen->width_in_pixels;
		win->height = win->screen->height_in_pixels;
	}

	xcb_create_window(win->connection,
		XCB_COPY_FROM_PARENT,
		win->window, win->screen->root,
		0, 0, win->width, win->height, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		win->screen->root_visual,
		value_mask, value_list);

	/* Magic code that will send notification when window is destroyed */
    setup_atoms();

	xcb_change_property(win->connection, XCB_PROP_MODE_REPLACE,
		win->window, linux_device.wm_protocols, 4, 32, 1,
		&linux_device.wm_delete_window);

	char* windowTitle = "Gabibits Wingsuit";
	xcb_change_property(win->connection, XCB_PROP_MODE_REPLACE,
		win->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
		sx_strlen(windowTitle), windowTitle);

	if (fullscreen)
	{
		xcb_intern_atom_reply_t *atom_wm_state = intern_atom_helper(win->connection, false, "_NET_WM_STATE");
		xcb_intern_atom_reply_t *atom_wm_fullscreen = intern_atom_helper(win->connection, false, "_NET_WM_STATE_FULLSCREEN");
		xcb_change_property(win->connection,
				XCB_PROP_MODE_REPLACE,
				win->window, atom_wm_state->atom,
				XCB_ATOM_ATOM, 32, 1,
				&(atom_wm_fullscreen->atom));
		free(atom_wm_fullscreen);
		free(atom_wm_state);
	}	

	linux_device.syms = xcb_key_symbols_alloc(win->connection);

	xcb_map_window(win->connection, win->window);
}

void create_window(u_int16_t width, u_int16_t height) {
    win.width = width;
    win.height = height;
    setup_window(&win, false);
    xcb_flush(win.connection);
}

bool s_exit = false;
sx_queue_spsc* event_queue = NULL;

int run() {
    init_xcb_connection(&win);
    xcb_flush(win.connection);
	xcb_generic_event_t *event;

    const sx_alloc* alloc = sx_alloc_malloc();
    event_queue = sx_queue_spsc_create(alloc, sizeof(OsEvent), 1024);

    linux_device.cursor_mode = CM_NORMAL;
    create_window(WIDTH, HEIGHT);

    if(!vk_renderer_init(win)) {
        printf("erro\n");
        return -1;
    }
    /*struct nk_context* ctx = nk_gui_init();*/
    /*struct nk_context* ctx = NULL;*/

    sx_thread* thrd = sx_thread_create(alloc, device_run, &win, 0, "RenderThread", NULL);
    if (!thrd) {
        return -1;
    }
    while (!s_exit) {
		event = xcb_poll_for_event(win.connection);
        OsEvent ev_item;
        if (!event) {
            sx_os_sleep(8);
        } else {
            switch (event->response_type & ~0x80) {
                case XCB_ENTER_NOTIFY:
                {
                    xcb_enter_notify_event_t* ev = (xcb_enter_notify_event_t*)event;
					linux_device.mouse_last_x = (s16)ev->event_x;
					linux_device.mouse_last_y = (s16)ev->event_y;
                    ev_item.axis.type = OST_AXIS;
                    ev_item.axis.device_id = IDT_MOUSE;
                    ev_item.axis.device_num = 0;
                    ev_item.axis.axis_num = MA_CURSOR;
                    ev_item.axis.axis_x = ev->event_x;
                    ev_item.axis.axis_y = ev->event_y;
                    ev_item.axis.axis_z = 0;
                    sx_queue_spsc_produce(event_queue, &ev_item);

                }
                break;
                case XCB_CLIENT_MESSAGE:
                {
                    xcb_client_message_event_t* ev = (xcb_client_message_event_t*)event;
                    if (ev->data.data32[0] == linux_device.wm_delete_window) {
                        ev_item.type = OST_EXIT;
                        s_exit = true;
                        sx_queue_spsc_produce(event_queue, &ev_item);
                    }
                }
                break;
                case XCB_MOTION_NOTIFY:
                {
                    xcb_get_geometry_cookie_t geom_cookie = xcb_get_geometry(win.connection, win.window);
                    xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)event;
                    const s32 mx = motion->event_x;
                    const s32 my = motion->event_y;
                    s16 deltax = mx - linux_device.mouse_last_x;
                    s16 deltay = my - linux_device.mouse_last_y;
                    if (linux_device.cursor_mode == CM_DISABLED)
                    {
                        xcb_get_geometry_reply_t* geom_reply = xcb_get_geometry_reply(win.connection, geom_cookie, &err);
                        unsigned width = geom_reply->width;
                        unsigned height = geom_reply->height;
                        if (mx != (s32)width/2 || my != (s32)height/2)
                        {
                            ev_item.axis.type = OST_AXIS;
                            ev_item.axis.device_id = IDT_MOUSE;
                            ev_item.axis.device_num = 0;
                            ev_item.axis.axis_num = MA_CURSOR_DELTA;
                            ev_item.axis.axis_x = deltax;
                            ev_item.axis.axis_y = deltay;
                            ev_item.axis.axis_z = 0;
                            sx_queue_spsc_produce(event_queue, &ev_item);
                            xcb_warp_pointer(win.connection
                                , XCB_WINDOW_NONE
                                , win.window
                                , 0
                                , 0
                                , 0
                                , 0
                                , width/2
                                , height/2
                                );
                            xcb_flush(win.connection);
                        }
                    }
                    else if (linux_device.cursor_mode == CM_NORMAL)
                    {
                        ev_item.axis.type = OST_AXIS;
                        ev_item.axis.device_id = IDT_MOUSE;
                        ev_item.axis.device_num = 0;
                        ev_item.axis.axis_num = MA_CURSOR_DELTA;
                        ev_item.axis.axis_x = deltax;
                        ev_item.axis.axis_y = deltay;
                        ev_item.axis.axis_z = 0;
                        sx_queue_spsc_produce(event_queue, &ev_item);
                    }
                    ev_item.axis.type = OST_AXIS;
                    ev_item.axis.device_id = IDT_MOUSE;
                    ev_item.axis.device_num = 0;
                    ev_item.axis.axis_num = MA_CURSOR;
                    ev_item.axis.axis_x = (uint16_t)mx;
                    ev_item.axis.axis_y = (uint16_t)my;
                    ev_item.axis.axis_z = 0;
                    sx_queue_spsc_produce(event_queue, &ev_item);
                    linux_device.mouse_last_x = (uint16_t)mx;
                    linux_device.mouse_last_y = (uint16_t)my;

                    break;
                }
                break;
                case XCB_BUTTON_PRESS:
                case XCB_BUTTON_RELEASE:
                {
                    xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
                    if (press->detail == XCB_BUTTON_INDEX_4 || 
                        press->detail == XCB_BUTTON_INDEX_5)
                    {
                        ev_item.axis.type = OST_AXIS;
                        ev_item.axis.device_id = IDT_MOUSE;
                        ev_item.axis.device_num = 0;
                        ev_item.axis.axis_num = MA_WHEEL;
                        ev_item.axis.axis_x = 0;
                        ev_item.axis.axis_y = XCB_BUTTON_INDEX_4 ? 1 : -1;
                        ev_item.axis.axis_z = 0;
                        sx_queue_spsc_produce(event_queue, &ev_item);
                        break;
                    }

                    MouseButton mb;
                    switch (press->detail)
                    {
                    case XCB_BUTTON_INDEX_1: mb = MB_LEFT; break;
                    case XCB_BUTTON_INDEX_2: mb = MB_MIDDLE; break;
                    case XCB_BUTTON_INDEX_3: mb = MB_RIGHT; break;
                    default: mb = MB_COUNT; break;
                    }
                    if (mb != MB_COUNT)
                    {
                        ev_item.button.type = OST_BUTTON;
                        ev_item.button.device_id = IDT_MOUSE;
                        ev_item.button.device_num = 0;
                        ev_item.button.button_num = mb;
                        ev_item.button.pressed = event->response_type == XCB_BUTTON_PRESS;
                        sx_queue_spsc_produce(event_queue, &ev_item);
                    }
                }
                break;
                case XCB_KEY_PRESS:
                case XCB_KEY_RELEASE:
                {
                    xcb_key_press_event_t *ev = (xcb_key_press_event_t*) event;
                    xcb_keysym_t keysym = xcb_key_press_lookup_keysym(linux_device.syms, ev, 0);
                    KeyboardButton kb = x11_translate_key(keysym);
                    if (kb != KB_COUNT) {
                        ev_item.button.type = OST_BUTTON;
                        ev_item.button.device_id = IDT_KEYBOARD;
                        ev_item.button.device_num = 0;
                        ev_item.button.button_num = kb;
                        ev_item.button.pressed = event->response_type == XCB_KEY_PRESS;
                        sx_queue_spsc_produce(event_queue, &ev_item);
                    }
                }
                break;
                case XCB_DESTROY_NOTIFY:
                    break;
                case XCB_CONFIGURE_NOTIFY:
                {
                    const xcb_configure_notify_event_t *cfg_event = (const xcb_configure_notify_event_t *)event;
                    if ( cfg_event->width != win.width || cfg_event->height != win.height) {
                        win.width = cfg_event->width;
                        win.height = cfg_event->height;
                        ev_item.resolution.type = OST_RESOLUTION;
                        ev_item.resolution.width = cfg_event->width;
                        ev_item.resolution.height = cfg_event->height;
                        sx_queue_spsc_produce(event_queue, &ev_item);
                    }

                }
                break;
                default:
                    break;
            }
        free(event);
        }
	}

    sx_thread_destroy(thrd, alloc);
    sx_queue_spsc_destroy(event_queue, alloc);
    return 0;
}

bool next_event(OsEvent* ev) {
    return sx_queue_spsc_consume(event_queue, ev);
}


int main(int argc, char** argv) {
    GB_UNUSED(argc);
    GB_UNUSED(argv);
    /*init_xcb_connection(&win);*/
    /*create_window(WIDTH, HEIGHT);*/
    run();
    /*device_run(&win, NULL);*/

    return 0;
}
#endif
