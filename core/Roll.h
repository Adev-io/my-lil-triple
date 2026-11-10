// Roll.h: My lil Roll's pattern, text format, undo history, clock and player. No Rack dependency.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

static const int PPQ = 96;            // ticks per quarter note
static const int BAR = 4 * PPQ;       // 4/4 only
static const int MAX_NOTES = 4096;
static const int MAX_CH = 16;

struct Note
{
    int start, len, pitch;   // ticks, ticks (>= 1), MIDI note (60 = C4 = 0 V)
    float vel;               // 0..1
    Note() : start (0), len (PPQ), pitch (60), vel (0.8f) {}
    Note (int s, int l, int p, float v) : start (s), len (l), pitch (p), vel (v) {}
    bool operator== (const Note& o) const { return start == o.start && len == o.len && pitch == o.pitch && vel == o.vel; }
    bool operator!= (const Note& o) const { return ! (*this == o); }
};

struct Pattern
{
    std::vector<Note> notes;
    int bars = 4;   // loop length, 1..16
    int ticks() const { return bars * BAR; }
    // A note on the same start + pitch replaces the old one. Refuses new notes past MAX_NOTES.
    bool add (const Note& n)
    {
        for (Note& o : notes)
            if (o.start == n.start && o.pitch == n.pitch) { o = n; return true; }
        if ((int) notes.size() >= MAX_NOTES) return false;
        notes.push_back (n);
        return true;
    }
};

struct Snap { const char* name; int ticks; };
static const Snap SNAPS[] = {
    { "OFF", 1 }, { "1/4", 96 }, { "1/8", 48 }, { "1/16", 24 }, { "1/32", 12 },
    { "1/4T", 64 }, { "1/8T", 32 }, { "1/16T", 16 }, { "1/32T", 8 },
};
static const int SNAP_N = 9, SNAP_DEFAULT = 3;

// "bars=N" then one "start len pitch vel" line per note. Saved in the patch and used by "Copy notes as text".
inline std::string toText (const Pattern& p)
{
    std::ostringstream s;
    s.imbue (std::locale::classic());
    s.precision (9);   // floats round-trip exactly
    s << "bars=" << p.bars << "\n";
    for (const Note& n : p.notes) s << n.start << ' ' << n.len << ' ' << n.pitch << ' ' << n.vel << "\n";
    return s.str();
}

// A bad header gives false and an empty 4-bar pattern; bad note lines are skipped.
inline bool fromText (const std::string& t, Pattern& p)
{
    p = Pattern();
    std::istringstream s (t);
    s.imbue (std::locale::classic());
    std::string line;
    if (! std::getline (s, line) || line.compare (0, 5, "bars=") != 0) return false;
    int bars = 0;
    {
        std::istringstream hs (line.substr (5));
        hs.imbue (std::locale::classic());
        std::string extra;
        if (! (hs >> bars) || (hs >> extra) || bars < 1 || bars > 16) return false;
    }
    p.bars = bars;
    while (std::getline (s, line))
    {
        Note n;
        std::istringstream ls (line);
        ls.imbue (std::locale::classic());
        std::string extra;
        if (! (ls >> n.start >> n.len >> n.pitch >> n.vel) || (ls >> extra)) continue;   // overflow sets failbit, so huge numbers are skipped
        if (n.start < 0 || n.start >= p.ticks() || n.len < 1 || n.pitch < 0 || n.pitch > 127 || ! (n.vel >= 0.f && n.vel <= 1.f)) continue;
        p.add (n);
    }
    return true;
}

// Roll-local undo: one snapshot of the note list per edit, 64 deep.
struct History
{
    std::vector<std::vector<Note>> undo, redo;
    void push (const std::vector<Note>& before)
    {
        undo.push_back (before);
        if (undo.size() > 64) undo.erase (undo.begin());
        redo.clear();
    }
    bool back (std::vector<Note>& cur)
    {
        if (undo.empty()) return false;
        redo.push_back (cur);
        cur = undo.back();
        undo.pop_back();
        return true;
    }
    bool forward (std::vector<Note>& cur)
    {
        if (redo.empty()) return false;
        undo.push_back (cur);
        cur = redo.back();
        redo.pop_back();
        return true;
    }
};

// Tick position from the internal tempo, or from clock pulses with smooth ticks in between.
struct Clock
{
    double pos = 0.0;          // ticks since reset
    int ppq = 4;               // external pulses per quarter note: 1, 4, 24 or 48
    bool started = false;      // a pulse has arrived since reset
    double base = 0.0;         // tick of the last pulse
    double since = 0.0, interval = 0.0;   // seconds since the last pulse, and between the last two

    void reset() { pos = 0.0; started = false; base = 0.0; since = 0.0; interval = 0.0; }
    void internal (double bpm, double dt) { pos += bpm / 60.0 * PPQ * dt; }
    // Returns false while stopped: no pulse since reset, or none for 2x the last interval.
    bool external (bool pulse, double dt)
    {
        const double step = (double) PPQ / ppq;
        since += dt;
        if (pulse)
        {
            if (started) { base += step; interval = since; }
            started = true;
            since = 0.0;
        }
        if (! started || (interval > 0.0 && since > 2.0 * interval)) return false;
        pos = base + (interval > 0.0 ? std::min (since / interval, 0.999) : 0.0) * step;   // never runs past the next pulse
        return true;
    }
};

struct Chan
{
    int key = -1;        // the note it plays (Player::key), -1 = free
    int pitch = 60;
    float vel = 0.f;
    int gap = 0;         // samples of forced-low gate left (retrigger)
    bool gate = false;
    long age = 0;        // assignment order, for stealing the oldest
};

// Turns the tick position into up to 16 channels of pitch, gate and velocity.
// Notes are only re-evaluated when the integer tick changes, so the per-sample cost is tiny.
struct Player
{
    const Pattern* pat = nullptr;
    Chan ch[MAX_CH];
    int chans = 1;          // output channel count: the highest channel used, until reset
    long last = -1;         // the tick evaluated last (absolute, since reset)
    bool resync = true;     // next evaluation starts every note that covers the playhead
    long ages = 0;

    static int key (const Note& n) { return n.start * 128 + n.pitch; }
    void setPattern (const Pattern* p) { pat = p; resync = true; }
    void releaseAll() { for (Chan& c : ch) { c.key = -1; c.gate = false; c.gap = 0; } }
    void reset() { releaseAll(); last = -1; resync = true; chans = 1; }
    bool gateOut (int i) const { return ch[i].gate && ch[i].gap == 0; }
    float voct (int i) const { return (ch[i].pitch - 60) / 12.f; }

    // pos = clock ticks since reset. Returns true on the tick the loop wraps (EOC).
    bool process (double pos, int gapSamples)
    {
        for (Chan& c : ch) if (c.gap > 0) --c.gap;
        const long t = (long) std::floor (pos);
        return (t != last || resync) ? evaluate (t, gapSamples) : false;
    }

    bool evaluate (long t, int gapSamples)
    {
        const int L = pat ? pat->ticks() : 4 * BAR;
        const long prev = last;
        last = t;
        const bool eoc = prev >= 0 && t / L != prev / L;
        const int lt = (int) (t % L), plt = prev >= 0 ? (int) (prev % L) : -1;
        const bool all = resync || prev < 0, sameLoop = prev >= 0 && t / L == prev / L;
        resync = false;

        // notes covering the playhead (cut at the loop end); per pitch only the newest start
        int best[128];
        std::fill (best, best + 128, -1);
        if (pat)
            for (int i = 0; i < (int) pat->notes.size(); ++i)
            {
                const Note& n = pat->notes[i];
                if (n.start <= lt && lt < std::min (n.start + n.len, L) && (best[n.pitch] < 0 || pat->notes[best[n.pitch]].start < n.start))
                    best[n.pitch] = i;
            }

        bool was[MAX_CH];
        for (int c = 0; c < MAX_CH; ++c)
        {
            was[c] = ch[c].gate;
            if (ch[c].key < 0) continue;
            const int b = best[ch[c].key % 128];
            if (b < 0 || key (pat->notes[b]) != ch[c].key) { ch[c].key = -1; ch[c].gate = false; }
        }

        for (int p = 0; p < 128; ++p)
        {
            const int b = best[p];
            if (b < 0) continue;
            const Note& n = pat->notes[b];
            const int k = key (n);
            bool held = false;
            for (const Chan& c : ch) held = held || c.key == k;
            if (held) continue;
            const bool fresh = all || (sameLoop ? (n.start > plt && n.start <= lt) : (n.start > plt || n.start <= lt));
            if (! fresh) continue;   // started earlier and lost its channel to a steal: stays silent
            int c = 0;
            while (c < MAX_CH && ch[c].key >= 0) ++c;
            if (c == MAX_CH)
            {
                c = 0;
                for (int i = 1; i < MAX_CH; ++i) if (ch[i].age < ch[c].age) c = i;
            }
            ch[c].key = k;
            ch[c].pitch = n.pitch;
            ch[c].vel = n.vel;
            ch[c].age = ++ages;
            ch[c].gap = was[c] ? gapSamples : 0;   // the channel was sounding: drop the gate so envelopes re-attack
            ch[c].gate = true;
            chans = std::max (chans, c + 1);
        }
        return eoc;
    }
};
