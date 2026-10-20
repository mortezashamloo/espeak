#!/bin/bash
# Cross-compiles espeak 1.48.15 as a Windows DLL (synchronous mode, like the old
# platforms/windows/windows_dll project) with MinGW-w64.  Run from anywhere:
#   bash tools/build-windows-dll.sh
# Output: build-out/espeak-x86_64.dll, espeak-i686.dll and espeak-cli-<arch>.exe (WAV writer)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$ROOT/build-tmp"
OUT="$ROOT/build-out"
rm -rf "$WORK" "$OUT"
mkdir -p "$WORK/src" "$OUT"
cp "$ROOT"/src/* "$WORK/src/"
cd "$WORK/src"

# Windows-only build: remove the stale MSVC stdint.h if it was copied, patch headers.
rm -f stdint.h
python3 - <<'PY'
s = open('speech.h', encoding='latin-1').read()
start = s.index('#define PLATFORM_POSIX')
end = s.index('#define __cdecl') + len('#define __cdecl')
win = ('#define PLATFORM_WINDOWS\n#define __WIN32__\n#define NEED_WCSTOF\n#define NEED_GETOPT\n'
       "#define PATHSEP '\\\\'\n#define NO_VARIADIC_MACROS\n#define LIBRARY")
s = s[:start] + win + s[end:]
# synchronous DLL: no pthread/async code
marker = '#ifdef LIBRARY\n#define USE_ASYNC\n#endif'
assert marker in s
s = s.replace(marker, marker + '\n#ifdef PLATFORM_WINDOWS\n#undef USE_ASYNC\n#endif', 1)
# long64 must hold a pointer: 'unsigned long' is only 32 bits on 64-bit Windows
old = 'typedef unsigned long long64;'
assert old in s
s = s.replace(old, '#ifdef _WIN64\ntypedef unsigned long long long64;\n#else\n' + old + '\n#endif', 1)
open('speech.h', 'w', encoding='latin-1').write(s)
PY

cat > StdAfx.h <<'H'
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
H

SRCS="compiledict dictionary intonation klatt sonic numbers phonemelist readclause setlengths speak_lib synth_mbrola synthdata synthesize tr_languages translate voices wavegen"
for ARCH in x86_64 i686; do
  CXX=$ARCH-w64-mingw32-g++
  OBJ="$WORK/obj-$ARCH"; mkdir -p "$OBJ"
  for s in $SRCS; do
    $CXX -c -O2 -fpermissive -w -include StdAfx.h -DWIN32 -D_WINDOWS -DNDEBUG -D_USRDLL -DESPEAK_EXPORTS -I. "$s.cpp" -o "$OBJ/$s.o"
  done
  $CXX -shared -o "$OUT/espeak-$ARCH.dll" "$OBJ"/*.o -static-libgcc -static-libstdc++ -lwinmm \
       -Wl,--out-implib,"$OUT/libespeak-$ARCH.a"
  echo "built $OUT/espeak-$ARCH.dll"
  $ARCH-w64-mingw32-gcc -O2 -municode -static-libgcc -o "$OUT/espeak-cli-$ARCH.exe" "$ROOT/tools/espeak-cli.c"
  echo "built $OUT/espeak-cli-$ARCH.exe"
  $ARCH-w64-mingw32-gcc -O2 -static-libgcc -o "$OUT/espeak-voices-$ARCH.exe" "$ROOT/tools/espeak-voices.c"
  echo "built $OUT/espeak-voices-$ARCH.exe"
done
rm -rf "$WORK"
