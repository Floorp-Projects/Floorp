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
#define INCL_WINLISTBOXES
#include <os2.h>
#include <assert.h>

#include "xp.h"
#include "resource.h"

#include "guihlp.h"
#include "plugin.h"
#include "comstrs.h"

static char szDefaultNPByteRangeList[] = "100-100,200-100,300-100";

static char szNotImplemented[] = "Currently not implemented";

void EnableWindowNow(HWND hWnd, BOOL bEnable)
{
  WinEnableWindow(hWnd, bEnable);
  WinUpdateWindow(hWnd);
}

void ShowWindowNow(HWND hWnd, BOOL iShow)
{
  WinShowWindow(hWnd, iShow);
  WinUpdateWindow(hWnd);
}

void fillAPIComboBoxAndSetSel(HWND hWndCombo, int iSel)
{
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_VERSION);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_GETURL);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_GETURLNOTIFY);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_POSTURL);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_POSTURLNOTIFY);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_REQUESTREAD);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_NEWSTREAM);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_DESTROYSTREAM);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_WRITE);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_STATUS);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_USERAGENT);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_MEMALLOC);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_MEMFREE);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_MEMFLUSH);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_RELOADPLUGINS);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_GETJAVAENV);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_GETJAVAPEER);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_GETVALUE);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_SETVALUE);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_INVALIDATERECT);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_INVALIDATEREGION);
  WinInsertLboxItem(hWndCombo, LIT_END, STRING_NPN_FORCEREDRAW);

  WinSendMsg(hWndCombo, LM_SELECTITEM, (MPARAM)iSel, (MPARAM)TRUE);
}

static void setStaticTexts7(HWND hWnd, PSZ s1, PSZ s2, PSZ s3, PSZ s4, PSZ s5,PSZ s6, PSZ s7)
{
  WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_ARG1), s1);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_ARG2), s2);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_ARG3), s3);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_ARG4), s4);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_ARG5), s5);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_ARG6), s6);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_ARG7), s7);
}

static void setEditTexts7(HWND hWnd, PSZ s1, PSZ s2, PSZ s3, PSZ s4, PSZ s5,PSZ s6, PSZ s7)
{
  WinSetWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG1), s1);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG2), s2);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG3), s3);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG4), s4);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG5), s5);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG6), s6);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG7), s7);
}

static void showArgControls7(HWND hWnd, BOOL b1, BOOL b2, BOOL b3, BOOL b4, BOOL b5, BOOL b6, BOOL b7)
{
  WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_ARG1), b1 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_ARG2), b2 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_ARG3), b3 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_ARG4), b4 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_ARG5), b5 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_ARG6), b6 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_ARG7), b7 ? TRUE : FALSE);

  WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG1), b1 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG2), b2 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG3), b3 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG4), b4 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG5), b5 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG6), b6 ? TRUE : FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG7), b7 ? TRUE : FALSE);
}

static void enableEdits7(HWND hWnd, BOOL b1, BOOL b2, BOOL b3, BOOL b4, BOOL b5, BOOL b6, BOOL b7)
{
  WinEnableWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG1), b1);
  WinEnableWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG2), b2);
  WinEnableWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG3), b3);
  WinEnableWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG4), b4);
  WinEnableWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG5), b5);
  WinEnableWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG6), b6);
  WinEnableWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG7), b7);
}

static void replaceEditWithCombo(HWND hWnd, BOOL b1, BOOL b2, BOOL b3, BOOL b6)
{
  if(b1)
  {
    WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG1), FALSE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_COMBO_ARG1), TRUE);
  }
  if(b2)
  {
    WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG2), FALSE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_COMBO_ARG2), TRUE);
  }
  if(b3)
  {
    WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG3), FALSE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_COMBO_ARG3), TRUE);
  }
  if(b6)
  {
    WinShowWindow(WinWindowFromID(hWnd, IDC_EDIT_ARG6), FALSE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_COMBO_ARG6), TRUE);
  }
}

#define NS_SAMPLE_URL "http://mozilla.org"

void updateUI(HWND hWnd)
{
  CPlugin * pPlugin = (CPlugin *)WinQueryWindowULong(hWnd, QWL_USER);
  assert(pPlugin != NULL);

  ULONG dwNPInstance = (ULONG)pPlugin->getNPInstance();
  char szNPInstance[16];
  sprintf(szNPInstance, "%#08lx", dwNPInstance);
  WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_INFO), "");
  WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_INFO), FALSE);

  setStaticTexts7(hWnd, "","","","","","","");
  setEditTexts7(hWnd, "","","","","","","");
  enableEdits7(hWnd, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE);

  WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), TRUE);

  char szString[80];
  SHORT iIndex = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_API_CALL));
  LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_API_CALL), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(iIndex), (MPARAM)NULL);
  WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_API_CALL), LM_QUERYITEMTEXT,  MPFROM2SHORT(iIndex, 80), (MPARAM)szString);

  WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), TRUE);

  WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG1), LM_DELETEALL, 0, 0);
  WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG2), LM_DELETEALL, 0, 0);
  WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG3), LM_DELETEALL, 0, 0);
  WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG6), LM_DELETEALL, 0, 0);
  WinShowWindow(WinWindowFromID(hWnd, IDC_COMBO_ARG1), FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_COMBO_ARG2), FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_COMBO_ARG3), FALSE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_COMBO_ARG6), FALSE);

  if(strcmp(szString, STRING_NPN_VERSION) == 0)
  {
    showArgControls7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_GETURL) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "URL:", "target (or 'NULL'):", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,NS_SAMPLE_URL,"_npapi_Display","","","","");
  }
  else if(strcmp(szString, STRING_NPN_GETURLNOTIFY) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "URL:", "target (or 'NULL'):", "notifyData:", "", "", "");
    setEditTexts7(hWnd, szNPInstance,NS_SAMPLE_URL,"_npapi_Display","0","","","");
  }
  else if(strcmp(szString, STRING_NPN_POSTURL) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE);
    setStaticTexts7(hWnd, "instance:", "URL:", "target (or 'NULL'):", "len:", "buf:", "file:", "");
    setEditTexts7(hWnd, szNPInstance,NS_SAMPLE_URL,"_npapi_Display","0","","","");
    replaceEditWithCombo(hWnd, FALSE, FALSE, FALSE, TRUE);
    HWND hWndCombo = WinWindowFromID(hWnd, IDC_COMBO_ARG6);
    WinInsertLboxItem(hWndCombo, LIT_END, "FALSE");
    WinInsertLboxItem(hWndCombo, LIT_END, "TRUE");
    WinSendMsg(hWndCombo, LM_SELECTITEM, (MPARAM)0, (MPARAM)TRUE);
  }
  else if(strcmp(szString, STRING_NPN_POSTURLNOTIFY) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE);
    setStaticTexts7(hWnd, "instance:", "URL:", "target (or 'NULL'):", "len:", "buf:", "file:", "notifyData:");
    setEditTexts7(hWnd, szNPInstance,NS_SAMPLE_URL,"_npapi_Display","0","","","0");
    replaceEditWithCombo(hWnd, FALSE, FALSE, FALSE, TRUE);
    HWND hWndCombo = WinWindowFromID(hWnd, IDC_COMBO_ARG6);
    WinInsertLboxItem(hWndCombo, LIT_END, "FALSE");
    WinInsertLboxItem(hWndCombo, LIT_END, "TRUE");
    WinSendMsg(hWndCombo, LM_SELECTITEM, (MPARAM)0, (MPARAM)TRUE);
  }
  else if(strcmp(szString, STRING_NPN_NEWSTREAM) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "type:", "target:", "**stream:", "", "", "");
    char szStream[16];
    const NPStream * pStream = pPlugin->getNPStream();
    sprintf(szStream, "%#08lx", &pStream);
    setEditTexts7(hWnd, szNPInstance,"text/plain","_npapi_Display",szStream,"","","");
    WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), (pStream != NULL) ? FALSE : TRUE);
  }
  else if(strcmp(szString, STRING_NPN_REQUESTREAD) == 0)
  {
//serge
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "URL:", "Range:", "notifyData:", "", "", "");
    setEditTexts7(hWnd, szNPInstance,NS_SAMPLE_URL,szDefaultNPByteRangeList,"0","","","");
   /*
    WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), FALSE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_INFO), TRUE);
    WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_INFO), szNotImplemented);
    showArgControls7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
    */
  }
  else if(strcmp(szString, STRING_NPN_DESTROYSTREAM) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "stream:", "reason:", "", "", "", "");
    char szStream[16];
    const NPStream * pStream = pPlugin->getNPStream();
    sprintf(szStream, "%#08lx", pStream);
    setEditTexts7(hWnd, szNPInstance,szStream,"","","","","");
    replaceEditWithCombo(hWnd, FALSE, FALSE, TRUE, FALSE);
    HWND hWndCombo = WinWindowFromID(hWnd, IDC_COMBO_ARG3);
    WinInsertLboxItem(hWndCombo, LIT_END, "NPRES_DONE");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPRES_USER_BREAK");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPRES_NETWORK_ERR");
    WinSendMsg(hWndCombo, LM_SELECTITEM, (MPARAM)0, (MPARAM)TRUE);
    WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), (pStream == NULL) ? FALSE : TRUE);
  }
  else if(strcmp(szString, STRING_NPN_WRITE) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "stream:", "len:", "buf:", "", "", "");
    char szStream[16];
    const NPStream * pStream = pPlugin->getNPStream();
    sprintf(szStream, "%#08lx", pStream);
    setEditTexts7(hWnd, szNPInstance,szStream,"0","","","","");
    WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), (pStream == NULL) ? FALSE : TRUE);
  }
  else if(strcmp(szString, STRING_NPN_STATUS) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "message:", "", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"Some message","","","","","");
  }
  else if(strcmp(szString, STRING_NPN_USERAGENT) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "", "", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"","","","","","");
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_MEMALLOC) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "size:", "", "", "", "", "", "");
    setEditTexts7(hWnd, "256","","","","","","");
    WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), (pPlugin->m_pNPNAlloced == NULL) ? TRUE : FALSE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_MEMFREE) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "ptr:", "", "", "", "", "", "");
    char szPtr[16];
    sprintf(szPtr, "%#08lx", pPlugin->m_pNPNAlloced);
    setEditTexts7(hWnd, szPtr,"","","","","","");
    WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), (pPlugin->m_pNPNAlloced == NULL) ? FALSE : TRUE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_MEMFLUSH) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "size:", "", "", "", "", "", "");
    setEditTexts7(hWnd, "256","","","","","","");
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_RELOADPLUGINS) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "reloadPages:", "", "", "", "", "", "");
    setEditTexts7(hWnd, "","","","","","","");
    replaceEditWithCombo(hWnd, TRUE, FALSE, FALSE, FALSE);
    HWND hWndCombo = WinWindowFromID(hWnd, IDC_COMBO_ARG1);
    WinInsertLboxItem(hWndCombo, LIT_END, "TRUE");
    WinInsertLboxItem(hWndCombo, LIT_END, "FALSE");
    WinSendMsg(hWndCombo, LM_SELECTITEM, (MPARAM)0, (MPARAM)TRUE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_GETJAVAENV) == 0)
  {
    showArgControls7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_GETJAVAPEER) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "", "", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"","","","","","");
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_GETVALUE) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "variable:", "", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"","","","","","");
    replaceEditWithCombo(hWnd, FALSE, TRUE, FALSE, FALSE);
    HWND hWndCombo = WinWindowFromID(hWnd, IDC_COMBO_ARG2);
    WinInsertLboxItem(hWndCombo, LIT_END, "NPNVxDisplay");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPNVxtAppContext");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPNVnetscapeWindow");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPNVjavascriptEnabledBool");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPNVasdEnabledBool");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPNVisOfflineBool");
    WinSendMsg(hWndCombo, LM_SELECTITEM, (MPARAM)0, (MPARAM)TRUE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_SETVALUE) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "variable:", "*value:", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"","","","","","");
    replaceEditWithCombo(hWnd, FALSE, TRUE, FALSE, FALSE);
    HWND hWndCombo = WinWindowFromID(hWnd, IDC_COMBO_ARG2);
    WinInsertLboxItem(hWndCombo, LIT_END, "NPPVpluginNameString");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPPVpluginDescriptionString");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPPVpluginWindowBool");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPPVpluginTransparentBool");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPPVpluginWindowSize");
    WinInsertLboxItem(hWndCombo, LIT_END, "NPPVpluginKeepLibraryInMemory");
    WinSendMsg(hWndCombo, LM_SELECTITEM, (MPARAM)0, (MPARAM)TRUE);
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_INVALIDATERECT) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "top:", "left:", "bottom:", "right:", "", "");
    setEditTexts7(hWnd, szNPInstance,"0","0","0","0","","");
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_INVALIDATEREGION) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "region:", "", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"0","0","0","0","","");
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else if(strcmp(szString, STRING_NPN_FORCEREDRAW) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "", "", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"","","","","","");
    WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_PASTE), FALSE);
  }
  else
    assert(0);
}

/*      
NPByteRange g_npByteRangeList[] = {
   {100, 100, npByteRangeList + 1},
   {200, 100, npByteRangeList + 2},
   {300, 100, 0}
};
*/

void onGo(HWND hWnd)
{
  CPlugin * pPlugin = (CPlugin *)WinQueryWindowULong(hWnd, QWL_USER);
  assert(pPlugin != NULL);

  char szString[80];
  char sz1[128];
  char sz2[128];
  char sz3[128];
  char sz4[128];
  SHORT bTranslated;

  WinQueryWindowText(WinWindowFromID(hWnd, IDC_COMBO_API_CALL), sizeof(szString), szString);

  if(strcmp(szString, STRING_NPN_VERSION) == 0)
  {
    pPlugin->makeNPNCall(action_npn_version, DEFAULT_DWARG_VALUE, DEFAULT_DWARG_VALUE, 
                         DEFAULT_DWARG_VALUE, DEFAULT_DWARG_VALUE);
  }
  else if(strcmp(szString, STRING_NPN_GETURL) == 0)
  {
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG2), sizeof(sz1), sz1);
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG3), sizeof(sz2), sz2);
    ULONG dwTarget = 0L;
    if(strcmp(sz2, "NULL") == 0)
      dwTarget = 0L;
    else
      dwTarget = (ULONG)sz2;
    pPlugin->makeNPNCall(action_npn_get_url, DEFAULT_DWARG_VALUE, (ULONG)sz1, dwTarget);
  }
  else if(strcmp(szString, STRING_NPN_GETURLNOTIFY) == 0)
  {
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG2), sizeof(sz1), sz1);
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG3), sizeof(sz2), sz2);
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG4, &bTranslated, FALSE);
    ULONG dwData = (ULONG) bTranslated;
    ULONG dwTarget = 0L;
    if(strcmp(sz2, "NULL") == 0)
      dwTarget = 0L;
    else
      dwTarget = (ULONG)sz2;
    pPlugin->makeNPNCall(action_npn_get_url_notify, DEFAULT_DWARG_VALUE, (ULONG)sz1, dwTarget, dwData);
  }
  else if(strcmp(szString, STRING_NPN_REQUESTREAD) == 0)
  {
    extern NPByteRange * convertStringToNPByteRangeList(PSZ szString);

    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG2), sizeof(sz1), sz1);
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG3), sizeof(sz2), sz2);
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG4, &bTranslated, FALSE);
    ULONG dwData = (ULONG) bTranslated;
    ULONG dwTarget = 0L;
    NPByteRange *npByteRangeList = convertStringToNPByteRangeList(sz2);
    if (!npByteRangeList) { // use default szDefaultNPByteRangeList
      npByteRangeList = convertStringToNPByteRangeList(szDefaultNPByteRangeList);
    }
    pPlugin->m_firstAction = action_npn_request_read;
    dwData = (ULONG) npByteRangeList;
    pPlugin->makeNPNCall(action_npn_get_url_notify, DEFAULT_DWARG_VALUE, (ULONG)sz1, dwTarget, dwData);
  }
  else if(strcmp(szString, STRING_NPN_POSTURL) == 0)
  {
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG2), sizeof(sz1), sz1);
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG3), sizeof(sz2), sz2);
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG4, &bTranslated, FALSE);
    ULONG dwLen = (ULONG) bTranslated;
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG5), sizeof(sz3), sz3);
    SHORT index = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_ARG6));
    LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG6), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(index), (MPARAM)NULL);
    WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG6), LM_QUERYITEMTEXT,  MPFROM2SHORT(index, 128), (MPARAM)sz4);
    BOOL bFile = (strcmp(sz4, "TRUE") == 0) ? TRUE : FALSE;
    ULONG dwTarget = 0L;
    if(strcmp(sz2, "NULL") == 0)
      dwTarget = 0L;
    else
      dwTarget = (ULONG)sz2;
    pPlugin->makeNPNCall(action_npn_post_url, DEFAULT_DWARG_VALUE, (ULONG)sz1, dwTarget, dwLen, (ULONG)sz3, (ULONG)bFile);
  }
  else if(strcmp(szString, STRING_NPN_POSTURLNOTIFY) == 0)
  {
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG2), sizeof(sz1), sz1);
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG3), sizeof(sz2), sz2);
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG4, &bTranslated, FALSE);
    ULONG dwLen = (ULONG) bTranslated;
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG5), sizeof(sz3), sz3);
    SHORT index = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_ARG6));
    LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG6), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(index), (MPARAM)NULL);
    WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG6), LM_QUERYITEMTEXT,  MPFROM2SHORT(index, 128), (MPARAM)sz4);
    BOOL bFile = (strcmp(sz4, "TRUE") == 0) ? TRUE : FALSE;
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG7, &bTranslated, FALSE);
    ULONG dwData = (ULONG) bTranslated;
    ULONG dwTarget = 0L;
    if(strcmp(sz2, "NULL") == 0)
      dwTarget = 0L;
    else
      dwTarget = (ULONG)sz2;
    pPlugin->makeNPNCall(action_npn_post_url_notify, DEFAULT_DWARG_VALUE, (ULONG)sz1, dwTarget, dwLen, (ULONG)sz3, 
                         (ULONG)bFile, dwData);
  }
  else if(strcmp(szString, STRING_NPN_NEWSTREAM) == 0)
  {
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG2), sizeof(sz1), sz1);
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG3), sizeof(sz2), sz2);
    pPlugin->makeNPNCall(action_npn_new_stream, DEFAULT_DWARG_VALUE, (ULONG)sz1, (ULONG)sz2, DEFAULT_DWARG_VALUE);
  }
  else if(strcmp(szString, STRING_NPN_DESTROYSTREAM) == 0)
  {
    SHORT index = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_ARG3));
    LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG3), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(index), (MPARAM)NULL);
    WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG3), LM_QUERYITEMTEXT,  MPFROM2SHORT(index, 128), (MPARAM)sz1);
  
    NPError reason;
    if(strcmp(sz1, "NPRES_DONE") == 0)
      reason = NPRES_DONE;
    else if(strcmp(sz1, "NPRES_USER_BREAK") == 0)
      reason = NPRES_USER_BREAK;
    else if(strcmp(sz1, "NPRES_NETWORK_ERR") == 0)
      reason = NPRES_NETWORK_ERR;
    else
      reason = NPRES_DONE;

    pPlugin->makeNPNCall(action_npn_destroy_stream, DEFAULT_DWARG_VALUE, DEFAULT_DWARG_VALUE, (ULONG)reason);
  }
  else if(strcmp(szString, STRING_NPN_REQUESTREAD) == 0)
  {
  }
  else if(strcmp(szString, STRING_NPN_WRITE) == 0)
  {
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG4), sizeof(sz1), sz1);
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG3, &bTranslated, FALSE);
    ULONG dwLen = (ULONG) bTranslated;
    pPlugin->makeNPNCall(action_npn_write, DEFAULT_DWARG_VALUE, DEFAULT_DWARG_VALUE, dwLen, (ULONG)sz1);
  }
  else if(strcmp(szString, STRING_NPN_STATUS) == 0)
  {
    WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG2), sizeof(sz1), sz1);
    pPlugin->makeNPNCall(action_npn_status, DEFAULT_DWARG_VALUE, (ULONG)sz1);
  }
  else if(strcmp(szString, STRING_NPN_USERAGENT) == 0)
  {
    pPlugin->makeNPNCall(action_npn_user_agent);
  }
  else if(strcmp(szString, STRING_NPN_MEMALLOC) == 0)
  {
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG1, &bTranslated, FALSE);
    ULONG dwSize = (ULONG) bTranslated;
    pPlugin->makeNPNCall(action_npn_mem_alloc, dwSize);
  }
  else if(strcmp(szString, STRING_NPN_MEMFREE) == 0)
  {
    pPlugin->makeNPNCall(action_npn_mem_free);
  }
  else if(strcmp(szString, STRING_NPN_MEMFLUSH) == 0)
  {
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG1, &bTranslated, FALSE);
    ULONG dwSize = (ULONG) bTranslated;
    pPlugin->makeNPNCall(action_npn_mem_flush, dwSize);
  }
  else if(strcmp(szString, STRING_NPN_RELOADPLUGINS) == 0)
  {
    SHORT index = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_ARG1));
    LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG1), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(index), (MPARAM)NULL);
    WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG1), LM_QUERYITEMTEXT,  MPFROM2SHORT(index, 128), (MPARAM)sz1);
    BOOL bReloadPages = (strcmp(sz1, "TRUE") == 0) ? TRUE : FALSE;
    pPlugin->makeNPNCall(action_npn_reload_plugins, (ULONG)bReloadPages);
  }
  else if(strcmp(szString, STRING_NPN_GETJAVAENV) == 0)
  {
    pPlugin->makeNPNCall(action_npn_get_java_env);
  }
  else if(strcmp(szString, STRING_NPN_GETJAVAPEER) == 0)
  {
    pPlugin->makeNPNCall(action_npn_get_java_peer);
  }
  else if(strcmp(szString, STRING_NPN_GETVALUE) == 0)
  {
    static ULONG dwValue = 0L;
    NPNVariable variable = (NPNVariable)NULL;

    SHORT index = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_ARG2));
    LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG2), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(index), (MPARAM)NULL);
    WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG2), LM_QUERYITEMTEXT,  MPFROM2SHORT(index, 128), (MPARAM)sz1);
    if(strcmp(sz1, "NPNVxDisplay") == 0)
      variable = NPNVxDisplay;
    else if(strcmp(sz1, "NPNVxtAppContext") == 0)
      variable = NPNVxtAppContext;
    else if(strcmp(sz1, "NPNVnetscapeWindow") == 0)
      variable = NPNVnetscapeWindow;
    else if(strcmp(sz1, "NPNVjavascriptEnabledBool") == 0)
      variable = NPNVjavascriptEnabledBool;
    else if(strcmp(sz1, "NPNVasdEnabledBool") == 0)
      variable = NPNVasdEnabledBool;
    else if(strcmp(sz1, "NPNVisOfflineBool") == 0)
      variable = NPNVisOfflineBool;

    pPlugin->m_pValue = (void *)&dwValue;

    pPlugin->makeNPNCall(action_npn_get_value, DEFAULT_DWARG_VALUE, (ULONG)variable, DEFAULT_DWARG_VALUE);
  }
  else if(strcmp(szString, STRING_NPN_SETVALUE) == 0)
  {
    NPPVariable variable = (NPPVariable)NULL;
    static char szStringValue[256];
    static BOOL bBoolValue;

    SHORT index = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_ARG2));
    LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG2), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(index), (MPARAM)NULL);
    WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG2), LM_QUERYITEMTEXT,  MPFROM2SHORT(index, 128), (MPARAM)sz1);
    if(strcmp(sz1, "NPPVpluginNameString") == 0)
    {
      variable = NPPVpluginNameString;
      WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG3), sizeof(szStringValue), szStringValue);
      pPlugin->m_pValue = (void *)&szStringValue[0];
    }
    else if(strcmp(sz1, "NPPVpluginDescriptionString") == 0)
    {
      variable = NPPVpluginDescriptionString;
      WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_ARG3), sizeof(szStringValue), szStringValue);
      pPlugin->m_pValue = (void *)&szStringValue[0];
    }
    else if(strcmp(sz1, "NPPVpluginWindowBool") == 0)
    {
      variable = NPPVpluginWindowBool;
      index = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_ARG3));
      LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG3), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(index), (MPARAM)NULL);
      WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG3), LM_QUERYITEMTEXT,  MPFROM2SHORT(index, 256), (MPARAM)szStringValue);
      bBoolValue = (strcmp(szStringValue, "TRUE") == 0) ? TRUE : FALSE;
      pPlugin->m_pValue = (void *)&bBoolValue;
    }
    else if(strcmp(sz1, "NPPVpluginTransparentBool") == 0)
    {
      variable = NPPVpluginTransparentBool;
      index = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_ARG3));
      LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG3), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(index), (MPARAM)NULL);
      WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG3), LM_QUERYITEMTEXT,  MPFROM2SHORT(index, 256), (MPARAM)szStringValue);
      bBoolValue = (strcmp(szStringValue, "TRUE") == 0) ? TRUE : FALSE;
      pPlugin->m_pValue = (void *)&bBoolValue;
    }
    else if(strcmp(sz1, "NPPVpluginWindowSize") == 0)
    {
      variable = NPPVpluginWindowSize;
      static NPSize npsize;
      WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG3, &bTranslated, TRUE);
      npsize.width = bTranslated;
      WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG4, &bTranslated, TRUE);
      npsize.height = bTranslated;
      pPlugin->m_pValue = (void *)&npsize;
    }
    else if(strcmp(sz1, "NPPVpluginKeepLibraryInMemory") == 0)
    {
      variable = NPPVpluginKeepLibraryInMemory;
      index = WinQueryLboxSelectedItem(WinWindowFromID(hWnd, IDC_COMBO_ARG3));
      LONG maxchar = (LONG)WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG3), LM_QUERYITEMTEXTLENGTH, MPFROMSHORT(index), (MPARAM)NULL);
      WinSendMsg(WinWindowFromID(hWnd, IDC_COMBO_ARG3), LM_QUERYITEMTEXT,  MPFROM2SHORT(index, 256), (MPARAM)szStringValue);
      bBoolValue = (strcmp(szStringValue, "TRUE") == 0) ? TRUE : FALSE;
      pPlugin->m_pValue = (void *)&bBoolValue;
    }
    pPlugin->makeNPNCall(action_npn_set_value, DEFAULT_DWARG_VALUE, (ULONG)variable, DEFAULT_DWARG_VALUE);
  }
  else if(strcmp(szString, STRING_NPN_INVALIDATERECT) == 0)
  {
    static NPRect nprect;
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG2, &bTranslated, TRUE);
    nprect.top = bTranslated;
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG3, &bTranslated, TRUE);
    nprect.left = bTranslated;
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG4, &bTranslated, TRUE);
    nprect.bottom = bTranslated;
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG5, &bTranslated, TRUE);
    nprect.right = bTranslated;
    pPlugin->makeNPNCall(action_npn_invalidate_rect, DEFAULT_DWARG_VALUE, (ULONG)&nprect);
  }
  else if(strcmp(szString, STRING_NPN_INVALIDATEREGION) == 0)
  {
    WinQueryDlgItemShort(hWnd, IDC_EDIT_ARG2, &bTranslated, FALSE);
    int i = bTranslated;
    pPlugin->makeNPNCall(action_npn_invalidate_region, DEFAULT_DWARG_VALUE, (ULONG)i);
  }
  else if(strcmp(szString, STRING_NPN_FORCEREDRAW) == 0)
  {
    pPlugin->makeNPNCall(action_npn_force_redraw);
  }
  else
    assert(0);
}

void onPaste(HWND hWndToPasteTo)
{
  if(hWndToPasteTo == NULL)
    return;
  if(!WinOpenClipbrd(NULL))
    return;
  WinSendMsg(hWndToPasteTo, EM_PASTE, 0, 0);
  WinCloseClipbrd(NULL);
}
