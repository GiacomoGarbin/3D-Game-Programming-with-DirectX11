struct Material
{
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float4 reflect;
};

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gWorldInverseTranspose;
	float4x4 gWorldViewProj;
	Material gMaterial;
	float4x4 gTexTransform;
};

struct PatchTess
{
	float EdgeTess[4]   : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

struct HullOut
{
	float3 PositionL : POSITION;
};

struct DomainOut
{
	float4 PositionH : SV_POSITION;
};

[domain("quad")]
DomainOut main(PatchTess pt, float2 uv : SV_DomainLocation, const OutputPatch<HullOut, 4> patch)
{
	DomainOut dout;

	// bilinear interpolation.
	float3 v1 = lerp(patch[0].PositionL, patch[1].PositionL, uv.x);
	float3 v2 = lerp(patch[2].PositionL, patch[3].PositionL, uv.x);
	float3 p = lerp(v1, v2, uv.y);

	// displacement mapping
	p.y = 0.3f * (p.z * sin(p.x) + p.x * cos(p.z));

	dout.PositionH = mul(gWorldViewProj, float4(p, 1));

	return dout;
}