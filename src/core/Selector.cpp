#include "Selector.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

namespace core {

namespace {

constexpr double kPi = 3.14159265358979323846;

double HeadingOf(const RunwayHeadings& headings, const std::string& runway)
{
    const auto it = headings.find(runway);
    if (it != headings.end())
        return it->second;
    return std::atoi(runway.c_str()) * 10.0;
}

// Worst wind of a configuration over all its runways.
struct ConfigWind
{
    const Configuration* configuration = nullptr;
    double worstTailwind = 0;
    double worstCrosswind = 0;
    double leastHeadwind = 0;
    std::string worstRunway;  // runway with the largest limit excess, for messages
    bool withinLimits = true;
};

ConfigWind Evaluate(const Configuration& c, const Airport& airport, const Metar& metar, const RunwayHeadings& headings)
{
    ConfigWind w;
    w.configuration = &c;
    w.leastHeadwind = std::numeric_limits<double>::max();
    const int speed = std::max(metar.speedKt, metar.gustKt);
    double worstExcess = -std::numeric_limits<double>::max();
    for (const auto& runway : c.Runways())
    {
        const auto comp = Components(metar.direction, speed, HeadingOf(headings, runway));
        const double tail = std::max(0.0, -comp.headwind);
        w.worstTailwind = std::max(w.worstTailwind, tail);
        w.worstCrosswind = std::max(w.worstCrosswind, comp.crosswind);
        w.leastHeadwind = std::min(w.leastHeadwind, comp.headwind);
        const double excess = std::max(tail - airport.maxTailwindKt, comp.crosswind - airport.maxCrosswindKt);
        if (excess > worstExcess)
        {
            worstExcess = excess;
            w.worstRunway = runway;
        }
    }
    w.withinLimits = w.worstTailwind <= airport.maxTailwindKt && w.worstCrosswind <= airport.maxCrosswindKt;
    return w;
}

std::string Knots(double v)
{
    return std::to_string(static_cast<int>(std::lround(v))) + " kt";
}

std::string LimitText(const ConfigWind& w, const Airport& airport)
{
    if (w.worstTailwind > airport.maxTailwindKt)
        return "tailwind " + Knots(w.worstTailwind) + " on RWY " + w.worstRunway + " (limit " +
               std::to_string(airport.maxTailwindKt) + " kt)";
    return "crosswind " + Knots(w.worstCrosswind) + " on RWY " + w.worstRunway + " (limit " +
           std::to_string(airport.maxCrosswindKt) + " kt)";
}

// Least tailwind, then least crosswind, then most headwind.
const ConfigWind& BestForWind(const std::vector<ConfigWind>& all)
{
    return *std::min_element(all.begin(), all.end(), [](const ConfigWind& a, const ConfigWind& b) {
        if (std::abs(a.worstTailwind - b.worstTailwind) > 0.5)
            return a.worstTailwind < b.worstTailwind;
        if (std::abs(a.worstCrosswind - b.worstCrosswind) > 0.5)
            return a.worstCrosswind < b.worstCrosswind;
        return a.leastHeadwind > b.leastHeadwind;
    });
}

bool UsesAny(const Configuration& c, const std::vector<std::string>& runways)
{
    for (const auto& r : c.Runways())
        if (std::find(runways.begin(), runways.end(), r) != runways.end())
            return true;
    return false;
}

}

WindOnRunway Components(int windFrom, int speedKt, double runwayHeading)
{
    const double angle = (windFrom - runwayHeading) * kPi / 180.0;
    return WindOnRunway{speedKt * std::cos(angle), std::abs(speedKt * std::sin(angle))};
}

Decision Decide(const Rules& rules, const Airport& airport, const Metar& metar, const RunwayHeadings& headings,
                int localMinuteOfDay, const std::string& currentConfiguration)
{
    Decision d;
    d.schedule = airport.ScheduleAt(localMinuteOfDay);
    if (!d.schedule || !metar.hasWind)
    {
        d.reason = "no usable wind in the METAR";
        return d;
    }

    std::vector<const Configuration*> candidates;
    for (const auto& c : d.schedule->configurations)
        candidates.push_back(&c);

    const bool lowVisibility =
        (airport.suspendBelowCeilingFt >= 0 && metar.ceilingFt >= 0 && metar.ceilingFt < airport.suspendBelowCeilingFt) ||
        (airport.suspendBelowVisibilityM >= 0 && metar.visibilityM >= 0 && metar.visibilityM < airport.suspendBelowVisibilityM);
    if (lowVisibility && !airport.notInLowVisibility.empty())
    {
        std::vector<const Configuration*> usable;
        for (const auto* c : candidates)
            if (!UsesAny(*c, airport.notInLowVisibility))
                usable.push_back(c);
        if (!usable.empty())
            candidates = usable;
    }

    // Approach selection: best approach first, keeping the file order for equal ranks.
    if (airport.selection == Selection::Approach)
        std::stable_sort(candidates.begin(), candidates.end(), [&](const Configuration* a, const Configuration* b) {
            return rules.ApproachRank(a->approach) < rules.ApproachRank(b->approach);
        });

    const std::string wind = "wind " + metar.WindText();
    if (metar.calm || metar.variable)
    {
        const Configuration* pick = candidates.front();
        if (airport.selection == Selection::Approach)
        {
            const int best = rules.ApproachRank(pick->approach);
            for (const auto* c : candidates)
                if (rules.ApproachRank(c->approach) == best && c->name == currentConfiguration)
                    pick = c;
        }
        d.configuration = pick;
        d.reason = wind + ": " + (airport.selection == Selection::Preferential ? "preferential configuration"
                                                                                 : "runway with the best approach");
        return d;
    }

    std::vector<ConfigWind> evaluated;
    for (const auto* c : candidates)
        evaluated.push_back(Evaluate(*c, airport, metar, headings));

    if (lowVisibility && airport.selection == Selection::Preferential)
    {
        const auto& best = BestForWind(evaluated);
        d.configuration = best.configuration;
        d.reason = wind + ", low ceiling/visibility: preferential configurations suspended, best for the wind";
        return d;
    }

    std::vector<const ConfigWind*> within;
    for (const auto& w : evaluated)
        if (w.withinLimits)
            within.push_back(&w);

    if (within.empty())
    {
        const auto& best = BestForWind(evaluated);
        d.configuration = best.configuration;
        d.reason = wind + ": no configuration within limits, best for the wind (" + LimitText(best, airport) + ")";
        return d;
    }

    const ConfigWind* pick = within.front();
    if (airport.selection == Selection::Approach)
    {
        // Same approach type: most headwind; same headwind (within 1 kt): keep the current one.
        const int best = rules.ApproachRank(pick->configuration->approach);
        for (const auto* w : within)
        {
            if (rules.ApproachRank(w->configuration->approach) != best || w == pick)
                continue;
            if (w->leastHeadwind > pick->leastHeadwind + 1.0 ||
                (std::abs(w->leastHeadwind - pick->leastHeadwind) <= 1.0 && w->configuration->name == currentConfiguration))
                pick = w;
        }
    }
    d.configuration = pick->configuration;

    if (pick == &evaluated.front())
        d.reason = wind + ": " + (airport.selection == Selection::Preferential ? "preferential configuration" : "runway with the best approach") + " within limits";
    else if (evaluated.front().withinLimits)
        d.reason = wind + ": same approach type on both, " +
                   (pick->configuration->name == currentConfiguration ? std::string("current configuration kept")
                                                                      : "more headwind on " + pick->configuration->name);
    else
        d.reason = wind + ": " + evaluated.front().configuration->name + " not usable, " + LimitText(evaluated.front(), airport);
    return d;
}

}
