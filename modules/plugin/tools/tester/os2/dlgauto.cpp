/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define INCL_WIN
#include <os2.h>
#include <assert.h>

#include "resource.h"

#include "guihlp.h"
#include "plugin.h"
#include "scripter.h"
#include "logger.h"
#include "xp.h"

extern CLogger * pLogger;
static CScripter * pScripter = NULL;

static void onCommand(HWND hWnd, int id, HWND hWndCtl, USHORT codeNotify)
{
  CPlugin * pPlugin = (CPlugin *)WinQueryWindowULong(hWnd, QWL_USER);
  if(!pPlugin)
    return;

  switch (id)
  {
    case IDC_EDIT_SCRIPT_FILE_NAME:
    {
      if(codeNotify != EN_CHANGE)
        break;
      char szString[256];
      WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_SCRIPT_FILE_NAME), sizeof(szString), szString);
      int iLen = strlen(szString);
      WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), (iLen > 0));
      if(iLen <= 0)
        break;

      // synchronize log filename with current script filename
      char szLogFileName[256];
      char * p = strchr(szString, '.');

      if(p != NULL)
        *p = '\0';

      strcpy(szLogFileName, szString);
      strcat(szLogFileName, ".log");
      WinSetWindowText(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_EDIT_LOG_FILE_NAME), szLogFileName);

      // restore
      if(p)
        *p = '.';

      pPlugin->updatePrefs(gp_scriptfile, FALSE, szString);

      break;
    }
    /*
    case IDC_BUTTON_STOP:
      assert(pScripter != NULL);
      pScripter->setStopAutoExecFlag(TRUE);
      WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), TRUE);
      WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_STOP), FALSE);
      break;
    */
    case IDC_BUTTON_GO:
    {
      pLogger->blockDumpToFile(FALSE);

      //WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), FALSE);
      //WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_STOP), TRUE);
      //UpdateWindow(WinWindowFromID(hWnd, IDC_BUTTON_STOP));
      EnableWindowNow(WinWindowFromID(hWnd, IDC_BUTTON_GO), FALSE);

      EnableWindowNow(WinWindowFromID(hWnd, IDC_EDIT_SCRIPT_FILE_NAME), FALSE);
      EnableWindowNow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_EDIT_LOG_FILE_NAME), FALSE);
      EnableWindowNow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_CHECK_SHOW_LOG), FALSE);
      EnableWindowNow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_CHECK_LOG_TO_FRAME), FALSE);
      EnableWindowNow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_CHECK_LOG_TO_FILE), FALSE);
      BOOL bFlashWasEnabled = FALSE;
      if(WinIsWindowEnabled(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_BUTTON_FLUSH)))
      {
        bFlashWasEnabled = TRUE;
        EnableWindowNow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_BUTTON_FLUSH), FALSE);
      }
      EnableWindowNow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_BUTTON_CLEAR), FALSE);
      EnableWindowNow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_RADIO_MODE_MANUAL), FALSE);
      EnableWindowNow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_RADIO_MODE_AUTO), FALSE);
    
      if(pScripter != NULL)
        delete pScripter;

      pScripter = new CScripter();
      pScripter->associate(pPlugin);

      char szFileName[_MAX_PATH];
      pPlugin ->getModulePath(szFileName, sizeof(szFileName));

      char szFile[_MAX_PATH];
      WinQueryWindowText(WinWindowFromID(hWnd, IDC_EDIT_SCRIPT_FILE_NAME), sizeof(szFile), szFile);
      strcat(szFileName, szFile);

      if(pScripter->createScriptFromFile(szFileName))
      {
        int iRepetitions = pScripter->getCycleRepetitions();
        int iDelay = pScripter->getCycleDelay();
        if(iDelay < 0)
          iDelay = 0;

        assert(pLogger != NULL);
        pLogger->resetStartTime();

        ShowWindowNow(WinWindowFromID(hWnd, IDC_STATIC_REPETITIONS_LABEL), TRUE);
        char szLeft[256];

        for(int i = 0; i < iRepetitions; i++)
        {
          sprintf(szLeft, "%i", iRepetitions - i);
          WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_REPETITIONS_LEFT), szLeft);
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
        WinMessageBox(HWND_DESKTOP, hWnd, "Script file not found or invalid", "", 0, MB_OK | MB_ERROR);
      }

      delete pScripter;
      pScripter = NULL;

      WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_REPETITIONS_LEFT), "");
      WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_REPETITIONS_LABEL), FALSE);

      //WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), TRUE);
      //WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_STOP), FALSE);
      WinEnableWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), TRUE);

      WinEnableWindow(WinWindowFromID(hWnd, IDC_EDIT_SCRIPT_FILE_NAME), TRUE);
      WinEnableWindow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_EDIT_LOG_FILE_NAME), TRUE);
      WinEnableWindow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_CHECK_SHOW_LOG), TRUE);
      WinEnableWindow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_CHECK_LOG_TO_FRAME), TRUE);
      WinEnableWindow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_CHECK_LOG_TO_FILE), TRUE);
      if(bFlashWasEnabled)
        WinEnableWindow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_BUTTON_FLUSH), TRUE);
      WinEnableWindow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_BUTTON_CLEAR), TRUE);
      WinEnableWindow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_RADIO_MODE_MANUAL), TRUE);
      WinEnableWindow(WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), IDC_RADIO_MODE_AUTO), TRUE);
      break;
    }
    default:
      break;
  }
}

static BOOL onInitDialog(HWND hWnd, HWND hWndFocus, MPARAM mParam)
{
  CPlugin * pPlugin = (CPlugin *)mParam;
  WinSetWindowPtr(hWnd, QWL_USER, (PVOID)pPlugin);

  int iTopMargin = 188;
  WinSetWindowPos(hWnd, NULL, 0, iTopMargin, 0, 0, SWP_SHOW );  

  WinSetWindowText(WinWindowFromID(hWnd, IDC_EDIT_SCRIPT_FILE_NAME), pPlugin->m_Pref_szScriptFile);

  WinSetWindowText(WinWindowFromID(hWnd, IDC_STATIC_REPETITIONS_LEFT), "");
  WinShowWindow(WinWindowFromID(hWnd, IDC_STATIC_REPETITIONS_LABEL), FALSE);

  WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_GO), TRUE);
  WinShowWindow(WinWindowFromID(hWnd, IDC_BUTTON_STOP), FALSE);

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

MRESULT EXPENTRY NP_LOADDS AutoDlgProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch(msg)
  {
    case WM_INITDLG:
      onInitDialog(hWnd, 0, mp2);
      return (MRESULT)FALSE;
    case WM_COMMAND:
    case WM_CONTROL:
      onCommand(hWnd, SHORT1FROMMP(mp1), 0, SHORT2FROMMP(mp1));
      break;
    case WM_DESTROY:
      onDestroy(hWnd);
      break;

    default:
      return WinDefDlgProc(hWnd, msg, mp1, mp2);
  }
  return (MRESULT)TRUE;
}
