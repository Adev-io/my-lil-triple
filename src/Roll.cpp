// Roll.cpp: the My lil Roll panel widget and the model.
// Author: Ari Klopfer / A_Development · MIT
#include "Roll.hpp"
#include "layout.hpp"

struct RollWidget : ModuleWidget
{
    RollWidget (Roll* module)
    {
        setModule (module);
        setPanel (createPanel (asset::plugin (pluginInstance, "res/Roll.svg")));
        namespace L = layout::roll;
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
};

Model* modelRoll = createModel<Roll, RollWidget> ("Roll");
