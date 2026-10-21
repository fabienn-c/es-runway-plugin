#pragma once

#include <string>

// The parts of a METAR the runway logic needs.
namespace core {

struct Metar
{
    std::string station;
    std::string observation;  // "211900Z": changes with every new report
    std::string raw;

    bool hasWind = false;
    bool calm = false;      // 00000KT
    bool variable = false;  // VRBxxKT
    int direction = 0;      // degrees true
    int speedKt = 0;
    int gustKt = 0;         // 0 when no gust

    int visibilityM = -1;   // -1 when not reported
    int ceilingFt = -1;     // lowest BKN/OVC/VV layer, -1 when none

    // Wind group as written for messages, e.g. "240/12G22" or "VRB03".
    std::string WindText() const;
};

// Parses one METAR line ("LEBL 211900Z 04006KT ..."). Returns false if it has no station or time.
bool ParseMetar(const std::string& line, Metar& out);

}
