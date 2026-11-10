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

inline void testToolsShape()
{
    ToolCtx c;
    {
        std::vector<Note> n = { Note (12, 24, 60, 0.8f) };
        std::vector<char> s (1, 0);
        QuantizeP q; q.start = 0.5f;
        quantize (n, s, q, c);
        check (n[0].start == 6 || n[0].start == 18, "quantize 50% strength moves halfway", n[0].start);   // 12 is a tie: either neighbour is fine
        std::vector<Note> m = { Note (11, 24, 60, 0.8f) };
        std::vector<char> ms (1, 0);
        QuantizeP narrow; narrow.sens = 0.5f;   // window 6 ticks; 11 is 11 from 0 and 13 from 24
        quantize (m, ms, narrow, c);
        check (m[0].start == 11, "quantize sensitivity: a note outside the window keeps its start", m[0].start);
        std::vector<Note> e = { Note (0, 30, 60, 0.8f) };
        std::vector<char> es (1, 0);
        QuantizeP qe; qe.lenMode = LEN_QUANT;
        quantize (e, es, qe, c);
        check (e[0].len == 24, "quantize length mode snaps the length", e[0].len);
    }
    {
        std::vector<Note> n = { Note (10, PPQ, 60, 0.8f) };
        std::vector<char> s (1, 0);
        ChopP p; p.absolute = true;
        chop (n, s, p, c);
        n = sorted (n);
        check (n.size() == 5 && n[0].start == 10 && n[0].len == 14 && n[1].start == 24, "absolute chop slices on the loop grid", (double) n.size());
        std::vector<Note> g = { Note (0, PPQ, 60, 0.8f) };
        std::vector<char> gs (1, 0);
        ChopP half; half.gate = 0.5f;
        chop (g, gs, half, c);
        check (g.size() == 4 && g[0].len == 12, "chop gate 50% halves the pieces", g[0].len);
    }
    {
        std::vector<Note> n = { Note (0, 10, 60, 0.8f), Note (48, 10, 62, 0.8f), Note (96, 10, 64, 0.8f) };
        std::vector<char> s (3, 0);
        quickLegato (n, s, c);
        n = sorted (n);
        check (n[0].len == 48 && n[1].len == 48 && n[2].len == 10, "legato: each note ends where the next starts; the last keeps its length");
        ArticulateP st = articulatePreset (ART_STACCATO);
        articulate (n, s, st, c);
        n = sorted (n);
        check (n[0].len == 17, "staccato: 35% of the length", n[0].len);   // round (48 * 0.35) = 17
    }
    {
        const std::vector<Note> orig = { Note (0, 24, 60, 0.8f), Note (48, 48, 64, 0.5f), Note (120, 12, 67, 0.3f) };
        std::vector<Note> n = orig;
        std::vector<char> s (3, 0);
        FlipP h;
        flip (n, s, h, c);
        flip (n, s, h, c);
        check (sorted (n) == sorted (orig), "flip horizontal twice = original");
        FlipP v; v.horizontal = false; v.vertical = true;
        flip (n, s, v, c);
        bool mirrored = true;
        for (const Note& x : n) if (x.start == 0) mirrored = x.pitch == 67;
        flip (n, s, v, c);
        check (mirrored && sorted (n) == sorted (orig), "flip vertical mirrors pitch; twice = original");
    }
}
