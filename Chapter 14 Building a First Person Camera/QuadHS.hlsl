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

struct VertexOut
{
	float3 PositionL : POSITION;
	float4 PositionH : SV_POSITION;
};

struct PatchTess
{
	float EdgeTess[4]   : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 4> patch, uint PatchID : SV_PrimitiveID)
{
	PatchTess pt;

	float3 CenterL = 0.25f * (patch[0].PositionL + patch[1].PositionL + patch[2].PositionL + patch[3].PositionL);
	float3 CenterW = mul(gWorld, float4(CenterL, 1)).xyz;

	float d = distance(CenterW, gEyePositionW);

	// tessellate the patch based on distance from the eye, the tessellation is 0 if d >= d1 and 64 if d <= d0

	const float d0 = 20;
	const float d1 = 100;
	float tess = 64 * saturate((d1 - d) / (d1 - d0));

	// uniformly tessellate the patch.

	pt.EdgeTess[0] = tess;
	pt.EdgeTess[1] = tess;
	pt.EdgeTess[2] = tess;
	pt.EdgeTess[3] = tess;

	pt.InsideTess[0] = tess;
	pt.InsideTess[1] = tess;

	return pt;
}

struct HullOut
{
	float3 PositionL : POSITION;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64)]
HullOut main(InputPatch<VertexOut, 4> patch, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{
	HullOut hout;

	hout.PositionL = patch[i].PositionL;

	return hout;
}