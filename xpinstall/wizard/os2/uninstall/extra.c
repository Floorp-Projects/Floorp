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
 *     IBM Corp. 
 */

#define INCl_DOSSESMGR
#define INCL_DOSMEMMGR 
#define INCL_DOSERRORS
#define INCL_WIN
#define INCL_WINWINDOWMGR
#define HIULONG(l)   ((ULONG) (((ULONG) (l) >> 32) & 0xFFFF))
#define LOULONG(l)   ((ULONG) (l))
#define INDEX_STR_LEN       10
#define PN_PROCESS          TEXT("Process")
#define PN_THREAD           TEXT("Thread")
#define INCL_WINSHELLDATA

#include "extern.h"
#include "extra.h"
#include "parser.h"
#include "dialogs.h"
#include "ifuncns.h"
#include <os2.h>

ULONG  (EXPENTRY *NS_GetDiskFreeSpace)(PCSZ, PULONG, PULONG, PULONG, PULONG);
ULONG  (EXPENTRY *NS_GetDiskFreeSpaceEx)(PCSZ, PULONG, PULONG, PULONG);

BOOL InitApplication(LHANDLE hInstance)
{

HAB hab;
PFNWP pGenericWndProc;
  
WinRegisterClass(hab, szClassName,  pGenericWndProc,  0L, 0);            
 
  dwScreenX = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
  dwScreenY = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);

  return(WinRegisterClass);
}

void PrintError(PSZ szMsg, ULONG dwErrorCodeSH)
{
  ULONG dwErr;
  char  szErrorString[MAX_BUF];

  if(dwErrorCodeSH == ERROR_CODE_SHOW)
  {
    dwErr = GetLastError();
    sprintf(szErrorString, "%d : %s", dwErr, szMsg);
  }
  else
    sprintf(szErrorString, "%s", szMsg);

  if((ugUninstall.dwMode != SILENT) && (ugUninstall.dwMode != AUTO))
  {
    WinMessageBox(HWND_DESKTOP, hWndMain, szErrorString, NULL, NULL, MB_ICONEXCLAMATION);
  }
  else if(ugUninstall.dwMode == AUTO)
  {
    ShowMessage(szErrorString, TRUE);
    Delay(5);
    ShowMessage(szErrorString, FALSE);
  }
}

void *NS_GlobalAlloc(ULONG dwMaxBuf)
{
   PVOID  MyObject    = NULL;
   APIRET rc; 

if((rc = DosAllocMem(&MyObject,  dwMaxBuf,  PAG_WRITE )) != 0)
  {
    return(FALSE);
  }
  else
  {
    return(rc);
  }
}
  
void FreeMemory(void **vPointer)
{
  if(*vPointer != NULL)
    *vPointer = GlobalFree(*vPointer);
}

APIRET NS_LoadStringAlloc(LHANDLE hInstance, ULONG dwID, PSZ *szStringBuf, ULONG dwStringBuf)
{
  char szBuf[MAX_BUF];

  if((*szStringBuf = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  
  if(!LoadString(hInstance, dwID, *szStringBuf, dwStringBuf))
  {
    if((szEStringLoad == NULL) ||(*szEStringLoad == '\0'))
      sprintf(szBuf, "Could not load string resource ID %d", dwID);
    else
      sprintf(szBuf, szEStringLoad, dwID);

    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  return(0);
}

APIRET NS_LoadString(LHANDLE hInstance, ULONG dwID, PSZ szStringBuf, ULONG dwStringBuf)
{
  char szBuf[MAX_BUF];

  if(!LoadString(hInstance, dwID, szStringBuf, dwStringBuf))
  {
    if((szEStringLoad == NULL) ||(*szEStringLoad == '\0'))
      sprintf(szBuf, "Could not load string resource ID %d", dwID);
    else
      sprintf(szBuf, szEStringLoad, dwID);

    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  return(WIZ_OK);
}

void Delay(ULONG dwSeconds)
{
  SleepEx(dwSeconds * 1000, FALSE);
}

APIRET Initialize(LHANDLE hInstance)
{
  char szBuf[MAX_BUF];
  HWND hwndFW;
  SWP swpCurrent;

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

  memset(szBuf, 0, sizeof(MAX_BUF));
  if((szClassName = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  strcpy(szClassName, CLASS_NAME);

  /* Allow only one instance of setup to run.
   * Detect a previous instance of setup, bring it to the 
   * foreground, and quit current instance */
  if((hwndFW = WinQueryObject(szClassName)) != NULL)
  {
    WinQueryWindowPos(hwndFW, &swpCurrent);
    WinSetWindowPos(hwndFW, HWND_TOP , swpCurrent.x, swpCurrent.y, swpCurrent.cx, swpCurrent.cy, SWP_RESTORE|SWP_MOVE|SWP_SIZE);
    WinShowWindow(hwndFW, TRUE);
    //SetForegroundWindow(hwndFW);
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

  strcpy(szFileIniUninstall, szUninstallDir);
  AppendBackSlash(szFileIniUninstall, MAX_BUF);
  strcat(szFileIniUninstall, FILE_INI_UNINSTALL);

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
    strcat(szTempDir, "TEMP");
  }
  strcpy(szOSTempDir, szTempDir);
  AppendBackSlash(szTempDir, MAX_BUF);
  strcat(szTempDir, WIZ_TEMP_DIR);

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
        sprintf(szBuf, szECreateTempDir, szTempDir);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }
      return(1);
    }
    RemoveBackSlash(szTempDir);
  }

  return(0);
}

/* Function to remove quotes from a string */
void RemoveQuotes(PSZ lpszSrc, PSZ lpszDest, int iDestSize)
{
  char *lpszBegin;

  if(strlen(lpszSrc) > iDestSize)
    return;

  if(*lpszSrc == '\"')
    lpszBegin = &lpszSrc[1];
  else
    lpszBegin = lpszSrc;

  strcpy(lpszDest, lpszBegin);

  if(lpszDest[strlen(lpszDest) - 1] == '\"')
    lpszDest[strlen(lpszDest) - 1] = '\0';
}

/* Function to locate the first non space character in a string,
 * and return a pointer to it. */
PSZ GetFirstNonSpace(PSZ lpszString)
{
  int   i;
  int   iStrLength;

  iStrLength = strlen(lpszString);

  for(i = 0; i < iStrLength; i++)
  {
    if(!isspace(lpszString[i]))
      return(&lpszString[i]);
  }

  return(NULL);
}

/* Function to return the argument count given a command line input
 * format string */
int GetArgC(PSZ lpszCommandLine)
{
  int   i;
  int   iArgCount;
  int   iStrLength;
  PSZ lpszBeginStr;
  BOOL  bFoundQuote;
  BOOL  bFoundSpace;

  iArgCount    = 0;
  lpszBeginStr = GetFirstNonSpace(lpszCommandLine);

  if(lpszBeginStr == NULL)
    return(iArgCount);

  iStrLength   = strlen(lpszBeginStr);
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
PSZ GetArgV(PSZ lpszCommandLine, int iIndex, PSZ lpszDest, int iDestSize)
{
  int   i;
  int   j;
  int   iArgCount;
  int   iStrLength;
  PSZ lpszBeginStr;
  PSZ lpszDestTemp;
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

  memset(lpszDest, 0, iDestSize);
  iStrLength    = strlen(lpszBeginStr);
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

void SetUninstallRunMode(PSZ szMode)
{
  if(strcmpi(szMode, "NORMAL") == 0)
    ugUninstall.dwMode = NORMAL;
  if(strcmpi(szMode, "AUTO") == 0)
    ugUninstall.dwMode = AUTO;
  if(strcmpi(szMode, "SILENT") == 0)
    ugUninstall.dwMode = SILENT;
}

void RemoveBackSlash(PSZ szInput)
{
  int   iCounter;
  ULONG dwInputLen;

  if(szInput != NULL)
  {
    dwInputLen = strlen(szInput);

    for(iCounter = dwInputLen -1; iCounter >= 0 ; iCounter--)
    {
      if(szInput[iCounter] == '\\')
        szInput[iCounter] = '\0';
      else
        break;
    }
  }
}

void AppendBackSlash(PSZ szInput, ULONG dwInputSize)
{
  if(szInput != NULL)
  {
    if(*szInput == '\0')
    {
      if(((ULONG)strlen(szInput) + 1) < dwInputSize)
      {
        strcat(szInput, "\\");
      }
    }
    else if(szInput[strlen(szInput) - 1] != '\\')
    {
      if(((ULONG)strlen(szInput) + 1) < dwInputSize)
      {
        strcat(szInput, "\\");
      }
    }
  }
}

void RemoveSlash(PSZ szInput)
{
  int   iCounter;
  ULONG dwInputLen;

  if(szInput != NULL)
  {
    dwInputLen = strlen(szInput);

    for(iCounter = dwInputLen -1; iCounter >= 0 ; iCounter--)
    {
      if(szInput[iCounter] == '/')
        szInput[iCounter] = '\0';
      else
        break;
    }
  }
}

void AppendSlash(PSZ szInput, ULONG dwInputSize)
{
  if(szInput != NULL)
  {
    if(*szInput == '\0')
    {
      if(((ULONG)strlen(szInput) + 1) < dwInputSize)
      {
        strcat(szInput, "/");
      }
    }
    else if(szInput[strlen(szInput) - 1] != '/')
    {
      if(((ULONG)strlen(szInput) + 1) < dwInputSize)
      {
        strcat(szInput, "/");
      }
    }
  }
}

void ParsePath(PSZ szInput, PSZ szOutput, ULONG dwOutputSize, ULONG dwType)
{
  int   iCounter;
  ULONG dwCounter;
  ULONG dwInputLen;
  BOOL  bFound;

  if((szInput != NULL) && (szOutput != NULL))
  {
    bFound        = TRUE;
    dwInputLen    = strlen(szInput);
    memset(szOutput, 0, dwOutputSize);

    if(dwInputLen < dwOutputSize)
    {
      switch(dwType)
      {
        case PP_FILENAME_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              strcpy(szOutput, &szInput[iCounter + 1]);
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            strcpy(szOutput, szInput);

          break;

        case PP_PATH_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              strcpy(szOutput, szInput);
              szOutput[iCounter + 1] = '\0';
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            strcpy(szOutput, szInput);

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
  ULONG         dwVersion;
  ULONG         dwWindowsMajorVersion;
  ULONG         dwWindowsMinorVersion;
  ULONG         dwWindowsVersion;
  BOOL          bIsWin95Debute;
  char          szESetupRequirement[MAX_BUF];

  ulOSType        = 0;
  dwVersion       = GetVersion();
  bIsWin95Debute  = IsWin95Debute();

  // Get major and minor version numbers of Windows
  dwWindowsMajorVersion =  (ULONG)(LOBYTE(LOWORD(dwVersion)));
  dwWindowsMinorVersion =  (ULONG)(HIBYTE(LOWORD(dwVersion)));
  dwWindowsVersion      =  (ULONG)(HIWORD(dwVersion));

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

APIRET WinSpawn(PSZ szClientName, PSZ szParameters, PSZ szCurrentDir, int iShowCmd, BOOL bWait)
{

HWND hwndNotify;
PPROGDETAILS pDetails;
HAPP happ;

   pDetails = (PPROGDETAILS) malloc( sizeof(PROGDETAILS) );   /* Allocate structure */

   pDetails->Length                      = sizeof(PROGDETAILS);
   pDetails->progt.progc                 = PROG_WINDOWABLEVIO;
   pDetails->progt.fbVisible             = SHE_VISIBLE;
   pDetails->pszTitle                    = "TEXT";
   pDetails->pszExecutable               = "TEXT.EXE";
   pDetails->pszParameters               = szParameters;
   pDetails->pszStartupDir               = szCurrentDir;
   pDetails->pszEnvironment              = "WORKPLACE\0\0";
   pDetails->swpInitial.fl               = SWP_ACTIVATE;        /* Window positioning   */
   pDetails->swpInitial.cy               = 0;                   /* Width of window      */
   pDetails->swpInitial.cx               = 0;                   /* Height of window     */
   pDetails->swpInitial.y                = 0;                   /* Lower edge of window */
   pDetails->swpInitial.x                = 0;                   /* Left edge of window  */
   pDetails->swpInitial.hwndInsertBehind = HWND_TOP;
   pDetails->swpInitial.hwnd             = hwndNotify;
   pDetails->swpInitial.ulReserved1      = 0;
   pDetails->swpInitial.ulReserved2      = 0;

   happ = WinStartApp(hwndNotify, pDetails, NULL, NULL, SAF_STARTCHILDAPP);

   WinTerminateApp(happ);
}

APIRET InitDlgUninstall(diU *diDialog)
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

APIRET InitUninstallGeneral()
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
  BOOL      bIsWin95Debute;
  HMODULE ModuleHandle = NULLHANDLE;
  UCHAR    LoadError[256] = "";
  PSZ        ModuleName = "kernel3.dll";
  APIRET   rc = NO_ERROR;

  PFN        NS_GetDiskFreeSpace;
  PFN        NS_GetDiskFreeSpaceEx;


  bIsWin95Debute = FALSE;
  
  if(DosLoadModule(LoadError, sizeof(LoadError), ModuleName, &ModuleHandle) == NO_ERROR)
  {
    if(DosQueryProcAddr(ModuleHandle, 0, "GetDiskFreeSpaceExA", NS_GetDiskFreeSpaceEx) == NO_ERROR)
    {       
        bIsWin95Debute = TRUE;        
        DosQueryProcAddr(ModuleHandle, 0, "GetDiskFreeSpaceA", NS_GetDiskFreeSpace);
    }

    DosFreeModule(ModuleHandle);
  }
  return(bIsWin95Debute);
}

void ParseCommandLine(PSZ lpszCmdLine)
{
  char  szArgVBuf[MAX_BUF];
  int   i;
  int   iArgC;

  i     = 0;
  iArgC = GetArgC(lpszCmdLine);
  while(i < iArgC)
  {
    GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));

    if((strcmpi(szArgVBuf, "-ma") == 0) || (strcmpi(szArgVBuf, "/ma") == 0))
    {
      SetUninstallRunMode("AUTO");
    }
    else if((strcmpi(szArgVBuf, "-ms") == 0) || (strcmpi(szArgVBuf, "/ms") == 0))
    {
      SetUninstallRunMode("SILENT");
    }
    else if((strcmpi(szArgVBuf, "-ua") == 0) || (strcmpi(szArgVBuf, "/ua") == 0))
    {
      if((i + 1) < iArgC)
        GetArgV(lpszCmdLine, ++i, ugUninstall.szUserAgent, MAX_BUF);
    }

    ++i;
  }
}

APIRET CheckInstances()
{
  char  szSection[MAX_BUF];
  char  szClassName[MAX_BUF];
  char  szWindowName[MAX_BUF];
  char  szMessage[MAX_BUF];
  char  szIndex[MAX_BUF];
  int   iIndex;
  BOOL  bContinue;
  HWND  hwndFW;
  PSZ szWN;
  PSZ szCN;
  ULONG dwRv0;
  ULONG dwRv1;

  bContinue = TRUE;
  iIndex    = -1;
  while(bContinue)
  {
    memset(szClassName, 0, sizeof(szClassName));
    memset(szWindowName, 0, sizeof(szWindowName));
    memset(szMessage, 0, sizeof(szMessage));

    ++iIndex;
    itoa(iIndex, szIndex, 10);
    strcpy(szSection, "Check Instance");
    strcat(szSection, szIndex);

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

      if((hwndFW = FindWindow(szClassName, szWN)) != NULL)
      {
        if(*szMessage != '\0')
        {
          if((ugUninstall.dwMode != SILENT) && (ugUninstall.dwMode != AUTO))
          {
            MessageBox(hWndMain, szMessage, NULL, MB_ICONEXCLAMATION);
          }
          else if(ugUninstall.dwMode == AUTO)
          {
            ShowMessage(szMessage, TRUE);
            Delay(5);
            ShowMessage(szMessage, FALSE);
          }
         }

        return(TRUE);
      }
    }
  }

  return(FALSE);
}

BOOL GetFileVersion(PSZ szFile, verBlock *vbVersion)
{
  UINT              uLen;
  UINT              dwLen;
  BOOL              bRv;
  ULONG             dwHandle;
  PPVOID            lpData;
  PPVOID            lpBuffer;

  vbVersion->ullMajor   = 0;
  vbVersion->ullMinor   = 0;
  vbVersion->ullRelease = 0;
  vbVersion->ullBuild   = 0;
  if(FileExists(szFile))
  {
     bRv    = TRUE;
     dwLen  = GetFileVersionInfoSize(szFile, &dwHandle);
     lpData = (PPVOID)malloc(sizeof(long)*dwLen);
     uLen   = 0;

    if(GetFileVersionInfo(szFile, dwHandle, dwLen, lpData) != 0)
    {
      if(VerQueryValue(lpData, "\\", &lpBuffer, &uLen) != 0)
      {
      }
    }
    free(lpData);
  }
  else
    /* File does not exist */
    bRv = FALSE;

  return(bRv);
}

void TranslateVersionStr(PSZ szVersion, verBlock *vbVersion)
{
  PSZ szNum1 = NULL;
  PSZ szNum2 = NULL;
  PSZ szNum3 = NULL;
  PSZ szNum4 = NULL;

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
  return FALSE;
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
  ULONG     dwRv0;
  ULONG     dwRv1;
  verBlock  vbVersionNew;
  verBlock  vbVersionOld;

  bContinue = TRUE;
  iIndex    = -1;
  while(bContinue)
  {
    memset(szFilename, 0, sizeof(szFilename));
    memset(szVersionNew, 0, sizeof(szVersionNew));
    memset(szMessage, 0, sizeof(szMessage));

    ++iIndex;
    itoa(iIndex, szIndex, 10);
    strcpy(szSection, "Legacy Check");
    strcat(szSection, szIndex);

    dwRv0 = PrfQueryProfileString(szSection, "Filename", "", szFilename, MAX_BUF, szFileIniUninstall);
    dwRv1 = PrfQueryProfileString(szSection, "Version", "", szVersionNew, MAX_BUF, szFileIniUninstall);
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
            strcpy(szMBWarningStr, "Warning");

          if(MessageBox(hDlg, szMessage, szMBWarningStr, MB_WARNING | MB_YESNO) == MBID_YES)
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
            strcpy(szMBWarningStr, "Warning");

          if(MessageBox(hDlg, szMessage, szMBWarningStr, MB_WARNING | MB_YESNO) == MBID_YES)
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

APIRET GetUninstallLogPath()
{
  char szBuf[MAX_BUF];
  char szLogFolder[MAX_BUF];
  char szKey[MAX_BUF];
  char szWindowsUninstallKey[MAX_BUF];
  char szErrorMsg[MAX_BUF];
  char szEUninstallLogFolder[MAX_BUF];
  char szRootKey[MAX_BUF];

  if(*ugUninstall.szUserAgent != '\0')
  {
    lstrcpy(szKey, ugUninstall.szWrMainKey);
    AppendBackSlash(szKey, sizeof(szKey));
    lstrcat(szKey, ugUninstall.szUserAgent);
    AppendBackSlash(szKey, sizeof(szKey));
    lstrcat(szKey, "Uninstall");
  }
  else
  {
    strcpy(szKey, ugUninstall.szWrKey);
  }

  GetProfReg( szKey,  "Uninstall Log Folder", szLogFolder, sizeof(szLogFolder));
  GetProfReg( szKey, "Description", ugUninstall.szUninstallKeyDescription, MAX_BUF);

    lstrcpy(szWindowsUninstallKey, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\");
    lstrcat(szWindowsUninstallKey, ugUninstall.szUninstallKeyDescription);
    GetProfReg( szWindowsUninstallKey, "DisplayName", ugUninstall.szDescription, MAX_BUF);

  /* if the DisplayName was not found in the windows registry,
   * use the description from read in from config.ini file */
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
//        GetStringRootKey(hkRoot, szRootKey, sizeof(szRootKey));
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


APIRET ParseUninstallIni(PSZ lpszCmdLine)
{
  char szBuf[MAX_BUF];
  char szKeyCrypted[MAX_BUF];
  char szShowDialog[MAX_BUF];

  if(CheckInstances())
    return(1);
  if(InitUninstallGeneral())
    return(1);
  if(InitDlgUninstall(&diUninstall))
    return(1);
 
  strcpy(ugUninstall.szLogFilename, FILE_LOG_INSTALL);

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
  if(strcmpi(szBuf, "TRUE") == 0)
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
  if(strcmpi(szBuf, "TRUE") == 0)
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
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diUninstall.bShowDialog = TRUE;

  switch(ugUninstall.dwMode)
  {
    case AUTO:
    case SILENT:
      gdwWhatToDo             = WTD_NO_TO_ALL;
      diUninstall.bShowDialog = FALSE;
      break;
  }

  return(GetUninstallLogPath());
}

void GetProfReg(PSZ szKey, PSZ szName, PSZ szReturnValue, ULONG dwReturnValueSize )
{
  BOOL rc  = TRUE;

  rc = PrfQueryProfileSize( HINI_PROFILE, szKey,  szName, &dwReturnValueSize );

  if( rc == TRUE )
  {
        rc = PrfQueryProfileString( HINI_PROFILE, szKey, szName,
                               (PSZ)"Error Retrieving Data",
                               (PVOID) szReturnValue,
                               dwReturnValueSize );
        if(rc == TRUE)
//                ExpandEnvironmentStrings(szBuf, szReturnValue, dwReturnValueSize);
                ;
        else
                *szReturnValue = '\0';
  }
  else
        *szReturnValue = '\0';
}

void SetProfReg( PSZ szKey, PSZ szName, PSZ szData )
{
  PrfWriteProfileString( HINI_PROFILE, szKey, szName, szData ); 
}

APIRET DecryptVariable(PSZ szVariable, ULONG dwVariableSize)
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

  if(strcmpi(szVariable, "PROGRAMFILESDIR") == 0)
  {
    /* parse for the "c:\Program Files" directory */
    GetProfReg( "Software\\Microsoft\\Windows\\CurrentVersion", "ProgramFilesDir", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "COMMONFILESDIR") == 0)
  {
    /* parse for the "c:\Program Files\Common Files" directory */
    GetProfReg( "Software\\Microsoft\\Windows\\CurrentVersion", "CommonFilesDir", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "MEDIAPATH") == 0)
  {
    /* parse for the "c:\Winnt40\Media" directory */
    GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion", "MediaPath", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "CONFIGPATH") == 0)
  {
    /* parse for the "c:\Windows\Config" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion", "ConfigPath", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "DEVICEPATH") == 0)
  {
    /* parse for the "c:\Winnt40\INF" directory */
     GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion", "DevicePath", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "COMMON_STARTUP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs\\Startup" directory */
    if(ulOSType & OS_WIN9x)
    {
     GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Startup", szVariable, dwVariableSize);
    }
    else
    {
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Startup", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "COMMON_PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Programs", szVariable, dwVariableSize);
    }
    else
    {
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Programs", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "COMMON_STARTMENU") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Start Menu", szVariable, dwVariableSize);
    }
    else
    {
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Start Menu", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "COMMON_DESKTOP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Desktop" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Desktop", szVariable, dwVariableSize);
    }
    else
    {
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Desktop", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "PERSONAL_STARTUP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu\Programs\Startup" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Startup", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu\Programs" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Programs", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_STARTMENU") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Start Menu", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_DESKTOP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Desktop" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Desktop", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_APPDATA") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Application Data" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "AppData", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_CACHE") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Temporary Internet Files" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Cache", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_COOKIES") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Cookies" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Cookies", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_FAVORITES") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Favorites" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Favorites", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_FONTS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Fonts" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Fonts", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_HISTORY") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\History" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "History", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_NETHOOD") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\NetHood" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "NetHood", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_PERSONAL") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Personal" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Personal", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_PRINTHOOD") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\PrintHood" directory */
    if(ulOSType & OS_NT)
    {
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "PrintHood", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "PERSONAL_RECENT") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Recent" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Recent", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_SENDTO") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\SendTo" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "SendTo", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_TEMPLATES") == 0)
  {
    /* parse for the "C:\WINNT40\ShellNew" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Templates", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "WIZTEMP") == 0)
  {
    /* parse for the "c:\Temp" path */
    strcpy(szVariable, szTempDir);
    if(szVariable[strlen(szVariable) - 1] == '\\')
      szVariable[strlen(szVariable) - 1] = '\0';
  }
  else if(strcmpi(szVariable, "TEMP") == 0)
  {
    /* parse for the "c:\Temp" path */
    strcpy(szVariable, szOSTempDir);
    if(szVariable[strlen(szVariable) - 1] == '\\')
      szVariable[strlen(szVariable) - 1] = '\0';
  }
  else if(strcmpi(szVariable, "WINDISK") == 0)
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
  else if(strcmpi(szVariable, "WINDIR") == 0)
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
  else if(strcmpi(szVariable, "WINSYSDIR") == 0)
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
  else if(strcmpi(szVariable, "JRE BIN PATH") == 0)
  {
    /* Locate the "c:\Program Files\JavaSoft\JRE\1.3\Bin" directory */
      GetProfReg("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\javaw.Exe", NULL, szVariable, dwVariableSize);
    if(*szVariable == '\0')
      return(FALSE);
    else
    {
      ParsePath(szVariable, szBuf, MAX_BUF, PP_PATH_ONLY);
      strcpy(szVariable, szBuf);
    }
  }
  else if(strcmpi(szVariable, "JRE PATH") == 0)
  {
    /* Locate the "c:\Program Files\JavaSoft\JRE\1.3" directory */
      GetProfReg("Software\\JavaSoft\\Java Plug-in\\1.3", "JavaHome", szVariable, dwVariableSize);
    if(*szVariable == '\0')
      return(FALSE);
  }
  else if(strcmpi(szVariable, "UNINSTALL STARTUP PATH") == 0)
  {
    strcpy(szVariable, szUninstallDir);
  }
  else if(strcmpi(szVariable, "Product CurrentVersion") == 0)
  {
    char szKey[MAX_BUF];

    strcpy(szKey, "Software\\");
    strcat(szKey, ugUninstall.szCompanyName);
    strcat(szKey, "\\");
    strcat(szKey, ugUninstall.szProductName);

    /* parse for the current Netscape WinReg key */
     GetProfReg(szKey, "CurrentVersion", szBuf, sizeof(szBuf));

    if(*szBuf == '\0')
      return(FALSE);

    sprintf(szVariable, "Software\\%s\\%s\\%s", ugUninstall.szCompanyName, ugUninstall.szProductName, szBuf);
  }
  else if(strcmpi(szVariable, "Product WinRegKey") == 0)
  {
    char szKey[MAX_BUF];

    strcpy(szKey, "Software\\");
    strcat(szKey, ugUninstall.szCompanyName);
    strcat(szKey, "\\");
    strcat(szKey, ugUninstall.szProductName);

    sprintf(szVariable, "Software\\%s\\%s", ugUninstall.szCompanyName, ugUninstall.szProductName);
  }
  else
    return(FALSE);

  return(TRUE);
}

APIRET DecryptString(PSZ szOutputStr, PSZ szInputStr)
{
  ULONG dwLenInputStr;
  ULONG dwCounter;
  ULONG dwVar;
  ULONG dwPrepend;
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

  strcpy(szPrepend, szInputStr);
  dwLenInputStr = strlen(szInputStr);
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
    strcpy(szAppend, &szInputStr[dwCounter]);

    szPrepend[dwPrepend] = '\0';

    bDecrypted = DecryptVariable(szVariable, MAX_BUF);
    if(!bDecrypted)
    {
      /* Variable was not able to be dcripted. */
      /* Leave the variable as it was read in by adding the '[' and ']' */
      /* characters back to the variable. */
      strcpy(szBuf, "[");
      strcat(szBuf, szVariable);
      strcat(szBuf, "]");
      strcpy(szVariable, szBuf);
    }

    strcpy(szOutputStr, szPrepend);
    strcat(szOutputStr, szVariable);
    strcat(szOutputStr, szAppend);

    if(bDecrypted)
    {
      DecryptString(szResultStr, szOutputStr);
      strcpy(szOutputStr, szResultStr);
    }
  }
  else
    strcpy(szOutputStr, szInputStr);

  return(TRUE);
}

APIRET FileExists(PSZ szFile)
{
  APIRET				rc = NO_ERROR;
  FILEFINDBUF3	fdFile;
  ULONG				ulResultBufLen = sizeof(FILEFINDBUF3);


  if((rc = DosQueryPathInfo(szFile, FIL_STANDARD, &fdFile, ulResultBufLen) != NO_ERROR))
  {
    return(FALSE);
  }
  else
  {
    return(rc);
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

