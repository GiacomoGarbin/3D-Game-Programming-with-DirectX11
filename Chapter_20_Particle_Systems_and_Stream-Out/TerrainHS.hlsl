struct LightDirectional
{
	float4 ambient;
	float4 diffuse;
	float4 specular;

	float3 direction;
	float  pad;
};

cbuffer PerFrameCB : register(b0)
{
	LightDirectional gLights[3];

	float3 gEyePositionW;
	float  pad1;

	float  gFogStart;
	float  gFogRange;
	float2 pad2;
	float4 gFogColor;

	float gHeightScale;
	float gMaxTessDistance;
	float gMinTessDistance;
	float gMinTessFactor;
	float gMaxTessFactor;
	float3 pad3;

	float4x4 gViewProj;

	float mTexelCellSpaceU;
	float mTexelCellSpaceV;
	float mWorldCellSpace;
	float pad4;
	float2 gTexelScale;
	float2 pad5;

	float4 gWorldFrustumPlanes[6];
};

struct VertexOut
{
	float3 PositionW : POSITION;
	float2 TexCoord  : TEXCOORD0;
	float2 BoundsY   : TEXCOORD1;
};

float ComputeTessFactor(float3 p)
{
	float d = distance(p, gEyePositionW);

	// max norm in xz plane (useful to see detail levels from a bird's eye).
	//float d = max( abs(p.x-gEyePosW.x), abs(p.z-gEyePosW.z) );

	float t = saturate((d - gMinTessDistance) / (gMaxTessDistance - gMinTessDistance));
	return pow(2, lerp(gMaxTessFactor, gMinTessFactor, t));
}

// true if the box is completely behind (in negative half space) of plane
bool AABBBehindPlaneTest(float3 center, float3 extents, float4 plane)
{
	float3 n = abs(plane.xyz);

	// this is always positive
	float r = dot(extents, n);

	// signed distance from center point to plane
	float s = dot(float4(center, 1.0f), plane);

	// if the center point of the box is a distance of e or more behind the
	// plane (in which case s is negative since it is behind the plane),
	// then the box is completely in the negative half space of the plane
	return (s + r) < 0.0f;
}

// true if the box is completely outside the frustum
bool AABBOutsideFrustumTest(float3 center, float3 extents, float4 FrustumPlanes[6])
{
	for (int i = 0; i < 6; ++i)
	{
		// if the box is completely behind any of the frustum planes then it is outside the frustum
		if (AABBBehindPlaneTest(center, extents, FrustumPlanes[i]))
		{
			return true;
		}
	}

	return false;
}

struct PatchTess
{
	float EdgeTess[4]   : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 4> patch, uint PatchID : SV_PrimitiveID)
{
	PatchTess pt;

	// frustum cull

	// we store the patch BoundsY in the first control point
	float MinY = patch[0].BoundsY.x;
	float MaxY = patch[0].BoundsY.y;

	// build axis-aligned bounding box
	//  patch[2] is lower-left corner
	//  patch[1] is upper-right corner
	float3 vMin = float3(patch[2].PositionW.x, MinY, patch[2].PositionW.z);
	float3 vMax = float3(patch[1].PositionW.x, MaxY, patch[1].PositionW.z);

	float3 BoxCenter  = 0.5f * (vMin + vMax);
	float3 BoxExtents = 0.5f * (vMax - vMin);
	if (AABBOutsideFrustumTest(BoxCenter, BoxExtents, gWorldFrustumPlanes))
	{
		pt.EdgeTess[0] = 0.0f;
		pt.EdgeTess[1] = 0.0f;
		pt.EdgeTess[2] = 0.0f;
		pt.EdgeTess[3] = 0.0f;

		pt.InsideTess[0] = 0.0f;
		pt.InsideTess[1] = 0.0f;

		return pt;
	}
	else // tessellation based on distance
	{
		// compute midpoint on edges, and patch center
		float3 e0 = 0.5f * (patch[0].PositionW + patch[2].PositionW);
		float3 e1 = 0.5f * (patch[0].PositionW + patch[1].PositionW);
		float3 e2 = 0.5f * (patch[1].PositionW + patch[3].PositionW);
		float3 e3 = 0.5f * (patch[2].PositionW + patch[3].PositionW);
		float3 c = 0.25f * (patch[0].PositionW + patch[1].PositionW + patch[2].PositionW + patch[3].PositionW);

		pt.EdgeTess[0] = ComputeTessFactor(e0);
		pt.EdgeTess[1] = ComputeTessFactor(e1);
		pt.EdgeTess[2] = ComputeTessFactor(e2);
		pt.EdgeTess[3] = ComputeTessFactor(e3);

		pt.InsideTess[0] = ComputeTessFactor(c);
		pt.InsideTess[1] = pt.InsideTess[0];

		return pt;
	}
}

struct HullOut
{
	float3 PositionW : POSITION;
	float2 TexCoord  : TEXCOORD0;
};

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut main(InputPatch<VertexOut, 4> patch,
			 uint i : SV_OutputControlPointID,
			 uint PatchId : SV_PrimitiveID)
{
	HullOut hout;

	hout.PositionW = patch[i].PositionW;
	hout.TexCoord = patch[i].TexCoord;

	return hout;
}