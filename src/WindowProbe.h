#pragma once

#include <windows.h>

#include <set>
#include <vector>

// Finds EuroScope's own windows (main window, radar view, dialogs).
namespace probe {

std::vector<HWND> ProcessTopLevelWindows();
std::vector<HWND> Descendants(HWND root);

// All dialog windows (#32770) currently owned by this process, at any depth.
std::set<HWND> CurrentDialogs();

}
