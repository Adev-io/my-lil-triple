// Roll.hpp: the My lil Roll module: a clocked piano roll with poly V/OCT, gate and velocity out; docks onto My lil Triple.
// Author: Ari Klopfer / A_Development · MIT
#pragma once
#include "plugin.hpp"
#include "Chords.h"
#include "Dock.hpp"
#include "Roll.h"
#include <atomic>

// What the editor works on and the patch saves. UI thread only.
struct RollDoc
{
    Pattern pat;
    int key = 0, mode = 0, snap = SNAP_DEFAULT, chord = 0;   // KEY_NAMES, MODE_NAMES, SNAPS, CHORDS indices
    bool scaleSnap = true;
};

struct Roll : Module
{
    enum ParamId { BPM_PARAM, RST_PARAM, RUN_PARAM, PARAMS_LEN };
    enum InputId { CLK_INPUT, RST_INPUT, RUN_INPUT, INPUTS_LEN };
    enum OutputId { VOCT_OUTPUT, GATE_OUTPUT, VEL_OUTPUT, EOC_OUTPUT, OUTPUTS_LEN };
    enum LightId { RUN_LIGHT, LIGHTS_LEN };

    RollDoc doc;                                   // UI thread
    Pattern* pat = nullptr;                        // the audio thread owns this one
    std::atomic<Pattern*> incoming { nullptr };    // the UI thread hands a new one over here
    std::atomic<int> ppq { 4 };                    // clock pulses per quarter note
    std::atomic<int> previewReq { -1 };            // pitch clicked on the keys column
    std::atomic<float> playTick { 0.f };           // playhead, ticks into the loop
    Clock clock;
    Player player;
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
        configInput (CLK_INPUT, "Clock (pulses per quarter in the menu)");
        configInput (RST_INPUT, "Reset");
        configInput (RUN_INPUT, "Run toggle");
        configOutput (VOCT_OUTPUT, "V/OCT (poly)");
        configOutput (GATE_OUTPUT, "Gate (poly)");
        configOutput (VEL_OUTPUT, "Velocity 0..10 V (poly)");
        configOutput (EOC_OUTPUT, "End of loop");
        commit();
    }

    ~Roll()
    {
        delete pat;
        delete incoming.load();
    }

    // UI thread: hand the audio thread a copy of the doc's pattern. A copy it has not taken yet is dropped.
    void commit() { delete incoming.exchange (new Pattern (doc.pat)); }

    void onReset() override
    {
        doc = RollDoc();
        ppq = 4;
        commit();
    }

    void process (const ProcessArgs& a) override
    {
        if (Pattern* p = incoming.exchange (nullptr))
        {
            delete pat;   // ponytail: frees on the audio thread, like Triple's wave table; edits are mouse-paced
            pat = p;
            player.setPattern (p);
        }
        const bool rstIn = rstTrig.process (inputs[RST_INPUT].getVoltage(), 0.1f, 1.f);
        const bool rstPress = rstBtn.process (params[RST_PARAM].getValue() > 0.f);
        if (rstIn || rstPress) { clock.reset(); player.reset(); }
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
            if (player.process (clock.pos, (int) (0.001f * a.sampleRate))) eoc.trigger (1e-3f);
        }
        else if (! stopped)
        {
            player.releaseAll();
            stopped = true;
        }
        playTick.store ((float) std::fmod (clock.pos, (double) (pat ? pat->ticks() : 4 * BAR)));

        const int pv = previewReq.exchange (-1);
        if (pv >= 0) { previewPitch = pv; previewLeft = (int) (0.2f * a.sampleRate); }
        const int n = player.chans;
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
            const float v = pc ? (previewPitch - 60) / 12.f : player.voct (c);
            const float g = (pc || player.gateOut (c)) ? 10.f : 0.f;
            const float vel = pc ? 0.8f : player.ch[c].vel;
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
        json_object_set_new (j, "notes", json_string (toText (doc.pat).c_str()));
        json_object_set_new (j, "key", json_integer (doc.key));
        json_object_set_new (j, "mode", json_integer (doc.mode));
        json_object_set_new (j, "snap", json_integer (doc.snap));
        json_object_set_new (j, "chord", json_integer (doc.chord));
        json_object_set_new (j, "scaleSnap", json_boolean (doc.scaleSnap));
        json_object_set_new (j, "ppq", json_integer (ppq.load()));
        return j;
    }

    // Missing or out-of-range fields load as defaults; bad note lines are skipped (fromText).
    void dataFromJson (json_t* j) override
    {
        auto num = [&] (const char* k, int lo, int hi, int def) {
            json_t* v = json_object_get (j, k);
            const int x = json_is_integer (v) ? (int) json_integer_value (v) : def;
            return x < lo || x > hi ? def : x;
        };
        doc = RollDoc();
        json_t* s = json_object_get (j, "notes");
        if (json_is_string (s)) fromText (json_string_value (s), doc.pat);
        doc.key = num ("key", 0, 11, 0);
        doc.mode = num ("mode", 0, 6, 0);
        doc.snap = num ("snap", 0, SNAP_N - 1, SNAP_DEFAULT);
        doc.chord = num ("chord", 0, CHORD_N - 1, 0);
        json_t* b = json_object_get (j, "scaleSnap");
        doc.scaleSnap = b ? json_is_true (b) : true;
        const int q = num ("ppq", 1, 48, 4);
        ppq = (q == 1 || q == 4 || q == 24 || q == 48) ? q : 4;
        commit();
    }
};
