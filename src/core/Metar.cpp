#include "Metar.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <regex>
#include <sstream>
#include <vector>

namespace core {

namespace {

const std::regex kTime(R"(^\d{6}Z$)");
const std::regex kWind(R"(^(\d{3}|VRB)(\d{2,3})(?:G(\d{2,3}))?(KT|MPS)$)");
const std::regex kVisibility(R"(^\d{4}$)");
const std::regex kCloud(R"(^(BKN|OVC|VV)(\d{3}))");

int ToKnots(int value, const std::string& unit)
{
    return unit == "MPS" ? static_cast<int>(std::lround(value * 1.943844)) : value;
}

}

std::string Metar::WindText() const
{
    if (!hasWind)
        return "no wind data";
    if (calm)
        return "calm";
    char buf[32];
    if (variable)
        std::snprintf(buf, sizeof(buf), "VRB%02d", speedKt);
    else
        std::snprintf(buf, sizeof(buf), "%03d/%02d", direction, speedKt);
    std::string text = buf;
    if (gustKt > 0)
        text += "G" + std::to_string(gustKt);
    return text;
}

bool ParseMetar(const std::string& line, Metar& out)
{
    out = Metar{};
    out.raw = line;
    std::istringstream ss(line);
    std::vector<std::string> tokens;
    for (std::string t; ss >> t;)
        tokens.push_back(t);

    size_t i = 0;
    while (i < tokens.size() && (tokens[i] == "METAR" || tokens[i] == "SPECI" || tokens[i] == "COR"))
        ++i;
    if (i >= tokens.size() || tokens[i].size() != 4)
        return false;
    out.station = tokens[i++];

    for (; i < tokens.size(); ++i)
    {
        const std::string& t = tokens[i];
        std::smatch m;
        if (t == "TEMPO" || t == "BECMG" || t == "NOSIG" || t == "RMK")
            break;  // trends describe the future, not the observation
        if (out.observation.empty() && std::regex_match(t, kTime))
            out.observation = t;
        else if (!out.hasWind && std::regex_match(t, m, kWind))
        {
            out.hasWind = true;
            const std::string unit = m[4];
            out.speedKt = ToKnots(std::stoi(m[2]), unit);
            out.gustKt = m[3].matched ? ToKnots(std::stoi(m[3]), unit) : 0;
            if (m[1] == "VRB")
                out.variable = true;
            else
                out.direction = std::stoi(m[1]);
            out.calm = !out.variable && out.direction == 0 && out.speedKt == 0;
        }
        else if (t == "CAVOK")
            out.visibilityM = 10000;
        else if (out.visibilityM < 0 && out.hasWind && std::regex_match(t, kVisibility))
            out.visibilityM = std::stoi(t) == 9999 ? 10000 : std::stoi(t);
        else if (std::regex_search(t, m, kCloud))
        {
            const int height = std::stoi(m[2]) * 100;
            out.ceilingFt = out.ceilingFt < 0 ? height : std::min(out.ceilingFt, height);
        }
    }
    return !out.observation.empty();
}

}
