cbuffer cbFixed
{
	static const float gWeights[11] = { 0.05f, 0.05f, 0.1f, 0.1f, 0.1f, 0.2f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f };
	static const int gBlurRadius = 5;
};

Texture2D gTextureInput : register(t0);
RWTexture2D<float4> gTextureOutput : register(u0);

#define ThreadGroupSize 256
#define CacheSize (ThreadGroupSize + 2 * gBlurRadius)
groupshared float4 gCache[CacheSize];

[numthreads(1, ThreadGroupSize, 1)]
void main(int3 GroupThreadID : SV_GroupThreadID, int3 DispatchThreadID : SV_DispatchThreadID)
{
	if (GroupThreadID.y < gBlurRadius)
	{
		int y = max(DispatchThreadID.y - gBlurRadius, 0);
		gCache[GroupThreadID.y] = gTextureInput[int2(DispatchThreadID.x, y)];
	}

	if (GroupThreadID.y >= ThreadGroupSize - gBlurRadius)
	{
		int y = min(DispatchThreadID.y + gBlurRadius, gTextureInput.Length.y - 1);
		gCache[GroupThreadID.y + 2 * gBlurRadius] = gTextureInput[int2(DispatchThreadID.x, y)];
	}

	gCache[GroupThreadID.y + gBlurRadius] = gTextureInput[min(DispatchThreadID.xy, gTextureInput.Length.xy - 1)];

	GroupMemoryBarrierWithGroupSync();

	float4 BlurColor = float4(0, 0, 0, 0);

	[unroll]
	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		int k = GroupThreadID.y + gBlurRadius + i;

		BlurColor += gWeights[i + gBlurRadius] * gCache[k];
	}

	gTextureOutput[DispatchThreadID.xy] = BlurColor;
}