#include "world/camera.h"
#include "device/device.h"

 void init(Camera* cam, float fov_deg, const sx_rect viewport, float fnear,
                           float ffar)
{
    cam->right = SX_VEC3_UNITX;
    cam->up = SX_VEC3_UNITY;
    cam->forward = SX_VEC3_UNITZ;
    cam->pos = SX_VEC3_ZERO;

    cam->quat = sx_quat_rotateX(sx_torad(90));
    cam->right = sx_vec3_mul_quat(SX_VEC3_UNITX, cam->quat);
    cam->up = sx_vec3_mul_quat(SX_VEC3_UNITY, cam->quat);
    cam->forward = sx_vec3_mul_quat(SX_VEC3_UNITZ, cam->quat);
    cam->fov = fov_deg;
    cam->fnear = fnear;
    cam->ffar = ffar;
    cam->viewport = viewport;
}

 void lookat(Camera* cam, const sx_vec3 pos, const sx_vec3 target,
                             const sx_vec3 up)
{
    cam->forward = sx_vec3_norm(sx_vec3_sub(pos, target));
    cam->right = sx_vec3_norm(sx_vec3_cross(up, cam->forward));
    cam->up = sx_vec3_cross(cam->forward, cam->right);
    cam->pos = pos;

    sx_mat4 m = sx_mat4v(sx_vec4f(cam->right.x, cam->up.x, cam->forward.x, 0),
                         sx_vec4f(cam->right.y, cam->up.y, cam->forward.y, 0),
                         sx_vec4f(cam->right.z, cam->up.z, cam->forward.z, 0), SX_VEC4_ZERO);
    cam->quat = sx_mat4_quat(&m);
}

 sx_mat4 perspective_mat(const Camera* cam)
{
    float w = cam->viewport.xmax - cam->viewport.xmin;
    float h = cam->viewport.ymax - cam->viewport.ymin;
    return sx_mat4_perspectiveFOV(sx_torad(cam->fov), w / h, cam->fnear, cam->ffar, false);
}

 sx_mat4 ortho_mat(const Camera* cam)
{
    return sx_mat4_ortho(cam->viewport.xmax - cam->viewport.xmin,
                         cam->viewport.ymax - cam->viewport.ymin, cam->fnear, cam->ffar, 0,
                         false);
}

 sx_mat4 view_mat(const Camera* cam)
{

    sx_vec3 zaxis = cam->forward;
    sx_vec3 xaxis = cam->right; // sx_vec3_norm(sx_vec3_cross(zaxis, up));
    sx_vec3 yaxis = cam->up;    // sx_vec3_cross(xaxis, zaxis);
    
    /*sx_mat4 rotation = sx_quat_mat4(cam->quat);*/
    /*sx_mat4 translation = sx_mat4_translate(cam->pos.x, cam->pos.y, cam->pos.z);*/
    /*sx_mat4 rot_trans = sx_mat4_mul(&rotation, &rotation);*/
    /*return sx_mat4_inv(&rot_trans);*/

    // clang-format off
    return sx_mat4v(sx_vec4f(xaxis.x, yaxis.x, zaxis.x, 0.0),
                    sx_vec4f(xaxis.y, yaxis.y, zaxis.y, 0.0),
                    sx_vec4f(xaxis.z, yaxis.z, zaxis.z, 0.0),
                    sx_vec4f(-sx_vec3_dot(xaxis, cam->pos), -sx_vec3_dot(yaxis, cam->pos), -sx_vec3_dot(zaxis, cam->pos), 1.0f));
}

 void frustum_points_range(const Camera* cam, sx_vec3 frustum[8], float fnear,
                                            float ffar)
{
    const float fov = sx_torad(cam->fov);
    const float w = cam->viewport.xmax - cam->viewport.xmin;
    const float h = cam->viewport.ymax - cam->viewport.ymin;
    const float aspect = w / h;

    sx_vec3 xaxis = cam->right;
    sx_vec3 yaxis = cam->up;
    sx_vec3 zaxis = cam->forward;
    sx_vec3 pos = cam->pos;

    float near_plane_h = sx_tan(fov * 0.5f) * fnear;
    float near_plane_w = near_plane_h * aspect;

    float far_plane_h = sx_tan(fov * 0.5f) * ffar;
    float far_plane_w = far_plane_h * aspect;

    sx_vec3 center_near = sx_vec3_add(sx_vec3_mulf(zaxis, fnear), pos);
    sx_vec3 center_far = sx_vec3_add(sx_vec3_mulf(zaxis, ffar), pos);

    // scaled axises
    sx_vec3 xnear_scaled = sx_vec3_mulf(xaxis, near_plane_w);
    sx_vec3 xfar_scaled = sx_vec3_mulf(xaxis, far_plane_w);
    sx_vec3 ynear_scaled = sx_vec3_mulf(yaxis, near_plane_h);
    sx_vec3 yfar_scaled = sx_vec3_mulf(yaxis, far_plane_h);

    // near quad (normal inwards)
    frustum[0] = sx_vec3_sub(center_near, sx_vec3_add(xnear_scaled, ynear_scaled));
    frustum[1] = sx_vec3_add(center_near, sx_vec3_sub(xnear_scaled, ynear_scaled));
    frustum[2] = sx_vec3_add(center_near, sx_vec3_add(xnear_scaled, ynear_scaled));
    frustum[3] = sx_vec3_sub(center_near, sx_vec3_sub(xnear_scaled, ynear_scaled));

    // far quad (normal inwards)
    frustum[4] = sx_vec3_sub(center_far, sx_vec3_add(xfar_scaled, yfar_scaled));
    frustum[5] = sx_vec3_sub(center_far, sx_vec3_sub(xfar_scaled, yfar_scaled));
    frustum[6] = sx_vec3_add(center_far, sx_vec3_sub(xfar_scaled, yfar_scaled));
    frustum[7] = sx_vec3_add(center_far, sx_vec3_add(xfar_scaled, yfar_scaled));
}

 void frustum_points(const Camera* cam, sx_vec3 frustum[8])
{
    frustum_points_range(cam, frustum, cam->fnear, cam->ffar);
}

 void fps_init(FpsCamera* cam, float fov_deg, const sx_rect viewport,
                               float fnear, float ffar)
{
    init(&cam->cam, fov_deg, viewport, fnear, ffar);
    cam->pitch = cam->yaw = 0;
}

 void fps_lookat(FpsCamera* cam, const sx_vec3 pos, const sx_vec3 target,
                                 const sx_vec3 up)
{
    lookat(&cam->cam, pos, target, up);

    sx_vec3 euler = sx_quat_toeuler(cam->cam.quat);
    cam->pitch = euler.x;
    cam->yaw = euler.y;
}

 void update_rot(Camera* cam)
{
    cam->quat = sx_quat_norm(cam->quat);
    sx_mat4 m = sx_quat_mat4(cam->quat);
    cam->right = sx_vec3fv(m.col1.f);
    cam->up = sx_vec3_mulf(sx_vec3fv(m.col2.f), -1.0f);
    cam->forward = sx_vec3fv(m.col3.f);
    /*cam->quat = sx_quat_mul(cam->quat, sx_quat_rotateX(sx_torad(-90)));*/

    /*cam->right = sx_vec3_mul_quat(SX_VEC3_UNITX, cam->quat);*/
    /*cam->up = sx_vec3_mul_quat(SX_VEC3_UNITY, cam->quat);*/
    /*cam->forward = sx_vec3_mul_quat(SX_VEC3_UNITZ, cam->quat);*/
}

 void fps_pitch(FpsCamera* fps, float pitch)
{
    fps->pitch -= pitch;
    fps->cam.quat = sx_quat_mul(sx_quat_rotateaxis(SX_VEC3_UNITY, fps->yaw),
                                sx_quat_rotateaxis(SX_VEC3_UNITX, fps->pitch));
    /*fps->cam.quat = sx_quat_mul(sx_quat_rotateaxis(SX_VEC3_UNITX, fps->pitch),*/
                                /*sx_quat_rotateaxis(SX_VEC3_UNITY, fps->yaw));*/
    update_rot(&fps->cam);
}

 void fps_pitch_range(FpsCamera* fps, float pitch, float _min, float _max)
{
    fps->pitch = sx_clamp(fps->pitch - pitch, _min, _max);
    fps->cam.quat = sx_quat_mul(sx_quat_rotateaxis(SX_VEC3_UNITY, fps->yaw),
                                sx_quat_rotateaxis(SX_VEC3_UNITX, fps->pitch));
    fps->cam.quat = sx_quat_norm(fps->cam.quat);
    update_rot(&fps->cam);
}

 void fps_yaw(FpsCamera* fps, float yaw)
{
    fps->yaw -= yaw;
    fps->cam.quat = sx_quat_mul(sx_quat_rotateaxis(SX_VEC3_UNITY, fps->yaw),
                                sx_quat_rotateaxis(SX_VEC3_UNITX, fps->pitch));
    /*fps->cam.quat = sx_quat_mul(sx_quat_rotateaxis(SX_VEC3_UNITX, fps->pitch),*/
                                /*sx_quat_rotateaxis(SX_VEC3_UNITY, fps->yaw));*/
    update_rot(&fps->cam);
}

 void fps_forward(FpsCamera* fps, float forward)
{
    fps->cam.pos = sx_vec3_add(fps->cam.pos, sx_vec3_mulf(fps->cam.forward, forward));
}

 void fps_strafe(FpsCamera* fps, float strafe)
{
    fps->cam.pos = sx_vec3_add(fps->cam.pos, sx_vec3_mulf(fps->cam.right, strafe));
}


void fps_camera_update(FpsCamera* cam, float dt, float translation_speed) {
        static bool wkey = false; 
        static bool skey = false;
        static bool akey = false;
        static bool dkey = false;
        float rotation_speed = 0.15;
        static float pitch = 0.0;
        static float yaw = 0.0;
        sx_vec3 delta = mouse_axis(device()->input_manager, MA_CURSOR_DELTA);

        float dx = delta.x;
        float dy = -delta.y;
        
        pitch = mouse_button(device()->input_manager, MB_RIGHT) != 0 ? rotation_speed*dy*dt : 0;
        yaw = mouse_button(device()->input_manager, MB_RIGHT) != 0 ? rotation_speed*dx*dt : 0;

        fps_yaw(cam, yaw);
        fps_pitch(cam, -pitch);

        if (keyboard_button_pressed(device()->input_manager, KB_W)) wkey = true;
        if (keyboard_button_pressed(device()->input_manager, KB_S)) skey = true;
        if (keyboard_button_pressed(device()->input_manager, KB_A)) akey = true;
        if (keyboard_button_pressed(device()->input_manager, KB_D)) dkey = true;

        if (keyboard_button_released(device()->input_manager, KB_W)) wkey = false;
        if (keyboard_button_released(device()->input_manager, KB_S)) skey = false;
        if (keyboard_button_released(device()->input_manager, KB_A)) akey = false;
        if (keyboard_button_released(device()->input_manager, KB_D)) dkey = false;



        if (wkey) {
           fps_forward(cam, -translation_speed);
        }
        if (skey) { 
           fps_forward(cam, translation_speed);
        }
        if (akey) { 
            fps_strafe(cam, -translation_speed);
        }
        if (dkey) { 
            fps_strafe(cam, translation_speed);
        }

}

