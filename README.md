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
| `.rwy info` | Which sector file is loaded and where its `.rwy` file is |
| `.rwy status LEBL` | Active runways as EuroScope reports them |
| `.rwy write LEBL 24R 24L` | Writes the `.rwy` file **while EuroScope runs** |
| `.rwy arm LEBL 06R 06L` | Writes the `.rwy` file at the **next EuroScope start** |
| `.rwy watch` | For 60 s, logs any dialog that opens. Open the *Active airport/runway* dialog meanwhile |
| `.rwy windows` | Logs every EuroScope window |

Runway lists are comma-separated: `.rwy write LEBL 24R,02 24L`.

The first write keeps a backup of your original file as `<sector>.rwy.bak`.

## Building

The DLL is built by GitHub Actions on every push (`.github/workflows/build.yml`). To build locally
you need the MSVC C++ Build Tools and CMake:

```
cmake -S . -B build -A Win32
cmake --build build --config Release
```

`sdk/` contains the EuroScope plugin SDK (`EuroScopePlugIn.h`, `EuroScopePlugInDll.lib`) from the
EuroScope installation.
