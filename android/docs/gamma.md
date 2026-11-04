# Median integrated benchmark — 0.1.3 Gamma

Android preview release. VersionCode 6; versionName 0.1.3-gamma.

## User-facing behavior

Median measurement now runs CPU image filtering and memory copy/verification concurrently with its original rendering, music, camera and timeline. The result emphasizes one composite score and display-update FPS. Detailed component counters remain in shared JSON. Playback does not start the mixed CPU/memory load. No separate CPU/memory screens or pre/post-Median test sequence were added.

The profile is `median-system-gamma-v1`, a combined-load index, not an isolated CPU or GPU peak benchmark. It does not cover storage, NPU or ray-tracing hardware. Earlier Median scores and AnTuTu/Geekbench scores are not comparable.

## Why the old FPS looked smoother than the screen

Beta counted GPU-completed offscreen frames while submitting its measurement preview at only 30 Hz. High offscreen throughput could therefore coexist with visibly coarse motion. This was confirmed directly in Engine.cpp; it is not evidence of a specific SoC fault. Frame delivery and rendering throughput are separate quantities. [Android frame pacing overview](https://developer.android.com/games/sdk/frame-pacing), [Samsung GPUWatch surface FPS](https://developer.samsung.com/galaxy-gamedev/gpuwatch-userguide/gpuwatch_v2.html).

Gamma receives display-vsync callbacks from Choreographer and sends at most one preview for each new callback, without the old fixed 30 Hz limit. Playback waits for new callbacks instead of generating an uncontrolled queue. Measurement keeps offscreen rendering uncapped. The native presentation timestamp follows the display callback's monotonic time plus one display interval; a late frame is not held artificially to make the number look better. The platform still chooses the panel refresh rate.

Where supported, EGL_ANDROID_get_frame_timestamps or VK_GOOGLE_display_timing records actual presentation times. Otherwise the result explicitly reports `presentation-submission`, measuring successful update submission frequency rather than claiming panel scan-out FPS. Advertising an extension does not guarantee immediately available feedback, so Gamma uses submission timing until at least two actual timestamps exist. The main metric is labeled display-update FPS; offscreen FPS and the source are available in details. The timing provider is capability-gated, not selected by SoC name. [Khronos EGL timestamps](https://registry.khronos.org/EGL/extensions/ANDROID/EGL_ANDROID_get_frame_timestamps.txt), [Android Vulkan timing](https://developer.android.com/games/develop/vulkan/frame-pacing-extensions).

Android recommends Swappy as a general higher-level pacing solution. Gamma uses the existing renderer with display callbacks and native timing extensions; it does not claim to implement all of Swappy's queue-depth and pacing heuristics. Smoothness on physical devices still requires verification. A display-update submission rate alone cannot prove perfectly smooth scan-out.

## SoC/GPU review and implementation policy

| Family | Technical consideration | Gamma policy |
| --- | --- | --- |
| Snapdragon / Adreno | Qualcomm documents tiled rendering and recent tile-memory capabilities. On-chip memory, attachment traffic and supported extensions vary by generation. | Retain the existing subpass MSAA resolve and precise synchronization; use standard APIs unless a device advertises the optional timing feature. No assumption that every Adreno supports recent Qualcomm extensions. |
| Dimensity / Arm GPU | Mali's geometry/binning and fragment work can overlap; broad barriers and unnecessary attachment round trips can prevent that. | Preserve Beta's stage-specific barriers, compatible-pass reuse and inline MSAA resolve. Query actual color/depth sample counts, including medium's 2x-to-1x fallback. |
| Exynos | Exynos 2500 uses a customized RDNA-3-based Xclipse 950. This does not justify using desktop AMD scheduling assumptions or extending that identification to every Exynos generation. | Record GPU/device/driver identity and use advertised capabilities. No Exynos FPS multiplier, clamp or unverified vendor workaround. |
| Tensor | Google platform generations must be identified individually. The AOSP Zuma configuration contains Arm graphics-buffer/AFBC settings, but that is not a complete GPU specification for every Tensor. | Do not hardcode a universal Tensor GPU family, extension set or core count. Use runtime renderer/driver/capability data. |
| Other detected GPUs, including PowerVR | Imagination documents tile-based deferred rendering and the importance of render-pass structure. A CPU/SoC brand is insufficient to choose a graphics path. | Keep the same validated color/depth/resolve and timing fallback rules; no unsupported cross-family hardware assumptions. |

Primary references: [Qualcomm tile memory](https://www.qualcomm.com/developer/blog/2026/05/high-performance-memory-extension-optimize-memory), [Arm synchronization](https://developer.arm.com/community/arm-community-blogs/b/mobile-graphics-and-gaming-blog/posts/vulkan-synchronization-how-pipeline-barriers-map-to-mali-gpu-hardware), [Samsung Exynos 2500](https://semiconductor.samsung.com/processor/mobile-processor/exynos-2500/), [Google Zuma configuration](https://android.googlesource.com/device/google/zuma/+/refs/heads/main/BoardConfig-common.mk), [Imagination render passes](https://docs.imgtec.com/performance-guides/graphics-recommendations/html/topics/the-powervr-with-vulkan-advantage.html), [Khronos tile-renderer guidance](https://docs.vulkan.org/guide/latest/tile_based_rendering_best_practices.html).

These are architectural considerations and source-level changes, not completed physical-device tests on those families. No public source cited here establishes their expected Median FPS ratios.

## Mixed workload and index

Native workers alternate a deterministic five-point integer image filter on 128x128 tiles and 64 KiB copy/verification jobs. The total initialized source/destination working set is 64 MiB. Workers own disjoint tile sequences, consume output hashes and verify copies; failed verification prevents a valid score. Reported copy traffic includes the copy's read/write and verification reads. This is workload throughput, not a claim of isolated DRAM peak bandwidth.

Hardware uses the process's available CPU count minus one worker (minimum one, maximum 32), with no pinned CPU IDs. Lower worker priority leaves scheduling headroom for display/audio; render and worker threads request ADPF hints where supported. Software-GPU diagnostic runs use one worker and never receive official scores. All workers stop on cancellation and are joined before result completion. System thermal/power controls remain active.

The first three playback seconds are warm-up. CPU/memory jobs started before the measurement boundary are not counted. The prototype composite index is:

`round(1000 * (renderFPS / 60)^0.5 * (filteredPixelsPerSecond / 20000000)^0.3 * (copyAndVerificationBytesPerSecond / 1000000000)^0.2)`

The denominators and weights are explicit normalization conventions, not measurements of a reference phone. A hardware score requires successful natural completion, valid accounting and valid mixed-work outputs. Interrupted/test/software runs retain diagnostics but no official score. This initial profile needs repeatability and physical-device calibration before general hardware rankings are defensible.

## Verification scope

Short emulator runs exercise both graphics backends and concurrent Median work. Native checks cover the score arithmetic, submission/feedback distinction, late/out-of-order feedback, unsupported-MSAA selection, and worker stop lifecycle. The display metric and offscreen rate are kept independently in results. Unsupported actual-present feedback is tested as an explicit fallback, not replaced with invented actual timestamps.

Physical Snapdragon, Dimensity, Exynos and Tensor devices, extension-backed actual scan-out timing, full Median playback, audio hardware and real performance improvement are not established by emulator results. The next physical comparison should use Gamma on both phones and inspect the presentation source, frame-time distribution and mixed-work counts, with matching backend, quality and actual MSAA.
