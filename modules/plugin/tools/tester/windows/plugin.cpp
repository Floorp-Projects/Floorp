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

#include <windows.h>
#include <windowsx.h>
#include <assert.h>

#include "resource.h"

#include "plugin.h"
#include "helpers.h"
#include "logger.h"

extern HINSTANCE hInst;
extern CLogger * pLogger;

/***********************************************/
/*                                             */
/*       CPlugin class implementation          */
/*                                             */
/***********************************************/

CPlugin::CPlugin(NPP pNPInstance, WORD wMode) :
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

static char szINIFile[] = NPAPI_INI_FILE_NAME;
static char szSection[] = SECTION_PREFERENCES;
static char szYes[] = ENTRY_YES;
static char szNo[] = ENTRY_NO;

void CPlugin::restorePreferences()
{
  char szFileName[_MAX_PATH];
  getModulePath(szFileName, sizeof(szFileName));
  strcat(szFileName, szINIFile);

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
}

void CPlugin::savePreferences()
{
  char szFileName[_MAX_PATH];
  getModulePath(szFileName, sizeof(szFileName));
  strcat(szFileName, szINIFile);

  BOOL bParam = IsDlgButtonChecked(m_hWnd, IDC_RADIO_MODE_AUTO);
  XP_WritePrivateProfileString(szSection, KEY_AUTO_MODE, bParam ? szYes : szNo, szFileName);

  char szParam[256];
  Edit_GetText(GetDlgItem(m_hWnd, IDC_EDIT_LOG_FILE_NAME), szParam, sizeof(szParam));
  XP_WritePrivateProfileString(szSection, KEY_LOG_FILE, szParam, szFileName);

  Edit_GetText(GetDlgItem(m_hWndAuto, IDC_EDIT_SCRIPT_FILE_NAME), szParam, sizeof(szParam));
  XP_WritePrivateProfileString(szSection, KEY_SCRIPT_FILE, szParam, szFileName);

  bParam = IsDlgButtonChecked(m_hWnd, IDC_CHECK_LOG_TO_FILE);
  XP_WritePrivateProfileString(szSection, KEY_TO_FILE, bParam ? szYes : szNo, szFileName);

  bParam = IsDlgButtonChecked(m_hWnd, IDC_CHECK_LOG_TO_FRAME);
  XP_WritePrivateProfileString(szSection, KEY_TO_FRAME, bParam ? szYes : szNo, szFileName);

  bParam = IsDlgButtonChecked(m_hWnd, IDC_CHECK_SHOW_LOG);
  XP_WritePrivateProfileString(szSection, KEY_FLUSH_NOW, bParam ? szYes : szNo, szFileName);
}

void CPlugin::getModulePath(LPSTR szPath, int iSize)
{
  char sz[_MAX_PATH];
  GetModuleFileName(m_hInst, sz, sizeof(sz));
  char * p = strrchr(sz, '\\');
  if(p != NULL)
  {
    *++p = '\0';
    if((int)strlen(sz) < iSize) 
      strcpy(szPath, sz);
    else
      strcpy(szPath, "");
  }
  else
    strcpy(szPath, "");
}

void CPlugin::getLogFileName(LPSTR szLogFileName, int iSize)
{
  if(getMode() == NP_EMBED)
  {
    char sz[256];
    getModulePath(szLogFileName, iSize);
    Edit_GetText(GetDlgItem(m_hWnd, IDC_EDIT_LOG_FILE_NAME), sz, sizeof(sz));
    strcat(szLogFileName, sz);
  }
  else
    CPluginBase::getLogFileName(szLogFileName, iSize);
}

BOOL CALLBACK NP_LOADDS TesterDlgProc(HWND, UINT, WPARAM, LPARAM);

BOOL CPlugin::initEmbed(DWORD dwInitData)
{
  restorePreferences();

  HWND hWndParent = (HWND)dwInitData;

  if(IsWindow(hWndParent))
    m_hWndParent = hWndParent;

  CreateDialogParam(m_hInst, MAKEINTRESOURCE(IDD_DIALOG_TESTER), m_hWndParent, (DLGPROC)TesterDlgProc, (LPARAM)this);

  m_bPluginReady = (m_hWnd != NULL);

  return m_bPluginReady;
}

BOOL CPlugin::initFull(DWORD dwInitData)
{
  m_bPluginReady = CPluginBase::initFull(dwInitData);
  return m_bPluginReady;
}

void CPlugin::shutEmbed()
{
  savePreferences();

  if(m_hWnd != NULL)
  {
    DestroyWindow(m_hWnd);
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

  pLogger->setShowImmediatelyFlag(IsDlgButtonChecked(m_hWnd, IDC_CHECK_SHOW_LOG) == BST_CHECKED);
  pLogger->setLogToFrameFlag(IsDlgButtonChecked(m_hWnd, IDC_CHECK_LOG_TO_FRAME) == BST_CHECKED);
  pLogger->setLogToFileFlag(IsDlgButtonChecked(m_hWnd, IDC_CHECK_LOG_TO_FILE) == BST_CHECKED);
}

int CPlugin::messageBox(LPSTR szMessage, LPSTR szTitle, UINT uStyle)
{
  return MessageBox(m_hWnd, szMessage, szTitle, uStyle);
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

const HINSTANCE CPlugin::getInstance()
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
      ShowWindow(m_hWndManual, SW_SHOW);
      ShowWindow(m_hWndAuto, SW_HIDE);
      break;
    case sg_auto:
      ShowWindow(m_hWndManual, SW_HIDE);
      ShowWindow(m_hWndAuto, SW_SHOW);
      break;
    default:
      assert(0);
      break;
  }
}

DWORD CPlugin::makeNPNCall(NPAPI_Action action, DWORD dw1, DWORD dw2, DWORD dw3, 
                           DWORD dw4, DWORD dw5, DWORD dw6, DWORD dw7)
{
  DWORD dwRet = CPluginBase::makeNPNCall(action, dw1, dw2, dw3, dw4, dw5, dw6, dw7);

    // update GUI for Manual mode action
  if((getMode() == NP_EMBED) && (IsWindowVisible(m_hWndManual)))
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
