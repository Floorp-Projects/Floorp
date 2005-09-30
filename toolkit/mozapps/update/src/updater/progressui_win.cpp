/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <windows.h>
#include <commctrl.h>
#include <process.h>
#include <io.h>
#include "resource.h"

#define TIMER_ID 1
#define TIMER_INTERVAL 100

static float sProgress;  // between 0 and 100
static BOOL  sQuit = FALSE;

static BOOL
GetStringsFile(char filename[MAX_PATH])
{
  if (!GetModuleFileName(NULL, filename, MAX_PATH))
    return FALSE;

  char *dot = strrchr(filename, '.');
  if (!dot || stricmp(dot + 1, "exe"))
    return FALSE;

  strcpy(dot + 1, "ini");
  return TRUE;
}

static void
UpdateDialog(HWND hDlg)
{
  int pos = int(sProgress + 0.5f);
  SendMessage(GetDlgItem(hDlg, IDC_PROGRESS), PBM_SETPOS, pos, 0L);
}

// The code in this function is from MSDN:
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winui/winui/windowsuserinterface/windowing/dialogboxes/usingdialogboxes.asp
static void
CenterDialog(HWND hDlg)
{
  RECT rc, rcOwner, rcDlg;

  // Get the owner window and dialog box rectangles. 
  HWND desktop = GetDesktopWindow();

  GetWindowRect(desktop, &rcOwner); 
  GetWindowRect(hDlg, &rcDlg); 
  CopyRect(&rc, &rcOwner); 

  // Offset the owner and dialog box rectangles so that 
  // right and bottom values represent the width and 
  // height, and then offset the owner again to discard 
  // space taken up by the dialog box. 

  OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top); 
  OffsetRect(&rc, -rc.left, -rc.top); 
  OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom); 

  // The new position is the sum of half the remaining 
  // space and the owner's original position. 

  SetWindowPos(hDlg, 
               HWND_TOP, 
               rcOwner.left + (rc.right / 2), 
               rcOwner.top + (rc.bottom / 2), 
               0, 0,          // ignores size arguments 
               SWP_NOSIZE); 
}

static void
SetItemText(HWND hwnd, const char *key, const char *ini)
{
  char text[512];
  if (!GetPrivateProfileString("Strings", key, NULL, text, sizeof(text), ini))
    return;
  SetWindowText(hwnd, text);
}

static void
InitDialog(HWND hDlg)
{
  char filename[MAX_PATH];
  if (!GetStringsFile(filename))
    return;

  SetItemText(hDlg, "Title", filename);
  SetItemText(GetDlgItem(hDlg, IDC_INFO), "Info", filename);

  SendMessage(GetDlgItem(hDlg, IDC_PROGRESS), PBM_SETRANGE, 0,
              MAKELPARAM(0, 100));

  CenterDialog(hDlg);  // make dialog appear in the center of the screen

  SetTimer(hDlg, TIMER_ID, TIMER_INTERVAL, NULL);
}

// Message handler for about box.
static LRESULT CALLBACK
DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
  case WM_INITDIALOG:
    InitDialog(hDlg);
    return TRUE;

  case WM_TIMER:
    if (sQuit)
      EndDialog(hDlg, 0);
    else
      UpdateDialog(hDlg);
    return TRUE;

  case WM_COMMAND:
    return TRUE;
  }
  return FALSE;
}

int
InitProgressUI(int *argc, char ***argv)
{
  return 0;
}

int
ShowProgressUI()
{
  // Only show the Progress UI if the process is taking significant time.
  // Here we measure significant time as taking more than one second.

  Sleep(500);

  if (sQuit || sProgress > 50.0f)
    return 0;

  // If we do not have updater.ini, then we should not bother showing UI.
  char filename[MAX_PATH];
  if (!GetStringsFile(filename))
    return -1;
  if (_access(filename, 04))
    return -1;

  INITCOMMONCONTROLSEX icc = {
    sizeof(INITCOMMONCONTROLSEX),
    ICC_PROGRESS_CLASS
  };
  InitCommonControlsEx(&icc);

  DialogBox(GetModuleHandle(NULL),
            MAKEINTRESOURCE(IDD_DIALOG), NULL,
            (DLGPROC) DialogProc);

  return 0;
}

void
QuitProgressUI()
{
  sQuit = TRUE;
}

void
UpdateProgressUI(float progress)
{
  sProgress = progress;  // 32-bit writes are atomic
}
