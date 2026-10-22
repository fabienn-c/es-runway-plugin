#pragma once

#include "MetarFeed.h"
#include "Popup.h"
#include "RunwayApplier.h"

#include "Config.h"
#include "Metar.h"
#include "Selector.h"

#include <windows.h>

#include <EuroScopePlugIn.h>

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

class RunwayScreen;

// Recommends and sets active runways for every airport with runways in the loaded sector file,
// using the rules in LEXX.json.
//
//  - Startup: as soon as the sector and METARs are available, every airport with rules is set to its
//    recommended configuration, airports without rules (e.g. foreign ones) get all runways deselected,
//    and a summary pop-up lists what was set.
//  - Afterwards: each new METAR (and each day/night schedule change) is re-evaluated; if the
//    recommendation differs from what is active, a pop-up offers Accept / Decline.
//    DEL/GND/TWR only get suggestions for their own airport, APP/CTR for every airport.
//
// Commands: .rwy status [ICAO] | check | startup | accept | decline | airports | find | capture
class RunwayPlugin : public EuroScopePlugIn::CPlugIn
{
public:
    RunwayPlugin();
    ~RunwayPlugin() override;

    // The loaded plugin, or null once EuroScope has unloaded it.
    static RunwayPlugin* Instance();

    bool OnCompileCommand(const char* sCommandLine) override;
    void OnTimer(int Counter) override;
    void OnNewMetarReceived(const char* sStation, const char* sFullMetar) override;
    EuroScopePlugIn::CRadarScreen* OnRadarScreenCreated(const char* sDisplayName, bool NeedRadarContent,
                                                        bool GeoReferenced, bool CanBeSaved, bool CanBeCreated) override;

    // Used by RunwayScreen.
    const Popup* FrontPopup() const { return m_popups.empty() ? nullptr : &m_popups.front(); }
    size_t PopupCount() const { return m_popups.size(); }
    void OnPopupButton(const std::string& button);
    // First visible line of the front pop-up; reset when another pop-up comes to the front.
    int PopupScroll() const { return m_popups.empty() || m_popups.front().id != m_scrollPopupId ? 0 : m_popupScroll; }
    void SetPopupScroll(int line)
    {
        m_scrollPopupId = m_popups.empty() ? 0 : m_popups.front().id;
        m_popupScroll = line;
    }
    POINT PopupOrigin() const { return m_popupOrigin; }
    void SetPopupOrigin(POINT origin) { m_popupOrigin = origin; }
    void ForgetScreen(RunwayScreen* screen);

    void TickApplier();

private:
    enum class Trigger
    {
        Metar,
        Schedule,
        Manual
    };

    struct AirportState
    {
        core::Metar metar;
        bool haveMetar = false;
        std::string period;  // period of the last evaluation (e.g. "day", "night", "day+weekend")
    };

    void Say(const std::string& message);
    void LoadRules();
    bool PrepareSector();
    std::vector<const core::Airport*> SectorAirports() const;  // airports with rules in this sector
    void ReadRunwayHeadings();
    void HandleMetarLine(const std::string& line);

    // Startup: offered with an OK-only pop-up; OK opens the runway dialog and sets the runways.
    struct StartupPlan
    {
        std::vector<AirportTarget> targets;
        std::vector<std::string> issues;  // no METAR, too old, no rules...
        std::vector<std::string> set;     // one line per airport with its configuration
        int changed = 0;
        int alreadyActive = 0;
    };
    StartupPlan BuildStartupPlan();
    void OfferStartup();
    void RunStartup();
    void ShowStartupSummary(const StartupPlan& plan, const std::string& failure);
    bool HasFreshMetar(const std::string& icao);
    // Shows a suggestion when the recommendation differs from the active runways; returns true if it did.
    bool Evaluate(const std::string& icao, Trigger trigger);
    core::Decision DecideFor(const std::string& icao);
    void ActiveRunways(const std::string& icao, std::set<std::string>& arrivals, std::set<std::string>& departures);
    bool HasActiveRunways(const std::string& icao);
    std::string CurrentConfiguration(const core::Airport& airport);
    bool InScope(const std::string& icao);
    void ReportStatus(const std::string& onlyIcao);

    void ShowPopup(Popup popup);
    void RemovePopups(Popup::Kind kind, const std::string& icao = "");
    void Refresh();
    void Apply(std::vector<AirportTarget> targets, RunwayApplier::DoneCallback done);

    core::Rules m_rules;
    bool m_ready = false;                    // sector loaded and airports known
    std::vector<std::string> m_unconfigured; // sector airports without rules: runways cleared at startup
    std::map<std::string, core::RunwayHeadings> m_headings;
    std::map<std::string, AirportState> m_airports;

    MetarFeed m_feed;
    RunwayApplier m_applier;
    UINT_PTR m_applierTimer = 0;

    std::deque<Popup> m_popups;
    int m_nextPopupId = 1;
    POINT m_popupOrigin = {-1, -1};
    int m_popupScroll = 0;
    int m_scrollPopupId = 0;
    std::set<RunwayScreen*> m_screens;

    bool m_startupDone = false;
    int m_readyAtTick = -1;
    int m_lastTick = 0;
    long long m_lastScheduleMinute = -1;
};
