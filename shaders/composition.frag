#version 450

layout (set = 1, input_attachment_index = 0, binding = 0) uniform subpassInput sampler_position;
layout (set = 1, input_attachment_index = 1, binding = 1) uniform subpassInput sampler_normal;
layout (set = 1, input_attachment_index = 2, binding = 2) uniform subpassInput sampler_albedo;
layout (set = 1, input_attachment_index = 3, binding = 3) uniform subpassInput sampler_metallicroughness;

layout (location = 0) in vec2 v_uv;

layout (location = 0) out vec4 out_color;

layout (constant_id = 0) const int NUM_LIGHTS = 1;

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


void main() 
{
	// Read G-Buffer values from previous sub pass
	vec3 frag_pos = subpassLoad(sampler_position).rgb;
	vec3 normal = subpassLoad(sampler_normal).rgb;
	vec4 albedo = subpassLoad(sampler_albedo);
	
	#define ambient 0.15
	
	// Ambient part
	vec3 frag_color  = albedo.rgb * ambient;
	
	for(int i = 0; i < NUM_LIGHTS; ++i)
	{
		// Vector to light
		vec3 L = global_ubo.light_position[i].xyz - frag_pos;
		// Distance from light to fragment position
		float dist = length(L);

		// Viewer to fragment
		vec3 V = global_ubo.camera_position.xyz - frag_pos;
		V = normalize(V);
		
		// Light to fragment
		L = normalize(L);

		// Attenuation
		//float atten = ubo.lights[i].radius / (pow(dist, 2.0) + 1.0);

		// Diffuse part
		vec3 N = normalize(normal);
		float NdotL = max(0.0, dot(N, L));
		vec3 diff =  albedo.rgb * NdotL;

		// Specular part
		// Specular map values are stored in alpha of albedo mrt
		vec3 R = reflect(-L, N);
		float NdotR = max(0.0, dot(R, V));
		//vec3 spec = ubo.lights[i].color * albedo.a * pow(NdotR, 32.0) * atten;

		frag_color += diff;// + spec;	
	}    	
   
	out_color = vec4(frag_color, 1.0);
}
