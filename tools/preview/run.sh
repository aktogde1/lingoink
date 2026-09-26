#!/usr/bin/env bash
# Host preview: render all LingoInk screens to BMPs in out/preview/.
# Run from the repo root. Uses g++ when available, otherwise MSVC Build
# Tools (vcvars64). Also needs an ArduinoJson copy fetched by PlatformIO.
set -euo pipefail
cd "$(dirname "$0")/../.."

OUT=out/preview
BIN=out/preview_tool
mkdir -p "$OUT"

# ArduinoJson: prefer the native env copy, fall back to esp32c3's.
AJ=""
for d in .pio/libdeps/native/ArduinoJson/src .pio/libdeps/esp32c3/ArduinoJson/src; do
  if [ -f "$d/ArduinoJson.h" ]; then AJ="$d"; break; fi
done
if [ -z "$AJ" ]; then
  echo "ArduinoJson not found — run 'pio run -e esp32c3' once to fetch it." >&2
  exit 1
fi

SRC=(tools/preview/main.cpp src/ui/Canvas.cpp src/ui/FontRegistry.cpp
     src/course/LessonLoader.cpp src/course/CourseCatalog.cpp
     src/progress/ProgressStore.cpp)
INCD=(tools/preview/shim tools/preview src "$AJ")

echo "compiling preview..."
GPP=""
if command -v g++ >/dev/null 2>&1; then
  GPP="g++"
elif [ -x "$USERPROFILE/mingw64/bin/g++.exe" ]; then
  GPP="$USERPROFILE/mingw64/bin/g++.exe"  # portable WinLibs toolchain
fi
if [ -n "$GPP" ]; then
  # -static: the exe must run without the MinGW runtime DLLs on PATH.
  "$GPP" -std=c++17 -O1 -Wall -Wextra -static -DPLATFORM_ESP32 \
    $(for d in "${INCD[@]}"; do printf -- "-I%s " "$d"; done) \
    "${SRC[@]}" -o "$BIN"
elif [ -f "/c/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat" ]; then
  # MSVC: compile through a response file to dodge bash/cmd quoting.
  VC="/c/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat"
  { printf '/nologo /std:c++17 /EHsc /O2 /W3 /utf-8 /DPLATFORM_ESP32\r\n'
    for i in "${INCD[@]}"; do printf '/I"%s"\r\n' "$(cygpath -w "$i")"; done
    for s in "${SRC[@]}"; do printf '"%s"\r\n' "$(cygpath -w "$s")"; done
    printf '/link /OUT:"%s"\r\n' "$(cygpath -w "$BIN.exe")"
  } > out/preview_cl.rsp
  cat > out/preview_cl.bat <<EOF
@call "$(cygpath -w "$VC")" >nul || exit /b 1
cl @$(cygpath -w "$(pwd)/out/preview_cl.rsp") || exit /b 1
EOF
  cmd //c "$(cygpath -w "$(pwd)/out/preview_cl.bat")"
  BIN="$BIN.exe"
else
  echo "No host compiler found: install g++ (MinGW) or MSVC Build Tools." >&2
  exit 1
fi

echo "running preview..."
./"$BIN"

# BMP -> PNG (viewable everywhere); needs Pillow in the system python.
if python -c "import PIL" >/dev/null 2>&1; then
  python tools/preview/to_png.py
else
  echo "note: Pillow not found — BMPs left unconverted in $OUT"
fi
