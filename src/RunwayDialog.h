#pragma once

#include <windows.h>

#include <string>
#include <vector>

// Drives EuroScope's "Active airport/runway selector dialog" the way a controller would.
//
// Layout seen in 3.2.3.2: a SysListView32 (id 1012) with one row per runway end, columns
//   0 airport | 1 airport DEP | 2 airport ARR | 3 runway | 4 runway DEP | 5 runway ARR
// Tick cells hold the text "ICON:<n>"; "ICON:0" is unticked. EuroScope ignores synthesized
// list clicks (posted mouse messages, NM_CLICK), so clicks go through the cursor position.
namespace rwydialog {

constexpr int kColRunwayDep = 4;
constexpr int kColRunwayArr = 5;

const char* const kTitle = "Active airport/runway selector dialog";

struct Row
{
    int index = 0;
    std::string airport;
    std::string runway;
    std::string cells[6];
};

enum class ClickMethod
{
    CursorAndPost,  // move the cursor onto the cell, post WM_LBUTTONDOWN/UP to the window
    RealMouse,      // real mouse click via SendInput
    Count
};

const char* MethodName(ClickMethod method);

// The click that opens the dialog (on EuroScope's top-bar runway icon), as recorded by ".rwy spy".
struct Opener
{
    int controlId = 0;
    std::string className;
    POINT point = {};  // client coordinates in that window
};

bool SaveOpener(const Opener& opener);
bool LoadOpener(Opener& opener);

HWND Find();
HWND List(HWND dialog);
std::vector<Row> ReadRows(HWND list);
bool IsTicked(const std::string& cell);

// Both return the cursor position to restore on the next tick.
POINT Click(HWND dialog, HWND list, int row, int column, ClickMethod method);
bool ReplayOpener(const Opener& opener, ClickMethod method, POINT& previousCursor);

void PressOk(HWND dialog);
void PressCancel(HWND dialog);

}
