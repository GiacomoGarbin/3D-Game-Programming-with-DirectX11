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

cbuffer PerFrameCB : register(b0)
{
	LightDirectional gLights[3];

	float3 gEyePositionW;
	float  pad1;

	float  gFogStart;
	float  gFogRange;
	float2 pad2;
	float4 gFogColor;

	float gHeightScale;
	float gMaxTessDistance;
	float gMinTessDistance;
	float gMinTessFactor;
	float gMaxTessFactor;
	float3 pad3;

	float4x4 gViewProj;

	float mTexelCellSpaceU;
	float mTexelCellSpaceV;
	float mWorldCellSpace;
	float pad4;
	float2 gTexelScale;
	float2 pad5;

	float4 gWorldFrustumPlanes[6];
};

cbuffer PerObjectCB : register(b1)
{
	float4x4 gWorld;
	float4x4 gWorldInverseTranspose;
	float4x4 gWorldViewProj;
	Material gMaterial;
	float4x4 gTexCoordTransform;
	float4x4 gShadowTransform;
	float4x4 gWorldViewProjTexture;
};

Texture2D gHeightMapTexture : register(t0);
SamplerState gHeightMapSamplerState : register(s0);

struct VertexIn
{
	float3 PositionL : POSITION;
	float2 TexCoord  : TEXCOORD0;
	float2 BoundsY   : TEXCOORD1;
};

struct VertexOut
{
	float3 PositionW : POSITION;
	float2 TexCoord  : TEXCOORD0;
	float2 BoundsY   : TEXCOORD1;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	// terrain specified directly in world space
	vout.PositionW = vin.PositionL;

	// pisplace the patch corners to world space
	vout.PositionW.y = gHeightMapTexture.SampleLevel(gHeightMapSamplerState, vin.TexCoord, 0).r;

	vout.TexCoord = vin.TexCoord;
	vout.BoundsY = vin.BoundsY;

	return vout;
}