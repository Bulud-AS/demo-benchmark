# Demo Benchmark — Android

A lightweight demo benchmark for Android 10+ devices with a 64-bit ARM processor. Supports OpenGL ES 3.0 and Vulkan 1.1, with a Material 3 Expressive UI. Unsupported rendering backends are unavailable for selection.

[Download v0.1.3 Gamma for Android ARM64](https://github.com/Bulud-AS/demo-benchmark/releases/download/android-v0.1.3-gamma/DemoBenchmark-Android-v0.1.3-gamma-arm64.apk). Install the signed APK; it includes the required rendering data and audio and can update the previous Android build. For the Windows version, see the [main README](../README.md).

## Demo Overview

- **Median**: For an overall benchmark combining real-time 3D rendering, CPU image filtering and memory processing during the demo (approx. 130 seconds). Five quality levels adjust model detail, rendering resolution and MSAA. Playback mode omits the CPU/memory benchmark load.
- **Second Reality**: For testing rendering, audio and synchronization. Runs at original playback speed with uncapped offscreen rendering. High quality upscales the original software-rendered image using interpolation.

*Note: Because Second Reality may redisplay identical original frames, its rating serves as a reference for display processing only. Do not directly compare scores between the two demos.*

## Score & Rating Guidelines

Median Gamma uses profile `median-system-gamma-v1`: one composite index from rendering (50%), CPU filtering (30%) and memory processing (20%), combined using a weighted geometric mean. Higher is better; 1,000 points is a normalization convention, not a measured reference phone. This prototype index is not a complete assessment of every phone subsystem.

Display-update FPS is shown separately from uncapped offscreen rendering FPS. Actual presentation timestamps are used where supported; otherwise details identify the metric as presentation submission frequency. That fallback does not prove smooth panel scan-out.

| Median Display-Update FPS | 95th-Percentile Frame Interval | Rating |
| --- | --- | --- |
| 90 or higher | At most 14 ms | Very Smooth |
| 55 or higher | At most 25 ms | Smooth |
| 27 or higher | At most 50 ms | Playable |
| 13 or higher | At most 100 ms | Slow |
| Otherwise | — | Unplayable |

Use the highest row whose two conditions are met. These labels describe display updates, separately from the composite score. Second Reality remains a display-processing reference: its score uses `round(offscreen rendering FPS / 60 × 1000)`, while its display-update rating uses FPS thresholds of 120 / 60 / 30 / 15 for the same five labels.

Official scores require a valid full run on a hardware GPU. The first three seconds are warm-up; live points are provisional. Interrupted, failed and software-GPU runs retain diagnostics but no official score. Compare only the same profile, app version, demo, backend, quality and actual MSAA count. Gamma scores are incompatible with earlier Android versions, Windows, AnTuTu and Geekbench. Device temperature and power settings affect results. For the formula, technical notes and verification limits, see the [Gamma notes](docs/gamma.md).

For build instructions and how to run the application, see [BUILD.md](BUILD.md).

## Credits & Licenses

- **Median**: Ümlaüt Design / Gargaj and original authors (2016)
- **Second Reality**: Future Crew (1993) / C++ port: XorJS / Conspiracy / Music playback: 8bitbubsy
- **Android UI & Audio**: Kotlin / Jetpack Compose / Material 3 Expressive / AndroidX / Oboe
- **Data & Image Libraries**: nlohmann JSON / stb_image

Application code: [MIT](../LICENSE.txt). Original demo assets and music have separate terms. See [Android credits](CREDITS.txt) and the full license texts included in the app.
