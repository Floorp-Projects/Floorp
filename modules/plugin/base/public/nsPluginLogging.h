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

/* Plugin Module Logging usage instructions and includes */
////////////////////////////////////////////////////////////////////////////////
#ifndef nsPluginLogging_h__
#define nsPluginLogging_h__

#ifdef PR_LOGGING
#include "prlog.h"

#define PLUGIN_LOGGING 1  // master compile-time switch for pluging logging

////////////////////////////////////////////////////////////////////////////////
// Basic Plugin Logging Usage Instructions
//
// 1. Set this environment variable: NSPR_LOG_MODULES=<name>:<level>

// Choose the <name> and <level> from this list (no quotes):

// Log Names            <name>
#define NPN_LOG_NAME    "PluginNPN"
#define NPP_LOG_NAME    "PluginNPP"
#define PLUGIN_LOG_NAME "Plugin"

// Levels                <level>
#define PLUGIN_LOG_ALWAYS 1
#define PLUGIN_LOG_BASIC  3
#define PLUGIN_LOG_NORMAL 5
#define PLUGIN_LOG_NOISY  7
#define PLUGIN_LOG_MAX    9

// 2. You can combine logs and levels by seperating them with a comma:
//    My favorite Win32 Example: SET NSPR_LOG_MODULES=Plugin:5,PluginNPP:5,PluginNPN:5

// 3. Instead of output going to the console, you can log to a file. Additionally, set the
//    NSPR_LOG_FILE environment variable to point to the full path of a file.
//    My favorite Win32 Example: SET NSPR_LOG_FILE=c:\temp\pluginLog.txt

// 4. For complete information see the NSPR Reference: 
//    http://www.mozilla.org/projects/nspr/reference/html/prlog.html


#ifdef PLUGIN_LOGGING

class nsPluginLogging
{
public:
  static PRLogModuleInfo* gNPNLog;  // 4.x NP API, calls into navigator
  static PRLogModuleInfo* gNPPLog;  // 4.x NP API, calls into plugin
  static PRLogModuleInfo* gPluginLog;  // general plugin log
};

#endif   // PLUGIN_LOGGING

#endif  // PR_LOGGING

// Quick-use macros
#ifdef PLUGIN_LOGGING
 #define NPN_PLUGIN_LOG(a, b)                              \
   PR_BEGIN_MACRO                                        \
   PR_LOG(nsPluginLogging::gNPNLog, a, b); \
   PR_LogFlush();                                                    \
   PR_END_MACRO
#else
 #define NPN_PLUGIN_LOG(a, b)
#endif

#ifdef PLUGIN_LOGGING
 #define NPP_PLUGIN_LOG(a, b)                              \
   PR_BEGIN_MACRO                                         \
   PR_LOG(nsPluginLogging::gNPPLog, a, b); \
   PR_LogFlush();                                                    \
   PR_END_MACRO
#else
 #define NPP_PLUGIN_LOG(a, b)
#endif

#ifdef PLUGIN_LOGGING
 #define PLUGIN_LOG(a, b)                              \
   PR_BEGIN_MACRO                                         \
   PR_LOG(nsPluginLogging::gPluginLog, a, b); \
   PR_LogFlush();                                                    \
   PR_END_MACRO
#else
 #define PLUGIN_LOG(a, b)
#endif

#endif   // nsPluginLogging_h__

