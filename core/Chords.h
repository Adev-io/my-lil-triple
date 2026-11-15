// Chords.h: chord types, the seven modes, scale snap and the one-click chord stamp. No Rack dependency.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <vector>

struct ChordType { const char* name; int n; int iv[6]; };   // semitones above the root

static const ChordType CHORDS[] = {
    { "NOTE", 1, { 0 } },
    { "MAJ", 3, { 0, 4, 7 } },            { "MIN", 3, { 0, 3, 7 } },            { "AUG", 3, { 0, 4, 8 } },
    { "DIM", 3, { 0, 3, 6 } },            { "SUS2", 3, { 0, 2, 7 } },           { "SUS4", 3, { 0, 5, 7 } },
    { "POWER", 3, { 0, 7, 12 } },         { "MAJ6", 4, { 0, 4, 7, 9 } },        { "MIN6", 4, { 0, 3, 7, 9 } },
    { "MAJ7", 4, { 0, 4, 7, 11 } },       { "MIN7", 4, { 0, 3, 7, 10 } },       { "DOM7", 4, { 0, 4, 7, 10 } },
    { "AUG7", 4, { 0, 4, 8, 10 } },       { "DIM7", 4, { 0, 3, 6, 9 } },        { "MIN7B5", 4, { 0, 3, 6, 10 } },
    { "MINMAJ7", 4, { 0, 3, 7, 11 } },    { "ADD9", 4, { 0, 4, 7, 14 } },       { "MINADD9", 4, { 0, 3, 7, 14 } },
    { "7SUS4", 4, { 0, 5, 7, 10 } },      { "9SUS4", 5, { 0, 5, 7, 10, 14 } },  { "MAJ9", 5, { 0, 4, 7, 11, 14 } },
    { "MIN9", 5, { 0, 3, 7, 10, 14 } },   { "DOM9", 5, { 0, 4, 7, 10, 14 } },   { "6/9", 5, { 0, 4, 7, 9, 14 } },
    { "MAJ11", 6, { 0, 4, 7, 11, 14, 17 } }, { "MIN11", 6, { 0, 3, 7, 10, 14, 17 } }, { "DOM11", 6, { 0, 4, 7, 10, 14, 17 } },
    { "MAJ13", 6, { 0, 4, 7, 11, 14, 21 } }, { "MIN13", 6, { 0, 3, 7, 10, 14, 21 } }, { "DOM13", 6, { 0, 4, 7, 10, 14, 21 } },
    { "MAJ7#11", 5, { 0, 4, 7, 11, 18 } }, { "7B9", 5, { 0, 4, 7, 10, 13 } },   { "7#9", 5, { 0, 4, 7, 10, 15 } },
    { "7B5", 4, { 0, 4, 6, 10 } },        { "7#5", 4, { 0, 4, 8, 10 } },
};
static const int CHORD_N = (int) (sizeof (CHORDS) / sizeof (CHORDS[0]));

static const char* const KEY_NAMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const MODE_NAMES[7] = { "IONIAN", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "AEOLIAN", "LOCRIAN" };
static const int MODES[7][7] = {
    { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 4, 6, 7, 9, 11 },
    { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 3, 5, 6, 8, 10 },
};

// key 0..11 (0 = C), mode 0..6 (MODE_NAMES)
inline bool inScale (int pitch, int key, int mode)
{
    const int d = ((pitch - key) % 12 + 12) % 12;
    for (int i = 0; i < 7; ++i) if (MODES[mode][i] == d) return true;
    return false;
}

inline int snapUp (int pitch, int key, int mode)
{
    while (pitch < 127 && ! inScale (pitch, key, mode)) ++pitch;
    return pitch;
}

// The pitches one click drops: root + the type's intervals, each moved up into the scale when snap is on.
// Sorted, no duplicates, nothing above 127.
inline std::vector<int> stamp (int root, int type, int key, int mode, bool snap)
{
    std::vector<int> out;
    const ChordType& c = CHORDS[type];
    for (int i = 0; i < c.n; ++i)
    {
        int p = root + c.iv[i];
        if (p > 127) continue;
        if (snap) p = snapUp (p, key, mode);
        if (std::find (out.begin(), out.end(), p) == out.end()) out.push_back (p);
    }
    std::sort (out.begin(), out.end());
    return out;
}
