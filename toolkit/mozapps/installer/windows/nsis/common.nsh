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
# The Initial Developer of the Original Code is Robert Strong
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

################################################################################
# Helper defines and macros

!define MUI_INSTALLOPTIONS_READ "!insertmacro MUI_INSTALLOPTIONS_READ"

/**
 * Posts WM_QUIT to the application's message window class.
 * @param   _MSG
 *          The message text to display in the message box.
 */
!macro CloseApp _MSG
  StrCpy $3 ${WindowClass}
  loop:
    FindWindow $1 "${WindowClass}"
    IntCmp $1 0 done
    MessageBox MB_OKCANCEL|MB_ICONSTOP "${_MSG}" IDCANCEL exit 0
    ; Only post this one time.
    System::Call 'user32::PostMessage(i r1, i ${WM_QUIT}, i 0, i 0)'
    # The amount of time to wait for the app to shutdown before prompting again
    Sleep 4000
    Goto loop
  exit:
    Quit
  done:
!macroend

/**
 * Removes quotes and trailing path separator from registry string paths.
 * @param   $R0
 *          Contains the registry string
 * @return  Modified string at the top of the stack.
 */
!macro GetPathFromRegStr
  Exch $R0 ; old $R0 is on top of stack
  Push $R9

  StrCpy $R9 "$R0" "" -1
  ${If} $R9 == '"'
  ${OrIf} $R9 == "'"
    StrCpy $R0 "$R0" -1
  ${EndIf}
  StrCpy $R9 "$R0" 1
  ${If} $R9 == '"'
  ${OrIf} $R9 == "'"
    StrCpy $R0 "$R0" "" 1
  ${EndIf}
  StrCpy $R9 "$R0" "" -1
  ${If} $R9 == "\"
    StrCpy $R0 "$R0" -1
  ${EndIf}
  ClearErrors
  GetFullPathName $R0 $R0

  Pop $R9
  Exch $R0 ; put $R0 on top of stack, restore $R0 to original value
!macroend
!define GetPathFromRegStr "!insertmacro GetPathFromRegStr"

/**
 * Attempts to delete a file if it exists. This will fail if the file is in use.
 * @param   _FILE
 *          The path to the file that is to be deleted.
 */
!macro DeleteFile _FILE
  ${If} ${FileExists} "${_FILE}"
    Delete "${_FILE}"
  ${EndIf}
!macroend
!define DeleteFile "!insertmacro DeleteFile"

/**
 * Removes a directory if it exists and is empty.
 * @param   _DIR
 *          The path to the directory that is to be removed.
 */
!macro RemoveDir _DIR
  ${If} ${FileExists} "${_DIR}"
    RmDir "${_DIR}"
  ${EndIf}
!macroend
!define RemoveDir "!insertmacro RemoveDir"

/**
 * Adds a section header to the human readable log.
 * @param   _HEADER
 *          The header text to write to the log.
 */
!macro LogHeader _HEADER
  Call WriteLogSeparator
  FileWrite $fhInstallLog "${_HEADER}"
  Call WriteLogSeparator
!macroend
!define LogHeader "!insertmacro LogHeader"

/**
 * Adds a section message to the human readable log.
 * @param   _MSG
 *          The message text to write to the log.
 */
!macro LogMsg _MSG
  FileWrite $fhInstallLog "  ${_MSG}$\r$\n"
!macroend
!define LogMsg "!insertmacro LogMsg"

/**
 * Adds a message to the uninstall log.
 * @param   _MSG
 *          The message text to write to the log.
 */
!macro LogUninstall _MSG
  FileWrite $fhUninstallLog "${_MSG}$\r$\n"
!macroend
!define LogUninstall "!insertmacro LogUninstall"

/**
 * Writes a registry string to both HKLM and HKCU. This will log the actions to
 * the install and uninstall logs.
 * @param   _KEY
 *          The subkey in relation to the key root.
 * @param   _NAME
 *          The key name to write to.
 * @param   _STR
 *          The string to write to the key name.
 */
!macro WriteRegStrHKLMandHKCU _KEY _NAME _STR
  ${WriteRegStr} HKCU "${_KEY}" "${_NAME}" "${_STR}"
  ${WriteRegStr} HKLM "${_KEY}" "${_NAME}" "${_STR}"
!macroend
!define WriteRegStrHKLMandHKCU "!insertmacro WriteRegStrHKLMandHKCU"

/**
 * Writes a registry string using the supplied params and logs the action to the
 * install and uninstall logs.
 * @param   _ROOT
 *          The registry key root as defined by NSIS (e.g. HKLM, HKCU, etc.).
 * @param   _KEY
 *          The subkey in relation to the key root.
 * @param   _NAME
 *          The key name to write to.
 * @param   _STR
 *          The string to write to the key name.
 */
!macro WriteRegStr _ROOT _KEY _NAME _STR
  ClearErrors
  WriteRegStr ${_ROOT} "${_KEY}" "${_NAME}" "${_STR}"
  ${If} ${Errors}
    ${LogMsg} "** ERROR Adding Registry String: ${_ROOT} | ${_KEY} | ${_NAME} | ${_STR} **"
  ${Else}
    ${LogUninstall} "RegVal: ${_ROOT} | ${_KEY} | ${_NAME}"
    ${LogMsg} "Added Registry String: ${_ROOT} | ${_KEY} | ${_NAME} | ${_STR}"
  ${EndIf}
!macroend
!define WriteRegStr "!insertmacro WriteRegStr"

/**
 * Writes a registry dword to both HKLM and HKCU. This will log the actions to
 * the install and uninstall logs.
 * @param   _KEY
 *          The subkey in relation to the key root.
 * @param   _NAME
 *          The key name to write to.
 * @param   _DWORD
 *          The dword to write to the key name.
 */
!macro WriteRegDWORDHKLMandHKCU _KEY _NAME _DWORD
  ${WriteRegDWORD} HKCU "${_KEY}" "${_NAME}" "${_DWORD}"
  ${WriteRegDWORD} HKLM "${_KEY}" "${_NAME}" "${_DWORD}"
!macroend
!define WriteRegDWORDHKLMandHKCU "!insertmacro WriteRegDWORDHKLMandHKCU"

/**
 * Writes a registry dword using the supplied params and logs the action to the
 * install and uninstall logs.
 * @param   _ROOT
 *          The registry key root as defined by NSIS (e.g. HKLM, HKCU, etc.).
 * @param   _KEY
 *          The subkey in relation to the key root.
 * @param   _NAME
 *          The key name to write to.
 * @param   _DWORD
 *          The dword to write to the key name.
 */
!macro WriteRegDWORD _ROOT _KEY _NAME _DWORD
  ClearErrors
  WriteRegDWORD ${_ROOT} "${_KEY}" "${_NAME}" "${_DWORD}"
  ${If} ${Errors}
    ${LogMsg} "** ERROR Adding Registry DWORD: ${_ROOT} | ${_KEY} | ${_NAME} | ${_DWORD} **"
  ${Else}
    ${LogUninstall} "RegVal: ${_ROOT} | ${_KEY} | ${_NAME}"
    ${LogMsg} "Added Registry DWORD: ${_ROOT} | ${_KEY} | ${_NAME} | ${_DWORD}"
  ${EndIf}
!macroend
!define WriteRegDWORD "!insertmacro WriteRegDWORD"


/**
 * Creates a registry key. NSIS doesn't supply a RegCreateKey method and instead
 * will auto create keys when a reg key name value pair is set.
 */
!define RegCreateKey "Advapi32::RegCreateKeyA(i, t, *i) i"

/**
 * Creates a registry key. This will log the actions to the install and
 * uninstall logs.
 * @param   _ROOT
 *          The registry key root as defined by NSIS (e.g. HKLM, HKCU, etc.).
 * @param   _KEY
 *          The subkey in relation to the key root.
 */
!macro CreateRegKey _ROOT _KEY
  Push $0 ; old $0 is on top of stack
  Push $R9
  ${Switch} ${_ROOT}
    ${Case} "HKCR"
      StrCpy $R9 "0x80000000"
      ${Break}
    ${Case} "HKCU"
      StrCpy $R9 "0x80000001"
      ${Break}
    ${Case} "HKLM"
      StrCpy $R9 "0x80000002"
      ${Break}
    ${Default}
      Return
  ${EndSwitch}
  System::Call "${RegCreateKey}($R9, '${_KEY}', .r0) .r0"
  ${If} $0 != 0
    ${LogMsg} "** ERROR Adding Registry Key: ${_ROOT} | ${_KEY} **"
  ${Else}
    ${LogUninstall} "RegKey: ${_ROOT} | ${_KEY}"
    ${LogMsg} "Added Registry Key: ${_ROOT} | ${_KEY}"
  ${EndIf}

  Pop $R9
  Pop $0 ; put $0 on top of stack, restore $0 to original value
!macroend
!define CreateRegKey "!insertmacro CreateRegKey"
