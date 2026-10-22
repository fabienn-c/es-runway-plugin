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

static std::string Pick(const Rules& fir, const std::string& icao, const std::string& metar, int localMinute,
                        const std::string& current = "", int weekday = -1)
{
    const Airport* a = fir.Find(icao);
    CHECK(a != nullptr);
    if (!a)
        return "";
    const auto d = Decide(fir, *a, M(metar), RunwayHeadings{}, localMinute, current, weekday);
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

    // Age: 2026-10-21 23:37 UTC = 1792625820
    const std::time_t now = 1792625820;
    CHECK_EQ(MetarAgeMinutes(M("LEPA 212330Z VRB01KT 9999 Q1023"), now), 7);
    CHECK_EQ(MetarAgeMinutes(M("LESU 211730Z 31012KT 9999 Q1021"), now), 367);
    // 2026-11-01 00:10 UTC = 1793491800, METAR from 31 October 23:50
    CHECK_EQ(MetarAgeMinutes(M("LEBL 312350Z 24005KT 9999 Q1020"), 1793491800), 20);
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
    // Weekday: 2026-10-24 06:30 UTC is Saturday 08:30 in Madrid; 2026-10-24 22:30 UTC is Sunday 00:30.
    CHECK_EQ(LocalWeekday("Europe/Madrid", 1792823400), 6);
    CHECK_EQ(LocalWeekday("Europe/Madrid", 1792881000), 0);
    CHECK_EQ(LocalWeekday("Atlantic/Canary", 1792881000), 6);
    CHECK_EQ(ParseWeekday("SAT"), 6);
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

static std::string Reason(const Rules& fir, const std::string& icao, const std::string& metar,
                          const std::string& current = "")
{
    const Airport* a = fir.Find(icao);
    return a ? Decide(fir, *a, M(metar), RunwayHeadings{}, 12 * 60, current).reason : "";
}

static bool Contains(const std::string& text, const std::string& part)
{
    const bool found = text.find(part) != std::string::npos;
    if (!found)
        std::printf("  reason was: %s\n", text.c_str());
    return found;
}

static void TestReasons(const Rules& fir)
{
    // Equal approaches: the reason explains the choice, whichever runway is listed first.
    CHECK(Contains(Reason(fir, "GCFV", "GCFV 221200Z 35010KT 9999 Q1020"),
                   "same approach type on both, RWY 19 not usable, tailwind 9 kt on RWY 19 (limit 5 kt)"));
    CHECK(Contains(Reason(fir, "GCHI", "GCHI 221200Z 34004KT 9999 Q1020"), "more headwind on RWY 34"));
    CHECK(Contains(Reason(fir, "LEVC", "LEVC 211200Z 30004KT CAVOK Q1020"), "more headwind on RWY 30"));
    // Equal approaches, crosswind only: current kept.
    CHECK(Contains(Reason(fir, "GCFV", "GCFV 221200Z 10010KT 9999 Q1020", "RWY 19"), "current configuration kept"));
    // Not a tie: best approach / preferential / not usable, unchanged.
    CHECK(Contains(Reason(fir, "LEGE", "LEGE 211200Z 01004KT CAVOK Q1020"), "runway with the best approach within limits"));
    CHECK(Contains(Reason(fir, "LEBL", "LEBL 211200Z 06008KT 9999 Q1020"), "preferential configuration within limits"));
    CHECK(Contains(Reason(fir, "LEVC", "LEVC 211200Z 30008KT CAVOK Q1020"), "RWY 12 not usable, tailwind 8 kt on RWY 12"));
    CHECK(Contains(Reason(fir, "LEIB", "LEIB 211200Z 06007KT CAVOK Q1020"), "RWY 24 not usable"));
}

static void TestRules(const Rules& fir)
{
    TestReasons(fir);
    CHECK_EQ(fir.name, std::string("LEXX"));
    CHECK_EQ(fir.airports.size(), static_cast<size_t>(56));

    // GCCC: Canary time zone, GCLP north preferred, GCLA 36 up to 10 kt tail / 25 kt cross.
    CHECK_EQ(fir.Find("GCLP")->timezone, std::string("Atlantic/Canary"));
    CHECK_EQ(Pick(fir, "GCLP", "GCLP 221200Z 03015KT 9999 Q1020", 12 * 60), std::string("North"));
    CHECK_EQ(Pick(fir, "GCLP", "GCLP 221200Z 21008KT 9999 Q1020", 12 * 60), std::string("South"));
    CHECK_EQ(Pick(fir, "GCLP", "GCLP 221200Z VRB02KT 9999 Q1020", 12 * 60, "South"), std::string("North"));
    CHECK_EQ(Pick(fir, "GCLA", "GCLA 221200Z 18009KT 9999 Q1020", 12 * 60), std::string("RWY 36"));
    CHECK_EQ(Pick(fir, "GCLA", "GCLA 221200Z 18012KT 9999 Q1020", 12 * 60), std::string("RWY 18"));
    CHECK_EQ(Pick(fir, "GCRR", "GCRR 221200Z 21004KT 9999 Q1020", 12 * 60), std::string("RWY 03"));
    // Calm or variable wind: GCTS 07, GCXO 30, GCFV 01, GCHI 34, even if the other runway is active.
    CHECK_EQ(Pick(fir, "GCTS", "GCTS 221200Z 00000KT 9999 Q1020", 12 * 60, "RWY 25"), std::string("RWY 07"));
    CHECK_EQ(Pick(fir, "GCXO", "GCXO 221200Z VRB03KT 9999 Q1020", 12 * 60, "RWY 12"), std::string("RWY 30"));
    CHECK_EQ(Pick(fir, "GCFV", "GCFV 221200Z VRB02KT 9999 Q1020", 12 * 60, "RWY 19"), std::string("RWY 01"));
    CHECK_EQ(Pick(fir, "GCHI", "GCHI 221200Z 00000KT 9999 Q1020", 12 * 60, "RWY 16"), std::string("RWY 34"));
    // Otherwise the best wind.
    CHECK_EQ(Pick(fir, "GCHI", "GCHI 221200Z 16008KT 9999 Q1020", 12 * 60, "RWY 34"), std::string("RWY 16"));
    CHECK_EQ(Pick(fir, "GCTS", "GCTS 221200Z 25010KT 9999 Q1020", 12 * 60, "RWY 07"), std::string("RWY 25"));
    // AFIS: GCGM always, GCHI only on Saturday (6) and Sunday (0).
    CHECK(fir.Find("GCGM")->IsAfis(3) && fir.Find("GCGM")->IsAfis(6));
    CHECK(fir.Find("GCHI")->IsAfis(6) && fir.Find("GCHI")->IsAfis(0) && !fir.Find("GCHI")->IsAfis(3));
    CHECK(!fir.Find("GCLP")->IsAfis(6));
    CHECK_EQ(fir.Find("LEBL")->timezone, std::string("Europe/Madrid"));
    CHECK(fir.Find("LEZG") && fir.Find("LELC") && fir.Find("LEAM"));  // once, shared by LECB and LECM

    // LEMD: north preferred up to 10 kt tailwind, night configuration 23-07 LT.
    CHECK_EQ(Pick(fir, "LEMD", "LEMD 211200Z 34008KT CAVOK Q1020", 12 * 60), std::string("North"));
    CHECK_EQ(Pick(fir, "LEMD", "LEMD 211200Z 16012KT CAVOK Q1020", 12 * 60), std::string("South"));
    CHECK_EQ(Pick(fir, "LEMD", "LEMD 210100Z VRB03KT CAVOK Q1020", 1 * 60, "South night"), std::string("North night"));
    CHECK_EQ(Pick(fir, "LEMD", "LEMD 210100Z 18015KT CAVOK Q1020", 1 * 60), std::string("South night"));
    // LEMD weekend: South night kept until 09:00 LT on Saturday (6) and Sunday (0), not on weekdays,
    // and never for North.
    const std::string south = "LEMD 220700Z 16012KT CAVOK Q1020";
    CHECK_EQ(Pick(fir, "LEMD", south, 8 * 60, "", 6), std::string("South night"));
    CHECK_EQ(Pick(fir, "LEMD", south, 8 * 60, "", 0), std::string("South night"));
    CHECK_EQ(Pick(fir, "LEMD", south, 9 * 60 + 30, "", 6), std::string("South"));
    CHECK_EQ(Pick(fir, "LEMD", south, 8 * 60, "", 3), std::string("South"));
    CHECK_EQ(Pick(fir, "LEMD", "LEMD 220700Z 34008KT CAVOK Q1020", 8 * 60, "", 6), std::string("North"));
    {
        const Airport* lemd = fir.Find("LEMD");
        const auto d = Decide(fir, *lemd, M(south), RunwayHeadings{}, 8 * 60, "", 6);
        CHECK_EQ(d.period, std::string("day+weekend"));
        CHECK_EQ(Decide(fir, *lemd, M(south), RunwayHeadings{}, 9 * 60 + 30, "", 6).period, std::string("day"));
    }

    // LEMG: south preferred.
    CHECK_EQ(Pick(fir, "LEMG", "LEMG 211200Z 31008KT CAVOK Q1020", 12 * 60), std::string("South"));
    CHECK_EQ(Pick(fir, "LEMG", "LEMG 211200Z 31012KT CAVOK Q1020", 12 * 60), std::string("North"));
    // LEAS: 29 preferred, default 5 kt tailwind.
    CHECK_EQ(Pick(fir, "LEAS", "LEAS 211200Z 11004KT CAVOK Q1020", 12 * 60), std::string("RWY 29"));
    CHECK_EQ(Pick(fir, "LEAS", "LEAS 211200Z 11008KT CAVOK Q1020", 12 * 60), std::string("RWY 11"));
    // LEST: ILS CAT III on 17 beats ILS CAT I on 35 while within limits.
    CHECK_EQ(Pick(fir, "LEST", "LEST 211200Z 35004KT CAVOK Q1020", 12 * 60), std::string("RWY 17"));
    CHECK_EQ(Pick(fir, "LEST", "LEST 211200Z 35009KT CAVOK Q1020", 12 * 60), std::string("RWY 35"));
    CHECK(fir.Find("LEBG")->afis && fir.Find("LEHC")->afis && fir.Find("LEBA")->afis);
    // GEML: wind only; calm or variable -> RWY 15, even if 33 is active.
    CHECK_EQ(Pick(fir, "GEML", "GEML 211200Z 00000KT CAVOK Q1020", 12 * 60, "RWY 33"), std::string("RWY 15"));
    CHECK_EQ(Pick(fir, "GEML", "GEML 211200Z VRB04KT CAVOK Q1020", 12 * 60, "RWY 33"), std::string("RWY 15"));
    CHECK_EQ(Pick(fir, "GEML", "GEML 211200Z 33008KT CAVOK Q1020", 12 * 60, "RWY 15"), std::string("RWY 33"));
    CHECK_EQ(Pick(fir, "GEML", "GEML 211200Z 15008KT CAVOK Q1020", 12 * 60, "RWY 33"), std::string("RWY 15"));
    // LEMG: two-runway configurations.
    const auto* lemgSouth = fir.Find("LEMG")->AllConfigurations().front();
    CHECK(lemgSouth->arrivals == std::vector<std::string>{"12"} && lemgSouth->departures == std::vector<std::string>{"13"});
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

    // Active runways must match a configuration exactly: extra runways are not "already active".
    const Airport* lepa = fir.Find("LEPA");
    CHECK_EQ(lepa->MatchActive({"24L"}, {"24R"}), std::string("West"));
    CHECK_EQ(lepa->MatchActive({"24L", "24R", "06L", "06R"}, {"24L", "24R", "06L", "06R"}), std::string(""));
    CHECK_EQ(lepa->MatchActive({"24L"}, {"24R", "24L"}), std::string(""));
    CHECK_EQ(lepa->MatchActive({}, {}), std::string(""));
    const Airport* lebl = fir.Find("LEBL");
    CHECK_EQ(lebl->MatchActive({"24L"}, {"24L"}), std::string("West single runway"));
    CHECK_EQ(lebl->MatchActive({"02"}, {"06R"}), std::string("North crossed"));

    CHECK(fir.Find("LESU")->afis);
    CHECK(!fir.Find("LEBL")->afis);

    // LEZG: 30R preferred, 12 configuration when needed.
    CHECK_EQ(Pick(fir, "LEZG", "LEZG 211200Z 12010KT CAVOK Q1020", day), std::string("RWY 12"));
    CHECK_EQ(Pick(fir, "LEZG", "LEZG 211200Z 31020KT CAVOK Q1020", day), std::string("RWY 30R"));
}

int main(int argc, char** argv)
{
    TestMetar();
    TestLocalTime();
    TestComponents();

    Rules fir;
    std::string error;
    const std::string path = argc > 1 ? argv[1] : "config/LEXX.json";
    CHECK(LoadRules(path, fir, error));
    if (!error.empty())
        std::printf("%s\n", error.c_str());
    else
        TestRules(fir);

    std::printf(g_failures ? "%d FAILED\n" : "all tests passed\n", g_failures);
    return g_failures ? 1 : 0;
}
