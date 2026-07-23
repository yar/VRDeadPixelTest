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

float Hash21(float2 value)
{
    value = frac(value * float2(123.34, 456.21));
    value += dot(value, value + 45.32);
    return frac(value.x * value.y);
}

float4 PixelMain(VertexOutput input) : SV_TARGET
{
    const float pi = 3.14159265358979323846;
    const float tau = 6.28318530717958647692;

    float viewX = lerp(gFov.x, gFov.y, input.uv.x);
    float viewY = lerp(gFov.z, gFov.w, input.uv.y);
    float3 viewDirection = normalize(float3(viewX, viewY, -1.0));
    float3 worldDirection = normalize(RotateByQuaternion(viewDirection, gOrientation));

    float longitude = atan2(worldDirection.x, -worldDirection.z);
    float u = frac(longitude / tau + 0.5);
    float v = asin(clamp(worldDirection.y, -1.0, 1.0)) / pi + 0.5;

    float wave =
        0.22 * sin(u * tau * 2.0 + 0.65) +
        0.085 * sin(u * tau * 3.0 - 0.9) +
        0.032 * sin(u * tau * 5.0 + v * 1.7);
    float bandCoordinate = v * 5.0 + wave;
    int bandIndex = (int)floor(frac(bandCoordinate / 5.0) * 5.0);
    float3 color = lerp(gColors[0].rgb, gColors[bandIndex + 1].rgb, 0.88);

    float withinBand = frac(bandCoordinate);
    float edgeDistance = min(withinBand, 1.0 - withinBand);
    float contourWidth = max(fwidth(bandCoordinate) * 1.2, 0.008);
    float contour = 1.0 - smoothstep(0.0, contourWidth, edgeDistance);
    color = lerp(color, gContour.rgb, contour * gContour.a);

    // Sparse, soft landmarks move with the world-locked sphere. They make a
    // stationary panel defect easier to separate without creating visual noise.
    float2 gridScale = float2(10.0, 6.0);
    float2 grid = float2(u, v) * gridScale;
    float2 cell = floor(grid);
    cell.x = fmod(cell.x + gridScale.x, gridScale.x);
    float2 center = float2(
        Hash21(cell + float2(2.1, 7.3)),
        Hash21(cell + float2(8.7, 1.9))
    );
    float2 local = frac(grid) - center;
    local.x *= 0.55 + Hash21(cell + 4.2) * 0.5;
    local.y *= 2.6;
    float fleck = 1.0 - smoothstep(0.10, 0.22, length(local));
    color = lerp(color, gFleck.rgb, fleck * gFleck.a);

    return float4(color, 1.0);
}
)hlsl";

}  // namespace pixel_flow
