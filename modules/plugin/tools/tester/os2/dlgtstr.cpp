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


#define INCL_WIN
#include <os2.h>

#include "xp.h"
#include "resource.h"

#include "plugin.h"
#include "logger.h"

extern CLogger * pLogger;

static void onCommand(HWND hWnd, int id, HWND hWndCtl, USHORT codeNotify)
{
  CPlugin * pPlugin = (CPlugin *)WinQueryWindowULong(hWnd, QWL_USER);
  if(!pPlugin)
    return;

  switch (id)
  {
    case IDC_RADIO_MODE_MANUAL:
      if((codeNotify == BN_CLICKED) && (WinQueryButtonCheckstate(hWnd, IDC_RADIO_MODE_MANUAL) == BST_CHECKED))
      {
        pPlugin->showGUI(sg_manual);
        pPlugin->updatePrefs(gp_mode, sg_manual);
      }
      break;
    case IDC_EDIT_LOG_FILE_NAME:
      if(codeNotify == EN_CHANGE)
      {
        char szString[256];
        WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_LOG_FILE_NAME), sizeof(szString), szString);
        pPlugin->updatePrefs(gp_logfile, FALSE, szString);
      }
    case IDC_RADIO_MODE_AUTO:
      if((codeNotify == BN_CLICKED) && (WinQueryButtonCheckstate(hWnd, IDC_RADIO_MODE_AUTO) == BST_CHECKED))
      {
        pPlugin->showGUI(sg_auto);
        pPlugin->updatePrefs(gp_mode, sg_auto);
      }
      break;
    case IDC_BUTTON_FLUSH:
      pLogger->clearTarget();
      pLogger->dumpLogToTarget();
      break;
    case IDC_BUTTON_CLEAR:
      pLogger->clearTarget();
      pLogger->clearLog();
      break;
      break;
    case IDC_CHECK_LOG_TO_FILE:
      if(codeNotify == BN_CLICKED)
      {
        pPlugin->updatePrefs(gp_tofile, BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FILE));
        WinEnableWindow(WinWindowFromID(hWnd, IDC_EDIT_LOG_FILE_NAME), (BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FILE)));
        pPlugin->onLogToFile(BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FILE));
      }
      break;
    case IDC_CHECK_LOG_TO_FRAME:
      if(codeNotify == BN_CLICKED)
      {
        pPlugin->updatePrefs(gp_toframe, BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FRAME));
        WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_FLUSH), (BST_UNCHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_SHOW_LOG))
                                                        && (BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FRAME)));
        WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_CLEAR), (BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FRAME)));
        WinEnableWindow(WinWindowFromID(hWnd, IDC_CHECK_SHOW_LOG), (BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FRAME)));
        pLogger->setLogToFrameFlag(BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FRAME));
      }
      break;
    case IDC_CHECK_SHOW_LOG:
      if(codeNotify == BN_CLICKED)
      {
        pPlugin->updatePrefs(gp_flush, WinQueryButtonCheckstate(hWnd, IDC_CHECK_SHOW_LOG) == BST_CHECKED);
        WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_FLUSH), (BST_UNCHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_SHOW_LOG))
                                                        && (BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FRAME)));
        pLogger->setShowImmediatelyFlag(WinQueryButtonCheckstate(hWnd, IDC_CHECK_SHOW_LOG) == BST_CHECKED);
      }
      break;
    case IDC_CHECK_REMEMBER_LAST:
      if(codeNotify == BN_CLICKED)
        pPlugin->updatePrefs(gp_rememberlast, WinQueryButtonCheckstate(hWnd, IDC_CHECK_REMEMBER_LAST) == BST_CHECKED);
      break;
    default:
      break;
  }
}

MRESULT EXPENTRY NP_LOADDS ManualDlgProc(HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY NP_LOADDS AutoDlgProc(HWND, ULONG, MPARAM, MPARAM);

static BOOL onInitDialog(HWND hWnd, HWND hWndFocus, MPARAM lParam)
{
  CPlugin * pPlugin = (CPlugin *)lParam;

  WinSetWindowULong(hWnd, QWL_USER, (ULONG)pPlugin);

  WinSetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_SHOW ); 

  HMODULE hInst = pPlugin->getInstance();
  HWND hWndManual = WinLoadDlg(hWnd, hWnd, (PFNWP)ManualDlgProc, hInst, IDD_DIALOG_MANUAL, (PVOID)pPlugin);
  HWND hWndAuto = WinLoadDlg(hWnd, hWnd, (PFNWP)AutoDlgProc, hInst, IDD_DIALOG_AUTO, (PVOID)pPlugin);

  WinCheckButton(hWnd, (pPlugin->m_Pref_ShowGUI == sg_manual) ? IDC_RADIO_MODE_MANUAL : IDC_RADIO_MODE_AUTO, BST_CHECKED);

  WinSetWindowText(WinWindowFromID(hWnd, IDC_EDIT_LOG_FILE_NAME), pPlugin->m_Pref_szLogFile);

  WinCheckButton(hWnd, IDC_CHECK_LOG_TO_FILE, (pPlugin->m_Pref_bToFile) ? BST_CHECKED : BST_UNCHECKED);
  WinEnableWindow(WinWindowFromID(hWnd, IDC_EDIT_LOG_FILE_NAME), (BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FILE)));

  WinCheckButton(hWnd, IDC_CHECK_LOG_TO_FRAME, (pPlugin->m_Pref_bToFrame) ? BST_CHECKED : BST_UNCHECKED);
  WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_CLEAR), (BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FRAME)));
  WinEnableWindow(WinWindowFromID(hWnd, IDC_CHECK_SHOW_LOG), (BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FRAME)));

  WinCheckButton(hWnd, IDC_CHECK_SHOW_LOG, (pPlugin->m_Pref_bFlushNow) ? BST_CHECKED : BST_UNCHECKED);
  WinCheckButton(hWnd, IDC_CHECK_REMEMBER_LAST, (pPlugin->m_Pref_bRememberLastCall) ? BST_CHECKED : BST_UNCHECKED);

  WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_FLUSH), (BST_UNCHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_SHOW_LOG))
                                                  && (BST_CHECKED == WinQueryButtonCheckstate(hWnd, IDC_CHECK_LOG_TO_FRAME)));

  pPlugin->onInit(hWnd, hWndManual, hWndAuto);

  pPlugin->showGUI(pPlugin->m_Pref_ShowGUI);

  return TRUE;
}

static void onDestroy(HWND hWnd)
{
  CPlugin * pPlugin = (CPlugin *)WinQueryWindowULong(hWnd, QWL_USER);
  pPlugin->onDestroy();
}

MRESULT EXPENTRY NP_LOADDS TesterDlgProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch(msg)
  {
    case WM_INITDLG:
      onInitDialog(hWnd, 0, mp2);
      return (MRESULT) FALSE;
    case WM_COMMAND:
    case WM_CONTROL:
      onCommand(hWnd, SHORT1FROMMP(mp1), 0, SHORT2FROMMP(mp1));
      break;
    case WM_DESTROY:
      onDestroy(hWnd);
      break;

    default:
      return WinDefDlgProc(hWnd, msg, mp1, mp2);
  }
  return (MRESULT)TRUE;
}
