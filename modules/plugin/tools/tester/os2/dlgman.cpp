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
#include "guiprefs.h"
#include "os2utils.h"
#include "xp.h"

extern CLogger * pLogger;

static void onCommand(HWND hWnd, int id, HWND hWndCtl, USHORT codeNotify)
{
  CPlugin * pPlugin = (CPlugin *)WinQueryWindowULong(hWnd, QWL_USER);
  if (!pPlugin)
    return;

  switch (id)
  {
    case IDC_COMBO_API_CALL:
      if(codeNotify != CBN_LBSELECT)
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
      SHORT iIndex = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_API_CALL));
      LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_API_CALL), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(iIndex), (MPARAM)NULL);
      WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_API_CALL), LM_QUERYITEMTEXT, MPFROM2SHORT(iIndex, 80), (MPARAM)szString);
      if(   ((strcmp(szString, STRING_NPN_POSTURL) == 0) || (strcmp(szString, STRING_NPN_POSTURLNOTIFY) == 0))
         && (id == IDC_EDIT_ARG5)
         && (codeNotify == EN_CHANGE))
      {
        WinQueryWindowText(hWndCtl, sizeof(szEdit), szEdit);
        int iLength = strlen(szEdit);
        WinSetDlgItemShort(hWnd, IDC_EDIT_ARG4, iLength, FALSE);
      }
      if(   ((strcmp(szString, STRING_NPN_WRITE) == 0))
         && (id == IDC_EDIT_ARG4)
         && (codeNotify == EN_CHANGE))
      {
        WinQueryWindowText(hWndCtl, sizeof(szEdit), szEdit);
        int iLength = strlen(szEdit);
        WinSetDlgItemShort(hWnd, IDC_EDIT_ARG3, iLength, FALSE);
      }

      // save values of window size on the fly as we type it
      if(    (strcmp(szString, STRING_NPN_SETVALUE) == 0)
          && ((id == IDC_EDIT_ARG3) || (id == IDC_EDIT_ARG4)) 
          && (codeNotify == EN_CHANGE)
          && (hWndCtl == WinQueryFocus(HWND_DESKTOP)))
      {
        iIndex = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_ARG2));
        maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG2), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(iIndex), (MPARAM)NULL);
        WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG2), LM_QUERYITEMTEXT,  MPFROM2SHORT(iIndex, 80), (MPARAM)szString);
        if(strcmp(szString, "NPPVpluginWindowSize") == 0)
        {
          SHORT bTranslated;
          WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG3, &bTranslated, TRUE);
          int iWidth = bTranslated;
          WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG4, &bTranslated, TRUE);
          int iHeight = bTranslated;

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
      if(codeNotify != CBN_LBSELECT)
        break;

      WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_ARG4), FALSE);
      WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG4), FALSE);
      WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_ARG3), "*value:");
      WinSetWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG3), "");

      char szString[80];
      SHORT iIndex = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_API_CALL));
      LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_API_CALL), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(iIndex), (MPARAM)NULL);
      WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_API_CALL), LM_QUERYITEMTEXT,  MPFROM2SHORT(iIndex, 80), (MPARAM)szString);
      if(strcmp(szString, STRING_NPN_SETVALUE) == 0)
      {
        iIndex = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_ARG2));
        maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG2), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(iIndex), (MPARAM)NULL);
        WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG2), LM_QUERYITEMTEXT,  MPFROM2SHORT(iIndex, 80), (MPARAM)szString);
        if((strcmp(szString, "NPPVpluginWindowBool") == 0) ||
           (strcmp(szString, "NPPVpluginTransparentBool") == 0) ||
           (strcmp(szString, "NPPVpluginKeepLibraryInMemory") == 0))
        {
          HWND hWndCombo = WinWindowFromID(hWnd, IDC_COMBO_ARG3);
          WinSendMsg(hWndCombo, LM_DELETEALL, 0, 0);
          WinInsertLboxItem(hWndCombo, LIT_END, "TRUE");
          WinInsertLboxItem(hWndCombo, LIT_END, "FALSE");
          WinSendMsg(hWndCombo, LM_SELECTITEM, (MPARAM)0, (MPARAM)TRUE);
          WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG3), FALSE);
          WinShowWindow(hWndCombo, TRUE);
        }
        else
        {
          WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG3), TRUE);
          WinShowWindow(WinWindowFromID(hWnd, IDC_COMBO_ARG3), FALSE);
          if(strcmp(szString, "NPPVpluginWindowSize") == 0)
          {
            WinEnableWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG4), TRUE);
            WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG4), TRUE);
            WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_ARG4), TRUE);
            WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_ARG3), "width:");
            WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_ARG4), "height:");
            WinSetDlgItemShort(hWnd, IDC_EDIT_ARG3, pPlugin->m_iWidth, TRUE);
            WinSetDlgItemShort(hWnd, IDC_EDIT_ARG4, pPlugin->m_iHeight, TRUE);
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

static BOOL onInitDialog(HWND hWnd, HWND hWndFocus, MPARAM lParam)
{
  CPlugin * pPlugin = (CPlugin *)lParam;
  WinSetWindowULong(hWnd, QWL_USER, (ULONG)pPlugin);

  // look at the last used API call if needed
  int iSel = 0;
  if (pPlugin && pPlugin->m_Pref_bRememberLastCall) {
    char szFileName[_MAX_PATH];
    GetINIFileName(pPlugin->getInstance(), szFileName, sizeof(szFileName));
    iSel = XP_GetPrivateProfileInt(SECTION_PREFERENCES, KEY_LAST_API_CALL, 0, szFileName);
  }
  fillAPIComboBoxAndSetSel(WinWindowFromID(hWnd, IDC_COMBO_API_CALL), iSel);
  updateUI(hWnd);

  int iTopMargin = 188;
  WinSetWindowPos(hWnd, NULL, 0, iTopMargin, 0, 0, SWP_SHOW ); 
  return TRUE;
}

static void onDestroy(HWND hWnd)
{
  CPlugin * pPlugin = (CPlugin *)WinQueryWindowULong(hWnd, QWL_USER);
  if(pPlugin && pPlugin->m_Pref_bRememberLastCall) {
    // save last API call if needed
    char szFileName[_MAX_PATH];
    GetINIFileName(pPlugin->getInstance(), szFileName, sizeof(szFileName));
    int iSel = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_API_CALL));
    XP_WritePrivateProfileInt(SECTION_PREFERENCES, KEY_LAST_API_CALL, iSel, szFileName);
  }
}

MRESULT EXPENTRY NP_LOADDS ManualDlgProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch(msg)
  {
    case WM_INITDLG:
      onInitDialog(hWnd, 0, mp2);
      return (MRESULT) FALSE;
    case WM_COMMAND:
    case WM_CONTROL:
      onCommand(hWnd, SHORT1FROMMP(mp1), WinWindowFromID(hWnd, SHORT1FROMMP(mp1)), SHORT2FROMMP(mp1));
      break;
    case WM_DESTROY:
      onDestroy(hWnd);
      break;

    default:
      return WinDefDlgProc(hWnd, msg, mp1, mp2);
  }
  return (MRESULT)TRUE;
}
