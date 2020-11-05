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
};

struct VertexOut
{
	float3 PositionL : POSITION;
	float4 PositionH : SV_POSITION;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	vout.PositionL = vin.PositionL;
	vout.PositionH = mul(gWorldViewProj, float4(vin.PositionL, 1));

	return vout;
}