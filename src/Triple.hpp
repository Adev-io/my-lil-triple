// Triple.hpp: the My lil Triple module: 16 poly voices of three oscillators, filter and two ADSRs.
// Author: Ari Klopfer / A_Development · MIT
#pragma once
#include "plugin.hpp"
#include "FaceTap.h"
#include "TripleVoice.h"
#include "WaveTable.h"
#include <atomic>

enum OscParam { O_WAVE, O_INV, O_PHASE, O_COARSE, O_FINE, O_PAN, O_MIX, O_N };

struct Triple : Module
{
    enum ParamId
    {
        OSC_PARAM,   // 3 x O_N
        AM_PARAM = OSC_PARAM + 3 * O_N, HQ_PARAM,
        A_ATK_PARAM, A_DEC_PARAM, A_SUS_PARAM, A_REL_PARAM,
        FTYPE_PARAM, CUTOFF_PARAM, RES_PARAM, ENVAMT_PARAM, KEYTRK_PARAM,
        F_ATK_PARAM, F_DEC_PARAM, F_SUS_PARAM, F_REL_PARAM,
        PARAMS_LEN
    };
    enum InputId { VOCT_INPUT, GATE_INPUT, CUTCV_INPUT, INPUTS_LEN };
    enum OutputId { OUT_L_OUTPUT, OUT_R_OUTPUT, OUTPUTS_LEN };
    enum LightId { INV_LIGHT, AM_LIGHT = INV_LIGHT + 3, HQ_LIGHT, LIGHTS_LEN };

    TripleVoice voice[16];
    TripleCfg cfg;
    std::vector<float>* table = nullptr;                      // custom wave; the audio thread owns this one
    std::atomic<std::vector<float>*> incoming { nullptr };    // the UI thread hands a new one over here
    std::string tablePath;                                    // UI thread only; saved in the patch
    FaceTap face;
    int tick = 0;

    Triple()
    {
        config (PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        static const char* const names[3] = { "Osc 1", "Osc 2", "Osc 3" };
        for (int o = 0; o < 3; ++o)
        {
            const int b = OSC_PARAM + o * O_N;
            const std::string n = names[o];
            configSwitch (b + O_WAVE, 0.f, W_N - 1, W_SAW, n + " wave", { "Sine", "Triangle", "Square", "Saw", "Rounded saw", "Noise", "Custom" });
            configSwitch (b + O_INV, 0.f, 1.f, 0.f, n + " invert", { "Off", "Inverted" });
            configParam (b + O_PHASE, 0.f, 1.f, 0.f, n + " phase offset (R ahead of L)", " deg", 0.f, 360.f);
            configParam (b + O_COARSE, -24.f, 24.f, 0.f, n + " coarse", " st")->snapEnabled = true;
            configParam (b + O_FINE, -1.f, 1.f, o == 1 ? 0.1f : o == 2 ? -0.1f : 0.f, n + " fine", " st");
            configParam (b + O_PAN, -1.f, 1.f, 0.f, n + " pan");
            if (o > 0) configParam (b + O_MIX, 0.f, 1.f, 0.5f, n + " mix (relative to osc 1)", "%", 0.f, 100.f);
        }
        configSwitch (AM_PARAM, 0.f, 1.f, 0.f, "AM OSC3 (osc 3 modulates osc 1 + 2)", { "Off", "On" });
        configSwitch (HQ_PARAM, 0.f, 1.f, 1.f, "HQ anti-aliasing", { "Off", "On" });
        configParam (A_ATK_PARAM, 0.0001f, 2.f, 0.002f, "Amp attack", " ms", 0.f, 1000.f);
        configParam (A_DEC_PARAM, 0.001f, 4.f, 0.3f, "Amp decay", " ms", 0.f, 1000.f);
        configParam (A_SUS_PARAM, 0.f, 1.f, 0.8f, "Amp sustain", "%", 0.f, 100.f);
        configParam (A_REL_PARAM, 0.001f, 4.f, 0.2f, "Amp release", " ms", 0.f, 1000.f);
        configSwitch (FTYPE_PARAM, 0.f, F_N - 1, F_LP, "Filter type", { "Low-pass", "High-pass", "Band-pass" });
        configParam (CUTOFF_PARAM, std::log2 (20.f), std::log2 (20000.f), std::log2 (20000.f), "Cutoff", " Hz", 2.f);
        configParam (RES_PARAM, 0.f, 1.f, 0.f, "Resonance", "%", 0.f, 100.f);
        configParam (ENVAMT_PARAM, -1.f, 1.f, 0.f, "Filter envelope amount (+-6 octaves)", "%", 0.f, 100.f);
        configParam (KEYTRK_PARAM, 0.f, 1.f, 0.f, "Key tracking", "%", 0.f, 100.f);
        configParam (F_ATK_PARAM, 0.0001f, 2.f, 0.002f, "Filter attack", " ms", 0.f, 1000.f);
        configParam (F_DEC_PARAM, 0.001f, 4.f, 0.3f, "Filter decay", " ms", 0.f, 1000.f);
        configParam (F_SUS_PARAM, 0.f, 1.f, 0.5f, "Filter sustain", "%", 0.f, 100.f);
        configParam (F_REL_PARAM, 0.001f, 4.f, 0.2f, "Filter release", " ms", 0.f, 1000.f);
        configInput (VOCT_INPUT, "V/OCT (poly)");
        configInput (GATE_INPUT, "Gate (poly)");
        configInput (CUTCV_INPUT, "Cutoff CV (1 V/oct, poly)");
        configOutput (OUT_L_OUTPUT, "Left (poly)");
        configOutput (OUT_R_OUTPUT, "Right (poly)");
        refresh();
    }

    ~Triple()
    {
        delete table;
        delete incoming.load();
    }

    // ---- custom wave (UI thread) ----
    // An empty table means "sine"; the audio thread only ever sees whole tables.
    bool loadTable (const std::string& path)
    {
        std::vector<float>* t = new std::vector<float>;
        bool ok = false;
        if (! path.empty())
        {
            try { std::vector<uint8_t> d = system::readFile (path); Sample s; ok = wavfile::parse (d.data(), d.size(), s) && makeTable (s, *t); }
            catch (const std::exception&) {}
        }
        if (! ok) t->clear();
        tablePath = ok ? path : "";
        delete incoming.exchange (t);   // a previous unadopted table is dropped
        return ok;
    }

    // ---- audio thread ----
    void refresh()
    {
        for (int o = 0; o < 3; ++o)
        {
            const int b = OSC_PARAM + o * O_N;
            OscCfg& c = cfg.osc[o];
            c.wave = (int) std::round (params[b + O_WAVE].getValue());
            c.invert = params[b + O_INV].getValue() > 0.5f;
            c.phase = params[b + O_PHASE].getValue();
            c.coarse = std::round (params[b + O_COARSE].getValue());
            c.fine = params[b + O_FINE].getValue();
            c.pan = params[b + O_PAN].getValue();
            c.mix = o == 0 ? 1.f : params[b + O_MIX].getValue();
            lights[INV_LIGHT + o].setBrightness (c.invert ? 1.f : 0.f);
        }
        cfg.am = params[AM_PARAM].getValue() > 0.5f;
        cfg.hq = params[HQ_PARAM].getValue() > 0.5f;
        lights[AM_LIGHT].setBrightness (cfg.am ? 1.f : 0.f);
        lights[HQ_LIGHT].setBrightness (cfg.hq ? 1.f : 0.f);
        cfg.ampA = params[A_ATK_PARAM].getValue();   cfg.ampD = params[A_DEC_PARAM].getValue();
        cfg.ampS = params[A_SUS_PARAM].getValue();   cfg.ampR = params[A_REL_PARAM].getValue();
        cfg.ftype = (int) std::round (params[FTYPE_PARAM].getValue());
        cfg.cutoff = std::exp2 (params[CUTOFF_PARAM].getValue());
        cfg.res = params[RES_PARAM].getValue();
        cfg.envAmt = params[ENVAMT_PARAM].getValue();
        cfg.keytrack = params[KEYTRK_PARAM].getValue();
        cfg.fltA = params[F_ATK_PARAM].getValue();   cfg.fltD = params[F_DEC_PARAM].getValue();
        cfg.fltS = params[F_SUS_PARAM].getValue();   cfg.fltR = params[F_REL_PARAM].getValue();
        cfg.table = table;
    }

    void process (const ProcessArgs& a) override
    {
        if (std::vector<float>* t = incoming.exchange (nullptr))
        {
            delete table;   // ponytail: frees on the audio thread; fine for a user-triggered load
            table = t;
            cfg.table = t;
        }
        if ((tick++ & 15) == 0) refresh();   // knobs are read every 16 samples, not every sample

        const int n = std::max (inputs[VOCT_INPUT].getChannels(), inputs[GATE_INPUT].getChannels());
        outputs[OUT_L_OUTPUT].setChannels (n);
        outputs[OUT_R_OUTPUT].setChannels (n);
        float sum = 0.f;
        for (int c = 0; c < n; ++c)
        {
            float l, r;
            voice[c].process (cfg, inputs[VOCT_INPUT].getPolyVoltage (c), inputs[GATE_INPUT].getPolyVoltage (c) >= 1.f,
                              inputs[CUTCV_INPUT].getPolyVoltage (c), a.sampleRate, l, r);
            outputs[OUT_L_OUTPUT].setVoltage (5.f * l, c);
            outputs[OUT_R_OUTPUT].setVoltage (5.f * r, c);
            sum += l;
        }
        face.push (clamp (sum, -1.f, 1.f), a.sampleRate);
    }

    json_t* dataToJson() override
    {
        json_t* j = json_object();
        json_object_set_new (j, "customWave", json_string (tablePath.c_str()));
        return j;
    }
    void dataFromJson (json_t* j) override
    {
        json_t* s = json_object_get (j, "customWave");
        if (json_is_string (s) && *json_string_value (s)) loadTable (json_string_value (s));
    }
};
