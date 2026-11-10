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

inline void testRollPlayer()
{
    const double dt = 1.0 / 48000.0;
    const int gap = 48;   // 1 ms at 48 kHz

    // spec test 1: 120 BPM internal clock, a note on beat 2 opens its gate at 0.5 s +- 1 sample
    {
        Pattern p;
        p.add (Note (PPQ, PPQ, 60, 0.8f));
        Clock c;
        Player pl;
        pl.setPattern (&p);
        int first = -1;
        for (int i = 0; i < 48000 && first < 0; ++i)
        {
            c.internal (120.0, dt);
            pl.process (c.pos, gap);
            if (pl.gateOut (0)) first = i;
        }
        check (std::abs (first - 24000) <= 1, "120 BPM: beat-2 gate opens at 0.5 s", first);
        check (near (pl.voct (0), 0.0) && near (pl.ch[0].vel, 0.8), "C4 is 0 V, velocity passes through", pl.voct (0));
    }
    // spec test 3: RST mid-bar goes to tick 0 and replays the first note
    {
        Pattern p;
        p.add (Note (0, 24, 60, 0.8f));
        Clock c;
        Player pl;
        pl.setPattern (&p);
        for (int i = 0; i < 14400; ++i) { c.internal (120.0, dt); pl.process (c.pos, gap); }
        const bool offBefore = ! pl.gateOut (0);
        c.reset();
        pl.reset();
        c.internal (120.0, dt);
        pl.process (c.pos, gap);
        check (offBefore && c.pos < 1.0 && pl.gateOut (0), "RST jumps to tick 0 and replays the first note", c.pos);
    }
    // spec test 4: a note crossing the loop end is cut there; EOC once per loop (10 samples per tick)
    {
        Pattern p;
        p.bars = 1;
        p.add (Note (300, 200, 60, 0.8f));
        Player pl;
        pl.setPattern (&p);
        int eocs = 0;
        bool at383 = false, at384 = true;
        for (int i = 0; i < 9600; ++i)
        {
            if (pl.process (i / 10.0, gap)) ++eocs;
            if (i == 3835) at383 = pl.gateOut (0);
            if (i == 3845) at384 = pl.gateOut (0);
        }
        check (at383 && ! at384, "a note crossing the loop end is cut at the end");
        check (eocs == 2, "EOC fires once per loop (2.5 loops)", eocs);
    }
    // spec test 6: 20 staggered notes, 16 channels, the newest steal the oldest (100 samples per tick)
    {
        Pattern p;
        for (int k = 0; k < 20; ++k) p.add (Note (k, PPQ, 40 + k, 0.8f));
        Player pl;
        pl.setPattern (&p);
        for (int i = 0; i < 2000; ++i) pl.process (i / 100.0, gap);
        int on = 0;
        bool newest = true;
        for (int c = 0; c < MAX_CH; ++c)
            if (pl.gateOut (c)) { ++on; if (pl.ch[c].pitch < 44) newest = false; }
        check (on == 16 && newest && pl.chans == 16, "20 notes: 16 channels, newest steal oldest", on);
    }
    // spec test 7: back-to-back same pitch, the gate drops >= 1 ms
    {
        Pattern p;
        p.add (Note (0, PPQ, 60, 0.8f));
        p.add (Note (PPQ, PPQ, 60, 0.8f));
        Player pl;
        pl.setPattern (&p);
        int low = 0;
        bool back = false;
        for (int i = 0; i < 9900; ++i)
        {
            pl.process (i / 100.0, gap);
            if (i >= 9500 && ! pl.gateOut (0)) ++low;
            if (i == 9890) back = pl.gateOut (0);
        }
        check (low >= gap && back, "same-pitch retrigger: gate low >= 1 ms, then high", low);
    }
    // overlapping same pitch: the newer note takes the pitch over (one channel, not two)
    {
        Pattern p;
        p.add (Note (0, 2 * PPQ, 60, 0.8f));
        p.add (Note (PPQ, PPQ, 60, 0.5f));
        Player pl;
        pl.setPattern (&p);
        for (int i = 0; i < 11000; ++i) pl.process (i / 100.0, gap);
        int on = 0;
        for (int c = 0; c < MAX_CH; ++c) on += pl.gateOut (c) ? 1 : 0;
        check (on == 1 && near (pl.ch[0].vel, 0.5), "newer same-pitch note wins the pitch", on);
    }
    // a pattern swap keeps held notes held (same start + pitch), no retrigger
    {
        Pattern p;
        p.add (Note (0, BAR, 60, 0.8f));
        Pattern q = p;
        q.add (Note (BAR, PPQ, 64, 0.8f));
        Player pl;
        pl.setPattern (&p);
        for (int i = 0; i < 1000; ++i) pl.process (i / 100.0, gap);
        pl.setPattern (&q);
        bool held = true;
        for (int i = 1000; i < 1100; ++i) { pl.process (i / 100.0, gap); held = held && pl.gateOut (0); }
        check (held, "editing while playing does not retrigger held notes");
    }
}
