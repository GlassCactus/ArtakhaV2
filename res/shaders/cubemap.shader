#shader vertex
#version 460 core

layout(location = 0) in vec3 pos;

uniform mat4 view;
uniform mat4 projection;

out vec3 TexCoords;

void main()
{
	TexCoords = pos;
	vec4 cubePos = projection * view * vec4(pos, 1.0f);
	gl_Position = cubePos.xyww;
}


//==============================================================================================================//
//==============================================================================================================//

#shader fragment
#version 460 core

out vec4 FragColor;
in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{

	FragColor = texture(skybox, TexCoords);
}