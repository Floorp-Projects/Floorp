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
#include <assert.h>

#include "resource.h"

#include "guihlp.h"
#include "plugin.h"
#include "scripter.h"
#include "logger.h"
#include "xp.h"

extern CLogger * pLogger;
static CScripter * pScripter = NULL;

static void onCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
  CPlugin * pPlugin = (CPlugin *)GetWindowLong(hWnd, DWL_USER);
  assert(pPlugin != NULL);

  switch (id)
  {
    case IDC_EDIT_SCRIPT_FILE_NAME:
    {
      if(codeNotify != EN_CHANGE)
        break;
      char szString[256];
      Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_SCRIPT_FILE_NAME), szString, sizeof(szString));
      int iLen = strlen(szString);
      EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), (iLen > 0));
      if(iLen <= 0)
        break;

      // synchronize log filename with current script filename
      char szLogFileName[256];
      char * p = strchr(szString, '.');

      if(p != NULL)
        *p = '\0';

      strcpy(szLogFileName, szString);
      strcat(szLogFileName, ".log");
      Edit_SetText(GetDlgItem(GetParent(hWnd), IDC_EDIT_LOG_FILE_NAME), szLogFileName);

      break;
    }
    /*
    case IDC_BUTTON_STOP:
      assert(pScripter != NULL);
      pScripter->setStopAutoExecFlag(TRUE);
      ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), SW_SHOW);
      ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_STOP), SW_HIDE);
      break;
    */
    case IDC_BUTTON_GO:
    {
      pLogger->blockDumpToFile(FALSE);

      //ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), SW_HIDE);
      //ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_STOP), SW_SHOW);
      //UpdateWindow(GetDlgItem(hWnd, IDC_BUTTON_STOP));
      EnableWindowNow(GetDlgItem(hWnd, IDC_BUTTON_GO), FALSE);

      EnableWindowNow(GetDlgItem(hWnd, IDC_EDIT_SCRIPT_FILE_NAME), FALSE);
      EnableWindowNow(GetDlgItem(GetParent(hWnd), IDC_EDIT_LOG_FILE_NAME), FALSE);
      EnableWindowNow(GetDlgItem(GetParent(hWnd), IDC_CHECK_SHOW_LOG), FALSE);
      EnableWindowNow(GetDlgItem(GetParent(hWnd), IDC_CHECK_LOG_TO_FRAME), FALSE);
      EnableWindowNow(GetDlgItem(GetParent(hWnd), IDC_CHECK_LOG_TO_FILE), FALSE);
      BOOL bFlashWasEnabled = FALSE;
      if(IsWindowEnabled(GetDlgItem(GetParent(hWnd), IDC_BUTTON_FLUSH)))
      {
        bFlashWasEnabled = TRUE;
        EnableWindowNow(GetDlgItem(GetParent(hWnd), IDC_BUTTON_FLUSH), FALSE);
      }
      EnableWindowNow(GetDlgItem(GetParent(hWnd), IDC_BUTTON_CLEAR), FALSE);
      EnableWindowNow(GetDlgItem(GetParent(hWnd), IDC_RADIO_MODE_MANUAL), FALSE);
      EnableWindowNow(GetDlgItem(GetParent(hWnd), IDC_RADIO_MODE_AUTO), FALSE);
    
      if(pScripter != NULL)
        delete pScripter;

      pScripter = new CScripter();
      pScripter->associate(pPlugin);

      char szFileName[_MAX_PATH];
      pPlugin ->getModulePath(szFileName, sizeof(szFileName));

      char szFile[_MAX_PATH];
      Edit_GetText(GetDlgItem(hWnd, IDC_EDIT_SCRIPT_FILE_NAME), szFile, sizeof(szFile));
      strcat(szFileName, szFile);

      if(pScripter->createScriptFromFile(szFileName))
      {
        int iRepetitions = pScripter->getCycleRepetitions();
        int iDelay = pScripter->getCycleDelay();
        if(iDelay < 0)
          iDelay = 0;

        assert(pLogger != NULL);
        pLogger->resetStartTime();

        ShowWindowNow(GetDlgItem(hWnd, IDC_STATIC_REPETITIONS_LABEL), SW_SHOW);
        char szLeft[256];

        for(int i = 0; i < iRepetitions; i++)
        {
          wsprintf(szLeft, "%i", iRepetitions - i);
          Static_SetText(GetDlgItem(hWnd, IDC_STATIC_REPETITIONS_LEFT), szLeft);
          pScripter->executeScript();
          /*
          if(pScripter->getStopAutoExecFlag())
          {
            pScripter->setStopAutoExecFlag(FALSE);
            break;
          }
          */
          if(iDelay != 0)
            XP_Sleep(iDelay);
        }
      }
      else
      {
        MessageBox(hWnd, "Script file not found or invalid", "", MB_OK | MB_ICONERROR);
      }

      delete pScripter;
      pScripter = NULL;

      Static_SetText(GetDlgItem(hWnd, IDC_STATIC_REPETITIONS_LEFT), "");
      ShowWindow(GetDlgItem(hWnd, IDC_STATIC_REPETITIONS_LABEL), SW_HIDE);

      //ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), SW_SHOW);
      //ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_STOP), SW_HIDE);
      EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), TRUE);

      EnableWindow(GetDlgItem(hWnd, IDC_EDIT_SCRIPT_FILE_NAME), TRUE);
      EnableWindow(GetDlgItem(GetParent(hWnd), IDC_EDIT_LOG_FILE_NAME), TRUE);
      EnableWindow(GetDlgItem(GetParent(hWnd), IDC_CHECK_SHOW_LOG), TRUE);
      EnableWindow(GetDlgItem(GetParent(hWnd), IDC_CHECK_LOG_TO_FRAME), TRUE);
      EnableWindow(GetDlgItem(GetParent(hWnd), IDC_CHECK_LOG_TO_FILE), TRUE);
      if(bFlashWasEnabled)
        EnableWindow(GetDlgItem(GetParent(hWnd), IDC_BUTTON_FLUSH), TRUE);
      EnableWindow(GetDlgItem(GetParent(hWnd), IDC_BUTTON_CLEAR), TRUE);
      EnableWindow(GetDlgItem(GetParent(hWnd), IDC_RADIO_MODE_MANUAL), TRUE);
      EnableWindow(GetDlgItem(GetParent(hWnd), IDC_RADIO_MODE_AUTO), TRUE);
      break;
    }
    default:
      break;
  }
}

static BOOL onInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam)
{
  CPlugin * pPlugin = (CPlugin *)lParam;
  SetWindowLong(hWnd, DWL_USER, (long)pPlugin);

  int iTopMargin = 160;
  SetWindowPos(hWnd, NULL, 0,iTopMargin, 0,0, SWP_NOZORDER | SWP_NOSIZE);

  Edit_SetText(GetDlgItem(hWnd, IDC_EDIT_SCRIPT_FILE_NAME), pPlugin->m_Pref_szScriptFile);

  Static_SetText(GetDlgItem(hWnd, IDC_STATIC_REPETITIONS_LEFT), "");
  ShowWindow(GetDlgItem(hWnd, IDC_STATIC_REPETITIONS_LABEL), SW_HIDE);

  ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_GO), SW_SHOW);
  ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_STOP), SW_HIDE);

  return TRUE;
}

static void onDestroy(HWND hWnd)
{
  if(pScripter != NULL)
  {
    delete pScripter;
    pScripter = NULL;
  }
}

BOOL CALLBACK NP_LOADDS AutoDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg)
  {
    case WM_INITDIALOG:
      return (BOOL)HANDLE_WM_INITDIALOG(hWnd, wParam, lParam, onInitDialog);
    case WM_COMMAND:
      HANDLE_WM_COMMAND(hWnd, wParam, lParam, onCommand);
      break;
    case WM_DESTROY:
      HANDLE_WM_DESTROY(hWnd, wParam, lParam, onDestroy);
      break;

    default:
      return FALSE;
  }
  return TRUE;
}
