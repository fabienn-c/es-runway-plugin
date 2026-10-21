#pragma once

#include <string>
#include <vector>

// EuroScope stores active runways in "<sector file name>.rwy" next to the .sct:
//   ACTIVE_AIRPORT:LEBL:1          airport active for departures (1) / arrivals (0)
//   ACTIVE_RUNWAY:LEBL:24R:1       runway active for departures (1) / arrivals (0)
namespace rwyfile {

struct AirportConfig
{
    std::string icao;
    std::vector<std::string> departures;
    std::vector<std::string> arrivals;
};

// "C:\...\LECB-...-0004.sct" -> "C:\...\LECB-...-0004.rwy"; empty if not a .sct path.
std::string PathForSector(const std::string& sectorFile);

// Replaces every line of this airport in the .rwy file, keeping other airports.
// Backs the original up to .rwy.bak the first time.
bool Apply(const std::string& rwyPath, const AirportConfig& config, std::string& error);

std::vector<std::string> SplitList(const std::string& csv);
std::string JoinList(const std::vector<std::string>& items);

}
