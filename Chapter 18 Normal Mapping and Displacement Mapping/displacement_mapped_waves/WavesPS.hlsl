struct Material
{
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float4 reflect;
};

struct LightDirectional
{
	float4 ambient;
	float4 diffuse;
	float4 specular;

	float3 direction;
	float  pad;
};

struct LightPoint
{
	float4 ambient;
	float4 diffuse;
	float4 specular;

	float3 position;
	float  range;
	float3 attenuation;
	float  pad;
};

struct LightSpot
{
	float4 ambient;
	float4 diffuse;
	float4 specular;

	float3 position;
	float  range;
	float3 direction;
	float  cone;
	float3 attenuation;
	float  pad;
};

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gWorldInverseTranspose;
	float4x4 gWorldViewProj;
	Material gMaterial;
	float4x4 gTexCoordTransform;

	float4x4 gWavesNormalMapTransform0;
	float4x4 gWavesNormalMapTransform1;
	float4x4 gWavesDisplacementMapTransform0;
	float4x4 gWavesDisplacementMapTransform1;
};

cbuffer cbPerFrame : register(b1)
{
	LightDirectional gLights[3];

	float3 gEyePositionW;
	float  pad1;

	float  gFogStart;
	float  gFogRange;
	float2 pad2;
	float4 gFogColor;

	float gHeightScale0;
	float gHeightScale1;
	float gMaxTessDistance;
	float gMinTessDistance;
	float gMinTessFactor;
	float gMaxTessFactor;
	float2 pad3;

	float4x4 gViewProj;
};

Texture2D gAlbedoTexture : register(t0);
Texture2D gWavesNormalMap0 : register(t1);
Texture2D gWavesNormalMap1 : register(t2);
TextureCube gCubeMap : register(t3);
SamplerState gSamplerState;

struct DomainOut
{
	float3 PositionW : POSITION;
	float4 PositionH : SV_POSITION;
	float3 NormalW   : NORMAL;
	float3 TangentW  : TANGENT;
	float2 TexCoord  : TEXCOORD0;
	float2 WavesNormalMapTexCoord0 : TEXCOORD1;
	float2 WavesNormalMapTexCoord1 : TEXCOORD2;
	float2 WavesDisplacementMapTexCoord0 : TEXCOORD3;
	float2 WavesDisplacementMapTexCoord1 : TEXCOORD4;
};

void ComputeLightDirectional(
	Material material,
	LightDirectional light,
	float3 N, // normal
	float3 E, // eye vector
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0, 0, 0, 0);
	diffuse = float4(0, 0, 0, 0);
	specular = float4(0, 0, 0, 0);

	float3 L = -light.direction; // the light vector aims opposite the light ray direction

	ambient = material.ambient * light.ambient;

	float d = dot(L, N); // diffuse factor

	[flatten]
	if (d > 0)
	{
		diffuse = d * material.diffuse * light.diffuse;

		float3 R = reflect(-L, N);
		float s = pow(max(dot(R, E), 0), material.specular.w); // specular factor

		specular = s * material.specular * light.specular;
	}
}

void ComputeLightPoint(
	Material material,
	LightPoint light,
	float3 P, // position
	float3 N, // normal
	float3 E, // eye vector
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0, 0, 0, 0);
	diffuse = float4(0, 0, 0, 0);
	specular = float4(0, 0, 0, 0);

	float3 L = light.position - P; // the light vector is oriented from the surface to the light

	float distance = length(L);

	if (distance > light.range) return;

	L /= distance; // normalize light vector

	ambient = material.ambient * light.ambient;

	float d = dot(L, N); // diffuse factor

	[flatten]
	if (d > 0)
	{
		diffuse = d * material.diffuse * light.diffuse;

		float3 R = reflect(-L, N);
		float s = pow(max(dot(R, E), 0), material.specular.w); // specular factor

		specular = s * material.specular * light.specular;
	}

	float attenuation = 1 / dot(light.attenuation, float3(1, distance, distance * distance));

	diffuse *= attenuation;
	specular *= attenuation;
}

void ComputeLightSpot(
	Material material,
	LightSpot light,
	float3 P, // position
	float3 N, // normal
	float3 E, // eye vector
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0, 0, 0, 0);
	diffuse = float4(0, 0, 0, 0);
	specular = float4(0, 0, 0, 0);

	float3 L = light.position - P; // the light vector is oriented from the surface to the light

	float distance = length(L);

	if (distance > light.range) return;

	L /= distance; // normalize light vector

	ambient = material.ambient * light.ambient;

	float d = dot(L, N); // diffuse factor

	[flatten]
	if (d > 0)
	{
		diffuse = d * material.diffuse * light.diffuse;

		float3 R = reflect(-L, N);
		float s = pow(max(dot(R, E), 0), material.specular.w); // specular factor

		specular = s * material.specular * light.specular;
	}

	float cone = pow(max(dot(-L, light.direction), 0), light.cone);

	float attenuation = cone / dot(light.attenuation, float3(1, distance, distance * distance));

	ambient *= cone;
	diffuse *= attenuation;
	specular *= attenuation;
}

float3 NormalFromTangentToWorld(float3 NormalS, float3 NormalW, float3 TangentW)
{
	// from [0,1] to [-1,+1]
	float3 NormalT = 2 * NormalS - 1;

	float3 N = NormalW;
	float3 T = normalize(TangentW - dot(TangentW, N) * N);
	float3 B = cross(N, T);

	float3x3 TBN = float3x3(T, B, N);

	return mul(NormalT, TBN);
}

float4 main(DomainOut pin) : SV_TARGET
{
	pin.NormalW = normalize(pin.NormalW); // interpolated normals can be unnormalized

#if ENABLE_SPHERE_TEXCOORD
	const float PI = 3.14159265359f;
	float3 D = -pin.NormalW;

	float u = 0.5f + atan2(D.x, D.z) / (2 * PI);
	float v = 0.5f - asin(D.y) / PI;
	pin.TexCoord = float2(u, v);

	// TODO : compute sphere tangent in the shader

#endif // ENABLE_SPHERE_TEXCOORD

	float3 E = gEyePositionW - pin.PositionW; // the eye vector is oriented from the surface to the eye position
	float DistToEye = length(E);
	E /= DistToEye;

	float4 TextureColor = float4(1, 1, 1, 1);

#if ENABLE_TEXTURE
	TextureColor = gAlbedoTexture.Sample(gSamplerState, pin.TexCoord);

	// if alpha clipping
	{
		clip(TextureColor.a - 0.1f);
	}
#endif // ENABLE_TEXTURE

#if ENABLE_NORMAL_MAPPING
	//float3 N = gNormalTexture.Sample(gSamplerState, pin.TexCoord).rgb;
	//pin.NormalW = NormalFromTangentToWorld(N, pin.NormalW, pin.TangentW);

	float3 N0 = gWavesNormalMap0.Sample(gSamplerState, pin.WavesNormalMapTexCoord0).rgb;
	N0 = NormalFromTangentToWorld(N0, pin.NormalW, pin.TangentW);

	float3 N1 = gWavesNormalMap1.Sample(gSamplerState, pin.WavesNormalMapTexCoord1).rgb;
	N1 = NormalFromTangentToWorld(N1, pin.NormalW, pin.TangentW);

	pin.NormalW = normalize(N0 + N1);
#endif // ENABLE_NORMAL_MAPPING

	float4 color = TextureColor;

	// if light
	{
		float4 ambient  = float4(0, 0, 0, 0);
		float4 diffuse  = float4(0, 0, 0, 0);
		float4 specular = float4(0, 0, 0, 0);

		[unroll]
		for (int i = 0; i < 3; ++i)
		{
			float4 a, d, s;

			ComputeLightDirectional(gMaterial, gLights[i], pin.NormalW, E, a, d, s);
			ambient  += a;
			diffuse  += d;
			specular += s;
		}

		color = TextureColor * (ambient + diffuse) + specular;

#if ENABLE_REFLECTION
		float3 ReflectVec = reflect(-E, pin.NormalW);
		//float3 RefractVec = refract(-E, pin.NormalW, 1);
		float4 ReflectCol = gCubeMap.Sample(gSamplerState, ReflectVec);
		color += gMaterial.reflect * ReflectCol;
#endif // ENABLE_REFLECTION

	}

#if ENABLE_FOG
	float t = saturate((DistToEye - gFogStart) / gFogRange);
	color = lerp(color, gFogColor, t);
#endif // ENABLE_FOG

	color.a = TextureColor.a * gMaterial.diffuse.a;

	return color;
}