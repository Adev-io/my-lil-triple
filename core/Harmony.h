// Harmony.h: chord-following lead quantizer and harmonizer for My lil Harmony. No Rack dependency.
// Pitches are semitones with 60 = C4 = 0 V. A Mask holds pitch classes (bit 0 = C).
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Chords.h"
#include <cmath>

typedef unsigned Mask;

inline int pclass (int p) { return ((p % 12) + 12) % 12; }
inline Mask bit (int p) { return 1u << pclass (p); }
inline bool has (Mask m, int p) { return (m & bit (p)) != 0; }

inline Mask modeMask (int key, int mode)
{
    Mask m = 0;
    for (int i = 0; i < 7; ++i) m |= bit (key + MODES[mode][i]);
    return m;
}

// The scale under a chord: each chord note outside the base scale replaces the scale note a semitone above it
// (a flattened degree, e.g. Bb for B), or else the one below it.
inline Mask chordScale (Mask scale, Mask chord)
{
    Mask s = scale;
    for (int pc = 0; pc < 12; ++pc)
    {
        if (! has (chord, pc) || has (scale, pc)) continue;
        if (has (s, pc + 1) && ! has (chord, pc + 1)) s &= ~bit (pc + 1);
        else if (has (s, pc - 1) && ! has (chord, pc - 1)) s &= ~bit (pc - 1);
        s |= bit (pc);
    }
    return s;
}

// Nearest pitch in the mask; ties go down. An empty mask passes the pitch through.
inline int snap (int p, Mask m)
{
    if (! m) return p;
    for (int d = 0; d < 12; ++d)
    {
        if (has (m, p - d)) return p - d;
        if (has (m, p + d)) return p + d;
    }
    return p;
}
inline int below (int limit, Mask m) { for (int p = limit; p > limit - 12; --p) if (has (m, p)) return p; return limit; }
inline int above (int limit, Mask m) { for (int p = limit; p < limit + 12; ++p) if (has (m, p)) return p; return limit; }

enum HarmonyMode { H_SOLO, H_BELOW, H_ABOVE, H_BOTH, H_SIXTH, H_BLOCK, H_OCTAVE, H_POWER, H_N };
static const char* const HARMONY_NAMES[H_N] = {
    "Solo", "3rd below", "3rd above", "Below + above", "6th below", "4-part block", "Octave below", "Power (4th/5th below)" };

// Fills out[] with the lead first, then the harmony voices; returns the voice count (1..4).
// A chord-tone lead harmonises with chord tones, a passing note with the scale (parallel diatonic motion).
// Every harmony sits at least a minor 3rd from its neighbour, so voices never rub.
inline int voices (int lead, int mode, Mask chord, Mask scale, int out[4])
{
    const Mask t = (chord && has (chord, lead)) ? chord : scale;
    int n = 0;
    out[n++] = lead;
    switch (mode)
    {
        case H_BELOW:  out[n++] = below (lead - 3, t); break;
        case H_ABOVE:  out[n++] = above (lead + 3, t); break;
        case H_BOTH:   out[n++] = below (lead - 3, t); out[n++] = above (lead + 3, t); break;
        case H_SIXTH:  out[n++] = below (below (lead - 3, t) - 3, t); break;
        case H_BLOCK:  { const int a = below (lead - 3, t), b = below (a - 3, t); out[n++] = a; out[n++] = b; out[n++] = below (b - 3, t); } break;
        case H_OCTAVE: out[n++] = lead - 12; break;
        case H_POWER:  out[n++] = below (lead - 5, t); break;
        default: break;
    }
    return n;
}

// Watches a poly V/OCT + gate (a My lil Roll) and latches the newest group of notes that start together.
// Single notes (a bass bounce) never replace the chord, and notes that keep ringing never shrink it.
struct ChordFollower
{
    static constexpr float WINDOW = 0.03f;   // notes starting within 30 ms belong to one chord
    Mask chord = 0;
    float onset[16] = {};
    int pitch[16] = {};
    bool held[16] = {};
    float t = 0.f;

    void step (const float* voct, const float* gate, int n, float dt)
    {
        t += dt;
        float newest = -1e9f;
        for (int c = 0; c < 16; ++c)
        {
            const bool g = c < n && gate[c] >= 1.f;
            const int p = c < n ? (int) std::lround (voct[c] * 12.f) : 0;
            if (g && (! held[c] || p != pitch[c])) onset[c] = t;
            held[c] = g;
            pitch[c] = p;
            if (g && onset[c] > newest) newest = onset[c];
        }
        if (t - newest > WINDOW) return;   // nothing started lately: keep the chord
        Mask m = 0;
        for (int c = 0; c < 16; ++c) if (held[c] && onset[c] >= newest - WINDOW) m |= bit (pitch[c]);
        if (__builtin_popcount (m) >= 2) chord = m;
    }
};
