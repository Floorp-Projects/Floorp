/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include <windows.h>
#include <windowsx.h>

#include "resource.h"

#include "plugin.h"
#include "utils.h"

static char szDefaultPluginFinderURL[] = DEFAULT_PLUGINFINDER_URL;

static void onCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
  CPlugin * pPlugin = (CPlugin *)GetWindowLong(hWnd, DWL_USER);
  switch (id)
  {
    case IDC_GET_PLUGIN:
      DestroyWindow(hWnd);
      if(pPlugin !=NULL)
      {
        pPlugin->m_hWndDialog = NULL;
        pPlugin->getPlugin();
      }
      break;
    case IDCANCEL:
    case IDC_BUTTON_CANCEL:
      DestroyWindow(hWnd);
      if(pPlugin !=NULL)
        pPlugin->m_hWndDialog = NULL;
      break;
    default:
      break;
  }
}

static BOOL onInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam)
{
  CPlugin * pPlugin = (CPlugin *)lParam;
  assert(pPlugin != NULL);
  if(pPlugin == NULL)
    return TRUE;

  SetWindowLong(hWnd, DWL_USER, (LONG)pPlugin);

  pPlugin->m_hWndDialog = hWnd;
  
  char szString[512];
  LoadString(hInst, IDS_TITLE, szString, sizeof(szString));
  SetWindowText(hWnd, szString);

  LoadString(hInst, IDS_INFO, szString, sizeof(szString));
  SetDlgItemText(hWnd, IDC_STATIC_INFO, szString);

  SetDlgItemText(hWnd, IDC_STATIC_INFOTYPE, (LPSTR)pPlugin->m_pNPMIMEType);

  LoadString(hInst, IDS_LOCATION, szString, sizeof(szString));
  SetDlgItemText(hWnd, IDC_STATIC_LOCATION, szString);

  char contentTypeIsJava = 0;

  if (NULL != pPlugin->m_pNPMIMEType) {
    contentTypeIsJava = (0 == strcmp("application/x-java-vm",
				     pPlugin->m_pNPMIMEType)) ? 1 : 0;
  }
  
  if(pPlugin->m_szPageURL == NULL || contentTypeIsJava)
    LoadString(hInst, IDS_FINDER_PAGE, szString, sizeof(szString));
  else
    strncpy(szString, pPlugin->m_szPageURL,511); // defect #362738
  
  SetDlgItemTextWrapped(hWnd, IDC_STATIC_URL, szString);

  LoadString(hInst, IDS_QUESTION, szString, sizeof(szString));
  SetDlgItemText(hWnd, IDC_STATIC_QUESTION, szString);

  SetDlgItemText(hWnd, IDC_STATIC_WARNING, "");

  if(!pPlugin->m_bOnline)
  {
    EnableWindow(GetDlgItem(hWnd, IDC_GET_PLUGIN), FALSE);
    LoadString(hInst, IDS_WARNING_OFFLINE, szString, sizeof(szString));
    SetDlgItemText(hWnd, IDC_STATIC_WARNING, szString);
    SetDlgItemText(hWnd, IDC_STATIC_QUESTION, "");
    return TRUE;
  }

  if((!pPlugin->m_bJava) || (!pPlugin->m_bJavaScript) || (!pPlugin->m_bSmartUpdate))
  {
    LoadString(hInst, IDS_WARNING_JS, szString, sizeof(szString));
    SetDlgItemText(hWnd, IDC_STATIC_WARNING, szString);
    return TRUE;
  }

  ShowWindow(GetDlgItem(hWnd, IDC_STATIC_WARNING), SW_HIDE);

  RECT rc;
  GetWindowRect(GetDlgItem(hWnd, IDC_STATIC_WARNING), &rc);
  int iHeight = rc.bottom - rc.top;
  GetWindowRect(hWnd, &rc);
  SetWindowPos(hWnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top - iHeight, SWP_NOZORDER | SWP_NOMOVE);

  HWND hWndQuestion = GetDlgItem(hWnd, IDC_STATIC_QUESTION);
  HWND hWndButtonGetPlugin = GetDlgItem(hWnd, IDC_GET_PLUGIN);
  HWND hWndButtonCancel = GetDlgItem(hWnd, IDC_BUTTON_CANCEL);

  POINT pt;

  GetWindowRect(hWndQuestion, &rc);
  pt.x = rc.left;
  pt.y = rc.top;
  ScreenToClient(hWnd, &pt);
  SetWindowPos(hWndQuestion, NULL, pt.x, pt.y - iHeight, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

  GetWindowRect(hWndButtonGetPlugin, &rc);
  pt.x = rc.left;
  pt.y = rc.top;
  ScreenToClient(hWnd, &pt);
  SetWindowPos(hWndButtonGetPlugin, NULL, pt.x, pt.y - iHeight, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

  GetWindowRect(hWndButtonCancel, &rc);
  pt.x = rc.left;
  pt.y = rc.top;
  ScreenToClient(hWnd, &pt);
  SetWindowPos(hWndButtonCancel, NULL, pt.x, pt.y - iHeight, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

  if(pPlugin->m_bHidden)
    SetForegroundWindow(hWnd);

  return TRUE;
}

static void onClose(HWND hWnd)
{
  DestroyWindow(hWnd);
}

static void onDestroy(HWND hWnd)
{
}

BOOL CALLBACK NP_LOADDS GetPluginDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch(uMsg)
  {
    case WM_INITDIALOG:
      return (BOOL)HANDLE_WM_INITDIALOG(hWnd, wParam, lParam, onInitDialog);
    case WM_COMMAND:
      HANDLE_WM_COMMAND(hWnd, wParam, lParam, onCommand);
      break;
    case WM_DESTROY:
      HANDLE_WM_DESTROY(hWnd, wParam, lParam, onDestroy);
      break;
    case WM_CLOSE:
      HANDLE_WM_CLOSE(hWnd, wParam, lParam, onClose);
      break;

    default:
      return FALSE;
  }
  return TRUE;
}
