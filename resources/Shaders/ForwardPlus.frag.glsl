#version 430

uniform mat4 M;
uniform mat4 V;
uniform mat4 P;
uniform vec4 Color;
uniform vec4 DiffuseColor;
uniform vec2 ScreenDimensions;
uniform vec4 FillColor;
uniform vec4 AmbientColor;
uniform float FillPercentage;
layout (binding = 0) uniform sampler2D DiffuseTexture;
layout (binding = 1) uniform sampler2D NormalMapTexture;
layout (binding = 2) uniform sampler2D SpecularMapTexture;
layout (binding = 3) uniform sampler2D GlowMapTexture;
layout (binding = 4) uniform sampler2DShadow DepthMap;

#define TILE_SIZE 16

struct LightSource {
	vec4 Position;
	vec4 Direction;
	vec4 Color;
	float Radius;
	float Intensity;
	float Falloff;
	int Type;
};

layout (std430, binding = 1) buffer LightBuffer
{
	LightSource List[];
} LightSources;

struct LightGrid {
	float Start;
	float Amount;
	vec2 Padding;
};

layout (std430, binding = 2) buffer LightGridBuffer
{
	LightGrid Data[];
} LightGrids;

layout (std430, binding = 4) buffer LightIndexBuffer
{
	float LightIndex[];
};


in VertexData{
	vec3 Position;
	vec3 Normal;
	vec3 Tangent;
	vec3 BiTangent;
	vec2 TextureCoordinate;
	vec4 ExplosionColor;
	float ExplosionPercentageElapsed;
	vec4 PositionLightSpace;
}Input;

out vec4 sceneColor;
out vec4 bloomColor;

struct LightResult {
	vec4 Diffuse;
	vec4 Specular;
};

float CalcAttenuation(float radius, float dist, float falloff) {
	return 1.0 - smoothstep(radius * 0.3, radius, dist);
}

vec4 CalcSpecular(vec4 lightColor, vec4 viewVec,  vec4 lightVec, vec4 normal) {
	vec4 R = normalize( reflect(-lightVec, normal));
	float RdotV = max( dot(R, viewVec), 0.0);
	return lightColor * pow(RdotV, 90.0);
}

vec4 CalcDiffuse(vec4 lightColor, vec4 lightVec, vec4 normal) {
	float power = max( dot(normal, lightVec), 0.0);
	return lightColor * power;
}

LightResult CalcPointLightSource(vec4 lightPos, float lightRadius, vec4 lightColor, float intensity, vec4 viewVec, vec4 position, vec4 normal, float falloff)
{
	vec4 L = lightPos - position;
	float dist = length(L);
	L = normalize(L);

	float attenuation = CalcAttenuation(lightRadius, dist, falloff);

	LightResult result;
	result.Diffuse = CalcDiffuse(lightColor, L, normal) * attenuation * intensity;
	result.Specular = CalcSpecular(lightColor, viewVec, L, normal) * attenuation *  intensity;
	return result;
}

LightResult CalcDirectionalLightSource(vec4 direction, vec4 color, float intensity, vec4 viewVec, vec4 vertNormal)
{
	vec4 L = normalize( -vec4(direction.xyz, 0) );

	LightResult result;
	result.Diffuse = CalcDiffuse(color, L, vertNormal) * intensity;
	result.Specular = CalcSpecular(color, viewVec, L, vertNormal) * intensity;
	return result;
}

vec4 CalcNormalMappedValue(vec3 normal, vec3 tangent, vec3 bitangent, vec2 textureCoordinate, sampler2D normalMap)
{
	mat3 TBN = mat3(tangent, bitangent, normal);
	vec3 NormalMap = texture(normalMap, textureCoordinate).xyz * 2.0 - vec3(1.0);
	return vec4(TBN * normalize(NormalMap), 0.0);
}

vec2 poissonDisk[16] = vec2[]( 
   vec2( -0.94201624, -0.39906216 ), 
   vec2( 0.94558609, -0.76890725 ), 
   vec2( -0.094184101, -0.92938870 ), 
   vec2( 0.34495938, 0.29387760 ), 
   vec2( -0.91588581, 0.45771432 ), 
   vec2( -0.81544232, -0.87912464 ), 
   vec2( -0.38277543, 0.27676845 ), 
   vec2( 0.97484398, 0.75648379 ), 
   vec2( 0.44323325, -0.97511554 ), 
   vec2( 0.53742981, -0.47373420 ), 
   vec2( -0.26496911, -0.41893023 ), 
   vec2( 0.79197514, 0.19090188 ), 
   vec2( -0.24188840, 0.99706507 ), 
   vec2( -0.81409955, 0.91437590 ), 
   vec2( 0.19984126, 0.78641367 ), 
   vec2( 0.14383161, -0.14100790 ) 
);

float random(vec3 seed, int i)
{
	vec4 seed4 = vec4(seed, i);
	float dot_product = dot(seed4, vec4(12.9898, 78.233, 45.164, 94.673));
	return fract(sin(dot_product) * 43758.5453);
}

float CalcShadowValue(vec4 positionLightSpace, vec3 normal, vec3 lightDir, sampler2DShadow depthTexture)
{
	
	float bias = 0.005;
	//float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	//float bias = 0.005 * tan(acos(clamp(dot(normal, -lightDir), 0.0, 1.0)));
	
    vec3 projCoords = vec3(positionLightSpace.xy, positionLightSpace.z + bias) / positionLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    //float shadowMapDepth = texture(depthTexture, projCoords.xy).r; 
	float shadowMapDepth;
    //for (int i = 0; i < 4; i++)
	//{
	//	int index = i;
	//	//int index = int(16.0 * random(gl_FragCoord.xyy, i)) % 16;
	//	shadowMapDepth += 0.25 * texture(depthTexture, projCoords + vec3(poissonDisk[index], 0.0) / 700.0); 
	//}
	
	shadowMapDepth = texture(depthTexture, projCoords);
	
	//float geometryDepth = projCoords.z;
	//float shadow = geometryDepth - bias > shadowMapDepth ? 1.0 : 0.0;
	//float shadow = 1.0 - bias > shadowMapDepth ? 0.0 : 1.0;
	
	float shadow = 1.0 - shadowMapDepth;
	
	//vec2 texelSize = 1.0 / textureSize(depthTexture, 0);
	//for(int x = -1; x <= 1; x++)
	//{
	//	for(int y = -1; y <= 1; y++)
	//	{
	//		float pcfDepth = texture(depthTexture, projCoords.xy + vec2(x, y) * texelSize).r; 
	//		shadow += geometryDepth - bias > pcfDepth ? 1.0 : 0.0;
	//	}    
	//}
	//shadow /= 9.0;
	
	//if(projCoords.z > 1.0)
	//{
	//	shadow = 0.0;
	//}

    return shadow;
	
} 

void main()
{
	vec4 diffuseTexel = texture2D(DiffuseTexture, Input.TextureCoordinate);
	vec4 glowTexel = texture2D(GlowMapTexture, Input.TextureCoordinate);
	vec4 specularTexel = texture2D(SpecularMapTexture, Input.TextureCoordinate);
	vec4 position = V * M * vec4(Input.Position, 1.0); 
	vec4 normal = V * CalcNormalMappedValue(Input.Normal, Input.Tangent, Input.BiTangent, Input.TextureCoordinate, NormalMapTexture);
	normal = normalize(normal);
	//vec4 normal = normalize(V  * vec4(Input.Normal, 0.0));
	vec4 viewVec = normalize(-position); 

	vec2 tilePos;
	tilePos.x = int(gl_FragCoord.x/TILE_SIZE);
	tilePos.y = int(gl_FragCoord.y/TILE_SIZE);

	LightResult totalLighting;
	totalLighting.Diffuse = vec4(AmbientColor.rgb, 1.0);
	int currentTile = int(floor(gl_FragCoord.x/TILE_SIZE) + (floor(gl_FragCoord.y/TILE_SIZE) * int(ScreenDimensions.x/TILE_SIZE)));

	int start = int(LightGrids.Data[currentTile].Start);
	int amount = int(LightGrids.Data[currentTile].Amount);

	float shadowFactor = 0.0;
	
	for(int i = start; i < start + amount; i++) {

		int l = int(LightIndex[i]);
		LightSource light = LightSources.List[l];
		
		LightResult light_result;
		//These if statements should be removed.
		if(light.Type == 1) { // point
			light_result = CalcPointLightSource(V * light.Position, light.Radius, light.Color, light.Intensity, viewVec, position, normal, light.Falloff);
		} else if (light.Type == 2) { //Directional
			light_result = CalcDirectionalLightSource(V * light.Direction, light.Color, light.Intensity, viewVec, normal);
			shadowFactor = CalcShadowValue(Input.PositionLightSpace, Input.Normal, vec3(light.Direction), DepthMap);
		}
	
		totalLighting.Diffuse += light_result.Diffuse;
		totalLighting.Specular += light_result.Specular; 
	}

	//totalLighting.Diffuse *= (1.5 + vec4(AmbientColor.rgb, 1.0)) - vec4(vec3(shadowFactor), 0.0); 
	//totalLighting.Specular *= (1.5 + vec4(AmbientColor.rgb, 1.0)) - vec4(vec3(shadowFactor), 0.0);
	totalLighting.Diffuse *= (1.0 + vec4(AmbientColor.rgb, 1.0)) + vec4(vec3(shadowFactor, shadowFactor, 0.0), 0.0); 
	totalLighting.Specular *= (1.0 + vec4(AmbientColor.rgb, 1.0)) + vec4(vec3(shadowFactor, shadowFactor, 0.0), 0.0);
	
	//LightResult getInformation;
	
	vec4 color_result = mix((Color * diffuseTexel * DiffuseColor), Input.ExplosionColor, Input.ExplosionPercentageElapsed);
	color_result = color_result * (totalLighting.Diffuse + (totalLighting.Specular * specularTexel));
	
	//color_result = (totalLighting.Diffuse + (1.0 - shadowFactor) * (getInformation.Diffuse + (getInformation.Specular * specularTexel))) * color_result;
	

	
	

	
	
	
	//vec4 color_result = (DiffuseColor + Input.ExplosionColor) * (totalLighting.Diffuse + (totalLighting.Specular * specularTexel)) * diffuseTexel * Color;
	

	float pos = ((P * vec4(Input.Position, 1)).y + 1.0)/2.0;

	if(pos <= FillPercentage) {
		color_result += FillColor;
	}
	sceneColor = vec4(color_result.xyz, clamp(color_result.a, 0, 1));
	color_result += glowTexel*3;

	bloomColor = vec4(clamp(color_result.xyz - 1.0, 0, 100), 1.0);

	//Tiled Debug Code
	/*
	if(int(gl_FragCoord.x)%16 == 0 || int(gl_FragCoord.y)%16 == 0 ) {
		sceneColor += vec4(0.5, 0, 0, 0);
	} else {
		sceneColor += vec4(LightGrids.Data[int(tilePos.x + tilePos.y*80)].Amount/LightSources.List.length(), 0, 0, 1);
	}
	*/
}


