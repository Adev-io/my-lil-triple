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

inline bool hasNote (const std::vector<Note>& n, int start, int pitch)
{
    for (const Note& x : n) if (x.start == start && x.pitch == pitch) return true;
    return false;
}

// A tool never drops a scoped note: out-of-loop starts, the MAX_NOTES cap, grace notes.
inline void testToolsNoLoss()
{
    ToolCtx c;
    {
        ToolCtx one = c;
        one.loopTicks = BAR;   // shrunk loop: the note at 400 is past the end
        std::vector<Note> n = { Note (0, 48, 60, 0.8f), Note (400, 48, 62, 0.8f) };
        std::vector<char> s (2, 1);
        ChopP p;
        chop (n, s, p, one);
        bool a = false, b = false;
        for (const Note& x : n) { a = a || x.pitch == 60; b = b || (x.pitch == 62 && x.start == BAR - 1 && x.len == 48); }
        check (a && b && parallel (n, s), "chop keeps a note that starts past the loop end (clamped like every tool)", (double) n.size());
        std::vector<Note> m = { Note (0, 48, 60, 0.8f), Note (400, 48, 62, 0.8f) };
        std::vector<char> ms (2, 1);
        arpeggiate (m, ms, ArpP(), one);
        bool c60 = false, c62 = false;
        for (const Note& x : m) { c60 = c60 || x.pitch == 60; c62 = c62 || (x.pitch == 62 && x.start == BAR - 1); }
        check (c60 && c62 && parallel (m, ms), "arpeggiate keeps a chord that starts past the loop end", (double) m.size());
    }
    {
        std::vector<Note> n;   // 100 chords x 96 steps would be 9600 notes
        for (int i = 0; i < 100; ++i) n.push_back (Note (i * 12, 96, 10 + i, 0.8f));
        std::vector<char> s (n.size(), 0);
        ArpP a; a.step = 1; a.gate = 1.f;
        arpeggiate (n, s, a, c);
        bool all = true;
        for (int i = 0; i < 100; ++i) all = all && hasNote (n, i * 12, 10 + i);
        check ((int) n.size() <= MAX_NOTES && all && parallel (n, s), "arpeggiate at MAX_NOTES: every source chord still has a note at its start", (double) n.size());
        std::vector<Note> m;
        for (int i = 0; i < 100; ++i) m.push_back (Note (i * 12, 96, 10 + i, 0.8f));
        std::vector<char> ms (m.size(), 0);
        ChopP p; p.slice = 2;   // 48 pieces each
        chop (m, ms, p, c);
        bool allc = true;
        for (int i = 0; i < 100; ++i) allc = allc && hasNote (m, i * 12, 10 + i);
        check ((int) m.size() <= MAX_NOTES && allc && parallel (m, ms), "chop at MAX_NOTES: every source note still has a note at its start", (double) m.size());
    }
    {
        std::vector<Note> n;   // 4000 notes + their grace notes would be 8000
        for (int i = 0; i < 4000; ++i) n.push_back (Note (24 + (i / 100) * 24, 10, i % 100, 0.8f));
        std::vector<char> s (n.size(), 0);
        const std::vector<Note> orig = n;
        flam (n, s, FlamP(), c);
        bool all = (int) n.size() <= MAX_NOTES;
        for (const Note& o : orig) all = all && hasNote (n, o.start, o.pitch);
        check (all && parallel (n, s), "flam at MAX_NOTES: grace notes give way, real notes stay", (double) n.size());
        std::vector<Note> k = { Note (24, 48, 60, 1.f), Note (12, 12, 60, 0.5f) };   // the grace note of the first lands on the second
        std::vector<char> ks (2, 0);
        flam (k, ks, FlamP(), c);
        bool real = false;
        for (const Note& x : k) if (x.start == 12 && x.pitch == 60) real = x.len == 12 && near (x.vel, 0.5);
        check (real, "flam: a real note wins a start + pitch clash with a grace note");
    }
    {
        std::vector<Note> n = { Note (0, 24, 61, 0.5f), Note (96, 24, 63, 0.5f), Note (192, 24, 66, 0.5f) };
        std::vector<char> s (3, 0);
        const size_t before = n.size();
        LimitP l; l.low = 60; l.high = 64; l.scaleFix = FIX_ABOVE;
        limit (n, s, l, c);
        strum (n, s, StrumP(), c);
        ScaleLevelsP sl; sl.multiply = 0.5f;
        scaleLevels (n, s, sl, c);
        LfoP lfo;
        velocityLfo (n, s, lfo, c);
        check (n.size() == before && parallel (n, s), "limit, strum (lone notes), scale levels and LFO never lose a note", (double) n.size());
    }
    {
        const std::vector<Note> base = { Note (0, 96, 30, 0.5f), Note (96, 96, 31, 0.5f), Note (192, 96, 32, 0.5f) };   // below the pattern octave
        std::vector<Note> on = base, off = base;
        std::vector<char> son (3, 0), soff (3, 0);
        RandomizeP r; r.pattern = true; r.seed = 3; r.velAmount = 0.5f;
        randomize (on, son, r, c);
        r.levels = false;
        randomize (off, soff, r, c);
        std::vector<Note> gOn, gOff;
        for (const Note& x : on) if (x.pitch >= 60) gOn.push_back (x);
        for (const Note& x : off) if (x.pitch >= 60) gOff.push_back (x);
        check (! gOn.empty() && sorted (gOn) == sorted (gOff), "randomize: the generated pattern is the same with Levels on or off", (double) gOn.size());
    }
}

inline void testToolsTime()
{
    ToolCtx c;
    const std::vector<Note> ceg = { Note (0, PPQ, 60, 0.8f), Note (0, PPQ, 64, 0.8f), Note (0, PPQ, 67, 0.8f) };
    {
        std::vector<Note> n = ceg;
        std::vector<char> s (3, 0);
        ArpP a;
        arpeggiate (n, s, a, c);
        n = sorted (n);
        check (n.size() == 4 && n[0].pitch == 60 && n[1].pitch == 64 && n[2].pitch == 67 && n[3].pitch == 60
               && n[1].start == 24 && n[3].start == 72 && n[0].len == 22, "arp up 1 oct 1/16 over a beat: C E G C", (double) n.size());   // round (24 * 0.9) = 22
        std::vector<Note> u = ceg;
        std::vector<char> us (3, 0);
        ArpP ud; ud.pattern = ARP_UPDOWN; ud.step = 12;
        arpeggiate (u, us, ud, c);
        u = sorted (u);
        check (u.size() == 8 && u[3].pitch == 64 && u[4].pitch == 60, "arp up-down: C E G E C E G E");
        std::vector<Note> o = ceg;
        std::vector<char> os (3, 0);
        ArpP two; two.octaves = 2; two.step = 12;
        arpeggiate (o, os, two, c);
        o = sorted (o);
        check (o.size() == 8 && o[3].pitch == 72 && o[5].pitch == 79, "arp 2 octaves climbs into the next octave");
    }
    {
        std::vector<Note> n = ceg;
        std::vector<char> s (3, 0);
        StrumP p;
        strum (n, s, p, c);
        n = sorted (n);
        check (n[0].pitch == 60 && n[0].start == 0 && n[1].start == 12 && n[2].start == 24, "strum + : low to high, even spacing at tension 0");
        check (n[2].start + n[2].len == PPQ, "strum preserve end keeps the ends");
        std::vector<Note> d = ceg;
        std::vector<char> ds (3, 0);
        StrumP neg; neg.time = -0.25f;
        strum (d, ds, neg, c);
        d = sorted (d);
        check (d[0].pitch == 67 && d[2].pitch == 60, "strum - : high to low");
        std::vector<Note> v = ceg;
        std::vector<char> vs (3, 0);
        StrumP fall; fall.velFall = 0.5f;
        strum (v, vs, fall, c);
        v = sorted (v);
        check (near (v[0].vel, 0.8, 1e-4) && near (v[2].vel, 0.4, 1e-4), "strum velocity fall: last note loses 50%", v[2].vel);
    }
    {
        std::vector<Note> n = { Note (96, 48, 60, 1.f) };
        std::vector<char> s (1, 0);
        FlamP f;
        flam (n, s, f, c);
        n = sorted (n);
        check (n.size() == 2 && n[0].start == 84 && n[0].len == 12 && near (n[0].vel, 0.6) && n[1].start == 96, "flam: grace note 12 ticks before at 60%");
        std::vector<Note> a = { Note (96, 48, 60, 1.f) };
        std::vector<char> as (1, 0);
        FlamP after; after.before = false;
        flam (a, as, after, c);
        a = sorted (a);
        check (a.size() == 2 && a[0].start == 96 && a[1].start == 108 && a[1].len == 36, "flam after: note delayed, grace on the beat");
        check (parallel (a, as), "sel stays parallel after flam");
    }
    {   // edge cases
        std::vector<Note> n = { Note (0, 96, 60, 0.8f), Note (0, 48, 64, 0.7f) };
        std::vector<char> s (2, 0);
        ArpP a; a.step = 48; a.gate = 1.f;
        arpeggiate (n, s, a, c);
        n = sorted (n);
        check (n.size() == 2 && n[0].pitch == 60 && n[0].len == 48 && n[1].pitch == 64 && n[1].start == 48 && near (n[1].vel, 0.7),
               "arp runs over the longest note; velocity from the source note", (double) n.size());
        std::vector<Note> pl = { Note (0, PPQ, 67, 0.8f), Note (0, PPQ, 60, 0.8f), Note (0, PPQ, 64, 0.8f) };
        std::vector<char> ps (3, 0);
        ArpP played; played.pattern = ARP_PLAYED; played.step = 48;
        arpeggiate (pl, ps, played, c);
        pl = sorted (pl);
        check (pl.size() == 2 && pl[0].pitch == 67 && pl[1].pitch == 60, "arp as played: pool in note order (G C)");
        std::vector<Note> r1 = ceg, r2 = ceg;
        std::vector<char> rs1 (3, 0), rs2 (3, 0);
        ArpP rnd; rnd.pattern = ARP_RANDOM; rnd.step = 6; rnd.seed = 5;
        arpeggiate (r1, rs1, rnd, c);
        arpeggiate (r2, rs2, rnd, c);
        bool inPool = r1.size() == 16;
        for (const Note& x : r1) inPool = inPool && (x.pitch == 60 || x.pitch == 64 || x.pitch == 67);
        check (r1 == r2 && inPool && parallel (r1, rs1), "arp random: same seed same run, only chord pitches", (double) r1.size());
        std::vector<Note> dn = ceg;
        std::vector<char> dns (3, 0);
        ArpP fl; fl.order = OCT_FLIP; fl.octaves = 2; fl.step = 12;
        arpeggiate (dn, dns, fl, c);
        dn = sorted (dn);
        check (dn[0].pitch == 72 && dn[3].pitch == 60, "arp flipped octave order starts in the upper octave", dn[0].pitch);
        std::vector<Note> one = { Note (0, 48, 60, 0.8f) };
        std::vector<char> os (1, 0);
        arpeggiate (one, os, ArpP(), c);
        check (one.size() == 2 && one[0].pitch == 60 && one[1].pitch == 60, "arp on a lone note repeats it");
    }
    {
        std::vector<Note> n = { Note (0, 48, 60, 0.8f) };
        const std::vector<Note> before = n;
        std::vector<char> s (1, 0);
        strum (n, s, StrumP(), c);
        check (n == before, "strum on a 1-note chord changes nothing (no n-1 divide)");
        std::vector<Note> two = { Note (0, 48, 60, 0.8f), Note (0, 48, 64, 0.8f), Note (96, 48, 60, 0.8f), Note (96, 48, 64, 0.8f), Note (192, 48, 60, 0.8f), Note (192, 48, 64, 0.8f) };
        std::vector<char> ts (6, 0);
        StrumP alt; alt.alternate = true;
        strum (two, ts, alt, c);
        two = sorted (two);
        check (two[0].pitch == 60 && two[2].pitch == 64 && two[2].start == 96 && two[3].pitch == 60 && two[3].start == 120 && two[4].pitch == 60 && two[4].start == 192,
               "strum alternate flips direction on every next chord");
        std::vector<Note> sh = { Note (0, 10, 60, 0.8f), Note (0, 10, 64, 0.8f) };
        std::vector<char> ss (2, 0);
        StrumP big; big.time = 1.f;
        strum (sh, ss, big, c);
        sh = sorted (sh);
        check (sh[1].start == 96 && sh[1].len == 1 && sh[0].len == 10, "strum preserve end never shrinks below 1 tick", sh[1].len);
        std::vector<Note> tn = ceg;
        std::vector<char> tns (3, 0);
        StrumP tp; tp.tension = 1.f;
        strum (tn, tns, tp, c);
        tn = sorted (tn);
        check (tn[1].start == 20 && tn[2].start == 24, "strum tension + bunches later notes", tn[1].start);   // 24 x 0.5^(2^-2) = 20.18
    }
    {
        std::vector<Note> n = { Note (0, 48, 60, 1.f) };
        std::vector<char> s (1, 0);
        flam (n, s, FlamP(), c);
        check (n.size() == 1 && n[0].start == 0 && n[0].len == 48, "flam before at start 0: no grace note");
        std::vector<Note> e = { Note (5, 48, 60, 1.f) };
        std::vector<char> es (1, 0);
        flam (e, es, FlamP(), c);
        e = sorted (e);
        check (e.size() == 2 && e[0].start == 0 && e[0].len == 5 && e[1].start == 5, "flam near the start: a shorter grace note from tick 0");
        std::vector<Note> t = { Note (96, 8, 60, 1.f) };
        std::vector<char> ts (1, 0);
        FlamP after; after.before = false;
        flam (t, ts, after, c);
        t = sorted (t);
        check (t.size() == 2 && t[1].start == 108 && t[1].len == 1, "flam after: the delayed note keeps at least 1 tick", t[1].len);
    }
    testToolsNoLoss();
}

inline void testToolsLevels()
{
    ToolCtx c;
    c.key = 0; c.mode = 0;   // C major
    {
        std::vector<Note> n = { Note (0, 24, 30, 0.8f), Note (24, 24, 61, 0.8f), Note (48, 24, 100, 0.8f) };
        std::vector<char> s (3, 0);
        LimitP p; p.scaleFix = FIX_ABOVE;
        limit (n, s, p, c);
        bool ok = true;
        for (const Note& x : n) ok = ok && x.pitch >= 48 && x.pitch <= 72 && inScale (x.pitch, 0, 0);
        check (ok, "limit: every pitch inside C3..C5 and in C major");
    }
    {
        std::vector<Note> a = { Note (0, 24, 60, 0.5f), Note (24, 24, 62, 0.5f), Note (48, 24, 64, 0.5f) }, b = a;
        std::vector<char> sa (3, 0), sb (3, 0);
        RandomizeP r; r.velAmount = 1.f;
        randomize (a, sa, r, c);
        randomize (b, sb, r, c);
        bool range = true;
        for (const Note& x : a) range = range && x.vel >= 0.f && x.vel <= 1.f;
        check (a == b && range, "randomize: same seed, same result; velocity stays 0..1");
        std::vector<Note> e;
        std::vector<char> es;
        RandomizeP g; g.levels = false; g.pattern = true; g.population = 1.f;
        randomize (e, es, g, c);
        bool inKey = ! e.empty();
        for (const Note& x : e) inKey = inKey && inScale (x.pitch, 0, 0) && x.pitch >= 60 && x.pitch < 72;
        check (inKey && parallel (e, es), "randomize pattern: in-scale notes in the chosen octave", (double) e.size());
    }
    {
        std::vector<Note> n = { Note (0, 24, 60, 0.2f), Note (24, 24, 62, 0.9f) };
        const std::vector<Note> before = n;
        std::vector<char> s (2, 0);
        ScaleLevelsP id;
        scaleLevels (n, s, id, c);
        check (n == before, "scale levels at defaults changes nothing");
        ScaleLevelsP half; half.multiply = 0.5f;
        scaleLevels (n, s, half, c);
        check (near (n[0].vel, 0.35, 1e-5) && near (n[1].vel, 0.7, 1e-5), "scale levels multiply 50% about 0.5");
    }
    {
        std::vector<Note> n = { Note (0, 96, 60, 0.5f), Note (96, 96, 60, 0.5f), Note (192, 96, 60, 0.5f), Note (288, 96, 60, 0.5f) };
        std::vector<char> s (4, 0);
        LfoP sq; sq.shape = LFO_SQUARE; sq.cycles = 2.f; sq.depth = 1.f;
        velocityLfo (n, s, sq, c);
        n = sorted (n);
        check (near (n[0].vel, 1.0) && near (n[1].vel, 0.0) && near (n[2].vel, 1.0) && near (n[3].vel, 0.0), "square LFO alternates full and zero");
    }
    {   // edge cases
        std::vector<Note> n = { Note (0, 24, 71, 0.8f), Note (24, 24, 72, 0.8f), Note (48, 24, 30, 0.8f) };
        std::vector<char> s (3, 0);
        LimitP nar; nar.low = 60; nar.high = 64;   // only pitch classes C..E fit
        limit (n, s, nar, c);
        n = sorted (n);
        check (n[0].pitch == 64 && n[1].pitch == 60 && n[2].pitch == 60 && parallel (n, s),
               "limit under 12 semitones: a pitch class that cannot fit clamps to the nearer edge", n[0].pitch);   // B (71) has no home in 60..64 -> high edge 64; C (72) -> 60; F# (30) -> low edge 60
        std::vector<Note> w = { Note (0, 24, 30, 0.8f), Note (24, 24, 100, 0.8f), Note (48, 24, 60, 0.8f) };
        std::vector<char> ws (3, 0);
        LimitP wrap; wrap.wrap = true;
        limit (w, ws, wrap, c);
        w = sorted (w);
        check (w[0].pitch == 66 && w[1].pitch == 52 && w[2].pitch == 60, "limit wrap: out-of-range notes land in the opposite end's octave", w[0].pitch);   // F#1 -> F#4 (66), E7 (100) -> E3 (52)
        std::vector<Note> f = { Note (0, 24, 61, 0.8f), Note (24, 24, 63, 0.8f), Note (48, 24, 66, 0.8f) };
        std::vector<char> fs (3, 0);
        LimitP alt; alt.scaleFix = FIX_ALT;
        limit (f, fs, alt, c);
        f = sorted (f);
        check (f[0].pitch == 62 && f[1].pitch == 62 && f[2].pitch == 67, "limit scale fix alternate: above, below, above", f[1].pitch);   // C# -> D, D# -> D, F# -> G
        std::vector<Note> ed = { Note (0, 24, 70, 0.8f) };
        std::vector<char> eds (1, 0);
        LimitP edge; edge.high = 70; edge.scaleFix = FIX_ABOVE;   // A# is off scale; above leaves the range, so it goes below
        limit (ed, eds, edge, c);
        check (ed[0].pitch == 69, "limit scale fix never leaves the range", ed[0].pitch);

        std::vector<Note> off;
        std::vector<char> offs;
        ToolCtx nosnap = c; nosnap.snap = 1;
        RandomizeP g; g.levels = false; g.pattern = true; g.population = 1.f;
        randomize (off, offs, g, nosnap);
        check (off.empty(), "randomize pattern with snap OFF does nothing");
        std::vector<Note> p1, p2;
        std::vector<char> ps1, ps2;
        RandomizeP full; full.levels = false; full.pattern = true; full.stack = 3; full.range = 2; full.lenVar = 1.f; full.seed = 9;
        randomize (p1, ps1, full, c);
        randomize (p2, ps2, full, c);
        bool ok = ! p1.empty() && p1 == p2 && p1.size() == ps1.size();
        for (const Note& x : p1) ok = ok && inScale (x.pitch, 0, 0) && x.pitch >= 60 && x.pitch < 84 && x.len >= 24 && near (x.vel, 0.8, 1e-5);
        check (ok, "randomize pattern: stack, range and length variation are seeded and in scale", (double) p1.size());
        std::vector<Note> m = { Note (0, 96, 60, 0.5f), Note (96, 96, 62, 0.5f) };
        std::vector<char> ms = { 1, 0 };
        RandomizeP span; span.levels = false; span.pattern = true; span.population = 1.f;
        randomize (m, ms, span, c);
        bool within = m.size() > 2;
        for (const Note& x : m)
            if (! (x.start == 0 && x.len == 96 && x.pitch == 60) && ! (x.start == 96 && x.pitch == 62)) within = within && x.start < 96 && x.start % 24 == 0;
        check (within && m.size() == ms.size(), "randomize pattern: fills only the selection's span, existing notes survive", (double) m.size());
        RandomizeP down; down.bipolar = false; down.velAmount = 0.3f;
        std::vector<Note> d = { Note (0, 24, 60, 0.5f), Note (24, 24, 62, 0.5f), Note (48, 24, 64, 0.5f) };
        std::vector<char> dsel (3, 0);
        randomize (d, dsel, down, c);
        bool lower = true;
        for (const Note& x : d) lower = lower && x.vel <= 0.5f + 1e-6f && x.vel >= 0.2f - 1e-6f;
        check (lower, "randomize levels, not bipolar: velocities only go down");

        std::vector<Note> sl = { Note (0, 24, 60, 0.5f) };
        std::vector<char> sls (1, 0);
        ScaleLevelsP off2; off2.offset = 0.7f;
        scaleLevels (sl, sls, off2, c);
        check (near (sl[0].vel, 1.0), "scale levels offset clamps at 1", sl[0].vel);
        ScaleLevelsP ten; ten.tension = 50.f;   // v^(2^-1) = sqrt
        std::vector<Note> t = { Note (0, 24, 60, 0.25f) };
        std::vector<char> tsel (1, 0);
        scaleLevels (t, tsel, ten, c);
        check (near (t[0].vel, 0.5, 1e-5), "scale levels tension 50 = square root curve", t[0].vel);

        std::vector<Note> z = { Note (0, 0, 60, 0.5f) };
        const std::vector<Note> zb = z;
        std::vector<char> zs (1, 0);
        velocityLfo (z, zs, LfoP(), c);
        check (z == zb, "LFO with span B <= A changes nothing");
        std::vector<Note> sn = { Note (0, 96, 60, 0.5f), Note (96, 96, 60, 0.5f), Note (192, 96, 60, 0.5f) };
        std::vector<char> sns (3, 0);
        LfoP sine; sine.cycles = 1.f; sine.depth = 1.f; sine.phase = 90.f;   // cos at the start: 1, then -0.5, -0.5
        velocityLfo (sn, sns, sine, c);
        sn = sorted (sn);
        check (near (sn[0].vel, 1.0, 1e-5) && near (sn[1].vel, 0.25, 1e-4), "sine LFO with 90 degrees phase starts at the top", sn[1].vel);   // 0.5 + 0.5 x cos (2pi/3) = 0.25
        std::vector<Note> tr = { Note (0, 96, 60, 0.5f), Note (96, 96, 60, 0.5f), Note (192, 96, 60, 0.5f), Note (288, 96, 60, 0.5f) };
        std::vector<char> trs (4, 0);
        LfoP tri; tri.shape = LFO_TRI; tri.depth = 1.f;
        velocityLfo (tr, trs, tri, c);
        tr = sorted (tr);
        check (near (tr[0].vel, 0.5, 1e-5) && near (tr[1].vel, 1.0, 1e-5) && near (tr[2].vel, 0.5, 1e-5) && near (tr[3].vel, 0.0, 1e-5), "triangle LFO: mid, top, mid, bottom", tr[1].vel);
        std::vector<Note> part = { Note (0, 96, 60, 0.5f), Note (96, 96, 60, 0.5f), Note (192, 96, 60, 0.5f) };
        std::vector<char> ps = { 0, 1, 1 };
        LfoP sqr; sqr.shape = LFO_SQUARE; sqr.depth = 1.f;
        velocityLfo (part, ps, sqr, c);
        part = sorted (part);
        check (near (part[0].vel, 0.5) && near (part[1].vel, 1.0) && near (part[2].vel, 0.0), "LFO spans only the selection; unselected notes untouched", part[2].vel);
    }
}
