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

///////////////////////////////////////////////////////////////////////////////
// DIALOG: EXIT SETUP
//

BOOL ShouldExitSetup(HWND hDlg)
{
  char szDlgQuitTitle[MAX_BUF];
  char szDlgQuitMsg[MAX_BUF];
  char szMsg[MAX_BUF];
  BOOL rv = FALSE;

  if (sgProduct.mode != SILENT && sgProduct.mode != AUTO) {
    if (!GetPrivateProfileString("Messages", "DLGQUITTITLE", "", szDlgQuitTitle, 
                                 sizeof(szDlgQuitTitle), szFileIniInstall) ||
        !GetPrivateProfileString("Messages", "DLGQUITMSG", "", szDlgQuitMsg, 
                                      sizeof(szDlgQuitMsg), szFileIniInstall))
      PostQuitMessage(1);

    
    rv = MessageBox(GetParent(hDlg), szDlgQuitMsg, szDlgQuitTitle, 
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_APPLMODAL | 
                    MB_SETFOREGROUND) == IDYES;
  }
  else {
    GetPrivateProfileString("Strings", "Message Cancel Setup AUTO mode", "", szMsg, 
                            sizeof(szMsg), szFileIniConfig);
    ShowMessage(szMsg, TRUE);
    Delay(5);
    ShowMessage(szMsg, FALSE);
    rv = TRUE;
  }

  if (!rv) 
    SetWindowLong(hDlg, DWL_MSGRESULT, (LONG)TRUE);

  return rv;
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

    case PSN_QUERYCANCEL:
      return !ShouldExitSetup(hDlg);
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

      if (IsDlgButtonChecked(hDlg, IDC_RADIO_DECLINE) == BST_CHECKED) 
        PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_BACK);  
      else
        PropSheet_SetWizButtons(GetParent(hDlg), PSWIZB_NEXT|PSWIZB_BACK);
      
      SendDlgItemMessage(hDlg, IDC_EDIT_LICENSE, EM_SETSEL, 0, 0);

      break;

    case PSN_QUERYCANCEL:
      return !ShouldExitSetup(hDlg);
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

    case PSN_QUERYCANCEL:
      return !ShouldExitSetup(hDlg);
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
      hDestinationPath = GetDlgItem(hParent, IDC_EDIT_DESTINATION);
      TruncateString(hDestinationPath, currDir, szBuf, sizeof(szBuf));
      SetDlgItemText(hParent, IDC_EDIT_DESTINATION, szBuf);

      SetCurrentDirectory(currDir);

      lstrcpy(szTempSetupPath, currDir);
    }

#if 0
    // XXXben LEAK!!!! but this code won't compile for some unknown reason- 
    // "Free is not a member of IMalloc, see objidl.h for details"
    // but that's a bald-faced lie!
		// The shell allocated an ITEMIDLIST, we need to free it. 
    // I guess we'll just leak shell objects for now :-D
    LPMALLOC shellMalloc;
    if (SUCCEEDED(SHGetMalloc(&shellMalloc))) {
      shellMalloc->Free(itemIDList);
      shellMalloc->Release();
		}
#endif
	}
}

void RecoverFromPathError(HWND aPanel)
{
  HWND destinationPath;
  char buf[MAX_BUF];

  // Reset the displayed path to the previous, valid value. 
  destinationPath = GetDlgItem(aPanel, IDC_EDIT_DESTINATION); 
  TruncateString(destinationPath, sgProduct.szPath, buf, sizeof(buf));
  SetDlgItemText(aPanel, IDC_EDIT_DESTINATION, buf);

  // Reset the temporary path string so we don't get stuck receiving 
  // the error message.
  lstrcpy(szTempSetupPath, sgProduct.szPath);

  // Prevent the Wizard from advancing because of this error. 
  SetWindowLong(aPanel, DWL_MSGRESULT, -1);
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

    hDestinationPath = GetDlgItem(hDlg, IDC_EDIT_DESTINATION);
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
      /* Make sure that the path is not within the windows dir */
      if(IsPathWithinWindir(szTempSetupPath)) {
        char errorMsg[MAX_BUF];
        char errorMsgTitle[MAX_BUF];

        GetPrivateProfileString("Messages", "ERROR_PATH_WITHIN_WINDIR",
            "", errorMsg, sizeof(errorMsg), szFileIniInstall);
        GetPrivateProfileString("Messages", "ERROR_MESSAGE_TITLE", "",
            errorMsgTitle, sizeof(errorMsgTitle), szFileIniInstall);
        MessageBox(hDlg, errorMsg, errorMsgTitle, MB_OK | MB_ICONERROR);

        RecoverFromPathError(hDlg);

        return TRUE;
      }

      lstrcpy(sgProduct.szPath, szTempSetupPath);
      /* append a backslash to the path because CreateDirectoriesAll()
         uses a backslash to determine directories */
      lstrcpy(szBuf, sgProduct.szPath);
      AppendBackSlash(szBuf, sizeof(szBuf));

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
          
          RecoverFromPathError(hDlg);

          return TRUE;
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

    case PSN_QUERYCANCEL:
      return !ShouldExitSetup(hDlg);
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
          DestroyWindow(hDlg);
          break;

        case IDSKIP:
          sgProduct.doCleanupOnUpgrade = FALSE;
          SiCNodeSetItemsSelected(dwSetupType);
          DestroyWindow(hDlg);
          break;

        case IDWIZBACK:
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
  LPDRAWITEMSTRUCT    lpdis;
#ifdef STUB_INSTALLER
  TCHAR               tchBuffer[MAX_BUF];
  ULONGLONG           ullDSBuf;
  char                szBuf[MAX_BUF];
#endif

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

#ifdef STUB_INSTALLER
    // XXXben We don't support net stub installs yet. 
    // SetDlgItemText(hDlg, IDC_STATIC_DOWNLOAD_SIZE, sgInstallGui.szTotalDownloadSize);
#endif

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

#ifdef STUB_INSTALLER
        // update the disk space required info in the dialog.  It is already
        // in Kilobytes
        ullDSBuf = GetDiskSpaceRequired(DSR_DOWNLOAD_SIZE);
        _ui64toa(ullDSBuf, tchBuffer, 10);
        wsprintf(szBuf, sgInstallGui.szDownloadSize, tchBuffer);
   
        // XXXben We don't support net stub installs yet. 
        // SetDlgItemText(hDlg, IDC_STATIC_DOWNLOAD_SIZE, szBuf);
#endif
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

    case PSN_QUERYCANCEL:
      return !ShouldExitSetup(hDlg);
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

BOOL IsComponentSelected(siC* aComponent)
{
  return aComponent->dwAttributes & SIC_SELECTED;
}

void GetRelativeRect(HWND aWindow, int aResourceID, RECT* aRect)
{
  HWND ctrl;
  POINT pt;

  ctrl = GetDlgItem(aWindow, aResourceID);

  GetWindowRect(ctrl, aRect);

  pt.x = aRect->left;
  pt.y = aRect->top;
  ScreenToClient(aWindow, &pt);
  aRect->left = pt.x;
  aRect->top = pt.y;

  pt.x = aRect->right;
  pt.y = aRect->bottom;
  ScreenToClient(aWindow, &pt);
  aRect->right = pt.x;
  aRect->bottom = pt.y;
}

void PositionControl(HWND aWindow, int aResourceIDRelative, int aResourceIDControl, int aOffset)
{
  HWND ctrl;
  RECT r1, r2;

  GetRelativeRect(aWindow, aResourceIDRelative, &r1);
  GetRelativeRect(aWindow, aResourceIDControl, &r2);

  ctrl = GetDlgItem(aWindow, aResourceIDControl);
  SetWindowPos(ctrl, NULL, r2.left, r1.bottom + aOffset, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}


LRESULT CALLBACK DlgProcSummary(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  HWND ctrl;
  LPSTR szMessage = NULL;
  LPNMHDR notifyMessage;
  char szAddtlComps[MAX_BUF];
  char szTemp[MAX_BUF];
  char szFormat[MAX_BUF_TINY];
  BOOL hasAddtlComps = FALSE;
  BOOL skipNext = FALSE;
  siC *currComponent, *mainComponent;
  HICON largeIcon, smallIcon;

  switch(msg) {
  case WM_INITDIALOG:
    SetDlgItemText(hDlg, IDC_MESSAGE1, diStartInstall.szMessage0);
    SetDlgItemText(hDlg, IDC_MESSAGE2, sgInstallGui.szProxyMessage);
    SetDlgItemText(hDlg, IDC_CONNECTION_SETTINGS, sgInstallGui.szProxyButton);
    SetDlgItemText(hDlg, IDC_INSTALL_FOLDER_LABEL, sgInstallGui.szInstallFolder);

    // This is a bit of a hack for now, not parameterizable as I'd like it to be. 
    // Unfortunately pressed for time. Revisit. 
    // -Ben
#if defined(MOZ_PHOENIX)
    ExtractIconEx("setuprsc.dll", 1, &largeIcon, &smallIcon, 1);
#elif defined(MOZ_THUNDERBIRD)
    ExtractIconEx("setuprsc.dll", 2, &largeIcon, &smallIcon, 1);
#endif
    SendMessage(GetDlgItem(hDlg, IDC_APP_ICON), STM_SETICON, (LPARAM)smallIcon, 0);

    ExtractIconEx("shell32.dll", 3, &largeIcon, &smallIcon, 1);
    SendMessage(GetDlgItem(hDlg, IDC_FOLDER_ICON), STM_SETICON, (LPARAM)smallIcon, 0);

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

#ifdef STUB_INSTALLER
      // Update strings that relate to whether or not files will be downloaded. 
      // The user may have selected additional components that require a download.
      if ((diAdvancedSettings.bShowDialog == FALSE) || (GetTotalArchivesToDownload() == 0)) {
#endif
        SetDlgItemText(hDlg, IDC_MESSAGE0, diStartInstall.szMessageInstall);

        // Hide Download-related UI:
        ShowWindow(GetDlgItem(hDlg, IDC_MESSAGE2), SW_HIDE);
        ShowWindow(GetDlgItem(hDlg, IDC_CONNECTION_SETTINGS), SW_HIDE);
#ifdef STUB_INSTALLER
      }
      else
        SetDlgItemText(hDlg, IDC_MESSAGE0, diStartInstall.szMessageDownload);
#endif

      // Show the components we're going to install
      szAddtlComps[0] = '\0';
      currComponent = siComponents;
      do {
        if (!currComponent)
          break;

        if (currComponent->dwAttributes & SIC_MAIN_COMPONENT)
          mainComponent = currComponent;
        else if (IsSelectableComponent(currComponent) && 
                 IsComponentSelected(currComponent)) {
          wsprintf(szFormat, "%s\r\n", sgInstallGui.szAddtlCompWrapper);
          wsprintf(szTemp, szFormat, currComponent->szDescriptionShort);
          lstrcat(szAddtlComps, szTemp);

          hasAddtlComps = TRUE;
        }

        currComponent = currComponent->Next;
      }
      while (currComponent && currComponent != siComponents);

      // Update the display to reflect whether or not additional components are going to 
      // be installed. If none are, we shift the install folder detail up so that it's
      // neatly under the Primary Component name. 
      if (hasAddtlComps) {
        wsprintf(szTemp, sgInstallGui.szPrimCompOthers, mainComponent->szDescriptionShort);

        ShowWindow(GetDlgItem(hDlg, IDC_OPTIONAL_COMPONENTS), SW_SHOW);

        SetDlgItemText(hDlg, IDC_OPTIONAL_COMPONENTS, szAddtlComps);

        PositionControl(hDlg, IDC_OPTIONAL_COMPONENTS, IDC_INSTALL_FOLDER_LABEL, 10);
      }
      else {
        wsprintf(szTemp, sgInstallGui.szPrimCompNoOthers, mainComponent->szDescriptionShort);

        ShowWindow(GetDlgItem(hDlg, IDC_OPTIONAL_COMPONENTS), SW_HIDE);

        // Shift the "Install Folder" text up in the "No Components" case
        PositionControl(hDlg, IDC_PRIMARY_COMPONENT, IDC_INSTALL_FOLDER_LABEL, 10);
      }
      PositionControl(hDlg, IDC_INSTALL_FOLDER_LABEL, IDC_INSTALL_FOLDER, 10);
      PositionControl(hDlg, IDC_INSTALL_FOLDER_LABEL, IDC_FOLDER_ICON, 7);

      SetDlgItemText(hDlg, IDC_PRIMARY_COMPONENT, szTemp);

      // Update the install folder. 
      ctrl = GetDlgItem(hDlg, IDC_INSTALL_FOLDER);
      TruncateString(ctrl, sgProduct.szPath, szTemp, sizeof(szTemp));
      SetDlgItemText(hDlg, IDC_INSTALL_FOLDER, szTemp);
      
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

    case PSN_QUERYCANCEL:
      return !ShouldExitSetup(hDlg);
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

#ifdef STUB_INSTALLER
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
      SetWindowText(hDlg, diAdvancedSettings.szTitle);
      SetDlgItemText(hDlg, IDC_MESSAGE0,          diAdvancedSettings.szMessage0);
      SetDlgItemText(hDlg, IDC_EDIT_PROXY_SERVER, diAdvancedSettings.szProxyServer);
      SetDlgItemText(hDlg, IDC_EDIT_PROXY_PORT,   diAdvancedSettings.szProxyPort);
      SetDlgItemText(hDlg, IDC_EDIT_PROXY_USER,   diAdvancedSettings.szProxyUser);
      SetDlgItemText(hDlg, IDC_EDIT_PROXY_PASSWD, diAdvancedSettings.szProxyPasswd);

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
          DestroyWindow(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// DIALOG: DOWNLOADING FILES
//

LRESULT CALLBACK DlgProcDownloading(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
#ifdef STUB_INSTALLER
  switch(msg)
  {
    case WM_INITDIALOG:
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
#endif

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// DIALOG: INSTALLING FILES
//

#define BTN_CLSNAME_LEN 7
#define BTN_CANCEL_OFFSET 4
BOOL CALLBACK DisableCancelButton(HWND aWindow, LPARAM aData)
{
  static int offset = 0;
  char className[BTN_CLSNAME_LEN];
  char text[MAX_BUF];

  GetClassName(aWindow, className, BTN_CLSNAME_LEN);
  if (!strcmp(className, "Button") && 
      GetParent(aWindow) == (HWND)aData) {
    GetWindowText(aWindow, text, MAX_BUF-1);
    if (++offset == BTN_CANCEL_OFFSET) {
      offset = 0;
      EnableWindow(aWindow, FALSE);
      return FALSE;
    }
  }

  return TRUE;
}

LRESULT CALLBACK DlgProcInstalling(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  LPNMHDR notifyMessage;
  static BOOL initialized = FALSE; 
  HWND parent = GetParent(hDlg);
  char installStart[MAX_BUF];

  switch(msg) {
  case WM_INITDIALOG:
    SetWindowText(hDlg, diInstalling.szTitle);

    SetDlgItemText(hDlg, IDC_STATUS0, diInstalling.szStatusFile);
    SetDlgItemText(hDlg, IDC_STATUS3, diInstalling.szStatusComponent);

    break;

  case WM_PAINT:
    if (initialized)
      break;

    initialized = TRUE;

    if (InstallFiles(hDlg)) {
#if WINTEGRATION_PAGE
      if (dwSetupType == ST_RADIO0) 
        PropSheet_SetCurSelByID(parent, DLG_INSTALL_SUCCESSFUL);
      else
#endif
        PropSheet_SetCurSelByID(parent, DLG_INSTALL_SUCCESSFUL);

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
      PropSheet_SetTitle(parent, 0, (LPTSTR)diInstalling.szTitle); 

      GetPrivateProfileString("Messages", "MSG_SMARTUPDATE_START", 
                              "", installStart, 
                              sizeof(installStart), 
                              szFileIniInstall);

      SetDlgItemText(hDlg, IDC_STATUS0, installStart);

      // Disable the Cancel button. This leaves the button disabled for
      // this page (Installing) and the final page (Finish) because it 
      // is meaningless and potentially damaging in both places. If we 
      // ever bring back the Wintegration page we're going to have to 
      // do something about it. 
      EnumChildWindows(parent, DisableCancelButton, (LPARAM)parent);

      PropSheet_SetWizButtons(parent, 0);

      break;

    case PSN_QUERYCANCEL:
      // Do NOT let the user cancel at this point. 
      SetWindowLong(hDlg, DWL_MSGRESULT, (LONG)FALSE);
      return TRUE;
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

  CleanupXpcomFile();

  gbProcessingXpnstallFiles = FALSE;

  return err == WIZ_OK || err == 999;
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

    case PSN_QUERYCANCEL:
      return !ShouldExitSetup(hDlg);
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
      launchAppChecked = IsDlgButtonChecked(hDlg, IDC_START_APP) == BST_CHECKED;
      
      break;

    case PSN_WIZFINISH:
      // Store state from the "Run App Now" checkbox. ProcessFileOpsForAll
      // uses this variable to decide whether or not to launch the browser.
      gbIgnoreRunAppX = IsDlgButtonChecked(hDlg, IDC_START_APP) != BST_CHECKED;

      // Apply settings and close. 
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

      break;


    case PSN_QUERYCANCEL:
      // Cancel is meaningless and potentially harmful here (the install work
      // is not yet complete). If the user finds a way of cancelling, e.g. by
      // clicking the X button on the window, send the FINISH message. 

      // Assume at least that they didn't want the app to run right away. 
      gbIgnoreRunAppX = FALSE;

      // Send the PSN_WIZFINISH so we can clean up properly. 
      notifyMessage->code = PSN_WIZFINISH;
      SendMessage(hDlg, WM_NOTIFY, wParam, (LPARAM)notifyMessage);

      // Tell the Wizard97 framework that we don't want to hard-quit. Processing
      // of the PSN_WIZFINISH will cause the app to shut down. 
      SetWindowLong(hDlg, DWL_MSGRESULT, FALSE);
      return TRUE;
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
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_MESSAGE:
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

