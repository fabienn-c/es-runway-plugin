#include "WindowProbe.h"

#include <string>

namespace probe {

static std::string ClassOf(HWND h)
{
    char buf[256] = {};
    GetClassNameA(h, buf, sizeof(buf));
    return buf;
}

std::vector<HWND> ProcessTopLevelWindows()
{
    std::vector<HWND> result;
    EnumWindows(
        [](HWND h, LPARAM param) -> BOOL {
            DWORD pid = 0;
            GetWindowThreadProcessId(h, &pid);
            if (pid == GetCurrentProcessId())
                reinterpret_cast<std::vector<HWND>*>(param)->push_back(h);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&result));
    return result;
}

std::vector<HWND> Descendants(HWND root)
{
    std::vector<HWND> result;
    EnumChildWindows(
        root,
        [](HWND h, LPARAM param) -> BOOL {
            reinterpret_cast<std::vector<HWND>*>(param)->push_back(h);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&result));
    return result;
}

std::set<HWND> CurrentDialogs()
{
    std::set<HWND> dialogs;
    for (HWND top : ProcessTopLevelWindows())
    {
        if (ClassOf(top) == "#32770")
            dialogs.insert(top);
        for (HWND child : Descendants(top))
            if (ClassOf(child) == "#32770")
                dialogs.insert(child);
    }
    return dialogs;
}

}
