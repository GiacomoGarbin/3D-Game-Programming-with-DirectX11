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
	float4x4 gTexTransform;
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
};

Texture2D gTexture : register(t0);
SamplerState gSamplerState;

struct VertexIn
{
	float3 PositionL : POSITION;
	float3 NormalL   : NORMAL;
	float2 TexCoord  : TEXCOORD;
};

struct VertexOut
{
	float3 PositionW : POSITION;
	float4 PositionH : SV_POSITION;
	float3 NormalW   : NORMAL;
	float2 TexCoord  : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	vout.PositionW = mul(gWorld, float4(vin.PositionL, 1)).xyz;
	vout.PositionH = mul(gWorldViewProj, float4(vin.PositionL, 1));
	vout.NormalW = mul((float3x3)gWorldInverseTranspose, vin.NormalL);
	vout.TexCoord = mul(gTexTransform, float4(vin.TexCoord, 0, 1)).xy;

	return vout;
}

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

float4 main(VertexOut pin) : SV_TARGET
{
	pin.NormalW = normalize(pin.NormalW); // interpolated normals can be unnormalized

	float3 E = gEyePositionW - pin.PositionW; // the eye vector is oriented from the surface to the eye position
	float DistToEye = length(E);
	E /= DistToEye;

	float4 TextureColor = float4(1, 1, 1, 1);

#if ENABLE_TEXTURE
		TextureColor = gTexture.Sample(gSamplerState, pin.TexCoord);
#if ENABLE_ALPHA_CLIPPING
		clip(TextureColor.a - 0.1f);
#endif // ENABLE_ALPHA_CLIPPING
#endif // ENABLE_TEXTURE

	float4 color = TextureColor;

#if ENABLE_LIGHTING
	float4 ambient = float4(0, 0, 0, 0);
	float4 diffuse = float4(0, 0, 0, 0);
	float4 specular = float4(0, 0, 0, 0);

	[unroll]
	for (int i = 0; i < 3; ++i)
	{
		float4 a, d, s;

		ComputeLightDirectional(gMaterial, gLights[i], pin.NormalW, E, a, d, s);
		ambient += a;
		diffuse += d;
		specular += s;
	}

	color = TextureColor * (ambient + diffuse) + specular;
#endif // ENABLE_LIGHTING

#if ENABLE_FOG
	float t = saturate((DistToEye - gFogStart) / gFogRange);
	color = lerp(color, gFogColor, t);
#endif // ENABLE_FOG

	color.a = TextureColor.a * gMaterial.diffuse.a;

	return color;
}