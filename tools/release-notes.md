eSpeak **1.48.15** (the last upstream release, 16 April 2015) for Windows, built from the source in this repository
with GitHub Actions.

**Files**
- `setup_espeak_1.48.15.exe` – installer: `espeak.dll`, `espeak-cli.exe`, `espeak-data`, SAPI5 voices (32- and 64-bit engine on 64-bit Windows).
- `espeak-1.48.15-windows-files.zip` – the DLLs and tools without the installer (`espeak-*.dll`, `espeak-cli-*.exe`, `espeak-voices-*.exe`).
- `SHA256SUMS.txt` – checksums.

**Installing** – the installer asks which voices to install (default `en en-us fa fa-en-us fa-en-us+max`).
Silent install: `setup_espeak_1.48.15.exe /S /VOICES="fa fa-en-us"`.

This is eSpeak 1.48.15, not eSpeak NG. See the README for how the source was checked and what has been tested.
