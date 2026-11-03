param([string]$Zig)
$ErrorActionPreference='Stop'
if(!$Zig){$Zig=(Get-Command zig -ErrorAction Stop).Source}
$runtime=Join-Path $PSScriptRoot '..\..\runtime'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
$sources=Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.cpp' | ForEach-Object FullName
& $Zig c++ -target x86_64-windows-gnu -std=c++20 -O2 -shared $sources -o (Join-Path $runtime 'NativeGraphics.dll') -ldxguid -Wno-deprecated-declarations
if($LASTEXITCODE -ne 0){throw 'Native graphics build failed'}
$image=[IO.File]::ReadAllBytes((Join-Path $runtime 'NativeGraphics.dll'))
if($image.Length -lt 256 -or $image[0] -ne 77 -or $image[1] -ne 90){throw 'NativeGraphics.dll is not a Windows PE image'}
$pe=[BitConverter]::ToInt32($image,60)
if([BitConverter]::ToUInt32($image,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($image,$pe+4) -ne 0x8664 -or ([BitConverter]::ToUInt16($image,$pe+22) -band 0x2000) -eq 0){throw 'NativeGraphics.dll is not an x64 DLL'}
