# eSpeak 1.48.15 – source tree and Windows build kit

eSpeak is the compact formant speech synthesizer by Jonathan Duddington (GPL v3 or later).
**1.48.15 (16 April 2015) is its last upstream release.** It was only ever published as a *development
source drop* on `espeak.sourceforge.net/test/latest.html`; that page now returns 404, and the
SourceForge file area only has 1.48.04 and older. This repository keeps the 1.48.15 source, and adds
a kit that builds a **Windows installer with SAPI5 voices (32-bit and 64-bit)** from it using GitHub Actions.

> This is **not** eSpeak NG. eSpeak NG is a separate, newer project with a different data format.
> Dictionaries and phoneme data of the two (and of 1.48.04 and 1.48.15) are not interchangeable.

**[Download the installer from the Releases page](../../releases)** (`setup_espeak_1.48.15.exe`).
Persian summary: [README.fa.md](README.fa.md).

## Where the source comes from, and how sure we are

| What | Result |
|---|---|
| Source of the tree | `rhdunn/espeak`, commit `df3989c633a3dfb5ea8b554a3e07db2c8e526a1b` ("1.48.15 (from http://espeak.sourceforge.net/test/latest.html)"), the head of its `development` branch. Note: that repository's default branch `master` is eSpeak NG history, and its tag `1.48.15` points to a *modified* commit (`c48bb4cd`, 110 files changed: autotools etc.), so neither of them is the pure 1.48.15 source |
| Files of that commit that are byte-identical here | 1816 of 1818 (only `README.md` → renamed to `README-upstream.md`, and `.gitignore` is different) |
| `src/` against Debian's `espeak_1.48.15+dfsg.orig.tar.gz` | every file both have is identical, including the Makefile (Debian's tarball leaves out the `espeakedit` GUI sources) |
| Version string inside `src/synthdata.cpp` | `1.48.15 16.Apr.15` |
| `platforms/windows/*` (windows_dll, windows_sapi, …) | identical to the official **1.48.04** source zip – upstream never changed them for 1.48.15 |
| `dictsource/` and `docs/` against the official `setup_espeak-1.48.15.exe` | identical (the installer additionally contains generated `dict_log`, `dict_phonemes`) |
| Compiled data `espeak-data/{phondata,phontab,phonindex,intonations}` | taken from Debian's 1.48.15 package; byte-identical to the official 1.48.15 Windows installer |
| Compiled dictionaries `espeak-data/*_dict` (71) | compiled by `tools/compile-dicts.sh` from `dictsource`; 70 are byte-identical to the official installer's, `hy_dict` differs by 2 bytes (same sources) |

Also present: `License.txt`, `ChangeLog.txt`, `ReadMe`, top-level `Makefile` (from the upstream zip as kept in
`nvaccess/espeak`, commit `dbee791`), and four extra test voices (`voices/test/{bn,eu,gu,te}`).
Never verified: a checksum of the original `espeak-1.48.15-source.zip` (no copy is available online), and the
`espeakedit` GUI program (not built).

## What was added (not part of upstream)

| Path | Purpose |
|---|---|
| `tools/compile-dicts.sh` | compiles `dictsource` into `espeak-data/*_dict` with a Linux build made from this same tree |
| `tools/build-windows-dll.sh` | MinGW-w64: `espeak.dll`, `espeak-cli.exe`, `espeak-voices.exe` (32- and 64-bit) |
| `tools/espeak-cli.c` | small command-line tool: `espeak-cli -v fa-en-us -w out.wav "text"` |
| `tools/sapi/build_sapi.py` | Visual C++ build of the SAPI5 engine from `platforms/windows/windows_sapi` (applies a few small fixes to a *copy* of the project; the files here are not modified) |
| `tools/espeak-voices.c` | registers the SAPI5 voices – a port of the `[Code]` section of the official `make_espeak.iss` |
| `tools/installer/espeak.nsi`, `tools/build-installer.sh` | NSIS installer |
| `.github/workflows/build.yml` | builds everything and (optionally) publishes a Release |

## Building (GitHub Actions)

1. Open **Actions → Build eSpeak 1.48.15 for Windows → Run workflow**.
2. Wait for the three jobs. Download the artifact `setup_espeak_1.48.15` (installer) or
   `espeak-1.48.15-windows-files` (DLLs and tools).
3. To publish a Release, type a tag such as `v1.48.15-win.1` into the *release_tag* box, or push a tag
   that starts with `v`. The release gets the installer, a zip of the DLLs and `SHA256SUMS.txt`.

The `sapi` job (Windows runner, Visual Studio) can fail without stopping the rest: the installer is then built
without SAPI5. The build was checked on a GitHub runner with MSVC 14.51 and Windows SDK 10.0.26100.

## The installer

* Installs `espeak.dll`, `espeak-cli.exe`, `espeak-data` and, if built, the SAPI5 engine to
  `C:\Program Files\eSpeak`. 64-bit Windows gets both the 32-bit and the 64-bit SAPI5 engine.
* A page asks **which voices to install** (default: `en en-us fa fa-en-us fa-en-us+max`). Voice names are
  eSpeak voice names; a variant can be added with `+`, e.g. `fa+m3`. (`fa` = Persian with UK English words,
  `fa-en-us` = Persian with US English words; `max` is not a real variant and sounds like plain `fa-en-us`.)
* Silent install: `setup_espeak_1.48.15.exe /S /VOICES="fa fa-en-us"`.
* Voices appear in SAPI5 applications (screen readers, etc.) as `eSpeak-FA`, `eSpeak-FA-EN-US`, …
* The installer does not include `TTSApp.exe`, the docs or `dictsource`.

## Status – what has and has not been tested

Tested: every source file compiles with MinGW (DLL) and with MSVC (SAPI engine, x86 and x64); the SAPI5 voices
were installed and heard on Windows; the 64-bit DLL, `espeak-cli` and `espeak-voices` were run under Wine.
Not tested: the 32-bit `espeak.dll`/CLI, silent install and uninstall on every Windows version, 64-bit SAPI5
voices in a 64-bit application, MBROLA voices, `espeakedit`.

## Licence and credits

GNU GPL v3 or later – see `License.txt`. eSpeak © Jonathan Duddington. Source history: Reece Dunn
(`rhdunn/espeak`), NV Access (`nvaccess/espeak`), the Debian eSpeak maintainers. The voice list and registry layout
follow the eSpeak installer script `make_espeak.iss`.
