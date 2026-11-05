param(
    [Parameter(Mandatory=$true)][string]$Sdk,
    [Parameter(Mandatory=$true)][string]$KeyStore,
    [Parameter(Mandatory=$true)][string]$Output,
    [string]$InputApk = "$PSScriptRoot\app\build\outputs\apk\release\app-release-unsigned.apk",
    [string]$Alias = 'demobenchmark'
)
$ErrorActionPreference = 'Stop'
$tools = Join-Path $Sdk 'build-tools\36.0.0'
if (!(Test-Path -LiteralPath $InputApk)) { throw 'Release APK was not found. Run gradlew assembleRelease first.' }
if (!(Test-Path -LiteralPath $KeyStore)) { throw 'Signing keystore was not found.' }
if (!$env:DB_STORE_PASSWORD) { throw 'Set DB_STORE_PASSWORD in the process environment before signing.' }
$destination = [IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
$aligned = Join-Path ([IO.Path]::GetTempPath()) (([guid]::NewGuid().ToString()) + '.apk')
try {
    & "$tools\zipalign.exe" -P 16 -f 4 $InputApk $aligned
    if ($LASTEXITCODE -ne 0) { throw 'APK alignment failed.' }
    & "$tools\apksigner.bat" sign --ks $KeyStore --ks-key-alias $Alias --ks-pass env:DB_STORE_PASSWORD --key-pass env:DB_STORE_PASSWORD --v1-signing-enabled true --v2-signing-enabled true --v3-signing-enabled true --v4-signing-enabled false --out $destination $aligned
    if ($LASTEXITCODE -ne 0) { throw 'APK signing failed.' }
    & "$tools\apksigner.bat" verify --verbose --print-certs $destination
    if ($LASTEXITCODE -ne 0) { throw 'APK signature verification failed.' }
    & "$tools\zipalign.exe" -c -P 16 -v 4 $destination | Select-Object -Last 1
    if ($LASTEXITCODE -ne 0) { throw 'Signed APK alignment verification failed.' }
    Get-FileHash -LiteralPath $destination -Algorithm SHA256
} finally {
    if (Test-Path -LiteralPath $aligned) { Remove-Item -LiteralPath $aligned }
}
