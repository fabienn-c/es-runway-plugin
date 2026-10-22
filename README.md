# ES Runway Plugin

EuroScope plugin for VATSIM Spain that recommends and sets the active runways of every airport in the
runway dialog from the VATSIM METAR, the AIP preferential configurations, the best available approach and
day/night rules.

**Status: beta.** LECB and LECM airports are configured; GCCC follows.

## What it does

- **At startup** (as soon as the sector and METARs are loaded) it sets the runways of every airport of
  the runway dialog (airports without rules, e.g. foreign ones, get all runways deselected) and shows a summary pop-up. It never touches the airport DEP/ARR ticks, only runways.
- **Afterwards**, every new METAR and every day/night change (LEBL 23:00 / 07:00 LT) is checked. If the
  recommended configuration differs from the active one, a pop-up offers **Accept** / **Decline**.
  Nothing changes without the controller accepting.
  - DEL / GND / TWR: suggestions only for their own airport.
  - APP / CTR (and when not connected): every airport.
- Rules per airport (see `config/LEXX.json`):
  1. AIP preferential configuration first, within its wind limits (gusts included).
  2. Without an AIP preference: the runway with the best approach (ILS CAT III > … > visual), if the
     wind allows; equal approaches: most headwind.
  3. Otherwise the configuration the wind suits best. Calm or variable wind keeps the preferential one.
- METARs come from `metar.vatsim.net` (checked every minute) and from EuroScope itself when connected.

## Installation

1. Download `ESRunwayPlugin.zip` from the [Releases](../../releases) page and extract the
   `ESRunwayPlugin` folder (DLL + `LEXX.json` …) anywhere, e.g. next to your other plugins.
2. In EuroScope: **OTHER SET → Plug-ins… → Load** → `ESRunwayPlugin.dll`.
3. In the same dialog, select the plugin and move **Standard ES radar screen** to *Allowed to draw on
   types* (`<<`), so the pop-ups can be drawn. Without it, messages still appear in the **RWY** chat tab.

Works with EuroScope 3.2.x (32-bit). Details are logged to `ESRunwayPlugin.log` next to the DLL.

## Commands

| Command | |
|---|---|
| `.rwy status [ICAO]` | Active and recommended configuration per airport, with the reason |
| `.rwy check` | Re-check now (also downloads the METARs again) |
| `.rwy startup` | Run the startup assignment again |
| `.rwy accept` / `.rwy decline` | Answer the current suggestion from the command line |
| `.rwy airports` | Airports handled in this sector, and those without rules (cleared) |
| `.rwy find` | Check that the runway icon is found in the top bar |
| `.rwy capture` | Save a picture of the top bar (for support) |

## Configuration files

One JSON file for all of Spain (`LEXX.json`), next to the DLL. Each airport lists its configurations in order of preference,
its wind limits and the AIP section they come from. They can be edited without rebuilding the plugin.

## Building

GitHub Actions builds the DLL, runs the unit tests and packages the release on every push
(`.github/workflows/build.yml`). Locally you need the MSVC C++ Build Tools and CMake:

```
cmake -S . -B build -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release
```

`sdk/` contains the EuroScope plugin SDK (`EuroScopePlugIn.h`, `EuroScopePlugInDll.lib`) from the
EuroScope installation.
