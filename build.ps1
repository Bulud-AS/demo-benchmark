$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$compiler = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$sourceFiles = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'source') -Filter '*.cs' | Select-Object -ExpandProperty FullName
& $compiler /nologo /target:winexe /platform:x64 /optimize+ /debug- /out:"$projectRoot\Demo Benchmark.exe" /win32manifest:"$projectRoot\source\app.manifest" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Core.dll /reference:System.Web.Extensions.dll /reference:System.Xml.Linq.dll $sourceFiles
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
