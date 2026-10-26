// test_voice.h: the voice end to end.
#pragma once
#include "check.h"
#include "TripleVoice.h"
#include "test_osc.h"

// one clean sine on osc 1 (the other two silent), filter open
inline TripleCfg sineCfg()
{
    TripleCfg c;
    for (int i = 0; i < 3; ++i) c.osc[i].wave = W_SINE;
    c.osc[1].mix = 0.f;
    c.osc[2].mix = 0.f;
    return c;
}

inline void renderVoice (const TripleCfg& c, float voct, int n, std::vector<float>& l, std::vector<float>& r)
{
    TripleVoice v;
    l.assign (n, 0.f);
    r.assign (n, 0.f);
    for (int i = 0; i < n; ++i) v.process (c, voct, true, 0.f, 48000.f, l[i], r[i]);
}

inline double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }
inline double tailHz (const std::vector<float>& x) { return measureHz (std::vector<float> (x.begin() + 4800, x.end()), 48000.0); }
inline double peakOf (const std::vector<float>& x, int from = 4800)
{
    double p = 0;
    for (size_t i = from; i < x.size(); ++i) p = std::max<double> (p, std::fabs (x[i]));
    return p;
}
inline double rmsOf (const std::vector<float>& x, int from = 4800)
{
    double s = 0;
    for (size_t i = from; i < x.size(); ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (x.size() - from));
}
inline double rmsDiff (const std::vector<float>& x, int from = 4800)
{
    double s = 0;
    for (size_t i = from + 1; i < x.size(); ++i) s += (double) (x[i] - x[i - 1]) * (x[i] - x[i - 1]);
    return std::sqrt (s / (x.size() - from));
}

inline void testVoice()
{
    std::vector<float> l, r;
    TripleCfg c = sineCfg();
    renderVoice (c, 0.f, 96000, l, r);
    check (std::fabs (cents (tailHz (l), 261.6256)) < 0.1, "0 V is C4, 261.63 Hz, within 0.1 cent", tailHz (l));
    renderVoice (c, 1.f, 96000, l, r);
    check (std::fabs (cents (tailHz (l), 523.2511)) < 0.1, "1 V is one octave up", tailHz (l));

    c.osc[0].coarse = 12.f;
    renderVoice (c, 0.f, 96000, l, r);
    check (std::fabs (cents (tailHz (l), 523.2511)) < 0.1, "coarse +12 is one octave up", tailHz (l));
    c.osc[0].coarse = 0.f;
    c.osc[0].fine = 1.f;
    renderVoice (c, 0.f, 96000, l, r);
    check (std::fabs (cents (tailHz (l), 261.6256 * std::pow (2.0, 1.0 / 12))) < 0.1, "fine +1 is one semitone up", tailHz (l));

    // inverted copy cancels
    c = sineCfg();
    c.osc[1].mix = 1.f;
    c.osc[1].invert = true;
    renderVoice (c, 0.f, 24000, l, r);
    check (peakOf (l) < 1e-4 && peakOf (r) < 1e-4, "an inverted copy of the same osc is silence", peakOf (l));

    // three identical oscs keep the level of one
    c = sineCfg();
    renderVoice (c, 0.f, 24000, l, r);
    const double one = peakOf (l);
    c.osc[1].mix = 1.f; c.osc[2].mix = 1.f;
    renderVoice (c, 0.f, 24000, l, r);
    check (near (peakOf (l), one, one * 0.02), "relative mixes keep one overall level", peakOf (l) / one);

    // hard-left pan
    c = sineCfg();
    c.osc[0].pan = -1.f;
    renderVoice (c, 0.f, 24000, l, r);
    check (peakOf (r) < 1e-6 && peakOf (l) > 0.5, "pan hard left leaves R silent", peakOf (r));

    // phase offset: R starts a quarter cycle ahead, so at the first sample R is far from L
    c = sineCfg();
    c.osc[0].phase = 0.25f;
    c.ampA = 0.0001f;
    renderVoice (c, 0.f, 24000, l, r);
    double diff = 0;
    for (int i = 4800; i < 4900; ++i) diff = std::max<double> (diff, std::fabs (l[i] - r[i]));
    check (diff > 0.3, "phase offset puts R out of step with L", diff);

    // AM: osc 3 as a 65 Hz square gates osc 1 on and off, so the level drops to about 1/sqrt(2)
    c = sineCfg();
    c.osc[2].wave = W_SQUARE;
    c.osc[2].coarse = -24.f;
    c.osc[2].mix = 0.f;
    renderVoice (c, 0.f, 96000, l, r);
    const double plain = rmsOf (l);
    c.am = true;
    renderVoice (c, 0.f, 96000, l, r);
    check (near (rmsOf (l) / plain, 0.7071, 0.1), "AM OSC3 gates osc 1 and 2", rmsOf (l) / plain);

    // filter envelope opens a 200 Hz low-pass on a saw; keytrack lifts it with pitch
    c = sineCfg();
    c.osc[0].wave = W_SAW;
    c.cutoff = 200.f;
    renderVoice (c, 0.f, 48000, l, r);
    const double closed = rmsDiff (l);
    c.envAmt = 1.f;
    renderVoice (c, 0.f, 48000, l, r);
    check (rmsDiff (l) > 3.0 * closed, "filter envelope amount brightens the saw", rmsDiff (l) / closed);
    c.envAmt = 0.f;
    renderVoice (c, 2.f, 48000, l, r);
    const double lowTrack = rmsDiff (l);
    c.keytrack = 1.f;
    renderVoice (c, 2.f, 48000, l, r);
    check (rmsDiff (l) > 2.0 * lowTrack, "keytrack opens the filter for high notes", rmsDiff (l) / lowTrack);

    // release: gate off lets the voice fade and go idle
    c = sineCfg();
    c.ampR = 0.05f;
    TripleVoice v;
    float a = 0.f, b = 0.f;
    for (int i = 0; i < 4800; ++i) v.process (c, 0.f, true, 0.f, 48000.f, a, b);
    check (v.active(), "voice is active while the gate is high");
    for (int i = 0; i < 3300; ++i) v.process (c, 0.f, false, 0.f, 48000.f, a, b);
    check (! v.active() && a == 0.f, "voice goes idle after the release");

    // 16 voices, every wave, both HQ settings, harsh filter: nothing blows up
    bool finite = true;
    double worst = 0;
    for (int n = 0; n < 16; ++n)
    {
        TripleCfg h;
        for (int i = 0; i < 3; ++i) { h.osc[i].wave = (n + i) % W_N; h.osc[i].mix = 1.f; h.osc[i].pan = (n % 3 - 1) * 0.9f; h.osc[i].phase = 0.1f * i; }
        h.hq = n & 1;
        h.am = (n % 5) == 0;
        h.ftype = n % F_N;
        h.res = 1.f;
        h.cutoff = 20.f * std::pow (1000.f, n / 15.f);
        h.envAmt = (n % 2) ? 1.f : -1.f;
        h.keytrack = 1.f;
        TripleVoice w;
        for (int i = 0; i < 4800; ++i)
        {
            w.process (h, (n - 8) * 0.4f, i < 3000, 0.f, 48000.f, a, b);
            if (! std::isfinite (a) || ! std::isfinite (b)) finite = false;
            worst = std::max<double> (worst, std::max (std::fabs (a), std::fabs (b)));
        }
    }
    check (finite && worst < 200.0, "16 harsh voices stay finite and bounded", worst);
}
