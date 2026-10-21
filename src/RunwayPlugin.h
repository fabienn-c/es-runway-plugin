#pragma once

#include <windows.h>

#include <EuroScopePlugIn.h>

#include <set>
#include <string>

// Proof of concept: can a plugin change EuroScope's active runways?
//
// Commands (type them in the EuroScope command line):
//   .rwy info                     controller, facility and connection
//   .rwy status <ICAO>            active runways as EuroScope reports them
//   .rwy set <ICAO> <DEP> <ARR>   e.g. ".rwy set LEBL 24R 24L,02": then open the runway dialog and
//                                 let the plugin tick the boxes and press OK
//   .rwy spy                      for 60 s, log the commands EuroScope handles (open the runway dialog)
//   .rwy windows                  log all EuroScope windows to ESRunwayPlugin.log
//   .rwy watch                    for 60 s, log every dialog that opens
class RunwayPlugin : public EuroScopePlugIn::CPlugIn
{
public:
    RunwayPlugin();
    ~RunwayPlugin() override;

    bool OnCompileCommand(const char* sCommandLine) override;
    void OnTimer(int Counter) override;
    void OnAirportRunwayActivityChanged() override;

private:
    // A pending ".rwy set": applied step by step, one click per timer tick.
    struct SetJob
    {
        bool active = false;
        std::string icao;
        std::set<std::string> departures;
        std::set<std::string> arrivals;
        int armedAtTick = 0;
        int method = 0;
        int clicks = 0;
        bool waitingForClick = false;
        int clickRow = 0;
        int clickColumn = 0;
        std::string cellBefore;
        bool restoreCursor = false;
        POINT cursor = {};
        bool okPressed = false;
    };

    void Say(const std::string& message);
    void ReportStatus(const std::string& icao);
    void StepSetJob(int tick);
    void FinishSetJob(const std::string& message);

    SetJob m_job;
    int m_watchUntilTick = -1;
    int m_spyUntilTick = -1;
    int m_lastTick = 0;
    std::set<HWND> m_knownDialogs;
};
