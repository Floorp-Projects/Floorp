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


#ifdef STUB_INSTALLER
#undef STUB_INSTALLER
#endif


LRESULT CALLBACK  DlgProcMain(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK  DlgProcWelcome(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcLicense(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcSetupType(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcSelectComponents(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcSelectInstallPath(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcUpgrade(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcWindowsIntegration(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
#ifdef STUB_INSTALLER
LRESULT CALLBACK  DlgProcAdvancedSettings(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
#endif
LRESULT CALLBACK  DlgProcSummary(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcDownloading(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcInstalling(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcInstallSuccessful(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  DlgProcMessage(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam);
LRESULT CALLBACK  NewListBoxWndProc( HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK  ListBoxBrowseWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL    IsSelectableComponent(siC* aComponent);
BOOL    IsComponentSelected(siC* aComponent);
HFONT   MakeFont(TCHAR* aFaceName, int aFontSize, LONG aWeight);
void    SaveUserChanges();
BOOL    IsDownloadRequired();
BOOL    InstallFiles(HWND hDlg);
void    InitPathDisplay (HWND aWindow, char* aPath, int aFolderIcon, int aFolderField);
void    CheckForUpgrade(HWND aPanel, int aNextPanel);

void    ToggleCheck(HWND hwndListBox, DWORD dwIndex, DWORD dwACFlag);
BOOL    ShouldExitSetup(HWND hDlg);
void    lbAddItem(HWND hList, siC *siCComponent);
HWND    InstantiateDialog(HWND hParent, DWORD dwDlgID, LPSTR szTitle, WNDPROC wpDlgProc);
void    InitSequence(HINSTANCE hInstance);
void    BrowseForDirectory(HWND hDlg);
void    ShowMessage(LPSTR szMessage, BOOL bShow);
void    DrawLBText(LPDRAWITEMSTRUCT lpdis, DWORD dwACFlag);
void    DrawCheck(LPDRAWITEMSTRUCT lpdis, DWORD dwACFlag);
void    InvalidateLBCheckbox(HWND hwndListBox);
void    ProcessWindowsMessages(void);
LPSTR   GetStartInstallMessage(void);
void    AppendStringWOAmpersand(LPSTR szInputString, DWORD dwInputStringSize, LPSTR szString);
void    TruncateString(HWND hWnd, LPSTR szInPath, LPSTR szOutPath, DWORD dwOutPathBufSize);
WNDPROC SubclassWindow( HWND hWnd, WNDPROC NewWndProc);

#endif /* _DIALOGS_H_ */

