#pragma once

#include <windows.h>

// Finds the runway icon in EuroScope's top bar, wherever the bar's layout puts it.
// The icon is matched by shape (bright pixels on the dark bar), not by exact colour.
namespace rwyicon {

struct Match
{
    bool found = false;
    HWND view = nullptr;  // the radar view holding the top bar
    POINT center = {};    // icon centre, client coordinates of `view`
    int mismatches = 0;   // differing pixels at the best position
    int pixels = 0;       // pixels in the icon pattern
};

Match Locate();

}
