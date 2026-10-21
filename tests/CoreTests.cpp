// Unit tests for the EuroScope-independent logic. Built and run by CI (ctest).
#include "Config.h"
#include "LocalTime.h"
#include "Metar.h"
#include "Selector.h"

#include <cmath>
#include <cstdio>
#include <string>

static int g_failures = 0;

#define CHECK(cond)                                                       \
    do                                                                    \
    {                                                                     \
        if (!(cond))                                                      \
        {                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);   \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b)                                                                              \
    do                                                                                              \
    {                                                                                               \
        const auto va = (a);                                                                        \
        const auto vb = (b);                                                                        \
        if (!(va == vb))                                                                            \
        {                                                                                           \
            std::printf("FAIL %s:%d  %s == %s\n", __FILE__, __LINE__, #a, #b);                      \
            ++g_failures;                                                                           \
        }                                                                                           \
    } while (0)

using namespace core;

static Metar M(const std::string& line)
{
    Metar m;
    CHECK(ParseMetar(line, m));
    return m;
}

static std::string Pick(const Fir& fir, const std::string& icao, const std::string& metar, int localMinute,
                        const std::string& current = "")
{
    const Airport* a = fir.Find(icao);
    CHECK(a != nullptr);
    if (!a)
        return "";
    const auto d = Decide(fir, *a, M(metar), RunwayHeadings{}, localMinute, current);
    return d.configuration ? d.configuration->name : "(none)";
}

static void TestMetar()
{
    auto m = M("LEBL 211900Z 04006KT 330V070 9999 FEW049 19/12 Q1022 NOSIG");
    CHECK_EQ(m.station, std::string("LEBL"));
    CHECK_EQ(m.observation, std::string("211900Z"));
    CHECK_EQ(m.direction, 40);
    CHECK_EQ(m.speedKt, 6);
    CHECK_EQ(m.visibilityM, 10000);
    CHECK_EQ(m.ceilingFt, -1);

    m = M("LEPA 211900Z VRB02KT 9999 FEW020 18/12 Q1022 NOSIG");
    CHECK(m.variable);
    CHECK_EQ(m.speedKt, 2);

    m = M("LEZG 211900Z 31025G38KT 0800 FG BKN004 OVC010 08/08 Q1010 TEMPO 0300");
    CHECK_EQ(m.gustKt, 38);
    CHECK_EQ(m.visibilityM, 800);
    CHECK_EQ(m.ceilingFt, 400);
    CHECK_EQ(m.WindText(), std::string("310/25G38"));

    m = M("LEIB 211900Z 00000KT CAVOK 17/13 Q1022 NOSIG");
    CHECK(m.calm);

    m = M("GCLP 211900Z 34008MPS 9999 SCT020 21/18 Q1021");
    CHECK_EQ(m.speedKt, 16);

    Metar bad;
    CHECK(!ParseMetar("No METAR found for XXXX", bad));
}

static void TestLocalTime()
{
    // 2026-10-21 22:30 UTC: summer time, Madrid UTC+2 -> 00:30, Canary UTC+1 -> 23:30
    const std::time_t t = 1792621800;
    CHECK_EQ(LocalMinuteOfDay("Europe/Madrid", t), 30);
    CHECK_EQ(LocalMinuteOfDay("Atlantic/Canary", t), 23 * 60 + 30);
    // 2026-12-01 12:00 UTC: winter, Madrid UTC+1
    CHECK_EQ(LocalMinuteOfDay("Europe/Madrid", 1796126400), 13 * 60);
    // Summer time ends 2026-10-25 01:00 UTC
    CHECK_EQ(UtcOffsetMinutes("Europe/Madrid", 1792890000 - 1), 120);
    CHECK_EQ(UtcOffsetMinutes("Europe/Madrid", 1792890000), 60);
    CHECK_EQ(ParseClock("23:00"), 1380);
    CHECK_EQ(ParseClock("24:00"), 1440);
}

static void TestComponents()
{
    const auto head = Components(240, 10, 240);
    CHECK(std::abs(head.headwind - 10) < 0.01 && head.crosswind < 0.01);
    const auto tail = Components(60, 10, 240);
    CHECK(std::abs(tail.headwind + 10) < 0.01);
    const auto cross = Components(330, 20, 240);
    CHECK(std::abs(cross.crosswind - 20) < 0.01);
}

static void TestLecb(const Fir& fir)
{
    CHECK_EQ(fir.name, std::string("LECB"));
    CHECK_EQ(fir.airports.size(), static_cast<size_t>(16));
    const int day = 12 * 60, night = 1 * 60;

    // LEBL day: west preferred up to 10 kt tailwind, gusts included.
    CHECK_EQ(Pick(fir, "LEBL", "LEBL 211200Z 06008KT 9999 Q1020", day), std::string("West parallel"));
    CHECK_EQ(Pick(fir, "LEBL", "LEBL 211200Z 06012KT 9999 Q1020", day), std::string("East parallel"));
    CHECK_EQ(Pick(fir, "LEBL", "LEBL 211200Z 06008G15KT 9999 Q1020", day), std::string("East parallel"));
    CHECK_EQ(Pick(fir, "LEBL", "LEBL 211200Z VRB05KT 9999 Q1020", day, "East parallel"), std::string("West parallel"));
    // LEBL night: north crossed, west single runway when 02/06R are out of limits.
    CHECK_EQ(Pick(fir, "LEBL", "LEBL 210100Z 04008KT 9999 Q1020", night), std::string("North crossed"));
    CHECK_EQ(Pick(fir, "LEBL", "LEBL 210100Z 24015KT 9999 Q1020", night), std::string("West single runway"));
    // LEBL low visibility: RWY 02 not usable, best for the wind among the rest.
    CHECK_EQ(Pick(fir, "LEBL", "LEBL 210100Z 03004KT 0800 FG Q1020", night), std::string("West single runway"));

    // LEIB: 24 preferred up to 5 kt tailwind.
    CHECK_EQ(Pick(fir, "LEIB", "LEIB 211200Z 06004KT CAVOK Q1020", day), std::string("RWY 24"));
    CHECK_EQ(Pick(fir, "LEIB", "LEIB 211200Z 06007KT CAVOK Q1020", day), std::string("RWY 06"));

    // LEGE: ILS CAT III on 19 wins while within limits.
    CHECK_EQ(Pick(fir, "LEGE", "LEGE 211200Z 01004KT CAVOK Q1020", day), std::string("RWY 19"));
    CHECK_EQ(Pick(fir, "LEGE", "LEGE 211200Z 01010KT CAVOK Q1020", day), std::string("RWY 01"));

    // LEVC: ILS on both ends -> most headwind; calm -> keep current.
    CHECK_EQ(Pick(fir, "LEVC", "LEVC 211200Z 30008KT CAVOK Q1020", day, "RWY 12"), std::string("RWY 30"));
    CHECK_EQ(Pick(fir, "LEVC", "LEVC 211200Z 00000KT CAVOK Q1020", day, "RWY 30"), std::string("RWY 30"));
    CHECK_EQ(Pick(fir, "LEVC", "LEVC 211200Z 00000KT CAVOK Q1020", day), std::string("RWY 12"));

    // LEZG: 30R preferred, 12 configuration when needed.
    CHECK_EQ(Pick(fir, "LEZG", "LEZG 211200Z 12010KT CAVOK Q1020", day), std::string("RWY 12"));
    CHECK_EQ(Pick(fir, "LEZG", "LEZG 211200Z 31020KT CAVOK Q1020", day), std::string("RWY 30R"));
}

int main(int argc, char** argv)
{
    TestMetar();
    TestLocalTime();
    TestComponents();

    Fir fir;
    std::string error;
    const std::string path = argc > 1 ? argv[1] : "config/LECB.json";
    CHECK(LoadFir(path, fir, error));
    if (!error.empty())
        std::printf("%s\n", error.c_str());
    else
        TestLecb(fir);

    std::printf(g_failures ? "%d FAILED\n" : "all tests passed\n", g_failures);
    return g_failures ? 1 : 0;
}
