# Run real lesson/reading/review/storage integration tests without hardware.
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..\..')
$compiler = Join-Path $env:USERPROFILE 'mingw64\bin\g++.exe'
if (-not (Test-Path $compiler)) { $compiler = (Get-Command g++ -ErrorAction Stop).Source }
$aj = @('.pio/libdeps/native/ArduinoJson/src','.pio/libdeps/esp32c3/ArduinoJson/src') | Where-Object { Test-Path "$_/ArduinoJson.h" } | Select-Object -First 1
if (-not $aj) { throw 'Run pio run -e esp32c3 first to obtain ArduinoJson.' }
New-Item -ItemType Directory -Force out/preview | Out-Null
& $compiler -std=c++17 -O1 -Wall -Wextra -static -DPLATFORM_ESP32 -Itools/preview/shim -Itools/preview -Isrc "-I$aj" tools/preview/learning_tests.cpp src/ui/Canvas.cpp src/ui/FontRegistry.cpp src/course/LessonLoader.cpp src/course/CourseCatalog.cpp src/progress/ProgressStore.cpp -o out/learning_tests.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./out/learning_tests.exe
exit $LASTEXITCODE
