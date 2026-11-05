# Gamma regression and Xclipse investigation

Investigated on 2026-11-05. This diagnostic preview is for collecting device evidence, not a confirmed performance fix.

## Two separate observations

- The user reports approximately 30 to 5 FPS on **arrows Alpha / Vulkan** after Gamma. This is a Dimensity regression report, not evidence of an Exynos defect. No new physical-device JSON for this run is available yet.
- Earlier Galaxy Z Flip 7 results showed approximately 841–943 offscreen FPS. The available shared JSON identifies Xclipse 950, driver 104869905 and profile `android-offscreen-v1`; it lacks GPU timestamps, per-scene counters and image probes. Its frame-count arithmetic is consistent, but the file cannot establish full rendering correctness or screen smoothness. It is not a Gamma log.

The old Android main metric counted completed offscreen frames. Gamma's main FPS instead describes display updates, while retaining rendering throughput separately. Comparing those different metrics alone cannot establish a sixfold execution slowdown. The user's visible stuttering remains a separate issue to investigate.

## Confirmed application changes and risks

1. Gamma adds unbounded background CPU filtering and memory copy/verification during Median measurement. On an unrestricted eight-CPU process it starts seven workers; on ten CPUs it starts nine. Actual affinity can reduce these counts. More CPU cores therefore cause more competing work, rather than merely providing more rendering capacity.
2. The 64 MiB source/destination working set, full-buffer verification and shared atomic counters generate CPU/cache/interconnect traffic independently of rendered frames. The checksum atomic is written by every worker. Nice priority 5 does not reserve GPU bandwidth, guarantee CPU placement or isolate the power budget. This is a credible explanation for the arrows regression, not a physical-device measurement of its cause.
3. A separate ADPF session on each worker uses the same 1 ms target and reports every small job. A target is a scheduling request, not a maximum-performance switch. These periodic-work hints do not prove that an uncontrolled all-core workload improves a graphics benchmark. Android now rate-limits reports internally; this investigation does not assume that each call is necessarily a Binder transaction. [NDK Performance Hint API](https://developer.android.com/ndk/reference/group/a-performance-hint).
4. The preview changed from approximately 30 submissions per second to Choreographer-driven submissions. At 120 Hz that can produce up to four times as many presentation operations, even with unchanged offscreen scene resolution. Acquisition, final composition, presentation and feedback queries can cost different amounts on different drivers.
5. Gamma combines MAILBOX with a future desired-present timestamp. The Vulkan specification explicitly permits a not-yet-displayed image to be replaced, and warns that continuously doing so can prevent new images from being displayed. This is an application policy risk, not an identified Xclipse defect. The local build now requests future timestamps only in FIFO during normal operation; diagnostic timing-on cases deliberately preserve Gamma's policy to allow A/B comparison. Actual presentation feedback remains available. This has not been physically confirmed as the cause of the reported slowdown. [Vulkan WSI timing semantics](https://github.khronos.org/Vulkan-Site/spec/latest/chapters/VK_KHR_surface/wsi.html).
6. Fixed-frame geometry, simple title/credit sections and large GPU-rendered scenes have very different costs. An average over the entire demo does not prove that the heavy scenes render at that rate. Missing/incorrect output, occlusion, legal driver optimizations and different memory/resolve costs must be distinguished with image probes and per-scene timing.

Google documents CPU/GPU downclocking under excessive heat. That is a general mechanism, not proof that thermal throttling caused this particular report. Battery temperature is not a GPU temperature sensor. [Android power guidance](https://developer.android.com/games/optimize/power?hl=en), [profiling guidance](https://developer.android.com/games/optimize/gameperformance?hl=en).

## What Exynos architecture does and does not establish

Samsung specifies Exynos 2500 as ten CPU cores: Cortex-X925 at 3.3 GHz, two A725 at 2.74 GHz, five A725 at 2.36 GHz and two A520 at 1.8 GHz. Xclipse 950 is a customized RDNA 3 GPU. CPU counts, Arm GPU core counts and AMD compute-unit counts are not interchangeable performance units. [Samsung product specifications](https://semiconductor.samsung.com/processor/mobile-processor/), [Exynos 2500](https://semiconductor.samsung.com/processor/mobile-processor/exynos-2500/).

Public specifications do not establish this phone's driver scheduling, effective clocks, cache behavior, GPU utilization or an expected Median FPS. Xclipse must not be treated as either a Mali GPU or an ordinary desktop Radeon solely from the SoC name. Samsung recommends capability checks, validation and testing on multiple GPU architectures. AMD also documents benefits from good render-pass structure on immediate renderers; render-pass improvements are not Mali-only. [Samsung Vulkan recommendations](https://developer.samsung.com/galaxy-gamedev/resources/articles/usage.html), [AMD render passes](https://gpuopen.com/learn/vulkan-renderpasses/).

Beta's precise barriers, subpass MSAA resolve and target-pass reuse remain appropriate portable changes. Blindly discarding preserved attachments, removing synchronization, adding an Exynos multiplier or capping its score would hide evidence. The current code already queries supported MSAA counts. A fresh cross-device comparison must include actual samples and image content, not just the selected quality label.

## Local on-device diagnostic

Choose Median, backend and quality, then **描画診断**. It is silent and unscored; normal CPU/memory benchmark load remains unchanged; normal MAILBOX/IMMEDIATE presentation no longer requests future timestamps. Approximately 36 seconds of diagnostic rendering plus preparation compares the same animated Median segment, using two seconds of warm-up and about four seconds of measured work per case:

| Case | CPU/memory stress | Preview | Presentation timing |
| --- | --- | --- | --- |
| 1 | Off | Display callbacks | Gamma timing where supported |
| 2 | Gamma load | Display callbacks | Gamma timing where supported |
| 3 | Gamma load | Legacy 30 Hz maximum | Disabled |
| 4 | Off | Legacy 30 Hz maximum | Disabled |
| 5 | Off | Display callbacks | Disabled |
| 6 | Off, repeat of case 1 | Display callbacks | Gamma timing where supported |

Cases 1/2 isolate added load; 1/5 isolate optional timestamp scheduling/feedback; 4/5 compare preview cadence with timing disabled. Case 6 checks drift after the heavier cases. Case 3 changes both cadence and timing relative to case 2 and must not be interpreted as a single-variable test. The legacy case uses today's rendering optimizations and does not recreate the complete old APK.

Shared JSON contains CPU command preparation, frame-slot wait, Vulkan acquisition, queue-presentation duration, supported GPU timestamps, actual/submission display-update source, mixed-work counters, CPU affinity/frequency metadata where readable, periodic OS thermal/power/battery conditions and six pre-run image probes. Reported CPU frequency may be a kernel-reported policy frequency, not independently measured effective clock. GPU timings include dependencies and cannot simply be added to CPU times to derive FPS.

Interpretation requires fresh diagnostic JSON from both phones, ideally both APIs at matching quality/MSAA. If case 2 falls while case 1 recovers, investigate the mixed workload before a vendor driver workaround. If cases 1/5 differ strongly, inspect timestamp support and presentation behavior. If case 4 recovers relative to case 5, inspect acquisition/presentation pressure. If Exynos remains unusually fast without presentation pressure, inspect images and GPU time for the same scene before calling it a driver defect. Samsung's GPUWatch can independently show SurfaceFlinger FPS and CPU/GPU load. [GPUWatch](https://developer.samsung.com/galaxy-gamedev/gpuwatch-userguide/gpuwatch_v2.html).

This diagnostic can reveal associations and reproducible differences; it cannot independently distinguish DRAM contention from thermal/power limits, or diagnose faulty silicon. Perfetto/driver-level tracing may still be needed. No device-specific workaround, score correction or claimed restoration of 30 FPS has been applied without that evidence. The local future-timestamp policy correction is portable and still needs physical-device verification. The published Gamma release has not been changed.

## Proposed mixed-workload redesign, not implemented

Keep CPU/memory work inside Median, but define a reproducible amount of useful work per rendered frame with a bounded queue. Make that workload independent of detected CPU count, use a small worker pool with system placement, consume verified outputs, and stop admitting jobs when its queue is full. This provides a combined rendering throughput test instead of running an unrelated saturation loop on every available core. Select the job budget as part of a versioned benchmark profile, not dynamically to favor a particular phone or restore an arbitrary FPS target. CPU/GPU peak throughput cannot both be guaranteed under one shared thermal/power budget.

The diagnostic intentionally retains Gamma's existing load for comparison. It does not silently change that workload or make its scores comparable with the proposed redesign. Fresh on-device results should determine whether the next production change should prioritize mixed-workload scheduling, presentation policy, rendering correctness or a driver-specific issue supported by evidence.
