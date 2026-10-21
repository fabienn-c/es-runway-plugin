#pragma once

#include <windows.h>

#include <set>
#include <vector>

// Diagnostics for the proof of concept: logs the structure of EuroScope's own
// windows so we can see how the "Active airport/runway" dialog is built.
namespace probe {

std::vector<HWND> ProcessTopLevelWindows();
std::vector<HWND> Descendants(HWND root);

// All dialog windows (#32770) currently owned by this process, at any depth.
std::set<HWND> CurrentDialogs();

// Writes every top-level window of the process (and, if deep, their children) to the log.
void DumpProcessWindows(bool deep);

// Writes one window and all its descendants, including list/checkbox contents.
void DumpWindowTree(HWND root);

// Logs WM_COMMAND / WM_SYSCOMMAND / WM_INITDIALOG and left clicks on EuroScope's UI thread
// until stopped. When the runway dialog opens, the click that opened it is saved as the opener.
void StartSpy();
void StopSpy();

}
