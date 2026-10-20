; NSIS installer for eSpeak 1.48.15 (Windows DLL build + data, optional SAPI5 voices).
; Built by tools/build-installer.sh.  WITH_SAPI includes/registers the 32-bit SAPI5 engine and adds a
; "which voices" page; WITH_SAPI_X64 additionally the 64-bit engine.
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

!ifndef STAGE
  !error "pass /DSTAGE=<staging dir>"
!endif

!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\eSpeak1.48.15"
!define DEFAULT_VOICES "en en-us fa fa-en-us fa-en-us+max"

Name "eSpeak 1.48.15"
OutFile "${STAGE}\..\setup_espeak_1.48.15.exe"
InstallDir "$PROGRAMFILES\eSpeak"
InstallDirRegKey HKLM "Software\eSpeak1.48.15" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

!insertmacro MUI_PAGE_DIRECTORY
!ifdef WITH_SAPI
  !include "nsDialogs.nsh"
  Var VoicesCtl
  Var VoicesText
  Page custom VoicesPageCreate VoicesPageLeave
!endif
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

!ifdef WITH_SAPI
Function .onInit
  ; silent installs:  setup_espeak_1.48.15.exe /S /VOICES="fa fa-en-us"
  StrCpy $VoicesText ""
  ${GetParameters} $0
  ${GetOptions} $0 "/VOICES=" $1
  StrCpy $VoicesText "$1"
FunctionEnd

Function VoicesPageCreate
  !insertmacro MUI_HEADER_TEXT "Select which voices to install" "Type voice names, or press Next to accept the defaults."
  StrCmp $VoicesText "" 0 +2
    StrCpy $VoicesText "${DEFAULT_VOICES}"
  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 40u "Voice names, separated by spaces. Examples: fa (Persian), fa-en-us (Persian with US English), en, en-us. A variant can be added with a plus sign, for example fa+m3."
  Pop $0
  ${NSD_CreateText} 0 44u 100% 12u "$VoicesText"
  Pop $VoicesCtl
  nsDialogs::Show
FunctionEnd

Function VoicesPageLeave
  ${NSD_GetText} $VoicesCtl $VoicesText
FunctionEnd
!endif

Section "eSpeak"
  SetOutPath "$INSTDIR\bin"
  ${If} ${RunningX64}
    File "/oname=espeak.dll" "${STAGE}\x64\espeak.dll"
    File "/oname=espeak-cli.exe" "${STAGE}\x64\espeak-cli.exe"
  ${Else}
    File "/oname=espeak.dll" "${STAGE}\x86\espeak.dll"
    File "/oname=espeak-cli.exe" "${STAGE}\x86\espeak-cli.exe"
  ${EndIf}

  SetOutPath "$INSTDIR\espeak-data"
  File /r "${STAGE}\espeak-data\*.*"

  SetOutPath "$INSTDIR"
  File "${STAGE}\License.txt"
  File "${STAGE}\ChangeLog.txt"
  File "${STAGE}\README-windows-build.txt"

!ifdef WITH_SAPI
  SetOutPath "$INSTDIR\bin"
  File "${STAGE}\espeak-voices.exe"
  SetOutPath "$INSTDIR\sapi\x86"
  File "/oname=espeak_sapi.dll" "${STAGE}\sapi-x86\espeak_sapi.dll"
  ${If} ${RunningX64}
!ifdef WITH_SAPI_X64
    SetOutPath "$INSTDIR\sapi\x64"
    File "/oname=espeak_sapi.dll" "${STAGE}\sapi-x64\espeak_sapi.dll"
    ; 64-bit regsvr32 for the 64-bit engine
    ${DisableX64FSRedirection}
    ExecWait '"$WINDIR\System32\regsvr32.exe" /s "$INSTDIR\sapi\x64\espeak_sapi.dll"'
    ${EnableX64FSRedirection}
!endif
    ; 32-bit regsvr32 for the 32-bit engine (used by 32-bit programs)
    ExecWait '"$WINDIR\SysWOW64\regsvr32.exe" /s "$INSTDIR\sapi\x86\espeak_sapi.dll"'
  ${Else}
    ExecWait '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\sapi\x86\espeak_sapi.dll"'
  ${EndIf}

  ; register the voices (same registry layout as the official eSpeak installer)
  StrCmp $VoicesText "" 0 +2
    StrCpy $VoicesText "${DEFAULT_VOICES}"
!ifdef WITH_SAPI_X64
  ExecWait '"$INSTDIR\bin\espeak-voices.exe" install "$VoicesText" "$INSTDIR" x64'
!else
  ExecWait '"$INSTDIR\bin\espeak-voices.exe" install "$VoicesText" "$INSTDIR"'
!endif
!endif

  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\eSpeak1.48.15" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "eSpeak 1.48.15"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "1.48.15"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1

  CreateDirectory "$SMPROGRAMS\eSpeak"
  CreateShortCut "$SMPROGRAMS\eSpeak\eSpeak folder.lnk" "$INSTDIR"
  CreateShortCut "$SMPROGRAMS\eSpeak\Uninstall eSpeak.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
!ifdef WITH_SAPI
  ExecWait '"$INSTDIR\bin\espeak-voices.exe" remove'
  ${If} ${RunningX64}
!ifdef WITH_SAPI_X64
    ${DisableX64FSRedirection}
    ExecWait '"$WINDIR\System32\regsvr32.exe" /u /s "$INSTDIR\sapi\x64\espeak_sapi.dll"'
    ${EnableX64FSRedirection}
!endif
    ExecWait '"$WINDIR\SysWOW64\regsvr32.exe" /u /s "$INSTDIR\sapi\x86\espeak_sapi.dll"'
  ${Else}
    ExecWait '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\sapi\x86\espeak_sapi.dll"'
  ${EndIf}
  RMDir /r "$INSTDIR\sapi"
!endif
  RMDir /r "$INSTDIR\bin"
  RMDir /r "$INSTDIR\espeak-data"
  Delete "$INSTDIR\License.txt"
  Delete "$INSTDIR\ChangeLog.txt"
  Delete "$INSTDIR\README-windows-build.txt"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  RMDir /r "$SMPROGRAMS\eSpeak"
  DeleteRegKey HKLM "Software\eSpeak1.48.15"
  DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
