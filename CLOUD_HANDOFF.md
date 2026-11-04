# Demo Benchmark: Codex Cloud handoff

Updated: 2026-11-04 (Asia/Tokyo).

## Current request

The user wants to continue this project's desktop conversation in Codex Cloud.
This document preserves the decisions and current work state; it is a summary,
not a verbatim copy of the original chat. After reading it, confirm readiness in
Japanese and wait for the user's next development instruction. The Android
prototype has already been delivered; no new feature or rewrite is requested.

Repository: https://github.com/Bulud-AS/demo-benchmark
Handoff branch: `codex/android-cloud-handoff-20261104`.
Windows base: `ae45983e185b9356fd17e123c32637f693fbcaee` on `main`.
The Android work and this handoff are on the handoff branch. Check out that
branch if the saved Cloud environment initially selects `main`.

## User preferences and decisions

- Respond in Japanese. Finish usable artifacts and keep explanations concise.
- UI and app content must have minimal text. Do not add promotional copy.
- Keep verification targeted and short; do not run long demos unnecessarily.
- Do not start a measurement automatically on normal application launch.
- Display live score beside FPS during measurement and always show results
  after a completed measurement, including the first run after installation.
- Median's colored spheres must be pure red, not pink.
- Preserve the source demo's audio, animation, camera and playback speed.
  Do not substitute video playback for native 3D rendering.
- App code uses MIT. Original demo assets and music have separate rights and
  credits. Retain third-party license notices.
- The user previously requested removal of old commit history and empty
  commit descriptions. The existing main branch has one root commit with an
  empty message. This transfer uses a separate branch and an empty commit
  message. Do not rewrite or force-push main, tags or published releases.
- Never disable Windows protections as part of app setup or troubleshooting.

## Windows version

Existing Windows x64 application: Demo Benchmark, releases v1.0.0 / v1.1.0.
Rendering choices include OpenGL 3.3, native Direct3D 9 / 9Ex / 11 / 12,
and Direct3D 11 / Vulkan via ANGLE. Quality has five levels. Existing desktop
source, assets, runtime components and build instructions remain in place.
See the root README and BUILD.md. This handoff did not rerun Windows tests.
Linux Cloud does not itself establish Windows GUI or driver compatibility.

## Android prototype delivered

- Version 0.1.0, application ID `io.github.buludas.demobenchmark`.
- Android 10+ (minSdk 29), target/compile SDK 36, ARM64 release only.
- Android official interaction/design conventions are mandatory. Material 3
  Expressive is the primary design system. Apply common Apple HIG principles
  such as clarity and feedback; do not impose iOS controls on Android.
- Kotlin / Jetpack Compose UI; Material3 `1.4.0-alpha18` is intentionally
  pinned because its Expressive APIs are public. Stable 1.4.0 hides those APIs.
  The app uses `MaterialExpressiveTheme` and the Expressive loading indicator.
- Android-native OpenGL ES 3.0 and Vulkan 1.1 backends, not the original engine.
- Median reads the repository's meshes, cameras, animation, textures and PCM.
  Five qualities change sphere LOD, resolution and MSAA: 640x360/1,
  960x540/1, 1280x720/2, 1600x900/4, 1920x1080/4.
- Second Reality retains the original CPU effects and tracker music. A
  private service process provides fresh globals for each run and transfers
  frames through bounded shared memory. It keeps the original 70Hz timing.
  Original display quality is 640x480; interpolated quality is 1440x1080.
  This is presentation/compatibility testing, not a detailed 3D remaster.
- Benchmarking renders offscreen without a display-FPS cap. Playback uses
  normal display synchronization. Benchmark preview updates at 30Hz.
  The animation clock follows the audio clock rather than rendered frames.
- `android-offscreen-v1`: exclude the first 3 seconds, count GPU-completed
  frames, score = round(average FPS / 60 * 1000). Scores require natural full
  completion on a hardware GPU. Interrupted, short diagnostic, error and
  software-GPU runs have no official score. Windows scores are not comparable.
- Results include ratings, settings/device metadata and interruption reasons.
  Room stores history, DataStore stores settings, JSON results can be shared.
  Audio uses Oboe. No Internet, microphone or broad-storage permission is used.
- Dynamic color, dark/light/system theme, large text, scrollable settings,
  adaptive wider layout, accessible control names and Android back behavior
  are implemented. Only the running screen holds the keep-screen-on flag.

## Source and build

Read `android/README.md`. Important files:

- `android/app/src/main/java/io/github/buludas/demobenchmark/`: UI, ViewModel,
  results/settings, JNI declarations and private Second Reality service.
- `android/app/src/main/cpp/`: native scene, GLES, Vulkan, audio, JNI and SR host.
- `android/app/src/main/assets/shaders/`: shared GLSL source; Gradle compiles
  Vulkan SPIR-V with the pinned NDK's glslc before packaging.
- `assets/`, `notices/`, `source/SecondReality/`: shared existing project data.
- `android/third_party/`: vendored Oboe 1.9.3, nlohmann JSON 3.12.0, stb_image.
- `android/prepare-assets.py`: regenerate Android timeline JSON and notices
  only when source timeline/credits change. Generated assets are checked in.

Use JDK 17, Gradle Wrapper 8.11.1, SDK platform 36, Build Tools 36.0.0,
NDK 28.2.13676358 and CMake 3.22.1. AGP is 8.10.1, Kotlin is 2.1.20.
Set `ANDROID_HOME`/`ANDROID_SDK_ROOT` or an ignored `android/local.properties`.
On Linux: `cd android && ./gradlew assembleRelease`.
Release output: `app/build/outputs/apk/release/app-release-unsigned.apk`.
Default ABI is arm64-v8a. `-PqaAbi=x86_64` is for emulator diagnostics only.
Debug-only intent extras permit short/capture tests; release builds ignore
them and never auto-start from those extras.

SDK packages can be prepared with the environment's SDK manager:

```sh
sdkmanager "platforms;android-36" "build-tools;36.0.0" \
  "ndk;28.2.13676358" "cmake;3.22.1" "platform-tools"
```

The portable Windows SDK/JDK/Gradle caches, emulator, recordings, screenshots,
build outputs and signing credentials are local files, not repository data.
Do not assume that Windows drive paths exist in Cloud. A static check found
no quoted-header filename-case mismatches in the Android/shared SR source;
a Linux build has not yet been executed.

## Delivered artifact and validation limits

Local artifact: `DemoBenchmark-Android-v0.1.0-arm64.apk`, 71,027,229 bytes.
SHA-256: `17b80a64e66c29d84177ec20715c30e71b9b629607e854a1e39a47a3d87c66b4`.
Certificate SHA-256:
`c201e00ae416c094073fd0f3f490c14d3da033813d6ed7c0e2370c561c590934`.
The signed APK is local-only; it was not uploaded to Git or a new release.

Verified locally:

- Release build and lintVitalRelease passed; APK signature v3 verified.
- ARM64-only ELF binaries and APK ZIP alignment support 16KB pages.
- Signed ARM64 APK installed and ran with ARM translation in an Android 15
  emulator. Median playback and stop worked with GLES and Vulkan.
- Debug short measurements returned and saved a result. Normal launch stayed
  on the settings screen. Software-GPU scores were correctly excluded.
- GLES/Vulkan captures at 2.7, 18, 38, 84, 100 and 124 seconds closely matched
  each other (mean absolute channel error up to 0.073 on a 0..255 scale).
  Five main scenes also closely matched the Windows reference; the ending
  frame differed, so do not claim perfect reproduction of every video frame.
- SR part 4 short tests in original and interpolated quality progressed at
  about 69.8-69.9 ticks/second. Dark theme and 2x font settings remained usable.

Not verified: physical ARM64 hardware, audible speaker/headphone output,
full uninterrupted playback of both demos, official hardware scores, every
quality level on physical GPUs, or complete guideline/accessibility coverage.
Do not represent emulator FPS as a device performance result.

Important fixes already applied: public GLES 3.0 varying syntax; avoid deleting
program object zero after the GLES capability probe; copy service message
options before starting its worker thread; per-swapchain-image Vulkan present
semaphores; combined depth/stencil layout transitions; cancellation before a
surface starts; visible feedback when playback initialization fails.

## Signing and release continuity

An Android RSA release key exists only on the user's original PC, outside Git.
Its password is protected with Windows-user DPAPI. Do not copy secrets into
the repository or this chat, create a replacement release key, or sign updates
with a different key. `android/sign-apk.ps1` signs and verifies artifacts locally.
Cloud can build an unsigned APK for local signing with the existing key.

Do not publish a release, alter repository visibility, or merge the transfer
branch merely because the conversation is being moved. Await the user's next
instruction after confirming that this context and branch are available.
