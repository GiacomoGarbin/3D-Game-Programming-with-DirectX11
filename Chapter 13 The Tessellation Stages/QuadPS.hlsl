struct VertexOut
{
	float3 PositionL : POSITION;
	float4 PositionH : SV_POSITION;
};

struct DomainOut
{
	float4 PositionH : SV_POSITION;
};

float4 main(DomainOut pin) : SV_TARGET
{
	float4 color = float4(1, 0, 0, 1);

	return color;
}