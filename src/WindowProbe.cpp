#include "WindowProbe.h"

#include "Log.h"
#include "RunwayDialog.h"

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

static HHOOK g_sentHook = nullptr;
static HHOOK g_postedHook = nullptr;
static rwydialog::Opener g_lastClick;
static bool g_haveLastClick = false;

static void LogMessage(const char* how, HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
    const char* name = message == WM_COMMAND ? "WM_COMMAND" : message == WM_SYSCOMMAND ? "WM_SYSCOMMAND" : "WM_INITDIALOG";
    char line[128];
    std::snprintf(line, sizeof(line), "spy %s %s id=%u code=%u lParam=%08lX to ", how, name,
                  static_cast<unsigned>(LOWORD(wParam)), static_cast<unsigned>(HIWORD(wParam)),
                  static_cast<unsigned long>(lParam));
    logfile::Write(line + Describe(h));
}

static bool Interesting(UINT message)
{
    return message == WM_COMMAND || message == WM_SYSCOMMAND || message == WM_INITDIALOG;
}

static void RememberClick(const MSG& m)
{
    g_lastClick.controlId = GetDlgCtrlID(m.hwnd);
    g_lastClick.className = ClassOf(m.hwnd);
    g_lastClick.point = POINT{static_cast<short>(LOWORD(m.lParam)), static_cast<short>(HIWORD(m.lParam))};
    g_haveLastClick = true;
    RECT client = {};
    GetClientRect(m.hwnd, &client);
    logfile::Write("spy click at " + std::to_string(g_lastClick.point.x) + "," + std::to_string(g_lastClick.point.y) +
                   " (window " + std::to_string(client.right) + "x" + std::to_string(client.bottom) + ") on " +
                   Describe(m.hwnd));
}

static void OnDialogInit(HWND dialog)
{
    if (TextOf(dialog) != rwydialog::kTitle || !g_haveLastClick)
        return;
    if (rwydialog::SaveOpener(g_lastClick))
        logfile::Write("spy saved the click that opened the runway dialog");
}

static LRESULT CALLBACK SentHook(int code, WPARAM wParam, LPARAM lParam)
{
    const auto* m = reinterpret_cast<const CWPSTRUCT*>(lParam);
    if (code == HC_ACTION && Interesting(m->message))
    {
        LogMessage("sent", m->hwnd, m->message, m->wParam, m->lParam);
        if (m->message == WM_INITDIALOG)
            OnDialogInit(m->hwnd);
    }
    return CallNextHookEx(g_sentHook, code, wParam, lParam);
}

static LRESULT CALLBACK PostedHook(int code, WPARAM wParam, LPARAM lParam)
{
    const auto* m = reinterpret_cast<const MSG*>(lParam);
    if (code == HC_ACTION && wParam == PM_REMOVE)
    {
        if (Interesting(m->message))
            LogMessage("posted", m->hwnd, m->message, m->wParam, m->lParam);
        else if (m->message == WM_LBUTTONDOWN)
            RememberClick(*m);
    }
    return CallNextHookEx(g_postedHook, code, wParam, lParam);
}

void StartSpy()
{
    StopSpy();
    g_haveLastClick = false;
    g_sentHook = SetWindowsHookExA(WH_CALLWNDPROC, SentHook, nullptr, GetCurrentThreadId());
    g_postedHook = SetWindowsHookExA(WH_GETMESSAGE, PostedHook, nullptr, GetCurrentThreadId());
    logfile::Write("=== spy started");
}

void StopSpy()
{
    if (g_sentHook)
        UnhookWindowsHookEx(g_sentHook);
    if (g_postedHook)
        UnhookWindowsHookEx(g_postedHook);
    if (g_sentHook || g_postedHook)
        logfile::Write("=== spy stopped");
    g_sentHook = g_postedHook = nullptr;
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
