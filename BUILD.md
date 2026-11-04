# Windows Build & Run Instructions

For the Android APK, use [android/README.md](android/README.md). Windows ZIPs and Android APKs are published as separate GitHub Releases; generated binaries are not committed to Git.

The target platform is Windows x64. A standard build uses the built-in Windows .NET Framework C# compiler.

Run the following commands from the root of the repository to build and launch the application:

```powershell
.\build.ps1
& '.\Demo Benchmark.exe'
```

The application starts in standby mode. Select your desired demo, rendering backend, and precision, then click "Play" (再生) or "Benchmark" (計測) to begin.
Please switch to fullscreen or change the window size before starting a benchmark run.

Built executables and benchmark results are excluded from Git tracking. Benchmark results are saved in the `results/` directory.

## Creating a Distribution ZIP

```powershell
.\package.ps1
```

This rebuilds the application and creates `dist/DemoBenchmark-v1.1.0-Windows-x64.zip` with the executable, README, required data/runtimes, credits, licenses and checksums. Source code and local benchmark results are excluded.

## Rebuilding Native Direct3D Backends

`runtime/NativeGraphics.dll` provides native Direct3D 9, 9Ex, 11 (Feature Level 10_0 or later), and 12 rendering. It loads the selected Windows graphics runtime dynamically. Direct3D 12 requires Windows 10 or later and a supported GPU driver; the other modes can run without the D3D12 runtime. The system supplies `d3dcompiler_47.dll`.

Rebuild the DLL with Zig 0.15.2 before packaging changes to its C++ sources:

```powershell
.\source\NativeGraphics\build-native.ps1 -Zig 'C:\tools\zig\zig.exe'
```

All backends use the same scene data, camera, timing, precision presets and score formula. Native Direct3D GPU timestamp measurements are not implemented; GPU ms is shown as unavailable. Frame FPS and scores remain available. Direct3D 9 device reset recreates GPU resources when the viewport or synchronization mode changes.

## Rebuilding the Second Reality Executable

A pre-built x64 executable built from the ported source code is bundled at `runtime/SecondRealityCore.exe`.
If you wish to rebuild it yourself, install [Zig 0.15.2](https://ziglang.org/download/) and run the following command:

```powershell
.\source\SecondReality\build-native.ps1 -Zig 'C:\tools\zig\zig.exe'
```

Modifications from the original code are documented in [source/SecondReality/CHANGES.txt](source/SecondReality/CHANGES.txt), and provenance details are recorded in [notices/SecondReality-provenance.json](notices/SecondReality-provenance.json).

## Data & Rendering Runtime

- `assets/`: Rendering and audio data for Median, along with quality-specific meshes.
- `source/SecondReality/`: Ported source code and embedded data for Second Reality.
- `runtime/angle-hardware/`: ANGLE and Vulkan Loader binaries for Direct3D 11 / Vulkan support. Build details can be found in `angle-build.json` within the same folder, while provenance and license information are located in `CREDITS.txt` and `notices/`.

*Note: The current repository configuration includes data intended for private use. The MIT License covering our custom code does not grant permission to publicly distribute the original demo assets.*
