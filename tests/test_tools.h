// test_tools.h: My lil Roll's edit tools (core/Tools.h).
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "check.h"
#include "Tools.h"

inline std::vector<Note> sorted (std::vector<Note> n)
{
    std::sort (n.begin(), n.end(), [] (const Note& a, const Note& b) { return a.start != b.start ? a.start < b.start : a.pitch < b.pitch; });
    return n;
}
inline bool parallel (const std::vector<Note>& n, const std::vector<char>& s) { return n.size() == s.size(); }

inline void testToolsQuick()
{
    ToolCtx c;   // 4 bars, snap 1/16 = 24
    {
        std::vector<Note> n = { Note (10, 30, 60, 0.8f), Note (40, 30, 62, 0.8f) };
        std::vector<char> s (2, 0);
        quickQuantizeStarts (n, s, c);
        n = sorted (n);
        check (n[0].start == 0 && n[1].start == 48 && n[0].len == 30, "quick quantize starts: to nearest snap, length kept");
    }
    {
        std::vector<Note> n = { Note (10, 30, 60, 0.8f) };
        std::vector<char> s (1, 0);
        quickQuantize (n, s, c);
        check (n[0].start == 0 && n[0].len == 48, "quick quantize: start and end to the grid", n[0].len);   // end 40 -> 48
    }
    {
        std::vector<Note> n = { Note (0, PPQ, 60, 0.8f), Note (0, PPQ, 72, 0.5f) };
        std::vector<char> s = { 1, 0 };
        quickChop (n, s, c);
        int c60 = 0, c72 = 0;
        for (const Note& x : n) { if (x.pitch == 60) { ++c60; check (x.len == 24, "chopped piece is one snap"); } if (x.pitch == 72) ++c72; }
        check (c60 == 4 && c72 == 1 && parallel (n, s), "quick chop: selected beat -> 4 pieces, unselected untouched", c60);
        int selected = 0;
        for (size_t i = 0; i < n.size(); ++i) selected += s[i] && n[i].pitch == 60;
        check (selected == 4, "the pieces become the selection", selected);
    }
    {
        std::vector<Note> n = { Note (0, 48, 60, 0.9f), Note (48, 48, 60, 0.4f), Note (100, 10, 60, 0.8f), Note (0, 96, 64, 0.8f) };
        std::vector<char> s (4, 0);
        glue (n, s, c);
        n = sorted (n);
        check (n.size() == 3 && n[0].pitch == 60 && n[0].len == 96 && near (n[0].vel, 0.9), "glue: touching same pitch merges, keeps first velocity");
    }
    {
        std::vector<Note> n = { Note (5, 50, 60, 0.8f) };
        std::vector<char> s (1, 0);
        discardLengths (n, s, c);
        check (n[0].len == 24, "discard lengths: len = snap", n[0].len);
    }
    {
        std::vector<Note> n = { Note (0, 24, 60, 0.8f), Note (4 * BAR - 24, 24, 62, 0.8f) };
        std::vector<char> s (2, 0);
        rotate (n, s, c, -1);
        n = sorted (n);
        check (n[0].start == 4 * BAR - 48 && n[1].start == 4 * BAR - 24 && n[1].pitch == 60, "rotate left wraps inside the loop");
    }
    {
        ToolCtx off = c;
        off.snap = 1;
        std::vector<Note> n = { Note (10, 30, 60, 0.8f) };
        std::vector<char> s (1, 0);
        const std::vector<Note> before = n;
        quickQuantize (n, s, off);
        quickChop (n, s, off);
        check (n == before, "snap OFF: snap-based tools do nothing");
    }
    {
        std::vector<Note> n = { Note (0, 24, 60, 0.8f), Note (24, 24, 60, 0.8f) };
        std::vector<char> s (2, 0);
        rotate (n, s, c, -1);   // the first wraps to the end; no collision
        std::vector<Note> d = { Note (0, 24, 60, 0.8f), Note (24, 24, 60, 0.8f) };
        std::vector<char> ds (2, 0);
        ToolCtx one = c; one.loopTicks = 48;
        rotate (d, ds, one, -1);
        check (d.size() == 2 && parallel (d, ds), "rotate in a full loop keeps both notes");
    }
}
