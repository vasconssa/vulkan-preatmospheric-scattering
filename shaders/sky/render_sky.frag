#version 450

layout (location = 0) in vec2 v_uv;
layout (location = 1) in vec3 camera_ray;

layout (location = 0) out vec4 out_color;

layout(set = 1, binding = 1) uniform sampler2D transmittanceTex;
layout(set = 1, binding = 2) uniform sampler2D multiScatTex;

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
    vec4 rayleighScattering;
    vec4 mieScattering;
    vec4 mieAbsorption;
    vec4 sunDir;
    float bottom;
    float top;
} atmosphere;

struct ScatteringResult {
    vec3 L;
    vec3 transmittance;
};

#define PI 3.14159265358f
#define TRANSMITTANCE_WIDTH 256
#define TRANSMITTANCE_HEIGHT 64

float intersectRaySphere(vec3 ro, vec3 rd, vec3 so, float sr)
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

float getRayTmax(vec3 pos, vec3 dir, float maxMax) {
    vec3 org = vec3(0.f);
    float tBottom = intersectRaySphere(pos, dir, org, atmosphere.bottom);
    float tTop = intersectRaySphere(pos, dir, org, atmosphere.top);
    float tMax = 0.0f;

    if (tBottom < 0.0f) {
        if (tTop < 0.0f) {
            return 0.0f;
        } else {
            tMax = tTop;
        }
    } else {
        if (tTop > 0.0f) {
            tMax = min(tTop, tBottom);
        }
    }

    tMax = min(tMax, maxMax);

    return tMax;
}


float getRayleighPhase(float mu) {
    return (3.f * (1.f + mu * mu)) / (16.f * PI);
}

float getCornettePhase(float g, float mu) {
    const float g2 = g * g;
    return (3.f * (1.f - g2) * (1.f + mu * mu)) / (8.f * PI * (2 + g2) * pow(1 + g2 - 2 * g * mu, 3.f / 2.f));
}

struct MediumRGBSample {
    vec3 scattering;
    vec3 absorption;
    vec3 extinction;

    vec3 mieScattering;
    vec3 mieAbsorption;
    vec3 mieExtinction;

    vec3 rayleighScattering;
    vec3 rayleighAbsorption;
    vec3 rayleighExtinction;
};

MediumRGBSample sampleMedium(vec3 pos) {
    const float height = length(pos) - atmosphere.bottom;
    
    const float mDensity = exp(-height / 1.2f);
    const float rDensity = exp(-height / 8.f);

    MediumRGBSample s;

    s.mieScattering = mDensity * atmosphere.mieScattering.rgb;
    s.mieAbsorption = mDensity * atmosphere.mieAbsorption.rgb;
    s.mieExtinction = mDensity * (atmosphere.mieScattering.rgb + atmosphere.mieAbsorption.rgb);

    s.rayleighScattering = rDensity * atmosphere.rayleighScattering.rgb;
    s.rayleighAbsorption = vec3(0.f);
    s.rayleighExtinction = s.rayleighScattering + s.rayleighAbsorption;

    s.scattering = s.mieScattering + s.rayleighScattering.rgb;
    s.absorption = s.mieAbsorption + s.rayleighAbsorption.rgb;
    s.extinction = s.mieExtinction + s.rayleighExtinction.rgb;

    return s;
}

vec3 sunRadiance(vec3 pos, vec3 dir, vec3 color, bool limb)
{
    const vec3 sunDir = normalize(atmosphere.sunDir.xyz);
    const vec3 forward = vec3(0.f, 0.f, 1.f);
    const float sun_scale = mix(1.f, 5.f, abs(dot(sunDir, forward)));
    float diameter_radians = (sun_scale * 1920.f / 3600.f) * (PI / 180.f);
    float cos_sun_angular_radius = cos(diameter_radians / 2.f);
    if (dot(dir, sunDir) > cos_sun_angular_radius) {
        float r = abs(dot(dir, sunDir)) / cos_sun_angular_radius;
        float t = intersectRaySphere(pos, dir, vec3(0.f), atmosphere.bottom);
        color = limb ? color : vec3(100.f);
        if (t < 0.0f)
            return mix(r * vec3(100.f), color, r);
    }

    return vec3(0.f);
}

float uvs2unit(float u, float resolution) 
{ 
    return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f)); 
}

float unit2uvs(float u, float resolution) { 
    return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f)); 
}

void uv2transmittance(vec2 uv, out float height, out float viewCosAngle)
{
    uvec2 res = uvec2(TRANSMITTANCE_WIDTH, TRANSMITTANCE_HEIGHT);
    uv = vec2(uvs2unit(uv.x, res.x), uvs2unit(uv.y, res.y));
    float xMu = uv.x;
    float xR = uv.y;

    float H = sqrt(atmosphere.top * atmosphere.top - atmosphere.bottom * atmosphere.bottom);
    float rho = H * xR;
    height = sqrt(rho * rho + atmosphere.bottom * atmosphere.bottom);

    float dMin = atmosphere.top - height;
    float dMax = rho + H;
    float d = dMin + xMu * (dMax - dMin);
    viewCosAngle = d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * height * d);
    viewCosAngle = clamp(viewCosAngle, -1.0, 1.0);
}

void transmittance2uv(in float height, in float viewCosAngle, out vec2 uv)
{
    float H = sqrt(max(0.0f, atmosphere.top * atmosphere.top - atmosphere.bottom * atmosphere.bottom));
    float rho = sqrt(max(0.0f, height * height - atmosphere.bottom * atmosphere.bottom));

    float discriminant = height * height * (viewCosAngle * viewCosAngle - 1.0) + atmosphere.top * atmosphere.top;
    float d = max(0.0, (-height * viewCosAngle + sqrt(max(discriminant, 0))));

    float dMin = atmosphere.top - height;
    float dMax = rho + H;
    float xMu = (d - dMin) / (dMax - dMin);
    float xR = rho / H;

    uv = vec2(xMu, xR);
}

bool move2topAtmosphere(inout vec3 pos, in vec3 dir, in float top)
{
    float height = length(pos);
    if (height > top) {
        dir = normalize(dir);
        float tTop = intersectRaySphere(pos, dir, vec3(0.f), top);
        if (tTop >= 0.0f) {
            vec3 upVec = normalize(global_ubo.camera_position.xyz / 1000.f);
            vec3 upOffset = upVec * -0.01f;
            pos = pos + dir * tTop + upOffset;
        } else {
            return false;
        }
    }
    return true;
}

vec3 getMultiScattering(vec3 pos, float viewCosAngle)
{
    vec2 uv = clamp(vec2(viewCosAngle * 0.5f + 0.5f, (length(pos) - atmosphere.bottom) / (atmosphere.top - atmosphere.bottom)), 0.0, 1.0);
    vec2 res = vec2(32.0, 32.0);
    uv = vec2(unit2uvs(uv.x, res.x), unit2uvs(uv.y, res.y));

    vec3 radiance = texture(multiScatTex, uv).rgb;
    return radiance;
}

ScatteringResult integrateRadiance(vec4 pos, vec3 dir) {
    ScatteringResult result = { vec3(0.f), vec3(0.f) };
    vec3 orig = vec3(0.f);
    if ((length(pos - orig) < atmosphere.bottom)) {
        return result;
    }

    float tMax = getRayTmax(pos, dir, 9000000.f);
    float sampleCount = mix(1, 40, clamp(tMax * 0.01, 0.f, 1.f));
    float countFloor = floor(sampleCount);
    float maxFloor = tMax * countFloor / sampleCount;
    float dt = tMax / sampleCount;

    vec3 sunDir = normalize(atmosphere.sunDir.xyz);
    float mu = dot(dir, sunDir);
    float miePhase = getCornettePhase(0.8f, mu);
    float rayleighPhase = getRayleighPhase(mu);

    vec3 L = vec3(0.f);
    vec3 transmittance = vec3(1.f, 1.f, 1.f);

    float t = 0.0f;
    uvec3 wd = uvec3(1000000000.f * (dir * 0.5 + 0.5));
    float segmentFrac = 0.3f;
    for (float s = 0.f; s < sampleCount; s += 1.f) {
        float t0 = (s) / countFloor;
        float t1 = (s + 1.0f) / countFloor;
        t0 = t0 * t0;
        t1 = t1 * t1;
        t0 = maxFloor * t0;
        t1 = t1 > 1.0 ? tMax : maxFloor * t1;
        t = t0 + (t1 - t0) * segmentFrac;
        dt = t1 - t0;
    
        vec3 p = pos + t * dir;

        MediumRGBSample medium = sampleMedium(p);
        const vec3 opticalThickness = medium.extinction * dt;
        const vec3 sampleTrasnmittance = exp(-opticalThickness);

        float height = length(p - orig);
        const vec3 upVec = (p - orig) / height;
        float sunCosAngle = dot(upVec, sunDir);

        vec2 uv;
        transmittance2uv(height, sunCosAngle, uv);
        vec3 sunTransmittance = texture(transmittanceTex, uv).rgb;
        vec3 multiScatRadiance = getMultiScattering(p, sunCosAngle);

        vec3 f = medium.mieScattering * miePhase + medium.rayleighScattering * rayleighPhase;
        float ts = intersectRaySphere(p, sunDir, orig + 0.01 * upVec, atmosphere.bottom);
        float eshadow = ts >= 0.0f ? 0.0f : 1.0f;
        vec3 S = 10.f * (eshadow * sunTransmittance * f + multiScatRadiance * medium.scattering);

        vec3 Sint = (S - S * sampleTrasnmittance) / medium.extinction;
        L += transmittance * Sint;
        transmittance *= sampleTrasnmittance;
    }

    float tBottom = intersectRaySphere(pos, dir, orig, atmosphere.bottom);

    if (tMax == tBottom && tBottom > 0.0) {
        vec3 p = pos + tBottom * dir;

        float height = length(p - orig);
        vec3 upVec = (p - orig) / height;
        float sunCosAngle = dot(upVec, sunDir);
        vec2 uv;
        transmittance2uv(height, sunCosAngle, uv);

        vec3 sunTransmittance = texture(transmittanceTex, uv).rgb;

        float nl = clamp(dot(normalize(upVec), normalize(sunDir)), 0.f, 1.f);
        L += 10.f * sunTransmittance * transmittance * nl * vec3(0.01f, 0.01f, 0.01f) / PI;
    }

    result.L = L;
    result.transmittance = transmittance;

    return result;
}

void main() 
{
    vec3 pos = global_ubo.camera_position.xyz / 1000.f + vec3(0.f, atmosphere.bottom + 1.f, 0.f);
    vec3 dir = normalize(camera_ray);
    vec3 L = vec3(0.f);
    ScatteringResult result;
    if (move2topAtmosphere(pos, dir, atmosphere.top)) {
        result = integrateRadiance(pos, dir);
        L += result.L;
        L += sunRadiance(pos, dir, L, true);
    } else {
        L += sunRadiance(pos, dir, vec3(0.f), false);
    }

	vec3 white_point = vec3(1.08241, 0.96756, 0.95003);
	float exposure = global_ubo.exposure_gama.x;
	vec4 color = vec4( pow(vec3(1.f) - exp(-L / white_point * exposure), vec3(1.0 / 2.2)), 1.0 );
	out_color = color;
}
