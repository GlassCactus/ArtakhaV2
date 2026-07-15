#shader vertex
#version 460 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex;

uniform mat4 normalMat;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

out vec3 fragPos;
out vec3 N;
out vec2 T;
out vec4 fragPosLightSpace;

void main()
{
	fragPos = (model * vec4(pos, 1.0f)).xyz;
	N = normalize(mat3(normalMat) * normal);
	T = tex;
	fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0f);

	gl_Position = projection * view * model * vec4(pos, 1.0f);
}

//==============================================================================================================//
//==============================================================================================================//

#shader fragment
#version 460 core

layout(location = 0) out vec4 FragColor;

in vec3 fragPos;
in vec3 N;
in vec2 T;
in vec4 fragPosLightSpace;

#define PI 3.14159265359
const float attConst = 1.0f;
const float attLinear = 0.0035f;
const float attQuad = 0.0005f;

uniform vec3 cameraPos;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform sampler2D depthMap;

uniform vec3 yarnColor;
uniform float GAMMA;


float calculateShadows(vec4 fragPosLightSpace)
{
	float shadow = 0.0f;
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5f + 0.5f;

	if (projCoords.z > 1.0f || projCoords.z < 0.0f || projCoords.x < 0.0f || projCoords.x > 1.0f || projCoords.y < 0.0f || projCoords.y > 1.0f)
	{
		return 0.0f;
	}

	float closestDepth = texture(depthMap, projCoords.xy).r;
	float currentDepth = projCoords.z;

	vec3 lightDir = normalize(lightPos - fragPos);
	float cosTheta = clamp(dot(N, lightDir), 0.0f, 1.0f);

	float bias = 0.005 * tan(acos(cosTheta));
	bias = clamp(bias, 0.0f, 0.01f);

	vec2 texelSize = 1.0f / textureSize(depthMap, 0);

	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			float PCF = texture(depthMap, projCoords.xy + vec2(x, y) * texelSize).r;

			if (currentDepth - bias > PCF)
			{
				shadow += 1.0f;
			}

		}
	}

	shadow /= 9.0f;

	return shadow;
}

vec3 BlinnPhong()
{
	vec3 viewDir = normalize(cameraPos - fragPos);
	vec3 specColor = vec3(0.1f, 0.1f, 0.1f);

	float distance = length(lightPos - fragPos);
	//float attenuation = 1.0f / (attConst + (attLinear * distance) + (attQuad * (distance * distance)));

	//ambient i guess
	vec3 ambient = yarnColor * 0.01f;
	float gloss = 4.0f;

	//diff
	vec3 lightDir = normalize(lightPos - fragPos);
	float diff = max(dot(N, lightDir), 0.0f);
	vec3 diffuse = yarnColor * diff;// / PI; //divide by PI for energy conservation

	//energy conserving specular
	vec3 halfDir = normalize(viewDir + lightDir);
	float spec = pow(max(dot(N, halfDir), 0.0f), gloss);
	float specEnergyConserving = (gloss + 2.0) / (8.0 * PI * (1.0 - pow(2.0f, (-gloss / 2.0f) - 1.0f)));
	vec3 specular = specColor * spec * specEnergyConserving;

	vec3 blinnPhong = (ambient + (diffuse + specular) * lightColor);// *attenuation;
	return blinnPhong;
}


void main()
{
	vec3 blinnPhong = BlinnPhong();
	float shadow = calculateShadows(fragPosLightSpace);
	FragColor = vec4(blinnPhong * (1.0f - shadow), 1.0f);
	FragColor.rgb = pow(FragColor.rgb, vec3(1.0f / GAMMA));
}