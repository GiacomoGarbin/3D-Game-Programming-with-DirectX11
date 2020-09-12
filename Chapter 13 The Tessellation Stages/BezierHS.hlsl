struct VertexOut
{
	float3 PositionL : POSITION;
	float4 PositionH : SV_POSITION;
};

struct PatchTess
{
	float EdgeTess[4]   : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 16> patch, uint PatchID : SV_PrimitiveID)
{
	PatchTess pt;

	// uniformly tessellate the patch.

	float tess = 25;

	pt.EdgeTess[0] = tess;
	pt.EdgeTess[1] = tess;
	pt.EdgeTess[2] = tess;
	pt.EdgeTess[3] = tess;

	pt.InsideTess[0] = tess;
	pt.InsideTess[1] = tess;

	return pt;
}

struct HullOut
{
	float3 PositionL : POSITION;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(16)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64)]
HullOut main(InputPatch<VertexOut, 16> patch, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{
	HullOut hout;

	hout.PositionL = patch[i].PositionL;

	return hout;
}