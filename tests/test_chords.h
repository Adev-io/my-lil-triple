// test_chords.h: chord stamp and scale snap.
#pragma once
#include "check.h"
#include "Chords.h"
#include <string>

inline int chordIndex (const char* name)
{
    for (int i = 0; i < CHORD_N; ++i) if (std::string (CHORDS[i].name) == name) return i;
    return -1;
}

inline void testChords()
{
    check (CHORD_N == 36, "chord list is NOTE + 35 types", CHORD_N);
    check (stamp (60, chordIndex ("MAJ7"), 0, 0, false) == std::vector<int> ({ 60, 64, 67, 71 }), "Maj7 on C4 is C E G B");
    check (stamp (62, chordIndex ("MAJ"), 2, 1, false) == std::vector<int> ({ 62, 66, 69 }), "scale snap off keeps F#");
    check (stamp (62, chordIndex ("MAJ"), 2, 1, true) == std::vector<int> ({ 62, 67, 69 }), "scale snap in D Dorian moves F# up to G");
    check (stamp (60, 0, 0, 0, true) == std::vector<int> ({ 60 }), "NOTE stamps one note");
    check (stamp (120, chordIndex ("DOM13"), 0, 0, false).size() == 3, "notes above 127 are dropped");
    bool seven = true;
    for (int m = 0; m < 7; ++m)
    {
        int n = 0;
        for (int p = 0; p < 12; ++p) n += inScale (p, 0, m) ? 1 : 0;
        seven = seven && n == 7;
    }
    check (seven, "every mode has seven notes");
    check (snapUp (61, 0, 0) == 62 && snapUp (60, 0, 0) == 60, "snapUp moves up to the next scale note");
}
