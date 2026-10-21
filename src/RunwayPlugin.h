#pragma once

#include <windows.h>

#include <EuroScopePlugIn.h>

#include <set>
#include <string>

// Proof of concept: can a plugin change EuroScope's active runways?
//
// Commands (type them in the EuroScope command line):
//   .rwy info                          controller, facility, sector file and .rwy path
//   .rwy status <ICAO>                 active runways as EuroScope reports them
//   .rwy write <ICAO> <DEP> <ARR>      write the .rwy file now (e.g. .rwy write LEBL 24R 24L,02)
//   .rwy arm <ICAO> <DEP> <ARR>        write the .rwy file at the next EuroScope start
//   .rwy windows                       log all EuroScope windows to ESRunwayPlugin.log
//   .rwy watch                         for 60 s, log every dialog that opens (open the runway dialog)
class RunwayPlugin : public EuroScopePlugIn::CPlugIn
{
public:
    RunwayPlugin();

    bool OnCompileCommand(const char* sCommandLine) override;
    void OnTimer(int Counter) override;
    void OnAirportRunwayActivityChanged() override;

private:
    void Say(const std::string& message);
    std::string RwyPath();
    void ReportStatus(const std::string& icao);
    void ApplyPendingAtStartup();

    std::string m_checkIcao;     // airport to report on after a startup write
    int m_checkAtTick = -1;
    int m_watchUntilTick = -1;
    int m_lastTick = 0;
    std::set<HWND> m_knownDialogs;
};
