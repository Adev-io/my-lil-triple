// Osc.h: one oscillator sample from a phase, for the seven 3x Osc style waves. No Rack dependency.
// HQ = polyBLEP on saw and square (3x Osc's anti-alias switch); off gives the raw, aliased shape.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

enum Wave { W_SINE, W_TRI, W_SQUARE, W_SAW, W_RSAW, W_NOISE, W_CUSTOM, W_N };

constexpr float PI2 = 6.2831853f;
constexpr int TABLE_N = 2048;   // samples in one custom-wave cycle

inline float frac (float x) { return x - std::floor (x); }

// two-sample polynomial band-limited step; t = phase 0..1, dt = phase step per sample
inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

inline float noise (uint32_t& s)
{
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;   // xorshift32
    return (int32_t) s * (1.f / 2147483648.f);
}

// ph 0..1 is the wave's phase, dt the phase step per sample (for polyBLEP), tbl the custom cycle (TABLE_N floats) or null.
// Rounded saw: a ramp with its reset smoothed into a 10% cosine fall (the manual gives no formula; this is our reading).
inline float oscSample (int w, float ph, float dt, bool hq, uint32_t& rng, const std::vector<float>* tbl)
{
    switch (w)
    {
        case W_TRI: { const float p = frac (ph + 0.25f); return 1.f - 2.f * std::fabs (2.f * p - 1.f); }
        case W_SQUARE:
        {
            float s = ph < 0.5f ? 1.f : -1.f;
            if (hq) s += polyBlep (ph, dt) - polyBlep (frac (ph + 0.5f), dt);
            return s;
        }
        case W_SAW:
        {
            float s = 2.f * ph - 1.f;
            if (hq) s -= polyBlep (ph, dt);
            return s;
        }
        case W_RSAW:
        {
            if (ph < 0.9f) return -1.f + 2.f * ph / 0.9f;
            return std::cos (3.14159265f * (ph - 0.9f) / 0.1f);   // 1 -> -1
        }
        case W_NOISE: return noise (rng);
        case W_CUSTOM:
        {
            if (! tbl || (int) tbl->size() != TABLE_N) break;   // no wave loaded: sine
            const float x = ph * TABLE_N;
            const int i = std::min ((int) x, TABLE_N - 1);
            const float f = x - i;
            const float a = (*tbl)[i], b = (*tbl)[(i + 1) % TABLE_N];
            return a + (b - a) * f;
        }
        default: break;
    }
    return std::sin (PI2 * ph);
}
