/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "xp.h"
#include <windowsx.h>

#include "windowsxx.h"

#include "resource.h"
#include "logger.h"

static void onCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
  switch (id)
  {
    case IDC_BUTTON_CHECKALL:
    {
      for(int i = IDC_CHECK_NPN_VERSION; i < IDC_CHECK_NPN_VERSION + TOTAL_NUMBER_OF_API_CALLS - 1; i++)
        CheckDlgButton(hWnd, i, BST_CHECKED);
      break;
    }
    case IDC_BUTTON_CLEARALL:
    {
      for(int i = IDC_CHECK_NPN_VERSION; i < IDC_CHECK_NPN_VERSION + TOTAL_NUMBER_OF_API_CALLS - 1; i++)
        CheckDlgButton(hWnd, i, BST_UNCHECKED);
      break;
    }
    default:
      break;
  }
}

static void onApply(HWND hWnd)
{
  Logger * logger = (Logger *)GetWindowLong(hWnd, DWL_USER);
  if(!logger)
    return;

  BOOL mutedcalls[TOTAL_NUMBER_OF_API_CALLS];

  mutedcalls[0] = FALSE; // for invalid call

  // we assume that checkbox ids start with IDC_CHECK_NPN_VERSION and are consequitive
  for(int i = IDC_CHECK_NPN_VERSION; i < IDC_CHECK_NPN_VERSION + TOTAL_NUMBER_OF_API_CALLS - 1; i++)
    mutedcalls[i - IDC_CHECK_NPN_VERSION + 1] = (BST_UNCHECKED == IsDlgButtonChecked(hWnd, i));

  logger->setMutedCalls(&mutedcalls[0]);
  logger->bSaveSettings = TRUE;
}

static void onNotify(HWND hWnd, int idCtrl, LPNMHDR lpNMHdr)
{
  switch(lpNMHdr->code)
  {
    case PSN_RESET:
      break;
    case PSN_APPLY:
      onApply(hWnd);
      break;
  }
}

static BOOL onInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam)
{
  Logger * logger = NULL;

  if(lParam)
  {
    logger = (Logger *)(((PROPSHEETPAGE *)lParam)->lParam);
    SetWindowLong(hWnd, DWL_USER, (long)logger);
  }

  BOOL * mutedcalls = logger->getMutedCalls();

  if(mutedcalls)
  {
    // we assume that checkbox ids start with IDC_CHECK_NPN_VERSION and are consequitive
    for(int i = IDC_CHECK_NPN_VERSION; i < IDC_CHECK_NPN_VERSION + TOTAL_NUMBER_OF_API_CALLS - 1; i++)
      CheckDlgButton(hWnd, i, mutedcalls[i - IDC_CHECK_NPN_VERSION + 1] ? BST_UNCHECKED : BST_CHECKED);
  }
  else
  {
    for(int i = IDC_CHECK_NPN_VERSION; i < IDC_CHECK_NPN_VERSION + TOTAL_NUMBER_OF_API_CALLS - 1; i++)
      CheckDlgButton(hWnd, i, BST_CHECKED);
  }

  return TRUE;
}

BOOL CALLBACK FilterPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg)
  {
    case WM_INITDIALOG:
      return (BOOL)HANDLE_WM_INITDIALOG(hWnd, wParam, lParam, onInitDialog);
    case WM_COMMAND:
      HANDLE_WM_COMMAND(hWnd, wParam, lParam, onCommand);
      break;
    case WM_NOTIFY:
      HANDLE_WM_NOTIFY(hWnd, wParam, lParam, onNotify);
      break;

    default:
      return FALSE;
  }
  return TRUE;
}
