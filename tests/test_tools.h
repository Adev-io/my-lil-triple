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
    {
        std::vector<Note> n = { Note (0, 24, 60, 0.8f), Note (48, 12, 64, 0.5f), Note (72, 36, 67, 0.3f) };
        const std::vector<Note> orig = n;
        std::vector<char> s (3, 0);
        FlipP f; f.preserveStarts = true;
        flip (n, s, f, c);
        n = sorted (n);
        check (n[0].start == 0 && n[0].pitch == 67 && n[0].len == 36 && near (n[0].vel, 0.3)
               && n[1].start == 48 && n[1].pitch == 64 && n[1].len == 12 && near (n[1].vel, 0.5)
               && n[2].start == 72 && n[2].pitch == 60 && n[2].len == 24 && near (n[2].vel, 0.8), "flip preserve starts: slots stay, notes reverse through them");
        flip (n, s, f, c);
        check (sorted (n) == sorted (orig), "flip preserve starts twice = original");
    }
    {
        std::vector<Note> e = { Note (10, 30, 60, 0.8f) };
        std::vector<char> es (1, 0);
        QuantizeP qe; qe.lenMode = END_QUANT;
        quantize (e, es, qe, c);
        check (e[0].start == 0 && e[0].len == 48, "quantize end: start 0, end 40 -> 48", e[0].len);
        std::vector<Note> l = { Note (10, 30, 60, 0.8f) };
        std::vector<char> ls (1, 0);
        QuantizeP ql; ql.lenMode = END_LEAVE;
        quantize (l, ls, ql, c);
        check (l[0].start == 0 && l[0].len == 40, "quantize leave end: the end stays at 40", l[0].len);
        std::vector<Note> h = { Note (0, 30, 60, 0.8f) };
        std::vector<char> hs (1, 0);
        QuantizeP qh; qh.lenMode = LEN_QUANT; qh.len = 0.5f;
        quantize (h, hs, qh, c);
        check (h[0].len == 27, "quantize length strength 50%: 30 -> 27", h[0].len);
    }
    {
        std::vector<Note> n = { Note (0, 10, 60, 0.8f), Note (48, 10, 62, 0.8f) };
        std::vector<char> s (2, 0);
        articulate (n, s, articulatePreset (ART_SMALLGAP), c);
        n = sorted (n);
        check (n[0].len == 46 && n[1].len == 10, "articulate small gap: ends 2 ticks before the next", n[0].len);
        std::vector<Note> k = { Note (0, 96, 60, 0.8f), Note (48, 48, 64, 0.8f) };
        std::vector<char> ks (2, 0);
        articulate (k, ks, articulatePreset (ART_CHOPCHORDS), c);
        k = sorted (k);
        check (k[0].len == 48 && k[1].len == 48, "articulate chop chords: a sounding note is cut where the next chord starts", k[0].len);
        const std::vector<Note> orig = { Note (0, 40, 60, 0.8f), Note (48, 40, 62, 0.8f), Note (96, 40, 64, 0.8f) };
        ArticulateP v; v.variation = 1.f; v.seed = 7; v.useLengths = true;
        std::vector<Note> a = orig, b = orig;
        std::vector<char> as (3, 0), bs (3, 0);
        articulate (a, as, v, c);
        articulate (b, bs, v, c);
        bool shorter = true;
        for (const Note& x : a) shorter = shorter && x.len <= 40 && x.len >= 1;
        check (a == b && shorter, "articulate variation: same seed -> same result, never longer");
    }
    {
        std::vector<Note> n = { Note (0, 24, 60, 0.8f) };
        std::vector<char> s (1, 0);
        rotate (n, s, c, 1);
        check (n[0].start == 24, "rotate right moves one snap later", n[0].start);
    }
    {
        std::vector<Note> n = { Note (0, 24, 60, 0.8f), Note (96, 24, 62, 0.8f) };
        std::vector<char> s = { 0, 1 };
        replaceScope (n, s, std::vector<int> (1, 1), { Note (48, 10, 64, 0.5f), Note (48, 20, 64, 0.9f) }, c);
        int at48 = 0;
        for (size_t i = 0; i < n.size(); ++i) if (n[i].start == 48) { ++at48; check (n[i].len == 20 && s[i] == 1, "replaceScope: the later output on a start+pitch wins and is selected"); }
        check (n.size() == 2 && at48 == 1 && parallel (n, s) && s[0] == 0, "replaceScope: duplicates collapse, other notes keep their selection", (double) n.size());
    }
    {
        std::vector<Note> n;
        for (int i = 0; i < MAX_NOTES; ++i) n.push_back (Note ((i / 128) * 10, 10, i % 128, 0.8f));
        std::vector<char> s (n.size(), 1);
        quickQuantize (n, s, c);
        check ((int) n.size() <= MAX_NOTES && n.size() > 0 && parallel (n, s), "4096 selected notes quantize (fast replaceScope)", (double) n.size());
    }
}
