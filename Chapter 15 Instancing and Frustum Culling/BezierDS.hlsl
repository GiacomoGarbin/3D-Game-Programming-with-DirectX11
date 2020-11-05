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

float4 BernsteinCoeff(float t)
{
	float T = 1 - t;

	return float4(T * T * T, 3 * t * T * T, 3 * t * t * T, t * t * t);
}

//float4 BernsteinCoeffDerivate(float t)
//{
//	float T = 1 - t;
//
//	return float4(-3 * T * T, 3 * T * T - 6 * t * T, 6 * t * T - 3 * t * t, 3 * t * t);
//}

float3 CubicBezierSum(const OutputPatch<HullOut, 16> patch, float4 CoeffU, float4 CoeffV)
{
	float3 sum = float3(0, 0, 0);

	sum  = CoeffV.x * (CoeffU.x * patch[ 0].PositionL + CoeffU.y * patch[ 1].PositionL + CoeffU.z * patch[ 2].PositionL + CoeffU.w * patch[ 3].PositionL);
	sum += CoeffV.y * (CoeffU.x * patch[ 4].PositionL + CoeffU.y * patch[ 5].PositionL + CoeffU.z * patch[ 6].PositionL + CoeffU.w * patch[ 7].PositionL);
	sum += CoeffV.z * (CoeffU.x * patch[ 8].PositionL + CoeffU.y * patch[ 9].PositionL + CoeffU.z * patch[10].PositionL + CoeffU.w * patch[11].PositionL);
	sum += CoeffV.w * (CoeffU.x * patch[12].PositionL + CoeffU.y * patch[13].PositionL + CoeffU.z * patch[14].PositionL + CoeffU.w * patch[15].PositionL);

	return sum;
}

[domain("quad")]
DomainOut main(PatchTess pt, float2 uv : SV_DomainLocation, const OutputPatch<HullOut, 16> patch)
{
	DomainOut dout;

	float4 CoeffU = BernsteinCoeff(uv.x);
	float4 CoeffV = BernsteinCoeff(uv.y);

	float3 p = CubicBezierSum(patch, CoeffU, CoeffV);

	dout.PositionH = mul(gWorldViewProj, float4(p, 1));

	return dout;
}