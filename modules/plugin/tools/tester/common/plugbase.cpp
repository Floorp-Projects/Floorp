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

#include "plugbase.h"
#include "logger.h"
#include "scripter.h"

extern CLogger * pLogger;
static char szINIFile[] = NPAPI_INI_FILE_NAME;
#ifdef XP_UNIX
    static char szTarget[] = "_npapi_Log";
#endif

CPluginBase::CPluginBase(NPP pNPInstance, WORD wMode) :
  m_pNPInstance(pNPInstance),
  m_wMode(wMode),
  m_pStream(NULL),
  m_pScriptStream(NULL),
  m_bWaitingForScriptStream(FALSE),
  m_pNPNAlloced(NULL),
  m_pValue(NULL)
{
  assert(m_pNPInstance != NULL);
  m_szScriptCacheFile[0] = '\0';

  if(m_wMode == NP_FULL)
  {
    // next should be NPP_NewStream with the script to perform
    // so we set a flag for a special stream treatment
    m_bWaitingForScriptStream = TRUE;
  }
}

CPluginBase::~CPluginBase()
{
  if(m_pStream != NULL)
  {
    NPN_DestroyStream(m_pNPInstance, m_pStream, NPRES_DONE);
    m_pStream = NULL;
  }

  if(m_pScriptStream != NULL)
  {
    NPN_DestroyStream(m_pNPInstance, m_pScriptStream, NPRES_DONE);
    m_pScriptStream = NULL;
  }

  if(m_pNPNAlloced != NULL)
  {
    NPN_MemFree(m_pNPNAlloced);
    m_pNPNAlloced = NULL;
  }
}

BOOL CPluginBase::initFull(DWORD dwInitData)
{
  pLogger->setLogToFrameFlag(FALSE);
  pLogger->setLogToFileFlag(FALSE);
  pLogger->clearLog();
  
  return TRUE;
}

void CPluginBase::shutFull()
{
}

BOOL CPluginBase::init(DWORD dwInitData)
{
  if(m_wMode == NP_EMBED)
    return initEmbed(dwInitData);
  else
    return initFull(dwInitData);
}

void CPluginBase::shut()
{
  if(m_wMode == NP_EMBED)
    shutEmbed();
  else
    shutFull();
}

void CPluginBase::getLogFileName(LPSTR szLogFileName, int iSize)
{
  char szFileName[_MAX_PATH];
  getModulePath(szFileName, sizeof(szFileName));
  strcat(szFileName, szINIFile);
  XP_GetPrivateProfileString(SECTION_LOG, KEY_FILE_NAME, "", szLogFileName, (DWORD)iSize, szFileName);
  if(strlen(szLogFileName) == 0)
  {
    strcpy(szLogFileName, m_szScriptCacheFile);

    // last three chars MUST be pts
    int iLength = strlen(szLogFileName);
    szLogFileName[iLength - 3] = 'l';
    szLogFileName[iLength - 2] = 'o';
    szLogFileName[iLength - 1] = 'g';
  }
}

const NPP CPluginBase::getNPInstance()
{
  return m_pNPInstance;
}

const WORD CPluginBase::getMode()
{
  return m_wMode;
}

const NPStream * CPluginBase::getNPStream()
{
  return m_pStream;
}

void CPluginBase::onNPP_NewStream(NPP pInstance, LPSTR szMIMEType, NPStream * pStream, NPBool bSeekable, uint16 * puType)
{
  if(!m_bWaitingForScriptStream)
    return;

  m_pScriptStream = pStream;
  *puType = NP_ASFILEONLY;

  m_bWaitingForScriptStream = FALSE;
}

void CPluginBase::onNPP_StreamAsFile(NPP pInstance, NPStream * pStream, const char * szFileName)
{
  if((m_pScriptStream == NULL) || (pStream != m_pScriptStream) || (szFileName == NULL))
    return;

  strcpy(m_szScriptCacheFile, szFileName);
#ifdef DEBUG
  fprintf(stderr,"CPluginBase::onNPP_StreamAsFile(), Script Cache File \"%s\"", m_szScriptCacheFile);
#endif
}

void CPluginBase::onNPP_DestroyStream(NPStream * pStream)
{
  // if this is a stream created by tester, reflect the fact it is destroyed
  if(pStream == m_pStream)
    m_pStream = NULL;

  // if this is a script stream, reflect the fact it is destroyed too, and execute the script
  if(pStream == m_pScriptStream)
  {
    m_pScriptStream = NULL;

    if(!XP_IsFile(m_szScriptCacheFile))
      return;

    CScripter * pScripter = new CScripter();
    pScripter->associate(this);

    if(pScripter->createScriptFromFile(m_szScriptCacheFile))
    {
      char szOutput[128];
      char szExecutingScript[] = "Executing script...";

      strcpy(szOutput, szExecutingScript);
      NPN_Status(m_pNPInstance, szOutput);

      int iRepetitions = pScripter->getCycleRepetitions();
      int iDelay = pScripter->getCycleDelay();
      if(iDelay < 0)
        iDelay = 0;

      assert(pLogger != NULL);
      pLogger->resetStartTime();

      pLogger->setLogToFileFlag(TRUE);
      pLogger->blockDumpToFile(FALSE);

      for(int i = 0; i < iRepetitions; i++)
      {
        wsprintf(szOutput, "%s %i", szExecutingScript, i);
        NPN_Status(m_pNPInstance, szOutput);

        pScripter->executeScript();
        if(iDelay != 0)
          XP_Sleep(iDelay);
      }
    }
    else
      NPN_Status(m_pNPInstance, "Cannot create script...");


    pLogger->setLogToFileFlag(FALSE);

    delete pScripter;

    m_szScriptCacheFile[0] = '\0';

    NPN_Status(m_pNPInstance, "Script execution complete");
  }
}

DWORD CPluginBase::makeNPNCall(NPAPI_Action action, DWORD dw1, DWORD dw2, DWORD dw3, 
                           DWORD dw4, DWORD dw5, DWORD dw6, DWORD dw7)
{
  DWORD dwRet = 0L;
  DWORD dwTickEnter = XP_GetTickCount();

  switch (action)
  {
    case action_invalid:
      assert(0);
      break;
    case action_npn_version:
    {
      static int iP_maj = 0;
      static int iP_min = 0;
      static int iN_maj = 0;
      static int iN_min = 0;
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)&iP_maj;
      if(dw2 == DEFAULT_DWARG_VALUE)
        dw2 = (DWORD)&iP_min;
      if(dw3 == DEFAULT_DWARG_VALUE)
        dw3 = (DWORD)&iN_maj;
      if(dw4 == DEFAULT_DWARG_VALUE)
        dw4 = (DWORD)&iN_min;
      NPN_Version((int *)dw1, (int *)dw2, (int *)dw3, (int *)dw4);
      break;
    }
    case action_npn_get_url_notify:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      dwRet = NPN_GetURLNotify((NPP)dw1, (char *)dw2, (char *)dw3, (void *)dw4);
      break;
    case action_npn_get_url:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      dwRet = NPN_GetURL((NPP)dw1, (char *)dw2, (char *)dw3);
      break;
    case action_npn_post_url_notify:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      dwRet = NPN_PostURLNotify((NPP)dw1, (char *)dw2, (char *)dw3, (int32)dw4, (char *)dw5, 
                                (BOOL)dw6, (void *)dw7);
      break;
    case action_npn_post_url:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      dwRet = NPN_PostURL((NPP)dw1, (char *)dw2, (char *)dw3, (int32)dw4, (char *)dw5, (BOOL)dw6);
      break;
    case action_npn_new_stream:
      assert(m_pStream == NULL);
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      if(dw4 == DEFAULT_DWARG_VALUE)
        dw4 = (DWORD)&m_pStream;
      dwRet = NPN_NewStream((NPP)dw1, (char *)dw2, (char *)dw3, (NPStream **)dw4);
      break;
    case action_npn_destroy_stream:
      assert(m_pStream != NULL);
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      if(dw2 == DEFAULT_DWARG_VALUE)
        dw2 = (DWORD)m_pStream;
      dwRet = NPN_DestroyStream((NPP)dw1, (NPStream *)dw2, (NPError)dw3);
      m_pStream = NULL;
      break;
    case action_npn_request_read:
      break;
    case action_npn_write:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      if(dw2 == DEFAULT_DWARG_VALUE)
        dw2 = (DWORD)m_pStream;
      dwRet = NPN_Write((NPP)dw1, (NPStream *)dw2, (int32)dw3, (void *)dw4);
      break;
    case action_npn_status:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      NPN_Status((NPP)dw1, (char *)dw2);
      break;
    case action_npn_user_agent:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      dwRet = (DWORD)NPN_UserAgent((NPP)dw1);
      break;
    case action_npn_mem_alloc:
      assert(m_pNPNAlloced == NULL);
      m_pNPNAlloced = NPN_MemAlloc((int32)dw1);
      dwRet = (DWORD)m_pNPNAlloced;
      if(m_pNPNAlloced != NULL)
      {
        for(int i = 0; i < (int)dw1; i++)
          *(((BYTE *)m_pNPNAlloced) + i) = 255;
      }
      break;
    case action_npn_mem_free:
      assert(m_pNPNAlloced != NULL);
      dw1 = (DWORD)m_pNPNAlloced;
      NPN_MemFree((void *)dw1);
      m_pNPNAlloced = NULL;
      break;
    case action_npn_mem_flush:
      dwRet = (DWORD)NPN_MemFlush((int32)dw1);
      break;
    case action_npn_reload_plugins:
      NPN_ReloadPlugins((NPBool)dw1);
      break;
    case action_npn_get_java_env:
      dwRet = (DWORD)NPN_GetJavaEnv();
      break;
    case action_npn_get_java_peer:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      dwRet = (DWORD)NPN_GetJavaPeer((NPP)dw1);
      break;
    case action_npn_get_value:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      if(dw3 == DEFAULT_DWARG_VALUE)
        dw3 = (DWORD)m_pValue;
      dwRet = (DWORD)NPN_GetValue((NPP)dw1, (NPNVariable)dw2, (void *)dw3);
      break;
    case action_npn_set_value:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      if(dw3 == DEFAULT_DWARG_VALUE)
        dw3 = (DWORD)m_pValue;
      dwRet = (DWORD)NPN_SetValue((NPP)dw1, (NPPVariable)dw2, (void *)dw3);
      break;
    case action_npn_invalidate_rect:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      NPN_InvalidateRect((NPP)dw1, (NPRect *)dw2);
      break;
    case action_npn_invalidate_region:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      NPN_InvalidateRegion((NPP)dw1, (NPRegion)dw2);
      break;
    case action_npn_force_redraw:
      if(dw1 == DEFAULT_DWARG_VALUE)
        dw1 = (DWORD)m_pNPInstance;
      NPN_ForceRedraw((NPP)dw1);
      break;
    default:
      assert(0);
      break;
  }

  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action, dwTickEnter, dwTickReturn, dwRet, dw1, dw2, dw3, dw4, dw5, dw6, dw7);

  return dwRet;
}
