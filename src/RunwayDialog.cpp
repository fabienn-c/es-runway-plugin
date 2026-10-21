#include "RunwayDialog.h"

#include "WindowProbe.h"

#include <commctrl.h>

namespace rwydialog {

namespace {

constexpr int kListId = 1012;

std::string Trim(const std::string& s)
{
    const auto first = s.find_first_not_of(' ');
    if (first == std::string::npos)
        return {};
    return s.substr(first, s.find_last_not_of(' ') - first + 1);
}

std::string CellText(HWND list, int row, int column)
{
    char text[256] = {};
    LVITEMA item = {};
    item.iSubItem = column;
    item.pszText = text;
    item.cchTextMax = sizeof(text);
    SendMessageA(list, LVM_GETITEMTEXTA, row, reinterpret_cast<LPARAM>(&item));
    return Trim(text);
}

POINT CellCenter(HWND list, int row, int column)
{
    SendMessageA(list, LVM_ENSUREVISIBLE, row, FALSE);
    RECT r = {};
    r.top = column;
    r.left = LVIR_BOUNDS;
    SendMessageA(list, LVM_GETSUBITEMRECT, row, reinterpret_cast<LPARAM>(&r));
    return POINT{(r.left + r.right) / 2, (r.top + r.bottom) / 2};
}

}

const char* MethodName(ClickMethod method)
{
    switch (method)
    {
    case ClickMethod::CursorAndPost: return "cursor + posted click";
    case ClickMethod::RealMouse: return "real mouse";
    default: return "?";
    }
}

HWND FindEuroScopeControl(int controlId, const std::string& className)
{
    for (HWND top : probe::ProcessTopLevelWindows())
    {
        char title[64] = {};
        GetWindowTextA(top, title, sizeof(title));
        if (std::string(title).rfind("EuroScope v", 0) != 0)
            continue;
        for (HWND child : probe::Descendants(top))
        {
            char cls[128] = {};
            GetClassNameA(child, cls, sizeof(cls));
            if (GetDlgCtrlID(child) == controlId && className == cls && IsWindowVisible(child))
                return child;
        }
    }
    return nullptr;
}

HWND Find()
{
    for (HWND dialog : probe::CurrentDialogs())
    {
        char title[128] = {};
        GetWindowTextA(dialog, title, sizeof(title));
        if (IsWindowVisible(dialog) && std::string(title) == kTitle)
            return dialog;
    }
    return nullptr;
}

HWND List(HWND dialog)
{
    return GetDlgItem(dialog, kListId);
}

std::vector<Row> ReadRows(HWND list)
{
    std::vector<Row> rows;
    const int count = static_cast<int>(SendMessageA(list, LVM_GETITEMCOUNT, 0, 0));
    for (int i = 0; i < count; ++i)
    {
        Row row;
        row.index = i;
        for (int c = 0; c < 6; ++c)
            row.cells[c] = CellText(list, i, c);
        row.airport = row.cells[0];
        row.runway = row.cells[3];
        rows.push_back(row);
    }
    return rows;
}

bool IsTicked(const std::string& cell)
{
    return !cell.empty() && cell != "ICON:0";
}

POINT ClickWindow(HWND window, POINT p, ClickMethod method)
{
    POINT previousCursor = {};
    GetCursorPos(&previousCursor);
    POINT screen = p;
    ClientToScreen(window, &screen);

    if (method == ClickMethod::CursorAndPost)
    {
        SetCursorPos(screen.x, screen.y);
        PostMessageA(window, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
        PostMessageA(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(p.x, p.y));
        PostMessageA(window, WM_LBUTTONUP, 0, MAKELPARAM(p.x, p.y));
        return previousCursor;
    }

    SetForegroundWindow(GetAncestor(window, GA_ROOT));
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN), h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const int x0 = GetSystemMetrics(SM_XVIRTUALSCREEN), y0 = GetSystemMetrics(SM_YVIRTUALSCREEN);
    INPUT in[3] = {};
    for (auto& i : in)
    {
        i.type = INPUT_MOUSE;
        i.mi.dx = static_cast<LONG>((screen.x - x0) * 65535.0 / (w - 1));
        i.mi.dy = static_cast<LONG>((screen.y - y0) * 65535.0 / (h - 1));
    }
    in[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    in[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    in[2].mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(3, in, sizeof(INPUT));
    return previousCursor;
}

POINT Click(HWND list, int row, int column, ClickMethod method)
{
    return ClickWindow(list, CellCenter(list, row, column), method);
}

void PressOk(HWND dialog)
{
    PostMessageA(dialog, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(dialog, IDOK)));
}

void PressCancel(HWND dialog)
{
    PostMessageA(dialog, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED),
                 reinterpret_cast<LPARAM>(GetDlgItem(dialog, IDCANCEL)));
}

}
