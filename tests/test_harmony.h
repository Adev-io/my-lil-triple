// test_harmony.h: chord-following scale, lead snap, harmony voices and the chord follower.
#pragma once
#include "check.h"
#include "Harmony.h"

inline Mask pcs (std::initializer_list<int> l) { Mask m = 0; for (int p : l) m |= bit (p); return m; }

inline void testHarmony()
{
    const Mask EPHR = modeMask (4, 2);                         // E phrygian = E F G A B C D
    const Mask Em = pcs ({ 4, 7, 11 }), F = pcs ({ 5, 9, 0 }), Gm = pcs ({ 7, 10, 2 }), Bb = pcs ({ 10, 2, 5 });
    const Mask FMAJ = pcs ({ 4, 5, 7, 9, 10, 0, 2 });          // E F G A Bb C D
    check (EPHR == pcs ({ 4, 5, 7, 9, 11, 0, 2 }), "E phrygian mask");
    check (chordScale (EPHR, Em) == EPHR && chordScale (EPHR, F) == EPHR, "Em and F keep E phrygian");
    check (chordScale (EPHR, Gm) == FMAJ && chordScale (EPHR, Bb) == FMAJ, "Gm and Bb turn B into Bb");
    check (chordScale (EPHR, 0) == EPHR, "no chord keeps the base scale");

    check (snap (61, modeMask (0, 0)) == 60, "snap ties go down (C# -> C)");
    check (snap (64, EPHR) == 64 && snap (66, EPHR) == 65, "snap keeps scale notes, F# -> F");
    check (snap (71, FMAJ) == 70, "B over Gm snaps to Bb");

    int v[6];
    check (voices (64, H_BELOW, Em, EPHR, v) == 2 && v[1] == 59, "E4 over Em: 3rd below = B3");
    check (voices (74, H_BELOW, Gm, FMAJ, v) == 2 && v[1] == 70, "D5 over Gm: 3rd below = Bb4");
    check (voices (65, H_BELOW, Em, EPHR, v) == 2 && v[1] == 62, "passing F4 over Em harmonises in the scale: D4");
    check (voices (64, H_ABOVE, Em, EPHR, v) == 2 && v[1] == 67, "E4 over Em: 3rd above = G4");
    check (voices (64, H_BLOCK, Em, EPHR, v) == 4 && v[1] == 59 && v[2] == 55 && v[3] == 52, "E4 block over Em: B3 G3 E3");
    check (voices (64, H_SOLO, Em, EPHR, v) == 1 && v[0] == 64, "solo is the lead alone");
    check (voices (64, H_OCTAVE, Em, EPHR, v) == 2 && v[1] == 52, "octave below");

    // no voice ever leaves the chord scale, and no two voices sit 1 or 2 semitones apart
    const Mask chords[4] = { Em, F, Gm, Bb };
    bool inKey = true, noRub = true;
    for (Mask c : chords)
    {
        const Mask s = chordScale (EPHR, c);
        for (int snapChord = 0; snapChord < 2; ++snapChord)
            for (int key = 40; key <= 90; ++key)
                for (int h = 0; h < H_N; ++h)
                {
                    const int lead = snap (key, snapChord ? c : s);
                    const int n = voices (lead, h, c, s, v);
                    for (int i = 0; i < n; ++i)
                    {
                        inKey = inKey && (s & bit (v[i]));
                        for (int j = i + 1; j < n; ++j) { const int d = std::abs (v[i] - v[j]); noRub = noRub && d != 1 && d != 2; }
                    }
                }
    }
    check (inKey, "every voice of every mode stays in the chord scale");
    check (noRub, "no two voices a semitone or a tone apart");

    // chord follower: single notes do not latch, a stab does, a new stab overlapping the old release wins
    ChordFollower f;
    float vo[16] = {}, g[16] = {};
    const float dt = 1.f / 48000.f;
    auto run = [&] (int n, float secs) { for (int i = 0; i < (int) (secs / dt); ++i) f.step (vo, g, n, dt); };
    vo[0] = 4 / 12.f - 1.f; g[0] = 10.f;                           // bounce note E3 alone
    run (1, 0.05f);
    check (f.chord == 0, "a single note does not latch");
    vo[0] = 7 / 12.f; vo[1] = 11 / 12.f; vo[2] = 4 / 12.f + 1.f;    // Em stab G4 B4 E5
    g[0] = g[1] = g[2] = 10.f;
    run (3, 0.05f);
    check (f.chord == Em, "an Em stab latches Em");
    g[2] = 0.f;                                                     // G4 and B4 keep ringing
    run (3, 0.2f);
    check (f.chord == Em, "the chord holds while its notes fade out");
    vo[2] = 10 / 12.f; vo[3] = 2 / 12.f + 1.f; vo[4] = 7 / 12.f + 1.f; g[2] = g[3] = g[4] = 10.f;   // Gm starts over them
    run (5, 0.05f);
    check (f.chord == Gm, "the newest stab wins over notes still ringing");

    // chord colors (bank B pads) and pad decoding
    int w[6];
    check (chordRoot (Em) == 4 && chordRoot (Bb) == 10 && chordRoot (0) == -1, "chord roots: Em = E, Bb = Bb, none = -1");
    check (padIndex ((36 - 60) / 12.f) == 0 && padIndex ((43 - 60) / 12.f) == 7, "bank A pads 36-43 -> 0-7");
    check (padIndex ((44 - 60) / 12.f) == 8 && padIndex ((51 - 60) / 12.f) == 15, "bank B pads 44-51 -> 8-15");
    check (padIndex ((35 - 60) / 12.f) == -1 && padIndex ((52 - 60) / 12.f) == -1, "notes outside the pads are ignored");
    auto is = [&] (int n, std::initializer_list<int> l) { if (n != (int) l.size()) return false; int i = 0; for (int p : l) if (w[i++] != p) return false; return true; };
    check (is (colorVoices (64, C_SEVENTH, Em, EPHR, 4, w), { 64, 59, 55, 50 }), "E4 over Em, 7th: E4 B3 G3 D3");
    check (is (colorVoices (64, C_ADDB9, Em, EPHR, 4, w), { 64, 59, 55, 53 }), "E4 over Em, add b9: E4 B3 G3 F3");
    check (is (colorVoices (64, C_SUS4, Em, EPHR, 4, w), { 64, 59, 45 }), "E4 over Em, sus4: E B A, no G");
    check (is (colorVoices (65, C_SUS4, F, EPHR, 4, w), { 65, 60, 55 }), "F4 over F, sus4 falls back to sus2: F C G");
    check (is (colorVoices (64, C_TRIAD, 0, EPHR, 4, w), { 64, 59, 55 }), "no chord yet: triad on the key root");

    // every color over every song chord: in the chord scale, lead on top, the right voice count, gap-3 colors never rub
    static const int COLOR_N[C_N] = { 1, 3, 4, 5, 6, 4, 3, 3, 4, 4, 3, 3, 3, 4 };
    bool cIn = true, cTop = true, cN = true, cRub = true;
    for (Mask c : chords)
    {
        const Mask s = chordScale (EPHR, c);
        for (int key = 40; key <= 90; ++key)
            for (int col = 1; col < C_N; ++col)
            {
                const int lead = snap (key, s);
                const int n = colorVoices (lead, col, c, s, 4, w);
                cN = cN && n == COLOR_N[col];
                const bool mayRub = col == C_CLUSTER || col == C_ADDB9 || col == C_SUS2 || col == C_SUS4;   // sus4 may fall back to sus2
                for (int i = 0; i < n; ++i)
                {
                    cIn = cIn && (s & bit (w[i]));
                    cTop = cTop && (i == 0 ? w[0] == lead : w[i] < lead);
                    if (! mayRub)
                        for (int j = i + 1; j < n; ++j) { const int d = std::abs (w[i] - w[j]); cRub = cRub && d != 1 && d != 2; }
                }
            }
    }
    check (cIn, "every color voice stays in the chord scale");
    check (cTop, "the lead is always the top voice");
    check (cN, "every color has its voice count");
    check (cRub, "gap-3 colors never put voices a semitone or a tone apart");
}
