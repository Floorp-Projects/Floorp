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

#include <windowsx.h>
#include <assert.h>

#include "resource.h"

#include "plugin.h"
#include "logger.h"

extern CLogger * pLogger;

static void onCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
  CPlugin * pPlugin = (CPlugin *)GetWindowLong(hWnd, DWL_USER);

  switch (id)
  {
    case IDC_RADIO_MODE_MANUAL:
      if((codeNotify == BN_CLICKED) && (IsDlgButtonChecked(hWnd, IDC_RADIO_MODE_MANUAL) == BST_CHECKED))
        pPlugin->showGUI(sg_manual);
      break;
    case IDC_RADIO_MODE_AUTO:
      if((codeNotify == BN_CLICKED) && (IsDlgButtonChecked(hWnd, IDC_RADIO_MODE_AUTO) == BST_CHECKED))
        pPlugin->showGUI(sg_auto);
      break;
    case IDC_BUTTON_FLUSH:
      pLogger->clearTarget();
      pLogger->dumpLogToTarget();
      break;
    case IDC_BUTTON_CLEAR:
      pLogger->clearTarget();
      pLogger->clearLog();
      break;
    case IDC_EDIT_LOG_FILE_NAME:
      break;
    case IDC_CHECK_LOG_TO_FILE:
      if(codeNotify == BN_CLICKED)
      {
        EnableWindow(GetDlgItem(hWnd, IDC_EDIT_LOG_FILE_NAME), (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_LOG_TO_FILE)));
        pPlugin->onLogToFile(BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_LOG_TO_FILE));
      }
      break;
    case IDC_CHECK_LOG_TO_FRAME:
      if(codeNotify == BN_CLICKED)
      {
        EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_FLUSH), (BST_UNCHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_SHOW_LOG))
                                                        && (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_LOG_TO_FRAME)));
        EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_CLEAR), (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_LOG_TO_FRAME)));
        EnableWindow(GetDlgItem(hWnd, IDC_CHECK_SHOW_LOG), (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_LOG_TO_FRAME)));
        pLogger->setLogToFrameFlag(BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_LOG_TO_FRAME));
      }
      break;
    case IDC_CHECK_SHOW_LOG:
      if(codeNotify == BN_CLICKED)
      {
        EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_FLUSH), (BST_UNCHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_SHOW_LOG))
                                                        && (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_LOG_TO_FRAME)));
        pLogger->setShowImmediatelyFlag(IsDlgButtonChecked(hWnd, IDC_CHECK_SHOW_LOG) == BST_CHECKED);
      }
      break;
    default:
      break;
  }
}

BOOL CALLBACK NP_LOADDS ManualDlgProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK NP_LOADDS AutoDlgProc(HWND, UINT, WPARAM, LPARAM);

static BOOL onInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam)
{
  CPlugin * pPlugin = (CPlugin *)lParam;

  SetWindowLong(hWnd, DWL_USER, (long)pPlugin);

  SetWindowPos(hWnd, NULL, 0,0, 0,0, SWP_NOZORDER | SWP_NOSIZE);

  HINSTANCE hInst = pPlugin->getInstance();
  HWND hWndManual = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG_MANUAL), hWnd, (DLGPROC)ManualDlgProc, (LPARAM)pPlugin);
  HWND hWndAuto = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG_AUTO), hWnd, (DLGPROC)AutoDlgProc, (LPARAM)pPlugin);

  CheckRadioButton(hWnd, IDC_RADIO_MODE_MANUAL, IDC_RADIO_MODE_AUTO, 
                  (pPlugin->m_Pref_ShowGUI == sg_manual) ? IDC_RADIO_MODE_MANUAL : IDC_RADIO_MODE_AUTO);

  Edit_SetText(GetDlgItem(hWnd, IDC_EDIT_LOG_FILE_NAME), pPlugin->m_Pref_szLogFile);

  CheckDlgButton(hWnd, IDC_CHECK_LOG_TO_FILE, (pPlugin->m_Pref_bToFile) ? BST_CHECKED : BST_UNCHECKED);
  EnableWindow(GetDlgItem(hWnd, IDC_EDIT_LOG_FILE_NAME), (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_LOG_TO_FILE)));

  CheckDlgButton(hWnd, IDC_CHECK_LOG_TO_FRAME, (pPlugin->m_Pref_bToFrame) ? BST_CHECKED : BST_UNCHECKED);
  EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_CLEAR), (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_LOG_TO_FRAME)));
  EnableWindow(GetDlgItem(hWnd, IDC_CHECK_SHOW_LOG), (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_LOG_TO_FRAME)));

  CheckDlgButton(hWnd, IDC_CHECK_SHOW_LOG, (pPlugin->m_Pref_bFlushNow) ? BST_CHECKED : BST_UNCHECKED);

  EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_FLUSH), (BST_UNCHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_SHOW_LOG))
                                                  && (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_LOG_TO_FRAME)));

  pPlugin->onInit(hWnd, hWndManual, hWndAuto);

  pPlugin->showGUI(pPlugin->m_Pref_ShowGUI);

  return TRUE;
}

static void onDestroy(HWND hWnd)
{
  CPlugin * pPlugin = (CPlugin *)GetWindowLong(hWnd, DWL_USER);
  pPlugin->onDestroy();
}

BOOL CALLBACK NP_LOADDS TesterDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg)
  {
    case WM_INITDIALOG:
      return (BOOL)HANDLE_WM_INITDIALOG(hWnd, wParam, lParam, onInitDialog);
    case WM_COMMAND:
      HANDLE_WM_COMMAND(hWnd, wParam, lParam, onCommand);
      break;
    case WM_DESTROY:
      HANDLE_WM_DESTROY(hWnd, wParam, lParam, onDestroy);
      break;

    default:
      return FALSE;
  }
  return TRUE;
}
