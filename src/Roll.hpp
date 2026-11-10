// Roll.hpp: the My lil Roll module: a clocked piano roll with poly V/OCT, gate and velocity out; docks onto My lil Triple.
// Author: Ari Klopfer / A_Development · MIT
#pragma once
#include "plugin.hpp"
#include "Chords.h"
#include "Dock.hpp"
#include "Roll.h"
#include <atomic>

static const int PATS = 16;   // the pattern bank

// What the editor works on and the patch saves. UI thread only.
struct RollDoc
{
    Pattern pats[PATS];
    int key = 0, mode = 0, snap = SNAP_DEFAULT, chord = 0;   // KEY_NAMES, MODE_NAMES, SNAPS, CHORDS indices
    bool scaleSnap = true;
};

struct Roll : Module
{
    enum ParamId { BPM_PARAM, RST_PARAM, RUN_PARAM, PAT_PARAM, ARP_PARAM, PARAMS_LEN };
    enum InputId { CLK_INPUT, RST_INPUT, RUN_INPUT, PAT_INPUT, INPUTS_LEN };
    enum OutputId { VOCT_OUTPUT, GATE_OUTPUT, VEL_OUTPUT, EOC_OUTPUT, OUTPUTS_LEN };
    enum LightId { RUN_LIGHT, LIGHTS_LEN };

    RollDoc doc;                                   // UI thread
    int docGen = 0;                                // bumped when doc is replaced wholesale (UI thread)
    Pattern* pat[PATS] = {};                       // the audio thread owns these
    std::atomic<Pattern*> incoming[PATS];          // the UI thread hands new ones over here
    std::atomic<bool> handover { false };          // some incoming slot may be full
    std::atomic<int> ppq { 4 };                    // clock pulses per quarter note
    std::atomic<int> previewReq { -1 };            // pitch clicked on the keys column
    std::atomic<float> playTick { 0.f };           // playhead, ticks into the playing pattern's loop
    std::atomic<int> selected { 0 };               // pattern the knob + CV point at (0-based); the editor shows it
    std::atomic<int> playing { 0 };                // pattern being played (0-based)
    std::atomic<int> snapTicks { 24 };             // live arp step, from the strip's SNAP (published by the UI)
    Clock clock;
    Player player;
    LiveArp arp;
    int play = 0;                                  // audio thread's copy of `playing`
    dsp::SchmittTrigger clkTrig, rstTrig, runTrig;
    dsp::BooleanTrigger rstBtn;
    dsp::PulseGenerator eoc;
    bool stopped = true;
    int previewPitch = 60, previewLeft = 0;

    Roll()
    {
        config (PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam (BPM_PARAM, 40.f, 240.f, 120.f, "Tempo when CLK is not patched", " BPM")->snapEnabled = true;
        configButton (RST_PARAM, "Reset to bar 1");
        configSwitch (RUN_PARAM, 0.f, 1.f, 1.f, "Run", { "Stopped", "Running" });
        configParam (PAT_PARAM, 1.f, 16.f, 1.f, "Pattern")->snapEnabled = true;
        configSwitch (ARP_PARAM, 0.f, 6.f, 0.f, "Live arp", { "Off", "Up", "Down", "Up-down", "Down-up", "As played", "Random" });
        configInput (CLK_INPUT, "Clock (pulses per quarter in the menu)");
        configInput (RST_INPUT, "Reset");
        configInput (RUN_INPUT, "Run toggle");
        configInput (PAT_INPUT, "Pattern select (0.625 V per pattern)");
        configOutput (VOCT_OUTPUT, "V/OCT (poly)");
        configOutput (GATE_OUTPUT, "Gate (poly)");
        configOutput (VEL_OUTPUT, "Velocity 0..10 V (poly)");
        configOutput (EOC_OUTPUT, "End of loop");
        for (std::atomic<Pattern*>& a : incoming) a.store (nullptr);
        commitAll();
    }

    ~Roll()
    {
        for (int i = 0; i < PATS; ++i)
        {
            delete pat[i];
            delete incoming[i].load();
        }
    }

    // UI thread: hand the audio thread a copy of pattern i. A copy it has not taken yet is dropped.
    void commit (int i)
    {
        delete incoming[i].exchange (new Pattern (doc.pats[i]));
        handover = true;
    }
    void commitAll() { for (int i = 0; i < PATS; ++i) commit (i); publishSnap(); }
    // UI thread: the live arp steps on the strip's SNAP; OFF steps on 1/16.
    void publishSnap() { const int s = SNAPS[doc.snap].ticks; snapTicks = s > 1 ? s : 24; }

    void onReset() override
    {
        doc = RollDoc();
        ppq = 4;
        ++docGen;
        commitAll();
    }

    void switchPattern (int i, long atTick)
    {
        play = i;
        player.switchTo (pat[i], atTick);
    }

    void process (const ProcessArgs& a) override
    {
        if (handover.load())
        {
            handover = false;   // cleared before the slots are read, so a hand-over that lands meanwhile is seen next sample
            for (int i = 0; i < PATS; ++i)
                if (Pattern* p = incoming[i].exchange (nullptr))
                {
                    delete pat[i];   // ponytail: frees on the audio thread, like Triple's wave table; edits are mouse-paced
                    pat[i] = p;
                    if (i == play) player.setPattern (p);
                }
        }
        const int want = clamp ((int) params[PAT_PARAM].getValue() - 1 + (int) std::floor (inputs[PAT_INPUT].getVoltage() / 0.625f), 0, PATS - 1);
        selected.store (want);

        const bool rstIn = rstTrig.process (inputs[RST_INPUT].getVoltage(), 0.1f, 1.f);
        const bool rstPress = rstBtn.process (params[RST_PARAM].getValue() > 0.f);
        if (rstIn || rstPress) { clock.reset(); player.reset(); arp.reset(); if (want != play) switchPattern (want, 0); }
        if (runTrig.process (inputs[RUN_INPUT].getVoltage(), 0.1f, 1.f))
            params[RUN_PARAM].setValue (params[RUN_PARAM].getValue() > 0.5f ? 0.f : 1.f);
        const bool run = params[RUN_PARAM].getValue() > 0.5f;
        lights[RUN_LIGHT].setBrightness (run ? 1.f : 0.f);

        bool going = false;
        if (run)
        {
            clock.ppq = ppq.load();
            if (inputs[CLK_INPUT].isConnected())
                going = clock.external (clkTrig.process (inputs[CLK_INPUT].getVoltage(), 0.1f, 1.f), a.sampleTime);
            else
            {
                clock.internal (params[BPM_PARAM].getValue(), a.sampleTime);
                going = true;
            }
        }
        if (going)
        {
            if (stopped) { player.resync = true; stopped = false; }
            // a pending switch lands on the tick where the playing pattern's loop wraps; the new one starts at its tick 0
            const long t = (long) std::floor (clock.pos);
            if (want != play && t != player.last)
            {
                const int L = pat[play] ? pat[play]->ticks() : 4 * BAR;
                const long rel = t - player.origin, prel = player.last - player.origin;
                if (player.last < 0 || (prel >= 0 && rel / L != prel / L)) switchPattern (want, player.origin + L * (rel / L));
            }
            if (player.process (clock.pos, (int) (0.001f * a.sampleRate))) eoc.trigger (1e-3f);
        }
        else
        {
            if (! stopped)
            {
                player.releaseAll();
                stopped = true;
            }
            if (want != play) switchPattern (want, (long) std::floor (clock.pos));   // stopped: at once
        }
        playing.store (play);
        const double L = (double) (pat[play] ? pat[play]->ticks() : 4 * BAR);
        double pt = std::fmod (clock.pos - (double) player.origin, L);
        if (pt < 0.0) pt += L;
        playTick.store ((float) pt);

        arp.mode = (int) params[ARP_PARAM].getValue();
        arp.process (player, clock.pos, snapTicks.load());
        const bool arping = arp.mode != LIVE_OFF;

        const int pv = previewReq.exchange (-1);
        if (pv >= 0) { previewPitch = pv; previewLeft = (int) (0.2f * a.sampleRate); }
        const int n = arping ? 1 : player.chans;
        const bool preview = previewLeft > 0 && n < MAX_CH;   // one channel above the used ones
        if (previewLeft > 0) --previewLeft;
        const int out = n + (preview ? 1 : 0);

        outputs[VOCT_OUTPUT].setChannels (out);
        outputs[GATE_OUTPUT].setChannels (out);
        outputs[VEL_OUTPUT].setChannels (out);
        DockMsg* d = (rightExpander.module && rightExpander.module->model == modelTriple)
            ? (DockMsg*) rightExpander.module->leftExpander.producerMessage : nullptr;
        for (int c = 0; c < out; ++c)
        {
            const bool pc = preview && c == n;
            float v, g, vel;
            if (pc) { v = (previewPitch - 60) / 12.f; g = 10.f; vel = 0.8f; }
            else if (arping) { v = arp.voct; g = arp.gate ? 10.f : 0.f; vel = arp.vel; }
            else { v = player.voct (c); g = player.gateOut (c) ? 10.f : 0.f; vel = player.ch[c].vel; }
            outputs[VOCT_OUTPUT].setVoltage (v, c);
            outputs[GATE_OUTPUT].setVoltage (g, c);
            outputs[VEL_OUTPUT].setVoltage (10.f * vel, c);
            if (d) { d->voct[c] = v; d->gate[c] = g; d->vel[c] = vel; }
        }
        if (d)
        {
            d->channels = out;
            rightExpander.module->leftExpander.messageFlipRequested = true;
        }
        outputs[EOC_OUTPUT].setVoltage (eoc.process (a.sampleTime) ? 10.f : 0.f);
    }

    json_t* dataToJson() override
    {
        json_t* j = json_object();
        json_t* pa = json_array();
        for (int i = 0; i < PATS; ++i) json_array_append_new (pa, json_string (toText (doc.pats[i]).c_str()));
        json_object_set_new (j, "patterns", pa);
        json_object_set_new (j, "key", json_integer (doc.key));
        json_object_set_new (j, "mode", json_integer (doc.mode));
        json_object_set_new (j, "snap", json_integer (doc.snap));
        json_object_set_new (j, "chord", json_integer (doc.chord));
        json_object_set_new (j, "scaleSnap", json_boolean (doc.scaleSnap));
        json_object_set_new (j, "ppq", json_integer (ppq.load()));
        return j;
    }

    // Missing or out-of-range fields load as defaults; bad note lines are skipped (fromText); a missing or bad pattern
    // is an empty 4-bar one. A 2.1 patch's "notes" lands in pattern 1.
    void dataFromJson (json_t* j) override
    {
        auto num = [&] (const char* k, int lo, int hi, int def) {
            json_t* v = json_object_get (j, k);
            const int x = json_is_integer (v) ? (int) json_integer_value (v) : def;
            return x < lo || x > hi ? def : x;
        };
        doc = RollDoc();
        json_t* pa = json_object_get (j, "patterns");
        if (json_is_array (pa))
        {
            for (int i = 0; i < PATS && i < (int) json_array_size (pa); ++i)
            {
                json_t* s = json_array_get (pa, i);
                if (json_is_string (s)) fromText (json_string_value (s), doc.pats[i]);
            }
        }
        else
        {
            json_t* s = json_object_get (j, "notes");
            if (json_is_string (s)) fromText (json_string_value (s), doc.pats[0]);
        }
        doc.key = num ("key", 0, 11, 0);
        doc.mode = num ("mode", 0, 6, 0);
        doc.snap = num ("snap", 0, SNAP_N - 1, SNAP_DEFAULT);
        doc.chord = num ("chord", 0, CHORD_N - 1, 0);
        json_t* b = json_object_get (j, "scaleSnap");
        doc.scaleSnap = b ? json_is_true (b) : true;
        const int q = num ("ppq", 1, 48, 4);
        ppq = (q == 1 || q == 4 || q == 24 || q == 48) ? q : 4;
        ++docGen;
        commitAll();
    }
};
