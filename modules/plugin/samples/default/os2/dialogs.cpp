/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   IBM Corp.
 */

#define INCL_WIN
#include <os2.h>
#include <string.h>

#include "npnulos2.h"

#include "plugin.h"
#include "utils.h"

static char szDefaultPluginFinderURL[] = DEFAULT_PLUGINFINDER_URL;

static void onCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
  CPlugin * pPlugin = (CPlugin *)WinQueryWindowPtr(hWnd, QWL_USER);
  switch (id)
  {
    case IDC_GET_PLUGIN:
      WinDismissDlg(hWnd, IDC_GET_PLUGIN);
      if(pPlugin !=NULL)
        pPlugin->m_hWndDialog = NULL;
      break;
    case DID_CANCEL:
      WinDismissDlg(hWnd, DID_CANCEL);
      if(pPlugin !=NULL)
        pPlugin->m_hWndDialog = NULL;
      break;
    default:
      break;
  }
}

static BOOL onInitDialog(HWND hWnd, HWND hWndFocus, MPARAM mParam)
{
  CPlugin * pPlugin = (CPlugin *)mParam;
  assert(pPlugin != NULL);
  if(pPlugin == NULL)
    return TRUE;

  WinSetWindowPtr(hWnd, QWL_USER, (PVOID)pPlugin);

  pPlugin->m_hWndDialog = hWnd;
  
  char szString[512];
  WinLoadString((HAB)0, hInst, IDS_TITLE, sizeof(szString), szString);
  WinSetWindowText(hWnd, szString);

  WinLoadString((HAB)0, hInst, IDS_INFO, sizeof(szString), szString);
  WinSetDlgItemText(hWnd, IDC_STATIC_INFO, szString);

  WinSetDlgItemText(hWnd, IDC_STATIC_INFOTYPE, pPlugin->m_pNPMIMEType);

  WinLoadString((HAB)0, hInst, IDS_LOCATION, sizeof(szString), szString);
  WinSetDlgItemText(hWnd, IDC_STATIC_LOCATION, szString);

  char contentTypeIsJava = 0;

  if (NULL != pPlugin->m_pNPMIMEType) {
    contentTypeIsJava = (0 == strcmp("application/x-java-vm",
				     pPlugin->m_pNPMIMEType)) ? 1 : 0;
  }
  
  if(pPlugin->m_szPageURL == NULL || contentTypeIsJava)
    WinLoadString((HAB)0, hInst, IDS_FINDER_PAGE, sizeof(szString), szString);
  else
    strncpy(szString, pPlugin->m_szPageURL,511); // defect #362738
  
  SetDlgItemTextWrapped(hWnd, IDC_STATIC_URL, szString);

  WinLoadString((HAB)0, hInst, IDS_QUESTION, sizeof(szString), szString);
  WinSetDlgItemText(hWnd, IDC_STATIC_QUESTION, szString);

  WinSetDlgItemText(hWnd, IDC_STATIC_WARNING, "");

  if(!pPlugin->m_bOnline)
  {
    WinEnableWindow(WinWindowFromID(hWnd, IDC_GET_PLUGIN), FALSE);
    WinLoadString((HAB)0, hInst, IDS_WARNING_OFFLINE, sizeof(szString), szString);
    WinSetDlgItemText(hWnd, IDC_STATIC_WARNING, szString);
    WinSetDlgItemText(hWnd, IDC_STATIC_QUESTION, "");
    return TRUE;
  }

  if((!pPlugin->m_bJava) || (!pPlugin->m_bJavaScript) || (!pPlugin->m_bSmartUpdate))
  {
    WinLoadString((HAB)0, hInst, IDS_WARNING_JS, sizeof(szString), szString);
    WinSetDlgItemText(hWnd, IDC_STATIC_WARNING, szString);
    return TRUE;
  }

  WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_WARNING), FALSE);

  RECTL rc;

  WinQueryWindowRect(WinWindowFromID(hWnd, IDC_STATIC_WARNING), &rc);
  int iHeight = rc.yTop - rc.yBottom;
  WinQueryWindowRect(hWnd, &rc);
  WinSetWindowPos(hWnd, 0, 0, 0, rc.xRight - rc.xLeft, rc.yTop - rc.yBottom - iHeight, SWP_SIZE);

  HWND hWndQuestion = WinWindowFromID(hWnd, IDC_STATIC_QUESTION);
  HWND hWndButtonGetPlugin = WinWindowFromID(hWnd, IDC_GET_PLUGIN);
  HWND hWndButtonCancel = WinWindowFromID(hWnd, IDC_BUTTON_CANCEL);

  POINTL pt;

  WinQueryWindowRect(hWndQuestion, &rc);
  pt.x = rc.xLeft;
  pt.y = rc.yBottom;
//  ScreenToClient(hWnd, &pt);
  WinSetWindowPos(hWndQuestion, 0, pt.x, pt.y - iHeight, 0, 0, SWP_MOVE);

  WinQueryWindowRect(hWndButtonGetPlugin, &rc);
  pt.x = rc.xLeft;
  pt.y = rc.yBottom;
//  ScreenToClient(hWnd, &pt);
  WinSetWindowPos(hWndButtonGetPlugin, 0, pt.x, pt.y - iHeight, 0, 0, SWP_MOVE);

  WinQueryWindowRect(hWndButtonCancel, &rc);
  pt.x = rc.xLeft;
  pt.y = rc.yBottom;
//  ScreenToClient(hWnd, &pt);
  WinSetWindowPos(hWndButtonCancel, 0, pt.x, pt.y - iHeight, 0, 0, SWP_MOVE);

  if(pPlugin->m_bHidden)
    WinSetActiveWindow(HWND_DESKTOP, hWnd);

  return TRUE;
}

static void onClose(HWND hWnd)
{
  WinDismissDlg(hWnd, DID_CANCEL);
  CPlugin * pPlugin = (CPlugin *)WinQueryWindowPtr(hWnd, QWL_USER);
  if(pPlugin !=NULL)
    pPlugin->m_hWndDialog = NULL;
}

static void onDestroy(HWND hWnd)
{
}

MRESULT EXPENTRY NP_LOADDS GetPluginDialogProc(HWND hWnd, ULONG uMsg, MPARAM mp1, MPARAM mp2)
{
  switch(uMsg)
  {
    case WM_INITDLG:
      onInitDialog(hWnd,0,mp2);
      return (MRESULT)FALSE;
   case WM_COMMAND:
      onCommand(hWnd, SHORT1FROMMP(mp1),0,0);
      break;
   case WM_DESTROY:
      onDestroy(hWnd);
      break;
   case WM_CLOSE:
      onClose(hWnd);
      break;

    default:
      return WinDefDlgProc(hWnd, uMsg, mp1, mp2);
  }
  return (MRESULT)TRUE;
}
