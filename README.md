# Demo Benchmark

A Windows x64 benchmark tool designed for low-spec PCs. Supports OpenGL 3.3, native Direct3D 9 / 9Ex / 11 / 12, and Direct3D 11 / Vulkan via ANGLE.

[Download v1.1.0 for Windows x64](https://github.com/Bulud-AS/demo-benchmark/releases/tag/v1.1.0). Extract the complete ZIP and run `Demo Benchmark.exe`.

For older GPUs, try Direct3D 9 / 9Ex (Shader Model 3.0) or native Direct3D 11 (Feature Level 10_0 or later). Direct3D 12 requires Windows 10 or later and a compatible driver. Benchmark display FPS is uncapped; unsupported backends or precision settings are skipped in "All" mode. This is a Windows x64 application; adding legacy GPU backends does not add Windows XP or 32-bit support.

## Demo Overview
- **Median**: For comparing real-time 3D rendering performance (approx. 130 seconds). Please compare results using identical precision, rendering backend, and screen resolution settings.
- **Second Reality**: For testing rendering, audio, and synchronization. Runs at original playback speed with uncapped display FPS. When set to high quality, the image is upscaled using interpolation.

*Note: Because Second Reality may redisplay identical original frames, its rating serves as a reference for display processing only. Do not directly compare scores between the two demos.*

## Score & Rating Guidelines

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
- Even if the average FPS is high, you may experience stuttering if the 1% Low FPS is low.
- Scores and ratings are guidelines specific to the selected demo and settings.
- Ratings for Second Reality are prefixed with `Display: ` (e.g., `表示：` in the Japanese UI).

For build instructions and how to run the application, please refer to [BUILD.md](BUILD.md).

## Credits & Licenses
- **Median**: Ümlaüt Design / Gargaj and original authors (2016)
- **Second Reality**: Future Crew (1993) / Windows port: XorJS / Conspiracy / Music playback: 8bitbubsy
- **Rendering Translation & Loader**: ANGLE Project / Vulkan Loader: Khronos Group and others
