#include "plugin.hpp"
Plugin* pluginInstance;
void init (Plugin* p) { pluginInstance = p; p->addModel (modelTriple); p->addModel (modelRoll); p->addModel (modelHarmony); }
