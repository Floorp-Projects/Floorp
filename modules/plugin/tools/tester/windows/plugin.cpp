/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <windows.h>
#include <windowsx.h>
#include <assert.h>

#include "resource.h"

#include "plugin.h"
#include "helpers.h"
#include "guihlp.h"
#include "logger.h"
#include "scripter.h"
#include "guiprefs.h"
#include "winutils.h"

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
  m_hWndStandAloneLogWindow(NULL),
  m_bPluginReady(FALSE),
  m_hWndLastEditFocus(NULL),
  m_iWidth(200),
  m_iHeight(500)
{
  restorePreferences();
  pLogger->setLogToFileFlag(m_Pref_bToFile);
  pLogger->blockDumpToFile(FALSE);
}

CPlugin::~CPlugin()
{
  savePreferences();
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

  XP_GetPrivateProfileString(szSection, KEY_STAND_ALONE, ENTRY_NO, sz, sizeof(sz), szFileName);
  m_Pref_bStandAlone = (strcmpi(sz, ENTRY_YES) == 0) ? TRUE : FALSE;

  XP_GetPrivateProfileString(szSection, KEY_AUTOSTART_SCRIPT, ENTRY_NO, sz, sizeof(sz), szFileName);
  m_Pref_bAutoStartScript = (strcmpi(sz, ENTRY_YES) == 0) ? TRUE : FALSE;
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
  XP_WritePrivateProfileString(szSection, KEY_STAND_ALONE, m_Pref_bStandAlone ? szYes : szNo, szFileName);
  XP_WritePrivateProfileString(szSection, KEY_AUTOSTART_SCRIPT, m_Pref_bAutoStartScript ? szYes : szNo, szFileName);
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
    case gp_standalone:
      m_Pref_bStandAlone = (BOOL)iValue;
      break;
    case gp_autostartscript:
      m_Pref_bAutoStartScript = (BOOL)iValue;
      break;
    default:
      break;
  }
}

void CPlugin::getModulePath(LPSTR szPath, int iSize)
{
  GetModulePath(m_hInst, szPath, iSize);
}

void CPlugin::getLogFileName(LPSTR szLogFileName, int iSize)
{
  if(getMode() == NP_EMBED)
  {
    char sz[256];
    getModulePath(szLogFileName, iSize);
    Edit_GetText(GetDlgItem(m_hWnd, IDC_EDIT_LOG_FILE_NAME), sz, sizeof(sz));
    if(!strlen(sz))
      strcpy(sz, m_Pref_szLogFile);
    strcat(szLogFileName, sz);
  }
  else
    CPluginBase::getLogFileName(szLogFileName, iSize);
}

BOOL CALLBACK NP_LOADDS TesterDlgProc(HWND, UINT, WPARAM, LPARAM);

static char szStandAlonePluginWindowClassName[] = "StandAloneWindowClass";

BOOL CPlugin::initStandAlone()
{
  HWND hWndParent = NULL;

  // ensure window class
  WNDCLASS wc;
  wc.style         = 0; 
  wc.lpfnWndProc   = DefWindowProc; 
  wc.cbClsExtra    = 0; 
  wc.cbWndExtra    = 0; 
  wc.hInstance     = hInst; 
  wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON_APP)); 
  wc.hCursor       = LoadCursor(0, IDC_ARROW); 
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszMenuName  = NULL; 
  wc.lpszClassName = szStandAlonePluginWindowClassName;

  // just register the window class, if class already exists GetLastError() 
  // will return ERROR_CLASS_ALREADY_EXISTS, let's not care about it
  RegisterClass(&wc);

  hWndParent = CreateWindow(szStandAlonePluginWindowClassName, 
                            "The Tester Plugin", 
                            WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE | WS_MINIMIZEBOX, 
                            0, 0, 800, 600, 
                            GetDesktopWindow(), NULL, m_hInst, NULL);

//  m_hWndStandAloneLogWindow = CreateWindow("LISTBOX", "", 
  //                                          WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT, 
    //                                        200, 3, 590, 562, 
      //                                      hWndParent, NULL, m_hInst, NULL);

  m_hWndStandAloneLogWindow = CreateWindow("EDIT", "", 
                                            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY,
                                            200, 3, 590, 562, 
                                            hWndParent, NULL, m_hInst, NULL);
  if(!IsWindow(hWndParent))
    return FALSE;

  m_hWndParent = hWndParent;

  HFONT hFont = GetStockFont(DEFAULT_GUI_FONT);
  SetWindowFont(m_hWndStandAloneLogWindow, hFont, FALSE);

  CreateDialogParam(m_hInst, MAKEINTRESOURCE(IDD_DIALOG_TESTER), m_hWndParent, (DLGPROC)TesterDlgProc, (LPARAM)this);

  m_bPluginReady = (m_hWnd != NULL);

  return m_bPluginReady;
}

void CPlugin::shutStandAlone()
{
  // we don't care  about unregistering window class, let it be in the system

  if (isStandAlone() && m_hWndParent) 
    DestroyWindow(m_hWndParent);

  m_bPluginReady = FALSE;
}

BOOL CPlugin::initEmbed(DWORD dwInitData)
{
  if (m_bPluginReady)
    return TRUE;

  HWND hWndParent = (HWND)dwInitData;

  if(!IsWindow(hWndParent))
    return FALSE;

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

BOOL CPlugin::isStandAlone()
{
  return m_Pref_bStandAlone;
}

void CPlugin::outputToNativeWindow(LPSTR szString)
{
  if (!m_hWndStandAloneLogWindow)
    return;

  // VERSION FOR EDIT BOX

  static char text[16384];

  if (strlen(szString)) {
    int length = Edit_GetTextLength(m_hWndStandAloneLogWindow);
    if ((length + strlen(szString)) > sizeof(text))
      strcpy(text, szString);
    else {
      Edit_GetText(m_hWndStandAloneLogWindow, text, sizeof(text));
      strcat(text, szString);
    }

    Edit_SetText(m_hWndStandAloneLogWindow, text);
    int lines = Edit_GetLineCount(m_hWndStandAloneLogWindow);
    Edit_Scroll(m_hWndStandAloneLogWindow, lines, 0);
  }
  else
    Edit_SetText(m_hWndStandAloneLogWindow, ""); // clear the window

/*
  // VERSION FOR LISTBOX

  // parse the string and add lines to the listbox
  char *p = 0;
  char *newline = szString;

  for (;;) {
    // could be either \r\n or \n, we don't need them at all for the listbox
    p = strchr(newline, '\n');
    if (!p)
      break;

    if (*(p - 1) == '\r')
      *(p - 1) = '\0';
    else
      *p = '\0';

    char line[512];
    strcpy(line, newline);

    ListBox_AddString(m_hWndStandAloneLogWindow, line);
    int count = ListBox_GetCount(m_hWndStandAloneLogWindow);
    if(count == 32767)
      ListBox_ResetContent(m_hWndStandAloneLogWindow);
    ListBox_SetCaretIndex(m_hWndStandAloneLogWindow, count - 1);
    UpdateWindow(m_hWndStandAloneLogWindow);

    // restore the original
    if (*p == '\n')
      *(p - 1) = '\r';
    else
      *p = '\n';

    newline = ++p;
  }
*/
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

void CPlugin::autoStartScriptIfNeeded()
{
  if (!m_Pref_bAutoStartScript)
    return;

  CScripter scripter;
  scripter.associate(this);

  char szFileName[_MAX_PATH];
  getModulePath(szFileName, sizeof(szFileName));
  strcat(szFileName, m_Pref_szScriptFile);

  if(scripter.createScriptFromFile(szFileName))
  {
    int iRepetitions = scripter.getCycleRepetitions();
    int iDelay = scripter.getCycleDelay();
    if(iDelay < 0)
      iDelay = 0;

    assert(pLogger != NULL);
    pLogger->resetStartTime();

    for(int i = 0; i < iRepetitions; i++)
    {
      scripter.executeScript();
      if(iDelay != 0)
        XP_Sleep(iDelay);
    }
  }
  else
  {
    MessageBox(NULL, "Script file not found or invalid", "", MB_OK | MB_ICONERROR);
  }
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
    delete (CPlugin *)pPlugin;
}
