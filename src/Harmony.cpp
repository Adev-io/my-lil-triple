// Harmony.cpp: My lil Harmony. A lead quantizer + harmonizer that follows the chords of a My lil Roll.
// Lead V/OCT in (a MIDI-CV), chord V/OCT + gate in (the Roll); out: poly V/OCT (lead first) + one gate per voice.
// Author: Ari Klopfer / A_Development · MIT
#include "plugin.hpp"
#include "Harmony.h"
#include "layout.hpp"

static const char* const FLAT_NAMES[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

// "Em", "F", "Bdim" ... for a latched triad; the note list for anything else.
static std::string chordName (Mask m)
{
    if (! m) return "-";
    const char* suffix = "";
    const int r = chordRoot (m, &suffix);
    if (r >= 0) return std::string (FLAT_NAMES[r]) + suffix;
    std::string s;
    for (int p = 0; p < 12; ++p) if (has (m, p)) s += std::string (s.empty() ? "" : " ") + FLAT_NAMES[p];
    return s;
}

struct Harmony : Module
{
    enum ParamId { KEY_PARAM, MODE_PARAM, HARM_PARAM, GLIDE_PARAM, SNAP_PARAM, COLOR_PARAM, SET_PARAM, PARAMS_LEN };
    enum InputId { VOCT_INPUT, GATE_INPUT, CHORD_INPUT, CHGATE_INPUT, PADS_INPUT, PADGATE_INPUT, INPUTS_LEN };
    enum OutputId { VOCT_OUTPUT, GATE_OUTPUT, OUTPUTS_LEN };
    enum LightId { SNAP_LIGHT, LIGHTS_LEN };

    ChordFollower follow;
    float y[6] = {};
    int lastN = 0;
    dsp::SchmittTrigger padTrig;
    // read by the display
    Mask shownChord = 0, shownScale = 0;
    int shownVoices[6] = {}, shownN = 0;

    Harmony()
    {
        config (PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configSwitch (KEY_PARAM, 0.f, 11.f, 4.f, "Key", { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" });
        configSwitch (MODE_PARAM, 0.f, 6.f, 2.f, "Mode", { "Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian", "Locrian" });
        configSwitch (HARM_PARAM, 0.f, H_N - 1, H_BELOW, "Harmony", std::vector<std::string> (HARMONY_NAMES, HARMONY_NAMES + H_N));
        configParam (GLIDE_PARAM, 0.f, 1.f, 0.f, "Glide", " ms", 0.f, 500.f);
        configSwitch (SNAP_PARAM, 0.f, 1.f, 0.f, "Lead snap", { "Scale (free solo)", "Chord tones only" });
        configSwitch (COLOR_PARAM, 0.f, 8.f, 0.f, "Chord color (bank B pad)", { "Off (line mode)", "1", "2", "3", "4", "5", "6", "7", "8" });
        configSwitch (SET_PARAM, 0.f, 2.f, 0.f, "Color set", { "Dark", "Lush", "Mix" });
        configInput (PADS_INPUT, "Pads V/OCT (MIDI-CV on channel 10)");
        configInput (PADGATE_INPUT, "Pads gate");
        configInput (VOCT_INPUT, "Lead V/OCT (e.g. MIDI-CV)");
        configInput (GATE_INPUT, "Lead gate");
        configInput (CHORD_INPUT, "Chord V/OCT (poly, e.g. My lil Roll V/OCT)");
        configInput (CHGATE_INPUT, "Chord gate (poly, e.g. My lil Roll GATE)");
        configOutput (VOCT_OUTPUT, "Lead + harmony V/OCT (poly, lead first)");
        configOutput (GATE_OUTPUT, "Gate (poly, one per voice)");
    }

    void process (const ProcessArgs& a) override
    {
        if (padTrig.process (inputs[PADGATE_INPUT].getVoltage(), 0.1f, 1.f))
        {
            const int pad = padIndex (inputs[PADS_INPUT].getVoltage());
            if (pad >= 0 && pad < 8) { params[HARM_PARAM].setValue (pad); params[COLOR_PARAM].setValue (0.f); }   // bank A: line mode
            else if (pad >= 8) params[COLOR_PARAM].setValue (pad - 7);                                            // bank B: chord color
        }
        Mask chord = 0;
        if (inputs[CHORD_INPUT].isConnected())
        {
            const int n = std::min (inputs[CHORD_INPUT].getChannels(), 16);
            float v[16], g[16];
            const bool gated = inputs[CHGATE_INPUT].isConnected();
            for (int c = 0; c < n; ++c)
            {
                v[c] = inputs[CHORD_INPUT].getVoltage (c);
                g[c] = gated ? inputs[CHGATE_INPUT].getPolyVoltage (c) : 10.f;
            }
            follow.step (v, g, n, a.sampleTime);
            chord = follow.chord;
        }
        const Mask scale = chordScale (modeMask ((int) params[KEY_PARAM].getValue(), (int) params[MODE_PARAM].getValue()), chord);
        const bool chordOnly = params[SNAP_PARAM].getValue() > 0.5f && chord;
        const int key = 60 + (int) std::lround (inputs[VOCT_INPUT].getVoltage() * 12.f);
        const int lead = snap (key, chordOnly ? chord : scale);
        const int slot = (int) params[COLOR_PARAM].getValue();
        const int color = slot ? COLOR_SETS[(int) params[SET_PARAM].getValue()][slot - 1] : C_OFF;
        int out[6];
        const int n = color ? colorVoices (lead, color, chord, scale, (int) params[KEY_PARAM].getValue(), out)
                            : voices (lead, (int) params[HARM_PARAM].getValue(), chord, scale, out);

        const float tau = 0.5f * params[GLIDE_PARAM].getValue();
        const float k = tau > 0.f ? 1.f - std::exp (-a.sampleTime / tau) : 1.f;
        const float gate = inputs[GATE_INPUT].getVoltage();
        outputs[VOCT_OUTPUT].setChannels (n);
        outputs[GATE_OUTPUT].setChannels (n);
        for (int i = 0; i < n; ++i)
        {
            const float target = (out[i] - 60) / 12.f;
            y[i] = i >= lastN ? target : y[i] + (target - y[i]) * k;   // a voice that just appeared starts on its note
            outputs[VOCT_OUTPUT].setVoltage (y[i], i);
            outputs[GATE_OUTPUT].setVoltage (gate, i);
        }
        lastN = n;
        lights[SNAP_LIGHT].setBrightness (params[SNAP_PARAM].getValue());
        shownChord = chord; shownScale = scale; shownN = n;
        for (int i = 0; i < n; ++i) shownVoices[i] = out[i];
    }
};

// Chord, scale, harmony and the sounding notes, lit so it reads on a dark stage.
struct HarmonyDisplay : Widget
{
    Harmony* m = nullptr;
    void drawLayer (const DrawArgs& args, int layer) override
    {
        if (layer != 1) return;
        std::shared_ptr<Font> font = APP->window->loadFont (asset::system ("res/fonts/ShareTechMono-Regular.ttf"));
        if (! font) return;
        NVGcontext* vg = args.vg;
        nvgFontFaceId (vg, font->handle);
        nvgTextAlign (vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        std::string scale, notes;
        const Mask s = m ? m->shownScale : modeMask (4, 2);
        for (int p = 0; p < 12; ++p) if (has (s, p)) scale += std::string (scale.empty() ? "" : " ") + FLAT_NAMES[p];
        if (m) for (int i = 0; i < m->shownN; ++i)
            notes += std::string (i ? " " : "") + FLAT_NAMES[pclass (m->shownVoices[i])] + std::to_string (m->shownVoices[i] / 12 - 1);
        const int h = m ? (int) m->params[Harmony::HARM_PARAM].getValue() : H_BELOW;
        const float x = 4.f;
        nvgFillColor (vg, nvgRGB (0x8d, 0x8d, 0x96)); nvgFontSize (vg, 9.f);
        nvgText (vg, x, 3.f, "CHORD", nullptr);
        nvgText (vg, x, 34.f, "SCALE", nullptr);
        nvgText (vg, x, 58.f, "VOICES", nullptr);
        nvgFillColor (vg, nvgRGB (0xc8, 0xff, 0x3a)); nvgFontSize (vg, 18.f);
        nvgText (vg, x + 34.f, -1.f, chordName (m ? m->shownChord : 0).c_str(), nullptr);
        nvgFillColor (vg, nvgRGB (0xf2, 0xf2, 0xf4)); nvgFontSize (vg, 9.f);
        const int slot = m ? (int) m->params[Harmony::COLOR_PARAM].getValue() : 0;
        const int set = m ? (int) m->params[Harmony::SET_PARAM].getValue() : 0;
        const std::string mode = slot ? std::string (SET_NAMES[set]) + ": " + COLOR_NAMES[COLOR_SETS[set][slot - 1]] : HARMONY_NAMES[h];
        nvgText (vg, x, 18.f, mode.c_str(), nullptr);
        nvgText (vg, x, 45.f, scale.c_str(), nullptr);
        nvgText (vg, x, 69.f, notes.c_str(), nullptr);
        nvgFillColor (vg, nvgRGB (0x5a, 0xc8, 0xe6));
        nvgText (vg, x, 84.f, (m && m->params[Harmony::SNAP_PARAM].getValue() > 0.5f) ? "lead: chord tones" : "lead: scale", nullptr);
    }
};

struct HarmonyWidget : ModuleWidget
{
    HarmonyWidget (Harmony* module)
    {
        setModule (module);
        setPanel (createPanel (asset::plugin (pluginInstance, "res/Harmony.svg")));
        namespace L = layout::harmony;
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        auto at = [] (const float* mm) { return mm2px (Vec (mm[0], mm[1])); };
        auto knob = [&] (const float* mm, int id) { addParam (createParamCentered<Rogan1PSWhite> (at (mm), module, id)); };
        knob (L::KEY_PARAM, Harmony::KEY_PARAM);
        knob (L::MODE_PARAM, Harmony::MODE_PARAM);
        knob (L::HARM_PARAM, Harmony::HARM_PARAM);
        knob (L::GLIDE_PARAM, Harmony::GLIDE_PARAM);
        knob (L::COLOR_PARAM, Harmony::COLOR_PARAM);
        knob (L::SET_PARAM, Harmony::SET_PARAM);
        addParam (createLightParamCentered<VCVLightLatch<MediumSimpleLight<LimeLight>>> (at (L::SNAP_PARAM), module, Harmony::SNAP_PARAM, Harmony::SNAP_LIGHT));
        addInput (createInputCentered<PJ301MPort> (at (L::VOCT_INPUT), module, Harmony::VOCT_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::GATE_INPUT), module, Harmony::GATE_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::CHORD_INPUT), module, Harmony::CHORD_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::CHGATE_INPUT), module, Harmony::CHGATE_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::PADS_INPUT), module, Harmony::PADS_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::PADGATE_INPUT), module, Harmony::PADGATE_INPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::VOCT_OUTPUT), module, Harmony::VOCT_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::GATE_OUTPUT), module, Harmony::GATE_OUTPUT));
        HarmonyDisplay* d = createWidget<HarmonyDisplay> (mm2px (Vec (L::DISPLAY_BOX[0], L::DISPLAY_BOX[1])));
        d->box.size = mm2px (Vec (L::DISPLAY_BOX[2], L::DISPLAY_BOX[3]));
        d->m = module;
        addChild (d);
    }
};

Model* modelHarmony = createModel<Harmony, HarmonyWidget> ("Harmony");
