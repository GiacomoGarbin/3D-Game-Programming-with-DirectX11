Texture2DArray gTextureArray : register(t0);
SamplerState gLinearSamplerState : register(s0);

struct GeometryOut
{
	float4 PositionH : SV_Position;
	float2 TexCoord  : TEXCOORD;
};

float4 main(GeometryOut pin) : SV_TARGET
{
	return gTextureArray.Sample(gLinearSamplerState, float3(pin.TexCoord, 0));
}