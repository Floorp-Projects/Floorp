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

#include "xp.h"
#include "windowsx.h"

#include "resource.h"
#include "loggerw.h"
#include "profilew.h"
#include "actionnames.h"

extern HINSTANCE hInst;
static char szClassName[] = "NPSpyWindowClass";

BOOL CALLBACK MainDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK PauseDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LoggerWin::LoggerWin() : Logger(),
  hWnd(NULL),
  width(0),
  height(0),
  x(0),
  y(0),
  bSaveSettings(FALSE)
{
}

LoggerWin::~LoggerWin()
{
}

BOOL LoggerWin::platformInit()
{
  WNDCLASS wc;
  wc.style         = 0; 
  wc.lpfnWndProc   = DefDlgProc; 
  wc.cbClsExtra    = 0; 
  wc.cbWndExtra    = DLGWINDOWEXTRA; 
  wc.hInstance     = hInst; 
  wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON_APP)); 
  wc.hCursor       = LoadCursor(0, IDC_ARROW); 
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszMenuName  = NULL; 
  wc.lpszClassName = szClassName;

  if(!RegisterClass(&wc))
    return FALSE;

  // restore prefs
  ProfileWin profile;

  profile.getBool(NPSPY_REG_KEY_ONTOP, &bOnTop);
  profile.getBool(NPSPY_REG_KEY_LOGTOWINDOW, &bToWindow);
  profile.getBool(NPSPY_REG_KEY_LOGTOCONSOLE, &bToConsole);
  profile.getBool(NPSPY_REG_KEY_LOGTOFILE, &bToFile);
  profile.getBool(NPSPY_REG_KEY_SPALID, &bSPALID);
  profile.getString(NPSPY_REG_KEY_LOGFILENAME, szFile, strlen(szFile));

  for(int i = 1; i < TOTAL_NUMBER_OF_API_CALLS; i++)
  {
    BOOL selected = TRUE;
    if(profile.getBool(ActionName[i], &selected))
      bMutedCalls[i] = !selected;
  }
  
  if(!profile.getSizeAndPosition(&width, &height, &x, &y))
  {
    width = 0;
    height = 0;
    x = 0;
    y = 0;
  }

  hWnd = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG_MAIN), GetDesktopWindow(), (DLGPROC)MainDlgProc, (LPARAM)this);
  if(hWnd == NULL)
  {
    UnregisterClass(szClassName, hInst);
    return FALSE;
  }

  if(bOnTop)
    SetWindowPos(hWnd, bOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);

  return TRUE;
}

void LoggerWin::platformShut()
{
  if(hWnd != NULL)
  {
    char szLog[] = "--- GOING AWAY... PRESS SPACE BAR TO CONTINUE ---";
    HWND hWndOutput = GetDlgItem(hWnd, IDC_MAIN_OUTPUT);
    ListBox_AddString(hWndOutput, "");
    ListBox_AddString(hWndOutput, szLog);
    int count = ListBox_GetCount(hWndOutput);
    ListBox_SetCaretIndex(hWndOutput, count - 1);
    UpdateWindow(hWndOutput);

    DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_PAUSE), hWnd, (DLGPROC)PauseDlgProc);

    ProfileWin profile;

    RECT rc;
    if(GetWindowRect(hWnd, &rc))
      profile.setSizeAndPosition(rc.right - rc.left, rc.bottom - rc.top, rc.left, rc.top);

    DestroyWindow(hWnd);
    hWnd = NULL;
  }

  UnregisterClass(szClassName, hInst);
}

void LoggerWin::onDestroyWindow()
{
  hWnd = NULL;
}

void LoggerWin::dumpStringToMainWindow(char * string)
{
  // listboxes don't want <CR> and <LF> so cut them off if any. The order is important.
  char * p = strrchr(string, '\n');
  if(p)
    *p = '\0';

  p = strrchr(string, '\r');
  if(p)
    *p = '\0';

  HWND hWndOutput = GetDlgItem(hWnd, IDC_MAIN_OUTPUT);
  ListBox_AddString(hWndOutput, string);
  int count = ListBox_GetCount(hWndOutput);
  if(count == 32767)
    ListBox_ResetContent(hWndOutput);
  ListBox_SetCaretIndex(hWndOutput, count - 1);
  UpdateWindow(hWndOutput);
}

void LoggerWin::onClear()
{
  HWND hWndOutput = GetDlgItem(hWnd, IDC_MAIN_OUTPUT);
  ListBox_ResetContent(hWndOutput);
  UpdateWindow(hWndOutput);
}

Logger * NewLogger()
{
  LoggerWin * res = new LoggerWin();
  return res;
}

void DeleteLogger(Logger * logger)
{
  if(logger)
    delete logger;
}