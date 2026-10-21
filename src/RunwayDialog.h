#pragma once

#include <windows.h>

#include <string>
#include <vector>

// Drives EuroScope's "Active airport/runway selector dialog" the way a controller would.
//
// Layout seen in 3.2.3.2: a SysListView32 (id 1012) with one row per runway end, columns
//   0 airport | 1 airport DEP | 2 airport ARR | 3 runway | 4 runway DEP | 5 runway ARR
// Tick cells hold the text "ICON:<n>"; "ICON:0" is unticked.
namespace rwydialog {

constexpr int kColAirportDep = 1;
constexpr int kColAirportArr = 2;
constexpr int kColRunwayDep = 4;
constexpr int kColRunwayArr = 5;

struct Row
{
    int index = 0;
    std::string airport;
    std::string runway;
    std::string cells[6];
};

enum class ClickMethod
{
    PostedMouse,   // WM_LBUTTONDOWN/UP posted to the list
    Notification,  // NM_CLICK sent to the dialog
    RealMouse,     // real mouse click via SendInput (moves the cursor briefly)
    Count
};

const char* MethodName(ClickMethod method);

HWND Find();
HWND List(HWND dialog);
std::vector<Row> ReadRows(HWND list);
bool IsTicked(const std::string& cell);

// Returns the cursor position to restore later (RealMouse moves it).
POINT Click(HWND dialog, HWND list, int row, int column, ClickMethod method);
void PressOk(HWND dialog);

}
