struct Material
{
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float4 reflect;
};

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

	float gTexelCellSpaceU;
	float gTexelCellSpaceV;
	float gWorldCellSpace;
	float pad4;
	float2 gTexelScale;
	float2 pad5;

	float4 gWorldFrustumPlanes[6];
};

cbuffer PerObjectCB : register(b1)
{
	float4x4 gWorld;
	float4x4 gWorldInverseTranspose;
	float4x4 gWorldViewProj;
	Material gMaterial;
	float4x4 gTexCoordTransform;
	float4x4 gShadowTransform;
	float4x4 gWorldViewProjTexture;
};

Texture2D gHeightMapTexture : register(t0);
Texture2DArray gLayerMapTextureArray : register(t1);
Texture2D gBlendMapTexture : register(t2);
Texture2D gShadowMapTexture : register(t3);
Texture2D gAmbientMapTexture : register(t4);

SamplerState gHeightMapSamplerState : register(s0);
SamplerState gLinearSamplerState : register(s1);
SamplerComparisonState gShadowMapSamplerState : register(s2);

void ComputeLightDirectional(
	Material material,
	LightDirectional light,
	float3 N, // normal
	float3 E, // eye vector
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0, 0, 0, 0);
	diffuse = float4(0, 0, 0, 0);
	specular = float4(0, 0, 0, 0);

	float3 L = -light.direction; // the light vector aims opposite the light ray direction

	ambient = material.ambient * light.ambient;

	float d = dot(L, N); // diffuse factor

	[flatten]
	if (d > 0)
	{
		diffuse = d * material.diffuse * light.diffuse;

		float3 R = reflect(-L, N);
		float s = pow(max(dot(R, E), 0), material.specular.w); // specular factor

		specular = s * material.specular * light.specular;
	}
}

float GetShadowFactor(float4 ShadowH)
{
	static const float ShadowMapSize = 2048;
	static const float ShadowMapDelta = 1.0f / ShadowMapSize;

	// complete projection by doing division by w
	ShadowH.xyz /= ShadowH.w;

	// depth in NDC space
	float depth = ShadowH.z;

	// texel size
	const float dx = ShadowMapDelta;

	const float2 offsets[9] =
	{
		float2(-dx, -dx), float2(0, -dx), float2(dx, -dx),
		float2(-dx,   0), float2(0,   0), float2(dx,   0),
		float2(-dx, +dx), float2(0, +dx), float2(dx, +dx)
	};

	float light = 0;

	[unroll]
	for (int i = 0; i < 9; ++i)
	{
		light += gShadowMapTexture.SampleCmpLevelZero(gShadowMapSamplerState, ShadowH.xy + offsets[i], depth).r;
	}

	return light / 9.0f;
}

struct DomainOut
{
	float4 PositionH     : SV_POSITION;
	float3 PositionW     : POSITION;
	float2 TexCoord      : TEXCOORD0;
	float2 TiledTexCoord : TEXCOORD1;
	float4 ShadowH       : TEXCOORD2;
	float4 AmbientH      : TEXCOORD3;
};

float4 main(DomainOut pin) : SV_Target
{
	// estimate normal and tangent using central differences
	float3 N;
	{
		float2 l = pin.TexCoord + float2(-gTexelCellSpaceU, 0.0f); // left texel
		float2 r = pin.TexCoord + float2(+gTexelCellSpaceU, 0.0f); // right texel
		float2 b = pin.TexCoord + float2(0.0f, +gTexelCellSpaceV); // bottom texel
		float2 t = pin.TexCoord + float2(0.0f, -gTexelCellSpaceV); // top texel

		float lY = gHeightMapTexture.SampleLevel(gHeightMapSamplerState, l, 0).r;
		float rY = gHeightMapTexture.SampleLevel(gHeightMapSamplerState, r, 0).r;
		float bY = gHeightMapTexture.SampleLevel(gHeightMapSamplerState, b, 0).r;
		float tY = gHeightMapTexture.SampleLevel(gHeightMapSamplerState, t, 0).r;

		// tangent, bitangent, normal
		float3 T = normalize(float3(2.0f * gWorldCellSpace, rY - lY, 0.0f));
		float3 B = normalize(float3(0.0f, bY - tY, -2.0f * gWorldCellSpace));
		N = cross(T, B);
	}

	float3 E = gEyePositionW - pin.PositionW; // the eye vector is oriented from the surface to the eye position
	float DistToEye = length(E);
	E /= DistToEye;

	// sample layers in texture array
	float4 LayerColor0 = gLayerMapTextureArray.Sample(gLinearSamplerState, float3(pin.TiledTexCoord, 0));
	float4 LayerColor1 = gLayerMapTextureArray.Sample(gLinearSamplerState, float3(pin.TiledTexCoord, 1));
	float4 LayerColor2 = gLayerMapTextureArray.Sample(gLinearSamplerState, float3(pin.TiledTexCoord, 2));
	float4 LayerColor3 = gLayerMapTextureArray.Sample(gLinearSamplerState, float3(pin.TiledTexCoord, 3));
	float4 LayerColor4 = gLayerMapTextureArray.Sample(gLinearSamplerState, float3(pin.TiledTexCoord, 4));
	
	// sample the blend map
	float4 t = gBlendMapTexture.Sample(gLinearSamplerState, pin.TexCoord);

	// blend the layers on top of each other
	float4 TextureColor = LayerColor0;
	TextureColor = lerp(TextureColor, LayerColor1, t.r);
	TextureColor = lerp(TextureColor, LayerColor2, t.g);
	TextureColor = lerp(TextureColor, LayerColor3, t.b);
	TextureColor = lerp(TextureColor, LayerColor4, t.a);

	float4 color = TextureColor;

	// if light
	{
		float4 ambient  = float4(0, 0, 0, 0);
		float4 diffuse  = float4(0, 0, 0, 0);
		float4 specular = float4(0, 0, 0, 0);

		// only the first light casts a shadow
		float3 shadow = float3(1, 1, 1);
		shadow[0] = GetShadowFactor(pin.ShadowH);

		// finish texture projection and sample ambient map
		//pin.AmbientH /= pin.AmbientH.w;
		float AmbientFactor = 1; //gAmbientTexture.SampleLevel(gLinearSamplerState, pin.AmbientH.xy, 0).r;

		[unroll]
		for (int i = 0; i < 3; ++i)
		{
			float4 a, d, s;

			ComputeLightDirectional(gMaterial, gLights[i], N, E, a, d, s);
			ambient  += a * AmbientFactor;
			diffuse  += d * shadow[i];
			specular += s * shadow[i];
		}

		color = TextureColor * (ambient + diffuse) + specular;

#if ENABLE_REFLECTION
		float3 ReflectVec = reflect(-E, N);
		//float3 RefractVec = refract(-E, pin.NormalW, 1);
		float4 ReflectCol = gCubeMap.Sample(gLinearSamplerState, ReflectVec);
		color += gMaterial.reflect * ReflectCol;
#endif // ENABLE_REFLECTION

	}

#if ENABLE_FOG
	float t = saturate((DistToEye - gFogStart) / gFogRange);
	color = lerp(color, gFogColor, t);
#endif // ENABLE_FOG

	color.a = TextureColor.a * gMaterial.diffuse.a;

	return color;
}