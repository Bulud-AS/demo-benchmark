# Demo Benchmark

A lightweight demo benchmark for Windows PCs and Android devices.

## Downloads

| Platform | Version | Download |
| --- | --- | --- |
| Windows x64 | v1.1.0 | [ZIP with EXE and required files](https://github.com/Bulud-AS/demo-benchmark/releases/download/v1.1.0/DemoBenchmark-v1.1.0-Windows-x64.zip) |
| Android 10+ / ARM64 | v0.1.1 Preview | [Signed APK](https://github.com/Bulud-AS/demo-benchmark/releases/download/android-v0.1.1/DemoBenchmark-Android-v0.1.1-arm64.apk) |

Windows: extract the complete ZIP and run `Demo Benchmark.exe`, keeping `assets/` and `runtime/` beside it. Android: install the APK; it can update the previous Android build. The APK includes its rendering data and audio. The two platforms have separate version numbers.

- **Windows rendering**: OpenGL 3.3, native Direct3D 9 / 9Ex / 11 / 12, and Direct3D 11 / Vulkan via ANGLE. For older GPUs, try Direct3D 9 / 9Ex or native Direct3D 11. Direct3D 12 needs Windows 10+ and a compatible GPU. Windows XP and 32-bit Windows are not supported.
- **Android rendering**: OpenGL ES 3.0 / Vulkan 1.1, with a Material 3 Expressive UI. This is a prototype; see [Android details](android/README.md).

## Demo Overview
- **Median**: For comparing real-time 3D rendering performance (approx. 130 seconds). Please compare results using identical precision, rendering backend, and screen resolution settings.
- **Second Reality**: For testing rendering, audio, and synchronization. Runs at original playback speed with uncapped display FPS. When set to high quality, the image is upscaled using interpolation.

*Note: Because Second Reality may redisplay identical original frames, its rating serves as a reference for display processing only. Do not directly compare scores between the two demos.*

## Score & Rating Guidelines

Compare results within the same platform, demo, rendering backend, quality and version. Windows uses display FPS; Android uses GPU-completed offscreen frames. Windows and Android scores are not directly comparable.

$$\text{Score} = \text{round}\left(\frac{\text{Average Display FPS}}{60} \times 1000\right)$$

Official scores and ratings are recorded only when a run completes under valid conditions. The "pts" shown during a run is a provisional value based on the cumulative average starting 3 seconds after launch. Runs that do not qualify for an official score will still display results along with the reason.

| Average FPS | Score Range | Rating |
| --- | --- | --- |
| 120 or higher | 2000+ | Very Smooth |
| 60 – <120 | 1000 – <2000 | Smooth |
| 30 – <60 | 500 – <1000 | Playable |
| 15 – <30 | 250 – <500 | Slow |
| Below 15 | Below 250 | Unplayable |

- Ratings are determined using the unrounded average FPS.
- On Windows, even if the average FPS is high, you may experience stuttering if the 1% Low FPS is low.
- Scores and ratings are guidelines specific to the selected demo and settings.
- Ratings for Second Reality are prefixed with `Display: ` (e.g., `表示：` in the Japanese UI).

For build instructions and how to run the application, please refer to [BUILD.md](BUILD.md).

Sources: Windows code and runtime files are in `source/` and `runtime/`; Android code and build files are in [`android/`](android/README.md). Shared Median data is in `assets/`, and shared notices are in `notices/`. Download binaries from Releases.

## Credits & Licenses
- **Median**: Ümlaüt Design / Gargaj and original authors (2016)
- **Second Reality**: Future Crew (1993) / Windows port: XorJS / Conspiracy / Music playback: 8bitbubsy
- **Rendering Translation & Loader**: ANGLE Project / Vulkan Loader: Khronos Group and others

Application code: [MIT](LICENSE.txt). Original demo assets and music have separate terms. See [Windows/shared credits](CREDITS.txt) and [Android credits](android/CREDITS.txt).
