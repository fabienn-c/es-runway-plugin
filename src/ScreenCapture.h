#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

// Grabs pixels from a EuroScope window and saves them as .bmp files.
namespace capture {

struct Image
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> bgr;  // top-down rows, 3 bytes per pixel, no padding

    bool Empty() const { return width <= 0 || height <= 0; }
};

// Copies the client-area rectangle `area` of window h (must be visible on screen).
bool Grab(HWND h, RECT area, Image& out);

bool SaveBmp(const std::string& path, const Image& image);

}
