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
 *     IBM Corp.  
 */

#ifndef _DIALOGS_H_
#define _DIALOGS_H_

#define BTN_UNCHECKED 0
#define BTN_CHECKED 1
#define BTN_INDETERMINATE 2

#ifndef OPENFILENAME
/* OPENFILENAME struct flags
 */

#define OFN_READONLY                 0x00000001
#define OFN_OVERWRITEPROMPT          0x00000002
#define OFN_HIDEREADONLY             0x00000004
#define OFN_NOCHANGEDIR              0x00000008
#define OFN_SHOWHELP                 0x00000010
#define OFN_ENABLEHOOK               0x00000020
#define OFN_ENABLETEMPLATE           0x00000040
#define OFN_ENABLETEMPLATEHANDLE     0x00000080
#define OFN_NOVALIDATE               0x00000100
#define OFN_ALLOWMULTISELECT         0x00000200
#define OFN_EXTENSIONDIFFERENT       0x00000400
#define OFN_PATHMUSTEXIST            0x00000800
#define OFN_FILEMUSTEXIST            0x00001000
#define OFN_CREATEPROMPT             0x00002000
#define OFN_SHAREAWARE               0x00004000
#define OFN_NOREADONLYRETURN         0x00008000
#define OFN_NOTESTFILECREATE         0x00010000
#define OFN_NONETWORKBUTTON          0x00020000
#define OFN_NOLONGNAMES              0x00040000

typedef struct _tagOFN {
   ULONG                             lStructSize;
   HWND                              hwndOwner;
   HMODULE                           hInstance;
   PCSZ                              lpstrFilter;
   PSZ                               lpstrCustomFilter;
   ULONG                             nMaxCustFilter;
   ULONG                             nFilterIndex;
   PSZ                               lpstrFile;
   ULONG                             nMaxFile;
   PSZ                               lpstrFileTitle;
   ULONG                             nMaxFileTitle;
   PCSZ                              lpstrInitialDir;
   PCSZ                              lpstrTitle;
   ULONG                             Flags;
   USHORT                            nFileOffset;
   USHORT                            nFileExtension;
   PCSZ                              lpstrDefExt;
   ULONG                             lCustData;
   PFN                               lpfnHook;
   PCSZ                              lpTemplateName;
} OPENFILENAME, *POPENFILENAME, *LPOPENFILENAME;

//extern "C" BOOL APIENTRY DaxOpenSave(BOOL, LONG *, LPOPENFILENAME, PFNWP);
#endif


MRESULT EXPENTRY  DlgProcMain(HWND hWnd, ULONG msg, MPARAM wParam, MPARAM lParam);
MRESULT EXPENTRY  DlgProcWelcome(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcLicense(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcSetupType(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcSelectComponents(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcSelectAdditionalComponents(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcWindowsIntegration(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcProgramFolder(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcDownloadOptions(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcAdvancedSettings(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcSiteSelector(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcStartInstall(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcReboot(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  DlgProcMessage(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);
MRESULT EXPENTRY  NewListBoxWndProc( HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY  DlgProcUpgrade(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam);

void              ToggleCheck(HWND hwndListBox, ULONG dwIndex, ULONG dwACFlag);
BOOL              AskCancelDlg(HWND hDlg);
void              lbAddItem(HWND hList, siC *siCComponent);
HWND              InstantiateDialog(HWND hParent, ULONG dwDlgID, PSZ szTitle, PFNWP wpDlgProc);
void              DlgSequenceNext(void);
void              DlgSequencePrev(void);
void              PaintGradientShade(HWND hWnd, HDC hdc);
BOOL              BrowseForDirectory(HWND hDlg, char *szCurrDir);
BOOL            GetTextExtentPoint32(HPS aPS, const char* aString, int aLength, PSIZEL aSizeL);
MRESULT EXPENTRY  BrowseHookProc(HWND hwnd, ULONG message, MPARAM wParam, MPARAM lParam);
void              ShowMessage(PSZ szMessage, BOOL bShow);
void              DrawCheck(POWNERITEM lpdis, ULONG dwACFlag);
void              InvalidateLBCheckbox(HWND hwndListBox);
void              ProcessWindowsMessages(void);
BOOL              CheckWizardStateCustom(ULONG dwDefault);
PSZ             GetStartInstallMessage(void);
void              AppendStringWOAmpersand(PSZ szInputString, ULONG dwInputStringSize, PSZ szString);
void              TruncateString(HWND hWnd, PSZ szInPath, PSZ szOutPath, ULONG dwOutPathBufSize);
void              SaveDownloadOptions(HWND hDlg, HWND hwndCBSiteSelector);
PFNWP           SubclassWindow( HWND hWnd, PFNWP NewWndProc);
MRESULT EXPENTRY  ListBoxBrowseWndProc(HWND hWnd, ULONG uMsg, MPARAM wParam, MPARAM lParam);
void              DisableSystemMenuItems(HWND hWnd, BOOL bDisableClose);
BOOL ExtTextOut(HPS aPS, int X, int Y, UINT fuOptions, const RECTL* lprc,
                const char* aString, unsigned int aLength, const int* pDx);
PSZ             myGetSysFont();

#endif /* _DIALOGS_H_ */

