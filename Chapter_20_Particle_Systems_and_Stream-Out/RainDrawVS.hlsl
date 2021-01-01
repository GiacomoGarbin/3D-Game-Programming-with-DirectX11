static float3 gAccelerationW = { -1.0f, -9.8f, 0.0f };

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
	uint   type       : TYPE;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	float t = vin.age;
	vout.PositionW = 0.5f * t * t * gAccelerationW + t * vin.VelocityW + vin.PositionW;

	vout.type = vin.type;

	return vout;
}