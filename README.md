# ES Runway Plugin

EuroScope plugin for VATSIM Spain that recommends and sets the active runways of every airport in the
loaded sector (LECB, LECM or GCCC), from the VATSIM METAR, the AIP preferential configurations, the best
available approach and day/night rules.

**Version 1.0.1.** All three FIRs (LECB, LECM, GCCC) are configured: 56 airports.

- Controller guide (Spanish): [docs/GUIA.md](docs/GUIA.md)
- Integration in the vACC package (Spanish): [docs/OPERACIONES.md](docs/OPERACIONES.md)

## What it does

- **At startup**, as soon as the sector and the METARs are loaded (1–2 seconds), a pop-up announces the
  runway configuration. Nothing happens until the controller presses **OK**. Then the plugin opens the
  *Active airport/runway* dialog, ticks the runways of every airport in it, presses OK and shows a summary.
  If every airport is already correct, only the summary is shown.
- **It only touches the runway ticks** (DEP/ARR of each runway), never the airport ticks.
- **Afterwards**, every new METAR and every day/night change is checked. If the recommended configuration
  differs from the active one, a pop-up offers **Accept** / **Decline**. Nothing changes without the
  controller accepting. A declined suggestion comes back with the next METAR if it still applies.
- **Which airports get suggestions** depends on the controller's position (facility reported by EuroScope):
  - DEL / GND / TWR: only their own airport (the first 4 letters of the callsign, e.g. `LEBL_TWR`).
  - APP / CTR, observer or not connected: every airport of the sector.
- **Clicking only happens while EuroScope is the active window.** Switching to another program pauses it;
  it continues when EuroScope is in front again (gives up after 2 minutes away). If the runway icon is not
  found in the top bar, the plugin never clicks blindly: it asks the controller to open the dialog and
  continues from there.
- METARs come from `metar.vatsim.net` (checked every minute) and from EuroScope itself when connected.

## Runway rules

For each airport, in this order (see `config/LEXX.json`):

1. **AIP preferential configuration**, as long as the wind is within its limits (gusts included).
   Examples: LEBL west by day / north crossed by night, LEMD north, LEMG south, LEPA west, GCLP north,
   GCLA 36.
2. **No AIP preference:** the runway with the best approach (ILS CAT III > ILS CAT II > ILS CAT I >
   RNP LPV > RNP LNAV/VNAV > RNP AR > LOC > RNP LNAV > VOR > TACAN > NDB > visual), if the wind allows.
   Same approach on both ends: the one with most headwind; similar wind: the current one is kept.
3. **Wind outside the limits on every option:** the configuration the wind suits best.
4. **Calm or variable wind:** the preferential configuration (or best approach), or the airport's calm-wind
   runway when it has one (GEML 15, GCTS 07, GCXO 30, GCFV 01, GCHI 34).
5. Default limits when the AIP gives none: **5 kt tailwind, 15 kt crosswind**.

Special cases:

- **Night configurations (23:00–07:00 local) at LEBL and LEMD**, with a pop-up at 23:00 and 07:00. The
  current configuration may be kept while traffic is heavy.
- **LEMD weekends:** in South configuration the night configuration is kept until 09:00 local on Saturday
  and Sunday.
- **Low visibility at LEBL and LEMD** (ceiling below 500 ft or visibility below 1.9 km): preferential
  configurations are suspended and the wind decides; LEBL does not use 02/20.
- **AFIS aerodromes** (LESU, LEBA, LEBG, LEHC, GCGM, and GCHI at weekends): no runway is selected, pilots
  decide. If runways are ticked there, a "deselect all runways" reminder appears at startup, with
  `.rwy check` and when GCHI switches between tower (weekdays) and AFIS (weekends, Canary time).
- **METAR older than 2 hours** (airport closed): its runways are deselected at startup and no suggestion
  is made for it.
- **Airports without rules** in the sector file (e.g. the foreign airports of the LECM sector): their
  runways are deselected at startup.
- **No internet at startup:** the plugin waits for the first METARs before showing the startup pop-up.
- Canary Islands airports (GC..) use Canary time automatically.

## Installation

With the VATSIM Spain sector package the plugin is loaded automatically (see
[docs/OPERACIONES.md](docs/OPERACIONES.md)). To install it by hand:

1. Download `ESRunwayPlugin.zip` from the [Releases](../../releases) page and extract the
   `ESRunwayPlugin` folder anywhere, e.g. next to your other plugins. **`LEXX.json` must stay in the same
   folder as the DLL.**
2. In EuroScope: **OTHER SET → Plug-ins… → Load** → `ESRunwayPlugin.dll`.
3. In the same dialog, select the plugin and move **Standard ES radar screen** (and **Ground Radar display**
   if you use it) to *Allowed to draw on types* (`<<`), so the pop-ups can be drawn. Without it, messages
   still appear in the **RWY** chat tab.

Works with EuroScope 3.2.x (32-bit).

## Pop-ups and log

- Pop-ups can be moved by dragging their title bar. Long ones scroll with the **▲ / ▼** buttons. Several
  pop-ups queue up ("1 of 3").
- Everything is also written to the **RWY** chat tab (long summaries are shortened there; the full list is
  in the log and in `.rwy status`).
- `ESRunwayPlugin.log` next to the DLL records METARs, decisions and actions. **It is deleted every time the
  plugin loads**, so save it before reopening EuroScope when reporting a problem.

## Commands

| Command | |
|---|---|
| `.rwy status [ICAO]` | Active and recommended configuration per airport, with the reason |
| `.rwy check` | Re-check now (also downloads the METARs again) |
| `.rwy startup` | Show the startup pop-up again (set all runways via the dialog) |
| `.rwy accept` / `.rwy decline` | Answer the current suggestion from the command line |
| `.rwy airports` | Airports handled in this sector, and those without rules (cleared) |
| `.rwy find` | Check that the runway icon is found in the top bar |
| `.rwy capture` | Save a picture of the top bar next to the DLL (for support) |

## Rules file (`LEXX.json`)

One file for all of Spain, next to the DLL. It can be edited without rebuilding the plugin; the plugin
reads it when it loads. Runway names must be written exactly as in the sector file.

Top level:

| Field | Meaning |
|---|---|
| `defaultTimezone` | `"Europe/Madrid"`; airports starting with `GC` use `"Atlantic/Canary"` automatically |
| `defaults` | `maxTailwindKt` / `maxCrosswindKt` used when an airport has no `limits` |
| `approachRanking` | Approach types from best to worst |
| `airports` | One entry per ICAO code |

Per airport:

| Field | Meaning |
|---|---|
| `source` | AIP section and amendment the rules come from (check it at each AIRAC) |
| `selection` | `"preferential"`: configurations in AIP order. `"approach"`: best `approach` first |
| `limits` | `maxTailwindKt`, `maxCrosswindKt` (gusts included) |
| `configurations` | List of `{ "name", "arr": [...], "dep": [...], "approach" }`, best first |
| `schedules` | Instead of `configurations`: `{ "name", "from": "HH:MM", "to": "HH:MM", "configurations" }` in local time (e.g. LEBL/LEMD `day` and `night`) |
| `weekendVariant` | On a configuration: `{ "configuration", "days": ["SAT","SUN"], "until": "HH:MM" }` uses another configuration on those days until that time (LEMD) |
| `calmConfiguration` | Configuration name used with calm or variable wind |
| `preferentialSuspendedBelow` | `{ "ceilingFt", "visibilityM" }`: below these the wind decides |
| `notInLowVisibility` | Runways not used in low visibility |
| `afis` | `true`: AFIS aerodrome, no runways selected |
| `afisDays` | With `afis`: AFIS only on these days, e.g. `["SAT","SUN"]` (GCHI) |
| `timezone` | Optional, overrides the default |
| `notes` | Free text for humans; ignored by the plugin |

Unit tests (`tests/CoreTests.cpp`) load the real `config/LEXX.json` and check many decisions, so a rules
change that breaks something is caught by the build.

## Building

GitHub Actions builds the DLL, runs the unit tests and packages the release on every push
(`.github/workflows/build.yml`). A tag `vX.Y.Z` publishes a release with `ESRunwayPlugin.zip`
(DLL + `LEXX.json`). Locally you need the MSVC C++ Build Tools and CMake:

```
cmake -S . -B build -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release
```

`sdk/` contains the EuroScope plugin SDK (`EuroScopePlugIn.h`, `EuroScopePlugInDll.lib`) from the
EuroScope installation.

## Licence

Copyright (C) 2026 Fabien Capmarty

This program is free software: you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version. It is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See [LICENSE](LICENSE) for the full text.

The EuroScope plugin SDK files in `sdk/` belong to the EuroScope project and are included only to
build the plugin; they are not covered by this licence. Runway rules in `config/LEXX.json` are derived
from the AIP España published by ENAIRE.
