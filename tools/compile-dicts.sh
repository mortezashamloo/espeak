#!/bin/bash
# Compiles dictsource/<lang>_rules + _list into espeak-data/<lang>_dict using a Linux
# build of eSpeak made from this same source tree (no audio, no PortAudio needed).
# Needs: g++.  Run from anywhere:  bash tools/compile-dicts.sh
# Languages whose phonemes are not in the shipped phontab (mt, tn, tt) fail and are skipped.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/src"
cp "$ROOT"/src/*.cpp "$ROOT"/src/*.h "$T/src/"
cp -r "$ROOT/dictsource" "$ROOT/espeak-data" "$T/"
for f in phontab phondata phonindex intonations; do
  [ -f "$T/espeak-data/$f" ] || { echo "missing espeak-data/$f (compiled phoneme data)"; exit 1; }
done
cd "$T/src"
SRCS="speak compiledict dictionary intonation readclause setlengths numbers synth_mbrola synthdata synthesize translate mbrowrap tr_languages voices wavegen phonemelist klatt sonic"
OBJS=""
for s in $SRCS; do
  g++ -c -O2 -fpermissive -Wno-narrowing -w -D_FILE_OFFSET_BITS=64 -DPATH_ESPEAK_DATA=\"/nonexistent\" -I. "$s.cpp" -o "$s.o" || exit 1
  OBJS="$OBJS $s.o"
done
g++ -o speak $OBJS -lstdc++ -lpthread || exit 1
export HOME="$T"
ok=0; bad=""
for f in "$T"/dictsource/*_rules; do
  l=$(basename "$f" _rules)
  rm -f "$T/espeak-data/${l}_dict"
  if (cd "$T/dictsource" && "$T/src/speak" --compile="$l") >/dev/null 2>&1 && [ -f "$T/espeak-data/${l}_dict" ]; then
    cp "$T/espeak-data/${l}_dict" "$ROOT/espeak-data/${l}_dict"; ok=$((ok+1))
  else bad="$bad $l"; fi
done
echo "dictionaries compiled: $ok   skipped:$bad"
