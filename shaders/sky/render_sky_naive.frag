#version 450

layout (location = 0) in vec2 v_uv;
layout (location = 1) in vec3 camera_ray;

layout (location = 0) out vec4 out_color;

layout(set=0, binding=0) uniform u_global_ubo {
    mat4 projection;
    mat4 view;
    mat4 projection_view;
    mat4 inverse_view;
    mat4 inverse_projection;
    vec4 light_position[4];
    vec4 camera_position;
    vec4 exposure_gama;
} global_ubo;

layout(set=1, binding=0) uniform u_atmosphere_ubo {
    vec4 rayleigh_scattering;
    vec4 mie_scattering;
    vec4 mie_absorption;
    vec4 sun_direction;
    float bottom_radius;
    float top_radius;
} atmosphere;

struct ScatteringResult {
    vec3 L;
    vec3 transmittance;
};

#define PI 3.14159265358f

// Improved precision ray/sphere intersection from https://link.springer.com/book/10.1007/978-1-4842-4427-2 chapter "Precision Improvements for Ray/Sphere Intersection"
// `ro` -- ray origin
// `rd` -- normalized ray direction
// `so` --  sphere center
// `sr` -- sphere radius
// Returns distance from r0 to first intersecion with sphere,
//   or -1.0 if no intersection.    
float ray_sphere_intersect_nearest(vec3 ro, vec3 rd, vec3 so, float sr)
{
    vec3 f = ro - so;
    float b2 = dot(f, rd);
    float r2 = sr * sr;
    vec3 fd = f - b2 * rd;
    float discriminant = r2 - dot(fd, fd);
    if (discriminant < 0.f)
        return -1.f;

    float c = dot(f, f) - r2;
    float sqrtv = sqrt(discriminant);
    float q = (b2 >= 0) ? -sqrtv - b2 : sqrtv - b2;

    float t0 = c / q;
    float t1 = q;

    if (t0 < 0.f && t1 < 0.f)
        return -1.f;        
    
    return t0 < 0.f ? max(0.f, t1) : t1 < 0.f ? max(0.f, t0) : max(0.f, min(t0, t1));
}

float get_ray_tmax(vec3 world_pos, vec3 world_dir, float t_max_max) {
    vec3 earth_org = vec3(0.f);
    float t_bottom = ray_sphere_intersect_nearest(world_pos, world_dir, earth_org, atmosphere.bottom_radius);
    float t_top = ray_sphere_intersect_nearest(world_pos, world_dir, earth_org, atmosphere.top_radius);
    float t_max = 0.0f;

    if (t_bottom < 0.0f) {
        if (t_top < 0.0f) {
            return 0.0f;
        } else {
            t_max = t_top;
        }
    } else {
        if (t_top > 0.0f) {
            t_max = min(t_top, t_bottom);
        }
    }

    t_max = min(t_max, t_max_max);

    return t_max;
}



float rayleigh_phase(float mu) {
    return (3.f * (1.f + mu * mu)) / (16.f * PI);
}

float hg_mie_phase(float g, float mu) {
    const float g2 = g * g;
    return (3.f * (1.f - g2) * (1.f - mu * mu)) / (8.f * PI * (2 + g2) * pow(1 + g2 + 2 * g * mu, 3.f / 2.f));
}

struct MediumRGBSample {
    vec3 scattering;
    vec3 absorption;
    vec3 extinction;

    vec3 mie_scattering;
    vec3 mie_absorption;
    vec3 mie_extinction;

    vec3 rayleigh_scattering;
    vec3 rayleigh_absorption;
    vec3 rayleigh_extinction;
};

MediumRGBSample sample_medium_rgb(vec3 world_pos) {
    const float view_height = length(world_pos) - atmosphere.bottom_radius;
    
    const float mie_density = exp(-view_height / 1.2f);
    const float rayleigh_density = exp(-view_height / 8.f);

    MediumRGBSample s;

    s.mie_scattering = mie_density * atmosphere.mie_scattering.rgb;
    s.mie_absorption = mie_density * atmosphere.mie_absorption.rgb;
    s.mie_extinction = mie_density * (atmosphere.mie_scattering.rgb + atmosphere.mie_absorption.rgb);

    s.rayleigh_scattering = rayleigh_density * atmosphere.rayleigh_scattering.rgb;
    s.rayleigh_absorption = vec3(0.f);
    s.rayleigh_extinction = s.rayleigh_scattering + s.rayleigh_absorption;

    s.scattering = s.mie_scattering + s.rayleigh_scattering.rgb;
    s.absorption = s.mie_absorption + s.rayleigh_absorption.rgb;
    s.extinction = s.mie_extinction + s.rayleigh_extinction.rgb;

    return s;
}

vec3 get_sun_luminance(vec3 world_pos, vec3 world_dir)
{
    const vec3 sun_dir = normalize(atmosphere.sun_direction.xyz);
    float diameter_radians = (1920.f / 3600.f) * (PI / 180.f);
    float cos_sun_angular_radius = cos(diameter_radians / 2.f);
    if (dot(world_dir, sun_dir) > cos_sun_angular_radius) {
        float t = ray_sphere_intersect_nearest(world_pos, world_dir, vec3(0.f), atmosphere.bottom_radius);
        if (t < 0.0f)
            return vec3(100000.f);
    }

    return vec3(0.f);
}

ScatteringResult integrate_lumminance(vec3 world_pos, vec3 world_dir) {
    ScatteringResult result = { vec3(0.f), vec3(0.f) };

    vec3 orig = vec3(0.f);

    const float t_max = get_ray_tmax(world_pos, world_dir, 900000000.f);
    const float sample_count = 30.f;
    const float dt = sample_count / t_max;

    const vec3 sun_dir = normalize(atmosphere.sun_direction.xyz);
    const float mu = dot(world_dir, sun_dir);
    const float mie_phase = hg_mie_phase(0.8f, mu);
    const float rayleigh_phase = rayleigh_phase(mu);

    vec3 L = vec3(0.f);
    vec3 transmittance = vec3(1.f, 1.f, 1.f);

    for (float s = 0.f; s < sample_count; s += 1.f) {
        vec3 p = world_pos + s * dt * world_dir;

        MediumRGBSample medium = sample_medium_rgb(p);
        const vec3 sample_optical_depth = medium.extinction * dt;
        const vec3 sample_transmittance = exp(-sample_optical_depth);

        vec3 phase_times_scattering = medium.mie_scattering * mie_phase + medium.rayleigh_scattering * rayleigh_phase;
        float earth_shadow = 1.f;
        vec3 transmittance_to_sun = vec3(1.f, 1.f, 1.f);
        for (float i = 0.f; i < 5.f; i += 1.f) {
            vec3 ps = p + i * dt * sun_dir;
            MediumRGBSample m = sample_medium_rgb(ps);
            const vec3 sun_optical_depth = m.extinction * dt;
            const vec3 sun_transmittance = exp(-sun_optical_depth);
            transmittance_to_sun *= sun_transmittance;
        }

        vec3 S = 20.f * (earth_shadow * transmittance_to_sun * phase_times_scattering);

        vec3 Sint = (S - S * sample_transmittance) / medium.extinction;
        L += transmittance * Sint;
        transmittance *= sample_transmittance;
    }

    //result.L = L;
    result.L = L;
    result.transmittance = transmittance;

    return result;
}

void main() 
{
    //vec3 world_pos = global_ubo.camera_position.xyz / 1000.f + vec3(0.f, atmosphere.bottom_radius + 1.f, 0.f);
    vec3 world_pos = vec3(0.f, atmosphere.bottom_radius + 0.1f, 0.f);
    vec3 world_dir = normalize(camera_ray);
    ScatteringResult result = integrate_lumminance(world_pos, world_dir);
    vec3 L = result.L;
    L += get_sun_luminance(world_pos, world_dir);

	out_color = vec4(L, 1.f);
}
