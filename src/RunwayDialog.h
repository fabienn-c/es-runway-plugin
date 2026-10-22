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
    CursorAndPost,  // move the cursor onto the target, post WM_LBUTTONDOWN/UP to the window
    RealMouse,      // real mouse click via SendInput
    Count
};

const char* MethodName(ClickMethod method);

// Visible child of the main EuroScope window with this control id and class
// (the radar view holding the top bar is id 1, class "AfxWnd140").
HWND FindEuroScopeControl(int controlId, const std::string& className);

// True when the active (foreground) window belongs to EuroScope: its main window or one of its dialogs.
bool EuroScopeIsActive();

HWND Find();
HWND List(HWND dialog);
std::vector<Row> ReadRows(HWND list);
bool IsTicked(const std::string& cell);

// Both return the cursor position to restore on the next tick.
POINT Click(HWND list, int row, int column, ClickMethod method);
POINT ClickWindow(HWND window, POINT client, ClickMethod method);

void PressOk(HWND dialog);
void PressCancel(HWND dialog);

}
