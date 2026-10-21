#pragma once

#include "Config.h"
#include "Metar.h"

#include <map>
#include <string>

// Chooses the runway configuration for one airport.
//
//  1. The schedule (e.g. LEBL day/night) is chosen from the local time.
//  2. Calm or variable wind: the preferential configuration (or best approach).
//  3. "preferential": the first configuration within the wind limits.
//     "approach": among configurations within the limits, the best approach type;
//     equal approach -> most headwind; still equal -> keep the current one.
//  4. Nothing within limits: the configuration the wind suits best.
//  Wind limits apply to every runway of a configuration, gusts included.
namespace core {

struct WindOnRunway
{
    double headwind = 0;   // negative = tailwind
    double crosswind = 0;  // always >= 0
};

// `runwayHeading` is the runway's true heading in degrees, METAR wind is true.
WindOnRunway Components(int windFrom, int speedKt, double runwayHeading);

// True headings by runway designator ("24R" -> 245.3). Missing runways fall back to designator x 10.
using RunwayHeadings = std::map<std::string, double>;

struct Decision
{
    const Configuration* configuration = nullptr;  // null when no decision is possible
    const Schedule* schedule = nullptr;
    std::string reason;
};

Decision Decide(const Fir& fir, const Airport& airport, const Metar& metar, const RunwayHeadings& headings,
                int localMinuteOfDay, const std::string& currentConfiguration);

}
