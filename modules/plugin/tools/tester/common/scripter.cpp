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

#include "scripter.h"
#include "scrpthlp.h"
#include "profile.h"

/********************************/
/*       CScripter class        */
/********************************/

CScripter::CScripter() :
  m_pPlugin(NULL),
  m_pScript(NULL),
  m_bStopAutoExecFlag(FALSE),
  m_iCycleRepetitions(1),
  m_dwCycleDelay(0L)
{
  m_pScript = new CScriptItemList();
}

CScripter::~CScripter()
{
  if(m_pScript != NULL)
    delete m_pScript;
}
  
void CScripter::associate(CPluginBase * pPlugin)
{
  m_pPlugin = pPlugin;
}
  
BOOL CScripter::getStopAutoExecFlag()
{
  return m_bStopAutoExecFlag;
}

void CScripter::setStopAutoExecFlag(BOOL bFlag)
{
  m_bStopAutoExecFlag = bFlag;
}

int CScripter::getCycleRepetitions()
{
  return m_iCycleRepetitions;
}

DWORD CScripter::getCycleDelay()
{
  return m_dwCycleDelay;
}

void CScripter::clearScript()
{
  if(m_pScript != NULL)
    delete m_pScript;

  m_pScript = new CScriptItemList();
}

BOOL CScripter::executeScript()
{
  if(m_pScript == NULL)
    return FALSE;

  for(ScriptItemListElement * psile = m_pScript->m_pFirst; psile != NULL; psile = psile->pNext)
  {
    executeScriptItem(psile->psis);
  }

  return TRUE;
}

BOOL CScripter::createScriptFromFile(LPSTR szFileName)
{
  clearScript();

  if(!XP_IsFile(szFileName))
    return FALSE;

  char sz[16];

  XP_GetPrivateProfileString(SECTION_OPTIONS, KEY_LAST, PROFILE_DEFAULT_STRING, sz, sizeof(sz), szFileName);
  if(strcmp(sz, PROFILE_DEFAULT_STRING) == 0)
    return FALSE;

  int iLast = atoi(sz);

  int iFirst = 1;

  XP_GetPrivateProfileString(SECTION_OPTIONS, KEY_FIRST, PROFILE_DEFAULT_STRING, sz, sizeof(sz), szFileName);
  if(strcmp(sz, PROFILE_DEFAULT_STRING) != 0)
    iFirst = atoi(sz);

  XP_GetPrivateProfileString(SECTION_OPTIONS, KEY_DELAY, PROFILE_DEFAULT_STRING, sz, sizeof(sz), szFileName);
  if(strcmp(sz, PROFILE_DEFAULT_STRING) == 0)
    m_dwCycleDelay = 0L;
  else
    m_dwCycleDelay = atol(sz);

  XP_GetPrivateProfileString(SECTION_OPTIONS, KEY_REPETITIONS, PROFILE_DEFAULT_STRING, sz, sizeof(sz), szFileName);
  if(strcmp(sz, PROFILE_DEFAULT_STRING) == 0)
    m_iCycleRepetitions = 1;
  else
    m_iCycleRepetitions = atoi(sz);

  for(int i = iFirst; i <= iLast; i++)
  {
    char szSection[16];
    wsprintf(szSection, "%i", i);
    ScriptItemStruct * psis = readProfileSectionAndCreateScriptItemStruct(szFileName, szSection);
    if(psis == NULL)
      continue;
    m_pScript->add(psis);
  }
  return TRUE;
}

BOOL CScripter::executeScriptItem(ScriptItemStruct * psis)
{
  if(psis == NULL)
    return FALSE;
  if(m_pPlugin == NULL)
    return FALSE;

  DWORD dw1 = psis->arg1.dwArg;
  DWORD dw2 = psis->arg2.dwArg;
  DWORD dw3 = psis->arg3.dwArg;
  DWORD dw4 = psis->arg4.dwArg;
  DWORD dw5 = psis->arg5.dwArg;
  DWORD dw6 = psis->arg6.dwArg;
  DWORD dw7 = psis->arg7.dwArg;

  if(psis->action == action_npn_get_value)
  {
    static DWORD dwValue = 0L;
    m_pPlugin->m_pValue = (void *)&dwValue;
  }

  m_pPlugin->makeNPNCall(psis->action, dw1, dw2, dw3, dw4, dw5, dw6, dw7);

  if(psis->dwDelay >=0)
    XP_Sleep(psis->dwDelay);

  return TRUE;
}
