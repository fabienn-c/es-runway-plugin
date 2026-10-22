#include "LocalTime.h"

#include <cstdio>

namespace core {

namespace {

// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm).
long long DaysFromCivil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

// 01:00 UTC on the last Sunday of the month, as seconds since the epoch.
std::time_t LastSundayAt0100(int year, unsigned month)
{
    const unsigned lastDay = 31;  // March and October both have 31 days
    const long long days = DaysFromCivil(year, month, lastDay);
    const long long weekday = (days + 4) % 7;  // 1970-01-01 was a Thursday (4); Sunday = 0
    return static_cast<std::time_t>((days - weekday) * 86400 + 3600);
}

bool IsEuSummerTime(std::time_t utc)
{
    std::tm parts = {};
    gmtime_s(&parts, &utc);
    const int year = parts.tm_year + 1900;
    return utc >= LastSundayAt0100(year, 3) && utc < LastSundayAt0100(year, 10);
}

}

int UtcOffsetMinutes(const std::string& timezone, std::time_t utc)
{
    const int standard = timezone == "Europe/Madrid" ? 60 : 0;
    return standard + (IsEuSummerTime(utc) ? 60 : 0);
}

int LocalMinuteOfDay(const std::string& timezone, std::time_t utc)
{
    const long long local = static_cast<long long>(utc) + UtcOffsetMinutes(timezone, utc) * 60LL;
    return static_cast<int>(((local / 60) % 1440 + 1440) % 1440);
}

int LocalWeekday(const std::string& timezone, std::time_t utc)
{
    const long long local = static_cast<long long>(utc) + UtcOffsetMinutes(timezone, utc) * 60LL;
    const long long days = local >= 0 ? local / 86400 : (local - 86399) / 86400;
    return static_cast<int>(((days + 4) % 7 + 7) % 7);  // 1970-01-01 was a Thursday
}

int ParseWeekday(const std::string& day)
{
    static const char* const kDays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    for (int i = 0; i < 7; ++i)
        if (day == kDays[i])
            return i;
    return -1;
}

int ParseClock(const std::string& hhmm)
{
    int h = 0, m = 0;
    if (std::sscanf(hhmm.c_str(), "%d:%d", &h, &m) != 2 || h < 0 || h > 24 || m < 0 || m > 59)
        return -1;
    return h * 60 + m;
}

std::string FormatClock(int minuteOfDay)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", (minuteOfDay / 60) % 24, minuteOfDay % 60);
    return buf;
}

}
