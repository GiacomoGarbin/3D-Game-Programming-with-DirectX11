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

struct HullOut
{
	float3 PositionW : POSITION;
	float2 TexCoord  : TEXCOORD0;
};

struct DomainOut
{
	float4 PositionH     : SV_POSITION;
	float3 PositionW     : POSITION;
	float2 TexCoord      : TEXCOORD0;
	float2 TiledTexCoord : TEXCOORD1;
	float4 ShadowH       : TEXCOORD2;
	float4 AmbientH      : TEXCOORD3;
};

struct PatchTess
{
	float EdgeTess[4]   : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

[domain("quad")]
DomainOut main(PatchTess patch,
			   float2 uv : SV_DomainLocation,
			   const OutputPatch<HullOut, 4> quad)
{
	DomainOut dout;

	// bilinear interpolation
	dout.PositionW = lerp(lerp(quad[0].PositionW, quad[1].PositionW, uv.x),
						  lerp(quad[2].PositionW, quad[3].PositionW, uv.x),
						  uv.y);

	dout.TexCoord = lerp(lerp(quad[0].TexCoord, quad[1].TexCoord, uv.x),
						 lerp(quad[2].TexCoord, quad[3].TexCoord, uv.x),
						 uv.y);

	// tile layer textures over terrain
	dout.TiledTexCoord = dout.TexCoord * gTexelScale;

	// displacement mapping
	dout.PositionW.y = gHeightMapTexture.SampleLevel(gHeightMapSamplerState, dout.TexCoord, 0).r;

	// project to homogeneous clip space
	dout.PositionH = mul(gViewProj, float4(dout.PositionW, 1.0f));

	dout.ShadowH = mul(gShadowTransform, float4(dout.PositionW, 1));
	dout.AmbientH = mul(gWorldViewProjTexture, float4(dout.PositionW, 1));

	return dout;
}