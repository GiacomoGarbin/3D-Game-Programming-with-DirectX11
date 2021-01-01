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

float3 RandVec3(float offset)
{
	// use game time plus offset to sample random texture
	float t = gGameTime + offset;

	// coordinates in [-1,+1]
	float3 v = gRandomTexture.SampleLevel(gLinearSamplerState, t, 0).xyz;

	return v;
}

struct VertexIn
{
	float3 PositionW : POSITION;
	float3 VelocityW : VELOCITY;
	float2 SizeW     : SIZE;
	float  age       : AGE;
	uint   type      : TYPE;
};

[maxvertexcount(6)]
void main(point VertexIn gin[1], inout PointStream<VertexIn> stream)
{
	gin[0].age += gTimeStep;

	if (gin[0].type == 0) // emitter
	{
		// time to emit a new particle?
		if (gin[0].age > 0.002f)
		{
			for (int i = 0; i < 5; ++i)
			{
				// spread rain drops out above the camera
				float3 v = 35.0f * RandVec3((float)i / 5.0f);
				v.y = 20.0f;

				VertexIn p;
				p.PositionW = gEmitPosW.xyz + v;
				p.VelocityW = float3(0, 0, 0);
				p.SizeW = float2(1, 1);
				p.age = 0;
				p.type = 1; // rain drop

				stream.Append(p);
			}

			// reset the time to emit
			gin[0].age = 0;
		}

		// always keep emitters
		stream.Append(gin[0]);
	}
	else
	{
		if (gin[0].age <= 3)
		{
			stream.Append(gin[0]);
		}
	}
}