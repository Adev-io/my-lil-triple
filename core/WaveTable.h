// WaveTable.h: turns a loaded Sample into one custom-wave cycle. No Rack dependency.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Osc.h"
#include "Sample.h"

// Resamples the left channel to TABLE_N points (linear) and normalises to peak 1. False for an empty or silent sample.
inline bool makeTable (const Sample& s, std::vector<float>& out)
{
    const int n = s.size();
    if (n < 2) return false;
    out.assign (TABLE_N, 0.f);
    float peak = 0.f;
    for (int i = 0; i < TABLE_N; ++i)
    {
        const float x = (float) i * n / TABLE_N;
        const int a = std::min ((int) x, n - 1), b = (a + 1) % n;   // the last point wraps to the first: it is a cycle
        out[i] = s.l[a] + (s.l[b] - s.l[a]) * (x - a);
        peak = std::max (peak, std::fabs (out[i]));
    }
    if (peak < 1e-6f) return false;
    for (float& v : out) v /= peak;
    return true;
}
