#pragma once

namespace vr_dead_pixel_test {

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

float ValueNoise3D(float3 position)
{
    float3 cell = floor(position);
    float3 fraction = frac(position);
    float3 blend = fraction * fraction * (3.0 - 2.0 * fraction);

    float lower00 = lerp(Hash31(cell + float3(0.0, 0.0, 0.0)),
                         Hash31(cell + float3(1.0, 0.0, 0.0)), blend.x);
    float lower10 = lerp(Hash31(cell + float3(0.0, 1.0, 0.0)),
                         Hash31(cell + float3(1.0, 1.0, 0.0)), blend.x);
    float upper00 = lerp(Hash31(cell + float3(0.0, 0.0, 1.0)),
                         Hash31(cell + float3(1.0, 0.0, 1.0)), blend.x);
    float upper10 = lerp(Hash31(cell + float3(0.0, 1.0, 1.0)),
                         Hash31(cell + float3(1.0, 1.0, 1.0)), blend.x);
    float lower = lerp(lower00, lower10, blend.y);
    float upper = lerp(upper00, upper10, blend.y);
    return lerp(lower, upper, blend.z);
}

float3 RotateNoiseSpace(float3 value)
{
    // Orthonormal rotation keeps the stochastic lattice from lining up with
    // the OpenXR LOCAL axes or with the dominant direction of the ribbons.
    return float3(
        dot(value, float3(0.36, 0.48, 0.80)),
        dot(value, float3(-0.80, 0.60, 0.00)),
        dot(value, float3(-0.48, -0.64, 0.60)));
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

    // Add smooth stochastic luminance variation inside every ribbon. The three
    // non-harmonic scales prevent a repeating wave or a second family of bands
    // from emerging, while their low total amplitude preserves the hard edges.
    float3 localSurface = surfacePosition - gSphereCenterRadius.xyz;
    float3 noiseSpace = RotateNoiseSpace(localSurface);
    float coarseNoise = ValueNoise3D(
        noiseSpace * 2.15 + float3(19.0, 7.0, -11.0)) - 0.5;
    float mediumNoise = ValueNoise3D(
        noiseSpace.yzx * 5.37 + float3(-5.0, 13.0, 23.0)) - 0.5;
    float fineNoise = ValueNoise3D(
        noiseSpace.zxy * 12.91 + float3(37.0, -17.0, 3.0)) - 0.5;
    float luminanceVariation =
        0.085 * coarseNoise + 0.043 * mediumNoise + 0.018 * fineNoise;
    color *= 1.0 + luminanceVariation;

    // Sub-LSB dither is indexed by the finite sphere's physical surface, not
    // by render-target pixels. It therefore follows the same geometry and
    // disparity in both eyes instead of producing binocularly independent noise.
    float dither =
        (Hash31(floor(noiseSpace * 260.0) + float3(17.0, 59.0, 101.0)) - 0.5) *
        gOutputParameters.y;
    if (gOutputParameters.x > 0.5)
    {
        float3 encoded = saturate(
            LinearToSrgb(color) * gOutputParameters.z + dither);
        color = SrgbToLinear(encoded);
    }
    else
    {
        color = saturate(color * gOutputParameters.z + dither);
    }

    return float4(color, 1.0);
}
)hlsl";

}  // namespace vr_dead_pixel_test
