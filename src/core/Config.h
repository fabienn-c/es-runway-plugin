#pragma once

#include <set>
#include <string>
#include <vector>

// Runway rules of one FIR, loaded from config/<FIR>.json.
namespace core {

struct Configuration
{
    std::string name;
    std::vector<std::string> arrivals;
    std::vector<std::string> departures;
    std::string approach;  // only used by "approach" selection

    std::vector<std::string> Runways() const;  // arrivals and departures, without duplicates
    std::string Describe() const;              // "West parallel (ARR 24R / DEP 24L)"
};

struct Schedule
{
    std::string name;
    int fromMinute = 0;  // local minutes of day; from > to wraps past midnight
    int toMinute = 1440;
    std::vector<Configuration> configurations;

    bool IsAllDay() const { return fromMinute == 0 && toMinute == 1440; }
    bool Covers(int minuteOfDay) const;
};

enum class Selection
{
    Preferential,  // AIP preferential order
    Approach       // best approach type, then wind
};

struct Airport
{
    std::string icao;
    Selection selection = Selection::Approach;
    int maxTailwindKt = 5;
    int maxCrosswindKt = 15;
    int suspendBelowCeilingFt = -1;  // preferential order suspended below these, -1 = never
    int suspendBelowVisibilityM = -1;
    std::vector<std::string> notInLowVisibility;
    std::vector<Schedule> schedules;

    const Schedule* ScheduleAt(int minuteOfDay) const;
    std::vector<const Configuration*> AllConfigurations() const;

    // The configuration whose runways are exactly the active ones (no missing, no extra runways),
    // or empty when the active runways match none.
    std::string MatchActive(const std::set<std::string>& arrivals, const std::set<std::string>& departures) const;
};

struct Fir
{
    std::string name;
    std::string timezone;
    std::vector<std::string> approachRanking;  // best first
    std::vector<Airport> airports;

    const Airport* Find(const std::string& icao) const;
    int ApproachRank(const std::string& approach) const;  // lower is better
};

// Loads one FIR file. On failure returns false and describes the problem in `error`.
bool LoadFir(const std::string& path, Fir& out, std::string& error);

}
