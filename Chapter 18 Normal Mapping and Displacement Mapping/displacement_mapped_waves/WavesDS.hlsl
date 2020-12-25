struct LightDirectional
{
	float4 ambient;
	float4 diffuse;
	float4 specular;

	float3 direction;
	float  pad;
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

Texture2D gWavesNormalMap0 : register(t1);
Texture2D gWavesNormalMap1 : register(t2);
SamplerState gSamplerState;

struct HullOut
{
	float3 PositionW : POSITION;
	float3 NormalW   : NORMAL;
	float3 TangentW  : TANGENT;
	float2 TexCoord  : TEXCOORD0;
	float2 WavesNormalMapTexCoord0 : TEXCOORD1;
	float2 WavesNormalMapTexCoord1 : TEXCOORD2;
	float2 WavesDisplacementMapTexCoord0 : TEXCOORD3;
	float2 WavesDisplacementMapTexCoord1 : TEXCOORD4;
};

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

struct PatchTess
{
	float EdgeTess[3] : SV_TessFactor;
	float InsideTess : SV_InsideTessFactor;
};

[domain("tri")]
DomainOut main(PatchTess patch, float3 bary : SV_DomainLocation, const OutputPatch<HullOut, 3> tri)
{
	DomainOut dout;

	dout.PositionW = bary.x * tri[0].PositionW + bary.y * tri[1].PositionW + bary.z * tri[2].PositionW;
	dout.NormalW   = bary.x * tri[0].NormalW   + bary.y * tri[1].NormalW   + bary.z * tri[2].NormalW;
	dout.TangentW  = bary.x * tri[0].TangentW  + bary.y * tri[1].TangentW  + bary.z * tri[2].TangentW;
	dout.TexCoord  = bary.x * tri[0].TexCoord  + bary.y * tri[1].TexCoord  + bary.z * tri[2].TexCoord;

	dout.WavesNormalMapTexCoord0 = bary.x * tri[0].WavesNormalMapTexCoord0 + bary.y * tri[1].WavesNormalMapTexCoord0 + bary.z * tri[2].WavesNormalMapTexCoord0;
	dout.WavesNormalMapTexCoord1 = bary.x * tri[0].WavesNormalMapTexCoord1 + bary.y * tri[1].WavesNormalMapTexCoord1 + bary.z * tri[2].WavesNormalMapTexCoord1;
	dout.WavesDisplacementMapTexCoord0 = bary.x * tri[0].WavesDisplacementMapTexCoord0 + bary.y * tri[1].WavesDisplacementMapTexCoord0 + bary.z * tri[2].WavesDisplacementMapTexCoord0;
	dout.WavesDisplacementMapTexCoord1 = bary.x * tri[0].WavesDisplacementMapTexCoord1 + bary.y * tri[1].WavesDisplacementMapTexCoord1 + bary.z * tri[2].WavesDisplacementMapTexCoord1;

	dout.NormalW = normalize(dout.NormalW);

	// displacement mapping
	
	const float MipInterval = 20;
	float d = distance(dout.PositionW, gEyePositionW);
	float MipLevel = clamp((d - MipInterval) / MipInterval, 0, 6);

	//float h = gNormalTexture.SampleLevel(gSamplerState, dout.TexCoord, MipLevel).a;
	//dout.PositionW += (gHeightScale * (h - 1)) * dout.NormalW;

	// sample height map stored in alpha channel
	float h0 = gWavesNormalMap0.SampleLevel(gSamplerState, dout.WavesDisplacementMapTexCoord0, MipLevel).a;
	float h1 = gWavesNormalMap1.SampleLevel(gSamplerState, dout.WavesDisplacementMapTexCoord1, MipLevel).a;

	dout.PositionW.y += gHeightScale0 * h0;
	dout.PositionW.y += gHeightScale1 * h1;

	dout.PositionH = mul(gViewProj, float4(dout.PositionW, 1));

	return dout;
}