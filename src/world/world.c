#include "world/world.h"
#include "device/device.h"

/*{{{World* create_world(const sx_alloc* alloc, uint32_t width, uint32_t height)*/
World* create_world(const sx_alloc* alloc, uint32_t width, uint32_t height) {
    World* world = sx_malloc(alloc, sizeof(*world));
    world->alloc = alloc;

    world->renderer = create_renderer(world->alloc, width, height);
    world->gui = nkgui_create(world->alloc, world->renderer);

    return world;
}
/*}}}*/

/*{{{void world_init(World* world)*/
void world_init(World* world) {
    struct nk_font_atlas* atlas;

    struct nk_color table[NK_COLOR_COUNT];
    table[NK_COLOR_TEXT] = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_WINDOW] = nk_rgba(57, 67, 71, 215);
    table[NK_COLOR_HEADER] = nk_rgba(51, 51, 56, 220);
    table[NK_COLOR_BORDER] = nk_rgba(46, 46, 46, 255);
    table[NK_COLOR_BUTTON] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_BUTTON_HOVER] = nk_rgba(58, 93, 121, 255);
    table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(63, 98, 126, 255);
    table[NK_COLOR_TOGGLE] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 53, 56, 255);
    table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_SELECT] = nk_rgba(57, 67, 61, 255);
    table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_SLIDER] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(48, 83, 111, 245);
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
    table[NK_COLOR_PROPERTY] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_EDIT] = nk_rgba(50, 58, 61, 225);
    table[NK_COLOR_EDIT_CURSOR] = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_COMBO] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_CHART] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_CHART_COLOR] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
    table[NK_COLOR_SCROLLBAR] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
    table[NK_COLOR_TAB_HEADER] = nk_rgba(48, 83, 111, 255);
    /*nk_style_from_table(&world->gui->context, table);*/
    /*nk_style_default(&world->gui->context);*/
    nk_font_stash_begin(world->gui, &atlas);
    nk_font_stash_end(world->gui);

}
/*}}}*/

/*{{{void world_update(World* world, float dt)*/
void world_update(World* world, float dt) {
struct nk_context* ctx = &world->gui->context;

    nk_input_begin(ctx);
    sx_vec3 m = mouse_axis(device()->input_manager, MA_CURSOR);
    nk_input_motion(ctx, (int)m.x, (int)m.y);
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)m.x, (int)m.y, mouse_button(device()->input_manager, MB_LEFT));
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)m.x, (int)m.y, mouse_button(device()->input_manager, MB_MIDDLE));
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)m.x, (int)m.y, mouse_button(device()->input_manager, MB_RIGHT));
    nk_input_end(ctx);

    if (nk_begin(&world->gui->context, "Gabibits Wingsuit", nk_rect(50, 50, 200, 200),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {
        enum {EASY, HARD};
        static int op = EASY;

        nk_layout_row_static(&world->gui->context, 30, 80, 1);
        if (nk_button_label(&world->gui->context, "button"))
            /*fprintf(stdout, "button pressed\n");*/
        nk_layout_row_dynamic(&world->gui->context, 30, 2);
        if (nk_option_label(&world->gui->context, "easy", op == EASY)) op = EASY;
        if (nk_option_label(&world->gui->context, "hard", op == HARD)) op = HARD;
    }
    nk_end(&world->gui->context);

    if (nk_begin(&world->gui->context, "Gabibits Wingsui", nk_rect(200, 200, 200, 200),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {
        enum {EASY, HARD};
        static int op = EASY;

        nk_layout_row_static(&world->gui->context, 30, 80, 1);
        if (nk_button_label(&world->gui->context, "button"))
            fprintf(stdout, "button pressed\n");
        nk_layout_row_dynamic(&world->gui->context, 30, 2);
        if (nk_option_label(&world->gui->context, "easy", op == EASY)) 
            op = EASY;
        if (nk_option_label(&world->gui->context, "hard", op == HARD))
            op = HARD;
    }
    nk_end(&world->gui->context);
    
    nkgui_update(world->gui);

    renderer_render(world->renderer);

    nk_clear(&world->gui->context);
}
/*}}}*/

/*{{{void world_destroy(World* world)*/
void world_destroy(World* world) {
}
/*}}}*/

