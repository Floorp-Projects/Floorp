/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Sean Su <ssu@netscape.com>
 */

#include "extern.h"
#include "extra.h"
#include "parser.h"
#include "dialogs.h"
#include "ifuncns.h"
#include <winver.h>

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

  if((ugUninstall.dwMode != SILENT) && (ugUninstall.dwMode != AUTO))
  {
    MessageBox(hWndMain, szErrorString, NULL, MB_ICONEXCLAMATION);
  }
  else if(ugUninstall.dwMode == AUTO)
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
  HWND hwndFW;

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

  lstrcpy(szClassName, CLASS_NAME);

  /* Allow only one instance of setup to run.
   * Detect a previous instance of setup, bring it to the 
   * foreground, and quit current instance */
  if((hwndFW = FindWindow(szClassName, szClassName)) != NULL)
  {
    ShowWindow(hwndFW, SW_RESTORE);
    SetForegroundWindow(hwndFW);
    return(1);
  }

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

/* Function to locate the first non space character in a string,
 * and return a pointer to it. */
LPSTR GetFirstNonSpace(LPSTR lpszString)
{
  int   i;
  int   iStrLength;

  iStrLength = lstrlen(lpszString);

  for(i = 0; i < iStrLength; i++)
  {
    if(!isspace(lpszString[i]))
      return(&lpszString[i]);
  }

  return(NULL);
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
  if(lstrcmpi(szMode, "NORMAL") == 0)
    ugUninstall.dwMode = NORMAL;
  if(lstrcmpi(szMode, "AUTO") == 0)
    ugUninstall.dwMode = AUTO;
  if(lstrcmpi(szMode, "SILENT") == 0)
    ugUninstall.dwMode = SILENT;
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
    bFound        = TRUE;
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
  DWORD         dwVersion;
  DWORD         dwWindowsMajorVersion;
  DWORD         dwWindowsMinorVersion;
  DWORD         dwWindowsVersion;
  BOOL          bIsWin95Debute;
  char          szESetupRequirement[MAX_BUF];

  ulOSType        = 0;
  dwVersion       = GetVersion();
  bIsWin95Debute  = IsWin95Debute();

  // Get major and minor version numbers of Windows
  dwWindowsMajorVersion =  (DWORD)(LOBYTE(LOWORD(dwVersion)));
  dwWindowsMinorVersion =  (DWORD)(HIBYTE(LOWORD(dwVersion)));
  dwWindowsVersion      =  (DWORD)(HIWORD(dwVersion));

  // Get build numbers for Windows NT or Win95/Win98
  if(dwVersion < 0x80000000) // Windows NT
  {
    ulOSType |= OS_NT;
    if(dwWindowsMajorVersion == 3)
      ulOSType |= OS_NT3;
    else if(dwWindowsMajorVersion == 4)
      ulOSType |= OS_NT4;
    else
      ulOSType |= OS_NT5;
  }
  else if(dwWindowsMajorVersion == 4)
  {
    ulOSType |= OS_WIN9x;
    if(dwWindowsMinorVersion == 0)
    {
      ulOSType |= OS_WIN95;

      if(bIsWin95Debute)
        ulOSType |= OS_WIN95_DEBUTE;
    }
    else
      ulOSType |= OS_WIN98;
  }
  else
  {
    if(GetPrivateProfileString("Messages", "ERROR_SETUP_REQUIREMENT", "",
                               szESetupRequirement, sizeof(szESetupRequirement), 
                               szFileIniUninstall))
      PrintError(szESetupRequirement, ERROR_CODE_HIDE);

    exit(1);
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
  ugUninstall.dwMode = NORMAL;

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
  if((ugUninstall.szWrMainKey               = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szDescription             = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szUninstallKeyDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((ugUninstall.szUninstallFilename       = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitUninstallGeneral()
{
  FreeMemory(&(ugUninstall.szLogPath));
  FreeMemory(&(ugUninstall.szLogFilename));
  FreeMemory(&(ugUninstall.szDescription));
  FreeMemory(&(ugUninstall.szUninstallKeyDescription));
  FreeMemory(&(ugUninstall.szUninstallFilename));
  FreeMemory(&(ugUninstall.szUserAgent));
  FreeMemory(&(ugUninstall.szWrKey));
  FreeMemory(&(ugUninstall.szCompanyName));
  FreeMemory(&(ugUninstall.szProductName));
  FreeMemory(&(ugUninstall.szWrMainKey));
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

void ParseCommandLine(LPSTR lpszCmdLine)
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

    ++i;
  }
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

  /* Read the win reg key path */
  GetPrivateProfileString(szSection,
                          "Extra Cmd Reg Key",
                          "",
                          szKey,
                          sizeof(szKey),
                          szIniFile);
  if(*szKey == '\0')
    return(iRv);

  /* Read the win reg root key */
  GetPrivateProfileString(szSection,
                          "Extra Cmd Reg Key Root",
                          "",
                          szBuf,
                          sizeof(szBuf),
                          szIniFile);
  if(*szBuf == '\0')
    return(iRv);
  hkeyRoot = ParseRootKey(szBuf);

  /* Read the win reg name value */
  GetPrivateProfileString(szSection,
                          "Extra Cmd Reg Name",
                          "",
                          szName,
                          sizeof(szName),
                          szIniFile);
  if(*szName == '\0')
    ptrName = NULL;
  else
    ptrName = szName;

  /* Read the parameter to use for quitting the browser's turbo mode */
  GetPrivateProfileString(szSection,
                          "Extra Cmd Parameter",
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
    /* Run the file */
    WinSpawn(szFile, szParameter, szPath, SW_HIDE, TRUE);

    /* Even though WinSpawn is suppose to wait for the app to finish, this
     * does not really work that way for trying to quit the browser when
     * it's in turbo mode, so we wait 2 secs for it to complete. */
    Delay(2);
  }

  return(iRv);
}

HRESULT CheckInstances()
{
  char  szSection[MAX_BUF];
  char  szClassName[MAX_BUF];
  char  szWindowName[MAX_BUF];
  char  szMessage[MAX_BUF];
  char  szIndex[MAX_BUF];
  int   iIndex;
  BOOL  bContinue;
  HWND  hwndFW;
  LPSTR szWN;
  LPSTR szCN;
  DWORD dwRv0;
  DWORD dwRv1;

  bContinue = TRUE;
  iIndex    = -1;
  while(bContinue)
  {
    ZeroMemory(szClassName,  sizeof(szClassName));
    ZeroMemory(szWindowName, sizeof(szWindowName));
    ZeroMemory(szMessage,    sizeof(szMessage));

    ++iIndex;
    itoa(iIndex, szIndex, 10);
    lstrcpy(szSection, "Check Instance");
    lstrcat(szSection, szIndex);

    GetPrivateProfileString(szSection, "Message", "", szMessage, MAX_BUF, szFileIniUninstall);

    /* Process Name= key did not exist, so look for other keys */
    dwRv0 = GetPrivateProfileString(szSection, "Class Name", "", szClassName, MAX_BUF, szFileIniUninstall);
    dwRv1 = GetPrivateProfileString(szSection, "Window Name", "", szWindowName, MAX_BUF, szFileIniUninstall);
    if((dwRv0 == 0L) &&
       (dwRv1 == 0L))
    {
      bContinue = FALSE;
    }
    else if((*szClassName != '\0') || (*szWindowName != '\0'))
    {
      if(*szClassName == '\0')
        szCN = NULL;
      else
        szCN = szClassName;

      if(*szWindowName == '\0')
        szWN = NULL;
      else
        szWN = szWindowName;

      /* If an instance is found, call PreCheckInstance first. */
      if((hwndFW = FindWindow(szCN, szWN)) != NULL)
        PreCheckInstance(szSection, szFileIniUninstall);

      if((hwndFW = FindWindow(szCN, szWN)) != NULL)
      {
        if(*szMessage != '\0')
        {
          switch(ugUninstall.dwMode)
          {
            case NORMAL:
              switch(MessageBox(hWndMain, szMessage, NULL, MB_ICONEXCLAMATION | MB_RETRYCANCEL))
              {
                case IDCANCEL:
                  /* User selected to cancel Setup */
                  return(TRUE);

                case IDRETRY:
                  /* User selected to retry.  Reset counter */
                  iIndex = -1;
                  break;
              }
              break;

            case AUTO:
              ShowMessage(szMessage, TRUE);
              Delay(5);
              ShowMessage(szMessage, FALSE);

              /* Setup mode is AUTO.  Show message, timeout, then cancel because we can't allow user to continue */
              return(TRUE);

            case SILENT:
              return(TRUE);
          }
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

HRESULT ParseUninstallIni(LPSTR lpszCmdLine)
{
  char szBuf[MAX_BUF];
  char szKeyCrypted[MAX_BUF];
  char szShowDialog[MAX_BUF];
  LOGFONT lf;
  char fontName[MAX_BUF];
  char fontSize[MAX_BUF];
  char charSet[MAX_BUF];

  if(CheckInstances())
    return(1);
  if(InitUninstallGeneral())
    return(1);
  if(InitDlgUninstall(&diUninstall))
    return(1);
 
  lstrcpy(ugUninstall.szLogFilename, FILE_LOG_INSTALL);

  /* get install Mode information */
  GetPrivateProfileString("General", "Run Mode", "", szBuf, MAX_BUF, szFileIniUninstall);
  SetUninstallRunMode(szBuf);
  ParseCommandLine(lpszCmdLine);

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
  if(lstrcmpi(szBuf, "TRUE") == 0)
  {
    DecryptString(ugUninstall.szWrMainKey, szKeyCrypted);
  }
  else
    strcpy(ugUninstall.szWrMainKey, szKeyCrypted);

  RemoveBackSlash(ugUninstall.szWrMainKey);

  GetPrivateProfileString("General", "Uninstall Filename", "", ugUninstall.szUninstallFilename, MAX_BUF, szFileIniUninstall);


  /* Uninstall dialog */
  GetPrivateProfileString("Dialog Uninstall",       "Show Dialog",  "", szShowDialog,                 MAX_BUF, szFileIniUninstall);
  GetPrivateProfileString("Dialog Uninstall",       "Title",        "", diUninstall.szTitle,          MAX_BUF, szFileIniUninstall);
  GetPrivateProfileString("Dialog Uninstall",       "Message0",     "", diUninstall.szMessage0,       MAX_BUF, szFileIniUninstall);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diUninstall.bShowDialog = TRUE;

  switch(ugUninstall.dwMode)
  {
    case AUTO:
    case SILENT:
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
      /* Variable was not able to be dcripted. */
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

void DeInitialize()
{
  DeInitDlgUninstall(&diUninstall);
  DeInitUninstallGeneral();

  FreeMemory(&szTempDir);
  FreeMemory(&szOSTempDir);
  FreeMemory(&szUninstallDir);
  FreeMemory(&szEGlobalAlloc);
  FreeMemory(&szEDllLoad);
  FreeMemory(&szEStringLoad);
  FreeMemory(&szEStringNull);
}

