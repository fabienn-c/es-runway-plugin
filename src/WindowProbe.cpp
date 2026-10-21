#include "WindowProbe.h"

#include "Log.h"

#include <commctrl.h>

#include <cstdio>
#include <string>
#include <vector>

namespace probe {

static std::string ClassOf(HWND h)
{
    char buf[256] = {};
    GetClassNameA(h, buf, sizeof(buf));
    return buf;
}

static std::string TextOf(HWND h)
{
    char buf[512] = {};
    GetWindowTextA(h, buf, sizeof(buf));
    return buf;
}

static std::string Describe(HWND h)
{
    char head[160];
    std::snprintf(head, sizeof(head), "hwnd=%p parent=%p id=%d style=%08lX visible=%d ", static_cast<void*>(h),
                  static_cast<void*>(GetParent(h)), GetDlgCtrlID(h), static_cast<unsigned long>(GetWindowLongA(h, GWL_STYLE)),
                  IsWindowVisible(h) ? 1 : 0);
    return head + std::string("class=\"") + ClassOf(h) + "\" text=\"" + TextOf(h) + "\"";
}

static std::vector<HWND> ProcessTopLevelWindows()
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

static std::vector<HWND> Descendants(HWND root)
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

static void DumpListView(HWND h)
{
    const int items = static_cast<int>(SendMessageA(h, LVM_GETITEMCOUNT, 0, 0));
    const HWND header = reinterpret_cast<HWND>(SendMessageA(h, LVM_GETHEADER, 0, 0));
    const int columns = header ? static_cast<int>(SendMessageA(header, HDM_GETITEMCOUNT, 0, 0)) : 1;
    const auto exStyle = SendMessageA(h, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
    logfile::Write("      listview items=" + std::to_string(items) + " columns=" + std::to_string(columns) +
                   " checkboxes=" + std::to_string((exStyle & LVS_EX_CHECKBOXES) ? 1 : 0));
    for (int i = 0; i < items && i < 300; ++i)
    {
        std::string row;
        for (int c = 0; c < (columns > 0 ? columns : 1); ++c)
        {
            char text[256] = {};
            LVITEMA item = {};
            item.iSubItem = c;
            item.pszText = text;
            item.cchTextMax = sizeof(text);
            SendMessageA(h, LVM_GETITEMTEXTA, i, reinterpret_cast<LPARAM>(&item));
            row += std::string(c ? " | " : "") + text;
        }
        const auto state = (SendMessageA(h, LVM_GETITEMSTATE, i, LVIS_STATEIMAGEMASK) & LVIS_STATEIMAGEMASK) >> 12;
        logfile::Write("        [" + std::to_string(i) + "] check=" + std::to_string(state) + "  " + row);
    }
}

static void DumpListBox(HWND h)
{
    const int items = static_cast<int>(SendMessageA(h, LB_GETCOUNT, 0, 0));
    logfile::Write("      listbox items=" + std::to_string(items));
    for (int i = 0; i < items && i < 300; ++i)
    {
        const auto len = SendMessageA(h, LB_GETTEXTLEN, i, 0);
        if (len < 0 || len > 1000)
            continue;
        std::string text(static_cast<size_t>(len) + 1, '\0');
        SendMessageA(h, LB_GETTEXT, i, reinterpret_cast<LPARAM>(text.data()));
        const auto selected = SendMessageA(h, LB_GETSEL, i, 0);
        logfile::Write("        [" + std::to_string(i) + "] sel=" + std::to_string(selected) + "  " + text.c_str());
    }
}

static void DumpControlDetails(HWND h)
{
    const std::string cls = ClassOf(h);
    if (cls == WC_LISTVIEWA)
        DumpListView(h);
    else if (cls == "ListBox")
        DumpListBox(h);
    else if (cls == "ComboBox")
        logfile::Write("      combobox items=" + std::to_string(SendMessageA(h, CB_GETCOUNT, 0, 0)) +
                       " selected=" + std::to_string(SendMessageA(h, CB_GETCURSEL, 0, 0)));
    else if (cls == "Button")
    {
        const auto type = GetWindowLongA(h, GWL_STYLE) & BS_TYPEMASK;
        if (type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE)
            logfile::Write("      checkbox checked=" + std::to_string(SendMessageA(h, BM_GETCHECK, 0, 0)));
    }
}

void DumpWindowTree(HWND root)
{
    logfile::Write("  WINDOW " + Describe(root));
    DumpControlDetails(root);
    for (HWND child : Descendants(root))
    {
        logfile::Write("    child " + Describe(child));
        DumpControlDetails(child);
    }
}

void DumpProcessWindows(bool deep)
{
    const auto windows = ProcessTopLevelWindows();
    logfile::Write("=== EuroScope windows: " + std::to_string(windows.size()) + " top-level");
    for (HWND h : windows)
    {
        if (deep)
            DumpWindowTree(h);
        else
            logfile::Write("  WINDOW " + Describe(h));
    }
    logfile::Write("=== end of window dump");
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
