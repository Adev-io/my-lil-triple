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
