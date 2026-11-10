// test_roll.h: My lil Roll's pattern, text format, undo, clock and player.
#pragma once
#include "check.h"
#include "Roll.h"

inline void testRollData()
{
    // spec test 8: text round-trip; garbage loads as an empty roll; bad lines are skipped
    {
        Pattern p;
        p.bars = 7;
        p.add (Note (0, 96, 60, 0.8f));
        p.add (Note (17, 5, 127, 0.123456789f));
        p.add (Note (2687, 1, 0, 0.f));
        Pattern q;
        const bool ok = fromText (toText (p), q);
        check (ok && q.bars == 7 && q.notes == p.notes, "text round-trip keeps every note exactly");
        Pattern g;
        check (! fromText ("{garbage", g) && g.notes.empty() && g.bars == 4, "garbage loads as an empty 4-bar roll");
        Pattern h;
        fromText ("bars=2\n0 96 60 0.5\nnope\n-5 96 60 0.5\n10 0 60 0.5\n20 96 200 0.5\n30 96 61 0.5 extra\n800 96 61 0.5\n99999999999999 96 62 0.5\n", h);
        check (h.notes.size() == 1 && h.notes[0].pitch == 60, "bad note lines are skipped", (double) h.notes.size());
        Pattern cr;
        fromText ("bars=1\r\n0 96 60 0.5\r\n", cr);
        check (cr.bars == 1 && cr.notes.size() == 1, "CRLF text (Windows clipboard) still loads", (double) cr.notes.size());
    }
    // add(): same start + pitch replaces; the cap refuses
    {
        Pattern p;
        p.add (Note (0, 96, 60, 0.8f));
        p.add (Note (0, 48, 60, 0.3f));
        check (p.notes.size() == 1 && p.notes[0].len == 48, "a note on the same start and pitch replaces the old one");
        for (int i = 0; i < MAX_NOTES + 10; ++i) p.add (Note (i % 1536, 1, i / 1536, 0.5f));
        check ((int) p.notes.size() == MAX_NOTES, "the pattern refuses notes past the cap", (double) p.notes.size());
    }
    // spec test 9: 5 edits, 5 undos, 5 redos
    {
        std::vector<Note> cur;
        const std::vector<Note> start = cur;
        History hs;
        for (int k = 0; k < 5; ++k) { hs.push (cur); cur.push_back (Note (k * 24, 24, 60 + k, 0.8f)); }
        const std::vector<Note> end = cur;
        for (int k = 0; k < 5; ++k) hs.back (cur);
        const bool atStart = cur == start;
        for (int k = 0; k < 5; ++k) hs.forward (cur);
        check (atStart && cur == end, "5 edits then 5 undos restores the start; 5 redos the end");
        History deep;
        for (int k = 0; k < 70; ++k) deep.push (cur);
        check (deep.undo.size() == 64, "undo depth is 64", (double) deep.undo.size());
    }
}

inline void testRollClock()
{
    const double dt = 1.0 / 48000.0;
    // spec test 2: PPQN 4, a pulse every 1200 samples (= 16ths at 150 BPM)
    Clock c;
    c.ppq = 4;
    double mid = -1;
    for (int k = 0; k < 17; ++k)
    {
        c.external (true, dt);
        if (k == 16) break;
        for (int i = 0; i < 1199; ++i)
        {
            c.external (false, dt);
            if (k == 1 && i == 599) mid = c.pos;
        }
    }
    check (near (c.pos, 384.0, 1e-9), "PPQN 4: 16 clock intervals advance exactly 1 bar", c.pos);
    check (near (mid, 36.0, 0.1), "ticks fill in smoothly between pulses", mid);
    bool going = true;
    for (int i = 0; i < 2500; ++i) going = c.external (false, dt);
    check (! going, "a stalled clock reports stopped");

    Clock n;
    check (! n.external (false, dt), "no pulse yet since reset: stopped");
    Clock in;
    for (int i = 0; i < 48000; ++i) in.internal (120.0, dt);
    check (near (in.pos, 2.0 * PPQ * 1.0, 1e-6), "internal 120 BPM: 1 s = 2 beats", in.pos);
}
