#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

// Grabs pixels from a EuroScope window and finds a small image (the runway icon) inside them.
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
Image Crop(const Image& image, int x, int y, int width, int height);

bool SaveBmp(const std::string& path, const Image& image);
bool LoadBmp(const std::string& path, Image& image);

// Best match of `needle` in `haystack`: top-left position and mean absolute difference
// per colour channel (0 = identical, 255 = opposite).
bool FindBest(const Image& haystack, const Image& needle, POINT& at, double& meanDifference);

}
