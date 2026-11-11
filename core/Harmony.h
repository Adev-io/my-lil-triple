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
inline int voices (int lead, int mode, Mask chord, Mask scale, int out[6])
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

// Root of a latched triad (any inversion); -1 if the mask holds no triad or more than 4 notes.
struct TriadShape { int a, b; const char* suffix; };
static const TriadShape TRIADS[] = { { 4, 7, "" }, { 3, 7, "m" }, { 3, 6, "dim" }, { 4, 8, "aug" }, { 5, 7, "sus4" }, { 2, 7, "sus2" } };
inline int chordRoot (Mask m, const char** suffix = nullptr)
{
    if (! m || __builtin_popcount (m) > 4) return -1;
    for (const auto& t : TRIADS)
        for (int r = 0; r < 12; ++r)
            if (has (m, r) && has (m, r + t.a) && has (m, r + t.b)) { if (suffix) *suffix = t.suffix; return r; }
    return -1;
}

// Pitch class of the k-th scale note above r (k = 1 is the 2nd, 6 is the 7th).
inline int degree (int r, int k, Mask s) { int p = r; for (int i = 0; i < k; ++i) p = above (p + 1, s); return pclass (p); }

// Lead on top, then voices downward from the pool, each at least `gap` below the one above.
// Pitch classes not yet sounding come first, so the color note always sounds.
inline int stack (int lead, Mask pool, int n, int gap, int out[6])
{
    Mask used = bit (lead);
    out[0] = lead;
    for (int k = 1; k < n; ++k)
    {
        const Mask fresh = pool & ~used;
        out[k] = below (out[k - 1] - gap, fresh ? fresh : pool);
        used |= bit (out[k]);
    }
    return n;
}

enum ChordColor { C_OFF, C_TRIAD, C_SEVENTH, C_NINTH, C_ELEVENTH, C_SIXTH, C_SUS2, C_SUS4, C_ADD9, C_ADDB9, C_FIFTHS, C_CLUSTER, C_SPREAD, C_OCTAVES, C_N };
static const char* const COLOR_NAMES[C_N] = {
    "off", "triad", "7th", "9th", "11th", "6th", "sus2", "sus4", "add9", "add b9", "5ths stack", "cluster", "wide spread", "octave doubles" };
static const char* const SET_NAMES[3] = { "Dark", "Lush", "Mix" };
static const int COLOR_SETS[3][8] = {
    { C_TRIAD, C_SEVENTH, C_SUS2, C_SUS4, C_ADD9, C_ADDB9, C_FIFTHS, C_CLUSTER },       // Dark
    { C_TRIAD, C_SEVENTH, C_NINTH, C_ELEVENTH, C_SIXTH, C_ADD9, C_SPREAD, C_OCTAVES },  // Lush
    { C_TRIAD, C_SEVENTH, C_NINTH, C_SUS2, C_SUS4, C_ADDB9, C_FIFTHS, C_SPREAD } };     // Mix

// A chord stacked under the lead, built from the latched chord (or, with none yet, the triad on the key root) and the scale.
inline int colorVoices (int lead, int color, Mask chord, Mask scale, int key, int out[6])
{
    if (! chord) chord = bit (key) | bit (degree (key, 2, scale)) | bit (degree (key, 4, scale));
    int r = chordRoot (chord);
    if (r < 0) r = pclass (key);   // ponytail: odd latched shapes (2 notes, clusters) borrow the key root
    const Mask third = bit (r + 3) | bit (r + 4), d2 = bit (degree (r, 1, scale)), d7 = bit (degree (r, 6, scale));
    switch (color)
    {
        case C_TRIAD:    return stack (lead, chord, 3, 3, out);
        case C_SEVENTH:  return stack (lead, chord | d7, 4, 3, out);
        case C_NINTH:    return stack (lead, chord | d7 | d2, 5, 3, out);
        case C_ELEVENTH: return stack (lead, chord | d7 | d2 | bit (degree (r, 3, scale)), 6, 3, out);
        case C_SIXTH:    return stack (lead, chord | bit (degree (r, 5, scale)), 4, 3, out);
        case C_SUS2:     return stack (lead, (chord & ~third) | d2, 3, 1, out);
        case C_SUS4:     if (! has (scale, r + 5)) return colorVoices (lead, C_SUS2, chord, scale, key, out);
                         return stack (lead, (chord & ~third) | bit (r + 5), 3, 3, out);
        case C_ADD9:     return stack (lead, chord | d2, 4, 3, out);
        case C_ADDB9:    return stack (lead, chord | (has (scale, r + 1) ? bit (r + 1) : d2), 4, 1, out);
        case C_FIFTHS:   out[0] = lead; out[1] = snap (lead - 7, scale); out[2] = snap (lead - 14, scale); return 3;
        case C_CLUSTER:  return stack (lead, scale, 3, 1, out);
        case C_SPREAD:   { int t[6]; stack (lead, chord, 3, 3, t); out[0] = lead; out[1] = t[2]; out[2] = t[1] - 12; return 3; }   // drop-2
        case C_OCTAVES:  out[0] = lead; out[1] = below (lead - 3, chord); out[2] = lead - 12; out[3] = lead - 24; return 4;
        default:         out[0] = lead; return 1;
    }
}

// MiniLab pad from a MIDI-CV V/OCT: notes 36-43 = bank A (0-7), 44-51 = bank B (8-15); anything else -1.
inline int padIndex (float v) { const int n = (int) std::lround (v * 12.f) + 60; return n >= 36 && n <= 51 ? n - 36 : -1; }

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
