// Tools.h: My lil Roll's FL-style edit tools. Every tool edits (notes, sel) in place through replaceScope. No Rack dependency.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include "Chords.h"
#include "Roll.h"

struct ToolCtx { int loopTicks = 4 * BAR, snap = 24, key = 0, mode = 0; };

struct Rng   // xorshift32; same seed -> same sequence
{
    uint32_t s;
    explicit Rng (uint32_t seed) : s (seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (next() >> 8) * (1.f / 16777216.f); }   // [0, 1)
};

// Rounds half up. Every tick value that comes out of a float goes through here.
inline int roundTicks (double v) { return (int) std::floor (v + 0.5); }

// The nearest multiple of g (ties go up), also for negative v.
inline int gridNear (int v, int g) { return (int) std::floor ((double) v / g + 0.5) * g; }

// A start on the grid: rounding up past the loop end gives the last grid line inside instead.
inline int gridStart (int v, int g, int loopTicks)
{
    const int r = gridNear (v, g);
    return r >= loopTicks ? r - g : r;
}

// Selected indices, or all 0..n-1 when none is selected.
inline std::vector<int> scope (const std::vector<char>& sel, int n)
{
    std::vector<int> out;
    for (int i = 0; i < n && i < (int) sel.size(); ++i) if (sel[i]) out.push_back (i);
    if (out.empty()) for (int i = 0; i < n; ++i) out.push_back (i);
    return out;
}

// Groups of idx with equal start. Groups ordered by start, each group ordered by pitch ascending.
inline std::vector<std::vector<int>> chords (const std::vector<Note>& notes, const std::vector<int>& idx)
{
    std::vector<int> v = idx;
    std::sort (v.begin(), v.end(), [&notes] (int a, int b)
    {
        return notes[a].start != notes[b].start ? notes[a].start < notes[b].start : notes[a].pitch < notes[b].pitch;
    });
    std::vector<std::vector<int>> out;
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i == 0 || notes[v[i]].start != notes[v[i - 1]].start) out.push_back (std::vector<int>());
        out.back().push_back (v[i]);
    }
    return out;
}

// start 0..loopTicks-1, len >= 1, pitch 0..127, vel 0..1
inline void clampNote (Note& n, const ToolCtx& c)
{
    n.start = std::max (0, std::min (n.start, c.loopTicks - 1));
    n.len = std::max (1, n.len);
    n.pitch = std::max (0, std::min (n.pitch, 127));
    n.vel = n.vel < 0.f ? 0.f : (n.vel > 1.f ? 1.f : n.vel);
}

// THE way every tool writes back: erase notes[idx], then add each of `out` (clamped) Pattern::add-style
// (an existing note on the same start + pitch is overwritten). Every written note is selected, other notes keep
// their sel. Stops adding at MAX_NOTES. Keeps sel.size() == notes.size().
inline void replaceScope (std::vector<Note>& notes, std::vector<char>& sel, const std::vector<int>& idx, const std::vector<Note>& out, const ToolCtx& c)
{
    sel.resize (notes.size(), 0);
    std::vector<char> gone (notes.size(), 0);
    for (int i : idx) if (i >= 0 && i < (int) notes.size()) gone[i] = 1;
    std::vector<Note> keep;
    std::vector<char> keepSel;
    for (size_t i = 0; i < notes.size(); ++i)
        if (! gone[i]) { keep.push_back (notes[i]); keepSel.push_back (sel[i]); }
    std::unordered_map<int, int> at;   // start * 128 + pitch -> index in keep
    for (size_t i = 0; i < keep.size(); ++i) at[keep[i].start * 128 + keep[i].pitch] = (int) i;
    for (Note n : out)
    {
        clampNote (n, c);
        const int key = n.start * 128 + n.pitch;
        const std::unordered_map<int, int>::iterator it = at.find (key);
        if (it != at.end()) { keep[it->second] = n; keepSel[it->second] = 1; continue; }
        if ((int) keep.size() >= MAX_NOTES) break;
        at[key] = (int) keep.size();
        keep.push_back (n);
        keepSel.push_back (1);
    }
    notes.swap (keep);
    sel.swap (keepSel);
}

// ---- one-click tools: they use the strip's snap (c.snap <= 1 is OFF: a no-op, except glue) ----

inline void quickQuantizeStarts (std::vector<Note>& notes, std::vector<char>& sel, const ToolCtx& c)
{
    if (c.snap <= 1) return;
    const std::vector<int> idx = scope (sel, (int) notes.size());
    std::vector<Note> out;
    for (int i : idx)
    {
        Note n = notes[i];
        n.start = gridStart (n.start, c.snap, c.loopTicks);
        out.push_back (n);
    }
    replaceScope (notes, sel, idx, out, c);
}

inline void quickQuantize (std::vector<Note>& notes, std::vector<char>& sel, const ToolCtx& c)
{
    if (c.snap <= 1) return;
    const std::vector<int> idx = scope (sel, (int) notes.size());
    std::vector<Note> out;
    for (int i : idx)
    {
        Note n = notes[i];
        const int s = gridStart (n.start, c.snap, c.loopTicks);
        const int e = gridNear (n.start + n.len, c.snap);
        n.start = s;
        n.len = std::max (c.snap, e - s);
        out.push_back (n);
    }
    replaceScope (notes, sel, idx, out, c);
}

inline void discardLengths (std::vector<Note>& notes, std::vector<char>& sel, const ToolCtx& c)
{
    if (c.snap <= 1) return;
    const std::vector<int> idx = scope (sel, (int) notes.size());
    std::vector<Note> out;
    for (int i : idx) { Note n = notes[i]; n.len = c.snap; out.push_back (n); }
    replaceScope (notes, sel, idx, out, c);
}

// Same-pitch notes where one ends exactly where the next starts become one note (the first note's velocity).
// Only the notes that merge are rewritten (and selected).
inline void glue (std::vector<Note>& notes, std::vector<char>& sel, const ToolCtx& c)
{
    std::vector<int> idx = scope (sel, (int) notes.size());
    std::sort (idx.begin(), idx.end(), [&notes] (int a, int b)
    {
        return notes[a].pitch != notes[b].pitch ? notes[a].pitch < notes[b].pitch : notes[a].start < notes[b].start;
    });
    std::vector<int> used;
    std::vector<Note> out;
    size_t i = 0;
    while (i < idx.size())
    {
        Note cur = notes[idx[i]];
        size_t j = i;
        while (j + 1 < idx.size() && notes[idx[j + 1]].pitch == cur.pitch && cur.start + cur.len == notes[idx[j + 1]].start)
        {
            cur.len += notes[idx[j + 1]].len;
            ++j;
        }
        if (j > i)
        {
            for (size_t k = i; k <= j; ++k) used.push_back (idx[k]);
            out.push_back (cur);
        }
        i = j + 1;
    }
    if (! used.empty()) replaceScope (notes, sel, used, out, c);
}

// dir -1 / +1: every start moves by one snap, wrapping inside the loop.
inline void rotate (std::vector<Note>& notes, std::vector<char>& sel, const ToolCtx& c, int dir)
{
    if (c.snap <= 1 || c.loopTicks < 1) return;
    const std::vector<int> idx = scope (sel, (int) notes.size());
    std::vector<Note> out;
    for (int i : idx)
    {
        Note n = notes[i];
        n.start = ((n.start + dir * c.snap) % c.loopTicks + c.loopTicks) % c.loopTicks;
        out.push_back (n);
    }
    replaceScope (notes, sel, idx, out, c);
}

// ---- quantize, chop, articulate, flip ----

enum { LEN_LEAVE, LEN_QUANT, END_LEAVE, END_QUANT };
struct QuantizeP { int grid = 24; float start = 1.f, sens = 1.f; int lenMode = LEN_LEAVE; float len = 1.f; };

// Start: nearest grid line, blended by strength; a start further than sens x half a step from its line is left alone.
// Length: leave it, snap it, keep the end, or snap the end (blended by the length strength).
inline void quantize (std::vector<Note>& notes, std::vector<char>& sel, const QuantizeP& p, const ToolCtx& c)
{
    if (p.grid <= 1) return;
    const std::vector<int> idx = scope (sel, (int) notes.size());
    std::vector<Note> out;
    for (int i : idx)
    {
        Note n = notes[i];
        const int os = n.start, oe = n.start + n.len;
        const int qs = gridStart (os, p.grid, c.loopTicks);
        int ns = os;
        if (std::abs (os - qs) <= (double) p.sens * p.grid * 0.5) ns = os + roundTicks ((double) p.start * (qs - os));
        if (p.lenMode == LEN_QUANT)
        {
            const int ql = std::max (p.grid, gridNear (n.len, p.grid));
            n.len = n.len + roundTicks ((double) p.len * (ql - n.len));
        }
        else if (p.lenMode == END_LEAVE) n.len = oe - ns;
        else if (p.lenMode == END_QUANT)
        {
            const int ne = oe + roundTicks ((double) p.len * (gridNear (oe, p.grid) - oe));
            n.len = ne - ns;
        }
        n.start = ns;
        out.push_back (n);
    }
    replaceScope (notes, sel, idx, out, c);
}

struct ChopP { int slice = 24; bool absolute = false; float gate = 1.f; };

// Slices each note: from its own start (the last piece keeps the remainder), or on the loop grid when absolute.
// Each piece is gate x its slice long.
inline void chop (std::vector<Note>& notes, std::vector<char>& sel, const ChopP& p, const ToolCtx& c)
{
    if (p.slice <= 1) return;
    const std::vector<int> idx = scope (sel, (int) notes.size());
    std::vector<Note> out;
    for (int i : idx)
    {
        const Note& n = notes[i];
        const int end = n.start + n.len;
        for (int a = n.start; a < end && a < c.loopTicks;)
        {
            const int b = std::min (end, p.absolute ? (a / p.slice + 1) * p.slice : a + p.slice);
            out.push_back (Note (a, std::min (b - a, roundTicks ((double) (b - a) * p.gate)), n.pitch, n.vel));
            a = b;
        }
    }
    replaceScope (notes, sel, idx, out, c);
}

inline void quickChop (std::vector<Note>& notes, std::vector<char>& sel, const ToolCtx& c)
{
    ChopP p;
    p.slice = c.snap;
    chop (notes, sel, p, c);
}

enum { ART_LEGATO, ART_PORTATO, ART_STACCATO, ART_SMALLGAP, ART_CHOPCHORDS };
struct ArticulateP { float length = 1.f, variation = 0.f; int seed = 1; bool useLengths = false, chopChords = false, smallGap = false; };

inline ArticulateP articulatePreset (int preset)
{
    ArticulateP a;
    switch (preset)
    {
    case ART_PORTATO: a.useLengths = true; a.length = 0.85f; break;
    case ART_STACCATO: a.useLengths = true; a.length = 0.35f; break;
    case ART_SMALLGAP: a.smallGap = true; break;
    case ART_CHOPCHORDS: a.useLengths = true; a.chopChords = true; break;
    default: break;
    }
    return a;
}

// Legato: a note ends where the next later start begins; the last keeps its length. Then length %, random shrink
// (variation), and optionally a cut where the next chord starts.
inline void articulate (std::vector<Note>& notes, std::vector<char>& sel, const ArticulateP& p, const ToolCtx& c)
{
    std::vector<int> idx = scope (sel, (int) notes.size());
    std::sort (idx.begin(), idx.end(), [&notes] (int a, int b)
    {
        return notes[a].start != notes[b].start ? notes[a].start < notes[b].start : notes[a].pitch < notes[b].pitch;
    });
    std::vector<int> starts;
    for (int i : idx) starts.push_back (notes[i].start);
    starts.erase (std::unique (starts.begin(), starts.end()), starts.end());
    Rng rng ((uint32_t) p.seed);
    std::vector<Note> out;
    for (int i : idx)
    {
        Note n = notes[i];
        const std::vector<int>::iterator next = std::upper_bound (starts.begin(), starts.end(), n.start);
        const bool hasNext = next != starts.end();
        int len = n.len;
        if ((! p.useLengths || p.smallGap) && hasNext) len = *next - n.start;
        if (p.smallGap && hasNext) len = std::max (1, len - 2);
        double l = len * (double) p.length;
        l *= 1.0 - (double) p.variation * rng.uni();
        n.len = std::max (1, roundTicks (l));
        if (p.chopChords && hasNext && n.start + n.len > *next) n.len = *next - n.start;
        out.push_back (n);
    }
    replaceScope (notes, sel, idx, out, c);
}

inline void quickLegato (std::vector<Note>& notes, std::vector<char>& sel, const ToolCtx& c)
{
    articulate (notes, sel, articulatePreset (ART_LEGATO), c);
}

struct FlipP { bool horizontal = true, vertical = false, preserveStarts = false; };

// Horizontal mirrors time across the scope (start' = A + B - end); preserveStarts keeps the start slots and
// reverses the notes through them instead. Vertical mirrors pitch across the scope's lowest and highest.
inline void flip (std::vector<Note>& notes, std::vector<char>& sel, const FlipP& p, const ToolCtx& c)
{
    const std::vector<int> idx = scope (sel, (int) notes.size());
    if (idx.empty()) return;
    std::vector<Note> src;
    for (int i : idx) src.push_back (notes[i]);
    std::sort (src.begin(), src.end(), [] (const Note& a, const Note& b)
    {
        return a.start != b.start ? a.start < b.start : a.pitch < b.pitch;
    });
    int A = src[0].start, B = src[0].start + src[0].len, lo = src[0].pitch, hi = src[0].pitch;
    for (const Note& n : src)
    {
        A = std::min (A, n.start);
        B = std::max (B, n.start + n.len);
        lo = std::min (lo, n.pitch);
        hi = std::max (hi, n.pitch);
    }
    const int m = (int) src.size();
    std::vector<Note> out;
    for (int k = 0; k < m; ++k)
    {
        Note n = src[k];
        if (p.horizontal)
        {
            if (p.preserveStarts) { n = src[m - 1 - k]; n.start = src[k].start; }
            else n.start = A + B - (src[k].start + src[k].len);
        }
        if (p.vertical) n.pitch = lo + hi - n.pitch;
        out.push_back (n);
    }
    replaceScope (notes, sel, idx, out, c);
}
