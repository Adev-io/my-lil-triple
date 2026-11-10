// Roll.cpp: the My lil Roll panel widget and the model.
// Author: Ari Klopfer / A_Development · MIT
#include "Roll.hpp"
#include "RollUi.hpp"
#include "layout.hpp"

struct RollWidget : ModuleWidget
{
    RollEditor ed;
    RollGrid* grid = nullptr;

    RollWidget (Roll* module)
    {
        setModule (module);
        setPanel (createPanel (asset::plugin (pluginInstance, "res/Roll.svg")));
        namespace L = layout::roll;
        ed.m = module;
        auto rect = [] (const float* mm) { return Rect (mm2px (Vec (mm[0], mm[1])), mm2px (Vec (mm[2], mm[3]))); };
        grid = new RollGrid;
        grid->ed = &ed;
        grid->box = rect (L::ROLL_BOX);
        addChild (grid);
        ed.ppt = (grid->box.size.x - ROLL_KEYW) / (2.0 * BAR);   // 2 bars across
        ed.rowH = grid->box.size.y / 36.f;                       // 3 octaves tall
        VelLane* lane = new VelLane;
        lane->ed = &ed;
        lane->grid = grid;
        lane->box = rect (L::VEL_BOX);
        addChild (lane);

        RollEditor* e = &ed;
        auto pick = [] (Menu* menu, std::vector<std::string> names, std::function<int()> get, std::function<void (int)> set) {
            for (int i = 0; i < (int) names.size(); ++i)
                menu->addChild (createCheckMenuItem (names[i], "", [=] { return get() == i; }, [=] { set (i); }));
        };
        std::vector<std::string> snapNames, chordNames, bars;
        for (int i = 0; i < SNAP_N; ++i) snapNames.push_back (SNAPS[i].name);
        for (int i = 0; i < CHORD_N; ++i) chordNames.push_back (CHORDS[i].name);
        for (int i = 1; i <= 16; ++i) bars.push_back (string::f ("%d BAR%s", i, i > 1 ? "S" : ""));
        const std::vector<std::string> tools = { "DRAW", "SELECT", "ERASE" };
        const std::vector<std::string> keys (KEY_NAMES, KEY_NAMES + 12), modes (MODE_NAMES, MODE_NAMES + 7);

        struct Spec { std::function<std::string()> text; std::function<void()> click; std::function<void (Menu*)> list; };
        auto doc = [=] () -> RollDoc& { return e->m->doc; };
        auto setBars = [=] (int b) { doc().pat.bars = b; e->commit(); };
        std::vector<Spec> cells = {
            { [=] { return tools[e->tool]; }, [=] { e->tool = (e->tool + 1) % RollEditor::TOOL_N; },
              [=] (Menu* m) { pick (m, tools, [=] { return e->tool; }, [=] (int i) { e->tool = i; }); } },
            { [=] { return "SNAP " + snapNames[doc().snap]; }, [=] { doc().snap = (doc().snap + 1) % SNAP_N; },
              [=] (Menu* m) { pick (m, snapNames, [=] { return doc().snap; }, [=] (int i) { doc().snap = i; }); } },
            { [=] { return "KEY " + keys[doc().key]; }, [=] { doc().key = (doc().key + 1) % 12; },
              [=] (Menu* m) { pick (m, keys, [=] { return doc().key; }, [=] (int i) { doc().key = i; }); } },
            { [=] { return modes[doc().mode]; }, [=] { doc().mode = (doc().mode + 1) % 7; },
              [=] (Menu* m) { pick (m, modes, [=] { return doc().mode; }, [=] (int i) { doc().mode = i; }); } },
            { [=] { return std::string (doc().scaleSnap ? "SCALE SNAP" : "FREE"); }, [=] { doc().scaleSnap = ! doc().scaleSnap; }, nullptr },
            { [=] { return chordNames[doc().chord]; }, [=] { doc().chord = (doc().chord + 1) % CHORD_N; },
              [=] (Menu* m) { pick (m, chordNames, [=] { return doc().chord; }, [=] (int i) { doc().chord = i; }); } },
            { [=] { return bars[doc().pat.bars - 1]; }, [=] { setBars (doc().pat.bars % 16 + 1); },
              [=] (Menu* m) { pick (m, bars, [=] { return doc().pat.bars - 1; }, [=] (int i) { setBars (i + 1); }); } },
            { [=] { return std::string ("TOOLS"); }, nullptr,
              [=] (Menu* m) { m->addChild (createMenuLabel ("FL tools (quantize, chop, strum...) arrive in piece 2")); } },
        };
        const Rect sb = rect (L::STRIP_BOX);
        const float gap = mm2px (1.2f), cw = (sb.size.x - gap * (cells.size() - 1)) / cells.size();
        for (int i = 0; i < (int) cells.size(); ++i)
        {
            StripCell* c = new StripCell;
            c->box = Rect (Vec (sb.pos.x + i * (cw + gap), sb.pos.y), Vec (cw, sb.size.y));
            if (module) { c->text = cells[i].text; c->click = cells[i].click; c->list = cells[i].list; }
            else c->text = [=] { return std::string (i == 0 ? "DRAW" : ""); };
            if (i == 7 && module) c->click = [=] { cells[7].list (createMenu()); };   // TOOLS opens on left click too
            addChild (c);
        }
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        auto at = [] (const float* mm) { return mm2px (Vec (mm[0], mm[1])); };
        addParam (createParamCentered<Rogan1PSWhite> (at (L::BPM_PARAM), module, Roll::BPM_PARAM));
        addParam (createParamCentered<VCVButton> (at (L::RST_PARAM), module, Roll::RST_PARAM));
        addParam (createLightParamCentered<VCVLightLatch<MediumSimpleLight<LimeLight>>> (at (L::RUN_PARAM), module, Roll::RUN_PARAM, Roll::RUN_LIGHT));
        addInput (createInputCentered<PJ301MPort> (at (L::CLK_INPUT), module, Roll::CLK_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::RST_INPUT), module, Roll::RST_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::RUN_INPUT), module, Roll::RUN_INPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::VOCT_OUTPUT), module, Roll::VOCT_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::GATE_OUTPUT), module, Roll::GATE_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::VEL_OUTPUT), module, Roll::VEL_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::EOC_OUTPUT), module, Roll::EOC_OUTPUT));
    }

    void appendContextMenu (Menu* menu) override
    {
        Roll* m = dynamic_cast<Roll*> (module);
        if (! m) return;
        RollEditor* e = &ed;
        menu->addChild (new MenuSeparator);
        menu->addChild (createIndexSubmenuItem ("Clock pulses per quarter", { "1", "4 (16ths)", "24", "48" },
            [=] { const int q = m->ppq; return (size_t) (q == 1 ? 0 : q == 24 ? 2 : q == 48 ? 3 : 1); },
            [=] (size_t i) { static const int Q[4] = { 1, 4, 24, 48 }; m->ppq = Q[i]; }));
        menu->addChild (createMenuItem ("Copy notes as text", "", [=] { glfwSetClipboardString (APP->window->win, toText (m->doc.pat).c_str()); }));
        menu->addChild (createMenuItem ("Clear notes", "", [=] {
            const std::vector<Note> before = m->doc.pat.notes;
            m->doc.pat.notes.clear();
            e->commit();
            e->record (before);
        }));
    }
};

Model* modelRoll = createModel<Roll, RollWidget> ("Roll");
