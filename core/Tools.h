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
// Each piece is gate x its slice long. A note with no pieces (it starts at or past the loop end) or whose pieces
// would not fit under MAX_NOTES is written back unchanged: a tool never drops a scoped note.
inline void chop (std::vector<Note>& notes, std::vector<char>& sel, const ChopP& p, const ToolCtx& c)
{
    if (p.slice <= 1) return;
    const std::vector<int> idx = scope (sel, (int) notes.size());
    const int room = MAX_NOTES - ((int) notes.size() - (int) idx.size());   // what replaceScope can hold for the scope
    int left = (int) idx.size();
    std::vector<Note> out;
    for (int i : idx)
    {
        const Note& n = notes[i];
        const int end = n.start + n.len;
        std::vector<Note> pieces;
        for (int a = n.start; a < end && a < c.loopTicks;)
        {
            const int b = std::min (end, p.absolute ? (a / p.slice + 1) * p.slice : a + p.slice);
            pieces.push_back (Note (a, std::min (b - a, roundTicks ((double) (b - a) * p.gate)), n.pitch, n.vel));
            a = b;
        }
        --left;
        if (pieces.empty() || (int) (out.size() + pieces.size()) + left > room) out.push_back (n);
        else out.insert (out.end(), pieces.begin(), pieces.end());
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

// ---- arpeggiate, strum, flam ----

enum { ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_DOWNUP, ARP_PLAYED, ARP_RANDOM };
enum { OCT_NORMAL, OCT_FLIP, OCT_ALT };
struct ArpP { int pattern = ARP_UP, step = 24, octaves = 1, order = OCT_NORMAL; float gate = 0.9f; int seed = 1; };
struct StrumP { float time = 0.25f, tension = 0.f, velFall = 0.f, velTension = 0.f; bool preserveEnd = true, alternate = false; };
struct FlamP { int time = 12; bool before = true; float vel = 0.6f; };

// Each chord (same start) becomes a run of one note per step over the chord's longest length. The pool is the chord's
// pitches ascending (ARP_PLAYED: in note order), repeated per octave; order picks the octave sequence (normal low->high,
// flip high->low, alternate 0, top, 1, top-1 ...). Velocity comes from the source note. A chord that gets no run
// is written back unchanged; runs are cut short to leave later chords room under MAX_NOTES.
inline void arpeggiate (std::vector<Note>& notes, std::vector<char>& sel, const ArpP& p, const ToolCtx& c)
{
    if (p.step < 1) return;
    const std::vector<int> idx = scope (sel, (int) notes.size());
    const int oct = std::max (1, std::min (p.octaves, 4));
    std::vector<int> octSeq;
    for (int lo = 0, hi = oct - 1; lo <= hi;)
    {
        if (p.order == OCT_FLIP) octSeq.push_back (hi--);
        else if (p.order == OCT_ALT) octSeq.push_back (octSeq.size() % 2 == 0 ? lo++ : hi--);
        else octSeq.push_back (lo++);
    }
    const int len = std::max (1, roundTicks ((double) p.step * p.gate));
    Rng rng ((uint32_t) p.seed);
    const int room = MAX_NOTES - ((int) notes.size() - (int) idx.size());   // what replaceScope can hold for the scope
    int left = (int) idx.size();
    std::vector<Note> out;
    for (std::vector<int>& ch : chords (notes, idx))
    {
        left -= (int) ch.size();
        const size_t before = out.size();
        if (p.pattern == ARP_PLAYED) std::sort (ch.begin(), ch.end());
        const int start = notes[ch[0]].start;
        int longest = 1;
        std::vector<Note> pool;   // pitch + vel only
        for (int i : ch) longest = std::max (longest, notes[i].len);
        for (int o : octSeq)
            for (int i : ch)
                if (notes[i].pitch + 12 * o <= 127) pool.push_back (Note (0, 1, notes[i].pitch + 12 * o, notes[i].vel));
        const int m = (int) pool.size();
        std::vector<int> cyc;   // pool indices of one pass
        for (int k = 0; k < m; ++k) cyc.push_back (p.pattern == ARP_DOWN || p.pattern == ARP_DOWNUP ? m - 1 - k : k);
        if (p.pattern == ARP_UPDOWN || p.pattern == ARP_DOWNUP)
            for (int k = m - 2; k >= 1; --k) cyc.push_back (p.pattern == ARP_DOWNUP ? m - 1 - k : k);
        const int end = start + longest;
        // the run stops early so the chords still to come keep room for at least their own notes
        for (int t = start, i = 0; t < end && t < c.loopTicks && (int) out.size() + left < room; t += p.step, ++i)
        {
            const int pick = p.pattern == ARP_RANDOM ? std::min (m - 1, (int) (rng.uni() * m)) : cyc[i % cyc.size()];
            out.push_back (Note (t, std::min (len, end - t), pool[pick].pitch, pool[pick].vel));
        }
        if (out.size() == before)   // no run (starts at or past the loop end, or no room): the chord stays as it was
            for (int i : ch) out.push_back (notes[i]);
    }
    replaceScope (notes, sel, idx, out, c);
}

// x^(2^(-2 t)): t = 0 even, t > 0 bunches later values together, t < 0 spreads them.
inline double tensionCurve (double x, double t) { return std::pow (x, std::pow (2.0, -2.0 * t)); }

// Chord notes start staggered over `time` beats: note k of n at start + |time| x f(k/(n-1)), low->high for time >= 0,
// high->low below 0 (alternate flips it on every next chord of 2+ notes). A lone note is left alone.
// Velocity fall: later notes lose up to velFall of their velocity.
inline void strum (std::vector<Note>& notes, std::vector<char>& sel, const StrumP& p, const ToolCtx& c)
{
    const std::vector<int> idx = scope (sel, (int) notes.size());
    std::vector<Note> out;
    bool flipped = false;
    for (std::vector<int>& ch : chords (notes, idx))
    {
        const int n = (int) ch.size();
        if (n < 2) { out.push_back (notes[ch[0]]); continue; }
        if ((p.time < 0.f) != flipped) std::reverse (ch.begin(), ch.end());
        if (p.alternate) flipped = ! flipped;
        for (int k = 0; k < n; ++k)
        {
            Note x = notes[ch[k]];
            const double f = (double) k / (n - 1);
            const int end = x.start + x.len;
            x.start += roundTicks (std::abs ((double) p.time) * PPQ * tensionCurve (f, p.tension));
            if (p.preserveEnd) x.len = std::max (1, end - x.start);
            x.vel = (float) (x.vel * (1.0 - (double) p.velFall * tensionCurve (f, p.velTension)));
            out.push_back (x);
        }
    }
    replaceScope (notes, sel, idx, out, c);
}

// A grace note of the same pitch, `time` ticks long, at vel x the note's velocity. Before: it ends at the note start
// (a note closer than `time` to 0 gets a shorter one; a note at 0 gets none). After: the note is delayed by `time`
// (length shrinks, never below 1) and the grace note takes the original start.
inline void flam (std::vector<Note>& notes, std::vector<char>& sel, const FlamP& p, const ToolCtx& c)
{
    const std::vector<int> idx = scope (sel, (int) notes.size());
    const int t = std::max (1, p.time);
    std::vector<Note> graces, mains;   // mains go first so MAX_NOTES drops grace notes, never the real ones
    for (int i : idx)
    {
        Note n = notes[i];
        const float gv = n.vel * p.vel;
        if (p.before)
        {
            const int gs = std::max (0, n.start - t);
            if (n.start > 0) graces.push_back (Note (gs, n.start - gs, n.pitch, gv));
        }
        else
        {
            graces.push_back (Note (n.start, t, n.pitch, gv));
            n.start += t;
            n.len = std::max (1, n.len - t);
        }
        mains.push_back (n);
    }
    std::unordered_map<int, char> taken;   // a grace note never replaces a real one on the same start + pitch
    for (const Note& n : mains) taken[n.start * 128 + n.pitch] = 1;
    for (const Note& g : graces) if (! taken.count (g.start * 128 + g.pitch)) mains.push_back (g);
    replaceScope (notes, sel, idx, mains, c);
}

// ---- limit, randomize, scale levels, velocity LFO ----

enum { FIX_OFF, FIX_ABOVE, FIX_BELOW, FIX_ALT };
struct LimitP { int low = 48, high = 72; bool wrap = false; int scaleFix = FIX_OFF; };
struct RandomizeP { bool levels = true; float velAmount = 0.2f; bool bipolar = true; bool pattern = false; float population = 0.5f;
                    int stack = 1, octave = 4, range = 1; float lenVar = 0.f; bool merge = true; int seed = 1; };
struct ScaleLevelsP { float center = 0.f, multiply = 1.f, tension = 0.f, offset = 0.f; };   // center/tension -100..100, offset -1..1
enum { LFO_SINE, LFO_TRI, LFO_SQUARE };
struct LfoP { int shape = LFO_SINE; float cycles = 1.f, phase = 0.f, depth = 0.5f, center = 0.5f; };   // phase in degrees

// Moves p by whole octaves into [lo, hi]. Returns false when the range holds no pitch of that pitch class.
inline bool octaveInto (int p, int lo, int hi, bool wrap, int& out)
{
    if (p >= lo && p <= hi) { out = p; return true; }
    if (p < lo) out = wrap ? hi - ((hi - p) % 12) : p + 12 * ((lo - p + 11) / 12);
    else out = wrap ? lo + ((p - lo) % 12) : p - 12 * ((p - hi + 11) / 12);
    return out >= lo && out <= hi;
}

// Pitches outside low..high move by octaves into it (wrap: to the opposite end's octave). A pitch class the range
// can't hold (range under 12 semitones) is clamped to the nearer edge. Scale fix then moves off-scale notes one
// semitone at a time (alternate: above, below, above ... in start/pitch order); if that leaves the range, the other
// direction is tried, then the note stays.
inline void limit (std::vector<Note>& notes, std::vector<char>& sel, const LimitP& p, const ToolCtx& c)
{
    std::vector<int> idx = scope (sel, (int) notes.size());
    std::sort (idx.begin(), idx.end(), [&notes] (int a, int b)
    {
        return notes[a].start != notes[b].start ? notes[a].start < notes[b].start : notes[a].pitch < notes[b].pitch;
    });
    const int lo = std::max (0, std::min (std::min (p.low, p.high), 127)), hi = std::max (0, std::min (std::max (p.low, p.high), 127));
    int altCount = 0;
    std::vector<Note> out;
    for (int i : idx)
    {
        Note n = notes[i];
        int q;
        if (octaveInto (n.pitch, lo, hi, p.wrap, q)) n.pitch = q;
        else n.pitch = n.pitch < lo ? lo : hi;
        if (p.scaleFix != FIX_OFF && ! inScale (n.pitch, c.key, c.mode))
        {
            const int first = p.scaleFix == FIX_ABOVE ? 1 : p.scaleFix == FIX_BELOW ? -1 : (altCount++ % 2 == 0 ? 1 : -1);
            for (int d : { first, -first })
            {
                int x = n.pitch + d;
                while (x >= lo && x <= hi && ! inScale (x, c.key, c.mode)) x += d;
                if (x >= lo && x <= hi) { n.pitch = x; break; }
            }
        }
        out.push_back (n);
    }
    replaceScope (notes, sel, idx, out, c);
}

// Levels: every scope note's velocity moves by up to velAmount (bipolar) or down by up to it.
// Pattern: fills the selection's time span (the whole loop when nothing is selected) with random in-scale notes: one
// roll per snap step for population, then a pitch and a length roll per stacked note. New notes never overwrite
// existing ones. Needs snap > 1.
inline void randomize (std::vector<Note>& notes, std::vector<char>& sel, const RandomizeP& p, const ToolCtx& c)
{
    const std::vector<int> idx = scope (sel, (int) notes.size());
    Rng rng ((uint32_t) p.seed);
    std::vector<Note> out;
    if (p.levels)
        for (int i : idx)
        {
            Note n = notes[i];
            const double u = rng.uni();
            n.vel = (float) (n.vel + p.velAmount * (p.bipolar ? 2.0 * u - 1.0 : -u));
            out.push_back (n);
        }
    if (p.pattern && c.snap > 1)
    {
        bool any = false;
        int A = c.loopTicks, B = 0;
        for (size_t i = 0; i < notes.size() && i < sel.size(); ++i)
            if (sel[i]) { any = true; A = std::min (A, notes[i].start); B = std::max (B, notes[i].start + notes[i].len); }
        if (! any) { A = 0; B = c.loopTicks; }
        B = std::min (B, c.loopTicks);
        std::vector<int> pool;
        const int base = (p.octave + 1) * 12;
        for (int q = base; q < base + 12 * std::max (1, std::min (p.range, 3)) && q <= 127; ++q)
            if (q >= 0 && inScale (q, c.key, c.mode)) pool.push_back (q);
        std::unordered_map<int, char> taken;   // start * 128 + pitch of every existing note
        for (const Note& n : notes) taken[n.start * 128 + n.pitch] = 1;
        Rng prng ((uint32_t) p.seed * 2654435761u + 1u);   // its own stream: toggling Levels leaves the pattern alone
        std::vector<Note> gen;
        for (int t = A; t < B && ! pool.empty(); t += c.snap)
        {
            if (prng.uni() >= p.population) continue;
            for (int s = 0; s < std::max (1, std::min (p.stack, 4)); ++s)
            {
                const int pitch = pool[std::min ((int) pool.size() - 1, (int) (prng.uni() * pool.size()))];
                const double u = prng.uni();
                int len = c.snap;
                if (p.lenVar > 0.f) len = c.snap * (1 + (int) std::floor (u * 4.0 * p.lenVar));
                else if (p.lenVar < 0.f) len = std::max (1, roundTicks (c.snap * (1.0 - u * -p.lenVar * 0.75)));
                if (taken.count (t * 128 + pitch)) continue;
                taken[t * 128 + pitch] = 1;
                gen.push_back (Note (t, len, pitch, 0.8f));
            }
        }
        if (p.merge)
        {
            std::sort (gen.begin(), gen.end(), [] (const Note& a, const Note& b)
            {
                return a.pitch != b.pitch ? a.pitch < b.pitch : a.start < b.start;
            });
            std::vector<Note> merged;
            for (const Note& n : gen)
            {
                if (! merged.empty() && merged.back().pitch == n.pitch && merged.back().start + merged.back().len == n.start) merged.back().len += n.len;
                else merged.push_back (n);
            }
            gen.swap (merged);
        }
        out.insert (out.end(), gen.begin(), gen.end());
    }
    replaceScope (notes, sel, p.levels ? idx : std::vector<int>(), out, c);
}

// v' = pivot + (v - pivot) x multiply, then v^(2^(-tension/50)), then + offset, clamped 0..1. pivot = 0.5 + center/200.
inline void scaleLevels (std::vector<Note>& notes, std::vector<char>& sel, const ScaleLevelsP& p, const ToolCtx& c)
{
    const std::vector<int> idx = scope (sel, (int) notes.size());
    const double pivot = 0.5 + p.center / 200.0;
    std::vector<Note> out;
    for (int i : idx)
    {
        Note n = notes[i];
        double v = pivot + (n.vel - pivot) * p.multiply;
        v = std::max (0.0, std::min (1.0, v));
        if (p.tension != 0.f) v = std::pow (v, std::pow (2.0, -p.tension / 50.0));
        n.vel = (float) (v + p.offset);
        out.push_back (n);
    }
    replaceScope (notes, sel, idx, out, c);
}

// vel = center + depth/2 x wave, the wave read at each note's start across the scope's span (cycles per span, phase in
// degrees). A span of zero length changes nothing.
inline void velocityLfo (std::vector<Note>& notes, std::vector<char>& sel, const LfoP& p, const ToolCtx& c)
{
    const std::vector<int> idx = scope (sel, (int) notes.size());
    if (idx.empty()) return;
    int A = notes[idx[0]].start, B = A;
    for (int i : idx) { A = std::min (A, notes[i].start); B = std::max (B, notes[i].start + notes[i].len); }
    if (B <= A) return;
    const double twoPi = 6.283185307179586;
    std::vector<Note> out;
    for (int i : idx)
    {
        Note n = notes[i];
        const double phi = p.cycles * (double) (n.start - A) / (B - A) + p.phase / 360.0;
        double w;
        if (p.shape == LFO_SQUARE) w = phi - std::floor (phi) < 0.5 ? 1.0 : -1.0;
        else if (p.shape == LFO_TRI) w = 4.0 / twoPi * std::asin (std::sin (twoPi * phi));
        else w = std::sin (twoPi * phi);
        n.vel = (float) (p.center + p.depth / 2.0 * w);
        out.push_back (n);
    }
    replaceScope (notes, sel, idx, out, c);
}
