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

cbuffer FixedCB : register(b2)
{
	float2 gTexelSize;
	float  gSpaceStep;
	float  pad;
};

Texture2D gCurrTexture : register(t1);
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

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	vin.PositionL.y = gCurrTexture.SampleLevel(gSamplerState, vin.TexCoord, 0).r;

	float du = gTexelSize.x;
	float dv = gTexelSize.y;

	float l = gCurrTexture.SampleLevel(gSamplerState, vin.TexCoord - float2(du, 0), 0).r;
	float r = gCurrTexture.SampleLevel(gSamplerState, vin.TexCoord + float2(du, 0), 0).r;
	float t = gCurrTexture.SampleLevel(gSamplerState, vin.TexCoord - float2(0, dv), 0).r;
	float b = gCurrTexture.SampleLevel(gSamplerState, vin.TexCoord + float2(0, dv), 0).r;

	vin.NormalL.x = l - r;
	vin.NormalL.y = 2 * gSpaceStep;
	vin.NormalL.z = b - t;
	vin.NormalL = normalize(vin.NormalL);

	vout.PositionW = mul(gWorld, float4(vin.PositionL, 1)).xyz;
	vout.PositionH = mul(gWorldViewProj, float4(vin.PositionL, 1));
	vout.NormalW = mul((float3x3)gWorldInverseTranspose, vin.NormalL);
	vout.TexCoord = mul(gTexTransform, float4(vin.TexCoord, 0, 1)).xy;

	return vout;
}