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

extern CLogger * pLogger;

static void onCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
  CPlugin * pPlugin = (CPlugin *)GetWindowLong(hWnd, DWL_USER);
  assert(pPlugin != NULL);

  switch (id)
  {
    case IDC_COMBO_API_CALL:
      if(codeNotify != CBN_SELCHANGE)
        break;
      updateUI(hWnd);
      break;
    case IDC_BUTTON_GO:
      pLogger->blockDumpToFile(FALSE);
      onGo(hWnd);
      break;
    case IDC_EDIT_ARG1:
    case IDC_EDIT_ARG2:
    case IDC_EDIT_ARG3:
    case IDC_EDIT_ARG4:
    case IDC_EDIT_ARG5:
    case IDC_EDIT_ARG6:
    case IDC_EDIT_ARG7:
    {
      if(codeNotify == EN_SETFOCUS)
        pPlugin->m_hWndLastEditFocus = hWndCtl;

      char szString[80];
      char szEdit[128];
      int iIndex = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_API_CALL));
      ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_API_CALL), iIndex, szString);
      if(   ((strcmp(szString, STRING_NPN_POSTURL) == 0) || (strcmp(szString, STRING_NPN_POSTURLNOTIFY) == 0))
         && (id == IDC_EDIT_ARG5)
         && (codeNotify == EN_CHANGE))
      {
        Edit_GetText(hWndCtl, szEdit, sizeof(szEdit));
        int iLength = strlen(szEdit);
        SetDlgItemInt(hWnd, IDC_EDIT_ARG4, iLength, FALSE);
      }
      if(   ((strcmp(szString, STRING_NPN_WRITE) == 0))
         && (id == IDC_EDIT_ARG4)
         && (codeNotify == EN_CHANGE))
      {
        Edit_GetText(hWndCtl, szEdit, sizeof(szEdit));
        int iLength = strlen(szEdit);
        SetDlgItemInt(hWnd, IDC_EDIT_ARG3, iLength, FALSE);
      }

      // save values of window size on the fly as we type it
      if(    (strcmp(szString, STRING_NPN_SETVALUE) == 0)
          && ((id == IDC_EDIT_ARG3) || (id == IDC_EDIT_ARG4)) 
          && (codeNotify == EN_CHANGE)
          && (hWndCtl == GetFocus()))
      {
        iIndex = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_ARG2));
        ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_ARG2), iIndex, szString);
        if(strcmp(szString, "NPPVpluginWindowSize") == 0)
        {
          BOOL bTranslated = FALSE;
          int iWidth = GetDlgItemInt(hWnd, IDC_EDIT_ARG3, &bTranslated, TRUE);
          int iHeight = GetDlgItemInt(hWnd, IDC_EDIT_ARG4, &bTranslated, TRUE);

          if(pPlugin->m_iWidth != iWidth)
            pPlugin->m_iWidth = iWidth;
          if(pPlugin->m_iHeight != iHeight)
            pPlugin->m_iHeight = iHeight;
        }
      }
      break;
    }
    case IDC_COMBO_ARG2:
    {
      if(codeNotify != CBN_SELCHANGE)
        break;

      ShowWindow(GetDlgItem(hWnd, IDC_STATIC_ARG4), SW_HIDE);
      ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG4), SW_HIDE);
      Static_SetText(GetDlgItem(hWnd, IDC_STATIC_ARG3), "*value:");
      Edit_SetText(GetDlgItem(hWnd, IDC_EDIT_ARG3), "");

      char szString[80];
      int iIndex = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_API_CALL));
      ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_API_CALL), iIndex, szString);
      if(strcmp(szString, STRING_NPN_SETVALUE) == 0)
      {
        iIndex = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_ARG2));
        ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_ARG2), iIndex, szString);
        if(   (strcmp(szString, "NPPVpluginWindowBool") == 0) 
           || (strcmp(szString, "NPPVpluginTransparentBool") == 0))
        {
          HWND hWndCombo = GetDlgItem(hWnd, IDC_COMBO_ARG3);
          ComboBox_ResetContent(hWndCombo);
          ComboBox_AddString(hWndCombo, "TRUE");
          ComboBox_AddString(hWndCombo, "FALSE");
          ComboBox_SetCurSel(hWndCombo, 0);
          ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG3), SW_HIDE);
          ShowWindow(hWndCombo, SW_SHOW);
        }
        else
        {
          ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG3), SW_SHOW);
          ShowWindow(GetDlgItem(hWnd, IDC_COMBO_ARG3), SW_HIDE);
          if(strcmp(szString, "NPPVpluginWindowSize") == 0)
          {
            EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ARG4), TRUE);
            ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG4), SW_SHOW);
            ShowWindow(GetDlgItem(hWnd, IDC_STATIC_ARG4), SW_SHOW);
            Static_SetText(GetDlgItem(hWnd, IDC_STATIC_ARG3), "width:");
            Static_SetText(GetDlgItem(hWnd, IDC_STATIC_ARG4), "height:");
            SetDlgItemInt(hWnd, IDC_EDIT_ARG3, pPlugin->m_iWidth, TRUE);
            SetDlgItemInt(hWnd, IDC_EDIT_ARG4, pPlugin->m_iHeight, TRUE);
          }
        }
      }
      break;
    }
    case IDC_BUTTON_PASTE:
      onPaste(pPlugin->m_hWndLastEditFocus);
      break;
    default:
      break;
  }
}

static BOOL onInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam)
{
  CPlugin * pPlugin = (CPlugin *)lParam;
  SetWindowLong(hWnd, DWL_USER, (long)pPlugin);

  fillAPIComboBox(GetDlgItem(hWnd, IDC_COMBO_API_CALL));
  updateUI(hWnd);

  int iTopMargin = 160;
  SetWindowPos(hWnd, NULL, 0,iTopMargin, 0,0, SWP_NOZORDER | SWP_NOSIZE);
  return TRUE;
}

BOOL CALLBACK NP_LOADDS ManualDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg)
  {
    case WM_INITDIALOG:
      return (BOOL)HANDLE_WM_INITDIALOG(hWnd, wParam, lParam, onInitDialog);
    case WM_COMMAND:
      HANDLE_WM_COMMAND(hWnd, wParam, lParam, onCommand);
      break;

    default:
      return FALSE;
  }
  return TRUE;
}
