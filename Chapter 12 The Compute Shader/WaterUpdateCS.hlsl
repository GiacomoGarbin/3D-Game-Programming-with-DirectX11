cbuffer FixedCB : register(b0)
{
	float gK1;
	float gK2;
	float gK3;
};

Texture2D gPrevInput : register(t0);
Texture2D gCurrInput : register(t1);
RWTexture2D<float> gCurrOutput : register(u0);
RWTexture2D<float> gNextOutput : register(u1);

#define ThreadGroupSize 256
//#define CacheSize (ThreadGroupSize + 2 * gBlurRadius)
//groupshared float4 gCache[CacheSize];

[numthreads(ThreadGroupSize, 1, 1)]
void main(uint3 GroupThreadID : SV_GroupThreadID, uint3 DispatchThreadID : SV_DispatchThreadID)
{
	// gCurrInput -> gCurrOutput
	// ...        -> gNextOutput

	gCurrOutput[DispatchThreadID.xy] = gCurrInput[DispatchThreadID.xy];

	//mPrevVertices.at(i * mCols + j).mPosition.y =
	//	gK1 * mPrevVertices.at(i * mCols + j).mPosition.y +
	//	gK2 * mCurrVertices.at(i * mCols + j).mPosition.y +
	//	gK3 * (mCurrVertices.at((i + 1) * mCols + j).mPosition.y +
	//		   mCurrVertices.at((i - 1) * mCols + j).mPosition.y +
	//		   mCurrVertices.at(i * mCols + (j + 1)).mPosition.y +
	//		   mCurrVertices.at(i * mCols + (j - 1)).mPosition.y);

	//if (GroupThreadID.x < gBlurRadius)
	//{
	//	int x = max(DispatchThreadID.x - gBlurRadius, 0);
	//	gCache[GroupThreadID.x] = gTextureInput[int2(x, DispatchThreadID.y)];
	//}

	//if (GroupThreadID.x >= ThreadGroupSize - gBlurRadius)
	//{
	//	int x = min(DispatchThreadID.x + gBlurRadius, gTextureInput.Length.x - 1);
	//	gCache[GroupThreadID.x + 2 * gBlurRadius] = gTextureInput[int2(x, DispatchThreadID.y)];
	//}

	//gCache[GroupThreadID.x + gBlurRadius] = gTextureInput[min(DispatchThreadID.xy, gTextureInput.Length.xy - 1)];

	//GroupMemoryBarrierWithGroupSync();

	//float4 BlurColor = float4(0, 0, 0, 0);

	//[unroll]
	//for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	//{
	//	int k = GroupThreadID.x + gBlurRadius + i;

	//	BlurColor += gWeights[i + gBlurRadius] * gCache[k];
	//}

	//gTextureOutput[DispatchThreadID.xy] = BlurColor;
}