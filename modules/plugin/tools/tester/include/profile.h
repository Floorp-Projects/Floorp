/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __PROFILE_H__
#define __PROFILE_H__

#include "script.h"

ScriptItemStruct * readProfileSectionAndCreateScriptItemStruct(LPSTR szFileName, LPSTR szSection);

#define PROFILE_DEFAULT_STRING  "*"
#define PROFILE_NUMERIC_PREFIX  "_true_numeric_value_"

#define SECTION_OPTIONS "Options"
#define KEY_FIRST       "first"
#define KEY_LAST        "last"
#define KEY_REPETITIONS "repetitions"

#define KEY_ACTION      "APICall"
#define KEY_ARG1        "arg1"
#define KEY_ARG2        "arg2"
#define KEY_ARG3        "arg3"
#define KEY_ARG4        "arg4"
#define KEY_ARG5        "arg5"
#define KEY_ARG6        "arg6"
#define KEY_ARG7        "arg7"
#define KEY_DELAY       "delay"

#define KEY_WIDTH       "width"
#define KEY_HEIGHT      "height"
#define KEY_TOP         "top"
#define KEY_BOTTOM      "bottom"
#define KEY_LEFT        "left"
#define KEY_RIGHT       "right"

#define ENTRY_TRUE      "TRUE"
#define ENTRY_FALSE     "FALSE"

//NPReason
#define ENTRY_NPRES_DONE			             "NPRES_DONE"
#define ENTRY_NPRES_NETWORK_ERR            "NPRES_NETWORK_ERR"
#define ENTRY_NPRES_USER_BREAK             "NPRES_USER_BREAK"

// NPPVariable
#define ENTRY_NPPVPLUGINNAMESTRING         "NPPVpluginNameString"
#define ENTRY_NPPVPLUGINDESCRIPTIONSTRING  "NPPVpluginDescriptionString"
#define ENTRY_NPPVPLUGINWINDOWBOOL         "NPPVpluginWindowBool"
#define ENTRY_NPPVPLUGINTRANSPARENTBOOL    "NPPVpluginTransparentBool"
#define ENTRY_NPPVPLUGINWINDOWSIZE         "NPPVpluginWindowSize"

// NPNVariable
#define ENTRY_NPNVXDISPLAY                 "NPNVxDisplay"
#define ENTRY_NPNVXTAPPCONTEXT             "NPNVxtAppContext"
#define ENTRY_NPNVNETSCAPEWINDOW           "NPNVnetscapeWindow"
#define ENTRY_NPNVJAVASCRIPTENABLEDBOOL    "NPNVjavascriptEnabledBool"
#define ENTRY_NPNVASDENABLEDBOOL           "NPNVasdEnabledBool"
#define ENTRY_NPNVISOFFLINEBOOL            "NPNVisOfflineBool"

#endif // __PROFILE_H__

