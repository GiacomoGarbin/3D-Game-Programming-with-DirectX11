struct VertexIn
{
	float3 PositionW : POSITION;
	float3 VelocityW : VELOCITY;
	float2 SizeW     : SIZE;
	float  age       : AGE;
	uint   type      : TYPE;
};

VertexIn main(VertexIn vin)
{
	return vin;
}