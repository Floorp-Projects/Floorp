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

#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"
#include "xpistub.h"
#include "xpi.h"
#include "logging.h"
#include <shlobj.h>
#include <logkeys.h>

static WNDPROC OldListBoxWndProc;
static BOOL    gbProcessingXpnstallFiles;
static DWORD   gdwACFlag;
static DWORD   gdwIndexLastSelected;

BOOL AskCancelDlg(HWND hDlg)
{
  char szDlgQuitTitle[MAX_BUF];
  char szDlgQuitMsg[MAX_BUF];
  char szMsg[MAX_BUF];
  BOOL bRv = FALSE;

  if((sgProduct.dwMode != SILENT) && (sgProduct.dwMode != AUTO))
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

LRESULT CALLBACK DlgProcWelcome(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  char szBuf[MAX_BUF];
  RECT rDlg;
  
  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diWelcome.szTitle);

      wsprintf(szBuf, diWelcome.szMessage0, sgProduct.szProductName, sgProduct.szProductName);
      SetDlgItemText(hDlg, IDC_STATIC0, szBuf);
      SetDlgItemText(hDlg, IDC_STATIC1, diWelcome.szMessage1);
      SetDlgItemText(hDlg, IDC_STATIC2, diWelcome.szMessage2);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      SetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
      SendDlgItemMessage (hDlg, IDC_STATIC0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDC_STATIC1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDC_STATIC2, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L); 
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

LRESULT CALLBACK DlgProcLicense(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  char            szBuf[MAX_BUF];
  LPSTR           szLicenseFilenameBuf = NULL;
  WIN32_FIND_DATA wfdFindFileData;
  DWORD           dwFileSize;
  DWORD           dwBytesRead;
  HANDLE          hFLicense;
  FILE            *fLicense;
  RECT            rDlg;

  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diLicense.szTitle);
      SetDlgItemText(hDlg, IDC_MESSAGE0, diLicense.szMessage0);
      SetDlgItemText(hDlg, IDC_MESSAGE1, diLicense.szMessage1);

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

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      SetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      SetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szAccept_);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szNo_);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_EDIT_LICENSE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          DestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

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
    case WM_INITDIALOG:		
      hwndLBFolders  = GetDlgItem(hDlg, 1121);
      SetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szTempSetupPath);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      OldListBoxWndProc    = SubclassWindow(hwndLBFolders, (WNDPROC)ListBoxBrowseWndProc);
      gdwIndexLastSelected = SendDlgItemMessage(hDlg, 1121, LB_GETCURSEL, 0, (LPARAM)0);

      SetWindowText(hDlg, sgInstallGui.szSelectDirectory);
      SetDlgItemText(hDlg, 1092, sgInstallGui.szDirectories_);
      SetDlgItemText(hDlg, 1091, sgInstallGui.szDrives_);
      SetDlgItemText(hDlg, 1, sgInstallGui.szOk);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel);
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
      switch(LOWORD(wParam))
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
                lstrcat(szPath, szBufIndex);
              }

              SendDlgItemMessage(hDlg, 1121, LB_GETTEXT, dwIndex, (LPARAM)szBufIndex);
              AppendBackSlash(szPath, sizeof(szPath));
              lstrcat(szPath, szBufIndex);
            }
            else
            {
              for(dwLoop = 1; dwLoop <= dwIndex; dwLoop++)
              {
                SendDlgItemMessage(hDlg, 1121, LB_GETTEXT, dwLoop, (LPARAM)szBufIndex);
                AppendBackSlash(szPath, sizeof(szPath));
                lstrcat(szPath, szBufIndex);
              }
            }
            SetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szPath);
          }
          break;

        case IDOK:
          GetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szBuf, MAX_BUF);
          if(*szBuf == '\0')
          {
            char szEDestinationPath[MAX_BUF];

            GetPrivateProfileString("Messages", "ERROR_DESTINATION_PATH", "", szEDestinationPath, sizeof(szEDestinationPath), szFileIniInstall);
            MessageBox(hDlg, szEDestinationPath, NULL, MB_OK | MB_ICONEXCLAMATION);
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
              lstrcpy(szBufTemp, "\n\n");
              lstrcat(szBufTemp, szBuf);
              RemoveBackSlash(szBufTemp);
              lstrcat(szBufTemp, "\n\n");
              wsprintf(szBufTemp2, szMsgCreateDirectory, szBufTemp);
            }

            if(MessageBox(hDlg, szBufTemp2, szStrCreateDirectory, MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
              char szBuf2[MAX_PATH];

              if(CreateDirectoriesAll(szBuf, TRUE) == FALSE)
              {
                char szECreateDirectory[MAX_BUF];

                lstrcpy(szBufTemp, "\n\n");
                lstrcat(szBufTemp, sgProduct.szPath);
                RemoveBackSlash(szBufTemp);
                lstrcat(szBufTemp, "\n\n");

                if(GetPrivateProfileString("Messages", "ERROR_CREATE_DIRECTORY", "", szECreateDirectory, sizeof(szECreateDirectory), szFileIniInstall))
                  wsprintf(szBuf, szECreateDirectory, szBufTemp);

                MessageBox(hDlg, szBuf, "", MB_OK | MB_ICONERROR);
                break;
              }

              if(*sgProduct.szSubPath != '\0')
              {
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
            else
            {
              break;
            }
          }

          lstrcpy(szTempSetupPath, szBuf);
          RemoveBackSlash(szTempSetupPath);
          EndDialog(hDlg, 0);
          break;
      }
      break;
  }
  return(0);
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

  ZeroMemory(szDlgBrowseTitle, sizeof(szDlgBrowseTitle));
  GetPrivateProfileString("Messages", "DLGBROWSETITLE", "", szDlgBrowseTitle, sizeof(szDlgBrowseTitle), szFileIniInstall);

  lstrcpy(szSearchPathBuf, szCurrDir);
  if((*szSearchPathBuf != '\0') && ((lstrlen(szSearchPathBuf) != 1) || (*szSearchPathBuf != '\\')))
  {
    RemoveBackSlash(szSearchPathBuf);
    while(FileExists(szSearchPathBuf) == FALSE)
    {
      RemoveBackSlash(szSearchPathBuf);
      ParsePath(szSearchPathBuf, szBuf, sizeof(szBuf), FALSE, PP_PATH_ONLY);
      lstrcpy(szSearchPathBuf, szBuf);
    }
  }

  ZeroMemory(ftitle, sizeof(ftitle));
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
LRESULT CALLBACK DlgProcSetupType(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
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
  RECT          rDlg;
  char          szBuf[MAX_BUF];
  char          szBufTemp[MAX_BUF];
  char          szBufTemp2[MAX_BUF];

  hRadioSt0   = GetDlgItem(hDlg, IDC_RADIO_ST0);
  hStaticSt0  = GetDlgItem(hDlg, IDC_STATIC_ST0_DESCRIPTION);
  hRadioSt1   = GetDlgItem(hDlg, IDC_RADIO_ST1);
  hStaticSt1  = GetDlgItem(hDlg, IDC_STATIC_ST1_DESCRIPTION);
  hRadioSt2   = GetDlgItem(hDlg, IDC_RADIO_ST2);
  hStaticSt2  = GetDlgItem(hDlg, IDC_STATIC_ST2_DESCRIPTION);
  hRadioSt3   = GetDlgItem(hDlg, IDC_RADIO_ST3);
  hStaticSt3  = GetDlgItem(hDlg, IDC_STATIC_ST3_DESCRIPTION);
  hReadme     = GetDlgItem(hDlg, IDC_README);

  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diSetupType.szTitle);

      hDestinationPath = GetDlgItem(hDlg, IDC_EDIT_DESTINATION); /* handle to the static destination path text window */
      TruncateString(hDestinationPath, szTempSetupPath, szBuf, sizeof(szBuf));

      SetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szBuf);
      SetDlgItemText(hDlg, IDC_STATIC_MSG0, diSetupType.szMessage0);

      if(diSetupType.stSetupType0.bVisible)
      {
        SetDlgItemText(hDlg, IDC_RADIO_ST0, diSetupType.stSetupType0.szDescriptionShort);
        SetDlgItemText(hDlg, IDC_STATIC_ST0_DESCRIPTION, diSetupType.stSetupType0.szDescriptionLong);
        ShowWindow(hRadioSt0, SW_SHOW);
        ShowWindow(hStaticSt0, SW_SHOW);
      }
      else
      {
        ShowWindow(hRadioSt0, SW_HIDE);
        ShowWindow(hStaticSt0, SW_HIDE);
      }

      if(diSetupType.stSetupType1.bVisible)
      {
        SetDlgItemText(hDlg, IDC_RADIO_ST1, diSetupType.stSetupType1.szDescriptionShort);
        SetDlgItemText(hDlg, IDC_STATIC_ST1_DESCRIPTION, diSetupType.stSetupType1.szDescriptionLong);
        ShowWindow(hRadioSt1, SW_SHOW);
        ShowWindow(hStaticSt1, SW_SHOW);
      }
      else
      {
        ShowWindow(hRadioSt1, SW_HIDE);
        ShowWindow(hStaticSt1, SW_HIDE);
      }

      if(diSetupType.stSetupType2.bVisible)
      {
        SetDlgItemText(hDlg, IDC_RADIO_ST2, diSetupType.stSetupType2.szDescriptionShort);
        SetDlgItemText(hDlg, IDC_STATIC_ST2_DESCRIPTION, diSetupType.stSetupType2.szDescriptionLong);
        ShowWindow(hRadioSt2, SW_SHOW);
        ShowWindow(hStaticSt2, SW_SHOW);
      }
      else
      {
        ShowWindow(hRadioSt2, SW_HIDE);
        ShowWindow(hStaticSt2, SW_HIDE);
      }

      if(diSetupType.stSetupType3.bVisible)
      {
        SetDlgItemText(hDlg, IDC_RADIO_ST3, diSetupType.stSetupType3.szDescriptionShort);
        SetDlgItemText(hDlg, IDC_STATIC_ST3_DESCRIPTION, diSetupType.stSetupType3.szDescriptionLong);
        ShowWindow(hRadioSt3, SW_SHOW);
        ShowWindow(hStaticSt3, SW_SHOW);
      }
      else
      {
        ShowWindow(hRadioSt3, SW_HIDE);
        ShowWindow(hStaticSt3, SW_HIDE);
      }

      /* enable the appropriate radio button */
      switch(dwTempSetupType)
      {
        case ST_RADIO0:
          CheckDlgButton(hDlg, IDC_RADIO_ST0, BST_CHECKED);
          SetFocus(hRadioSt0);
          break;

        case ST_RADIO1:
          CheckDlgButton(hDlg, IDC_RADIO_ST1, BST_CHECKED);
          SetFocus(hRadioSt1);
          break;

        case ST_RADIO2:
          CheckDlgButton(hDlg, IDC_RADIO_ST2, BST_CHECKED);
          SetFocus(hRadioSt2);
          break;

        case ST_RADIO3:
          CheckDlgButton(hDlg, IDC_RADIO_ST3, BST_CHECKED);
          SetFocus(hRadioSt3);
          break;
      }

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      if((*diSetupType.szReadmeFilename == '\0') || (FileExists(diSetupType.szReadmeFilename) == FALSE))
        ShowWindow(hReadme, SW_HIDE);
      else
        ShowWindow(hReadme, SW_SHOW);

      SetDlgItemText(hDlg, IDC_DESTINATION, sgInstallGui.szDestinationDirectory);
      SetDlgItemText(hDlg, IDC_BUTTON_BROWSE, sgInstallGui.szBrowse_);
      SetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      SetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
      SetDlgItemText(hDlg, IDC_README, sgInstallGui.szReadme_);
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

      if(sgProduct.bLockPath)
        EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_BROWSE), FALSE);
      else
        EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_BROWSE), TRUE);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_BUTTON_BROWSE:
          if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST0)      == BST_CHECKED)
            dwTempSetupType = ST_RADIO0;
          else if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST1) == BST_CHECKED)
            dwTempSetupType = ST_RADIO1;
          else if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST2) == BST_CHECKED)
            dwTempSetupType = ST_RADIO2;
          else if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST3) == BST_CHECKED)
            dwTempSetupType = ST_RADIO3;

          BrowseForDirectory(hDlg, szTempSetupPath);

          hDestinationPath = GetDlgItem(hDlg, IDC_EDIT_DESTINATION); /* handle to the static destination path text window */
          TruncateString(hDestinationPath, szTempSetupPath, szBuf, sizeof(szBuf));
          SetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szBuf);
          break;

        case IDC_README:
          if(*diSetupType.szReadmeApp == '\0')
            WinSpawn(diSetupType.szReadmeFilename, NULL, szSetupDir, SW_SHOWNORMAL, FALSE);
          else
            WinSpawn(diSetupType.szReadmeApp, diSetupType.szReadmeFilename, szSetupDir, SW_SHOWNORMAL, FALSE);
          break;

        case IDWIZNEXT:
          lstrcpy(sgProduct.szPath, szTempSetupPath);

          /* append a backslash to the path because CreateDirectoriesAll()
             uses a backslash to determine directories */
          lstrcpy(szBuf, sgProduct.szPath);
          AppendBackSlash(szBuf, sizeof(szBuf));

          if(FileExists(szBuf) == FALSE)
          {
            char szMsgCreateDirectory[MAX_BUF];
            char szStrCreateDirectory[MAX_BUF];

            GetPrivateProfileString("Messages", "STR_CREATE_DIRECTORY", "", szStrCreateDirectory, sizeof(szStrCreateDirectory), szFileIniInstall);
            if(GetPrivateProfileString("Messages", "MSG_CREATE_DIRECTORY", "", szMsgCreateDirectory, sizeof(szMsgCreateDirectory), szFileIniInstall))
            {
              lstrcpy(szBufTemp, "\n\n");
              lstrcat(szBufTemp, szBuf);
              RemoveBackSlash(szBufTemp);
              lstrcat(szBufTemp, "\n\n");
              wsprintf(szBufTemp2, szMsgCreateDirectory, szBufTemp);
            }

            if(MessageBox(hDlg, szBufTemp2, szStrCreateDirectory, MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
              char szBuf2[MAX_PATH];

              if(CreateDirectoriesAll(szBuf, TRUE) == FALSE)
              {
                char szECreateDirectory[MAX_BUF];

                lstrcpy(szBufTemp, "\n\n");
                lstrcat(szBufTemp, sgProduct.szPath);
                RemoveBackSlash(szBufTemp);
                lstrcat(szBufTemp, "\n\n");

                if(GetPrivateProfileString("Messages", "ERROR_CREATE_DIRECTORY", "", szECreateDirectory, sizeof(szECreateDirectory), szFileIniInstall))
                  wsprintf(szBuf, szECreateDirectory, szBufTemp);

                MessageBox(hDlg, szBuf, "", MB_OK | MB_ICONERROR);
                break;
              }

              if(*sgProduct.szSubPath != '\0')
              {
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
            else
            {
              break;
            }
          }

          /* retrieve and save the state of the selected radio button */
          if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST0)      == BST_CHECKED)
          {
            SiCNodeSetItemsSelected(ST_RADIO0);

            dwSetupType     = ST_RADIO0;
            dwTempSetupType = dwSetupType;
          }
          else if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST1) == BST_CHECKED)
          {
            SiCNodeSetItemsSelected(ST_RADIO1);

            dwSetupType     = ST_RADIO1;
            dwTempSetupType = dwSetupType;
          }
          else if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST2) == BST_CHECKED)
          {
            SiCNodeSetItemsSelected(ST_RADIO2);

            dwSetupType     = ST_RADIO2;
            dwTempSetupType = dwSetupType;
          }
          else if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST3) == BST_CHECKED)
          {
            SiCNodeSetItemsSelected(ST_RADIO3);

            dwSetupType     = ST_RADIO3;
            dwTempSetupType = dwSetupType;
          }

          /* set the next dialog to be shown depending on the 
             what the user selected */
          dwWizardState = DLG_SETUP_TYPE;
          CheckWizardStateCustom(DLG_ADVANCED_SETTINGS);

          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          dwTempSetupType = dwSetupType;
          lstrcpy(szTempSetupPath, sgProduct.szPath);
          DestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDCANCEL:
          lstrcpy(sgProduct.szPath, szTempSetupPath);
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
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

  dwItem = SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)siCComponent->szDescriptionShort);
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

  // set the right coordinate of the rectangle to be the same
  // as the right edge of the bitmap drawn.
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
      SiCNodeSetAttributes(dwIndex, SIC_SELECTED, FALSE, FALSE, dwACFlag);
      szToggledReferenceName = SiCNodeGetReferenceName(dwIndex, FALSE, dwACFlag);
      ResolveDependees(szToggledReferenceName);
    }
    else
    {
      SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE, FALSE, dwACFlag);
      bMoreToResolve = ResolveDependencies(dwIndex);

      while(bMoreToResolve)
        bMoreToResolve = ResolveDependencies(-1);

      szToggledReferenceName = SiCNodeGetReferenceName(dwIndex, FALSE, dwACFlag);
      ResolveDependees(szToggledReferenceName);
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
  DWORD               dwPosX;
  DWORD               dwPosY;
  DWORD               dwIndex;

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
  BOOL                bReturn = FALSE;
  siC                 *siCTemp;
  DWORD               dwIndex;
  DWORD               dwItems = MAX_BUF;
  HWND                hwndLBComponents;
  RECT                rDlg;
  TCHAR               tchBuffer[MAX_BUF];
  TEXTMETRIC          tm;
  DWORD               y;
  LPDRAWITEMSTRUCT    lpdis;
  ULONGLONG           ullDSBuf;
  char                szBuf[MAX_BUF];

  hwndLBComponents  = GetDlgItem(hDlg, IDC_LIST_COMPONENTS);

  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diSelectComponents.szTitle);
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

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      /* update the disk space available info in the dialog.  GetDiskSpaceAvailable()
         returns value in kbytes */
      ullDSBuf = GetDiskSpaceAvailable(sgProduct.szPath);
      _ui64toa(ullDSBuf, tchBuffer, 10);
      ParsePath(sgProduct.szPath, szBuf, sizeof(szBuf), FALSE, PP_ROOT_ONLY);
      RemoveBackSlash(szBuf);
      lstrcat(szBuf, " - ");
      lstrcat(szBuf, tchBuffer);
      lstrcat(szBuf, " KB");
      SetDlgItemText(hDlg, IDC_SPACE_AVAILABLE, szBuf);

      SetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szComponents_);
      SetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szDescription);
      SetDlgItemText(hDlg, IDC_STATIC3, sgInstallGui.szTotalDownloadSize);
      SetDlgItemText(hDlg, IDC_STATIC4, sgInstallGui.szSpaceAvailable);
      SetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      SetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
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

      gdwACFlag = AC_COMPONENTS;
      OldListBoxWndProc = SubclassWindow(hwndLBComponents, (WNDPROC)NewListBoxWndProc);
      break;

    case WM_DRAWITEM:
      lpdis = (LPDRAWITEMSTRUCT)lParam;

      // If there are no list box items, skip this message.
      if(lpdis->itemID == -1)
        break;

      SendMessage(lpdis->hwndItem, LB_GETTEXT, lpdis->itemID, (LPARAM)tchBuffer);
      if((lpdis->itemAction & ODA_FOCUS) && (lpdis->itemState & ODS_SELECTED))
      {
        // remove the focus rect on the previous selected item
        DrawFocusRect(lpdis->hDC, &(lpdis->rcItem));
      }

      siCTemp = SiCNodeGetObject(lpdis->itemID, FALSE, AC_COMPONENTS);
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

      if(lpdis->itemAction & (ODA_DRAWENTIRE | ODA_FOCUS))
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
      lstrcpy(szBuf, tchBuffer);
      lstrcat(szBuf, " KB");
      
      SetDlgItemText(hDlg, IDC_DOWNLOAD_SIZE, szBuf);
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_LIST_COMPONENTS:
          /* to update the long description for each component the user selected */
          if((dwIndex = SendMessage(hwndLBComponents, LB_GETCURSEL, 0, 0)) != LB_ERR)
            SetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(dwIndex, FALSE, AC_COMPONENTS));
          break;

        case IDWIZNEXT:
          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          DestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }

  return(bReturn);
}

LRESULT CALLBACK DlgProcSelectAdditionalComponents(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  BOOL                bReturn = FALSE;
  siC                 *siCTemp;
  DWORD               dwIndex;
  DWORD               dwItems = MAX_BUF;
  HWND                hwndLBComponents;
  RECT                rDlg;
  TCHAR               tchBuffer[MAX_BUF];
  TEXTMETRIC          tm;
  DWORD               y;
  LPDRAWITEMSTRUCT    lpdis;
  ULONGLONG           ullDSBuf;
  char                szBuf[MAX_BUF];

  hwndLBComponents  = GetDlgItem(hDlg, IDC_LIST_COMPONENTS);

  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diSelectAdditionalComponents.szTitle);
      SetDlgItemText(hDlg, IDC_MESSAGE0, diSelectAdditionalComponents.szMessage0);

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
        SetFocus(hwndLBComponents);
        SendMessage(hwndLBComponents, LB_SETCURSEL, 0, 0);
        SetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(0, FALSE, AC_ADDITIONAL_COMPONENTS));
      }

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      /* update the disk space available info in the dialog.  GetDiskSpaceAvailable()
         returns value in kbytes */
      ullDSBuf = GetDiskSpaceAvailable(sgProduct.szPath);
      _ui64toa(ullDSBuf, tchBuffer, 10);
      ParsePath(sgProduct.szPath, szBuf, sizeof(szBuf), FALSE, PP_ROOT_ONLY);
      RemoveBackSlash(szBuf);
      lstrcat(szBuf, " - ");
      lstrcat(szBuf, tchBuffer);
      lstrcat(szBuf, " KB");
      SetDlgItemText(hDlg, IDC_SPACE_AVAILABLE, szBuf);

      SetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szAdditionalComponents_);
      SetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szDescription);
      SetDlgItemText(hDlg, IDC_STATIC3, sgInstallGui.szTotalDownloadSize);
      SetDlgItemText(hDlg, IDC_STATIC4, sgInstallGui.szSpaceAvailable);
      SetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      SetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
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

      gdwACFlag = AC_ADDITIONAL_COMPONENTS;
      OldListBoxWndProc = SubclassWindow(hwndLBComponents, (WNDPROC)NewListBoxWndProc);
      break;

    case WM_DRAWITEM:
      lpdis = (LPDRAWITEMSTRUCT)lParam;

      // If there are no list box items, skip this message.
      if(lpdis->itemID == -1)
        break;

      SendMessage(lpdis->hwndItem, LB_GETTEXT, lpdis->itemID, (LPARAM)tchBuffer);
      if((lpdis->itemAction & ODA_FOCUS) && (lpdis->itemState & ODS_SELECTED))
      {
        // remove the focus rect on the previous selected item
        DrawFocusRect(lpdis->hDC, &(lpdis->rcItem));
      }

      siCTemp = SiCNodeGetObject(lpdis->itemID, FALSE, AC_ADDITIONAL_COMPONENTS);
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

      if(lpdis->itemAction & (ODA_DRAWENTIRE | ODA_FOCUS))
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
      lstrcpy(szBuf, tchBuffer);
      lstrcat(szBuf, " KB");
      
      SetDlgItemText(hDlg, IDC_DOWNLOAD_SIZE, szBuf);
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_LIST_COMPONENTS:
          /* to update the long description for each component the user selected */
          if((dwIndex = SendMessage(hwndLBComponents, LB_GETCURSEL, 0, 0)) != LB_ERR)
            SetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(dwIndex, FALSE, AC_ADDITIONAL_COMPONENTS));
          break;

        case IDWIZNEXT:
          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          DestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }

  return(bReturn);
}

LRESULT CALLBACK DlgProcWindowsIntegration(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  HWND hcbCheck0;
  HWND hcbCheck1;
  HWND hcbCheck2;
  HWND hcbCheck3;
  RECT rDlg;

  hcbCheck0 = GetDlgItem(hDlg, IDC_CHECK0);
  hcbCheck1 = GetDlgItem(hDlg, IDC_CHECK1);
  hcbCheck2 = GetDlgItem(hDlg, IDC_CHECK2);
  hcbCheck3 = GetDlgItem(hDlg, IDC_CHECK3);

  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diWindowsIntegration.szTitle);
      SetDlgItemText(hDlg, IDC_MESSAGE0, diWindowsIntegration.szMessage0);
      SetDlgItemText(hDlg, IDC_MESSAGE1, diWindowsIntegration.szMessage1);

      if(diWindowsIntegration.wiCB0.bEnabled)
      {
        ShowWindow(hcbCheck0, SW_SHOW);
        CheckDlgButton(hDlg, IDC_CHECK0, diWindowsIntegration.wiCB0.bCheckBoxState);
        SetDlgItemText(hDlg, IDC_CHECK0, diWindowsIntegration.wiCB0.szDescription);
      }
      else
        ShowWindow(hcbCheck0, SW_HIDE);

      if(diWindowsIntegration.wiCB1.bEnabled)
      {
        ShowWindow(hcbCheck1, SW_SHOW);
        CheckDlgButton(hDlg, IDC_CHECK1, diWindowsIntegration.wiCB1.bCheckBoxState);
        SetDlgItemText(hDlg, IDC_CHECK1, diWindowsIntegration.wiCB1.szDescription);
      }
      else
        ShowWindow(hcbCheck1, SW_HIDE);

      if(diWindowsIntegration.wiCB2.bEnabled)
      {
        ShowWindow(hcbCheck2, SW_SHOW);
        CheckDlgButton(hDlg, IDC_CHECK2, diWindowsIntegration.wiCB2.bCheckBoxState);
        SetDlgItemText(hDlg, IDC_CHECK2, diWindowsIntegration.wiCB2.szDescription);
      }
      else
        ShowWindow(hcbCheck2, SW_HIDE);

      if(diWindowsIntegration.wiCB3.bEnabled)
      {
        ShowWindow(hcbCheck3, SW_SHOW);
        CheckDlgButton(hDlg, IDC_CHECK3, diWindowsIntegration.wiCB3.bCheckBoxState);
        SetDlgItemText(hDlg, IDC_CHECK3, diWindowsIntegration.wiCB3.szDescription);
      }
      else
        ShowWindow(hcbCheck3, SW_HIDE);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      SetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      SetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
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
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          if(IsDlgButtonChecked(hDlg, IDC_CHECK0) == BST_CHECKED)
          {
          }

          if(diWindowsIntegration.wiCB0.bEnabled)
          {
            if(IsDlgButtonChecked(hDlg, IDC_CHECK0) == BST_CHECKED)
              diWindowsIntegration.wiCB0.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB0.bCheckBoxState = FALSE;
          }

          if(diWindowsIntegration.wiCB1.bEnabled)
          {
            if(IsDlgButtonChecked(hDlg, IDC_CHECK1) == BST_CHECKED)
              diWindowsIntegration.wiCB1.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB1.bCheckBoxState = FALSE;
          }

          if(diWindowsIntegration.wiCB2.bEnabled)
          {
            if(IsDlgButtonChecked(hDlg, IDC_CHECK2) == BST_CHECKED)
              diWindowsIntegration.wiCB2.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB2.bCheckBoxState = FALSE;
          }

          if(diWindowsIntegration.wiCB3.bEnabled)
          {
            if(IsDlgButtonChecked(hDlg, IDC_CHECK3) == BST_CHECKED)
              diWindowsIntegration.wiCB3.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB3.bCheckBoxState = FALSE;
          }

          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          dwWizardState = DLG_WINDOWS_INTEGRATION;
          CheckWizardStateCustom(DLG_SETUP_TYPE);

          DestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

LRESULT CALLBACK DlgProcProgramFolder(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  char            szBuf[MAX_BUF];
  HANDLE          hDir;
  DWORD           dwIndex;
  WIN32_FIND_DATA wfdFindFileData;
  RECT            rDlg;

  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diProgramFolder.szTitle);
      SetDlgItemText(hDlg, IDC_MESSAGE0, diProgramFolder.szMessage0);
      SetDlgItemText(hDlg, IDC_EDIT_PROGRAM_FOLDER, sgProduct.szProgramFolderName);

      lstrcpy(szBuf, sgProduct.szProgramFolderPath);
      lstrcat(szBuf, "\\*.*");
      if((hDir = FindFirstFile(szBuf , &wfdFindFileData)) != INVALID_HANDLE_VALUE)
      {
        if((wfdFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (lstrcmpi(wfdFindFileData.cFileName, ".") != 0) && (lstrcmpi(wfdFindFileData.cFileName, "..") != 0))
        {
          SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)wfdFindFileData.cFileName);
        }

        while(FindNextFile(hDir, &wfdFindFileData))
        {
          if((wfdFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (lstrcmpi(wfdFindFileData.cFileName, ".") != 0) && (lstrcmpi(wfdFindFileData.cFileName, "..") != 0))
            SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)wfdFindFileData.cFileName);
        }
        FindClose(hDir);
      }

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      SetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szProgramFolder_);
      SetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szExistingFolder_);
      SetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      SetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
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
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          GetDlgItemText(hDlg, IDC_EDIT_PROGRAM_FOLDER, szBuf, MAX_BUF);
          if(*szBuf == '\0')
          {
            char szEProgramFolderName[MAX_BUF];

            GetPrivateProfileString("Messages", "ERROR_PROGRAM_FOLDER_NAME", "", szEProgramFolderName, sizeof(szEProgramFolderName), szFileIniInstall);
            MessageBox(hDlg, szEProgramFolderName, NULL, MB_OK | MB_ICONEXCLAMATION);
            break;
          }
          lstrcpy(sgProduct.szProgramFolderName, szBuf);
          dwWizardState = DLG_ADVANCED_SETTINGS;

          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          DestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDC_LIST:
          if((dwIndex = SendDlgItemMessage(hDlg, IDC_LIST, LB_GETCURSEL, 0, 0)) != LB_ERR)
          {
            SendDlgItemMessage(hDlg, IDC_LIST, LB_GETTEXT, dwIndex, (LPARAM)szBuf);
            SetDlgItemText(hDlg, IDC_EDIT_PROGRAM_FOLDER, szBuf);
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
  return(0);
}

void SaveDownloadProtocolOption(HWND hDlg)
{
  if(IsDlgButtonChecked(hDlg, IDC_USE_FTP) == BST_CHECKED)
    diDownloadOptions.dwUseProtocol = UP_FTP;
  else if(IsDlgButtonChecked(hDlg, IDC_USE_HTTP) == BST_CHECKED)
    diDownloadOptions.dwUseProtocol = UP_HTTP;
}

LRESULT CALLBACK DlgProcAdvancedSettings(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  RECT  rDlg;
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

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

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

      switch(diDownloadOptions.dwUseProtocol)
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

      if((diDownloadOptions.bShowProtocols) && (diDownloadOptions.bUseProtocolSettings))
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
          dwWizardState = DLG_QUICK_LAUNCH;

          /* get the proxy server and port information */
          GetDlgItemText(hDlg, IDC_EDIT_PROXY_SERVER, diAdvancedSettings.szProxyServer, MAX_BUF);
          GetDlgItemText(hDlg, IDC_EDIT_PROXY_PORT,   diAdvancedSettings.szProxyPort,   MAX_BUF);
          GetDlgItemText(hDlg, IDC_EDIT_PROXY_USER,   diAdvancedSettings.szProxyUser,   MAX_BUF);
          GetDlgItemText(hDlg, IDC_EDIT_PROXY_PASSWD, diAdvancedSettings.szProxyPasswd, MAX_BUF);

          SaveDownloadProtocolOption(hDlg);
          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
        case IDCANCEL:
          DestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

void SaveDownloadOptions(HWND hDlg, HWND hwndCBSiteSelector)
{
  int iIndex;

  /* get selected item from the site selector's pull down list */
  iIndex = SendMessage(hwndCBSiteSelector, CB_GETCURSEL, 0, 0);
  SendMessage(hwndCBSiteSelector, CB_GETLBTEXT, (WPARAM)iIndex, (LPARAM)szSiteSelectorDescription);

  /* get the state of the Save Installer Files checkbox */
  if(IsDlgButtonChecked(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES) == BST_CHECKED)
    diDownloadOptions.bSaveInstaller = TRUE;
  else
    diDownloadOptions.bSaveInstaller = FALSE;
}

LRESULT CALLBACK DlgProcDownloadOptions(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  RECT  rDlg;
  char  szBuf[MAX_BUF];
  HWND  hwndCBSiteSelector;
  int   iIndex;
  ssi   *ssiTemp;
  char  szCBDefault[MAX_BUF];

  hwndCBSiteSelector = GetDlgItem(hDlg, IDC_LIST_SITE_SELECTOR);

  switch(msg)
  {
    case WM_INITDIALOG:
      if(gdwSiteSelectorStatus == SS_HIDE)
      {
        ShowWindow(GetDlgItem(hDlg, IDC_MESSAGE0),  SW_HIDE);
        ShowWindow(GetDlgItem(hDlg, IDC_LIST_SITE_SELECTOR),  SW_HIDE);
      }

      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diDownloadOptions.szTitle);
      SetDlgItemText(hDlg, IDC_MESSAGE0, diDownloadOptions.szMessage0);
      SetDlgItemText(hDlg, IDC_MESSAGE1, diDownloadOptions.szMessage1);

      GetPrivateProfileString("Strings", "IDC Save Installer Files", "", szBuf, sizeof(szBuf), szFileIniConfig);
      SetDlgItemText(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, szBuf);

      GetSaveInstallerPath(szBuf, sizeof(szBuf));
      SetDlgItemText(hDlg, IDC_EDIT_LOCAL_INSTALLER_PATH, szBuf);

      SetDlgItemText(hDlg, IDC_BUTTON_PROXY_SETTINGS, sgInstallGui.szProxySettings_);
      SetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      SetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
      SendDlgItemMessage (hDlg, IDC_BUTTON_PROXY_SETTINGS, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_LIST_SITE_SELECTOR, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_EDIT_LOCAL_INSTALLER_PATH, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      ssiTemp = ssiSiteSelector;
      do
      {
        if(ssiTemp == NULL)
          break;

        SendMessage(hwndCBSiteSelector, CB_ADDSTRING, 0, (LPARAM)(ssiTemp->szDescription));
        ssiTemp = ssiTemp->Next;
      } while(ssiTemp != ssiSiteSelector);

      if((szSiteSelectorDescription == NULL) || (*szSiteSelectorDescription == '\0'))
      {
          if(GetPrivateProfileString("Messages", "CB_DEFAULT", "", szCBDefault, sizeof(szCBDefault), szFileIniInstall) &&
          ((iIndex = SendMessage(hwndCBSiteSelector, CB_SELECTSTRING, -1, (LPARAM)szCBDefault)) != CB_ERR))
          SendMessage(hwndCBSiteSelector, CB_SETCURSEL, (WPARAM)iIndex, 0);
        else
          SendMessage(hwndCBSiteSelector, CB_SETCURSEL, 0, 0);
      }
      else if((iIndex = SendMessage(hwndCBSiteSelector, CB_SELECTSTRING, -1, (LPARAM)szSiteSelectorDescription)) != CB_ERR)
        SendMessage(hwndCBSiteSelector, CB_SETCURSEL, (WPARAM)iIndex, 0);
      else
        SendMessage(hwndCBSiteSelector, CB_SETCURSEL, 0, 0);

      if(diDownloadOptions.bSaveInstaller)
        CheckDlgButton(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, BST_CHECKED);
      else
        CheckDlgButton(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, BST_UNCHECKED);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          SaveDownloadOptions(hDlg, hwndCBSiteSelector);
          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          SaveDownloadOptions(hDlg, hwndCBSiteSelector);
          DestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDC_BUTTON_ADDITIONAL_SETTINGS:
          SaveDownloadOptions(hDlg, hwndCBSiteSelector);
          dwWizardState = DLG_PROGRAM_FOLDER;
          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

void AppendStringWOAmpersand(LPSTR szInputString, DWORD dwInputStringSize, LPSTR szString)
{
  DWORD i;
  DWORD iInputStringCounter;
  DWORD iInputStringLen;
  DWORD iStringLen;


  iInputStringLen = lstrlen(szInputString);
  iStringLen      = lstrlen(szString);

  if((iInputStringLen + iStringLen) >= dwInputStringSize)
    return;

  iInputStringCounter = iInputStringLen;
  for(i = 0; i < iStringLen; i++)
  {
    if(szString[i] != '&')
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
    dwBufSize += lstrlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters
  dwBufSize += 4; // take into account 4 indentation spaces

  switch(dwSetupType)
  {
    case ST_RADIO3:
      dwBufSize += lstrlen(diSetupType.stSetupType3.szDescriptionShort) + 2; // the extra 2 bytes is for the \r\n characters
      break;

    case ST_RADIO2:
      dwBufSize += lstrlen(diSetupType.stSetupType2.szDescriptionShort) + 2; // the extra 2 bytes is for the \r\n characters
      break;

    case ST_RADIO1:
      dwBufSize += lstrlen(diSetupType.stSetupType1.szDescriptionShort) + 2; // the extra 2 bytes is for the \r\n characters
      break;

    default:
      dwBufSize += lstrlen(diSetupType.stSetupType0.szDescriptionShort) + 2; // the extra 2 bytes is for the \r\n characters
      break;
  }
  dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

  /* selected components */
  if(GetPrivateProfileString("Messages", "STR_SELECTED_COMPONENTS", "", szBuf, sizeof(szBuf), szFileIniInstall))
    dwBufSize += lstrlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

  dwIndex0 = 0;
  siCObject = SiCNodeGetObject(dwIndex0, FALSE, AC_ALL);
  while(siCObject)
  {
    if(siCObject->dwAttributes & SIC_SELECTED)
    {
      dwBufSize += 4; // take into account 4 indentation spaces
      dwBufSize += lstrlen(siCObject->szDescriptionShort);
    }

    if(siCObject->bForceUpgrade)
    {
      /* add the "(Required)" string (or something equivalent) after the component description */
      if(*szSTRRequired != '\0')
      {
        dwBufSize += 1; // space after the short description
        dwBufSize += lstrlen(szSTRRequired);
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
    dwBufSize += lstrlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

  dwBufSize += 4; // take into account 4 indentation spaces
  dwBufSize += lstrlen(sgProduct.szPath) + 2; // the extra 2 bytes is for the \r\n characters
  dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

  /* program folder */
  if(GetPrivateProfileString("Messages", "STR_PROGRAM_FOLDER", "", szBuf, sizeof(szBuf), szFileIniInstall))
    dwBufSize += lstrlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

  dwBufSize += 4; // take into account 4 indentation spaces
  dwBufSize += lstrlen(sgProduct.szProgramFolderName) + 2; // the extra 2 bytes is for the \r\n\r\n characters

  if(GetTotalArchivesToDownload() > 0)
  {
    dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

    /* site selector info */
    if(GetPrivateProfileString("Messages", "STR_DOWNLOAD_SITE", "", szBuf, sizeof(szBuf), szFileIniInstall))
      dwBufSize += lstrlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

    dwBufSize += 4; // take into account 4 indentation spaces
    dwBufSize += lstrlen(szSiteSelectorDescription) + 2; // the extra 2 bytes is for the \r\n characters

    if(diDownloadOptions.bSaveInstaller)
    {
      dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

      /* site selector info */
      if(GetPrivateProfileString("Messages", "STR_SAVE_INSTALLER_FILES", "", szBuf, sizeof(szBuf), szFileIniInstall))
        dwBufSize += lstrlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

      GetSaveInstallerPath(szBuf, sizeof(szBuf));
      dwBufSize += 4; // take into account 4 indentation spaces
      dwBufSize += lstrlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters
    }
  }

  dwBufSize += 1; // take into account the null character


  /* From here down, the buffer is created given the above calculated buffer size.  If the 
   * string concatenation (addition) is changed below, then the buffer size calculation above
   * needs to be changed accordingly! */

  /* allocate the memory */
  if((szMessageBuf = NS_GlobalAlloc(dwBufSize)) != NULL)
  {
    ZeroMemory(szMessageBuf, dwBufSize);

    /* Setup Type */
    if(GetPrivateProfileString("Messages", "STR_SETUP_TYPE", "", szBuf, sizeof(szBuf), szFileIniInstall))
    {
      lstrcat(szMessageBuf, szBuf);
      lstrcat(szMessageBuf, "\r\n");
    }
    lstrcat(szMessageBuf, "    "); // add 4 indentation spaces
      
    switch(dwSetupType)
    {
      case ST_RADIO3:
        AppendStringWOAmpersand(szMessageBuf, dwBufSize, diSetupType.stSetupType3.szDescriptionShort);
        break;

      case ST_RADIO2:
        AppendStringWOAmpersand(szMessageBuf, dwBufSize, diSetupType.stSetupType2.szDescriptionShort);
        break;

      case ST_RADIO1:
        AppendStringWOAmpersand(szMessageBuf, dwBufSize, diSetupType.stSetupType1.szDescriptionShort);
        break;

      default:
        AppendStringWOAmpersand(szMessageBuf, dwBufSize, diSetupType.stSetupType0.szDescriptionShort);
        break;
    }
    lstrcat(szMessageBuf, "\r\n\r\n");

    /* Selected Components */
    if(GetPrivateProfileString("Messages", "STR_SELECTED_COMPONENTS", "", szBuf, sizeof(szBuf), szFileIniInstall))
    {
      lstrcat(szMessageBuf, szBuf);
      lstrcat(szMessageBuf, "\r\n");
    }

    dwIndex0  = 0;
    siCObject = SiCNodeGetObject(dwIndex0, FALSE, AC_ALL);
    while(siCObject)
    {
      if(siCObject->dwAttributes & SIC_SELECTED)
      {
        lstrcat(szMessageBuf, "    "); // add 4 indentation spaces
        lstrcat(szMessageBuf, siCObject->szDescriptionShort);
      }

      if(siCObject->bForceUpgrade)
      {
        /* add the "(Required)" string (or something equivalent) after the component description */
        if(*szSTRRequired != '\0')
        {
          lstrcat(szMessageBuf, " "); // add 1 space
          lstrcat(szMessageBuf, szSTRRequired);
        }
      }

      if(siCObject->dwAttributes & SIC_SELECTED)
        lstrcat(szMessageBuf, "\r\n");

      ++dwIndex0;
      siCObject = SiCNodeGetObject(dwIndex0, FALSE, AC_ALL);
    }
    lstrcat(szMessageBuf, "\r\n");

    /* destination directory */
    if(GetPrivateProfileString("Messages", "STR_DESTINATION_DIRECTORY", "", szBuf, sizeof(szBuf), szFileIniInstall))
    {
      lstrcat(szMessageBuf, szBuf);
      lstrcat(szMessageBuf, "\r\n");
    }
    lstrcat(szMessageBuf, "    "); // add 4 indentation spaces
    lstrcat(szMessageBuf, sgProduct.szPath);
    lstrcat(szMessageBuf, "\r\n\r\n");

    /* program folder */
    if(GetPrivateProfileString("Messages", "STR_PROGRAM_FOLDER", "", szBuf, sizeof(szBuf), szFileIniInstall))
    {
      lstrcat(szMessageBuf, szBuf);
      lstrcat(szMessageBuf, "\r\n");
    }
    lstrcat(szMessageBuf, "    "); // add 4 indentation spaces
    lstrcat(szMessageBuf, sgProduct.szProgramFolderName);
    lstrcat(szMessageBuf, "\r\n");

    if(GetTotalArchivesToDownload() > 0)
    {
      lstrcat(szMessageBuf, "\r\n");

      /* site selector info */
      if(GetPrivateProfileString("Messages", "STR_DOWNLOAD_SITE", "", szBuf, sizeof(szBuf), szFileIniInstall))
      {
        lstrcat(szMessageBuf, szBuf);
        lstrcat(szMessageBuf, "\r\n");
      }

      lstrcat(szMessageBuf, "    "); // add 4 indentation spaces
      lstrcat(szMessageBuf, szSiteSelectorDescription); // site selector description
      lstrcat(szMessageBuf, "\r\n");

      if(diDownloadOptions.bSaveInstaller)
      {
        lstrcat(szMessageBuf, "\r\n");

        /* site selector info */
        if(GetPrivateProfileString("Messages", "STR_SAVE_INSTALLER_FILES", "", szBuf, sizeof(szBuf), szFileIniInstall))
        {
          lstrcat(szMessageBuf, szBuf);
          lstrcat(szMessageBuf, "\r\n");
        }

        GetSaveInstallerPath(szBuf, sizeof(szBuf));
        lstrcat(szMessageBuf, "    "); // add 4 indentation spaces
        lstrcat(szMessageBuf, szBuf);
        lstrcat(szMessageBuf, "\r\n");
      }
    }
  }

  return(szMessageBuf);
}

// XXX also defined in extra.c, need to factor out

#define SETUP_STATE_REG_KEY "Software\\%s\\%s\\%s\\Setup"

LRESULT CALLBACK DlgProcQuickLaunch(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  RECT  rDlg;
  LPSTR szMessage = NULL;
  char  szBuf[MAX_BUF];

  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diQuickLaunch.szTitle);

      GetPrivateProfileString("Strings", "IDC Turbo Mode", "", szBuf, sizeof(szBuf), szFileIniConfig);
      SetDlgItemText(hDlg, IDC_CHECK_TURBO_MODE, szBuf);
      SetDlgItemText(hDlg, IDC_STATIC, sgInstallGui.szCurrentSettings);
      SetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      SetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
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
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      {
        SetDlgItemText(hDlg, IDC_MESSAGE0, diQuickLaunch.szMessage0);
        SetDlgItemText(hDlg, IDC_MESSAGE1, diQuickLaunch.szMessage1);
        SetDlgItemText(hDlg, IDC_MESSAGE2, diQuickLaunch.szMessage2);
      }

      if(diQuickLaunch.bTurboModeEnabled)
        ShowWindow(GetDlgItem(hDlg, IDC_CHECK_TURBO_MODE),  SW_SHOW);
      else
      {
        ShowWindow(GetDlgItem(hDlg, IDC_CHECK_TURBO_MODE),  SW_HIDE);
        diQuickLaunch.bTurboMode = FALSE;
      }

      if(diQuickLaunch.bTurboMode)
        CheckDlgButton(hDlg, IDC_CHECK_TURBO_MODE, BST_CHECKED);
      else
        CheckDlgButton(hDlg, IDC_CHECK_TURBO_MODE, BST_UNCHECKED);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          if(diQuickLaunch.bTurboModeEnabled)
          {
            if(IsDlgButtonChecked(hDlg, IDC_CHECK_TURBO_MODE) == BST_CHECKED)
              diQuickLaunch.bTurboMode = TRUE;
            else
              diQuickLaunch.bTurboMode = FALSE;
          }
 
          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          /* remember the last state of the TurboMode checkbox */
          if(IsDlgButtonChecked(hDlg, IDC_CHECK_TURBO_MODE) == BST_CHECKED) {
            diQuickLaunch.bTurboMode = TRUE;
          }
          else {
            diQuickLaunch.bTurboMode = FALSE;
          }
          DestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

LRESULT CALLBACK DlgProcStartInstall(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  RECT  rDlg;
  LPSTR szMessage = NULL;

  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      SetWindowText(hDlg, diStartInstall.szTitle);

      SetDlgItemText(hDlg, IDC_STATIC, sgInstallGui.szCurrentSettings);
      SetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      SetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szInstall_);
      SetDlgItemText(hDlg, IDCANCEL, sgInstallGui.szCancel_);
      SendDlgItemMessage (hDlg, IDC_STATIC, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZBACK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_CURRENT_SETTINGS, WM_SETFONT, (WPARAM)sgInstallGui.systemFont, 0L);
 
      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      if((diAdvancedSettings.bShowDialog == FALSE) || (GetTotalArchivesToDownload() == 0))
      {
        ShowWindow(GetDlgItem(hDlg, IDC_BUTTON_SITE_SELECTOR), SW_HIDE);
        SetDlgItemText(hDlg, IDC_MESSAGE0, diStartInstall.szMessageInstall);
      }
      else
      {
        ShowWindow(GetDlgItem(hDlg, IDC_BUTTON_SITE_SELECTOR), SW_SHOW);
        SetDlgItemText(hDlg, IDC_MESSAGE0, diStartInstall.szMessageDownload);
      }

      if((szMessage = GetStartInstallMessage()) != NULL)
      {
        SetDlgItemText(hDlg, IDC_CURRENT_SETTINGS, szMessage);
        FreeMemory(&szMessage);
      }

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          dwWizardState = DLG_ADVANCED_SETTINGS;
          DestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDC_BUTTON_SITE_SELECTOR:
          dwWizardState = DLG_PROGRAM_FOLDER;
          DestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDCANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

LRESULT CALLBACK DlgProcReboot(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  HANDLE            hToken;
  TOKEN_PRIVILEGES  tkp;
  HWND              hRadioYes;
  RECT              rDlg;

  hRadioYes = GetDlgItem(hDlg, IDC_RADIO_YES);

  switch(msg)
  {
    case WM_INITDIALOG:
      DisableSystemMenuItems(hDlg, FALSE);
      CheckDlgButton(hDlg, IDC_RADIO_YES, BST_CHECKED);
      SetFocus(hRadioYes);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

      SetDlgItemText(hDlg, 202, sgInstallGui.szSetupMessage);
      SetDlgItemText(hDlg, IDC_RADIO_YES, sgInstallGui.szYesRestart);
      SetDlgItemText(hDlg, IDC_RADIO_NO, sgInstallGui.szNoRestart);
      SetDlgItemText(hDlg, IDOK, sgInstallGui.szOk);
      SendDlgItemMessage (hDlg, 202, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_RADIO_YES, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDC_RADIO_NO, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hDlg, IDOK, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDOK:
          if(IsDlgButtonChecked(hDlg, IDC_RADIO_YES) == BST_CHECKED)
          {
            // Get a token for this process.
            OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);

            // Get the LUID for the shutdown privilege.
            LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
            tkp.PrivilegeCount = 1;  // one privilege to set
            tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            // Get the shutdown privilege for this process.
            AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

            DestroyWindow(hDlg);
            PostQuitMessage(0);
            DestroyWindow(hWndMain);

            // Reboot the system and force all applications to close.
            ExitWindowsEx(EWX_REBOOT, 0);
          }
          else
          {
            DestroyWindow(hDlg);
            PostQuitMessage(0);
          }
          break;

        case IDCANCEL:
          DestroyWindow(hDlg);
          PostQuitMessage(0);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
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
      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.right/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.bottom/2),
                     0,
                     0,
                     SWP_NOSIZE);

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

          SetWindowPos(hDlg, HWND_TOP,
                      (gSystemInfo.dwScreenX/2)-((sizeString.cx + 55)/2),
                      (gSystemInfo.dwScreenY/2)-((sizeString.cy + 50)/2),
                      sizeString.cx + 55,
                      sizeString.cy + 50,
                      SWP_SHOWWINDOW);

          if(GetClientRect(hDlg, &rDlg))
            SetWindowPos(hSTMessage,
                         HWND_TOP,
                         rDlg.left,
                         rDlg.top,
                         rDlg.right,
                         rDlg.bottom,
                         SWP_SHOWWINDOW);

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
  char szBuf[MAX_BUF];
 
  if(sgProduct.dwMode != SILENT)
  {
    if((bShow) && (hDlgMessage == NULL))
    {
      ZeroMemory(szBuf, sizeof(szBuf));
      GetPrivateProfileString("Messages", "MB_MESSAGE_STR", "", szBuf, sizeof(szBuf), szFileIniInstall);
      hDlgMessage = InstantiateDialog(hWndMain, DLG_MESSAGE, szBuf, DlgProcMessage);
      SendMessage(hDlgMessage, WM_COMMAND, IDC_MESSAGE, (LPARAM)szMessage);
      SendDlgItemMessage (hDlgMessage, IDC_MESSAGE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
    }
    else if(!bShow && hDlgMessage)
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

BOOL CheckWizardStateCustom(DWORD dwDefault)
{
  if(sgProduct.dwCustomType != dwSetupType)
  {
    dwWizardState = dwDefault;
    return(FALSE);
  }

  return(TRUE);
}

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

    wsprintf(szKey,
             SETUP_STATE_REG_KEY,
             sgProduct.szCompanyName,
             sgProduct.szProductName,
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

void DlgSequenceNext()
{
  HRESULT hrValue;
  HRESULT hrErr;
  char    szDestPath[MAX_BUF];
  char    szInstallLogFile[MAX_BUF];

  BOOL    bDone = FALSE;

  do
  {
    switch(dwWizardState)
    {
      case DLG_NONE:
        dwWizardState = DLG_WELCOME;
        gbProcessingXpnstallFiles = FALSE;
        if(diWelcome.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diWelcome.szTitle, DlgProcWelcome);
          bDone = TRUE;
        }
        break;

      case DLG_WELCOME:
        dwWizardState = DLG_LICENSE;
        gbProcessingXpnstallFiles = FALSE;
        if(diLicense.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diLicense.szTitle, DlgProcLicense);
          bDone = TRUE;
        }
        break;

      case DLG_LICENSE:
        dwWizardState = DLG_SETUP_TYPE;
        gbProcessingXpnstallFiles = FALSE;
        if(diSetupType.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diSetupType.szTitle, DlgProcSetupType);
          bDone = TRUE;
        }
        else
        {
          CheckWizardStateCustom(DLG_ADVANCED_SETTINGS);
        }
        break;

      case DLG_SETUP_TYPE:
        dwWizardState = DLG_SELECT_COMPONENTS;
        gbProcessingXpnstallFiles = FALSE;
        if(diSelectComponents.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diSelectComponents.szTitle, DlgProcSelectComponents);
          bDone = TRUE;
        }
        break;

      case DLG_SELECT_COMPONENTS:
        dwWizardState = DLG_SELECT_ADDITIONAL_COMPONENTS;
        gbProcessingXpnstallFiles = FALSE;
        if((diSelectAdditionalComponents.bShowDialog) && (GetAdditionalComponentsCount() > 0))
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diSelectAdditionalComponents.szTitle, DlgProcSelectAdditionalComponents);
          bDone = TRUE;
        }
        break;

      case DLG_SELECT_ADDITIONAL_COMPONENTS:
        dwWizardState = DLG_WINDOWS_INTEGRATION;
        gbProcessingXpnstallFiles = FALSE;
        if(diWindowsIntegration.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diWindowsIntegration.szTitle, DlgProcWindowsIntegration);
          bDone = TRUE;
        }
        break;

      case DLG_WINDOWS_INTEGRATION:
        dwWizardState = DLG_PROGRAM_FOLDER;
        gbProcessingXpnstallFiles = FALSE;
        if(diProgramFolder.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diProgramFolder.szTitle, DlgProcProgramFolder);
          bDone = TRUE;
        }
        else
        {
          dwWizardState = DLG_ADVANCED_SETTINGS;
        }
        break;

      case DLG_PROGRAM_FOLDER:
        dwWizardState = DLG_ADVANCED_SETTINGS;
        gbProcessingXpnstallFiles = FALSE;
        if(diAdvancedSettings.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diAdvancedSettings.szTitle, DlgProcAdvancedSettings);
          bDone = TRUE;
        }
        else
        {
          dwWizardState = DLG_QUICK_LAUNCH;
        }
        break;

      case DLG_ADVANCED_SETTINGS:
        dwWizardState = DLG_QUICK_LAUNCH;
        gbProcessingXpnstallFiles = FALSE;
        if(diQuickLaunch.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diQuickLaunch.szTitle, DlgProcQuickLaunch);
          bDone = TRUE;
        }
        break;

 
     case DLG_QUICK_LAUNCH:
        dwWizardState = DLG_DOWNLOAD_OPTIONS;
        gbProcessingXpnstallFiles = FALSE;

        do
        {
          hrValue = VerifyDiskSpace();
          if(hrValue == IDOK)
          {
            /* show previous visible window */
            dwWizardState = DLG_SELECT_COMPONENTS;
            DlgSequencePrev();
            bDone = TRUE;
            break;
          }
          else if(hrValue == IDCANCEL)
          {
            AskCancelDlg(hWndMain);
            hrValue = IDRETRY;
          }
        }while(hrValue == IDRETRY);

        if(hrValue == IDOK)
        {
          /* break out of this case because we need to show the previous dialog */
          bDone = TRUE;
          break;
        }

        if((diDownloadOptions.bShowDialog == TRUE) && (GetTotalArchivesToDownload() > 0))
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diDownloadOptions.szTitle, DlgProcDownloadOptions);
          bDone = TRUE;
        }
        else
        {
          dwWizardState = DLG_DOWNLOAD_OPTIONS;
        }
        break;

      case DLG_DOWNLOAD_OPTIONS:
        dwWizardState = DLG_START_INSTALL;
        gbProcessingXpnstallFiles = FALSE;
        if(diStartInstall.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diStartInstall.szTitle, DlgProcStartInstall);
          bDone = TRUE;
        }
        break;

      default:
        dwWizardState = DLG_START_INSTALL;
        gbProcessingXpnstallFiles = TRUE;

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
        ProcessFileOpsForAll(T_PRE_DOWNLOAD);

        if(RetrieveArchives() == WIZ_OK)
        {
          /* Check to see if Turbo is required.  If so, set the
           * appropriate Windows registry keys */
          SetTurboArgs();

          /* POST_DOWNLOAD process file manipulation functions */
          ProcessFileOpsForAll(T_POST_DOWNLOAD);
          /* PRE_XPCOM process file manipulation functions */
          ProcessFileOpsForAll(T_PRE_XPCOM);

          if(ProcessXpcomFile() != FO_SUCCESS)
          {
            bSDUserCanceled = TRUE;
            CleanupXpcomFile();
            PostQuitMessage(0);

            /* break out of switch statment */
            bDone = TRUE;
            break;
          }

          /* POST_XPCOM process file manipulation functions */
          ProcessFileOpsForAll(T_POST_XPCOM);
          /* PRE_SMARTUPDATE process file manipulation functions */
          ProcessFileOpsForAll(T_PRE_SMARTUPDATE);

          /* save the installer files in the local machine */
          if(diDownloadOptions.bSaveInstaller)
            SaveInstallerFiles();

          if(CheckInstances())
          {
            bSDUserCanceled = TRUE;
            CleanupXpcomFile();
            PostQuitMessage(0);

            /* break out of switch statment */
            bDone = TRUE;
            break;
          }

          lstrcat(szDestPath, "uninstall\\");
          CreateDirectoriesAll(szDestPath, TRUE);

          /* save the installer files in the local machine */
          if(diDownloadOptions.bSaveInstaller)
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
            ClearWinRegUninstallFileDeletion();
            if(!gbIgnoreProgramFolderX)
              ProcessProgramFolderShowCmd();

            CleanupArgsRegistry();
            CleanupPreviousVersionRegKeys();
            if(NeedReboot())
            {
              CleanupXpcomFile();
              hDlgCurrent = InstantiateDialog(hWndMain, DLG_RESTART, diReboot.szTitle, DlgProcReboot);
            }
            else
            {
              CleanupXpcomFile();
              PostQuitMessage(0);
            }
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
        bDone = TRUE;
        break;
    }
  } while(!bDone);
}

void DlgSequencePrev()
{
  BOOL bDone = FALSE;

  do
  {
    switch(dwWizardState)
    {
      case DLG_START_INSTALL:
        dwWizardState = DLG_ADVANCED_SETTINGS;
        gbProcessingXpnstallFiles = FALSE;
        if(diAdvancedSettings.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diAdvancedSettings.szTitle, DlgProcAdvancedSettings);
          bDone = TRUE;
        }
        break;

      case DLG_ADVANCED_SETTINGS:
        dwWizardState = DLG_DOWNLOAD_OPTIONS;
        gbProcessingXpnstallFiles = FALSE;
        if((diDownloadOptions.bShowDialog == TRUE) && (GetTotalArchivesToDownload() > 0))
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diDownloadOptions.szTitle, DlgProcDownloadOptions);
          bDone = TRUE;
        }
        break;

      case DLG_DOWNLOAD_OPTIONS:
        dwWizardState = DLG_QUICK_LAUNCH;
        gbProcessingXpnstallFiles = FALSE;
        if(diQuickLaunch.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diQuickLaunch.szTitle, DlgProcQuickLaunch);
          bDone = TRUE;
        }
        break;

      case DLG_QUICK_LAUNCH:
        dwWizardState = DLG_PROGRAM_FOLDER;
        gbProcessingXpnstallFiles = FALSE;
        if(CheckWizardStateCustom(DLG_SELECT_COMPONENTS))
        {
          if(diProgramFolder.bShowDialog)
          {
            hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diProgramFolder.szTitle, DlgProcProgramFolder);
            bDone = TRUE;
          }
        }
        break;

      case DLG_PROGRAM_FOLDER:
        dwWizardState = DLG_WINDOWS_INTEGRATION;
        gbProcessingXpnstallFiles = FALSE;
        if(diWindowsIntegration.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diWindowsIntegration.szTitle, DlgProcWindowsIntegration);
          bDone = TRUE;
        }
        break;

      case DLG_WINDOWS_INTEGRATION:
        dwWizardState = DLG_SELECT_ADDITIONAL_COMPONENTS;
        gbProcessingXpnstallFiles = FALSE;
        if((diSelectAdditionalComponents.bShowDialog) && (GetAdditionalComponentsCount() > 0))
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diSelectAdditionalComponents.szTitle, DlgProcSelectAdditionalComponents);
          bDone = TRUE;
        }
        break;

      case DLG_SELECT_ADDITIONAL_COMPONENTS:
        dwWizardState = DLG_SELECT_COMPONENTS;
        gbProcessingXpnstallFiles = FALSE;
        if(diSelectComponents.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diSelectComponents.szTitle, DlgProcSelectComponents);
          bDone = TRUE;
        }
        break;

      case DLG_SELECT_COMPONENTS:
        dwWizardState = DLG_SETUP_TYPE;
        gbProcessingXpnstallFiles = FALSE;
        if(diSetupType.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diSetupType.szTitle, DlgProcSetupType);
          bDone = TRUE;
        }
        break;

      case DLG_SETUP_TYPE:
        dwWizardState = DLG_LICENSE;
        gbProcessingXpnstallFiles = FALSE;
        if(diLicense.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diLicense.szTitle, DlgProcLicense);
          bDone = TRUE;
        }
        break;

      case DLG_LICENSE:
        dwWizardState = DLG_WELCOME;
        gbProcessingXpnstallFiles = FALSE;
        if(diWelcome.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diWelcome.szTitle, DlgProcWelcome);
          bDone = TRUE;
        }
        break;

      default:
        dwWizardState = DLG_WELCOME;
        gbProcessingXpnstallFiles = FALSE;
        if(diWelcome.bShowDialog)
        {
          hDlgCurrent = InstantiateDialog(hWndMain, DLG_WELCOME, diWelcome.szTitle, DlgProcWelcome);
          bDone = TRUE;
        }
        break;
    }
  } while(!bDone);
}
