#include "RunwayPlugin.h"

#include "Log.h"
#include "RwyFile.h"
#include "WindowProbe.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

using namespace EuroScopePlugIn;

namespace {

const char* const kName = "ES Runway Plugin";
const char* const kHandler = "RWY";

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

std::vector<std::string> Tokens(const std::string& line)
{
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string t;
    while (ss >> t)
        out.push_back(t);
    return out;
}

std::string PendingPath()
{
    return logfile::PluginDirectory() + "\\ESRunwayPlugin.pending";
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

}

RunwayPlugin::RunwayPlugin()
    : CPlugIn(COMPATIBILITY_CODE, kName, PLUGIN_VERSION, "VATSIM Spain", "Proof of concept")
{
    logfile::Write(std::string("---- ") + kName + " " + PLUGIN_VERSION + " loaded");
    ApplyPendingAtStartup();
}

void RunwayPlugin::Say(const std::string& message)
{
    DisplayUserMessage(kHandler, kName, message.c_str(), true, true, false, false, false);
    logfile::Write(message);
}

std::string RunwayPlugin::RwyPath()
{
    return rwyfile::PathForSector(Str(ControllerMyself().GetSectorFileName()));
}

void RunwayPlugin::ApplyPendingAtStartup()
{
    std::ifstream in(PendingPath());
    if (!in)
        return;
    rwyfile::AirportConfig config;
    std::string path, dep, arr;
    std::getline(in, path);
    std::getline(in, config.icao);
    std::getline(in, dep);
    std::getline(in, arr);
    in.close();
    std::remove(PendingPath().c_str());

    config.departures = rwyfile::SplitList(dep);
    config.arrivals = rwyfile::SplitList(arr);
    std::string error;
    if (rwyfile::Apply(path, config, error))
    {
        logfile::Write("startup: wrote " + config.icao + " DEP " + dep + " ARR " + arr + " into " + path);
        m_checkIcao = config.icao;
        m_checkAtTick = 10;  // report once the sector file has had time to load
    }
    else
    {
        logfile::Write("startup: " + error);
    }
}

void RunwayPlugin::ReportStatus(const std::string& icao)
{
    SelectActiveSectorfile();
    int airports = 0;
    std::string airportState = "not found in sector file";
    for (CSectorElement e = SectorFileElementSelectFirst(SECTOR_ELEMENT_AIRPORT); e.IsValid();
         e = SectorFileElementSelectNext(e, SECTOR_ELEMENT_AIRPORT))
    {
        ++airports;
        if (Upper(Str(e.GetName())) == icao)
            airportState = std::string("dep=") + (e.IsElementActive(true) ? "on" : "off") +
                           " arr=" + (e.IsElementActive(false) ? "on" : "off");
    }

    std::string deps, arrs;
    for (CSectorElement e = SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY); e.IsValid();
         e = SectorFileElementSelectNext(e, SECTOR_ELEMENT_RUNWAY))
    {
        if (Upper(Str(e.GetAirportName())) != icao)
            continue;
        for (int i = 0; i < 2; ++i)
        {
            const std::string name = Str(e.GetRunwayName(i));
            if (e.IsElementActive(true, i))
                deps += (deps.empty() ? "" : ",") + name;
            if (e.IsElementActive(false, i))
                arrs += (arrs.empty() ? "" : ",") + name;
        }
    }
    Say(icao + " (" + airportState + "): DEP " + (deps.empty() ? "-" : deps) + "  ARR " + (arrs.empty() ? "-" : arrs) +
        "  [" + std::to_string(airports) + " airports in sector]");
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
        Say(std::string("callsign=") + (me.IsValid() ? Str(me.GetCallsign()) : "-") +
            " facility=" + FacilityName(me.IsValid() ? me.GetFacility() : 0) +
            " connection=" + std::to_string(GetConnectionType()));
        Say("sector file=" + Str(me.GetSectorFileName()));
        Say("rwy file=" + RwyPath() + "  log=" + logfile::PluginDirectory() + "\\ESRunwayPlugin.log");
        return true;
    }
    if (cmd == "STATUS" && args.size() == 3)
    {
        ReportStatus(Upper(args[2]));
        return true;
    }
    if ((cmd == "WRITE" || cmd == "ARM") && args.size() == 5)
    {
        const std::string path = RwyPath();
        if (path.empty())
        {
            Say(std::string("cannot find the .rwy path, sector file is '") + Str(ControllerMyself().GetSectorFileName()) + "'");
            return true;
        }
        rwyfile::AirportConfig config{Upper(args[2]), rwyfile::SplitList(Upper(args[3])),
                                      rwyfile::SplitList(Upper(args[4]))};
        if (cmd == "WRITE")
        {
            std::string error;
            if (rwyfile::Apply(path, config, error))
                Say("wrote " + config.icao + " to " + path + " - now check the runway dialog / .rwy status " + config.icao);
            else
                Say(error);
        }
        else
        {
            std::ofstream out(PendingPath(), std::ios::trunc);
            out << path << "\n"
                << config.icao << "\n"
                << rwyfile::JoinList(config.departures) << "\n"
                << rwyfile::JoinList(config.arrivals) << "\n";
            Say("armed: " + config.icao + " will be written at the next EuroScope start. Close EuroScope WITHOUT "
                "touching the runway dialog, reopen it, wait 10 s.");
        }
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
        Say("watching for 60 s: open the Active airport/runway dialog now and tick something");
        return true;
    }

    Say("usage: .rwy info | status <ICAO> | write <ICAO> <DEP,..> <ARR,..> | arm <ICAO> <DEP,..> <ARR,..> | windows | watch");
    return true;
}

void RunwayPlugin::OnTimer(int Counter)
{
    m_lastTick = Counter;

    if (m_checkAtTick >= 0 && Counter >= m_checkAtTick)
    {
        m_checkAtTick = -1;
        Say("startup check - plugin wrote " + m_checkIcao + " before the sector loaded. EuroScope now reports:");
        ReportStatus(m_checkIcao);
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
    if (!m_checkIcao.empty())
        ReportStatus(m_checkIcao);
}
