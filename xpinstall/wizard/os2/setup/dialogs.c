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
 * Rights and limitations under the License.
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

#define INCL_DOSERRORS
#define INCL_DOS
#define INCL_DOSFILEMGR
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_PM
#define INCL_WIN
#define INCL_WINDIALOGS
#define INCL_WINMENUS
#define INCL_WINWINDOWMGR
#define INCL_winstdfile

#define MAX_BUF 4096
#define DOSDD12 "\\DEV\\DOS$    \0"

#include <os2.h>
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
static ULONG   gdwACFlag;
static ULONG   gdwIndexLastSelected;

BOOL AskCancelDlg(HWND hDlg)
{
  char szDlgQuitTitle[MAX_BUF];
  char szDlgQuitMsg[MAX_BUF];
  char szMsg[MAX_BUF];
  BOOL bRv = FALSE;
  HINI   hiniInstall;
  HINI   hiniConfig;

  if((sgProduct.dwMode != SILENT) && (sgProduct.dwMode != AUTO))
  {
    hiniInstall  = PrfOpenProfile((HAB)0, szFileIniInstall);
    if(!PrfQueryProfileString(hiniInstall, "Messages", "DLGQUITTITLE", "", szDlgQuitTitle, sizeof(szDlgQuitTitle)))
      PostQuitMessage(1);
    else if(!PrfQueryProfileString(hiniInstall, "Messages", "DLGQUITMSG", "", szDlgQuitMsg, sizeof(szDlgQuitMsg)))
      PostQuitMessage(1);
    else if(WinMessageBox(HWND_DESKTOP, hDlg, szDlgQuitMsg, szDlgQuitTitle, NULL, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_APPLMODAL) == MBID_YES)
    {
      WinDestroyWindow(hDlg);
      PostQuitMessage(0);
      bRv = TRUE;
    }
    PrfCloseProfile(hiniInstall);
  }
  else
  { 
    hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
    PrfQueryProfileString(hiniConfig, "Strings", "Message Cancel Setup AUTO mode", "", szMsg, sizeof(szMsg));
    ShowMessage(szMsg, TRUE);
    Delay(5);
    ShowMessage(szMsg, FALSE);
    bRv = TRUE;
    PrfCloseProfile(hiniConfig);
  }

  return(bRv);
} 

void DisableSystemMenuItems(HWND hWnd, BOOL bDisableClose)
{
  HWND		hWndSysMenu;
  
  hWndSysMenu = WinWindowFromID(hWnd, FID_SYSMENU);

  WinEnableMenuItem(hWndSysMenu, SC_RESTORE,  SWL_GRAYED);
  WinEnableMenuItem(hWndSysMenu, SC_SIZE,     SWL_GRAYED);
  WinEnableMenuItem(hWndSysMenu, SC_MAXIMIZE, SWL_GRAYED);

  if(bDisableClose)
    WinEnableMenuItem(hWndSysMenu, SC_CLOSE, SWL_GRAYED);
}

MRESULT EXPENTRY DlgProcWelcome(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
{
  char szBuf[MAX_BUF];
  RECTL rDlg;
  PSZ pszFontNameSize;
  ULONG ulFontNameSizeLength;
  
  switch(msg)
  {
    case WM_INITDLG:
      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diWelcome.szTitle);

      sprintf(szBuf, diWelcome.szMessage0, sgProduct.szProductName);
      WinSetDlgItemText(hDlg, IDC_STATIC0, szBuf);
      WinSetDlgItemText(hDlg, IDC_STATIC1, diWelcome.szMessage1);
      WinSetDlgItemText(hDlg, IDC_STATIC2, diWelcome.szMessage2);

      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, MBID_CANCEL, sgInstallGui.szCancel_);

      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC2), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case MBID_CANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}


MRESULT EXPENTRY DlgProcLicense(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
{
  char            szBuf[MAX_BUF];
  PSZ           szLicenseFilenameBuf = NULL;
  FILEFINDBUF3 wfdFindFileData;
  ULONG           dwFileSize;
  ULONG           dwBytesRead;
  ULONG         ulFindCount = 1;
  LHANDLE          hFLicense;
  FILE            *fLicense;
  RECTL            rDlg;
  PSZ             pszFontNameSize;
  ULONG          ulFontNameSizeLength;

  switch(msg)
  {
    case WM_INITDLG:
      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diLicense.szTitle);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0, diLicense.szMessage0);
      WinSetDlgItemText(hDlg, IDC_MESSAGE1, diLicense.szMessage1);

      strcpy(szBuf, szSetupDir);
      AppendBackSlash(szBuf, sizeof(szBuf));
      strcat(szBuf, diLicense.szLicenseFilename);

      if((DosFindFirst(szBuf, &hFLicense, FILE_NORMAL, &wfdFindFileData, sizeof(wfdFindFileData), &ulFindCount, FIL_STANDARD)) != ERROR_INVALID_HANDLE)
      {
        dwFileSize = wfdFindFileData.cbFile;
        DosFindClose(hFLicense);
        if((szLicenseFilenameBuf = NS_GlobalAlloc(dwFileSize)) != NULL)
        {
          if((fLicense = fopen(szBuf, "rb")) != NULL)
          {
            dwBytesRead = fread(szLicenseFilenameBuf, sizeof(char), dwFileSize, fLicense);
            fclose(fLicense);
            WinSetDlgItemText(hDlg, IDC_EDIT_LICENSE, szLicenseFilenameBuf);
          }

          DosFreeMem(&szLicenseFilenameBuf);
        }
      }

      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szAccept_);
      WinSetDlgItemText(hDlg, MBID_CANCEL, sgInstallGui.szNo_);

      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_EDIT_LICENSE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZBACK), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);

       break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          WinDestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case MBID_CANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

MRESULT EXPENTRY ListBoxBrowseWndProc(HWND hWnd, ULONG uMsg, MPARAM wParam, MPARAM lParam)
{
  switch(uMsg)
  {
    case LM_SELECTITEM:
      gdwIndexLastSelected = (ULONG)wParam;
      break;
  }

  return(CallWindowProc(OldListBoxWndProc, hWnd, uMsg, wParam, lParam));
}

MRESULT EXPENTRY BrowseHookProc(HWND hDlg, ULONG message, MPARAM wParam, MPARAM lParam)
{
  ULONG dwIndex;
  ULONG dwLoop;
  RECTL  rDlg;
  char  szBuf[MAX_BUF];
  char  szBufIndex[MAX_BUF];
  char  szPath[MAX_BUF];
  HWND  hwndLBFolders;
  HINI hiniInstall;

  PSZ pszFontNameSize;
  ULONG ulFontNameSizeLength;

  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  switch(message)
  {
    case WM_INITDLG:
      hwndLBFolders  = WinWindowFromID(hDlg, 1121);
      WinSetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szTempSetupPath);

      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);

      OldListBoxWndProc    = WinSubclassWindow(hwndLBFolders, (PFNWP)ListBoxBrowseWndProc);
      gdwIndexLastSelected = WinSendDlgItemMsg(hDlg, 1121, LM_QUERYSELECTION, 0, (MPARAM)0);

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetWindowText(hDlg, sgInstallGui.szSelectDirectory);
      WinSetDlgItemText(hDlg, 1092, sgInstallGui.szDirectories_);
      WinSetDlgItemText(hDlg, 1091, sgInstallGui.szDrives_);
      WinSetDlgItemText(hDlg, 1, sgInstallGui.szOk);
      WinSetDlgItemText(hDlg, MBID_CANCEL, sgInstallGui.szCancel);

      WinSetPresParam(WinWindowFromID(hDlg, DLG_BROWSE_DIR), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, 1092), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, 1091), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, 1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_EDIT_DESTINATION), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, 1121), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, 1137), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case 1121:
          if(HIWORD(wParam) == LN_ENTER)
          {
            dwIndex = WinSendDlgItemMsg(hDlg, 1121, LM_QUERYSELECTION, 0, (MPARAM)0);
            WinSendDlgItemMsg(hDlg, 1121, LM_QUERYITEMTEXT, 0, (MPARAM)szPath);

            if(gdwIndexLastSelected < dwIndex)
            {
              for(dwLoop = 1; dwLoop <= gdwIndexLastSelected; dwLoop++)
              {
                WinSendDlgItemMsg(hDlg, 1121, LM_QUERYITEMTEXT, dwLoop, (MPARAM)szBufIndex);
                AppendBackSlash(szPath, sizeof(szPath));
                strcat(szPath, szBufIndex);
              }

              WinSendDlgItemMsg(hDlg, 1121, LM_QUERYITEMTEXT, dwIndex, (MPARAM)szBufIndex);
              AppendBackSlash(szPath, sizeof(szPath));
              strcat(szPath, szBufIndex);
            }
            else
            {
              for(dwLoop = 1; dwLoop <= dwIndex; dwLoop++)
              {
                WinSendDlgItemMsg(hDlg, 1121, LM_QUERYITEMTEXT, dwLoop, (MPARAM)szBufIndex);
                AppendBackSlash(szPath, sizeof(szPath));
                strcat(szPath, szBufIndex);
              }
            }
            WinSetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szPath);
          }
          break;

        case MBID_OK:
          WinWindowFromIDText(hDlg, IDC_EDIT_DESTINATION, szBuf, MAX_BUF);
          if(*szBuf == '\0')
          {
            char szEDestinationPath[MAX_BUF];

            PrfQueryProfileString(hiniInstall, "Messages", "ERROR_DESTINATION_PATH", "", szEDestinationPath, sizeof(szEDestinationPath));
            WinMessageBox(HWND_DESKTOP, hDlg, szEDestinationPath, NULL, NULL, MB_OK | MB_ICONEXCLAMATION);
            break;
          }

          AppendBackSlash(szBuf, sizeof(szBuf));
          if(FileExists(szBuf) == FALSE)
          {
            char szMsgCreateDirectory[MAX_BUF];
            char szStrCreateDirectory[MAX_BUF];
            char szBufTemp[MAX_BUF];
            char szBufTemp2[MAX_BUF];

            PrfQueryProfileString(hiniInstall, "Messages", "STR_CREATE_DIRECTORY", "", szStrCreateDirectory, sizeof(szStrCreateDirectory));
            if(PrfQueryProfileString(hiniInstall, "Messages", "MSG_CREATE_DIRECTORY", "", szMsgCreateDirectory, sizeof(szMsgCreateDirectory)))
            {
              strcpy(szBufTemp, "\n\n");
              strcat(szBufTemp, szBuf);
              RemoveBackSlash(szBufTemp);
              strcat(szBufTemp, "\n\n");
              sprintf(szBufTemp2, szMsgCreateDirectory, szBufTemp);
            }

            if(WinMessageBox(HWND_DESKTOP, hDlg, szBufTemp2, szStrCreateDirectory, NULL, MB_YESNO | MB_ICONQUESTION) == MBID_YES)
            {
              char szBuf2[CCHMAXPATHCOMP];

              if(CreateDirectoriesAll(szBuf, TRUE) == FALSE)
              {
                char szECreateDirectory[MAX_BUF];

                strcpy(szBufTemp, "\n\n");
                strcat(szBufTemp, sgProduct.szPath);
                RemoveBackSlash(szBufTemp);
                strcat(szBufTemp, "\n\n");

                if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_CREATE_DIRECTORY", "", szECreateDirectory, sizeof(szECreateDirectory)))
                  sprintf(szBuf, szECreateDirectory, szBufTemp);

                WinMessageBox(HWND_DESKTOP, hDlg, szBuf, "", NULL, MB_OK | MB_ERROR);
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
  PrfCloseProfile(hiniInstall);
  return(0);
}

BOOL BrowseForDirectory(HWND hDlg, char *szCurrDir)
{ 
  OPENFILENAME      of;
  char           ftitle[CCHMAXPATHCOMP];
  char           fname[CCHMAXPATHCOMP];
  char           szCDir[MAX_BUF];
  char           szBuf[MAX_BUF];
  char           szSearchPathBuf[MAX_BUF];
  char           szDlgBrowseTitle[MAX_BUF];
  BOOL           bRet;
  HINI            hiniInstall;

  /* save the current directory */
  GetCurrentDirectory(MAX_BUF, szCDir);

  memset(szDlgBrowseTitle, 0, sizeof(szDlgBrowseTitle));
  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  PrfQueryProfileString(hiniInstall, "Messages", "DLGBROWSETITLE", "", szDlgBrowseTitle, sizeof(szDlgBrowseTitle));
  PrfCloseProfile(hiniInstall);
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
  of.nMaxFile           = CCHMAXPATH;
  of.lpstrFileTitle     = ftitle;
  of.nMaxFileTitle      = CCHMAXPATH;
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
  DosSetCurrentDir(szCDir);
  return(bRet);
}

BOOL GetTextExtentPoint32(HPS aPS, const char* aString, int aLength, PSIZEL aSizeL)
{
  POINTL ptls[5];

  aSizeL->cx = 0;

  while(aLength)
  {
    ULONG thislen = min(aLength, 512);
    GpiQueryTextBox (aPS, thislen, (PCH)aString, 5, ptls);
    aSizeL->cx += ptls[TXTBOX_CONCAT].x;
    aLength -= thislen;
    aString += thislen;
  }
  aSizeL->cy = ptls[TXTBOX_TOPLEFT].y - ptls[TXTBOX_BOTTOMLEFT].y;
  return TRUE;
}

void TruncateString(HWND hWnd, PSZ szInURL, PSZ szOutString, ULONG dwOutStringBufSize)
{
  HPS           hpsWnd;
//  LOGFONT       logFont;
//  HFONT         hfontNew;
//  HFONT         hfontOld;
  RECTL          rWndRect;
  SIZEL          sizeString;
  char          *ptr = NULL;
  int           iHalfLen;
  int           iOutStringLen;
  PSZ         pszFontNameSize;

  if((ULONG)strlen(szInURL) > dwOutStringBufSize)
    return;

  memset(szOutString, 0, dwOutStringBufSize);
  strcpy(szOutString, szInURL);
  iOutStringLen = strlen(szOutString);
  hpsWnd        = WinGetPS(hWnd);
  WinQueryWindow(hWnd, &rWndRect);
//  SystemParametersInfo(SPI_GETICONTITLELOGFONT,
//                       sizeof(logFont),
//                       (PVOID)&logFont,
//                      0);

  pszFontNameSize = myGetSysFont();

//  hfontNew = CreateFontIndirect(&logFont);
//  if(hfontNew)
//  {
//   hfontOld = (HFONT)SelectObject(hdcWnd, hfontNew);

    GetTextExtentPoint32(hpsWnd, szOutString, iOutStringLen, &sizeString);
    while(sizeString.cx > rWndRect.xRight)
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
      GetTextExtentPoint32(hpsWnd, szOutString, iOutStringLen, &sizeString);
    }
//  }

//  SelectObject(hdcWnd, hfontOld);
//  DeleteObject(hfontNew);
  WinReleasePS(hpsWnd);
}

MRESULT EXPENTRY DlgProcSetupType(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
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
  RECTL          rDlg;
  char          szBuf[MAX_BUF];
  char          szBufTemp[MAX_BUF];
  char          szBufTemp2[MAX_BUF];
  HINI            hiniInstall;
  PSZ           pszFontNameSize;
  ULONG        ulFontNameSizeLength;

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
      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diSetupType.szTitle);

      hDestinationPath = WinWindowFromID(hDlg, IDC_EDIT_DESTINATION); /* handle to the static destination path text window */
      TruncateString(hDestinationPath, szTempSetupPath, szBuf, sizeof(szBuf));

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
      switch(dwTempSetupType)
      {
        case ST_RADIO0:
          //CheckDlgButton(hDlg, IDC_RADIO_ST0, BST_CHECKED);
		  WinSendDlgItemMsg (hDlg, IDC_RADIO_ST0, BM_SETCHECK, BTN_CHECKED, 0L);
          WinSetFocus(HWND_DESKTOP, hRadioSt0);
          break;

        case ST_RADIO1:
   		  WinSendDlgItemMsg (hDlg, IDC_RADIO_ST1, BM_SETCHECK, BTN_CHECKED, 0L);
          WinSetFocus(HWND_DESKTOP, hRadioSt1);
          break;

        case ST_RADIO2:
          WinSendDlgItemMsg (hDlg, IDC_RADIO_ST2, BM_SETCHECK, BTN_CHECKED, 0L);
          WinSetFocus(HWND_DESKTOP, hRadioSt2);
          break;

        case ST_RADIO3:
          WinSendDlgItemMsg (hDlg, IDC_RADIO_ST3, BM_SETCHECK, BTN_CHECKED, 0L);
          WinSetFocus(HWND_DESKTOP, hRadioSt3);
          break;
      }

      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);

      if((*diSetupType.szReadmeFilename == '\0') || (FileExists(diSetupType.szReadmeFilename) == FALSE))
        WinShowWindow(hReadme, FALSE);
      else
        WinShowWindow(hReadme, TRUE);

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetDlgItemText(hDlg, IDC_DESTINATION, sgInstallGui.szDestinationDirectory);
      WinSetDlgItemText(hDlg, IDC_BUTTON_BROWSE, sgInstallGui.szBrowse_);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, MBID_CANCEL, sgInstallGui.szCancel_);
      WinSetDlgItemText(hDlg, IDC_README, sgInstallGui.szReadme_);

      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC_MSG0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 
      WinSetPresParam(WinWindowFromID(hDlg, IDC_RADIO_ST0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 
      WinSetPresParam(WinWindowFromID(hDlg, IDC_RADIO_ST1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 
      WinSetPresParam(WinWindowFromID(hDlg, IDC_RADIO_ST2), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 
      WinSetPresParam(WinWindowFromID(hDlg, IDC_RADIO_ST3), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC_ST0_DESCRIPTION), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC_ST1_DESCRIPTION), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC_ST2_DESCRIPTION), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC_ST3_DESCRIPTION), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_EDIT_DESTINATION), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 
      WinSetPresParam(WinWindowFromID(hDlg, IDC_BUTTON_BROWSE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZBACK), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_README), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize); 

      if(sgProduct.bLockPath)
        EnableWindow(WinWindowFromID(hDlg, IDC_BUTTON_BROWSE), FALSE);
      else
        EnableWindow(WinWindowFromID(hDlg, IDC_BUTTON_BROWSE), TRUE);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_BUTTON_BROWSE:
          if(WinSendDlgItemMsg(hDlg, IDC_RADIO_ST0, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
            dwTempSetupType = ST_RADIO0;
          else if(WinSendDlgItemMsg(hDlg, IDC_RADIO_ST1, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
            dwTempSetupType = ST_RADIO1;
          else if(WinSendDlgItemMsg(hDlg, IDC_RADIO_ST2, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
            dwTempSetupType = ST_RADIO2;
          else if(WinSendDlgItemMsg(hDlg, IDC_RADIO_ST3, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
            dwTempSetupType = ST_RADIO3;

          BrowseForDirectory(hDlg, szTempSetupPath);

          hDestinationPath = WinWindowFromID(hDlg, IDC_EDIT_DESTINATION); /* handle to the static destination path text window */
          TruncateString(hDestinationPath, szTempSetupPath, szBuf, sizeof(szBuf));
          WinSetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szBuf);
          break;

        case IDC_README:
          if(*diSetupType.szReadmeApp == '\0')
            //WinSpawn(diSetupType.szReadmeFilename, NULL, szSetupDir, TRUENORMAL, FALSE);
            WinSpawn(diSetupType.szReadmeFilename, NULL, szSetupDir, TRUE, FALSE);
          else
            //WinSpawn(diSetupType.szReadmeApp, diSetupType.szReadmeFilename, szSetupDir, TRUENORMAL, FALSE);
            WinSpawn(diSetupType.szReadmeApp, diSetupType.szReadmeFilename, szSetupDir, TRUE, FALSE);
          break;

        case IDWIZNEXT:
          strcpy(sgProduct.szPath, szTempSetupPath);

          /* append a backslash to the path because CreateDirectoriesAll()
             uses a backslash to determine directories */
          strcpy(szBuf, sgProduct.szPath);
          AppendBackSlash(szBuf, sizeof(szBuf));

          if(FileExists(szBuf) == FALSE)
          {
            char szMsgCreateDirectory[MAX_BUF];
            char szStrCreateDirectory[MAX_BUF];

            hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
            PrfQueryProfileString(hiniInstall, "Messages", "STR_CREATE_DIRECTORY", "", szStrCreateDirectory, sizeof(szStrCreateDirectory));
            if(PrfQueryProfileString(hiniInstall, "Messages", "MSG_CREATE_DIRECTORY", "", szMsgCreateDirectory, sizeof(szMsgCreateDirectory)))
            {
              strcpy(szBufTemp, "\n\n");
              strcat(szBufTemp, szBuf);
              RemoveBackSlash(szBufTemp);
              strcat(szBufTemp, "\n\n");
              sprintf(szBufTemp2, szMsgCreateDirectory, szBufTemp);
            }

            if(WinMessageBox(HWND_DESKTOP, hDlg, szBufTemp2, szStrCreateDirectory, NULL, MB_YESNO | MB_ICONQUESTION) == MBID_YES)
            {
              char szBuf2[CCHMAXPATHCOMP];

              if(CreateDirectoriesAll(szBuf, TRUE) == FALSE)
              {
                char szECreateDirectory[MAX_BUF];

                strcpy(szBufTemp, "\n\n");
                strcat(szBufTemp, sgProduct.szPath);
                RemoveBackSlash(szBufTemp);
                strcat(szBufTemp, "\n\n");

                if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_CREATE_DIRECTORY", "", szECreateDirectory, sizeof(szECreateDirectory)))
                  sprintf(szBuf, szECreateDirectory, szBufTemp);

                WinMessageBox(HWND_DESKTOP, hDlg, szBuf, "", NULL, MB_OK | MB_ERROR);
                PrfCloseProfile(hiniInstall);
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
              PrfCloseProfile(hiniInstall);
              break;
            }
            PrfCloseProfile(hiniInstall);
          }

          /* retrieve and save the state of the selected radio button */
          if(WinSendDlgItemMsg(hDlg, IDC_RADIO_ST0, BM_QUERYCHECK, 0, 0L)      == BTN_CHECKED)
          {
            SiCNodeSetItemsSelected(ST_RADIO0);

            dwSetupType     = ST_RADIO0;
            dwTempSetupType = dwSetupType;
          }
          else if(WinSendDlgItemMsg(hDlg, IDC_RADIO_ST1, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
          {
            SiCNodeSetItemsSelected(ST_RADIO1);

            dwSetupType     = ST_RADIO1;
            dwTempSetupType = dwSetupType;
          }
          else if(WinSendDlgItemMsg(hDlg, IDC_RADIO_ST2, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
          {
            SiCNodeSetItemsSelected(ST_RADIO2);

            dwSetupType     = ST_RADIO2;
            dwTempSetupType = dwSetupType;
          }
          else if(WinSendDlgItemMsg(hDlg, IDC_RADIO_ST3, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
          {
            SiCNodeSetItemsSelected(ST_RADIO3);

            dwSetupType     = ST_RADIO3;
            dwTempSetupType = dwSetupType;
          }

          /* set the next dialog to be shown depending on the 
             what the user selected */
          dwWizardState = DLG_SETUP_TYPE;
          CheckWizardStateCustom(DLG_ADVANCED_SETTINGS);

          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          dwTempSetupType = dwSetupType;
          strcpy(szTempSetupPath, sgProduct.szPath);
          WinDestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case MBID_CANCEL:
          strcpy(sgProduct.szPath, szTempSetupPath);
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

void DrawCheck(POWNERITEM pOI, ULONG dwACFlag)
{
  siC     *siCTemp  = NULL;
  HDC     hdcMem;
  HBITMAP hbmpCheckBox;

  siCTemp = SiCNodeGetObject(pOI->idItem, FALSE, dwACFlag);
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

    WinSendMsg(pOI->hItem, LM_SETITEMHANDLE, pOI->idItem, (MPARAM)hbmpCheckBox);
//    if((hdcMem = CreateCompatibleDC(pOI->hps)) != NULL)
//    {
      SelectObject(hdcMem, hbmpCheckBox);

      WinDrawBitmap(pOI->hps, hbmpCheckBox, NULL, (PPOINTL)&pOI->rclItem, 0, 0, ROP_SRCCOPY);
//    }
  }
}

void lbAddItem(HWND hList, siC *siCComponent)
{
  ULONG dwItem;

  dwItem = WinSendMsg(hList, LM_INSERTITEM, 0, (MPARAM)siCComponent->szDescriptionShort);
  if(siCComponent->dwAttributes & SIC_DISABLED)
    WinSendMsg(hList, LM_SETITEMHANDLE, dwItem, (MPARAM)hbmpBoxCheckedDisabled);
  else if(siCComponent->dwAttributes & SIC_SELECTED)
    WinSendMsg(hList, LM_SETITEMHANDLE, dwItem, (MPARAM)hbmpBoxChecked);
  else
    WinSendMsg(hList, LM_SETITEMHANDLE, dwItem, (MPARAM)hbmpBoxUnChecked);
}

void InvalidateLBCheckbox(HWND hwndListBox)
{
  RECTL rcCheckArea;

  // retrieve the rectangle of all list items to update.
  WinQueryWindowRect(hwndListBox, &rcCheckArea);

  // set the right coordinate of the rectangle to be the same
  // as the right edge of the bitmap drawn.
  rcCheckArea.xRight = CX_CHECKBOX;

  // It then invalidates the checkbox region to be redrawn.
  // Invalidating the region sends a WM_DRAWITEM message to
  // the dialog, which redraws the region given the
  // node attirbute, in this case it is a bitmap of a
  // checked/unchecked checkbox.
  WinInvalidateRect(hwndListBox, &rcCheckArea, TRUE);
}

void ToggleCheck(HWND hwndListBox, ULONG dwIndex, ULONG dwACFlag)
{
  BOOL  bMoreToResolve;
  PSZ szToggledReferenceName = NULL;
  ULONG dwAttributes;

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
// FUNCTION : SubclassWindow( HWND, PFNWP )
// PURPOSE  : Subclasses a window procedure
// COMMENTS : Returns the old window procedure
// ************************************************************************
//PFNWP SubclassWindow( HWND hWnd, PFNWP NewWndProc)
//{
//  PFNWP OldWndProc = NULL;

//  OldWndProc = (PFNWP)WinQueryWindowUlong(hWnd, QWL_PFNWP);
//  WinSetWindowUlong(hWnd, QWL_PFNWP, (LONG) NewWndProc);

//  return OldWndProc;
//}

// ************************************************************************
// FUNCTION : NewListBoxWndProc( HWND, ULONG, MPARAM, MPARAM )
// PURPOSE  : Processes messages for "LISTBOX" class.
// COMMENTS : Prevents the user from moving the window
//            by dragging the titlebar.
// ************************************************************************
MRESULT EXPENTRY NewListBoxWndProc(HWND hWnd, ULONG uMsg, MPARAM wParam, MPARAM lParam)
{
  ULONG               dwPosX;
  ULONG               dwPosY;
  ULONG               dwIndex;

  switch(uMsg)
  {
    case WM_CHAR:
      /* check for the space key */
      if((UCHAR)wParam == 32)
      {
        dwIndex = WinSendMsg(hWnd,
                              LM_QUERYSELECTION,
                              0,
                              0);
        ToggleCheck(hWnd, dwIndex, gdwACFlag);
      }
      break;

    case WM_BUTTON1DOWN:
      {
        dwPosX = LOWORD(lParam); // x pos
        dwPosY = HIWORD(lParam); // y pos

        if((dwPosX > 1) && (dwPosX <= CX_CHECKBOX))
        {
          dwIndex = LOWORD(WinSendMsg(hWnd,
                                       LM_SELECTITEM,
                                       0,
                                       (MPARAM)MAKEMPARAM(dwPosX, dwPosY)));
          ToggleCheck(hWnd, dwIndex, gdwACFlag);
        }
      }
      break;
  }

  return(CallWindowProc(OldListBoxWndProc, hWnd, uMsg, wParam, lParam));
}

MRESULT EXPENTRY DlgProcSelectComponents(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
{
  BOOL                bReturn = FALSE;
  siC                 *siCTemp;
  ULONG               dwIndex;
  ULONG               dwItems = MAX_BUF;
  HWND                hwndLBComponents;
  RECTL                rDlg;
  UCHAR               tchBuffer[MAX_BUF];
  FONTMETRICS          fm;
  ULONG               y;
  POWNERITEM    pOI;
  ULONGLONG           ullDSBuf;
  char                szBuf[MAX_BUF];
  PSZ                 pszFontNameSize;
  ULONG              ulFontNameSizeLength;

  hwndLBComponents  = WinWindowFromID(hDlg, IDC_LIST_COMPONENTS);

  switch(msg)
  {
    case WM_INITDLG:
      DisableSystemMenuItems(hDlg, FALSE);
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

      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);

      /* update the disk space available info in the dialog.  GetDiskSpaceAvailable()
         returns value in kbytes */
      ullDSBuf = GetDiskSpaceAvailable(sgProduct.szPath);
      _ui64toa(ullDSBuf, tchBuffer, 10);
      ParsePath(sgProduct.szPath, szBuf, sizeof(szBuf), FALSE, PP_ROOT_ONLY);
      RemoveBackSlash(szBuf);
      strcat(szBuf, " - ");
      strcat(szBuf, tchBuffer);
      strcat(szBuf, " KB");
      WinSetDlgItemText(hDlg, IDC_SPACE_AVAILABLE, szBuf);

      WinSetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szComponents_);
      WinSetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szDescription);
      WinSetDlgItemText(hDlg, IDC_STATIC3, sgInstallGui.szTotalDownloadSize);
      WinSetDlgItemText(hDlg, IDC_STATIC4, sgInstallGui.szSpaceAvailable);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, MBID_CANCEL, sgInstallGui.szCancel_);

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC2), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC3), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC4), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZBACK), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_LIST_COMPONENTS), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC_DESCRIPTION), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_DOWNLOAD_SIZE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_SPACE_AVAILABLE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);

      gdwACFlag = AC_COMPONENTS;
      OldListBoxWndProc = WinSubclassWindow(hwndLBComponents, (PFNWP)NewListBoxWndProc);
      break;

    case WM_DRAWITEM:
      pOI = (POWNERITEM) PVOIDFROMMP(lParam);

      // If there are no list box items, skip this message.
      if(pOI->idItem == -1)
        break;

      WinSendMsg(pOI->hItem, LM_QUERYITEMTEXT, pOI->idItem, (MPARAM)tchBuffer);
      if(pOI->fsState != pOI->fsStateOld)
      {
         if(pOI->fsState)
         {
            WinSetPresParam(pOI->hwnd, PP_ACTIVETEXTFGNDCOLORINDEX, sizeof(LONG), SYSCLR_HILITEFOREGROUND);
            WinSetPresParam(pOI->hwnd, PP_BACKGROUNDCOLORINDEX, sizeof(LONG), SYSCLR_HILITEBACKGROUND);
         }
         else
         {
            WinSetPresParam(pOI->hwnd, PP_INACTIVETEXTFGNDCOLORINDEX, sizeof(LONG), SYSCLR_SHADOWTEXT);
         }
         pOI->fsState = pOI->fsStateOld = 1;
      }
      else
      {
         siCTemp = SiCNodeGetObject(pOI->idItem, FALSE, AC_COMPONENTS);
         if(siCTemp->dwAttributes & SIC_DISABLED)
         //SetTextColor(pOI->hDC, WinQuerySysColor(HWND_DESKTOP, SYSCLR_SHADOWTEXT, 0));
            WinSetPresParam(pOI->hwnd, PP_INACTIVETEXTFGNDCOLORINDEX, sizeof(LONG), SYSCLR_SHADOWTEXT);
         else
          //SetTextColor(pOI->hDC, WinQuerySysColor(HWND_DESKTOP, SYSCLR_WINDOWTEXT, 0));
            WinSetPresParam(pOI->hwnd, PP_ACTIVETEXTFGNDCOLORINDEX, sizeof(LONG), SYSCLR_WINDOWTEXT);

         if(WinQueryFocus(HWND_DESKTOP) == pOI->hwnd)
         {
            // Display the text associated with the item.
            //GetTextMetrics(pOI->hps, &fm);
            GpiQueryFontMetrics(pOI->hps, sizeof(FONTMETRICS), &fm);
            y = (pOI->rclItem.yBottom + pOI->rclItem.yTop - fm.lEmHeight) / 2;

            ExtTextOut(pOI->hps,
                   CX_CHECKBOX + 5,
                   y,
                   0,
                   &(pOI->rclItem),
                   tchBuffer,
                   strlen(tchBuffer),
                   NULL);

          }
      
          DrawCheck(pOI, AC_COMPONENTS);

          // draw the focus rect on the selected item
          if((WinQueryFocus(HWND_DESKTOP) == pOI->hwnd) &&
            (pOI->fsState))
          {
            DrawFocusRect(pOI->hps, &(pOI->rclItem));
          }

          bReturn = TRUE;

          /* update the disk space required info in the dialog.  It is already
            in Kilobytes */
          ullDSBuf = GetDiskSpaceRequired(DSR_DOWNLOAD_SIZE);
          _ui64toa(ullDSBuf, tchBuffer, 10);
          strcpy(szBuf, tchBuffer);
          strcat(szBuf, " KB");
      
          WinSetDlgItemText(hDlg, IDC_DOWNLOAD_SIZE, szBuf);
          if(pOI->fsState)
          {
            WinSetPresParam(pOI->hwnd, PP_ACTIVETEXTFGNDCOLORINDEX, sizeof(LONG), SYSCLR_HILITEFOREGROUND);
            WinSetPresParam(pOI->hwnd, PP_BACKGROUNDCOLORINDEX, sizeof(LONG), SYSCLR_HILITEBACKGROUND);
          }
      }
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_LIST_COMPONENTS:
          /* to update the long description for each component the user selected */
          if((dwIndex = WinSendMsg(hwndLBComponents, LM_QUERYSELECTION, 0, 0)) != LIT_NONE)
            WinSetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(dwIndex, FALSE, AC_COMPONENTS));
          break;

        case IDWIZNEXT:
          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          WinDestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case MBID_CANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }

  return(bReturn);
}

MRESULT EXPENTRY DlgProcSelectAdditionalComponents(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
{
  BOOL                bReturn = FALSE;
  siC                 *siCTemp;
  ULONG               dwIndex;
  ULONG               dwItems = MAX_BUF;
  HWND                hwndLBComponents;
  RECTL                rDlg;
  UCHAR               tchBuffer[MAX_BUF];
  FONTMETRICS          fm;
  ULONG               y;
  POWNERITEM    pOI;
  ULONGLONG           ullDSBuf;
  char                szBuf[MAX_BUF];
  PSZ                 pszFontNameSize;
  ULONG             ulFontNameSizeLength;

  hwndLBComponents  = WinWindowFromID(hDlg, IDC_LIST_COMPONENTS);

  switch(msg)
  {
    case WM_INITDLG:
      DisableSystemMenuItems(hDlg, FALSE);
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

      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);

      /* update the disk space available info in the dialog.  GetDiskSpaceAvailable()
         returns value in kbytes */
      ullDSBuf = GetDiskSpaceAvailable(sgProduct.szPath);
      _ui64toa(ullDSBuf, tchBuffer, 10);
      ParsePath(sgProduct.szPath, szBuf, sizeof(szBuf), FALSE, PP_ROOT_ONLY);
      RemoveBackSlash(szBuf);
      strcat(szBuf, " - ");
      strcat(szBuf, tchBuffer);
      strcat(szBuf, " KB");

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetDlgItemText(hDlg, IDC_SPACE_AVAILABLE, szBuf);
      WinSetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szAdditionalComponents_);
      WinSetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szDescription);
      WinSetDlgItemText(hDlg, IDC_STATIC3, sgInstallGui.szTotalDownloadSize);
      WinSetDlgItemText(hDlg, IDC_STATIC4, sgInstallGui.szSpaceAvailable);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, MBID_CANCEL, sgInstallGui.szCancel_);

      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC2), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC3), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC4), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZBACK), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_LIST_COMPONENTS), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC_DESCRIPTION), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_DOWNLOAD_SIZE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_SPACE_AVAILABLE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);

      gdwACFlag = AC_ADDITIONAL_COMPONENTS;
      OldListBoxWndProc = WinSubclassWindow(hwndLBComponents, (PFNWP)NewListBoxWndProc);
      break;

    case WM_DRAWITEM:
      pOI = (POWNERITEM)lParam;

      // If there are no list box items, skip this message.
      if(pOI->idItem == -1)
        break;

      WinSendMsg(pOI->hItem, LM_QUERYITEMTEXT, pOI->idItem, (MPARAM)tchBuffer);

      if(pOI->fsState != pOI->fsStateOld)
      {
         if(pOI->fsState)
         {
            WinSetPresParam(pOI->hwnd, PP_ACTIVETEXTFGNDCOLORINDEX, sizeof(LONG), SYSCLR_HILITEFOREGROUND);
            WinSetPresParam(pOI->hwnd, PP_BACKGROUNDCOLORINDEX, sizeof(LONG), SYSCLR_HILITEBACKGROUND);
         }
         else
         {
            WinSetPresParam(pOI->hwnd, PP_INACTIVETEXTFGNDCOLORINDEX, sizeof(LONG), SYSCLR_SHADOWTEXT);
         }
         pOI->fsState = pOI->fsStateOld = 1;
      }
      else
      {
         siCTemp = SiCNodeGetObject(pOI->idItem, FALSE, AC_ADDITIONAL_COMPONENTS);
         if(siCTemp->dwAttributes & SIC_DISABLED)
            WinSetPresParam(pOI->hwnd, PP_INACTIVETEXTFGNDCOLORINDEX, sizeof(LONG), SYSCLR_SHADOWTEXT);
         else
         {
            WinSetPresParam(pOI->hwnd, PP_ACTIVETEXTFGNDCOLORINDEX, sizeof(LONG), SYSCLR_WINDOWTEXT);
            WinSetPresParam(pOI->hwnd, PP_BACKGROUNDCOLORINDEX, sizeof(LONG), SYSCLR_WINDOW);
         }

         if(WinQueryFocus(HWND_DESKTOP) == pOI->hwnd)
         {
            // Display the text associated with the item.
            GpiQueryFontMetrics(pOI->hps, sizeof(FONTMETRICS), &fm);
            y = (pOI->rclItem.yBottom + pOI->rclItem.yTop - fm.lEmHeight) / 2;

            ExtTextOut(pOI->hps,
                   CX_CHECKBOX + 5,
                   y,
                   0,
                   &(pOI->rclItem),
                   tchBuffer,
                   strlen(tchBuffer),
                   NULL);

          }
      
          DrawCheck(pOI, AC_ADDITIONAL_COMPONENTS);

          // draw the focus rect on the selected item
          if((WinQueryFocus(HWND_DESKTOP) == pOI->hwnd) &&
            (pOI->fsState))
          {
            DrawFocusRect(pOI->hps, &(pOI->rclItem));
          }

          bReturn = TRUE;

          /* update the disk space required info in the dialog.  It is already
            in Kilobytes */
          ullDSBuf = GetDiskSpaceRequired(DSR_DOWNLOAD_SIZE);
          _ui64toa(ullDSBuf, tchBuffer, 10);
          strcpy(szBuf, tchBuffer);
          strcat(szBuf, " KB");
      
          WinSetDlgItemText(hDlg, IDC_DOWNLOAD_SIZE, szBuf);
          if(pOI->fsState)
          {
            WinSetPresParam(pOI->hwnd, PP_ACTIVETEXTFGNDCOLORINDEX, sizeof(LONG), SYSCLR_HILITEFOREGROUND);
            WinSetPresParam(pOI->hwnd, PP_BACKGROUNDCOLORINDEX, sizeof(LONG), SYSCLR_HILITEBACKGROUND);
          }
      }
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_LIST_COMPONENTS:
          /* to update the long description for each component the user selected */
          if((dwIndex = WinSendMsg(hwndLBComponents, LM_QUERYSELECTION, 0, 0)) != LIT_NONE)
            WinSetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(dwIndex, FALSE, AC_ADDITIONAL_COMPONENTS));
          break;

        case IDWIZNEXT:
          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          WinDestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case MBID_CANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }

  return(bReturn);
}

MRESULT EXPENTRY DlgProcWindowsIntegration(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
{
  HWND hcbCheck0;
  HWND hcbCheck1;
  HWND hcbCheck2;
  HWND hcbCheck3;
  RECTL rDlg;
  PSZ pszFontNameSize;
  ULONG ulFontNameSizeLength;

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
        WinSendDlgItemMsg(hDlg, IDC_CHECK0, BM_SETCHECK, diWindowsIntegration.wiCB0.bCheckBoxState, 0L);
        WinSetDlgItemText(hDlg, IDC_CHECK0, diWindowsIntegration.wiCB0.szDescription);
      }
      else
        WinShowWindow(hcbCheck0, FALSE);

      if(diWindowsIntegration.wiCB1.bEnabled)
      {
        WinShowWindow(hcbCheck1, TRUE);
        WinSendDlgItemMsg(hDlg, IDC_CHECK1, BM_SETCHECK, diWindowsIntegration.wiCB1.bCheckBoxState, 0L);
        WinSetDlgItemText(hDlg, IDC_CHECK1, diWindowsIntegration.wiCB1.szDescription);
      }
      else
        WinShowWindow(hcbCheck1, FALSE);

      if(diWindowsIntegration.wiCB2.bEnabled)
      {
        WinShowWindow(hcbCheck2, TRUE);
        WinSendDlgItemMsg(hDlg, IDC_CHECK2, BM_SETCHECK, diWindowsIntegration.wiCB2.bCheckBoxState, 0L);
        WinSetDlgItemText(hDlg, IDC_CHECK2, diWindowsIntegration.wiCB2.szDescription);
      }
      else
        WinShowWindow(hcbCheck2, FALSE);

      if(diWindowsIntegration.wiCB3.bEnabled)
      {
        WinShowWindow(hcbCheck3, TRUE);
        WinSendDlgItemMsg(hDlg, IDC_CHECK3, BM_SETCHECK, diWindowsIntegration.wiCB3.bCheckBoxState, 0L);
        WinSetDlgItemText(hDlg, IDC_CHECK3, diWindowsIntegration.wiCB3.szDescription);
      }
      else
        WinShowWindow(hcbCheck3, FALSE);

      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, MBID_CANCEL, sgInstallGui.szCancel_);

      WinSetPresParam(WinWindowFromID(hDlg, IDWIZBACK), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_CHECK0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_CHECK1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_CHECK2), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_CHECK3), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          if(WinSendDlgItemMsg(hDlg, IDC_CHECK0, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
          {
          }

          if(diWindowsIntegration.wiCB0.bEnabled)
          {
            if(WinSendDlgItemMsg(hDlg, IDC_CHECK0, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
              diWindowsIntegration.wiCB0.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB0.bCheckBoxState = FALSE;
          }

          if(diWindowsIntegration.wiCB1.bEnabled)
          {
            if(WinSendDlgItemMsg(hDlg, IDC_CHECK1, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
              diWindowsIntegration.wiCB1.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB1.bCheckBoxState = FALSE;
          }

          if(diWindowsIntegration.wiCB2.bEnabled)
          {
            if(WinSendDlgItemMsg(hDlg, IDC_CHECK2, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
              diWindowsIntegration.wiCB2.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB2.bCheckBoxState = FALSE;
          }

          if(diWindowsIntegration.wiCB3.bEnabled)
          {
            if(WinSendDlgItemMsg(hDlg, IDC_CHECK3, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
              diWindowsIntegration.wiCB3.bCheckBoxState = TRUE;
            else
              diWindowsIntegration.wiCB3.bCheckBoxState = FALSE;
          }

          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          dwWizardState = DLG_WINDOWS_INTEGRATION;
          CheckWizardStateCustom(DLG_SETUP_TYPE);

          WinDestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case MBID_CANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

MRESULT EXPENTRY DlgProcProgramFolder(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
{
  char            szBuf[MAX_BUF];
  HDIR          hDir;
  ULONG           dwIndex;
  FILEFINDBUF3 wfdFindFileData;
  RECTL            rDlg;
  HINI            hiniInstall;
  PSZ           pszFontNameSize;
  ULONG        ulFontNameSizeLength;

  switch(msg)
  {
    case WM_INITDLG:
      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diProgramFolder.szTitle);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0, diProgramFolder.szMessage0);
      WinSetDlgItemText(hDlg, IDC_EDIT_PROGRAM_FOLDER, sgProduct.szProgramFolderName);

      strcpy(szBuf, sgProduct.szProgramFolderPath);
      strcat(szBuf, "\\*.*");      
      if((hDir = FindFirstFile(szBuf , &wfdFindFileData)) != ERROR_INVALID_HANDLE)
      {
        if((wfdFindFileData.attrFile & FILE_DIRECTORY) && (strcmpi(wfdFindFileData.achName, ".") != 0) && (strcmpi(wfdFindFileData.achName, "..") != 0))
        {
          WinSendDlgItemMsg(hDlg, IDC_LIST, LM_INSERTITEM, 0, (MPARAM)wfdFindFileData.achName);
        }

        while(FindNextFile(hDir, &wfdFindFileData))
        {
          if((wfdFindFileData.attrFile & FILE_DIRECTORY) && (strcmpi(wfdFindFileData.achName, ".") != 0) && (strcmpi(wfdFindFileData.achName, "..") != 0))
            WinSendDlgItemMsg(hDlg, IDC_LIST, LM_INSERTITEM, 0, (MPARAM)wfdFindFileData.achName);
        }
        DosFindClose(hDir);
      }

      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szProgramFolder_);
      WinSetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szExistingFolder_);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, MBID_CANCEL, sgInstallGui.szCancel_);

      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC2), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZBACK), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_EDIT_PROGRAM_FOLDER), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_LIST), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          WinQueryDlgItemText(hDlg, IDC_EDIT_PROGRAM_FOLDER, MAX_BUF, szBuf);
          if(*szBuf == '\0')
          {
            char szEProgramFolderName[MAX_BUF];
            hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
            PrfQueryProfileString(hiniInstall, "Messages", "ERROR_PROGRAM_FOLDER_NAME", "", szEProgramFolderName, sizeof(szEProgramFolderName));
            WinMessageBox(HWND_DESKTOP, hDlg, szEProgramFolderName, NULL, NULL, MB_OK | MB_ICONEXCLAMATION);
            PrfCloseProfile(hiniInstall);
            break;
          }
          strcpy(sgProduct.szProgramFolderName, szBuf);
          dwWizardState = DLG_ADVANCED_SETTINGS;

          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          WinDestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDC_LIST:
          if((dwIndex = WinSendDlgItemMsg(hDlg, IDC_LIST, LM_QUERYSELECTION, 0, 0)) != LIT_NONE)
          {
            WinSendDlgItemMsg(hDlg, IDC_LIST, LM_QUERYITEMTEXT, dwIndex, (MPARAM)szBuf);
            WinSetDlgItemText(hDlg, IDC_EDIT_PROGRAM_FOLDER, szBuf);
          }
          break;

        case MBID_CANCEL:
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
  if(WinSendDlgItemMsg(hDlg, IDC_USE_FTP, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
    diDownloadOptions.dwUseProtocol = UP_FTP;
  else if(WinSendDlgItemMsg(hDlg, IDC_USE_HTTP, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
    diDownloadOptions.dwUseProtocol = UP_HTTP;
}

MRESULT EXPENTRY DlgProcAdvancedSettings(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
{
  RECTL  rDlg;
  char  szBuf[MAX_BUF];
  HINI    hiniConfig;
  PSZ   pszFontNameSize;
  ULONG ulFontNameSizeLength;

  switch(msg)
  {
    case WM_INITDLG:
      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diAdvancedSettings.szTitle);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0,          diAdvancedSettings.szMessage0);
      WinSetDlgItemText(hDlg, IDC_EDIT_PROXY_SERVER, diAdvancedSettings.szProxyServer);
      WInSetDlgItemText(hDlg, IDC_EDIT_PROXY_PORT,   diAdvancedSettings.szProxyPort);
      WinSetDlgItemText(hDlg, IDC_EDIT_PROXY_USER,   diAdvancedSettings.szProxyUser);
      WinSetDlgItemText(hDlg, IDC_EDIT_PROXY_PASSWD, diAdvancedSettings.szProxyPasswd);

      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);
      hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
      PrfQueryProfileString(hiniConfig, "Strings", "IDC Use Ftp", "", szBuf, sizeof(szBuf));
      WinSetDlgItemText(hDlg, IDC_USE_FTP, szBuf);
      PrfQueryProfileString(hiniConfig, "Strings", "IDC Use Http", "", szBuf, sizeof(szBuf));
      PrfCloseProfile(hiniConfig);

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetDlgItemText(hDlg, IDC_USE_HTTP, szBuf);
      WinSetDlgItemText(hDlg, IDC_STATIC, sgInstallGui.szProxySettings);
      WinSetDlgItemText(hDlg, IDC_STATIC1, sgInstallGui.szServer);
      WinSetDlgItemText(hDlg, IDC_STATIC2, sgInstallGui.szPort);
      WinSetDlgItemText(hDlg, IDC_STATIC3, sgInstallGui.szUserId);
      WinSetDlgItemText(hDlg, IDC_STATIC4, sgInstallGui.szPassword);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szOk_);
      WinSetDlgItemText(hDlg, MBID_CANCEL, sgInstallGui.szCancel_);

      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC2), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC3), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC4), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_EDIT_PROXY_SERVER), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_EDIT_PROXY_PORT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_EDIT_PROXY_USER), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_EDIT_PROXY_PASSWD), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_USE_FTP), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_USE_HTTP), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);

      switch(diDownloadOptions.dwUseProtocol)
      {
        case UP_HTTP:
          WinSendDlgItemMsg(hDlg, IDC_USE_FTP, BM_SETCHECK, BTN_UNCHECKED, 0L);
          WinSendDlgItemMsg(hDlg, IDC_USE_HTTP, BM_SETCHECK, BTN_CHECKED, 0L);
          break;


        case UP_FTP:
        default: 
          WinSendDlgItemMsg(hDlg, IDC_USE_FTP, BM_SETCHECK, BTN_CHECKED, 0L);
          WinSendDlgItemMsg(hDlg, IDC_USE_HTTP, BM_SETCHECK, BTN_UNCHECKED, 0L);
          break;

      }

      if((diDownloadOptions.bShowProtocols) && (diDownloadOptions.bUseProtocolSettings))
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
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          dwWizardState = DLG_ADVANCED_SETTINGS;

          /* get the proxy server and port information */
          WinQueryDlgItemText(hDlg, IDC_EDIT_PROXY_SERVER, MAX_BUF, diAdvancedSettings.szProxyServer);
          WinQueryDlgItemText(hDlg, IDC_EDIT_PROXY_PORT, MAX_BUF, diAdvancedSettings.szProxyPort);
          WinQueryDlgItemText(hDlg, IDC_EDIT_PROXY_USER, MAX_BUF, diAdvancedSettings.szProxyUser);
          WinQueryDlgItemText(hDlg, IDC_EDIT_PROXY_PASSWD, MAX_BUF, diAdvancedSettings.szProxyPasswd);

          SaveDownloadProtocolOption(hDlg);
          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
        case MBID_CANCEL:
          WinDestroyWindow(hDlg);
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
  iIndex = WinSendMsg(hwndCBSiteSelector, LM_QUERYSELECTION, 0, 0);
  WinSendMsg(hwndCBSiteSelector, LM_QUERYITEMTEXT, (MPARAM)iIndex, (MPARAM)szSiteSelectorDescription);

  /* get the state of the Save Installer Files checkbox */
  if(WinSendDlgItemMsg(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, BM_QUERYCHECK, 0, 0L) == BTN_CHECKED)
    diDownloadOptions.bSaveInstaller = TRUE;
  else
    diDownloadOptions.bSaveInstaller = FALSE;
}

MRESULT EXPENTRY DlgProcDownloadOptions(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
{
  RECTL  rDlg;
  char  szBuf[MAX_BUF];
  HWND  hwndCBSiteSelector;
  int   iIndex;
  ssi   *ssiTemp;
  char  szCBDefault[MAX_BUF];
  HINI    hiniInstall;
  HINI    hiniConfig;
  PSZ   pszFontNameSize;
  ULONG ulFontNameSizeLength;

  hwndCBSiteSelector = WinWindowFromID(hDlg, IDC_LIST_SITE_SELECTOR);

  switch(msg)
  {
    case WM_INITDLG:
      if(gdwSiteSelectorStatus == SS_HIDE)
      {
        WinShowWindow(WinWindowFromID(hDlg, IDC_MESSAGE0),  FALSE);
        WinShowWindow(WinWindowFromID(hDlg, IDC_LIST_SITE_SELECTOR),  FALSE);
      }

      DisableSystemMenuItems(hDlg, FALSE);
      WinSetWindowText(hDlg, diDownloadOptions.szTitle);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0, diDownloadOptions.szMessage0);
      WinSetDlgItemText(hDlg, IDC_MESSAGE1, diDownloadOptions.szMessage1);

      hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
      PrfQueryProfileString(hiniConfig, "Strings", "IDC Save Installer Files", "", szBuf, sizeof(szBuf));
      PrfCloseProfile(hiniConfig);
      WinSetDlgItemText(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, szBuf);

      GetSaveInstallerPath(szBuf, sizeof(szBuf));

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetDlgItemText(hDlg, IDC_EDIT_LOCAL_INSTALLER_PATH, szBuf);
      WinSetDlgItemText(hDlg, IDC_BUTTON_PROXY_SETTINGS, sgInstallGui.szProxySettings_);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szNext_);
      WinSetDlgItemText(hDlg, MBID_CANCEL, sgInstallGui.szCancel_);

      WinSetPresParam(WinWindowFromID(hDlg, IDC_BUTTON_PROXY_SETTINGS), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZBACK), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_LIST_SITE_SELECTOR), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_EDIT_LOCAL_INSTALLER_PATH), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);

       if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);

      ssiTemp = ssiSiteSelector;
      do
      {
        if(ssiTemp == NULL)
          break;

        WinSendMsg(hwndCBSiteSelector, LM_INSERTITEM, 0, (MPARAM)(ssiTemp->szDescription));
        ssiTemp = ssiTemp->Next;
      } while(ssiTemp != ssiSiteSelector);

      if((szSiteSelectorDescription == NULL) || (*szSiteSelectorDescription == '\0'))
      {  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
          if(PrfQueryProfileString(hiniInstall, "Messages", "CB_DEFAULT", "", szCBDefault, sizeof(szCBDefault)) &&
          ((iIndex = WinSendMsg(hwndCBSiteSelector, LM_SEARCHSTRING, -1, (MPARAM)szCBDefault)) != LIT_ERROR))
          WinSendMsg(hwndCBSiteSelector, LM_SELECTITEM, (MPARAM)iIndex, 0);
        else
          WinSendMsg(hwndCBSiteSelector, LM_SELECTITEM, 0, 0);
        PrfCloseProfile(hiniInstall);
      }
      else if((iIndex = WinSendMsg(hwndCBSiteSelector, LM_SEARCHSTRING, -1, (MPARAM)szSiteSelectorDescription)) != LIT_ERROR)
        WinSendMsg(hwndCBSiteSelector, LM_SELECTITEM, (MPARAM)iIndex, 0);
      else
        WinSendMsg(hwndCBSiteSelector, LM_SELECTITEM, 0, 0);

      if(diDownloadOptions.bSaveInstaller)
         WinSendDlgItemMsg(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, BM_SETCHECK, BTN_CHECKED, 0L);
      else
         WinSendDlgItemMsg(hDlg, IDC_CHECK_SAVE_INSTALLER_FILES, BM_SETCHECK, BTN_UNCHECKED, 0L);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          SaveDownloadOptions(hDlg, hwndCBSiteSelector);
          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          SaveDownloadOptions(hDlg, hwndCBSiteSelector);
          WinDestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDC_BUTTON_ADDITIONAL_SETTINGS:
          SaveDownloadOptions(hDlg, hwndCBSiteSelector);
          dwWizardState = DLG_PROGRAM_FOLDER;
          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case MBID_CANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

void AppendStringWOAmpersand(PSZ szInputString, ULONG dwInputStringSize, PSZ szString)
{
  ULONG i;
  ULONG iInputStringCounter;
  ULONG iInputStringLen;
  ULONG iStringLen;


  iInputStringLen = strlen(szInputString);
  iStringLen      = strlen(szString);

  if((iInputStringLen + iStringLen) >= dwInputStringSize)
    return;

  iInputStringCounter = iInputStringLen;
  for(i = 0; i < iStringLen; i++)
  {
    if(szString[i] != '&')
      szInputString[iInputStringCounter++] = szString[i];
  }
}

PSZ GetStartInstallMessage()
{
  char  szBuf[MAX_BUF];
  char  szSTRRequired[MAX_BUF_TINY];
  siC   *siCObject   = NULL;
  PSZ szMessageBuf = NULL;
  ULONG dwBufSize;
  ULONG dwIndex0;
  HINI hiniConfig;
  HINI hiniInstall;

  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, "Strings", "STR Force Upgrade Required", "", szSTRRequired, sizeof(szSTRRequired));
  PrfCloseProfile(hiniConfig);
  /* calculate the amount of memory to allocate for the buffer */
  dwBufSize = 0;

  /* setup type */
  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  if(PrfQueryProfileString(hiniInstall, "Messages", "STR_SETUP_TYPE", "", szBuf, sizeof(szBuf)))
    dwBufSize += strlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters
  dwBufSize += 4; // take into account 4 indentation spaces

  switch(dwSetupType)
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
  if(PrfQueryProfileString(hiniInstall, "Messages", "STR_SELECTED_COMPONENTS", "", szBuf, sizeof(szBuf)))
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
  if(PrfQueryProfileString(hiniInstall, "Messages", "STR_DESTINATION_DIRECTORY", "", szBuf, sizeof(szBuf)))
    dwBufSize += strlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

  dwBufSize += 4; // take into account 4 indentation spaces
  dwBufSize += strlen(sgProduct.szPath) + 2; // the extra 2 bytes is for the \r\n characters
  dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

  /* program folder */
  if(PrfQueryProfileString(hiniInstall, "Messages", "STR_PROGRAM_FOLDER", "", szBuf, sizeof(szBuf)))
    dwBufSize += strlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

  dwBufSize += 4; // take into account 4 indentation spaces
  dwBufSize += strlen(sgProduct.szProgramFolderName) + 2; // the extra 2 bytes is for the \r\n\r\n characters

  if(GetTotalArchivesToDownload() > 0)
  {
    dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

    /* site selector info */
    if(PrfQueryProfileString(hiniInstall,"Messages", "STR_DOWNLOAD_SITE", "", szBuf, sizeof(szBuf)))
      dwBufSize += strlen(szBuf) + 2; // the extra 2 bytes is for the \r\n characters

    dwBufSize += 4; // take into account 4 indentation spaces
    dwBufSize += strlen(szSiteSelectorDescription) + 2; // the extra 2 bytes is for the \r\n characters

    if(diDownloadOptions.bSaveInstaller)
    {
      dwBufSize += 2; // the extra 2 bytes is for the \r\n characters

      /* site selector info */
      if(PrfQueryProfileString(hiniInstall, "Messages", "STR_SAVE_INSTALLER_FILES", "", szBuf, sizeof(szBuf)))
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
    if(PrfQueryProfileString(hiniInstall, "Messages", "STR_SETUP_TYPE", "", szBuf, sizeof(szBuf)))
    {
      strcat(szMessageBuf, szBuf);
      strcat(szMessageBuf, "\r\n");
    }
    strcat(szMessageBuf, "    "); // add 4 indentation spaces
      
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
    strcat(szMessageBuf, "\r\n\r\n");

    /* Selected Components */
    if(PrfQueryProfileString(hiniInstall, "Messages", "STR_SELECTED_COMPONENTS", "", szBuf, sizeof(szBuf)))
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
    if(PrfQueryProfileString(hiniInstall, "Messages", "STR_DESTINATION_DIRECTORY", "", szBuf, sizeof(szBuf)))
    {
      strcat(szMessageBuf, szBuf);
      strcat(szMessageBuf, "\r\n");
    }
    strcat(szMessageBuf, "    "); // add 4 indentation spaces
    strcat(szMessageBuf, sgProduct.szPath);
    strcat(szMessageBuf, "\r\n\r\n");

    /* program folder */
    if(PrfQueryProfileString(hiniInstall, "Messages", "STR_PROGRAM_FOLDER", "", szBuf, sizeof(szBuf)))
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
      if(PrfQueryProfileString(hiniInstall, "Messages", "STR_DOWNLOAD_SITE", "", szBuf, sizeof(szBuf)))
      {
        strcat(szMessageBuf, szBuf);
        strcat(szMessageBuf, "\r\n");
      }

      strcat(szMessageBuf, "    "); // add 4 indentation spaces
      strcat(szMessageBuf, szSiteSelectorDescription); // site selector description
      strcat(szMessageBuf, "\r\n");

      if(diDownloadOptions.bSaveInstaller)
      {
        strcat(szMessageBuf, "\r\n");

        /* site selector info */
        if(PrfQueryProfileString(hiniInstall, "Messages", "STR_SAVE_INSTALLER_FILES", "", szBuf, sizeof(szBuf)))
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
  PrfCloseProfile(hiniInstall);
  return(szMessageBuf);
}

MRESULT EXPENTRY DlgProcStartInstall(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
{
  RECTL  rDlg;
  PSZ szMessage = NULL;
  PSZ pszFontNameSize;
  ULONG ulFontNameSizeLength;

  switch(msg)
  {
    case WM_INITDLG:
      DisableSystemMenuItems(hDlg, FALSE);

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetWindowText(hDlg, diStartInstall.szTitle);

      WinSetDlgItemText(hDlg, IDC_STATIC, sgInstallGui.szCurrentSettings);
      WinSetDlgItemText(hDlg, IDWIZBACK, sgInstallGui.szBack_);
      WinSetDlgItemText(hDlg, IDWIZNEXT, sgInstallGui.szInstall_);
      WinSetDlgItemText(hDlg, MBID_CANCEL, sgInstallGui.szCancel_);

      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZBACK), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_CURRENT_SETTINGS), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);

      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
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
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case IDWIZBACK:
          dwWizardState = DLG_ADVANCED_SETTINGS;
          WinDestroyWindow(hDlg);
          DlgSequencePrev();
          break;

        case IDC_BUTTON_SITE_SELECTOR:
          dwWizardState = DLG_PROGRAM_FOLDER;
          WinDestroyWindow(hDlg);
          DlgSequenceNext();
          break;

        case MBID_CANCEL:
          AskCancelDlg(hDlg);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

MRESULT EXPENTRY DlgProcReboot(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
{
  LHANDLE            hToken;
  //TOKEN_PRIVILEGES  tkp;
  HWND              hRadioYes;
  RECTL              rDlg;
  PSZ                pszFontNameSize;
  ULONG             ulFontNameSizeLength;

  hRadioYes = WinWindowFromID(hDlg, IDC_RADIO_YES);

  switch(msg)
  {
    case WM_INITDLG:
      DisableSystemMenuItems(hDlg, FALSE);
      WinSendDlgItemMsg(hDlg, IDC_RADIO_YES, BM_SETCHECK, BTN_CHECKED, 0L);
      WinSetFocus(HWND_DESKTOP, hRadioYes);

      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetDlgItemText(hDlg, 202, sgInstallGui.szSetupMessage);
      WinSetDlgItemText(hDlg, IDC_RADIO_YES, sgInstallGui.szYesRestart);
      WinSetDlgItemText(hDlg, IDC_RADIO_NO, sgInstallGui.szNoRestart);
      WinSetDlgItemText(hDlg, MBID_OK, sgInstallGui.szOk);

      WinSetPresParam(WinWindowFromID(hDlg, 202), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_RADIO_YES), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_RADIO_NO), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_OK), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case MBID_OK:
          if(WinSendDlgItemMsg(hDlg, IDC_RADIO_YES, BM_QUERYCHECK, BTN_CHECKED, 0L) == BTN_CHECKED)
          {
            // Get a token for this process.
            //OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);

            // Get the LUID for the shutdown privilege.
            //LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
            //tkp.PrivilegeCount = 1;  // one privilege to set
            //tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            // Get the shutdown privilege for this process.
            //AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

            WinDestroyWindow(hDlg);
            PostQuitMessage(0);
            WinDestroyWindow(hWndMain);

            // Reboot the system and force all applications to close.
           //ExitWindowsEx(EWX_REBOOT, 0);
            {
            HFILE handle;
            ULONG ulAction;
            APIRET rc;

            DosOpen((PSZ)DOSDD12, &handle, &ulAction, 0L, FILE_SYSTEM,
            OPEN_ACTION_OPEN_IF_EXISTS, OPEN_SHARE_DENYNONE, 0L);

            if (rc == NO_ERROR) {
                DosDevIOCtl(handle, 0xD5, 0xAB, NULL, 0, NULL, 0, 0, NULL);
            }
            }

            //WinShutDownSystem((HAB)0, (HMQ)0);
          }
          else
          {
            WinDestroyWindow(hDlg);
            PostQuitMessage(0);
          }
          break;

        case MBID_CANCEL:
          WinDestroyWindow(hDlg);
          PostQuitMessage(0);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

MRESULT EXPENTRY DlgProcMessage(HWND hDlg, ULONG msg, MPARAM wParam, LONG lParam)
{
  RECTL      rDlg;
  HWND      hSTMessage = WinWindowFromID(hDlg, IDC_MESSAGE); /* handle to the Static Text message window */
  HDC       hdcSTMessage;
  HPS       hpsSTMessage;
  SIZEL      sizeString;
  //LOGFONT   logFont;
  //HFONT     hfontTmp;
  //HFONT     hfontOld;
  char      szBuf[MAX_BUF];
  char      szBuf2[MAX_BUF];
  HINI        hiniInstall;
  PSZ       pszFontNameSize  = NULL;

  memset(szBuf, 0, sizeof(szBuf));
  memset(szBuf2, 0, sizeof(szBuf2));

  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  switch(msg)
  {
    case WM_INITDLG:
      if(PrfQueryProfileString(hiniInstall, "Messages", "STR_MESSAGEBOX_TITLE", "", szBuf2, sizeof(szBuf2)))      {
        if((sgProduct.szProductName != NULL) && (*sgProduct.szProductName != '\0'))
          sprintf(szBuf, szBuf2, sgProduct.szProductName);
        else
          sprintf(szBuf, szBuf2, "");
      }
      else if((sgProduct.szProductName != NULL) && (*sgProduct.szProductName != '\0'))
        strcpy(szBuf, sgProduct.szProductName);

      WinSetWindowText(hDlg, szBuf);
      if(WinQueryWindowRect(hDlg, &rDlg))
        WinSetWindowPos(hDlg,
                     HWND_TOP,
                     (gSystemInfo.dwScreenX/2)-(rDlg.xRight/2),
                     (gSystemInfo.dwScreenY/2)-(rDlg.yBottom/2),
                     0,
                     0,
                     SWP_MOVE);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_MESSAGE:
          //hdcSTMessage = GetWindowDC(hSTMessage);
          hpsSTMessage = WinGetPS(hSTMessage);

          //SystemParametersInfo(SPI_GETICONTITLELOGFONT,
          //                     sizeof(logFont),
          //                     (PVOID)&logFont,
          //                     0);
          //hfontTmp = CreateFontIndirect(&logFont);

          pszFontNameSize = myGetSysFont();

          if(pszFontNameSize != NULL)
            //hfontOld = SelectObject(hdcSTMessage, hfontTmp);
            

          GetTextExtentPoint32(hpsSTMessage, (PSZ)lParam, strlen((PSZ)lParam), &sizeString);
          //SelectObject(hdcSTMessage, hfontOld);
          //DeleteObject(hfontTmp);
          ReleaseDC(hSTMessage, hdcSTMessage);

          WinSetWindowPos(hDlg, HWND_TOP,
                      (gSystemInfo.dwScreenX/2)-((sizeString.cx + 55)/2),
                      (gSystemInfo.dwScreenY/2)-((sizeString.cy + 50)/2),
                      sizeString.cx + 55,
                      sizeString.cy + 50,
                      SWP_SHOW);

          if(WinQueryWindowRect(hDlg, &rDlg))
            WinSetWindowPos(hSTMessage,
                         HWND_TOP,
                         rDlg.xLeft,
                         rDlg.yTop,
                         rDlg.xRight,
                         rDlg.yBottom,
                         SWP_SHOW);

          WinSetDlgItemText(hDlg, IDC_MESSAGE, (PSZ)lParam);
          break;
     }
      break;
  }
  PrfCloseProfile(hiniInstall);
  return(0);
}

void ProcessWindowsMessages()
{
  PQMSG pqmsg;
  while(WinPeekMessage((HAB)0, pqmsg, 0, 0, 0, PM_REMOVE))
  {
    //TranslateMessage(&msg);
    WinDispatchMessage(pqmsg);
  }
}

void ShowMessage(PSZ szMessage, BOOL bShow)
{
  char szBuf[MAX_BUF];
  HINI hiniInstall;
  PSZ pszFontNameSize;
  ULONG ulFontNameSizeLength;
 
  if(sgProduct.dwMode != SILENT)
  {
    if((bShow) && (hDlgMessage == NULL))
    {
      memset(szBuf, 0, sizeof(szBuf));
      hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
      PrfQueryProfileString(hiniInstall, "Messages", "MB_MESSAGE_STR", "", szBuf, sizeof(szBuf));
      PrfCloseProfile(hiniInstall);
      hDlgMessage = InstantiateDialog(hWndMain, DLG_MESSAGE, szBuf, DlgProcMessage);
      WinSendMsg(hDlgMessage, WM_COMMAND, IDC_MESSAGE, (MPARAM)szMessage);
      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;
      WinSetPresParam(WinWindowFromID(hDlgMessage, IDC_MESSAGE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);      
    }
    else if(!bShow && hDlgMessage)
    {
      WinDestroyWindow(hDlgMessage);
      hDlgMessage = NULL;
    }
  }
}

HWND InstantiateDialog(HWND hParent, ULONG dwDlgID, PSZ szTitle, PFNWP wpDlgProc)
{
  char szBuf[MAX_BUF];
  HWND hDlg = NULL;
  HINI hiniInstall;

  if((hDlg = WinCreateDlg(hParent, NULLHANDLE, wpDlgProc, MAKEINTRESOURCE(dwDlgID), NULL)) == NULL)
  {
    char szEDialogCreate[MAX_BUF];
    hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
    if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_DIALOG_CREATE", "", szEDialogCreate, sizeof(szEDialogCreate)))
    {
      sprintf(szBuf, szEDialogCreate, szTitle);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    PrfCloseProfile(hiniInstall);
    PostQuitMessage(1);
  }
  return(hDlg);
}

BOOL CheckWizardStateCustom(ULONG dwDefault)
{
  if(sgProduct.dwCustomType != dwSetupType)
  {
    dwWizardState = dwDefault;
    return(FALSE);
  }

  return(TRUE);
}

void DlgSequenceNext()
{
  APIRET hrValue;
  APIRET hrErr;
  char    szDestPath[CCHMAXPATHCOMP];
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
          dwWizardState = DLG_ADVANCED_SETTINGS;
        }
        break;

      case DLG_ADVANCED_SETTINGS:
        dwWizardState = DLG_DOWNLOAD_OPTIONS;
        gbProcessingXpnstallFiles = FALSE;

        do
        {
          hrValue = VerifyDiskSpace();
          if(hrValue == MBID_OK)
          {
            /* show previous visible window */
            dwWizardState = DLG_SELECT_COMPONENTS;
            DlgSequencePrev();
            bDone = TRUE;
            break;
          }
          else if(hrValue == MBID_CANCEL)
          {
            AskCancelDlg(hWndMain);
            hrValue = MBID_RETRY;
          }
        }while(hrValue == MBID_RETRY);

        if(hrValue == MBID_OK)
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
        DeleteFile(szInstallLogFile);

        /* copy the install_status.log file from the temp\ns_temp dir to
         * the destination dir and use the new destination file to continue
         * logging.
         */
        strcpy(szInstallLogFile, szTempDir);
        AppendBackSlash(szInstallLogFile, sizeof(szInstallLogFile));
        strcat(szInstallLogFile, FILE_INSTALL_STATUS_LOG);
        FileCopy(szInstallLogFile, szDestPath, FALSE, FALSE);
        DeleteFile(szInstallLogFile);

        /* PRE_DOWNLOAD process file manipulation functions */
        ProcessFileOps(T_PRE_DOWNLOAD, NULL);

        if(RetrieveArchives() == WIZ_OK)
        {
          /* POST_DOWNLOAD process file manipulation functions */
          ProcessFileOps(T_POST_DOWNLOAD, NULL);
          /* PRE_XPCOM process file manipulation functions */
          ProcessFileOps(T_PRE_XPCOM, NULL);

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
          ProcessFileOps(T_POST_XPCOM, NULL);
          /* PRE_SMARTUPDATE process file manipulation functions */
          ProcessFileOps(T_PRE_SMARTUPDATE, NULL);

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

          strcat(szDestPath, "uninstall\\");
          CreateDirectoriesAll(szDestPath, TRUE);

          /* save the installer files in the local machine */
          if(diDownloadOptions.bSaveInstaller)
            SaveInstallerFiles();

          hrErr = SmartUpdateJars();
          if((hrErr == WIZ_OK) || (hrErr == 999))
          {
            UpdateJSProxyInfo();

            /* POST_SMARTUPDATE process file manipulation functions */
            ProcessFileOps(T_POST_SMARTUPDATE, NULL);
            /* PRE_LAUNCHAPP process file manipulation functions */
            ProcessFileOps(T_PRE_LAUNCHAPP, NULL);

            LaunchApps();

            /* POST_LAUNCHAPP process file manipulation functions */
            ProcessFileOps(T_POST_LAUNCHAPP, NULL);
            /* DEPEND_REBOOT process file manipulation functions */
            ProcessFileOps(T_DEPEND_REBOOT, NULL);
            ClearWinRegUninstallFileDeletion();
            if(!gbIgnoreProgramFolderX)
              ProcessProgramFolderShowCmd();

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
        dwWizardState = DLG_PROGRAM_FOLDER;
        gbProcessingXpnstallFiles = FALSE;
        if(CheckWizardStateCustom(DLG_SELECT_COMPONENTS))
        {
          if(diProgramFolder.bShowDialog)
            hDlgCurrent = InstantiateDialog(hWndMain, dwWizardState, diProgramFolder.szTitle, DlgProcProgramFolder);
          bDone = TRUE;
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

BOOL ExtTextOut(HPS aPS, int X, int Y, UINT fuOptions, const RECTL* lprc,
                const char* aString, unsigned int aLength, const int* pSpacing)
{
  POINTL ptl = {X, Y};

  GpiMove (aPS, &ptl);

  // GpiCharString has a max length of 512 chars at a time...
  while( aLength)
  {
    ULONG ulChunkLen = min(aLength, 512);
    if (pSpacing)
    {
      GpiCharStringPos (aPS, NULL, CHS_VECTOR, ulChunkLen,
                               (PCH)aString, (PLONG)pSpacing);
        pSpacing += ulChunkLen;
    }
    else
    {
      GpiCharString (aPS, ulChunkLen, (PCH)aString);
    }
    aLength -= ulChunkLen;
    aString += ulChunkLen;
  }
  return TRUE;
}

PSZ myGetSysFont()
{
  PSZ szFontNameSize[MAXNAMEL];
  PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "IconText",
                              "9.WarpSans",
                              szFontNameSize, MAXNAMEL);
  return szFontNameSize;
}

