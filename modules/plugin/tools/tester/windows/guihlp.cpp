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

#include "guihlp.h"
#include "plugin.h"
#include "comstrs.h"

static char szNotImplemented[] = "Currently not implemented";

void EnableWindowNow(HWND hWnd, BOOL bEnable)
{
  EnableWindow(hWnd, bEnable);
  UpdateWindow(hWnd);
}

void ShowWindowNow(HWND hWnd, int iShow)
{
  ShowWindow(hWnd, iShow);
  UpdateWindow(hWnd);
}

void fillAPIComboBox(HWND hWndCombo)
{
  ComboBox_AddString(hWndCombo, STRING_NPN_VERSION);
  ComboBox_AddString(hWndCombo, STRING_NPN_GETURL);
  ComboBox_AddString(hWndCombo, STRING_NPN_GETURLNOTIFY);
  ComboBox_AddString(hWndCombo, STRING_NPN_POSTURL);
  ComboBox_AddString(hWndCombo, STRING_NPN_POSTURLNOTIFY);
  ComboBox_AddString(hWndCombo, STRING_NPN_REQUESTREAD);
  ComboBox_AddString(hWndCombo, STRING_NPN_NEWSTREAM);
  ComboBox_AddString(hWndCombo, STRING_NPN_DESTROYSTREAM);
  ComboBox_AddString(hWndCombo, STRING_NPN_WRITE);
  ComboBox_AddString(hWndCombo, STRING_NPN_STATUS);
  ComboBox_AddString(hWndCombo, STRING_NPN_USERAGENT);
  ComboBox_AddString(hWndCombo, STRING_NPN_MEMALLOC);
  ComboBox_AddString(hWndCombo, STRING_NPN_MEMFREE);
  ComboBox_AddString(hWndCombo, STRING_NPN_MEMFLUSH);
  ComboBox_AddString(hWndCombo, STRING_NPN_RELOADPLUGINS);
  ComboBox_AddString(hWndCombo, STRING_NPN_GETJAVAENV);
  ComboBox_AddString(hWndCombo, STRING_NPN_GETJAVAPEER);
  ComboBox_AddString(hWndCombo, STRING_NPN_GETVALUE);
  ComboBox_AddString(hWndCombo, STRING_NPN_SETVALUE);
  ComboBox_AddString(hWndCombo, STRING_NPN_INVALIDATERECT);
  ComboBox_AddString(hWndCombo, STRING_NPN_INVALIDATEREGION);
  ComboBox_AddString(hWndCombo, STRING_NPN_FORCEREDRAW);

  ComboBox_SetCurSel(hWndCombo, 0);
}

static void setStaticTexts7(HWND hWnd, LPSTR s1, LPSTR s2, LPSTR s3, LPSTR s4, LPSTR s5,LPSTR s6, LPSTR s7)
{
  Static_SetText(GetDlgItem(hWnd, IDC_STATIC_ARG1), s1);
  Static_SetText(GetDlgItem(hWnd, IDC_STATIC_ARG2), s2);
  Static_SetText(GetDlgItem(hWnd, IDC_STATIC_ARG3), s3);
  Static_SetText(GetDlgItem(hWnd, IDC_STATIC_ARG4), s4);
  Static_SetText(GetDlgItem(hWnd, IDC_STATIC_ARG5), s5);
  Static_SetText(GetDlgItem(hWnd, IDC_STATIC_ARG6), s6);
  Static_SetText(GetDlgItem(hWnd, IDC_STATIC_ARG7), s7);
}

static void setEditTexts7(HWND hWnd, LPSTR s1, LPSTR s2, LPSTR s3, LPSTR s4, LPSTR s5,LPSTR s6, LPSTR s7)
{
  Edit_SetText(GetDlgItem(hWnd, IDC_EDIT_ARG1), s1);
  Edit_SetText(GetDlgItem(hWnd, IDC_EDIT_ARG2), s2);
  Edit_SetText(GetDlgItem(hWnd, IDC_EDIT_ARG3), s3);
  Edit_SetText(GetDlgItem(hWnd, IDC_EDIT_ARG4), s4);
  Edit_SetText(GetDlgItem(hWnd, IDC_EDIT_ARG5), s5);
  Edit_SetText(GetDlgItem(hWnd, IDC_EDIT_ARG6), s6);
  Edit_SetText(GetDlgItem(hWnd, IDC_EDIT_ARG7), s7);
}

static void showArgControls7(HWND hWnd, BOOL b1, BOOL b2, BOOL b3, BOOL b4, BOOL b5, BOOL b6, BOOL b7)
{
  ShowWindow(GetDlgItem(hWnd, IDC_STATIC_ARG1), b1 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_STATIC_ARG2), b2 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_STATIC_ARG3), b3 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_STATIC_ARG4), b4 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_STATIC_ARG5), b5 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_STATIC_ARG6), b6 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_STATIC_ARG7), b7 ? SW_SHOW : SW_HIDE);

  ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG1), b1 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG2), b2 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG3), b3 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG4), b4 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG5), b5 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG6), b6 ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG7), b7 ? SW_SHOW : SW_HIDE);
}

static void enableEdits7(HWND hWnd, BOOL b1, BOOL b2, BOOL b3, BOOL b4, BOOL b5, BOOL b6, BOOL b7)
{
  EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ARG1), b1);
  EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ARG2), b2);
  EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ARG3), b3);
  EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ARG4), b4);
  EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ARG5), b5);
  EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ARG6), b6);
  EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ARG7), b7);
}

static void replaceEditWithCombo(HWND hWnd, BOOL b1, BOOL b2, BOOL b3, BOOL b6)
{
  if(b1)
  {
    ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG1), SW_HIDE);
    ShowWindow(GetDlgItem(hWnd, IDC_COMBO_ARG1), SW_SHOW);
  }
  if(b2)
  {
    ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG2), SW_HIDE);
    ShowWindow(GetDlgItem(hWnd, IDC_COMBO_ARG2), SW_SHOW);
  }
  if(b3)
  {
    ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG3), SW_HIDE);
    ShowWindow(GetDlgItem(hWnd, IDC_COMBO_ARG3), SW_SHOW);
  }
  if(b6)
  {
    ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ARG6), SW_HIDE);
    ShowWindow(GetDlgItem(hWnd, IDC_COMBO_ARG6), SW_SHOW);
  }
}

void updateUI(HWND hWnd)
{
  CPlugin * pPlugin = (CPlugin *)GetWindowLong(hWnd, DWL_USER);
  assert(pPlugin != NULL);

  DWORD dwNPInstance = (DWORD)pPlugin->getNPInstance();
  char szNPInstance[16];
  wsprintf(szNPInstance, "%#08lx", dwNPInstance);
  Static_SetText(GetDlgItem(hWnd, IDC_STATIC_INFO), "");
  ShowWindow(GetDlgItem(hWnd, IDC_STATIC_INFO), SW_HIDE);

  setStaticTexts7(hWnd, "","","","","","","");
  setEditTexts7(hWnd, "","","","","","","");
  enableEdits7(hWnd, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE);

  EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), TRUE);

  char szString[80];
  int iIndex = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_API_CALL));
  ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_API_CALL), iIndex, szString);

  ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_SHOW);

  ComboBox_ResetContent(GetDlgItem(hWnd, IDC_COMBO_ARG1));
  ComboBox_ResetContent(GetDlgItem(hWnd, IDC_COMBO_ARG2));
  ComboBox_ResetContent(GetDlgItem(hWnd, IDC_COMBO_ARG3));
  ComboBox_ResetContent(GetDlgItem(hWnd, IDC_COMBO_ARG6));
  ShowWindow(GetDlgItem(hWnd, IDC_COMBO_ARG1), SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_COMBO_ARG2), SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_COMBO_ARG3), SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_COMBO_ARG6), SW_HIDE);

  if(strcmp(szString, STRING_NPN_VERSION) == 0)
  {
    showArgControls7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_GETURL) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "URL:", "target (or 'NULL'):", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"http://w3","_npapi_Display","","","","");
  }
  else if(strcmp(szString, STRING_NPN_GETURLNOTIFY) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "URL:", "target (or 'NULL'):", "notifyData:", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"http://w3","_npapi_Display","0","","","");
  }
  else if(strcmp(szString, STRING_NPN_POSTURL) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE);
    setStaticTexts7(hWnd, "instance:", "URL:", "target (or 'NULL'):", "len:", "buf:", "file:", "");
    setEditTexts7(hWnd, szNPInstance,"http://w3","_npapi_Display","0","","","");
    replaceEditWithCombo(hWnd, FALSE, FALSE, FALSE, TRUE);
    HWND hWndCombo = GetDlgItem(hWnd, IDC_COMBO_ARG6);
    ComboBox_AddString(hWndCombo, "FALSE");
    ComboBox_AddString(hWndCombo, "TRUE");
    ComboBox_SetCurSel(hWndCombo, 0);
  }
  else if(strcmp(szString, STRING_NPN_POSTURLNOTIFY) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE);
    setStaticTexts7(hWnd, "instance:", "URL:", "target (or 'NULL'):", "len:", "buf:", "file:", "notifyData:");
    setEditTexts7(hWnd, szNPInstance,"http://w3","_npapi_Display","0","","","0");
    replaceEditWithCombo(hWnd, FALSE, FALSE, FALSE, TRUE);
    HWND hWndCombo = GetDlgItem(hWnd, IDC_COMBO_ARG6);
    ComboBox_AddString(hWndCombo, "FALSE");
    ComboBox_AddString(hWndCombo, "TRUE");
    ComboBox_SetCurSel(hWndCombo, 0);
  }
  else if(strcmp(szString, STRING_NPN_NEWSTREAM) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "type:", "target:", "**stream:", "", "", "");
    char szStream[16];
    const NPStream * pStream = pPlugin->getNPStream();
    wsprintf(szStream, "%#08lx", &pStream);
    setEditTexts7(hWnd, szNPInstance,"text/plain","_npapi_Display",szStream,"","","");
    EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), (pStream != NULL) ? FALSE : TRUE);
  }
  else if(strcmp(szString, STRING_NPN_REQUESTREAD) == 0)
  {
    EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), FALSE);
    ShowWindow(GetDlgItem(hWnd, IDC_STATIC_INFO), SW_SHOW);
    Static_SetText(GetDlgItem(hWnd, IDC_STATIC_INFO), szNotImplemented);
    showArgControls7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_DESTROYSTREAM) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "stream:", "reason:", "", "", "", "");
    char szStream[16];
    const NPStream * pStream = pPlugin->getNPStream();
    wsprintf(szStream, "%#08lx", pStream);
    setEditTexts7(hWnd, szNPInstance,szStream,"","","","","");
    replaceEditWithCombo(hWnd, FALSE, FALSE, TRUE, FALSE);
    HWND hWndCombo = GetDlgItem(hWnd, IDC_COMBO_ARG3);
    ComboBox_AddString(hWndCombo, "NPRES_DONE");
    ComboBox_AddString(hWndCombo, "NPRES_USER_BREAK");
    ComboBox_AddString(hWndCombo, "NPRES_NETWORK_ERR");
    ComboBox_SetCurSel(hWndCombo, 0);
    EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), (pStream == NULL) ? FALSE : TRUE);
  }
  else if(strcmp(szString, STRING_NPN_WRITE) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "stream:", "len:", "buf:", "", "", "");
    char szStream[16];
    const NPStream * pStream = pPlugin->getNPStream();
    wsprintf(szStream, "%#08lx", pStream);
    setEditTexts7(hWnd, szNPInstance,szStream,"0","","","","");
    EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), (pStream == NULL) ? FALSE : TRUE);
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
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_MEMALLOC) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "size:", "", "", "", "", "", "");
    setEditTexts7(hWnd, "256","","","","","","");
    EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), (pPlugin->m_pNPNAlloced == NULL) ? TRUE : FALSE);
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_MEMFREE) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "ptr:", "", "", "", "", "", "");
    char szPtr[16];
    wsprintf(szPtr, "%#08lx", pPlugin->m_pNPNAlloced);
    setEditTexts7(hWnd, szPtr,"","","","","","");
    EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), (pPlugin->m_pNPNAlloced == NULL) ? FALSE : TRUE);
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_MEMFLUSH) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "size:", "", "", "", "", "", "");
    setEditTexts7(hWnd, "256","","","","","","");
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_RELOADPLUGINS) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "reloadPages:", "", "", "", "", "", "");
    setEditTexts7(hWnd, "","","","","","","");
    replaceEditWithCombo(hWnd, TRUE, FALSE, FALSE, FALSE);
    HWND hWndCombo = GetDlgItem(hWnd, IDC_COMBO_ARG1);
    ComboBox_AddString(hWndCombo, "TRUE");
    ComboBox_AddString(hWndCombo, "FALSE");
    ComboBox_SetCurSel(hWndCombo, 0);
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_GETJAVAENV) == 0)
  {
    showArgControls7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_GETJAVAPEER) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "", "", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"","","","","","");
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_GETVALUE) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "variable:", "", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"","","","","","");
    replaceEditWithCombo(hWnd, FALSE, TRUE, FALSE, FALSE);
    HWND hWndCombo = GetDlgItem(hWnd, IDC_COMBO_ARG2);
    ComboBox_AddString(hWndCombo, "NPNVxDisplay");
    ComboBox_AddString(hWndCombo, "NPNVxtAppContext");
    ComboBox_AddString(hWndCombo, "NPNVnetscapeWindow");
    ComboBox_AddString(hWndCombo, "NPNVjavascriptEnabledBool");
    ComboBox_AddString(hWndCombo, "NPNVasdEnabledBool");
    ComboBox_AddString(hWndCombo, "NPNVisOfflineBool");
    ComboBox_SetCurSel(hWndCombo, 0);
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_SETVALUE) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "variable:", "*value:", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"","","","","","");
    replaceEditWithCombo(hWnd, FALSE, TRUE, FALSE, FALSE);
    HWND hWndCombo = GetDlgItem(hWnd, IDC_COMBO_ARG2);
    ComboBox_AddString(hWndCombo, "NPPVpluginNameString");
    ComboBox_AddString(hWndCombo, "NPPVpluginDescriptionString");
    ComboBox_AddString(hWndCombo, "NPPVpluginWindowBool");
    ComboBox_AddString(hWndCombo, "NPPVpluginTransparentBool");
    ComboBox_AddString(hWndCombo, "NPPVpluginWindowSize");
    ComboBox_SetCurSel(hWndCombo, 0);
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_INVALIDATERECT) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "top:", "left:", "bottom:", "right:", "", "");
    setEditTexts7(hWnd, szNPInstance,"0","0","0","0","","");
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_INVALIDATEREGION) == 0)
  {
    showArgControls7(hWnd, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "region:", "", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"0","0","0","0","","");
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else if(strcmp(szString, STRING_NPN_FORCEREDRAW) == 0)
  {
    showArgControls7(hWnd, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    enableEdits7(hWnd, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    setStaticTexts7(hWnd, "instance:", "", "", "", "", "", "");
    setEditTexts7(hWnd, szNPInstance,"","","","","","");
    ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_PASTE), SW_HIDE);
  }
  else
    assert(0);
}

void onGo(HWND hWnd)
{
  CPlugin * pPlugin = (CPlugin *)GetWindowLong(hWnd, DWL_USER);
  assert(pPlugin != NULL);

  char szString[80];
  char sz1[128];
  char sz2[128];
  char sz3[128];
  char sz4[128];
  BOOL bTranslated = FALSE;

  ComboBox_GetText(GetDlgItem(hWnd, IDC_COMBO_API_CALL), szString, sizeof(szString));

  if(strcmp(szString, STRING_NPN_VERSION) == 0)
  {
    pPlugin->makeNPNCall(action_npn_version, DEFAULT_DWARG_VALUE, DEFAULT_DWARG_VALUE, 
                         DEFAULT_DWARG_VALUE, DEFAULT_DWARG_VALUE);
  }
  else if(strcmp(szString, STRING_NPN_GETURL) == 0)
  {
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG2), sz1, sizeof(sz1));
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG3), sz2, sizeof(sz2));
    DWORD dwTarget = 0L;
    if(strcmp(sz2, "NULL") == 0)
      dwTarget = 0L;
    else
      dwTarget = (DWORD)sz2;
    pPlugin->makeNPNCall(action_npn_get_url, DEFAULT_DWARG_VALUE, (DWORD)sz1, dwTarget);
  }
  else if(strcmp(szString, STRING_NPN_GETURLNOTIFY) == 0)
  {
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG2), sz1, sizeof(sz1));
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG3), sz2, sizeof(sz2));
    DWORD dwData = (DWORD)GetDlgItemInt(hWnd, IDC_EDIT_ARG4, &bTranslated, FALSE);
    DWORD dwTarget = 0L;
    if(strcmp(sz2, "NULL") == 0)
      dwTarget = 0L;
    else
      dwTarget = (DWORD)sz2;
    pPlugin->makeNPNCall(action_npn_get_url_notify, DEFAULT_DWARG_VALUE, (DWORD)sz1, dwTarget, dwData);
  }
  else if(strcmp(szString, STRING_NPN_POSTURL) == 0)
  {
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG2), sz1, sizeof(sz1));
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG3), sz2, sizeof(sz2));
    DWORD dwLen = (DWORD)GetDlgItemInt(hWnd, IDC_EDIT_ARG4, &bTranslated, FALSE);
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG5), sz3, sizeof(sz3));
    int index = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_ARG6));
    ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_ARG6), index, sz4);
    BOOL bFile = (strcmp(sz4, "TRUE") == 0) ? TRUE : FALSE;
    DWORD dwTarget = 0L;
    if(strcmp(sz2, "NULL") == 0)
      dwTarget = 0L;
    else
      dwTarget = (DWORD)sz2;
    pPlugin->makeNPNCall(action_npn_post_url, DEFAULT_DWARG_VALUE, (DWORD)sz1, dwTarget, dwLen, (DWORD)sz3, (DWORD)bFile);
  }
  else if(strcmp(szString, STRING_NPN_POSTURLNOTIFY) == 0)
  {
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG2), sz1, sizeof(sz1));
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG3), sz2, sizeof(sz2));
    DWORD dwLen = (DWORD)GetDlgItemInt(hWnd, IDC_EDIT_ARG4, &bTranslated, FALSE);
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG5), sz3, sizeof(sz3));
    int index = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_ARG6));
    ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_ARG6), index, sz4);
    BOOL bFile = (strcmp(sz4, "TRUE") == 0) ? TRUE : FALSE;
    DWORD dwData = (DWORD)GetDlgItemInt(hWnd, IDC_EDIT_ARG7, &bTranslated, FALSE);
    DWORD dwTarget = 0L;
    if(strcmp(sz2, "NULL") == 0)
      dwTarget = 0L;
    else
      dwTarget = (DWORD)sz2;
    pPlugin->makeNPNCall(action_npn_post_url_notify, DEFAULT_DWARG_VALUE, (DWORD)sz1, dwTarget, dwLen, (DWORD)sz3, 
                         (DWORD)bFile, dwData);
  }
  else if(strcmp(szString, STRING_NPN_NEWSTREAM) == 0)
  {
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG2), sz1, sizeof(sz1));
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG3), sz2, sizeof(sz2));
    pPlugin->makeNPNCall(action_npn_new_stream, DEFAULT_DWARG_VALUE, (DWORD)sz1, (DWORD)sz2, DEFAULT_DWARG_VALUE);
  }
  else if(strcmp(szString, STRING_NPN_DESTROYSTREAM) == 0)
  {
    int index = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_ARG3));
    ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_ARG3), index, sz1);
  
    NPError reason;
    if(strcmp(sz1, "NPRES_DONE") == 0)
      reason = NPRES_DONE;
    else if(strcmp(sz1, "NPRES_USER_BREAK") == 0)
      reason = NPRES_USER_BREAK;
    else if(strcmp(sz1, "NPRES_NETWORK_ERR") == 0)
      reason = NPRES_NETWORK_ERR;
    else
      reason = NPRES_DONE;

    pPlugin->makeNPNCall(action_npn_destroy_stream, DEFAULT_DWARG_VALUE, DEFAULT_DWARG_VALUE, (DWORD)reason);
  }
  else if(strcmp(szString, STRING_NPN_REQUESTREAD) == 0)
  {
  }
  else if(strcmp(szString, STRING_NPN_WRITE) == 0)
  {
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG4), sz1, sizeof(sz1));
    DWORD dwLen = (DWORD)GetDlgItemInt(hWnd, IDC_EDIT_ARG3, &bTranslated, FALSE);
    pPlugin->makeNPNCall(action_npn_write, DEFAULT_DWARG_VALUE, DEFAULT_DWARG_VALUE, dwLen, (DWORD)sz1);
  }
  else if(strcmp(szString, STRING_NPN_STATUS) == 0)
  {
    Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG2), sz1, sizeof(sz1));
    pPlugin->makeNPNCall(action_npn_status, DEFAULT_DWARG_VALUE, (DWORD)sz1);
  }
  else if(strcmp(szString, STRING_NPN_USERAGENT) == 0)
  {
    pPlugin->makeNPNCall(action_npn_user_agent);
  }
  else if(strcmp(szString, STRING_NPN_MEMALLOC) == 0)
  {
    DWORD dwSize = (DWORD)GetDlgItemInt(hWnd, IDC_EDIT_ARG1, &bTranslated, FALSE);
    pPlugin->makeNPNCall(action_npn_mem_alloc, dwSize);
  }
  else if(strcmp(szString, STRING_NPN_MEMFREE) == 0)
  {
    pPlugin->makeNPNCall(action_npn_mem_free);
  }
  else if(strcmp(szString, STRING_NPN_MEMFLUSH) == 0)
  {
    DWORD dwSize = (DWORD)GetDlgItemInt(hWnd, IDC_EDIT_ARG1, &bTranslated, FALSE);
    pPlugin->makeNPNCall(action_npn_mem_flush, dwSize);
  }
  else if(strcmp(szString, STRING_NPN_RELOADPLUGINS) == 0)
  {
    int index = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_ARG1));
    ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_ARG1), index, sz1);
    BOOL bReloadPages = (strcmp(sz1, "TRUE") == 0) ? TRUE : FALSE;
    pPlugin->makeNPNCall(action_npn_reload_plugins, (DWORD)bReloadPages);
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
    static DWORD dwValue = 0L;
    NPNVariable variable = (NPNVariable)NULL;

    int index = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_ARG2));
    ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_ARG2), index, sz1);
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

    pPlugin->makeNPNCall(action_npn_get_value, DEFAULT_DWARG_VALUE, (DWORD)variable, DEFAULT_DWARG_VALUE);
  }
  else if(strcmp(szString, STRING_NPN_SETVALUE) == 0)
  {
    NPPVariable variable = (NPPVariable)NULL;
    static char szStringValue[256];
    static BOOL bBoolValue;

    int index = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_ARG2));
    ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_ARG2), index, sz1);
    if(strcmp(sz1, "NPPVpluginNameString") == 0)
    {
      variable = NPPVpluginNameString;
      Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG3), szStringValue, sizeof(szStringValue));
      pPlugin->m_pValue = (void *)&szStringValue[0];
    }
    else if(strcmp(sz1, "NPPVpluginDescriptionString") == 0)
    {
      variable = NPPVpluginDescriptionString;
      Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_ARG3), szStringValue, sizeof(szStringValue));
      pPlugin->m_pValue = (void *)&szStringValue[0];
    }
    else if(strcmp(sz1, "NPPVpluginWindowBool") == 0)
    {
      variable = NPPVpluginWindowBool;
      index = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_ARG3));
      ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_ARG3), index, szStringValue);
      bBoolValue = (strcmp(szStringValue, "TRUE") == 0) ? TRUE : FALSE;
      pPlugin->m_pValue = (void *)&bBoolValue;
    }
    else if(strcmp(sz1, "NPPVpluginTransparentBool") == 0)
    {
      variable = NPPVpluginTransparentBool;
      index = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_ARG3));
      ComboBox_GetLBText(GetDlgItem(hWnd, IDC_COMBO_ARG3), index, szStringValue);
      bBoolValue = (strcmp(szStringValue, "TRUE") == 0) ? TRUE : FALSE;
      pPlugin->m_pValue = (void *)&bBoolValue;
    }
    else if(strcmp(sz1, "NPPVpluginWindowSize") == 0)
    {
      variable = NPPVpluginWindowSize;
      static NPSize npsize;
      npsize.width = GetDlgItemInt(hWnd, IDC_EDIT_ARG3, &bTranslated, TRUE);
      npsize.height = GetDlgItemInt(hWnd, IDC_EDIT_ARG4, &bTranslated, TRUE);
      pPlugin->m_pValue = (void *)&npsize;
    }
    pPlugin->makeNPNCall(action_npn_set_value, DEFAULT_DWARG_VALUE, (DWORD)variable, DEFAULT_DWARG_VALUE);
  }
  else if(strcmp(szString, STRING_NPN_INVALIDATERECT) == 0)
  {
    static NPRect nprect;
    nprect.top    = GetDlgItemInt(hWnd, IDC_EDIT_ARG2, &bTranslated, TRUE);
    nprect.left   = GetDlgItemInt(hWnd, IDC_EDIT_ARG3, &bTranslated, TRUE);
    nprect.bottom = GetDlgItemInt(hWnd, IDC_EDIT_ARG4, &bTranslated, TRUE);
    nprect.right  = GetDlgItemInt(hWnd, IDC_EDIT_ARG5, &bTranslated, TRUE);
    pPlugin->makeNPNCall(action_npn_invalidate_rect, DEFAULT_DWARG_VALUE, (DWORD)&nprect);
  }
  else if(strcmp(szString, STRING_NPN_INVALIDATEREGION) == 0)
  {
    int i = GetDlgItemInt(hWnd, IDC_EDIT_ARG2, &bTranslated, FALSE);
    pPlugin->makeNPNCall(action_npn_invalidate_region, DEFAULT_DWARG_VALUE, (DWORD)i);
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
  if(!OpenClipboard(NULL))
    return;
  HANDLE hData = GetClipboardData(CF_TEXT);
  Edit_ReplaceSel(hWndToPasteTo, (char *)hData);
  CloseClipboard();
}
