// TripleVoice.h: one voice of My lil Triple: three oscillators -> stereo mix -> filter -> amp. No Rack dependency.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Adsr.h"
#include "Osc.h"
#include "Svf.h"

struct OscCfg
{
    int wave = W_SAW;
    bool invert = false;
    float phase = 0.f;                       // R channel starts this far (0..1 of a cycle) ahead of L
    float coarse = 0.f, fine = 0.f;          // semitones: +-24, +-1
    float pan = 0.f;                         // -1 left .. 1 right, constant power
    float mix = 1.f;                         // 0..1, relative to osc 1 (osc 1 is always 1)
};

struct TripleCfg
{
    OscCfg osc[3];
    bool am = false;                         // osc 3 amplitude-modulates osc 1 + 2 instead of being heard
    bool hq = true;                          // polyBLEP anti-aliasing
    float ampA = 0.001f, ampD = 0.2f, ampS = 1.f, ampR = 0.1f;
    int ftype = F_LP;
    float cutoff = 20000.f, res = 0.f;       // Hz, 0..1
    float envAmt = 0.f;                      // -1..1 = -/+ 6 octaves of filter envelope
    float keytrack = 0.f;                    // 0..1: cutoff follows pitch
    float fltA = 0.001f, fltD = 0.2f, fltS = 1.f, fltR = 0.1f;
    const std::vector<float>* table = nullptr;   // custom wave, TABLE_N floats, or null
};

struct TripleVoice
{
    float ph[3] = {};
    uint32_t rng = 0x9E3779B9u;
    Adsr amp, flt;
    Svf fl, fr;
    bool gateWas = false;

    bool active() const { return amp.active(); }

    // voct = volts, 0 = C4. cutCv = octaves added to the cutoff. l, r are about +-1 before the module's 5 V scale.
    void process (const TripleCfg& c, float voct, bool gate, float cutCv, float sr, float& l, float& r)
    {
        amp.a = c.ampA; amp.d = c.ampD; amp.s = c.ampS; amp.r = c.ampR;   // before gate(): the release rate reads r
        flt.a = c.fltA; flt.d = c.fltD; flt.s = c.fltS; flt.r = c.fltR;
        if (gate && ! gateWas)
        {
            if (! amp.active()) { fl = Svf(); fr = Svf(); }
            ph[0] = ph[1] = ph[2] = 0.f;
            amp.gate (true);
            flt.gate (true);
        }
        else if (! gate && gateWas)
        {
            amp.gate (false);
            flt.gate (false);
        }
        gateWas = gate;
        l = r = 0.f;
        if (! amp.active()) return;

        const float dt = 1.f / sr, a = amp.next (dt), fe = flt.next (dt);
        float oL[3], oR[3], gl[3], gr[3];
        for (int i = 0; i < 3; ++i)
        {
            const OscCfg& o = c.osc[i];
            const float inc = 261.6256f * std::exp2 (voct + (o.coarse + o.fine) / 12.f) * dt;
            const float bdt = std::min (inc, 0.45f);
            oL[i] = oscSample (o.wave, ph[i], bdt, c.hq, rng, c.table);
            oR[i] = oscSample (o.wave, frac (ph[i] + o.phase), bdt, c.hq, rng, c.table);
            if (o.invert) { oL[i] = -oL[i]; oR[i] = -oR[i]; }
            const float t = (o.pan + 1.f) * 0.78539816f;   // constant power: centre is -3 dB, hard left has R exactly 0
            gl[i] = std::cos (t);
            gr[i] = std::sin (t);
            ph[i] = frac (ph[i] + inc);
        }

        const float w0 = 1.f, w1 = c.osc[1].mix, w2 = c.osc[2].mix;
        float sl, sr2;
        if (c.am)
        {
            const float n = 1.f / (w0 + w1);   // osc 3's pan and mix are not used: it only modulates
            sl = (oL[0] * gl[0] * w0 + oL[1] * gl[1] * w1) * n * 0.5f * (1.f + oL[2]);
            sr2 = (oR[0] * gr[0] * w0 + oR[1] * gr[1] * w1) * n * 0.5f * (1.f + oR[2]);
        }
        else
        {
            const float n = 1.f / (w0 + w1 + w2);   // relative mixes: the sum keeps one level
            sl = (oL[0] * gl[0] * w0 + oL[1] * gl[1] * w1 + oL[2] * gl[2] * w2) * n;
            sr2 = (oR[0] * gr[0] * w0 + oR[1] * gr[1] * w1 + oR[2] * gr[2] * w2) * n;
        }

        const float fc = c.cutoff * std::exp2 (c.envAmt * 6.f * fe + c.keytrack * voct + cutCv);
        const SvfCoef k = svfCoef (fc, c.res, sr);
        l = fl.process (sl, k, c.ftype) * a;
        r = fr.process (sr2, k, c.ftype) * a;
    }
};
