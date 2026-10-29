; Installer for the WINCTRL build with the RMP TCAS/squawk mode.
; Build: makensis -DPLUGIN_XPL=<path to win_x64 winctrl.xpl> installer/winctrl-tcas.nsi

Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "nsDialogs.nsh"

!ifndef PLUGIN_XPL
    !error "Pass -DPLUGIN_XPL=<path to winctrl.xpl>"
!endif
!ifndef VERSION
    !define VERSION "1.0.2-tcas"
!endif

Name "WINCTRL RMP TCAS ${VERSION}"
OutFile "WINCTRL-RMP-TCAS-${VERSION}-setup.exe"
RequestExecutionLevel admin
InstallDir "C:\X-Plane 12"
ShowInstDetails show

!define PLUGIN_DIR "$INSTDIR\Resources\plugins\winctrl"

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TEXT "This installer replaces the WINCTRL plugin with a build that adds a TCAS/squawk mode to the WinWing RMP3 (ToLiSS, FlightFactor 777 and 737).$\r$\n$\r$\nA key of your choice (LOAD by default) toggles the TCAS mode. The left window shows TCAS, the right one the squawk. The large knob sets the thousands, the small knob the hundreds (0 to 7, no wrap-around); pressing the knob switches to tens and units. The arrows show the TCAS mode switch; holding them for 3 s sends IDENT and shows IDENT for 3 s. The code is set in the aircraft 3 s after the last knob turn.$\r$\n$\r$\nThe original winctrl.xpl is kept as winctrl.xpl.bak.$\r$\n$\r$\nClose X-Plane before installing."
!define MUI_DIRECTORYPAGE_TEXT_TOP "Select the X-Plane 12 root folder (the one containing X-Plane.exe)."
!define MUI_COMPONENTSPAGE_NODESC

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
Page custom AircraftPage AircraftPageLeave
Page custom ButtonPage ButtonPageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

!macro SetUpdaterLocked DIR VALUE
    ${If} ${FileExists} "${DIR}\skunkcrafts_updater.cfg"
        DetailPrint "skunkcrafts_updater.cfg: locked|${VALUE}"
        FileOpen $0 "${DIR}\skunkcrafts_updater.cfg" r
        FileOpen $1 "${DIR}\skunkcrafts_updater.cfg.new" w
        ${Do}
            ClearErrors
            FileRead $0 $2
            ${If} ${Errors}
                ${Break}
            ${EndIf}
            StrCpy $3 $2 7
            ${If} $3 == "locked|"
                FileWrite $1 "locked|${VALUE}$\r$\n"
            ${Else}
                FileWrite $1 $2
            ${EndIf}
        ${Loop}
        FileClose $0
        FileClose $1
        Delete "${DIR}\skunkcrafts_updater.cfg"
        Rename "${DIR}\skunkcrafts_updater.cfg.new" "${DIR}\skunkcrafts_updater.cfg"
    ${EndIf}
!macroend

Function .onInit
    ; X-Plane 12 records its install folders here, the first line is the latest one.
    ClearErrors
    FileOpen $0 "$LOCALAPPDATA\x-plane_install_12.txt" r
    ${IfNot} ${Errors}
        FileRead $0 $1
        FileClose $0
        ; Strip the trailing newline and any trailing backslash.
        ${Do}
            StrCpy $2 $1 1 -1
            ${If} $2 == "$\n"
            ${OrIf} $2 == "$\r"
            ${OrIf} $2 == "\"
                StrCpy $1 $1 -1
            ${Else}
                ${Break}
            ${EndIf}
        ${Loop}
        ${If} $1 != ""
            StrCpy $INSTDIR $1
        ${EndIf}
    ${EndIf}
FunctionEnd

Var AircraftToliss
Var AircraftFF777
Var AircraftZibo
Var CheckToliss
Var CheckFF777
Var CheckZibo

; Pre-ticks the boxes from an earlier install, everything on for a first one.
!macro LoadAircraftChoice VAR KEY
    ${If} ${VAR} == ""
        ReadINIStr ${VAR} "${PLUGIN_DIR}\rmp-tcas.ini" "aircraft" "${KEY}"
        ${If} ${VAR} != "false"
            StrCpy ${VAR} ${BST_CHECKED}
        ${Else}
            StrCpy ${VAR} ${BST_UNCHECKED}
        ${EndIf}
    ${EndIf}
!macroend

!macro SaveAircraftChoice VAR KEY
    ; Unset (page skipped) counts as ticked.
    ${If} ${VAR} == ${BST_UNCHECKED}
        WriteINIStr "${PLUGIN_DIR}\rmp-tcas.ini" "aircraft" "${KEY}" "false"
    ${Else}
        WriteINIStr "${PLUGIN_DIR}\rmp-tcas.ini" "aircraft" "${KEY}" "true"
    ${EndIf}
!macroend

Function AircraftPage
    !insertmacro MUI_HEADER_TEXT "Aircraft" "Aircraft in which the RMP3 TCAS mode is available."
    !insertmacro LoadAircraftChoice $AircraftToliss "toliss"
    !insertmacro LoadAircraftChoice $AircraftFF777 "ff777"
    !insertmacro LoadAircraftChoice $AircraftZibo "b737"

    nsDialogs::Create 1018
    Pop $0
    ${If} $0 == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 24u "Tick the aircraft in which the RMP3 TCAS/squawk mode should be available. In the others the RMP keeps working as before."
    Pop $0

    ${NSD_CreateCheckbox} 10u 32u 100% 12u "ToLiSS (A319, A320, A321, A330-900, A340-600)"
    Pop $CheckToliss
    ${NSD_SetState} $CheckToliss $AircraftToliss

    ${NSD_CreateCheckbox} 10u 50u 100% 12u "FlightFactor 777 v2"
    Pop $CheckFF777
    ${NSD_SetState} $CheckFF777 $AircraftFF777

    ${NSD_CreateCheckbox} 10u 68u 100% 12u "737 (Zibo, Level Up, default Laminar 737-800)"
    Pop $CheckZibo
    ${NSD_SetState} $CheckZibo $AircraftZibo

    nsDialogs::Show
FunctionEnd

Function AircraftPageLeave
    ${NSD_GetState} $CheckToliss $AircraftToliss
    ${NSD_GetState} $CheckFF777 $AircraftFF777
    ${NSD_GetState} $CheckZibo $AircraftZibo
FunctionEnd

Var TcasButton
Var ButtonList

Function ButtonPage
    !insertmacro MUI_HEADER_TEXT "TCAS mode key" "The RMP3 key that toggles the TCAS mode."
    ${If} $TcasButton == ""
        ReadINIStr $TcasButton "${PLUGIN_DIR}\rmp-tcas.ini" "rmp" "tcas_button"
        ${If} $TcasButton == ""
            StrCpy $TcasButton "LOAD"
        ${EndIf}
    ${EndIf}

    nsDialogs::Create 1018
    Pop $0
    ${If} $0 == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 36u "Choose the key that switches the RMP3 into the TCAS/squawk mode and back. While the TCAS mode is available, that key loses its normal radio function, so pick one you do not use (LOAD has none on most aircraft)."
    Pop $0

    ${NSD_CreateDropList} 10u 44u 120u 140u ""
    Pop $ButtonList
    ${NSD_CB_AddString} $ButtonList "LOAD"
    ${NSD_CB_AddString} $ButtonList "VHF1"
    ${NSD_CB_AddString} $ButtonList "VHF2"
    ${NSD_CB_AddString} $ButtonList "VHF3"
    ${NSD_CB_AddString} $ButtonList "HF1"
    ${NSD_CB_AddString} $ButtonList "HF2"
    ${NSD_CB_AddString} $ButtonList "AM"
    ${NSD_CB_AddString} $ButtonList "NAV"
    ${NSD_CB_AddString} $ButtonList "VOR"
    ${NSD_CB_AddString} $ButtonList "ILS"
    ${NSD_CB_AddString} $ButtonList "GLS"
    ${NSD_CB_AddString} $ButtonList "MLS"
    ${NSD_CB_AddString} $ButtonList "ADF"
    ${NSD_CB_SelectString} $ButtonList $TcasButton

    nsDialogs::Show
FunctionEnd

Function ButtonPageLeave
    ${NSD_GetText} $ButtonList $TcasButton
    ${If} $TcasButton == ""
        StrCpy $TcasButton "LOAD"
    ${EndIf}
FunctionEnd

Function .onVerifyInstDir
    IfFileExists "$INSTDIR\X-Plane.exe" +2
        Abort
FunctionEnd

Section "WINCTRL plugin with TCAS mode (required)" SecPlugin
    SectionIn RO

    ${If} ${FileExists} "${PLUGIN_DIR}\win_x64\winctrl.xpl"
    ${AndIfNot} ${FileExists} "${PLUGIN_DIR}\win_x64\winctrl.xpl.bak"
        DetailPrint "Backing up the original as winctrl.xpl.bak"
        CopyFiles /SILENT "${PLUGIN_DIR}\win_x64\winctrl.xpl" "${PLUGIN_DIR}\win_x64\winctrl.xpl.bak"
    ${EndIf}

    SetOutPath "${PLUGIN_DIR}\win_x64"
    SetOverwrite on
    File "/oname=winctrl.xpl" "${PLUGIN_XPL}"

    ; Fonts only fill in what is missing, so custom fonts stay untouched.
    SetOutPath "${PLUGIN_DIR}\fonts"
    SetOverwrite off
    File "..\fonts\*.*"
    SetOverwrite on

    !insertmacro SaveAircraftChoice $AircraftToliss "toliss"
    !insertmacro SaveAircraftChoice $AircraftFF777 "ff777"
    !insertmacro SaveAircraftChoice $AircraftZibo "b737"

    ${If} $TcasButton == ""
        StrCpy $TcasButton "LOAD"
    ${EndIf}
    WriteINIStr "${PLUGIN_DIR}\rmp-tcas.ini" "rmp" "tcas_button" "$TcasButton"
    ; Older test builds stored the 737 choice under "zibo".
    DeleteINIStr "${PLUGIN_DIR}\rmp-tcas.ini" "aircraft" "zibo"

    WriteUninstaller "${PLUGIN_DIR}\uninstall-winctrl-tcas.exe"
SectionEnd

Section "Block automatic WINCTRL updates (Skunkcrafts)" SecLock
    ; Without this an update of WINCTRL overwrites the TCAS build.
    SetOutPath "${PLUGIN_DIR}"
    !insertmacro SetUpdaterLocked "${PLUGIN_DIR}" "true"
SectionEnd

Section "Uninstall"
    ${If} ${FileExists} "$INSTDIR\win_x64\winctrl.xpl.bak"
        Delete "$INSTDIR\win_x64\winctrl.xpl"
        Rename "$INSTDIR\win_x64\winctrl.xpl.bak" "$INSTDIR\win_x64\winctrl.xpl"
        DetailPrint "Restored the original winctrl.xpl"
    ${EndIf}
    !insertmacro SetUpdaterLocked "$INSTDIR" "false"
    Delete "$INSTDIR\rmp-tcas.ini"
    Delete "$INSTDIR\uninstall-winctrl-tcas.exe"
SectionEnd
