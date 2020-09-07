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

[numthreads(ThreadGroupSize, 1, 1)]
void main(int3 GroupThreadID : SV_GroupThreadID, int3 DispatchThreadID : SV_DispatchThreadID)
{
	if (GroupThreadID.x < gBlurRadius)
	{
		int x = max(DispatchThreadID.x - gBlurRadius, 0);
		gCache[GroupThreadID.x] = gTextureInput[int2(x, DispatchThreadID.y)];
	}

	if (GroupThreadID.x >= ThreadGroupSize - gBlurRadius)
	{
		int x = min(DispatchThreadID.x + gBlurRadius, gTextureInput.Length.x - 1);
		gCache[GroupThreadID.x + 2 * gBlurRadius] = gTextureInput[int2(x, DispatchThreadID.y)];
	}

	gCache[GroupThreadID.x + gBlurRadius] = gTextureInput[min(DispatchThreadID.xy, gTextureInput.Length.xy - 1)];

	GroupMemoryBarrierWithGroupSync();

	float4 BlurColor = float4(0, 0, 0, 0);

	[unroll]
	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		int k = GroupThreadID.x + gBlurRadius + i;

		BlurColor += gWeights[i + gBlurRadius] * gCache[k];
	}

	gTextureOutput[DispatchThreadID.xy] = BlurColor;
}