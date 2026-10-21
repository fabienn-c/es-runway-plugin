#include "RwyFile.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace rwyfile {

std::string PathForSector(const std::string& sectorFile)
{
    fs::path p(sectorFile);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext != ".sct" && ext != ".sct2")
        return {};
    p.replace_extension(".rwy");
    return p.string();
}

static bool BelongsTo(const std::string& line, const std::string& icao)
{
    return line.rfind("ACTIVE_AIRPORT:" + icao + ":", 0) == 0 || line.rfind("ACTIVE_RUNWAY:" + icao + ":", 0) == 0;
}

bool Apply(const std::string& rwyPath, const AirportConfig& config, std::string& error)
{
    std::vector<std::string> kept;
    {
        std::ifstream in(rwyPath);
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty() && !BelongsTo(line, config.icao))
                kept.push_back(line);
        }
    }

    std::error_code ec;
    const std::string backup = rwyPath + ".bak";
    if (fs::exists(rwyPath, ec) && !fs::exists(backup, ec))
        fs::copy_file(rwyPath, backup, ec);

    std::ofstream out(rwyPath, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        error = "cannot write " + rwyPath;
        return false;
    }
    for (const auto& line : kept)
        out << line << "\r\n";
    if (!config.departures.empty())
        out << "ACTIVE_AIRPORT:" << config.icao << ":1\r\n";
    if (!config.arrivals.empty())
        out << "ACTIVE_AIRPORT:" << config.icao << ":0\r\n";
    for (const auto& rwy : config.departures)
        out << "ACTIVE_RUNWAY:" << config.icao << ":" << rwy << ":1\r\n";
    for (const auto& rwy : config.arrivals)
        out << "ACTIVE_RUNWAY:" << config.icao << ":" << rwy << ":0\r\n";
    return true;
}

std::vector<std::string> SplitList(const std::string& csv)
{
    std::vector<std::string> items;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ','))
        if (!item.empty())
            items.push_back(item);
    return items;
}

std::string JoinList(const std::vector<std::string>& items)
{
    std::string out;
    for (const auto& item : items)
        out += (out.empty() ? "" : ",") + item;
    return out;
}

}
