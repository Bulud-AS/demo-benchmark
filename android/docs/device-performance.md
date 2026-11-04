# Android device performance analysis — 0.1.3 Beta

Investigated on 2026-11-04. The reported Median/Vulkan/highest results are 21.5 FPS on arrows Alpha and 943.2 FPS on Galaxy Z Flip 7, a ratio of approximately 43.9. These are user observations, not laboratory measurements. The physical devices are not connected to this build environment.

## CPU and GPU architecture

| Device | Processor and CPU topology | GPU |
| --- | --- | --- |
| arrows Alpha | FCNT specifies Dimensity 8350 Extreme. The base 8350 specification lists four Cortex-A715 and four Cortex-A510 cores, with a maximum CPU clock of 3.35 GHz. | The base 8350 specification lists Mali-G615 MC6. Confirm the phone's actual GPU/driver with its result JSON. |
| Galaxy Z Flip 7 | The supplied Galaxy result identifies s5e9955 and Samsung Xclipse 950. Samsung describes Exynos 2500 as ten cores in a 1+7+2 arrangement: one Cortex-X925, seven Cortex-A725 and two Cortex-A520. | Samsung describes Xclipse 950 as a customized GPU based on AMD RDNA 3. |

Sources: [FCNT arrows Alpha](https://www.fcnt.com/product/arrows/m08-rm/), [MediaTek 8350 specifications](https://www.mediatek.com/products/smartphones/mediatek-dimensity-8350), [Samsung Exynos 2500](https://semiconductor.samsung.com/processor/mobile-processor/exynos-2500/).

CPU core count, Mali shader-core count and AMD compute-unit count are different quantities. They cannot be divided to predict this benchmark's FPS. A ten-core CPU does not imply that a graphics command-recording thread should occupy ten cores. GPU execution, memory traffic, command preparation and presentation can each limit throughput. Public product specifications do not establish actual sustained GPU frequency, utilization, driver correctness or the expected FPS ratio of these two phones.

Arm documents Mali's separate geometry/binning and fragment work, tile memory and synchronization constraints. Broad dependencies can prevent otherwise useful overlap. Samsung's public page does not expose every Xclipse scheduling detail; this investigation does not assume that its mobile GPU behaves identically to a desktop AMD product. [Arm synchronization analysis](https://developer.arm.com/community/arm-community-blogs/b/mobile-graphics-and-gaming-blog/posts/vulkan-synchronization-how-pipeline-barriers-map-to-mali-gpu-hardware).

## Findings in this application

The following were confirmed by source inspection, independently of the reported phone results:

1. Every image transition used ALL_COMMANDS on both sides with general memory access masks. This was safe but overly conservative for the renderer's known attachment, sampling and transfer operations.
2. The Vulkan backend used external vkCmdResolveImage operations for MSAA. That requires storing and rereading multisampled images instead of resolving in the subpass. Khronos documents substantial bandwidth costs for this pattern on mobile GPUs. [MSAA guidance](https://docs.vulkan.org/samples/latest/samples/performance/msaa/README.html).
3. Binding even the same destination ended and restarted its render pass. Textured draws also ended the pass before checking whether their source needed a transition. Static image layers could therefore cause avoidable attachment round trips.
4. All passes loaded prior attachment contents before explicit clearing. CLEAR load operations can avoid unnecessary reads when the complete attachment will be cleared. [Attachment guidance](https://docs.vulkan.org/samples/latest/samples/performance/render_passes/README.html).
5. Measurement used FIFO presentation for its preview. Presentation and acquisition behavior may introduce driver-dependent waits even though offscreen frame generation has no FPS limiter.
6. The old final drawCalls/triangles fields described only the most recent published frame. A value of one draw/two triangles near the ending does not prove that the full demo skipped its geometry. Total loaded triangles likewise do not prove that the GPU executed every intended fragment.

These are application bottlenecks or diagnostic limitations. They are not evidence of a defect in Exynos silicon. The approximately 44-fold difference remains unconfirmed until fresh comparable device logs and image probes are available.

## Beta implementation

The Vulkan backend now resolves MSAA in the subpass, combines consecutive work on the same target, uses CLEAR when starting a pass with a full clear, and expresses transitions with the relevant pipeline stages/access masks. MSAA color and depth contents are still stored where a later pass must preserve them; blindly discarding them or enabling lazy allocation would corrupt this compositor's interrupted passes.

Measurement prefers MAILBOX, then IMMEDIATE presentation when supported; FIFO remains the compatibility fallback. Playback keeps FIFO. The measured rendering loop remains uncapped, with three frames in flight and a 30 Hz preview. Audio continues to control animation time. No vendor-specific score multiplier, FPS clamp or artificial busy-work was added.

On supported Android versions, the render thread creates an ADPF performance-hint session with a 1 ms CPU-side work target and reports real work duration. This target is a scheduler hint, not a sleep or FPS cap. Older/unsupported systems continue without the session. Android chooses the CPU placement; no affinity to a numbered big/little core is forced. Android documents hints as requests that help the system allocate CPU resources, rather than a guarantee of maximum clock. [Performance Hint API](https://source.android.com/docs/core/perf/performance-hint-api).

The result records allowed CPU IDs and readable maximum-frequency/capacity metadata. Missing sysfs data remains missing. ADPF active/report counts mean the API accepted requests, not that maximum clocks were independently measured. Thermal and power policies remain in force. Median remains a graphics workload; it does not test peak NPU, ray-tracing, CPU-all-core or storage performance.

## Evidence recorded by profile android-offscreen-v4

- FPS remains GPU-completed frames divided by monotonic measured wall seconds after warm-up. Submitted and completed counts must agree before a final score is accepted.
- Supported Vulkan queues record timestamp pairs with the device's timestampPeriod and timestampValidBits. Results are read only after the frame fence completes. GPU milliseconds include the command-buffer interval and GPU dependencies; their reciprocal is not necessarily the end-to-end FPS. Unsupported timing remains null.
- CPU scene/command preparation, frame-slot waits and submit/present durations are separate. GPU and CPU times can overlap and should not simply be added to derive FPS.
- Per-playback-second records identify whether high throughput comes from a simple title/credit layer or a detailed 3D scene. A partial first/last bin is a frame count, not a guaranteed one-second FPS estimate.
- Six fixed-time image probes are read back before audio starts and before measurement. The 16x9 RGB samples, workload counts and hashes help reveal missing rendering. Cross-driver MSAA edge differences can change hashes, so compare image content rather than requiring identical hashes across phones.
- OS build, GPU/driver, quality, MSAA, conditions and benchmark profile accompany shared results. Earlier profiles are not directly comparable.

## Verification and remaining work

On the local software-GPU emulator, the six representative full-resolution, highest-quality images at 2.7, 18, 38, 84, 100 and 124 seconds were pixel-identical before and after the rendering optimization. Short Vulkan and GLES measurements verified count arithmetic, image probes and timing fields. Khronos synchronization validation was enabled for Vulkan; no VUID or synchronization-hazard errors were recorded in those checked scenes. These checks establish tested behavior only, not physical-device performance.

For the device comparison, use fresh results from this Beta on both phones with the same demo, backend, quality, sound setting and orientation. Compare per-second workload/probe content, GPU timing, CPU and presentation waits, then thermal/power state. Also compare GLES and Vulkan on the Galaxy. A persistently high Vulkan FPS alone is insufficient to diagnose an Exynos fault or certify that all of its hardware is fully utilized.
