#pragma once

#include <array>
#include <cstdint>

namespace pixel_flow {

struct AccentColor {
    std::uint32_t rgb;
    float opacity;
};

struct Palette {
    const wchar_t* name;
    const wchar_t* note;
    std::uint32_t base;
    std::array<std::uint32_t, 5> bands;
    AccentColor contour;
    AccentColor fleck;
    bool darkInk;
};

inline constexpr std::array<Palette, 14> kPalettes{{
    {L"Balanced grey", L"Mid neutral", 0x777B78,
     {0x818581, 0x707572, 0x898C87, 0x6E7371, 0x7F837F},
     {0xE7EBE5, 0.13F}, {0xEFF2EB, 0.10F}, false},
    {L"Cool grey", L"Blue-neutral", 0x737B82,
     {0x7D858C, 0x69737B, 0x858C91, 0x6C757C, 0x7B8389},
     {0xE2EAEE, 0.14F}, {0xEDF2F4, 0.10F}, false},
    {L"Warm sand", L"Muted beige", 0x91806B,
     {0x9C8A74, 0x857461, 0xA08E78, 0x82715F, 0x978570},
     {0xF6EAD7, 0.14F}, {0xF9EEDD, 0.10F}, false},
    {L"Mist blue", L"Muted cool", 0x657F8B,
     {0x708B96, 0x5B7480, 0x78929C, 0x58717C, 0x6C8690},
     {0xE0EFF3, 0.15F}, {0xE9F5F7, 0.10F}, false},
    {L"Soft sage", L"Muted green", 0x708173,
     {0x7B8C7D, 0x657668, 0x829184, 0x627466, 0x76877A},
     {0xE7F1E5, 0.14F}, {0xEEF5EA, 0.10F}, false},
    {L"Dusty rose", L"Muted warm", 0x8D6F72,
     {0x99797C, 0x816568, 0x9E8081, 0x7E6265, 0x947477},
     {0xF6E6E5, 0.14F}, {0xF9ECE9, 0.10F}, false},
    {L"Mid red", L"Red subpixel", 0x9A4C4D,
     {0xA95756, 0x8B4345, 0xAE5B59, 0x873F42, 0xA05251},
     {0xFFDFD6, 0.14F}, {0xFFE6DC, 0.10F}, false},
    {L"Mid green", L"Green subpixel", 0x4F8256,
     {0x5A8E62, 0x46764D, 0x609467, 0x437349, 0x55895C},
     {0xDEF7DC, 0.14F}, {0xE7FAE3, 0.10F}, false},
    {L"Mid blue", L"Blue subpixel", 0x456B99,
     {0x5278A5, 0x3D608C, 0x587EAA, 0x3A5D88, 0x4B719F},
     {0xDCEAFF, 0.15F}, {0xE5F0FF, 0.10F}, false},
    {L"Deep graphite", L"Dark neutral", 0x292D2E,
     {0x323738, 0x242829, 0x383C3C, 0x222627, 0x2E3334},
     {0xD8E2E0, 0.10F}, {0xE2EAE8, 0.08F}, false},
    {L"Deep ocean", L"Dark cool", 0x263943,
     {0x304650, 0x203139, 0x354B54, 0x1E2F37, 0x2B4049},
     {0xD3EAF0, 0.11F}, {0xDEF0F4, 0.08F}, false},
    {L"Bright ivory", L"Bright warm", 0xD5D1C5,
     {0xDEDACF, 0xC8C4B9, 0xE2DED3, 0xC5C1B6, 0xD0CCC0},
     {0x43413B, 0.11F}, {0x3E3C37, 0.08F}, true},
    {L"Bright cloud", L"Bright cool", 0xCBD1D3,
     {0xD6DCDD, 0xBEC5C8, 0xDBE0E1, 0xBBC2C5, 0xC6CDCF},
     {0x333D41, 0.11F}, {0x303A3E, 0.08F}, true},
    {L"Near black", L"Stuck-pixel check", 0x0D0F10,
     {0x151819, 0x090B0C, 0x191C1D, 0x080A0B, 0x111415},
     {0xCEDADC, 0.08F}, {0xDDE6E8, 0.06F}, false},
}};

}  // namespace pixel_flow
