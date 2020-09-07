cbuffer FixedCB : register(b0)
{
	uint2 gTexCoord;
	float gMagnitude;
	float gHalfMagnitude;
};

Texture2D gCurrInput : register(t0);
RWTexture2D<float> gNextOutput : register(u0);

#define ThreadGroupSize 256

[numthreads(ThreadGroupSize, 1, 1)]
void main(uint3 GroupThreadID : SV_GroupThreadID, uint3 DispatchThreadID : SV_DispatchThreadID)
{
	gNextOutput[DispatchThreadID.xy] = gCurrInput[DispatchThreadID.xy];

	//if (DispatchThreadID.xy == gTexCoord)
	//{
	//	gNextOutput[DispatchThreadID.xy] += gMagnitude;
	//}
	
	if (DispatchThreadID.x == gTexCoord.x && (DispatchThreadID.y >= gTexCoord.y - 1 && DispatchThreadID.y <= gTexCoord.y + 1))
	{
		gNextOutput[DispatchThreadID.xy] += gHalfMagnitude;
	}
	
	if (DispatchThreadID.y == gTexCoord.y && (DispatchThreadID.x >= gTexCoord.x - 1 && DispatchThreadID.x <= gTexCoord.x + 1))
	{
		gNextOutput[DispatchThreadID.xy] += gHalfMagnitude;
	}
}