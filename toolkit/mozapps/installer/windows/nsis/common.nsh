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

; Modified version of the following MUI macros to support Mozilla localization.
; MUI_LANGUAGE
; MUI_LANGUAGEFILE_BEGIN
; MOZ_MUI_LANGUAGEFILE_END
; See <NSIS App Dir>/Contrib/Modern UI/System.nsh for more information
!define MUI_INSTALLOPTIONS_READ "!insertmacro MUI_INSTALLOPTIONS_READ"

!macro MOZ_MUI_LANGUAGE LANGUAGE
  !verbose push
  !verbose ${MUI_VERBOSE}
  !include "${LANGUAGE}.nsh"
  !verbose pop
!macroend

!macro MOZ_MUI_LANGUAGEFILE_BEGIN LANGUAGE
  !ifndef MUI_INSERT
    !define MUI_INSERT
    !insertmacro MUI_INSERT
  !endif
  !ifndef "MUI_LANGUAGEFILE_${LANGUAGE}_USED"
    !define "MUI_LANGUAGEFILE_${LANGUAGE}_USED"
    LoadLanguageFile "${LANGUAGE}.nlf"
  !else
    !error "Modern UI language file ${LANGUAGE} included twice!"
  !endif
!macroend

; Custom version of MUI_LANGUAGEFILE_END. The macro to add the default MUI
; strings and the macros for several strings that are part of the NSIS MUI and
; not in our locale files have been commented out.
!macro MOZ_MUI_LANGUAGEFILE_END

#  !include "${NSISDIR}\Contrib\Modern UI\Language files\Default.nsh"
  !ifdef MUI_LANGUAGEFILE_DEFAULT_USED
    !undef MUI_LANGUAGEFILE_DEFAULT_USED
    !warning "${LANGUAGE} Modern UI language file version doesn't match. Using default English texts for missing strings."
  !endif

  !insertmacro MUI_LANGUAGEFILE_DEFINE "MUI_${LANGUAGE}_LANGNAME" "MUI_LANGNAME"

  !ifndef MUI_LANGDLL_PUSHLIST
    !define MUI_LANGDLL_PUSHLIST "'${MUI_${LANGUAGE}_LANGNAME}' ${LANG_${LANGUAGE}} "
  !else
    !ifdef MUI_LANGDLL_PUSHLIST_TEMP
      !undef MUI_LANGDLL_PUSHLIST_TEMP
    !endif
    !define MUI_LANGDLL_PUSHLIST_TEMP "${MUI_LANGDLL_PUSHLIST}"
    !undef MUI_LANGDLL_PUSHLIST
    !define MUI_LANGDLL_PUSHLIST "'${MUI_${LANGUAGE}_LANGNAME}' ${LANG_${LANGUAGE}} ${MUI_LANGDLL_PUSHLIST_TEMP}"
  !endif

  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE WELCOME "MUI_TEXT_WELCOME_INFO_TITLE"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE WELCOME "MUI_TEXT_WELCOME_INFO_TEXT"

  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE LICENSE "MUI_TEXT_LICENSE_TITLE"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE LICENSE "MUI_TEXT_LICENSE_SUBTITLE"
  !insertmacro MUI_LANGUAGEFILE_MULTILANGSTRING_PAGE LICENSE "MUI_INNERTEXT_LICENSE_TOP"

#  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE LICENSE "MUI_INNERTEXT_LICENSE_BOTTOM"
#  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE LICENSE "MUI_INNERTEXT_LICENSE_BOTTOM_CHECKBOX"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE LICENSE "MUI_INNERTEXT_LICENSE_BOTTOM_RADIOBUTTONS"

  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE COMPONENTS "MUI_TEXT_COMPONENTS_TITLE"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE COMPONENTS "MUI_TEXT_COMPONENTS_SUBTITLE"
  !insertmacro MUI_LANGUAGEFILE_MULTILANGSTRING_PAGE COMPONENTS "MUI_INNERTEXT_COMPONENTS_DESCRIPTION_TITLE"
  !insertmacro MUI_LANGUAGEFILE_MULTILANGSTRING_PAGE COMPONENTS "MUI_INNERTEXT_COMPONENTS_DESCRIPTION_INFO"

  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE DIRECTORY "MUI_TEXT_DIRECTORY_TITLE"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE DIRECTORY "MUI_TEXT_DIRECTORY_SUBTITLE"

  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE STARTMENU "MUI_TEXT_STARTMENU_TITLE"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE STARTMENU "MUI_TEXT_STARTMENU_SUBTITLE"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE STARTMENU "MUI_INNERTEXT_STARTMENU_TOP"
#  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE STARTMENU "MUI_INNERTEXT_STARTMENU_CHECKBOX"

  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE INSTFILES "MUI_TEXT_INSTALLING_TITLE"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE INSTFILES "MUI_TEXT_INSTALLING_SUBTITLE"

  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE INSTFILES "MUI_TEXT_FINISH_TITLE"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE INSTFILES "MUI_TEXT_FINISH_SUBTITLE"

  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE INSTFILES "MUI_TEXT_ABORT_TITLE"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE INSTFILES "MUI_TEXT_ABORT_SUBTITLE"

  !insertmacro MUI_LANGUAGEFILE_MULTILANGSTRING_PAGE FINISH "MUI_BUTTONTEXT_FINISH"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE FINISH "MUI_TEXT_FINISH_INFO_TITLE"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE FINISH "MUI_TEXT_FINISH_INFO_TEXT"
  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_PAGE FINISH "MUI_TEXT_FINISH_INFO_REBOOT"
  !insertmacro MUI_LANGUAGEFILE_MULTILANGSTRING_PAGE FINISH "MUI_TEXT_FINISH_REBOOTNOW"
  !insertmacro MUI_LANGUAGEFILE_MULTILANGSTRING_PAGE FINISH "MUI_TEXT_FINISH_REBOOTLATER"
#  !insertmacro MUI_LANGUAGEFILE_MULTILANGSTRING_PAGE FINISH "MUI_TEXT_FINISH_RUN"
#  !insertmacro MUI_LANGUAGEFILE_MULTILANGSTRING_PAGE FINISH "MUI_TEXT_FINISH_SHOWREADME"

  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_DEFINE MUI_ABORTWARNING "MUI_TEXT_ABORTWARNING"


  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE WELCOME "MUI_UNTEXT_WELCOME_INFO_TITLE"
  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE WELCOME "MUI_UNTEXT_WELCOME_INFO_TEXT"

  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE CONFIRM "MUI_UNTEXT_CONFIRM_TITLE"
  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE CONFIRM "MUI_UNTEXT_CONFIRM_SUBTITLE"

#  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE LICENSE "MUI_UNTEXT_LICENSE_TITLE"
#  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE LICENSE "MUI_UNTEXT_LICENSE_SUBTITLE"

#  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE LICENSE "MUI_UNINNERTEXT_LICENSE_BOTTOM"
#  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE LICENSE "MUI_UNINNERTEXT_LICENSE_BOTTOM_CHECKBOX"
#  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE LICENSE "MUI_UNINNERTEXT_LICENSE_BOTTOM_RADIOBUTTONS"

#  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE COMPONENTS "MUI_UNTEXT_COMPONENTS_TITLE"
#  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE COMPONENTS "MUI_UNTEXT_COMPONENTS_SUBTITLE"

#  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE DIRECTORY "MUI_UNTEXT_DIRECTORY_TITLE"
#  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE DIRECTORY  "MUI_UNTEXT_DIRECTORY_SUBTITLE"

  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE INSTFILES "MUI_UNTEXT_UNINSTALLING_TITLE"
  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE INSTFILES "MUI_UNTEXT_UNINSTALLING_SUBTITLE"

  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE INSTFILES "MUI_UNTEXT_FINISH_TITLE"
  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE INSTFILES "MUI_UNTEXT_FINISH_SUBTITLE"

  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE INSTFILES "MUI_UNTEXT_ABORT_TITLE"
  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE INSTFILES "MUI_UNTEXT_ABORT_SUBTITLE"

  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE FINISH "MUI_UNTEXT_FINISH_INFO_TITLE"
  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE FINISH "MUI_UNTEXT_FINISH_INFO_TEXT"
  !insertmacro MUI_LANGUAGEFILE_UNLANGSTRING_PAGE FINISH "MUI_UNTEXT_FINISH_INFO_REBOOT"

  !insertmacro MUI_LANGUAGEFILE_LANGSTRING_DEFINE MUI_UNABORTWARNING "MUI_UNTEXT_ABORTWARNING"

!macroend

!macro createComponentsINI
  WriteINIStr "$PLUGINSDIR\components.ini" "Settings" NumFields "5"

  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Type   "label"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Text   "$(OPTIONAL_COMPONENTS_LABEL)"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Top    "5"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Bottom "15"

  StrCpy $R1 2
  ${If} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Type   "checkbox"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Text   "$(DOMI_TITLE)"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Left   "15"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Right  "-1"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Top    "20"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Bottom "30"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" State  "1"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Flags  "GROUP"
    IntOp $R1 $R1 + 1
  ${EndIf}

  ${If} ${FileExists} "$EXEDIR\optional\extensions\talkback@mozilla.org"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Type   "checkbox"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Text   "$(QFA_TITLE)"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Left   "15"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Right  "-1"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Top    "55"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Bottom "65"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" State  "1"
    IntOp $R1 $R1 + 1
  ${EndIf}

  ${If} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Type   "label"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Text   "$(DOMI_TEXT)"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Left   "30"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Right  "-1"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Top    "32"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Bottom "52"
    IntOp $R1 $R1 + 1
  ${EndIf}

  ${If} ${FileExists} "$EXEDIR\optional\extensions\talkback@mozilla.org"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Type   "label"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Text   "$(QFA_TEXT)"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Left   "30"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Right  "-1"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Top    "67"
    WriteINIStr "$PLUGINSDIR\components.ini" "Field $R1" Bottom "87"
  ${EndIf}
!macroend

!macro createShortcutsINI
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Settings" NumFields "4"

  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Type   "label"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Text   "$(CREATE_ICONS_DESC)"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Top    "5"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Bottom "15"

  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Type   "checkbox"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Text   "$(ICONS_DESKTOP)"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Left   "15"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Right  "-1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Top    "20"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Bottom "30"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" State  "1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Flags  "GROUP"

  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Type   "checkbox"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Text   "$(ICONS_STARTMENU)"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Left   "15"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Right  "-1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Top    "40"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Bottom "50"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" State  "1"

  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Type   "checkbox"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Text   "$(ICONS_QUICKLAUNCH)"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Left   "15"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Right  "-1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Top    "60"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Bottom "70"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" State  "1"
!macroend

!macro createBasicCustomOptionsINI
  WriteINIStr "$PLUGINSDIR\options.ini" "Settings" NumFields "5"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Text   "$(OPTIONS_SUMMARY)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Top    "0"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Bottom "10"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Type   "RadioButton"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Text   "$(OPTION_STANDARD_RADIO)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Left   "15"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Top    "25"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Bottom "35"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" State  "1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Flags  "GROUP"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Type   "RadioButton"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Text   "$(OPTION_CUSTOM_RADIO)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Left   "15"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Top    "55"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Bottom "65"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" State  "0"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Text   "$(OPTION_STANDARD_DESC)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Left   "30"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Top    "37"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Bottom "57"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Text   "$(OPTION_CUSTOM_DESC)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Left   "30"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Top    "67"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Bottom "87"
!macroend

!macro createBasicCompleteCustomOptionsINI
  WriteINIStr "$PLUGINSDIR\options.ini" "Settings" NumFields "7"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Text   "$(OPTIONS_DESC)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Top    "0"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Bottom "10"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Type   "RadioButton"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Text   "$(OPTION_STANDARD_RADIO)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Left   "15"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Top    "25"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Bottom "35"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" State  "1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Flags  "GROUP"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Type   "RadioButton"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Text   "$(OPTION_COMPLETE_RADIO)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Left   "15"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Top    "55"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Bottom "65"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" State  "0"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Type   "RadioButton"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Text   "$(OPTION_CUSTOM_RADIO)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Left   "15"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Top    "85"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Bottom "95"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" State  "0"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Text   "$(OPTION_STANDARD_DESC)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Left   "30"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Top    "37"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Bottom "57"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Text   "$(OPTION_COMPLETE_DESC)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Left   "30"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Top    "67"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Bottom "87"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 7" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 7" Text   "$(OPTION_CUSTOM_DESC)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 7" Left   "30"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 7" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 7" Top    "97"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 7" Bottom "117"
!macroend

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
  IfErrors +2 0
  StrCpy $R0 $R8

  ClearErrors
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
 * Checks whether we can write to the install directory. If the install
 * directory already exists this will attempt to create a temporary file in the
 * install directory and then delete it. If it does not exist this will attempt
 * to create the directory and then delete it. If we can write to the install
 * directory this will return true... if not, this will return false.
 *
 * IMPORTANT! $R9 will be overwritten by this macro with the return value so
 *            protect yourself!
 *
 * @return  _RESULT
 *          true if the install directory can be written to otherwise false.
 *
 * $R7 = temp filename in installation directory returned from GetTempFileName
 * $R8 = filehandle to temp file used for writing
 * $R9 = _RESULT
 */
!macro CanWriteToInstallDir

  !ifndef ${_MOZFUNC_UN}CanWriteToInstallDir
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}CanWriteToInstallDir "!insertmacro ${_MOZFUNC_UN}CanWriteToInstallDirCall"

    Function ${_MOZFUNC_UN}CanWriteToInstallDir
      Push $R8
      Push $R7

      StrCpy $R9 "true"
      IfFileExists "$INSTDIR" 0 checkCreateDir
      GetTempFileName $R7 "$INSTDIR"
      FileOpen $R8 $R7 w
      FileWrite $R8 "Write Access Test"
      FileClose $R8
      IfFileExists "$R7" +3 0
      StrCpy $R9 "false"
      GoTo end

      Delete $R7
      GoTo end

      checkCreateDir:
      CreateDirectory "$INSTDIR"
      IfFileExists "$INSTDIR" +3 0
      StrCpy $R9 "false"
      GoTo end

      RmDir "$INSTDIR"

      end:
      ClearErrors

      Pop $R7
      Pop $R8
      Push $R9
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro CanWriteToInstallDirCall _RESULT
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Call CanWriteToInstallDir
  Pop ${_RESULT}
  !verbose pop
!macroend

!macro un.CanWriteToInstallDirCall _RESULT
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Call un.CanWriteToInstallDir
  Pop ${_RESULT}
  !verbose pop
!macroend

!macro un.CanWriteToInstallDir
  !ifndef un.CanWriteToInstallDir
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro CanWriteToInstallDir

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
!macroend

/**
 * Checks whether there is sufficient free space available on the installation
 * directory's drive. If there is sufficient free space this will return true...
 * if not, this will return false. This will only calculate the size of the
 * first three sections.
 *
 * IMPORTANT! $R9 will be overwritten by this macro with the return value so
 *            protect yourself!
 *
 * @return  _RESULT
 *          true if there is sufficient free space otherwise false.
 *
 * $R2 = return value from greater than comparison (0=false 1=true)
 * $R3 = free space for the install directory's drive
 * $R4 = install directory root
 * $R5 = value returned from SectionGetSize
 * $R6 = value returned from 'and' comparison of SectionGetFlags (1=selected)
 * $R7 = value returned from SectionGetFlags
 * $R8 = size in KB required for this installation
 * $R9 = _RESULT
 */
!macro CheckDiskSpace

  !ifndef ${_MOZFUNC_UN}CheckDiskSpace
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}CheckDiskSpace "!insertmacro ${_MOZFUNC_UN}CheckDiskSpaceCall"

    Function ${_MOZFUNC_UN}CheckDiskSpace
      Push $R8
      Push $R7
      Push $R6
      Push $R5
      Push $R4
      Push $R3
      Push $R2

      StrCpy $R9 "true"
      SectionGetSize 0 $R8

      SectionGetFlags 1 $R7
      IntOp $R6 ${SF_SELECTED} & $R7
      IntCmp $R6 0 +3 0
      SectionGetSize 1 $R5
      IntOp $R8 $R8 + $R5

      SectionGetFlags 2 $R7
      IntOp $R6 ${SF_SELECTED} & $R7
      IntCmp $R6 0 +3 0
      SectionGetSize 2 $R5
      IntOp $R8 $R8 + $R5

      ${GetRoot} "$INSTDIR" $R4
      ${DriveSpace} "$R4" "/D=F /S=K" $R3

      System::Int64Op $R3 > $R8
      Pop $R2

      IntCmp $R2 1 end
      StrCpy $R9 "false"

      end:
      ClearErrors

      Pop $R2
      Pop $R3
      Pop $R4
      Pop $R5
      Pop $R6
      Pop $R7
      Pop $R8
      Push $R9
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro CheckDiskSpaceCall _RESULT
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Call CheckDiskSpace
  Pop ${_RESULT}
  !verbose pop
!macroend

!macro un.CheckDiskSpaceCall _RESULT
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Call un.CheckDiskSpace
  Pop ${_RESULT}
  !verbose pop
!macroend

!macro un.CheckDiskSpace
  !ifndef un.CheckDiskSpace
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro CheckDiskSpace

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
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
 * message window's class. This macro uses the nsProcess plugin available
 * from http://nsis.sourceforge.net/NsProcess_plugin
 *
 * @param   _MSG
 *          The message text to display in the message box.
 * @param   _PROMPT
 *          If false don't prompt the user and automatically exit the
 *          application if it is running.
 *
 * $R6 = return value for nsProcess::_FindProcess and nsProcess::_KillProcess
 * $R7 = value returned from FindWindow
 * $R8 = _PROMPT
 * $R9 = _MSG
 */
!macro CloseApp

  !ifndef ${_MOZFUNC_UN}CloseApp
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}CloseApp "!insertmacro ${_MOZFUNC_UN}CloseAppCall"

    Function ${_MOZFUNC_UN}CloseApp
      Exch $R9
      Exch 1
      Exch $R8
      Push $R7
      Push $R6

      loop:
      Push $R6
      nsProcess::_FindProcess /NOUNLOAD "${FileMainEXE}"
      Pop $R6
      StrCmp $R6 0 0 end

      StrCmp $R8 "false" +2 0
      MessageBox MB_OKCANCEL|MB_ICONQUESTION "$R9" IDCANCEL exit 0

      FindWindow $R7 "${WindowClass}"
      IntCmp $R7 0 +4
      System::Call 'user32::PostMessage(i r17, i ${WM_QUIT}, i 0, i 0)'
      # The amount of time to wait for the app to shutdown before prompting again
      Sleep 5000

      Push $R6
      nsProcess::_FindProcess /NOUNLOAD "${FileMainEXE}"
      Pop $R6
      StrCmp $R6 0 0 end
      Push $R6
      nsProcess::_KillProcess /NOUNLOAD "${FileMainEXE}"
      Pop $R6
      Sleep 2000

      Goto loop

      exit:
      nsProcess::_Unload
      Quit

      end:
      nsProcess::_Unload

      Pop $R6
      Pop $R7
      Exch $R8
      Exch 1
      Exch $R9
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro CloseAppCall _MSG _PROMPT
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_MSG}"
  Push "${_PROMPT}"
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

!macro un.CloseAppCall _MSG _PROMPT
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_MSG}"
  Push "${_PROMPT}"
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

!ifndef NO_LOG
      IfErrors 0 +3
      FileWrite $fhInstallLog "  ** ERROR Adding Registry String: $R5 | $R6 | $R7 | $R8 **$\r$\n"
      GoTo +4
      IntCmp $R9 1 0 +2
      FileWrite $fhUninstallLog "RegVal: $R5 | $R6 | $R7$\r$\n"
      FileWrite $fhInstallLog "  Added Registry String: $R5 | $R6 | $R7 | $R8$\r$\n"
!endif

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
!ifndef NO_LOG
      IfErrors 0 +3
      FileWrite $fhInstallLog "  ** ERROR Adding Registry DWord: $R5 | $R6 | $R7 | $R8 **$\r$\n"
      GoTo +4
      IntCmp $R5 1 0 +2
      FileWrite $fhUninstallLog "RegVal: $R5 | $R6 | $R7$\r$\n"
      FileWrite $fhInstallLog "  Added Registry DWord: $R5 | $R6 | $R7 | $R8$\r$\n"
!endif

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

!ifndef NO_LOG
      IfErrors 0 +3
      FileWrite $fhInstallLog "  ** ERROR Adding Registry String: $R5 | $R6 | $R7 | $R8 **$\r$\n"
      GoTo +4
      IntCmp $R5 1 0 +2
      FileWrite $fhUninstallLog "RegVal: $R5 | $R6 | $R7$\r$\n"
      FileWrite $fhInstallLog "  Added Registry String: $R5 | $R6 | $R7 | $R8$\r$\n"
!endif

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

!ifndef NO_LOG
      ; if $R5 is not 0 then there was an error creating the registry key.
      IntCmp $R5 0 +3
      FileWrite $fhInstallLog "  ** ERROR Adding Registry Key: $R7 | $R8 **$\r$\n"
      GoTo +4
      IntCmp $R9 1 0 +2
      FileWrite $fhUninstallLog "RegKey: $R7 | $R8$\r$\n"
      FileWrite $fhInstallLog "  Added Registry Key: $R7 | $R8$\r$\n"
!endif

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

/**
 * Writes common registry values for a handler using SHCTX.
 * @param   _KEY
 *          The subkey in relation to the key root.
 * @param   _VALOPEN
 *          The path and args to launch the application.
 * @param   _VALICON
 *          The path to an exe that contains an icon and the icon resource id.
 * @param   _DISPNAME
 *          The display name for the handler. If emtpy no value will be set.
 * @param   _ISPROTOCOL
 *          Sets protocol handler specific registry values when "true".
 * @param   _ISDDE
 *          Sets DDE specific registry values when "true".
 *
 * $R3 = string value of the current registry key path.
 * $R4 = _KEY
 * $R5 = _VALOPEN
 * $R6 = _VALICON
 * $R7 = _DISPNAME
 * $R8 = _ISPROTOCOL
 * $R9 = _ISDDE
 */
!macro AddHandlerValues

  !ifndef ${_MOZFUNC_UN}AddHandlerValues
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}AddHandlerValues "!insertmacro ${_MOZFUNC_UN}AddHandlerValuesCall"

    Function ${_MOZFUNC_UN}AddHandlerValues
      Exch $R9
      Exch 1
      Exch $R8
      Exch 2
      Exch $R7
      Exch 3
      Exch $R6
      Exch 4
      Exch $R5
      Exch 5
      Exch $R4
      Push $R3

      StrCmp "$R7" "" +6 0
      ReadRegStr $R3 SHCTX "$R4" "FriendlyTypeName"

      StrCmp "$R3" "" 0 +3
      WriteRegStr SHCTX "$R4" "" "$R7"
      WriteRegStr SHCTX "$R4" "FriendlyTypeName" "$R7"

      StrCmp "$R8" "true" 0 +4
      DeleteRegValue SHCTX "$R4" "EditFlags"
      WriteRegBin SHCTX "$R4" "EditFlags" 2
      WriteRegStr SHCTX "$R4" "URL Protocol" ""

      StrCmp "$R9" "true" 0 +13
      WriteRegStr SHCTX "$R4\DefaultIcon" "" "$R6"
      WriteRegStr SHCTX "$R4\shell\open\command" "" "$R5"
      WriteRegStr SHCTX "$R4\shell\open\ddeexec" "" "$\"%1$\",,0,0,,,,"
      WriteRegStr SHCTX "$R4\shell\open\ddeexec" "NoActivateHandler" ""
      WriteRegStr SHCTX "$R4\shell\open\ddeexec\Application" "" "${DDEApplication}"
      WriteRegStr SHCTX "$R4\shell\open\ddeexec\Topic" "" "WWW_OpenURL"
      ; The ifexec key may have been added by another application so try to
      ; delete it to prevent it from breaking this app's shell integration.
      ; Also, IE 6 and below doesn't remove this key when it sets itself as the
      ; default handler and if this key exists IE's shell integration breaks.
      DeleteRegKey HKLM "$R4\shell\open\ddeexec\ifexec"
      DeleteRegKey HKCU "$R4\shell\open\ddeexec\ifexec"

      ClearErrors

      Pop $R3
      Exch $R4
      Exch 5
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

!macro AddHandlerValuesCall _KEY _VALOPEN _VALICON _DISPNAME _ISPROTOCOL _ISDDE
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_KEY}"
  Push "${_VALOPEN}"
  Push "${_VALICON}"
  Push "${_DISPNAME}"
  Push "${_ISPROTOCOL}"
  Push "${_ISDDE}"
  Call AddHandlerValues
  !verbose pop
!macroend

!macro un.AddHandlerValuesCall _KEY _VALOPEN _VALICON _DISPNAME _ISPROTOCOL _ISDDE
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_KEY}"
  Push "${_VALOPEN}"
  Push "${_VALICON}"
  Push "${_DISPNAME}"
  Push "${_ISPROTOCOL}"
  Push "${_ISDDE}"
  Call un.AddHandlerValues
  !verbose pop
!macroend

!macro un.AddHandlerValues
  !ifndef un.AddHandlerValues
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro AddHandlerValues

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
!macroend

/**
 * Displays a error message when a file can't be copied.
 *
 * $0 = file name inserted into the error message
 */
!macro DisplayCopyErrMsg

  !ifndef ${_MOZFUNC_UN}DisplayCopyErrMsg
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !define ${_MOZFUNC_UN}DisplayCopyErrMsg "!insertmacro ${_MOZFUNC_UN}DisplayCopyErrMsgCall"

    Function ${_MOZFUNC_UN}DisplayCopyErrMsg
      Exch $0

      MessageBox MB_RETRYCANCEL|MB_ICONQUESTION "$(^FileError_NoIgnore)" IDRETRY +2
      Quit

      Exch $0
    FunctionEnd

    !verbose pop
  !endif
!macroend

!macro DisplayCopyErrMsgCall _FILE
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_FILE}"
  Call DisplayCopyErrMsg
  !verbose pop
!macroend

!macro un.DisplayCopyErrMsgCall _FILE
  !verbose push
  !verbose ${_MOZFUNC_VERBOSE}
  Push "${_FILE}"
  Call un.DisplayCopyErrMsg
  !verbose pop
!macroend

!macro un.DisplayCopyErrMsg
  !ifndef un.DisplayCopyErrMsg
    !verbose push
    !verbose ${_MOZFUNC_VERBOSE}
    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN "un."

    !insertmacro DisplayCopyErrMsg

    !undef _MOZFUNC_UN
    !define _MOZFUNC_UN
    !verbose pop
  !endif
!macroend
