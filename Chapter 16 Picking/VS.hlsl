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
	float4x4 gViewProj;
	Material gMaterial;
	float4x4 gTexTransform;
};

cbuffer cbPerFrame : register(b1)
{
	LightDirectional gLightDirectional;
	LightPoint       gLightPoint;
	LightSpot        gLightSpot;

	float3 gEyePositionW;
	float  pad1;

	float  gFogStart;
	float  gFogRange;
	float2 pad2;
	float4 gFogColor;
};

Texture2D gTexture0 : register(t0);
//Texture2D gTexture1 : register(t1);
SamplerState gSamplerState;

struct VertexIn
{
	float3 PositionL : POSITION;
	float3 NormalL   : NORMAL;
	float2 TexCoord  : TEXCOORD;
	float4x4 world   : WORLD;
	float4 color     : COLOR;
	uint InstanceID  : SV_InstanceID;
};

struct VertexOut
{
	float3 PositionW : POSITION;
	float4 PositionH : SV_POSITION;
	float3 NormalW   : NORMAL;
	float2 TexCoord  : TEXCOORD;
	float4 color     : COLOR;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	vout.PositionW = mul(vin.world, float4(vin.PositionL, 1)).xyz;

	//vout.PositionW.x = vin.PositionL.x + vin.world._41;
	//vout.PositionW.y = vin.PositionL.y + vin.world._42;
	//vout.PositionW.z = vin.PositionL.z + vin.world._43;

	vout.PositionH = mul(gViewProj, float4(vout.PositionW, 1));
	vout.NormalW = mul((float3x3)vin.world, vin.NormalL);
	vout.TexCoord = mul(gTexTransform, float4(vin.TexCoord, 0, 1)).xy;
	vout.color = vin.color;
	//vout.color = float4(vin.world._41, vin.world._42, vin.world._43, 1);

	return vout;
}