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
 *     Curt Patrick <curt@netscape.com>
 *
 *  Next Generation Apps Version:
 *     Ben Goodger <ben@mozilla.org>
 */

#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"
#include "xpistub.h"
#include "xpi.h"
#include "logging.h"
#include <shlobj.h>
#include <shellapi.h>
#include <objidl.h>
#include <logkeys.h>

// commdlg.h is needed to build with WIN32_LEAN_AND_MEAN
#include <commdlg.h>

static WNDPROC OldListBoxWndProc;
static BOOL    gbProcessingXpnstallFiles;
static DWORD   gdwACFlag;
static DWORD   gdwIndexLastSelected;

/* List of Dialog Item IDs from the Download dialog
 * that need to be repositioned (up) when the banner
 * in the dialog is hidden.
 */
const int DownloadDlgItemList[] = {IDPAUSE,
                                   IDRESUME,
                                   IDCANCEL,
                                   IDC_MESSAGE0,
                                   IDC_STATIC3,
                                   IDC_STATUS_URL,
                                   IDC_STATIC1,
                                   IDC_STATUS_STATUS,
                                   IDC_STATIC2,
                                   IDC_STATUS_FILE,
                                   IDC_PERCENTAGE,
                                   IDC_STATIC4,
                                   IDC_STATUS_TO,
                                   -2};  /* -1 is used by IDC_STATIC.  Even though
                                          * there shouldn't be any IDC_STATIC in
                                          * list, we shouldn't use it.
                                          */

/* List of Dialog Item IDs from the Install Progress dialog
 * that need to be repositioned (up) when the banner
 * in the dialog is hidden.
*/
const int InstallProgressDlgItemList[] = {IDC_STATUS0,
                                          IDC_STATUS3,
                                          -2};  /* -1 is used by IDC_STATIC.  Even though
                                                 * there shouldn't be any IDC_STATIC in
                                                 * list, we shouldn't use it.
                                                 */

BOOL AskCancelDlg(HWND hDlg)
{
  char szDlgQuitTitle[MAX_BUF];
  char szDlgQuitMsg[MAX_BUF];
  char szMsg[MAX_BUF];
  BOOL bRv = FALSE;

  if((sgProduct.mode != SILENT) && (sgProduct.mode != AUTO))
  {
    if(!GetPrivateProfileString("Messages", "DLGQUITTITLE", "", szDlgQuitTitle, sizeof(szDlgQuitTitle), szFileIniInstall))
      PostQuitMessage(1);
    else if(!GetPrivateProfileString("Messages", "DLGQUITMSG", "", szDlgQuitMsg, sizeof(szDlgQuitMsg), szFileIniInstall))
      PostQuitMessage(1);
    else if(MessageBox(hDlg, szDlgQuitMsg, szDlgQuitTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_APPLMODAL | MB_SETFOREGROUND) == IDYES)
    {
      DestroyWindow(hDlg);
      PostQuitMessage(0);
      bRv = TRUE;
    }
  }
  else
  {
    GetPrivateProfileString("Strings", "Message Cancel Setup AUTO mode", "", szMsg, sizeof(szMsg), szFileIniConfig);
    ShowMessage(szMsg, TRUE);
    Delay(5);
    ShowMessage(szMsg, FALSE);
    bRv = TRUE;
  }

  return(bRv);
} 

void DisableSystemMenuItems(HWND hWnd, BOOL bDisableClose)
{
  EnableMenuItem(GetSystemMenu(hWnd, FALSE), SC_RESTORE,  MF_BYCOMMAND | MF_GRAYED);
  EnableMenuItem(GetSystemMenu(hWnd, FALSE), SC_SIZE,     MF_BYCOMMAND | MF_GRAYED);
  EnableMenuItem(GetSystemMenu(hWnd, FALSE), SC_MAXIMIZE, MF_BYCOMMAND | MF_GRAYED);

  if(bDisableClose)
    EnableMenuItem(GetSystemMenu(hWnd, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
}

/* Function: MoveDlgItem()
 *
 *       in: HWND  aWndDlg     : handle to a dialog containing items in aIDList.
 *           const int *aIDList: list of dlg item IDs that require moving.
 *           DWORD aWidth      : width to move the dlg items by (+/-).
 *           DWORD aHeight     : height to move the dlg items by (+/-).
 *           
 *  purpose: To move dialog items (given a list of item ids) +aWidth/+aHeight from
 *           its current position.
 *           This is for when the banner logo in the download/install process
 *           dialogs are not to be displayed, it leaves an empty area above
 *           the dialog items/controls.  So this helps move them up by the
 *           height of the banner.
 *           The resizing of the window given the lack of the banner is done
 *           RepositionWindow().
 */
void MoveDlgItem(HWND aWndDlg, const int *aIDList, DWORD aWidth, DWORD aHeight)
{
  RECT rect;
  HWND hDlgItem;
  int i;
  int id;

  i  = 0;
  id = aIDList[i];
  while(id != -2)
  {
    hDlgItem = GetDlgItem(aWndDlg, id);
    if(hDlgItem)
    {
      GetWindowRect(hDlgItem, &rect);
      SetWindowPos(hDlgItem, NULL, rect.left + aWidth, rect.top + aHeight,
                   -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    id = aIDList[++i];
  }
}

/* Function: RepositionWindow()
 *
 *       in: HWND  aHwndDlg    : Window handle to reposition.
 *           DWORD aBannerImage: Integer indicating which dialog needs to have
 *                               it's dlg items moved.
 *                               There are only 3 types:
 *                                 NO_BANNER_IMAGE
 *                                 BANNER_IMAGE_DOWNLOAD
 *                                 BANNER_IMAGE_INSTALLING
 *
 *  purpose: To reposition a window given the screen position of the previous
 *           window.  The previous window position is saved in:
 *             gSystemInfo.lastWindowPosCenterX
 *             gSystemInfo.lastWindowPosCenterY
 *
 *           aHwndDlg is the window handle to the dialog to reposition.
 *           aBannerImage is a DWORD value that indicates which dialog
 *           the banner is displayed in.  There are only two possible dialogs:
 *             Download dialog
 *             Install dialog
 *
 *           This function also hides the banner image normally displayed in
 *           the Download and Install Process dialgs.  Once hidden, it also
 *           moves all of their dialog items up by the height of the hidden
 *           banner image and resizes the dialogs so it'll look nice.
 */
void RepositionWindow(HWND aHwndDlg, DWORD aBannerImage)
{
  RECT  rect;
  int   iLeft, iTop;
  DWORD width = -1;
  DWORD height = -1;
  DWORD windowFlags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;

  GetWindowRect(aHwndDlg, &rect);
  if(aBannerImage && !gShowBannerImage)
  {
    RECT  rLogo;
    HWND  hwndBanner;

    hwndBanner = GetDlgItem(aHwndDlg, IDB_BITMAP_BANNER);
    GetWindowRect(hwndBanner, &rLogo);
    ShowWindow(hwndBanner, SW_HIDE);
    width = rect.right;
    height = rect.bottom - rLogo.bottom + rLogo.top;
    windowFlags = SWP_NOZORDER | SWP_NOACTIVATE;

    /* aBannerImage indicates which dialog we need to move it's dlg items
     * up to fit the resized window.
     */
    switch(aBannerImage)
    {
      case BANNER_IMAGE_DOWNLOAD:
        MoveDlgItem(aHwndDlg, DownloadDlgItemList, 0, -rLogo.bottom);
        break;

      case BANNER_IMAGE_INSTALLING:
        MoveDlgItem(aHwndDlg, InstallProgressDlgItemList, 0, -rLogo.bottom);
        break;

      default:
        break;
    }
  }

  iLeft = (gSystemInfo.lastWindowPosCenterX - ((rect.right - rect.left) / 2));
  iTop  = (gSystemInfo.lastWindowPosCenterY - ((rect.bottom - rect.top) / 2));
  SetWindowPos(aHwndDlg, NULL, iLeft, iTop, width, height, windowFlags);
}

/* Function: SaveWindowPosition()
 *
 *       in: HWND aDlg: Window handle to remember the position of.
 *
 *  purpose: Saves the current window's position so that it can be
 *           used to position the next window created.
 */
void SaveWindowPosition(HWND aDlg)
{
  RECT rectDlg;

  if(GetWindowRect(aDlg, &rectDlg))
  {
    gSystemInfo.lastWindowPosCenterX = ((rectDlg.right - rectDlg.left) / 2) + rectDlg.left;
    gSystemInfo.lastWindowPosCenterY = ((rectDlg.bottom - rectDlg.top) / 2) + rectDlg.top;
  }
}

///////////////////////////////////////////////////////////////////////////////
// DIALOG: WELCOME
//

LRESULT CALLBACK DlgProcWelcome(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  char szBuf[MAX_BUF];
  LPNMHDR notifyMessage;
  HWND hControl;
  
  switch(msg) {
  case WM_INITDIALOG:
    // The header on the welcome page uses a larger font.
    hControl = GetDlgItem(hDlg, IDC_STATIC_TITLE);
    SendMessage(hControl, WM_SETFONT, (WPARAM)sgInstallGui.welcomeTitleFont, (LPARAM)TRUE);

    // UI Text, from localized config files
    wsprintf(szBuf, diWelcome.szMessageWelcome, sgProduct.szProductName);
    SetDlgItemText(hDlg, IDC_STATIC_TITLE, szBuf);
    wsprintf(szBuf, diWelcome.szMessage0, sgProduct.szProductName);
    SetDlgItemText(hDlg, IDC_STATIC0, szBuf);
    SetDlgItemText(hDlg, IDC_STATIC1, diWelcome.szMessage1);
    SetDlgItemText(hDlg, IDC_STATIC2, diWelcome.szMessage2);
    wsprintf(szBuf, diWelcome.szMessage3, sgProduct.szProductName);
    SetDlgItemText(hDlg, IDC_STATIC3, szBuf);

    break;

  case WM_NOTIFY:
    notifyMessage = (LPNMHDR)lParam;
    switch (notifyMessage->code) {
    case PSN_SETACTIVE:
      // Wizard dialog title
      PropSheet_SetTitle(GetParent(hDlg), 0, (LPTSTR)diWelcome.szTitle); 

      PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_NEXT);
      break;
    }
    break;
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// DIALOG: LICENSE
//

LRESULT CALLBACK DlgProcLicense(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  char            szBuf[MAX_BUF];
  LPSTR           szLicenseFilenameBuf = NULL;
  WIN32_FIND_DATA wfdFindFileData;
  DWORD           dwFileSize;
  DWORD           dwBytesRead;
  HANDLE          hFLicense;
  FILE            *fLicense;
  LPNMHDR         notifyMessage;
 
  switch(msg) {
  case WM_INITDIALOG:
    SetDlgItemText(hDlg, IDC_MESSAGE0, diLicense.szMessage0);
    SetDlgItemText(hDlg, IDC_RADIO_ACCEPT, diLicense.szRadioAccept);
    SetDlgItemText(hDlg, IDC_RADIO_DECLINE, diLicense.szRadioDecline);

    // Check the "Accept" Radio button by default. 
    CheckDlgButton(hDlg, IDC_RADIO_ACCEPT, BST_CHECKED);
    SendMessage(GetDlgItem(hDlg, IDC_RADIO_ACCEPT), BM_SETCHECK, BST_CHECKED, 0);

    // License Text
    lstrcpy(szBuf, szSetupDir);
    AppendBackSlash(szBuf, sizeof(szBuf));
    lstrcat(szBuf, diLicense.szLicenseFilename);

    if((hFLicense = FindFirstFile(szBuf, &wfdFindFileData)) != INVALID_HANDLE_VALUE)
    {
      dwFileSize = (wfdFindFileData.nFileSizeHigh * MAXDWORD) + wfdFindFileData.nFileSizeLow + 1;
      FindClose(hFLicense);
      if((szLicenseFilenameBuf = NS_GlobalAlloc(dwFileSize)) != NULL)
      {
        if((fLicense = fopen(szBuf, "rb")) != NULL)
        {
          dwBytesRead = fread(szLicenseFilenameBuf, sizeof(char), dwFileSize, fLicense);
          fclose(fLicense);
          SetDlgItemText(hDlg, IDC_EDIT_LICENSE, szLicenseFilenameBuf);
        }

        FreeMemory(&szLicenseFilenameBuf);
      }
    }
    break;

  case WM_COMMAND:
    switch (wParam) {
    case IDC_RADIO_ACCEPT:
      PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_NEXT|PSWIZB_BACK);
      break;
    case IDC_RADIO_DECLINE:
      PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_BACK);
      break;
    }
    break;

  case WM_NOTIFY:
    notifyMessage = (LPNMHDR)lParam;
    switch (notifyMessage->code) {
    case PSN_SETACTIVE:
      // Wizard dialog title
      PropSheet_SetTitle(GetParent(hDlg), 0, (LPTSTR)diLicense.szTitle); 

      PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_NEXT|PSWIZB_BACK);
      break;
    }
    break;
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// DIALOG: SETUP TYPE
//

LRESULT CALLBACK DlgProcSetupType(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  HWND    hRadioSt0, hStaticSt0, hRadioSt1, hStaticSt1;
  LPNMHDR notifyMessage;
  BOOL    skipNext = FALSE;

  hRadioSt0   = GetDlgItem(hDlg, IDC_RADIO_ST0);
  hStaticSt0  = GetDlgItem(hDlg, IDC_STATIC_ST0_DESCRIPTION);
  hRadioSt1   = GetDlgItem(hDlg, IDC_RADIO_ST1);
  hStaticSt1  = GetDlgItem(hDlg, IDC_STATIC_ST1_DESCRIPTION);

  switch(msg) {
  case WM_INITDIALOG:
    SetDlgItemText(hDlg, IDC_STATIC_MSG0, diSetupType.szMessage0);

    if(diSetupType.stSetupType0.bVisible) {
      SetDlgItemText(hDlg, IDC_RADIO_ST0, diSetupType.stSetupType0.szDescriptionShort);
      SetDlgItemText(hDlg, IDC_STATIC_ST0_DESCRIPTION, diSetupType.stSetupType0.szDescriptionLong);
      ShowWindow(hRadioSt0, SW_SHOW);
      ShowWindow(hStaticSt0, SW_SHOW);
    }
    else {
      ShowWindow(hRadioSt0, SW_HIDE);
      ShowWindow(hStaticSt0, SW_HIDE);
    }

    if(diSetupType.stSetupType1.bVisible) {
      SetDlgItemText(hDlg, IDC_RADIO_ST1, diSetupType.stSetupType1.szDescriptionShort);
      SetDlgItemText(hDlg, IDC_STATIC_ST1_DESCRIPTION, diSetupType.stSetupType1.szDescriptionLong);
      ShowWindow(hRadioSt1, SW_SHOW);
      ShowWindow(hStaticSt1, SW_SHOW);
    }
    else {
      ShowWindow(hRadioSt1, SW_HIDE);
      ShowWindow(hStaticSt1, SW_HIDE);
    }

    break;

  case WM_NOTIFY:
    notifyMessage = (LPNMHDR)lParam;

    switch (notifyMessage->code) {
    case PSN_SETACTIVE:
      // Wizard dialog title
      PropSheet_SetTitle(GetParent(hDlg), 0, (LPTSTR)diSetupType.szTitle); 

      PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_NEXT|PSWIZB_BACK);

      switch(dwTempSetupType) {
      case ST_RADIO0:
        CheckDlgButton(hDlg, IDC_RADIO_ST0, BST_CHECKED);
        SetFocus(hRadioSt0);
        break;

      case ST_RADIO1:
        CheckDlgButton(hDlg, IDC_RADIO_ST1, BST_CHECKED);
        SetFocus(hRadioSt1);
        break;
      }
      break;

    case PSN_WIZNEXT:
    case PSN_WIZBACK:
      // retrieve and save the state of the selected radio button 
      if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST0) == BST_CHECKED)
        dwSetupType = ST_RADIO0;
      else if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST1) == BST_CHECKED)
        dwSetupType = ST_RADIO1;
      SiCNodeSetItemsSelected(dwSetupType);

      dwTempSetupType = dwSetupType;

      // If the user has selected "Standard" (easy install) no further data 
      // collection is required, go right to the end. 
      if (notifyMessage->code == PSN_WIZNEXT && 
          dwSetupType == ST_RADIO0) {
        SetWindowLong(hDlg, DWL_MSGRESULT, DLG_START_INSTALL);
        skipNext = TRUE;
      }
      
      break;
    }

    break;
  }

  return skipNext;
}

///////////////////////////////////////////////////////////////////////////////
// DIALOG: SELECT INSTALL PATH
//

#ifndef BIF_USENEWUI
// BIF_USENEWUI isn't defined in the platform SDK that comes with
// MSVC6.0. 
#define BIF_USENEWUI 0x50
#endif

void BrowseForDirectory(HWND hParent)
{ 
	LPITEMIDLIST  itemIDList;
	BROWSEINFO    browseInfo;
  HWND          hDestinationPath;
  char          currDir[MAX_PATH];
  char          szBuf[MAX_PATH];
  
  GetCurrentDirectory(MAX_PATH, currDir);

	browseInfo.hwndOwner		  = hParent;
	browseInfo.pidlRoot			  = NULL;
	browseInfo.pszDisplayName	= currDir;
	browseInfo.lpszTitle		  = sgInstallGui.szBrowseInfo;
	browseInfo.ulFlags			  = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
	browseInfo.lpfn				    = NULL;
	browseInfo.lParam			    = 0;
	
	// Show the dialog
	itemIDList = SHBrowseForFolder(&browseInfo);

  if (itemIDList) {
    if (SHGetPathFromIDList(itemIDList, currDir)) {
      // Update the displayed path
      hDestinationPath = GetDlgItem(hParent, IDC_EDIT_DESTINATION); /* handle to the static destination path text window */
      TruncateString(hDestinationPath, currDir, szBuf, sizeof(szBuf));
      SetDlgItemText(hParent, IDC_EDIT_DESTINATION, szBuf);

      SetCurrentDirectory(currDir);

      lstrcpy(sgProduct.szPath, currDir);
    }

#if 0
    // XXXben LEAK!!!! but this code won't compile for some unknown reason- 
    // "Free is not a member of IMalloc, see objidl.h for details"
    // but that's a bald-faced lie!
		// The shell allocated an ITEMIDLIST, we need to free it. 
    LPMALLOC shellMalloc;
    if (SUCCEEDED(SHGetMalloc(&shellMalloc))) {
      shellMalloc->Free(itemIDList);
      shellMalloc->Release();
		}
#endif
	}
}

LRESULT CALLBACK DlgProcSelectInstallPath(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  HWND    hDestinationPath;
  char    szBuf[MAX_BUF];
  char    szBufTemp[MAX_BUF];
  LPNMHDR notifyMessage;
  HICON   hSmallFolderIcon, hLargeFolderIcon;

  switch(msg) {
  case WM_INITDIALOG:
    SetDlgItemText(hDlg, IDC_STATIC_MSG0, diSelectInstallPath.szMessage0);

    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_BROWSE), !sgProduct.bLockPath);

    // Folder Icon
    ExtractIconEx("shell32.dll", 3, &hLargeFolderIcon, &hSmallFolderIcon, 1);
    SendMessage(GetDlgItem(hDlg, IDC_FOLDER_ICON), STM_SETICON, (LPARAM)hSmallFolderIcon, 0);
    
    SetCurrentDirectory(szTempSetupPath);

    hDestinationPath = GetDlgItem(hDlg, IDC_EDIT_DESTINATION); /* handle to the static destination path text window */
    TruncateString(hDestinationPath, szTempSetupPath, szBuf, sizeof(szBuf));
    SetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szBuf);

    SetDlgItemText(hDlg, IDC_BUTTON_BROWSE, sgInstallGui.szBrowse_);
    break;

  case WM_COMMAND:
    switch(LOWORD(wParam)) {
    case IDC_BUTTON_BROWSE:
      BrowseForDirectory(hDlg);
      break;
    }
    break;

  case WM_NOTIFY:
    notifyMessage = (LPNMHDR)lParam;

    switch (notifyMessage->code) {
    case PSN_SETACTIVE:
      // Wizard dialog title
      PropSheet_SetTitle(GetParent(hDlg), 0, (LPTSTR)diSelectInstallPath.szTitle); 

      PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_NEXT|PSWIZB_BACK);

      break;

    case PSN_WIZNEXT:
      lstrcpy(sgProduct.szPath, szTempSetupPath);

      /* append a backslash to the path because CreateDirectoriesAll()
         uses a backslash to determine directories */
      lstrcpy(szBuf, sgProduct.szPath);
      AppendBackSlash(szBuf, sizeof(szBuf));

      /* Make sure that the path is not within the windows dir */
      if(IsPathWithinWindir(szBuf)) {
        char errorMsg[MAX_BUF];
        char errorMsgTitle[MAX_BUF];

        GetPrivateProfileString("Messages", "ERROR_PATH_WITHIN_WINDIR",
            "", errorMsg, sizeof(errorMsg), szFileIniInstall);
        GetPrivateProfileString("Messages", "ERROR_MESSAGE_TITLE", "",
            errorMsgTitle, sizeof(errorMsgTitle), szFileIniInstall);
        MessageBox(hDlg, errorMsg, errorMsgTitle, MB_OK | MB_ICONERROR);
        break;
      }

      /* Create the path if it does not exist */
      if(FileExists(szBuf) == FALSE) {
        char szBuf2[MAX_PATH];

        if(CreateDirectoriesAll(szBuf, ADD_TO_UNINSTALL_LOG) != WIZ_OK) {
          char szECreateDirectory[MAX_BUF];
          char szEMessageTitle[MAX_BUF];

          lstrcpy(szBufTemp, "\n\n");
          lstrcat(szBufTemp, sgProduct.szPath);
          RemoveBackSlash(szBufTemp);
          lstrcat(szBufTemp, "\n\n");

          if(GetPrivateProfileString("Messages", "ERROR_CREATE_DIRECTORY", "", szECreateDirectory, sizeof(szECreateDirectory), szFileIniInstall))
            wsprintf(szBuf, szECreateDirectory, szBufTemp);

          GetPrivateProfileString("Messages", "ERROR_MESSAGE_TITLE", "", szEMessageTitle, sizeof(szEMessageTitle), szFileIniInstall);

          MessageBox(hDlg, szBuf, szEMessageTitle, MB_OK | MB_ICONERROR);
          break;
        }

        if(*sgProduct.szSubPath != '\0') {
           /* log the subpath for uninstallation.  This subpath does not normally get logged
            * for uninstallation due to a chicken and egg problem with creating the log file
            * and the directory its in */
          lstrcpy(szBuf2, szBuf);
          AppendBackSlash(szBuf2, sizeof(szBuf2));
          lstrcat(szBuf2, sgProduct.szSubPath);
          UpdateInstallLog(KEY_CREATE_FOLDER, szBuf2, FALSE);
        }

        bCreateDestinationDir = TRUE;
      }

      break;
    }

    break;
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// DIALOG: UPGRADE CONFIRMATION
//

LRESULT CALLBACK DlgProcUpgrade(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  char buf[MAX_BUF];

  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);

      GetPrivateProfileString("Messages", "MB_WARNING_STR", "",
          buf, sizeof(buf), szFileIniInstall);
      SetWindowText(hDlg, buf);

      GetPrivateProfileString("Strings", "Message Cleanup On Upgrade", "",
          buf, sizeof(buf), szFileIniConfig);
      ReplacePrivateProfileStrCR(buf);
      SetDlgItemText(hDlg, IDC_MESSAGE0, buf);

      GetPrivateProfileString("Strings", "Cleanup On Upgrade Path Box String", "",
          buf, sizeof(buf), szFileIniConfig);
      SetDlgItemText(hDlg, IDC_STATIC, buf);

      MozCopyStr(sgProduct.szPath, buf, sizeof(buf));
      RemoveBackSlash(buf);
      SetDlgItemText(hDlg, IDC_DELETE_PATH, buf);

      RepositionWindow(hDlg, NO_BANNER_IMAGE);

      SetDlgItemText(hDlg, IDCONTINUE, sgInstallGui.szContinue_);
      SetDlgItemText(hDlg, IDSKIP, sgInstallGui.szSkip_);
      SetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      SendDlgItemMessage (hDlg, IDC_STATIC, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDC_DELETE_PATH, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDCONTINUE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDSKIP, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDCONTINUE:
          /* If the installation path happens to be within the %windir%, then
           * show error message and continue without removing the previous
           * installation path. */
          if(IsPathWithinWindir(sgProduct.szPath))
          {
            GetPrivateProfileString("Strings", "Message Cleanup On Upgrade Windir", "",
                buf, sizeof(buf), szFileIniConfig);
            MessageBox(hWndMain, buf, NULL, MB_ICONEXCLAMATION);
          }
          else
            /* set the var to delete target path here */
            sgProduct.doCleanupOnUpgrade = TRUE;

          SiCNodeSetItemsSelected(dwSetupType);
          SaveWindowPosition(hDlg);
          DestroyWindow(hDlg);
          break;

        case IDSKIP:
          sgProduct.doCleanupOnUpgrade = FALSE;
          SiCNodeSetItemsSelected(dwSetupType);
          SaveWindowPosition(hDlg);
          DestroyWindow(hDlg);
          break;

        case IDWIZBACK:
          SaveWindowPosition(hDlg);
          DestroyWindow(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

///////////////////////////////////////////////////////////////////////////////
// DIALOG: SELECT COMPONENTS
//

void DrawLBText(LPDRAWITEMSTRUCT lpdis, DWORD dwACFlag)
{
  siC     *siCTemp  = NULL;
  TCHAR   tchBuffer[MAX_BUF];
  TEXTMETRIC tm;
  DWORD      y;

  siCTemp = SiCNodeGetObject(lpdis->itemID, FALSE, dwACFlag);
  if(siCTemp != NULL)
  {
    SendMessage(lpdis->hwndItem, LB_GETTEXT, lpdis->itemID, (LPARAM)tchBuffer);

    if((lpdis->itemAction & ODA_FOCUS) && (lpdis->itemState & ODS_SELECTED))
    {
      // remove the focus rect on the previous selected item
      DrawFocusRect(lpdis->hDC, &(lpdis->rcItem));
    }

    siCTemp = SiCNodeGetObject(lpdis->itemID, FALSE, dwACFlag);
    if(lpdis->itemAction & ODA_FOCUS)
    {
      if((lpdis->itemState & ODS_SELECTED) &&
        !(lpdis->itemState & ODS_FOCUS))
      {
        if(siCTemp->dwAttributes & SIC_DISABLED)
          SetTextColor(lpdis->hDC,        GetSysColor(COLOR_GRAYTEXT));
        else
        {
          SetTextColor(lpdis->hDC,        GetSysColor(COLOR_WINDOWTEXT));
          SetBkColor(lpdis->hDC,          GetSysColor(COLOR_WINDOW));
        }
      }
      else
      {
        if(siCTemp->dwAttributes & SIC_DISABLED)
          SetTextColor(lpdis->hDC,        GetSysColor(COLOR_GRAYTEXT));
        else
        {
          SetTextColor(lpdis->hDC,        GetSysColor(COLOR_HIGHLIGHTTEXT));
          SetBkColor(lpdis->hDC,          GetSysColor(COLOR_HIGHLIGHT));
        }
      }
    }
    else if(lpdis->itemAction & ODA_DRAWENTIRE)
    {
      if(siCTemp->dwAttributes & SIC_DISABLED)
        SetTextColor(lpdis->hDC, GetSysColor(COLOR_GRAYTEXT));
      else
        SetTextColor(lpdis->hDC, GetSysColor(COLOR_WINDOWTEXT));
    }

    // If a screen reader is being used we want to redraw the text whether
    //   it has focus or not because the text changes whenever the checkbox
    //   changes.
    if( gSystemInfo.bScreenReader || (lpdis->itemAction & (ODA_DRAWENTIRE | ODA_FOCUS)) )
    {
      // Display the text associated with the item.
      GetTextMetrics(lpdis->hDC, &tm);
      y = (lpdis->rcItem.bottom + lpdis->rcItem.top - tm.tmHeight) / 2;

      ExtTextOut(lpdis->hDC,
                 CX_CHECKBOX + 5,
                 y,
                 ETO_OPAQUE | ETO_CLIPPED,
                 &(lpdis->rcItem),
                 tchBuffer,
                 strlen(tchBuffer),
                 NULL);
    }
  }
}

void DrawCheck(LPDRAWITEMSTRUCT lpdis, DWORD dwACFlag)
{
  siC     *siCTemp  = NULL;
  HDC     hdcMem;
  HBITMAP hbmpCheckBox;

  siCTemp = SiCNodeGetObject(lpdis->itemID, FALSE, dwACFlag);
  if(siCTemp != NULL)
  {
    if(!(siCTemp->dwAttributes & SIC_SELECTED))
      /* Component is not selected.  Use the unchecked bitmap regardless if the 
       * component is disabled or not.  The unchecked bitmap looks the same if
       * it's disabled or enabled. */
      hbmpCheckBox = hbmpBoxUnChecked;
    else if(siCTemp->dwAttributes & SIC_DISABLED)
      /* Component is checked and disabled */
      hbmpCheckBox = hbmpBoxCheckedDisabled;
    else
      /* Component is checked and enabled */
      hbmpCheckBox = hbmpBoxChecked;

    SendMessage(lpdis->hwndItem, LB_SETITEMDATA, lpdis->itemID, (LPARAM)hbmpCheckBox);
    if((hdcMem = CreateCompatibleDC(lpdis->hDC)) != NULL)
    {
      SelectObject(hdcMem, hbmpCheckBox);

      // BitBlt() is used to prepare the checkbox icon into the list box item's device context.
      // The SendMessage() function using LB_SETITEMDATA performs the drawing.
      BitBlt(lpdis->hDC,
             lpdis->rcItem.left + 2,
             lpdis->rcItem.top + 2,
             lpdis->rcItem.right - lpdis->rcItem.left,
             lpdis->rcItem.bottom - lpdis->rcItem.top,
             hdcMem,
             0,
             0,
             SRCCOPY);

      DeleteDC(hdcMem);
    }
  }
}

void lbAddItem(HWND hList, siC *siCComponent)
{
  DWORD dwItem;
  TCHAR tchBuffer[MAX_BUF];

  lstrcpy(tchBuffer, siCComponent->szDescriptionShort);
  if(gSystemInfo.bScreenReader)
  {
    lstrcat(tchBuffer, " - ");
    if(!(siCComponent->dwAttributes & SIC_SELECTED))
      lstrcat(tchBuffer, sgInstallGui.szUnchecked);
    else
      lstrcat(tchBuffer, sgInstallGui.szChecked);
  }
  dwItem = SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)tchBuffer);

  if(siCComponent->dwAttributes & SIC_DISABLED)
    SendMessage(hList, LB_SETITEMDATA, dwItem, (LPARAM)hbmpBoxCheckedDisabled);
  else if(siCComponent->dwAttributes & SIC_SELECTED)
    SendMessage(hList, LB_SETITEMDATA, dwItem, (LPARAM)hbmpBoxChecked);
  else
    SendMessage(hList, LB_SETITEMDATA, dwItem, (LPARAM)hbmpBoxUnChecked);
} 

void InvalidateLBCheckbox(HWND hwndListBox)
{
  RECT rcCheckArea;

  // retrieve the rectangle of all list items to update.
  GetClientRect(hwndListBox, &rcCheckArea);

  // Set the right coordinate of the rectangle to be the same
  //   as the right edge of the bitmap drawn.
  // But if a screen reader is being used we want to redraw the text
  //   as well as the checkbox so we do not set the right coordinate.
  if(!gSystemInfo.bScreenReader)
    rcCheckArea.right = CX_CHECKBOX;

  // It then invalidates the checkbox region to be redrawn.
  // Invalidating the region sends a WM_DRAWITEM message to
  // the dialog, which redraws the region given the
  // node attirbute, in this case it is a bitmap of a
  // checked/unchecked checkbox.
  InvalidateRect(hwndListBox, &rcCheckArea, TRUE);
}
  
void ToggleCheck(HWND hwndListBox, DWORD dwIndex, DWORD dwACFlag)
{
  BOOL  bMoreToResolve;
  LPSTR szToggledReferenceName = NULL;
  DWORD dwAttributes;

  // Checks to see if the checkbox is checked or not checked, and
  // toggles the node attributes appropriately.
  dwAttributes = SiCNodeGetAttributes(dwIndex, FALSE, dwACFlag);
  if(!(dwAttributes & SIC_DISABLED))
  {
    if(dwAttributes & SIC_SELECTED)
    {
      SiCNodeSetAttributes(dwIndex, SIC_SELECTED, FALSE, FALSE, dwACFlag, hwndListBox);

      szToggledReferenceName = SiCNodeGetReferenceName(dwIndex, FALSE, dwACFlag);
      ResolveDependees(szToggledReferenceName, hwndListBox);
    }
    else
    {
      SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE, FALSE, dwACFlag, hwndListBox);
      bMoreToResolve = ResolveDependencies(dwIndex, hwndListBox);

      while(bMoreToResolve)
        bMoreToResolve = ResolveDependencies(-1, hwndListBox);

      szToggledReferenceName = SiCNodeGetReferenceName(dwIndex, FALSE, dwACFlag);
      ResolveDependees(szToggledReferenceName, hwndListBox);
    }

    InvalidateLBCheckbox(hwndListBox);
  }
}

// ************************************************************************
// FUNCTION : SubclassWindow( HWND, WNDPROC )
// PURPOSE  : Subclasses a window procedure
// COMMENTS : Returns the old window procedure
// ************************************************************************
WNDPROC SubclassWindow( HWND hWnd, WNDPROC NewWndProc)
{
  WNDPROC OldWndProc;

  OldWndProc = (WNDPROC)GetWindowLong(hWnd, GWL_WNDPROC);
  SetWindowLong(hWnd, GWL_WNDPROC, (LONG) NewWndProc);

  return OldWndProc;
}

// ************************************************************************
// FUNCTION : NewListBoxWndProc( HWND, UINT, WPARAM, LPARAM )
// PURPOSE  : Processes messages for "LISTBOX" class.
// COMMENTS : Prevents the user from moving the window
//            by dragging the titlebar.
// ************************************************************************
LRESULT CALLBACK NewListBoxWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  DWORD   dwPosX;
  DWORD   dwPosY;
  DWORD   dwIndex;
  
  switch(uMsg)
  {
    case WM_CHAR:
      /* check for the space key */
      if((TCHAR)wParam == 32)
      {
        dwIndex = SendMessage(hWnd,
                              LB_GETCURSEL,
                              0,
                              0);
        ToggleCheck(hWnd, dwIndex, gdwACFlag);
      }
      break;

    case WM_LBUTTONDOWN:
      if(wParam == MK_LBUTTON)
      {
        dwPosX = LOWORD(lParam); // x pos
        dwPosY = HIWORD(lParam); // y pos

        if((dwPosX > 1) && (dwPosX <= CX_CHECKBOX))
        {
          dwIndex = LOWORD(SendMessage(hWnd,
                                       LB_ITEMFROMPOINT,
                                       0,
                                       (LPARAM)MAKELPARAM(dwPosX, dwPosY)));
          ToggleCheck(hWnd, dwIndex, gdwACFlag);
        }
      }
      break;
  }

  return(CallWindowProc(OldListBoxWndProc, hWnd, uMsg, wParam, lParam));
}

LRESULT CALLBACK DlgProcSelectComponents(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  siC                 *siCTemp;
  DWORD               dwIndex;
  DWORD               dwItems = MAX_BUF;
  HWND                hwndLBComponents;
  TCHAR               tchBuffer[MAX_BUF];
  LPDRAWITEMSTRUCT    lpdis;
  ULONGLONG           ullDSBuf;
  char                szBuf[MAX_BUF];

  LPNMHDR             notifyMessage;

  hwndLBComponents  = GetDlgItem(hDlg, IDC_LIST_COMPONENTS);

  switch(msg) {
  case WM_INITDIALOG:
    SetDlgItemText(hDlg, IDC_MESSAGE0, diSelectComponents.szMessage0);

    siCTemp = siComponents;
    if(siCTemp != NULL)
    {
      if((!(siCTemp->dwAttributes & SIC_INVISIBLE)) && (!(siCTemp->dwAttributes & SIC_ADDITIONAL)))
        lbAddItem(hwndLBComponents, siCTemp);

      siCTemp = siCTemp->Next;
      while((siCTemp != siComponents) && (siCTemp != NULL))
      {
        if((!(siCTemp->dwAttributes & SIC_INVISIBLE)) && (!(siCTemp->dwAttributes & SIC_ADDITIONAL)))
          lbAddItem(hwndLBComponents, siCTemp);

        siCTemp = siCTemp->Next;
      }
      SetFocus(hwndLBComponents);
      SendMessage(hwndLBComponents, LB_SETCURSEL, 0, 0);
      SetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(0, FALSE, AC_COMPONENTS));
    }

    SetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szComponents_);
    SetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szDescription);
    SetDlgItemText(hDlg, IDC_STATIC_DOWNLOAD_SIZE, sgInstallGui.szTotalDownloadSize);

    gdwACFlag = AC_COMPONENTS;
    OldListBoxWndProc = SubclassWindow(hwndLBComponents, (WNDPROC)NewListBoxWndProc);
    break;

  case WM_DRAWITEM:
    lpdis = (LPDRAWITEMSTRUCT)lParam;

    // If there are no list box items, skip this message.
    if(lpdis->itemID == -1)
      break;

    DrawLBText(lpdis, AC_COMPONENTS);
    DrawCheck(lpdis, AC_COMPONENTS);

    // draw the focus rect on the selected item
    if((lpdis->itemAction & ODA_FOCUS) &&
       (lpdis->itemState & ODS_FOCUS))
      DrawFocusRect(lpdis->hDC, &(lpdis->rcItem));

    break;

  case WM_COMMAND:
    switch(LOWORD(wParam)) {
    case IDC_LIST_COMPONENTS:
      /* to update the long description for each component the user selected */
      if((dwIndex = SendMessage(hwndLBComponents, LB_GETCURSEL, 0, 0)) != LB_ERR) {
        SetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(dwIndex, FALSE, AC_COMPONENTS));

        // update the disk space required info in the dialog.  It is already
        // in Kilobytes
        ullDSBuf = GetDiskSpaceRequired(DSR_DOWNLOAD_SIZE);
        _ui64toa(ullDSBuf, tchBuffer, 10);
        wsprintf(szBuf, sgInstallGui.szDownloadSize, tchBuffer);
   
        SetDlgItemText(hDlg, IDC_STATIC_DOWNLOAD_SIZE, szBuf);
      }

      break;
    }
    break;

  case WM_NOTIFY:
    notifyMessage = (LPNMHDR)lParam;

    switch (notifyMessage->code) {
    case PSN_SETACTIVE:
      // Wizard dialog title
      PropSheet_SetTitle(GetParent(hDlg), 0, (LPTSTR)diSelectComponents.szTitle); 

      PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_NEXT|PSWIZB_BACK);

      break;
    }

    break;
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// DIALOG: SUMMARY
//

BOOL IsSelectableComponent(siC* aComponent)
{
  return !(aComponent->dwAttributes & SIC_INVISIBLE) && 
         !(aComponent->dwAttributes & SIC_ADDITIONAL);
}

LRESULT CALLBACK DlgProcSummary(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  HWND hInstallFolder;
  LPSTR szMessage = NULL;
  LPNMHDR notifyMessage;
  char szAddtlComps[MAX_BUF];
  char szTemp[MAX_BUF];
  char szFormat[MAX_BUF_TINY];
  BOOL hasAddtlComps;
  BOOL skipNext = FALSE;
  siC *currComponent, *mainComponent;

  switch(msg) {
  case WM_INITDIALOG:
    DisableSystemMenuItems(hDlg, FALSE);
    SetWindowText(hDlg, diStartInstall.szTitle);

    SetDlgItemText(hDlg, IDC_MESSAGE1, diStartInstall.szMessage0);
    SetDlgItemText(hDlg, IDC_MESSAGE2, sgInstallGui.szProxyMessage);
    SetDlgItemText(hDlg, IDC_CONNECTION_SETTINGS, sgInstallGui.szProxyButton);
    SetDlgItemText(hDlg, IDC_INSTALL_FOLDER_LABEL, sgInstallGui.szInstallFolder);

    if ((diAdvancedSettings.bShowDialog == FALSE) || (GetTotalArchivesToDownload() == 0))
      SetDlgItemText(hDlg, IDC_MESSAGE0, diStartInstall.szMessageInstall);
    else
      SetDlgItemText(hDlg, IDC_MESSAGE0, diStartInstall.szMessageDownload);

    break;
  
  case WM_NOTIFY:
    notifyMessage = (LPNMHDR)lParam;

    switch (notifyMessage->code) {
    case PSN_SETACTIVE:
      // Wizard dialog title
      PropSheet_SetTitle(GetParent(hDlg), 0, (LPTSTR)diSelectComponents.szTitle); 

      // Set the user-determined fields here in case they go back and change
      // options - WM_INITDIALOG only gets called once, not every time the
      // panel is displayed.

      *szAddtlComps = '\0';

      currComponent = siComponents;
      do {
        if (!currComponent)
          break;

        if (currComponent->dwAttributes & SIC_MAIN_COMPONENT)
          mainComponent = currComponent;
        else if (IsSelectableComponent(currComponent)) {
          wsprintf(szFormat, "%s\r\n", sgInstallGui.szAddtlCompWrapper);
          wsprintf(szTemp, szFormat, currComponent->szDescriptionShort);
          lstrcat(szAddtlComps, szTemp);

          hasAddtlComps = TRUE;
        }

        currComponent = currComponent->Next;
      }
      while (currComponent && currComponent != siComponents);

      if (hasAddtlComps)
        wsprintf(szTemp, sgInstallGui.szPrimCompOthers, mainComponent->szDescriptionShort);
      else
        wsprintf(szTemp, sgInstallGui.szPrimCompNoOthers, mainComponent->szDescriptionShort);
      SetDlgItemText(hDlg, IDC_PRIMARY_COMPONENT, szTemp);

      SetDlgItemText(hDlg, IDC_OPTIONAL_COMPONENTS, szAddtlComps);

      SetDlgItemText(hDlg, IDC_INSTALL_FOLDER, sgProduct.szPath);
      hInstallFolder = GetDlgItem(hDlg, IDC_INSTALL_FOLDER);
      TruncateString(hInstallFolder, sgProduct.szPath, szTemp, sizeof(szTemp));
      
      PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_NEXT|PSWIZB_BACK);

      break;
      
    case PSN_WIZNEXT:
      SaveUserChanges();

      if (!IsDownloadRequired()) {
        // No download required, bypass the download step and go directly to 
        // the installing stage. 
        SetWindowLong(hDlg, DWL_MSGRESULT, DLG_EXTRACTING);
        skipNext = TRUE;
      }
      break;

    case PSN_WIZBACK:
      // If the user selected Easy Install, go right back to the setup type
      // selection page, bypassing the advanced configuration steps. 
      if (dwSetupType == ST_RADIO0) {
        SetWindowLong(hDlg, DWL_MSGRESULT, DLG_SETUP_TYPE);
        skipNext = TRUE;
      }
      break;

    }
    break;
  case WM_COMMAND:
    switch(LOWORD(wParam)) {
    case IDC_CONNECTION_SETTINGS:
      break;
    }
    break;
  }

  return skipNext;
}

void SaveUserChanges()
{
  char szDestPath[MAX_BUF];
  char szInstallLogFile[MAX_BUF];

  gbProcessingXpnstallFiles = TRUE;

  LogISShared();
  LogISDestinationPath();
  LogISSetupType();
  LogISComponentsSelected();
  LogISComponentsToDownload();
  LogISDiskSpace(gdsnComponentDSRequirement);

  lstrcpy(szDestPath, sgProduct.szPath);
  if(*sgProduct.szSubPath != '\0')
  {
    AppendBackSlash(szDestPath, sizeof(szDestPath));
    lstrcat(szDestPath, sgProduct.szSubPath);
  }
  AppendBackSlash(szDestPath, sizeof(szDestPath));

  // Create the destination path here in case it had not been created,
  // as in the case of silent or auto mode installs 
  if(CreateDirectoriesAll(szDestPath, ADD_TO_UNINSTALL_LOG) != WIZ_OK)
  {
    char buf[MAX_BUF];
    char errorCreateDir[MAX_BUF];
    char pathToShow[MAX_PATH];

    // reformat the path to display so that it'll be readable in the
    // error dialog shown 
    _snprintf(pathToShow, sizeof(pathToShow), "\"%s\" ", szDestPath);
    pathToShow[sizeof(pathToShow) - 1] = '\0';
    if(GetPrivateProfileString("Messages", "ERROR_CREATE_DIRECTORY", "", errorCreateDir, sizeof(errorCreateDir), szFileIniInstall))
      wsprintf(buf, errorCreateDir, pathToShow);
    assert(*buf != '\0');
    PrintError(buf, ERROR_CODE_HIDE);
    PostQuitMessage(1);
    return;
  }

  // Set global var, that determines where the log file is to update, to
  // not use the TEMP dir *before* the FileCopy() calls because we want
  // to log the FileCopy() calls to where the log files were copied to.
  // This is possible because the logging, that is done within the
  // FileCopy() function, is done after the actual copy
  //
  gbILUseTemp = FALSE;

  // copy the install_wizard.log file from the temp\ns_temp dir to
  // the destination dir and use the new destination file to continue
  // logging.
  //
  lstrcpy(szInstallLogFile, szTempDir);
  AppendBackSlash(szInstallLogFile, sizeof(szInstallLogFile));
  lstrcat(szInstallLogFile, FILE_INSTALL_LOG);
  FileCopy(szInstallLogFile, szDestPath, FALSE, FALSE);
  DeleteFile(szInstallLogFile);

  // copy the install_status.log file from the temp\ns_temp dir to
  // the destination dir and use the new destination file to continue
  // logging.
  //
  lstrcpy(szInstallLogFile, szTempDir);
  AppendBackSlash(szInstallLogFile, sizeof(szInstallLogFile));
  lstrcat(szInstallLogFile, FILE_INSTALL_STATUS_LOG);
  FileCopy(szInstallLogFile, szDestPath, FALSE, FALSE);
  DeleteFile(szInstallLogFile);
}

BOOL IsDownloadRequired()
{
  DWORD dwIndex0;
  DWORD dwFileCounter;
  siC   *siCObject = NULL;
  long  result;
  char  szFileIdiGetArchives[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szCorruptedArchiveList[MAX_BUF];
  char  szPartiallyDownloadedFilename[MAX_BUF];

  ZeroMemory(szCorruptedArchiveList, sizeof(szCorruptedArchiveList));
  lstrcpy(szFileIdiGetArchives, szTempDir);
  AppendBackSlash(szFileIdiGetArchives, sizeof(szFileIdiGetArchives));
  lstrcat(szFileIdiGetArchives, FILE_IDI_GETARCHIVES);
  GetSetupCurrentDownloadFile(szPartiallyDownloadedFilename,
                              sizeof(szPartiallyDownloadedFilename));

  gbDownloadTriggered= FALSE;
  result            = WIZ_OK;
  dwIndex0           = 0;
  dwFileCounter      = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  
  while (siCObject) {
    if (siCObject->dwAttributes & SIC_SELECTED) {
      // If a previous unfinished setup was detected, then
      // include the TEMP dir when searching for archives.
      // Only download jars if not already in the local machine.
      // Also if the last file being downloaded should be resumed.
      // The resume detection is done automatically.
      if ((LocateJar(siCObject, NULL, 0, 
                    gbPreviousUnfinishedDownload) == AP_NOT_FOUND) ||
          (lstrcmpi(szPartiallyDownloadedFilename,
                    siCObject->szArchiveName) == 0)) {
        wsprintf(szSection, "File%d", dwFileCounter);

        result = AddArchiveToIdiFile(siCObject,
                                     szSection,
                                     szFileIdiGetArchives);
        if (result)
          return result;

        ++dwFileCounter;
      }
    }

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  // The existence of the getarchives.idi file determines if there are
  // any archives needed to be downloaded.
  return FileExists(szFileIdiGetArchives);
}

void TruncateString(HWND hWnd, LPSTR szInURL, LPSTR szOutString, DWORD dwOutStringBufSize)
{
  HDC           hdcWnd;
  LOGFONT       logFont;
  HFONT         hfontNew;
  HFONT         hfontOld;
  RECT          rWndRect;
  SIZE          sizeString;
  char          *ptr = NULL;
  int           iHalfLen;
  int           iOutStringLen;

  if((DWORD)lstrlen(szInURL) > dwOutStringBufSize)
    return;

  ZeroMemory(szOutString, dwOutStringBufSize);
  lstrcpy(szOutString, szInURL);
  iOutStringLen = lstrlen(szOutString);
  hdcWnd        = GetWindowDC(hWnd);
  GetClientRect(hWnd, &rWndRect);
  SystemParametersInfo(SPI_GETICONTITLELOGFONT,
                       sizeof(logFont),
                       (PVOID)&logFont,
                       0);

  hfontNew = CreateFontIndirect(&logFont);
  if(hfontNew)
  {
    hfontOld = (HFONT)SelectObject(hdcWnd, hfontNew);

    GetTextExtentPoint32(hdcWnd, szOutString, iOutStringLen, &sizeString);
    while(sizeString.cx > rWndRect.right)
    {
      iHalfLen = iOutStringLen / 2;
      if(iHalfLen == 2)
        break;

      ptr = szOutString + iHalfLen;
      memmove(ptr - 1, ptr, lstrlen(ptr) + 1);
      szOutString[iHalfLen - 2] = '.';
      szOutString[iHalfLen - 1] = '.';
      szOutString[iHalfLen]     = '.';
      iOutStringLen = lstrlen(szOutString);
      GetTextExtentPoint32(hdcWnd, szOutString, iOutStringLen, &sizeString);
    }
  }

  SelectObject(hdcWnd, hfontOld);
  DeleteObject(hfontNew);
  ReleaseDC(hWnd, hdcWnd);
}

///////////////////////////////////////////////////////////////////////////////
// DIALOG: CONNECTION SETTINGS
//

void SaveDownloadProtocolOption(HWND hDlg)
{
  if(IsDlgButtonChecked(hDlg, IDC_USE_FTP) == BST_CHECKED)
    diAdditionalOptions.dwUseProtocol = UP_FTP;
  else if(IsDlgButtonChecked(hDlg, IDC_USE_HTTP) == BST_CHECKED)
    diAdditionalOptions.dwUseProtocol = UP_HTTP;
}

LRESULT CALLBACK DlgProcAdvancedSettings(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  char  szBuf[MAX_BUF];

  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diAdvancedSettings.szTitle);
      SetDlgItemText(hDlg, IDC_MESSAGE0,          diAdvancedSettings.szMessage0);
      SetDlgItemText(hDlg, IDC_EDIT_PROXY_SERVER, diAdvancedSettings.szProxyServer);
      SetDlgItemText(hDlg, IDC_EDIT_PROXY_PORT,   diAdvancedSettings.szProxyPort);
      SetDlgItemText(hDlg, IDC_EDIT_PROXY_USER,   diAdvancedSettings.szProxyUser);
      SetDlgItemText(hDlg, IDC_EDIT_PROXY_PASSWD, diAdvancedSettings.szProxyPasswd);

      RepositionWindow(hDlg, NO_BANNER_IMAGE);

      GetPrivateProfileString("Strings", "IDC Use Ftp", "", szBuf, sizeof(szBuf), szFileIniConfig);
      SetDlgItemText(hDlg, IDC_USE_FTP, szBuf);
      GetPrivateProfileString("Strings", "IDC Use Http", "", szBuf, sizeof(szBuf), szFileIniConfig);
      SetDlgItemText(hDlg, IDC_USE_HTTP, szBuf);

      SetDlgItemText(hDlg, IDC_STATIC, sgInstallGui.szProxySettings);
      SetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szServer);
      SetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szPort);
      SetDlgItemText(hDlg, IDC_STATIC3, sgInstallGui.szUserId);
      SetDlgItemText(hDlg, IDC_STATIC4, sgInstallGui.szPassword);
      SetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szOk_);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
      SendDlgItemMessage (hDlg, IDC_STATIC, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC2, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC3, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC4, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_EDIT_PROXY_SERVER, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
      SendDlgItemMessage (hDlg, IDC_EDIT_PROXY_PORT, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
      SendDlgItemMessage (hDlg, IDC_EDIT_PROXY_USER, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
      SendDlgItemMessage (hDlg, IDC_EDIT_PROXY_PASSWD, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
      SendDlgItemMessage (hDlg, IDC_USE_FTP, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_USE_HTTP, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);

      switch(diAdditionalOptions.dwUseProtocol)
      {
        case UP_HTTP:
          CheckDlgButton(hDlg, IDC_USE_FTP,  BST_UNCHECKED);
          CheckDlgButton(hDlg, IDC_USE_HTTP, BST_CHECKED);
          break;

        case UP_FTP:
        default:
          CheckDlgButton(hDlg, IDC_USE_FTP,  BST_CHECKED);
          CheckDlgButton(hDlg, IDC_USE_HTTP, BST_UNCHECKED);
          break;

      }

      if((diAdditionalOptions.bShowProtocols) && (diAdditionalOptions.bUseProtocolSettings))
      {
        ShowWindow(GetDlgItem(hDlg, IDC_USE_FTP),  SW_SHOW);
        ShowWindow(GetDlgItem(hDlg, IDC_USE_HTTP), SW_SHOW);
      }
      else
      {
        ShowWindow(GetDlgItem(hDlg, IDC_USE_FTP),  SW_HIDE);
        ShowWindow(GetDlgItem(hDlg, IDC_USE_HTTP), SW_HIDE);
      }

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          SaveWindowPosition(hDlg);
          /* get the proxy server and port information */
          GetDlgItemText(hDlg, IDC_EDIT_PROXY_SERVER, diAdvancedSettings.szProxyServer, MAX_BUF);
          GetDlgItemText(hDlg, IDC_EDIT_PROXY_PORT,   diAdvancedSettings.szProxyPort,   MAX_BUF);
          GetDlgItemText(hDlg, IDC_EDIT_PROXY_USER,   diAdvancedSettings.szProxyUser,   MAX_BUF);
          GetDlgItemText(hDlg, IDC_EDIT_PROXY_PASSWD, diAdvancedSettings.szProxyPasswd, MAX_BUF);

          SaveDownloadProtocolOption(hDlg);
          DestroyWindow(hDlg);
          break;

        case IDWIZBACK:
        case IDCANCEL:
          SaveWindowPosition(hDlg);
          DestroyWindow(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

///////////////////////////////////////////////////////////////////////////////
// DIALOG: DOWNLOADING FILES
//

LRESULT CALLBACK DlgProcDownloading(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diDownloading.szTitle);

      SetDlgItemText(hDlg, IDC_MESSAGE0, diDownloading.szBlurb);
      SetDlgItemText(hDlg, IDC_STATIC0, diDownloading.szFileNameKey);
      SetDlgItemText(hDlg, IDC_STATIC1, diDownloading.szTimeRemainingKey);
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        default:
          break;
      }
      break;
  }
  return(0);
}

///////////////////////////////////////////////////////////////////////////////
// DIALOG: INSTALLING FILES
//

LRESULT CALLBACK DlgProcInstalling(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  HWND ctrl; 
  LPNMHDR notifyMessage;
  static BOOL initialized = FALSE; 
  
  switch(msg) {
  case WM_INITDIALOG:
    DisableSystemMenuItems(hDlg, FALSE);
    SetWindowText(hDlg, diInstalling.szTitle);

    SetDlgItemText(hDlg, IDC_STATUS0, diInstalling.szStatusFile);
    SetDlgItemText(hDlg, IDC_STATUS3, diInstalling.szStatusComponent);

    ctrl = GetDlgItem(hDlg, IDC_PROGRESS_FILE);
    SendMessage(ctrl, PBM_SETRANGE, 0, 100); 
    SendMessage(ctrl, PBM_SETSTEP, 1, 0); 

    ctrl = GetDlgItem(hDlg, IDC_PROGRESS_ARCHIVE);
    SendMessage(ctrl, PBM_SETRANGE, 0, 100); 
    SendMessage(ctrl, PBM_SETSTEP, 1, 0); 

    break;

  case WM_PAINT:
    if (initialized)
      break;

    initialized = TRUE;

    if (InstallFiles(hDlg)) {
#if WINTEGRATION_PAGE
      if (dwSetupType == ST_RADIO0) 
        PropSheet_SetCurSelByID(GetParent(hDlg), DLG_INSTALL_SUCCESSFUL);
      else
#endif
        PropSheet_SetCurSelByID(GetParent(hDlg), DLG_INSTALL_SUCCESSFUL);

      break;
    }
    else {
      // XXXben TODO: handle error. 
      printf("Files NOT Installed...\n");
    }
    break;

  case WM_NOTIFY:
    notifyMessage = (LPNMHDR)lParam;

    switch (notifyMessage->code) {
    case PSN_SETACTIVE:
      // Wizard dialog title
      PropSheet_SetTitle(GetParent(hDlg), 0, (LPTSTR)diInstalling.szTitle); 

      PropSheet_SetWizButtons(GetParent(hDlg), 0);

      break;
    }
  }
  return 0;
}

BOOL InstallFiles(HWND hDlg)
{
  HRESULT err;
  char szDestPath[MAX_PATH];

  // Clean up old versions of GRE previously installed.
  // These GREs should only be fully uninstalled if they were only
  // being used by the mozilla that we're installing over/ontop of
  // (upgrade scenario).
  // We should only do this type of cleanup if we're about to install'
  // GRE in shared mode.
  //
  // This should only be called when the installer is installing GRE!
  if (IsInstallerProductGRE())
    CleanupOrphanedGREs();

  if (sgProduct.bInstallFiles) {
    // POST_DOWNLOAD process file manipulation functions 
    ProcessFileOpsForAll(T_POST_DOWNLOAD);
    // PRE_XPCOM process file manipulation functions 
    ProcessFileOpsForAll(T_PRE_XPCOM);

    // save the installer files in the local machine 
    if (diAdditionalOptions.bSaveInstaller)
      SaveInstallerFiles();

    if (CheckInstances()) {
      bSDUserCanceled = TRUE;
      CleanupXpcomFile();
      return FALSE;
    }

    // Remove the previous installation of the product here.
    // This should be done before processing the Xpinstall engine.
    if(sgProduct.doCleanupOnUpgrade) {
      SetSetupState(SETUP_STATE_REMOVING_PREV_INST);
      CleanupOnUpgrade();
    }

    if(gbDownloadTriggered || gbPreviousUnfinishedDownload)
      SetSetupState(SETUP_STATE_UNPACK_XPCOM);

    if(ProcessXpinstallEngine() != WIZ_OK) {
      bSDUserCanceled = TRUE;
      CleanupXpcomFile();
      return FALSE;
    }

    if (gbDownloadTriggered || gbPreviousUnfinishedDownload)
      SetSetupState(SETUP_STATE_INSTALL_XPI); // clears and sets new setup state

    // POST_XPCOM process file manipulation functions
    ProcessFileOpsForAll(T_POST_XPCOM);
    // PRE_SMARTUPDATE process file manipulation functions
    ProcessFileOpsForAll(T_PRE_SMARTUPDATE);

    lstrcat(szDestPath, "uninstall\\");
    CreateDirectoriesAll(szDestPath, ADD_TO_UNINSTALL_LOG);

    //XXXben TODO - process this return result!
    err = SmartUpdateJars(hDlg);
  }
  else
    err = WIZ_OK;

  //XXXben TODO: shift this until after the FINISH step in DlgProcInstallSuccess
  if (err == WIZ_OK || err == 999){
    if (sgProduct.bInstallFiles)
      UpdateJSProxyInfo();

    /* POST_SMARTUPDATE process file manipulation functions */
    ProcessFileOpsForAll(T_POST_SMARTUPDATE);

    if (sgProduct.bInstallFiles) {
      /* PRE_LAUNCHAPP process file manipulation functions */
      ProcessFileOpsForAll(T_PRE_LAUNCHAPP);

      LaunchApps();

      // Refresh system icons if necessary
      if (gSystemInfo.bRefreshIcons)
        RefreshIcons();

      UnsetSetupState(); // clear setup state
      ClearWinRegUninstallFileDeletion();
      if (!gbIgnoreProgramFolderX)
        ProcessProgramFolderShowCmd();

      CleanupArgsRegistry();
      CleanupPreviousVersionRegKeys();

      /* POST_LAUNCHAPP process file manipulation functions */
      ProcessFileOpsForAll(T_POST_LAUNCHAPP);
      /* DEPEND_REBOOT process file manipulation functions */
      ProcessFileOpsForAll(T_DEPEND_REBOOT);
    }
  }
  
  CleanupXpcomFile();

  gbProcessingXpnstallFiles = FALSE;

  return TRUE;
}

#if WINTEGRATION_PAGE
///////////////////////////////////////////////////////////////////////////////
// DIALOG: WINTEGRATION
//         Not actually used yet!

LRESULT CALLBACK DlgProcWindowsIntegration(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  LPNMHDR notifyMessage;
  char szBuf[MAX_BUF];

  switch (msg) {
  case WM_INITDIALOG:
    wsprintf(szBuf, diWindowsIntegration.szMessage0, sgProduct.szProductName);
    SetDlgItemText(hDlg, IDC_MESSAGE0, szBuf);

    if (diWindowsIntegration.wiCB0.bEnabled) {
      ShowWindow(GetDlgItem(hDlg, IDC_CHECK0), SW_SHOW);
      SetDlgItemText(hDlg, IDC_CHECK0, diWindowsIntegration.wiCB0.szDescription);
    }
    else
      ShowWindow(GetDlgItem(hDlg, IDC_CHECK0), SW_HIDE);

    if (diWindowsIntegration.wiCB1.bEnabled) {
      ShowWindow(GetDlgItem(hDlg, IDC_CHECK1), SW_SHOW);
      SetDlgItemText(hDlg, IDC_CHECK1, diWindowsIntegration.wiCB1.szDescription);
    }
    else
      ShowWindow(GetDlgItem(hDlg, IDC_CHECK1), SW_HIDE);

    if (diWindowsIntegration.wiCB2.bEnabled) {
      ShowWindow(GetDlgItem(hDlg, IDC_CHECK2), SW_SHOW);
      SetDlgItemText(hDlg, IDC_CHECK2, diWindowsIntegration.wiCB2.szDescription);
    }
    else
      ShowWindow(GetDlgItem(hDlg, IDC_CHECK2), SW_HIDE);

    if (diWindowsIntegration.wiCB3.bEnabled) {
      ShowWindow(GetDlgItem(hDlg, IDC_CHECK3), SW_SHOW);
      wsprintf(szBuf, diWindowsIntegration.wiCB3.szDescription, sgProduct.szProductName);
      SetDlgItemText(hDlg, IDC_CHECK3, szBuf);
    }
    else
      ShowWindow(GetDlgItem(hDlg, IDC_CHECK3), SW_HIDE);

    break;

  case WM_NOTIFY:
    notifyMessage = (LPNMHDR)lParam;
    switch (notifyMessage->code) {
    case PSN_SETACTIVE:
      // Wizard dialog title
      PropSheet_SetTitle(GetParent(hDlg), 0, (LPTSTR)diWindowsIntegration.szTitle); 

      // Restore state from default or cached value. 
      CheckDlgButton(hDlg, IDC_CHECK0, 
                     diWindowsIntegration.wiCB0.bCheckBoxState);
      CheckDlgButton(hDlg, IDC_CHECK1, 
                     diWindowsIntegration.wiCB1.bCheckBoxState);
      CheckDlgButton(hDlg, IDC_CHECK2, 
                     diWindowsIntegration.wiCB2.bCheckBoxState);
      CheckDlgButton(hDlg, IDC_CHECK3, 
                     diWindowsIntegration.wiCB3.bCheckBoxState);

      // Don't show the back button here UNLESS the previous 
      // page was Windows Integration - and that only happens on a custom
      // install. 
      PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_NEXT);
      break;

    case PSN_WIZNEXT:
      diWindowsIntegration.wiCB0.bCheckBoxState = 
        IsDlgButtonChecked(hDlg, IDC_CHECK0) == BST_CHECKED;
      diWindowsIntegration.wiCB1.bCheckBoxState = 
        IsDlgButtonChecked(hDlg, IDC_CHECK0) == BST_CHECKED;
      diWindowsIntegration.wiCB2.bCheckBoxState = 
        IsDlgButtonChecked(hDlg, IDC_CHECK0) == BST_CHECKED;
      diWindowsIntegration.wiCB3.bCheckBoxState = 
        IsDlgButtonChecked(hDlg, IDC_CHECK0) == BST_CHECKED;

      break;
    }
      
    break;
  }

  return 0;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// DIALOG: SUCCESSFUL INSTALL
//

LRESULT CALLBACK DlgProcInstallSuccessful(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  char szBuf[MAX_BUF];
  LPNMHDR notifyMessage;
  HWND ctrl;
  static BOOL launchAppChecked = TRUE;
  
  switch(msg) {
  case WM_INITDIALOG:
    // The header on the welcome page uses a larger font.
    ctrl = GetDlgItem(hDlg, IDC_STATIC_TITLE);
    SendMessage(ctrl, WM_SETFONT, (WPARAM)sgInstallGui.welcomeTitleFont, (LPARAM)TRUE);

    // UI Text, from localized config files
    SetDlgItemText(hDlg, IDC_STATIC_TITLE, diInstallSuccessful.szMessageHeader);
    wsprintf(szBuf, diInstallSuccessful.szMessage0, sgProduct.szProductName);
    SetDlgItemText(hDlg, IDC_STATIC0, szBuf);
    SetDlgItemText(hDlg, IDC_STATIC1, diInstallSuccessful.szMessage1);
    wsprintf(szBuf, diInstallSuccessful.szLaunchApp, sgProduct.szProductName);
    SetDlgItemText(hDlg, IDC_START_APP, szBuf);

    launchAppChecked = diInstallSuccessful.bLaunchAppChecked;

    break;

  case WM_NOTIFY:
    notifyMessage = (LPNMHDR)lParam;
    switch (notifyMessage->code) {
    case PSN_SETACTIVE:
      // Wizard dialog title
      PropSheet_SetTitle(GetParent(hDlg), 0, (LPTSTR)diInstallSuccessful.szTitle); 

      // Restore state from default or cached value. 
      CheckDlgButton(hDlg, IDC_START_APP, 
                     launchAppChecked ? BST_CHECKED : BST_UNCHECKED);

      // Don't show the back button here UNLESS the previous 
      // page was Windows Integration - and that only happens on a custom
      // install. 
      // XXXben we've disabled the wintegration panel for now since its 
      // functionality is duplicated elsewhere.
      PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_FINISH);
      break;

    case PSN_WIZBACK:
      // Store the checkbox state in case the user goes back to the Wintegration
      // page. 
      launchAppChecked = IsDlgButtonChecked(hDlg, IDC_START_APP);
      
      break;

    case PSN_WIZFINISH:
      // Apply settings and close. 
      break;
    }
      
    break;
  }
  
  return 0;
}

LRESULT CALLBACK DlgProcMessage(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  RECT      rDlg;
  HWND      hSTMessage = GetDlgItem(hDlg, IDC_MESSAGE); /* handle to the Static Text message window */
  HDC       hdcSTMessage;
  SIZE      sizeString;
  LOGFONT   logFont;
  HFONT     hfontTmp;
  HFONT     hfontOld;
  char      szBuf[MAX_BUF];
  char      szBuf2[MAX_BUF];

  ZeroMemory(szBuf, sizeof(szBuf));
  ZeroMemory(szBuf2, sizeof(szBuf2));

  switch(msg)
  {
    case WM_INITDIALOG:
      if(GetPrivateProfileString("Messages", "STR_MESSAGEBOX_TITLE", "", szBuf2, sizeof(szBuf2), szFileIniInstall))
      {
        if((sgProduct.szProductName != NULL) && (*sgProduct.szProductName != '\0'))
          wsprintf(szBuf, szBuf2, sgProduct.szProductName);
        else
          wsprintf(szBuf, szBuf2, "");
      }
      else if((sgProduct.szProductName != NULL) && (*sgProduct.szProductName != '\0'))
        lstrcpy(szBuf, sgProduct.szProductName);

      SetWindowText(hDlg, szBuf);
      RepositionWindow(hDlg, NO_BANNER_IMAGE);
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_MESSAGE:
          SaveWindowPosition(hDlg);
          hdcSTMessage = GetWindowDC(hSTMessage);

          SystemParametersInfo(SPI_GETICONTITLELOGFONT,
                               sizeof(logFont),
                               (PVOID)&logFont,
                               0);
          hfontTmp = CreateFontIndirect(&logFont);

          if(hfontTmp)
            hfontOld = SelectObject(hdcSTMessage, hfontTmp);

          GetTextExtentPoint32(hdcSTMessage, (LPSTR)lParam, lstrlen((LPSTR)lParam), &sizeString);
          SelectObject(hdcSTMessage, hfontOld);
          DeleteObject(hfontTmp);
          ReleaseDC(hSTMessage, hdcSTMessage);

          SetWindowPos(hDlg, NULL,
                      (gSystemInfo.lastWindowPosCenterX)-((sizeString.cx + 55)/2),
                      (gSystemInfo.lastWindowPosCenterY)-((sizeString.cy + 50)/2),
                      sizeString.cx + 55,
                      sizeString.cy + 50,
                      SWP_SHOWWINDOW|SWP_NOZORDER);

          if(GetClientRect(hDlg, &rDlg))
            SetWindowPos(hSTMessage,
                         HWND_TOP,
                         rDlg.left,
                         rDlg.top,
                         rDlg.right,
                         rDlg.bottom,
                         SWP_SHOWWINDOW|SWP_NOZORDER);

          SetDlgItemText(hDlg, IDC_MESSAGE, (LPSTR)lParam);
          break;
      }
      break;
  }
  return(0);
}

void ProcessWindowsMessages()
{
  MSG msg;

  while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

void ShowMessage(LPSTR szMessage, BOOL bShow)
{
  if(sgProduct.mode != SILENT)
  {
    if(bShow && szMessage)
    {
      char szBuf[MAX_BUF];
 
      ZeroMemory(szBuf, sizeof(szBuf));
      GetPrivateProfileString("Messages", "MB_MESSAGE_STR", "", szBuf, sizeof(szBuf), szFileIniInstall);
      hDlgMessage = InstantiateDialog(hWndMain, DLG_MESSAGE, szBuf, DlgProcMessage);
      SendMessage(hDlgMessage, WM_COMMAND, IDC_MESSAGE, (LPARAM)szMessage);
      SendDlgItemMessage (hDlgMessage, IDC_MESSAGE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
    }
    else
    {
      SaveWindowPosition(hDlgMessage);
      DestroyWindow(hDlgMessage);
      hDlgMessage = NULL;
    }
  }
}

HWND InstantiateDialog(HWND hParent, DWORD dwDlgID, LPSTR szTitle, WNDPROC wpDlgProc)
{
  char szBuf[MAX_BUF];
  HWND hDlg = NULL;


  if((hDlg = CreateDialog(hSetupRscInst, MAKEINTRESOURCE(dwDlgID), hParent, wpDlgProc)) == NULL)
  {
    char szEDialogCreate[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_DIALOG_CREATE", "", szEDialogCreate, sizeof(szEDialogCreate), szFileIniInstall))
    {
      wsprintf(szBuf, szEDialogCreate, szTitle);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    PostQuitMessage(1);
  }

  return(hDlg);
}

void InitSequence(HINSTANCE hInstance)
{
  // Wizard data structures
  PROPSHEETPAGE psp;
  HPROPSHEETPAGE pages[11] = {0};
  PROPSHEETHEADER psh;
  int count = 0;

  /////////////////////////////////////////////////////////////////////////////
  // Create the Wizard Sequence
  //
  psp.dwSize            = sizeof(psp);
  psp.hInstance         = hSetupRscInst;
  psp.lParam            = 0;

  // Welcome Page
  if (diWelcome.bShowDialog) {
    psp.dwFlags           = PSP_DEFAULT|PSP_HIDEHEADER;
    psp.pfnDlgProc        = DlgProcWelcome;
    psp.pszTemplate       = MAKEINTRESOURCE(DLG_WELCOME);
    
    pages[count++]        = CreatePropertySheetPage(&psp);
  }

  // License Page
  if (diLicense.bShowDialog) {
    psp.dwFlags           = PSP_DEFAULT|PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    psp.pszHeaderTitle    = diLicense.szTitle;
    psp.pszHeaderSubTitle = diLicense.szSubTitle;
    psp.pfnDlgProc        = DlgProcLicense;
    psp.pszTemplate       = MAKEINTRESOURCE(DLG_LICENSE);

    pages[count++]        = CreatePropertySheetPage(&psp);
  }

  // License Page
  if (diSetupType.bShowDialog) {
    psp.dwFlags           = PSP_DEFAULT|PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    psp.pszHeaderTitle    = diSetupType.szTitle;
    psp.pszHeaderSubTitle = diSetupType.szSubTitle;
    psp.pfnDlgProc        = DlgProcSetupType;
    psp.pszTemplate       = MAKEINTRESOURCE(DLG_SETUP_TYPE);

    pages[count++]        = CreatePropertySheetPage(&psp);
  }

  if (diSelectInstallPath.bShowDialog) {
    psp.dwFlags           = PSP_DEFAULT|PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    psp.pszHeaderTitle    = diSelectInstallPath.szTitle;
    psp.pszHeaderSubTitle = diSelectInstallPath.szSubTitle;
    psp.pfnDlgProc        = DlgProcSelectInstallPath;
    psp.pszTemplate       = MAKEINTRESOURCE(DLG_SELECT_INSTALL_PATH);

    pages[count++]        = CreatePropertySheetPage(&psp);
  }

  if (diSelectComponents.bShowDialog) {
    psp.dwFlags           = PSP_DEFAULT|PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    psp.pszHeaderTitle    = diSelectComponents.szTitle;
    psp.pszHeaderSubTitle = diSelectComponents.szSubTitle;
    psp.pfnDlgProc        = DlgProcSelectComponents;
    psp.pszTemplate       = MAKEINTRESOURCE(DLG_SELECT_COMPONENTS);

    pages[count++]        = CreatePropertySheetPage(&psp);
  }

  if (diStartInstall.bShowDialog) {
    psp.dwFlags           = PSP_DEFAULT|PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    psp.pszHeaderTitle    = diStartInstall.szTitle;
    psp.pszHeaderSubTitle = diStartInstall.szSubTitle;
    psp.pfnDlgProc        = DlgProcSummary;
    psp.pszTemplate       = MAKEINTRESOURCE(DLG_START_INSTALL);

    pages[count++]        = CreatePropertySheetPage(&psp);
  }

  if (diDownloading.bShowDialog) {
    psp.dwFlags           = PSP_DEFAULT|PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    psp.pszHeaderTitle    = diDownloading.szTitle;
    psp.pszHeaderSubTitle = diDownloading.szSubTitle;
    psp.pfnDlgProc        = DlgProcDownloading;
    psp.pszTemplate       = MAKEINTRESOURCE(DLG_DOWNLOADING);

    pages[count++]        = CreatePropertySheetPage(&psp);
  }

  if (diInstalling.bShowDialog) {
    psp.dwFlags           = PSP_DEFAULT|PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    psp.pszHeaderTitle    = diInstalling.szTitle;
    psp.pszHeaderSubTitle = diInstalling.szSubTitle;
    psp.pfnDlgProc        = DlgProcInstalling;
    psp.pszTemplate       = MAKEINTRESOURCE(DLG_EXTRACTING);

    pages[count++]        = CreatePropertySheetPage(&psp);
  }

#if WINTEGRATION_PAGE
  // Windows Integration Page
  if (diWindowsIntegration.bShowDialog) {
    psp.dwFlags           = PSP_DEFAULT|PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    psp.pszHeaderTitle    = diWindowsIntegration.szTitle;
    psp.pszHeaderSubTitle = diWindowsIntegration.szSubTitle;
    psp.pfnDlgProc        = DlgProcWindowsIntegration;
    psp.pszTemplate       = MAKEINTRESOURCE(DLG_WINDOWS_INTEGRATION);
    
    pages[count++]        = CreatePropertySheetPage(&psp);
  }
#endif

  // Successful Install Page
  if (diInstallSuccessful.bShowDialog) {
    psp.dwFlags           = PSP_DEFAULT|PSP_HIDEHEADER;
    psp.pfnDlgProc        = DlgProcInstallSuccessful;
    psp.pszTemplate       = MAKEINTRESOURCE(DLG_INSTALL_SUCCESSFUL);
    
    pages[count++]        = CreatePropertySheetPage(&psp);
  }

  // Property Sheet
  psh.dwSize            = sizeof(psh);
  psh.hInstance         = hSetupRscInst;
  psh.hwndParent        = NULL;
  psh.phpage            = pages;
  psh.dwFlags           = PSH_WIZARD97|PSH_WATERMARK|PSH_HEADER;
  psh.pszbmWatermark    = MAKEINTRESOURCE(IDB_WATERMARK);
  psh.pszbmHeader       = MAKEINTRESOURCE(IDB_HEADER);
  psh.nStartPage        = 0;
  psh.nPages            = count;


  // Create the Font for Intro/End page headers.
  sgInstallGui.welcomeTitleFont = MakeFont(TEXT("Trebuchet MS Bold"), 14, FW_BOLD);

  // Start the Wizard.
  PropertySheet(&psh);

  DeleteObject(sgInstallGui.welcomeTitleFont);
}

HFONT MakeFont(TCHAR* aFaceName, int aFontSize, LONG aWeight) 
{
  // Welcome Page Header font data
  NONCLIENTMETRICS ncm = {0};
  LOGFONT lf;
  HDC hDC;

  ncm.cbSize = sizeof(ncm);
  SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0);

  if (aFaceName) 
    lstrcpy(lf.lfFaceName, aFaceName); 

  lf = ncm.lfMessageFont;
  lf.lfWeight = aWeight;

  hDC = GetDC(NULL); 
  lf.lfHeight = 0 - GetDeviceCaps(hDC, LOGPIXELSY) * aFontSize / 72;
  ReleaseDC(NULL, hDC);

  return CreateFontIndirect(&lf);
}

#if 0
void CommitInstall(void)
{
  HRESULT hrErr;
  char    szDestPath[MAX_BUF];
  char    szInstallLogFile[MAX_BUF];
  long    RetrieveResults;

  LogISShared();
  LogISDestinationPath();
  LogISSetupType();
  LogISComponentsSelected();
  LogISComponentsToDownload();
  LogISDiskSpace(gdsnComponentDSRequirement);

  lstrcpy(szDestPath, sgProduct.szPath);
  if(*sgProduct.szSubPath != '\0')
  {
    AppendBackSlash(szDestPath, sizeof(szDestPath));
    lstrcat(szDestPath, sgProduct.szSubPath);
  }
  AppendBackSlash(szDestPath, sizeof(szDestPath));

  /* Create the destination path here in case it had not been created,
   * as in the case of silent or auto mode installs */
  if(CreateDirectoriesAll(szDestPath, ADD_TO_UNINSTALL_LOG) != WIZ_OK)
  {
    char buf[MAX_BUF];
    char errorCreateDir[MAX_BUF];
    char pathToShow[MAX_PATH];

    /* reformat the path to display so that it'll be readable in the
     * error dialog shown */
    _snprintf(pathToShow, sizeof(pathToShow), "\"%s\" ", szDestPath);
    pathToShow[sizeof(pathToShow) - 1] = '\0';
    if(GetPrivateProfileString("Messages", "ERROR_CREATE_DIRECTORY", "", errorCreateDir, sizeof(errorCreateDir), szFileIniInstall))
      wsprintf(buf, errorCreateDir, pathToShow);
    assert(*buf != '\0');
    PrintError(buf, ERROR_CODE_HIDE);
    PostQuitMessage(1);
    return;
  }

  /* Set global var, that determines where the log file is to update, to
   * not use the TEMP dir *before* the FileCopy() calls because we want
   * to log the FileCopy() calls to where the log files were copied to.
   * This is possible because the logging, that is done within the
   * FileCopy() function, is done after the actual copy
   */
  gbILUseTemp = FALSE;

  /* copy the install_wizard.log file from the temp\ns_temp dir to
   * the destination dir and use the new destination file to continue
   * logging.
   */
  lstrcpy(szInstallLogFile, szTempDir);
  AppendBackSlash(szInstallLogFile, sizeof(szInstallLogFile));
  lstrcat(szInstallLogFile, FILE_INSTALL_LOG);
  FileCopy(szInstallLogFile, szDestPath, FALSE, FALSE);
  DeleteFile(szInstallLogFile);

  /* copy the install_status.log file from the temp\ns_temp dir to
   * the destination dir and use the new destination file to continue
   * logging.
   */
  lstrcpy(szInstallLogFile, szTempDir);
  AppendBackSlash(szInstallLogFile, sizeof(szInstallLogFile));
  lstrcat(szInstallLogFile, FILE_INSTALL_STATUS_LOG);
  FileCopy(szInstallLogFile, szDestPath, FALSE, FALSE);
  DeleteFile(szInstallLogFile);

  /* PRE_DOWNLOAD process file manipulation functions */
  RetrieveResults = WIZ_OK;
  if(sgProduct.bInstallFiles)
  {
    ProcessFileOpsForAll(T_PRE_DOWNLOAD);
    RetrieveResults = RetrieveArchives();
  }

  if(RetrieveResults == WIZ_OK)
  {
    // Clean up old versions of GRE previously installed.
    // These GREs should only be fully uninstalled if they were only
    // being used by the mozilla that we're installing over/ontop of
    // (upgrade scenario).
    // We should only do this type of cleanup if we're about to install'
    // GRE in shared mode.
    //
    // This should only be called when the installer is installing GRE!
    if(IsInstallerProductGRE())
      CleanupOrphanedGREs();

    if(sgProduct.bInstallFiles)
    {
      /* Check to see if Turbo is required.  If so, set the
       * appropriate Windows registry keys */
      SetTurboArgs();

      /* POST_DOWNLOAD process file manipulation functions */
      ProcessFileOpsForAll(T_POST_DOWNLOAD);
      /* PRE_XPCOM process file manipulation functions */
      ProcessFileOpsForAll(T_PRE_XPCOM);

      /* save the installer files in the local machine */
      if(diAdditionalOptions.bSaveInstaller)
        SaveInstallerFiles();

      if(CheckInstances())
      {
        bSDUserCanceled = TRUE;
        CleanupXpcomFile();
        PostQuitMessage(0);

        return;
      }

      /* Remove the previous installation of the product here.
       * This should be done before processing the Xpinstall engine. */
      if(sgProduct.doCleanupOnUpgrade)
      {
        SetSetupState(SETUP_STATE_REMOVING_PREV_INST);
        CleanupOnUpgrade();
      }

      if(gbDownloadTriggered || gbPreviousUnfinishedDownload)
        SetSetupState(SETUP_STATE_UNPACK_XPCOM);

      if(ProcessXpinstallEngine() != WIZ_OK)
      {
        bSDUserCanceled = TRUE;
        CleanupXpcomFile();
        PostQuitMessage(0);

        return;
      }

      if(gbDownloadTriggered || gbPreviousUnfinishedDownload)
        SetSetupState(SETUP_STATE_INSTALL_XPI); // clears and sets new setup state

      /* POST_XPCOM process file manipulation functions */
      ProcessFileOpsForAll(T_POST_XPCOM);
      /* PRE_SMARTUPDATE process file manipulation functions */
      ProcessFileOpsForAll(T_PRE_SMARTUPDATE);

      lstrcat(szDestPath, "uninstall\\");
      CreateDirectoriesAll(szDestPath, ADD_TO_UNINSTALL_LOG);
      hrErr = SmartUpdateJars();
    }
    else
      hrErr = WIZ_OK;

    if((hrErr == WIZ_OK) || (hrErr == 999))
    {
      if(sgProduct.bInstallFiles)
        UpdateJSProxyInfo();

      /* POST_SMARTUPDATE process file manipulation functions */
      ProcessFileOpsForAll(T_POST_SMARTUPDATE);

      if(sgProduct.bInstallFiles)
      {
        /* PRE_LAUNCHAPP process file manipulation functions */
        ProcessFileOpsForAll(T_PRE_LAUNCHAPP);

        LaunchApps();

        // Refresh system icons if necessary
        if(gSystemInfo.bRefreshIcons)
          RefreshIcons();

        UnsetSetupState(); // clear setup state
        ClearWinRegUninstallFileDeletion();
        if(!gbIgnoreProgramFolderX)
          ProcessProgramFolderShowCmd();

        CleanupArgsRegistry();
        CleanupPreviousVersionRegKeys();

        /* POST_LAUNCHAPP process file manipulation functions */
        ProcessFileOpsForAll(T_POST_LAUNCHAPP);
        /* DEPEND_REBOOT process file manipulation functions */
        ProcessFileOpsForAll(T_DEPEND_REBOOT);
      }

      CleanupXpcomFile();
      if(NeedReboot())
      {
        LogExitStatus("Reboot");
        if(sgProduct.mode == NORMAL)
          hDlgCurrent = InstantiateDialog(hWndMain, DLG_RESTART, diReboot.szTitle, DlgProcReboot);
        else
          PostQuitMessage(0);
      }
      else
        PostQuitMessage(0);
    }
    else
    {
      CleanupXpcomFile();
      PostQuitMessage(0);
    }
  }
  else
  {
    bSDUserCanceled = TRUE;
    CleanupXpcomFile();
    CleanupArgsRegistry();
    PostQuitMessage(0);
  }
  gbProcessingXpnstallFiles = FALSE;
}
#endif