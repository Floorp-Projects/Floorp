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
LRESULT CALLBACK  DlgProcWelcome(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcLicense(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcSetupType(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcSelectComponents(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcSelectAdditionalComponents(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcWindowsIntegration(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcProgramFolder(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcDownloadOptions(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcAdvancedSettings(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcSiteSelector(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcStartInstall(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcReboot(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcMessage(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  NewListBoxWndProc( HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK  DlgProcUpgrade(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);

void              ToggleCheck(HWND hwndListBox, DWORD dwIndex, DWORD dwACFlag);
void              AskCancelDlg(HWND hDlg);
void              lbAddItem(HWND hList, siC *siCComponent);
HWND              InstantiateDialog(HWND hParent, DWORD dwDlgID, LPSTR szTitle, WNDPROC wpDlgProc);
void              DlgSequenceNext(void);
void              DlgSequencePrev(void);
void              PaintGradientShade(HWND hWnd, HDC hdc);
BOOL              BrowseForDirectory(HWND hDlg, char *szCurrDir);
LRESULT CALLBACK  BrowseHookProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void              ShowMessage(LPSTR szMessage, BOOL bShow);
void              DrawCheck(LPDRAWITEMSTRUCT lpdis, DWORD dwACFlag);
void              InvalidateLBCheckbox(HWND hwndListBox);
void              ProcessWindowsMessages(void);
BOOL              CheckWizardStateCustom(DWORD dwDefault);
void              SunJavaDependencyHack(DWORD dwIndex, BOOL bSelected, DWORD dwACFlag);
LPSTR             GetStartInstallMessage(void);
void              AppendStringWOAmpersand(LPSTR szInputString, DWORD dwInputStringSize, LPSTR szString);
void              TruncateString(HWND hWnd, LPSTR szInPath, DWORD dwInPathBufSize, LPSTR szOutPath, DWORD dwOutPathBufSize);
void              SaveDownloadOptions(HWND hDlg, HWND hwndCBSiteSelector);
WNDPROC           SubclassWindow( HWND hWnd, WNDPROC NewWndProc);
LRESULT CALLBACK  ListBoxBrowseWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif
