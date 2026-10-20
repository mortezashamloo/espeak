eSpeak 1.48.15 for Windows - build kit added on top of the 1.48.15 source
(nothing under src/ or dictsource/ was changed)

Added files
  tools/sapi/build_sapi.py     builds the SAPI5 engine espeak_sapi.dll with Visual C++ (Windows runner)
  .github/workflows/build-windows-dll.yml   GitHub Actions workflow
  tools/compile-dicts.sh       compiles dictsource/*_rules,_list -> espeak-data/*_dict
  tools/build-windows-dll.sh   MinGW-w64 build: espeak.dll + espeak-cli.exe, 64- and 32-bit
  tools/espeak-cli.c           small command-line tool: writes speech to a WAV file
  tools/build-installer.sh     builds setup_espeak_1.48.15.exe with NSIS
  tools/installer/espeak.nsi   the installer script
  espeak-data/phontab, phondata, phonindex, intonations   compiled phoneme data (taken from
                               the Debian espeak 1.48.15 package; the source tree only has the
                               phsource files and needs espeakedit to compile them)
  espeak-data/*_dict           71 compiled dictionaries (70 byte-identical to Debian's;
                               hy_dict differs by 2 bytes from identical sources)

How to get the installer
  Put the contents of this folder in a GitHub repo, open Actions, run
  "Build Windows espeak (DLL + installer)", download the artifact "setup_espeak_1.48.15".
  The workflow recompiles the dictionaries from dictsource first, so edits to fa_list /
  fa_rules take effect in the installer.

SAPI5 voices (new, NOT TESTED - needs Windows to build and to try)
  The "sapi" job of the workflow builds espeak_sapi.dll (32- and 64-bit) from
  platforms/windows/windows_sapi + the 1.48.15 sources, using Visual C++ on a GitHub Windows
  runner, and the installer then registers it with regsvr32 and creates five SAPI voices in the
  registry (Persian [fa, English words with UK accent], Persian + English US [fa-en-us],
  Persian-Pinglish, English US, English UK), for both 32-bit and 64-bit apps.
  If the sapi job fails, the installer is still built, without SAPI5. If only the 32-bit engine
  is built, the installer includes just that one (64-bit programs then see no SAPI5 voices).
  Real run on a GitHub runner (VS 2026 / MSVC 14.51, SDK 10.0.26100): all sources compiled, the
  32-bit engine linked; the 64-bit link first failed on the project's 32-bit sapi.lib - fixed by
  dropping that library and defining the two SPDFID GUIDs (the SDK x64 sapi.lib is used).  The build script applies
  these fixes to a copy of the project: no VC6 stdint.h, _WIN32_WINNT 0x0601, __WIN64__ for x64,
  typelib not registered.  The 17 library sources were syntax-checked with the project's speech.h
  (MinGW); ttseng.cpp / ttsengobj.cpp (ATL + SAPI) could not be compiled outside Visual C++.
  Local test:  python tools/sapi/build_sapi.py --arch x64 --dry-run   (works on any OS)

What the installer installs (to C:\Program Files\eSpeak by default)
  bin\espeak.dll, bin\espeak-cli.exe  (64-bit files on 64-bit Windows, else 32-bit)
  espeak-data\, License.txt, ChangeLog.txt; Start menu entry and an uninstaller.
  With the SAPI5 build also: sapi\x86 and sapi\x64 and the four voices (see above).
  It does not plug into NVDA's own eSpeak driver; NVDA can use the SAPI5 voices.
  Try it:  espeak-cli.exe -v fa -w out.wav "سلام دنیا"

Notes
  - Languages mt, tn, tt (dictsource only) are skipped: their phonemes are not in the
    shipped phontab.
  - The DLL is synchronous-only (like the old windows_dll project).
  - The 64-bit DLL and espeak-cli were tested under Wine. The installer itself, the 32-bit
    files and everything SAPI5 were built or checked only, never run.
  - build-windows-dll.sh patches copies of speech.h at build time (Windows defines, no async,
    64-bit long64); the files in src/ stay as they are.

  Further fixes found by the real build: `_export` removed from TtsEngObj.h, SPDBG_FUNC defined
  as a no-op (missing from the Windows 10 SDK headers).

Voices (SAPI5)
  The installer has a "Select which voices to install" page, like the official installer.
  Type voice names separated by spaces. Default: en en-us fa fa-en-us fa-en-us+max
  (fa = Persian with UK English words, fa-en-us = Persian with US English words; "+max" is not a
  real espeak variant and sounds the same as plain fa-en-us).  Silent install:
      setup_espeak_1.48.15.exe /S /VOICES="fa fa-en-us"
  tools/espeak-voices.c is a port of the [Code] part of the official make_espeak.iss: same
  tokens (HKLM\Software\Microsoft\Speech\Voices\Tokens\eSpeak, eSpeak_1, ...), same language
  codes (fa = 429), same PhoneConverters handling; it also writes the 32-bit registry view and,
  when the 64-bit engine is included, the 64-bit view.  Tested under Wine (64-bit build): install,
  re-install with duplicates, remove.  The page itself (NSIS) was only compiled, never run.
