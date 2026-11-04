# Android Build & Run Instructions

## Install & Run

Install the signed APK from the [Android release](https://github.com/Bulud-AS/demo-benchmark/releases/tag/android-v0.1.3). It supports Android 10+ on ARM64 devices and includes its rendering data and audio.

The app starts in standby mode. Select the demo, rendering backend and quality, then choose Benchmark or Play. Benchmark saves results and ratings locally and supports sharing them as JSON. Play does not save a benchmark score.

Playback and measurement begin in landscape. Status is shown at the top left, Stop at the bottom right, and the portrait/landscape toggle at the bottom left. Changing orientation keeps the current run active.

## Build

Use JDK 17, Android SDK platform 36, NDK 28.2.13676358 and CMake 3.22.1. The repository includes Gradle Wrapper 8.11.1. Configure the SDK path using `ANDROID_HOME` or `sdk.dir` in an ignored `local.properties` file in this directory.

Run from the `android/` directory:

```sh
./gradlew assembleRelease
```

On Windows, use `gradlew.bat assembleRelease`. The default release targets ARM64 only. The unsigned APK is generated at `app/build/outputs/apk/release/app-release-unsigned.apk`.

Material3 is pinned to `1.4.0-alpha18` for its public Material 3 Expressive APIs. Vulkan shaders are compiled by the pinned NDK during the build. Run `prepare-assets.py` only when the source timeline or credits change; the generated Android assets are already included in Git.

## Signing

Use `sign-apk.ps1` on Windows with an Android signing keystore. Set `DB_STORE_PASSWORD` in the process environment and provide the SDK directory, keystore and output path:

```powershell
.\sign-apk.ps1 -Sdk $env:ANDROID_HOME -KeyStore 'C:\keys\DemoBenchmark-Android.jks' -Output '.\DemoBenchmark-Android-arm64.apk'
```

Use the same signing key for app updates. Keep the key and password outside Git. Generated APKs are distributed through Releases rather than committed to the repository.

## Verification Status

This is an alpha build. Short emulator runs covered installation, both rendering backends, orientation changes, stopping and result display. Physical ARM64 devices, audible audio output, full playback of both demos and official hardware performance scores remain unverified.
