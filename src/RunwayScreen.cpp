#include "RunwayScreen.h"

#include "RunwayPlugin.h"

#include <string>
#include <vector>

namespace {

constexpr int kObjectTitle = 1;
constexpr int kObjectButton = 2;

constexpr int kWidth = 520;
constexpr int kPadding = 10;
constexpr int kTitleHeight = 24;
constexpr int kButtonWidth = 90;
constexpr int kButtonHeight = 24;

const COLORREF kBackground = RGB(22, 38, 42);
const COLORREF kTitleBar = RGB(0, 95, 85);
const COLORREF kBorder = RGB(0, 200, 170);
const COLORREF kText = RGB(235, 240, 240);
const COLORREF kAccept = RGB(30, 130, 70);
const COLORREF kDecline = RGB(150, 50, 45);
const COLORREF kNeutral = RGB(60, 80, 85);

void Fill(HDC dc, const RECT& r, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &r, brush);
    DeleteObject(brush);
}

void Frame(HDC dc, const RECT& r, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(dc, &r, brush);
    DeleteObject(brush);
}

int TextHeight(HDC dc, const std::string& text, int width)
{
    RECT r = {0, 0, width, 0};
    DrawTextA(dc, text.c_str(), -1, &r, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    return r.bottom;
}

}

void RunwayScreen::OnAsrContentToBeClosed()
{
    if (RunwayPlugin* plugin = RunwayPlugin::Instance())
        plugin->ForgetScreen(this);
    delete this;
}

void RunwayScreen::OnRefresh(HDC hDC, int Phase)
{
    RunwayPlugin* plugin = RunwayPlugin::Instance();
    if (Phase != EuroScopePlugIn::REFRESH_PHASE_AFTER_LISTS || !plugin)
        return;
    const Popup* popup = plugin->FrontPopup();
    if (!popup)
        return;

    HFONT font = CreateFontA(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    HFONT bold = CreateFontA(-14, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    HGDIOBJ oldFont = SelectObject(hDC, font);
    const int oldMode = SetBkMode(hDC, TRANSPARENT);
    const COLORREF oldColor = SetTextColor(hDC, kText);

    // Layout: title bar, wrapped lines, buttons.
    const int textWidth = kWidth - 2 * kPadding;
    int bodyHeight = 0;
    for (const auto& line : popup->lines)
        bodyHeight += TextHeight(hDC, line, textWidth) + 4;
    const int height = kTitleHeight + kPadding + bodyHeight + kPadding + kButtonHeight + kPadding;

    POINT origin = plugin->PopupOrigin();
    if (origin.x < 0)
    {
        const RECT radar = GetRadarArea();
        const RECT toolbar = GetToolbarArea();
        origin = POINT{(radar.left + radar.right - kWidth) / 2, toolbar.bottom + 40};
    }
    const RECT box = {origin.x, origin.y, origin.x + kWidth, origin.y + height};
    Fill(hDC, box, kBackground);

    RECT title = {box.left, box.top, box.right, box.top + kTitleHeight};
    Fill(hDC, title, kTitleBar);
    SelectObject(hDC, bold);
    std::string titleText = popup->title;
    if (plugin->PopupCount() > 1)
        titleText += "   (1 of " + std::to_string(plugin->PopupCount()) + ")";
    RECT titleTextRect = {title.left + kPadding, title.top, title.right - kPadding, title.bottom};
    DrawTextA(hDC, titleText.c_str(), -1, &titleTextRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    AddScreenObject(kObjectTitle, "title", title, true, "Drag to move");
    SelectObject(hDC, font);

    int y = title.bottom + kPadding;
    for (const auto& line : popup->lines)
    {
        RECT r = {box.left + kPadding, y, box.right - kPadding, y + 1000};
        DrawTextA(hDC, line.c_str(), -1, &r, DT_WORDBREAK | DT_NOPREFIX);
        y += TextHeight(hDC, line, textWidth) + 4;
    }

    struct Button
    {
        const char* id;
        const char* label;
        COLORREF color;
    };
    std::vector<Button> buttons;
    if (popup->kind == Popup::Kind::Suggestion)
        buttons = {{"accept", "Accept", kAccept}, {"decline", "Decline", kDecline}};
    else
        buttons = {{"ok", popup->kind == Popup::Kind::Info ? "Dismiss" : "OK", kNeutral}};

    int x = box.right - kPadding - static_cast<int>(buttons.size()) * (kButtonWidth + kPadding) + kPadding;
    const int top = box.bottom - kPadding - kButtonHeight;
    for (const auto& b : buttons)
    {
        RECT r = {x, top, x + kButtonWidth, top + kButtonHeight};
        Fill(hDC, r, b.color);
        Frame(hDC, r, kBorder);
        DrawTextA(hDC, b.label, -1, &r, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        AddScreenObject(kObjectButton, b.id, r, false, b.label);
        x += kButtonWidth + kPadding;
    }
    Frame(hDC, box, kBorder);

    SetTextColor(hDC, oldColor);
    SetBkMode(hDC, oldMode);
    SelectObject(hDC, oldFont);
    DeleteObject(font);
    DeleteObject(bold);
}

void RunwayScreen::OnClickScreenObject(int ObjectType, const char* sObjectId, POINT, RECT, int Button)
{
    RunwayPlugin* plugin = RunwayPlugin::Instance();
    if (plugin && ObjectType == kObjectButton && Button == EuroScopePlugIn::BUTTON_LEFT && sObjectId)
        plugin->OnPopupButton(sObjectId);
}

void RunwayScreen::OnMoveScreenObject(int ObjectType, const char*, POINT, RECT Area, bool)
{
    RunwayPlugin* plugin = RunwayPlugin::Instance();
    if (plugin && ObjectType == kObjectTitle)
    {
        plugin->SetPopupOrigin(POINT{Area.left, Area.top});
        RequestRefresh();
    }
}
