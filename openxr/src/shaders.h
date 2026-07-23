#pragma once

namespace pixel_flow {

inline constexpr const char* kShaderSource = R"hlsl(
cbuffer PatternConstants : register(b0)
{
    float4 gColors[6];
    float4 gContour;
    float4 gFleck;
    float4 gOrientation;
    float4 gFov;
    float4 gEyePosition;
    float4 gSphereCenterRadius;
    float4 gOutputParameters;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput VertexMain(uint vertexId : SV_VertexID)
{
    float2 corner = float2((vertexId << 1) & 2, vertexId & 2);
    VertexOutput output;
    output.uv = corner;
    output.position = float4(corner * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float3 RotateByQuaternion(float3 value, float4 quaternion)
{
    return value + 2.0 * cross(quaternion.xyz,
        cross(quaternion.xyz, value) + quaternion.w * value);
}

float Hash31(float3 value)
{
    value = frac(value * 0.1031);
    value += dot(value, value.yzx + 33.33);
    return frac((value.x + value.y) * value.z);
}

float3 LinearToSrgb(float3 value)
{
    float3 low = value * 12.92;
    float3 high = 1.055 * pow(max(value, 0.0), 1.0 / 2.4) - 0.055;
    return lerp(low, high, step(0.0031308, value));
}

float3 SrgbToLinear(float3 value)
{
    float3 low = value / 12.92;
    float3 high = pow((max(value, 0.0) + 0.055) / 1.055, 2.4);
    return lerp(low, high, step(0.04045, value));
}

float4 PixelMain(VertexOutput input) : SV_TARGET
{
    float viewX = lerp(gFov.x, gFov.y, input.uv.x);
    float viewY = lerp(gFov.z, gFov.w, input.uv.y);
    float3 viewDirection = normalize(float3(viewX, viewY, -1.0));
    float3 rayDirection = normalize(RotateByQuaternion(viewDirection, gOrientation));

    // Intersect this eye's world-space ray with the finite inspection sphere.
    // The larger root is the forward-facing surface seen from inside the sphere.
    float3 centerToEye = gEyePosition.xyz - gSphereCenterRadius.xyz;
    float projectedOrigin = dot(centerToEye, rayDirection);
    float sphereEquation =
        dot(centerToEye, centerToEye) -
        gSphereCenterRadius.w * gSphereCenterRadius.w;
    float discriminant = max(
        projectedOrigin * projectedOrigin - sphereEquation, 0.0);
    float distanceToSurface = -projectedOrigin + sqrt(discriminant);
    float3 surfacePosition =
        gEyePosition.xyz + rayDirection * distanceToSurface;
    float3 worldDirection = normalize(
        surfacePosition - gSphereCenterRadius.xyz);

    // Deliberately quantize a smooth, curved scalar field into constant-color
    // ribbons. Every transition is an exact geometric boundary on the shared
    // sphere, giving both eyes a strong and unambiguous stereo correspondence.
    // The field is evaluated in 3D rather than UV space, so it has no seam.
    float bandPhase =
        dot(worldDirection, float3(2.8, 8.5, 1.2)) +
        1.10 * sin(dot(worldDirection, float3(-3.2, 1.4, 3.8))) +
        0.32 * sin(dot(worldDirection, float3(5.1, 2.3, -2.7)));
    float bandIndex = floor(frac(bandPhase / 5.0) * 5.0);
    float3 color = gColors[(int)bandIndex + 1].rgb;

    // Sub-LSB dither is indexed by the finite sphere's physical surface, not
    // by render-target pixels. It therefore follows the same geometry and
    // disparity in both eyes instead of producing binocularly independent noise.
    float dither =
        (Hash31(floor(surfacePosition * 260.0)) - 0.5) * gOutputParameters.y;
    if (gOutputParameters.x > 0.5)
    {
        float3 encoded = saturate(LinearToSrgb(color) + dither);
        color = SrgbToLinear(encoded);
    }
    else
    {
        color = saturate(color + dither);
    }

    return float4(color, 1.0);
}
)hlsl";

}  // namespace pixel_flow
