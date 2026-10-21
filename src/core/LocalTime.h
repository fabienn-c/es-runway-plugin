#pragma once

#include <ctime>
#include <string>

// Local time for the FIR time zones, with the EU summer-time rule
// (last Sunday of March to last Sunday of October, changing at 01:00 UTC).
namespace core {

// Offset from UTC in minutes. Known zones: "Europe/Madrid" (UTC+1), "Atlantic/Canary" (UTC+0).
int UtcOffsetMinutes(const std::string& timezone, std::time_t utc);

// Minutes since local midnight (0..1439).
int LocalMinuteOfDay(const std::string& timezone, std::time_t utc);

// "23:00" -> 1380; "24:00" -> 1440. Returns -1 if malformed.
int ParseClock(const std::string& hhmm);

std::string FormatClock(int minuteOfDay);

}
