#include "RunwayIcon.h"

#include "RunwayDialog.h"
#include "ScreenCapture.h"

#include <algorithm>
#include <vector>

namespace rwyicon {

namespace {

// EuroScope 3.2.3.2 runway icon, cropped between the button separators.
// '#' = bright (icon), '.' = dark (bar background).
const char* const kPattern[] = {
    ".......................",
    ".............########..",
    "............#.......#..",
    "...........#...#...#...",
    "...........#.......#...",
    "..........#...#....#...",
    ".........#........#....",
    "........#....#....#....",
    "........#........#.....",
    ".......#.........#.....",
    "......#..##..#..#......",
    ".....#..##..##..#......",
    ".....#.........#.......",
    "....############.......",
    "......#.#.#.#..........",
    ".......#.#.#...........",
    "......#.#.#............",
    ".......................",
    ".......#...............",
    ".......................",
    "......#................",
};
constexpr int kPatternHeight = static_cast<int>(sizeof(kPattern) / sizeof(kPattern[0]));
constexpr int kPatternWidth = 23;

constexpr int kTopBarHeight = 80;          // rows of the radar view searched
constexpr int kBrightLuminance = 160;      // threshold between icon and background
constexpr double kMaxMismatchRatio = 0.03; // accept up to 3 % differing pixels

std::vector<bool> Binarize(const capture::Image& image)
{
    std::vector<bool> bright(static_cast<size_t>(image.width) * image.height);
    for (size_t i = 0; i < bright.size(); ++i)
    {
        const uint8_t* p = &image.bgr[i * 3];
        bright[i] = 0.114 * p[0] + 0.587 * p[1] + 0.299 * p[2] > kBrightLuminance;
    }
    return bright;
}

}

Match Locate()
{
    Match match;
    match.pixels = kPatternWidth * kPatternHeight;
    match.view = rwydialog::FindEuroScopeControl(1, "AfxWnd140");
    if (!match.view)
        return match;

    RECT client = {};
    GetClientRect(match.view, &client);
    capture::Image bar;
    if (!capture::Grab(match.view, RECT{0, 0, client.right, std::min<LONG>(kTopBarHeight, client.bottom)}, bar))
        return match;
    const auto bright = Binarize(bar);

    int best = match.pixels + 1;
    for (int y = 0; y + kPatternHeight <= bar.height; ++y)
    {
        for (int x = 0; x + kPatternWidth <= bar.width; ++x)
        {
            int mismatches = 0;
            for (int row = 0; row < kPatternHeight && mismatches < best; ++row)
            {
                const size_t base = static_cast<size_t>(y + row) * bar.width + x;
                for (int col = 0; col < kPatternWidth; ++col)
                    mismatches += bright[base + col] != (kPattern[row][col] == '#');
            }
            if (mismatches < best)
            {
                best = mismatches;
                match.center = POINT{x + kPatternWidth / 2, y + kPatternHeight / 2};
            }
        }
    }
    match.mismatches = best;
    match.found = best <= static_cast<int>(match.pixels * kMaxMismatchRatio);
    return match;
}

}
