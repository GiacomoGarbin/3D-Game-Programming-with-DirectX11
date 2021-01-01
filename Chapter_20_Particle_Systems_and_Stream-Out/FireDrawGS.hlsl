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

static float2 gQuadTexCoord[4] =
{
	float2(0.0f, 1.0f),
	float2(1.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f)
};

struct VertexOut
{
	float3 PositionW  : POSITION;
	float2 SizeW      : SIZE;
	float4 color      : COLOR;
	uint   type       : TYPE;
};

struct GeometryOut
{
	float4 PositionH : SV_Position;
	float4 color     : COLOR;
	float2 TexCoord  : TEXCOORD;
};

[maxvertexcount(4)]
void main(point VertexOut gin[1], inout TriangleStream<GeometryOut> stream)
{
	// do not draw emitter particles
	if (gin[0].type != 0)
	{
		// compute world matrix so that billboard faces the camera
		float3 look  = normalize(gEyePosW.xyz - gin[0].PositionW);
		float3 right = normalize(cross(float3(0, 1, 0), look));
		float3 up    = cross(look, right);

		// compute triangle strip vertices (quad) in world space
		float HalfWidth  = 0.5f * gin[0].SizeW.x;
		float HalfHeight = 0.5f * gin[0].SizeW.y;

		float4 v[4];
		v[0] = float4(gin[0].PositionW + HalfWidth * right - HalfHeight * up, 1.0f);
		v[1] = float4(gin[0].PositionW + HalfWidth * right + HalfHeight * up, 1.0f);
		v[2] = float4(gin[0].PositionW - HalfWidth * right - HalfHeight * up, 1.0f);
		v[3] = float4(gin[0].PositionW - HalfWidth * right + HalfHeight * up, 1.0f);

		// transform quad vertices to world space and output them as a triangle strip
		GeometryOut gout;
		[unroll]
		for (int i = 0; i < 4; ++i)
		{
			gout.PositionH = mul(gViewProj, v[i]);
			gout.TexCoord  = gQuadTexCoord[i];
			gout.color     = gin[0].color;
			
			stream.Append(gout);
		}
	}
}