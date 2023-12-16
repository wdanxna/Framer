#include <iostream>
#include "framer.hpp"

int main() {
    using namespace WDFramer;
    Framer framer(
        {720, 1280},
        {360, 480}, 
        Framer::origin_t::bottom_left, 
        Framer::mode_t::aspectFit
    );

    auto p = framer.frame2Pic({0.0f, 480.0f}, Framer::norm_t::normalize);

    float verts[8], tex[8];
    framer.gl_fullscreen_quad(verts, tex, Framer::rot_t::none);

    std::cout << "hello" << std::endl;
}