/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Sean Su <ssu@netscape.com>
 */

#ifndef _DIALOGS_H_
#define _DIALOGS_H_

LRESULT CALLBACK  DlgProcMain(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
MRESULT EXPENTRY  DlgProcWelcome(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY  DlgProcLicense(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY  DlgProcSetupType(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY  DlgProcSelectComponents(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY  DlgProcSelectAdditionalComponents(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY  DlgProcOS2Integration(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY  DlgProcAdditionalOptions(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY  DlgProcAdvancedSettings(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
LRESULT CALLBACK  DlgProcQuickLaunch(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcSiteSelector(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcStartInstall(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT CALLBACK  DlgProcReboot(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY  DlgProcMessage(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY  NewListBoxWndProc( HWND, ULONG, MPARAM, MPARAM);
LRESULT CALLBACK  DlgProcUpgrade(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);

void              ToggleCheck(HWND hwndListBox, DWORD dwIndex, DWORD dwACFlag);
BOOL              AskCancelDlg(HWND hDlg);
void              lbAddItem(HWND hList, siC *siCComponent);
HWND              InstantiateDialog(HWND hParent, ULONG ulDlgID, PSZ szTitle, PFNWP pfnwpDlgProc);
void              DlgSequence(int iDirection);
void              PaintGradientShade(HWND hWnd, HDC hdc);
BOOL              BrowseForDirectory(HWND hDlg, char *szCurrDir);
LRESULT CALLBACK  BrowseHookProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void              ShowMessage(PSZ szMessage, BOOL bShow);
void              DrawLBText(LPDRAWITEMSTRUCT lpdis, DWORD dwACFlag);
void              DrawCheck(LPDRAWITEMSTRUCT lpdis, DWORD dwACFlag);
void              InvalidateLBCheckbox(HWND hwndListBox);
void              ProcessWindowsMessages(void);
LPSTR             GetStartInstallMessage(void);
void              AppendStringWOTilde(LPSTR szInputString, DWORD dwInputStringSize, LPSTR szString);
void              TruncateString(HWND hWnd, LPSTR szInPath, LPSTR szOutPath, DWORD dwOutPathBufSize);
void              SaveAdditionalOptions(HWND hDlg, HWND hwndCBSiteSelector);
WNDPROC           SubclassWindow( HWND hWnd, WNDPROC NewWndProc);
LRESULT CALLBACK  ListBoxBrowseWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void              DisableSystemMenuItems(HWND hWnd, BOOL bDisableClose);
void              CommitInstall(void);
void              AdjustDialogSize(HWND hwndDlg);

#endif /* _DIALOGS_H_ */

