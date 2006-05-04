# Also requires:
# Processes plugin http://nsis.sourceforge.net/Processes_plug-in
# ShellLink plugin http://nsis.sourceforge.net/ShellLink_plug-in

; 7-Zip provides better compression than lzma from NSIS so we add the files
; uncompressed and use 7-Zip to create a SFX archive of it
; SetCompressor /SOLID /FINAL lzma
; SetCompressorDictSize 8
; SetDatablockOptimize on
; SetCompressorFilter 1
; !packhdr "$%TEMP%\exehead.tmp" '"C:\dev\nsis\upx125w\upx.exe --best $%TEMP%\exehead.tmp"'

SetCompress off

!include "FileFunc.nsh"


!include "TextFunc.nsh"
!insertmacro FileJoin
!insertmacro FileReadFromEnd
!insertmacro TrimNewLines
!insertmacro un.TrimNewLines

!insertmacro un.LineFind
!insertmacro TextCompare
!addplugindir plugins


; IMPORTANT
; SetShellVarContext current or all
; Then use SHCTX or SHELL_CONTEXT to use that reg root

; Using these add a couple of KB to the file size but using them makes the code
; much easier to read and understand.
!include WordFunc.nsh
!include LogicLib.nsh
!include StrFunc.nsh

!insertmacro StrFilter

!ifndef APP_FULL_NAME
!define APP_FULL_NAME "Mozilla Firefox"
!endif

!ifndef APP_SHORT_NAME
!define APP_SHORT_NAME "Firefox"
!endif

!ifndef APP_EXE
!define APP_EXE "firefox.exe"
!endif

;@FIREFOX_VERSION@
!ifndef APP_VER
!define APP_VER "1.5"
!endif

;@AB_CD@
!ifndef AB_CD
!define AB_CD "en-US"
!endif

;Nothing currently? @GRE_BUILD_ID@ being used by xulrunner w/ firefox?
; perhaps use @TOOLKIT_EM_VERSION@
!ifndef GRE_VER
!define GRE_VER "1.8"
!endif

; Additional localized strings
!include commonLocale.nsh


!include SetProgramAccess.nsi
!include common.nsh
!include version.nsh

!define LOG_FILES "files.log"
!define LOG_DIRS "directories.log"
!define LOG_REG "registry.log"

; TODO
; Need to handle checkIfAppIsLoaded on leave for different install types
; Verify that the app has TRULY exited

; To register IE as the default browser
; HKEY_LOCAL_MACHINE\SOFTWARE\Clients\StartMenuInternet\IEXPLORE.EXE\InstallInfo
; ReinstallCommand
; %systemroot%\system32\shmgrate.exe OCInstallReinstallIE

# For uninstaller
# /D=Install Dir or
# or possibly
# _?= sets $INSTDIR

Name "${APP_FULL_NAME}"
OutFile "setup.exe"
InstallDir "$PROGRAMFILES\${APP_FULL_NAME}"
BrandingText " "
; ShowInstDetails show
; ShowUnInstDetails show
ShowInstDetails nevershow
ShowUnInstDetails nevershow
CRCCheck on


InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_FULL_NAME} (${APP_VER})" "InstallLocation"

; Use the pre-processor where ever possible
; Var's create smaller packages than !define's
Var TmpVal
Var MUI_TEMP
Var STARTMENU_FOLDER
Var PREFIXDir
Var LOG
Var UNDIR_VAR
Var INI_VALUE
Var INSTALLTYPE
Var ADD_STARTMENU
Var ADD_QUICKLAUNCH
Var ADD_DESKTOP

!include MUI.nsh

!insertmacro MUI_RESERVEFILE_LANGDLL
ReserveFile options.ini
ReserveFile shortcuts.ini

!define MUI_ICON images\setup.ico
!define MUI_UNICON images\setup.ico

!define MUI_WELCOMEPAGE_TITLE_3LINES
; define MUI_UNWELCOMEFINISHPAGE_BITMAP before MUI_WELCOMEFINISHPAGE_BITMAP
; otherwise it won't display in the uninstaller possibly due to the orderring
; of statements in this script or a bug
!define MUI_UNWELCOMEFINISHPAGE_BITMAP images\left.bmp
!define MUI_WELCOMEFINISHPAGE_BITMAP images\left.bmp

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP images\header.bmp

!define MUI_ABORTWARNING

!define MUI_CUSTOMFUNCTION_GUIINIT myGuiInit

!define MUI_CUSTOMFUNCTION_UNGUIINIT un.myGuiInit

;--------------------------------
; Pages

  # Install
  ; Welcome Page
  !insertmacro MUI_PAGE_WELCOME

  ; License Page
  LicenseForceSelection radiobuttons
  !insertmacro MUI_PAGE_LICENSE EULA

  ; Custom Options Page
  Page custom Options ChangeOptions

  ; Select Install Directory Page
  !define MUI_PAGE_CUSTOMFUNCTION_PRE CheckCustom
  !insertmacro MUI_PAGE_DIRECTORY

  ; Select Install Components Page
  !define MUI_PAGE_CUSTOMFUNCTION_PRE CheckCustom
  !insertmacro MUI_PAGE_COMPONENTS

  ; Custom Shortcuts Page - CheckCustom is Called in Shortcuts
  Page custom Shortcuts ChangeShortcuts

# rstrong - may be better to just use reg Calls - perhaps the uninstall keys
  ; Start Menu Folder Page Configuration
  !define MUI_PAGE_CUSTOMFUNCTION_PRE CheckStartMenu
  !define MUI_STARTMENUPAGE_NODISABLE
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM"
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Mozilla\${APP_FULL_NAME}\${APP_VER} (${AB_CD})\Main"
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER


  ; Install Files Page
  !define MUI_PAGE_CUSTOMFUNCTION_PRE checkIfAppIsLoaded
  !insertmacro MUI_PAGE_INSTFILES

  ; Finish Page
  !define MUI_FINISHPAGE_NOREBOOTSUPPORT
  !define MUI_FINISHPAGE_TITLE_3LINES
  !define MUI_FINISHPAGE_RUN $INSTDIR\${APP_EXE}
  !define MUI_FINISHPAGE_RUN_TEXT $(LAUNCH_TEXT)
  !define MUI_PAGE_CUSTOMFUNCTION_PRE disableCancel
  !insertmacro MUI_PAGE_FINISH

  # Uninstall
  ; Welcome Page
  !insertmacro MUI_UNPAGE_WELCOME

  ; Uninstall Confirm Page
  !insertmacro MUI_UNPAGE_CONFIRM

  ; Remove Files Page
  !define MUI_PAGE_CUSTOMFUNCTION_PRE un.checkIfAppIsLoaded
  !insertmacro MUI_UNPAGE_INSTFILES

  ; Finish Page
  !define MUI_PAGE_CUSTOMFUNCTION_PRE un.disableCancel
  !define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
  !define MUI_FINISHPAGE_SHOWREADME ""
  !define MUI_FINISHPAGE_SHOWREADME_TEXT $(SURVEY_TEXT)
  !define MUI_FINISHPAGE_SHOWREADME_FUNCTION un.survey
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages
  ; DEF_MUI_LANGUAGE is defined in commonLocale.nsh so MUI_LANGUAGE can be
  ; easily defined along with the other locale specific settings
  !insertmacro DEF_MUI_LANGUAGE

Function myGUIInit
  ClearErrors

  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "options.ini"
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "shortcuts.ini"
FunctionEnd

Function un.checkIfAppIsLoaded
  !insertmacro CLOSE_APP ${APP_EXE} $(WARN_APP_RUNNING_UNINSTALL)
FunctionEnd

Function checkIfAppIsLoaded
  !insertmacro CLOSE_APP ${APP_EXE} $(WARN_APP_RUNNING_INSTALL)
FunctionEnd

Function un.GetParameters

   Push $R0
   Push $R1
   Push $R2
   Push $R3

   StrCpy $R2 1
   StrLen $R3 $CMDLINE

   ;Check for quote or space
   StrCpy $R0 $CMDLINE $R2
   StrCmp $R0 '"' 0 +3
     StrCpy $R1 '"'
     Goto loop
   StrCpy $R1 " "

   loop:
     IntOp $R2 $R2 + 1
     StrCpy $R0 $CMDLINE 1 $R2
     StrCmp $R0 $R1 get
     StrCmp $R2 $R3 get
     Goto loop

   get:
     IntOp $R2 $R2 + 1
     StrCpy $R0 $CMDLINE 1 $R2
     StrCmp $R0 " " get
     StrCpy $R0 $CMDLINE "" $R2

   Pop $R3
   Pop $R2
   Pop $R1
   Exch $R0

FunctionEnd

Function un.myGUIInit
  GetFullPathName $INSTDIR "$INSTDIR\.."

  Call un.SetAccess

  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "options.ini"
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "shortcuts.ini"
FunctionEnd

Function un.survey
  Exec '"$TmpVal" "https://survey.mozilla.com/1/Firefox/${APP_VER} (${AB_CD})/exit.html"'
FunctionEnd

; Check whether to display the current page
Function CheckCustom
  ${If} $INSTALLTYPE != 4
    Abort
  ${EndIf}
FunctionEnd

Function CheckStartMenu
  Call CheckCustom
  ${If} $ADD_STARTMENU != 1
    Abort
  ${EndIf}
FunctionEnd

Section "Application" Section1
  SectionIn 1 RO
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  ${If} ${FileExists} $INSTDIR\uninstall\${LOG_FILES}
    ${If} ${FileExists} "$INSTDIR\uninstall\${LOG_FILES}.bak"
      Delete "$INSTDIR\uninstall\${LOG_FILES}.bak"
    ${EndIf}
    Rename "$INSTDIR\uninstall\${LOG_FILES}" "$INSTDIR\uninstall\${LOG_FILES}.bak"
  ${EndIf}

  ${If} ${FileExists} $INSTDIR\uninstall\${LOG_DIRS}
    ${If} ${FileExists} "$INSTDIR\uninstall\${LOG_DIRS}.bak"
      Delete "$INSTDIR\uninstall\${LOG_DIRS}.bak"
    ${EndIf}
    Rename "$INSTDIR\uninstall\${LOG_DIRS}" "$INSTDIR\uninstall\${LOG_DIRS}.bak"
  ${EndIf}
  StrCpy $PREFIXDir "$INSTDIR\extensions"
  StrCpy $UNDIR_VAR "$EXEDIR\addons\inspector"
  Call ListDirEntries
  StrCpy $PREFIXDir "$INSTDIR\extensions"
  StrCpy $UNDIR_VAR "$EXEDIR\addons\talkback"
  Call ListDirEntries

  StrCpy $PREFIXDir "$INSTDIR"
  StrCpy $UNDIR_VAR "$EXEDIR\app"
  Call ListDirEntries
  StrCpy $PREFIXDir "$INSTDIR\chrome"
  StrCpy $UNDIR_VAR "$EXEDIR\locale"
  Call ListDirEntries

  ; Diff and add missing entries from the previous file log if it exists
  ${If} ${FileExists} "$INSTDIR\uninstall\${LOG_FILES}.bak"
    StrCpy $R0 "$INSTDIR\uninstall\${LOG_FILES}"
    StrCpy $R1 "$INSTDIR\uninstall\${LOG_FILES}.bak"
    GetTempFileName $R2
    FileOpen $R3 $R2 w
    ${TextCompare} "$R1" "$R0" "SlowDiff" "GetDiff"
    FileClose $R3
    IfErrors endFile 0
    ${FileJoin} '$INSTDIR\uninstall\${LOG_FILES}' '$R2' '$INSTDIR\uninstall\${LOG_FILES}'
    endFile:
    Delete "$INSTDIR\uninstall\${LOG_FILES}.bak"
    ${If} ${FileExists} "$2"
      Delete "$2"
    ${EndIf}
  ${EndIf}

  ; Diff and add missing entries from the previous directory log if it exists
  ${If} ${FileExists} "$INSTDIR\uninstall\${LOG_DIRS}.bak"
    StrCpy $R0 "$INSTDIR\uninstall\${LOG_DIRS}"
    StrCpy $R1 "$INSTDIR\uninstall\${LOG_DIRS}.bak"
    GetTempFileName $R2
    FileOpen $R3 $R2 w
    ${TextCompare} "$R1" "$R0" "SlowDiff" "GetDiff"
    FileClose $R3
    IfErrors endDir 0
    ${FileJoin} '$INSTDIR\uninstall\${LOG_DIRS}' '$R2' '$INSTDIR\uninstall\${LOG_DIRS}'
    endDir:
    Delete "$INSTDIR\uninstall\${LOG_DIRS}.bak"
    ${If} ${FileExists} "$2"
      Delete "$2"
    ${EndIf}
  ${EndIf}

  ; Install all required files
  CopyFiles /SILENT "$EXEDIR\app\*" "$INSTDIR\"
  CopyFiles /SILENT "$EXEDIR\locale\*" "$INSTDIR\chrome\"

  ; Register DLLs
  RegDLL "$INSTDIR\AccessibleMarshal.dll"

  Call CopyInstalledPlugins

  StrCpy $TmpVal "Software\Mozilla\Mozilla"
  WriteRegStr HKLM $TmpVal "CurrentVersion" "${GRE_VER}"
  WriteRegStr HKCU $TmpVal "CurrentVersion" "${GRE_VER}"

  StrCpy $TmpVal "Software\Mozilla\${APP_FULL_NAME}"
  WriteRegStr HKLM $TmpVal "" "${GRE_VER}"
  WriteRegStr HKLM $TmpVal "CurrentVersion" "${APP_VER} (${AB_CD})"
  WriteRegStr HKCU $TmpVal "" "${GRE_VER}"
  WriteRegStr HKCU $TmpVal "CurrentVersion" "${APP_VER} (${AB_CD})"

  StrCpy $TmpVal "Software\Mozilla\${APP_FULL_NAME}\${APP_VER} (${AB_CD})"
  WriteRegStr HKLM $TmpVal "" "${APP_VER} (${AB_CD})"
  WriteRegStr HKCU $TmpVal "" "${APP_VER} (${AB_CD})"

  StrCpy $TmpVal "Software\Mozilla\${APP_FULL_NAME}\${APP_VER} (${AB_CD})\Uninstall"
  WriteRegStr HKLM $TmpVal "Uninstall Log Folder" "$INSTDIR\${APP_FULL_NAME}\uninstall"
  WriteRegStr HKLM $TmpVal "Description" "${APP_FULL_NAME} (${APP_VER})"
  WriteRegStr HKCU $TmpVal "Uninstall Log Folder" "$INSTDIR\${APP_FULL_NAME}\uninstall"
  WriteRegStr HKCU $TmpVal "Description" "${APP_FULL_NAME} (${APP_VER})"

  StrCpy $TmpVal "Software\Mozilla\${APP_FULL_NAME} ${APP_VER}"
  WriteRegStr HKLM $TmpVal "GeckoVer" "${GRE_VER}"
  WriteRegStr HKCU $TmpVal "GeckoVer" "${GRE_VER}"

  StrCpy $TmpVal "Software\Mozilla\${APP_FULL_NAME} ${APP_VER}\bin"
  WriteRegStr HKLM $TmpVal "PathToExe" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKCU $TmpVal "PathToExe" "$INSTDIR\${APP_EXE}"

  StrCpy $TmpVal "Software\Mozilla\${APP_FULL_NAME} ${APP_VER}\extensions"
  WriteRegStr HKLM $TmpVal "Components" "$INSTDIR\components"
  WriteRegStr HKLM $TmpVal "Plugins" "$INSTDIR\plugins"
  WriteRegStr HKCU $TmpVal "Components" "$INSTDIR\components"
  WriteRegStr HKCU $TmpVal "Plugins" "$INSTDIR\plugins"

  ; Write a reg str and then delete the value so it displays as (value not set)
  StrCpy $TmpVal "Software\Microsoft\MediaPlayer\ShimInclusionList\${APP_EXE}"
  WriteRegStr HKLM $TmpVal "" ""
  DeleteRegValue HKLM $TmpVal ""

  StrCpy $TmpVal "Software\Microsoft\Windows\CurrentVersion\App Paths\${APP_EXE}"
  WriteRegStr HKLM $TmpVal "" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKLM $TmpVal "Path" "$INSTDIR"

  ${StrFilter} "${APP_EXE}" "+" "" "" $R9
  StrCpy $TmpVal "Software\Clients\StartMenuInternet\$R9"
  WriteRegStr HKLM $TmpVal "" "${APP_FULL_NAME}"

  StrCpy $TmpVal "Software\Clients\StartMenuInternet\$R9\DefaultIcon"
  WriteRegStr HKLM $TmpVal "" '"$INSTDIR\${APP_EXE}",0'

  StrCpy $TmpVal "Software\Clients\StartMenuInternet\$R9\InstallInfo"

  WriteRegStr HKLM $TmpVal "HideIconsCommand" '"$INSTDIR\uninstall\uninstaller.exe" /ua "${APP_VER} (${AB_CD})" /hs browser'
  WriteRegDWORD HKLM $TmpVal "IconsVisible" 1

  ; The Reinstall Command from
  ; http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/programmersguide/shell_adv/registeringapps.asp
  ; Once the reinstall process is complete, the program launched by the
  ; reinstall command line should exit. It should not launch the corresponding
  ; program in interactive mode; it should merely register defaults. For
  ; example, the reinstall command for a browser should not open the user's
  ; home page.
  WriteRegStr HKLM $TmpVal "ReinstallCommand" '"$INSTDIR\${APP_EXE}" -silent -setDefaultBrowser'

  WriteRegStr HKLM $TmpVal "ShowIconsCommand" '"$INSTDIR\uninstall\uninstaller.exe" /ua "${APP_VER} (${AB_CD})" /ss browser'

  StrCpy $TmpVal "Software\Clients\StartMenuInternet\$R9\shell\open\command"
  WriteRegStr HKLM $TmpVal "" "$INSTDIR\${APP_EXE}"
# Is this bogus?
#  WriteRegStr HKLM $TmpVal "ShowIconsCommand" "$INSTDIR\${APP_EXE}"

  StrCpy $TmpVal "Software\Clients\StartMenuInternet\$R9\shell\properties"
  WriteRegStr HKLM $TmpVal "" "&Options"

  StrCpy $TmpVal "Software\Clients\StartMenuInternet\$R9\shell\properties\command"
  WriteRegStr HKLM $TmpVal "" "$\"$INSTDIR\${APP_EXE}$\" -preferences"

  StrCpy $TmpVal "Software\Clients\StartMenuInternet\$R9\shell\safemode"
  WriteRegStr HKLM $TmpVal "" "&$(SAFE_MODE)"

  StrCpy $TmpVal "Software\Clients\StartMenuInternet\$R9\shell\safemode\command"
  WriteRegStr HKLM $TmpVal "" "$\"$INSTDIR\${APP_EXE}$\" -safe-mode"

  StrCpy $TmpVal "MIME\Database\Content Type\application/x-xpinstall;app=firefox"
  WriteRegStr HKCR $TmpVal "Extension" ".xpi"

  StrCpy $TmpVal "Software\Mozilla\${APP_FULL_NAME}\${APP_VER} (${AB_CD})\Main"
  WriteRegStr HKLM $TmpVal "Install Directory" "$INSTDIR"
  WriteRegStr HKLM $TmpVal "PathToExe" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKCU $TmpVal "Install Directory" "$INSTDIR"
  WriteRegStr HKCU $TmpVal "PathToExe" "$INSTDIR\${APP_EXE}"

  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application

; perhaps use the uninstall keys
  ${If} $ADD_QUICKLAUNCH == 1
    CreateShortCut "$QUICKLAUNCH\${APP_FULL_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
    WriteRegDWORD HKCU $TmpVal "Create Quick Launch Shortcut" 1
  ${Else}
    ${If} ${FileExists} "$QUICKLAUNCH\${APP_FULL_NAME}.lnk"
      Delete "$QUICKLAUNCH\${APP_FULL_NAME}.lnk"
    ${EndIf}
    WriteRegDWORD HKCU $TmpVal "Create Quick Launch Shortcut" 0
  ${EndIf}

  ${If} $ADD_DESKTOP == 1
    CreateShortCut "$DESKTOP\${APP_FULL_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
    WriteRegDWORD HKCU $TmpVal "Create Desktop Shortcut" 1
  ${Else}
    ${If} ${FileExists} "$DESKTOP\${APP_FULL_NAME}.lnk"
      Delete "$DESKTOP\${APP_FULL_NAME}.lnk"
    ${EndIf}
    WriteRegDWORD HKCU $TmpVal "Create Desktop Shortcut" 0
  ${EndIf}

  ; Create Start Menu shortcuts
  ${If} $ADD_STARTMENU == 1
    CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\${APP_FULL_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\${APP_FULL_NAME} ($(SAFE_MODE)).lnk" "$INSTDIR\${APP_EXE}" "-safe-mode" "$INSTDIR\${APP_EXE}" 0
    WriteRegDWORD HKCU $TmpVal "Create Start Menu Shortcut" 1
    WriteRegStr HKLM $TmpVal "Program Folder Path" "$SMPROGRAMS\$STARTMENU_FOLDER"
    WriteRegStr HKCU $TmpVal "Program Folder Path" "$SMPROGRAMS\$STARTMENU_FOLDER"
  ${Else}
    ${If} ${FileExists} "$SMPROGRAMS\$STARTMENU_FOLDER\${APP_FULL_NAME}.lnk"
      Delete "$SMPROGRAMS\$STARTMENU_FOLDER\${APP_FULL_NAME}.lnk"
    ${EndIf}
    ${If} ${FileExists} "$SMPROGRAMS\$STARTMENU_FOLDER\${APP_FULL_NAME} ($(SAFE_MODE)).lnk"
      Delete "$SMPROGRAMS\$STARTMENU_FOLDER\${APP_FULL_NAME} ($(SAFE_MODE)).lnk"
    ${EndIf}
    ${If} ${FileExists} "$SMPROGRAMS\$STARTMENU_FOLDER"
      RmDir "$SMPROGRAMS\$STARTMENU_FOLDER"
      WriteRegDWORD HKCU $TmpVal "Create Start Menu Shortcut" 0
      DeleteRegValue HKLM $TmpVal "Program Folder Path"
      DeleteRegValue HKCU $TmpVal "Program Folder Path"
    ${EndIf}
  ${EndIf}

  !insertmacro MUI_STARTMENU_WRITE_END

  Call UninstallCleanup

  StrCpy $TmpVal "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_FULL_NAME} (${APP_VER})"
  ; Write the uninstall keys for Windows
# Possible additions HelpLink, VersionMajor, VersionMinor, Readme, InstallDate, TSAware, QuietUninstallString, HelpTelephone, Contact, UninstallPath, Version
# Comment should be Comments
  WriteRegStr HKLM $TmpVal "Comment" "${APP_FULL_NAME}"
  WriteRegStr HKLM $TmpVal "DisplayIcon" '"$INSTDIR\${APP_EXE}",0'
  WriteRegStr HKLM $TmpVal "DisplayName" "${APP_FULL_NAME} (${APP_VER})"
  WriteRegStr HKLM $TmpVal "DisplayVersion" "${APP_VER} (${AB_CD})"
  WriteRegStr HKLM $TmpVal "InstallLocation" '"$INSTDIR"'
  WriteRegStr HKLM $TmpVal "Publisher" "Mozilla"
  WriteRegStr HKLM $TmpVal "UninstallString" '"$INSTDIR\uninstall\uninstaller.exe"'
  WriteRegDWORD HKLM $TmpVal "NoModify" 1
  WriteRegDWORD HKLM $TmpVal "NoRepair" 1
  WriteRegStr HKLM $TmpVal "URLInfoAbout" "http://www.mozilla.org/"
  WriteRegStr HKLM $TmpVal "URLUpdateInfo" "http://www.mozilla.org/products/firefox/"
  WriteUninstaller "$INSTDIR\uninstall\uninstaller.exe"
SectionEnd

Section /o "Developer Tools" Section2
  CopyFiles /SILENT "$EXEDIR\addons\inspector\*" "$INSTDIR\extensions\"
SectionEnd

Section /o "Quality Feedback Agent" Section3
  CopyFiles /SILENT "$EXEDIR\addons\talkback\*" "$INSTDIR\extensions\"
SectionEnd

Section "Uninstall"
  SectionIn RO

  ; Unregister DLLs
  UnRegDLL "$INSTDIR\AccessibleMarshal.dll"

  ; Remove files. If we don't have a log file skip
  ${If} ${FileExists} "$INSTDIR\uninstall\${LOG_FILES}"
    GetTempFileName $LOG
    CopyFiles "$INSTDIR\uninstall\${LOG_FILES}" "$LOG"
    Delete "$INSTDIR\uninstall\${LOG_FILES}"
    Call un.removeFiles
    Delete $LOG
  ${EndIf}

  ; Remove directories. If we don't have a log file skip
  ${If} ${FileExists} "$INSTDIR\uninstall\${LOG_DIRS}"
    GetTempFileName $LOG
    CopyFiles "$INSTDIR\uninstall\${LOG_DIRS}" "$LOG"
    Delete "$INSTDIR\uninstall\${LOG_DIRS}"
    ; The uninstall and updates directories are managed by the application and
    ; they both get a lot of cruft so delete them recursively after copying our
    ; log files.
    ${If} ${FileExists} "$INSTDIR\uninstall"
      RMDir /r "$INSTDIR\uninstall"
    ${EndIf}
    ${If} ${FileExists} "$INSTDIR\updates"
      RMDir /r "$INSTDIR\updates"
    ${EndIf}
    Call un.removeDirs
    Delete $LOG
  ${EndIf}

  IfFileExists "$QUICKLAUNCH\${APP_FULL_NAME}.lnk" +1 +2
  Delete "$QUICKLAUNCH\${APP_FULL_NAME}.lnk"

  IfFileExists "$DESKTOP\${APP_FULL_NAME}.lnk" +1 +2
  Delete "$DESKTOP\${APP_FULL_NAME}.lnk"

  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM"
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Mozilla\${APP_FULL_NAME}\${APP_VER} (${AB_CD})\Main"
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP

  ;Delete empty start menu parent diretories
  StrCpy $MUI_TEMP "$SMPROGRAMS\$MUI_TEMP"

  IfFileExists "$MUI_TEMP\${APP_FULL_NAME}.lnk" +1 +2
  Delete "$MUI_TEMP\${APP_FULL_NAME}.lnk"
  IfFileExists "$MUI_TEMP\${APP_FULL_NAME} ($(SAFE_MODE)).lnk" +1 +2
  Delete "$MUI_TEMP\${APP_FULL_NAME} ($(SAFE_MODE)).lnk"
  IfFileExists "$MUI_TEMP" +1 +2
  RmDir "$MUI_TEMP"

  startMenuDeleteLoop:
  ClearErrors
    RMDir $MUI_TEMP
    GetFullPathName $MUI_TEMP "$MUI_TEMP\.."

    IfErrors startMenuDeleteLoopDone

    StrCmp $MUI_TEMP $SMPROGRAMS startMenuDeleteLoopDone startMenuDeleteLoop
  startMenuDeleteLoopDone:

  RMDir $INSTDIR

  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_FULL_NAME} (${APP_VER})"
  DeleteRegKey HKLM "Software\Mozilla\${APP_FULL_NAME}"
  DeleteRegKey HKLM "Software\Mozilla\${APP_FULL_NAME} ${APP_VER}"
  DeleteRegKey HKCU "Software\Mozilla\${APP_FULL_NAME}"
  DeleteRegKey HKCU "Software\Mozilla\${APP_FULL_NAME} ${APP_VER}"

  DeleteRegKey HKLM "Software\Microsoft\MediaPlayer\ShimInclusionList\$R9"
  DeleteRegKey HKCR "MIME\Database\Content Type\application/x-xpinstall;app=firefox"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\$R9"
  DeleteRegKey HKLM "Software\Clients\StartMenuInternet\$R9"

# if dir still exists at this point perhaps prompt user to ask if they would
# like to delete it anyway with appropriate safe gaurds

SectionEnd

; When we add an optional action to the finish page the cancel button is
; enable. The next two function disable it for install and uninstall
Function disableCancel
  !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "cancelenabled" "0"
FunctionEnd

Function un.disableCancel
  !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "cancelenabled" "0"

  ; Only display the survey checkbox if we can find IE
  StrCpy $TmpVal "SOFTWARE\Microsoft\IE Setup\Setup"
  ClearErrors
  ReadRegStr $0 HKLM $TmpVal "Path"
  IfErrors +8 +1
  ExpandEnvStrings $0 "$0" ; this value will usually contain %programfiles%
  ${If} $0 != "\"
    StrCpy $0 "$0\"
  ${EndIf}
  StrCpy $0 "$0\iexplore.exe"
  ClearErrors
  GetFullPathName $TmpVal $0
  IfErrors +1 +2
  !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "NumFields" "3"
FunctionEnd

Function Options
  !insertmacro MUI_HEADER_TEXT $(OPTIONS_PAGE_TITLE) $(OPTIONS_PAGE_SUBTITLE)
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "options.ini"
FunctionEnd


Function ChangeOptions
  ReadINIStr $0 "$PLUGINSDIR\options.ini" "Settings" "State"
  ${If} $0 != 0
    Abort
  ${EndIf}
  !insertmacro MUI_INSTALLOPTIONS_READ $INI_VALUE "options.ini" "Field 2" "State"
  StrCmp $INI_VALUE "1" +1 +2
  StrCpy $INSTALLTYPE "1"
  !insertmacro MUI_INSTALLOPTIONS_READ $INI_VALUE "options.ini" "Field 3" "State"
  StrCmp $INI_VALUE "1" +1 +2
  StrCpy $INSTALLTYPE "2"
  !insertmacro MUI_INSTALLOPTIONS_READ $INI_VALUE "options.ini" "Field 4" "State"
  StrCmp $INI_VALUE "1" +1 +2
  StrCpy $INSTALLTYPE "4"
FunctionEnd

Function Shortcuts
  Call CheckCustom
  !insertmacro MUI_HEADER_TEXT "$(SHORTCUTS_PAGE_TITLE)" "$(SHORTCUTS_PAGE_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "shortcuts.ini"
FunctionEnd

Function ChangeShortcuts
  ReadINIStr $0 "$PLUGINSDIR\options.ini" "Settings" "State"
  ${If} $0 != 0
    Abort
  ${EndIf}
  !insertmacro MUI_INSTALLOPTIONS_READ $ADD_DESKTOP "shortcuts.ini" "Field 2" "State"
  !insertmacro MUI_INSTALLOPTIONS_READ $ADD_STARTMENU "shortcuts.ini" "Field 3" "State"
  !insertmacro MUI_INSTALLOPTIONS_READ $ADD_QUICKLAUNCH "shortcuts.ini" "Field 4" "State"
FunctionEnd

; Call this before setting the uninstall reg key so we don't have to exclude it
; when checking for old keys
Function UninstallCleanup
  StrCpy $0 0
  StrCpy $R1 "Software\Microsoft\Windows\CurrentVersion\Uninstall"

  GetFullPathName $4 "$INSTDIR"
  StrCpy $3 $4 "" -1
  ${If} $3 != "\"
    StrCpy $4 "$4\"
  ${EndIf}

loop:
  EnumRegKey $1 HKLM $R1 $0
  StrCmp $1 "" done
  IntOp $0 $0 + 1
  ReadRegStr $2 HKLM "$R1\$1" "InstallLocation"

  ; Need a cleaner way for unsetting vars?
  StrCpy $TmpVal " "
  ClearErrors
  ${Unless} ${Errors}
    StrCpy $3 $2 "" -1
    ${If} $3 = '"'
      StrCpy $2 $2 -1
    ${EndIf}
    StrCpy $3 $2 1
    ${If} $3 = '"'
      StrCpy $2 $2 "" 1
    ${EndIf}
    StrCpy $3 $2 "" -1
    ${If} $3 != "\"
      StrCpy $2 "$2\"
    ${EndIf}
    GetFullPathName $TmpVal $2
  ${EndUnless}

  ${Unless} ${Errors}
    ${If} $TmpVal == $4
      DeleteRegKey HKLM "$R1\$1"
    ${EndIf}
  ${EndUnless}

  GoTo loop

done:

FunctionEnd


Function CopyInstalledPlugins
  ; Check if QuickTime is installed and copy
  ; nsIQTScriptablePlugin.xpt from its plugins directory into our
  ; plugins directory. If we don't do this, QuickTime will load in
  ; Firefox, but it won't be scriptable.
  ClearErrors
  ReadRegStr $0 HKLM "Software\Apple Computer, Inc.\QuickTime" "InstallDir"
  ${Unless} ${Errors}
    GetFullPathName $TmpVal $0
    ${Unless} ${Errors}
      StrCpy $1 $TmpVal "" -1
      ${If} $1 != "\"
        StrCpy $TmpVal "$TmpVal\"
      ${EndIf}
      GetFullPathName $TmpVal "$TmpVal\Plugins\nsIQTScriptablePlugin.xpt"
      ${Unless} ${Errors}
        CopyFiles /SILENT $TmpVal "$INSTDIR\plugins"
      ${EndUnless}
    ${EndUnless}
  ${EndUnless}

  ; Check if Netscape Navigator (pre 6.0) is installed and if the
  ; flash player is installed in Netscape's plugin folder. If it is,
  ; try to copy the flashplayer.xpt file into our plugins folder to
  ; make ensure that flash is scriptable if we're using it from
  ; Netscape's plugins folder.
  ClearErrors
  StrCpy $TmpVal "Software\Netscape\Netscape Navigator"
  ReadRegStr $0 HKLM $TmpVal "CurrentVersion"
  ${Unless} ${Errors}
    StrCpy $TmpVal "$TmpVal\$0\Main"
    ReadRegStr $0 HKLM $TmpVal "Plugins Directory"
    ${Unless} ${Errors}
      GetFullPathName $TmpVal $0
      ${Unless} ${Errors}
        StrCpy $1 $TmpVal "" -1
        ${If} $1 != "\"
          StrCpy $TmpVal "$TmpVal\"
        ${EndIf}
        GetFullPathName $TmpVal "$TmpVal\flashplayer.xpt"
        ${Unless} ${Errors}
          CopyFiles /SILENT $TmpVal "$INSTDIR\plugins"
        ${EndUnless}
      ${EndUnless}
    ${EndUnless}
  ${EndUnless}
FunctionEnd

Function GetDiff
  FileWrite $R3 '$9'

  Push $0
FunctionEnd

Function ListDirEntries
  ClearErrors
  IfErrors error
  StrLen $R5 $UNDIR_VAR
  IntOp $R5 $R5 + 1

  GetTempFileName $R1 $EXEDIR
  GetTempFileName $R2 $EXEDIR
  GetTempFileName $R3 $EXEDIR
  ExpandEnvStrings $R0 %COMSPEC%


  nsExec::Exec '"$R0" /C DIR "$UNDIR_VAR\*.*" /A-D /B /S /ON>"$R1"'
  FileOpen $R4 $R2 w
  ${FileReadFromEnd} '$R1' FilesCallback
  FileClose $R4

  nsExec::Exec '"$R0" /C DIR "$UNDIR_VAR\*.*" /AD /B /S /ON>"$R1"'
  FileOpen $R4 $R3 w
  ${FileReadFromEnd} '$R1' DirectoriesCallback
  FileClose $R4

  StrCpy $TmpVal "$INSTDIR\uninstall\${LOG_FILES}"
  ${If} ${FileExists} $TmpVal
    ${FileJoin} '$TmpVal' '$R2' '$TmpVal'
  ${Else}
    SetOutPath "$INSTDIR\uninstall"
    CopyFiles $R2 $TmpVal
    SetOutPath "$INSTDIR"
  ${EndIf}

  StrCpy $TmpVal "$INSTDIR\uninstall\${LOG_DIRS}"
  ${If} ${FileExists} $TmpVal
    ${FileJoin} '$TmpVal' '$R3' '$TmpVal'
  ${Else}
    SetOutPath "$INSTDIR\uninstall"
    CopyFiles $R3 $TmpVal
    SetOutPath "$INSTDIR"
  ${EndIf}
error:
FunctionEnd

Function FilesCallback
  System::Call 'user32::OemToChar(t r9, t .r9)'
  ${TrimNewLines} '$9' $9

  StrCpy $9 $9 '' $R5
  FileWrite $R4 "$PREFIXDir\$9$\r$\n"
  Push 0
FunctionEnd

Function DirectoriesCallback
  System::Call 'user32::OemToChar(t r9, t .r9)'
  ${TrimNewLines} '$9' $9
  StrCpy $9 $9 '' $R5

  FileWrite $R4 "$PREFIXDir\$9$\r$\n"

  Push 0
FunctionEnd

Function un.removeFiles
  ${un.LineFind} "$LOG" "/NUL" "1:-1" "un.RemoveFilesCallback"
FunctionEnd

Function un.removeDirs
  ${un.LineFind} "$LOG" "/NUL" "1:-1" "un.RemoveDirsCallback"
FunctionEnd

Function un.RemoveFilesCallback
  ${un.TrimNewLines} '$R9' $R9
  ${If} ${FileExists} $R9
    Delete $R9
  ${EndIf}
  Push 0
FunctionEnd

Function un.RemoveDirsCallback
  ${un.TrimNewLines} '$R9' $R9
DetailPrint "Dir $9"
  ${If} ${FileExists} $R9
DetailPrint "Removing $9"
    RMDir $R9
  ${EndIf}
  Push 0
FunctionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${Section1} $(APP_DESC)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section2} $(DEV_TOOLS_DESC)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section3} $(QFA_DESC)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
