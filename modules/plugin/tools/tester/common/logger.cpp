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

#include "xp.h"
#include "logger.h"
#include "loghlp.h"

#ifdef XP_MAC
	CLogger *pLogger = new CLogger();
#endif

/********************************/
/*       CLogger class          */
/********************************/

CLogger::CLogger(LPSTR szTarget) :
  m_pPlugin(NULL),
  m_pPluginInstance(NULL),
  m_pLog(NULL),
  m_pLogFile(NULL),
  m_bShowImmediately(FALSE),
  m_bLogToFile(FALSE),
  m_bLogToFrame(TRUE),
  m_bBlockLogToFile(TRUE),
  m_bBlockLogToFrame(FALSE),
  m_pStream(NULL),
  m_dwStartTime(0xFFFFFFFF),
  m_iStringDataWrap(LOGGER_DEFAULT_STRING_WRAP)
{
  if(szTarget != NULL)
    strcpy(m_szTarget, szTarget);
  else
    strcpy(m_szTarget, "");

  m_pLog = new CLogItemList();
  strcpy(m_szStreamType, "text/plain");
}

CLogger::~CLogger()
{
  if(m_pLogFile != NULL)
    delete m_pLogFile;

  if(m_pLog != NULL)
    delete m_pLog;
}

void CLogger::associate(CPluginBase * pPlugin)
{
  m_pPlugin = pPlugin;
  m_pPluginInstance = m_pPlugin->getNPInstance();
}

void CLogger::restorePreferences(LPSTR szFileName)
{
  XP_GetPrivateProfileString(SECTION_LOG, KEY_RECORD_SEPARATOR, "", m_szItemSeparator, 
                             sizeof(m_szItemSeparator), szFileName);
  m_iStringDataWrap = XP_GetPrivateProfileInt(SECTION_LOG, KEY_STRING_WRAP, 
                                              LOGGER_DEFAULT_STRING_WRAP, szFileName);
}

BOOL CLogger::onNPP_DestroyStream(NPStream * npStream)
{
  if(npStream == m_pStream) // Navigator itself destroys it
  {
    m_pStream = NULL;
    return TRUE;
  }
  return FALSE;
}

BOOL CLogger::appendToLog(NPAPI_Action action, DWORD dwTickEnter, DWORD dwTickReturn, 
                          DWORD dwRet, 
                          DWORD dw1, DWORD dw2, DWORD dw3, DWORD dw4, 
                          DWORD dw5, DWORD dw6, DWORD dw7)
{
  if(m_pLog == NULL)
    return FALSE;

  if(!m_bLogToFrame && !m_bLogToFile)
    return TRUE;

  DWORD dwTimeEnter;
  DWORD dwTimeReturn;
  if(m_dwStartTime == 0xFFFFFFFF)
  {
    m_dwStartTime = dwTickEnter;
    dwTimeEnter = 0L;
  }
  else
    dwTimeEnter = dwTickEnter - m_dwStartTime;

  dwTimeReturn = dwTickReturn - m_dwStartTime;

  LogItemStruct * plis = makeLogItemStruct(action, dwTimeEnter, dwTimeReturn, dwRet, 
                                           dw1, dw2, dw3, dw4, dw5, dw6, dw7);

  static char szOutput[1024];

  // dump to file
  if(m_bLogToFile && !m_bBlockLogToFile)
  {
    if(m_pLogFile == NULL)
    {
      m_pLogFile = new CLogFile();
      if(m_pLogFile == NULL)
        return FALSE;

      char szFile[256];
      m_pPlugin->getLogFileName(szFile, sizeof(szFile));

      if(m_pPlugin->getMode() == NP_EMBED)
      {
        if(!m_pLogFile->create(szFile, FALSE))
        {
          char szMessage[512];
          wsprintf(szMessage, "File '%s'\n probably exists. Overwrite?", szFile);
          if(IDYES == m_pPlugin->messageBox(szMessage, "", MB_ICONQUESTION | MB_YESNO))
          {
            if(!m_pLogFile->create(szFile, TRUE))
            {
              m_pPlugin->messageBox("Cannot create file.", "", MB_ICONERROR | MB_OK);
              delete m_pLogFile;
              m_pLogFile = NULL;
              return FALSE;
            }
          }
          else
          {
            delete m_pLogFile;
            m_pLogFile = NULL;
            goto Frame;
          }
        }
      }
      else // NP_FULL
      {
        if(!m_pLogFile->create(szFile, TRUE))
        {
          delete m_pLogFile;
          m_pLogFile = NULL;
          return FALSE;
        }
      }
    }

    int iLength = formatLogItem(plis, szOutput, m_szItemSeparator, TRUE);
    m_pLogFile->write(szOutput);
    m_pLogFile->flush();
  }

Frame:

  // dump to frame
  if(m_bLogToFrame && !m_bBlockLogToFrame)
  {
    if(m_bShowImmediately)
    {
      if(m_pStream == NULL)
        NPN_NewStream(m_pPluginInstance, m_szStreamType, m_szTarget, &m_pStream);

      int iLength = formatLogItem(plis, szOutput, "");

      NPN_Write(m_pPluginInstance, m_pStream, iLength, (void *)szOutput);
      delete plis;
    }
    else
      m_pLog->add(plis);
  }

  return TRUE;
}

void CLogger::setShowImmediatelyFlag(BOOL bFlagState)
{
  m_bShowImmediately = bFlagState;
}

BOOL CLogger::getShowImmediatelyFlag()
{
  return m_bShowImmediately;
}

void CLogger::setLogToFileFlag(BOOL bFlagState)
{
  m_bLogToFile = bFlagState;
}

BOOL CLogger::getLogToFileFlag()
{
  return m_bLogToFile;
}

void CLogger::setLogToFrameFlag(BOOL bFlagState)
{
  m_bLogToFrame = bFlagState;
}

BOOL CLogger::getLogToFrameFlag()
{
  return m_bLogToFrame;
}

int CLogger::getStringDataWrap()
{
  return m_iStringDataWrap;
}

void CLogger::clearLog()
{
  if(m_pLog != NULL)
    delete m_pLog;

  m_pLog = new CLogItemList();
}

void CLogger::clearTarget()
{
  if(m_pStream != NULL)
    NPN_DestroyStream(m_pPluginInstance, m_pStream, NPRES_DONE);
  NPN_NewStream(m_pPluginInstance, m_szStreamType, m_szTarget, &m_pStream);
  NPN_Write(m_pPluginInstance, m_pStream, 1, "\n");

  if(!m_bShowImmediately)
  {
    NPN_DestroyStream(m_pPluginInstance, m_pStream, NPRES_DONE);
    m_pStream = NULL;
  }
}

void CLogger::resetStartTime()
{
  m_dwStartTime = XP_GetTickCount();
}

void CLogger::dumpLogToTarget()
{
  if(m_pLog == NULL)
    return;

  BOOL bTemporaryStream = ((m_pStream == NULL) && !getShowImmediatelyFlag());

  if(m_pStream == NULL)
    NPN_NewStream(m_pPluginInstance, m_szStreamType, m_szTarget, &m_pStream);

  for(LogItemListElement * plile = m_pLog->m_pFirst; plile != NULL; plile = plile->pNext)
  {
    static char szOutput[1024];

    int iLength = formatLogItem(plile->plis, szOutput, "");

    NPN_Write(m_pPluginInstance, m_pStream, iLength, (void *)szOutput);
  }

  if(bTemporaryStream)
  {
    NPN_DestroyStream(m_pPluginInstance, m_pStream, NPRES_DONE);
    m_pStream = NULL;
  }
}

void CLogger::closeLogToFile()
{
  if(m_pLogFile != NULL)
  {
    delete m_pLogFile;
    m_pLogFile = NULL;
  }
}

void CLogger::blockDumpToFile(BOOL bBlock)
{
  m_bBlockLogToFile = bBlock;
}

void CLogger::blockDumpToFrame(BOOL bBlock)
{
  m_bBlockLogToFrame = bBlock;
}
