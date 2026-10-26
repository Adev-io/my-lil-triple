// Triple.cpp: the panel widget and the model.
// Author: Ari Klopfer / A_Development · MIT
#include "Triple.hpp"
#include "TalkingFace.hpp"
#include "layout.hpp"
#include <osdialog.h>

struct TripleWidget : ModuleWidget
{
    TripleWidget (Triple* module)
    {
        setModule (module);
        setPanel (createPanel (asset::plugin (pluginInstance, "res/Triple.svg")));
        namespace L = layout::triple;
        {
            TalkingFace* f = createWidget<TalkingFace> (mm2px (Vec (L::FACE[0] - L::FACE[2], L::FACE[1] - L::FACE[2])));
            f->box.size = mm2px (Vec (2 * L::FACE[2], 2 * L::FACE[2]));
            if (module) f->tap = &module->face;
            addChild (f);
        }
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        auto at = [] (const float* mm) { return mm2px (Vec (mm[0], mm[1])); };
        auto knob = [&] (const float* mm, int id) { addParam (createParamCentered<Rogan1PSWhite> (at (mm), module, id)); };
        auto latch = [&] (const float* mm, int id, int light) {
            addParam (createLightParamCentered<VCVLightLatch<MediumSimpleLight<LimeLight>>> (at (mm), module, id, light));
        };

        for (int o = 0; o < 3; ++o)
            for (int j = 0; j < O_N; ++j)
            {
                if (o == 0 && j == O_MIX) continue;   // osc 1 has no mix
                const int id = Triple::OSC_PARAM + o * O_N + j;
                if (j == O_INV) latch (L::OSC[o][j], id, Triple::INV_LIGHT + o);
                else knob (L::OSC[o][j], id);
            }
        latch (L::AM_PARAM, Triple::AM_PARAM, Triple::AM_LIGHT);
        latch (L::HQ_PARAM, Triple::HQ_PARAM, Triple::HQ_LIGHT);
        knob (L::A_ATK_PARAM, Triple::A_ATK_PARAM);     knob (L::A_DEC_PARAM, Triple::A_DEC_PARAM);
        knob (L::A_SUS_PARAM, Triple::A_SUS_PARAM);     knob (L::A_REL_PARAM, Triple::A_REL_PARAM);
        knob (L::FTYPE_PARAM, Triple::FTYPE_PARAM);     knob (L::CUTOFF_PARAM, Triple::CUTOFF_PARAM);
        knob (L::RES_PARAM, Triple::RES_PARAM);         knob (L::ENVAMT_PARAM, Triple::ENVAMT_PARAM);
        knob (L::KEYTRK_PARAM, Triple::KEYTRK_PARAM);
        knob (L::F_ATK_PARAM, Triple::F_ATK_PARAM);     knob (L::F_DEC_PARAM, Triple::F_DEC_PARAM);
        knob (L::F_SUS_PARAM, Triple::F_SUS_PARAM);     knob (L::F_REL_PARAM, Triple::F_REL_PARAM);
        addInput (createInputCentered<PJ301MPort> (at (L::VOCT_INPUT), module, Triple::VOCT_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::GATE_INPUT), module, Triple::GATE_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::CUTCV_INPUT), module, Triple::CUTCV_INPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::OUT_L_OUTPUT), module, Triple::OUT_L_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::OUT_R_OUTPUT), module, Triple::OUT_R_OUTPUT));
    }

    void appendContextMenu (Menu* menu) override
    {
        Triple* m = dynamic_cast<Triple*> (module);
        if (! m) return;
        menu->addChild (new MenuSeparator);
        menu->addChild (createMenuItem ("Load custom wave (single-cycle WAV)...", "", [=] {
            osdialog_filters* filters = osdialog_filters_parse ("WAV:wav");
            char* path = osdialog_file (OSDIALOG_OPEN, nullptr, nullptr, filters);
            osdialog_filters_free (filters);
            if (path) { m->loadTable (path); std::free (path); }
        }));
        menu->addChild (createMenuLabel (m->tablePath.empty() ? "Custom wave: none (sine)" : "Custom wave: " + system::getFilename (m->tablePath)));
        menu->addChild (createMenuItem ("Clear custom wave", "", [=] { m->loadTable (""); }));
    }
};

Model* modelTriple = createModel<Triple, TripleWidget> ("Triple");
