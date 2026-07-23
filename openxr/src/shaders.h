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

float3 PaletteGradient(float coordinate)
{
    float scaled = saturate(coordinate) * 4.0;
    int lowerIndex = min((int)floor(scaled), 3);
    float amount = scaled - (float)lowerIndex;
    amount = amount * amount * (3.0 - 2.0 * amount);
    return lerp(gColors[lowerIndex + 1].rgb,
                gColors[lowerIndex + 2].rgb, amount);
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

    // Analytic waves evaluated directly on the unit sphere. Because the field
    // depends on the 3D direction rather than latitude/longitude, it remains
    // continuous at the rear meridian and at both poles.
    float broadPhase =
        dot(worldDirection, float3(1.35, 8.20, 0.90)) +
        0.72 * sin(dot(worldDirection, float3(-3.40, 1.20, 2.80)));
    float crossingPhase =
        dot(worldDirection, float3(-4.80, 2.10, 5.60)) +
        0.44 * sin(dot(worldDirection, float3(2.30, 3.80, -1.70)));
    float diagonalPhase = dot(worldDirection, float3(8.10, -3.00, -6.40)) + 0.85;

    float broadWave = sin(broadPhase);
    float crossingWave = sin(crossingPhase);
    float diagonalWave = sin(diagonalPhase);
    float continuousField =
        0.58 * broadWave + 0.29 * crossingWave + 0.13 * diagonalWave;

    // Smoothly travel through all five nearby palette tones. There are no
    // constant-fill bands: even the broad color structure is interpolated.
    float paletteCoordinate = 0.5 + 0.42 * continuousField;
    float3 paletteColor = PaletteGradient(paletteCoordinate);
    float3 color = lerp(gColors[0].rgb, paletteColor, 0.86);

    // Two shorter, crossing waves add gentle continuous luminance variation
    // inside the broad shapes. Their periods remain large enough to avoid a
    // grating or moire-like appearance while eliminating visually flat areas.
    float detailPhase =
        dot(worldDirection, float3(17.0, 5.5, -11.0)) +
        0.36 * sin(dot(worldDirection, float3(-7.5, 12.0, 4.0)));
    float finePhase = dot(worldDirection, float3(-23.0, 9.5, 14.5)) + 1.1;
    float detailWave = sin(detailPhase);
    float fineWave = sin(finePhase);
    float luminance = 1.0 + 0.040 * detailWave + 0.018 * fineWave;
    color *= luminance;

    // A very small phase-separated channel component reduces 8-bit plateaus
    // without becoming visibly colorful; perceived modulation remains brightness-led.
    float3 subpixelRipple = float3(
        sin(finePhase + 0.0),
        sin(finePhase + 2.0943951),
        sin(finePhase + 4.1887902)
    );
    color *= 1.0 + 0.006 * subpixelRipple;

    // Retain the palette's subdued highlight/shadow bias at wave extrema.
    float crest = smoothstep(0.72, 1.0, broadWave * 0.5 + 0.5);
    float trough = 1.0 - smoothstep(0.0, 0.28, broadWave * 0.5 + 0.5);
    color = lerp(color, gContour.rgb, crest * gContour.a * 0.22);
    color = lerp(color, gFleck.rgb, trough * gFleck.a * 0.18);

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
