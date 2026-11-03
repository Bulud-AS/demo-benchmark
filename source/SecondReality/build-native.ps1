param([string]$Zig)
$ErrorActionPreference='Stop'
if (!$Zig) { $Zig=(Get-Command zig -ErrorAction Stop).Source }
$output=Join-Path $PSScriptRoot '..\..\runtime\SecondRealityCore.exe'
New-Item -ItemType Directory -Path (Split-Path $output) -Force | Out-Null
$sources=Get-ChildItem -LiteralPath $PSScriptRoot -Recurse -Filter '*.cpp' | Where-Object { $_.Name -ne 'Graphics.cpp' } | ForEach-Object FullName
& $Zig c++ -target x86_64-windows-gnu -std=c++20 -O2 -DNDEBUG -D_CRT_SECURE_NO_WARNINGS -I $PSScriptRoot $sources -o $output -lwinmm -Wno-narrowing -Wno-int-to-pointer-cast -Wno-deprecated-declarations
if($LASTEXITCODE -ne 0){throw 'Second Reality native build failed'}
