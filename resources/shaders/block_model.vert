#version 430

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 pass_tex;
layout (location = 2) in vec3 pass_normal;
layout (location = 3) in uint pass_tex_id;

uniform mat4 MVP;

out float logz;
out vec2 tex;
out vec3 normal;
flat out uint tex_id;

void main()
{
	gl_Position = MVP * vec4(pos, 1.0f);
	
	// logarithmic depth buffers
	// https://outerra.blogspot.com/2012/11/maximizing-depth-buffer-range-and.html
	const float C = 1;
	const float far = 1500;
	const float FC = 1.0/log(far*C + 1);
	
	gl_Position.z = (2.0*log(gl_Position.w*C + 1))/log(far*C+1) - 1; 
    gl_Position.z *= gl_Position.w;
	logz = log(gl_Position.w*C + 1)*FC;
	
	tex = pass_tex;
	normal = pass_normal;
	tex_id = pass_tex_id;
}
