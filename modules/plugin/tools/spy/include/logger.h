/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef _LOGGER_H__
#define __LOGGER_H__

#include "npupp.h"
#include "format.h"
#include "logfile.h"

#define TOTAL_NUMBER_OF_API_CALLS 37
#define DEFAULT_LOG_FILE_NAME "spylog.txt"

class Logger
{
public:
  BOOL bMutedAll;
  BOOL bToWindow;
  BOOL bToConsole;
  BOOL bToFile;
  BOOL bOnTop;
  BOOL bSPALID; //ShutdownPluginAfterLastInstanceDestroyed
                // as opposed to 'only when NS asks to'
  CLogFile filer;

  BOOL bSaveSettings;
  char szFile[_MAX_PATH];
  
  // 37 is the total number of API calls 
  // (NPN_* and NPP_* only, NPP_Initialize and NPP_Shutdown not included)
  BOOL bMutedCalls[TOTAL_NUMBER_OF_API_CALLS];

public:
  Logger();
  ~Logger();

  BOOL init();
  void shut();

  // platform dependent virtuals
  virtual BOOL platformInit() = 0;
  virtual void platformShut() = 0;
  virtual void dumpStringToMainWindow(char * string) = 0;

  void setOnTop(BOOL ontop);
  void setToFile(BOOL tofile, char * filename);

  BOOL * getMutedCalls();
  void setMutedCalls(BOOL * mutedcalls);

  BOOL isMuted(NPAPI_Action action);

  void logNS_NP_GetEntryPoints();
  void logNS_NP_Initialize();
  void logNS_NP_Shutdown();

  void logSPY_NP_GetEntryPoints(NPPluginFuncs * pNPPFuncs);
  void logSPY_NP_Initialize();
  void logSPY_NP_Shutdown(char * mimetype);

  void logCall(NPAPI_Action action, DWORD dw1 = 0L, DWORD dw2 = 0L, 
               DWORD dw3 = 0L, DWORD dw4 = 0L, DWORD dw5 = 0L, DWORD dw6 = 0L, DWORD dw7 = 0L);
  void logReturn(DWORD dwRet = 0L);
};

Logger * NewLogger();
void DeleteLogger(Logger * logger);

#endif
