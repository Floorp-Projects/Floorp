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

################################################################################
# Helper defines and macros

/*
Avoid creating macros / functions that overwrite vars! If you decide to do so
document which var is being overwritten (see the GetSecondInstallPath macro for
an example).

Before using the vars get the passed in params and save existing var values to
the stack.

Exch $R9 ; exhange the original $R9 with the top of the stack
Exch 1   ; exchange the top of the stack with 1 below the top of the stack
Exch $R8 ; exchange the original $R8 with the top of the stack
Exch 2   ; exchange the top of the stack with 2 below the top of the stack
Exch $R7 ; exchange the original $R7 with the top of the stack
Push $R6 ; push the original $R6 onto the top of the stack
Push $R5 ; push the original $R5 onto the top of the stack
Push $R4 ; push the original $R4 onto the top of the stack

<do stuff>

Then restore the values.
Pop $R4  ; restore the value for $R4 from the top of the stack
Pop $R5  ; restore the value for $R5 from the top of the stack
Pop $R6  ; restore the value for $R6 from the top of the stack
Exch $R7 ; exchange the new $R7 value with the top of the stack
Exch 2   ; exchange the top of the stack with 2 below the top of the stack
Exch $R8 ; exchange the new $R8 value with the top of the stack
Exch 1   ; exchange the top of the stack with 2 below the top of the stack
Exch $R9 ; exchange the new $R9 value with the top of the stack
*/

!define MUI_INSTALLOPTIONS_READ "!insertmacro MUI_INSTALLOPTIONS_READ"

!macro GetParentDir
   Exch $R0
   Push $R1
   Push $R2
   Push $R3
   StrLen $R3 $R0
  ${DoWhile} 1 > 0
    IntOp $R1 $R1 - 1
    ${If} $R1 <= -$R3
      ${Break}
    ${EndIf}
    StrCpy $R2 $R0 1 $R1
    ${If} $R2 == "\"
      ${Break}
    ${EndIf}
  ${Loop}

  StrCpy $R0 $R0 $R1
  Pop $R3
  Pop $R2
  Pop $R1
  Exch $R0
!macroend
!define GetParentDir "!insertmacro GetParentDir"

/**
 * Removes quotes and trailing path separator from registry string paths.
 * @param   $R0
 *          Contains the registry string
 * @return  Modified string at the top of the stack.
 */
!macro GetPathFromRegStr
  Exch $R0
  Push $R8
  Push $R9

  StrCpy $R9 "$R0" "" -1
  StrCmp $R9 '"' +2 0
  StrCmp $R9 "'" 0 +2
  StrCpy $R0 "$R0" -1

  StrCpy $R9 "$R0" 1
  StrCmp $R9 '"' +2 0
  StrCmp $R9 "'" 0 +2
  StrCpy $R0 "$R0" "" 1

  StrCpy $R9 "$R0" "" -1
  StrCmp $R9 "\" 0 +2
  StrCpy $R0 "$R0" -1

  ClearErrors
  GetFullPathName $R8 $R0
  IfErrors 0 +2
  StrCpy $R0 $R8

  Pop $R9
  Pop $R8
  Exch $R0
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
 * Common installer macros used by toolkit applications.
 * The macros below will automatically prepend un. to the function names when
 * they are defined (e.g. !define un.RegCleanMain).
 */
!verbose push
!verbose 3
!ifndef _MOZFUNC_VERBOSE
  !define _MOZFUNC_VERBOSE 3
!endif
!verbose ${_MOZFUNC_VERBOSE}
!define MOZFUNC_VERBOSE "!insertmacro MOZFUNC_VERBOSE"
!define _MOZFUNC_UN
!define _MOZFUNC_S
!verbose pop

!macro MOZFUNC_VERBOSE _VERBOSE
  !verbose push
  !verbose 3
  !undef _MOZFUNC_VERBOSE
  !define _MOZFUNC_VERBOSE ${_VERBOSE}
  !verbose pop
!macroend

/**
 * Removes registry keys that reference this install location and for paths that
 * no longer exist. This uses SHCTX to determine the registry hive so you must
 * call SetShellVarContext first.
 * @param   _KEY
 *          The registry subkey (typically this will be Software\Mozilla).
 *
 * XXXrstrong - there is the potential for Key: Software/Mozilla/AppName,
 * ValueName: CurrentVersion, ValueData: AppVersion to reference a key that is
 * no longer available due to this cleanup. This should be no worse than prior
 * to this reg cleanup since the referenced key would be for an app that is no
 * longer installed on the system.
 *
 * $R2 = _KEY
 * $R3 = value returned from the outer loop's EnumRegKey
 * $R4 = value returned from the inner loop's EnumRegKey
 * $R5 = value returned from ReadRegStr
 * $R6 = counter for the outer loop's EnumRegKey
 * $R7 = counter for the inner loop's EnumRegKey
 * $R8 = value returned popped from the stack for GetPathFromRegStr macro
 * $R9 = value returned popped from the stack for GetParentDir macro
 */
!macro RegCleanMain

  !ifndef ${_MOZFUNC_UN}RegCleanMain
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}RegCleanMain "!insertmacro ${_MOZFUNC_UN}RegCleanMainCall"

    Function ${_MOZFUNC_UN}RegCleanMain
      Exch $R2
      Push $R3
      Push $R4
      Push $R5
      Push $R6
      Push $R7
      Push $R8
      Push $R9

      StrCpy $R6 0  ; set the counter for the outer loop to 0

      outerloop:
      EnumRegKey $R3 SHCTX $R2 $R6
      StrCmp $R3 "" end  ; if empty there are no more keys to enumerate
      IntOp $R6 $R6 + 1  ; increment the outer loop's counter
      ClearErrors
      ReadRegStr $R5 SHCTX "$R2\$R3\bin" "PathToExe"
      IfErrors 0 outercontinue
      StrCpy $R7 0  ; set the counter for the inner loop to 0

      innerloop:
      EnumRegKey $R4 SHCTX "$R2\$R3" $R7
      StrCmp $R4 "" outerloop  ; if empty there are no more keys to enumerate
      IntOp $R7 $R7 + 1  ; increment the inner loop's counter
      ClearErrors
      ReadRegStr $R5 SHCTX "$R2\$R3\$R4\Main" "PathToExe"
      IfErrors innerloop

      Push $R5
      ${GetPathFromRegStr}
      Pop $R8

      Push $R5
      ${GetParentDir}
      Pop $R9

      IfFileExists "$R8" 0 +2
      StrCmp $R9 $INSTDIR 0 innerloop
      ClearErrors
      DeleteRegKey SHCTX "$R2\$R3\$R4"
      IfErrors innerloop
      IntOp $R7 $R7 - 1 ; decrement the inner loop's counter when the key is deleted successfully.
      ClearErrors
      DeleteRegKey /ifempty SHCTX "$R2\$R3"
      IfErrors innerloop outerdecrement

      outercontinue:
      Push $R5
      ${GetPathFromRegStr}
      Pop $R8
      Push $R5
      ${GetParentDir}
      Pop $R9

      IfFileExists "$R8" 0 +2
      StrCmp $R9 $INSTDIR 0 outerloop
      ClearErrors
      DeleteRegKey SHCTX "$R2\$R3"
      IfErrors outerloop

      outerdecrement:
      IntOp $R6 $R6 - 1 ; decrement the outer loop's counter when the key is deleted successfully.
      GoTo outerloop

      end:
      ClearErrors

      Pop $R9
      Pop $R8
      Pop $R7
      Pop $R6
      Pop $R5
      Pop $R4
      Pop $R3
      Exch $R2
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro RegCleanMainCall _KEY
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_KEY}"
  Call RegCleanMain
  !verbose pop
!macroend

!macro un.RegCleanMainCall _KEY
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_KEY}"
  Call un.RegCleanMain
  !verbose pop
!macroend

!macro un.RegCleanMain
  !ifndef un.RegCleanMain
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro RegCleanMain

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
!macroend

/**
 * Removes all registry keys from HKLM\Software\Windows\CurrentVersion\Uninstall
 * that reference this install location.
 *
 * $R5 = value returned from ReadRegStr
 * $R6 = string for the base reg key (e.g. Software\Microsoft\Windows\CurrentVersion\Uninstall)
 * $R7 = value returned from the EnumRegKey
 * $R8 = counter for the EnumRegKey
 * $R9 = value returned from the stack from the GetPathFromRegStr macro
 */
!macro RegCleanUninstall

  !ifndef ${_MOZFUNC_UN}RegCleanUninstall
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}RegCleanUninstall "!insertmacro ${_MOZFUNC_UN}RegCleanUninstallCall"

    Function ${_MOZFUNC_UN}RegCleanUninstall
      Push $R9
      Push $R8
      Push $R7
      Push $R6
      Push $R5

      StrCpy $R6 "Software\Microsoft\Windows\CurrentVersion\Uninstall"
      StrCpy $R8 0
      StrCpy $R7 ""

      loop:
      EnumRegKey $R7 HKLM $R6 $R8
      StrCmp $R7 "" end
      IntOp $R8 $R8 + 1
      ClearErrors
      ReadRegStr $R5 HKLM "$R6\$R7" "InstallLocation"
      IfErrors loop
      Push $R5
      ${GetPathFromRegStr}
      Pop $R9
      StrCmp $R9 $INSTDIR 0 loop
      ClearErrors
      DeleteRegKey HKLM "$R6\$R7"
      IfErrors loop
      IntOp $R8 $R8 + 1
      GoTo loop

      end:
      ClearErrors

      Pop $R5
      Pop $R6
      Pop $R7
      Pop $R8
      Pop $R9
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro RegCleanUninstallCall
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Call RegCleanUninstall
  !verbose pop
!macroend

!macro un.RegCleanUninstallCall
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Call un.RegCleanUninstall
  !verbose pop
!macroend

!macro un.RegCleanUninstall
  !ifndef un.RegCleanUninstall
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro RegCleanUninstall

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
!macroend

/**
 * Posts WM_QUIT to the application's message window which is found using the
 * message window's class.
 * @param   _MSG
 *          The message text to display in the message box.
 *
 * $R8 = value returned from FindWindow
 * $R9 = _MSG
 */
!macro CloseApp

  !ifndef ${_MOZFUNC_UN}CloseApp
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}CloseApp "!insertmacro ${_MOZFUNC_UN}CloseAppCall"

    Function ${_MOZFUNC_UN}CloseApp
      Exch $R9
      Push $R8

      loop:
      FindWindow $R8 "${WindowClass}"
      IntCmp $R8 0 end
      MessageBox MB_OKCANCEL|MB_ICONQUESTION "$R9" IDCANCEL exit 0
      ; Only post this one time.
      System::Call 'user32::PostMessage(i r1, i ${WM_QUIT}, i 0, i 0)'
      # The amount of time to wait for the app to shutdown before prompting again
      Sleep 4000
      Goto loop

      exit:
      Quit

      end:

      Pop $R8
      Exch $R9
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro CloseAppCall _MSG
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_MSG}"
  Call CloseApp
  !verbose pop
!macroend

!macro un.CloseApp
  !ifndef un.CloseApp
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro CloseApp

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
!macroend

!macro un.CloseAppCall _MSG
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_MSG}"
  Call un.CloseApp
  !verbose pop
!macroend

/**
 * Writes a registry string using SHCTX and the supplied params and logs the
 * action to the install log and the uninstall log if _LOG_UNINSTALL equals 1.
 * @param   _ROOT
 *          The registry key root as defined by NSIS (e.g. HKLM, HKCU, etc.).
 *          This will only be used for logging.
 * @param   _KEY
 *          The subkey in relation to the key root.
 * @param   _NAME
 *          The key value name to write to.
 * @param   _STR
 *          The string to write to the key value name.
 * @param   _LOG_UNINSTALL
 *          0 = don't add to uninstall log, 1 = add to uninstall log.
 *
 * $R5 = _ROOT
 * $R6 = _KEY
 * $R7 = _NAME
 * $R8 = _STR
 * $R9 = _LOG_UNINSTALL
 */
!macro WriteRegStr2

  !ifndef ${_MOZFUNC_UN}WriteRegStr2
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}WriteRegStr2 "!insertmacro ${_MOZFUNC_UN}WriteRegStr2Call"

    Function ${_MOZFUNC_UN}WriteRegStr2
      Exch $R9
      Exch 1
      Exch $R8
      Exch 2
      Exch $R7
      Exch 3
      Exch $R6
      Exch 4
      Exch $R5

      ClearErrors
      WriteRegStr SHCTX "$R6" "$R7" "$R8"
      IfErrors 0 +3
      FileWrite $fhInstallLog "  ** ERROR Adding Registry String: $R5 | $R6 | $R7 | $R8 **$\r$\n"
      GoTo +4
      IntCmp $R9 1 0 +2
      FileWrite $fhUninstallLog "RegVal: $R5 | $R6 | $R7$\r$\n"
      FileWrite $fhInstallLog "  Added Registry String: $R5 | $R6 | $R7 | $R8$\r$\n"

      Exch $R5
      Exch 4
      Exch $R6
      Exch 3
      Exch $R7
      Exch 2
      Exch $R7
      Exch 1
      Exch $R9
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro WriteRegStr2Call _ROOT _KEY _NAME _STR _LOG_UNINSTALL
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_ROOT}"
  Push "${_KEY}"
  Push "${_NAME}"
  Push "${_STR}"
  Push "${_LOG_UNINSTALL}"
  Call WriteRegStr2
  !verbose pop
!macroend

!macro un.WriteRegStr2Call _ROOT _KEY _NAME _STR _LOG_UNINSTALL
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_ROOT}"
  Push "${_KEY}"
  Push "${_NAME}"
  Push "${_STR}"
  Push "${_LOG_UNINSTALL}"
  Call un.WriteRegStr2
  !verbose pop
!macroend

!macro un.WriteRegStr2
  !ifndef un.WriteRegStr2
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro WriteRegStr2

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
!macroend

/**
 * Writes a registry dword using SHCTX and the supplied params and logs the
 * action to the install log and the uninstall log if _LOG_UNINSTALL equals 1.
 * @param   _ROOT
 *          The registry key root as defined by NSIS (e.g. HKLM, HKCU, etc.).
 *          This will only be used for logging.
 * @param   _KEY
 *          The subkey in relation to the key root.
 * @param   _NAME
 *          The key value name to write to.
 * @param   _DWORD
 *          The dword to write to the key value name.
 * @param   _LOG_UNINSTALL
 *          0 = don't add to uninstall log, 1 = add to uninstall log.
 *
 * $R5 = _ROOT
 * $R6 = _KEY
 * $R7 = _NAME
 * $R8 = _DWORD
 * $R9 = _LOG_UNINSTALL
 */
!macro WriteRegDWORD2

  !ifndef ${_MOZFUNC_UN}WriteRegDWORD2
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}WriteRegDWORD2 "!insertmacro ${_MOZFUNC_UN}WriteRegDWORD2Call"

    Function ${_MOZFUNC_UN}WriteRegDWORD2
      Exch $R9
      Exch 1
      Exch $R8
      Exch 2
      Exch $R7
      Exch 3
      Exch $R6
      Exch 4
      Exch $R5

      ClearErrors
      WriteRegDWORD SHCTX "$R6" "$R7" "$R8"
      IfErrors 0 +3
      FileWrite $fhInstallLog "  ** ERROR Adding Registry DWord: $R5 | $R6 | $R7 | $R8 **$\r$\n"
      GoTo +4
      IntCmp $R5 1 0 +2
      FileWrite $fhUninstallLog "RegVal: $R5 | $R6 | $R7$\r$\n"
      FileWrite $fhInstallLog "  Added Registry DWord: $R5 | $R6 | $R7 | $R8$\r$\n"

      Exch $R5
      Exch 4
      Exch $R6
      Exch 3
      Exch $R7
      Exch 2
      Exch $R8
      Exch 1
      Exch $R9
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro WriteRegDWORD2Call _ROOT _KEY _NAME _DWORD _LOG_UNINSTALL
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_ROOT}"
  Push "${_KEY}"
  Push "${_NAME}"
  Push "${_DWORD}"
  Push "${_LOG_UNINSTALL}"
  Call WriteRegDWORD2
  !verbose pop
!macroend

!macro un.WriteRegDWORD2Call _ROOT _KEY _NAME _DWORD _LOG_UNINSTALL
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_ROOT}"
  Push "${_KEY}"
  Push "${_NAME}"
  Push "${_DWORD}"
  Push "${_LOG_UNINSTALL}"
  Call un.WriteRegDWORD2
  !verbose pop
!macroend

!macro un.WriteRegDWORD2
  !ifndef un.WriteRegDWORD2
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro WriteRegDWORD2

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
!macroend

/**
 * Writes a registry string to HKCR using the supplied params and logs the
 * action to the install log and the uninstall log if _LOG_UNINSTALL equals 1.
 * @param   _ROOT
 *          The registry key root as defined by NSIS (e.g. HKLM, HKCU, etc.).
 *          This will only be used for logging.
 * @param   _KEY
 *          The subkey in relation to the key root.
 * @param   _NAME
 *          The key value name to write to.
 * @param   _STR
 *          The string to write to the key value name.
 * @param   _LOG_UNINSTALL
 *          0 = don't add to uninstall log, 1 = add to uninstall log.
 *
 * $R5 = _ROOT
 * $R6 = _KEY
 * $R7 = _NAME
 * $R8 = _STR
 * $R9 = _LOG_UNINSTALL
 */
!macro WriteRegStrHKCR

  !ifndef ${_MOZFUNC_UN}WriteRegStrHKCR
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}WriteRegStrHKCR "!insertmacro ${_MOZFUNC_UN}WriteRegStrHKCRCall"

    Function ${_MOZFUNC_UN}WriteRegStrHKCR
      Exch $R9
      Exch 1
      Exch $R8
      Exch 2
      Exch $R7
      Exch 3
      Exch $R6
      Exch 4
      Exch $R5

      ClearErrors
      WriteRegStr HKCR "$R6" "$R7" "$R8"
      IfErrors 0 +3
      FileWrite $fhInstallLog "  ** ERROR Adding Registry String: $R5 | $R6 | $R7 | $R8 **$\r$\n"
      GoTo +4
      IntCmp $R5 1 0 +2
      FileWrite $fhUninstallLog "RegVal: $R5 | $R6 | $R7$\r$\n"
      FileWrite $fhInstallLog "  Added Registry String: $R5 | $R6 | $R7 | $R8$\r$\n"

      Exch $R5
      Exch 4
      Exch $R6
      Exch 3
      Exch $R7
      Exch 2
      Exch $R8
      Exch 1
      Exch $R9
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro WriteRegStrHKCRCall _ROOT _KEY _NAME _STR _LOG_UNINSTALL
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_ROOT}"
  Push "${_KEY}"
  Push "${_NAME}"
  Push "${_STR}"
  Push "${_LOG_UNINSTALL}"
  Call WriteRegStrHKCR
  !verbose pop
!macroend

!macro un.WriteRegStrHKCRCall _ROOT _KEY _NAME _STR _LOG_UNINSTALL
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_ROOT}"
  Push "${_KEY}"
  Push "${_NAME}"
  Push "${_STR}"
  Push "${_LOG_UNINSTALL}"
  Call un.WriteRegStrHKCR
  !verbose pop
!macroend

!macro un.WriteRegStrHKCR
  !ifndef un.WriteRegStrHKCR
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro WriteRegStrHKCR

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
!macroend


/**
 * Creates a registry key. NSIS doesn't supply a RegCreateKey method and instead
 * will auto create keys when a reg key name value pair is set.
 * i - int (includes char, byte, short, handles, pointers and so on)
 * t - text, string (LPCSTR, pointer to first character)
 * * - pointer specifier -> the proc needs the pointer to type, affects next
 *     char (parameter) [ex: '*i' - pointer to int]
 * see the NSIS documentation for additional information.
 */
!define RegCreateKey "Advapi32::RegCreateKeyA(i, t, *i) i"

/**
 * Creates a registry key. This will log the actions to the install and
 * uninstall logs. Alternatively you can set a registry value to create the key
 * and then delete the value.
 * @param   _ROOT
 *          The registry key root as defined by NSIS (e.g. HKLM, HKCU, etc.).
 * @param   _KEY
 *          The subkey in relation to the key root.
 * @param   _LOG_UNINSTALL
 *          0 = don't add to uninstall log, 1 = add to uninstall log.
 *
 * $R4 = [out] handle to newly created registry key. If this is not a key
 *       located in one of the predefined registry keys this must be closed
 *       with RegCloseKey (this should not be needed unless someone decides to
 *       do something extremely squirrelly with NSIS).
 * $R5 = return value from RegCreateKeyA (represented by r15 in the system call).
 * $R6 = [in] hKey passed to RegCreateKeyA.
 * $R7 = _ROOT
 * $R8 = _KEY
 * $R9 = _LOG_UNINSTALL
 */
!macro CreateRegKey

  !ifndef ${_MOZFUNC_UN}CreateRegKey
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}CreateRegKey "!insertmacro ${_MOZFUNC_UN}CreateRegKeyCall"

    Function ${_MOZFUNC_UN}CreateRegKey
      Exch $R9
      Exch 1
      Exch $R8
      Exch 2
      Exch $R7
      Push $R6
      Push $R5
      Push $R4

      StrCmp $R7 "HKCR" 0 +2
      StrCpy $R6 "0x80000000"
      StrCmp $R7 "HKCU" 0 +2
      StrCpy $R6 "0x80000001"
      StrCmp $R7 "HKLM" 0 +2
      StrCpy $R6 "0x80000002"

      ; see definition of RegCreateKey
      System::Call "${RegCreateKey}($R6, '$R8', .r14) .r15"

      ; if $R5 is not 0 then there was an error creating the registry key.
      IntCmp $R5 0 +3
      FileWrite $fhInstallLog "  ** ERROR Adding Registry Key: $R7 | $R8 **$\r$\n"
      GoTo +4
      IntCmp $R9 1 0 +2
      FileWrite $fhUninstallLog "RegKey: $R7 | $R8$\r$\n"
      FileWrite $fhInstallLog "  Added Registry Key: $R7 | $R8$\r$\n"

      Pop $R4
      Pop $R5
      Pop $R6
      Exch $R7
      Exch 2
      Exch $R8
      Exch 1
      Exch $R9
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro CreateRegKeyCall _ROOT _KEY _LOG_UNINSTALL
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_ROOT}"
  Push "${_KEY}"
  Push "${_LOG_UNINSTALL}"
  Call CreateRegKey
  !verbose pop
!macroend

!macro un.CreateRegKeyCall _ROOT _KEY _LOG_UNINSTALL
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_ROOT}"
  Push "${_KEY}"
  Push "${_LOG_UNINSTALL}"
  Call un.CreateRegKey
  !verbose pop
!macroend

!macro un.CreateRegKey
  !ifndef un.CreateRegKey
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro CreateRegKey

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
!macroend

/**
 * Finds a second installation of the application so we can make informed
 * decisions about registry operations. This uses SHCTX to determine the
 * registry hive so you must call SetShellVarContext first.
 *
 * IMPORTANT! $R9 will be overwritten by this macro with the return value so
 *            protect yourself!
 *
 * @param   _KEY
 *          The registry subkey (typically this will be Software\Mozilla).
 * @return  _RESULT
 *          false if a second install isn't found, path to the main exe if a
 *          second install is found.
 *
 * $R3 = _KEY
 * $R4 = value returned from the outer loop's EnumRegKey
 * $R5 = value returned from ReadRegStr
 * $R6 = counter for the outer loop's EnumRegKey
 * $R7 = value returned popped from the stack for GetPathFromRegStr macro
 * $R8 = value returned popped from the stack for GetParentDir macro
 * $R9 = _RESULT
 */
!macro GetSecondInstallPath

  !ifndef ${_MOZFUNC_UN}GetSecondInstallPath
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}GetSecondInstallPath "!insertmacro ${_MOZFUNC_UN}GetSecondInstallPathCall"

    Function ${_MOZFUNC_UN}GetSecondInstallPath
      Exch $R3
      Push $R4
      Push $R5
      Push $R6
      Push $R7
      Push $R8

      StrCpy $R9 "false"
      StrCpy $R6 0  ; set the counter for the loop to 0

      loop:
      EnumRegKey $R4 SHCTX $R3 $R6
      StrCmp $R4 "" end  ; if empty there are no more keys to enumerate
      IntOp $R6 $R6 + 1  ; increment the loop's counter
      ClearErrors
      ReadRegStr $R5 SHCTX "$R3\$R4\bin" "PathToExe"
      IfErrors loop
      Push $R5
      ${GetPathFromRegStr}
      Pop $R7
      Push $R5
      ${GetParentDir}
      Pop $R8

      IfFileExists "$R7" 0 +5
      StrCmp $R8 $INSTDIR +4 0
      StrCmp "$R8\${FileMainEXE}" "$R7" 0 +3
      StrCpy $R9 "$R7"
      GoTo end

      GoTo loop

      end:
      ClearErrors

      Pop $R8
      Pop $R7
      Pop $R6
      Pop $R5
      Pop $R4
      Exch $R3
      Push $R9
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro GetSecondInstallPathCall _KEY _RESULT
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_KEY}"
  Call GetSecondInstallPath
  Pop ${_RESULT}
  !verbose pop
!macroend

!macro un.GetSecondInstallPathCall _KEY _RESULT
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_KEY}"
  Call un.GetSecondInstallPath
  Pop ${_RESULT}
  !verbose pop
!macroend

!macro un.GetSecondInstallPath
  !ifndef un.GetSecondInstallPath
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro GetSecondInstallPath

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
!macroend

