/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#include "xp.h"
#include <windowsx.h>

#include "resource.h"
#include "loggerw.h"
#include "profilew.h"

extern HINSTANCE hInst;
extern char * ActionName[];
extern char szAppName[];

BOOL CALLBACK GeneralPageProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK LogPageProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK FilterPageProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK AdvancedPageProc(HWND, UINT, WPARAM, LPARAM);

static void onOptions(HWND hWnd, Logger * logger)
{
  if(!logger)
    return;

  PROPSHEETPAGE psp[4];

  psp[0].dwSize = sizeof(psp[0]);
  psp[0].dwFlags = PSP_DEFAULT;
  psp[0].hInstance = hInst;
  psp[0].pszTemplate = MAKEINTRESOURCE(IDD_PAGE_GENERAL);
  psp[0].pszIcon = 0;
  psp[0].pfnDlgProc = GeneralPageProc;
  psp[0].pszTitle = 0;
  psp[0].lParam = (LPARAM)logger;
  psp[0].pfnCallback = NULL;

  psp[1].dwSize = sizeof(psp[1]);
  psp[1].dwFlags = PSP_DEFAULT;
  psp[1].hInstance = hInst;
  psp[1].pszTemplate = MAKEINTRESOURCE(IDD_PAGE_LOG);
  psp[1].pszIcon = 0;
  psp[1].pfnDlgProc = LogPageProc;
  psp[1].pszTitle = 0;
  psp[1].lParam = (LPARAM)logger;
  psp[1].pfnCallback = NULL;

  psp[2].dwSize = sizeof(psp[2]);
  psp[2].dwFlags = PSP_DEFAULT;
  psp[2].hInstance = hInst;
  psp[2].pszTemplate = MAKEINTRESOURCE(IDD_PAGE_FILTER);
  psp[2].pszIcon = 0;
  psp[2].pfnDlgProc = FilterPageProc;
  psp[2].pszTitle = 0;
  psp[2].lParam = (LPARAM)logger;
  psp[2].pfnCallback = NULL;

  psp[3].dwSize = sizeof(psp[3]);
  psp[3].dwFlags = PSP_DEFAULT;
  psp[3].hInstance = hInst;
  psp[3].pszTemplate = MAKEINTRESOURCE(IDD_PAGE_ADVANCED);
  psp[3].pszIcon = 0;
  psp[3].pfnDlgProc = AdvancedPageProc;
  psp[3].pszTitle = 0;
  psp[3].lParam = (LPARAM)logger;
  psp[3].pfnCallback = NULL;

  PROPSHEETHEADER psh;
  psh.dwSize = sizeof(PROPSHEETHEADER);
  psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW;
  psh.hwndParent = hWnd;
  psh.hInstance = hInst;
  psh.pszIcon = 0;
  psh.pszCaption = "Settings";
  psh.nPages = sizeof(psp) / sizeof(psp[0]);
  psh.nStartPage = 0;
  psh.ppsp = psp;
  psh.pfnCallback = NULL;

  logger->bSaveSettings = FALSE;

  int rv = PropertySheet(&psh);
  if(rv == -1)
    return;

  if(logger->bSaveSettings)
  {
    ProfileWin profile;

    if(hWnd != NULL)
    {
      RECT rc;
      if(GetWindowRect(hWnd, &rc))
        profile.setSizeAndPosition(rc.right - rc.left, rc.bottom - rc.top, rc.left, rc.top);
    }

    profile.setBool(NPSPY_REG_KEY_ONTOP, logger->bOnTop);
    profile.setBool(NPSPY_REG_KEY_LOGTOWINDOW, logger->bToWindow);
    profile.setBool(NPSPY_REG_KEY_LOGTOCONSOLE, logger->bToConsole);
    profile.setBool(NPSPY_REG_KEY_LOGTOFILE, logger->bToFile);
    profile.setBool(NPSPY_REG_KEY_SPALID, logger->bSPALID);
    profile.setString(NPSPY_REG_KEY_LOGFILENAME, logger->szFile);

    for(int i = 1; i < TOTAL_NUMBER_OF_API_CALLS; i++)
      profile.setBool(ActionName[i], !logger->bMutedCalls[i]);

    SetWindowPos(hWnd, logger->bOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
  }

  logger->bSaveSettings = FALSE;
}

static void onCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
  LoggerWin * logger = (LoggerWin *)GetWindowLong(hWnd, DWL_USER);
  switch (id)
  {
    case IDC_CHECK_MUTE:
      if(logger)
        logger->bMutedAll = (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CHECK_MUTE));
      break;
    case IDC_BUTTON_OPTIONS:
      onOptions(hWnd, logger);
      break;
    case IDC_BUTTON_CLEAR:
      if(logger)
        logger->onClear();
      break;
    default:
      break;
  }
}

static BOOL onInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam)
{
  LoggerWin * logger = (LoggerWin *)lParam;
  SetWindowLong(hWnd, DWL_USER, (long)logger);
  SetWindowText(hWnd, szAppName);
  HFONT hFont = GetStockFont(ANSI_FIXED_FONT);
  SetWindowFont(GetDlgItem(hWnd, IDC_MAIN_OUTPUT), hFont, FALSE);

  if(logger)
  {
    CheckDlgButton(hWnd, IDC_CHECK_MUTE, logger->bMutedAll ? BST_CHECKED : BST_UNCHECKED);
    if(logger->width && logger->height)
      SetWindowPos(hWnd, NULL, logger->x, logger->y, logger->width, logger->height, SWP_NOZORDER);

    SetWindowPos(hWnd, logger->bOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
  }
  return TRUE;
}

static void onDestroy(HWND hWnd)
{
  LoggerWin * logger = (LoggerWin *)GetWindowLong(hWnd, DWL_USER);
  if(logger)
    logger->onDestroyWindow();
}

static void onSize(HWND hWnd, UINT state, int cx, int cy)
{
  long bu = GetDialogBaseUnits();
  int bux = LOWORD(bu);
  int buy = HIWORD(bu);
  int factorx = bux/4;
  int factory = buy/8;

  int marginLeft   = 7 * factorx;
  int marginRight  = 0 * factorx;
  int marginTop    = 0 * factory;
  int marginBottom = 7 * factory;
  int spaceHor     = 4 * factorx;
  int spaceVer     = 7 * factory;

  HWND hWndOutput = GetDlgItem(hWnd, IDC_MAIN_OUTPUT);
  HWND hWndCheckMute = GetDlgItem(hWnd, IDC_CHECK_MUTE);
  HWND hWndCheckOntop = GetDlgItem(hWnd, IDC_CHECK_ONTOP);
  HWND hWndButtonOptions = GetDlgItem(hWnd, IDC_BUTTON_OPTIONS);
  HWND hWndClear = GetDlgItem(hWnd, IDC_BUTTON_CLEAR);

  RECT rcMain;
  GetClientRect(hWnd, &rcMain);

  int width = rcMain.right - rcMain.left;
  int height = rcMain.bottom - rcMain.top;

  RECT rcButtonOptions;
  GetWindowRect(hWndButtonOptions, &rcButtonOptions);
  SetWindowPos(hWndButtonOptions, NULL, 
               width - marginLeft - rcButtonOptions.right + rcButtonOptions.left, 
               height - rcButtonOptions.bottom + rcButtonOptions.top - marginBottom, 
               0, 0, SWP_NOZORDER | SWP_NOSIZE);

  RECT rcClear;
  GetWindowRect(hWndClear, &rcClear);
  SetWindowPos(hWndClear, NULL, 
               width - marginLeft - rcClear.right + rcClear.left - rcButtonOptions.right + rcButtonOptions.left - spaceHor, 
               height - rcClear.bottom + rcClear.top - marginBottom, 
               0, 0, SWP_NOZORDER | SWP_NOSIZE);

  RECT rcCheckMute;
  GetWindowRect(hWndCheckMute, &rcCheckMute);
  SetWindowPos(hWndCheckMute, NULL, 
               marginLeft, 
               height - rcCheckMute.bottom + rcCheckMute.top - marginBottom, 
               0, 0, SWP_NOZORDER | SWP_NOSIZE);

  RECT rcCheckOntop;
  GetWindowRect(hWndCheckOntop, &rcCheckOntop);
  SetWindowPos(hWndCheckOntop, NULL, 
               marginLeft + rcCheckMute.right - rcCheckMute.left + spaceHor, 
               height - rcCheckOntop.bottom + rcCheckOntop.top - marginBottom, 
               0, 0, SWP_NOZORDER | SWP_NOSIZE);

  SetWindowPos(hWndOutput, NULL, 
               0, 0, 
               width, 
               height - rcButtonOptions.bottom + rcButtonOptions.top - marginBottom - marginTop - spaceVer, 
               SWP_NOZORDER | SWP_NOMOVE);

  // somehow the Clear button doesn't redraw itself well, so force it
  InvalidateRect(hWndClear, NULL, TRUE);
  UpdateWindow(hWndClear);
}

BOOL CALLBACK MainDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg)
  {
    case WM_INITDIALOG:
      return (BOOL)HANDLE_WM_INITDIALOG(hWnd, wParam, lParam, onInitDialog);
    case WM_COMMAND:
      HANDLE_WM_COMMAND(hWnd, wParam, lParam, onCommand);
      break;
    case WM_CLOSE:
      DestroyWindow(hWnd);
      break;
    case WM_DESTROY:
      HANDLE_WM_DESTROY(hWnd, wParam, lParam, onDestroy);
      break;
    case WM_SIZE:
      HANDLE_WM_SIZE(hWnd, wParam, lParam, onSize);
      break;

    default:
      return FALSE;
  }
  return TRUE;
}

// This is exported function which allows to access Settings GUI from other applications
void WINAPI SPY_Setup()
{
  LoggerWin logger;
  ProfileWin profile;

  profile.getBool(NPSPY_REG_KEY_ONTOP, &logger.bOnTop);
  profile.getBool(NPSPY_REG_KEY_LOGTOWINDOW, &logger.bToWindow);
  profile.getBool(NPSPY_REG_KEY_LOGTOCONSOLE, &logger.bToConsole);
  profile.getBool(NPSPY_REG_KEY_LOGTOFILE, &logger.bToFile);
  profile.getBool(NPSPY_REG_KEY_SPALID, &logger.bSPALID);
  profile.getString(NPSPY_REG_KEY_LOGFILENAME, logger.szFile, strlen(logger.szFile));

  for(int i = 1; i < TOTAL_NUMBER_OF_API_CALLS; i++)
  {
    BOOL selected = TRUE;
    if(profile.getBool(ActionName[i], &selected))
      logger.bMutedCalls[i] = !selected;
  }

  onOptions(NULL, &logger);
}