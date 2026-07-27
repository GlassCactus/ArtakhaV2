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
uniform int colorByRow;
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

//RAINBOWS
vec3 RowColor(float t)
{
	if (t == 0.0 || t == 1.0f)
		return vec3(1.0f, 1.0f, 1.0f);

	return 0.5 + 0.5 * cos(2.0 * PI * (t + vec3(0.0, 0.33, 0.67)));
}

//RvB -- Double O Donut
vec3 Gradient(float t)
{
	return mix(vec3(0.75, 0.0, 0.0), vec3(0.0, 0.0, 0.75),t);
}

vec3 Viridis(float t)
{
	const vec3 c0 = vec3(0.277727, 0.005407, 0.334100);
	const vec3 c1 = vec3(0.105093, 1.404613, 1.384590);
	const vec3 c2 = vec3(-0.330861, 0.214847, 0.095095);
	const vec3 c3 = vec3(-4.634230, -5.799100, -19.332440);
	const vec3 c4 = vec3(6.228269, 14.179933, 56.690552);
	const vec3 c5 = vec3(4.776384, -13.745145, -65.353032);
	const vec3 c6 = vec3(-5.435455, 4.645852, 26.312435);

	return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}


vec3 Magma(float t)
{
	const vec3 c0 = vec3(-0.002136, -0.000749, -0.005386);
	const vec3 c1 = vec3(0.251660, 0.677523, 2.494026);
	const vec3 c2 = vec3(8.353717, -3.577719, 0.314467);
	const vec3 c3 = vec3(-27.668733, 14.264730, -13.649213);
	const vec3 c4 = vec3(52.176139, -27.943606, 12.944169);
	const vec3 c5 = vec3(-50.768525, 29.046582, -3.318579);
	const vec3 c6 = vec3(18.655705, -11.489773, 1.152056);

	return clamp(c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6))))), 0.0, 1.0);
}

vec3 Grad(float t)
{
	const vec3 c0 = vec3(0.000218, 0.000000, 0.000000);
	const vec3 c1 = vec3(0.078148, 0.054000, 0.177000);
	const vec3 c2 = vec3(1.220000, 1.000000, 2.200000);
	const vec3 c3 = vec3(2.300000, 0.500000, -0.300000);
	const vec3 c4 = vec3(-2.000000, -0.300000, -1.200000);
	const vec3 c5 = vec3(0.800000, -0.100000, 0.200000);

	return clamp(c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * c5)))), 0.0, 1.0);
}

vec3 BlinnPhong(vec3 color)
{
	vec3 viewDir = normalize(cameraPos - fragPos);
	vec3 specColor = vec3(0.1f, 0.1f, 0.1f);

	float distance = length(lightPos - fragPos);
	//float attenuation = 1.0f / (attConst + (attLinear * distance) + (attQuad * (distance * distance)));

	//ambient i guess
	vec3 ambient = color * 0.01f;
	float gloss = 4.0f;

	//diff
	vec3 lightDir = normalize(lightPos - fragPos);
	float diff = max(dot(N, lightDir), 0.0f);
	vec3 diffuse = color * diff;// / PI; //divide by PI for energy conservation

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
	vec3 baseColor = (colorByRow == 1) ? Gradient(T.x) : yarnColor;
	vec3 blinnPhong = BlinnPhong(baseColor);
	float shadow = calculateShadows(fragPosLightSpace);
	FragColor = vec4(blinnPhong * (1.0f - shadow), 1.0f);
	FragColor.rgb = pow(FragColor.rgb, vec3(1.0f / GAMMA));
}