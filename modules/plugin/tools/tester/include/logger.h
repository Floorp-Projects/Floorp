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

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "plugbase.h"
#include "action.h"
#include "log.h"
#include "logFile.h"

class CLogger
{
private:
  CPluginBase * m_pPlugin;
  NPP m_pPluginInstance;
  CLogItemList * m_pLog;
  CLogFile * m_pLogFile;
  BOOL m_bShowImmediately;
  BOOL m_bLogToFile;
  BOOL m_bLogToFrame;
  BOOL m_bBlockLogToFile;
  BOOL m_bBlockLogToFrame;
  char m_szTarget[256];
  NPStream * m_pStream;
  char m_szStreamType[80];
  DWORD m_dwStartTime;
  char m_szItemSeparator[80];
  int m_iStringDataWrap;

public:
  CLogger(LPSTR szTarget = NULL);
  ~CLogger();

  void associate(CPluginBase * pPlugin);

  void setShowImmediatelyFlag(BOOL bFlagState);
  void setLogToFileFlag(BOOL bFlagState);
  void setLogToFrameFlag(BOOL bFlagState);

  BOOL getShowImmediatelyFlag();
  BOOL getLogToFileFlag();
  BOOL getLogToFrameFlag();
  int getStringDataWrap();

  void restorePreferences(LPSTR szFileName);

  BOOL onNPP_DestroyStream(NPStream * npStream);

  BOOL appendToLog(NPAPI_Action action, DWORD dwTickEnter, DWORD dwTickReturn, DWORD dwRet = 0L, 
                   DWORD dw1 = 0L, DWORD dw2 = 0L, DWORD dw3 = 0L, DWORD dw4 = 0L, 
                   DWORD dw5 = 0L, DWORD dw6 = 0L, DWORD dw7 = 0L);
  void dumpLogToTarget();
  void clearLog();
  void clearTarget();
  void resetStartTime();
  void blockDumpToFile(BOOL bBlock);
  void blockDumpToFrame(BOOL bBlock);

  void closeLogToFile();
};

#define LOGGER_DEFAULT_STRING_WRAP    10

// Preferences profile stuff
#define SECTION_LOG           "Log"
#define KEY_RECORD_SEPARATOR  "RecordSeparator"
#define KEY_STRING_WRAP       "StringDataWrap"
#define KEY_FILE_NAME         "FileName"

#endif // __LOGGER_H__
