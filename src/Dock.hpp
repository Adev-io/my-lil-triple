// Dock.hpp: what My lil Roll sends to a My lil Triple placed directly on its right (Rack expander message).
// Author: Ari Klopfer / A_Development · MIT
#pragma once

struct DockMsg
{
    int channels = 0;
    float voct[16] = {}, gate[16] = {}, vel[16] = {};   // volts (0 = C4), volts (>= 1 = on), 0..1
};
