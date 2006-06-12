# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Mozilla Installer code.
#
# The Initial Developer of the Original Code is Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Robert Strong <robert.bugzilla@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# Also requires:
# ShellLink plugin http://nsis.sourceforge.net/ShellLink_plug-in

; NOTES
; Size matters! Try different methods to accomplish the same result and use the
; one that results in the smallest size. Every KB counts!
; LogicLib.nsh must be included in all installers to ease script creation and
; readability. It adds a couple of KB to the size but it is worth it.

; 7-Zip provides better compression than the lzma from NSIS so we add the files
; uncompressed and use 7-Zip to create a SFX archive of it
SetDatablockOptimize on
SetCompress off
CRCCheck on

!addplugindir ./

; Other files may depend upon these includes!
!include FileFunc.nsh
!include LogicLib.nsh
!include TextFunc.nsh
!include WinMessages.nsh
!include WordFunc.nsh
!include MUI.nsh

!insertmacro FileJoin
!insertmacro GetTime
!insertmacro LineFind
!insertmacro un.LineFind
!insertmacro Locate
!insertmacro StrFilter
!insertmacro TextCompare
!insertmacro TrimNewLines
!insertmacro un.TrimNewLines
!insertmacro WordFind
!insertmacro un.WordFind
!insertmacro WordReplace

; Use the pre-processor where ever possible
; Remember that !define's create smaller packages than Var's!
Var TmpVal
Var StartMenuDir
Var InstallType
Var AddStartMenuSC
Var AddQuickLaunchSC
Var AddDesktopSC
Var fhInstallLog
Var fhUninstallLog

!include defines.nsi
!include commonLocale.nsh
!include SetProgramAccess.nsi
!include common.nsh
!include version.nsh
; If/when appLocale.nsi contains required locale strings remove /NONFATAL
!include /NONFATAL appLocale.nsi

!insertmacro RegCleanMain
!insertmacro un.RegCleanMain
!insertmacro RegCleanUninstall
!insertmacro un.RegCleanUninstall
!insertmacro CloseApp
!insertmacro un.CloseApp
!insertmacro WriteRegStr2
!insertmacro WriteRegDWORD2
!insertmacro WriteRegStrHKCR
!insertmacro CreateRegKey
!insertmacro un.GetSecondInstallPath

Name "${BrandFullName}"
OutFile "setup.exe"
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BrandFullName} (${AppVersion})" "InstallLocation"
InstallDir "$PROGRAMFILES\${BrandFullName}"

; using " " for BrandingText will hide the "Nullsoft Install System..." branding
BrandingText " "

ShowInstDetails nevershow
ShowUnInstDetails nevershow

################################################################################
# Modern User Interface - MUI

;!insertmacro MUI_RESERVEFILE_LANGDLL
ReserveFile options.ini
ReserveFile shortcuts.ini

!define MUI_ICON setup.ico
!define MUI_UNICON setup.ico

!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_WELCOMEFINISHPAGE_BITMAP wizWatermark.bmp
!define MUI_UNWELCOMEFINISHPAGE_BITMAP wizWatermark.bmp

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP wizHeader.bmp

!define MUI_ABORTWARNING

!define MUI_CUSTOMFUNCTION_GUIINIT myGUIINIT
!define MUI_CUSTOMFUNCTION_UNGUIINIT un.myGUIINIT

/**
 * Installation Pages
 */
; Welcome Page
!insertmacro MUI_PAGE_WELCOME

; License Page
LicenseForceSelection radiobuttons
!insertmacro MUI_PAGE_LICENSE license.txt

; Custom Options Page
Page custom Options ChangeOptions

; Select Install Directory Page
!define MUI_PAGE_CUSTOMFUNCTION_PRE CheckCustom
!insertmacro MUI_PAGE_DIRECTORY

; Select Install Components Page
!define MUI_PAGE_CUSTOMFUNCTION_PRE CheckCustom
!insertmacro MUI_PAGE_COMPONENTS

; Custom Shortcuts Page - CheckCustom is Called in Shortcuts
Page custom preShortcuts ChangeShortcuts

; Start Menu Folder Page Configuration
!define MUI_PAGE_CUSTOMFUNCTION_PRE preCheckStartMenu
!define MUI_STARTMENUPAGE_NODISABLE
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Mozilla\${BrandFullName}\${AppVersion} (${AB_CD})\Main"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuDir

; Install Files Page
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE FinishInstall
!insertmacro MUI_PAGE_INSTFILES

; Finish Page
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!define MUI_FINISHPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_RUN $INSTDIR\${FileMainEXE}
!define MUI_FINISHPAGE_RUN_TEXT $(LAUNCH_TEXT)
!define MUI_PAGE_CUSTOMFUNCTION_PRE disableCancel
!insertmacro MUI_PAGE_FINISH

/**
 * Uninstall Pages
 */
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

; Only setup the survey controls, functions, etc. when the appLocale.nsi
; has DO_UNINSTALL_SURVEY defined to indicate the survey has been localized.
!ifdef DO_UNINSTALL_SURVEY
!define MUI_FINISHPAGE_SHOWREADME_TEXT $(SURVEY_TEXT)
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION un.survey
!endif

!insertmacro MUI_UNPAGE_FINISH

; Languages
; DEF_MUI_LANGUAGE is defined in commonLocale.nsh so MUI_LANGUAGE can be
; easily defined along with the other locale specific settings
!insertmacro DEF_MUI_LANGUAGE


################################################################################
# Modern User Interface - MUI

/**
 * Adds a section divider to the human readable log.
 */
Function WriteLogSeparator
  FileWrite $fhInstallLog "$\r$\n-------------------------------------------------------------------------------$\r$\n"
FunctionEnd

; Runs before the UI has been initialized for install
Function myGUIINIT
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "options.ini"
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "shortcuts.ini"
FunctionEnd

; Runs before the UI has been initialized for uninstall
Function un.myGUIINIT
  GetFullPathName $INSTDIR "$INSTDIR\.."
; XXXrstrong - should we quit when the app exe is not present?
  Call un.SetAccess
FunctionEnd

; Callback used to check if the app being uninstalled is running.
Function un.checkIfAppIsLoaded
  ; Try to delete the app executable and if we can't delete it try to close the
  ; app. This allows running an instance that is located in another directory.
  ClearErrors
  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    ${DeleteFile} "$INSTDIR\${FileMainEXE}"
  ${EndIf}
  ${If} ${Errors}
    ClearErrors
    ${un.CloseApp} $(WARN_APP_RUNNING_UNINSTALL)
    ; Try to delete it again to prevent launching the app while we are
    ; installing.
    ${DeleteFile} "$INSTDIR\${FileMainEXE}"
    ClearErrors
  ${EndIf}
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

; Only setup the survey controls, functions, etc. when the appLocale.nsi
; has DO_UNINSTALL_SURVEY defined to indicate the survey has been localized.
!ifdef DO_UNINSTALL_SURVEY
Function un.survey
  Exec "$\"$TmpVal$\" $\"${SurveyURL}$\""
FunctionEnd
!endif

; Check whether to display the current page (e.g. if we aren't performing a
; custom install don't display the custom pages).
Function CheckCustom
  ${If} $InstallType != 4
    Abort
  ${EndIf}
FunctionEnd

Function preCheckStartMenu
  Call CheckCustom
  ${If} $AddStartMenuSC != 1
    Abort
  ${EndIf}
FunctionEnd

Function onInstallDeleteFile
  ${TrimNewLines} "$R9" "$R9"
  StrCpy $R1 "$R9" 5
  ${If} $R1 == "File:"
    StrCpy $R9 "$R9" "" 6
    ${If} ${FileExists} "$INSTDIR$R9"
      ClearErrors
      Delete "$INSTDIR$R9"
      ${If} ${Errors}
        ${LogMsg} "** ERROR Deleting File: $INSTDIR$R9 **"
      ${Else}
        ${LogMsg} "Deleted File: $INSTDIR$R9"
      ${EndIf}
    ${EndIf}
  ${EndIf}
  ClearErrors
  Push 0
FunctionEnd

; The previous installer removed directories even when they aren't empty so this
; funtion does as well.
Function onInstallRemoveDir
  ${TrimNewLines} "$R9" "$R9"
  StrCpy $R1 "$R9" 4
  ${If} $R1 == "Dir:"
    StrCpy $R9 "$R9" "" 5
    StrCpy $R1 "$R9" "" -1
    ${If} $R1 == "\"
      StrCpy $R9 "$R9" -1
    ${EndIf}
    ${If} ${FileExists} "$INSTDIR$R9"
      ClearErrors
      RmDir /r "$INSTDIR$R9"
      ${If} ${Errors}
        ${LogMsg} "** ERROR Removing Directory: $INSTDIR$R9 **"
      ${Else}
        ${LogMsg} "Removed Directory: $INSTDIR$R9"
      ${EndIf}
    ${EndIf}
  ${EndIf}
  ClearErrors
  Push 0
FunctionEnd

Function FinishInstall
  FileClose $fhUninstallLog
  ; Diff and add missing entries from the previous file log if it exists
  ${If} ${FileExists} "$INSTDIR\uninstall\uninstall.bak"
    ${LogHeader} "Updating Uninstall Log With Previous Uninstall Log"
    StrCpy $R0 "$INSTDIR\uninstall\uninstall.log"
    StrCpy $R1 "$INSTDIR\uninstall\uninstall.bak"
    GetTempFileName $R2
    FileOpen $R3 $R2 w
    ${TextCompare} "$R1" "$R0" "SlowDiff" "GetDiff"
    FileClose $R3
    ${Unless} ${Errors}
      ${FileJoin} "$INSTDIR\uninstall\uninstall.log" "$R2" "$INSTDIR\uninstall\uninstall.log"
    ${EndUnless}
    ${DeleteFile} "$INSTDIR\uninstall\uninstall.bak"
    ${DeleteFile} "$R2"
  ${EndIf}

  Call WriteLogSeparator
  ${GetTime} "" "L" $0 $1 $2 $3 $4 $5 $6
  FileWrite $fhInstallLog "${BrandFullName} Installation Finished: $2-$1-$0 $4:$5:$6$\r$\n"
  FileClose $fhInstallLog
FunctionEnd

Section "Application" Section1
  SectionIn 1 RO
  SetOutPath $INSTDIR
  ; For a "Standard" upgrade without talkback installed add the InstallDisabled
  ; file to the talkback source files so it will be disabled by the extension
  ; manager. This is done at the start of the installation since we check for
  ; the existence of a directory to determine if this is an upgrade.
  ${If} $InstallType == 1
  ${AndIf} ${FileExists} "$INSTDIR\greprefs"
  ${AndIf} ${FileExists} "$EXEDIR\optional\extensions\talkback@mozilla.org"
    ${Unless} ${FileExists} "$INSTDIR\extensions\talkback@mozilla.org"
      ${Unless} ${FileExists} "$INSTDIR\extensions"
        CreateDirectory "$INSTDIR\extensions"
      ${EndUnless}
      CreateDirectory "$INSTDIR\extensions\talkback@mozilla.org"
      FileOpen $2 "$EXEDIR\optional\extensions\talkback@mozilla.org\InstallDisabled" w
      FileWrite $2 "$\r$\n"
      FileClose $2
    ${EndUnless}
  ${EndIf}

  ; Try to delete the app executable and if we can't delete it try to close the
  ; app. This allows running an instance that is located in another directory.
  ClearErrors
  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    ${DeleteFile} "$INSTDIR\${FileMainEXE}"
  ${EndIf}
  ${If} ${Errors}
    ClearErrors
    ${CloseApp} $(WARN_APP_RUNNING_INSTALL)
    ; Try to delete it again to prevent launching the app while we are
    ; installing.
    ${DeleteFile} "$INSTDIR\${FileMainEXE}"
    ClearErrors
  ${EndIf}

  Call CleanupOldLogs

  ${If} ${FileExists} "$INSTDIR\uninstall\uninstall.log"
    ; Diff cleanup.log with uninstall.bak
    ${LogHeader} "Updating Uninstall Log With XPInstall Wizard Logs"
    StrCpy $R0 "$INSTDIR\uninstall\uninstall.log"
    StrCpy $R1 "$INSTDIR\uninstall\cleanup.log"
    GetTempFileName $R2
    FileOpen $R3 $R2 w
    ${TextCompare} "$R1" "$R0" "SlowDiff" "GetDiff"
    FileClose $R3

    ${Unless} ${Errors}
      ${FileJoin} "$INSTDIR\uninstall\uninstall.log" "$R2" "$INSTDIR\uninstall\uninstall.log"
    ${EndUnless}
    ${DeleteFile} "$INSTDIR\uninstall\cleanup.log"
    ${DeleteFile} "$R2"
    ${DeleteFile} "$INSTDIR\uninstall\uninstall.bak"
    Rename "$INSTDIR\uninstall\uninstall.log" "$INSTDIR\uninstall\uninstall.bak"
  ${EndIf}

  ${Unless} ${FileExists} "$INSTDIR\uninstall"
    CreateDirectory "$INSTDIR\uninstall"
  ${EndUnless}

  FileOpen $fhUninstallLog "$INSTDIR\uninstall\uninstall.log" w
  FileOpen $fhInstallLog "$INSTDIR\install.log" w

  ${GetTime} "" "L" $0 $1 $2 $3 $4 $5 $6
  FileWrite $fhInstallLog "${BrandFullName} Installation Started: $2-$1-$0 $4:$5:$6"
  Call WriteLogSeparator

  ${LogHeader} "Installation Details"
  ${LogMsg} "Install Dir: $INSTDIR"
  ${LogMsg} "Locale     : ${AB_CD}"
  ${LogMsg} "App Version: ${AppVersion}"
  ${LogMsg} "GRE Version: ${GREVersion}"

  ${If} ${FileExists} "$EXEDIR\config\removed-files.log"
    ${LogHeader} "Removing Obsolete Files and Directories"
    ${LineFind} "$EXEDIR\config\removed-files.log" "/NUL" "1:-1" "onInstallDeleteFile"
    ${LineFind} "$EXEDIR\config\removed-files.log" "/NUL" "1:-1" "onInstallRemoveDir"
  ${EndIf}

  ${DeleteFile} "$INSTDIR\install_wizard.log"
  ${DeleteFile} "$INSTDIR\install_status.log"

  ${LogHeader} "Installing Main Files"
  StrCpy $R0 "$EXEDIR\nonlocalized"
  StrCpy $R1 "$INSTDIR"
  Call DoCopyFiles

  ; Register DLLs
  ; XXXrstrong - AccessibleMarshal.dll can be used by multiple applications but
  ; is only registered for the last application installed. When the last
  ; application installed is uninstalled AccessibleMarshal.dll will no longer be
  ; registered. bug 338878
  ${LogHeader} "DLL Registration"
  ClearErrors
  RegDLL "$INSTDIR\AccessibleMarshal.dll"
  ${If} ${Errors}
    ${LogMsg} "** ERROR Registering: $INSTDIR\AccessibleMarshal.dll **"
  ${Else}
    ${LogUninstall} "DLLReg: \AccessibleMarshal.dll"
    ${LogMsg} "Registered: $INSTDIR\AccessibleMarshal.dll"
  ${EndIf}

  ; Write extra files created by the application to the uninstall.log so they
  ; will be removed when the application is uninstalled. To remove an empty
  ; directory write a bogus filename to the deepest directory and all empty
  ; parent directories will be removed.
  ${LogUninstall} "File: \components\compreg.dat"
  ${LogUninstall} "File: \components\xpti.dat"
  ${LogUninstall} "File: \.autoreg"
  ${LogUninstall} "File: \active-update.xml"
  ${LogUninstall} "File: \install.log"
  ${LogUninstall} "File: \install_status.log"
  ${LogUninstall} "File: \install_wizard.log"
  ${LogUninstall} "File: \updates.xml"

  ${LogHeader} "Installing Localized Files"
  StrCpy $R0 "$EXEDIR\localized"
  StrCpy $R1 "$INSTDIR"
  Call DoCopyFiles

  ${If} $InstallType != 4
    Call install_talkback
    ${If} ${FileExists} "$INSTDIR\extensions\inspector@mozilla.org"
      Call install_inspector
    ${EndIf}
  ${EndIf}

  !include /NONFATAL instfiles-extra.nsi

  ; Create Start Menu shortcuts
  ${LogHeader} "Adding Shortcuts"
  ${If} $AddStartMenuSC == 1
    CreateDirectory "$SMPROGRAMS\$StartMenuDir"
    CreateShortCut "$SMPROGRAMS\$StartMenuDir\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $SMPROGRAMS\$StartMenuDir\${BrandFullName}.lnk"
    CreateShortCut "$SMPROGRAMS\$StartMenuDir\${BrandFullName} ($(SAFE_MODE)).lnk" "$INSTDIR\${FileMainEXE}" "-safe-mode" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $SMPROGRAMS\$StartMenuDir\${BrandFullName} ($(SAFE_MODE)).lnk"
  ${Else}
    ${If} ${FileExists} "$SMPROGRAMS\$StartMenuDir"
      ${If} ${FileExists} "$SMPROGRAMS\$StartMenuDir\${BrandFullName}.lnk"
        Delete "$SMPROGRAMS\$StartMenuDir\${BrandFullName}.lnk"
        ${If} ${Errors}
          ClearErrors
          ${LogMsg} "** ERROR Deleting Start Menu Shortcut: $SMPROGRAMS\$StartMenuDir\${BrandFullName}.lnk **"
        ${Else}
          ${LogMsg} "Removed Start Menu Shortcut: $SMPROGRAMS\$StartMenuDir\${BrandFullName}.lnk"
        ${EndIf}
      ${EndIf}
      ${If} ${FileExists} "$SMPROGRAMS\$StartMenuDir\${BrandFullName} ($(SAFE_MODE)).lnk"
        Delete "$SMPROGRAMS\$StartMenuDir\${BrandFullName} ($(SAFE_MODE)).lnk"
        ${If} ${Errors}
          ClearErrors
          ${LogMsg} "** ERROR Deleting Start Menu Shortcut: $SMPROGRAMS\$StartMenuDir\${BrandFullName} ($(SAFE_MODE)).lnk **"
        ${Else}
          ${LogMsg} "Removed Start Menu Shortcut: $SMPROGRAMS\$StartMenuDir\${BrandFullName} ($(SAFE_MODE)).lnk"
        ${EndIf}
      ${EndIf}
      RmDir "$SMPROGRAMS\$StartMenuDir"
      ${If} ${Errors}
        ClearErrors
        ${LogMsg} "** ERROR Removing Start Menu Directory: $SMPROGRAMS\$StartMenuDir **"
      ${Else}
        ${LogMsg} "Removed Start Menu Shortcut: $SMPROGRAMS\$StartMenuDir"
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ; perhaps use the uninstall keys
  ${If} $AddQuickLaunchSC == 1
    CreateShortCut "$QUICKLAUNCH\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $QUICKLAUNCH\${BrandFullName}.lnk"
  ${Else}
    ${DeleteFile} "$QUICKLAUNCH\${BrandFullName}.lnk"
  ${EndIf}

  ${LogHeader} "Updating Quick Launch Shortcuts"
  ${If} $AddDesktopSC == 1
    CreateShortCut "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $DESKTOP\${BrandFullName}.lnk"
  ${Else}
    ${DeleteFile} "$DESKTOP\${BrandFullName}.lnk"
  ${EndIf}

  !insertmacro MUI_STARTMENU_WRITE_END

  ; Refresh destop icons
  System::Call "shell32::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)"

  WriteUninstaller "$INSTDIR\uninstall\uninstaller.exe"

SectionEnd

Section /o "Developer Tools" Section2
  Call install_inspector
SectionEnd


Section /o "Quality Feedback Agent" Section3
  Call install_talkback
SectionEnd

Function install_inspector
  ${If} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
    ${RemoveDir} "$INSTDIR\extensions\extensions\inspector@mozilla.org"
    ClearErrors
    ${LogHeader} "Installing Developer Tools"
    StrCpy $R0 "$EXEDIR\optional\extensions\inspector@mozilla.org"
    StrCpy $R1 "$INSTDIR\extensions\inspector@mozilla.org"
    Call DoCopyFiles
  ${EndIf}
FunctionEnd

Function install_talkback
  StrCpy $R0 "$EXEDIR\optional\extensions\talkback@mozilla.org"
  ${If} ${FileExists} "$R0"
    StrCpy $R1 "$INSTDIR\extensions\talkback@mozilla.org"
    ${If} ${FileExists} "$R1"
      ; If there is an existing InstallDisabled file copy it to the source dir.
      ; This will add it during install to the uninstall.log and retains the
      ; original disabled state from the installation.
      ${If} ${FileExists} "$R1\InstallDisabled"
        CopyFiles "$R1\InstallDisabled" "$R0"
      ${EndIf}
      ; Remove the existing install of talkback
      RmDir /r "$R1"
    ${ElseIf} $InstallType == 1
      ; For standard installations only enable talkback for the x percent as
      ; defined by the application. We use QueryPerformanceCounter for the seed
      ; since it returns a 64bit integer which should improve the accuracy.
      System::Call "kernel32::QueryPerformanceCounter(*l.r1)"
      System::Int64Op $1 % 100
      Pop $0
      ; The percentage provided by the application refers to the percentage to
      ; include so all numbers equal or greater than should be disabled.
      ${If} $0 >= ${RandomPercent}
        FileOpen $2 "$R0\InstallDisabled" w
        FileWrite $2 "$\r$\n"
        FileClose $2
      ${EndIf}
    ${EndIf}
    ClearErrors
    ${LogHeader} "Installing Quality Feedback Agent"
    Call DoCopyFiles
  ${EndIf}
FunctionEnd

Section "Uninstall"
  !include /NONFATAL uninstfiles-extra.nsi

  ; Remove files. If we don't have a log file skip
  ${If} ${FileExists} "$INSTDIR\uninstall\uninstall.log"
    ; Copy the uninstall log file to a temporary file
    GetTempFileName $TmpVal
    CopyFiles "$INSTDIR\uninstall\uninstall.log" "$TmpVal"

    ; Unregister DLL's
    ${un.LineFind} "$TmpVal" "/NUL" "1:-1" "un.UnRegDLLsCallback"

    ; Delete files
    ${un.LineFind} "$TmpVal" "/NUL" "1:-1" "un.RemoveFilesCallback"

    ; Remove directories we always control
    RmDir /r "$INSTDIR\uninstall"
    RmDir /r "$INSTDIR\updates"
    RmDir /r "$INSTDIR\defaults\shortcuts"

    ; Remove empty directories
    ${un.LineFind} "$TmpVal" "/NUL" "1:-1" "un.RemoveDirsCallback"

    ; Delete the temporary uninstall log file
    ${DeleteFile} "$TmpVal"

    ; Remove the installation directory if it is empty
    ${RemoveDir} "$INSTDIR"
  ${EndIf}
SectionEnd

; When we add an optional action to the finish page the cancel button is
; enabled. The next two function disable it for install and uninstall and leave
; the finish button as the only choice.
Function disableCancel
  !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "cancelenabled" "0"
FunctionEnd

Function un.disableCancel
  !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "cancelenabled" "0"

  ; Only setup the survey controls, functions, etc. when the appLocale.nsi
  ; has DO_UNINSTALL_SURVEY defined to indicate the survey has been localized.
  !ifndef DO_UNINSTALL_SURVEY
    !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "NumFields" "3"
  !else
    StrCpy $TmpVal "SOFTWARE\Microsoft\IE Setup\Setup"
    ClearErrors
    ReadRegStr $0 HKLM $TmpVal "Path"
    ${If} ${Errors}
      !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "NumFields" "3"
    ${Else}
      ExpandEnvStrings $0 "$0" ; this value will usually contain %programfiles%
      ${If} $0 != "\"
        StrCpy $0 "$0\"
      ${EndIf}
      StrCpy $0 "$0\iexplore.exe"
      ClearErrors
      GetFullPathName $TmpVal $0
      ${If} ${Errors}
        !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "NumFields" "3"
      ${EndIf}
    ${EndIf}
  !endif
FunctionEnd

Function Options
  !insertmacro MUI_HEADER_TEXT $(OPTIONS_PAGE_TITLE) $(OPTIONS_PAGE_SUBTITLE)
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "options.ini"
FunctionEnd


Function ChangeOptions
  ${MUI_INSTALLOPTIONS_READ} $0 "options.ini" "Settings" "State"
;  ReadINIStr $0 "$PLUGINSDIR\options.ini" "Settings" "State"
  ${If} $0 != 0
    Abort
  ${EndIf}
  ${MUI_INSTALLOPTIONS_READ} $R0 "options.ini" "Field 2" "State"
  StrCmp $R0 "1" +1 +2
  StrCpy $InstallType "1"
  ${MUI_INSTALLOPTIONS_READ} $R0 "options.ini" "Field 3" "State"
  StrCmp $R0 "1" +1 +2
  StrCpy $InstallType "2"
  ${MUI_INSTALLOPTIONS_READ} $R0 "options.ini" "Field 4" "State"
  StrCmp $R0 "1" +1 +2
  StrCpy $InstallType "4"
FunctionEnd

Function preShortcuts
  Call CheckCustom
  !insertmacro MUI_HEADER_TEXT "$(SHORTCUTS_PAGE_TITLE)" "$(SHORTCUTS_PAGE_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "shortcuts.ini"
FunctionEnd

Function ChangeShortcuts
  ReadINIStr $0 "$PLUGINSDIR\options.ini" "Settings" "State"
  ${If} $0 != 0
    Abort
  ${EndIf}
  ${MUI_INSTALLOPTIONS_READ} $AddDesktopSC "shortcuts.ini" "Field 2" "State"
  ${MUI_INSTALLOPTIONS_READ} $AddStartMenuSC "shortcuts.ini" "Field 3" "State"
  ${MUI_INSTALLOPTIONS_READ} $AddQuickLaunchSC "shortcuts.ini" "Field 4" "State"
FunctionEnd

Function GetDiff
  ${TrimNewLines} "$9" "$9"
  ${If} $9 != ""
    FileWrite $R3 "$9$\r$\n"
    ${LogMsg} "Added To Uninstall Log: $9"
  ${EndIf}
  Push 0
FunctionEnd

Function DoCopyFiles
  StrLen $R2 $R0
  ${Locate} "$R0" "/L=FD" "CopyFile"
FunctionEnd

Function CopyFile
  StrCpy $R3 $R8 "" $R2
  ${If} $R6 ==  ""
    ${Unless} ${FileExists} "$R1$R3\$R7"
      ClearErrors
      CreateDirectory "$R1$R3\$R7"
      ${If} ${Errors}
        ${LogMsg}  "** ERROR Creating Directory: $R1$R3\$R7 **"
      ${Else}
        ${LogMsg}  "Created Directory: $R1$R3\$R7"
      ${EndIf}
    ${EndUnless}
  ${Else}
    ${Unless} ${FileExists} "$R1$R3"
      ClearErrors
      CreateDirectory "$R1$R3"
      ${If} ${Errors}
        ${LogMsg}  "** ERROR Creating Directory: $R1$R3 **"
      ${Else}
        ${LogMsg}  "Created Directory: $R1$R3"
      ${EndIf}
    ${EndUnless}
    ${If} ${FileExists} "$R1$R3\$R7"
      Delete "$R1$R3\$R7"
    ${EndIf}
    ClearErrors
    CopyFiles /SILENT $R9 "$R1$R3"
    ${If} ${Errors}
      ; XXXrstrong - what should we do if there is an error installing a file?
      ${LogMsg} "** ERROR Installing File: $R1$R3\$R7 **"
    ${Else}
      ${LogMsg} "Installed File: $R1$R3\$R7"
    ${EndIf}
    ; If the file is installed into the installation directory remove the
    ; installation directory's path from the file path when writing to the
    ; uninstall.log so it will be a relative path. This allows the same
    ; uninstaller.exe to be used with zip builds if we supply an uninstall.log.
    ${WordReplace} "$R1$R3\$R7" "$INSTDIR" "" "+" $R3
    ${LogUninstall} "File: $R3"
  ${EndIf}
  Push 0
FunctionEnd

; There must be a cleaner way of doing this but this will suffice for now.
Function un.RemoveRegEntriesCallback
  ${un.TrimNewLines} "$R9" "$R9"
  StrCpy $R3 "$R9" 7
  ${If} $R3 == "RegVal:"
  ${OrIf} $R3 == "RegKey:"
    StrCpy $R9 "$R9" "" 8
    ${un.WordFind} "$R9" " | " "+1" $R0
    ${un.WordFind} "$R9" " | " "+2" $R1
    ${If} $R3 == "RegVal:"
      ${un.WordFind} "$R9" " | " "+3" $R2
      ; When setting the default value the name will be empty and WordFind will
      ; return the entire string so copy an empty string into $R2 when this
      ; occurs.
      ${If} $R2 == $R9
        StrCpy $R2 ""
      ${EndIf}
    ${EndIf}
    ${Switch} $R0
      ${Case} "HKCR"
        ${If} $R3 == "RegVal:"
          DeleteRegValue HKCR "$R1" "$R2"
        ${EndIf}
        DeleteRegKey /ifempty HKCR "$R1"
        ${Break}
      ${Case} "HKCU"
        ${If} $R3 == "RegVal:"
          DeleteRegValue HKCU "$R1" "$R2"
        ${EndIf}
        DeleteRegKey /ifempty HKCU "$R1"
        ${Break}
      ${Case} "HKLM"
        ${If} $R3 == "RegVal:"
          DeleteRegValue HKLM "$R1" "$R2"
        ${EndIf}
        DeleteRegKey /ifempty HKLM "$R1"
        ${Break}
    ${EndSwitch}
  ${EndIf}
  ClearErrors
  Push 0
FunctionEnd

Function un.RemoveFilesCallback
  ${un.TrimNewLines} "$R9" "$R9"
  StrCpy $R1 "$R9" 5
  ${If} $R1 == "File:"
    StrCpy $R9 "$R9" "" 6
    StrCpy $R0 "$R9" 1
    ; If the path is relative prepend the install directory
    ${If} $R0 == "\"
      StrCpy $R0 "$INSTDIR$R9"
    ${Else}
      StrCpy $R0 "$R9"
    ${EndIf}
    ${If} ${FileExists} "$R0"
      ${DeleteFile} "$R0"
    ${EndIf}
  ${EndIf}
  ClearErrors
  Push 0
FunctionEnd

; Using locate will leave file handles open to some of the directories which
; will prevent the deletion of these directories. This parses the uninstall.log
; and uses the file entries to find / remove empty directories.
Function un.RemoveDirsCallback
  ${un.TrimNewLines} "$R9" "$R9"
  StrCpy $R1 "$R9" 5
  ${If} $R1 == "File:"
    StrCpy $R9 "$R9" "" 6
    StrCpy $R1 "$R9" 1
    ${If} $R1 == "\"
  	  StrCpy $R2 "INSTDIR"
      StrCpy $R1 "$INSTDIR$R9"
    ${Else}
  	  StrCpy $R2 " "
      StrCpy $R1 "$R9"
    ${EndIf}
    loop:
      Push $R1
      Call un.GetParentDir
      Pop $R0
      GetFullPathName $R1 "$R0"
      ${If} ${FileExists} "$R1"
        RmDir "$R1"
      ${EndIf}
      ${If} ${Errors}
      ${OrIf} $R2 != "INSTDIR"
        GoTo end
      ${Else}
        GoTo loop
      ${EndIf}
  ${EndIf}

  end:
    ClearErrors
    Push 0
FunctionEnd

Function un.UnRegDLLsCallback
  ${un.TrimNewLines} "$R9" "$R9"
  StrCpy $R1 "$R9" 7
  ${If} $R1 == "DLLReg:"
    StrCpy $R9 "$R9" "" 8
    StrCpy $R1 "$R9" 1
    ${If} $R1 == "\"
      StrCpy $R1 "$INSTDIR$R9"
    ${Else}
      StrCpy $R1 "$R9"
    ${EndIf}
    UnRegDLL $R1
  ${EndIf}
  ClearErrors
  Push 0
FunctionEnd

Function un.GetParentDir
   Exch $R0 ; old $R0 is on top of stack
   Push $R1
   Push $R2
   Push $R3
   StrLen $R3 $R0
   loop:
     IntOp $R1 $R1 - 1
     IntCmp $R1 -$R3 exit exit
     StrCpy $R2 $R0 1 $R1
     StrCmp $R2 "\" exit
   Goto loop
   exit:
     StrCpy $R0 $R0 $R1
     Pop $R3
     Pop $R2
     Pop $R1
     Exch $R0 ; put $R0 on top of stack, restore $R0 to original value
 FunctionEnd

; Clean up the old log files. We only diff the first two found since it is
; possible for there to be several MB and comparing that many would take a very
; long time to diff.
Function CleanupOldLogs
  FindFirst $0 $TmpVal "$INSTDIR\uninstall\*wizard*"
  StrCmp $TmpVal "" done
  StrCpy $TmpVal "$INSTDIR\uninstall\$TmpVal"

  FindNext $0 $1
  StrCmp $1 "" cleanup
  StrCpy $1 "$INSTDIR\uninstall\$1"
  Push $1
  Call DiffOldLogFiles
  FindClose $0
  ${DeleteFile} "$1"

  cleanup:
    StrCpy $2 "$INSTDIR\uninstall\cleanup.log"
    ${DeleteFile} "$2"
    FileOpen $R2 $2 w
    Push $TmpVal
    ${LineFind} "$INSTDIR\uninstall\$TmpVal" "/NUL" "1:-1" "CleanOldLogFilesCallback"
    ${DeleteFile} "$INSTDIR\uninstall\$TmpVal"
  done:
    FindClose $0
    FileClose $R2
    FileClose $R3
FunctionEnd

Function DiffOldLogFiles
  StrCpy $R1 "$1"
  GetTempFileName $R2
  FileOpen $R3 $R2 w
  ${TextCompare} "$R1" "$TmpVal" "SlowDiff" "GetDiff"
  FileClose $R3
  ${FileJoin} "$TmpVal" "$R2" "$TmpVal"
  ${DeleteFile} "$R2"
FunctionEnd

Function CleanOldLogFilesCallback
  ${TrimNewLines} "$R9" $R9
  ${WordReplace} "$R9" "$INSTDIR" "" "+" $R3
  ${WordFind} "$R9" "	" "E+1}" $R0
  IfErrors updater 0

  ${WordFind} "$R0" "Installing: " "E+1}" $R1
  ${Unless} ${Errors}
    FileWrite $R2 "File: $R1$\r$\n"
    GoTo done
  ${EndUnless}

  ${WordFind} "$R0" "Replacing: " "E+1}" $R1
  ${Unless} ${Errors}
    FileWrite $R2 "File: $R1$\r$\n"
    GoTo done
  ${EndUnless}

  ${WordFind} "$R0" "Windows Shortcut: " "E+1}" $R1
  ${Unless} ${Errors}
    FileWrite $R2 "File: $R1.lnk$\r$\n"
    GoTo done
  ${EndUnless}

  ${WordFind} "$R0" "Create Folder: " "E+1}" $R1
  ${Unless} ${Errors}
    FileWrite $R2 "Dir: $R1$\r$\n"
    GoTo done
  ${EndUnless}

  updater:
    ${WordFind} "$R9" "installing: " "E+1}" $R0
    ${Unless} ${Errors}
      FileWrite $R2 "File: $R0$\r$\n"
    ${EndUnless}

  done:
    Push 0
FunctionEnd

/*
install_wizard1.log

     [12/12]	Installing: C:\Program Files\Mozilla Thunderbird\extensions\talkback@mozilla.org\components\talkback.cnt
     ** performInstall() returned: 0

     Install completed successfully  --  2005-12-05 11:36:25

     ** copy file: C:\WINDOWS\UninstallThunderbird.exe to C:\Program Files\Mozilla Thunderbird\uninstall\UninstallThunderbird.exe

UPDATE [Wed, 21 Dec 2005 20:32:51 GMT]
installing: C:\Program Files\Mozilla Thunderbird\softokn3.chk
*/

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${Section1} $(APP_DESC)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section2} $(DEV_TOOLS_DESC)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section3} $(QFA_DESC)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
