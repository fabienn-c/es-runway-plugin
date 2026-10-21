#include "RunwayPlugin.h"

#include "Log.h"
#include "RunwayIcon.h"
#include "RunwayScreen.h"
#include "ScreenCapture.h"

#include "LocalTime.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <sstream>

using namespace EuroScopePlugIn;

namespace {

const char* const kName = "ES Runway Plugin";
const char* const kHandler = "RWY";
constexpr int kStartupMetarWaitTicks = 60;  // seconds to wait for all METARs before the startup assignment
constexpr int kApplierTimerMs = 100;
constexpr int kTopBarHeight = 80;
constexpr double kPi = 3.14159265358979323846;

RunwayPlugin* g_instance = nullptr;

// The SDK may hand back null pointers when nothing is loaded yet.
std::string Str(const char* s)
{
    return s ? s : "";
}

std::string Upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::string Trim(const std::string& s)
{
    const auto first = s.find_first_not_of(" \t");
    return first == std::string::npos ? std::string() : s.substr(first, s.find_last_not_of(" \t") - first + 1);
}

// Sector airport names can carry padding or a description ("LEBL Barcelona"): keep the ICAO code.
std::string Icao(const char* name)
{
    return Upper(Trim(Str(name)).substr(0, 4));
}

std::vector<std::string> Tokens(const std::string& line)
{
    std::vector<std::string> out;
    std::istringstream ss(line);
    for (std::string t; ss >> t;)
        out.push_back(t);
    return out;
}

// Initial great-circle bearing in degrees true.
double TrueBearing(const CPosition& from, const CPosition& to)
{
    const double rad = kPi / 180.0;
    const double phi1 = from.m_Latitude * rad, phi2 = to.m_Latitude * rad;
    const double dLambda = (to.m_Longitude - from.m_Longitude) * rad;
    const double y = std::sin(dLambda) * std::cos(phi2);
    const double x = std::cos(phi1) * std::sin(phi2) - std::sin(phi1) * std::cos(phi2) * std::cos(dLambda);
    return std::fmod(std::atan2(y, x) / rad + 360.0, 360.0);
}

const core::Configuration* FindConfiguration(const core::Airport& airport, const std::string& name)
{
    for (const auto* c : airport.AllConfigurations())
        if (c->name == name)
            return c;
    return nullptr;
}

AirportTarget TargetFor(const std::string& icao, const core::Configuration& c)
{
    return AirportTarget{icao, std::set<std::string>(c.arrivals.begin(), c.arrivals.end()),
                         std::set<std::string>(c.departures.begin(), c.departures.end())};
}

void CALLBACK ApplierTimerProc(HWND, UINT, UINT_PTR, DWORD)
{
    if (g_instance)
        g_instance->TickApplier();
}

}

RunwayPlugin* RunwayPlugin::Instance()
{
    return g_instance;
}

RunwayPlugin::RunwayPlugin()
    : CPlugIn(COMPATIBILITY_CODE, kName, PLUGIN_VERSION, "Fabien Capmarty", "VATSIM Spain")
{
    g_instance = this;
    logfile::Write(std::string("---- ") + kName + " " + PLUGIN_VERSION + " loaded");
    m_applier.SetNeedUserCallback([this](bool needUser) {
        if (!needUser)
        {
            RemovePopups(Popup::Kind::Info);
            return;
        }
        Popup p;
        p.kind = Popup::Kind::Info;
        p.title = "Please open the runway dialog";
        p.lines = {"The runway icon could not be found in the top bar, so the plugin will not click anything.",
                   "Open the Active airport/runway dialog yourself: the plugin will then set the runways and press OK."};
        ShowPopup(p);
    });
    LoadConfigs();
}

RunwayPlugin::~RunwayPlugin()
{
    m_feed.Stop();
    if (m_applierTimer)
        KillTimer(nullptr, m_applierTimer);
    g_instance = nullptr;
}

void RunwayPlugin::Say(const std::string& message)
{
    DisplayUserMessage(kHandler, kName, message.c_str(), true, true, false, false, false);
    logfile::Write(message);
}

// ---------------------------------------------------------------------------------------------
// Setup: configs, FIR, runway headings

void RunwayPlugin::LoadConfigs()
{
    namespace fs = std::filesystem;
    const fs::path dir = logfile::PluginDirectory();
    std::error_code ec;
    for (const auto& folder : {dir, dir / "config"})
    {
        for (const auto& entry : fs::directory_iterator(folder, ec))
        {
            if (entry.path().extension() != ".json")
                continue;
            core::Fir fir;
            std::string error;
            if (core::LoadFir(entry.path().string(), fir, error))
            {
                logfile::Write("loaded " + fir.name + " (" + std::to_string(fir.airports.size()) + " airports) from " +
                               entry.path().string());
                m_firs.push_back(std::move(fir));
            }
            else
                Say("config error: " + error);
        }
    }
    if (m_firs.empty())
        Say("no runway configuration files found next to the DLL (expected e.g. LECB.json)");
}

bool RunwayPlugin::DetectFir(const std::string& forced)
{
    SelectActiveSectorfile();
    std::set<std::string> sectorAirports;
    for (CSectorElement e = SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY); e.IsValid();
         e = SectorFileElementSelectNext(e, SECTOR_ELEMENT_RUNWAY))
        sectorAirports.insert(Icao(e.GetAirportName()));
    if (sectorAirports.empty())
        return false;  // sector not loaded yet

    // The FIR whose airports are most completely present in the sector file. Neighbouring
    // sectors contain some airports of other FIRs, never all of them.
    const core::Fir* best = nullptr;
    double bestScore = 0;
    for (const auto& fir : m_firs)
    {
        int present = 0;
        for (const auto& a : fir.airports)
            present += sectorAirports.count(a.icao) ? 1 : 0;
        const double score = fir.airports.empty() ? 0 : static_cast<double>(present) / fir.airports.size() + present * 1e-4;
        if (!forced.empty() ? Upper(forced) == fir.name : score > bestScore)
        {
            best = &fir;
            bestScore = score;
        }
    }
    if (!best || (forced.empty() && bestScore < 0.5))
    {
        if (!forced.empty())
            Say("no configuration loaded for FIR " + Upper(forced));
        return false;
    }

    m_fir = best;
    m_airports.clear();
    for (const auto& a : m_fir->airports)
        m_airports[a.icao] = AirportState{};
    ReadRunwayHeadings();
    std::vector<std::string> stations;
    for (const auto& a : m_fir->airports)
        stations.push_back(a.icao);
    m_feed.Start(stations);
    m_detectedAtTick = m_lastTick;
    m_startupDone = false;
    Say(m_fir->name + ": " + std::to_string(m_fir->airports.size()) +
        " airports, downloading METARs to set the runways");
    return true;
}

void RunwayPlugin::ReadRunwayHeadings()
{
    m_headings.clear();
    SelectActiveSectorfile();
    for (CSectorElement e = SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY); e.IsValid();
         e = SectorFileElementSelectNext(e, SECTOR_ELEMENT_RUNWAY))
    {
        const std::string icao = Icao(e.GetAirportName());
        if (!m_fir || !m_fir->Find(icao))
            continue;
        CPosition start, end;
        if (!e.GetPosition(&start, 0) || !e.GetPosition(&end, 1))
            continue;
        const double heading = TrueBearing(start, end);
        m_headings[icao][Trim(Str(e.GetRunwayName(0)))] = heading;
        m_headings[icao][Trim(Str(e.GetRunwayName(1)))] = std::fmod(heading + 180.0, 360.0);
    }
}

// ---------------------------------------------------------------------------------------------
// METARs and decisions

void RunwayPlugin::OnNewMetarReceived(const char* sStation, const char* sFullMetar)
{
    std::string line = Trim(Str(sFullMetar));
    const std::string station = Upper(Str(sStation));
    if (line.rfind(station, 0) != 0)
        line = station + " " + line;
    HandleMetarLine(line);
}

void RunwayPlugin::HandleMetarLine(const std::string& line)
{
    core::Metar metar;
    if (!core::ParseMetar(line, metar))
        return;
    const auto it = m_airports.find(metar.station);
    if (it == m_airports.end())
        return;
    AirportState& state = it->second;
    if (state.haveMetar && state.metar.observation == metar.observation)
        return;  // same report again
    state.metar = metar;
    state.haveMetar = true;
    logfile::Write("METAR " + line);
    if (m_startupDone && InScope(metar.station))
        Evaluate(metar.station, Trigger::Metar);
}

core::Decision RunwayPlugin::DecideFor(const std::string& icao)
{
    const core::Airport* airport = m_fir->Find(icao);
    const int minute = core::LocalMinuteOfDay(m_fir->timezone, std::time(nullptr));
    return core::Decide(*m_fir, *airport, m_airports[icao].metar, m_headings[icao], minute,
                        CurrentConfiguration(*airport));
}

std::string RunwayPlugin::CurrentConfiguration(const core::Airport& airport)
{
    std::set<std::string> arrivals, departures;
    SelectActiveSectorfile();
    for (CSectorElement e = SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY); e.IsValid();
         e = SectorFileElementSelectNext(e, SECTOR_ELEMENT_RUNWAY))
    {
        if (Icao(e.GetAirportName()) != airport.icao)
            continue;
        for (int i = 0; i < 2; ++i)
        {
            const std::string name = Trim(Str(e.GetRunwayName(i)));
            if (e.IsElementActive(false, i))
                arrivals.insert(name);
            if (e.IsElementActive(true, i))
                departures.insert(name);
        }
    }
    // Exact match only: extra active runways mean the configuration still has to be applied,
    // which also unticks every runway that isn't part of it.
    return airport.MatchActive(arrivals, departures);
}

bool RunwayPlugin::InScope(const std::string& icao)
{
    if (GetConnectionType() == CONNECTION_TYPE_NO)
        return true;
    CController me = ControllerMyself();
    if (!me.IsValid())
        return true;
    const int facility = me.GetFacility();
    if (facility >= 2 && facility <= 4)  // DEL, GND, TWR: own airport only
    {
        const std::string own = Upper(Str(me.GetCallsign()).substr(0, 4));
        if (m_fir->Find(own))
            return icao == own;
    }
    return true;
}

bool RunwayPlugin::HasFreshMetar(const std::string& icao)
{
    const AirportState& state = m_airports[icao];
    if (!state.haveMetar)
        return false;
    const int age = core::MetarAgeMinutes(state.metar, std::time(nullptr));
    return age >= 0 && age <= core::kMaxMetarAgeMinutes;
}

bool RunwayPlugin::Evaluate(const std::string& icao, Trigger trigger)
{
    const core::Airport* airport = m_fir->Find(icao);
    AirportState& state = m_airports[icao];
    if (!airport || !HasFreshMetar(icao))
        return false;

    const core::Decision d = DecideFor(icao);
    state.scheduleName = d.schedule ? d.schedule->name : "";
    if (!d.configuration)
    {
        logfile::Write(icao + ": no recommendation (" + d.reason + ")");
        return false;
    }
    const std::string current = CurrentConfiguration(*airport);
    if (d.configuration->name == current)
    {
        RemovePopups(Popup::Kind::Suggestion, icao);
        return false;
    }

    Popup p;
    p.kind = Popup::Kind::Suggestion;
    p.icao = icao;
    p.configuration = d.configuration->name;
    p.title = icao + ": runway change suggested";
    const core::Configuration* active = FindConfiguration(*airport, current);
    p.lines.push_back("Active:  " + (active ? active->Describe() : std::string("runways not matching any configuration")));
    p.lines.push_back("Suggested:  " + d.configuration->Describe());
    p.lines.push_back("Reason:  " + d.reason);
    if (trigger == Trigger::Schedule && d.schedule && !d.schedule->IsAllDay())
    {
        const std::string start = core::FormatClock(d.schedule->fromMinute);
        if (d.schedule->name == "night")
            p.lines.push_back("The night configuration period started at " + start +
                              " LT. The current configuration can be kept while inbound and outbound traffic is "
                              "heavy; once the traffic load is back to normal, change to the night configuration.");
        else
            p.lines.push_back("The " + d.schedule->name + " configuration period started at " + start + " LT.");
    }
    p.lines.push_back("METAR:  " + state.metar.raw);
    ShowPopup(p);
    logfile::Write(icao + ": suggested " + d.configuration->Describe() + " - " + d.reason);
    return true;
}

void RunwayPlugin::RunStartup()
{
    m_startupDone = true;
    std::vector<AirportTarget> targets;
    std::vector<std::string> lines;
    for (const auto& airport : m_fir->airports)
    {
        AirportState& state = m_airports[airport.icao];
        if (!state.haveMetar)
        {
            lines.push_back(airport.icao + ": no METAR available, not set");
            continue;
        }
        if (!HasFreshMetar(airport.icao))
        {
            // No current weather (e.g. closed for the night): leave no runway selected.
            targets.push_back(AirportTarget{airport.icao, {}, {}});
            lines.push_back(airport.icao + ": METAR too old (" + state.metar.observation + "), no runways selected" +
                            (airport.afis ? ". AFIS aerodrome: pilots decide which runway to use" : ""));
            continue;
        }
        const core::Decision d = DecideFor(airport.icao);
        state.scheduleName = d.schedule ? d.schedule->name : "";
        if (!d.configuration)
        {
            lines.push_back(airport.icao + ": not set (" + d.reason + ")");
            continue;
        }
        std::string line = airport.icao + ": " + d.configuration->Describe() + "  -  wind " + state.metar.WindText();
        if (d.configuration->name == CurrentConfiguration(airport))
            line += " (already active)";
        else
            targets.push_back(TargetFor(airport.icao, *d.configuration));
        lines.push_back(line);
    }

    const std::string title = m_fir->name + ": runways set automatically";
    if (targets.empty())
    {
        Popup p;
        p.title = title;
        p.lines = lines;
        ShowPopup(p);
        return;
    }
    Apply(targets, [this, lines, title](bool ok, const std::string& message) {
        Popup p;
        p.title = title;
        p.lines = lines;
        if (!ok)
            p.lines.push_back("Not completed: " + message + ". Check the runways manually.");
        p.lines.push_back("You can change any of them in the runway dialog.");
        ShowPopup(p);
    });
}

void RunwayPlugin::ReportStatus(const std::string& onlyIcao)
{
    if (!m_fir)
    {
        Say("no FIR detected yet");
        return;
    }
    for (const auto& airport : m_fir->airports)
    {
        if (!onlyIcao.empty() && airport.icao != onlyIcao)
            continue;
        const AirportState& state = m_airports[airport.icao];
        const std::string current = CurrentConfiguration(airport);
        std::string line = airport.icao + ": active " + (current.empty() ? "-" : current);
        if (!state.haveMetar)
            line += " | no METAR yet";
        else if (!HasFreshMetar(airport.icao))
            line += " | METAR too old (" + state.metar.observation + "), no recommendation";
        else
        {
            const auto d = DecideFor(airport.icao);
            line += " | recommended " + (d.configuration ? d.configuration->name : "-") + " | " + d.reason;
        }
        Say(line);
    }
}

// ---------------------------------------------------------------------------------------------
// Timer and commands

void RunwayPlugin::OnTimer(int Counter)
{
    m_lastTick = Counter;
    if (!m_fir)
    {
        if (Counter % 5 == 0 && !m_firs.empty())
            DetectFir();
        return;
    }

    for (const auto& line : m_feed.TakeLines())
        HandleMetarLine(line);

    if (!m_startupDone && !m_applier.Busy())
    {
        bool all = true, any = false;
        for (const auto& [icao, state] : m_airports)
        {
            all = all && state.haveMetar;
            any = any || HasFreshMetar(icao);
        }
        if (any && (all || Counter - m_detectedAtTick >= kStartupMetarWaitTicks))
            RunStartup();
        return;
    }

    // Day/night schedule changes (e.g. LEBL at 23:00 and 07:00 local).
    const int minute = core::LocalMinuteOfDay(m_fir->timezone, std::time(nullptr));
    if (m_startupDone && minute != m_lastScheduleMinute)
    {
        m_lastScheduleMinute = minute;
        for (const auto& airport : m_fir->airports)
        {
            const AirportState& state = m_airports[airport.icao];
            const core::Schedule* schedule = airport.ScheduleAt(minute);
            if (HasFreshMetar(airport.icao) && schedule && !state.scheduleName.empty() && schedule->name != state.scheduleName &&
                InScope(airport.icao))
                Evaluate(airport.icao, Trigger::Schedule);
        }
    }
}

bool RunwayPlugin::OnCompileCommand(const char* sCommandLine)
{
    const auto args = Tokens(Str(sCommandLine));
    if (args.empty() || Upper(args[0]) != ".RWY")
        return false;
    const std::string cmd = args.size() > 1 ? Upper(args[1]) : "";

    if (cmd == "STATUS")
        ReportStatus(args.size() > 2 ? Upper(args[2]) : "");
    else if (cmd == "CHECK")
    {
        if (!m_fir)
            Say("no FIR detected yet");
        else
        {
            m_feed.FetchNow();
            int suggested = 0, set = 0;
            std::string stale;
            for (const auto& airport : m_fir->airports)
            {
                if (!InScope(airport.icao))
                    continue;
                if (!HasFreshMetar(airport.icao))
                    stale += " " + airport.icao;
                else if (Evaluate(airport.icao, Trigger::Manual))
                    ++suggested;
                else
                    ++set;
            }
            Say("check: " + std::to_string(set) + " airports already set, " + std::to_string(suggested) +
                " suggestion(s)" + (stale.empty() ? "" : ", no current METAR:" + stale));
        }
    }
    else if (cmd == "STARTUP")
    {
        if (!m_fir)
            Say("no FIR detected yet");
        else if (m_applier.Busy())
            Say("still applying runways, try again in a moment");
        else
            RunStartup();
    }
    else if (cmd == "ACCEPT" || cmd == "DECLINE")
    {
        if (FrontPopup() && FrontPopup()->kind == Popup::Kind::Suggestion)
            OnPopupButton(cmd == "ACCEPT" ? "accept" : "decline");
        else
            Say("no runway suggestion pending");
    }
    else if (cmd == "FIR")
    {
        if (args.size() > 2)
            DetectFir(args[2]);
        else
        {
            std::string loaded;
            for (const auto& fir : m_firs)
                loaded += " " + fir.name;
            Say("FIR: " + (m_fir ? m_fir->name : std::string("not detected")) + " | loaded:" + loaded);
        }
    }
    else if (cmd == "FIND")
    {
        const auto icon = rwyicon::Locate();
        Say(std::string(icon.found ? "runway icon found at " : "runway icon NOT found, best guess ") +
            std::to_string(icon.center.x) + "," + std::to_string(icon.center.y) + " (" + std::to_string(icon.mismatches) +
            " of " + std::to_string(icon.pixels) + " pixels differ)");
    }
    else if (cmd == "CAPTURE")
    {
        const HWND view = rwydialog::FindEuroScopeControl(1, "AfxWnd140");
        RECT client = {};
        capture::Image bar;
        if (view)
            GetClientRect(view, &client);
        const std::string path = logfile::PluginDirectory() + "\\ESRunwayPlugin-topbar.bmp";
        if (view && capture::Grab(view, RECT{0, 0, client.right, std::min<LONG>(kTopBarHeight, client.bottom)}, bar) &&
            capture::SaveBmp(path, bar))
            Say("saved the top bar to " + path);
        else
            Say("could not capture the top bar: is EuroScope in front and not minimised?");
    }
    else
        Say("usage: .rwy status [ICAO] | check | startup | accept | decline | fir [NAME] | find | capture");
    return true;
}

// ---------------------------------------------------------------------------------------------
// Pop-ups, radar screens and applying

EuroScopePlugIn::CRadarScreen* RunwayPlugin::OnRadarScreenCreated(const char*, bool, bool, bool, bool)
{
    auto* screen = new RunwayScreen();
    m_screens.insert(screen);
    return screen;
}

void RunwayPlugin::ForgetScreen(RunwayScreen* screen)
{
    m_screens.erase(screen);
}

void RunwayPlugin::Refresh()
{
    for (auto* screen : m_screens)
        screen->RequestRefresh();
}

void RunwayPlugin::ShowPopup(Popup popup)
{
    popup.id = m_nextPopupId++;
    if (popup.kind == Popup::Kind::Suggestion)
        RemovePopups(Popup::Kind::Suggestion, popup.icao);
    if (popup.kind == Popup::Kind::Info)
        m_popups.push_front(popup);
    else
        m_popups.push_back(popup);
    // Also in the chat, for controllers who haven't allowed the plugin to draw on the radar screen.
    std::string text = popup.title;
    for (const auto& line : popup.lines)
        if (line.rfind("METAR:", 0) != 0)
            text += " | " + line;
    if (popup.kind == Popup::Kind::Suggestion)
        text += " | .rwy accept / .rwy decline";
    Say(text);
    Refresh();
}

void RunwayPlugin::RemovePopups(Popup::Kind kind, const std::string& icao)
{
    const auto before = m_popups.size();
    m_popups.erase(std::remove_if(m_popups.begin(), m_popups.end(),
                                  [&](const Popup& p) { return p.kind == kind && (icao.empty() || p.icao == icao); }),
                   m_popups.end());
    if (m_popups.size() != before)
        Refresh();
}

void RunwayPlugin::OnPopupButton(const std::string& button)
{
    if (m_popups.empty())
        return;
    const Popup popup = m_popups.front();

    if (button == "accept")
    {
        const core::Airport* airport = m_fir ? m_fir->Find(popup.icao) : nullptr;
        const core::Configuration* c = airport ? FindConfiguration(*airport, popup.configuration) : nullptr;
        if (!c)
            return;
        if (m_applier.Busy())
        {
            Say("still applying runways, try again in a moment");
            return;
        }
        m_popups.pop_front();
        const std::string icao = popup.icao, description = c->Describe();
        Apply({TargetFor(icao, *c)}, [this, icao, description](bool ok, const std::string& message) {
            if (ok)
                Say(icao + ": " + description + " set");
            else
            {
                Popup p;
                p.title = icao + ": runways not set";
                p.lines = {message + ". Please set " + description + " manually."};
                ShowPopup(p);
            }
        });
    }
    else
    {
        m_popups.pop_front();
        if (button == "decline")
            logfile::Write(popup.icao + ": suggestion " + popup.configuration + " declined");
    }
    Refresh();
}

void RunwayPlugin::Apply(std::vector<AirportTarget> targets, RunwayApplier::DoneCallback done)
{
    if (!m_applier.Start(std::move(targets), std::move(done)))
    {
        Say("still applying runways, try again in a moment");
        return;
    }
    if (!m_applierTimer)
        m_applierTimer = SetTimer(nullptr, 0, kApplierTimerMs, ApplierTimerProc);
}

void RunwayPlugin::TickApplier()
{
    m_applier.Tick();
    if (!m_applier.Busy() && m_applierTimer)
    {
        KillTimer(nullptr, m_applierTimer);
        m_applierTimer = 0;
    }
}
