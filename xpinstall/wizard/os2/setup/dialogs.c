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
 */

#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"
#include "xpistub.h"
#include "xpi.h"
#include "logging.h"
//#include <shlobj.h>
#include <logkeys.h>

static PFNWP OldListBoxWndProc;
static BOOL    gbProcessingXpnstallFiles;
static DWORD   gdwACFlag;
static DWORD   gdwIndexLastSelected;

BOOL AskCancelDlg(HWND hDlg)
{
  char szDlgQuitTitle[MAX_BUF];
  char szDlgQuitMsg[MAX_BUF];
  char szMsg[MAX_BUF];
  BOOL bRv = FALSE;

  if((sgProduct.ulMode != SILENT) && (sgProduct.ulMode != AUTO))
  {
    if(!GetPrivateProfileString("Messages", "DLGQUITTITLE", "", szDlgQuitTitle, sizeof(szDlgQuitTitle), szFileIniInstall))
      WinPostQueueMsg(0, WM_QUIT, 1, 0);
    else if(!GetPrivateProfileString("Messages", "DLGQUITMSG", "", szDlgQuitMsg, sizeof(szDlgQuitMsg), szFileIniInstall))
      WinPostQueueMsg(0, WM_QUIT, 1, 0);
    else if(WinMessageBox(HWND_DESKTOP, hDlg, szDlgQuitMsg, szDlgQuitTitle, 0, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_APPLMODAL) == MBID_YES)
    {
      WinDestroyWindow(hDlg);
      WinPostQueueMsg(0, WM_QUIT, 0, 0);
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

#ifdef OLDCODE
void DisableSystemMenuItems(HWND hWnd, BOOL bDisableClose)
{
  EnableMenuItem(GetSystemMenu(hWnd, FALSE), SC_RESTORE,  MF_BYCOMMAND | MF_GRAYED);
  EnableMenuItem(GetSystemMenu(hWnd, FALSE), SC_SIZE,     MF_BYCOMMAND | MF_GRAYED);
  EnableMenuItem(GetSystemMenu(hWnd, FALSE), SC_MAXIMIZE, MF_BYCOMMAND | MF_GRAYED);

  if(bDisableClose)
    EnableMenuItem(GetSystemMenu(hWnd, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
}
#endif

MRESULT EXPENTRY DlgProcWelcome(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  BOOL rc;
  char szBuf[MAX_BUF];
  SWP  swpDlg;
  
  switch(msg)
  {
    case WM_INITDLG:
//      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diWelcome.szTitle);

      sprintf(szBuf, diWelcome.szMessage0, sgProduct.szProductName, sgProduct.szProductName);
      WinSetDlgItemText(hDlg, IDC_STATIC0, szBuf);
      WinSetDlgItemText(hDlg, IDC_STATIC1, diWelcome.szMessage1);
      WinSetDlgItemText(hDlg, IDC_STATIC2, diWelcome.szMessage2);

      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (gSystemInfo.lScreenX/2)-(swpDlg.cx/2),
                      (gSystemInfo.lScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);

      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
      
      rc = WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC0), PP_FONTNAMESIZE,
                                           strlen(sgInstallGui.szDefinedFont), sgInstallGui.szDefinedFont);
      rc = WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC1), PP_FONTNAMESIZE,
                                           strlen(sgInstallGui.szDefinedFont), sgInstallGui.szDefinedFont);
      rc = WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC2), PP_FONTNAMESIZE,
                                           strlen(sgInstallGui.szDefinedFont), sgInstallGui.szDefinedFont);
      rc = WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE,
                                           strlen(sgInstallGui.szDefinedFont), sgInstallGui.szDefinedFont);
      rc = WinSetPresParam(WinWindowFromID(hDlg, IDCANCEL), PP_FONTNAMESIZE,
                                           strlen(sgInstallGui.szDefinedFont), sgInstallGui.szDefinedFont);
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDWIZNEXT:
          WinDestroyWindow(hDlg);
          DlgSequence(NEXT_DLG);
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

MRESULT EXPENTRY DlgProcLicense(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  char            szBuf[MAX_BUF];
  LPSTR           szLicenseFilenameBuf = NULL;
  DWORD           dwFileSize;
  DWORD           dwBytesRead;
  HANDLE          hFLicense;
  FILE            *fLicense;
  SWP             swpDlg;

  switch(msg)
  {
    case WM_INITDLG:
//      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diLicense.szTitle);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0, diLicense.szMessage0);
      WinSetDlgItemText(hDlg, IDC_MESSAGE1, diLicense.szMessage1);

      strcpy(szBuf, szSetupDir);
      AppendBackSlash(szBuf, sizeof(szBuf));
      strcat(szBuf, diLicense.szLicenseFilename);

      if((fLicense = fopen(szBuf, "rb")) != NULL)
      {
        fseek(fLicense, 0L, SEEK_END);
        dwFileSize = ftell(fLicense);
        fseek(fLicense, 0L, SEEK_SET);
        if((szLicenseFilenameBuf = NS_GlobalAlloc(dwFileSize)) != NULL) {
          dwBytesRead = fread(szLicenseFilenameBuf, sizeof(char), dwFileSize, fLicense);
          WinSetDlgItemText(hDlg, IDC_EDIT_LICENSE, szLicenseFilenameBuf);
          FreeMemory(&szLicenseFilenameBuf);
        }
        fclose(fLicense);
      }

      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (gSystemInfo.lScreenX/2)-(swpDlg.cx/2),
                      (gSystemInfo.lScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);

      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szAccept_);
      WinSetDlgItemText(hDlg, DID_CANCEL, sgInstallGui.szDecline_);
#ifdef OLDCODE
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_EDIT_LICENSE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
#endif
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDWIZNEXT:
          WinDestroyWindow(hDlg);
          DlgSequence(NEXT_DLG);
          break;

        case IDWIZBACK:
          WinDestroyWindow(hDlg);
          DlgSequence(PREV_DLG);
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

#ifdef OLDCODE
LRESULT CALLBACK ListBoxBrowseWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch(uMsg)
  {
    case LB_SETCURSEL:
      gdwIndexLastSelected = (DWORD)wParam;
      break;
  }

  return(CallWindowProc(OldListBoxWndProc, hWnd, uMsg, wParam, lParam));
}

LRESULT CALLBACK BrowseHookProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  DWORD dwIndex;
  DWORD dwLoop;
  RECT  rDlg;
  char  szBuf[MAX_BUF];
  char  szBufIndex[MAX_BUF];
  char  szPath[MAX_BUF];
  HWND  hwndLBFolders;

  switch(message)
  {
    case WM_INITDLG:		
      hwndLBFolders  = WinWindowFromID(hDlg, 1121);
      WinSetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szTempSetupPath);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.lScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.lScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      OldListBoxWndProc    = SubclassWindow(hwndLBFolders, (WNDPROC)ListBoxBrowseWndProc);
      gdwIndexLastSelected = SendDlgItemMessage(hDlg, 1121, LB_GETCURSEL, 0, (LPARAM)0);

      WinSetWindowText(hDlg, sgInstallGui.szSelectDirectory);
      WinSetDlgItemText(hDlg, 1092, sgInstallGui.szDirectories_);
      WinSetDlgItemText(hDlg, 1091, sgInstallGui.szDrives_);
      WinSetDlgItemText(hDlg, 1, sgInstallGui.szOk);
      WinSetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel);
      SendDlgItemMessage (hDlg, DLG_BROWSE_DIR, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, 1092, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, 1091, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, 1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDC_EDIT_DESTINATION, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L); 
      SendDlgItemMessage (hDlg, 1121, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L); 
      SendDlgItemMessage (hDlg, 1137, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L); 
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case 1121:
          if(HIWORD(wParam) == LBN_DBLCLK)
          {
            dwIndex = SendDlgItemMessage(hDlg, 1121, LB_GETCURSEL, 0, (LPARAM)0);
            SendDlgItemMessage(hDlg, 1121, LB_GETTEXT, 0, (LPARAM)szPath);

            if(gdwIndexLastSelected < dwIndex)
            {
              for(dwLoop = 1; dwLoop <= gdwIndexLastSelected; dwLoop++)
              {
                SendDlgItemMessage(hDlg, 1121, LB_GETTEXT, dwLoop, (LPARAM)szBufIndex);
                AppendBackSlash(szPath, sizeof(szPath));
                strcat(szPath, szBufIndex);
              }

              SendDlgItemMessage(hDlg, 1121, LB_GETTEXT, dwIndex, (LPARAM)szBufIndex);
              AppendBackSlash(szPath, sizeof(szPath));
              strcat(szPath, szBufIndex);
            }
            else
            {
              for(dwLoop = 1; dwLoop <= dwIndex; dwLoop++)
              {
                SendDlgItemMessage(hDlg, 1121, LB_GETTEXT, dwLoop, (LPARAM)szBufIndex);
                AppendBackSlash(szPath, sizeof(szPath));
                strcat(szPath, szBufIndex);
              }
            }
            WinSetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szPath);
          }
          break;

        case DID_OK:
          WinQueryDlgItemText(hDlg, IDC_EDIT_DESTINATION, MAX_BUF, szBuf);
          if(*szBuf == '\0')
          {
            char szEDestinationPath[MAX_BUF];

            GetPrivateProfileString("Messages", "ERROR_DESTINATION_PATH", "", szEDestinationPath, sizeof(szEDestinationPath), szFileIniInstall);
            WinMessageBox(HWND_DESKTOP, hDlg, szEDestinationPath, NULL, MB_OK | MB_ICONEXCLAMATION);
            break;
          }

          AppendBackSlash(szBuf, sizeof(szBuf));
          if(FileExists(szBuf) == FALSE)
          {
            char szMsgCreateDirectory[MAX_BUF];
            char szStrCreateDirectory[MAX_BUF];
            char szBufTemp[MAX_BUF];
            char szBufTemp2[MAX_BUF];

            GetPrivateProfileString("Messages", "STR_CREATE_DIRECTORY", "", szStrCreateDirectory, sizeof(szStrCreateDirectory), szFileIniInstall);
            if(GetPrivateProfileString("Messages", "MSG_CREATE_DIRECTORY", "", szMsgCreateDirectory, sizeof(szMsgCreateDirectory), szFileIniInstall))
            {
              strcpy(szBufTemp, "\n\n");
              strcat(szBufTemp, szBuf);
              RemoveBackSlash(szBufTemp);
              strcat(szBufTemp, "\n\n");
              sprintf(szBufTemp2, szMsgCreateDirectory, szBufTemp);
            }

            if(WinMessageBox(HWND_DESKTOP, hDlg, szBufTemp2, szStrCreateDirectory, 0, MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
              char szBuf2[MAX_PATH];

              if(CreateDirectoriesAll(szBuf, TRUE) == FALSE)
              {
                char szECreateDirectory[MAX_BUF];

                strcpy(szBufTemp, "\n\n");
                strcat(szBufTemp, sgProduct.szPath);
                RemoveBackSlash(szBufTemp);
                strcat(szBufTemp, "\n\n");

                if(GetPrivateProfileString("Messages", "ERROR_CREATE_DIRECTORY", "", szECreateDirectory, sizeof(szECreateDirectory), szFileIniInstall))
                  sprintf(szBuf, szECreateDirectory, szBufTemp);

                WinMessageBox(HWND_DESKTOP, hDlg, szBuf, "", 0, MB_OK | MB_ERROR);
                break;
              }

              if(*sgProduct.szSubPath != '\0')
              {
                 /* log the subpath for uninstallation.  This subpath does not normally get logged
                  * for uninstallation due to a chicken and egg problem with creating the log file
                  * and the directory its in */
                strcpy(szBuf2, szBuf);
                AppendBackSlash(szBuf2, sizeof(szBuf2));
                strcat(szBuf2, sgProduct.szSubPath);
                UpdateInstallLog(KEY_CREATE_FOLDER, szBuf2, FALSE);
              }

              bCreateDestinationDir = TRUE;
            }
            else
            {
              break;
            }
          }

          strcpy(szTempSetupPath, szBuf);
          RemoveBackSlash(szTempSetupPath);
          EndDialog(hDlg, 0);
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

BOOL BrowseForDirectory(HWND hDlg, char *szCurrDir)
{ 
  OPENFILENAME   of;
  char           ftitle[MAX_PATH];
  char           fname[MAX_PATH];
  char           szCDir[MAX_BUF];
  char           szBuf[MAX_BUF];
  char           szSearchPathBuf[MAX_BUF];
  char           szDlgBrowseTitle[MAX_BUF];
  BOOL           bRet;

  /* save the current directory */
  GetCurrentDirectory(MAX_BUF, szCDir);

  memset(szDlgBrowseTitle, 0, sizeof(szDlgBrowseTitle));
  GetPrivateProfileString("Messages", "DLGBROWSETITLE", "", szDlgBrowseTitle, sizeof(szDlgBrowseTitle), szFileIniInstall);

  strcpy(szSearchPathBuf, szCurrDir);
  if((*szSearchPathBuf != '\0') && ((strlen(szSearchPathBuf) != 1) || (*szSearchPathBuf != '\\')))
  {
    RemoveBackSlash(szSearchPathBuf);
    while(FileExists(szSearchPathBuf) == FALSE)
    {
      RemoveBackSlash(szSearchPathBuf);
      ParsePath(szSearchPathBuf, szBuf, sizeof(szBuf), FALSE, PP_PATH_ONLY);
      strcpy(szSearchPathBuf, szBuf);
    }
  }

  memset(ftitle, 0, sizeof(ftitle));
  strcpy(fname, "*.*");
  of.lStructSize        = sizeof(OPENFILENAME);
  of.hwndOwner          = hDlg;
  of.hInstance          = hSetupRscInst;
  of.lpstrFilter        = NULL;
  of.lpstrCustomFilter  = NULL;
  of.nMaxCustFilter     = 0;
  of.nFilterIndex       = 0;
  of.lpstrFile          = fname;
  of.nMaxFile           = MAX_PATH;
  of.lpstrFileTitle     = ftitle;
  of.nMaxFileTitle      = MAX_PATH;
  of.lpstrInitialDir    = szSearchPathBuf;
  of.lpstrTitle         = szDlgBrowseTitle;
  of.Flags              = OFN_NONETWORKBUTTON |
                          OFN_ENABLEHOOK      |
                          OFN_NOCHANGEDIR  |
                          OFN_ENABLETEMPLATE;
  of.nFileOffset        = 0;
  of.nFileExtension     = 0;
  of.lpstrDefExt        = NULL;
  of.lCustData          = 0;
  of.lpfnHook           = BrowseHookProc;
  of.lpTemplateName     = MAKEINTRESOURCE(DLG_BROWSE_DIR);

  if(GetOpenFileName(&of))
    bRet = TRUE;
  else
    bRet = FALSE;

  /* restore the current directory */
  SetCurrentDirectory(szCDir);
  return(bRet);
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

  if((DWORD)strlen(szInURL) > dwOutStringBufSize)
    return;

  memset(szOutString, 0, dwOutStringBufSize);
  strcpy(szOutString, szInURL);
  iOutStringLen = strlen(szOutString);
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
      memmove(ptr - 1, ptr, strlen(ptr) + 1);
      szOutString[iHalfLen - 2] = '.';
      szOutString[iHalfLen - 1] = '.';
      szOutString[iHalfLen]     = '.';
      iOutStringLen = strlen(szOutString);
      GetTextExtentPoint32(hdcWnd, szOutString, iOutStringLen, &sizeString);
    }
  }

  SelectObject(hdcWnd, hfontOld);
  DeleteObject(hfontNew);
  ReleaseDC(hWnd, hdcWnd);
}
#endif


MRESULT EXPENTRY DlgProcSetupType(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  HWND          hRadioSt0;
  HWND          hStaticSt0;
  HWND          hRadioSt1;
  HWND          hStaticSt1;
  HWND          hRadioSt2;
  HWND          hStaticSt2;
  HWND          hRadioSt3;
  HWND          hStaticSt3;
  HWND          hReadme;
  HWND          hDestinationPath;
  SWP           swpDlg;
  char          szBuf[MAX_BUF];
  char          szBufTemp[MAX_BUF];
  char          szBufTemp2[MAX_BUF];

  hRadioSt0   = WinWindowFromID(hDlg, IDC_RADIO_ST0);
  hStaticSt0  = WinWindowFromID(hDlg, IDC_STATIC_ST0_DESCRIPTION);
  hRadioSt1   = WinWindowFromID(hDlg, IDC_RADIO_ST1);
  hStaticSt1  = WinWindowFromID(hDlg, IDC_STATIC_ST1_DESCRIPTION);
  hRadioSt2   = WinWindowFromID(hDlg, IDC_RADIO_ST2);
  hStaticSt2  = WinWindowFromID(hDlg, IDC_STATIC_ST2_DESCRIPTION);
  hRadioSt3   = WinWindowFromID(hDlg, IDC_RADIO_ST3);
  hStaticSt3  = WinWindowFromID(hDlg, IDC_STATIC_ST3_DESCRIPTION);
  hReadme     = WinWindowFromID(hDlg, IDC_README);

  switch(msg)
  {
    case WM_INITDLG:
//      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diSetupType.szTitle);

      hDestinationPath = WinWindowFromID(hDlg, IDC_EDIT_DESTINATION); /* handle to the static destination path text window */
//      TruncateString(hDestinationPath, szTempSetupPath, szBuf, sizeof(szBuf));

      WinSetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szBuf);
      WinSetDlgItemText(hDlg, IDC_STATIC_MSG0, diSetupType.szMessage0);

      if(diSetupType.stSetupType0.bVisible)
      {
        WinSetDlgItemText(hDlg, IDC_RADIO_ST0, diSetupType.stSetupType0.szDescriptionShort);
        WinSetDlgItemText(hDlg, IDC_STATIC_ST0_DESCRIPTION, diSetupType.stSetupType0.szDescriptionLong);
        WinShowWindow(hRadioSt0, TRUE);
        WinShowWindow(hStaticSt0, TRUE);
      }
      else
      {
        WinShowWindow(hRadioSt0, FALSE);
        WinShowWindow(hStaticSt0, FALSE);
      }

      if(diSetupType.stSetupType1.bVisible)
      {
        WinSetDlgItemText(hDlg, IDC_RADIO_ST1, diSetupType.stSetupType1.szDescriptionShort);
        WinSetDlgItemText(hDlg, IDC_STATIC_ST1_DESCRIPTION, diSetupType.stSetupType1.szDescriptionLong);
        WinShowWindow(hRadioSt1, TRUE);
        WinShowWindow(hStaticSt1, TRUE);
      }
      else
      {
        WinShowWindow(hRadioSt1, FALSE);
        WinShowWindow(hStaticSt1, FALSE);
      }

      if(diSetupType.stSetupType2.bVisible)
      {
        WinSetDlgItemText(hDlg, IDC_RADIO_ST2, diSetupType.stSetupType2.szDescriptionShort);
        WinSetDlgItemText(hDlg, IDC_STATIC_ST2_DESCRIPTION, diSetupType.stSetupType2.szDescriptionLong);
        WinShowWindow(hRadioSt2, TRUE);
        WinShowWindow(hStaticSt2, TRUE);
      }
      else
      {
        WinShowWindow(hRadioSt2, FALSE);
        WinShowWindow(hStaticSt2, FALSE);
      }

      if(diSetupType.stSetupType3.bVisible)
      {
        WinSetDlgItemText(hDlg, IDC_RADIO_ST3, diSetupType.stSetupType3.szDescriptionShort);
        WinSetDlgItemText(hDlg, IDC_STATIC_ST3_DESCRIPTION, diSetupType.stSetupType3.szDescriptionLong);
        WinShowWindow(hRadioSt3, TRUE);
        WinShowWindow(hStaticSt3, TRUE);
      }
      else
      {
        WinShowWindow(hRadioSt3, FALSE);
        WinShowWindow(hStaticSt3, FALSE);
      }

      /* enable the appropriate radio button */
      switch(ulTempSetupType)
      {
        case ST_RADIO0:
          WinCheckButton(hDlg, IDC_RADIO_ST0, 1);
          WinSetFocus(HWND_DESKTOP, hRadioSt0);
          break;

        case ST_RADIO1:
          WinCheckButton(hDlg, IDC_RADIO_ST1, 1);
          WinSetFocus(HWND_DESKTOP, hRadioSt1);
          break;

        case ST_RADIO2:
          WinCheckButton(hDlg, IDC_RADIO_ST2, 1);
          WinSetFocus(HWND_DESKTOP, hRadioSt2);
          break;

        case ST_RADIO3:
          WinCheckButton(hDlg, IDC_RADIO_ST3, 1);
          WinSetFocus(HWND_DESKTOP, hRadioSt3);
          break;
      }

      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (gSystemInfo.lScreenX/2)-(swpDlg.cx/2),
                      (gSystemInfo.lScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);

      if((*diSetupType.szReadmeFilename == '\0') || (FileExists(diSetupType.szReadmeFilename) == FALSE))
        WinShowWindow(hReadme, FALSE);
      else
        WinShowWindow(hReadme, TRUE);

      WinSetDlgItemText(hDlg, IDC_DESTINATION, sgInstallGui.szDestinationDirectory);
      WinSetDlgItemText(hDlg, IDC_BUTTON_BROWSE, sgInstallGui.szBrowse_);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
      WinSetDlgItemText(hDlg, IDC_README, sgInstallGui.szReadme_);
#ifdef OLDCODE
      SendDlgItemMessage (hDlg, IDC_STATIC_MSG0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_RADIO_ST0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_RADIO_ST1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_RADIO_ST2, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_RADIO_ST3, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC_ST0_DESCRIPTION, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC_ST1_DESCRIPTION, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC_ST2_DESCRIPTION, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC_ST3_DESCRIPTION, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_EDIT_DESTINATION, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
      SendDlgItemMessage (hDlg, IDC_BUTTON_BROWSE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_README, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
#endif

      if(sgProduct.bLockPath)
        WinEnableWindow(WinWindowFromID(hDlg, IDC_BUTTON_BROWSE), FALSE);
      else
        WinEnableWindow(WinWindowFromID(hDlg, IDC_BUTTON_BROWSE), TRUE);

      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDC_BUTTON_BROWSE:
          if(WinQueryButtonCheckstate(hDlg, IDC_RADIO_ST0)      == 1)
            ulTempSetupType = ST_RADIO0;
          else if(WinQueryButtonCheckstate(hDlg, IDC_RADIO_ST1) == 1)
            ulTempSetupType = ST_RADIO1;
          else if(WinQueryButtonCheckstate(hDlg, IDC_RADIO_ST2) == 1)
            ulTempSetupType = ST_RADIO2;
          else if(WinQueryButtonCheckstate(hDlg, IDC_RADIO_ST3) == 1)
            ulTempSetupType = ST_RADIO3;

//          BrowseForDirectory(hDlg, szTempSetupPath);

          hDestinationPath = WinWindowFromID(hDlg, IDC_EDIT_DESTINATION); /* handle to the static destination path text window */
//          TruncateString(hDestinationPath, szTempSetupPath, szBuf, sizeof(szBuf));
          WinSetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szBuf);
          break;

        case IDC_README:
#ifdef OLDCODE /* @MAK - need to port this */
          if(*diSetupType.szReadmeApp == '\0')
            WinSpawn(diSetupType.szReadmeFilename, NULL, szSetupDir, TRUENORMAL, FALSE);
          else
            WinSpawn(diSetupType.szReadmeApp, diSetupType.szReadmeFilename, szSetupDir, TRUENORMAL, FALSE);
#endif
          break;

        case IDWIZNEXT:
          strcpy(sgProduct.szPath, szTempSetupPath);

          strcpy(szBuf, sgProduct.szPath);

          if(FileExists(szBuf) == FALSE)
          {
            char szMsgCreateDirectory[MAX_BUF];
            char szStrCreateDirectory[MAX_BUF];

            GetPrivateProfileString("Messages", "STR_CREATE_DIRECTORY", "", szStrCreateDirectory, sizeof(szStrCreateDirectory), szFileIniInstall);
            if(GetPrivateProfileString("Messages", "MSG_CREATE_DIRECTORY", "", szMsgCreateDirectory, sizeof(szMsgCreateDirectory), szFileIniInstall))
            {
              strcpy(szBufTemp, "\n\n");
              strcat(szBufTemp, szBuf);
              RemoveBackSlash(szBufTemp);
              strcat(szBufTemp, "\n\n");
              sprintf(szBufTemp2, szMsgCreateDirectory, szBufTemp);
            }

            if(WinMessageBox(HWND_DESKTOP, hDlg, szBufTemp2, szStrCreateDirectory, 0, MB_YESNO | MB_ICONQUESTION) == MBID_YES)
            {
              char szBuf2[MAX_PATH];

              /* append a backslash to the path because CreateDirectoriesAll()
                 uses a backslash to determine directories */
              AppendBackSlash(szBuf, sizeof(szBuf));
              if(CreateDirectoriesAll(szBuf, TRUE) == FALSE)
              {
                char szECreateDirectory[MAX_BUF];

                strcpy(szBufTemp, "\n\n");
                strcat(szBufTemp, sgProduct.szPath);
                RemoveBackSlash(szBufTemp);
                strcat(szBufTemp, "\n\n");

                if(GetPrivateProfileString("Messages", "ERROR_CREATE_DIRECTORY", "", szECreateDirectory, sizeof(szECreateDirectory), szFileIniInstall))
                  sprintf(szBuf, szECreateDirectory, szBufTemp);

                WinMessageBox(HWND_DESKTOP, hDlg, szBuf, "", 0, MB_OK | MB_ERROR);
                break;
              }

              if(*sgProduct.szSubPath != '\0')
              {
                 /* log the subpath for uninstallation.  This subpath does not normally get logged
                  * for uninstallation due to a chicken and egg problem with creating the log file
                  * and the directory its in */
                strcpy(szBuf2, szBuf);
                AppendBackSlash(szBuf2, sizeof(szBuf2));
                strcat(szBuf2, sgProduct.szSubPath);
                UpdateInstallLog(KEY_CREATE_FOLDER, szBuf2, FALSE);
              }

              bCreateDestinationDir = TRUE;
            }
            else
            {
              break;
            }
          }

          /* retrieve and save the state of the selected radio button */
          if(WinQueryButtonCheckstate(hDlg, IDC_RADIO_ST0)      == 1)
          {
            SiCNodeSetItemsSelected(ST_RADIO0);

            ulSetupType     = ST_RADIO0;
            ulTempSetupType = ulSetupType;
          }
          else if(WinQueryButtonCheckstate(hDlg, IDC_RADIO_ST1) == 1)
          {
            SiCNodeSetItemsSelected(ST_RADIO1);

            ulSetupType     = ST_RADIO1;
            ulTempSetupType = ulSetupType;
          }
          else if(WinQueryButtonCheckstate(hDlg, IDC_RADIO_ST2) == 1)
          {
            SiCNodeSetItemsSelected(ST_RADIO2);

            ulSetupType     = ST_RADIO2;
            ulTempSetupType = ulSetupType;
          }
          else if(WinQueryButtonCheckstate(hDlg, IDC_RADIO_ST3) == 1)
          {
            SiCNodeSetItemsSelected(ST_RADIO3);

            ulSetupType     = ST_RADIO3;
            ulTempSetupType = ulSetupType;
          }

          WinDestroyWindow(hDlg);
          DlgSequence(NEXT_DLG);
          break;

        case IDWIZBACK:
          ulTempSetupType = ulSetupType;
          strcpy(szTempSetupPath, sgProduct.szPath);
          WinDestroyWindow(hDlg);
          DlgSequence(PREV_DLG);
          break;

        case IDCANCEL:
          strcpy(sgProduct.szPath, szTempSetupPath);
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

#ifdef OLDCODE
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
#endif

void lbAddItem(HWND hList, siC *siCComponent)
{
  DWORD dwItem;
  CHAR tchBuffer[MAX_BUF];

  strcpy(tchBuffer, siCComponent->szDescriptionShort);
  if(gSystemInfo.bScreenReader)
  {
    strcat(tchBuffer, " - ");
    if(!(siCComponent->dwAttributes & SIC_SELECTED))
      strcat(tchBuffer, sgInstallGui.szUnchecked);
    else
      strcat(tchBuffer, sgInstallGui.szChecked);
  }
  dwItem = WinSendMsg(hList, LM_INSERTITEM, LIT_END, (LPARAM)tchBuffer);

#ifdef OLDCODE
  if(siCComponent->dwAttributes & SIC_DISABLED)
    SendMessage(hList, LB_SETITEMDATA, dwItem, (LPARAM)hbmpBoxCheckedDisabled);
  else if(siCComponent->dwAttributes & SIC_SELECTED)
    SendMessage(hList, LB_SETITEMDATA, dwItem, (LPARAM)hbmpBoxChecked);
  else
    SendMessage(hList, LB_SETITEMDATA, dwItem, (LPARAM)hbmpBoxUnChecked);
#endif
} 

void InvalidateLBCheckbox(HWND hwndListBox)
{
  RECTL rcCheckArea;

  // retrieve the rectangle of all list items to update.
  WinQueryWindowRect(hwndListBox, &rcCheckArea);

  // Set the right coordinate of the rectangle to be the same
  //   as the right edge of the bitmap drawn.
  // But if a screen reader is being used we want to redraw the text
  //   as well as the checkbox so we do not set the right coordinate.
  if(!gSystemInfo.bScreenReader)
    rcCheckArea.xRight = CX_CHECKBOX;

  // It then invalidates the checkbox region to be redrawn.
  // Invalidating the region sends a WM_DRAWITEM message to
  // the dialog, which redraws the region given the
  // node attirbute, in this case it is a bitmap of a
  // checked/unchecked checkbox.
  WinInvalidateRect(hwndListBox, &rcCheckArea, TRUE);
}
  
void ToggleCheck(HWND hwndListBox, DWORD dwIndex, DWORD dwACFlag)
{
  BOOL  bMoreToResolve;
  LPSTR szToggledReferenceName = NULL;
  DWORD dwAttributes;
  CHAR tchBuffer[MAX_BUF];

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
// FUNCTION : NewListBoxWndProc( HWND, UINT, WPARAM, LPARAM )
// PURPOSE  : Processes messages for "LISTBOX" class.
// COMMENTS : Prevents the user from moving the window
//            by dragging the titlebar.
// ************************************************************************
MRESULT APIENTRY NewListBoxWndProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  DWORD  dwIndex;
  POINTS ptspointerpos;
  USHORT fsflags;
  
  switch(msg)
  {
    case WM_CHAR:
      fsflags = SHORT1FROMMP(mp1);
      if (SHORT2FROMMP(mp2) == VK_SPACE) {
        if (!(fsflags & KC_KEYUP)) {
          dwIndex = WinSendMsg(hWnd,
                               LM_QUERYSELECTION,
                               0,
                               0);
          ToggleCheck(hWnd, dwIndex, gdwACFlag);
        }
      }
      break;

    case WM_BUTTON1DOWN:
      ptspointerpos.x = SHORT1FROMMP(mp1);
      ptspointerpos.y = SHORT1FROMMP(mp1);

      if((ptspointerpos.x > 1) && (ptspointerpos.y <= CX_CHECKBOX))
      {
#ifdef OLDCODE
        dwIndex = LOWORD(SendMessage(hWnd,
                                     LB_ITEMFROMPOINT,
                                     0,
                                     (LPARAM)MAKELPARAM(dwPosX, dwPosY)));
#endif
        ToggleCheck(hWnd, dwIndex, gdwACFlag);
      }
      break;
  }

  return(OldListBoxWndProc)(hWnd, msg, mp1, mp2);
}

MRESULT EXPENTRY DlgProcSelectComponents(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  BOOL                bReturn = FALSE;
  siC                 *siCTemp;
  DWORD               dwIndex;
  DWORD               dwItems = MAX_BUF;
  HWND                hwndLBComponents;
  SWP                 swpDlg;
  CHAR                tchBuffer[MAX_BUF];
//  LPDRAWITEMSTRUCT    lpdis;
  ULONG               ulDSBuf;
  char                szBuf[MAX_BUF];

  hwndLBComponents  = WinWindowFromID(hDlg, IDC_LIST_COMPONENTS);

  switch(msg)
  {
    case WM_INITDLG:
//      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diSelectComponents.szTitle);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0, diSelectComponents.szMessage0);

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
        WinSetFocus(HWND_DESKTOP, hwndLBComponents);
        WinSendMsg(hwndLBComponents, LM_SELECTITEM, 0, 0);
        WinSetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(0, FALSE, AC_COMPONENTS));
      }

      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (gSystemInfo.lScreenX/2)-(swpDlg.cx/2),
                      (gSystemInfo.lScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);

      /* update the disk space available info in the dialog.  GetDiskSpaceAvailable()
         returns value in kbytes */
      ulDSBuf = GetDiskSpaceAvailable(sgProduct.szPath);
      itoa(ulDSBuf, tchBuffer, 10);
      ParsePath(sgProduct.szPath, szBuf, sizeof(szBuf), FALSE, PP_ROOT_ONLY);
      RemoveBackSlash(szBuf);
      strcat(szBuf, "   ");
      strcat(szBuf, tchBuffer);
      strcat(szBuf, " KB");
      WinSetDlgItemText(hDlg, IDC_SPACE_AVAILABLE, szBuf);

      WinSetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szComponents_);
      WinSetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szDescription);
      WinSetDlgItemText(hDlg, IDC_STATIC3, sgInstallGui.szTotalDownloadSize);
      WinSetDlgItemText(hDlg, IDC_STATIC4, sgInstallGui.szSpaceAvailable);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
#ifdef OLDCODE
      SendDlgItemMessage (hDlg, IDC_STATIC1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC2, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC3, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC4, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_LIST_COMPONENTS, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC_DESCRIPTION, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_DOWNLOAD_SIZE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_SPACE_AVAILABLE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
#endif

      gdwACFlag = AC_COMPONENTS;
      OldListBoxWndProc = WinSubclassWindow(hwndLBComponents, (PFNWP)NewListBoxWndProc);
      break;

#ifdef OLDCODE
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
      {
        DrawFocusRect(lpdis->hDC, &(lpdis->rcItem));
      }

      bReturn = TRUE;

      /* update the disk space required info in the dialog.  It is already
         in Kilobytes */
      ullDSBuf = GetDiskSpaceRequired(DSR_DOWNLOAD_SIZE);
      _ui64toa(ullDSBuf, tchBuffer, 10);
      strcpy(szBuf, tchBuffer);
      strcat(szBuf, " KB");
      
      WinSetDlgItemText(hDlg, IDC_DOWNLOAD_SIZE, szBuf);
      break;
#endif

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDC_LIST_COMPONENTS:
          /* to update the long description for each component the user selected */
          if((dwIndex = WinSendMsg(hwndLBComponents, LM_QUERYSELECTION, 0, 0)) != LIT_NONE)
            WinSetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(dwIndex, FALSE, AC_COMPONENTS));
          break;
      }
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDWIZNEXT:
          WinDestroyWindow(hDlg);
          DlgSequence(NEXT_DLG);
          break;

        case IDWIZBACK:
          WinDestroyWindow(hDlg);
          DlgSequence(PREV_DLG);
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }

  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

MRESULT EXPENTRY DlgProcSelectAdditionalComponents(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  BOOL                bReturn = FALSE;
  siC                 *siCTemp;
  DWORD               dwIndex;
  DWORD               dwItems = MAX_BUF;
  HWND                hwndLBComponents;
  SWP                 swpDlg;
  TCHAR               tchBuffer[MAX_BUF];
//  LPDRAWITEMSTRUCT    lpdis;
  ULONGLONG           ullDSBuf;
  char                szBuf[MAX_BUF];

  hwndLBComponents  = WinWindowFromID(hDlg, IDC_LIST_COMPONENTS);

  switch(msg)
  {
    case WM_INITDLG:
//      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diSelectAdditionalComponents.szTitle);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0, diSelectAdditionalComponents.szMessage0);

      siCTemp = siComponents;
      if(siCTemp != NULL)
      {
        if((!(siCTemp->dwAttributes & SIC_INVISIBLE)) && (siCTemp->dwAttributes & SIC_ADDITIONAL))
          lbAddItem(hwndLBComponents, siCTemp);

        siCTemp = siCTemp->Next;
        while((siCTemp != siComponents) && (siCTemp != NULL))
        {
          if((!(siCTemp->dwAttributes & SIC_INVISIBLE)) && (siCTemp->dwAttributes & SIC_ADDITIONAL))
            lbAddItem(hwndLBComponents, siCTemp);

          siCTemp = siCTemp->Next;
        }
        WinSetFocus(HWND_DESKTOP, hwndLBComponents);
        WinSendMsg(hwndLBComponents, LM_SELECTITEM, 0, 0);
        WinSetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(0, FALSE, AC_ADDITIONAL_COMPONENTS));
      }

      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (gSystemInfo.lScreenX/2)-(swpDlg.cx/2),
                      (gSystemInfo.lScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);

      /* update the disk space available info in the dialog.  GetDiskSpaceAvailable()
         returns value in kbytes */
      ullDSBuf = GetDiskSpaceAvailable(sgProduct.szPath);
      itoa(ullDSBuf, tchBuffer, 10);
      ParsePath(sgProduct.szPath, szBuf, sizeof(szBuf), FALSE, PP_ROOT_ONLY);
      RemoveBackSlash(szBuf);
      strcat(szBuf, "   ");
      strcat(szBuf, tchBuffer);
      strcat(szBuf, " KB");
      WinSetDlgItemText(hDlg, IDC_SPACE_AVAILABLE, szBuf);

      WinSetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szAdditionalComponents_);
      WinSetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szDescription);
      WinSetDlgItemText(hDlg, IDC_STATIC3, sgInstallGui.szTotalDownloadSize);
      WinSetDlgItemText(hDlg, IDC_STATIC4, sgInstallGui.szSpaceAvailable);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
#ifdef OLDCODE
      SendDlgItemMessage (hDlg, IDC_STATIC1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC2, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC3, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC4, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_LIST_COMPONENTS, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC_DESCRIPTION, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_DOWNLOAD_SIZE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_SPACE_AVAILABLE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
#endif

      gdwACFlag = AC_ADDITIONAL_COMPONENTS;
      OldListBoxWndProc = WinSubclassWindow(hwndLBComponents, (WNDPROC)NewListBoxWndProc);
      break;

#ifdef OLDCODE
    case WM_DRAWITEM:
      lpdis = (LPDRAWITEMSTRUCT)lParam;

      // If there are no list box items, skip this message.
      if(lpdis->itemID == -1)
        break;

      DrawLBText(lpdis, AC_ADDITIONAL_COMPONENTS);
      DrawCheck(lpdis, AC_ADDITIONAL_COMPONENTS);

      // draw the focus rect on the selected item
      if((lpdis->itemAction & ODA_FOCUS) &&
         (lpdis->itemState & ODS_FOCUS))
      {
        DrawFocusRect(lpdis->hDC, &(lpdis->rcItem));
      }

      bReturn = TRUE;

      /* update the disk space required info in the dialog.  It is already
         in Kilobytes */
      ullDSBuf = GetDiskSpaceRequired(DSR_DOWNLOAD_SIZE);
      _ui64toa(ullDSBuf, tchBuffer, 10);
      strcpy(szBuf, tchBuffer);
      strcat(szBuf, " KB");
      
      WinSetDlgItemText(hDlg, IDC_DOWNLOAD_SIZE, szBuf);
      break;
#endif

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDC_LIST_COMPONENTS:
          /* to update the long description for each component the user selected */
          if((dwIndex = WinSendMsg(hwndLBComponents, LM_QUERYSELECTION, 0, 0)) != LIT_NONE)
            WinSetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(dwIndex, FALSE, AC_ADDITIONAL_COMPONENTS));
          break;
      }
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDWIZNEXT:
          WinDestroyWindow(hDlg);
          DlgSequence(NEXT_DLG);
          break;

        case IDWIZBACK:
          WinDestroyWindow(hDlg);
          DlgSequence(PREV_DLG);
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }

  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

#ifdef OLDCODE
LRESULT CALLBACK DlgProcWindowsIntegration(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  HWND hcbCheck0;
  HWND hcbCheck1;
  HWND hcbCheck2;
  HWND hcbCheck3;
  RECT rDlg;

  hcbCheck0 = WinWindowFromID(hDlg, IDC_CHECK0);
  hcbCheck1 = WinWindowFromID(hDlg, IDC_CHECK1);
  hcbCheck2 = WinWindowFromID(hDlg, IDC_CHECK2);
  hcbCheck3 = WinWindowFromID(hDlg, IDC_CHECK3);

  switch(msg)
  {
    case WM_INITDLG:
      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diWindowsIntegration.szTitle);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0, diWindowsIntegration.szMessage0);
      WinSetDlgItemText(hDlg, IDC_MESSAGE1, diWindowsIntegration.szMessage1);

      if(diWindowsIntegration.wiCB0.bEnabled)
      {
        WinShowWindow(hcbCheck0, TRUE);
        WinCheckButton(hDlg, IDC_CHECK0, diWindowsIntegration.wiCB0.bCheckBoxState);
        WinSetDlgItemText(hDlg, IDC_CHECK0, diWindowsIntegration.wiCB0.szDescription);
      }
      else
        WinShowWindow(hcbCheck0, FALSE);

      if(diWindowsIntegration.wiCB1.bEnabled)
      {
        WinShowWindow(hcbCheck1, TRUE);
        WinCheckButton(hDlg, IDC_CHECK1, diWindowsIntegration.wiCB1.bCheckBoxState);
        WinSetDlgItemText(hDlg, IDC_CHECK1, diWindowsIntegration.wiCB1.szDescription);
      }
      else
        WinShowWindow(hcbCheck1, FALSE);

      if(diWindowsIntegration.wiCB2.bEnabled)
      {
        WinShowWindow(hcbCheck2, TRUE);
        WinCheckButton(hDlg, IDC_CHECK2, diWindowsIntegration.wiCB2.bCheckBoxState);
        WinSetDlgItemText(hDlg, IDC_CHECK2, diWindowsIntegration.wiCB2.szDescription);
      }
      else
        WinShowWindow(hcbCheck2, FALSE);

      if(diWindowsIntegration.wiCB3.bEnabled)
      {
        WinShowWindow(hcbCheck3, TRUE);
        WinCheckButton(hDlg, IDC_CHECK3, diWindowsIntegration.wiCB3.bCheckBoxState);
        WinSetDlgItemText(hDlg, IDC_CHECK3, diWindowsIntegration.wiCB3.szDescription);
      }
      else
        WinShowWindow(hcbCheck3, FALSE);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.lScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.lScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_CHECK0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_CHECK1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_CHECK2, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_CHECK3, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDWIZNEXT:
          if(WinQueryButtonCheckstate(hDlg, IDC_CHECK0) == 1)
          {
          }

          if(diWindowsIntegration.wiCB0.bEnabled)
          {
            if(WinQueryButtonCheckstate(hDlg, IDC_CHECK0) == 1)
              diWindowsIntegration.wiCB0.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB0.bCheckBoxState = FALSE;
          }

          if(diWindowsIntegration.wiCB1.bEnabled)
          {
            if(WinQueryButtonCheckstate(hDlg, IDC_CHECK1) == 1)
              diWindowsIntegration.wiCB1.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB1.bCheckBoxState = FALSE;
          }

          if(diWindowsIntegration.wiCB2.bEnabled)
          {
            if(WinQueryButtonCheckstate(hDlg, IDC_CHECK2) == 1)
              diWindowsIntegration.wiCB2.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB2.bCheckBoxState = FALSE;
          }

          if(diWindowsIntegration.wiCB3.bEnabled)
          {
            if(WinQueryButtonCheckstate(hDlg, IDC_CHECK3) == 1)
              diWindowsIntegration.wiCB3.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB3.bCheckBoxState = FALSE;
          }

          WinDestroyWindow(hDlg);
          DlgSequence(NEXT_DLG);
          break;

        case IDWIZBACK:
          WinDestroyWindow(hDlg);
          DlgSequence(PREV_DLG);
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

MRESULT EXPENTRY DlgProcProgramFolder(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  char            szBuf[MAX_BUF];
  HANDLE          hDir;
  DWORD           dwIndex;
  WIN32_FIND_DATA wfdFindFileData;
  RECT            rDlg;

  switch(msg)
  {
    case WM_INITDLG:
//      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diProgramFolder.szTitle);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0, diProgramFolder.szMessage0);
      WinSetDlgItemText(hDlg, IDC_EDIT_PROGRAM_FOLDER, sgProduct.szProgramFolderName);

      strcpy(szBuf, sgProduct.szProgramFolderPath);
      strcat(szBuf, "\\*.*");
      if((hDir = FindFirstFile(szBuf , &wfdFindFileData)) != INVALID_HANDLE_VALUE)
      {
        if((wfdFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (strcmpi(wfdFindFileData.cFileName, ".") != 0) && (strcmpi(wfdFindFileData.cFileName, "..") != 0))
        {
          SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)wfdFindFileData.cFileName);
        }

        while(FindNextFile(hDir, &wfdFindFileData))
        {
          if((wfdFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (strcmpi(wfdFindFileData.cFileName, ".") != 0) && (strcmpi(wfdFindFileData.cFileName, "..") != 0))
            SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)wfdFindFileData.cFileName);
        }
        FindClose(hDir);
      }

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.lScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.lScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      WinSetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szProgramFolder_);
      WinSetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szExistingFolder_);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
      SendDlgItemMessage (hDlg, IDC_STATIC1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_STATIC2, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_EDIT_PROGRAM_FOLDER, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
      SendDlgItemMessage (hDlg, IDC_LIST, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDWIZNEXT:
          WinQueryDlgItemText(hDlg, IDC_EDIT_PROGRAM_FOLDER, MAX_BUF, szBuf);
          if(*szBuf == '\0')
          {
            char szEProgramFolderName[MAX_BUF];

            GetPrivateProfileString("Messages", "ERROR_PROGRAM_FOLDER_NAME", "", szEProgramFolderName, sizeof(szEProgramFolderName), szFileIniInstall);
            WinMessageBox(HWND_DESKTOP, hDlg, szEProgramFolderName, NULL, 0, MB_OK | MB_ICONEXCLAMATION);
            break;
          }
          strcpy(sgProduct.szProgramFolderName, szBuf);
        
          WinDestroyWindow(hDlg);
          DlgSequence(NEXT_DLG);
          break;

        case IDWIZBACK:
          WinDestroyWindow(hDlg);
          DlgSequence(PREV_DLG);
          break;

        case IDC_LIST:
          if((dwIndex = SendDlgItemMessage(hDlg, IDC_LIST, LB_GETCURSEL, 0, 0)) != LB_ERR)
          {
            SendDlgItemMessage(hDlg, IDC_LIST, LB_GETTEXT, dwIndex, (LPARAM)szBuf);
            WinSetDlgItemText(hDlg, IDC_EDIT_PROGRAM_FOLDER, szBuf);
          }
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}
#endif

void SaveDownloadProtocolOption(HWND hDlg)
{
  if(WinQueryButtonCheckstate(hDlg, IDC_USE_FTP) == 1)
    diAdditionalOptions.dwUseProtocol = UP_FTP;
  else if(WinQueryButtonCheckstate(hDlg, IDC_USE_HTTP) == 1)
    diAdditionalOptions.dwUseProtocol = UP_HTTP;
}

MRESULT EXPENTRY DlgProcAdvancedSettings(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  SWP   swpDlg;
  char  szBuf[MAX_BUF];

  switch(msg)
  {
    case WM_INITDLG:
//      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diAdvancedSettings.szTitle);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0,          diAdvancedSettings.szMessage0);
      WinSetDlgItemText(hDlg, IDC_EDIT_PROXY_SERVER, diAdvancedSettings.szProxyServer);
      WinSetDlgItemText(hDlg, IDC_EDIT_PROXY_PORT,   diAdvancedSettings.szProxyPort);
      WinSetDlgItemText(hDlg, IDC_EDIT_PROXY_USER,   diAdvancedSettings.szProxyUser);
      WinSetDlgItemText(hDlg, IDC_EDIT_PROXY_PASSWD, diAdvancedSettings.szProxyPasswd);

      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (gSystemInfo.lScreenX/2)-(swpDlg.cx/2),
                      (gSystemInfo.lScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);

      GetPrivateProfileString("Strings", "IDC Use Ftp", "", szBuf, sizeof(szBuf), szFileIniConfig);
      WinSetDlgItemText(hDlg, IDC_USE_FTP, szBuf);
      GetPrivateProfileString("Strings", "IDC Use Http", "", szBuf, sizeof(szBuf), szFileIniConfig);
      WinSetDlgItemText(hDlg, IDC_USE_HTTP, szBuf);

      WinSetDlgItemText(hDlg, IDC_STATIC, sgInstallGui.szProxySettings);
      WinSetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szServer);
      WinSetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szPort);
      WinSetDlgItemText(hDlg, IDC_STATIC3, sgInstallGui.szUserId);
      WinSetDlgItemText(hDlg, IDC_STATIC4, sgInstallGui.szPassword);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szOk_);
      WinSetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
#ifdef OLDCODE
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
#endif

      switch(diAdditionalOptions.dwUseProtocol)
      {
        case UP_HTTP:
          WinCheckButton(hDlg, IDC_USE_FTP,  0);
          WinCheckButton(hDlg, IDC_USE_HTTP, 1);
          break;

        case UP_FTP:
        default:
          WinCheckButton(hDlg, IDC_USE_FTP,  1);
          WinCheckButton(hDlg, IDC_USE_HTTP, 0);
          break;

      }

      if((diAdditionalOptions.bShowProtocols) && (diAdditionalOptions.bUseProtocolSettings))
      {
        WinShowWindow(WinWindowFromID(hDlg, IDC_USE_FTP),  TRUE);
        WinShowWindow(WinWindowFromID(hDlg, IDC_USE_HTTP), TRUE);
      }
      else
      {
        WinShowWindow(WinWindowFromID(hDlg, IDC_USE_FTP),  FALSE);
        WinShowWindow(WinWindowFromID(hDlg, IDC_USE_HTTP), FALSE);
      }

      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDWIZNEXT:
          /* get the proxy server and port information */
          WinQueryDlgItemText(hDlg, IDC_EDIT_PROXY_SERVER, MAX_BUF, diAdvancedSettings.szProxyServer);
          WinQueryDlgItemText(hDlg, IDC_EDIT_PROXY_PORT,   MAX_BUF, diAdvancedSettings.szProxyPort);
          WinQueryDlgItemText(hDlg, IDC_EDIT_PROXY_USER,   MAX_BUF, diAdvancedSettings.szProxyUser);
          WinQueryDlgItemText(hDlg, IDC_EDIT_PROXY_PASSWD, MAX_BUF, diAdvancedSettings.szProxyPasswd);

          SaveDownloadProtocolOption(hDlg);
          WinDestroyWindow(hDlg);
          DlgSequence(NEXT_DLG);
          break;

        case IDWIZBACK:
        case IDCANCEL:
          WinDestroyWindow(hDlg);
          DlgSequence(PREV_DLG);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

void SaveAdditionalOptions(HWND hDlg, HWND hwndCBSiteSelector)
{
  int iIndex;

  /* get selected item from the site selector's pull down list */
  iIndex = WinSendMsg(hwndCBSiteSelector, LM_QUERYSELECTION, 0, 0);
  WinSendMsg(hwndCBSiteSelector, LM_QUERYITEMTEXT, MPFROM2SHORT(iIndex, MAX_BUF), (MPARAM)szSiteSelectorDescription);

  /* get the state of the Save Installer Files checkbox */
  if(WinQueryButtonCheckstate(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES) == 1)
    diAdditionalOptions.bSaveInstaller = TRUE;
  else
    diAdditionalOptions.bSaveInstaller = FALSE;

  /* get the state of the Recapture Homepage checkbox */
  if(WinQueryButtonCheckstate(hDlg, IDC_CHECK_RECAPTURE_HOMEPAGE) == 1)
    diAdditionalOptions.bRecaptureHomepage = TRUE;
  else
    diAdditionalOptions.bRecaptureHomepage = FALSE;
}

MRESULT EXPENTRY DlgProcAdditionalOptions(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  SWP   swpDlg;
  char  szBuf[MAX_BUF];
  HWND  hwndCBSiteSelector;
  int   iIndex;
  ssi   *ssiTemp;
  char  szCBDefault[MAX_BUF];

  hwndCBSiteSelector = WinWindowFromID(hDlg, IDC_LIST_SITE_SELECTOR);

  switch(msg)
  {
    case WM_INITDLG:
//      if(gdwSiteSelectorStatus == SS_HIDE)
//      {
//        WinShowWindow(WinWindowFromID(hDlg, IDC_MESSAGE0),  FALSE);
//        WinShowWindow(WinWindowFromID(hDlg, IDC_LIST_SITE_SELECTOR),  FALSE);
//      }

      if(diAdditionalOptions.bShowHomepageOption == FALSE)
      {
        WinShowWindow(WinWindowFromID(hDlg, IDC_MESSAGE0),  FALSE);
        WinShowWindow(WinWindowFromID(hDlg, IDC_CHECK_RECAPTURE_HOMEPAGE),  FALSE);
      }

      if(GetTotalArchivesToDownload() == 0)
     {
        WinShowWindow(WinWindowFromID(hDlg, IDC_MESSAGE1),  FALSE);
        WinShowWindow(WinWindowFromID(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES),  FALSE);
        WinShowWindow(WinWindowFromID(hDlg, IDC_EDIT_LOCAL_INSTALLER_PATH), FALSE);
        WinShowWindow(WinWindowFromID(hDlg, IDC_BUTTON_PROXY_SETTINGS), FALSE);
      }

//      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diAdditionalOptions.szTitle);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0, diAdditionalOptions.szMessage0);
      WinSetDlgItemText(hDlg, IDC_MESSAGE1, diAdditionalOptions.szMessage1);

      GetPrivateProfileString("Strings", "IDC Save Installer Files", "", szBuf, sizeof(szBuf), szFileIniConfig);
      WinSetDlgItemText(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, szBuf);
      GetPrivateProfileString("Strings", "IDC Recapture Homepage", "", szBuf, sizeof(szBuf), szFileIniConfig);
      WinSetDlgItemText(hDlg, IDC_CHECK_RECAPTURE_HOMEPAGE, szBuf);

      GetSaveInstallerPath(szBuf, sizeof(szBuf));
      WinSetDlgItemText(hDlg, IDC_EDIT_LOCAL_INSTALLER_PATH, szBuf);

      WinSetDlgItemText(hDlg, IDC_BUTTON_PROXY_SETTINGS, sgInstallGui.szProxySettings_);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
#ifdef OLDCODE
      SendDlgItemMessage (hDlg, IDC_BUTTON_PROXY_SETTINGS, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_LIST_SITE_SELECTOR, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_CHECK_RECAPTURE_HOMEPAGE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_EDIT_LOCAL_INSTALLER_PATH, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
#endif

      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (gSystemInfo.lScreenX/2)-(swpDlg.cx/2),
                      (gSystemInfo.lScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);

      ssiTemp = ssiSiteSelector;
      do
      {
        if(ssiTemp == NULL)
          break;

        WinSendMsg(hwndCBSiteSelector, LM_INSERTITEM, 0, (LPARAM)(ssiTemp->szDescription));
        ssiTemp = ssiTemp->Next;
      } while(ssiTemp != ssiSiteSelector);

      if((szSiteSelectorDescription == NULL) || (*szSiteSelectorDescription == '\0'))
      {
          if(GetPrivateProfileString("Messages", "CB_DEFAULT", "", szCBDefault, sizeof(szCBDefault), szFileIniInstall) &&
          ((iIndex = WinSendMsg(hwndCBSiteSelector, LM_SEARCHSTRING, MPFROM2SHORT(0, LIT_FIRST), (LPARAM)szCBDefault)) != LIT_NONE))
          WinSendMsg(hwndCBSiteSelector, LM_SELECTITEM, (WPARAM)iIndex, 0);
        else
          WinSendMsg(hwndCBSiteSelector, LM_SELECTITEM, 0, 0);
      }
      else if((iIndex = WinSendMsg(hwndCBSiteSelector, LM_SEARCHSTRING, MPFROM2SHORT(0, LIT_FIRST), (LPARAM)szSiteSelectorDescription)) != LIT_NONE)
        WinSendMsg(hwndCBSiteSelector, LM_SELECTITEM, (WPARAM)iIndex, 0);
      else
        WinSendMsg(hwndCBSiteSelector, LM_SELECTITEM, 0, 0);

      if(diAdditionalOptions.bSaveInstaller)
        WinCheckButton(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, 1);
      else
        WinCheckButton(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, 0);

      if(diAdditionalOptions.bRecaptureHomepage)
        WinCheckButton(hDlg, IDC_CHECK_RECAPTURE_HOMEPAGE, 1);
      else
        WinCheckButton(hDlg, IDC_CHECK_RECAPTURE_HOMEPAGE, 0);

      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDWIZNEXT:
          SaveAdditionalOptions(hDlg, hwndCBSiteSelector);
          WinDestroyWindow(hDlg);
          DlgSequence(NEXT_DLG);
          break;

        case IDWIZBACK:
          SaveAdditionalOptions(hDlg, hwndCBSiteSelector);
          WinDestroyWindow(hDlg);
          DlgSequence(PREV_DLG);
          break;

        case IDC_BUTTON_ADDITIONAL_SETTINGS:
          SaveAdditionalOptions(hDlg, hwndCBSiteSelector);
          WinDestroyWindow(hDlg);
          DlgSequence(OTHER_DLG_1);
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

void AppendStringWOTilde(LPSTR szInputString, DWORD dwInputStringSize, LPSTR szString)
{
  DWORD i;
  DWORD iInputStringCounter;
  DWORD iInputStringLen;
  DWORD iStringLen;


  iInputStringLen = strlen(szInputString);
  iStringLen      = strlen(szString);

  if((iInputStringLen + iStringLen) >= dwInputStringSize)
    return;

  iInputStringCounter = iInputStringLen;
  for(i = 0; i < iStringLen; i++)
  {
    if(szString[i] != '~')
      szInputString[iInputStringCounter++] = szString[i];
  }
}

LPSTR GetStartInstallMessage()
{
  char  szBuf[MAX_BUF];
  char  szSTRRequired[MAX_BUF_TINY];
  siC   *siCObject   = NULL;
  LPSTR szMessageBuf = NULL;
  DWORD dwBufSize;
  DWORD dwIndex0;

  GetPrivateProfileString("Strings", "STR Force Upgrade Required", "", szSTRRequired, sizeof(szSTRRequired), szFileIniConfig);

  /* calculate the amount of memory to allocate for the buffer */
  dwBufSize = 0;

  /* setup type */
  if(GetPrivateProfileString("Messages", "STR_SETUP_TYPE", "", szBuf, sizeof(szBuf), szFileIniInstall))
    dwBufSize += strlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters
  dwBufSize += 4; // take into account 4 indentation spaces

  switch(ulSetupType)
  {
    case ST_RADIO3:
      dwBufSize += strlen(diSetupType.stSetupType3.szDescriptionShort) + 2; // the extra 2 bytes is for the \r\n characters
      break;

    case ST_RADIO2:
      dwBufSize += strlen(diSetupType.stSetupType2.szDescriptionShort) + 2; // the extra 2 bytes is for the \r\n characters
      break;

    case ST_RADIO1:
      dwBufSize += strlen(diSetupType.stSetupType1.szDescriptionShort) + 2; // the extra 2 bytes is for the \r\n characters
      break;

    default:
      dwBufSize += strlen(diSetupType.stSetupType0.szDescriptionShort) + 2; // the extra 2 bytes is for the \r\n characters
      break;
  }
  dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

  /* selected components */
  if(GetPrivateProfileString("Messages", "STR_SELECTED_COMPONENTS", "", szBuf, sizeof(szBuf), szFileIniInstall))
    dwBufSize += strlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

  dwIndex0 = 0;
  siCObject = SiCNodeGetObject(dwIndex0, FALSE, AC_ALL);
  while(siCObject)
  {
    if(siCObject->dwAttributes & SIC_SELECTED)
    {
      dwBufSize += 4; // take into account 4 indentation spaces
      dwBufSize += strlen(siCObject->szDescriptionShort);
    }

    if(siCObject->bForceUpgrade)
    {
      /* add the "(Required)" string (or something equivalent) after the component description */
      if(*szSTRRequired != '\0')
      {
        dwBufSize += 1; // space after the short description
        dwBufSize += strlen(szSTRRequired);
      }
    }

    if(siCObject->dwAttributes & SIC_SELECTED)
      dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, FALSE, AC_ALL);
  }
  dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

  /* destination path */
  if(GetPrivateProfileString("Messages", "STR_DESTINATION_DIRECTORY", "", szBuf, sizeof(szBuf), szFileIniInstall))
    dwBufSize += strlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

  dwBufSize += 4; // take into account 4 indentation spaces
  dwBufSize += strlen(sgProduct.szPath) + 2; // the extra 2 bytes is for the \r\n characters
  dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

  /* program folder */
  if(GetPrivateProfileString("Messages", "STR_PROGRAM_FOLDER", "", szBuf, sizeof(szBuf), szFileIniInstall))
    dwBufSize += strlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

  dwBufSize += 4; // take into account 4 indentation spaces
  dwBufSize += strlen(sgProduct.szProgramFolderName) + 2; // the extra 2 bytes is for the \r\n\r\n characters

  if(GetTotalArchivesToDownload() > 0)
  {
    dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

    /* site selector info */
    if(GetPrivateProfileString("Messages", "STR_DOWNLOAD_SITE", "", szBuf, sizeof(szBuf), szFileIniInstall))
      dwBufSize += strlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

    dwBufSize += 4; // take into account 4 indentation spaces
    dwBufSize += strlen(szSiteSelectorDescription) + 2; // the extra 2 bytes is for the \r\n characters

    if(diAdditionalOptions.bSaveInstaller)
    {
      dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

      /* site selector info */
      if(GetPrivateProfileString("Messages", "STR_SAVE_INSTALLER_FILES", "", szBuf, sizeof(szBuf), szFileIniInstall))
        dwBufSize += strlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

      GetSaveInstallerPath(szBuf, sizeof(szBuf));
      dwBufSize += 4; // take into account 4 indentation spaces
      dwBufSize += strlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters
    }
  }

  dwBufSize += 1; // take into account the null character


  /* From here down, the buffer is created given the above calculated buffer size.  If the 
   * string concatenation (addition) is changed below, then the buffer size calculation above
   * needs to be changed accordingly! */

  /* allocate the memory */
  if((szMessageBuf = NS_GlobalAlloc(dwBufSize)) != NULL)
  {
    memset(szMessageBuf, 0, dwBufSize);

    /* Setup Type */
    if(GetPrivateProfileString("Messages", "STR_SETUP_TYPE", "", szBuf, sizeof(szBuf), szFileIniInstall))
    {
      strcat(szMessageBuf, szBuf);
      strcat(szMessageBuf, "\r\n");
    }
    strcat(szMessageBuf, "    "); // add 4 indentation spaces
      
    switch(ulSetupType)
    {
      case ST_RADIO3:
        AppendStringWOTilde(szMessageBuf, dwBufSize, diSetupType.stSetupType3.szDescriptionShort);
        break;

      case ST_RADIO2:
        AppendStringWOTilde(szMessageBuf, dwBufSize, diSetupType.stSetupType2.szDescriptionShort);
        break;

      case ST_RADIO1:
        AppendStringWOTilde(szMessageBuf, dwBufSize, diSetupType.stSetupType1.szDescriptionShort);
        break;

      default:
        AppendStringWOTilde(szMessageBuf, dwBufSize, diSetupType.stSetupType0.szDescriptionShort);
        break;
    }
    strcat(szMessageBuf, "\r\n\r\n");

    /* Selected Components */
    if(GetPrivateProfileString("Messages", "STR_SELECTED_COMPONENTS", "", szBuf, sizeof(szBuf), szFileIniInstall))
    {
      strcat(szMessageBuf, szBuf);
      strcat(szMessageBuf, "\r\n");
    }

    dwIndex0  = 0;
    siCObject = SiCNodeGetObject(dwIndex0, FALSE, AC_ALL);
    while(siCObject)
    {
      if(siCObject->dwAttributes & SIC_SELECTED)
      {
        strcat(szMessageBuf, "    "); // add 4 indentation spaces
        strcat(szMessageBuf, siCObject->szDescriptionShort);
      }

      if(siCObject->bForceUpgrade)
      {
        /* add the "(Required)" string (or something equivalent) after the component description */
        if(*szSTRRequired != '\0')
        {
          strcat(szMessageBuf, " "); // add 1 space
          strcat(szMessageBuf, szSTRRequired);
        }
      }

      if(siCObject->dwAttributes & SIC_SELECTED)
        strcat(szMessageBuf, "\r\n");

      ++dwIndex0;
      siCObject = SiCNodeGetObject(dwIndex0, FALSE, AC_ALL);
    }
    strcat(szMessageBuf, "\r\n");

    /* destination directory */
    if(GetPrivateProfileString("Messages", "STR_DESTINATION_DIRECTORY", "", szBuf, sizeof(szBuf), szFileIniInstall))
    {
      strcat(szMessageBuf, szBuf);
      strcat(szMessageBuf, "\r\n");
    }
    strcat(szMessageBuf, "    "); // add 4 indentation spaces
    strcat(szMessageBuf, sgProduct.szPath);
    strcat(szMessageBuf, "\r\n\r\n");

    /* program folder */
    if(GetPrivateProfileString("Messages", "STR_PROGRAM_FOLDER", "", szBuf, sizeof(szBuf), szFileIniInstall))
    {
      strcat(szMessageBuf, szBuf);
      strcat(szMessageBuf, "\r\n");
    }
    strcat(szMessageBuf, "    "); // add 4 indentation spaces
    strcat(szMessageBuf, sgProduct.szProgramFolderName);
    strcat(szMessageBuf, "\r\n");

    if(GetTotalArchivesToDownload() > 0)
    {
      strcat(szMessageBuf, "\r\n");

      /* site selector info */
      if(GetPrivateProfileString("Messages", "STR_DOWNLOAD_SITE", "", szBuf, sizeof(szBuf), szFileIniInstall))
      {
        strcat(szMessageBuf, szBuf);
        strcat(szMessageBuf, "\r\n");
      }

      strcat(szMessageBuf, "    "); // add 4 indentation spaces
      strcat(szMessageBuf, szSiteSelectorDescription); // site selector description
      strcat(szMessageBuf, "\r\n");

      if(diAdditionalOptions.bSaveInstaller)
      {
        strcat(szMessageBuf, "\r\n");

        /* site selector info */
        if(GetPrivateProfileString("Messages", "STR_SAVE_INSTALLER_FILES", "", szBuf, sizeof(szBuf), szFileIniInstall))
        {
          strcat(szMessageBuf, szBuf);
          strcat(szMessageBuf, "\r\n");
        }

        GetSaveInstallerPath(szBuf, sizeof(szBuf));
        strcat(szMessageBuf, "    "); // add 4 indentation spaces
        strcat(szMessageBuf, szBuf);
        strcat(szMessageBuf, "\r\n");
      }
    }
  }

  return(szMessageBuf);
}

#ifdef OLDCODE
// XXX also defined in extra.c, need to factor out

#define SETUP_STATE_REG_KEY "Software\\%s\\%s\\%s\\Setup"

LRESULT CALLBACK DlgProcQuickLaunch(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  RECT  rDlg;
  LPSTR szMessage = NULL;
  char  szBuf[MAX_BUF];

  switch(msg)
  {
    case WM_INITDLG:
      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diQuickLaunch.szTitle);

      GetPrivateProfileString("Strings", "IDC Turbo Mode", "", szBuf, sizeof(szBuf), szFileIniConfig);
      WinSetDlgItemText(hDlg, IDC_CHECK_TURBO_MODE, szBuf);
      WinSetDlgItemText(hDlg, IDC_STATIC, sgInstallGui.szCurrentSettings);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
      SendDlgItemMessage (hDlg, IDC_STATIC, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE2, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_CHECK_TURBO_MODE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.lScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.lScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      {
        WinSetDlgItemText(hDlg, IDC_MESSAGE0, diQuickLaunch.szMessage0);
        WinSetDlgItemText(hDlg, IDC_MESSAGE1, diQuickLaunch.szMessage1);
        WinSetDlgItemText(hDlg, IDC_MESSAGE2, diQuickLaunch.szMessage2);
      }

      if(diQuickLaunch.bTurboModeEnabled)
        WinShowWindow(WinWindowFromID(hDlg, IDC_CHECK_TURBO_MODE),  TRUE);
      else
      {
        WinShowWindow(WinWindowFromID(hDlg, IDC_CHECK_TURBO_MODE),  FALSE);
        diQuickLaunch.bTurboMode = FALSE;
      }

      if(diQuickLaunch.bTurboMode)
        WinCheckButton(hDlg, IDC_CHECK_TURBO_MODE, 1);
      else
        WinCheckButton(hDlg, IDC_CHECK_TURBO_MODE, 0);

      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDWIZNEXT:
          if(diQuickLaunch.bTurboModeEnabled)
          {
            if(WinQueryButtonCheckstate(hDlg, IDC_CHECK_TURBO_MODE) == 1)
              diQuickLaunch.bTurboMode = TRUE;
            else
              diQuickLaunch.bTurboMode = FALSE;
          }
 
          WinDestroyWindow(hDlg);
          DlgSequence(NEXT_DLG);
          break;

        case IDWIZBACK:
          /* remember the last state of the TurboMode checkbox */
          if(WinQueryButtonCheckstate(hDlg, IDC_CHECK_TURBO_MODE) == 1) {
            diQuickLaunch.bTurboMode = TRUE;
          }
          else {
            diQuickLaunch.bTurboMode = FALSE;
          }
          WinDestroyWindow(hDlg);
          DlgSequence(PREV_DLG);
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}
#endif

MRESULT EXPENTRY DlgProcStartInstall(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  SWP   swpDlg;
  LPSTR szMessage = NULL;

  switch(msg)
  {
    case WM_INITDLG:
//      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diStartInstall.szTitle);

      WinSetDlgItemText(hDlg, IDC_STATIC, sgInstallGui.szCurrentSettings);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szInstall_);
      WinSetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
#ifdef OLDCODE
      SendDlgItemMessage (hDlg, IDC_STATIC, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_CURRENT_SETTINGS, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
#endif
 
      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (gSystemInfo.lScreenX/2)-(swpDlg.cx/2),
                      (gSystemInfo.lScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);

      if((diAdvancedSettings.bShowDialog == FALSE) || (GetTotalArchivesToDownload() == 0))
      {
        WinShowWindow(WinWindowFromID(hDlg, IDC_BUTTON_SITE_SELECTOR), FALSE);
        WinSetDlgItemText(hDlg, IDC_MESSAGE0, diStartInstall.szMessageInstall);
      }
      else
      {
        WinShowWindow(WinWindowFromID(hDlg, IDC_BUTTON_SITE_SELECTOR), TRUE);
        WinSetDlgItemText(hDlg, IDC_MESSAGE0, diStartInstall.szMessageDownload);
      }

      if((szMessage = GetStartInstallMessage()) != NULL)
      {
        WinSetDlgItemText(hDlg, IDC_CURRENT_SETTINGS, szMessage);
        FreeMemory(&szMessage);
      }

      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDWIZNEXT:
          WinDestroyWindow(hDlg);
          DlgSequence(NEXT_DLG);
          break;

        case IDWIZBACK:
          WinDestroyWindow(hDlg);
          DlgSequence(PREV_DLG);
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

MRESULT EXPENTRY DlgProcReboot(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  HANDLE            hToken;
  HWND              hRadioYes;
  SWP               swpDlg;

  hRadioYes = WinWindowFromID(hDlg, IDC_RADIO_YES);

  switch(msg)
  {
    case WM_INITDLG:
//      DisableSystemMenuItems(hDlg, FALSE);
      WinCheckButton(hDlg, IDC_RADIO_YES, 1);
      WinSetFocus(HWND_DESKTOP, hRadioYes);

      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (gSystemInfo.lScreenX/2)-(swpDlg.cx/2),
                      (gSystemInfo.lScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);

      WinSetDlgItemText(hDlg, 202, sgInstallGui.szSetupMessage);
      WinSetDlgItemText(hDlg, IDC_RADIO_YES, sgInstallGui.szYesRestart);
      WinSetDlgItemText(hDlg, IDC_RADIO_NO, sgInstallGui.szNoRestart);
      WinSetDlgItemText(hDlg, DID_OK, sgInstallGui.szOk);
#ifdef OLDCODE
      SendDlgItemMessage (hDlg, 202, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_RADIO_YES, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_RADIO_NO, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, DID_OK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
#endif
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case DID_OK:
          if(WinQueryButtonCheckstate(hDlg, IDC_RADIO_YES) == 1)
          {
            WinDestroyWindow(hDlg);
            WinPostQueueMsg(0, WM_QUIT, 0, 0);
            WinDestroyWindow(hWndMain);

            // Reboot the system and force all applications to close.
            WinShutdownSystem(0, 0);
          }
          else
          {
            WinDestroyWindow(hDlg);
            WinPostQueueMsg(0, WM_QUIT, 0, 0);
          }
          break;

        case IDCANCEL:
          WinDestroyWindow(hDlg);
          WinPostQueueMsg(0, WM_QUIT, 0, 0);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

MRESULT EXPENTRY DlgProcMessage(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  SWP       swpDlg;
  HWND      hSTMessage = WinWindowFromID(hDlg, IDC_MESSAGE); /* handle to the Static Text message window */
  HPS       hpsSTMessage;
  RECTL     rectlString;
  char      szBuf[MAX_BUF];
  char      szBuf2[MAX_BUF];

  memset(szBuf, 0, sizeof(szBuf));
  memset(szBuf2, 0, sizeof(szBuf2));

  switch(msg)
  {
    case WM_INITDLG:
      if(GetPrivateProfileString("Messages", "STR_MESSAGEBOX_TITLE", "", szBuf2, sizeof(szBuf2), szFileIniInstall))
      {
        if((sgProduct.szProductName != NULL) && (*sgProduct.szProductName != '\0'))
          sprintf(szBuf, szBuf2, sgProduct.szProductName);
        else
          sprintf(szBuf, szBuf2, "");
      }
      else if((sgProduct.szProductName != NULL) && (*sgProduct.szProductName != '\0'))
        strcpy(szBuf, sgProduct.szProductName);

      WinSetWindowText(hDlg, szBuf);
      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (gSystemInfo.lScreenX/2)-(swpDlg.cx/2),
                      (gSystemInfo.lScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);

      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDC_MESSAGE:
          hpsSTMessage = WinGetPS(hSTMessage);

          rectlString.xLeft = 0;
          rectlString.xRight = 10000;
          rectlString.yTop = 10000;
          rectlString.yBottom = 0;
          WinDrawText(hpsSTMessage, strlen((char*)mp2), (char*)mp2,
                            &rectlString, 0, 0,
                            DT_BOTTOM | DT_QUERYEXTENT | DT_TEXTATTRS | DT_WORDBREAK);
          WinReleasePS(hpsSTMessage);

          WinSetWindowPos(hDlg, HWND_TOP,
                      (gSystemInfo.lScreenX/2)-((rectlString.xRight + 55)/2),
                      (gSystemInfo.lScreenY/2)-((rectlString.yTop + 50)/2),
                      rectlString.xRight + 55,
                      rectlString.yTop + 50,
                      SWP_SHOW);

          WinQueryWindowPos(hDlg, &swpDlg);

          WinSetWindowPos(hSTMessage,
                         HWND_TOP,
                         swpDlg.x,
                         swpDlg.y,
                         swpDlg.cx,
                         swpDlg.cy,
                         SWP_SHOW);

          WinSetDlgItemText(hDlg, IDC_MESSAGE, (PSZ)mp2);
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

void ProcessWindowsMessages()
{
#ifdef OLDCODE /* @MAK no idea what this does */
  MSG msg;

  while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
#endif
}

void ShowMessage(PSZ szMessage, BOOL bShow)
{
  char szBuf[MAX_BUF];
 
  if(sgProduct.ulMode != SILENT)
  {
    if((bShow) && (hDlgMessage == NULL))
    {
      memset(szBuf, 0, sizeof(szBuf));
      GetPrivateProfileString("Messages", "MB_MESSAGE_STR", "", szBuf, sizeof(szBuf), szFileIniInstall);
      hDlgMessage = InstantiateDialog(hWndMain, DLG_MESSAGE, szBuf, DlgProcMessage);
      WinSendMsg(hDlgMessage, WM_COMMAND, IDC_MESSAGE, (LPARAM)szMessage);
      WinSetPresParam(hDlgMessage, PP_FONTNAMESIZE, strlen(sgInstallGui.szDefinedFont) + 1, sgInstallGui.szDefinedFont);
    }
    else if(!bShow && hDlgMessage)
    {
      WinDestroyWindow(hDlgMessage);
      hDlgMessage = NULL;
    }
  }
}

HWND InstantiateDialog(HWND hParent, ULONG ulDlgID, PSZ szTitle, PFNWP pfnwpDlgProc)
{
  char szBuf[MAX_BUF];
  HWND hDlg = NULL;


  if(hDlg = WinLoadDlg(HWND_DESKTOP, hParent, pfnwpDlgProc, hSetupRscInst, ulDlgID, NULL) == NULL)
  {
    char szEDialogCreate[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_DIALOG_CREATE", "", szEDialogCreate, sizeof(szEDialogCreate), szFileIniInstall))
    {
      sprintf(szBuf, szEDialogCreate, szTitle);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    WinPostQueueMsg(NULL, WM_QUIT, 1, 0);
  }

  return(hDlg);
}

#ifdef OLDCODE
/*
 * Check to see if turbo is enabled.  If so, do the following:
 *   * Log the turbo status that use had chosen.
 *   * Set the appropriate Windows registry keys/values.
 */
void SetTurboArgs(void)
{
  char szData[MAX_BUF];
  char szKey[MAX_BUF];

  if(diQuickLaunch.bTurboModeEnabled)
  {
    /* log if the user selected the turbo mode or not */
    LogISTurboMode(diQuickLaunch.bTurboMode);
    LogMSTurboMode(diQuickLaunch.bTurboMode);

    if(diQuickLaunch.bTurboMode)
      strcpy( szData, "turbo=yes" );
    else
      strcpy( szData, "turbo=no" );

    sprintf(szKey,
             SETUP_STATE_REG_KEY,
             sgProduct.szCompanyName,
             sgProduct.szProductNameInternal,
             sgProduct.szUserAgent);
    AppendWinReg(HKEY_CURRENT_USER,
                 szKey,
                 "browserargs",
                 REG_SZ,
                 szData,
                 0,
                 strlen( szData ) + 1,
                 FALSE,
                 FALSE );
  }
}
#endif

void DlgSequence(int iDirection)
{
  HRESULT hrValue;
  BOOL    bDone = FALSE;

  do
  {
    gbProcessingXpnstallFiles = FALSE;

    if(iDirection == NEXT_DLG)
    {
      switch(ulWizardState)
      {
        case DLG_NONE:
          ulWizardState = DLG_WELCOME;
          break;
        case DLG_WELCOME:
          ulWizardState = DLG_LICENSE;
          break;
        case DLG_LICENSE:
          ulWizardState = DLG_SETUP_TYPE;
          break;
        case DLG_SETUP_TYPE:
          ulWizardState = DLG_SELECT_COMPONENTS;
          break;
        case DLG_SELECT_COMPONENTS:
          ulWizardState = DLG_SELECT_ADDITIONAL_COMPONENTS;
          break;
        case DLG_SELECT_ADDITIONAL_COMPONENTS:
          ulWizardState = DLG_WINDOWS_INTEGRATION;
          break;
        case DLG_WINDOWS_INTEGRATION:
          ulWizardState = DLG_PROGRAM_FOLDER;
          break;
        case DLG_PROGRAM_FOLDER:
          ulWizardState = DLG_QUICK_LAUNCH;
          break;
        case DLG_QUICK_LAUNCH:
          ulWizardState = DLG_ADDITIONAL_OPTIONS;
          break;
        case DLG_ADDITIONAL_OPTIONS:
          ulWizardState = DLG_START_INSTALL;
          break;
        case DLG_START_INSTALL:
          ulWizardState = DLG_COMMIT_INSTALL;
          break;

        case DLG_ADVANCED_SETTINGS:
          ulWizardState = DLG_ADDITIONAL_OPTIONS;
          break;

        default:
          ulWizardState = DLG_WELCOME;
          break;      }
    }
    else if(iDirection == PREV_DLG)
    {
      switch(ulWizardState)
      {
        case DLG_LICENSE:
          ulWizardState = DLG_WELCOME;
          break;
        case DLG_SETUP_TYPE:
          ulWizardState = DLG_LICENSE;
          break;
        case DLG_SELECT_COMPONENTS:
          ulWizardState = DLG_SETUP_TYPE;
          break;
        case DLG_SELECT_ADDITIONAL_COMPONENTS:
          ulWizardState = DLG_SELECT_COMPONENTS;
          break;
        case DLG_WINDOWS_INTEGRATION:
          ulWizardState = DLG_SELECT_ADDITIONAL_COMPONENTS;
          break;
        case DLG_PROGRAM_FOLDER:
          ulWizardState = DLG_WINDOWS_INTEGRATION;
          break;
        case DLG_QUICK_LAUNCH:
          ulWizardState = DLG_PROGRAM_FOLDER;
          break;
        case DLG_ADDITIONAL_OPTIONS:
          ulWizardState = DLG_QUICK_LAUNCH;
          break;
        case DLG_START_INSTALL:
          ulWizardState = DLG_ADDITIONAL_OPTIONS;
          break;

        case DLG_ADVANCED_SETTINGS:
          ulWizardState = DLG_ADDITIONAL_OPTIONS;
          break;

        default:
          ulWizardState = DLG_WELCOME;
          break;
      }
    }
    else if(iDirection == OTHER_DLG_1)
    {
      switch(ulWizardState)
      {
        case DLG_ADDITIONAL_OPTIONS:
          ulWizardState = DLG_ADVANCED_SETTINGS;
          break;

        // You'll get here only if DLG_ADVANCED_SETTINGS is not displayed, which really should
        //   should never be the case unless DLG_ADDITIONAL_OPTIONS is also not displayed, since this
        //   is a button off that dialog.  But if the user turns this off in error, handling the case
        //   will keep from dropping into an infinite loop.
        case DLG_ADVANCED_SETTINGS:
          ulWizardState = DLG_ADDITIONAL_OPTIONS;
          break;
      }
    }

    switch(ulWizardState)
    {
      case DLG_WELCOME:
        if(diWelcome.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, ulWizardState, diWelcome.szTitle, DlgProcWelcome);
          bDone = TRUE;
        }
        break;

      case DLG_LICENSE:
        if(diLicense.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, ulWizardState, diLicense.szTitle, DlgProcLicense);
          bDone = TRUE;
        }
        break;

      case DLG_SETUP_TYPE:
        if(diSetupType.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, ulWizardState, diSetupType.szTitle, DlgProcSetupType);
          bDone = TRUE;
        }
        break;

      case DLG_SELECT_COMPONENTS:
        if((diSelectComponents.bShowDialog) && (sgProduct.ulCustomType == ulSetupType))
        {
          hDlgCurrent = InstantiateDialog(hWndMain, ulWizardState, diSelectComponents.szTitle, DlgProcSelectComponents);
          bDone = TRUE;
        }
        break;

      case DLG_SELECT_ADDITIONAL_COMPONENTS:
        if((diSelectAdditionalComponents.bShowDialog) && (GetAdditionalComponentsCount() > 0))
        {
//          hDlgCurrent = InstantiateDialog(hWndMain, ulWizardState, diSelectAdditionalComponents.szTitle, DlgProcSelectAdditionalComponents);
          bDone = TRUE;
        }
        break;

      case DLG_WINDOWS_INTEGRATION:
        if(diWindowsIntegration.bShowDialog)
        {
//          hDlgCurrent = InstantiateDialog(hWndMain, ulWizardState, diWindowsIntegration.szTitle, DlgProcWindowsIntegration);
          bDone = TRUE;
        }
        break;

      case DLG_PROGRAM_FOLDER:
        if(diProgramFolder.bShowDialog && (sgProduct.ulCustomType == ulSetupType))
        {
//          hDlgCurrent = InstantiateDialog(hWndMain, ulWizardState, diProgramFolder.szTitle, DlgProcProgramFolder);
          bDone = TRUE;
        }
      break;

      case DLG_ADVANCED_SETTINGS:
        if(diAdvancedSettings.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, ulWizardState, diAdvancedSettings.szTitle, DlgProcAdvancedSettings);
          bDone = TRUE;
        }
        break;

     case DLG_QUICK_LAUNCH:
        if(diQuickLaunch.bShowDialog)
        {
//          hDlgCurrent = InstantiateDialog(hWndMain, ulWizardState, diQuickLaunch.szTitle, DlgProcQuickLaunch);
          bDone = TRUE;
        }
        break;

     case DLG_ADDITIONAL_OPTIONS:
        do
        {
          hrValue = VerifyDiskSpace();
          if(hrValue == DID_OK)
          {
            /* show previous visible window */
            iDirection = PREV_DLG;
          }
          else if(hrValue == IDCANCEL)
          {
            AskCancelDlg(hWndMain);
            hrValue = MBID_RETRY;
          }
        }while(hrValue == MBID_RETRY);

        if(hrValue != DID_OK)
        {
          if(ShowAdditionalOptionsDialog() == TRUE)
          {
            hDlgCurrent = InstantiateDialog(hWndMain, ulWizardState, diAdditionalOptions.szTitle, DlgProcAdditionalOptions);
            bDone = TRUE;
          }
        }
        break;

      case DLG_START_INSTALL:
        if(diStartInstall.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, ulWizardState, diStartInstall.szTitle, DlgProcStartInstall);
          bDone = TRUE;
        }
        break;

      case DLG_COMMIT_INSTALL:
        gbProcessingXpnstallFiles = TRUE;
        CommitInstall();
        bDone = TRUE;
        break;
    }
  } while(!bDone);
}

void CommitInstall(void)
{
  HRESULT hrErr;
  char    szDestPath[MAX_BUF];
  char    szInstallLogFile[MAX_BUF];

        LogISDestinationPath();
        LogISSetupType();
        LogISComponentsSelected();
        LogISComponentsToDownload();
        LogISDiskSpace(gdsnComponentDSRequirement);

        strcpy(szDestPath, sgProduct.szPath);
        if(*sgProduct.szSubPath != '\0')
        {
          AppendBackSlash(szDestPath, sizeof(szDestPath));
          strcat(szDestPath, sgProduct.szSubPath);
        }
        AppendBackSlash(szDestPath, sizeof(szDestPath));

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
        strcpy(szInstallLogFile, szTempDir);
        AppendBackSlash(szInstallLogFile, sizeof(szInstallLogFile));
        strcat(szInstallLogFile, FILE_INSTALL_LOG);
        FileCopy(szInstallLogFile, szDestPath, FALSE, FALSE);
        DosDelete(szInstallLogFile);

        /* copy the install_status.log file from the temp\ns_temp dir to
         * the destination dir and use the new destination file to continue
         * logging.
         */
        strcpy(szInstallLogFile, szTempDir);
        AppendBackSlash(szInstallLogFile, sizeof(szInstallLogFile));
        strcat(szInstallLogFile, FILE_INSTALL_STATUS_LOG);
        FileCopy(szInstallLogFile, szDestPath, FALSE, FALSE);
        DosDelete(szInstallLogFile);

        /* PRE_DOWNLOAD process file manipulation functions */
        ProcessFileOpsForAll(T_PRE_DOWNLOAD);

        if(RetrieveArchives() == WIZ_OK)
        {
#ifdef OLDCODE
          /* Check to see if Turbo is required.  If so, set the
           * appropriate Windows registry keys */
          SetTurboArgs();
#endif

          if(gbDownloadTriggered || gbPreviousUnfinishedDownload)
            SetSetupState(SETUP_STATE_UNPACK_XPCOM);

          /* POST_DOWNLOAD process file manipulation functions */
          ProcessFileOpsForAll(T_POST_DOWNLOAD);
          /* PRE_XPCOM process file manipulation functions */
          ProcessFileOpsForAll(T_PRE_XPCOM);

          if(ProcessXpcomFile() != FO_SUCCESS)
          {
            bSDUserCanceled = TRUE;
            CleanupXpcomFile();
            WinPostQueueMsg(0, WM_QUIT, 0, 0);

            return;
          }

          if(gbDownloadTriggered || gbPreviousUnfinishedDownload)
            SetSetupState(SETUP_STATE_INSTALL_XPI); // clears and sets new setup state

          /* POST_XPCOM process file manipulation functions */
          ProcessFileOpsForAll(T_POST_XPCOM);
          /* PRE_SMARTUPDATE process file manipulation functions */
          ProcessFileOpsForAll(T_PRE_SMARTUPDATE);

          /* save the installer files in the local machine */
          if(diAdditionalOptions.bSaveInstaller)
            SaveInstallerFiles();

          if(CheckInstances())
          {
            bSDUserCanceled = TRUE;
            CleanupXpcomFile();
            WinPostQueueMsg(0, WM_QUIT, 0, 0);

            return;
          }

          strcat(szDestPath, "uninstall\\");
          CreateDirectoriesAll(szDestPath, TRUE);

          /* save the installer files in the local machine */
          if(diAdditionalOptions.bSaveInstaller)
            SaveInstallerFiles();

          hrErr = SmartUpdateJars();
          if((hrErr == WIZ_OK) || (hrErr == 999))
          {
            UpdateJSProxyInfo();

            /* POST_SMARTUPDATE process file manipulation functions */
            ProcessFileOpsForAll(T_POST_SMARTUPDATE);
            /* PRE_LAUNCHAPP process file manipulation functions */
            ProcessFileOpsForAll(T_PRE_LAUNCHAPP);

            LaunchApps();

            /* POST_LAUNCHAPP process file manipulation functions */
            ProcessFileOpsForAll(T_POST_LAUNCHAPP);
            /* DEPEND_REBOOT process file manipulation functions */
            ProcessFileOpsForAll(T_DEPEND_REBOOT);

            // Refresh system icons if necessary
            if(gSystemInfo.bRefreshIcons)
              RefreshIcons();

            UnsetSetupState(); // clear setup state
            if(!gbIgnoreProgramFolderX)
              ProcessProgramFolderShowCmd();

#ifdef OLDCODE
            CleanupArgsRegistry();
            CleanupPreviousVersionRegKeys();
#endif
            if(NeedReboot())
            {
              CleanupXpcomFile();
              hDlgCurrent = InstantiateDialog(hWndMain, DLG_RESTART, diReboot.szTitle, DlgProcReboot);
            }
            else
            {
              CleanupXpcomFile();
              WinPostQueueMsg(0, WM_QUIT, 0, 0);
            }
          }
          else
          {
            CleanupXpcomFile();
            WinPostQueueMsg(0, WM_QUIT, 0, 0);
          }
        }
        else
        {
          bSDUserCanceled = TRUE;
          CleanupXpcomFile();
#ifdef OLDCODE
          CleanupArgsRegistry();
#endif
          WinPostQueueMsg(0, WM_QUIT, 0, 0);
        }
        gbProcessingXpnstallFiles = FALSE;
}
