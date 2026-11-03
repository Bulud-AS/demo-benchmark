param([ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version = '1.1.0')
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
& (Join-Path $projectRoot 'build.ps1')
$executable = Join-Path $projectRoot 'Demo Benchmark.exe'
$fileVersion = (Get-Item -LiteralPath $executable).VersionInfo.FileVersion
if ($fileVersion -ne "$Version.0") { throw "Executable version $fileVersion does not match v$Version" }
$nativeImage = [IO.File]::ReadAllBytes((Join-Path $projectRoot 'runtime\NativeGraphics.dll'))
if ($nativeImage.Length -lt 256 -or $nativeImage[0] -ne 77 -or $nativeImage[1] -ne 90) { throw 'NativeGraphics.dll is not a Windows PE image' }
$peOffset = [BitConverter]::ToInt32($nativeImage,60)
if ([BitConverter]::ToUInt16($nativeImage,$peOffset+4) -ne 0x8664 -or ([BitConverter]::ToUInt16($nativeImage,$peOffset+22) -band 0x2000) -eq 0) { throw 'NativeGraphics.dll is not an x64 DLL' }

$dist = [IO.Path]::GetFullPath((Join-Path $projectRoot 'dist'))
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$stage = Join-Path $dist ('stage-' + [Guid]::NewGuid().ToString('N'))
$package = Join-Path $stage 'Demo Benchmark'
$archive = Join-Path $dist "DemoBenchmark-v$Version-Windows-x64.zip"
New-Item -ItemType Directory -Path $package -Force | Out-Null
try {
    foreach ($name in @('assets','runtime','notices')) {
        Copy-Item -LiteralPath (Join-Path $projectRoot $name) -Destination (Join-Path $package $name) -Recurse -Exclude '*.pdb','*.lib','*.a'
    }
    foreach ($name in @('Demo Benchmark.exe','LICENSE.txt','CREDITS.txt')) {
        Copy-Item -LiteralPath (Join-Path $projectRoot $name) -Destination (Join-Path $package $name)
    }
    New-Item -ItemType Directory -Path (Join-Path $package 'results') | Out-Null
    $readme = [IO.File]::ReadAllText((Join-Path $projectRoot 'README.md'))
    $readme = $readme.Replace('# Demo Benchmark', "# Demo Benchmark v$Version")
    $readme = $readme.Replace('For build instructions and how to run the application, please refer to [BUILD.md](BUILD.md).', 'Extract the complete ZIP, keep assets/ and runtime/ beside the executable, and launch Demo Benchmark.exe. Change the window size or switch to fullscreen before starting a benchmark.')
    [IO.File]::WriteAllText((Join-Path $package 'README.txt'), $readme, (New-Object Text.UTF8Encoding($false)))

    $hashes = [ordered]@{}
    Get-ChildItem -LiteralPath $package -Recurse -File | Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($package.Length + 1).Replace('\','/')
        $hashes[$relative] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    [IO.File]::WriteAllText((Join-Path $package 'SHA256.json'), ($hashes | ConvertTo-Json -Depth 3), (New-Object Text.UTF8Encoding($false)))
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
    [IO.Compression.ZipFile]::CreateFromDirectory($stage, $archive, [IO.Compression.CompressionLevel]::Optimal, $false)
    Write-Output $archive
    Get-FileHash -LiteralPath $archive -Algorithm SHA256
} finally {
    $resolvedStage = [IO.Path]::GetFullPath($stage)
    $distPrefix = $dist + [IO.Path]::DirectorySeparatorChar
    if (-not $resolvedStage.StartsWith($distPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid staging path' }
    if (Test-Path -LiteralPath $resolvedStage) { Remove-Item -LiteralPath $resolvedStage -Recurse -Force }
}
