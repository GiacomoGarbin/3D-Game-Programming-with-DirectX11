cbuffer FixedCB : register(b0)
{
	float gK1;
	float gK2;
	float gK3;
};

Texture2D gPrevInput : register(t0);
Texture2D gCurrInput : register(t1);
RWTexture2D<float> gNextOutput : register(u0);

#define ThreadGroupSize 256

[numthreads(ThreadGroupSize, 1, 1)]
void main(uint3 GroupThreadID : SV_GroupThreadID, uint3 DispatchThreadID : SV_DispatchThreadID)
{
	//gCurrOutput[DispatchThreadID.xy] = gCurrInput[DispatchThreadID.xy];
	//gNextOutput[DispatchThreadID.xy] = gPrevInput[DispatchThreadID.xy];

	//mPrevVertices.at(i * mCols + j).mPosition.y =
	//	gK1 * mPrevVertices.at(i * mCols + j).mPosition.y +
	//	gK2 * mCurrVertices.at(i * mCols + j).mPosition.y +
	//	gK3 * (mCurrVertices.at((i + 1) * mCols + j).mPosition.y +
	//		   mCurrVertices.at((i - 1) * mCols + j).mPosition.y +
	//		   mCurrVertices.at(i * mCols + (j + 1)).mPosition.y +
	//		   mCurrVertices.at(i * mCols + (j - 1)).mPosition.y);

	uint2 t = DispatchThreadID.xy; t.y = max(t.y - 1, 0);
	uint2 b = DispatchThreadID.xy; b.y = min(b.y + 1, gCurrInput.Length.y);
	uint2 l = DispatchThreadID.xy; l.x = max(l.x - 1, 0);
	uint2 r = DispatchThreadID.xy; r.x = min(r.x + 1, gCurrInput.Length.x);

	gNextOutput[DispatchThreadID.xy] =
		gK1 * gPrevInput[DispatchThreadID.xy] +
		gK2 * gCurrInput[DispatchThreadID.xy] +
		gK3 * (gCurrInput[t] + gCurrInput[b] + gCurrInput[l] + gCurrInput[r]);
}