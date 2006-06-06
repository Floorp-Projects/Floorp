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

; Notes
; Possible values for languages that are provided by NSIS are:
; Albanian, Arabic, Belarusian, Bosnian, Breton, Bulgarian, Catalan, Croatian,
; Czech, Danish, Dutch, English, Estonian, Farsi, Finnish, French, German,
; Greek, Hebrew, Hungarian, Icelandic, Indonesian, Italian, Japanese, Korean,
; Kurdish, Latvian, Lithuanian, Luxembourgish, Macedonian, Malay, Mongolian,
; Norwegian, Polish, Portuguese, PortugueseBR, Romanian, Russian, Serbian,
; SerbianLatin, SimpChinese, Slovak, Slovenian, Spanish, Swedish, Thai,
; TradChinese, Turkish, Ukrainian
!macro DEF_MUI_LANGUAGE
  !insertmacro MUI_LANGUAGE "English"
!macroend

LangString APP_DESC ${LANG_ENGLISH} "Required files for the ${BrandShortName} application"
LangString DEV_TOOLS_DESC ${LANG_ENGLISH} "A tool for inspecting the DOM of HTML, XUL, and XML pages, including the browser chrome."
LangString QFA_DESC ${LANG_ENGLISH} "A tool for submitting crash reports to Mozilla.org."
LangString SAFE_MODE ${LANG_ENGLISH} "Safe Mode"
LangString OPTIONS_PAGE_TITLE ${LANG_ENGLISH} "Setup Type"
LangString OPTIONS_PAGE_SUBTITLE ${LANG_ENGLISH} "Choose setup options"
LangString SHORTCUTS_PAGE_TITLE ${LANG_ENGLISH} "Set Up Shortcuts"
LangString SHORTCUTS_PAGE_SUBTITLE ${LANG_ENGLISH} "Create Program Icons"
# to enable the survey, see DO_UNINSTALL_SURVEY in appLocale.nsi
LangString SURVEY_TEXT ${LANG_ENGLISH} "&Tell us what you thought of ${BrandShortName}"
LangString LAUNCH_TEXT ${LANG_ENGLISH} '&Launch ${BrandFullName} now'
LangString WARN_APP_RUNNING_INSTALL ${LANG_ENGLISH} '${BrandFullName} must be closed to proceed with the installation.$\r$\n$\r$\nClick "OK" to exit ${BrandFullName} automatically and continue.'
LangString WARN_APP_RUNNING_UNINSTALL ${LANG_ENGLISH} '${BrandFullName} must be closed to proceed with the uninstall.$\r$\n$\r$\nClick "OK" to exit ${BrandFullName} automatically and continue.'
