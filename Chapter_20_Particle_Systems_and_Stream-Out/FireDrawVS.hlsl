static float3 gAccelerationW = { 0.0f, 7.8f, 0.0f };

struct VertexIn
{
	float3 PositionW : POSITION;
	float3 VelocityW : VELOCITY;
	float2 SizeW     : SIZE;
	float  age       : AGE;
	uint   type      : TYPE;
};

struct VertexOut
{
	float3 PositionW  : POSITION;
	float2 SizeW      : SIZE;
	float4 color      : COLOR;
	uint   type       : TYPE;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	float t = vin.age;
	vout.PositionW = 0.5f * t * t * gAccelerationW + t * vin.VelocityW + vin.PositionW;

	// fade color with time
	float opacity = 1.0f - smoothstep(0.0f, 1.0f, t / 1.0f);
	vout.color = float4(1.0f, 1.0f, 1.0f, opacity);

	vout.SizeW = vin.SizeW;
	vout.type = vin.type;

	return vout;
}