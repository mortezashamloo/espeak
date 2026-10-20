#!/bin/bash
# Builds setup_espeak_1.48.15.exe (NSIS) from build-out/ + espeak-data/.
# Run tools/build-windows-dll.sh first. If build-out/sapi/espeak_sapi-x86.dll (and optionally -x64.dll) exist
# (tools/sapi/build_sapi.py, Windows only) the SAPI5 voices are included and registered.  Needs: makensis (apt install nsis).
# Output: build-out/setup_espeak_1.48.15.exe
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build-out"
STAGE="$OUT/stage"
for f in espeak-x86_64.dll espeak-i686.dll espeak-cli-x86_64.exe espeak-cli-i686.exe espeak-voices-i686.exe; do
  [ -f "$OUT/$f" ] || { echo "missing $OUT/$f - run tools/build-windows-dll.sh first"; exit 1; }
done
for f in phontab phondata phonindex intonations fa_dict; do
  [ -f "$ROOT/espeak-data/$f" ] || { echo "missing espeak-data/$f - run tools/compile-dicts.sh"; exit 1; }
done
rm -rf "$STAGE"; mkdir -p "$STAGE/x64" "$STAGE/x86"
cp "$OUT/espeak-x86_64.dll" "$STAGE/x64/espeak.dll"
cp "$OUT/espeak-cli-x86_64.exe" "$STAGE/x64/espeak-cli.exe"
cp "$OUT/espeak-i686.dll" "$STAGE/x86/espeak.dll"
cp "$OUT/espeak-cli-i686.exe" "$STAGE/x86/espeak-cli.exe"
cp "$OUT/espeak-voices-i686.exe" "$STAGE/espeak-voices.exe"
cp -r "$ROOT/espeak-data" "$STAGE/espeak-data"
SAPI_DEF=""
if [ -f "$OUT/sapi/espeak_sapi-x86.dll" ]; then
  mkdir -p "$STAGE/sapi-x86"
  cp "$OUT/sapi/espeak_sapi-x86.dll" "$STAGE/sapi-x86/espeak_sapi.dll"
  SAPI_DEF="-DWITH_SAPI"
  echo "including the 32-bit SAPI5 engine"
  if [ -f "$OUT/sapi/espeak_sapi-x64.dll" ]; then
    mkdir -p "$STAGE/sapi-x64"
    cp "$OUT/sapi/espeak_sapi-x64.dll" "$STAGE/sapi-x64/espeak_sapi.dll"
    SAPI_DEF="$SAPI_DEF -DWITH_SAPI_X64"
    echo "including the 64-bit SAPI5 engine"
  else
    echo "no 64-bit SAPI5 DLL - 64-bit programs will not see the SAPI5 voices"
  fi
else
  echo "no SAPI5 DLL in build-out/sapi - building the installer without them"
fi
cp "$ROOT/License.txt" "$ROOT/ChangeLog.txt" "$ROOT/tools/README-windows-build.txt" "$STAGE/"
makensis -V2 $SAPI_DEF -DSTAGE="$STAGE" "$ROOT/tools/installer/espeak.nsi"
rm -rf "$STAGE"
ls -l "$OUT/setup_espeak_1.48.15.exe"
