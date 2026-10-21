# ES Runway Plugin

EuroScope plugin for VATSIM Spain that recommends and sets active runways for every airport in the
FIR (LECB, LECM, GCCC) from the VATSIM METAR, preferential configurations and day/night rules.

**Status: proof of concept.** This version only tests whether a plugin can change EuroScope's active
runways. The weather logic and pop-ups come next.

## Download

Get `ESRunwayPlugin.dll` from the [Releases](../../releases) page (or the latest
[Actions](../../actions) run). Works with EuroScope 3.2.x (32-bit).

## Proof-of-concept test

1. In EuroScope: **OTHER SET → Plug-ins… → Load**, pick `ESRunwayPlugin.dll`.
2. Type the commands below in the EuroScope command line. Replies appear in the **RWY** chat tab;
   details go to `ESRunwayPlugin.log` next to the DLL.

| Command | What it tests |
|---|---|
| `.rwy info` | Controller, facility and connection |
| `.rwy status LEBL` | Active runways as EuroScope reports them |
| `.rwy spy` | For 60 s, logs what EuroScope does. Open the runway dialog by clicking its icon meanwhile: the plugin remembers that click |
| `.rwy open` | The plugin opens the runway dialog by itself, then closes it |
| `.rwy set LEBL 24R 24L` | The plugin opens the dialog (or waits for you to open it), ticks the runway boxes and presses OK. Airport ticks are never touched |
| `.rwy watch` | For 60 s, logs any dialog that opens |
| `.rwy windows` | Logs every EuroScope window |

Runway lists are comma-separated, `-` for none: `.rwy set LEBL 24R,02 24L`.

## Building

The DLL is built by GitHub Actions on every push (`.github/workflows/build.yml`). To build locally
you need the MSVC C++ Build Tools and CMake:

```
cmake -S . -B build -A Win32
cmake --build build --config Release
```

`sdk/` contains the EuroScope plugin SDK (`EuroScopePlugIn.h`, `EuroScopePlugInDll.lib`) from the
EuroScope installation.
