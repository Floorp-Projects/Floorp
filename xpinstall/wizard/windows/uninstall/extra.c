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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "extern.h"
#include "extra.h"
#include "parser.h"
#include "dialogs.h"
#include "ifuncns.h"
#include "process.h"
#include <winver.h>

// shellapi.h is needed to build with WIN32_LEAN_AND_MEAN
#include <shellapi.h>

#define HIDWORD(l)   ((DWORD) (((ULONG) (l) >> 32) & 0xFFFF))
#define LODWORD(l)   ((DWORD) (l))

#define INDEX_STR_LEN       10
#define PN_PROCESS          TEXT("Process")
#define PN_THREAD           TEXT("Thread")

ULONG  (PASCAL *NS_GetDiskFreeSpace)(LPCTSTR, LPDWORD, LPDWORD, LPDWORD, LPDWORD);
ULONG  (PASCAL *NS_GetDiskFreeSpaceEx)(LPCTSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);

BOOL InitApplication(HINSTANCE hInstance)
{
  WNDCLASS wc;

  wc.style         = CS_DBLCLKS | CS_SAVEBITS | CS_BYTEALIGNWINDOW;
  wc.lpfnWndProc   = DefDlgProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = DLGWINDOWEXTRA;
  wc.hInstance     = hInstance;
  wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_UNINSTALL));
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = szClassName;

  dwScreenX = GetSystemMetrics(SM_CXSCREEN);
  dwScreenY = GetSystemMetrics(SM_CYSCREEN);

  return(RegisterClass(&wc));
}

void PrintError(LPSTR szMsg, DWORD dwErrorCodeSH)
{
  DWORD dwErr;
  char  szErrorString[MAX_BUF];

  if(dwErrorCodeSH == ERROR_CODE_SHOW)
  {
    dwErr = GetLastError();
    wsprintf(szErrorString, "%d : %s", dwErr, szMsg);
  }
  else
    wsprintf(szErrorString, "%s", szMsg);

  if((ugUninstall.mode != SILENT) && (ugUninstall.mode != AUTO))
  {
    MessageBox(hWndMain, szErrorString, NULL, MB_ICONEXCLAMATION);
  }
  else if(ugUninstall.mode == AUTO)
  {
    ShowMessage(szErrorString, TRUE);
    Delay(5);
    ShowMessage(szErrorString, FALSE);
  }
}

void *NS_GlobalAlloc(DWORD dwMaxBuf)
{
  LPSTR szBuf = NULL;

  if((szBuf = GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT, dwMaxBuf)) == NULL)
  {     
    if((szEGlobalAlloc == NULL) || (*szEGlobalAlloc == '\0'))
      PrintError(TEXT("Memory allocation error."), ERROR_CODE_HIDE);
    else
      PrintError(szEGlobalAlloc, ERROR_CODE_SHOW);

    return(NULL);
  }
  else
    return(szBuf);
}

void FreeMemory(void **vPointer)
{
  if(*vPointer != NULL)
    *vPointer = GlobalFree(*vPointer);
}

HRESULT NS_LoadStringAlloc(HANDLE hInstance, DWORD dwID, LPSTR *szStringBuf, DWORD dwStringBuf)
{
  char szBuf[MAX_BUF];

  if((*szStringBuf = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  
  if(!LoadString(hInstance, dwID, *szStringBuf, dwStringBuf))
  {
    if((szEStringLoad == NULL) ||(*szEStringLoad == '\0'))
      wsprintf(szBuf, "Could not load string resource ID %d", dwID);
    else
      wsprintf(szBuf, szEStringLoad, dwID);

    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  return(0);
}

HRESULT NS_LoadString(HANDLE hInstance, DWORD dwID, LPSTR szStringBuf, DWORD dwStringBuf)
{
  char szBuf[MAX_BUF];

  if(!LoadString(hInstance, dwID, szStringBuf, dwStringBuf))
  {
    if((szEStringLoad == NULL) ||(*szEStringLoad == '\0'))
      wsprintf(szBuf, "Could not load string resource ID %d", dwID);
    else
      wsprintf(szBuf, szEStringLoad, dwID);

    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  return(WIZ_OK);
}

void Delay(DWORD dwSeconds)
{
  SleepEx(dwSeconds * 1000, FALSE);
}

HRESULT Initialize(HINSTANCE hInstance)
{
  char szBuf[MAX_BUF];

  hDlgMessage = NULL;
  DetermineOSVersion();
  gdwWhatToDo = WTD_ASK;

  /* load strings from setup.exe */
  if(NS_LoadStringAlloc(hInst, IDS_ERROR_GLOBALALLOC, &szEGlobalAlloc, MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInst, IDS_ERROR_STRING_LOAD, &szEStringLoad,  MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInst, IDS_ERROR_DLL_LOAD,    &szEDllLoad,     MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInst, IDS_ERROR_STRING_NULL, &szEStringNull,  MAX_BUF))
    return(1);

  ZeroMemory(szBuf, sizeof(MAX_BUF));
  if((szClassName = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  lstrcpy(szClassName, CLASS_NAME_UNINSTALL_DLG);
  hAccelTable = LoadAccelerators(hInst, szClassName);
  if((szUninstallDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  GetModuleFileName(NULL, szBuf, sizeof(szBuf));
  ParsePath(szBuf, szUninstallDir, MAX_BUF, PP_PATH_ONLY);

  if((szTempDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szOSTempDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szFileIniUninstall = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  lstrcpy(szFileIniUninstall, szUninstallDir);
  AppendBackSlash(szFileIniUninstall, MAX_BUF);
  lstrcat(szFileIniUninstall, FILE_INI_UNINSTALL);

  if((szFileIniDefaultsInfo = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  lstrcpy(szFileIniDefaultsInfo, szUninstallDir);
  AppendBackSlash(szFileIniDefaultsInfo, MAX_BUF);
  GetPrivateProfileString("General", "Defaults Info Filename", "", szBuf, MAX_BUF, szFileIniUninstall);
  lstrcat(szFileIniDefaultsInfo, szBuf);

  // determine the system's TEMP path
  if(GetTempPath(MAX_BUF, szTempDir) == 0)
  {
    if(GetWindowsDirectory(szTempDir, MAX_BUF) == 0)
    {
      char szEGetWinDirFailed[MAX_BUF];

      if(GetPrivateProfileString("Messages", "ERROR_GET_WINDOWS_DIRECTORY_FAILED", "", 
                                 szEGetWinDirFailed, sizeof(szEGetWinDirFailed),
                                 szFileIniUninstall))
        PrintError(szEGetWinDirFailed, ERROR_CODE_SHOW);

      return(1);
    }

    AppendBackSlash(szTempDir, MAX_BUF);
    lstrcat(szTempDir, "TEMP");
  }
  lstrcpy(szOSTempDir, szTempDir);
  AppendBackSlash(szTempDir, MAX_BUF);
  lstrcat(szTempDir, WIZ_TEMP_DIR);

  /*  if multiple installer instances are allowed; 
      each instance requires a unique temp directory
   */
  if(gbAllowMultipleInstalls)
  {
    DWORD dwLen = lstrlen(szTempDir);

    if (strncmp(szUninstallDir, szTempDir, dwLen) == 0)
    {
      lstrcpy(szTempDir, szUninstallDir);
    }
    else
    {
      int i;
      for(i = 1; i <= 100 && (FileExists(szTempDir) != FALSE); i++)
      {
        itoa(i, (szTempDir + dwLen), 10);
      }
    
      if (FileExists(szTempDir) != FALSE)
      {
        MessageBox(hWndMain, "Cannot create temp directory", NULL, MB_OK | MB_ICONEXCLAMATION);
        exit(1);
      }
    }
  }
  else
  {
    // we're not running in mmi mode (allow multiple instances of installer
    // to run at the same time), we should look for and remove the dirs
    // that are created in the mmi mode.
    DWORD dwLen;
    char tempDirToCleanup[MAX_BUF];
    int i = 1;

    lstrcpy(tempDirToCleanup, szTempDir);
    dwLen = lstrlen(tempDirToCleanup);
    itoa(i, (tempDirToCleanup + dwLen), 10);
    for(i = 2; i <= 100 && (FileExists(tempDirToCleanup)); i++)
    {
      DirectoryRemove(tempDirToCleanup, TRUE);
      itoa(i, (tempDirToCleanup + dwLen), 10);
    }
  }

  if(!FileExists(szTempDir))
  {
    AppendBackSlash(szTempDir, MAX_BUF);
    CreateDirectoriesAll(szTempDir);
    if(!FileExists(szTempDir))
    {
      char szECreateTempDir[MAX_BUF];

      if(GetPrivateProfileString("Messages", "ERROR_CREATE_TEMP_DIR", "",
                                 szECreateTempDir, sizeof(szECreateTempDir), 
                                 szFileIniUninstall))
      {
        wsprintf(szBuf, szECreateTempDir, szTempDir);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }
      return(1);
    }
    RemoveBackSlash(szTempDir);
  }

  ugUninstall.bVerbose = FALSE;
  ugUninstall.bUninstallFiles = TRUE;
  ugUninstall.bSharedInst = FALSE;

  return(0);
}

/* Function to remove quotes from a string */
void RemoveQuotes(LPSTR lpszSrc, LPSTR lpszDest, int iDestSize)
{
  char *lpszBegin;

  if(lstrlen(lpszSrc) > iDestSize)
    return;

  if(*lpszSrc == '\"')
    lpszBegin = &lpszSrc[1];
  else
    lpszBegin = lpszSrc;

  lstrcpy(lpszDest, lpszBegin);

  if(lpszDest[lstrlen(lpszDest) - 1] == '\"')
    lpszDest[lstrlen(lpszDest) - 1] = '\0';
}

/* Function to copy strings safely.
 *   returns the amount of memory required (including NULL byte) if there's not enough
 *   else, it returns 0 for success.
 */
int MozCopyStr(LPSTR szSrc, LPSTR szDest, DWORD dwDestBufSize)
{
  DWORD length = lstrlen(szSrc) + 1;
  strncpy(szDest, szSrc, dwDestBufSize);
  if(length > dwDestBufSize)
  {
    szDest[dwDestBufSize - 1] = '\0';
    return(length);
  }
  return(0);
}

/* Function to locate the first non space character in a string,
 * and return a pointer to it. */
LPSTR GetFirstNonSpace(LPSTR lpszString)
{
  char* p = lpszString;
  while (*p && isspace(*p))
    p = CharNext(p);

  if(*p == '\0')  // null means end of string
    return NULL;

  return p;
}

/* Function to locate the first space character in a string,
 * and return a pointer to it. */
LPSTR GetFirstSpace(LPSTR lpszString)
{
  char* p = lpszString;
  while (*p && !isspace(*p))
    p = CharNext(p);

  if (*p == '\0')  // null means end of string
    return NULL;

  return p;
}

/* Function to locate the first character c in lpszString,
 * and return a pointer to it. */
LPSTR MozStrChar(LPSTR lpszString, char c)
{
  char* p = lpszString;
  while (*p && (*p != c))
    p = CharNext(p);

  if (*p == '\0')  // null means end of string
    return NULL;

  return p;
}

/* Function to return the argument count given a command line input
 * format string */
int GetArgC(LPSTR lpszCommandLine)
{
  int   i;
  int   iArgCount;
  int   iStrLength;
  LPSTR lpszBeginStr;
  BOOL  bFoundQuote;
  BOOL  bFoundSpace;

  iArgCount    = 0;
  lpszBeginStr = GetFirstNonSpace(lpszCommandLine);

  if(lpszBeginStr == NULL)
    return(iArgCount);

  iStrLength   = lstrlen(lpszBeginStr);
  bFoundQuote  = FALSE;
  bFoundSpace  = TRUE;

  for(i = 0; i < iStrLength; i++)
  {
    if(lpszCommandLine[i] == '\"')
    {
      if(bFoundQuote == FALSE)
      {
        ++iArgCount;
        bFoundQuote = TRUE;
      }
      else
      {
        bFoundQuote = FALSE;
      }
    }
    else if(bFoundQuote == FALSE)
    {
      if(!isspace(lpszCommandLine[i]) && (bFoundSpace == TRUE))
      {
        ++iArgCount;
        bFoundSpace = FALSE;
      }
      else if(isspace(lpszCommandLine[i]))
      {
        bFoundSpace = TRUE;
      }
    }
  }

  return(iArgCount);
}

/* Function to return a specific argument parameter from a given command line input
 * format string. */
LPSTR GetArgV(LPSTR lpszCommandLine, int iIndex, LPSTR lpszDest, int iDestSize)
{
  int   i;
  int   j;
  int   iArgCount;
  int   iStrLength;
  LPSTR lpszBeginStr;
  LPSTR lpszDestTemp;
  BOOL  bFoundQuote;
  BOOL  bFoundSpace;

  iArgCount    = 0;
  lpszBeginStr = GetFirstNonSpace(lpszCommandLine);

  if(lpszBeginStr == NULL)
    return(NULL);

  lpszDestTemp = (char *)calloc(iDestSize, sizeof(char));
  if(lpszDestTemp == NULL)
  {
    PrintError("Out of memory", ERROR_CODE_HIDE);
    exit(1);
  }

  ZeroMemory(lpszDest, iDestSize);
  iStrLength    = lstrlen(lpszBeginStr);
  bFoundQuote   = FALSE;
  bFoundSpace   = TRUE;
  j             = 0;

  for(i = 0; i < iStrLength; i++)
  {
    if(lpszCommandLine[i] == '\"')
    {
      if(bFoundQuote == FALSE)
      {
        ++iArgCount;
        bFoundQuote = TRUE;
      }
      else
      {
        bFoundQuote = FALSE;
      }
    }
    else if(bFoundQuote == FALSE)
    {
      if(!isspace(lpszCommandLine[i]) && (bFoundSpace == TRUE))
      {
        ++iArgCount;
        bFoundSpace = FALSE;
      }
      else if(isspace(lpszCommandLine[i]))
      {
        bFoundSpace = TRUE;
      }
    }

    if((iIndex == (iArgCount - 1)) &&
      ((bFoundQuote == TRUE) || (bFoundSpace == FALSE) ||
      ((bFoundQuote == FALSE) && (lpszCommandLine[i] == '\"'))))
    {
      if(j < iDestSize)
      {
        lpszDestTemp[j] = lpszCommandLine[i];
        ++j;
      }
      else
      {
        lpszDestTemp[j] = '\0';
      }
    }
  }

  RemoveQuotes(lpszDestTemp, lpszDest, iDestSize);
  free(lpszDestTemp);
  return(lpszDest);
}

void SetUninstallRunMode(LPSTR szMode)
{
  /* Check to see if mode has already been set.  If so,
   * then do not override it.
   */
  if(ugUninstall.mode != NOT_SET)
    return;

  if(lstrcmpi(szMode, "NORMAL") == 0)
    ugUninstall.mode = NORMAL;
  if(lstrcmpi(szMode, "AUTO") == 0)
    ugUninstall.mode = AUTO;
  if(lstrcmpi(szMode, "SILENT") == 0)
    ugUninstall.mode = SILENT;
  if(lstrcmpi(szMode, "SHOWICONS") == 0)
    ugUninstall.mode = SHOWICONS;
  if(lstrcmpi(szMode, "HIDEICONS") == 0)
    ugUninstall.mode = HIDEICONS;
  if(lstrcmpi(szMode, "SETDEFAULT") == 0)
    ugUninstall.mode = SETDEFAULT;
}

void RemoveBackSlash(LPSTR szInput)
{
  int   iCounter;
  DWORD dwInputLen;

  if(szInput != NULL)
  {
    dwInputLen = lstrlen(szInput);

    for(iCounter = dwInputLen -1; iCounter >= 0 ; iCounter--)
    {
      if(szInput[iCounter] == '\\')
        szInput[iCounter] = '\0';
      else
        break;
    }
  }
}

void AppendBackSlash(LPSTR szInput, DWORD dwInputSize)
{
  if(szInput != NULL)
  {
    if(*szInput == '\0')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "\\");
      }
    }
    else if(szInput[strlen(szInput) - 1] != '\\')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "\\");
      }
    }
  }
}

void RemoveSlash(LPSTR szInput)
{
  int   iCounter;
  DWORD dwInputLen;

  if(szInput != NULL)
  {
    dwInputLen = lstrlen(szInput);

    for(iCounter = dwInputLen -1; iCounter >= 0 ; iCounter--)
    {
      if(szInput[iCounter] == '/')
        szInput[iCounter] = '\0';
      else
        break;
    }
  }
}

void AppendSlash(LPSTR szInput, DWORD dwInputSize)
{
  if(szInput != NULL)
  {
    if(*szInput == '\0')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "/");
      }
    }
    else if(szInput[strlen(szInput) - 1] != '/')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "/");
      }
    }
  }
}

void ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwOutputSize, DWORD dwType)
{
  int   iCounter;
  DWORD dwCounter;
  DWORD dwInputLen;
  BOOL  bFound;

  if((szInput != NULL) && (szOutput != NULL))
  {
    bFound        = FALSE;
    dwInputLen    = lstrlen(szInput);
    ZeroMemory(szOutput, dwOutputSize);

    if(dwInputLen < dwOutputSize)
    {
      switch(dwType)
      {
        case PP_FILENAME_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              lstrcpy(szOutput, &szInput[iCounter + 1]);
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            lstrcpy(szOutput, szInput);

          break;

        case PP_PATH_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              lstrcpy(szOutput, szInput);
              szOutput[iCounter + 1] = '\0';
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            lstrcpy(szOutput, szInput);

          break;

        case PP_ROOT_ONLY:
          if(szInput[1] == ':')
          {
            szOutput[0] = szInput[0];
            szOutput[1] = szInput[1];
            AppendBackSlash(szOutput, dwOutputSize);
          }
          else if(szInput[1] == '\\')
          {
            int iFoundBackSlash = 0;
            for(dwCounter = 0; dwCounter < dwInputLen; dwCounter++)
            {
              if(szInput[dwCounter] == '\\')
              {
                szOutput[dwCounter] = szInput[dwCounter];
                ++iFoundBackSlash;
              }

              if(iFoundBackSlash == 3)
                break;
            }

            if(iFoundBackSlash != 0)
              AppendBackSlash(szOutput, dwOutputSize);
          }
          break;
      }
    }
  }
}

void DetermineOSVersion()
{
  BOOL          bIsWin95Debute;
  char          szEMsg[MAX_BUF];
  OSVERSIONINFO osVersionInfo;

  ulOSType = 0;
  osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  if(!GetVersionEx(&osVersionInfo))
  {
    /* GetVersionEx() failed for some reason.  It's not fatal, but could cause
     * some complications during installation */
    if(GetPrivateProfileString("Messages", "ERROR_GETVERSION", "", szEMsg, sizeof(szEMsg), szFileIniUninstall))
      PrintError(szEMsg, ERROR_CODE_SHOW);
  }

  bIsWin95Debute  = IsWin95Debute();
  switch(osVersionInfo.dwPlatformId)
  {
    case VER_PLATFORM_WIN32_WINDOWS:
      ulOSType |= OS_WIN9x;
      if(osVersionInfo.dwMinorVersion == 0)
      {
        ulOSType |= OS_WIN95;
        if(bIsWin95Debute)
          ulOSType |= OS_WIN95_DEBUTE;
      }
      else
        ulOSType |= OS_WIN98;
      break;

    case VER_PLATFORM_WIN32_NT:
      ulOSType |= OS_NT;
      switch(osVersionInfo.dwMajorVersion)
      {
        case 3:
          ulOSType |= OS_NT3;
          break;

        case 4:
          ulOSType |= OS_NT4;
          break;

        default:
          ulOSType |= OS_NT5;
          switch(osVersionInfo.dwMinorVersion)
          {
            case 0:
              /* a minor version of 0 (major.minor.build) indicates Win2000 */
              ulOSType |= OS_NT50;
              break;

            case 1:
              /* a minor version of 1 (major.minor.build) indicates WinXP */
              ulOSType |= OS_NT51;
          break;
          }
        break;
      }
      break;

    default:
      if(GetPrivateProfileString("Messages", "ERROR_SETUP_REQUIREMENT", "", szEMsg, sizeof(szEMsg), szFileIniUninstall))
        PrintError(szEMsg, ERROR_CODE_HIDE);

      exit(1);
      break;
  }
}

HRESULT WinSpawn(LPSTR szClientName, LPSTR szParameters, LPSTR szCurrentDir, int iShowCmd, BOOL bWait)
{
  SHELLEXECUTEINFO seInfo;

  seInfo.cbSize       = sizeof(SHELLEXECUTEINFO);
  seInfo.fMask        = SEE_MASK_DOENVSUBST | SEE_MASK_FLAG_DDEWAIT | SEE_MASK_NOCLOSEPROCESS;
  seInfo.hwnd         = hWndMain;
  seInfo.lpVerb       = NULL;
  seInfo.lpFile       = szClientName;
  seInfo.lpParameters = szParameters;
  seInfo.lpDirectory  = szCurrentDir;
  seInfo.nShow        = SW_SHOWNORMAL;
  seInfo.hInstApp     = 0;
  seInfo.lpIDList     = NULL;
  seInfo.lpClass      = NULL;
  seInfo.hkeyClass    = 0;
  seInfo.dwHotKey     = 0;
  seInfo.hIcon        = 0;
  seInfo.hProcess     = 0;

  if((ShellExecuteEx(&seInfo) != 0) && (seInfo.hProcess != NULL))
  {
    if(bWait)
    {
      for(;;)
      {
        if(WaitForSingleObject(seInfo.hProcess, 200) == WAIT_OBJECT_0)
          break;

        ProcessWindowsMessages();
      }
    }
    return(TRUE);
  }
  return(FALSE);
}

HRESULT InitDlgUninstall(diU *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgUninstall(diU *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
}

HRESULT InitUninstallGeneral()
{
  ugUninstall.mode = NOT_SET;

  if((ugUninstall.szAppPath                 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szLogPath                 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szLogFilename             = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szCompanyName             = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szProductName             = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szWrKey                   = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szUserAgent               = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szDefaultComponent        = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szWrMainKey               = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szDescription             = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szUninstallKeyDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szUninstallFilename       = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szClientAppID             = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szClientAppPath           = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitUninstallGeneral()
{
  FreeMemory(&(ugUninstall.szAppPath));
  FreeMemory(&(ugUninstall.szLogPath));
  FreeMemory(&(ugUninstall.szLogFilename));
  FreeMemory(&(ugUninstall.szDescription));
  FreeMemory(&(ugUninstall.szUninstallKeyDescription));
  FreeMemory(&(ugUninstall.szUninstallFilename));
  FreeMemory(&(ugUninstall.szUserAgent));
  FreeMemory(&(ugUninstall.szDefaultComponent));
  FreeMemory(&(ugUninstall.szWrKey));
  FreeMemory(&(ugUninstall.szCompanyName));
  FreeMemory(&(ugUninstall.szProductName));
  FreeMemory(&(ugUninstall.szWrMainKey));
  FreeMemory(&(ugUninstall.szClientAppID));
  FreeMemory(&(ugUninstall.szClientAppPath));
  DeleteObject(ugUninstall.definedFont);
}

sil *CreateSilNode()
{
  sil *silNode;

  if((silNode = NS_GlobalAlloc(sizeof(struct sInfoLine))) == NULL)
    exit(1);

  if((silNode->szLine = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);

  silNode->ullLineNumber = 0;
  silNode->Next          = silNode;
  silNode->Prev          = silNode;

  return(silNode);
}

void SilNodeInsert(sil *silHead, sil *silTemp)
{
  if(silHead == NULL)
  {
    silHead          = silTemp;
    silHead->Next    = silHead;
    silHead->Prev    = silHead;
  }
  else
  {
    silTemp->Next           = silHead;
    silTemp->Prev           = silHead->Prev;
    silHead->Prev->Next     = silTemp;
    silHead->Prev           = silTemp;
  }
}

void SilNodeDelete(sil *silTemp)
{
  if(silTemp != NULL)
  {
    silTemp->Next->Prev = silTemp->Prev;
    silTemp->Prev->Next = silTemp->Next;
    silTemp->Next       = NULL;
    silTemp->Prev       = NULL;

    FreeMemory(&(silTemp->szLine));
    FreeMemory(&silTemp);
  }
}

BOOL IsWin95Debute()
{
  HINSTANCE hLib;
  BOOL      bIsWin95Debute;

  bIsWin95Debute = FALSE;
  if((hLib = LoadLibraryEx("kernel32.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) != NULL)
  {
    if(((FARPROC)NS_GetDiskFreeSpaceEx = GetProcAddress(hLib, "GetDiskFreeSpaceExA")) == NULL)
    {
      (FARPROC)NS_GetDiskFreeSpace = GetProcAddress(hLib, "GetDiskFreeSpaceA");
      bIsWin95Debute = TRUE;
    }

    FreeLibrary(hLib);
  }
  return(bIsWin95Debute);
}

DWORD ParseCommandLine(LPSTR lpszCmdLine)
{
  char  szArgVBuf[MAX_BUF];
  int   i;
  int   iArgC;

  i     = 0;
  iArgC = GetArgC(lpszCmdLine);
  while(i < iArgC)
  {
    GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));

    if((lstrcmpi(szArgVBuf, "-ma") == 0) || (lstrcmpi(szArgVBuf, "/ma") == 0))
    {
      SetUninstallRunMode("AUTO");
    }
    else if((lstrcmpi(szArgVBuf, "-ms") == 0) || (lstrcmpi(szArgVBuf, "/ms") == 0))
    {
      SetUninstallRunMode("SILENT");
    }
    else if((lstrcmpi(szArgVBuf, "-ua") == 0) || (lstrcmpi(szArgVBuf, "/ua") == 0))
    {
      if((i + 1) < iArgC)
        GetArgV(lpszCmdLine, ++i, ugUninstall.szUserAgent, MAX_BUF);
    }
    else if((lstrcmpi(szArgVBuf, "-ss") == 0) || (lstrcmpi(szArgVBuf, "/ss") == 0))
    // Show Shortcuts
    {
      SetUninstallRunMode("SHOWICONS");
      if((i + 1) < iArgC)
        GetArgV(lpszCmdLine, ++i, ugUninstall.szDefaultComponent, MAX_BUF);
    }
    else if((lstrcmpi(szArgVBuf, "-hs") == 0) || (lstrcmpi(szArgVBuf, "/hs") == 0))
    // Hide Shortcuts
    {
      SetUninstallRunMode("HIDEICONS");
      if((i + 1) < iArgC)
        GetArgV(lpszCmdLine, ++i, ugUninstall.szDefaultComponent, MAX_BUF);
    }
    else if((lstrcmpi(szArgVBuf, "-sd") == 0) || (lstrcmpi(szArgVBuf, "/sd") == 0))
    // SetDefault
    {
      SetUninstallRunMode("SETDEFAULT");
      if((i + 1) < iArgC)
        GetArgV(lpszCmdLine, ++i, ugUninstall.szDefaultComponent, MAX_BUF);
    }
    else if((lstrcmpi(szArgVBuf, "-app") == 0) || (lstrcmpi(szArgVBuf, "/app") == 0))
    // Set the App ID
    {
      if((i + 1) < iArgC)
        GetArgV(lpszCmdLine, ++i, ugUninstall.szClientAppID, MAX_BUF);
    }
    else if((lstrcmpi(szArgVBuf, "-app_path") == 0) || (lstrcmpi(szArgVBuf, "/app_path") == 0))
    // Set the App Path
    {
      if((i + 1) < iArgC)
        GetArgV(lpszCmdLine, ++i, ugUninstall.szClientAppPath, MAX_BUF);
    }
    else if((lstrcmpi(szArgVBuf, "-reg_path") == 0) || (lstrcmpi(szArgVBuf, "/reg_path") == 0))
    // Set the alternative Windows registry path
    {
      if((i + 1) < iArgC)
        GetArgV(lpszCmdLine, ++i, ugUninstall.szWrMainKey, MAX_BUF);
    }
    else if((lstrcmpi(szArgVBuf, "-v") == 0) || (lstrcmpi(szArgVBuf, "/v") == 0))
    // Set Verbose
    {
      ugUninstall.bVerbose = TRUE;
    }
    else if(!lstrcmpi(szArgVBuf, "-mmi") || !lstrcmpi(szArgVBuf, "/mmi"))
    {
      gbAllowMultipleInstalls = TRUE;    
    }

    ++i;
  }
  return(WIZ_OK);
}

int PreCheckInstance(char *szSection, char *szIniFile)
{
  char  szBuf[MAX_BUF];
  char  szKey[MAX_BUF];
  char  szName[MAX_BUF];
  char  szParameter[MAX_BUF];
  char  szPath[MAX_BUF];
  char  szFile[MAX_BUF];
  char  *ptrName = NULL;
  HKEY  hkeyRoot;
  int   iRv = WIZ_OK;
  DWORD dwCounter = 0;
  BOOL  bContinue = TRUE;
  char  szExtraCmd[] = "Extra Cmd";
  char  szExtraCmdKey[MAX_BUF];

  do
  {
    /* Read the win reg key path */
    wsprintf(szExtraCmdKey, "%s%d Reg Key", szExtraCmd, dwCounter);
    GetPrivateProfileString(szSection,
                            szExtraCmdKey,
                            "",
                            szKey,
                            sizeof(szKey),
                            szIniFile);
    if(*szKey == '\0')
    {
      bContinue = FALSE;
      continue;
    }

    /* Read the win reg root key */
    wsprintf(szExtraCmdKey, "%s%d Reg Key Root", szExtraCmd, dwCounter);
    GetPrivateProfileString(szSection,
                            szExtraCmdKey,
                            "",
                            szBuf,
                            sizeof(szBuf),
                            szIniFile);
    if(*szBuf == '\0')
    {
      bContinue = FALSE;
      continue;
    }
    hkeyRoot = ParseRootKey(szBuf);

    /* Read the win reg name value */
    wsprintf(szExtraCmdKey, "%s%d Reg Name", szExtraCmd, dwCounter);
    GetPrivateProfileString(szSection,
                            szExtraCmdKey,
                            "",
                            szName,
                            sizeof(szName),
                            szIniFile);
    if(*szName == '\0')
      ptrName = NULL;
    else
      ptrName = szName;

    /* Read the parameter to use for quitting the browser's turbo mode */
    wsprintf(szExtraCmdKey, "%s%d Parameter", szExtraCmd, dwCounter);
    GetPrivateProfileString(szSection,
                            szExtraCmdKey,
                            "",
                            szParameter,
                            sizeof(szParameter),
                            szIniFile);

    /* Read the win reg key that contains the path to the browser */
    GetWinReg(hkeyRoot, szKey, ptrName, szFile, sizeof(szFile));
    ParsePath(szFile, szPath, sizeof(szPath), PP_PATH_ONLY);

    /* Make sure the file exists */
    if(FileExists(szFile))
    {
      // we've found a file, so let's execute it and stop.  No need to look
      // for other keys to parse.  We only want to do that if the file is
      // _not_ found.  This is for when we change the name of the browser
      // app file and still need to deal with locating it and calling
      // -kill on it. ie.
      //   previous name: netscp6.exe
      //   new name: netscp.exe
      // We only need to call one of them, not both.
      bContinue = FALSE;

      /* Run the file */
      WinSpawn(szFile, szParameter, szPath, SW_HIDE, WS_WAIT);

      /* Even though WinSpawn is suppose to wait for the app to finish, this
       * does not really work that way for trying to quit the browser when
       * it's in turbo mode, so we wait 2 secs for it to complete. */
      Delay(2);
    }

    ++dwCounter;
  } while(bContinue);

  return(iRv);
}

HRESULT GetCheckInstanceQuitMessage(char *aSection, char *aMessage, DWORD aMessageBufSize)
{
  *aMessage = '\0';
  GetPrivateProfileString(aSection, "Message", "", aMessage, aMessageBufSize, szFileIniUninstall);
  return(WIZ_OK);
}

HRESULT ShowMessageAndQuitProcess(HWND aHwndFW, char *aMsgQuitProcess, char *aMsgWait, BOOL aCloseAllWindows, char *aProcessName, BOOL aForceQuit)
{
  switch(ugUninstall.mode)
  {
    case NORMAL:
    {
      char msgTitleStr[MAX_BUF];
      GetPrivateProfileString("Messages", "MB_ATTENTION_STR", "", msgTitleStr, sizeof(msgTitleStr), szFileIniUninstall);
      MessageBox(hWndMain, aMsgQuitProcess, msgTitleStr, MB_ICONEXCLAMATION | MB_SETFOREGROUND);
      break;
    }

    case AUTO:
    {
      /* Setup mode is AUTO.  Show message, timeout, then auto close
       * all the windows associated with the process */
      ShowMessage(aMsgQuitProcess, TRUE);
      Delay(5);
      ShowMessage(aMsgQuitProcess, FALSE);
      break;
    }

    case SILENT:
      break;
  }

  if(aForceQuit)
  {
    assert(aProcessName);
    FindAndKillProcess(aProcessName, KP_KILL_PROCESS);
  }
  else
  {
    assert(aHwndFW);
    /* First try sending a WM_QUIT message to the window because this is the
     * preferred way to quit a process.  If it works, then we're good and
     * CloseAllWindowsOfWindowHandle() will have nothing to do.
     * If it doesn't work, then CloseAllWindowsOfWindowHandle will try to
     * quit the process. */
    SendMessageTimeout(aHwndFW, WM_QUIT, (WPARAM)1, (LPARAM)0, SMTO_NORMAL, WM_CLOSE_TIMEOUT_VALUE, NULL);
    if(aCloseAllWindows)
      CloseAllWindowsOfWindowHandle(aHwndFW, aMsgWait);
  }
  Delay(2);
  return(WIZ_OK);
}

HRESULT CheckInstances()
{
  char  section[MAX_BUF];
  char  processName[MAX_BUF];
  char  className[MAX_BUF];
  char  windowName[MAX_BUF];
  char  closeAllWindows[MAX_BUF];
  char  message[MAX_BUF];
  char  msgTitleStr[MAX_BUF];
  char  prettyName[MAX_BUF];
  char  buf[MAX_BUF];
  char  msgWait[MAX_BUF];
  int   index;
  int   killProcessTries = 0;
  int   instanceOfFoundProcess = 0;
  BOOL  bContinue;
  BOOL  bCloseAllWindows;
  HWND  hwndFW;
  LPSTR szWN;
  LPSTR szCN;
  DWORD dwRv0;
  DWORD dwRv1;

  GetPrivateProfileString("Messages", "MB_ATTENTION_STR", "", msgTitleStr, sizeof(msgTitleStr), szFileIniUninstall);
  bContinue = TRUE;
  index    = -1;
  while(bContinue)
  {
    *className  = '\0';
    *windowName = '\0';
    *message    = '\0';

    wsprintf(section, "Check Instance%d", ++index);
    GetPrivateProfileString(section, "Process Name", "", processName, sizeof(processName), szFileIniUninstall);
    GetPrivateProfileString(section, "Pretty Name", "", prettyName, sizeof(prettyName), szFileIniUninstall);
    GetPrivateProfileString(section, "Message Wait", "", msgWait, sizeof(msgWait), szFileIniUninstall);
    GetPrivateProfileString(section, "Close All Process Windows", "", closeAllWindows, sizeof(closeAllWindows), szFileIniUninstall);
    if(lstrcmpi(closeAllWindows, "TRUE") == 0)
      bCloseAllWindows = TRUE;
    else
      bCloseAllWindows = FALSE;

    if(instanceOfFoundProcess != index)
    {
      killProcessTries = 0;
      instanceOfFoundProcess = index;
    }

    if((killProcessTries == 1) && (*processName != '\0'))
    {
      if(FindAndKillProcess(processName, KP_DO_NOT_KILL_PROCESS))
      {
        /* found process, display warning message, then kill process */
        GetPrivateProfileString("Messages", "MSG_FORCE_QUIT_PROCESS", "", message, sizeof(message), szFileIniUninstall);
        if(*message != '\0')
        {
          wsprintf(buf, message, prettyName, processName, prettyName, prettyName);
          ShowMessageAndQuitProcess(NULL, buf, msgWait, bCloseAllWindows, processName, CI_FORCE_QUIT_PROCESS);
          ++killProcessTries;
          instanceOfFoundProcess = index--;
        }
      }
      continue;
    }
    else if(killProcessTries == MAX_KILL_PROCESS_RETRIES)
    {
      GetPrivateProfileString("Messages", "MSG_FORCE_QUIT_PROCESS_FAILED", "", message, sizeof(message), szFileIniUninstall);
      if(*message != '\0')
      {
        wsprintf(buf, message, prettyName, processName, prettyName);
        switch(ugUninstall.mode)
        {
          case NORMAL:
            MessageBox(hWndMain, buf, msgTitleStr, MB_ICONEXCLAMATION | MB_SETFOREGROUND);
            break;

          case AUTO:
            /* Setup mode is AUTO.  Show message, timeout, then auto close
             * all the windows associated with the process */
            ShowMessage(buf, TRUE);
            Delay(5);
            ShowMessage(buf, FALSE);
            break;

          default:
            break;
        }
      }

      /* can't kill the process for some unknown reason.  Stop the installation. */
      return(TRUE);
    }
    else if((killProcessTries > 1) &&
            (killProcessTries < MAX_KILL_PROCESS_RETRIES) &&
            (*processName != '\0'))
    {
      if(FindAndKillProcess(processName, KP_KILL_PROCESS))
      {
        ++killProcessTries;
        instanceOfFoundProcess = index--;
      }
      continue;
    }

    dwRv0 = GetPrivateProfileString(section, "Class Name",  "", className,  sizeof(className), szFileIniUninstall);
    dwRv1 = GetPrivateProfileString(section, "Window Name", "", windowName, sizeof(windowName), szFileIniUninstall);
    if((dwRv0 == 0L) && (dwRv1 == 0L) && (*processName == '\0'))
    {
      bContinue = FALSE;
    }
    else if((*className != '\0') || (*windowName != '\0'))
    {
      if(*className == '\0')
        szCN = NULL;
      else
        szCN = className;

      if(*windowName == '\0')
        szWN = NULL;
      else
        szWN = windowName;

      /* If an instance is found, call PreCheckInstance first.
       * PreCheckInstance will try to disable the browser's
       * QuickLaunch feature. If the browser was in QuickLaunch
       * mode without any windows open, PreCheckInstance would
       * shutdown the browser, thus a second call to FindAndKillProcess
       * is required to see if the process is still around. */
      if((hwndFW = FindWindow(szCN, szWN)) != NULL)
        PreCheckInstance(section, szFileIniUninstall);

      if((hwndFW = FindWindow(szCN, szWN)) != NULL)
      {
        GetCheckInstanceQuitMessage(section, message, sizeof(message));
        if(*message != '\0')
        {
          ShowMessageAndQuitProcess(hwndFW, message, msgWait, bCloseAllWindows, processName, CI_CLOSE_PROCESS);
          ++killProcessTries;
          instanceOfFoundProcess = index--;
        }
        else
        {
          /* No message to display.  Assume cancel because we can't allow user to continue */
          return(TRUE);
        }
      }
    }

    if((killProcessTries == 0) && (*processName != '\0'))
    {
      /* The first attempt used FindWindow(), but sometimes the browser can be
       * in a state where there's no window open and still not fully shutdown.
       * In this case, we need to check for the process itself and kill it. */
      if(FindAndKillProcess(processName, KP_DO_NOT_KILL_PROCESS))
      {
        GetCheckInstanceQuitMessage(section, message, sizeof(message));
        if(*message != '\0')
        {
          ShowMessageAndQuitProcess(hwndFW, message, msgWait, bCloseAllWindows, processName, CI_FORCE_QUIT_PROCESS);
          ++killProcessTries;
          instanceOfFoundProcess = index--;
        }
        else
        {
          /* No message to display.  Assume cancel because we can't allow user to continue */
          return(TRUE);
        }
      }
    }
  }

  return(FALSE);
}

BOOL GetFileVersion(LPSTR szFile, verBlock *vbVersion)
{
  UINT              uLen;
  UINT              dwLen;
  BOOL              bRv;
  DWORD             dwHandle;
  LPVOID            lpData;
  LPVOID            lpBuffer;
  VS_FIXEDFILEINFO  *lpBuffer2;

  vbVersion->ullMajor   = 0;
  vbVersion->ullMinor   = 0;
  vbVersion->ullRelease = 0;
  vbVersion->ullBuild   = 0;
  if(FileExists(szFile))
  {
    bRv    = TRUE;
    dwLen  = GetFileVersionInfoSize(szFile, &dwHandle);
    lpData = (LPVOID)malloc(sizeof(long)*dwLen);
    uLen   = 0;

    if(GetFileVersionInfo(szFile, dwHandle, dwLen, lpData) != 0)
    {
      if(VerQueryValue(lpData, "\\", &lpBuffer, &uLen) != 0)
      {
        lpBuffer2             = (VS_FIXEDFILEINFO *)lpBuffer;
        vbVersion->ullMajor   = HIWORD(lpBuffer2->dwFileVersionMS);
        vbVersion->ullMinor   = LOWORD(lpBuffer2->dwFileVersionMS);
        vbVersion->ullRelease = HIWORD(lpBuffer2->dwFileVersionLS);
        vbVersion->ullBuild   = LOWORD(lpBuffer2->dwFileVersionLS);
      }
    }
    free(lpData);
  }
  else
    /* File does not exist */
    bRv = FALSE;

  return(bRv);
}

void TranslateVersionStr(LPSTR szVersion, verBlock *vbVersion)
{
  LPSTR szNum1 = NULL;
  LPSTR szNum2 = NULL;
  LPSTR szNum3 = NULL;
  LPSTR szNum4 = NULL;

  szNum1 = strtok(szVersion, ".");
  szNum2 = strtok(NULL,      ".");
  szNum3 = strtok(NULL,      ".");
  szNum4 = strtok(NULL,      ".");

  vbVersion->ullMajor   = _atoi64(szNum1);
  vbVersion->ullMinor   = _atoi64(szNum2);
  vbVersion->ullRelease = _atoi64(szNum3);
  vbVersion->ullBuild   = _atoi64(szNum4);
}

int CompareVersion(verBlock vbVersionOld, verBlock vbVersionNew)
{
  if(vbVersionOld.ullMajor > vbVersionNew.ullMajor)
    return(4);
  else if(vbVersionOld.ullMajor < vbVersionNew.ullMajor)
    return(-4);

  if(vbVersionOld.ullMinor > vbVersionNew.ullMinor)
    return(3);
  else if(vbVersionOld.ullMinor < vbVersionNew.ullMinor)
    return(-3);

  if(vbVersionOld.ullRelease > vbVersionNew.ullRelease)
    return(2);
  else if(vbVersionOld.ullRelease < vbVersionNew.ullRelease)
    return(-2);

  if(vbVersionOld.ullBuild > vbVersionNew.ullBuild)
    return(1);
  else if(vbVersionOld.ullBuild < vbVersionNew.ullBuild)
    return(-1);

  /* the versions are all the same */
  return(0);
}

BOOL VerifyRestrictedAccess(void)
{
  char  szSubKey[MAX_BUF];
  char  szSubKeyToTest[] = "Software\\%s - Test Key";
  BOOL  bRv;
  DWORD dwDisp = 0;
  DWORD dwErr;
  HKEY  hkRv;

  wsprintf(szSubKey, szSubKeyToTest, ugUninstall.szCompanyName);
  dwErr = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                         szSubKey,
                         0,
                         NULL,
                         REG_OPTION_NON_VOLATILE,
                         KEY_WRITE,
                         NULL,
                         &hkRv,
                         &dwDisp);
  if(dwErr == ERROR_SUCCESS)
  {
    RegCloseKey(hkRv);
    switch(dwDisp)
    {
      case REG_CREATED_NEW_KEY:
        RegDeleteKey(HKEY_LOCAL_MACHINE, szSubKey);
        break;

      case REG_OPENED_EXISTING_KEY:
        break;
    }
    bRv = FALSE;
  }
  else
    bRv = TRUE;

  return(bRv);
}

BOOL CheckLegacy(HWND hDlg)
{
  char      szSection[MAX_BUF];
  char      szFilename[MAX_BUF];
  char      szMessage[MAX_BUF];
  char      szIndex[MAX_BUF];
  char      szVersionNew[MAX_BUF];
  char      szDecryptedFilePath[MAX_BUF];
  int       iIndex;
  BOOL      bContinue;
  DWORD     dwRv0;
  DWORD     dwRv1;
  verBlock  vbVersionNew;
  verBlock  vbVersionOld;

  bContinue = TRUE;
  iIndex    = -1;
  while(bContinue)
  {
    ZeroMemory(szFilename,      sizeof(szFilename));
    ZeroMemory(szVersionNew,    sizeof(szVersionNew));
    ZeroMemory(szMessage,       sizeof(szMessage));

    ++iIndex;
    itoa(iIndex, szIndex, 10);
    lstrcpy(szSection, "Legacy Check");
    lstrcat(szSection, szIndex);

    dwRv0 = GetPrivateProfileString(szSection, "Filename", "", szFilename, MAX_BUF, szFileIniUninstall);
    dwRv1 = GetPrivateProfileString(szSection, "Version", "", szVersionNew, MAX_BUF, szFileIniUninstall);
    if(dwRv0 == 0L)
    {
      bContinue = FALSE;
    }
    else if(*szFilename != '\0')
    {
      GetPrivateProfileString(szSection, "Message", "", szMessage, MAX_BUF, szFileIniUninstall);
      if(*szMessage == '\0')
        /* no message string input. so just continue with the next check */
        continue;

      DecryptString(szDecryptedFilePath, szFilename);
      if((dwRv1 == 0L) || (*szVersionNew == '\0'))
      {
        if(FileExists(szDecryptedFilePath))
        {
          char szMBWarningStr[MAX_BUF];

          if(!GetPrivateProfileString("Messages", "MB_WARNING_STR", "",
                                      szMBWarningStr, sizeof(szMBWarningStr), 
                                      szFileIniUninstall))
            lstrcpy(szMBWarningStr, "Warning");

          if(MessageBox(hDlg, szMessage, szMBWarningStr, MB_ICONWARNING | MB_YESNO) == IDYES)
            return(TRUE);
        }
        /* file does not exist, so it's okay.  Continue with the next check */
        continue;
      }

      if(GetFileVersion(szDecryptedFilePath, &vbVersionOld))
      {
        TranslateVersionStr(szVersionNew, &vbVersionNew);
        if(CompareVersion(vbVersionOld, vbVersionNew) < 0)
        {
          char szMBWarningStr[MAX_BUF];

          if(!GetPrivateProfileString("Messages", "MB_WARNING_STR", "",
                                      szMBWarningStr, sizeof(szMBWarningStr), 
                                      szFileIniUninstall))
            lstrcpy(szMBWarningStr, "Warning");

          if(MessageBox(hDlg, szMessage, szMBWarningStr, MB_ICONWARNING | MB_YESNO) == IDYES)
            return(TRUE);
        }
      }
    }
  }
  /* returning TRUE means the user wants to go back and choose a different destination path
   * returning FALSE means the user is ignoring the warning
   */
  return(FALSE);
}

// This function looks up the path for the application being uninstalled, NOT the client
//   app in a shared install environment.
HRESULT GetAppPath()
{
  char szTmpAppPath[MAX_BUF];
  char szKey[MAX_BUF];
  BOOL bRestrictedAccess;
  HKEY hkRoot;

  if(*ugUninstall.szUserAgent != '\0')
  {
    hkRoot = ugUninstall.hWrMainRoot;
    wsprintf(szKey, "%s\\%s\\Main", ugUninstall.szWrMainKey, ugUninstall.szUserAgent);
  }
  else
  {
    hkRoot = ugUninstall.hWrRoot;
    strcpy(szKey, ugUninstall.szWrKey);
  }

  bRestrictedAccess = VerifyRestrictedAccess();
  if(bRestrictedAccess)
    hkRoot = HKEY_CURRENT_USER;

  GetWinReg(hkRoot, szKey, "PathToExe", szTmpAppPath, sizeof(szTmpAppPath));
  if(FileExists(szTmpAppPath))
    lstrcpy(ugUninstall.szAppPath, szTmpAppPath);

  GetWinReg(hkRoot, szKey, "Install Directory", ugUninstall.szInstallPath, sizeof(ugUninstall.szInstallPath));

  return(0);
}

// For shared installs we maintain an app list, which is a glorified
//   refcount of the apps which are using this version of the 
//   shared app.  Processing the app list does two things:
//
//   1) Cleans up any registry entries for apps which are no longer
//      installed.  The app list includes a representative file for
//      each app.  If that file is not installed than the app is 
//      assumed to have been removed from the system.
//   2) Returns a count of the installed apps which depend upon this
//      shared app.  If this app is not shared, there will be no app
//      list so this will always return 0.
DWORD CleanupAppList()
{
  typedef struct siAppListStruct siAppList;
  struct siAppListStruct
  {
    char szAppID[MAX_BUF];
    siAppList *Next;
  };

  siAppList *siALHead;
  siAppList *siALPrev;
  siAppList *siALTmp;

  char      *szRv = NULL;
  char      szKey[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      szDefaultApp[MAX_BUF_TINY];
  BOOL      bFoundDefaultApp;
  HKEY      hkResult;
  DWORD     dwIndex;
  DWORD     dwBufSize;
  DWORD     dwTotalSubKeys;
  DWORD     dwTotalValues;
  DWORD     dwAppCount;
  FILETIME  ftLastWriteFileTime;

  GetPrivateProfileString("General", "Default AppID", "", szDefaultApp, MAX_BUF_TINY, szFileIniUninstall);
  wsprintf(szKey, "%s\\%s\\AppList", ugUninstall.szWrMainKey, ugUninstall.szUserAgent);
  if(RegOpenKeyEx(ugUninstall.hWrMainRoot, szKey, 0, KEY_READ, &hkResult) != ERROR_SUCCESS)
  {
    return(0);
  }

  siALHead = NULL;
  dwAppCount = 0;
  dwTotalSubKeys = 0;
  dwTotalValues  = 0;
  RegQueryInfoKey(hkResult, NULL, NULL, NULL, &dwTotalSubKeys, NULL, NULL, &dwTotalValues, NULL, NULL, NULL, NULL);

  for(dwIndex = 0; dwIndex < dwTotalSubKeys; dwIndex++)
  {
    dwBufSize = sizeof(szBuf);
    if(RegEnumKeyEx(hkResult, dwIndex, szBuf, &dwBufSize, NULL, NULL, NULL, &ftLastWriteFileTime) == ERROR_SUCCESS)
    {
      if((siALTmp = NS_GlobalAlloc(sizeof(struct siAppListStruct))) == NULL)
        exit(1);
      lstrcpy(siALTmp->szAppID, szBuf);
      siALTmp->Next = NULL;

      if(siALHead == NULL)
        siALHead = siALTmp; //New list, point the head at it.
      else
        siALPrev->Next = siALTmp;

      siALPrev = siALTmp;
    }
  }
  RegCloseKey(hkResult);

  siALTmp = siALHead;
  while(siALTmp != NULL)
  {
    if(lstrcmpi(siALTmp->szAppID, szDefaultApp) == 0)
      bFoundDefaultApp = TRUE;

    // ProcessAppItem returns the # of installations of the App
    dwAppCount = dwAppCount + ProcessAppItem(ugUninstall.hWrMainRoot, szKey, siALTmp->szAppID);

    siALPrev = siALTmp;
    siALTmp = siALTmp->Next;
    FreeMemory(&siALPrev);
  }

  if(dwAppCount == 0)
    RegDeleteKey(ugUninstall.hWrMainRoot, szKey);

  // If the Default App is not listed in AppList, then the shared app should not be in the 
  //   Windows Add/Remove Programs list either.
  if(!bFoundDefaultApp)
  {
    wsprintf(szKey, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s (%s)",ugUninstall.szProductName, ugUninstall.szUserAgent);
    RegDeleteKey(ugUninstall.hWrMainRoot, szKey);
  }

  return(dwAppCount);
}


// Removes the app item if it is the app identified with the /app command-line option
// If an app item is not installed this removes it from the app list.
// Returns the # of installations of the app item found in the AppList.
DWORD ProcessAppItem(HKEY hkRootKey, LPSTR szKeyAppList, LPSTR szAppID)
{
  DWORD dwIndex = 1;
  const DWORD dwUpperLimit = 100;
  BOOL  bDefaultApp;
  char szBuf[MAX_BUF];
  char szKey[MAX_BUF];
  char szName[MAX_BUF];
  char szUninstKey[MAX_BUF];

  GetPrivateProfileString("General", "Default AppID", "", szBuf, sizeof(szBuf), szFileIniUninstall);
  bDefaultApp = (lstrcmpi(szAppID, szBuf) == 0);

  wsprintf(szKey, "%s\\%s", szKeyAppList, szAppID);

  if(lstrcmpi(szAppID, ugUninstall.szClientAppID) == 0) // This is the app that the user said 
  {                                                     // on the command-line to uninstall.
    if((ugUninstall.szClientAppPath[0] == '\0') || (bDefaultApp)) // If we didn't get an /app_path or this
    {                                                             // is the default app, just remove it.
      RegDeleteKey(hkRootKey, szKey);
      if(bDefaultApp)
      {
        wsprintf(szKey, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GRE (%s)", ugUninstall.szUserAgent);
        RegDeleteKey(hkRootKey, szKey);
      }

      return 0;
    }

    wsprintf(szName, "PathToExe%02d", dwIndex);
    while(WinRegNameExists(hkRootKey, szKey, szName) && // Since we got an /app_path we need to cycle
         (dwIndex < dwUpperLimit))                      // through the list looking for that instance.
    {
      GetWinReg(hkRootKey, szKey, szName, szBuf, sizeof(szBuf));
      if((lstrcmpi(szBuf, ugUninstall.szClientAppPath) == 0) || (!FileExists(szBuf)))
      {
        // RemovePathToExeXX() will delete the dwIndex Name and reorder
        // PathToExeXX so there are no gaps.
        RemovePathToExeXX(hkRootKey, szKey, dwIndex, dwUpperLimit);
      }
      else
        wsprintf(szName, "PathToExe%02d", ++dwIndex);
    }

    if(--dwIndex == 0)
      RegDeleteKey(hkRootKey, szKey);

    return dwIndex;
  }

  if(bDefaultApp) // The Default App does not have an installed file registered.  However, an entry in 
  {               // the Windows Add/Remove products list indicates that the Default App is installed
                  // and needs to be counted.
    wsprintf(szUninstKey, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s (%s)",ugUninstall.szProductName, ugUninstall.szUserAgent);
    GetWinReg(hkRootKey, szUninstKey, "UninstallString", szBuf, sizeof(szBuf));

    if(szBuf[0] != '\0')
      return 1;
  }

  dwIndex = 1;
  wsprintf(szName, "PathToExe%02d", dwIndex);
  while(WinRegNameExists(hkRootKey, szKey, szName) && // Count the entries which can be verified by the
       (dwIndex < dwUpperLimit))                      // existence of the file pointed to by PathToExeXX
  {
    GetWinReg(hkRootKey, szKey, szName, szBuf, sizeof(szBuf));
    if(!FileExists(szBuf))    
    {
      // RemovePathToExeXX() will delete the dwIndex Name and reorder
      // PathToExeXX so there are no gaps.
      RemovePathToExeXX(hkRootKey, szKey, dwIndex, dwUpperLimit);
    }
    else
      wsprintf(szName, "PathToExe%02d", ++dwIndex);
  }

  if(--dwIndex == 0)
    RegDeleteKey(hkRootKey, szKey);

  return dwIndex;
}

void RemovePathToExeXX(HKEY hkRootKey, LPSTR szKey, DWORD dwIndex, const DWORD dwUpperLimit)
{
  char szBuf[MAX_BUF_TINY];
  char szName[MAX_BUF_TINY];
  char szNextName[MAX_BUF_TINY];

  wsprintf(szName,     "PathToExe%02d", dwIndex++);
  wsprintf(szNextName, "PathToExe%02d", dwIndex);
  while(WinRegNameExists(hkRootKey, szKey, szNextName) && (dwIndex < dwUpperLimit))
  {
    GetWinReg(hkRootKey, szKey, szNextName, szBuf, sizeof(szBuf));
    SetWinReg(hkRootKey, szKey, szName, REG_SZ, szBuf, lstrlen(szBuf));
    lstrcpy(szName, szNextName);
    wsprintf(szNextName, "PathToExe%02d", ++dwIndex);
  }        

  DeleteWinRegValue(hkRootKey, szKey, szName);
}

/* Function: ReplacePrivateProfileStrCR()
 *
 *       in: LPSTR aInputOutputStr: In/out string to containing "\\n" to replace.
 *           
 *  purpose: To parse for and replace "\\n" string with "\n".  Strings stored
 *           in .ini files cannot contain "\n" because it's a key delimiter.
 *           To work around this limination, "\\n" chars can be used to
 *           represent '\n'.  This function will look for "\\n" and replace
 *           them with a true "\n".
 *           If it encounters a string of "\\\\n" (which looks like '\\n'),
 *           then this function will strip out the extra '\\' and just show
 *           "\\n";
 */
void ReplacePrivateProfileStrCR(LPSTR aInputOutputStr)
{
  LPSTR pSearch          = aInputOutputStr;
  LPSTR pSearchEnd       = aInputOutputStr + lstrlen(aInputOutputStr);
  LPSTR pPreviousSearch  = NULL;

  while (pSearch < pSearchEnd)
  {
    if (('\\' == *pSearch) || ('n' == *pSearch))
    {
      // found a '\\' or 'n'.  check to see if  the prefivous char is also a '\\'.
      if (pPreviousSearch && ('\\' == *pPreviousSearch))
      {
        if ('n' == *pSearch)
          *pSearch = '\n';

        memmove(pPreviousSearch, pSearch, pSearchEnd-pSearch+1);

        // our string is shorter now ...
        pSearchEnd -= pSearch - pPreviousSearch;
      }
    }

    pPreviousSearch = pSearch;
    pSearch = CharNext(pSearch);
  }
}

/* Function: IsPathWithinWindir()
 *       in: char *aTargetPath
 *      out: returns a BOOL type indicating whether the install path chosen
 *           by the user is within the %windir% or not.
 *  purpose: To see if aTargetPath is within the %windir% path.
 */
BOOL IsPathWithinWindir(char *aTargetPath)
{
  char    windir[MAX_PATH];
  char    targetPath[MAX_PATH];
  HRESULT rv;

  assert(aTargetPath);

  rv = GetWindowsDirectory(windir, sizeof(windir));
  assert(rv);
  MozCopyStr(aTargetPath, targetPath, sizeof(targetPath));
  RemoveBackSlash(targetPath);
  CharUpperBuff(targetPath, sizeof(targetPath));
  RemoveBackSlash(windir);
  CharUpperBuff(windir, sizeof(windir));

  if(strstr(targetPath, windir))
    return(TRUE);

  return(FALSE);
}

/* Function: VerifyAndDeleteInstallationFolder()
 *       in: none
 *      out: none
 *  purpose: To verify that the installation folder has been completely
 *           deleted, else prompt the user if they would like to delete
 *           the folder and it's left over contents.
 *           It will also check to make sure that the installation
 *           folder is not within the %windir% folder.  If it is,
 *           it will warn the user and not perform the deletion of the
 *           installation folder.
 */
void VerifyAndDeleteInstallationFolder()
{
  if(FileExists(ugUninstall.szInstallPath))
  {
    char buf[MAX_BUF];
    char msg[MAX_BUF];
    char msgBoxTitle[MAX_BUF];
    char installationPath[MAX_BUF];

    MozCopyStr(ugUninstall.szInstallPath, installationPath, sizeof(installationPath));
    RemoveBackSlash(installationPath);

    if(ugUninstall.mode == NORMAL)
    {
      GetPrivateProfileString("Messages", "MB_ATTENTION_STR", "", msgBoxTitle, sizeof(msgBoxTitle), szFileIniUninstall);
      GetPrivateProfileString("Messages", "MSG_DELETE_INSTALLATION_PATH", "", buf, sizeof(buf), szFileIniUninstall);
      ReplacePrivateProfileStrCR(buf);
      _snprintf(msg, sizeof(msg), buf, installationPath);
      msg[sizeof(msg) - 1] = '\0';

      /* Prompt user on if they want the completely remove the
       * installation folder */
      if(MessageBox(hWndMain, msg, msgBoxTitle, MB_ICONQUESTION | MB_YESNO) == IDYES)
      {
        if(IsPathWithinWindir(installationPath))
        {
          GetPrivateProfileString("Messages", "MSG_INSTALLATION_PATH_WITHIN_WINDIR",
              "", msg, sizeof(msg), szFileIniUninstall);
          MessageBox(hWndMain, msg, NULL, MB_ICONEXCLAMATION);
          return;
        }

        /* Remove the installation folder here */
        AppendBackSlash(installationPath, sizeof(installationPath));
        DirectoryRemove(installationPath, TRUE);
      }
    }
    else
    {
      /* If uninstall is running in !NORMAL mode, assume user wants to
       * completely delete the installation folder */
      AppendBackSlash(installationPath, sizeof(installationPath));
      DirectoryRemove(installationPath, TRUE);
    }
  }
}

HRESULT GetUninstallLogPath()
{
  char szBuf[MAX_BUF];
  char szLogFolder[MAX_BUF];
  char szKey[MAX_BUF];
  char szWindowsUninstallKey[MAX_BUF];
  char szErrorMsg[MAX_BUF];
  char szEUninstallLogFolder[MAX_BUF];
  char szRootKey[MAX_BUF];
  BOOL bRestrictedAccess;
  HKEY hkRoot;

  if(*ugUninstall.szUserAgent != '\0')
  {
    hkRoot = ugUninstall.hWrMainRoot;
    lstrcpy(szKey, ugUninstall.szWrMainKey);
    AppendBackSlash(szKey, sizeof(szKey));
    lstrcat(szKey, ugUninstall.szUserAgent);
    AppendBackSlash(szKey, sizeof(szKey));
    lstrcat(szKey, "Uninstall");
  }
  else
  {
    hkRoot = ugUninstall.hWrRoot;
    strcpy(szKey, ugUninstall.szWrKey);
  }

  bRestrictedAccess = VerifyRestrictedAccess();
  if(bRestrictedAccess)
    hkRoot = HKEY_CURRENT_USER;

  GetWinReg(hkRoot, szKey, "Uninstall Log Folder",   szLogFolder, sizeof(szLogFolder));
  GetWinReg(hkRoot, szKey, "Description",            ugUninstall.szUninstallKeyDescription, MAX_BUF);

  if(!bRestrictedAccess)
  {
    lstrcpy(szWindowsUninstallKey, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\");
    lstrcat(szWindowsUninstallKey, ugUninstall.szUninstallKeyDescription);
    GetWinReg(HKEY_LOCAL_MACHINE, szWindowsUninstallKey, "DisplayName", ugUninstall.szDescription, MAX_BUF);
  }

  /* if the DisplayName was not found in the windows registry,
   * use the description read in from config.ini file */
  if(*ugUninstall.szDescription == '\0')
    lstrcpy(ugUninstall.szDescription, ugUninstall.szUninstallKeyDescription);

  if(FileExists(szLogFolder) == FALSE)
  {
    if(GetPrivateProfileString("Messages", "ERROR_UNINSTALL_LOG_FOLDER", "",
                               szEUninstallLogFolder, sizeof(szEUninstallLogFolder), 
                               szFileIniUninstall))
    {
      lstrcpy(szBuf, "\n\n    ");

      if(*szLogFolder == '\0')
      {
        GetStringRootKey(hkRoot, szRootKey, sizeof(szRootKey));
        lstrcat(szBuf, szRootKey);
        lstrcat(szBuf, "\\");
        lstrcat(szBuf, szKey);
        lstrcat(szBuf, "\\Uninstall Log Folder");
      }
      else
        lstrcat(szBuf, szLogFolder);

      lstrcat(szBuf, "\n");
      wsprintf(szErrorMsg, szEUninstallLogFolder, szBuf);
      PrintError(szErrorMsg, ERROR_CODE_SHOW);
    }

    return(1);
  }
  lstrcpy(ugUninstall.szLogPath, szLogFolder);

  return(0);
}

HRESULT ParseUninstallIni()
{
  char szBuf[MAX_BUF];
  char szKeyCrypted[MAX_BUF];
  char szShowDialog[MAX_BUF];
  LOGFONT lf;
  char fontName[MAX_BUF];
  char fontSize[MAX_BUF];
  char charSet[MAX_BUF];

  lstrcpy(ugUninstall.szLogFilename, FILE_LOG_INSTALL);

  GetPrivateProfileString("General", "Shared Install", "", szBuf, MAX_BUF, szFileIniUninstall);
  ugUninstall.bSharedInst = (lstrcmpi(szBuf, "TRUE") == 0);

  /* get install Mode information */
  GetPrivateProfileString("General", "Run Mode", "", szBuf, MAX_BUF, szFileIniUninstall);
  SetUninstallRunMode(szBuf);

  if((ugUninstall.mode != SHOWICONS) && (ugUninstall.mode != HIDEICONS) && (ugUninstall.mode != SETDEFAULT))
    if(CheckInstances())
      return(1);
  if(InitDlgUninstall(&diUninstall))
    return(1);
 
  /* get product name description */
  GetPrivateProfileString("General", "Company Name", "", ugUninstall.szCompanyName, MAX_BUF, szFileIniUninstall);
  GetPrivateProfileString("General", "Product Name", "", ugUninstall.szProductName, MAX_BUF, szFileIniUninstall);
  GetPrivateProfileString("General", "Root Key",     "", szBuf, MAX_BUF, szFileIniUninstall);
  ugUninstall.hWrRoot = ParseRootKey(szBuf);

  GetPrivateProfileString("General", "Key",          "", szKeyCrypted, MAX_BUF, szFileIniUninstall);
  GetPrivateProfileString("General", "Decrypt Key",  "", szBuf, MAX_BUF, szFileIniUninstall);
  if(lstrcmpi(szBuf, "TRUE") == 0)
  {
    DecryptString(ugUninstall.szWrKey, szKeyCrypted);
  }
  else
    strcpy(ugUninstall.szWrKey, szKeyCrypted);

  RemoveBackSlash(ugUninstall.szWrKey);

  GetPrivateProfileString("General", "Main Root Key",    "", szBuf, MAX_BUF, szFileIniUninstall);
  ugUninstall.hWrMainRoot = ParseRootKey(szBuf);

  GetPrivateProfileString("General", "Main Key",         "", szKeyCrypted, MAX_BUF, szFileIniUninstall);
  GetPrivateProfileString("General", "Decrypt Main Key", "", szBuf, MAX_BUF, szFileIniUninstall);

  // If szWrMainKey is not null then it was set on the command-line and that is
  //    what we want to use.
  if(*ugUninstall.szWrMainKey == '\0') 
  {
    if(lstrcmpi(szBuf, "TRUE") == 0)
    {
      DecryptString(ugUninstall.szWrMainKey, szKeyCrypted);
    }
    else
      strcpy(ugUninstall.szWrMainKey, szKeyCrypted);
  }

  RemoveBackSlash(ugUninstall.szWrMainKey);

  GetPrivateProfileString("General", "Uninstall Filename", "", ugUninstall.szUninstallFilename, MAX_BUF, szFileIniUninstall);


  /* Uninstall dialog */
  GetPrivateProfileString("Dialog Uninstall",       "Show Dialog",  "", szShowDialog,                 MAX_BUF, szFileIniUninstall);
  GetPrivateProfileString("Dialog Uninstall",       "Title",        "", diUninstall.szTitle,          MAX_BUF, szFileIniUninstall);
  GetPrivateProfileString("Dialog Uninstall",       "Message0",     "", diUninstall.szMessage0,       MAX_BUF, szFileIniUninstall);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diUninstall.bShowDialog = TRUE;

  switch(ugUninstall.mode)
  {
    case AUTO:
    case SILENT:
    case SHOWICONS:
    case HIDEICONS:
    case SETDEFAULT:
      gdwWhatToDo             = WTD_NO_TO_ALL;
      diUninstall.bShowDialog = FALSE;
      break;
  }

  /* get defined font */
  GetPrivateProfileString("Dialog Uninstall", "FONTNAME", "", fontName, MAX_BUF, szFileIniUninstall);
  GetPrivateProfileString("Dialog Uninstall", "FONTSIZE", "", fontSize, MAX_BUF, szFileIniUninstall);
  GetPrivateProfileString("Dialog Uninstall", "CHARSET", "", charSet, MAX_BUF, szFileIniUninstall);
  memset(&lf, 0, sizeof(lf));
  strcpy(lf.lfFaceName, fontName);
  lf.lfHeight = -MulDiv(atoi(fontSize), GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);
  lf.lfCharSet = atoi(charSet);
  ugUninstall.definedFont = CreateFontIndirect( &lf ); 

  GetAppPath();


  if(ugUninstall.bSharedInst)
  {
    // Shared installations require a user agent.  Without one there is no way to know 
    //   what version of GRE to clean up so we can't do anything--including CleanupAppList()
    //   so don't change the order of the if statement.  
    //   (We should add UI warning the user when a required UserAgent is not supplied.)
    // CleanupAppList() returns the number of installations of apps dependant on this shared app.
    if((ugUninstall.szUserAgent[0] == '\0') || (CleanupAppList() != 0))
      ugUninstall.bUninstallFiles = FALSE;
  }

  return(GetUninstallLogPath());
}

void GetWinReg(HKEY hkRootKey, LPSTR szKey, LPSTR szName, LPSTR szReturnValue, DWORD dwReturnValueSize)
{
  HKEY  hkResult;
  DWORD dwErr;
  DWORD dwSize;
  char  szBuf[MAX_BUF];

  ZeroMemory(szBuf, sizeof(szBuf));
  ZeroMemory(szReturnValue, dwReturnValueSize);

  if((dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult)) == ERROR_SUCCESS)
  {
    dwSize = sizeof(szBuf);
    dwErr = RegQueryValueEx(hkResult, szName, 0, NULL, szBuf, &dwSize);

    if((*szBuf != '\0') && (dwErr == ERROR_SUCCESS))
      ExpandEnvironmentStrings(szBuf, szReturnValue, dwReturnValueSize);
    else
      *szReturnValue = '\0';

    RegCloseKey(hkResult);
  }
}

void SetWinReg(HKEY hkRootKey, LPSTR szKey, LPSTR szName, DWORD dwType, LPSTR szData, DWORD dwSize)
{
  HKEY    hkResult;
  DWORD   dwErr;
  DWORD   dwDisp;
  char    szBuf[MAX_BUF];

  memset(szBuf, '\0', MAX_BUF);

  dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_WRITE, &hkResult);
  if(dwErr != ERROR_SUCCESS)
    dwErr = RegCreateKeyEx(hkRootKey, szKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkResult, &dwDisp);

  if(dwErr == ERROR_SUCCESS)
  {
    dwErr = RegSetValueEx(hkResult, szName, 0, dwType, szData, dwSize);

    RegCloseKey(hkResult);
  }
}

void SetWinRegNumValue(HKEY hkRootKey, LPSTR szKey, LPSTR szName, DWORD dwData)
{
  HKEY    hkResult;
  DWORD   dwErr;
  DWORD   dwDisp;
  DWORD   dwVal;
  DWORD   dwValSize;

  dwVal = dwData;
  dwValSize = sizeof(DWORD);

  dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_WRITE, &hkResult);
  if(dwErr != ERROR_SUCCESS)
    dwErr = RegCreateKeyEx(hkRootKey, szKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkResult, &dwDisp);

  if(dwErr == ERROR_SUCCESS)
  {
    dwErr = RegSetValueEx(hkResult, szName, 0, REG_DWORD, (LPBYTE)&dwVal, dwValSize);

    RegCloseKey(hkResult);
  }
}

HRESULT DecryptVariable(LPSTR szVariable, DWORD dwVariableSize)
{
  char szBuf[MAX_BUF];
  char szKey[MAX_BUF];
  char szName[MAX_BUF];
  char szValue[MAX_BUF];

  /* zero out the memory allocations */
  ZeroMemory(szBuf,       sizeof(szBuf));
  ZeroMemory(szKey,       sizeof(szKey));
  ZeroMemory(szName,      sizeof(szName));
  ZeroMemory(szValue,     sizeof(szValue));

  if(lstrcmpi(szVariable, "PROGRAMFILESDIR") == 0)
  {
    /* parse for the "c:\Program Files" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "ProgramFilesDir", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "COMMONFILESDIR") == 0)
  {
    /* parse for the "c:\Program Files\Common Files" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "CommonFilesDir", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "MEDIAPATH") == 0)
  {
    /* parse for the "c:\Winnt40\Media" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "MediaPath", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "CONFIGPATH") == 0)
  {
    /* parse for the "c:\Windows\Config" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "ConfigPath", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "DEVICEPATH") == 0)
  {
    /* parse for the "c:\Winnt40\INF" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "DevicePath", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "COMMON_STARTUP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs\\Startup" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Startup", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Startup", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "COMMON_PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Programs", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Programs", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "COMMON_STARTMENU") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Start Menu", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Start Menu", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "COMMON_DESKTOP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Desktop" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Desktop", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Desktop", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "PERSONAL_STARTUP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu\Programs\Startup" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Startup", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu\Programs" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Programs", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_STARTMENU") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Start Menu", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_DESKTOP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Desktop" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Desktop", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_APPDATA") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Application Data" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "AppData", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_CACHE") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Temporary Internet Files" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Cache", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_COOKIES") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Cookies" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Cookies", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_FAVORITES") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Favorites" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Favorites", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_FONTS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Fonts" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Fonts", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_HISTORY") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\History" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "History", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_NETHOOD") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\NetHood" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "NetHood", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_PERSONAL") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Personal" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Personal", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_PRINTHOOD") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\PrintHood" directory */
    if(ulOSType & OS_NT)
    {
      GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "PrintHood", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "PERSONAL_RECENT") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Recent" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Recent", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_SENDTO") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\SendTo" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "SendTo", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_TEMPLATES") == 0)
  {
    /* parse for the "C:\WINNT40\ShellNew" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Templates", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "WIZTEMP") == 0)
  {
    /* parse for the "c:\Temp" path */
    lstrcpy(szVariable, szTempDir);
    if(szVariable[strlen(szVariable) - 1] == '\\')
      szVariable[strlen(szVariable) - 1] = '\0';
  }
  else if(lstrcmpi(szVariable, "TEMP") == 0)
  {
    /* parse for the "c:\Temp" path */
    lstrcpy(szVariable, szOSTempDir);
    if(szVariable[strlen(szVariable) - 1] == '\\')
      szVariable[strlen(szVariable) - 1] = '\0';
  }
  else if(lstrcmpi(szVariable, "WINDISK") == 0)
  {
    /* Locate the drive that Windows is installed on, and only use the drive letter and the ':' character (C:). */
    if(GetWindowsDirectory(szBuf, MAX_BUF) == 0)
    {
      char szEGetWinDirFailed[MAX_BUF];

      if(GetPrivateProfileString("Messages", "ERROR_GET_WINDOWS_DIRECTORY_FAILED", "",
                                 szEGetWinDirFailed, sizeof(szEGetWinDirFailed), 
                                 szFileIniUninstall))
        PrintError(szEGetWinDirFailed, ERROR_CODE_SHOW);

      exit(1);
    }
    else
    {
      /* Copy the first 2 characters from the path..        */
      /* This is the drive letter and the ':' character for */
      /* where Windows is installed at.                     */
      memset(szVariable, '\0', MAX_BUF);
      szVariable[0] = szBuf[0];
      szVariable[1] = szBuf[1];
    }
  }
  else if(lstrcmpi(szVariable, "WINDIR") == 0)
  {
    /* Locate the "c:\Windows" directory */
    if(GetWindowsDirectory(szVariable, dwVariableSize) == 0)
    {
      char szEGetWinDirFailed[MAX_BUF];

      if(GetPrivateProfileString("Messages", "ERROR_GET_WINDOWS_DIRECTORY_FAILED", "",
                                 szEGetWinDirFailed, sizeof(szEGetWinDirFailed), 
                                 szFileIniUninstall))
        PrintError(szEGetWinDirFailed, ERROR_CODE_SHOW);
      exit(1);
    }
  }
  else if(lstrcmpi(szVariable, "WINSYSDIR") == 0)
  {
    /* Locate the "c:\Windows\System" (for Win95/Win98) or "c:\Windows\System32" (for NT) directory */
    if(GetSystemDirectory(szVariable, dwVariableSize) == 0)
    {
      char szEGetSysDirFailed[MAX_BUF];

      if(GetPrivateProfileString("Messages", "ERROR_GET_SYSTEM_DIRECTORY_FAILED", "",
                                 szEGetSysDirFailed, sizeof(szEGetSysDirFailed), 
                                 szFileIniUninstall))
        PrintError(szEGetSysDirFailed, ERROR_CODE_SHOW);

      exit(1);
    }
  }
  else if(lstrcmpi(szVariable, "JRE BIN PATH") == 0)
  {
    /* Locate the "c:\Program Files\JavaSoft\JRE\1.3\Bin" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\javaw.Exe", NULL, szVariable, dwVariableSize);
    if(*szVariable == '\0')
      return(FALSE);
    else
    {
      ParsePath(szVariable, szBuf, MAX_BUF, PP_PATH_ONLY);
      lstrcpy(szVariable, szBuf);
    }
  }
  else if(lstrcmpi(szVariable, "JRE PATH") == 0)
  {
    /* Locate the "c:\Program Files\JavaSoft\JRE\1.3" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\JavaSoft\\Java Plug-in\\1.3", "JavaHome", szVariable, dwVariableSize);
    if(*szVariable == '\0')
      return(FALSE);
  }
  else if(lstrcmpi(szVariable, "UNINSTALL STARTUP PATH") == 0)
  {
    lstrcpy(szVariable, szUninstallDir);
  }
  else if(lstrcmpi(szVariable, "Product CurrentVersion") == 0)
  {
    char szKey[MAX_BUF];

    lstrcpy(szKey, "Software\\");
    lstrcat(szKey, ugUninstall.szCompanyName);
    lstrcat(szKey, "\\");
    lstrcat(szKey, ugUninstall.szProductName);

    /* parse for the current Netscape WinReg key */
    GetWinReg(HKEY_LOCAL_MACHINE, szKey, "CurrentVersion", szBuf, sizeof(szBuf));

    if(*szBuf == '\0')
      return(FALSE);

    wsprintf(szVariable, "Software\\%s\\%s\\%s", ugUninstall.szCompanyName, ugUninstall.szProductName, szBuf);
  }
  else if(lstrcmpi(szVariable, "Product WinRegKey") == 0)
  {
    char szKey[MAX_BUF];

    lstrcpy(szKey, "Software\\");
    lstrcat(szKey, ugUninstall.szCompanyName);
    lstrcat(szKey, "\\");
    lstrcat(szKey, ugUninstall.szProductName);

    wsprintf(szVariable, "Software\\%s\\%s", ugUninstall.szCompanyName, ugUninstall.szProductName);
  }
  else
    return(FALSE);

  return(TRUE);
}

HRESULT DecryptString(LPSTR szOutputStr, LPSTR szInputStr)
{
  DWORD dwLenInputStr;
  DWORD dwCounter;
  DWORD dwVar;
  DWORD dwPrepend;
  char  szBuf[MAX_BUF];
  char  szVariable[MAX_BUF];
  char  szPrepend[MAX_BUF];
  char  szAppend[MAX_BUF];
  char  szResultStr[MAX_BUF];
  BOOL  bFoundVar;
  BOOL  bBeginParse;
  BOOL  bDecrypted;

  /* zero out the memory addresses */
  memset(szBuf,       '\0', MAX_BUF);
  memset(szVariable,  '\0', MAX_BUF);
  memset(szPrepend,   '\0', MAX_BUF);
  memset(szAppend,    '\0', MAX_BUF);
  memset(szResultStr, '\0', MAX_BUF);

  lstrcpy(szPrepend, szInputStr);
  dwLenInputStr = lstrlen(szInputStr);
  bBeginParse   = FALSE;
  bFoundVar     = FALSE;

  for(dwCounter = 0; dwCounter < dwLenInputStr; dwCounter++)
  {
    if((szInputStr[dwCounter] == ']') && bBeginParse)
      break;

    if(bBeginParse)
      szVariable[dwVar++] = szInputStr[dwCounter];

    if((szInputStr[dwCounter] == '[') && !bBeginParse)
    {
      dwVar        = 0;
      dwPrepend    = dwCounter;
      bBeginParse  = TRUE;
    }
  }

  if(dwCounter == dwLenInputStr)
    /* did not find anything to expand. */
    dwCounter = 0;
  else
  {
    bFoundVar = TRUE;
    ++dwCounter;
  }

  if(bFoundVar)
  {
    lstrcpy(szAppend, &szInputStr[dwCounter]);

    szPrepend[dwPrepend] = '\0';

    bDecrypted = DecryptVariable(szVariable, MAX_BUF);
    if(!bDecrypted)
    {
      /* Variable was not able to be decrypted. */
      /* Leave the variable as it was read in by adding the '[' and ']' */
      /* characters back to the variable. */
      lstrcpy(szBuf, "[");
      lstrcat(szBuf, szVariable);
      lstrcat(szBuf, "]");
      lstrcpy(szVariable, szBuf);
    }

    lstrcpy(szOutputStr, szPrepend);
    lstrcat(szOutputStr, szVariable);
    lstrcat(szOutputStr, szAppend);

    if(bDecrypted)
    {
      DecryptString(szResultStr, szOutputStr);
      lstrcpy(szOutputStr, szResultStr);
    }
  }
  else
    lstrcpy(szOutputStr, szInputStr);

  return(TRUE);
}

HRESULT FileExists(LPSTR szFile)
{
  DWORD rv;

  if((rv = GetFileAttributes(szFile)) == -1)
  {
    return(FALSE);
  }
  else
  {
    return(rv);
  }
}

BOOL WinRegNameExists(HKEY hkRootKey, LPSTR szKey, LPSTR szName)
{
  HKEY  hkResult;
  DWORD dwErr;
  DWORD dwSize;
  char  szBuf[MAX_BUF];
  BOOL  bNameExists = FALSE;

  szBuf[0] = '\0';
  if((dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult)) == ERROR_SUCCESS)
  {
    dwSize = sizeof(szBuf);
    dwErr  = RegQueryValueEx(hkResult, szName, 0, NULL, szBuf, &dwSize);

    if((*szBuf != '\0') && (dwErr == ERROR_SUCCESS))
      bNameExists = TRUE;

    RegCloseKey(hkResult);
  }

  return(bNameExists);
}

void DeleteWinRegValue(HKEY hkRootKey, LPSTR szKey, LPSTR szName)
{
  HKEY    hkResult;
  DWORD   dwErr;

  dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_WRITE, &hkResult);
  if(dwErr == ERROR_SUCCESS)
  {
    if(*szName == '\0')
      dwErr = RegDeleteValue(hkResult, NULL);
    else
      dwErr = RegDeleteValue(hkResult, szName);

    RegCloseKey(hkResult);
  }
}

void DeInitialize()
{
  DeInitDlgUninstall(&diUninstall);

  FreeMemory(&szTempDir);
  FreeMemory(&szOSTempDir);
  FreeMemory(&szUninstallDir);
  FreeMemory(&szEGlobalAlloc);
  FreeMemory(&szEDllLoad);
  FreeMemory(&szEStringLoad);
  FreeMemory(&szEStringNull);
  FreeMemory(&szFileIniUninstall);
  FreeMemory(&szFileIniDefaultsInfo);
}

