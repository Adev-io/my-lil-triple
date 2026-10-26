// test_osc.h: wave shapes, polyBLEP, noise, custom table.
#pragma once
#include "check.h"
#include "Osc.h"
#include "WaveTable.h"

// frequency of a sine from its first to last rising zero crossing (sub-sample interpolated)
inline double measureHz (const std::vector<float>& x, double sr)
{
    double first = 0, last = 0;
    int n = 0;
    for (size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] < 0.f && x[i] >= 0.f)
        {
            const double t = (double) (i - 1) + (0.0 - x[i - 1]) / (x[i] - x[i - 1]);
            if (n++ == 0) first = t;
            last = t;
        }
    return n > 1 ? (n - 1) * sr / (last - first) : 0.0;
}

// magnitude of one frequency in x
inline double goertzel (const std::vector<float>& x, double hz, double sr)
{
    const double c = 2.0 * std::cos (6.283185307179586 * hz / sr);
    double s0 = 0, s1 = 0, s2 = 0;
    for (float v : x) { s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / x.size();
}

inline std::vector<float> renderOsc (int wave, double hz, double sr, int n, bool hq, const std::vector<float>* tbl = nullptr)
{
    std::vector<float> out (n);
    uint32_t rng = 1;
    for (int i = 0; i < n; ++i)
    {
        double p = i * hz / sr;
        p -= std::floor (p);   // exact phase, so the spectrum has no smear
        out[i] = oscSample (wave, (float) p, (float) (hz / sr), hq, rng, tbl);
    }
    return out;
}

inline void testOsc()
{
    uint32_t rng = 1;
    std::vector<float> sine = renderOsc (W_SINE, 1000.0, 48000.0, 48000, true);
    check (near (measureHz (sine, 48000.0), 1000.0, 0.01), "sine runs at the asked frequency", measureHz (sine, 48000.0));
    check (near (oscSample (W_TRI, 0.f, 0.01f, true, rng, nullptr), 0.0, 1e-6), "triangle starts at 0");
    check (near (oscSample (W_TRI, 0.25f, 0.01f, true, rng, nullptr), 1.0, 1e-6), "triangle peaks at a quarter cycle");

    bool inRange = true;
    for (int w : { W_SINE, W_TRI, W_SQUARE, W_SAW, W_RSAW })
        for (float v : renderOsc (w, 1234.0, 48000.0, 4800, true))
            if (std::fabs (v) > 1.05f) inRange = false;
    check (inRange, "every wave stays inside +-1.05 with HQ on");

    // HQ: a 7 kHz saw's 4th harmonic (28 kHz) folds to 20 kHz; polyBLEP must cut that alias well below the raw saw's
    const double raw = goertzel (renderOsc (W_SAW, 7000.0, 48000.0, 48000, false), 20000.0, 48000.0);
    const double blep = goertzel (renderOsc (W_SAW, 7000.0, 48000.0, 48000, true), 20000.0, 48000.0);
    check (blep < 0.7 * raw, "HQ cuts a saw's folded harmonic at 20 kHz", blep / raw);
    const double sraw = goertzel (renderOsc (W_SQUARE, 7000.0, 48000.0, 48000, false), 20000.0, 48000.0);
    const double sblep = goertzel (renderOsc (W_SQUARE, 7000.0, 48000.0, 48000, true), 20000.0, 48000.0);
    check (sblep < 0.7 * sraw, "HQ cuts a square's folded harmonic at 20 kHz", sblep / sraw);

    std::vector<float> rs = renderOsc (W_RSAW, 100.0, 48000.0, 480, true);
    float step = 0.f;
    for (size_t i = 1; i < rs.size(); ++i) step = std::max (step, std::fabs (rs[i] - rs[i - 1]));
    check (step < 0.1f, "rounded saw has no jump at its reset (a raw saw steps by 2)", step);

    double mean = 0, lo = 1, hi = -1;
    for (int i = 0; i < 100000; ++i) { const float v = noise (rng); mean += v; lo = std::min<double> (lo, v); hi = std::max<double> (hi, v); }
    check (std::fabs (mean / 100000) < 0.02 && lo >= -1.0 && hi <= 1.0 && hi - lo > 1.9, "noise is centred, full range, inside +-1", mean / 100000);

    check (near (oscSample (W_CUSTOM, 0.3f, 0.01f, true, rng, nullptr), std::sin (PI2 * 0.3f), 1e-6), "custom with no table falls back to sine");
    std::vector<float> ramp (TABLE_N);
    for (int i = 0; i < TABLE_N; ++i) ramp[i] = (float) i / TABLE_N;
    check (near (oscSample (W_CUSTOM, 0.25f, 0.01f, true, rng, &ramp), 0.25, 1e-4), "custom reads its table", oscSample (W_CUSTOM, 0.25f, 0.01f, true, rng, &ramp));
    std::vector<float> cyc (TABLE_N);
    for (int i = 0; i < TABLE_N; ++i) cyc[i] = std::sin (PI2 * i / TABLE_N);
    check (near (oscSample (W_CUSTOM, 0.99999f, 0.01f, true, rng, &cyc), 0.0, 1e-3), "custom interpolates across the wrap", oscSample (W_CUSTOM, 0.99999f, 0.01f, true, rng, &cyc));
    check (near (oscSample (W_CUSTOM, 0.3f, 0.01f, true, rng, &cyc), std::sin (PI2 * 0.3f), 1e-3), "custom table of a sine reads as a sine");

    Sample s;
    s.l.assign (100, 0.f);
    for (int i = 0; i < 100; ++i) s.l[i] = 0.1f * i;
    std::vector<float> t;
    check (makeTable (s, t) && (int) t.size() == TABLE_N, "a sample becomes a TABLE_N cycle");
    float peak = 0.f;
    for (float v : t) peak = std::max (peak, std::fabs (v));
    check (near (peak, 1.0, 1e-5), "the table is normalised to peak 1", peak);
    Sample silent;
    silent.l.assign (100, 0.f);
    check (! makeTable (silent, t) && ! makeTable (Sample(), t), "silent and empty samples are refused");
}
