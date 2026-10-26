// Svf.h: TPT state-variable filter (12 dB/oct LP / HP / BP), one channel. No Rack dependency.
// The coefficients are shared by both channels of a voice, so they are computed once.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <cmath>

enum FilterType { F_LP, F_HP, F_BP, F_N };

struct SvfCoef { float g, R; };

// fc in Hz (clamped to 20 Hz .. 0.45 sr), res 0..1: 0 = Butterworth, 1 = Q 20 (stable, near self-oscillation)
inline SvfCoef svfCoef (float fc, float res, float sr)
{
    fc = std::max (20.f, std::min (fc, 0.45f * sr));
    const float Q = 0.7071f + 19.3f * res * res;
    return { std::tan (3.14159265f * fc / sr), 1.f / (2.f * Q) };
}

struct Svf
{
    float s1 = 0.f, s2 = 0.f;

    float process (float x, const SvfCoef& k, int type)
    {
        const float hp = (x - (2.f * k.R + k.g) * s1 - s2) / (1.f + 2.f * k.R * k.g + k.g * k.g);
        const float bp = k.g * hp + s1;
        s1 = k.g * hp + bp;
        const float lp = k.g * bp + s2;
        s2 = k.g * bp + lp;
        return type == F_HP ? hp : type == F_BP ? bp : lp;
    }
};
