#pragma once

#include "sx/math.h"
#include <stdbool.h>

typedef struct Camera {
    sx_vec3 forward;
    sx_vec3 right;
    sx_vec3 up;
    sx_vec3 pos;

    sx_quat quat;
    float ffar;
    float fnear;
    float fov;
    sx_rect viewport;
} Camera;

typedef struct FpsCamera {
    Camera cam;
    float pitch;
    float yaw;
} FpsCamera;

extern void init(Camera* cam, float fov_deg, const sx_rect viewport, float fnear, float ffar);
extern void lookat(Camera* cam, const sx_vec3 pos, const sx_vec3 target, const sx_vec3 up);
extern sx_mat4 ortho_mat(const Camera* cam);
extern sx_mat4 perspective_mat(const Camera* cam);
extern sx_mat4 view_mat(const Camera* cam);
extern void calc_frustum_points(const Camera* cam, sx_vec3 frustum[8]);
extern void calc_frustum_points_range(const Camera* cam, sx_vec3 frustum[8], float fnear, float ffar);

extern void fps_init(FpsCamera* cam, float fov_deg, const sx_rect viewport, float fnear, float ffar);
extern void fps_lookat(FpsCamera* cam, const sx_vec3 pos, const sx_vec3 target, const sx_vec3 up);
extern void fps_pitch(FpsCamera* cam, float pitch);
extern void fps_pitch_range(FpsCamera* cam, float pitch, float _min, float _max);
extern void fps_yaw(FpsCamera* cam, float yaw);
extern void fps_forward(FpsCamera* cam, float forward);
extern void fps_strafe(FpsCamera* cam, float strafe);

extern void fps_camera_update(FpsCamera* cam, float dt, float translation_speed);

