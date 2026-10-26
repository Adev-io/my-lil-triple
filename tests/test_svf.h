// test_svf.h: the state-variable filter.
#pragma once
#include "check.h"
#include "Svf.h"

// peak of the filter's output for a sine at hz, after it has settled
inline float svfGain (int type, float fc, float res, float hz)
{
    Svf s;
    const float sr = 48000.f;
    const SvfCoef k = svfCoef (fc, res, sr);
    float peak = 0.f;
    for (int i = 0; i < 48000; ++i)
    {
        const float y = s.process ((float) std::sin (6.283185307179586 * hz * i / sr), k, type);
        if (i > 24000) peak = std::max (peak, std::fabs (y));
    }
    return peak;
}

inline void testSvf()
{
    check (svfGain (F_LP, 1000.f, 0.f, 8000.f) < 0.1f, "low-pass at 1 kHz cuts 8 kHz by > 20 dB", svfGain (F_LP, 1000.f, 0.f, 8000.f));
    check (svfGain (F_LP, 1000.f, 0.f, 100.f) > 0.95f, "low-pass at 1 kHz passes 100 Hz", svfGain (F_LP, 1000.f, 0.f, 100.f));
    check (svfGain (F_HP, 8000.f, 0.f, 1000.f) < 0.1f, "high-pass at 8 kHz cuts 1 kHz by > 20 dB", svfGain (F_HP, 8000.f, 0.f, 1000.f));
    check (svfGain (F_HP, 1000.f, 0.f, 10000.f) > 0.95f, "high-pass at 1 kHz passes 10 kHz", svfGain (F_HP, 1000.f, 0.f, 10000.f));
    check (near (svfGain (F_BP, 1000.f, 0.f, 1000.f), 0.7071, 0.05), "band-pass peaks at its centre", svfGain (F_BP, 1000.f, 0.f, 1000.f));
    check (svfGain (F_BP, 1000.f, 0.f, 8000.f) < 0.2f, "band-pass rejects far above its centre", svfGain (F_BP, 1000.f, 0.f, 8000.f));
    check (svfGain (F_LP, 1000.f, 1.f, 1000.f) > 5.f, "full resonance rings at the cutoff", svfGain (F_LP, 1000.f, 1.f, 1000.f));

    Svf s;
    float y = 0.f;
    for (int i = 0; i < 2000; ++i) y = s.process (i & 1 ? 1.f : -1.f, svfCoef (1e9f, 1.f, 48000.f), F_LP);
    check (std::isfinite (y), "extreme cutoff and full resonance stay finite", y);
    for (int i = 0; i < 2000; ++i) y = s.process (i & 1 ? 1.f : -1.f, svfCoef (-5.f, 0.f, 48000.f), F_HP);
    check (std::isfinite (y), "negative cutoff stays finite", y);
}
