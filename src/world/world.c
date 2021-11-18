#include "world/world.h"
#include "device/device.h"
#include "sx/math.h"

/*{{{World* create_world(const sx_alloc* alloc, uint32_t width, uint32_t height)*/
World* create_world(const sx_alloc* alloc, uint32_t width, uint32_t height) {
    World* world = sx_malloc(alloc, sizeof(*world));
    world->alloc = alloc;
    world->cam_translation_speed = 5000.f;

    world->renderer = create_renderer(world->alloc, width, height);
    world->sky = sky_create(alloc, world->renderer);
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
    nk_style_from_table(&world->gui->context, table);
    /*nk_style_default(&world->gui->context);*/
    nk_font_stash_begin(world->gui, &atlas);
    nk_font_stash_end(world->gui);

}
/*}}}*/

static struct nk_colorf convert_to_nk_color(sx_vec4 color) {
    struct nk_colorf col = { 0 };
    col.r = color.x;
    col.g = color.y;
    col.b = color.z;
    col.a = 1.0f;
    return col;
}

static sx_vec4 convert_to_vec4(struct nk_colorf color) {
    sx_vec4 col = { 0 };
    col.x = color.r;
    col.y = color.g;
    col.z = color.b;
    col.w = color.a;

    return col;
}

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

    if (nk_begin(&world->gui->context, "Atmospheric Parameters", nk_rect(50, 0, 400, 800),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {

        {
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Exposure:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 30, 1);
            world->renderer->exposure = nk_propertyf(ctx, "#gamma:", 0.1, world->renderer->exposure, 10.f, 0.5f, 0.005f);
        }

        {
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Camera speed:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 30, 1);
            world->cam_translation_speed = nk_propertyf(ctx, "#Speed:", 100.0, world->cam_translation_speed, 100000.0f, 100.f, 100.f);
        }

        {
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Sun direction:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 30, 2);
            sx_vec3 sun_dir = { { world->sky->atmosphere.sun_direction.x, world->sky->atmosphere.sun_direction.y, world->sky->atmosphere.sun_direction.z } };
            sun_dir.x = nk_propertyf(ctx, "#left/right:", -10.0, sun_dir.x, 10.0f, 0.01f,0.005f);
            sun_dir.y = nk_propertyf(ctx, "#up/down:", -10.0, sun_dir.y, 10.0f, 0.01f,0.005f);
            sun_dir = sx_vec3_norm(sun_dir);
            world->sky->atmosphere.sun_direction = (sx_vec4){ { sun_dir.x, sun_dir.y, sun_dir.z, 0.f } };
        }

        {
            struct nk_colorf rayleigh_color = convert_to_nk_color(sx_vec4_mulf(world->sky->atmosphere.rayleigh_scattering, 1.f / world->sky->rayleigh_scattering_scale));
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Rayleigh Scattering:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 30, 1);
            world->sky->rayleigh_scattering_scale = nk_propertyf(ctx, "#Scale:", 0, world->sky->rayleigh_scattering_scale, 1.0, 0.01, 0.005);

            nk_layout_row_dynamic(ctx, 40, 1);
            if (nk_combo_begin_color(ctx, nk_rgb_cf(rayleigh_color), nk_vec2(nk_widget_width(ctx),400))) {
                nk_layout_row_dynamic(ctx, 120, 1);
                rayleigh_color = nk_color_picker(ctx, rayleigh_color, NK_RGBA);
                nk_layout_row_dynamic(ctx, 25, 1);
                rayleigh_color.r = nk_propertyf(ctx, "#R:", 0.001, rayleigh_color.r, 10.0f, 0.01f,0.005f);
                rayleigh_color.g = nk_propertyf(ctx, "#G:", 0.001, rayleigh_color.g, 10.0f, 0.01f,0.005f);
                rayleigh_color.b = nk_propertyf(ctx, "#B:", 0.001, rayleigh_color.b, 10.0f, 0.01f,0.005f);
                rayleigh_color.a = nk_propertyf(ctx, "#A:", 0.001, rayleigh_color.a, 10.0f, 0.01f,0.005f);
                nk_combo_end(ctx);
            }
            world->sky->atmosphere.rayleigh_scattering = sx_vec4_mulf(convert_to_vec4(rayleigh_color), world->sky->rayleigh_scattering_scale);
        }

        {
            struct nk_colorf mie_color = convert_to_nk_color(sx_vec4_mulf(world->sky->atmosphere.mie_scattering, 1.f / world->sky->mie_scattering_scale));
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Mie Scattering:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 30, 1);
            world->sky->mie_scattering_scale = nk_propertyf(ctx, "#Scale:", 0.0, world->sky->mie_scattering_scale + 0.01, 1.0, 0.001, 0.005) - 0.01;

            nk_layout_row_dynamic(ctx, 40, 1);
            if (nk_combo_begin_color(ctx, nk_rgb_cf(mie_color), nk_vec2(nk_widget_width(ctx),400))) {
                nk_layout_row_dynamic(ctx, 120, 1);
                mie_color = nk_color_picker(ctx, mie_color, NK_RGBA);
                nk_layout_row_dynamic(ctx, 25, 1);
                mie_color.r = nk_propertyf(ctx, "#R:", 0.001, mie_color.r, 10.0f, 0.01f,0.005f);
                mie_color.g = nk_propertyf(ctx, "#G:", 0.001, mie_color.g, 10.0f, 0.01f,0.005f);
                mie_color.b = nk_propertyf(ctx, "#B:", 0.001, mie_color.b, 10.0f, 0.01f,0.005f);
                mie_color.a = nk_propertyf(ctx, "#A:", 0.001, mie_color.a, 10.0f, 0.01f,0.005f);
                nk_combo_end(ctx);
            }
            world->sky->atmosphere.mie_scattering = sx_vec4_mulf(convert_to_vec4(mie_color), world->sky->mie_scattering_scale);
        }

        {
            struct nk_colorf mie_color = convert_to_nk_color(sx_vec4_mulf(world->sky->atmosphere.mie_absorption, 1.f / world->sky->mie_absorption_scale));
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Mie absorption:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 30, 1);
            world->sky->mie_absorption_scale = nk_propertyf(ctx, "#Scale:", 0.0, world->sky->mie_absorption_scale + 0.01, 1.0, 0.001, 0.005) - 0.01;

            nk_layout_row_dynamic(ctx, 40, 1);
            if (nk_combo_begin_color(ctx, nk_rgb_cf(mie_color), nk_vec2(nk_widget_width(ctx),400))) {
                nk_layout_row_dynamic(ctx, 120, 1);
                mie_color = nk_color_picker(ctx, mie_color, NK_RGBA);
                nk_layout_row_dynamic(ctx, 25, 1);
                mie_color.r = nk_propertyf(ctx, "#R:", 0.001, mie_color.r, 10.0f, 0.01f,0.005f);
                mie_color.g = nk_propertyf(ctx, "#G:", 0.001, mie_color.g, 10.0f, 0.01f,0.005f);
                mie_color.b = nk_propertyf(ctx, "#B:", 0.001, mie_color.b, 10.0f, 0.01f,0.005f);
                mie_color.a = nk_propertyf(ctx, "#A:", 0.001, mie_color.a, 10.0f, 0.01f,0.005f);
                nk_combo_end(ctx);
            }
            world->sky->atmosphere.mie_absorption = sx_vec4_mulf(convert_to_vec4(mie_color), world->sky->mie_absorption_scale);
        }

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

