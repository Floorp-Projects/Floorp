/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 */

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
