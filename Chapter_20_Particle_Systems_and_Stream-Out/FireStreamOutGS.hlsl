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

Texture1D gRandomTexture : register(t0);
SamplerState gLinearSamplerState : register(s0);

float3 RandUnitVec3(float offset)
{
	// use game time plus offset to sample random texture
	float t = gGameTime + offset;

	// coordinates in [-1,+1]
	float3 v = gRandomTexture.SampleLevel(gLinearSamplerState, t, 0).xyz;

	// project onto unit sphere
	return normalize(v);
}

struct VertexIn
{
	float3 PositionW : POSITION;
	float3 VelocityW : VELOCITY;
	float2 SizeW     : SIZE;
	float  age       : AGE;
	uint   type      : TYPE;
};

[maxvertexcount(2)]
void main(point VertexIn gin[1], inout PointStream<VertexIn> stream)
{
	gin[0].age += gTimeStep;

	if (gin[0].type == 0) // emitter
	{
		// time to emit a new particle?
		if (gin[0].age > 0.005f)
		{
			float3 v = RandUnitVec3(0);
			v.x *= 0.5f;
			v.z *= 0.5f;

			VertexIn p;
			p.PositionW = gEmitPosW.xyz;
			p.VelocityW = 4 * v;
			p.SizeW = float2(3, 3);
			p.age = 0;
			p.type = 1; // flare

			stream.Append(p);

			// reset the time to emit
			gin[0].age = 0;
		}

		// always keep emitters
		stream.Append(gin[0]);
	}
	else
	{
		if (gin[0].age <= 1)
		{
			stream.Append(gin[0]);
		}
	}
}