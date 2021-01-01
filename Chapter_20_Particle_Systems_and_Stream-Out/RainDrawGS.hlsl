static float3 gAccelerationW = { -1.0f, -9.8f, 0.0f };

cbuffer ConstantBuffer  : register(b0)
{
	float4 gEyePosW;

	float4 gEmitPosW;
	float4 gEmitDirW;

	float gGameTime;
	float gTimeStep;
	float2 padding;

	float4x4 gViewProj;
};

struct VertexOut
{
	float3 PositionW  : POSITION;
	uint   type       : TYPE;
};

struct GeometryOut
{
	float4 PositionH : SV_Position;
	float2 TexCoord  : TEXCOORD;
};

[maxvertexcount(2)]
void main(point VertexOut gin[1], inout LineStream<GeometryOut> stream)
{
	// do not draw emitter particles
	if (gin[0].type != 0)
	{
		// slant line in acceleration direction
		float3 p0 = gin[0].PositionW;
		float3 p1 = gin[0].PositionW + 0.07f * gAccelerationW;

		GeometryOut v0;
		v0.PositionH = mul(gViewProj, float4(p0, 1.0f));
		v0.TexCoord = float2(0.0f, 0.0f);
		stream.Append(v0);

		GeometryOut v1;
		v1.PositionH = mul(gViewProj, float4(p1, 1.0f));
		v1.TexCoord = float2(1.0f, 1.0f);
		stream.Append(v1);
	}
}