// Tools.h: My lil Roll's FL-style edit tools. Every tool edits (notes, sel) in place through replaceScope. No Rack dependency.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
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

// The nearest multiple of g (ties go up). v >= 0.
inline int gridNear (int v, int g) { return (v + g / 2) / g * g; }

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
    for (Note n : out)
    {
        clampNote (n, c);
        bool done = false;
        for (size_t i = 0; i < keep.size() && ! done; ++i)
            if (keep[i].start == n.start && keep[i].pitch == n.pitch) { keep[i] = n; keepSel[i] = 1; done = true; }
        if (done) continue;
        if ((int) keep.size() >= MAX_NOTES) break;
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

inline void quickChop (std::vector<Note>& notes, std::vector<char>& sel, const ToolCtx& c)
{
    if (c.snap <= 1) return;
    const std::vector<int> idx = scope (sel, (int) notes.size());
    std::vector<Note> out;
    for (int i : idx)
    {
        const Note& n = notes[i];
        const int end = n.start + n.len;
        for (int a = n.start; a < end && a < c.loopTicks; a += c.snap)
            out.push_back (Note (a, std::min (c.snap, end - a), n.pitch, n.vel));   // the last piece keeps the remainder
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
