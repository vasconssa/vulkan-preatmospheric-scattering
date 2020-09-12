#pragma once
#include <stdint.h>

#if defined(GB_USE_XCB)
#include <xcb/xcb.h>
#elif defined(GB_USE_XLIB)
#include <X11/Xlib.h>
#endif

typedef struct DeviceWindow {
#if defined(GB_USE_XCB)
    xcb_connection_t* connection;
    xcb_screen_t* screen;
    xcb_window_t window;
#elif defined(GB_USE_XLIB)
    Display* connection;
    Window window;
#endif
    uint32_t width;
    uint32_t height;
} DeviceWindow;

typedef enum MouseCursor
{
    MC_ARROW,
    MC_HAND,
    MC_TEXT_INPUT,
    MC_CORNER_TOP_LEFT,
    MC_CORNER_TOP_RIGHT,
    MC_CORNER_BOTTOM_LEFT,
    MC_CORNER_BOTTOM_RIGHT,
    MC_SIZE_HORIZONTAL,
    MC_SIZE_VERTICAL,
    MC_WAIT,

    MC_COUNT
} MouseCursor;

typedef enum CursorMode
{
    CM_NORMAL,
    CM_DISABLED,

    CM_COUNT
} CursorMode;

typedef enum OsEventType
{
    OST_BUTTON,
    OST_AXIS,
    OST_STATUS,
    OST_RESOLUTION,
    OST_EXIT,
    OST_PAUSE,
    OST_RESUME,
    OST_TEXT
} OsEventType;

typedef struct ButtonEvent
{
	uint16_t type;
	uint16_t device_id  : 3;
	uint16_t device_num : 2;
	uint16_t button_num : 8;
	uint16_t pressed    : 1;
} ButtonEvent;

typedef struct AxisEvent
{
	uint16_t type;
	uint16_t device_id  : 3;
	uint16_t device_num : 2;
	uint16_t axis_num   : 4;
	int16_t axis_x;
	int16_t axis_y;
	int16_t axis_z;
} AxisEvent;

typedef struct StatusEvent
{
	uint16_t type;
	uint16_t device_id  : 3;
	uint16_t device_num : 2;
	uint16_t connected  : 1;
} StatusEvent;

typedef struct ResolutionEvent
{
	uint16_t type;
	uint16_t width;
	uint16_t height;
} ResolutionEvent;

typedef struct TextEvent
{
	uint16_t type;
	uint8_t len;
	uint8_t utf8[4];
} TextEvent;

typedef union OsEvent
{
	uint16_t type;
	ButtonEvent button;
	AxisEvent axis;
	StatusEvent status;
	ResolutionEvent resolution;
	TextEvent text;
} OsEvent;

typedef enum InputDeviceType
{
    IDT_KEYBOARD,
    IDT_MOUSE,
    IDT_TOUCHSCREEN,
    IDT_JOYPAD
} InputDeviceType;

/// Enumerates keyboard buttons.
///
/// @ingroup Input
typedef enum KeyboardButton
{
    KB_TAB,
    KB_ENTER,
    KB_ESCAPE,
    KB_SPACE,
    KB_BACKSPACE,

    /* Numpad */
    KB_NUM_LOCK,
    KB_NUMPAD_ENTER,
    KB_NUMPAD_DELETE,
    KB_NUMPAD_MULTIPLY,
    KB_NUMPAD_ADD,
    KB_NUMPAD_SUBTRACT,
    KB_NUMPAD_DIVIDE,
    KB_NUMPAD_0,
    KB_NUMPAD_1,
    KB_NUMPAD_2,
    KB_NUMPAD_3,
    KB_NUMPAD_4,
    KB_NUMPAD_5,
    KB_NUMPAD_6,
    KB_NUMPAD_7,
    KB_NUMPAD_8,
    KB_NUMPAD_9,

    /* Function keys */
    KB_F1,
    KB_F2,
    KB_F3,
    KB_F4,
    KB_F5,
    KB_F6,
    KB_F7,
    KB_F8,
    KB_F9,
    KB_F10,
    KB_F11,
    KB_F12,

    /* Other keys */
    KB_HOME,
    KB_LEFT,
    KB_UP,
    KB_RIGHT,
    KB_DOWN,
    KB_PAGE_UP,
    KB_PAGE_DOWN,
    KB_INS,
    KB_DEL,
    KB_END,

    /* Modifier keys */
    KB_CTRL_LEFT,
    KB_CTRL_RIGHT,
    KB_SHIFT_LEFT,
    KB_SHIFT_RIGHT,
    KB_CAPS_LOCK,
    KB_ALT_LEFT,
    KB_ALT_RIGHT,
    KB_SUPER_LEFT,
    KB_SUPER_RIGHT,

    KB_NUMBER_0,
    KB_NUMBER_1,
    KB_NUMBER_2,
    KB_NUMBER_3,
    KB_NUMBER_4,
    KB_NUMBER_5,
    KB_NUMBER_6,
    KB_NUMBER_7,
    KB_NUMBER_8,
    KB_NUMBER_9,

    KB_A,
    KB_B,
    KB_C,
    KB_D,
    KB_E,
    KB_F,
    KB_G,
    KB_H,
    KB_I,
    KB_J,
    KB_K,
    KB_L,
    KB_M,
    KB_N,
    KB_O,
    KB_P,
    KB_Q,
    KB_R,
    KB_S,
    KB_T,
    KB_U,
    KB_V,
    KB_W,
    KB_X,
    KB_Y,
    KB_Z,

    KB_COUNT
} KeyboardButton;

/// Enumerates mouse buttons.
///
/// @ingroup Input
typedef enum MouseButton
{
    MB_LEFT,
    MB_MIDDLE,
    MB_RIGHT,
    MB_EXTRA_1,
    MB_EXTRA_2,
    MB_COUNT
} MouseButton;

/// Enumerates mouse axes.
///
/// @ingroup Input
typedef enum MouseAxis
{
    MA_CURSOR,
    MA_CURSOR_DELTA,
    MA_WHEEL,
    MA_COUNT
} MouseAxis;

/// Enumerates touch panel buttons.
///
/// @ingroup Input
typedef enum TouchButton
{
    TB_POINTER_0,
    TB_POINTER_1,
    TB_POINTER_2,
    TB_POINTER_3,
    TB_COUNT
} TouchButton;

/// Enumerates touch panel axes.
///
/// @ingroup Input
typedef enum TouchAxis
{
    TA_POINTER_0,
    TA_POINTER_1,
    TA_POINTER_2,
    TA_POINTER_3,
    TA_COUNT
} TouchAxis;

/// Enumerates joypad buttons.
///
/// @ingroup Input
typedef enum JoypadButton
{
    JB_UP,
    JB_DOWN,
    JB_LEFT,
    JB_RIGHT,
    JB_START,
    JB_BACK,
    JB_GUIDE,
    JB_THUMB_LEFT,
    JB_THUMB_RIGHT,
    JB_SHOULDER_LEFT,
    JB_SHOULDER_RIGHT,
    JB_A,
    JB_B,
    JB_X,
    JB_Y,
    JB_COUNT
} JoypadButton;

/// Enumerates joypad axes.
///
/// @ingroup Input
typedef enum JoypadAxis
{
    JA_LEFT,
    JA_RIGHT,
    JA_TRIGGER_LEFT,
    JA_TRIGGER_RIGHT,
    JA_COUNT
} JoypadAxis;

/// Enumerates deadzone modes.
///
/// @ingroup Input
typedef enum DeadzoneMode
{
    DZM_RAW,         ///< No deadzone.
    DZM_INDEPENDENT, ///< The deadzone is applied on each axis independently.
    DZM_CIRCULAR,    ///< The deadzone value is interpreted as the radius of a circle.
    DZM_COUNT
} DeadzoneMode;


