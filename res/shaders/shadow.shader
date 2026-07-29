#shader vertex
#version 410 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
	gl_Position = lightSpaceMatrix * model * vec4(pos, 1.0f);
}

//==============================================================================================================//
//==============================================================================================================//
#shader fragment
#version 410 core

void main()
{
	//I can write an entire story here or something.
}