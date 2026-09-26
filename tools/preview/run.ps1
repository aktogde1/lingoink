# Host preview: render all LingoInk screens to BMPs in out\preview\.
# Run from the repo root (PowerShell). Uses MSVC Build Tools when present,
# otherwise g++. Also needs an ArduinoJson copy fetched by PlatformIO.
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..\..")

New-Item -ItemType Directory -Force -Path out\preview | Out-Null
$bin = "out\preview_tool.exe"

$aj = @(".pio\libdeps\native\ArduinoJson\src", ".pio\libdeps\esp32c3\ArduinoJson\src") |
  Where-Object { Test-Path (Join-Path $_ "ArduinoJson.h") } |
  Select-Object -First 1
if (-not $aj) {
  Write-Error "ArduinoJson not found - run 'pio run -e esp32c3' once to fetch it."
}

$src = @(
  "tools\preview\main.cpp",
  "src\ui\Canvas.cpp",
  "src\ui\FontRegistry.cpp",
  "src\course\LessonLoader.cpp",
  "src\course\CourseCatalog.cpp",
  "src\progress\ProgressStore.cpp"
)
$inc = @("-Itools\preview\shim", "-Itools\preview", "-Isrc", "-I$aj")

Write-Host "compiling preview..."
$vc = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (Test-Path $vc) {
  $args = @("/nologo", "/std:c++17", "/EHsc", "/O2", "/W3", "/utf-8", "/DPLATFORM_ESP32") + $inc + $src + @("/link", "/OUT:$bin")
  cmd /c "`"$vc`" >nul && cl $($args -join ' ')"
  if ($LASTEXITCODE -ne 0) { exit 1 }
} elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
  & g++ -std=c++17 -O1 -Wall -Wextra -DPLATFORM_ESP32 $inc $src -o $bin
  if ($LASTEXITCODE -ne 0) { exit 1 }
} else {
  Write-Error "No host compiler found: install MSVC Build Tools or g++."
}

Write-Host "running preview..."
& ".\$bin"
