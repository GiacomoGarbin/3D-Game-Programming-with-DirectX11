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
	float pad;
};

struct LightPoint
{
	float4 ambient;
	float4 diffuse;
	float4 specular;

	float3 position;
	float range;
	float3 attenuation;
	float pad;
};

struct LightSpot
{
	float4 ambient;
	float4 diffuse;
	float4 specular;

	float3 position;
	float range;
	float3 direction;
	float cone;
	float3 attenuation;
	float pad;
};

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gWorldInverseTranspose;
	float4x4 gWorldViewProj;
	Material gMaterial;
};

cbuffer cbPerFrame : register(b1)
{
	LightDirectional gLightDirectional;
	LightPoint gLightPoint;
	LightSpot gLightSpot;

	float3 gEyePositionW;
	float pad;
};

struct VertexIn
{
	float3 PositionL : POSITION;
	float3 NormalL   : NORMAL;
};

struct VertexOut
{
	float3 PositionW : POSITION;
	float4 PositionH : SV_POSITION;
	float3 NormalW   : NORMAL;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	vout.PositionW = mul(gWorld, float4(vin.PositionL, 1)).xyz;
	vout.PositionH = mul(gWorldViewProj, float4(vin.PositionL, 1));
	vout.NormalW = mul((float3x3)gWorldInverseTranspose, vin.NormalL);

	return vout;
}

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

void ComputeLightPoint(
	Material material,
	LightPoint light,
	float3 P, // position
	float3 N, // normal
	float3 E, // eye vector
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0, 0, 0, 0);
	diffuse = float4(0, 0, 0, 0);
	specular = float4(0, 0, 0, 0);

	float3 L = light.position - P; // the light vector is oriented from the surface to the light

	float distance = length(L);

	if (distance > light.range) return;

	L /= distance; // normalize light vector

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

	float attenuation = 1 / dot(light.attenuation, float3(1, distance, distance * distance));

	diffuse *= attenuation;
	specular *= attenuation;
}

void ComputeLightSpot(
	Material material,
	LightSpot light,
	float3 P, // position
	float3 N, // normal
	float3 E, // eye vector
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0, 0, 0, 0);
	diffuse = float4(0, 0, 0, 0);
	specular = float4(0, 0, 0, 0);

	float3 L = light.position - P; // the light vector is oriented from the surface to the light

	float distance = length(L);

	if (distance > light.range) return;

	L /= distance; // normalize light vector

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

	float cone = pow(max(dot(-L, light.direction), 0), light.cone);

	float attenuation = cone / dot(light.attenuation, float3(1, distance, distance * distance));

	ambient *= cone;
	diffuse *= attenuation;
	specular *= attenuation;
}

float4 PS(VertexOut pin) : SV_TARGET
{
	pin.NormalW = normalize(pin.NormalW); // interpolated normals can be unnormalized

	float3 E = normalize(gEyePositionW - pin.PositionW); // the eye vector is oriented from the surface to the eye position

	float4 ambient = float4(0, 0, 0, 0);
	float4 diffuse = float4(0, 0, 0, 0);
	float4 specular = float4(0, 0, 0, 0);

	float4 a, d, s;

	ComputeLightDirectional(gMaterial, gLightDirectional, pin.NormalW, E, a, d, s);
	ambient += a;
	diffuse += d;
	specular += s;

	ComputeLightPoint(gMaterial, gLightPoint, pin.PositionW, pin.NormalW, E, a, d, s);
	ambient += a;
	diffuse += d;
	specular += s;

	ComputeLightSpot(gMaterial, gLightSpot, pin.PositionW, pin.NormalW, E, a, d, s);
	ambient += a;
	diffuse += d;
	specular += s;

	float4 color = ambient + diffuse + specular;
	color.a = diffuse.a;

	return color;
}