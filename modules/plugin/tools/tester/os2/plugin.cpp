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

#define INCL_WIN
#include <os2.h>
#include <assert.h>

#include "resource.h"

#include "plugin.h"
#include "helpers.h"
#include "logger.h"
#include "guiprefs.h"
#include "os2utils.h"

extern HMODULE hInst;
extern CLogger * pLogger;

/***********************************************/
/*                                             */
/*       CPlugin class implementation          */
/*                                             */
/***********************************************/

CPlugin::CPlugin(NPP pNPInstance, USHORT wMode) :
  CPluginBase(pNPInstance, wMode),
  m_hInst(hInst),
  m_hWnd(NULL),
  m_hWndManual(NULL),
  m_hWndAuto(NULL),
  m_hWndParent(NULL),
  m_bPluginReady(FALSE),
  m_hWndLastEditFocus(NULL),
  m_iWidth(200),
  m_iHeight(500)
{
}

CPlugin::~CPlugin()
{
}

static char szSection[] = SECTION_PREFERENCES;
static char szYes[] = ENTRY_YES;
static char szNo[] = ENTRY_NO;

void CPlugin::restorePreferences()
{
  char szFileName[_MAX_PATH];
  GetINIFileName(m_hInst, szFileName, sizeof(szFileName));

  char sz[256];
  XP_GetPrivateProfileString(szSection, KEY_AUTO_MODE, ENTRY_NO, sz, sizeof(sz), szFileName);
  m_Pref_ShowGUI = (strcmpi(sz, ENTRY_YES) == 0) ? sg_auto : sg_manual;

  XP_GetPrivateProfileString(szSection, KEY_LOG_FILE, "Test.log", sz, sizeof(sz), szFileName);
  strcpy(m_Pref_szLogFile, sz);

  XP_GetPrivateProfileString(szSection, KEY_SCRIPT_FILE, "Test.pts", sz, sizeof(sz), szFileName);
  strcpy(m_Pref_szScriptFile, sz);

  XP_GetPrivateProfileString(szSection, KEY_TO_FILE, ENTRY_NO, sz, sizeof(sz), szFileName);
  m_Pref_bToFile = (strcmpi(sz, ENTRY_YES) == 0) ? TRUE : FALSE;

  XP_GetPrivateProfileString(szSection, KEY_TO_FRAME, ENTRY_YES, sz, sizeof(sz), szFileName);
  m_Pref_bToFrame = (strcmpi(sz, ENTRY_YES) == 0) ? TRUE : FALSE;

  XP_GetPrivateProfileString(szSection, KEY_FLUSH_NOW, ENTRY_YES, sz, sizeof(sz), szFileName);
  m_Pref_bFlushNow = (strcmpi(sz, ENTRY_YES) == 0) ? TRUE : FALSE;

  XP_GetPrivateProfileString(szSection, KEY_REMEMBER_LAST_API_CALL, ENTRY_YES, sz, sizeof(sz), szFileName);
  m_Pref_bRememberLastCall = (strcmpi(sz, ENTRY_YES) == 0) ? TRUE : FALSE;
}

void CPlugin::savePreferences()
{
  char szFileName[_MAX_PATH];
  GetINIFileName(m_hInst, szFileName, sizeof(szFileName));

  XP_WritePrivateProfileString(szSection, KEY_AUTO_MODE, (m_Pref_ShowGUI == sg_auto) ? szYes : szNo, szFileName);
  XP_WritePrivateProfileString(szSection, KEY_LOG_FILE, m_Pref_szLogFile, szFileName);
  XP_WritePrivateProfileString(szSection, KEY_SCRIPT_FILE, m_Pref_szScriptFile, szFileName);
  XP_WritePrivateProfileString(szSection, KEY_TO_FILE, m_Pref_bToFile ? szYes : szNo, szFileName);
  XP_WritePrivateProfileString(szSection, KEY_TO_FRAME, m_Pref_bToFrame ? szYes : szNo, szFileName);
  XP_WritePrivateProfileString(szSection, KEY_FLUSH_NOW, m_Pref_bFlushNow ? szYes : szNo, szFileName);
  XP_WritePrivateProfileString(szSection, KEY_REMEMBER_LAST_API_CALL, m_Pref_bRememberLastCall ? szYes : szNo, szFileName);
}

void CPlugin::updatePrefs(GUIPrefs prefs, int iValue, char * szValue)
{
  switch(prefs)
  {
    case gp_mode:
      m_Pref_ShowGUI = (ShowGUI)iValue;
      break;
    case gp_logfile:
      if(szValue && (strlen(szValue) < sizeof(m_Pref_szLogFile)))
        strcpy(m_Pref_szLogFile, szValue);
      break;
    case gp_scriptfile:
      if(szValue && (strlen(szValue) < sizeof(m_Pref_szScriptFile)))
        strcpy(m_Pref_szScriptFile, szValue);
      break;
    case gp_tofile:
      m_Pref_bToFile = (BOOL)iValue;
      break;
    case gp_toframe:
      m_Pref_bToFrame = (BOOL)iValue;
      break;
    case gp_flush:
      m_Pref_bFlushNow = (BOOL)iValue;
      break;
    case gp_rememberlast:
      m_Pref_bRememberLastCall = (BOOL)iValue;
      break;
    default:
      break;
  }
}

void CPlugin::getModulePath(PSZ szPath, int iSize)
{
  GetModulePath(m_hInst, szPath, iSize);
}

void CPlugin::getLogFileName(PSZ szLogFileName, int iSize)
{
  if(getMode() == NP_EMBED)
  {
    char sz[256];
    getModulePath(szLogFileName, iSize);
    WinQueryWindowText(WinWindowFromID(m_hWnd, IDC_EDIT_LOG_FILE_NAME), sizeof(sz), sz);
    strcat(szLogFileName, sz);
  }
  else
    CPluginBase::getLogFileName(szLogFileName, iSize);
}

MRESULT EXPENTRY NP_LOADDS TesterDlgProc(HWND, ULONG, MPARAM, MPARAM);

BOOL CPlugin::initEmbed(ULONG dwInitData)
{
  restorePreferences();

  HWND hWndParent = (HWND)dwInitData;

  if(WinIsWindow((HAB)0, hWndParent))
    m_hWndParent = hWndParent;

  WinLoadDlg(m_hWndParent, m_hWndParent, (PFNWP)TesterDlgProc, m_hInst, IDD_DIALOG_TESTER, (PVOID)this);

  m_bPluginReady = (m_hWnd != NULL);

  return m_bPluginReady;
}

BOOL CPlugin::initFull(ULONG dwInitData)
{
  m_bPluginReady = CPluginBase::initFull(dwInitData);
  return m_bPluginReady;
}

void CPlugin::shutEmbed()
{
  savePreferences();

  if(m_hWnd != NULL)
  {
    WinDismissDlg(m_hWnd, IDD_DIALOG_TESTER); //WinDestroyWindow(m_hWnd);
    m_hWnd = NULL;
  }

  m_bPluginReady = FALSE;
}

void CPlugin::shutFull()
{
  CPluginBase::shutFull();
  m_bPluginReady = FALSE;
}

BOOL CPlugin::isInitialized()
{
  return m_bPluginReady;
}

void CPlugin::onInit(HWND hWnd, HWND hWndManual, HWND hWndAuto)
{
  assert(hWnd != NULL);
  assert(hWndManual != NULL);
  assert(hWndAuto != NULL);

  m_hWnd = hWnd;
  m_hWndManual = hWndManual;
  m_hWndAuto = hWndAuto;

  pLogger->setShowImmediatelyFlag(WinQueryButtonCheckstate(m_hWnd, IDC_CHECK_SHOW_LOG) == BST_CHECKED);
  pLogger->setLogToFrameFlag(WinQueryButtonCheckstate(m_hWnd, IDC_CHECK_LOG_TO_FRAME) == BST_CHECKED);
  pLogger->setLogToFileFlag(WinQueryButtonCheckstate(m_hWnd, IDC_CHECK_LOG_TO_FILE) == BST_CHECKED);
}

int CPlugin::messageBox(LPSTR szMessage, LPSTR szTitle, UINT uStyle)
{
  return WinMessageBox(HWND_DESKTOP, m_hWnd, szMessage, szTitle, 0, uStyle);
}

void CPlugin::onDestroy()
{
  m_hWnd = NULL;
}

void CPlugin::onLogToFile(BOOL bLogToFile)
{
  pLogger->setLogToFileFlag(bLogToFile);
  if(!bLogToFile)
    pLogger->closeLogToFile();
}

const HMODULE CPlugin::getInstance()
{
  return m_hInst;
}

const HWND CPlugin::getWindow()
{
  return m_hWnd;
}

const HWND CPlugin::getParent()
{
  return m_hWndParent;
}

void CPlugin::showGUI(ShowGUI sg)
{
  switch (sg)
  {
    case sg_manual:
      WinShowWindow(m_hWndManual, TRUE);
      WinShowWindow(m_hWndAuto, FALSE);
      break;
    case sg_auto:
      WinShowWindow(m_hWndManual, FALSE);
      WinShowWindow(m_hWndAuto, TRUE);
      break;
    default:
      assert(0);
      break;
  }
}

ULONG CPlugin::makeNPNCall(NPAPI_Action action, ULONG dw1, ULONG dw2, ULONG dw3, 
                           ULONG dw4, ULONG dw5, ULONG dw6, ULONG dw7)
{
  ULONG dwRet = CPluginBase::makeNPNCall(action, dw1, dw2, dw3, dw4, dw5, dw6, dw7);

    // update GUI for Manual mode action
  if((getMode() == NP_EMBED) && (WinIsWindowVisible(m_hWndManual)))
  {
    switch (action)
    {
      case action_npn_new_stream:
      case action_npn_destroy_stream:
      case action_npn_mem_alloc:
      case action_npn_mem_free:
        updateUI(m_hWndManual);
        break;
      default:
        break;
    }
  }

  return dwRet;
}

// Creation and destruction
CPluginBase * CreatePlugin(NPP instance, uint16 mode) 
{
  CPlugin * pPlugin = new CPlugin(instance, mode);
  return (CPluginBase *)pPlugin;
}

void DestroyPlugin(CPluginBase * pPlugin)
{
  if(pPlugin != NULL)
    delete (CPluginBase *)pPlugin;
}
