/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <windows.h>
#include <windowsx.h>
#include <direct.h>
#include <stdio.h>

#include "resource.h"

extern HINSTANCE hInst;

#define ITEM_BITMAPWIDTH     16
#define ITEM_BITMAPHEIGHT    16
#define ITEM_LEFTMARGIN       4
#define ITEM_GAP              4

static HWND hWndDirPicker;
static HICON hIconDrives[5];
static HICON hIconFolders[3];
static LPSTR lpszStringToReturn;
static char szUNCRoot[256] = "";

UINT DriveType(UINT iType);

static void fillComboBox(HWND hWnd) 
{
  HWND hWndCB = GetDlgItem(hWnd, ID_COMBO_DIR);
  HWND hWndTempLB = GetDlgItem(hWnd, ID_LISTTEMP_DIR);
  if(hWndCB == NULL)
    return;
  ComboBox_ResetContent(hWndCB);
  ListBox_ResetContent(hWndTempLB);
  ListBox_Dir(hWndTempLB, DDL_DRIVES|DDL_EXCLUSIVE, (LPSTR)"*");

  int iDriveCount = ListBox_GetCount(hWndTempLB);
  int iCurDrive=_getdrive() - 1;

  char szDrive[16];
  char szItem[80];

  for (int i = 0; i < iDriveCount;  i++) 
  {
    ListBox_GetText(hWndTempLB, i, szDrive);
    CharLower(szDrive);
    int iDrive = szDrive[2] - 'a';
    char szRoot[16];
    sprintf(szRoot, "%c:\\", szDrive[2]);

    int iType = DriveType(iDrive);

    if(iType < 2)
      continue;

    //Start the item string with the drive letter, colon, and two spaces
    sprintf(szItem, "%c%s", szDrive[2], ": ");

    if((iType == DRIVE_FIXED) || (iType == DRIVE_RAMDISK)) 
    { // get volume ID
      char szVolumeID[80];
      DWORD dwMaxLength;
      DWORD dwSysFlags;
      GetVolumeInformation(szRoot,             // address of root directory of the file system 
                           szVolumeID,         // address of name of the volume 
                           sizeof(szVolumeID), // length of lpVolumeNameBuffer 
                           NULL,               // address of volume serial number 
                           &dwMaxLength,       // address of system's maximum filename length
                           &dwSysFlags,        // address of file system flags 
                           NULL,               // address of name of file system 
                           NULL);              // length of lpFileSystemNameBuffer 

      CharLower(szVolumeID);
      lstrcat(szItem, szVolumeID);
    }

    //For network drives, go grab the \\server\share for it.
    if(DRIVE_REMOTE == iType) 
    {
      char szNet[64];
      szNet[0] = '\0';
      DWORD dwSizeOfszNet = sizeof(szNet);

      sprintf(szDrive, "%c:", szDrive[2]);
      CharUpper(szDrive);
      WNetGetConnection(szDrive, szNet, &dwSizeOfszNet);
      CharLower(szNet);
      lstrcat(szItem, szNet);
    }

    int index = ComboBox_AddString(hWndCB, szItem);
    ComboBox_SetItemData(hWndCB, index, MAKELONG(iDrive, iType));
    if(iDrive == iCurDrive)
      ComboBox_SetCurSel(hWndCB, index);
    if(szUNCRoot[0] != '\0')
      ComboBox_SetCurSel(hWndCB, -1);
  }
}

static void fillTempLBWithDirs(HWND hWndTempLB, LPSTR lpszDir) 
{
  BOOL bDone = FALSE;
  WIN32_FIND_DATA ffdataStruct;

  char szPath[_MAX_PATH];
  char szFileName[_MAX_PATH];
  lstrcpy(szPath, lpszDir);
  if(szPath[lstrlen(szPath) - 1] == '\\')
    szPath[lstrlen(szPath) - 1] = '\0';
  lstrcpy(szFileName, szPath);
  lstrcat(szFileName, "\\*");
  HANDLE handle = FindFirstFile(szFileName, &ffdataStruct);
  if(handle == INVALID_HANDLE_VALUE) 
  {
    FindClose(handle);
    return;
  }
  while(!bDone) 
  {
    lstrcpy(szFileName, szPath);
    lstrcat(szFileName, "\\");
    lstrcat(szFileName, ffdataStruct.cFileName);
    if(ffdataStruct. dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
    {
      char szStringToAdd[_MAX_PATH + 2];
      lstrcpy(szStringToAdd, "[");
      lstrcat(szStringToAdd, ffdataStruct.cFileName);
      lstrcat(szStringToAdd, "]");
      CharLower(szStringToAdd);
      ListBox_AddString(hWndTempLB, szStringToAdd);
    }
    bDone = !FindNextFile(handle, &ffdataStruct);
  }
  FindClose(handle);
}

static void fillListBox(HWND hWnd, LPSTR lpszDir) 
{
  HWND hWndLB = GetDlgItem(hWnd, ID_LIST_DIR);
  HWND hWndTempLB = GetDlgItem(hWnd, ID_LISTTEMP_DIR);
  HWND hWndEdit = GetDlgItem(hWnd, ID_EDIT_DIR);
  if((hWndLB == NULL) || (lpszDir == NULL))
    return;
  
  int iLastChar = lstrlen(lpszDir);
  if(lpszDir[iLastChar - 1] == '\\')
    lpszDir[iLastChar - 1] = '\0';

  SetWindowRedraw(hWndLB, FALSE);
  ListBox_ResetContent(hWndLB);
  ListBox_ResetContent(hWndTempLB);

  LPSTR lpszLast;
  lpszLast = CharLower(lpszDir);

  SetWindowText(hWndLB, lpszDir);

  char szDir[_MAX_DIR];
  char szFullDir[_MAX_DIR];
  sprintf(szFullDir, "%s", lpszDir);
  sprintf(szDir, "%s\\*.*", lpszDir);

  BOOL bFirst = TRUE;
  char ch;
  int index;
  while (TRUE) 
  {
    LPSTR lpsz;
    if((lpszDir[0] == '\\') && (lpszDir[1] == '\\') && bFirst)
      lpsz = strchr(lpszLast + lstrlen(szUNCRoot), '\\');
    else
      lpsz = strchr(lpszLast, '\\');
    if(lpsz != NULL) {
      if (bFirst)
        ch = *(++lpsz);
      else
        ch = *lpsz;
      *lpsz = 0;
    } 
    else 
    {
      //If we're looking at a drive only, then append a backslash
      if (lpszLast == lpszDir && bFirst)
        lstrcat(lpszLast, "\\");
    }
    //Add the drive string--includes the last one where lpsz == NULL
    index = ListBox_AddString(hWndLB, lpszLast);

    UINT i = (NULL != lpsz) ? ID_ICON_FOLDEROPEN : ID_ICON_OPENSELECT;
    ListBox_SetItemData(hWndLB, index, MAKELONG(index, i));

    if(NULL == lpsz)
      break;

      //Restore last character.
    *lpsz = ch;
    lpsz += (bFirst) ? 0 : 1;

    bFirst=FALSE;
    lpszLast = lpsz;
  }
  int indent = index + 1;

  //Get available directories
  fillTempLBWithDirs(hWndTempLB, lpszDir);

  int itemCount = ListBox_GetCount(hWndTempLB);

  for (int i = 0; i < itemCount; i++) {
    index = ListBox_GetText(hWndTempLB, i, lpszDir);
    //Skip directories beginning with . (skipping . and ..)
    if(lpszDir[1] == '.')
      continue;
    //Remove the ending ']'
    iLastChar = lstrlen(lpszDir);
    lpszDir[iLastChar - 1] = '\0';
    //Add the string to the real directory list.
    index = ListBox_AddString(hWndLB, lpszDir + 1);
    ListBox_SetItemData(hWndLB, index, MAKELONG(indent, ID_ICON_FOLDERCLOSED));
  }
  //Force a listbox repaint.
  SetWindowRedraw(hWndLB, TRUE);
  InvalidateRect(hWndLB, NULL, TRUE);

  if(szFullDir[lstrlen(szFullDir) - 1] == ':')
    lstrcat(szFullDir, "\\");
  Edit_SetText(hWndEdit, szFullDir);

  GetScrollRange(hWndLB, SB_VERT, (LPINT)&i, (LPINT)&index);

  if(!(i == 0 && index == 0))
    ListBox_SetTopIndex(hWndLB, max((int)(index - 2), 0));

  ListBox_SetCurSel(hWndLB, indent - 1);
}

static void onDrawItem(LPDRAWITEMSTRUCT lpdis, BOOL bDrive) 
{
  if((int)lpdis->itemID < 0)
    return;

  char szItem[_MAX_DIR];
  DWORD dwItemData;

  if(bDrive) 
  {
    dwItemData = ComboBox_GetItemData(lpdis->hwndItem, lpdis->itemID);
    ComboBox_GetLBText(lpdis->hwndItem, lpdis->itemID, szItem);
  } 
  else 
  {
    dwItemData = ListBox_GetItemData(lpdis->hwndItem, lpdis->itemID);
    ListBox_GetText(lpdis->hwndItem, lpdis->itemID, szItem);
  }

  if(lpdis->itemAction & (ODA_DRAWENTIRE | ODA_SELECT)) 
  {
    COLORREF colorText;
    COLORREF colorBack;
    if(lpdis->itemState & ODS_SELECTED) 
    {
      colorText = SetTextColor(lpdis->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
      colorBack = SetBkColor(lpdis->hDC, GetSysColor(COLOR_HIGHLIGHT));
    }
    HICON hIcon;
    int indent = 0;
    if(bDrive) 
    {
      int iType=(int)HIWORD(dwItemData);
      switch (iType) 
      {
        case DRIVE_REMOVABLE:
          hIcon = hIconDrives[0];
          break; 
        case DRIVE_FIXED:
          hIcon = hIconDrives[1];
          break;
        case DRIVE_REMOTE:
          hIcon = hIconDrives[2];
          break; 
        case DRIVE_CDROM:
          hIcon = hIconDrives[3];
          break; 
        case DRIVE_RAMDISK:
          hIcon = hIconDrives[4];
          break; 
      }

    } 
    else 
    {
      int iconID = (int)HIWORD(lpdis->itemData);
      switch (iconID) 
      {
        case ID_ICON_FOLDERCLOSED:
          hIcon = hIconFolders[0];
          break;
        case ID_ICON_FOLDEROPEN:
          hIcon = hIconFolders[1];
          break;
        case ID_ICON_OPENSELECT:
          hIcon = hIconFolders[2];
          break;
      }
      indent = 4 * (1 + LOWORD(lpdis->itemData));
    }

    ExtTextOut(lpdis->hDC, 
               lpdis->rcItem.left + ITEM_LEFTMARGIN + ITEM_BITMAPWIDTH + ITEM_GAP + indent,
               lpdis->rcItem.top,
               ETO_OPAQUE | ETO_CLIPPED,
               &lpdis->rcItem,
               szItem,
               lstrlen(szItem),
               NULL);

    BOOL res = DrawIcon(lpdis->hDC, 
                        lpdis->rcItem.left + ITEM_LEFTMARGIN + indent,
                        lpdis->rcItem.top, 
                        hIcon);

    if(lpdis->itemState & ODS_SELECTED) 
    {
      SetTextColor(lpdis->hDC, colorText);
      SetBkColor(lpdis->hDC, colorBack);
    }
  }
  if((lpdis->itemAction & ODA_FOCUS) || (lpdis->itemState & ODS_FOCUS))
    DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
}

static void fillUNCRootArray(LPSTR lpsz) 
{
  char szCurDir[_MAX_PATH];
  _getcwd(szCurDir, sizeof(szCurDir));
  lstrcpy(szUNCRoot, lpsz);
  if(szUNCRoot[lstrlen(szUNCRoot) - 1] == '\\')
    szUNCRoot[lstrlen(szUNCRoot) - 1] = '\0';
  for(;;) 
  {
    LPSTR lptemp = strrchr(szUNCRoot, '\\');
    if(lptemp == NULL)
      break;
    *lptemp = '\0';
    if(_chdir(szUNCRoot) == -1) 
    {
      *lptemp = '\\';
      break;
    }
  }
  _chdir(szCurDir);
}

static void onInitDialog(HWND hWnd, LPSTR lpsz) 
{
  hWndDirPicker = hWnd;
  lpszStringToReturn = lpsz;
  
  hIconDrives[0] = LoadIcon(hInst, MAKEINTRESOURCE(ID_ICON_DRIVEFLOPPY));
  hIconDrives[1] = LoadIcon(hInst, MAKEINTRESOURCE(ID_ICON_DRIVEHARD));
  hIconDrives[2] = LoadIcon(hInst, MAKEINTRESOURCE(ID_ICON_DRIVENETWORK));
  hIconDrives[3] = LoadIcon(hInst, MAKEINTRESOURCE(ID_ICON_DRIVECDROM));
  hIconDrives[4] = LoadIcon(hInst, MAKEINTRESOURCE(ID_ICON_DRIVERAM));

  hIconFolders[0] = LoadIcon(hInst, MAKEINTRESOURCE(ID_ICON_FOLDERCLOSED));
  hIconFolders[1] = LoadIcon(hInst, MAKEINTRESOURCE(ID_ICON_FOLDEROPEN));
  hIconFolders[2] = LoadIcon(hInst, MAKEINTRESOURCE(ID_ICON_OPENSELECT));
  
  if(lpsz[0] == '\0') 
    _getcwd(lpsz, _MAX_PATH);
  else if(lpsz[lstrlen(lpsz) - 1] == ':')
    lstrcat(lpsz, "\\");

  int ret = _chdir(lpsz);
  if(ret == -1) 
  {
    char szText[_MAX_PATH + 80];
    sprintf(szText, "The specified directory %s\ncannot be found", lpsz);
    MessageBox(GetParent(hWnd), szText, "Choose Directory", MB_ICONEXCLAMATION|MB_OK);
    _getcwd(lpsz, _MAX_PATH);
  }
  if((lpsz[0] == '\\') && (lpsz[1] == '\\'))
    fillUNCRootArray(lpsz);
  fillListBox(hWnd, lpsz);
  fillComboBox(hWnd);
}

static void shutDialog(HWND hWnd) 
{
  szUNCRoot[0] = '\0';
}

static void onCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify) 
{
  char szCurDir[_MAX_PATH];
  switch(id) 
  {
    case ID_LIST_DIR:
      if(codeNotify == LBN_DBLCLK) 
      {
        int index = ListBox_GetCurSel(hWndCtl);
        DWORD dwItemData = ListBox_GetItemData(hWndCtl, index);

        if(HIWORD(dwItemData) == ID_ICON_OPENSELECT) 
        {
          shutDialog(hWnd);
          char szString[_MAX_PATH];
          Edit_GetText(GetDlgItem(hWndDirPicker, ID_EDIT_DIR), szString, sizeof(szString));
          lstrcpy(lpszStringToReturn, szString);
          EndDialog(hWnd, IDOK);
          break;
        }

        ListBox_GetText(hWndCtl, index, szCurDir);

        char szDir[_MAX_DIR];
        LPSTR lpsz;
        if((HIWORD(dwItemData) == ID_ICON_FOLDEROPEN) && (index != 0)) 
        {
          GetWindowText(hWndCtl, szDir, sizeof(szDir));
          lpsz=_fstrstr(szDir, szCurDir);
          *(lpsz + lstrlen(szCurDir)) = '\0';
          lstrcpy(szCurDir, szDir);
        }
        if (_chdir(szCurDir) == 0) 
        {
          _getcwd(szCurDir, _MAX_PATH);
          fillListBox(hWndDirPicker, szCurDir);
        }
      }
      break;
    case ID_COMBO_DIR:
      if(codeNotify == CBN_SELCHANGE) 
      {
        char szDrive[80];
        int index = ComboBox_GetCurSel(hWndCtl);
        if(index == CB_ERR)
          break;
        ComboBox_GetLBText(hWndCtl, index, szDrive);

        int iCurDrive = _getdrive();
Retry:
        HCURSOR hCursorOld = SetCursor(LoadCursor(NULL, IDC_WAIT));
        SetCapture(hWndDirPicker);
        if((0 == _chdrive((int)(szDrive[0] - 'a' + 1))) && (NULL != _getcwd(szCurDir, _MAX_PATH))) 
        {
          fillListBox(hWndDirPicker, szCurDir);
          ListBox_SetTopIndex(GetDlgItem(hWndDirPicker, ID_LIST_DIR), 0);
          SetCursor(hCursorOld);
          ReleaseCapture();
          break;
        }
        SetCursor(hCursorOld);
        ReleaseCapture();

        char szText[80];        
        sprintf(szText, "Cannot read drive %c:", szDrive[0]);
        if(IDRETRY == MessageBox(hWndDirPicker, szText, "Choose Directory", MB_ICONEXCLAMATION|MB_RETRYCANCEL))
          goto Retry;
        
        //Changing drives failed so restore drive and selection
        _chdrive(iCurDrive);

        sprintf(szDrive, "%c:", (char)(iCurDrive + 'a' - 1));
        index = ComboBox_SelectString(hWndCtl, -1, szDrive);
      }
      break;
    case IDOK:
      shutDialog(hWnd);
      char szString[_MAX_PATH];
      Edit_GetText(GetDlgItem(hWndDirPicker, ID_EDIT_DIR), szString, sizeof(szString));
      lstrcpy(lpszStringToReturn, szString);
      EndDialog(hWnd, IDOK);
      break;
    case IDCANCEL:
      shutDialog(hWnd);
      lpszStringToReturn[0] = '\0';
      EndDialog(hWnd, IDCANCEL);
      break;
    default:
      break;
  }
}

static BOOL CALLBACK DirPickDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) 
{
  switch(msg) {
    case WM_INITDIALOG:
      onInitDialog(hWnd, (LPSTR)lParam);
      break;
    case WM_COMMAND:
      HANDLE_WM_COMMAND(hWnd, wParam, lParam, onCommand);
      break;
    case WM_MEASUREITEM: 
    {
      static int cyItem = -1;      //Height of a listbox item
      LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
      if(cyItem == -1) 
      {
        HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0L);
        if(hFont == NULL)
          hFont = GetStockFont(SYSTEM_FONT);
        HDC hDC = GetDC(hWnd);
        HFONT hFontOld = SelectFont(hDC, hFont);
        TEXTMETRIC tm;
        GetTextMetrics(hDC, &tm);
        cyItem = max(ITEM_BITMAPHEIGHT, tm.tmHeight);
        SelectFont(hDC, hFontOld);
        ReleaseDC(hWnd, hDC);
      }

      lpmis->itemHeight = cyItem;
    }
      break;
    case WM_DRAWITEM:
      onDrawItem((LPDRAWITEMSTRUCT)lParam, ((UINT)wParam == ID_COMBO_DIR));
      return TRUE; // to prevent default action in listbox (drawing focus)
    default:
      return FALSE;
  }
  return TRUE;
}

/*
 * DriveType
 *
 * Purpose:
 *  Augments the Windows API GetDriveType with a call to the CD-ROM
 *  extensions to determine if a drive is a floppy, hard disk, CD-ROM,
 *  RAM-drive, or networked  drive.
 *
 * Parameters:
 *  iDrive          UINT containing the zero-based drive index
 *
 * Return Value:
 *  UINT            One of the following values describing the drive:
 *                  DRIVE_FLOPPY, DRIVE_HARD, DRIVE_CDROM, DRIVE_RAMDISK,
 *                  DRIVE_NETWORK.
 *
 * Copyright (c)1992 Kraig Brockschmidt, All Right Reserved
 * Compuserve:  70750,2344
 * Internet  :  kraigb@microsoft.com
 *
 */
UINT DriveType(UINT iDrive) 
{
  //Validate possible drive indices
  if((0 > iDrive)  || (25 < iDrive))
    return (UINT)-1;

  static char path[] = "d:\\";
  path[0] = 'a' + iDrive;
  int iType = GetDriveType(path);  

  /*
   * Under Windows NT, GetDriveType returns complete information
   * not provided under Windows 3.x which we now get through other
   * means.
   */

  return iType;
}

BOOL PickupDirectory(HWND hWndOwner, LPSTR lpszString) 
{
  if(hWndOwner == NULL)
    hWndOwner = GetDesktopWindow();
  int ret = DialogBoxParam(hInst, MAKEINTRESOURCE(ID_DIALOG_CHOOSEDIR), hWndOwner, DirPickDlgProc, (LPARAM)lpszString);
  return (ret == IDOK);
}
