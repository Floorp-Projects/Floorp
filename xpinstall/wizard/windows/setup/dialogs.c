/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Sean Su <ssu@netscape.com>
 */

#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"
#include "xpistub.h"
#include "xpi.h"

static WNDPROC  OldListBoxWndProc;
static int      iLBCheckboxRedraw = 0;

void AskCancelDlg(HWND hDlg)
{
  char szDlgQuitTitle[MAX_BUF];
  char szDlgQuitMsg[MAX_BUF];

  if(NS_LoadString(hSetupRscInst, IDS_DLGQUITTITLE, szDlgQuitTitle, MAX_BUF) != WIZ_OK)
    PostQuitMessage(1);
  else if(NS_LoadString(hSetupRscInst, IDS_DLGQUITMSG, szDlgQuitMsg, MAX_BUF) != WIZ_OK)
    PostQuitMessage(1);
  else if(MessageBox(hDlg, szDlgQuitMsg, szDlgQuitTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_APPLMODAL | MB_SETFOREGROUND) == IDYES)
  {
    DestroyWindow(hDlg);
    PostQuitMessage(0);
  }
} 

LRESULT CALLBACK DlgProcMain(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  HDC           hdc;
  PAINTSTRUCT   ps;

  switch(msg)
  {
    case WM_CREATE:
      hWndMain = hWnd;
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case ID_WIZNEXT:
          DlgSequenceNext();
          break;

        case ID_WIZBACK:
          DlgSequencePrev();
          break;

        default:
          break;
      }
      break;

    case WM_PAINT:
      hdc = BeginPaint(hWnd, &ps);
      // Add any drawing code here...

      OutputTitle(hdc, sgProduct.szProductName);

      EndPaint(hWnd, &ps);
      break;

    case WM_CLOSE:
      AskCancelDlg(hWnd);
      break;

    default:
      return(DefWindowProc(hWnd, msg, wParam, lParam));
  }
  return(0);
}

LRESULT CALLBACK DlgProcWelcome(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  char szBuf[MAX_BUF];
  RECT rDlg;
  
  switch(msg)
  {
    case WM_INITDIALOG:
      SetWindowText(hDlg, diWelcome.szTitle);

      wsprintf(szBuf, diWelcome.szMessage0, sgProduct.szProductName);
      SetDlgItemText(hDlg, IDC_STATIC0, szBuf);
      SetDlgItemText(hDlg, IDC_STATIC1, diWelcome.szMessage1);
      SetDlgItemText(hDlg, IDC_STATIC2, diWelcome.szMessage2);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.right/2), (dwScreenY/2)-(rDlg.bottom/2), 0, 0, SWP_NOSIZE);
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case ID_WIZNEXT:
          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
          break;

        case ID_WIZBACK:
          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
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
      SetWindowText(hDlg, diLicense.szTitle);

      lstrcpy(szBuf, szSetupDir);
      lstrcat(szBuf, "\\");
      lstrcat(szBuf, diLicense.szLicenseFilename);

      if((hFLicense = FindFirstFile(szBuf, &wfdFindFileData)) != INVALID_HANDLE_VALUE)
      {
        dwFileSize = (wfdFindFileData.nFileSizeHigh * MAXDWORD) + wfdFindFileData.nFileSizeLow;
        FindClose(hFLicense);
        if((szLicenseFilenameBuf = NS_GlobalAlloc(dwFileSize)) != NULL)
        {
          if((fLicense = fopen(szBuf, "r+b")) != NULL)
          {
            dwBytesRead = fread(szLicenseFilenameBuf, 1, dwFileSize, fLicense);
            fclose(fLicense);
            SetDlgItemText(hDlg, IDC_EDIT_LICENSE, szLicenseFilenameBuf);
          }

          FreeMemory(&szLicenseFilenameBuf);
        }
      }

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.right/2), (dwScreenY/2)-(rDlg.bottom/2), 0, 0, SWP_NOSIZE);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case ID_WIZNEXT:
          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
          break;

        case ID_WIZBACK:
          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
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

LRESULT CALLBACK BrowseHookProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  DWORD dwIndex;
  DWORD dwLoop;
  RECT  rDlg;
  char  szBuf[MAX_BUF];
  char  szBufIndex[MAX_BUF];
  char  szPath[MAX_BUF];
  char  szTempPath[MAX_BUF];

  switch(message)
  {
    case WM_INITDIALOG:
      SetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szTempSetupPath);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.right/2), (dwScreenY/2)-(rDlg.bottom/2), 0, 0, SWP_NOSIZE);
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case 1121:
          if(HIWORD(wParam) == LBN_DBLCLK)
          {
            SendDlgItemMessage(hDlg, 1121, LB_GETTEXT, 0, (LPARAM)szBuf);
            lstrcpy(szPath, szBuf);
            dwIndex = SendDlgItemMessage(hDlg, 1121, LB_GETCURSEL, 0, (LPARAM)0);
            for(dwLoop = 1; dwLoop <= dwIndex; dwLoop++)
            {
              SendDlgItemMessage(hDlg, 1121, LB_GETTEXT, dwIndex, (LPARAM)szBufIndex);
              lstrcpy(szTempPath, szPath);
              AppendBackSlash(szTempPath, sizeof(szTempPath));
              lstrcat(szTempPath, szBufIndex);
              if(FileExists(szTempPath))
              {
                AppendBackSlash(szPath, sizeof(szPath));
                lstrcpy(szPath, szTempPath);
                break;
              }
              else
              {
                SendDlgItemMessage(hDlg, 1121, LB_GETTEXT, dwLoop, (LPARAM)szBuf);
                lstrcpy(szTempPath, szPath);
                AppendBackSlash(szTempPath, sizeof(szTempPath));
                lstrcat(szTempPath, szBuf);

                if(FileExists(szTempPath))
                {
                  AppendBackSlash(szPath, sizeof(szPath));
                  lstrcpy(szPath, szTempPath);
                }
              }
            }
            SetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szPath);
          }
          break;

        case IDOK:
          GetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szTempSetupPath, MAX_BUF);
          DestroyWindow(hDlg);
          break;
      }
      break;
  }
  return(0);
}

BOOL BrowseForDirectory(HWND hDlg, char *szCurrDir)
{ 
  OPENFILENAME   of;
  char           ftitle [MAX_PATH];
  char           fname  [MAX_PATH];
  char           szCDir [MAX_BUF];
  char           szDlgBrowseTitle[MAX_BUF];
  BOOL           bRet;

  /* save the current directory */
  GetCurrentDirectory(MAX_BUF, szCDir);

  ZeroMemory(szDlgBrowseTitle, sizeof(szDlgBrowseTitle));
  NS_LoadString(hSetupRscInst, IDS_DLGBROWSETITLE, szDlgBrowseTitle, MAX_BUF);

  ftitle[0] = 0;
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
  of.lpstrInitialDir    = szCurrDir;
  of.lpstrTitle         = szDlgBrowseTitle;
  of.Flags              = OFN_NONETWORKBUTTON |
                          OFN_ENABLEHOOK      |
                          OFN_NOCHANGEDIR     |
                          OFN_ENABLETEMPLATE;
  of.nFileOffset        = 0;
  of.nFileExtension     = 0;
  of.lpstrDefExt        = NULL;
  of.lCustData          = 0;
  of.lpfnHook           = BrowseHookProc;
  of.lpTemplateName     = MAKEINTRESOURCE(DLG_BROWSE_DIR);

  if(GetSaveFileName(&of))
    bRet = TRUE;
  else
    bRet = FALSE;

  /* restore the current directory */
  SetCurrentDirectory(szCDir);
  return(bRet);
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
  RECT          rDlg;
  char          szBuf[MAX_BUF];
  char          szBufTemp[MAX_BUF];

  hRadioSt0   = GetDlgItem(hDlg, IDC_RADIO_ST0);
  hStaticSt0  = GetDlgItem(hDlg, IDC_STATIC_ST0_DESCRIPTION);
  hRadioSt1   = GetDlgItem(hDlg, IDC_RADIO_ST1);
  hStaticSt1  = GetDlgItem(hDlg, IDC_STATIC_ST1_DESCRIPTION);
  hRadioSt2   = GetDlgItem(hDlg, IDC_RADIO_ST2);
  hStaticSt2  = GetDlgItem(hDlg, IDC_STATIC_ST2_DESCRIPTION);
  hRadioSt3   = GetDlgItem(hDlg, IDC_RADIO_ST3);
  hStaticSt3  = GetDlgItem(hDlg, IDC_STATIC_ST3_DESCRIPTION);

  switch(msg)
  {
    case WM_INITDIALOG:
      if(bCreateDestinationDir)
        DirectoryRemove(sgProduct.szPath, FALSE);

      SetWindowText(hDlg, diSetupType.szTitle);

      SetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szTempSetupPath);
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
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.right/2), (dwScreenY/2)-(rDlg.bottom/2), 0, 0, SWP_NOSIZE);

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

          GetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szTempSetupPath, MAX_PATH);
          BrowseForDirectory(hDlg, szTempSetupPath);

          /* fix: hack to work around bug with this dlg proc no longer being called
             after returning from BrowseForDirectory() */
          DestroyWindow(hDlg);
          dwWizardState = DLG_LICENSE;
          DlgSequenceNext();
          break;

        case ID_WIZNEXT:
          GetDlgItemText(hDlg, IDC_EDIT_DESTINATION, szTempSetupPath, MAX_PATH);
          lstrcpy(sgProduct.szPath, szTempSetupPath);

          /* append a backslash to the path because CreateDirectoriesAll()
             uses a backslash to determine directories */
          lstrcpy(szBuf, sgProduct.szPath);
          AppendBackSlash(szBuf, sizeof(szBuf));

          if(FileExists(szBuf) == FALSE)
          {
            if(CreateDirectoriesAll(szBuf) == FALSE)
            {
              char szECreateDirectory[MAX_BUF];

              lstrcpy(szBufTemp, "\n\n    ");
              lstrcat(szBufTemp, sgProduct.szPath);
              lstrcat(szBufTemp, "\n\n");

              if(NS_LoadString(hSetupRscInst, IDS_ERROR_CREATE_DIRECTORY, szECreateDirectory, MAX_BUF) == WIZ_OK)
                wsprintf(szBuf, szECreateDirectory, szBufTemp);

              MessageBox(hDlg, szBuf, "", MB_OK | MB_ICONERROR);
              break;
            }
            bCreateDestinationDir = TRUE;
          }
          else
          {
            bCreateDestinationDir = FALSE;
          }

          /* retrieve and save the state of the selected radio button */
          if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST0)      == BST_CHECKED)
          {
            dwSetupType     = ST_RADIO0;
            dwTempSetupType = dwSetupType;
            SiCNodeSetItemsSelected(diSetupType.stSetupType0.dwItems, diSetupType.stSetupType0.dwItemsSelected);
          }
          else if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST1) == BST_CHECKED)
          {
            dwSetupType     = ST_RADIO1;
            dwTempSetupType = dwSetupType;
            SiCNodeSetItemsSelected(diSetupType.stSetupType1.dwItems, diSetupType.stSetupType1.dwItemsSelected);
          }
          else if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST2) == BST_CHECKED)
          {
            dwSetupType     = ST_RADIO2;
            dwTempSetupType = dwSetupType;
            SiCNodeSetItemsSelected(diSetupType.stSetupType2.dwItems, diSetupType.stSetupType2.dwItemsSelected);
          }
          else if(IsDlgButtonChecked(hDlg, IDC_RADIO_ST3) == BST_CHECKED)
          {
            dwSetupType     = ST_RADIO3;
            dwTempSetupType = dwSetupType;
            SiCNodeSetItemsSelected(diSetupType.stSetupType3.dwItems, diSetupType.stSetupType3.dwItemsSelected);
          }

          /* set the next dialog to be shown depending on the 
             what the user selected */
          dwWizardState = DLG_SETUP_TYPE;
          CheckWizardStateCustom(DLG_WINDOWS_INTEGRATION);

          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
          break;

        case ID_WIZBACK:
          dwTempSetupType = dwSetupType;
          lstrcpy(szTempSetupPath, sgProduct.szPath);
          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
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

void DrawCheck(LPDRAWITEMSTRUCT lpdis, HDC hdcMem)
{
  siC     *siCTemp  = NULL;
  HBITMAP hbmpCheck;

  siCTemp = SiCNodeGetObject(lpdis->itemID, FALSE);
  if(siCTemp != NULL)
  {
    if(siCTemp->dwAttributes & SIC_SELECTED)
      hbmpCheck = LoadBitmap(hSetupRscInst, MAKEINTRESOURCE(IDB_BOX_CHECKED));
    else
      hbmpCheck = LoadBitmap(hSetupRscInst, MAKEINTRESOURCE(IDB_BOX_UNCHECKED));

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

    SendMessage(lpdis->hwndItem, LB_SETITEMDATA, lpdis->itemID, (LPARAM)hbmpCheck);
  }
}

void lbAddItem(HWND hList, siC *siCComponent)
{
  DWORD dwItem;

  dwItem = SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)siCComponent->szDescriptionShort);
  if(siCComponent->dwAttributes & SIC_SELECTED)
    SendMessage(hList, LB_SETITEMDATA, dwItem, (LPARAM)hbmpBoxChecked);
  else
    SendMessage(hList, LB_SETITEMDATA, dwItem, (LPARAM)hbmpBoxUnChecked);
} 

void InvalidateLBCheckbox(HWND hwndListBox)
{
//  BOOL bReturn;
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
//  bReturn = RedrawWindow(hwndListBox, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void ToggleCheck(HWND hwndListBox, DWORD dwIndex)
{
  BOOL bMoreToResolve;

  // Checks to see if the checkbox is checked or not checked, and
  // toggles the node attributes appropriately.
  if(SiCNodeGetAttributes(dwIndex) & SIC_SELECTED)
  {
    SiCNodeSetAttributes(dwIndex, SIC_SELECTED, FALSE);
  }
  else
  {
    SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE);
    bMoreToResolve = ResolveDependencies(dwIndex);
    while(bMoreToResolve)
    {
      bMoreToResolve = ResolveDependencies(-1);
    }
    iLBCheckboxRedraw = 2;
  }

  InvalidateLBCheckbox(hwndListBox);
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
  DWORD       dwPosX;
  DWORD       dwPosY;
  DWORD       dwIndex;

  switch(uMsg)
  {
    case WM_CHAR:
      if((TCHAR)wParam == 32)
      {
        dwIndex = SendMessage(hWnd,
                              LB_GETCURSEL,
                              0,
                              0);
        ToggleCheck(hWnd, dwIndex);
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
          ToggleCheck(hWnd, dwIndex);
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
  DWORD               dwCurrentItem;
  DWORD               dwArrayIndex;
  DWORD               dwIndex;
  DWORD               dwItems = MAX_BUF;
  DWORD               dwItemsSelected[MAX_BUF];
  HWND                hwndLBComponents;
  RECT                rDlg;
  RECT                rLBComponentSize;
  RECT                rListBox;
  HBITMAP             hbmpOld;
  HBITMAP             hbmpLast;
  TCHAR               tchBuffer[MAX_BUF];
  TEXTMETRIC          tm;
  DWORD               y;
  HDC                 hdcMem;
  HDC                 hdcComponentSize;
  LPDRAWITEMSTRUCT    lpdis;
//  PAINTSTRUCT         ps;
  RECT                rTemp;
  BOOL                bR;
  ULONGLONG           ullDSBuf;
  char                szBuf[MAX_BUF];

  hwndLBComponents  = GetDlgItem(hDlg, IDC_LIST_COMPONENTS);

  switch(msg)
  {
    case WM_INITDIALOG:
      SetWindowText(hDlg, diSelectComponents.szTitle);

      siCTemp = siComponents;
      if(siCTemp != NULL)
      {
        lbAddItem(hwndLBComponents, siCTemp);

        siCTemp = siCTemp->Next;
        while((siCTemp != siComponents) && (siCTemp != NULL))
        {
          lbAddItem(hwndLBComponents, siCTemp);
          siCTemp = siCTemp->Next;
        }
        SetFocus(hwndLBComponents);
        SendMessage(hwndLBComponents, LB_SETCURSEL, 0, 0);
        SetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(0));
      }

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.right/2), (dwScreenY/2)-(rDlg.bottom/2), 0, 0, SWP_NOSIZE);

      /* update the disk space available info in the dialog.  GetDiskSpaceAvailable()
         returns value in bytes, therefore a division by 1000 is required to be in
         kilobytes */
      if((ullDSBuf = GetDiskSpaceAvailable(sgProduct.szPath)) > 1000)
        ullDSBuf /= 1000;
      else
        ullDSBuf = 0;

      _ui64toa(ullDSBuf, tchBuffer, 10);
      ParsePath(sgProduct.szPath, szBuf, sizeof(szBuf), PP_ROOT_ONLY);
      RemoveBackSlash(szBuf);
      lstrcat(szBuf, " - ");
      lstrcat(szBuf, tchBuffer);
      lstrcat(szBuf, " K");
      SetDlgItemText(hDlg, IDC_STATIC_DRIVE_SPACE_AVAILABLE, szBuf);

      OldListBoxWndProc = SubclassWindow(hwndLBComponents, (WNDPROC)NewListBoxWndProc);
      break;

//    case WM_PAINT:
//    hdc = BeginPaint(hDlg, &ps);
//      // Add any drawing code here...
//
//      OutputTitle(hdc, sgProduct.szProductName);
//
//      EndPaint(hWnd, &ps);
//      break;
//
    case WM_DRAWITEM:
      lpdis = (LPDRAWITEMSTRUCT)lParam;
      SendMessage(lpdis->hwndItem, LB_GETTEXT, lpdis->itemID, (LPARAM)tchBuffer);
      hbmpLast = (HBITMAP)SendMessage(lpdis->hwndItem,
                                       LB_GETITEMDATA,
                                       lpdis->itemID,
                                       (LPARAM)0);

      bR = GetClientRect(lpdis->hwndItem, &rTemp);

      hdcMem  = CreateCompatibleDC(lpdis->hDC);
      hbmpOld = SelectObject(hdcMem, hbmpLast);
      hdcComponentSize = GetDC(lpdis->hwndItem);
      SelectObject(hdcComponentSize, GetCurrentObject(lpdis->hDC, OBJ_FONT));

      // If there are no list box items, skip this message.
      if(lpdis->itemID == -1)
        break;

//      SetTextColor(lpdis->hDC,        GetSysColor(COLOR_WINDOWTEXT));
//      SetBkColor(lpdis->hDC,          GetSysColor(COLOR_WINDOW));
//      SetTextColor(hdcComponentSize,  GetSysColor(COLOR_WINDOWTEXT));
//      SetBkColor(hdcComponentSize,    GetSysColor(COLOR_WINDOW));

      if((lpdis->itemAction & ODA_FOCUS) && (lpdis->itemState & ODS_SELECTED))
      {
        // remove the focus rect on the previous selected item
        DrawFocusRect(lpdis->hDC, &(lpdis->rcItem));
      }

      if(lpdis->itemAction & ODA_FOCUS)
      {
        if((lpdis->itemState & ODS_SELECTED) &&
          !(lpdis->itemState & ODS_FOCUS))
        {
          SetTextColor(lpdis->hDC,        GetSysColor(COLOR_WINDOWTEXT));
          SetBkColor(lpdis->hDC,          GetSysColor(COLOR_WINDOW));
          SetTextColor(hdcComponentSize,  GetSysColor(COLOR_WINDOWTEXT));
          SetBkColor(hdcComponentSize,    GetSysColor(COLOR_WINDOW));
        }
        else
        {
          SetTextColor(lpdis->hDC,        GetSysColor(COLOR_HIGHLIGHTTEXT));
          SetBkColor(lpdis->hDC,          GetSysColor(COLOR_HIGHLIGHT));
          SetTextColor(hdcComponentSize,  GetSysColor(COLOR_HIGHLIGHTTEXT));
          SetBkColor(hdcComponentSize,    GetSysColor(COLOR_HIGHLIGHT));
        }
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

        siCTemp = SiCNodeGetObject(lpdis->itemID, FALSE);
        _ui64toa(siCTemp->ullInstallSize, tchBuffer, 10);
        lstrcat(tchBuffer, " K");

        /* calculate clipping region.  The region being the entire listbox window */
        GetClientRect(hwndLBComponents, &rListBox);
        if(lpdis->rcItem.bottom > rListBox.bottom)
        {
          rLBComponentSize.bottom = rListBox.bottom - 1;
        }
        else
        {
          rLBComponentSize.bottom = lpdis->rcItem.bottom - 1;
        }
        rLBComponentSize.left  = lpdis->rcItem.right - 50;
        rLBComponentSize.right = lpdis->rcItem.right;
        if(lpdis->rcItem.top < rListBox.top)
        {
          rLBComponentSize.top = rListBox.top + 1;
        }
        else
        {
          rLBComponentSize.top = lpdis->rcItem.top + 1;
        }
        /* set text alignment */
        SetTextAlign(hdcComponentSize, TA_RIGHT);
        /* output string */
        ExtTextOut(hdcComponentSize,
                   lpdis->rcItem.right - 3,
                   y,
                   ETO_OPAQUE | ETO_CLIPPED,
                   &(rLBComponentSize),
                   tchBuffer,
                   strlen(tchBuffer),
                   NULL);
      }
      
      DrawCheck(lpdis, hdcMem);

      // draw the focus rect on the selected item
      if((lpdis->itemAction & ODA_FOCUS) &&
         (lpdis->itemState & ODS_FOCUS))
      {
        DrawFocusRect(lpdis->hDC, &(lpdis->rcItem));
      }

      DeleteDC(hdcMem);
      ReleaseDC(lpdis->hwndItem, hdcComponentSize);
      bReturn = TRUE;

      if(iLBCheckboxRedraw > 0)
      {
        --iLBCheckboxRedraw;
        InvalidateLBCheckbox(hwndLBComponents);
//        UpdateWindow(hwndLBComponents);
      }

      /* update the disk space required info in the dialog.  It is already
         in Kilobytes (unlike what GetDiskSpaceAvailable() returns) */
      ullDSBuf = GetDiskSpaceRequired(DSR_DESTINATION);
      _ui64toa(ullDSBuf, tchBuffer, 10);
      ParsePath(sgProduct.szPath, szBuf, sizeof(szBuf), PP_ROOT_ONLY);
      RemoveBackSlash(szBuf);
      lstrcat(szBuf, " - ");
      lstrcat(szBuf, tchBuffer);
      lstrcat(szBuf, " K");
      
      SetDlgItemText(hDlg, IDC_STATIC_DRIVE_SPACE_REQUIRED, szBuf);
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_LIST_COMPONENTS:
          /* to update the long description for each component the user selected */
          if((dwIndex = SendMessage(hwndLBComponents, LB_GETCURSEL, 0, 0)) != LB_ERR)
            SetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, SiCNodeGetDescriptionLong(dwIndex));
          break;

        case ID_WIZNEXT:
          dwItems       = ListView_GetItemCount(hwndLBComponents);
          dwArrayIndex  = 0;
          for(dwCurrentItem = 0; dwCurrentItem < dwItems; dwCurrentItem++)
          {
            if(ListView_GetCheckState(hwndLBComponents, dwCurrentItem))
            {
              dwItemsSelected[dwArrayIndex] = dwCurrentItem;
              ++dwArrayIndex;
            }
          }

          SiCNodeSetItemsSelected(dwItems, dwItemsSelected);

          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
          break;

        case ID_WIZBACK:
          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
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
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.right/2), (dwScreenY/2)-(rDlg.bottom/2), 0, 0, SWP_NOSIZE);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case ID_WIZNEXT:
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
          PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
          break;

        case ID_WIZBACK:
          dwWizardState = DLG_WINDOWS_INTEGRATION;
          CheckWizardStateCustom(DLG_SETUP_TYPE);

          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
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
      SetWindowText(hDlg, diProgramFolder.szTitle);
      SetDlgItemText(hDlg, IDC_MESSAGE0, diProgramFolder.szMessage0);
      SetDlgItemText(hDlg, IDC_EDIT_PROGRAM_FOLDER, sgProduct.szProgramFolderName);

      lstrcpy(szBuf, sgProduct.szProgramFolderPath);
      lstrcat(szBuf, "\\*.*");
      if((hDir = FindFirstFile(szBuf , &wfdFindFileData)) != INVALID_HANDLE_VALUE)
      {
        if((wfdFindFileData.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) && (lstrcmpi(wfdFindFileData.cFileName, ".") != 0) && (lstrcmpi(wfdFindFileData.cFileName, "..") != 0))
        {
          SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)wfdFindFileData.cFileName);
        }

        while(FindNextFile(hDir, &wfdFindFileData))
        {
          if((wfdFindFileData.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) && (lstrcmpi(wfdFindFileData.cFileName, ".") != 0) && (lstrcmpi(wfdFindFileData.cFileName, "..") != 0))
            SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)wfdFindFileData.cFileName);
        }
        FindClose(hDir);
      }

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.right/2), (dwScreenY/2)-(rDlg.bottom/2), 0, 0, SWP_NOSIZE);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case ID_WIZNEXT:
          GetDlgItemText(hDlg, IDC_EDIT_PROGRAM_FOLDER, sgProduct.szProgramFolderName, MAX_BUF);

          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
          break;

        case ID_WIZBACK:
          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
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

LRESULT CALLBACK DlgProcStartInstall(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  RECT rDlg;

  switch(msg)
  {
    case WM_INITDIALOG:
      SetWindowText(hDlg, diStartInstall.szTitle);
      SetDlgItemText(hDlg, IDC_MESSAGE0, diStartInstall.szMessage0);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.right/2), (dwScreenY/2)-(rDlg.bottom/2), 0, 0, SWP_NOSIZE);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case ID_WIZNEXT:
          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
          break;

        case ID_WIZBACK:
          DestroyWindow(hDlg);
          PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
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
      CheckDlgButton(hDlg, IDC_RADIO_YES, BST_CHECKED);
      SetFocus(hRadioYes);

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.right/2), (dwScreenY/2)-(rDlg.bottom/2), 0, 0, SWP_NOSIZE);

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
  RECT rDlg;
  HWND hSTMessage = GetDlgItem(hDlg, IDC_MESSAGE); /* handle to the Static Text message window */
  HDC  hdcSTMessage;
  SIZE sizeString;
  int  iLen;
//  int  iCount;
//  int  iCharWidth;
//  UINT uiTotalWidth;

  switch(msg)
  {
    case WM_INITDIALOG:
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_MESSAGE:
          hdcSTMessage = GetWindowDC(hSTMessage);
          iLen = lstrlen((LPSTR)lParam);
          GetTextExtentPoint32(hdcSTMessage, (LPSTR)lParam, iLen, &sizeString);

/*          uiTotalWidth = 0;
          for(iCount = 0; iCount < iLen; iCount ++)
          {
            GetCharWidth32(hdcSTMessage, ((LPSTR)lParam)[iCount], ((LPSTR)lParam)[iCount], &iCharWidth);
            uiTotalWidth += iCharWidth;
          }
*/

          ReleaseDC(hSTMessage, hdcSTMessage);

          SetWindowPos(hDlg, HWND_TOP,
                      (dwScreenX/2)-((sizeString.cx - (iLen * 1))/2), (dwScreenY/2)-((sizeString.cy + 50)/2),
                      (sizeString.cx - (iLen * 1)), sizeString.cy + 50,
                       SWP_SHOWWINDOW);

          if(GetClientRect(hDlg, &rDlg))
            SetWindowPos(hSTMessage, HWND_TOP,
                         rDlg.left, rDlg.top,
                         (sizeString.cx - (iLen * 1)), rDlg.bottom,
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
  if((bShow) && (hDlgMessage == NULL))
  {
    InstantiateDialog(DLG_MESSAGE, "Message", DlgProcMessage);
    SendMessage(hDlgMessage, WM_COMMAND, IDC_MESSAGE, (LPARAM)szMessage);
  }
  else if(!bShow && hDlgMessage)
  {
    DestroyWindow(hDlgMessage);
    hDlgMessage = NULL;
  }
}

void InstantiateDialog(DWORD dwDlgID, LPSTR szTitle, WNDPROC wpDlgProc)
{
  char szBuf[MAX_BUF];

  if((hDlg = CreateDialog(hSetupRscInst, MAKEINTRESOURCE(dwDlgID), hWndMain, wpDlgProc)) == NULL)
  {
    char szEDialogCreate[MAX_BUF];

    if(NS_LoadString(hSetupRscInst, IDS_ERROR_DIALOG_CREATE, szEDialogCreate, MAX_BUF) == WIZ_OK)
    {
      wsprintf(szBuf, szEDialogCreate, szTitle);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    PostQuitMessage(1);
  }
  else if(dwDlgID == DLG_MESSAGE)
  {
    hDlgMessage = hDlg;
  }
}

void CheckWizardStateCustom(DWORD dwDefault)
{
  if(sgProduct.dwCustomType != dwSetupType)
    dwWizardState = dwDefault;
}

void DlgSequenceNext()
{
  HRESULT hrValue;

  switch(dwWizardState)
  {
    case DLG_NONE:
      dwWizardState = DLG_WELCOME;
      if(diWelcome.bShowDialog)
        InstantiateDialog(dwWizardState, diWelcome.szTitle, DlgProcWelcome);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
      break;

    case DLG_WELCOME:
      dwWizardState = DLG_LICENSE;
      if(diLicense.bShowDialog)
        InstantiateDialog(dwWizardState, diLicense.szTitle, DlgProcLicense);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
      break;

    case DLG_LICENSE:
      dwWizardState = DLG_SETUP_TYPE;
      if(diSetupType.bShowDialog)
        InstantiateDialog(dwWizardState, diSetupType.szTitle, DlgProcSetupType);
      else
      {
        CheckWizardStateCustom(DLG_SELECT_COMPONENTS);
        PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
      }
      break;

    case DLG_SETUP_TYPE:
      /* depending on what the users chooses, the Select Components dialog  */
      /* might not be the next dialog in the sequence.                      */
      dwWizardState = DLG_SELECT_COMPONENTS;
      if(diSelectComponents.bShowDialog)
        InstantiateDialog(dwWizardState, diSelectComponents.szTitle, DlgProcSelectComponents);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
      break;

    case DLG_SELECT_COMPONENTS:
      dwWizardState = DLG_WINDOWS_INTEGRATION;
      if(diWindowsIntegration.bShowDialog)
        InstantiateDialog(dwWizardState, diWindowsIntegration.szTitle, DlgProcWindowsIntegration);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
      break;

    case DLG_WINDOWS_INTEGRATION:
      dwWizardState = DLG_PROGRAM_FOLDER;
      do
      {
        hrValue = VerifyDiskSpace();
        if(hrValue == FALSE)
        {
          /* show previous visible window */
          DlgSequencePrev();
          break;
        }
      }while(hrValue == IDRETRY);

      if(hrValue == FALSE)
      {
        /* break out of this case because we need to show the previous dialog */
        break;
      }

      if(diProgramFolder.bShowDialog)
        InstantiateDialog(dwWizardState, diProgramFolder.szTitle, DlgProcProgramFolder);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
      break;

    case DLG_PROGRAM_FOLDER:
      dwWizardState = DLG_START_INSTALL;
      if(diStartInstall.bShowDialog)
        InstantiateDialog(dwWizardState, diStartInstall.szTitle, DlgProcStartInstall);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZNEXT, 0);
      break;

    default:
      dwWizardState = DLG_START_INSTALL;

      /* PRE_DOWNLOAD process file manipulation functions */
      ProcessFileOps(T_PRE_DOWNLOAD);

      if(RetrieveArchives() == 0)
      {
        /* POST_DOWNLOAD process file manipulation functions */
        ProcessFileOps(T_POST_DOWNLOAD);
        /* PRE_CORE process file manipulation functions */
        ProcessFileOps(T_PRE_CORE);

        ProcessCoreFile();

        /* POST_CORE process file manipulation functions */
        ProcessFileOps(T_POST_CORE);
        /* PRE_SMARTUPDATE process file manipulation functions */
        ProcessFileOps(T_PRE_SMARTUPDATE);

        if(SmartUpdateJars() == 0)
        {
          /* POST_SMARTUPDATE process file manipulation functions */
          ProcessFileOps(T_POST_SMARTUPDATE);
          /* PRE_LAUNCHAPP process file manipulation functions */
          ProcessFileOps(T_PRE_LAUNCHAPP);

          LaunchApps();

          /* POST_LAUNCHAPP process file manipulation functions */
          ProcessFileOps(T_POST_LAUNCHAPP);
          ProcessProgramFolderShowCmd();

          if(diReboot.dwShowDialog)
          {
            CleanupCoreFile();
            InstantiateDialog(DLG_RESTART, diReboot.szTitle, DlgProcReboot);
          }
        }
      }
      else
        bSDUserCanceled = TRUE;

      CleanupCoreFile();
      PostQuitMessage(0);
      break;
  }
}

void DlgSequencePrev()
{
  switch(dwWizardState)
  {
    case DLG_START_INSTALL:
      dwWizardState = DLG_PROGRAM_FOLDER;
      if(diStartInstall.bShowDialog)
        InstantiateDialog(dwWizardState, diProgramFolder.szTitle, DlgProcProgramFolder);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
      break;

    case DLG_PROGRAM_FOLDER:
      dwWizardState = DLG_WINDOWS_INTEGRATION;
      if(diWindowsIntegration.bShowDialog)
        InstantiateDialog(dwWizardState, diWindowsIntegration.szTitle, DlgProcWindowsIntegration);
      else
      {
        CheckWizardStateCustom(DLG_SELECT_COMPONENTS);
        PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
      }
      break;

    case DLG_WINDOWS_INTEGRATION:
      /* depending on what the users chooses, the Select Components dialog  */
      /* might not be the next dialog in the sequence.                      */
      dwWizardState = DLG_SELECT_COMPONENTS;
      if(diSelectComponents.bShowDialog)
        InstantiateDialog(dwWizardState, diSelectComponents.szTitle, DlgProcSelectComponents);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
      break;

    case DLG_SELECT_COMPONENTS:
      dwWizardState = DLG_SETUP_TYPE;
      if(diSetupType.bShowDialog)
        InstantiateDialog(dwWizardState, diSetupType.szTitle, DlgProcSetupType);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
      break;

    case DLG_SETUP_TYPE:
      dwWizardState = DLG_LICENSE;
      if(diLicense.bShowDialog)
        InstantiateDialog(dwWizardState, diLicense.szTitle, DlgProcLicense);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
      break;

    case DLG_LICENSE:
      dwWizardState = DLG_WELCOME;
      if(diWelcome.bShowDialog)
        InstantiateDialog(dwWizardState, diWelcome.szTitle, DlgProcWelcome);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
      break;

    default:
      dwWizardState = DLG_WELCOME;
      if(diWelcome.bShowDialog)
        InstantiateDialog(DLG_WELCOME, diWelcome.szTitle, DlgProcWelcome);
      else
        PostMessage(hWndMain, WM_COMMAND, ID_WIZBACK, 0);
      break;
  }
}
