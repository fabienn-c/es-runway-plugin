#include "Config.h"

#include "LocalTime.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

using nlohmann::json;

namespace core {

namespace {

std::string Join(const std::vector<std::string>& items)
{
    std::string out;
    for (const auto& item : items)
        out += (out.empty() ? "" : ",") + item;
    return out.empty() ? "-" : out;
}

Configuration ReadConfiguration(const json& j)
{
    Configuration c;
    c.name = j.at("name").get<std::string>();
    c.arrivals = j.at("arr").get<std::vector<std::string>>();
    c.departures = j.at("dep").get<std::vector<std::string>>();
    c.approach = j.value("approach", "");
    return c;
}

std::vector<Configuration> ReadConfigurations(const json& list)
{
    std::vector<Configuration> out;
    for (const auto& c : list)
        out.push_back(ReadConfiguration(c));
    return out;
}

Airport ReadAirport(const std::string& icao, const json& j, int defaultTail, int defaultCross)
{
    Airport a;
    a.icao = icao;
    a.selection = j.value("selection", "approach") == "preferential" ? Selection::Preferential : Selection::Approach;
    const json limits = j.value("limits", json::object());
    a.maxTailwindKt = limits.value("maxTailwindKt", defaultTail);
    a.maxCrosswindKt = limits.value("maxCrosswindKt", defaultCross);
    if (j.contains("preferentialSuspendedBelow"))
    {
        a.suspendBelowCeilingFt = j["preferentialSuspendedBelow"].value("ceilingFt", -1);
        a.suspendBelowVisibilityM = j["preferentialSuspendedBelow"].value("visibilityM", -1);
    }
    a.notInLowVisibility = j.value("notInLowVisibility", std::vector<std::string>{});

    if (j.contains("schedules"))
    {
        for (const auto& s : j["schedules"])
        {
            Schedule schedule;
            schedule.name = s.at("name").get<std::string>();
            schedule.fromMinute = ParseClock(s.at("from").get<std::string>());
            schedule.toMinute = ParseClock(s.at("to").get<std::string>());
            if (schedule.fromMinute < 0 || schedule.toMinute < 0)
                throw std::runtime_error(icao + ": bad time in schedule '" + schedule.name + "'");
            schedule.configurations = ReadConfigurations(s.at("configurations"));
            a.schedules.push_back(schedule);
        }
    }
    else
    {
        Schedule allDay;
        allDay.name = "all day";
        allDay.configurations = ReadConfigurations(j.at("configurations"));
        a.schedules.push_back(allDay);
    }
    for (const auto& s : a.schedules)
        if (s.configurations.empty())
            throw std::runtime_error(icao + ": schedule '" + s.name + "' has no configurations");
    return a;
}

}

std::vector<std::string> Configuration::Runways() const
{
    std::vector<std::string> out = arrivals;
    for (const auto& r : departures)
        if (std::find(out.begin(), out.end(), r) == out.end())
            out.push_back(r);
    return out;
}

std::string Configuration::Describe() const
{
    return name + " (ARR " + Join(arrivals) + " / DEP " + Join(departures) + ")";
}

bool Schedule::Covers(int minuteOfDay) const
{
    if (fromMinute <= toMinute)
        return minuteOfDay >= fromMinute && minuteOfDay < toMinute;
    return minuteOfDay >= fromMinute || minuteOfDay < toMinute;
}

const Schedule* Airport::ScheduleAt(int minuteOfDay) const
{
    for (const auto& s : schedules)
        if (s.Covers(minuteOfDay))
            return &s;
    return schedules.empty() ? nullptr : &schedules.front();
}

std::vector<const Configuration*> Airport::AllConfigurations() const
{
    std::vector<const Configuration*> out;
    for (const auto& s : schedules)
        for (const auto& c : s.configurations)
            out.push_back(&c);
    return out;
}

const Airport* Fir::Find(const std::string& icao) const
{
    for (const auto& a : airports)
        if (a.icao == icao)
            return &a;
    return nullptr;
}

int Fir::ApproachRank(const std::string& approach) const
{
    const auto it = std::find(approachRanking.begin(), approachRanking.end(), approach);
    return it == approachRanking.end() ? static_cast<int>(approachRanking.size()) : static_cast<int>(it - approachRanking.begin());
}

bool LoadFir(const std::string& path, Fir& out, std::string& error)
{
    try
    {
        std::ifstream in(path);
        if (!in)
        {
            error = "cannot open " + path;
            return false;
        }
        const json j = json::parse(in);
        Fir fir;
        fir.name = j.at("fir").get<std::string>();
        fir.timezone = j.value("timezone", "Europe/Madrid");
        fir.approachRanking = j.value("approachRanking", std::vector<std::string>{});
        const json defaults = j.value("defaults", json::object());
        const int tail = defaults.value("maxTailwindKt", 5);
        const int cross = defaults.value("maxCrosswindKt", 15);
        for (const auto& [icao, airport] : j.at("airports").items())
            fir.airports.push_back(ReadAirport(icao, airport, tail, cross));
        out = std::move(fir);
        return true;
    }
    catch (const std::exception& e)
    {
        error = path + ": " + e.what();
        return false;
    }
}

}
