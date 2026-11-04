# Demo Benchmark — Android

A lightweight demo benchmark for Android 10+ devices with a 64-bit ARM processor. Supports OpenGL ES 3.0 and Vulkan 1.1, with a Material 3 Expressive UI. Unsupported rendering backends are unavailable for selection.

[Download v0.1.3 Beta for Android ARM64](https://github.com/Bulud-AS/demo-benchmark/releases/download/android-v0.1.3-beta/DemoBenchmark-Android-v0.1.3-beta-arm64.apk). Install the signed APK; it includes the required rendering data and audio and can update the previous Android build. For the Windows version, see the [main README](../README.md).

## Demo Overview

- **Median**: For comparing real-time 3D rendering performance (approx. 130 seconds). Five quality levels adjust model detail, rendering resolution and MSAA.
- **Second Reality**: For testing rendering, audio and synchronization. Runs at original playback speed with uncapped offscreen rendering. High quality upscales the original software-rendered image using interpolation.

*Note: Because Second Reality may redisplay identical original frames, its rating serves as a reference for display processing only. Do not directly compare scores between the two demos.*

## Score & Rating Guidelines

Android measures GPU-completed offscreen frames independently of the screen refresh rate. Compare results using the same demo, rendering backend, quality and app version. Android and Windows scores are not directly comparable.

Version 0.1.3 Beta uses profile `android-offscreen-v4`: fixed resolution in either orientation, subpass MSAA resolve, fewer render-pass restarts and precise synchronization. Earlier profiles are not directly comparable. Shared JSON includes GPU timestamps where supported, CPU/wait times, per-second frame/workload counts, CPU topology/ADPF status and six image probes collected before measurement. See the [device analysis](docs/device-performance.md).

$$\text{Score} = \text{round}\left(\frac{\text{Average Rendering FPS}}{60} \times 1000\right)$$

Official scores and ratings are recorded only when a full run completes on a hardware GPU under valid conditions. The "pts" shown during a run is a provisional value based on the cumulative average starting 3 seconds after playback begins. Interrupted, failed and software-GPU runs still display results with the reason, but receive no official score.

| Average Rendering FPS | Score Range | Rating |
| --- | --- | --- |
| 120 or higher | 2000+ | Very Smooth |
| 60 – <120 | 1000 – <2000 | Smooth |
| 30 – <60 | 500 – <1000 | Playable |
| 15 – <30 | 250 – <500 | Slow |
| Below 15 | Below 250 | Unplayable |

- Ratings are determined using the unrounded average FPS.
- Scores and ratings are guidelines specific to the selected demo and settings. Device temperature and power-saving settings can affect results.
- Ratings for Second Reality describe display processing rather than the original effect rendering speed.

For build instructions and how to run the application, see [BUILD.md](BUILD.md).

## Credits & Licenses

- **Median**: Ümlaüt Design / Gargaj and original authors (2016)
- **Second Reality**: Future Crew (1993) / C++ port: XorJS / Conspiracy / Music playback: 8bitbubsy
- **Android UI & Audio**: Kotlin / Jetpack Compose / Material 3 Expressive / AndroidX / Oboe
- **Data & Image Libraries**: nlohmann JSON / stb_image

Application code: [MIT](../LICENSE.txt). Original demo assets and music have separate terms. See [Android credits](CREDITS.txt) and the full license texts included in the app.
