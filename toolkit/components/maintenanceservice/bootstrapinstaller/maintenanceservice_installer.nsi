# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

; Set verbosity to 3 (e.g. no script) to lessen the noise in the build logs
!verbose 3

; 7-Zip provides better compression than the lzma from NSIS so we add the files
; uncompressed and use 7-Zip to create a SFX archive of it
SetDatablockOptimize on
SetCompress off
CRCCheck on

RequestExecutionLevel admin

Unicode true
ManifestSupportedOS all
ManifestDPIAware true

!addplugindir ./

; Variables
Var TempMaintServiceName
Var BrandFullNameDA
Var BrandFullName

; Other included files may depend upon these includes!
; The following includes are provided by NSIS.
!include FileFunc.nsh
!include LogicLib.nsh
!include MUI.nsh
!include WinMessages.nsh
!include WinVer.nsh
!include WordFunc.nsh

!insertmacro GetOptions
!insertmacro GetParameters
!insertmacro GetSize

; The test slaves use this fallback key to run tests.
; And anyone that wants to run tests themselves should already have 
; this installed.
!define FallbackKey \
  "SOFTWARE\Mozilla\MaintenanceService\3932ecacee736d366d6436db0f55bce4"

!define CompanyName "Mozilla Corporation"
!define BrandFullNameInternal ""

; The following includes are custom.
!include defines.nsi
; We keep defines.nsi defined so that we get other things like 
; the version number, but we redefine BrandFullName
!define MaintFullName "Mozilla Maintenance Service"
!undef BrandFullName
!define BrandFullName "${MaintFullName}"

!include common.nsh
!include locales.nsi

VIAddVersionKey "FileDescription" "${MaintFullName} Installer"
VIAddVersionKey "OriginalFilename" "maintenanceservice_installer.exe"

Name "${MaintFullName}"
OutFile "maintenanceservice_installer.exe"

; Get installation folder from registry if available
InstallDirRegKey HKLM "Software\Mozilla\MaintenanceService" ""

SetOverwrite on

!define MaintUninstallKey \
 "Software\Microsoft\Windows\CurrentVersion\Uninstall\MozillaMaintenanceService"

; Always install into the 32-bit location even if we have a 64-bit build.
; This is because we use only 1 service for all Firefox channels.
; Allow either x86 and x64 builds to exist at this location, depending on
; what is the latest build.
InstallDir "$PROGRAMFILES32\${MaintFullName}\"
ShowUnInstDetails nevershow

################################################################################
# Modern User Interface - MUI

!define MUI_ICON setup.ico
!define MUI_UNICON setup.ico
!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_UNWELCOMEFINISHPAGE_BITMAP wizWatermark.bmp

;Interface Settings
!define MUI_ABORTWARNING

; Uninstaller Pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

################################################################################
# Language

!insertmacro MOZ_MUI_LANGUAGE 'baseLocale'
!verbose push
!verbose 3
!include "overrideLocale.nsh"
!include "customLocale.nsh"
!verbose pop

; Set this after the locale files to override it if it is in the locale
; using " " for BrandingText will hide the "Nullsoft Install System..." branding
BrandingText " "

Function .onInit
  ; Remove the current exe directory from the search order.
  ; This only effects LoadLibrary calls and not implicitly loaded DLLs.
  System::Call 'kernel32::SetDllDirectoryW(w "")'

  SetSilent silent
  ${Unless} ${AtLeastWin7}
    Abort
  ${EndUnless}
FunctionEnd

Function un.onInit
  ; Remove the current exe directory from the search order.
  ; This only effects LoadLibrary calls and not implicitly loaded DLLs.
  System::Call 'kernel32::SetDllDirectoryW(w "")'

  StrCpy $BrandFullNameDA "${MaintFullName}"
  StrCpy $BrandFullName "${MaintFullName}"
FunctionEnd

Section "MaintenanceService"
  AllowSkipFiles off

  CreateDirectory $INSTDIR
  SetOutPath $INSTDIR

  ; If the service already exists, then it will be stopped when upgrading it
  ; via the maintenanceservice_tmp.exe command executed below.
  ; The maintenanceservice_tmp.exe command will rename the file to
  ; maintenanceservice.exe if maintenanceservice_tmp.exe is newer.
  ; If the service does not exist yet, we install it and drop the file on
  ; disk as maintenanceservice.exe directly.
  StrCpy $TempMaintServiceName "maintenanceservice.exe"
  IfFileExists "$INSTDIR\maintenanceservice.exe" 0 skipAlreadyExists
    StrCpy $TempMaintServiceName "maintenanceservice_tmp.exe"
  skipAlreadyExists:

  ; We always write out a copy and then decide whether to install it or 
  ; not via calling its 'install' cmdline which works by version comparison.
  CopyFiles "$EXEDIR\maintenanceservice.exe" "$INSTDIR\$TempMaintServiceName"

  ; The updater.ini file is only used when performing an install or upgrade,
  ; and only if that install or upgrade is successful.  If an old updater.ini
  ; happened to be copied into the maintenance service installation directory
  ; but the service was not newer, the updater.ini file would be unused.
  ; It is used to fill the description of the service on success.
  CopyFiles "$EXEDIR\updater.ini" "$INSTDIR\updater.ini"

  ; Install the application maintenance service.
  ; If a service already exists, the command line parameter will stop the
  ; service and only install itself if it is newer than the already installed
  ; service.  If successful it will remove the old maintenanceservice.exe
  ; and replace it with maintenanceservice_tmp.exe.
  ClearErrors
  ;${GetParameters} $0
  ;${GetOptions} "$0" "/Upgrade" $0
  ;${If} ${Errors}
    ExecWait '"$INSTDIR\$TempMaintServiceName" forceinstall'
  ;${Else}
    ; The upgrade cmdline is the same as install except
    ; It will fail if the service isn't already installed.
  ;  ExecWait '"$INSTDIR\$TempMaintServiceName" upgrade'
  ;${EndIf}

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${MaintUninstallKey}" "DisplayName" "${MaintFullName}"
  WriteRegStr HKLM "${MaintUninstallKey}" "UninstallString" \
                   '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${MaintUninstallKey}" "DisplayIcon" \
                   "$INSTDIR\Uninstall.exe,0"
  WriteRegStr HKLM "${MaintUninstallKey}" "DisplayVersion" "${AppVersion}"
  WriteRegStr HKLM "${MaintUninstallKey}" "Publisher" "Mozilla"
  WriteRegStr HKLM "${MaintUninstallKey}" "Comments" "${BrandFullName}"
  WriteRegDWORD HKLM "${MaintUninstallKey}" "NoModify" 1
  ${GetSize} "$INSTDIR" "/S=0K" $R2 $R3 $R4
  WriteRegDWORD HKLM "${MaintUninstallKey}" "EstimatedSize" $R2

  ; Write out that a maintenance service was attempted.
  ; We do this because on upgrades we will check this value and we only
  ; want to install once on the first upgrade to maintenance service.
  ; Also write out that we are currently installed, preferences will check
  ; this value to determine if we should show the service update pref.
  ; Since the Maintenance service can be installed either x86 or x64,
  ; always use the 64-bit registry for checking if an attempt was made.
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  WriteRegDWORD HKLM "Software\Mozilla\MaintenanceService" "Attempted" 1
  WriteRegDWORD HKLM "Software\Mozilla\MaintenanceService" "Installed" 1
  DeleteRegValue HKLM "Software\Mozilla\MaintenanceService" "FFPrefetchDisabled"

  ; Included here for debug purposes only.  
  ; These keys are used to bypass the installation dir is a valid installation
  ; check from the service so that tests can be run.
  WriteRegStr HKLM "${FallbackKey}\0" "name" "Mozilla Corporation"
  WriteRegStr HKLM "${FallbackKey}\0" "issuer" "DigiCert SHA2 Assured ID Code Signing CA"
  WriteRegStr HKLM "${FallbackKey}\1" "name" "Mozilla Fake SPC"
  WriteRegStr HKLM "${FallbackKey}\1" "issuer" "Mozilla Fake CA"
  ${If} ${RunningX64}
    SetRegView lastused
  ${EndIf}
SectionEnd

; By renaming before deleting we improve things slightly in case
; there is a file in use error. In this case a new install can happen.
Function un.RenameDelete
  Pop $9
  ; If the .moz-delete file already exists previously, delete it
  ; If it doesn't exist, the call is ignored.
  ; We don't need to pass /REBOOTOK here since it was already marked that way
  ; if it exists.
  Delete "$9.moz-delete"
  Rename "$9" "$9.moz-delete"
  ${If} ${Errors}
    Delete /REBOOTOK "$9"
  ${Else} 
    Delete /REBOOTOK "$9.moz-delete"
  ${EndIf}
  ClearErrors
FunctionEnd

Section "Uninstall"
  ; Delete the service so that no updates will be attempted
  ExecWait '"$INSTDIR\maintenanceservice.exe" uninstall'

  Push "$INSTDIR\updater.ini"
  Call un.RenameDelete
  Push "$INSTDIR\maintenanceservice.exe"
  Call un.RenameDelete
  Push "$INSTDIR\maintenanceservice_tmp.exe"
  Call un.RenameDelete
  Push "$INSTDIR\maintenanceservice.old"
  Call un.RenameDelete
  Push "$INSTDIR\Uninstall.exe"
  Call un.RenameDelete
  Push "$INSTDIR\update\updater.ini"
  Call un.RenameDelete
  Push "$INSTDIR\update\updater.exe"
  Call un.RenameDelete
  RMDir /REBOOTOK "$INSTDIR\update"
  RMDir /REBOOTOK "$INSTDIR"
  DeleteRegKey HKLM "${MaintUninstallKey}"

  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  DeleteRegValue HKLM "Software\Mozilla\MaintenanceService" "Installed"
  DeleteRegValue HKLM "Software\Mozilla\MaintenanceService" "FFPrefetchDisabled"
  DeleteRegKey HKLM "${FallbackKey}\"
  ${If} ${RunningX64}
    SetRegView lastused
  ${EndIf}
SectionEnd

