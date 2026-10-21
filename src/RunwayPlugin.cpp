#include "RunwayPlugin.h"

#include "Log.h"
#include "RunwayDialog.h"
#include "WindowProbe.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

using namespace EuroScopePlugIn;

namespace {

const char* const kName = "ES Runway Plugin";
const char* const kHandler = "RWY";
constexpr int kSetTimeoutTicks = 120;  // seconds to open the dialog after ".rwy set"
constexpr int kMaxClicks = 40;

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

// Sector names can carry padding or a trailing description ("LEBL Barcelona").
bool IsAirport(const char* name, const std::string& icao)
{
    const std::string n = Upper(Trim(Str(name)));
    return n.rfind(icao, 0) == 0 && (n.size() == icao.size() || n[icao.size()] == ' ');
}

std::vector<std::string> Tokens(const std::string& line)
{
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string t;
    while (ss >> t)
        out.push_back(t);
    return out;
}

std::set<std::string> RunwaySet(const std::string& csv)
{
    std::set<std::string> out;
    std::stringstream ss(Upper(csv));
    std::string item;
    while (std::getline(ss, item, ','))
        if (!item.empty() && item != "-")
            out.insert(item);
    return out;
}

std::string Join(const std::set<std::string>& items)
{
    std::string out;
    for (const auto& item : items)
        out += (out.empty() ? "" : ",") + item;
    return out.empty() ? "-" : out;
}

const char* FacilityName(int facility)
{
    switch (facility)
    {
    case 1: return "FSS";
    case 2: return "DEL";
    case 3: return "GND";
    case 4: return "TWR";
    case 5: return "APP/DEP";
    case 6: return "CTR";
    default: return "none/OBS";
    }
}

void LogAirportRows(const std::vector<rwydialog::Row>& rows, const std::string& icao)
{
    for (const auto& r : rows)
        if (r.airport == icao)
            logfile::Write("    row " + std::to_string(r.index) + ": " + r.runway + "  aptDEP=" + r.cells[1] +
                           " aptARR=" + r.cells[2] + " rwyDEP=" + r.cells[4] + " rwyARR=" + r.cells[5]);
}

}

RunwayPlugin::RunwayPlugin()
    : CPlugIn(COMPATIBILITY_CODE, kName, PLUGIN_VERSION, "Fabien Capmarty", "Proof of concept")
{
    logfile::Write(std::string("---- ") + kName + " " + PLUGIN_VERSION + " loaded");
}

RunwayPlugin::~RunwayPlugin()
{
    probe::StopSpy();
}

void RunwayPlugin::Say(const std::string& message)
{
    DisplayUserMessage(kHandler, kName, message.c_str(), true, true, false, false, false);
    logfile::Write(message);
}

void RunwayPlugin::ReportStatus(const std::string& icao)
{
    SelectActiveSectorfile();
    std::string airportState = "not found in sector file";
    for (CSectorElement e = SectorFileElementSelectFirst(SECTOR_ELEMENT_AIRPORT); e.IsValid();
         e = SectorFileElementSelectNext(e, SECTOR_ELEMENT_AIRPORT))
    {
        if (IsAirport(e.GetName(), icao))
            airportState = std::string("dep=") + (e.IsElementActive(true) ? "on" : "off") +
                           " arr=" + (e.IsElementActive(false) ? "on" : "off");
    }

    std::string deps, arrs;
    int matched = 0;
    for (CSectorElement e = SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY); e.IsValid();
         e = SectorFileElementSelectNext(e, SECTOR_ELEMENT_RUNWAY))
    {
        if (matched == 0 && Str(e.GetAirportName()).find(icao) != std::string::npos)
            logfile::Write("  runway element: name='" + Str(e.GetName()) + "' airport='" + Str(e.GetAirportName()) +
                           "' ends='" + Str(e.GetRunwayName(0)) + "'/'" + Str(e.GetRunwayName(1)) + "'");
        if (!IsAirport(e.GetAirportName(), icao))
            continue;
        ++matched;
        for (int i = 0; i < 2; ++i)
        {
            const std::string name = Trim(Str(e.GetRunwayName(i)));
            if (e.IsElementActive(true, i))
                deps += (deps.empty() ? "" : ",") + name;
            if (e.IsElementActive(false, i))
                arrs += (arrs.empty() ? "" : ",") + name;
        }
    }
    Say(icao + " (" + airportState + "): DEP " + (deps.empty() ? "-" : deps) + "  ARR " + (arrs.empty() ? "-" : arrs) +
        "  [" + std::to_string(matched) + " runways]");
}

bool RunwayPlugin::OnCompileCommand(const char* sCommandLine)
{
    const auto args = Tokens(sCommandLine);
    if (args.empty() || Upper(args[0]) != ".RWY")
        return false;
    const std::string cmd = args.size() > 1 ? Upper(args[1]) : "";

    if (cmd == "INFO")
    {
        CController me = ControllerMyself();
        Say("callsign=" + (me.IsValid() ? Str(me.GetCallsign()) : "-") +
            " facility=" + FacilityName(me.IsValid() ? me.GetFacility() : 0) +
            " connection=" + std::to_string(GetConnectionType()) +
            "  log=" + logfile::PluginDirectory() + "\\ESRunwayPlugin.log");
        return true;
    }
    if (cmd == "STATUS" && args.size() == 3)
    {
        ReportStatus(Upper(args[2]));
        return true;
    }
    if (cmd == "SET" && args.size() == 5)
    {
        m_job = SetJob{};
        m_job.active = true;
        m_job.icao = Upper(args[2]);
        m_job.departures = RunwaySet(args[3]);
        m_job.arrivals = RunwaySet(args[4]);
        m_job.armedAtTick = m_lastTick;
        rwydialog::Opener opener;
        Say("setting " + m_job.icao + " DEP " + Join(m_job.departures) + " ARR " + Join(m_job.arrivals) +
            (rwydialog::LoadOpener(opener) ? ": opening the runway dialog, don't touch the mouse"
                                           : ": open the Active airport/runway dialog now and don't click anything"));
        return true;
    }
    if (cmd == "OPEN")
    {
        rwydialog::Opener opener;
        if (!rwydialog::LoadOpener(opener))
        {
            Say("no recorded opener yet: run .rwy spy and open the runway dialog once");
            return true;
        }
        m_job = SetJob{};
        m_job.active = true;
        m_job.openOnly = true;
        m_job.armedAtTick = m_lastTick;
        Say("trying to open the runway dialog, don't touch the mouse");
        return true;
    }
    if (cmd == "SPY")
    {
        probe::StartSpy();
        m_spyUntilTick = m_lastTick + 60;
        Say("spying for 60 s: open the Active airport/runway dialog by clicking its icon, then close it");
        return true;
    }
    if (cmd == "WINDOWS")
    {
        probe::DumpProcessWindows(true);
        Say("window list written to the log");
        return true;
    }
    if (cmd == "WATCH")
    {
        m_knownDialogs = probe::CurrentDialogs();
        m_watchUntilTick = m_lastTick + 60;
        Say("watching for 60 s: open the Active airport/runway dialog now");
        return true;
    }

    Say("usage: .rwy info | status <ICAO> | set <ICAO> <DEP,..> <ARR,..> | open | spy | windows | watch");
    return true;
}

void RunwayPlugin::FinishSetJob(const std::string& message)
{
    if (m_job.restoreCursor)
        SetCursorPos(m_job.cursor.x, m_job.cursor.y);
    const std::string icao = m_job.icao;
    m_job = SetJob{};
    Say(message);
    if (!icao.empty())
        ReportStatus(icao);
}

void RunwayPlugin::StepSetJob(int tick)
{
    if (m_job.restoreCursor)
    {
        SetCursorPos(m_job.cursor.x, m_job.cursor.y);
        m_job.restoreCursor = false;
    }

    const HWND dialog = rwydialog::Find();
    if (!dialog)
    {
        if (m_job.okPressed)
        {
            FinishSetJob(m_job.openOnly ? "dialog closed again" : "dialog closed by the plugin. EuroScope now reports:");
            return;
        }
        rwydialog::Opener opener;
        if (m_job.openAttempts < static_cast<int>(rwydialog::ClickMethod::Count) && rwydialog::LoadOpener(opener))
        {
            const auto method = static_cast<rwydialog::ClickMethod>(m_job.openAttempts++);
            if (m_job.openAttempts > 1)
                Say(std::string("the dialog did not open, trying again with ") + rwydialog::MethodName(method));
            if (rwydialog::ReplayOpener(opener, method, m_job.cursor))
                m_job.restoreCursor = true;
            else
                Say("could not find the EuroScope window that opens the dialog");
            return;
        }
        if (m_job.openOnly)
            FinishSetJob("could not open the runway dialog. Please send me the log.");
        else if (tick - m_job.armedAtTick > kSetTimeoutTicks)
            FinishSetJob("gave up: the runway dialog was not opened within 2 minutes");
        return;
    }
    if (m_job.okPressed)
        return;  // waiting for the dialog to close
    if (m_job.openOnly)
    {
        Say(m_job.openAttempts > 0
                ? std::string("the plugin opened the runway dialog with ") +
                      rwydialog::MethodName(static_cast<rwydialog::ClickMethod>(m_job.openAttempts - 1)) + ", closing it"
                : std::string("the runway dialog is open, closing it"));
        rwydialog::PressCancel(dialog);
        m_job.okPressed = true;
        return;
    }

    const HWND list = rwydialog::List(dialog);
    const auto rows = rwydialog::ReadRows(list);

    // Check the result of the previous click.
    if (m_job.waitingForClick)
    {
        m_job.waitingForClick = false;
        const std::string after = rows.size() > static_cast<size_t>(m_job.clickRow)
                                      ? rows[m_job.clickRow].cells[m_job.clickColumn]
                                      : "";
        const auto method = static_cast<rwydialog::ClickMethod>(m_job.method);
        logfile::Write("  click row " + std::to_string(m_job.clickRow) + " col " + std::to_string(m_job.clickColumn) +
                       " with " + rwydialog::MethodName(method) + ": " + m_job.cellBefore + " -> " + after);
        if (after == m_job.cellBefore)
        {
            Say(std::string("click method '") + rwydialog::MethodName(method) + "' did nothing, trying the next one");
            if (++m_job.method >= static_cast<int>(rwydialog::ClickMethod::Count))
            {
                FinishSetJob("no click method worked - leaving the dialog to you. Please send me the log.");
                return;
            }
        }
    }

    // Find the next runway cell that differs from the wanted configuration. The airport
    // columns are left alone: another plugin activates airports when the controller connects.
    bool airportFound = false;
    for (const auto& r : rows)
    {
        if (r.airport != m_job.icao || r.runway.empty())
            continue;
        airportFound = true;
        const std::pair<int, bool> wanted[] = {
            {rwydialog::kColRunwayDep, m_job.departures.count(r.runway) > 0},
            {rwydialog::kColRunwayArr, m_job.arrivals.count(r.runway) > 0},
        };
        for (const auto& [column, on] : wanted)
        {
            if (rwydialog::IsTicked(r.cells[column]) == on)
                continue;
            if (++m_job.clicks > kMaxClicks)
            {
                LogAirportRows(rows, m_job.icao);
                FinishSetJob("too many clicks without reaching the configuration - stopped. Please send me the log.");
                return;
            }
            const auto method = static_cast<rwydialog::ClickMethod>(m_job.method);
            m_job.waitingForClick = true;
            m_job.clickRow = r.index;
            m_job.clickColumn = column;
            m_job.cellBefore = r.cells[column];
            m_job.cursor = rwydialog::Click(dialog, list, r.index, column, method);
            m_job.restoreCursor = true;
            return;  // check the effect on the next tick
        }
    }

    if (!airportFound)
    {
        rwydialog::PressCancel(dialog);
        FinishSetJob(m_job.icao + " is not in the runway dialog");
        return;
    }
    logfile::Write("  configuration reached, pressing OK:");
    LogAirportRows(rows, m_job.icao);
    rwydialog::PressOk(dialog);
    m_job.okPressed = true;
}

void RunwayPlugin::OnTimer(int Counter)
{
    m_lastTick = Counter;

    if (m_job.active)
        StepSetJob(Counter);

    if (m_spyUntilTick >= 0 && Counter >= m_spyUntilTick)
    {
        m_spyUntilTick = -1;
        probe::StopSpy();
        Say("spy finished");
    }

    if (m_watchUntilTick >= 0)
    {
        for (HWND dialog : probe::CurrentDialogs())
        {
            if (m_knownDialogs.insert(dialog).second)
            {
                logfile::Write("=== new dialog appeared:");
                probe::DumpWindowTree(dialog);
                Say("logged a new dialog");
            }
        }
        if (Counter >= m_watchUntilTick)
        {
            m_watchUntilTick = -1;
            Say("watch finished");
        }
    }
}

void RunwayPlugin::OnAirportRunwayActivityChanged()
{
    logfile::Write("EuroScope reported a runway activity change");
}
