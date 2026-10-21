#pragma once

#include <windows.h>

#include <EuroScopePlugIn.h>

// Draws the plugin's pop-ups on a EuroScope radar screen and forwards button clicks.
// The controller must allow the plugin to draw on "Standard ES radar screen".
class RunwayScreen : public EuroScopePlugIn::CRadarScreen
{
public:
    void OnAsrContentToBeClosed() override;
    void OnRefresh(HDC hDC, int Phase) override;
    void OnClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button) override;
    void OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released) override;
};
