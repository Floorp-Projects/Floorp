/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sean Su <ssu@netscape.com>
 *   Darin Fisher <darin@meer.net>
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

#include "uninstall.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"
#include "parser.h"

/* global variables */
HINSTANCE       hInst;

HANDLE          hAccelTable;

HWND            hDlgUninstall;
HWND            hDlgMessage;
HWND            hWndMain;

LPSTR           szEGlobalAlloc;
LPSTR           szEStringLoad;
LPSTR           szEDllLoad;
LPSTR           szEStringNull;

LPSTR           szClassName;
LPSTR           szUninstallDir;
LPSTR           szTempDir;
LPSTR           szOSTempDir;
LPSTR           szFileIniUninstall;
LPSTR           szFileIniDefaultsInfo;
LPSTR           gszSharedFilename;

ULONG           ulOSType;
DWORD           dwScreenX;
DWORD           dwScreenY;

DWORD           gdwWhatToDo;

BOOL            gbAllowMultipleInstalls = FALSE;

uninstallGen    ugUninstall;
diU             diUninstall;

DWORD           dwParentPID = 0;

BOOL            gbUninstallCompleted = FALSE;

/* Copy a file into a directory.  Write the path to the new file
 * into the result buffer (MAX_PATH in size). */
static BOOL CopyTo(LPCSTR file, LPCSTR destDir, LPSTR result)
{
  char leaf[MAX_BUF_TINY];

  ParsePath(file, leaf, sizeof(leaf), PP_FILENAME_ONLY);
  lstrcpy(result, destDir);
  lstrcat(result, "\\");
  lstrcat(result, leaf);

  return CopyFile(file, result, TRUE);
}

/* Spawn child process. */
static BOOL SpawnProcess(LPCSTR exePath, LPCSTR cmdLine)
{
  STARTUPINFO si = {sizeof(si), 0};
  PROCESS_INFORMATION pi = {0};

  BOOL ok = CreateProcess(exePath,
                          cmdLine,
                          NULL,  // no special security attributes
                          NULL,  // no special thread attributes
                          FALSE, // don't inherit filehandles
                          0,     // No special process creation flags
                          NULL,  // inherit my environment
                          NULL,  // use my current directory
                          &si,
                          &pi);

  if (ok) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }

  return ok;
}

/* This function is called to ensure that the running executable is a copy of
 * the actual uninstaller.  If not, then this function copies the uninstaller
 * into a temporary directory and invokes the copy of itself.  This is done to
 * enable the uninstaller to remove itself. */
static BOOL EnsureRunningAsCopy(LPCSTR cmdLine)
{
  char uninstExe[MAX_PATH], uninstIni[MAX_PATH];
  char tempBuf[MAX_PATH], tempDir[MAX_PATH] = ""; 
  DWORD n;

  if (dwParentPID != 0)
  {
    HANDLE hParent;
    
    /* OpenProcess may return NULL if the parent process has already gone away.
     * If not, then wait for the parent process to exit.  NOTE: The process may
     * be signaled before it releases the executable image, so we sit in a loop
     * until OpenProcess returns NULL. */
    while ((hParent = OpenProcess(SYNCHRONIZE, FALSE, dwParentPID)) != NULL)
    {
      DWORD rv = WaitForSingleObject(hParent, 5000);
      CloseHandle(hParent);
      if (rv != WAIT_OBJECT_0)
        return FALSE;
      Sleep(50);  /* prevent burning CPU while waiting */
    }

    return TRUE;
  }

  /* otherwise, copy ourselves to a temp location and execute the copy. */

  /* make unique folder under the Temp folder */
  n = GetTempPath(sizeof(tempDir)-1, tempDir);
  if (n == 0 || n > sizeof(tempDir)-1)
    return FALSE;
  lstrcat(tempDir, "nstmp");
  if (!MakeUniquePath(tempDir) || !CreateDirectory(tempDir, NULL))
    return FALSE;

  if (!GetModuleFileName(hInst, tempBuf, sizeof(tempBuf)))
    return FALSE;

  /* copy exe file into temp folder */
  if (!CopyTo(tempBuf, tempDir, uninstExe))
  {
    RemoveDirectory(tempDir);
    return FALSE;
  }

  /* copy ini file into temp folder */
  ParsePath(tempBuf, uninstIni, sizeof(uninstIni), PP_PATH_ONLY); 
  lstrcat(uninstIni, FILE_INI_UNINSTALL);

  if (!CopyTo(uninstIni, tempDir, tempBuf))
  {
    DeleteFile(uninstExe);
    RemoveDirectory(tempDir);
    return FALSE;
  }

  /* schedule temp dir and contents to be deleted on reboot */
  DeleteOnReboot(uninstExe);
  DeleteOnReboot(tempBuf);
  DeleteOnReboot(tempDir);

  /* append -ppid command line flag  */
  _snprintf(tempBuf, sizeof(tempBuf), "%s %s /ppid %lu",
            uninstExe, cmdLine, GetCurrentProcessId());

  /* call CreateProcess */
  SpawnProcess(uninstExe, tempBuf);

  return FALSE;  /* exit this process */
}

/* Uninstall completed; show some UI... */
static void OnUninstallComplete()
{
  char exePath[MAX_PATH], buf[MAX_PATH], title[MAX_BUF];
  int rv;

  if (!(ugUninstall.szProductName && ugUninstall.szProductName[0]) ||
      !(ugUninstall.szProductName && ugUninstall.szProductName[0]))
    return;

  /* only show the exit survey message box if a string is defined */
  GetPrivateProfileString("Messages", "MSG_EXIT_SURVEY", "", buf, sizeof(buf),
                          szFileIniUninstall);
  GetPrivateProfileString("Dialog Uninstall", "Title", "", title, sizeof(title),
                          szFileIniUninstall);
  if (!buf[0] || !title[0])
    return;

  rv = MessageBox(NULL, buf, title, MB_OKCANCEL | MB_ICONINFORMATION);
  if (rv != IDOK)
    return;

  /* find iexplore.exe.  we cannot use ShellExecute because the protocol
   * association (in HKEY_CLASSES_ROOT) may reference the app we just
   * uninstalled, which is a bug in and of itself. */
  GetWinReg(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\IE Setup\\Setup", "Path",
            exePath, sizeof(exePath));
  if (!exePath[0])
    return;
  lstrcat(exePath, "\\iexplore.exe");

  /* launch IE, and point it at the survey URL (whitespace in the product name
   * or user agent is okay) */
  _snprintf(buf, sizeof(buf),
            "\"%s\" \"https://survey.mozilla.com/1/%s/%s/exit.html\"", exePath,
            ugUninstall.szProductName, ugUninstall.szUserAgent);
  SpawnProcess(exePath, buf);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
  /***********************************************************************/
  /* HANDLE hInstance;       handle for this instance                    */
  /* HANDLE hPrevInstance;   handle for possible previous instances      */
  /* LPSTR  lpszCmdLine;     long pointer to exec command line           */
  /* int    nCmdShow;        Show code for main window display           */
  /***********************************************************************/

  MSG   msg;
  char  szBuf[MAX_BUF];
  int   iRv = WIZ_OK;
  HWND  hwndFW;

  if(!hPrevInstance)
  {
    hInst = GetModuleHandle(NULL);
    if(InitUninstallGeneral() || ParseCommandLine(lpszCmdLine))
    {
      PostQuitMessage(1);
    }
    else if((hwndFW = FindWindow(CLASS_NAME_UNINSTALL_DLG, NULL)) != NULL && !gbAllowMultipleInstalls)
    {
    /* Allow only one instance of setup to run.
     * Detect a previous instance of setup, bring it to the 
     * foreground, and quit current instance */

      ShowWindow(hwndFW, SW_RESTORE);
      SetForegroundWindow(hwndFW);
      iRv = WIZ_SETUP_ALREADY_RUNNING;
      PostQuitMessage(1);
    }
    else if(!EnsureRunningAsCopy(lpszCmdLine) || Initialize(hInst))
    {
      PostQuitMessage(1);
    }
    else if(!InitApplication(hInst))
    {
      char szEFailed[MAX_BUF];

      if(NS_LoadString(hInst, IDS_ERROR_FAILED, szEFailed, MAX_BUF) == WIZ_OK)
      {
        wsprintf(szBuf, szEFailed, "InitApplication().");
        PrintError(szBuf, ERROR_CODE_SHOW);
      }
      PostQuitMessage(1);
    }
    else if(ParseUninstallIni())
    {
      PostQuitMessage(1);
    }
    else if(ugUninstall.bUninstallFiles == TRUE)
    {
      if(diUninstall.bShowDialog == TRUE)
        hDlgUninstall = InstantiateDialog(hWndMain, DLG_UNINSTALL, diUninstall.szTitle, DlgProcUninstall);
      // Assumes that SHOWICONS, HIDEICONS, and SETDEFAULT never show dialogs
      else if((ugUninstall.mode == SHOWICONS) || (ugUninstall.mode == HIDEICONS))
        ParseDefaultsInfo();
      else if(ugUninstall.mode == SETDEFAULT)
        SetDefault();
      else
        ParseAllUninstallLogs();
    }
  }

  if((ugUninstall.bUninstallFiles == TRUE) && (diUninstall.bShowDialog == TRUE))
  {
    while(GetMessage(&msg, NULL, 0, 0))
    {
      if((!IsDialogMessage(hDlgUninstall, &msg)) && (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }

  if(diUninstall.bShowDialog == TRUE && gbUninstallCompleted)
  {
    OnUninstallComplete();
  }

  /* garbage collection */
  DeInitUninstallGeneral();
  if(iRv != WIZ_SETUP_ALREADY_RUNNING)
    /* Do clean up before exiting from the application */
    DeInitialize();

  return(msg.wParam);
} /*  End of WinMain */
