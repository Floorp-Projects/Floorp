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
#include "dialogs.h"
#include "ifuncns.h"
#include "xpnetHook.h"
#include "time.h"
#include "xpi.h"
#include "logging.h"
#include "nsEscape.h"
#include <winnls.h>
#include <winver.h>
#include <tlhelp32.h>
#include <winperf.h>

#define HIDWORD(l)   ((DWORD) (((ULONG) (l) >> 32) & 0xFFFF))
#define LODWORD(l)   ((DWORD) (l))

#define INDEX_STR_LEN       10
#define PN_PROCESS          TEXT("Process")
#define PN_THREAD           TEXT("Thread")

#define FTPSTR_LEN (sizeof(szFtp) - 1)
#define HTTPSTR_LEN (sizeof(szHttp) - 1)

ULONG  (PASCAL *NS_GetDiskFreeSpace)(LPCTSTR, LPDWORD, LPDWORD, LPDWORD, LPDWORD);
ULONG  (PASCAL *NS_GetDiskFreeSpaceEx)(LPCTSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);
typedef BOOL   (WINAPI *NS_ProcessWalk)(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
typedef HANDLE (WINAPI *NS_CreateSnapshot)(DWORD dwFlags, DWORD th32ProcessID);
typedef PERF_DATA_BLOCK             PERF_DATA,      *PPERF_DATA;
typedef PERF_OBJECT_TYPE            PERF_OBJECT,    *PPERF_OBJECT;
typedef PERF_INSTANCE_DEFINITION    PERF_INSTANCE,  *PPERF_INSTANCE;
TCHAR   INDEX_PROCTHRD_OBJ[2*INDEX_STR_LEN];
DWORD   PX_PROCESS;
DWORD   PX_THREAD;

char *ArchiveExtensions[] = {"zip",
                             "xpi",
                             "jar",
                             ""};

#define SETUP_STATE_REG_KEY "Software\\%s\\%s\\%s\\Setup"

typedef struct structVer
{
  ULONGLONG ullMajor;
  ULONGLONG ullMinor;
  ULONGLONG ullRelease;
  ULONGLONG ullBuild;
} verBlock;

void  TranslateVersionStr(LPSTR szVersion, verBlock *vbVersion);
BOOL  GetFileVersion(LPSTR szFile, verBlock *vbVersion);
int   CompareVersion(verBlock vbVersionOld, verBlock vbVersionNew);
BOOL CheckProcessNT4(LPSTR szProcessName, DWORD dwProcessNameSize);
DWORD GetTitleIdx(HWND hWnd, LPTSTR Title[], DWORD LastIndex, LPTSTR Name);
PPERF_OBJECT FindObject (PPERF_DATA pData, DWORD TitleIndex);
PPERF_OBJECT NextObject (PPERF_OBJECT pObject);
PPERF_OBJECT FirstObject (PPERF_DATA pData);
PPERF_INSTANCE   NextInstance (PPERF_INSTANCE pInst);
PPERF_INSTANCE   FirstInstance (PPERF_OBJECT pObject);
DWORD   GetPerfData    (HKEY        hPerfKey,
                        LPTSTR      szObjectIndex,
                        PPERF_DATA  *ppData,
                        DWORD       *pDataSize);

BOOL InitDialogClass(HINSTANCE hInstance, HINSTANCE hSetupRscInst)
{
  WNDCLASS  wc;

  wc.style         = CS_DBLCLKS | CS_SAVEBITS | CS_BYTEALIGNWINDOW;
  wc.lpfnWndProc   = DefDlgProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = DLGWINDOWEXTRA;
  wc.hInstance     = hSetupRscInst;
  wc.hIcon         = LoadIcon(hSetupRscInst, MAKEINTRESOURCE(IDI_SETUP));
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = CLASS_NAME_SETUP_DLG;

  return(RegisterClass(&wc));
}

BOOL InitApplication(HINSTANCE hInstance, HINSTANCE hSetupRscInst)
{
  return(InitDialogClass(hInstance, hSetupRscInst));
}

BOOL InitInstance(HINSTANCE hInstance, DWORD dwCmdShow)
{
  gSystemInfo.dwScreenX = GetSystemMetrics(SM_CXSCREEN);
  gSystemInfo.dwScreenY = GetSystemMetrics(SM_CYSCREEN);

  hInst = hInstance;
  hWndMain = NULL;

  return(TRUE);
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

  if((sgProduct.dwMode != SILENT) && (sgProduct.dwMode != AUTO))
  {
    MessageBox(hWndMain, szErrorString, NULL, MB_ICONEXCLAMATION);
  }
  else if(sgProduct.dwMode == AUTO)
  {
    ShowMessage(szErrorString, TRUE);
    Delay(5);
    ShowMessage(szErrorString, FALSE);
  }
}

/* Windows API does offer a GlobalReAlloc() routine, but it kept
 * returning an out of memory error for subsequent calls to it
 * after the first time, thus the reason for this routine. */
void *NS_GlobalReAlloc(HGLOBAL *hgMemory,
                       DWORD dwMemoryBufSize,
                       DWORD dwNewSize)
{
  HGLOBAL hgPtr = NULL;

  if((hgPtr = NS_GlobalAlloc(dwNewSize)) == NULL)
    return(NULL);
  else
  {
    memcpy(hgPtr, *hgMemory, dwMemoryBufSize);
    FreeMemory(hgMemory);
    *hgMemory = hgPtr;
    return(hgPtr);
  }
}

void *NS_GlobalAlloc(DWORD dwMaxBuf)
{
  void *vBuf = NULL;

  if((vBuf = GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT, dwMaxBuf)) == NULL)
  {     
    if((szEGlobalAlloc == NULL) || (*szEGlobalAlloc == '\0'))
      PrintError(TEXT("Memory allocation error."), ERROR_CODE_HIDE);
    else
      PrintError(szEGlobalAlloc, ERROR_CODE_SHOW);

    return(NULL);
  }
  else
  {
    ZeroMemory(vBuf, dwMaxBuf);
    return(vBuf);
  }
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

BOOL VerifyRestrictedAccess(void)
{
  char  szSubKey[MAX_BUF];
  char  szSubKeyToTest[] = "Software\\%s - Test Key";
  BOOL  bRv;
  DWORD dwDisp = 0;
  DWORD dwErr;
  HKEY  hkRv;

  wsprintf(szSubKey, szSubKeyToTest, sgProduct.szCompanyName);
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

void UnsetDownloadState(void)
{
  char szKey[MAX_BUF_TINY];

  wsprintf(szKey,
           SETUP_STATE_REG_KEY,
           sgProduct.szCompanyName,
           sgProduct.szProductName,
           sgProduct.szUserAgent);
  DeleteWinRegValue(HKEY_CURRENT_USER, szKey, "Setup State");
}

void SetDownloadState(void)
{
  char szKey[MAX_BUF_TINY];
  char szValue[MAX_BUF_TINY];

  wsprintf(szKey,
           SETUP_STATE_REG_KEY,
           sgProduct.szCompanyName,
           sgProduct.szProductName,
           sgProduct.szUserAgent);
  lstrcpy(szValue, "downloading");

  SetWinReg(HKEY_CURRENT_USER, szKey, TRUE, "Setup State", TRUE,
            REG_SZ, szValue, lstrlen(szValue), TRUE, FALSE);
}

BOOL CheckForPreviousUnfinishedDownload(void)
{
  char szBuf[MAX_BUF_TINY];
  char szKey[MAX_BUF_TINY];
  BOOL bRv = FALSE;

  if(sgProduct.szCompanyName &&
     sgProduct.szProductName &&
     sgProduct.szUserAgent)
  {
    wsprintf(szKey,
             SETUP_STATE_REG_KEY,
             sgProduct.szCompanyName,
             sgProduct.szProductName,
             sgProduct.szUserAgent);
    GetWinReg(HKEY_CURRENT_USER, szKey, "Setup State", szBuf, sizeof(szBuf));
    if(lstrcmpi(szBuf, "downloading") == 0)
      bRv = TRUE;
  }

  return(bRv);
}

void UnsetSetupCurrentDownloadFile(void)
{
  char szKey[MAX_BUF];

  wsprintf(szKey,
           SETUP_STATE_REG_KEY,
           sgProduct.szCompanyName,
           sgProduct.szProductName,
           sgProduct.szUserAgent);
  DeleteWinRegValue(HKEY_CURRENT_USER,
                    szKey,
                    "Setup Current Download");
}

void SetSetupCurrentDownloadFile(char *szCurrentFilename)
{
  char szKey[MAX_BUF];

  wsprintf(szKey,
           SETUP_STATE_REG_KEY,
           sgProduct.szCompanyName,
           sgProduct.szProductName,
           sgProduct.szUserAgent);
  SetWinReg(HKEY_CURRENT_USER,
            szKey,
            TRUE,
            "Setup Current Download",
            TRUE,
            REG_SZ,
            szCurrentFilename,
            lstrlen(szCurrentFilename),
            TRUE,
            FALSE);
}

char *GetSetupCurrentDownloadFile(char *szCurrentDownloadFile,
                                  DWORD dwCurrentDownloadFileBufSize)
{
  char szKey[MAX_BUF];

  if(!szCurrentDownloadFile)
    return(NULL);

  ZeroMemory(szCurrentDownloadFile, dwCurrentDownloadFileBufSize);
  if(sgProduct.szCompanyName &&
     sgProduct.szProductName &&
     sgProduct.szUserAgent)
  {
    wsprintf(szKey,
             SETUP_STATE_REG_KEY,
             sgProduct.szCompanyName,
             sgProduct.szProductName,
             sgProduct.szUserAgent);
    GetWinReg(HKEY_CURRENT_USER,
              szKey,
              "Setup Current Download", 
              szCurrentDownloadFile,
              dwCurrentDownloadFileBufSize);
  }

  return(szCurrentDownloadFile);
}

BOOL UpdateFile(char *szInFilename, char *szOutFilename, char *szIgnoreStr)
{
  FILE *ifp;
  FILE *ofp;
  char szLineRead[MAX_BUF];
  char szLCIgnoreLongStr[MAX_BUF];
  char szLCIgnoreShortStr[MAX_BUF];
  char szLCLineRead[MAX_BUF];
  BOOL bFoundIgnoreStr = FALSE;

  if((ifp = fopen(szInFilename, "rt")) == NULL)
    return(bFoundIgnoreStr);
  if((ofp = fopen(szOutFilename, "w+t")) == NULL)
  {
    fclose(ifp);
    return(bFoundIgnoreStr);
  }

  if(lstrlen(szIgnoreStr) < sizeof(szLCIgnoreLongStr))
  {
    lstrcpy(szLCIgnoreLongStr, szIgnoreStr);
    CharLower(szLCIgnoreLongStr);
  }
  if(lstrlen(szIgnoreStr) < sizeof(szLCIgnoreShortStr))
  {
    GetShortPathName(szIgnoreStr, szLCIgnoreShortStr, sizeof(szLCIgnoreShortStr));
    CharLower(szLCIgnoreShortStr);
  }

  while(fgets(szLineRead, sizeof(szLineRead), ifp) != NULL)
  {
    lstrcpy(szLCLineRead, szLineRead);
    CharLower(szLCLineRead);
    if(!strstr(szLCLineRead, szLCIgnoreLongStr) && !strstr(szLCLineRead, szLCIgnoreShortStr))
      fputs(szLineRead, ofp);
    else
      bFoundIgnoreStr = TRUE;
  }
  fclose(ifp);
  fclose(ofp);

  return(bFoundIgnoreStr);
}

/* Looks for and removes the uninstaller from the Windows Registry
 * that is set to delete the uninstaller at the next restart of
 * Windows.  This key is set/created when the user does the following:
 *
 * 1) Runs the uninstaller from the previous version of the product.
 * 2) User does not reboot the OS and starts the installation of
 *    the next version of the product.
 *
 * This functions prevents the uninstaller from being deleted on a
 * system reboot after the user performs 2).
 */
void ClearWinRegUninstallFileDeletion(void)
{
  char  *szPtrIn  = NULL;
  char  *szPtrOut = NULL;
  char  szInMultiStr[MAX_BUF];
  char  szOutMultiStr[MAX_BUF];
  char  szLCKeyBuf[MAX_BUF];
  char  szLCUninstallFilenameLongBuf[MAX_BUF];
  char  szLCUninstallFilenameShortBuf[MAX_BUF];
  char  szWinInitFile[MAX_BUF];
  char  szTempInitFile[MAX_BUF];
  char  szWinDir[MAX_BUF];
  DWORD dwOutMultiStrLen;
  DWORD dwType;
  BOOL  bFoundUninstaller = FALSE;

  if(!GetWindowsDirectory(szWinDir, sizeof(szWinDir)))
    return;

  wsprintf(szLCUninstallFilenameLongBuf, "%s\\%s", szWinDir, sgProduct.szUninstallFilename);
  GetShortPathName(szLCUninstallFilenameLongBuf, szLCUninstallFilenameShortBuf, sizeof(szLCUninstallFilenameShortBuf));
  CharLower(szLCUninstallFilenameLongBuf);
  CharLower(szLCUninstallFilenameShortBuf);

  if(gSystemInfo.dwOSType & OS_NT)
  {
    ZeroMemory(szInMultiStr,  sizeof(szInMultiStr));
    ZeroMemory(szOutMultiStr, sizeof(szOutMultiStr));

    dwType = GetWinReg(HKEY_LOCAL_MACHINE,
                       "System\\CurrentControlSet\\Control\\Session Manager",
                       "PendingFileRenameOperations",
                       szInMultiStr,
                       sizeof(szInMultiStr));
    if((dwType == REG_MULTI_SZ) && (szInMultiStr != '\0'))
    {
      szPtrIn          = szInMultiStr;
      szPtrOut         = szOutMultiStr;
      dwOutMultiStrLen = 0;
      do
      {
        lstrcpy(szLCKeyBuf, szPtrIn);
        CharLower(szLCKeyBuf);
        if(!strstr(szLCKeyBuf, szLCUninstallFilenameLongBuf) && !strstr(szLCKeyBuf, szLCUninstallFilenameShortBuf))
        {
          if((dwOutMultiStrLen + lstrlen(szPtrIn) + 3) <= sizeof(szOutMultiStr))
          {
            /* uninstaller not found, so copy the szPtrIn string to szPtrOut buffer */
            lstrcpy(szPtrOut, szPtrIn);
            dwOutMultiStrLen += lstrlen(szPtrIn) + 2;            /* there are actually 2 NULL bytes between the strings */
            szPtrOut          = &szPtrOut[lstrlen(szPtrIn) + 2]; /* there are actually 2 NULL bytes between the strings */
          }
          else
          {
            bFoundUninstaller = FALSE;
            /* not enough memory; break out of while loop. */
            break;
          }
        }
        else
          bFoundUninstaller = TRUE;

        szPtrIn = &szPtrIn[lstrlen(szPtrIn) + 2];              /* there are actually 2 NULL bytes between the strings */
      }while(*szPtrIn != '\0');
    }

    if(bFoundUninstaller)
    {
      if(dwOutMultiStrLen > 0)
      {
        /* take into account the 3rd NULL byte that signifies the end of the MULTI string */
        ++dwOutMultiStrLen;
        SetWinReg(HKEY_LOCAL_MACHINE,
                  "System\\CurrentControlSet\\Control\\Session Manager",
                  TRUE,
                  "PendingFileRenameOperations",
                  TRUE,
                  REG_MULTI_SZ,
                  szOutMultiStr,
                  dwOutMultiStrLen,
                  FALSE,
                  FALSE);
      }
      else
        DeleteWinRegValue(HKEY_LOCAL_MACHINE,
                          "System\\CurrentControlSet\\Control\\Session Manager",
                          "PendingFilerenameOperations");
    }
  }
  else
  {
    /* OS type is win9x */
    wsprintf(szWinInitFile, "%s\\wininit.ini", szWinDir);
    wsprintf(szTempInitFile, "%s\\wininit.moz", szWinDir);
    if(FileExists(szWinInitFile))
    {
      if(UpdateFile(szWinInitFile, szTempInitFile, szLCUninstallFilenameLongBuf))
        CopyFile(szTempInitFile, szWinInitFile, FALSE);

      DeleteFile(szTempInitFile);
    }
  }
}

HRESULT Initialize(HINSTANCE hInstance)
{
  char szBuf[MAX_BUF];
  char szCurrentProcessDir[MAX_BUF];

  bSDUserCanceled     = FALSE;
  hDlgMessage         = NULL;

  /* load strings from setup.exe */
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_GLOBALALLOC, &szEGlobalAlloc, MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_STRING_LOAD, &szEStringLoad,  MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_DLL_LOAD,    &szEDllLoad,     MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_STRING_NULL, &szEStringNull,  MAX_BUF))
    return(1);

  GetModuleFileName(NULL, szBuf, sizeof(szBuf));
  ParsePath(szBuf, szCurrentProcessDir,
            sizeof(szCurrentProcessDir),
            FALSE,
            PP_PATH_ONLY);
  hAccelTable = LoadAccelerators(hInstance, CLASS_NAME_SETUP_DLG);

  if((hSetupRscInst = LoadLibraryEx("Setuprsc.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) == NULL)
  {
    char szFullFilename[MAX_BUF];

    lstrcpy(szFullFilename, szCurrentProcessDir);
    AppendBackSlash(szFullFilename, sizeof(szFullFilename));
    lstrcat(szFullFilename, "Setuprsc.dll");
    if((hSetupRscInst = LoadLibraryEx(szFullFilename, NULL, 0)) == NULL)
    {
      wsprintf(szBuf, szEDllLoad, szFullFilename);
      PrintError(szBuf, ERROR_CODE_HIDE);
      return(1);
    }
  }

  dwWizardState         = DLG_NONE;
  dwTempSetupType       = dwWizardState;
  siComponents          = NULL;
  bCreateDestinationDir = FALSE;
  bReboot               = FALSE;
  gdwUpgradeValue       = UG_NONE;
  gdwSiteSelectorStatus = SS_SHOW;
  gbILUseTemp           = TRUE;
  gbIgnoreRunAppX        = FALSE;
  gbIgnoreProgramFolderX = FALSE;

  if((szSetupDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  lstrcpy(szSetupDir, szCurrentProcessDir);

  if((szTempDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szOSTempDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szFileIniConfig = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szFileIniInstall = NS_GlobalAlloc(MAX_BUF)) == NULL)
  return(1);

  // determine the system's TEMP path
  if(GetTempPath(MAX_BUF, szTempDir) == 0)
  {
    if(GetWindowsDirectory(szTempDir, MAX_BUF) == 0)
    {
      char szEGetWinDirFailed[MAX_BUF];

      if(GetPrivateProfileString("Messages", "ERROR_GET_WINDOWS_DIRECTORY_FAILED", "", szEGetWinDirFailed, sizeof(szEGetWinDirFailed), szFileIniInstall))
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
    CreateDirectoriesAll(szTempDir, FALSE);
    if(!FileExists(szTempDir))
    {
      char szECreateTempDir[MAX_BUF];

      if(GetPrivateProfileString("Messages", "ERROR_CREATE_TEMP_DIR", "", szECreateTempDir, sizeof(szECreateTempDir), szFileIniInstall))
      {
        wsprintf(szBuf, szECreateTempDir, szTempDir);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }
      return(1);
    }
    RemoveBackSlash(szTempDir);
  }

  hbmpBoxChecked         = LoadBitmap(hSetupRscInst, MAKEINTRESOURCE(IDB_BOX_CHECKED));
  hbmpBoxCheckedDisabled = LoadBitmap(hSetupRscInst, MAKEINTRESOURCE(IDB_BOX_CHECKED_DISABLED));
  hbmpBoxUnChecked       = LoadBitmap(hSetupRscInst, MAKEINTRESOURCE(IDB_BOX_UNCHECKED));

  DeleteIdiGetConfigIni();
  bIdiArchivesExists = DeleteIdiGetArchives();
  DeleteIdiGetRedirect();
  DeleteInstallLogFile(FILE_INSTALL_LOG);
  DeleteInstallLogFile(FILE_INSTALL_STATUS_LOG);
  LogISTime(W_START);
  DetermineOSVersionEx();
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

BOOL IsInArchivesLst(siC *siCObject, BOOL bModify)
{
  char *szBufPtr;
  char szBuf[MAX_BUF];
  char szArchiveLstFile[MAX_BUF_MEDIUM];
  BOOL bRet = FALSE;

  lstrcpy(szArchiveLstFile, szTempDir);
  AppendBackSlash(szArchiveLstFile, sizeof(szArchiveLstFile));
  lstrcat(szArchiveLstFile, "Archive.lst");
  GetPrivateProfileString("Archives", NULL, "", szBuf, sizeof(szBuf), szArchiveLstFile);
  if(*szBuf != '\0')
  {
    szBufPtr = szBuf;
    while(*szBufPtr != '\0')
    {
      if(lstrcmpi(siCObject->szArchiveName, szBufPtr) == 0)
      {
        if(bModify)
        {
          /* jar file found.  Unset attribute to download from the net */
          siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
          /* save the path of where jar was found at */
          lstrcpy(siCObject->szArchivePath, szTempDir);
          AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
        }
        bRet = TRUE;

        /* found what we're looking for.  No need to continue */
        break;
      }
      szBufPtr += lstrlen(szBufPtr) + 1;
    }
  }
  return(bRet);
}

HRESULT ParseSetupIni()
{
  char szBuf[MAX_BUF];
  char szFileIniSetup[MAX_BUF];
  char szFileIdiGetConfigIni[MAX_BUF];

  lstrcpy(szFileIdiGetConfigIni, szTempDir);
  AppendBackSlash(szFileIdiGetConfigIni, sizeof(szFileIdiGetConfigIni));
  lstrcat(szFileIdiGetConfigIni, FILE_IDI_GETCONFIGINI);

  lstrcpy(szFileIniSetup, szSetupDir);
  AppendBackSlash(szFileIniSetup, sizeof(szFileIniSetup));
  lstrcat(szFileIniSetup, FILE_INI_SETUP);

  CopyFile(szFileIniSetup, szFileIdiGetConfigIni, FALSE);

  if(!FileExists(szFileIdiGetConfigIni))
  {
    char szEFileNotFound[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_FILE_NOT_FOUND", "", szEFileNotFound, sizeof(szEFileNotFound), szFileIniInstall))
    {
      wsprintf(szBuf, szEFileNotFound, szFileIdiGetConfigIni);
      PrintError(szBuf, ERROR_CODE_HIDE);
    }
    return(1);
  }

  return(0);
}

HRESULT GetConfigIni()
{
  char    szFileIniTempDir[MAX_BUF];
  char    szFileIniSetupDir[MAX_BUF];
  char    szMsgRetrieveConfigIni[MAX_BUF];
  char    szBuf[MAX_BUF];
  HRESULT hResult = 0;

  if(!GetPrivateProfileString("Messages", "MSG_RETRIEVE_CONFIGINI", "", szMsgRetrieveConfigIni, sizeof(szMsgRetrieveConfigIni), szFileIniInstall))
    return(1);
    
  lstrcpy(szFileIniTempDir, szTempDir);
  AppendBackSlash(szFileIniTempDir, sizeof(szFileIniTempDir));
  lstrcat(szFileIniTempDir, FILE_INI_CONFIG);

  /* set default value for szFileIniConfig here */
  lstrcpy(szFileIniConfig, szFileIniTempDir);

  lstrcpy(szFileIniSetupDir, szSetupDir);
  AppendBackSlash(szFileIniSetupDir, sizeof(szFileIniSetupDir));
  lstrcat(szFileIniSetupDir, FILE_INI_CONFIG);

  /* if config.ini exists, then use it, else download config.ini from the net */
  if(!FileExists(szFileIniTempDir))
  {
    if(FileExists(szFileIniSetupDir))
    {
      lstrcpy(szFileIniConfig, szFileIniSetupDir);
      hResult = 0;
    }
    else
    {
      char szEFileNotFound[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_FILE_NOT_FOUND", "", szEFileNotFound, sizeof(szEFileNotFound), szFileIniInstall))
      {
        wsprintf(szBuf, szEFileNotFound, FILE_INI_CONFIG);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }
      hResult = 1;
    }
  }
  else
    hResult = 0;

  return(hResult);
}

HRESULT GetInstallIni()
{
  char    szFileIniTempDir[MAX_BUF];
  char    szFileIniSetupDir[MAX_BUF];
  char    szMsgRetrieveInstallIni[MAX_BUF];
  char    szBuf[MAX_BUF];
  HRESULT hResult = 0;

  if(NS_LoadString(hSetupRscInst, IDS_MSG_RETRIEVE_INSTALLINI, szMsgRetrieveInstallIni, MAX_BUF) != WIZ_OK)
    return(1);
    
  lstrcpy(szFileIniTempDir, szTempDir);
  AppendBackSlash(szFileIniTempDir, sizeof(szFileIniTempDir));
  lstrcat(szFileIniTempDir, FILE_INI_INSTALL);

  /* set default value for szFileIniInstall here */
  lstrcpy(szFileIniInstall, szFileIniTempDir);

  lstrcpy(szFileIniSetupDir, szSetupDir);
  AppendBackSlash(szFileIniSetupDir, sizeof(szFileIniSetupDir));
  lstrcat(szFileIniSetupDir, FILE_INI_INSTALL);

  /* if install.ini exists, then use it, else download install.ini from the net */
  if(!FileExists(szFileIniTempDir))
  {
    if(FileExists(szFileIniSetupDir))
    {
      lstrcpy(szFileIniInstall, szFileIniSetupDir);
      hResult = 0;
    }
    else
    {
      char szEFileNotFound[MAX_BUF];

      if(NS_LoadString(hSetupRscInst, IDS_ERROR_FILE_NOT_FOUND, szEFileNotFound, MAX_BUF) == WIZ_OK)
      {
        wsprintf(szBuf, szEFileNotFound, FILE_INI_INSTALL);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }
      hResult = 1;
    }
  }
  else
    hResult = 0;

  return(hResult);
}

int LocateJar(siC *siCObject, LPSTR szPath, int dwPathSize, BOOL bIncludeTempDir)
{
  BOOL bRet;
  char szBuf[MAX_BUF * 2];
  char szSEADirTemp[MAX_BUF];
  char szSetupDirTemp[MAX_BUF];
  char szTempDirTemp[MAX_BUF];

  /* initialize default behavior */
  bRet = AP_NOT_FOUND;
  if(szPath != NULL)
    ZeroMemory(szPath, dwPathSize);
  siCObject->dwAttributes |= SIC_DOWNLOAD_REQUIRED;

  lstrcpy(szSEADirTemp, sgProduct.szAlternateArchiveSearchPath);
  AppendBackSlash(szSEADirTemp, sizeof(szSEADirTemp));
  lstrcat(szSEADirTemp, siCObject->szArchiveName);

  /* XXX_QUICK_FIX 
   * checking sgProduct.szAlternateArchiveSearchPath for empty string
   * should be done prior to AppendBackSlash() above.
   * This is a quick fix for the time frame that we are currently in. */
  if((*sgProduct.szAlternateArchiveSearchPath != '\0') && (FileExists(szSEADirTemp)))
  {
    /* jar file found.  Unset attribute to download from the net */
    siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
    /* save the path of where jar was found at */
    lstrcpy(siCObject->szArchivePath, sgProduct.szAlternateArchiveSearchPath);
    AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
    bRet = AP_ALTERNATE_PATH;

    /* save path where archive is located */
    if((szPath != NULL) && (lstrlen(sgProduct.szAlternateArchiveSearchPath) < dwPathSize))
      lstrcpy(szPath, sgProduct.szAlternateArchiveSearchPath);
  }
  else
  {
    lstrcpy(szSetupDirTemp, szSetupDir);
    AppendBackSlash(szSetupDirTemp, sizeof(szSetupDirTemp));

    lstrcpy(szTempDirTemp,  szTempDir);
    AppendBackSlash(szTempDirTemp, sizeof(szTempDirTemp));

    if(lstrcmpi(szTempDirTemp, szSetupDirTemp) == 0)
    {
      /* check the temp dir for the .xpi file */
      lstrcpy(szBuf, szTempDirTemp);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, siCObject->szArchiveName);

      if(FileExists(szBuf))
      {
        if(bIncludeTempDir == TRUE)
        {
          /* jar file found.  Unset attribute to download from the net */
          siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
          /* save the path of where jar was found at */
          lstrcpy(siCObject->szArchivePath, szTempDirTemp);
          AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
          bRet = AP_TEMP_PATH;
        }

        /* if the archive name is in the archive.lst file, then it was uncompressed
         * by the self extracting .exe file.  Assume that the .xpi file exists.
         * This is a safe assumption because the self extracting.exe creates the
         * archive.lst with what it uncompresses everytime it is run. */
        if(IsInArchivesLst(siCObject, TRUE))
          bRet = AP_SETUP_PATH;

        /* save path where archive is located */
        if((szPath != NULL) && (lstrlen(szTempDirTemp) < dwPathSize))
          lstrcpy(szPath, szTempDirTemp);
      }
    }
    else
    {
      /* check the setup dir for the .xpi file */
      lstrcpy(szBuf, szSetupDirTemp);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, siCObject->szArchiveName);

      if(FileExists(szBuf))
      {
        /* jar file found.  Unset attribute to download from the net */
        siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
        /* save the path of where jar was found at */
        lstrcpy(siCObject->szArchivePath, szSetupDirTemp);
        AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
        bRet = AP_SETUP_PATH;

        /* save path where archive is located */
        if((szPath != NULL) && (lstrlen(sgProduct.szAlternateArchiveSearchPath) < dwPathSize))
          lstrcpy(szPath, szSetupDirTemp);
      }
      else
      {
        /* check the ns_temp dir for the .xpi file */
        lstrcpy(szBuf, szTempDirTemp);
        AppendBackSlash(szBuf, sizeof(szBuf));
        lstrcat(szBuf, siCObject->szArchiveName);

        if(FileExists(szBuf))
        {
          if(bIncludeTempDir == TRUE)
          {
            /* jar file found.  Unset attribute to download from the net */
            siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
            /* save the path of where jar was found at */
            lstrcpy(siCObject->szArchivePath, szTempDirTemp);
            AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
            bRet = AP_TEMP_PATH;
          }

          /* save path where archive is located */
          if((szPath != NULL) && (lstrlen(sgProduct.szAlternateArchiveSearchPath) < dwPathSize))
            lstrcpy(szPath, szTempDirTemp);
        }
      }
    }
  }
  return(bRet);
}

void SwapFTPAndHTTP(char *szInUrl, DWORD dwInUrlSize)
{
  char szTmpBuf[MAX_BUF];
  char *ptr       = NULL;
  char szFtp[]    = "ftp://";
  char szHttp[]   = "http://";

  if((!szInUrl) || !diDownloadOptions.bUseProtocolSettings)
    return;

  ZeroMemory(szTmpBuf, sizeof(szTmpBuf));
  switch(diDownloadOptions.dwUseProtocol)
  {
    case UP_HTTP:
      if((strncmp(szInUrl, szFtp, FTPSTR_LEN) == 0) &&
         ((int)dwInUrlSize > lstrlen(szInUrl) + 1))
      {
        ptr = szInUrl + FTPSTR_LEN;
        memmove(ptr + 1, ptr, lstrlen(ptr) + 1);
        memcpy(szInUrl, szHttp, HTTPSTR_LEN);
      }
      break;

    case UP_FTP:
    default:
      if((strncmp(szInUrl, szHttp, HTTPSTR_LEN) == 0) &&
         ((int)dwInUrlSize > lstrlen(szInUrl) + 1))
      {
        ptr = szInUrl + HTTPSTR_LEN;
        memmove(ptr - 1, ptr, lstrlen(ptr) + 1);
        memcpy(szInUrl, szFtp, FTPSTR_LEN);
      }
      break;
  }
}

int UpdateIdiFile(char  *szPartialUrl,
                  DWORD dwPartialUrlBufSize,
                  siC   *siCObject,
                  char  *szSection,
                  char  *szKey,
                  char  *szFileIdiGetArchives)
{
  char      szUrl[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      szBufTemp[MAX_BUF];

  SwapFTPAndHTTP(szPartialUrl, dwPartialUrlBufSize);
  RemoveSlash(szPartialUrl);
  wsprintf(szUrl, "%s/%s", szPartialUrl, siCObject->szArchiveName);
  if(WritePrivateProfileString(szSection,
                               szKey,
                               szUrl,
                               szFileIdiGetArchives) == 0)
  {
    char szEWPPS[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_WRITEPRIVATEPROFILESTRING", "", szEWPPS, sizeof(szEWPPS), szFileIniInstall))
    {
      wsprintf(szBufTemp,
               "%s\n    [%s]\n    url=%s",
               szFileIdiGetArchives,
               szSection,
               szUrl);
      wsprintf(szBuf, szEWPPS, szBufTemp);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    return(1);
  }
  return(0);
}

HRESULT AddArchiveToIdiFile(siC *siCObject,
                            char *szSection,
                            char *szFileIdiGetArchives)
{
  char      szFile[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      szUrl[MAX_BUF];
  char      szIdentifier[MAX_BUF];
  char      szArchiveSize[MAX_ITOA];
  char      szKey[MAX_BUF_TINY];
  int       iIndex = 0;
  ssi       *ssiSiteSelectorTemp;

  WritePrivateProfileString(szSection,
                            "desc",
                            siCObject->szDescriptionShort,
                            szFileIdiGetArchives);
  _ui64toa(siCObject->ullInstallSizeArchive, szArchiveSize, 10);
  WritePrivateProfileString(szSection,
                            "size",
                            szArchiveSize,
                            szFileIdiGetArchives);
  itoa(siCObject->dwAttributes & SIC_IGNORE_DOWNLOAD_ERROR, szBuf, 10);
  WritePrivateProfileString(szSection,
                            "Ignore File Network Error",
                            szBuf,
                            szFileIdiGetArchives);

  lstrcpy(szFile, szTempDir);
  AppendBackSlash(szFile, sizeof(szFile));
  lstrcat(szFile, FILE_INI_REDIRECT);

  ZeroMemory(szIdentifier, sizeof(szIdentifier));
  ssiSiteSelectorTemp = SsiGetNode(szSiteSelectorDescription);

  GetPrivateProfileString("Redirect",
                          "Status",
                          "",
                          szBuf,
                          sizeof(szBuf),
                          szFileIniConfig);
  if(lstrcmpi(szBuf, "ENABLED") != 0)
  {
    /* redirect.ini is *not* enabled, so use the url from the
     * config.ini's [Site Selector] section */
    if(*ssiSiteSelectorTemp->szDomain != '\0')
    {
      wsprintf(szKey, "url%d", iIndex);
      lstrcpy(szUrl, ssiSiteSelectorTemp->szDomain);
      UpdateIdiFile(szUrl, sizeof(szUrl), siCObject, szSection, szKey, szFileIdiGetArchives);
      ++iIndex;
    }

    /* use the url from the config.ini's [General] section as well */
    GetPrivateProfileString("General",
                            "url",
                            "",
                            szUrl,
                            sizeof(szUrl),
                            szFileIniConfig);
    if(*szUrl != 0)
    {
      wsprintf(szKey, "url%d", iIndex);
      UpdateIdiFile(szUrl, sizeof(szUrl), siCObject, szSection, szKey, szFileIdiGetArchives);
    }
  }
  else if(FileExists(szFile))
  {
    /* redirect.ini is enabled *and* it exists */
    GetPrivateProfileString("Site Selector",
                            ssiSiteSelectorTemp->szIdentifier,
                            "",
                            szUrl,
                            sizeof(szUrl),
                            szFile);
    if(*szUrl != '\0')
    {
      wsprintf(szKey, "url%d", iIndex);
      UpdateIdiFile(szUrl, sizeof(szUrl), siCObject, szSection, szKey, szFileIdiGetArchives);
      ++iIndex;
    }

    /* use the url from the config.ini's [General] section as well */
    GetPrivateProfileString("General",
                            "url",
                            "",
                            szUrl,
                            sizeof(szUrl),
                            szFileIniConfig);
    if(*szUrl != 0)
    {
      wsprintf(szKey, "url%d", iIndex);
      UpdateIdiFile(szUrl, sizeof(szUrl), siCObject, szSection, szKey, szFileIdiGetArchives);
    }
  }
  else
  {
    /* redirect.ini is enabled, but the file does not exist,
     * so fail over to the url from the config.ini's [General] section */
    GetPrivateProfileString("General",
                            "url",
                            "",
                            szUrl,
                            sizeof(szUrl),
                            szFileIniConfig);
    if(*szUrl != 0)
    {
      wsprintf(szKey, "url%d", iIndex);
      UpdateIdiFile(szUrl, sizeof(szUrl), siCObject, szSection, szKey, szFileIdiGetArchives);
    }
  }

  return(0);
}

void SetSetupRunMode(LPSTR szMode)
{
  if(lstrcmpi(szMode, "NORMAL") == 0)
    sgProduct.dwMode = NORMAL;
  if(lstrcmpi(szMode, "AUTO") == 0)
    sgProduct.dwMode = AUTO;
  if(lstrcmpi(szMode, "SILENT") == 0)
    sgProduct.dwMode = SILENT;
}

BOOL CheckForArchiveExtension(LPSTR szFile)
{
  int  i;
  BOOL bRv = FALSE;
  char szExtension[MAX_BUF_TINY];

  /* if there is no extension in szFile, szExtension will be zero'ed out */
  ParsePath(szFile, szExtension, sizeof(szExtension), FALSE, PP_EXTENSION_ONLY);
  i = 0;
  while(*ArchiveExtensions[i] != '\0')
  {
    if(lstrcmpi(szExtension, ArchiveExtensions[i]) == 0)
    {
      bRv = TRUE;
      break;
    }

    ++i;
  }
  return(bRv);
}

long RetrieveRedirectFile()
{
  long      lResult;
  char      szBuf[MAX_BUF];
  char      szBufUrl[MAX_BUF];
  char      szBufTemp[MAX_BUF];
  char      szIndex0[MAX_BUF];
  char      szFileIdiGetRedirect[MAX_BUF];
  char      szFileIniRedirect[MAX_BUF];
  ssi       *ssiSiteSelectorTemp;

  if(GetTotalArchivesToDownload() == 0)
    return(0);

  lstrcpy(szFileIniRedirect, szTempDir);
  AppendBackSlash(szFileIniRedirect, sizeof(szFileIniRedirect));
  lstrcat(szFileIniRedirect, FILE_INI_REDIRECT);

  if(FileExists(szFileIniRedirect))
    DeleteFile(szFileIniRedirect);

  GetPrivateProfileString("Redirect", "Status", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "ENABLED") != 0)
    return(0);

  ssiSiteSelectorTemp = SsiGetNode(szSiteSelectorDescription);
  if(ssiSiteSelectorTemp != NULL)
  {
    if(ssiSiteSelectorTemp->szDomain != NULL)
      lstrcpy(szBufUrl, ssiSiteSelectorTemp->szDomain);
  }
  else
    /* No domain to download the redirect.ini file from.
     * Assume that it does not exist.
     * This should trigger the backup/alternate url. */
    return(0);

  lstrcpy(szFileIdiGetRedirect, szTempDir);
  AppendBackSlash(szFileIdiGetRedirect, sizeof(szFileIdiGetRedirect));
  lstrcat(szFileIdiGetRedirect, FILE_IDI_GETREDIRECT);

  GetPrivateProfileString("Redirect", "Description", "", szBuf, sizeof(szBuf), szFileIniConfig);
  WritePrivateProfileString("File0", "desc", szBuf, szFileIdiGetRedirect);
  GetPrivateProfileString("Redirect", "Server Path", "", szBuf, sizeof(szBuf), szFileIniConfig);
  AppendSlash(szBufUrl, sizeof(szBufUrl));
  lstrcat(szBufUrl, szBuf);
  SwapFTPAndHTTP(szBufUrl, sizeof(szBufUrl));
  if(WritePrivateProfileString("File0", "url", szBufUrl, szFileIdiGetRedirect) == 0)
  {
    char szEWPPS[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_WRITEPRIVATEPROFILESTRING", "", szEWPPS, sizeof(szEWPPS), szFileIniInstall))
    {
      wsprintf(szBufTemp, "%s\n    [%s]\n    %s=%s", szFileIdiGetRedirect, "File0", szIndex0, szBufUrl);
      wsprintf(szBuf, szEWPPS, szBufTemp);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    return(1);
  }

  lResult = DownloadFiles(szFileIdiGetRedirect,               /* input idi file to parse                 */
                          szTempDir,                          /* download directory                      */
                          diAdvancedSettings.szProxyServer,   /* proxy server name                       */
                          diAdvancedSettings.szProxyPort,     /* proxy server port                       */
                          diAdvancedSettings.szProxyUser,     /* proxy server user (optional)            */
                          diAdvancedSettings.szProxyPasswd,   /* proxy password (optional)               */
                          FALSE,                              /* show retry message                      */
                          TRUE,                               /* ignore network error                    */
                          NULL,                               /* buffer to store the name of failed file */
                          0);                                 /* size of failed file name buffer         */
  return(lResult);
}

int CRCCheckDownloadedArchives(char *szCorruptedArchiveList,
                               DWORD dwCorruptedArchiveListSize,
                               char *szFileIdiGetArchives)
{
  DWORD dwIndex0;
  DWORD dwFileCounter;
  siC   *siCObject = NULL;
  char  szArchivePathWithFilename[MAX_BUF];
  char  szArchivePath[MAX_BUF];
  char  szMsgCRCCheck[MAX_BUF];
  char  szSection[MAX_INI_SK];
  int   iRv;
  int   iResult;

  /* delete the getarchives.idi file because it needs to be recreated
   * if there are any files that fails the CRC check */
  if(szFileIdiGetArchives)
    DeleteFile(szFileIdiGetArchives);

  if(szCorruptedArchiveList != NULL)
    ZeroMemory(szCorruptedArchiveList, dwCorruptedArchiveListSize);

  GetPrivateProfileString("Strings", "Message Verifying Archives", "", szMsgCRCCheck, sizeof(szMsgCRCCheck), szFileIniConfig);
  ShowMessage(szMsgCRCCheck, TRUE);
  
  iResult           = WIZ_CRC_PASS;
  dwIndex0          = 0;
  dwFileCounter     = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    if((siCObject->dwAttributes & SIC_SELECTED) &&
      !(siCObject->dwAttributes & SIC_IGNORE_DOWNLOAD_ERROR))
    {
      if((iRv = LocateJar(siCObject, szArchivePath, sizeof(szArchivePath), TRUE)) == AP_NOT_FOUND)
      {
        char szBuf[MAX_BUF];
        char szEFileNotFound[MAX_BUF];

       if(GetPrivateProfileString("Messages", "ERROR_FILE_NOT_FOUND", "", szEFileNotFound, sizeof(szEFileNotFound), szFileIniInstall))
        {
          wsprintf(szBuf, szEFileNotFound, siCObject->szArchiveName);
          PrintError(szBuf, ERROR_CODE_HIDE);
        }
        iResult = WIZ_ARCHIVES_MISSING; // not all .xpi files were downloaded
        break;
      }

      if(lstrlen(szArchivePath) < sizeof(szArchivePathWithFilename))
        lstrcpy(szArchivePathWithFilename, szArchivePath);

      AppendBackSlash(szArchivePathWithFilename, sizeof(szArchivePathWithFilename));
      if((lstrlen(szArchivePathWithFilename) + lstrlen(siCObject->szArchiveName)) < sizeof(szArchivePathWithFilename))
        lstrcat(szArchivePathWithFilename, siCObject->szArchiveName);

      if(CheckForArchiveExtension(szArchivePathWithFilename))
      {
        /* Make sure that the Archive that failed is located in the TEMP
         * folder.  This means that it was downloaded at one point and not
         * simply uncompressed from the self-extracting exe file. */
        if(VerifyArchive(szArchivePathWithFilename) != ZIP_OK)
        {
          if(iRv == AP_TEMP_PATH)
          {
            /* Delete the archive even though the download lib will automatically
             * overwrite the file.  This is in case that Setup is canceled, at the
             * next restart, the file will be downloaded during the first attempt,
             * not after a VerifyArchive() call. */
            DeleteFile(szArchivePathWithFilename);
            wsprintf(szSection, "File%d", dwFileCounter);
            ++dwFileCounter;
            if(szFileIdiGetArchives)
              if((AddArchiveToIdiFile(siCObject,
                                      szSection,
                                      szFileIdiGetArchives)) != 0)
                return(WIZ_ERROR_UNDEFINED);

            ++siCObject->iCRCRetries;
            if(szCorruptedArchiveList != NULL)
            {
              if((DWORD)(lstrlen(szCorruptedArchiveList) + lstrlen(siCObject->szArchiveName + 1)) < dwCorruptedArchiveListSize)
              {
                lstrcat(szCorruptedArchiveList, "        ");
                lstrcat(szCorruptedArchiveList, siCObject->szArchiveName);
                lstrcat(szCorruptedArchiveList, "\n");
              }
            }
          }
          iResult = WIZ_CRC_FAIL;
        }
      }
    }

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }
  ShowMessage(szMsgCRCCheck, FALSE);
  return(iResult);
}

long RetrieveArchives()
{
  DWORD     dwIndex0;
  DWORD     dwFileCounter;
  BOOL      bDone;
  BOOL      bDownloadTriggered;
  siC       *siCObject = NULL;
  long      lResult;
  char      szFileIdiGetArchives[MAX_BUF];
  char      szSection[MAX_BUF];
  char      szCorruptedArchiveList[MAX_BUF];
  char      szFailedFile[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      szPartiallyDownloadedFilename[MAX_BUF];
  int       iCRCRetries;
  int       iRv;

  /* retrieve the redirect.ini file */
  RetrieveRedirectFile();

  ZeroMemory(szCorruptedArchiveList, sizeof(szCorruptedArchiveList));
  lstrcpy(szFileIdiGetArchives, szTempDir);
  AppendBackSlash(szFileIdiGetArchives, sizeof(szFileIdiGetArchives));
  lstrcat(szFileIdiGetArchives, FILE_IDI_GETARCHIVES);
  GetSetupCurrentDownloadFile(szPartiallyDownloadedFilename,
                              sizeof(szPartiallyDownloadedFilename));

  bDownloadTriggered = FALSE;
  lResult            = WIZ_OK;
  dwIndex0           = 0;
  dwFileCounter      = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    if(siCObject->dwAttributes & SIC_SELECTED)
    {
      /* If a previous unfinished setup was detected, then
       * include the TEMP dir when searching for archives.
       * Only download jars if not already in the local machine.
       * Also if the last file being downloaded should be resumed.
       * The resume detection is done automatically. */
      if((LocateJar(siCObject,
                    NULL,
                    0,
                    gbPreviousUnfinishedDownload) == AP_NOT_FOUND) ||
         (lstrcmpi(szPartiallyDownloadedFilename,
                   siCObject->szArchiveName) == 0))
      {
        wsprintf(szSection, "File%d", dwFileCounter);
        if((lResult = AddArchiveToIdiFile(siCObject,
                                          szSection,
                                          szFileIdiGetArchives)) != 0)
          return(lResult);

        ++dwFileCounter;
      }
    }

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  SetDownloadState();

  /* iCRCRetries is initially set to 0 because the first attemp at downloading
   * the archives is not considered a "retry".  Subsequent downloads are
   * considered retries. */
  iCRCRetries = 0;
  bDone = FALSE;
  do
  {
    /* the existence of the getarchives.idi file determines if there are
       any archives needed to be downloaded */
    if(FileExists(szFileIdiGetArchives))
    {
      bDownloadTriggered = TRUE;
      lResult = DownloadFiles(szFileIdiGetArchives,               /* input idi file to parse                 */
                              szTempDir,                          /* download directory                      */
                              diAdvancedSettings.szProxyServer,   /* proxy server name                       */
                              diAdvancedSettings.szProxyPort,     /* proxy server port                       */
                              diAdvancedSettings.szProxyUser,     /* proxy server user (optional)            */
                              diAdvancedSettings.szProxyPasswd,   /* proxy password (optional)               */
                              iCRCRetries,                        /* show retry message                      */
                              FALSE,                              /* ignore network error                    */
                              szFailedFile,                       /* buffer to store the name of failed file */
                              sizeof(szFailedFile));              /* size of failed file name buffer         */
      if(lResult == WIZ_OK)
      {
        /* CRC check only the archives that were downloaded.
         * It will regenerate the idi file with the list of files
         * that have not passed the CRC check. */
        iRv = CRCCheckDownloadedArchives(szCorruptedArchiveList,
                                         sizeof(szCorruptedArchiveList),
                                         szFileIdiGetArchives);
        switch(iRv)
        {
          case WIZ_CRC_PASS:
            bDone = TRUE;
            break;

          default:
            bDone = FALSE;
            break;
        }
      }
      else
      {
        /* Download failed.  Error message was already shown by DownloadFiles().
         * Simple exit loop here  */
        bDone = TRUE;
      }
    }
    else
      /* no idi file, so exit loop */
      bDone = TRUE;

    if(!bDone)
    {
      ++iCRCRetries;
      if(iCRCRetries > MAX_CRC_FAILED_DOWNLOAD_RETRIES)
        bDone = TRUE;
    }

  } while(!bDone);

  if(iCRCRetries > MAX_CRC_FAILED_DOWNLOAD_RETRIES)
  {
    /* too many retries from failed CRC checks */
    char szMsg[MAX_BUF];

    LogISComponentsFailedCRC(szCorruptedArchiveList, W_DOWNLOAD);
    GetPrivateProfileString("Strings", "Error Too Many CRC Failures", "", szMsg, sizeof(szMsg), szFileIniConfig);
    if(*szMsg != '\0')
      PrintError(szMsg, ERROR_CODE_HIDE);

    lResult = WIZ_CRC_FAIL;
  }
  else
  {
    if(bDownloadTriggered)
      LogISComponentsFailedCRC(NULL, W_DOWNLOAD);
  }

  LogISDownloadProtocol(diDownloadOptions.dwUseProtocol);
  LogMSDownloadProtocol(diDownloadOptions.dwUseProtocol);

  if(lResult == WIZ_OK)
  {
    LogISDownloadStatus("ok", NULL);
  }
  else if(bDownloadTriggered)
  {
    wsprintf(szBuf, "failed: %d", lResult);
    LogISDownloadStatus(szBuf, szFailedFile);
  }

  /* We want to log the download status regardless if we downloaded or not. */
  LogMSDownloadStatus(lResult);

  if(lResult == WIZ_OK)
  {
    UnsetSetupCurrentDownloadFile();
    UnsetDownloadState();
  }

  return(lResult);
}

void RemoveBackSlash(LPSTR szInput)
{
  DWORD dwInputLen;
  BOOL  bDone;
  char  *ptrChar = NULL;

  if(szInput)
  {
    dwInputLen = lstrlen(szInput);
    bDone = FALSE;
    ptrChar = &szInput[dwInputLen];
    while(!bDone)
    {
      ptrChar = CharPrev(szInput, ptrChar);
      if(*ptrChar == '\\')
        *ptrChar = '\0';
      else
        bDone = TRUE;
    }
  }
}

void AppendBackSlash(LPSTR szInput, DWORD dwInputSize)
{
  DWORD dwInputLen = lstrlen(szInput);

  if(szInput)
  {
    if(*szInput == '\0')
    {
      if((dwInputLen + 1) < dwInputSize)
      {
        lstrcat(szInput, "\\");
      }
    }
    else if(*CharPrev(szInput, &szInput[dwInputLen]) != '\\')
    {
      if((dwInputLen + 1) < dwInputSize)
      {
        lstrcat(szInput, "\\");
      }
    }
  }
}

void RemoveSlash(LPSTR szInput)
{
  DWORD dwInputLen;
  BOOL  bDone;
  char  *ptrChar = NULL;

  if(szInput)
  {
    dwInputLen = lstrlen(szInput);
    bDone = FALSE;
    ptrChar = &szInput[dwInputLen];
    while(!bDone)
    {
      ptrChar = CharPrev(szInput, ptrChar);
      if(*ptrChar == '/')
        *ptrChar = '\0';
      else
        bDone = TRUE;
    }
  }
}

void AppendSlash(LPSTR szInput, DWORD dwInputSize)
{
  DWORD dwInputLen = lstrlen(szInput);

  if(szInput)
  {
    if(*szInput == '\0')
    {
      if((dwInputLen + 1) < dwInputSize)
      {
        lstrcat(szInput, "/");
      }
    }
    else if(*CharPrev(szInput, &szInput[dwInputLen]) != '/')
    {
      if((dwInputLen + 1) < dwInputSize)
      {
        lstrcat(szInput, "/");
      }
    }
  }
}

void ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwOutputSize, BOOL bURLPath, DWORD dwType)
{
  int   iFoundDelimiter;
  DWORD dwInputLen;
  DWORD dwOutputLen;
  BOOL  bFound;
  BOOL  bDone = FALSE;
  char  cDelimiter;
  char  *ptrChar = NULL;
  char  *ptrLastChar = NULL;

  if(bURLPath)
    cDelimiter = '/';
  else
    cDelimiter = '\\';

  if(szInput && szOutput)
  {
    bFound        = TRUE;
    dwInputLen    = lstrlen(szInput);
    ZeroMemory(szOutput, dwOutputSize);

    if(dwInputLen < dwOutputSize)
    {
      switch(dwType)
      {
        case PP_FILENAME_ONLY:
          ptrChar = &szInput[dwInputLen];
          bDone = FALSE;
          while(!bDone)
          {
            ptrChar = CharPrev(szInput, ptrChar);
            if(*ptrChar == cDelimiter)
            {
              lstrcpy(szOutput, CharNext(ptrChar));
              bDone = TRUE;
            }
            else if(ptrChar == szInput)
            {
              /* we're at the beginning of the string and still
               * nothing found.  So just return the input string. */
              lstrcpy(szOutput, szInput);
              bDone = TRUE;
            }
          }
          break;

        case PP_PATH_ONLY:
          lstrcpy(szOutput, szInput);
          dwOutputLen = lstrlen(szOutput);
          ptrChar = &szOutput[dwOutputLen];
          bDone = FALSE;
          while(!bDone)
          {
            ptrChar = CharPrev(szOutput, ptrChar);
            if(*ptrChar == cDelimiter)
            {
              *CharNext(ptrChar) = '\0';
              bDone = TRUE;
            }
            else if(ptrChar == szOutput)
            {
              /* we're at the beginning of the string and still
               * nothing found.  So just return the input string. */
              bDone = TRUE;
            }
          }
          break;

        case PP_EXTENSION_ONLY:
          /* check the last character first */
          ptrChar = CharPrev(szInput, &szInput[dwInputLen]);
          if(*ptrChar == '.')
            break;

          bDone = FALSE;
          while(!bDone)
          {
            ptrChar = CharPrev(szInput, ptrChar);
            if(*ptrChar == cDelimiter)
              /* found path delimiter before '.' */
              bDone = TRUE;
            else if(*ptrChar == '.')
            {
              lstrcpy(szOutput, CharNext(ptrChar));
              bDone = TRUE;
            }
            else if(ptrChar == szInput)
              bDone = TRUE;
          }
          break;

        case PP_ROOT_ONLY:
          lstrcpy(szOutput, szInput);
          dwOutputLen = lstrlen(szOutput);
          ptrLastChar = CharPrev(szOutput, &szOutput[dwOutputLen]);
          ptrChar     = CharNext(szOutput);
          if(*ptrChar == ':')
          {
            ptrChar = CharNext(ptrChar);
            *ptrChar = cDelimiter;
            *CharNext(ptrChar) = '\0';
          }
          else
          {
            iFoundDelimiter = 0;
            ptrChar = szOutput;
            while(!bDone)
            {
              if(*ptrChar == cDelimiter)
                ++iFoundDelimiter;

              if(iFoundDelimiter == 4)
              {
                *CharNext(ptrChar) = '\0';
                bDone = TRUE;
              }
              else if(ptrChar == ptrLastChar)
                bDone = TRUE;
              else
                ptrChar = CharNext(ptrChar);
            }
          }
          break;
      }
    }
  }
}

HRESULT LaunchApps()
{
  DWORD     dwIndex0;
  BOOL      bArchiveFound;
  siC       *siCObject = NULL;
  char      szArchive[MAX_BUF];
  char      szMsg[MAX_BUF];

  LogISLaunchApps(W_START);
  if(!GetPrivateProfileString("Messages", "MSG_CONFIGURING", "", szMsg, sizeof(szMsg), szFileIniInstall))
    return(1);

  dwIndex0 = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    /* launch 3rd party executable */
    if((siCObject->dwAttributes & SIC_SELECTED) && (siCObject->dwAttributes & SIC_LAUNCHAPP))
    {
      bArchiveFound = TRUE;
      lstrcpy(szArchive, sgProduct.szAlternateArchiveSearchPath);
      AppendBackSlash(szArchive, sizeof(szArchive));
      lstrcat(szArchive, siCObject->szArchiveName);
      if((*sgProduct.szAlternateArchiveSearchPath == '\0') || !FileExists(szArchive))
      {
        lstrcpy(szArchive, szSetupDir);
        AppendBackSlash(szArchive, sizeof(szArchive));
        lstrcat(szArchive, siCObject->szArchiveName);
        if(!FileExists(szArchive))
        {
          lstrcpy(szArchive, szTempDir);
          AppendBackSlash(szArchive, sizeof(szArchive));
          lstrcat(szArchive, siCObject->szArchiveName);
          if(!FileExists(szArchive))
          {
            bArchiveFound = FALSE;
          }
        }
      }

      if(bArchiveFound)
      {
        char szParameterBuf[MAX_BUF];
        char szSpawnFile[MAX_BUF];
        char szMessageString[MAX_BUF];
        DWORD dwErr = FO_SUCCESS;

        wsprintf(szMessageString, szMsg, siCObject->szDescriptionShort);
        ShowMessage(szMessageString, TRUE);
        DecryptString(szParameterBuf, siCObject->szParameter);

        lstrcpy(szSpawnFile, szArchive);
        if(siCObject->dwAttributes & SIC_UNCOMPRESS)
        {
          if((dwErr = FileUncompress(szArchive, szTempDir)) == FO_SUCCESS)
          {
            lstrcpy(szSpawnFile, szTempDir);
            AppendBackSlash(szSpawnFile, sizeof(szSpawnFile));
            lstrcat(szSpawnFile, siCObject->szArchiveNameUncompressed);
          }

          LogISLaunchAppsComponentUncompress(siCObject->szDescriptionShort, dwErr);
          if(dwErr != FO_SUCCESS)
          {
            ShowMessage(szMessageString, FALSE);
            continue;
          }
        }

        LogISLaunchAppsComponent(siCObject->szDescriptionShort);
        WinSpawn(szSpawnFile, szParameterBuf, szTempDir, SW_SHOWNORMAL, TRUE);

        if(siCObject->dwAttributes & SIC_UNCOMPRESS)
          FileDelete(szSpawnFile);

        ShowMessage(szMessageString, FALSE);
      }
    }
    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  LogISLaunchApps(W_END);
  return(0);
}

char *GetOSTypeString(char *szOSType, DWORD dwOSTypeBufSize)
{
  ZeroMemory(szOSType, dwOSTypeBufSize);

  if(gSystemInfo.dwOSType & OS_WIN95_DEBUTE)
    lstrcpy(szOSType, "Win95 debute");
  else if(gSystemInfo.dwOSType & OS_WIN95)
    lstrcpy(szOSType, "Win95");
  else if(gSystemInfo.dwOSType & OS_WIN98)
    lstrcpy(szOSType, "Win98");
  else if(gSystemInfo.dwOSType & OS_NT3)
    lstrcpy(szOSType, "NT3");
  else if(gSystemInfo.dwOSType & OS_NT4)
    lstrcpy(szOSType, "NT4");
  else if(gSystemInfo.dwOSType & OS_NT50)
    lstrcpy(szOSType, "NT50");
  else if(gSystemInfo.dwOSType & OS_NT51)
    lstrcpy(szOSType, "NT51");
  else
    lstrcpy(szOSType, "NT5");

  return(szOSType);
}

void DetermineOSVersionEx()
{
  BOOL          bIsWin95Debute;
  char          szBuf[MAX_BUF];
  char          szOSType[MAX_BUF];
  char          szESetupRequirement[MAX_BUF];
  DWORD         dwStrLen;
  OSVERSIONINFO osVersionInfo;
  MEMORYSTATUS  msMemoryInfo;

  gSystemInfo.dwOSType = 0;
  osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  if(!GetVersionEx(&osVersionInfo))
  {
    /* GetVersionEx() failed for some reason.  It's not fatal, but could cause
     * some complications during installation */
    char szEMsg[MAX_BUF_TINY];

    if(GetPrivateProfileString("Messages", "ERROR_GETVERSION", "", szEMsg, sizeof(szEMsg), szFileIniInstall))
      PrintError(szEMsg, ERROR_CODE_SHOW);
  }

  bIsWin95Debute  = IsWin95Debute();
  switch(osVersionInfo.dwPlatformId)
  {
    case VER_PLATFORM_WIN32_WINDOWS:
      gSystemInfo.dwOSType |= OS_WIN9x;
      if(osVersionInfo.dwMinorVersion == 0)
      {
        gSystemInfo.dwOSType |= OS_WIN95;
        if(bIsWin95Debute)
          gSystemInfo.dwOSType |= OS_WIN95_DEBUTE;
      }
      else
        gSystemInfo.dwOSType |= OS_WIN98;
      break;

    case VER_PLATFORM_WIN32_NT:
      gSystemInfo.dwOSType |= OS_NT;
      switch(osVersionInfo.dwMajorVersion)
      {
        case 3:
          gSystemInfo.dwOSType |= OS_NT3;
          break;

        case 4:
          gSystemInfo.dwOSType |= OS_NT4;
          break;

        default:
          gSystemInfo.dwOSType |= OS_NT5;
          switch(osVersionInfo.dwMinorVersion)
          {
            case 0:
              /* a minor version of 0 (major.minor.build) indicates Win2000 */
              gSystemInfo.dwOSType |= OS_NT50;
              break;

            case 1:
              /* a minor version of 1 (major.minor.build) indicates WinXP */
              gSystemInfo.dwOSType |= OS_NT51;
          break;
      }
      break;
      }
      break;

    default:
      if(GetPrivateProfileString("Messages", "ERROR_SETUP_REQUIREMENT", "", szESetupRequirement, sizeof(szESetupRequirement), szFileIniInstall))
        PrintError(szESetupRequirement, ERROR_CODE_HIDE);
      break;
  }

  gSystemInfo.dwMajorVersion = osVersionInfo.dwMajorVersion;
  gSystemInfo.dwMinorVersion = osVersionInfo.dwMinorVersion;
  gSystemInfo.dwBuildNumber  = (DWORD)(LOWORD(osVersionInfo.dwBuildNumber));

  dwStrLen = sizeof(gSystemInfo.szExtraString) >
                    lstrlen(osVersionInfo.szCSDVersion) ?
                    lstrlen(osVersionInfo.szCSDVersion) :
                    sizeof(gSystemInfo.szExtraString) - 1;
  ZeroMemory(gSystemInfo.szExtraString, sizeof(gSystemInfo.szExtraString));
  strncpy(gSystemInfo.szExtraString, osVersionInfo.szCSDVersion, dwStrLen);

  msMemoryInfo.dwLength = sizeof(MEMORYSTATUS);
  GlobalMemoryStatus(&msMemoryInfo);
  gSystemInfo.dwMemoryTotalPhysical = msMemoryInfo.dwTotalPhys/1024;
  gSystemInfo.dwMemoryAvailablePhysical = msMemoryInfo.dwAvailPhys/1024;

  GetOSTypeString(szOSType, sizeof(szOSType));
  wsprintf(szBuf,
"    System Info:\n\
        OS Type: %s\n\
        Major Version: %d\n\
        Minor Version: %d\n\
        Build Number: %d\n\
        Extra String: %s\n\
        Total Physical Memory: %dKB\n\
        Total Available Physical Memory: %dKB\n",
           szOSType,
           gSystemInfo.dwMajorVersion,
           gSystemInfo.dwMinorVersion,
           gSystemInfo.dwBuildNumber,
           gSystemInfo.szExtraString,
           gSystemInfo.dwMemoryTotalPhysical,
           gSystemInfo.dwMemoryAvailablePhysical);

  UpdateInstallStatusLog(szBuf);
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

HRESULT InitDlgWelcome(diW *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage1 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage2 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgWelcome(diW *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szMessage1));
  FreeMemory(&(diDialog->szMessage2));
}

HRESULT InitDlgLicense(diL *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szLicenseFilename = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage1 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgLicense(diL *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szLicenseFilename));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szMessage1));
}

HRESULT InitDlgQuickLaunch(diQL *diDialog)
{
  diDialog->bTurboMode         = FALSE;
  diDialog->bTurboModeEnabled  = FALSE;
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage1 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage2 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgQuickLaunch(diQL *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szMessage1));
  FreeMemory(&(diDialog->szMessage2));
}

HRESULT InitDlgSetupType(diST *diDialog)
{
  diDialog->bShowDialog = FALSE;

  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szReadmeFilename = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szReadmeApp = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  diDialog->stSetupType0.dwCItems = 0;
  diDialog->stSetupType1.dwCItems = 0;
  diDialog->stSetupType2.dwCItems = 0;
  diDialog->stSetupType3.dwCItems = 0;
  if((diDialog->stSetupType0.szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->stSetupType0.szDescriptionLong = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((diDialog->stSetupType1.szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->stSetupType1.szDescriptionLong = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((diDialog->stSetupType2.szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->stSetupType2.szDescriptionLong = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((diDialog->stSetupType3.szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->stSetupType3.szDescriptionLong = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgSetupType(diST *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));

  FreeMemory(&(diDialog->szReadmeFilename));
  FreeMemory(&(diDialog->szReadmeApp));
  FreeMemory(&(diDialog->stSetupType0.szDescriptionShort));
  FreeMemory(&(diDialog->stSetupType0.szDescriptionLong));
  FreeMemory(&(diDialog->stSetupType1.szDescriptionShort));
  FreeMemory(&(diDialog->stSetupType1.szDescriptionLong));
  FreeMemory(&(diDialog->stSetupType2.szDescriptionShort));
  FreeMemory(&(diDialog->stSetupType2.szDescriptionLong));
  FreeMemory(&(diDialog->stSetupType3.szDescriptionShort));
  FreeMemory(&(diDialog->stSetupType3.szDescriptionLong));
}

HRESULT InitDlgSelectComponents(diSC *diDialog, DWORD dwSM)
{
  diDialog->bShowDialog = FALSE;

  /* set to show the Single dialog or the Multi dialog for the SelectComponents dialog */
  diDialog->bShowDialogSM = dwSM;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgSelectComponents(diSC *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
}

HRESULT InitDlgWindowsIntegration(diWI *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((diDialog->szMessage1 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);

  diDialog->wiCB0.bEnabled = FALSE;
  diDialog->wiCB1.bEnabled = FALSE;
  diDialog->wiCB2.bEnabled = FALSE;
  diDialog->wiCB3.bEnabled = FALSE;

  diDialog->wiCB0.bCheckBoxState = FALSE;
  diDialog->wiCB1.bCheckBoxState = FALSE;
  diDialog->wiCB2.bCheckBoxState = FALSE;
  diDialog->wiCB3.bCheckBoxState = FALSE;

  if((diDialog->wiCB0.szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB1.szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB2.szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB3.szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((diDialog->wiCB0.szArchive = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB1.szArchive = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB2.szArchive = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB3.szArchive = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgWindowsIntegration(diWI *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szMessage1));

  FreeMemory(&(diDialog->wiCB0.szDescription));
  FreeMemory(&(diDialog->wiCB1.szDescription));
  FreeMemory(&(diDialog->wiCB2.szDescription));
  FreeMemory(&(diDialog->wiCB3.szDescription));
  FreeMemory(&(diDialog->wiCB0.szArchive));
  FreeMemory(&(diDialog->wiCB1.szArchive));
  FreeMemory(&(diDialog->wiCB2.szArchive));
  FreeMemory(&(diDialog->wiCB3.szArchive));
}

HRESULT InitDlgProgramFolder(diPF *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgProgramFolder(diPF *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
}

HRESULT InitDlgDownloadOptions(diDO *diDialog)
{
  diDialog->bShowDialog           = FALSE;
  diDialog->bSaveInstaller        = FALSE;
  diDialog->dwUseProtocol         = UP_FTP;
  diDialog->bUseProtocolSettings  = TRUE;
  diDialog->bShowProtocols        = TRUE;
  if((diDialog->szTitle           = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0        = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage1        = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgDownloadOptions(diDO *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szMessage1));
}

HRESULT InitDlgAdvancedSettings(diAS *diDialog)
{
  diDialog->bShowDialog       = FALSE;
  if((diDialog->szTitle       = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0    = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szProxyServer = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szProxyPort   = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szProxyUser   = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szProxyPasswd = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgAdvancedSettings(diAS *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szProxyServer));
  FreeMemory(&(diDialog->szProxyPort));
  FreeMemory(&(diDialog->szProxyUser));
  FreeMemory(&(diDialog->szProxyPasswd));
}

HRESULT InitDlgStartInstall(diSI *diDialog)
{
  diDialog->bShowDialog        = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessageInstall = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessageDownload = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgStartInstall(diSI *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessageInstall));
  FreeMemory(&(diDialog->szMessageDownload));
}

HRESULT InitDlgDownload(diD *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF_TINY)) == NULL)
    return(1);
  if((diDialog->szMessageDownload0 = NS_GlobalAlloc(MAX_BUF_MEDIUM)) == NULL)
    return(1);
  if((diDialog->szMessageRetry0 = NS_GlobalAlloc(MAX_BUF_MEDIUM)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgDownload(diD *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessageDownload0));
  FreeMemory(&(diDialog->szMessageRetry0));
}

DWORD InitDlgReboot(diR *diDialog)
{
  diDialog->dwShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF_MEDIUM)) == NULL)
    return(1);
  GetPrivateProfileString("Messages", "DLG_REBOOT_TITLE", "", diDialog->szTitle, MAX_BUF_MEDIUM, szFileIniInstall);

  return(0);
}

void DeInitDlgReboot(diR *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
}

HRESULT InitSetupGeneral()
{
  char szBuf[MAX_BUF];

  sgProduct.dwMode               = NORMAL;
  sgProduct.dwCustomType         = ST_RADIO0;
  sgProduct.dwNumberOfComponents = 0;
  sgProduct.bLockPath            = FALSE;

  if((sgProduct.szPath                        = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szSubPath                     = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProgramName                 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szCompanyName                 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProductName                 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szUninstallFilename           = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szUserAgent                   = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProgramFolderName           = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProgramFolderPath           = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szAlternateArchiveSearchPath  = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szParentProcessFilename       = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((szTempSetupPath                         = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szSiteSelectorDescription               = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if(GetPrivateProfileString("Messages", "CB_DEFAULT", "", szBuf, sizeof(szBuf), szFileIniInstall))
    lstrcpy(szSiteSelectorDescription, szBuf);

  return(0);
}

void DeInitSetupGeneral()
{
  FreeMemory(&(sgProduct.szPath));
  FreeMemory(&(sgProduct.szSubPath));
  FreeMemory(&(sgProduct.szProgramName));
  FreeMemory(&(sgProduct.szCompanyName));
  FreeMemory(&(sgProduct.szProductName));
  FreeMemory(&(sgProduct.szUninstallFilename));
  FreeMemory(&(sgProduct.szUserAgent));
  FreeMemory(&(sgProduct.szProgramFolderName));
  FreeMemory(&(sgProduct.szProgramFolderPath));
  FreeMemory(&(sgProduct.szAlternateArchiveSearchPath));
  FreeMemory(&(sgProduct.szParentProcessFilename));
  FreeMemory(&(szTempSetupPath));
  FreeMemory(&(szSiteSelectorDescription));
}

HRESULT InitSDObject()
{
  if((siSDObject.szXpcomFile = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szXpcomDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szNoAds = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szSilent = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szExecution = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szConfirmInstall = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szExtractMsg = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szExe = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szExeParam = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szXpcomFilePath = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitSDObject()
{
  FreeMemory(&(siSDObject.szXpcomFile));
  FreeMemory(&(siSDObject.szXpcomDir));
  FreeMemory(&(siSDObject.szNoAds));
  FreeMemory(&(siSDObject.szSilent));
  FreeMemory(&(siSDObject.szExecution));
  FreeMemory(&(siSDObject.szConfirmInstall));
  FreeMemory(&(siSDObject.szExtractMsg));
  FreeMemory(&(siSDObject.szExe));
  FreeMemory(&(siSDObject.szExeParam));
  FreeMemory(&(siSDObject.szXpcomFilePath));
}

HRESULT InitSXpcomFile()
{
  if((siCFXpcomFile.szSource = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siCFXpcomFile.szDestination = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siCFXpcomFile.szMessage = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  siCFXpcomFile.bCleanup         = TRUE;
  siCFXpcomFile.ullInstallSize   = 0;
  return(0);
}

void DeInitSXpcomFile()
{
  FreeMemory(&(siCFXpcomFile.szSource));
  FreeMemory(&(siCFXpcomFile.szDestination));
  FreeMemory(&(siCFXpcomFile.szMessage));
}

siC *CreateSiCNode()
{
  siC *siCNode;

  if((siCNode = NS_GlobalAlloc(sizeof(struct sinfoComponent))) == NULL)
    exit(1);

  siCNode->dwAttributes             = 0;
  siCNode->ullInstallSize           = 0;
  siCNode->ullInstallSizeSystem     = 0;
  siCNode->ullInstallSizeArchive    = 0;
  siCNode->lRandomInstallPercentage = 0;
  siCNode->lRandomInstallValue      = 0;
  siCNode->bForceUpgrade            = FALSE;

  if((siCNode->szArchiveName = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szArchiveNameUncompressed = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szArchivePath = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szDestinationPath = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szDescriptionLong = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szParameter = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szReferenceName = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);

  siCNode->iNetRetries      = 0;
  siCNode->iCRCRetries      = 0;
  siCNode->iNetTimeOuts     = 0;
  siCNode->siCDDependencies = NULL;
  siCNode->siCDDependees    = NULL;
  siCNode->Next             = NULL;
  siCNode->Prev             = NULL;

  return(siCNode);
}

void SiCNodeInsert(siC **siCHead, siC *siCTemp)
{
  if(*siCHead == NULL)
  {
    *siCHead          = siCTemp;
    (*siCHead)->Next  = *siCHead;
    (*siCHead)->Prev  = *siCHead;
  }
  else
  {
    siCTemp->Next           = *siCHead;
    siCTemp->Prev           = (*siCHead)->Prev;
    (*siCHead)->Prev->Next  = siCTemp;
    (*siCHead)->Prev        = siCTemp;
  }
}

void SiCNodeDelete(siC *siCTemp)
{
  if(siCTemp != NULL)
  {
    DeInitSiCDependencies(siCTemp->siCDDependencies);
    DeInitSiCDependencies(siCTemp->siCDDependees);

    siCTemp->Next->Prev = siCTemp->Prev;
    siCTemp->Prev->Next = siCTemp->Next;
    siCTemp->Next       = NULL;
    siCTemp->Prev       = NULL;

    FreeMemory(&(siCTemp->szDestinationPath));
    FreeMemory(&(siCTemp->szArchivePath));
    FreeMemory(&(siCTemp->szArchiveName));
    FreeMemory(&(siCTemp->szArchiveNameUncompressed));
    FreeMemory(&(siCTemp->szParameter));
    FreeMemory(&(siCTemp->szReferenceName));
    FreeMemory(&(siCTemp->szDescriptionLong));
    FreeMemory(&(siCTemp->szDescriptionShort));
    FreeMemory(&siCTemp);
  }
}

siCD *CreateSiCDepNode()
{
  siCD *siCDepNode;

  if((siCDepNode = NS_GlobalAlloc(sizeof(struct sinfoComponentDep))) == NULL)
    exit(1);

  if((siCDepNode->szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCDepNode->szReferenceName = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  siCDepNode->Next = NULL;
  siCDepNode->Prev = NULL;

  return(siCDepNode);
}

void SiCDepNodeInsert(siCD **siCDepHead, siCD *siCDepTemp)
{
  if(*siCDepHead == NULL)
  {
    *siCDepHead          = siCDepTemp;
    (*siCDepHead)->Next  = *siCDepHead;
    (*siCDepHead)->Prev  = *siCDepHead;
  }
  else
  {
    siCDepTemp->Next           = *siCDepHead;
    siCDepTemp->Prev           = (*siCDepHead)->Prev;
    (*siCDepHead)->Prev->Next  = siCDepTemp;
    (*siCDepHead)->Prev        = siCDepTemp;
  }
}

void SiCDepNodeDelete(siCD *siCDepTemp)
{
  if(siCDepTemp != NULL)
  {
    siCDepTemp->Next->Prev = siCDepTemp->Prev;
    siCDepTemp->Prev->Next = siCDepTemp->Next;
    siCDepTemp->Next       = NULL;
    siCDepTemp->Prev       = NULL;

    FreeMemory(&(siCDepTemp->szDescriptionShort));
    FreeMemory(&(siCDepTemp->szReferenceName));
    FreeMemory(&siCDepTemp);
  }
}

ssi *CreateSsiSiteSelectorNode()
{
  ssi *ssiNode;

  if((ssiNode = NS_GlobalAlloc(sizeof(struct ssInfo))) == NULL)
    exit(1);
  if((ssiNode->szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((ssiNode->szDomain = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((ssiNode->szIdentifier = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);

  ssiNode->Next = NULL;
  ssiNode->Prev = NULL;

  return(ssiNode);
}

void SsiSiteSelectorNodeInsert(ssi **ssiHead, ssi *ssiTemp)
{
  if(*ssiHead == NULL)
  {
    *ssiHead          = ssiTemp;
    (*ssiHead)->Next  = *ssiHead;
    (*ssiHead)->Prev  = *ssiHead;
  }
  else
  {
    ssiTemp->Next           = *ssiHead;
    ssiTemp->Prev           = (*ssiHead)->Prev;
    (*ssiHead)->Prev->Next  = ssiTemp;
    (*ssiHead)->Prev        = ssiTemp;
  }
}

void SsiSiteSelectorNodeDelete(ssi *ssiTemp)
{
  if(ssiTemp != NULL)
  {
    ssiTemp->Next->Prev = ssiTemp->Prev;
    ssiTemp->Prev->Next = ssiTemp->Next;
    ssiTemp->Next       = NULL;
    ssiTemp->Prev       = NULL;

    FreeMemory(&(ssiTemp->szDescription));
    FreeMemory(&(ssiTemp->szDomain));
    FreeMemory(&(ssiTemp->szIdentifier));
    FreeMemory(&ssiTemp);
  }
}

HRESULT SiCNodeGetAttributes(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->dwAttributes);

      ++dwCount;
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->dwAttributes);

        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(-1);
}

void SiCNodeSetAttributes(DWORD dwIndex, DWORD dwAttributes, BOOL bSet, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount  = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
      {
        if(bSet)
          siCTemp->dwAttributes |= dwAttributes;
        else
          siCTemp->dwAttributes &= ~dwAttributes;
      }

      ++dwCount;
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
        {
          if(bSet)
            siCTemp->dwAttributes |= dwAttributes;
          else
            siCTemp->dwAttributes &= ~dwAttributes;
        }

        ++dwCount;
      }

      siCTemp = siCTemp->Next;
    }
  }
}

BOOL IsInList(DWORD dwCurrentItem, DWORD dwItems, DWORD *dwItemsSelected)
{
  DWORD i;

  for(i = 0; i < dwItems; i++)
  {
    if(dwItemsSelected[i] == dwCurrentItem)
      return(TRUE);
  }

  return(FALSE);
}

void SiCNodeSetItemsSelected(DWORD dwSetupType)
{
  siC  *siCNode;
  char szBuf[MAX_BUF];
  char szIndex0[MAX_BUF];
  char szSTSection[MAX_BUF];
  char szComponentKey[MAX_BUF];
  char szComponentSection[MAX_BUF];
  DWORD dwIndex0;

  lstrcpy(szSTSection, "Setup Type");
  itoa(dwSetupType, szBuf, 10);
  lstrcat(szSTSection, szBuf);

  /* for each component in the global list, unset its SELECTED attribute */
  siCNode = siComponents;
  do
  {
    if(siCNode == NULL)
      break;

    siCNode->dwAttributes &= ~SIC_SELECTED;

    /* Force Upgrade needs to be performed here because the user has just
     * selected the destination path for the product.  The destination path is
     * critical to the checking of the Force Upgrade. */
    ResolveForceUpgrade(siCNode);
    siCNode = siCNode->Next;
  } while((siCNode != NULL) && (siCNode != siComponents));

  /* for each component in a setup type, set its attribute to be SELECTED */
  dwIndex0 = 0;
  itoa(dwIndex0, szIndex0, 10);
  lstrcpy(szComponentKey, "C");
  lstrcat(szComponentKey, szIndex0);
  GetPrivateProfileString(szSTSection, szComponentKey, "", szComponentSection, sizeof(szComponentSection), szFileIniConfig);
  while(*szComponentSection != '\0')
  {
    if((siCNode = SiCNodeFind(siComponents, szComponentSection)) != NULL)
    {
      if((siCNode->lRandomInstallPercentage != 0) &&
         (siCNode->lRandomInstallPercentage <= siCNode->lRandomInstallValue) &&
         !(siCNode->dwAttributes & SIC_DISABLED))
        /* Random Install Percentage check passed *and* the component
         * is not DISABLED */
        siCNode->dwAttributes &= ~SIC_SELECTED;
      else if(sgProduct.dwCustomType != dwSetupType)
        /* Setup Type other than custom detected, so
         * make sure all components from this Setup Type
         * is selected (regardless if it's DISABLED or not). */
        siCNode->dwAttributes |= SIC_SELECTED;
      else if(!(siCNode->dwAttributes & SIC_DISABLED) && !siCNode->bForceUpgrade)
      {
        /* Custom setup type detected and the component is
         * not DISABLED and FORCE_UPGRADE.  Reset the component's 
         * attribute to default.  If the component is DISABLED and
         * happens not be SELECTED in the config.ini file, we leave
         * it as is.  The user will see the component in the Options
         * Dialogs as not selected and DISABLED.  Not sure why we
         * want this, but marketing might find it useful someday. */
        GetPrivateProfileString(siCNode->szReferenceName, "Attributes", "", szBuf, sizeof(szBuf), szFileIniConfig);
        siCNode->dwAttributes = ParseComponentAttributes(szBuf);
      }
    }

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    lstrcpy(szComponentKey, "C");
    lstrcat(szComponentKey, szIndex0);
    GetPrivateProfileString(szSTSection, szComponentKey, "", szComponentSection, sizeof(szComponentSection), szFileIniConfig);
  }
}

char *SiCNodeGetReferenceName(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->szReferenceName);

      ++dwCount;
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->szReferenceName);
      
        ++dwCount;
      }

      siCTemp = siCTemp->Next;
    }
  }
  return(NULL);
}

char *SiCNodeGetDescriptionShort(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->szDescriptionShort);

      ++dwCount;
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->szDescriptionShort);
      
        ++dwCount;
      }

      siCTemp = siCTemp->Next;
    }
  }
  return(NULL);
}

char *SiCNodeGetDescriptionLong(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->szDescriptionLong);

      ++dwCount;
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->szDescriptionLong);
      
        ++dwCount;
      }

      siCTemp = siCTemp->Next;
    }
  }
  return(NULL);
}

ULONGLONG SiCNodeGetInstallSize(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount   = 0;
  siC   *siCTemp  = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->ullInstallSize);

      ++dwCount;
    }
    
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->ullInstallSize);
      
        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(0L);
}

ULONGLONG SiCNodeGetInstallSizeSystem(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount   = 0;
  siC   *siCTemp  = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->ullInstallSizeSystem);

      ++dwCount;
    }
    
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->ullInstallSizeSystem);
      
        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(0L);
}

ULONGLONG SiCNodeGetInstallSizeArchive(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount   = 0;
  siC   *siCTemp  = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->ullInstallSizeArchive);

      ++dwCount;
    }
    
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->ullInstallSizeArchive);
      
        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(0L);
}

/* retrieve Index of node containing short description */
int SiCNodeGetIndexDS(char *szInDescriptionShort)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(lstrcmpi(szInDescriptionShort, siCTemp->szDescriptionShort) == 0)
      return(dwCount);

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(lstrcmpi(szInDescriptionShort, siCTemp->szDescriptionShort) == 0)
        return(dwCount);
      
      ++dwCount;
      siCTemp = siCTemp->Next;
    }
  }
  return(-1);
}

/* retrieve Index of node containing Reference Name */
int SiCNodeGetIndexRN(char *szInReferenceName)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(lstrcmpi(szInReferenceName, siCTemp->szReferenceName) == 0)
      return(dwCount);

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(lstrcmpi(szInReferenceName, siCTemp->szReferenceName) == 0)
        return(dwCount);
      
      ++dwCount;
      siCTemp = siCTemp->Next;
    }
  }
  return(-1);
}

siC *SiCNodeGetObject(DWORD dwIndex, BOOL bIncludeInvisibleObjs, DWORD dwACFlag)
{
  DWORD dwCount = -1;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if((bIncludeInvisibleObjs) &&
      ((dwACFlag == AC_ALL) ||
      ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
      ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      ++dwCount;
    }
    else if((!(siCTemp->dwAttributes & SIC_INVISIBLE)) &&
           ((dwACFlag == AC_ALL) ||
           ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
           ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      ++dwCount;
    }

    if(dwIndex == dwCount)
      return(siCTemp);

    siCTemp = siCTemp->Next;
    while((siCTemp != siComponents) && (siCTemp != NULL))
    {
      if(bIncludeInvisibleObjs)
      {
        ++dwCount;
      }
      else if((!(siCTemp->dwAttributes & SIC_INVISIBLE)) &&
             ((dwACFlag == AC_ALL) ||
             ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
             ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        ++dwCount;
      }

      if(dwIndex == dwCount)
        return(siCTemp);
      
      siCTemp = siCTemp->Next;
    }
  }
  return(NULL);
}

DWORD GetAdditionalComponentsCount()
{
  DWORD dwCount  = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(siCTemp->dwAttributes & SIC_ADDITIONAL)
    {
      ++dwCount;
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != siComponents) && (siCTemp != NULL))
    {
      if(siCTemp->dwAttributes & SIC_ADDITIONAL)
      {
        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(dwCount);
}

dsN *CreateDSNode()
{
  dsN *dsNode;

  if((dsNode = NS_GlobalAlloc(sizeof(struct diskSpaceNode))) == NULL)
    exit(1);

  dsNode->ullSpaceRequired = 0;

  if((dsNode->szVDSPath = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((dsNode->szPath = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  dsNode->Next             = dsNode;
  dsNode->Prev             = dsNode;

  return(dsNode);
}

void DsNodeInsert(dsN **dsNHead, dsN *dsNTemp)
{
  if(*dsNHead == NULL)
  {
    *dsNHead          = dsNTemp;
    (*dsNHead)->Next  = *dsNHead;
    (*dsNHead)->Prev  = *dsNHead;
  }
  else
  {
    dsNTemp->Next           = *dsNHead;
    dsNTemp->Prev           = (*dsNHead)->Prev;
    (*dsNHead)->Prev->Next  = dsNTemp;
    (*dsNHead)->Prev        = dsNTemp;
  }
}

void DsNodeDelete(dsN **dsNTemp)
{
  if(*dsNTemp != NULL)
  {
    (*dsNTemp)->Next->Prev = (*dsNTemp)->Prev;
    (*dsNTemp)->Prev->Next = (*dsNTemp)->Next;
    (*dsNTemp)->Next       = NULL;
    (*dsNTemp)->Prev       = NULL;

    FreeMemory(&((*dsNTemp)->szVDSPath));
    FreeMemory(&((*dsNTemp)->szPath));
    FreeMemory(dsNTemp);
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

/* returns the value in kilobytes */
ULONGLONG GetDiskSpaceRequired(DWORD dwType)
{
  ULONGLONG ullTotalSize = 0;
  siC       *siCTemp     = siComponents;

  if(siCTemp != NULL)
  {
    if(siCTemp->dwAttributes & SIC_SELECTED)
    {
      switch(dwType)
      {
        case DSR_DESTINATION:
          ullTotalSize += siCTemp->ullInstallSize;
          break;

        case DSR_SYSTEM:
          ullTotalSize += siCTemp->ullInstallSizeSystem;
          break;

        case DSR_TEMP:
        case DSR_DOWNLOAD_SIZE:
          if((LocateJar(siCTemp, NULL, 0, gbPreviousUnfinishedDownload) == AP_NOT_FOUND) ||
             (dwType == DSR_DOWNLOAD_SIZE))
            ullTotalSize += siCTemp->ullInstallSizeArchive;
          break;
      }
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(siCTemp->dwAttributes & SIC_SELECTED)
      {
        switch(dwType)
        {
          case DSR_DESTINATION:
            ullTotalSize += siCTemp->ullInstallSize;
            break;

          case DSR_SYSTEM:
            ullTotalSize += siCTemp->ullInstallSizeSystem;
            break;

          case DSR_TEMP:
          case DSR_DOWNLOAD_SIZE:
            if((LocateJar(siCTemp, NULL, 0, gbPreviousUnfinishedDownload) == AP_NOT_FOUND) ||
               (dwType == DSR_DOWNLOAD_SIZE))
              ullTotalSize += siCTemp->ullInstallSizeArchive;
            break;
        }
      }

      siCTemp = siCTemp->Next;
    }
  }

  /* add the amount of disk space it will take for the 
     xpinstall engine in the TEMP area */
  if(dwType == DSR_TEMP)
    ullTotalSize += siCFXpcomFile.ullInstallSize;

  return(ullTotalSize);
}

int LocateExistingPath(char *szPath, char *szExistingPath, DWORD dwExistingPathSize)
{
  char szBuf[MAX_BUF];

  lstrcpy(szExistingPath, szPath);
  AppendBackSlash(szExistingPath, dwExistingPathSize);
  while((FileExists(szExistingPath) == FALSE))
  {
    RemoveBackSlash(szExistingPath);
    ParsePath(szExistingPath, szBuf, sizeof(szBuf), FALSE, PP_PATH_ONLY);
    lstrcpy(szExistingPath, szBuf);
    AppendBackSlash(szExistingPath, dwExistingPathSize);
  }
  return(WIZ_OK);
}

/* returns the value in bytes */
ULONGLONG GetDiskSpaceAvailable(LPSTR szPath)
{
  char            szTempPath[MAX_BUF];
  char            szBuf[MAX_BUF];
  char            szExistingPath[MAX_BUF];
  char            szBuf2[MAX_BUF];
  ULARGE_INTEGER  uliFreeBytesAvailableToCaller;
  ULARGE_INTEGER  uliTotalNumberOfBytesToCaller;
  ULARGE_INTEGER  uliTotalNumberOfFreeBytes;
  ULONGLONG       ullReturn = 0;
  DWORD           dwSectorsPerCluster;
  DWORD           dwBytesPerSector;
  DWORD           dwNumberOfFreeClusters;
  DWORD           dwTotalNumberOfClusters;

  if((gSystemInfo.dwOSType & OS_WIN95_DEBUTE) && (NS_GetDiskFreeSpace != NULL))
  {
    ParsePath(szPath, szTempPath, MAX_BUF, FALSE, PP_ROOT_ONLY);
    NS_GetDiskFreeSpace(szTempPath, 
                        &dwSectorsPerCluster,
                        &dwBytesPerSector,
                        &dwNumberOfFreeClusters,
                        &dwTotalNumberOfClusters);
    ullReturn = ((ULONGLONG)dwBytesPerSector * (ULONGLONG)dwSectorsPerCluster * (ULONGLONG)dwNumberOfFreeClusters);
  }
  else if(NS_GetDiskFreeSpaceEx != NULL)
  {
    LocateExistingPath(szPath, szExistingPath, sizeof(szExistingPath));
    AppendBackSlash(szExistingPath, sizeof(szExistingPath));

    /* Appearently under Win9x, the path still needs to be in 8.3 format
     * or GetDiskFreeSpaceEx() will fail. */
    if(gSystemInfo.dwOSType & OS_WIN9x)
    {
      lstrcpy(szBuf, szExistingPath);
      GetShortPathName(szBuf, szExistingPath, sizeof(szExistingPath));
    }

    if(NS_GetDiskFreeSpaceEx(szExistingPath,
                             &uliFreeBytesAvailableToCaller,
                             &uliTotalNumberOfBytesToCaller,
                             &uliTotalNumberOfFreeBytes) == FALSE)
    {
      char szEDeterminingDiskSpace[MAX_BUF];

      if(GetPrivateProfileString("Messages", "ERROR_DETERMINING_DISK_SPACE", "", szEDeterminingDiskSpace, sizeof(szEDeterminingDiskSpace), szFileIniInstall))
      {
        lstrcpy(szBuf2, "\n    ");
        lstrcat(szBuf2, szPath);
        wsprintf(szBuf, szEDeterminingDiskSpace, szBuf2);
        PrintError(szBuf, ERROR_CODE_SHOW);
      }
    }
    ullReturn = uliFreeBytesAvailableToCaller.QuadPart;
  }

  if(ullReturn > 1024)
    ullReturn /= 1024;
  else
    ullReturn = 0;

  return(ullReturn);
}

HRESULT ErrorMsgDiskSpace(ULONGLONG ullDSAvailable, ULONGLONG ullDSRequired, LPSTR szPath, BOOL bCrutialMsg)
{
  char      szBuf0[MAX_BUF];
  char      szBuf1[MAX_BUF];
  char      szBuf2[MAX_BUF];
  char      szBuf3[MAX_BUF];
  char      szBufRootPath[MAX_BUF];
  char      szBufMsg[MAX_BUF];
  char      szDSAvailable[MAX_BUF];
  char      szDSRequired[MAX_BUF];
  char      szDlgDiskSpaceCheckTitle[MAX_BUF];
  char      szDlgDiskSpaceCheckMsg[MAX_BUF];
  DWORD     dwDlgType;

  if(!GetPrivateProfileString("Messages", "DLG_DISK_SPACE_CHECK_TITLE", "", szDlgDiskSpaceCheckTitle, sizeof(szDlgDiskSpaceCheckTitle), szFileIniInstall))
    exit(1);

  if(bCrutialMsg)
  {
    dwDlgType = MB_RETRYCANCEL;
    if(!GetPrivateProfileString("Messages", "DLG_DISK_SPACE_CHECK_CRUTIAL_MSG", "", szDlgDiskSpaceCheckMsg, sizeof(szDlgDiskSpaceCheckMsg), szFileIniInstall))
      exit(1);
  }
  else
  {
    dwDlgType = MB_OK;
    if(!GetPrivateProfileString("Messages", "DLG_DISK_SPACE_CHECK_MSG", "", szDlgDiskSpaceCheckMsg, sizeof(szDlgDiskSpaceCheckMsg), szFileIniInstall))
      exit(1);
  }

  ParsePath(szPath, szBufRootPath, sizeof(szBufRootPath), FALSE, PP_ROOT_ONLY);
  RemoveBackSlash(szBufRootPath);
  lstrcpy(szBuf0, szPath);
  RemoveBackSlash(szBuf0);

  _ui64toa(ullDSAvailable, szDSAvailable, 10);
  _ui64toa(ullDSRequired, szDSRequired, 10);

  wsprintf(szBuf1, "\n\n    %s\n\n    ", szBuf0);
  wsprintf(szBuf2, "%s KB\n    ",        szDSRequired);
  wsprintf(szBuf3, "%s KB\n\n",          szDSAvailable);
  wsprintf(szBufMsg, szDlgDiskSpaceCheckMsg, szBufRootPath, szBuf1, szBuf2, szBuf3);

  if((sgProduct.dwMode != SILENT) && (sgProduct.dwMode != AUTO))
  {
    return(MessageBox(hWndMain, szBufMsg, szDlgDiskSpaceCheckTitle, dwDlgType | MB_ICONEXCLAMATION | MB_DEFBUTTON2 | MB_APPLMODAL | MB_SETFOREGROUND));
  }
  else if(sgProduct.dwMode == AUTO)
  {
    ShowMessage(szBufMsg, TRUE);
    Delay(5);
    ShowMessage(szBufMsg, FALSE);
    exit(1);
  }

  return(IDCANCEL);
}

BOOL ContainsReparseTag(char *szPath, char *szReparsePath, DWORD dwReparsePathSize)
{
  BOOL  bFoundReparseTag  = FALSE;
  DWORD dwOriginalLen     = lstrlen(szPath);
  DWORD dwLen             = dwOriginalLen;
  char  *szPathTmp        = NULL;
  char  *szBuf            = NULL;

  if((szPathTmp = NS_GlobalAlloc(dwLen + 1)) == NULL)
    exit(1);
  if((szBuf = NS_GlobalAlloc(dwLen + 1)) == NULL)
    exit(1);

  lstrcpy(szPathTmp, szPath);
  while((szPathTmp[dwLen - 2] != ':') && (szPathTmp[dwLen - 2] != '\\'))
  {
    if(FileExists(szPathTmp) & FILE_ATTRIBUTE_REPARSE_POINT)
    {
      if((DWORD)lstrlen(szPathTmp) <= dwReparsePathSize)
        lstrcpy(szReparsePath, szPathTmp);
      else
        ZeroMemory(szReparsePath, dwReparsePathSize);

      bFoundReparseTag = TRUE;
      break;
    }

    lstrcpy(szBuf, szPathTmp);
    RemoveBackSlash(szBuf);
    ParsePath(szBuf, szPathTmp, dwOriginalLen + 1, FALSE, PP_PATH_ONLY);
    dwLen = lstrlen(szPathTmp);
  }

  FreeMemory(&szBuf);
  FreeMemory(&szPathTmp);
  return(bFoundReparseTag);
}

void UpdatePathDiskSpaceRequired(LPSTR szPath, ULONGLONG ullSize, dsN **dsnComponentDSRequirement)
{
  BOOL  bFound = FALSE;
  dsN   *dsnTemp = *dsnComponentDSRequirement;
  char  szReparsePath[MAX_BUF];
  char  szVDSPath[MAX_BUF];
  char  szRootPath[MAX_BUF];

  if(ullSize > 0)
  {
    ParsePath(szPath, szRootPath, sizeof(szRootPath), FALSE, PP_ROOT_ONLY);

    if(gSystemInfo.dwOSType & OS_WIN95_DEBUTE)
      // check for Win95 debute version
      lstrcpy(szVDSPath, szRootPath);
    else if((gSystemInfo.dwOSType & OS_NT5) &&
             ContainsReparseTag(szPath, szReparsePath, sizeof(szReparsePath)))
    {
      // check for Reparse Tag (mount points under NT5 only)
      if(*szReparsePath == '\0')
      {
        // szReparsePath is not big enough to hold the Reparse path.  This is a
        // non fatal error.  Keep going using szPath instead.
        lstrcpy(szVDSPath, szPath);
      }
      else if(GetDiskSpaceAvailable(szReparsePath) == GetDiskSpaceAvailable(szPath))
        // Check for user quota on path.  It is very unlikely that the user quota
        // for the path will be the same as the quota for its root path when user
        // quota is enabled.
        //
        // If it happens to be the same, then I'm assuming that the disk space on
        // the path's root path will decrease at the same rate as the path with
        // user quota enabled.
        //
        // If user quota is not enabled on the folder, then use the root path.
        lstrcpy(szVDSPath, szReparsePath);
      else
        lstrcpy(szVDSPath, szPath);
    }
    else if(GetDiskSpaceAvailable(szRootPath) == GetDiskSpaceAvailable(szPath))
      // Check for user quota on path.  It is very unlikely that the user quota
      // for the path will be the same as the quota for its root path when user
      // quota is enabled.
      //
      // If it happens to be the same, then I'm assuming that the disk space on
      // the path's root path will decrease at the same rate as the path with
      // user quota enabled.
      //
      // If user quota is not enabled on the folder, then use the root path.
      lstrcpy(szVDSPath, szRootPath);
    else
      lstrcpy(szVDSPath, szPath);

    do
    {
      if(*dsnComponentDSRequirement == NULL)
      {
        *dsnComponentDSRequirement = CreateDSNode();
        dsnTemp = *dsnComponentDSRequirement;
        strcpy(dsnTemp->szVDSPath, szVDSPath);
        strcpy(dsnTemp->szPath, szPath);
        dsnTemp->ullSpaceRequired = ullSize;
        bFound = TRUE;
      }
      else if(lstrcmpi(dsnTemp->szVDSPath, szVDSPath) == 0)
      {
        dsnTemp->ullSpaceRequired += ullSize;
        bFound = TRUE;
      }
      else
        dsnTemp = dsnTemp->Next;

    } while((dsnTemp != *dsnComponentDSRequirement) && (dsnTemp != NULL) && (bFound == FALSE));

    if(bFound == FALSE)
    {
      dsnTemp = CreateDSNode();
      strcpy(dsnTemp->szVDSPath, szVDSPath);
      strcpy(dsnTemp->szPath, szPath);
      dsnTemp->ullSpaceRequired = ullSize;
      DsNodeInsert(dsnComponentDSRequirement, dsnTemp);
    }
  }
}

HRESULT InitComponentDiskSpaceInfo(dsN **dsnComponentDSRequirement)
{
  DWORD     dwIndex0;
  siC       *siCObject = NULL;
  HRESULT   hResult    = 0;
  char      szBuf[MAX_BUF];
  char      szIndex0[MAX_BUF];
  char      szSysPath[MAX_BUF];
  char      szBufSysPath[MAX_BUF];
  char      szBufTempPath[MAX_BUF];

  if(GetSystemDirectory(szSysPath, MAX_BUF) == 0)
  {
    ZeroMemory(szSysPath, MAX_BUF);
    ZeroMemory(szBufSysPath, MAX_BUF);
  }
  else
  {
    ParsePath(szSysPath, szBufSysPath, sizeof(szBufSysPath), FALSE, PP_ROOT_ONLY);
    AppendBackSlash(szBufSysPath, sizeof(szBufSysPath));
  }

  ParsePath(szTempDir, szBufTempPath, sizeof(szBufTempPath), FALSE, PP_ROOT_ONLY);
  AppendBackSlash(szBufTempPath, sizeof(szBufTempPath));

  dwIndex0 = 0;
  itoa(dwIndex0, szIndex0, 10);
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    if(siCObject->dwAttributes & SIC_SELECTED)
    {
      if(*(siCObject->szDestinationPath) == '\0')
        lstrcpy(szBuf, sgProduct.szPath);
      else
        lstrcpy(szBuf, siCObject->szDestinationPath);

      AppendBackSlash(szBuf, sizeof(szBuf));
      UpdatePathDiskSpaceRequired(szBuf, siCObject->ullInstallSize, dsnComponentDSRequirement);

      if(*szTempDir != '\0')
        UpdatePathDiskSpaceRequired(szTempDir, siCObject->ullInstallSizeArchive, dsnComponentDSRequirement);

      if(*szSysPath != '\0')
        UpdatePathDiskSpaceRequired(szSysPath, siCObject->ullInstallSizeSystem, dsnComponentDSRequirement);
    }

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  /* take the uncompressed size of Xpcom into account */
  if(*szTempDir != '\0')
    UpdatePathDiskSpaceRequired(szTempDir, siCFXpcomFile.ullInstallSize, dsnComponentDSRequirement);

  return(hResult);
}

HRESULT VerifyDiskSpace()
{
  ULONGLONG ullDSAvailable;
  HRESULT   hRetValue = FALSE;
  dsN       *dsnTemp = NULL;

  DeInitDSNode(&gdsnComponentDSRequirement);
  InitComponentDiskSpaceInfo(&gdsnComponentDSRequirement);
  if(gdsnComponentDSRequirement != NULL)
  {
    dsnTemp = gdsnComponentDSRequirement;

    do
    {
      if(dsnTemp != NULL)
      {
        ullDSAvailable = GetDiskSpaceAvailable(dsnTemp->szVDSPath);
        if(ullDSAvailable < dsnTemp->ullSpaceRequired)
        {
          hRetValue = ErrorMsgDiskSpace(ullDSAvailable, dsnTemp->ullSpaceRequired, dsnTemp->szPath, FALSE);
          break;
        }

        dsnTemp = dsnTemp->Next;
      }
    } while((dsnTemp != NULL) && (dsnTemp != gdsnComponentDSRequirement));
  }
  return(hRetValue);
}

/* Function: ParseOSType
 *
 * Input: char *
 *
 * Return: DWORD
 *
 * Description: This function parses an input string (szOSType) for specific
 * string keys:
 *     WIN95_DEBUTE, WIN95, WIN98, NT3, NT4, NT5, NT50, NT51
 *
 * It then stores the information in a DWORD, each bit corresponding to a
 * particular OS type.
 */ 
DWORD ParseOSType(char *szOSType)
{
  char  szBuf[MAX_BUF];
  DWORD dwOSType = 0;

  lstrcpy(szBuf, szOSType);
  strupr(szBuf);

  if(strstr(szBuf, "WIN95 DEBUTE"))
    dwOSType |= OS_WIN95_DEBUTE;
  if(strstr(szBuf, "WIN95") &&
     !strstr(szBuf, "WIN95 DEBUTE"))
    dwOSType |= OS_WIN95;
  if(strstr(szBuf, "WIN98"))
    dwOSType |= OS_WIN98;
  if(strstr(szBuf, "NT3"))
    dwOSType |= OS_NT3;
  if(strstr(szBuf, "NT4"))
    dwOSType |= OS_NT4;
  if(strstr(szBuf, "NT50"))
    dwOSType |= OS_NT50;
  if(strstr(szBuf, "NT51"))
    dwOSType |= OS_NT51;
  if(strstr(szBuf, "NT5") &&
     !strstr(szBuf, "NT50") &&
     !strstr(szBuf, "NT51"))
    dwOSType |= OS_NT5;

  return(dwOSType);
}

HRESULT ParseComponentAttributes(char *szAttribute)
{
  char  szBuf[MAX_BUF];
  DWORD dwAttributes = 0;

  lstrcpy(szBuf, szAttribute);
  strupr(szBuf);

  if(strstr(szBuf, "SELECTED") && !strstr(szBuf, "UNSELECTED"))
    /* Check for SELECTED attribute only. Since strstr will also return a
     * SELECTED attribute for UNSELECTED, we need to make sure it's not the
     * UNSELECTED attribute.
     *
     * Even though we're already checking for the UNSELECTED attribute
     * later on in this function, we're trying not to be order dependent.
     */
    dwAttributes |= SIC_SELECTED;
  if(strstr(szBuf, "INVISIBLE"))
    dwAttributes |= SIC_INVISIBLE;
  if(strstr(szBuf, "LAUNCHAPP"))
    dwAttributes |= SIC_LAUNCHAPP;
  if(strstr(szBuf, "DOWNLOAD_ONLY"))
    dwAttributes |= SIC_DOWNLOAD_ONLY;
  if(strstr(szBuf, "ADDITIONAL"))
    dwAttributes |= SIC_ADDITIONAL;
  if(strstr(szBuf, "DISABLED"))
    dwAttributes |= SIC_DISABLED;
  if(strstr(szBuf, "UNSELECTED"))
    dwAttributes &= ~SIC_SELECTED;
  if(strstr(szBuf, "FORCE_UPGRADE"))
    dwAttributes |= SIC_FORCE_UPGRADE;
  if(strstr(szBuf, "IGNORE_DOWNLOAD_ERROR"))
    dwAttributes |= SIC_IGNORE_DOWNLOAD_ERROR;
  if(strstr(szBuf, "IGNORE_XPINSTALL_ERROR"))
    dwAttributes |= SIC_IGNORE_XPINSTALL_ERROR;
  if(strstr(szBuf, "UNCOMPRESS"))
    dwAttributes |= SIC_UNCOMPRESS;

  return(dwAttributes);
}

long RandomSelect()
{
  long lArbitrary = 0;

  srand((unsigned)time(NULL));
  lArbitrary = rand() % 100;
  return(lArbitrary);
}

siC *SiCNodeFind(siC *siCHeadNode, char *szInReferenceName)
{
  siC *siCNode = siCHeadNode;

  do
  {
    if(siCNode == NULL)
      break;

    if(lstrcmpi(siCNode->szReferenceName, szInReferenceName) == 0)
      return(siCNode);

    siCNode = siCNode->Next;
  } while((siCNode != NULL) && (siCNode != siCHeadNode));

  return(NULL);
}

BOOL ResolveForceUpgrade(siC *siCObject)
{
  DWORD dwIndex;
  char  szFilePath[MAX_BUF];
  char  szKey[MAX_BUF_TINY];
  char  szForceUpgradeFile[MAX_BUF];

  siCObject->bForceUpgrade = FALSE;
  if(siCObject->dwAttributes & SIC_FORCE_UPGRADE)
  {
    dwIndex = 0;
    BuildNumberedString(dwIndex, NULL, "Force Upgrade File", szKey, sizeof(szKey));
    GetPrivateProfileString(siCObject->szReferenceName, szKey, "", szForceUpgradeFile, sizeof(szForceUpgradeFile), szFileIniConfig);
    while(*szForceUpgradeFile != '\0')
    {
      DecryptString(szFilePath, szForceUpgradeFile);
      if(FileExists(szFilePath))
      {
        siCObject->bForceUpgrade = TRUE;

        /* Found at least one file, so break out of while loop */
        break;
      }

      BuildNumberedString(++dwIndex, NULL, "Force Upgrade File", szKey, sizeof(szKey));
      GetPrivateProfileString(siCObject->szReferenceName, szKey, "", szForceUpgradeFile, sizeof(szForceUpgradeFile), szFileIniConfig);
    }

    if(siCObject->bForceUpgrade)
    {
      siCObject->dwAttributes |= SIC_SELECTED;
      siCObject->dwAttributes |= SIC_DISABLED;
    }
    else
      /* Make sure to unset the DISABLED bit.  If the Setup Type is other than
       * Cutsom, then we don't care if it's DISABLED or not because this flag
       * is only used in the Custom dialogs.
       *
       * If the Setup Type is Custom and this component is DISABLED by default
       * via the config.ini, it's default value will be restored in the
       * SiCNodeSetItemsSelected() function that called ResolveForceUpgrade(). */
      siCObject->dwAttributes &= ~SIC_DISABLED;
  }
  return(siCObject->bForceUpgrade);
}

void InitSiComponents(char *szFileIni)
{
  DWORD dwIndex0;
  DWORD dwIndex1;
  int   iCurrentLoop;
  char  szIndex0[MAX_BUF];
  char  szIndex1[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szComponentKey[MAX_BUF];
  char  szComponentSection[MAX_BUF];
  char  szDependency[MAX_BUF];
  char  szDependee[MAX_BUF];
  char  szSTSection[MAX_BUF];
  char  szDPSection[MAX_BUF];
  siC   *siCTemp            = NULL;
  siCD  *siCDepTemp         = NULL;
  siCD  *siCDDependeeTemp   = NULL;

  /* clean up the list before reading new components given the Setup Type */
  DeInitSiComponents(&siComponents);

  /* Parse the Setup Type sections in reverse order because
   * the Custom Setup Type is always last.  It needs to be parsed
   * first because the component list it has will be shown in its
   * other custom dialogs.  Order matters! */
  for(iCurrentLoop = 3; iCurrentLoop >= 0; iCurrentLoop--)
  {
    lstrcpy(szSTSection, "Setup Type");
    itoa(iCurrentLoop, szBuf, 10);
    lstrcat(szSTSection, szBuf);

    /* read in each component given a setup type */
    dwIndex0 = 0;
    itoa(dwIndex0, szIndex0, 10);
    lstrcpy(szComponentKey, "C");
    lstrcat(szComponentKey, szIndex0);
    GetPrivateProfileString(szSTSection, szComponentKey, "", szComponentSection, sizeof(szComponentSection), szFileIni);
    while(*szComponentSection != '\0')
    {
      GetPrivateProfileString(szComponentSection, "Archive", "", szBuf, sizeof(szBuf), szFileIni);
      if((*szBuf != '\0') && (SiCNodeFind(siComponents, szComponentSection) == NULL))
      {
        /* create and initialize empty node */
        siCTemp = CreateSiCNode();

        /* store name of archive for component */
        lstrcpy(siCTemp->szArchiveName, szBuf);

        /* store name of the uncompressed archive for the component */
        GetPrivateProfileString(szComponentSection,
                                "Archive Uncompressed",
                                "",
                                siCTemp->szArchiveNameUncompressed,
                                sizeof(szBuf),
                                szFileIni);
        
        /* get short description of component */
        GetPrivateProfileString(szComponentSection, "Description Short", "", szBuf, sizeof(szBuf), szFileIni);
        lstrcpy(siCTemp->szDescriptionShort, szBuf);

        /* get long description of component */
        GetPrivateProfileString(szComponentSection, "Description Long", "", szBuf, sizeof(szBuf), szFileIni);
        lstrcpy(siCTemp->szDescriptionLong, szBuf);

        /* get commandline parameter for component */
        GetPrivateProfileString(szComponentSection, "Parameter", "", siCTemp->szParameter, MAX_BUF, szFileIni);

        /* set reference name for component */
        lstrcpy(siCTemp->szReferenceName, szComponentSection);

        /* get install size required in destination for component.  Sould be in Kilobytes */
        GetPrivateProfileString(szComponentSection, "Install Size", "", szBuf, sizeof(szBuf), szFileIni);
        if(*szBuf != '\0')
          siCTemp->ullInstallSize = _atoi64(szBuf);
        else
          siCTemp->ullInstallSize = 0;

        /* get install size required in system for component.  Sould be in Kilobytes */
        GetPrivateProfileString(szComponentSection, "Install Size System", "", szBuf, sizeof(szBuf), szFileIni);
        if(*szBuf != '\0')
          siCTemp->ullInstallSizeSystem = _atoi64(szBuf);
        else
          siCTemp->ullInstallSizeSystem = 0;

        /* get install size required in temp for component.  Sould be in Kilobytes */
        GetPrivateProfileString(szComponentSection, "Install Size Archive", "", szBuf, sizeof(szBuf), szFileIni);
        if(*szBuf != '\0')
          siCTemp->ullInstallSizeArchive = _atoi64(szBuf);
        else
          siCTemp->ullInstallSizeArchive = 0;

        /* get attributes of component */
        GetPrivateProfileString(szComponentSection, "Attributes", "", szBuf, sizeof(szBuf), szFileIni);
        siCTemp->dwAttributes = ParseComponentAttributes(szBuf);

        /* get the random percentage value and select or deselect the component (by default) for
         * installation */
        GetPrivateProfileString(szComponentSection, "Random Install Percentage", "", szBuf, sizeof(szBuf), szFileIni);
        if(*szBuf != '\0')
        {
          siCTemp->lRandomInstallPercentage = atol(szBuf);
          if(siCTemp->lRandomInstallPercentage != 0)
            siCTemp->lRandomInstallValue = RandomSelect();
        }

        /* get all dependencies for this component */
        dwIndex1 = 0;
        itoa(dwIndex1, szIndex1, 10);
        lstrcpy(szDependency, "Dependency");
        lstrcat(szDependency, szIndex1);
        GetPrivateProfileString(szComponentSection, szDependency, "", szBuf, sizeof(szBuf), szFileIni);
        while(*szBuf != '\0')
        {
          /* create and initialize empty node */
          siCDepTemp = CreateSiCDepNode();

          /* store name of archive for component */
          lstrcpy(siCDepTemp->szReferenceName, szBuf);

          /* inserts the newly created component into the global component list */
          SiCDepNodeInsert(&(siCTemp->siCDDependencies), siCDepTemp);

          ProcessWindowsMessages();
          ++dwIndex1;
          itoa(dwIndex1, szIndex1, 10);
          lstrcpy(szDependency, "Dependency");
          lstrcat(szDependency, szIndex1);
          GetPrivateProfileString(szComponentSection, szDependency, "", szBuf, sizeof(szBuf), szFileIni);
        }

        /* get all dependees for this component */
        dwIndex1 = 0;
        itoa(dwIndex1, szIndex1, 10);
        lstrcpy(szDependee, "Dependee");
        lstrcat(szDependee, szIndex1);
        GetPrivateProfileString(szComponentSection, szDependee, "", szBuf, sizeof(szBuf), szFileIni);
        while(*szBuf != '\0')
        {
          /* create and initialize empty node */
          siCDDependeeTemp = CreateSiCDepNode();

          /* store name of archive for component */
          lstrcpy(siCDDependeeTemp->szReferenceName, szBuf);

          /* inserts the newly created component into the global component list */
          SiCDepNodeInsert(&(siCTemp->siCDDependees), siCDDependeeTemp);

          ProcessWindowsMessages();
          ++dwIndex1;
          itoa(dwIndex1, szIndex1, 10);
          lstrcpy(szDependee, "Dependee");
          lstrcat(szDependee, szIndex1);
          GetPrivateProfileString(szComponentSection, szDependee, "", szBuf, sizeof(szBuf), szFileIni);
        }

        // locate previous path if necessary
        lstrcpy(szDPSection, szComponentSection);
        lstrcat(szDPSection, "-Destination Path");
        if(LocatePreviousPath(szDPSection, siCTemp->szDestinationPath, MAX_PATH) == FALSE)
          ZeroMemory(siCTemp->szDestinationPath, MAX_PATH);

        /* inserts the newly created component into the global component list */
        SiCNodeInsert(&siComponents, siCTemp);
      }

      ProcessWindowsMessages();
      ++dwIndex0;
      itoa(dwIndex0, szIndex0, 10);
      lstrcpy(szComponentKey, "C");
      lstrcat(szComponentKey, szIndex0);
      GetPrivateProfileString(szSTSection, szComponentKey, "", szComponentSection, sizeof(szComponentSection), szFileIni);
    }
  }

  sgProduct.dwNumberOfComponents = dwIndex0;
}

void ResetComponentAttributes(char *szFileIni)
{
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szComponentItem[MAX_BUF];
  siC   *siCTemp;
  DWORD dwCounter;

  for(dwCounter = 0; dwCounter < sgProduct.dwNumberOfComponents; dwCounter++)
  {
    itoa(dwCounter, szIndex, 10);
    lstrcpy(szComponentItem, "Component");
    lstrcat(szComponentItem, szIndex);

    siCTemp = SiCNodeGetObject(dwCounter, TRUE, AC_ALL);
    GetPrivateProfileString(szComponentItem, "Attributes", "", szBuf, sizeof(szBuf), szFileIni);
    siCTemp->dwAttributes = ParseComponentAttributes(szBuf);
  }
}

void UpdateSiteSelector()
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szKDescription[MAX_BUF];
  char  szDescription[MAX_BUF];
  char  szKDomain[MAX_BUF];
  char  szDomain[MAX_BUF];
  char  szFileIniRedirect[MAX_BUF];
  ssi   *ssiSiteSelectorTemp;

  lstrcpy(szFileIniRedirect, szTempDir);
  AppendBackSlash(szFileIniRedirect, sizeof(szFileIniRedirect));
  lstrcat(szFileIniRedirect, FILE_INI_REDIRECT);

  if(FileExists(szFileIniRedirect) == FALSE)
    return;

  /* get all dependees for this component */
  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szKDescription, "Description");
  lstrcpy(szKDomain,      "Domain");
  lstrcat(szKDescription, szIndex);
  lstrcat(szKDomain,      szIndex);
  GetPrivateProfileString("Site Selector", szKDescription, "", szDescription, sizeof(szDescription), szFileIniRedirect);
  while(*szDescription != '\0')
  {
    if(lstrcmpi(szDescription, szSiteSelectorDescription) == 0)
    {
      GetPrivateProfileString("Site Selector", szKDomain, "", szDomain, sizeof(szDomain), szFileIniRedirect);
      if(*szDomain != '\0')
      {
        ssiSiteSelectorTemp = SsiGetNode(szDescription);
        if(ssiSiteSelectorTemp != NULL)
        {
          lstrcpy(ssiSiteSelectorTemp->szDomain, szDomain);
        }
        else
        {
          /* no match found for the given domain description, so assume there's nothing
           * to change. just return. */
          return;
        }
      }
      else
      {
        /* found matched description, but domain was not set, so assume there's no
         * redirect required, just return. */
        return;
      }
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szKDescription, "Description");
    lstrcpy(szKDomain,      "Domain");
    lstrcat(szKDescription, szIndex);
    lstrcat(szKDomain,      szIndex);
    ZeroMemory(szDescription, sizeof(szDescription));
    ZeroMemory(szDomain,      sizeof(szDomain));
    GetPrivateProfileString("Site Selector", szKDescription, "", szDescription, sizeof(szDescription), szFileIniRedirect);
  }
}

void InitSiteSelector(char *szFileIni)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szKDescription[MAX_BUF];
  char  szDescription[MAX_BUF];
  char  szKDomain[MAX_BUF];
  char  szDomain[MAX_BUF];
  char  szKIdentifier[MAX_BUF];
  char  szIdentifier[MAX_BUF];
  ssi   *ssiSiteSelectorNewNode;

  ssiSiteSelector = NULL;

  /* get all dependees for this component */
  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szKDescription, "Description");
  lstrcpy(szKDomain,      "Domain");
  lstrcpy(szKIdentifier,  "Identifier");
  lstrcat(szKDescription, szIndex);
  lstrcat(szKDomain,      szIndex);
  lstrcat(szKIdentifier,  szIndex);
  GetPrivateProfileString("Site Selector", szKDescription, "", szDescription, sizeof(szDescription), szFileIni);
  while(*szDescription != '\0')
  {
    /* if the Domain and Identifier are not set, then skip */
    GetPrivateProfileString("Site Selector", szKDomain,     "", szDomain,     sizeof(szDomain), szFileIni);
    GetPrivateProfileString("Site Selector", szKIdentifier, "", szIdentifier, sizeof(szIdentifier), szFileIni);
    if((*szDomain != '\0') && (*szIdentifier != '\0'))
    {
      /* create and initialize empty node */
      ssiSiteSelectorNewNode = CreateSsiSiteSelectorNode();

      lstrcpy(ssiSiteSelectorNewNode->szDescription, szDescription);
      lstrcpy(ssiSiteSelectorNewNode->szDomain,      szDomain);
      lstrcpy(ssiSiteSelectorNewNode->szIdentifier,  szIdentifier);

      /* inserts the newly created node into the global node list */
      SsiSiteSelectorNodeInsert(&(ssiSiteSelector), ssiSiteSelectorNewNode);
    }

    ProcessWindowsMessages();
    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szKDescription, "Description");
    lstrcpy(szKDomain,      "Domain");
    lstrcpy(szKIdentifier,  "Identifier");
    lstrcat(szKDescription, szIndex);
    lstrcat(szKDomain,      szIndex);
    lstrcat(szKIdentifier,  szIndex);
    ZeroMemory(szDescription, sizeof(szDescription));
    ZeroMemory(szDomain,      sizeof(szDomain));
    ZeroMemory(szIdentifier,  sizeof(szIdentifier));
    GetPrivateProfileString("Site Selector", szKDescription, "", szDescription, sizeof(szDescription), szFileIni);
  }
}

void InitErrorMessageStream(char *szFileIni)
{
  char szBuf[MAX_BUF_TINY];

  GetPrivateProfileString("Message Stream",
                          "Status",
                          "",
                          szBuf,
                          sizeof(szBuf),
                          szFileIni);

  if(lstrcmpi(szBuf, "disabled") == 0)
    gErrorMessageStream.bEnabled = FALSE;
  else
    gErrorMessageStream.bEnabled = TRUE;

  GetPrivateProfileString("Message Stream",
                          "url",
                          "",
                          gErrorMessageStream.szURL,
                          sizeof(gErrorMessageStream.szURL),
                          szFileIni);

  GetPrivateProfileString("Message Stream",
                          "Show Confirmation",
                          "",
                          szBuf,
                          sizeof(szBuf),
                          szFileIni);
  if(strcmpi(szBuf, "FALSE") == 0)
    gErrorMessageStream.bShowConfirmation = FALSE;
  else
    gErrorMessageStream.bShowConfirmation = TRUE;

  GetPrivateProfileString("Message Stream",
                          "Confirmation Message",
                          "",
                          gErrorMessageStream.szConfirmationMessage,
                          sizeof(szBuf),
                          szFileIni);

  gErrorMessageStream.bSendMessage = FALSE;
  gErrorMessageStream.dwMessageBufSize = MAX_BUF;
  gErrorMessageStream.szMessage = NS_GlobalAlloc(gErrorMessageStream.dwMessageBufSize);
}

void DeInitErrorMessageStream()
{
  FreeMemory(&gErrorMessageStream.szMessage);
}

#ifdef SSU_DEBUG
void ViewSiComponentsDependency(char *szBuffer, char *szIndentation, siC *siCNode)
{
  siC  *siCNodeTemp;
  siCD *siCDependencyTemp;

  siCDependencyTemp = siCNode->siCDDependencies;
  if(siCDependencyTemp != NULL)
  {
    char  szIndentationPadding[MAX_BUF];
    DWORD dwIndex;

    lstrcpy(szIndentationPadding, szIndentation);
    lstrcat(szIndentationPadding, "    ");

    do
    {
      lstrcat(szBuffer, szIndentationPadding);
      lstrcat(szBuffer, siCDependencyTemp->szReferenceName);
      lstrcat(szBuffer, "::");

      if((dwIndex = SiCNodeGetIndexRN(siCDependencyTemp->szReferenceName)) != -1)
        lstrcat(szBuffer, SiCNodeGetDescriptionShort(dwIndex, TRUE, AC_ALL));
      else
        lstrcat(szBuffer, "Component does not exist");

      lstrcat(szBuffer, ":");
      lstrcat(szBuffer, "\n");

      if(dwIndex != -1)
      {
        if((siCNodeTemp = SiCNodeGetObject(dwIndex, TRUE, AC_ALL)) != NULL)
          ViewSiComponentsDependency(szBuffer, szIndentationPadding, siCNodeTemp);
        else
          lstrcat(szBuffer, "Node not found");
      }

      siCDependencyTemp = siCDependencyTemp->Next;
    }while((siCDependencyTemp != NULL) && (siCDependencyTemp != siCNode->siCDDependencies));
  }
}

void ViewSiComponentsDependee(char *szBuffer, char *szIndentation, siC *siCNode)
{
  siC  *siCNodeTemp;
  siCD *siCDependeeTemp;

  siCDependeeTemp = siCNode->siCDDependees;
  if(siCDependeeTemp != NULL)
  {
    char  szIndentationPadding[MAX_BUF];
    DWORD dwIndex;

    lstrcpy(szIndentationPadding, szIndentation);
    lstrcat(szIndentationPadding, "    ");

    do
    {
      lstrcat(szBuffer, szIndentationPadding);
      lstrcat(szBuffer, siCDependeeTemp->szReferenceName);
      lstrcat(szBuffer, "::");

      if((dwIndex = SiCNodeGetIndexRN(siCDependeeTemp->szReferenceName)) != -1)
        lstrcat(szBuffer, SiCNodeGetDescriptionShort(dwIndex, TRUE, AC_ALL));
      else
        lstrcat(szBuffer, "Component does not exist");

      lstrcat(szBuffer, ":");
      lstrcat(szBuffer, "\n");

      if(dwIndex != -1)
      {
        if((siCNodeTemp = SiCNodeGetObject(dwIndex, TRUE, AC_ALL)) != NULL)
          ViewSiComponentsDependency(szBuffer, szIndentationPadding, siCNodeTemp);
        else
          lstrcat(szBuffer, "Node not found");
      }

      siCDependeeTemp = siCDependeeTemp->Next;
    }while((siCDependeeTemp != NULL) && (siCDependeeTemp != siCNode->siCDDependees));
  }
}

void ViewSiComponents()
{
  char  szBuf[MAX_BUF];
  siC   *siCTemp = siComponents;

  // build dependency list
  ZeroMemory(szBuf, sizeof(szBuf));
  lstrcpy(szBuf, "Dependency:\n");

  do
  {
    if(siCTemp == NULL)
      break;

    lstrcat(szBuf, "    ");
    lstrcat(szBuf, siCTemp->szReferenceName);
    lstrcat(szBuf, "::");
    lstrcat(szBuf, siCTemp->szDescriptionShort);
    lstrcat(szBuf, ":\n");

    ViewSiComponentsDependency(szBuf, "    ", siCTemp);

    siCTemp = siCTemp->Next;
  } while((siCTemp != NULL) && (siCTemp != siComponents));

  MessageBox(hWndMain, szBuf, NULL, MB_ICONEXCLAMATION);

  // build dependee list
  ZeroMemory(szBuf, sizeof(szBuf));
  lstrcpy(szBuf, "Dependee:\n");

  do
  {
    if(siCTemp == NULL)
      break;

    lstrcat(szBuf, "    ");
    lstrcat(szBuf, siCTemp->szReferenceName);
    lstrcat(szBuf, "::");
    lstrcat(szBuf, siCTemp->szDescriptionShort);
    lstrcat(szBuf, ":\n");

    ViewSiComponentsDependee(szBuf, "    ", siCTemp);

    siCTemp = siCTemp->Next;
  } while((siCTemp != NULL) && (siCTemp != siComponents));

  MessageBox(hWndMain, szBuf, NULL, MB_ICONEXCLAMATION);
}
#endif /* SSU_DEBUG */

void DeInitSiCDependencies(siCD *siCDDependencies)
{
  siCD   *siCDepTemp;
  
  if(siCDDependencies == NULL)
  {
    return;
  }
  else if((siCDDependencies->Prev == NULL) || (siCDDependencies->Prev == siCDDependencies))
  {
    SiCDepNodeDelete(siCDDependencies);
    return;
  }
  else
  {
    siCDepTemp = siCDDependencies->Prev;
  }

  while(siCDepTemp != siCDDependencies)
  {
    SiCDepNodeDelete(siCDepTemp);
    siCDepTemp = siCDDependencies->Prev;
  }
  SiCDepNodeDelete(siCDepTemp);
}

void DeInitSiComponents(siC **siCHeadNode)
{
  siC   *siCTemp;
  
  if((*siCHeadNode) == NULL)
  {
    return;
  }
  else if(((*siCHeadNode)->Prev == NULL) || ((*siCHeadNode)->Prev == (*siCHeadNode)))
  {
    SiCNodeDelete((*siCHeadNode));
    return;
  }
  else
  {
    siCTemp = (*siCHeadNode)->Prev;
  }

  while(siCTemp != (*siCHeadNode))
  {
    SiCNodeDelete(siCTemp);
    siCTemp = (*siCHeadNode)->Prev;
  }
  SiCNodeDelete(siCTemp);
}

void DeInitDSNode(dsN **dsnComponentDSRequirement)
{
  dsN *dsNTemp;
  
  if(*dsnComponentDSRequirement == NULL)
  {
    return;
  }
  else if(((*dsnComponentDSRequirement)->Prev == NULL) || ((*dsnComponentDSRequirement)->Prev == *dsnComponentDSRequirement))
  {
    DsNodeDelete(dsnComponentDSRequirement);
    return;
  }
  else
  {
    dsNTemp = (*dsnComponentDSRequirement)->Prev;
  }

  while(dsNTemp != *dsnComponentDSRequirement)
  {
    DsNodeDelete(&dsNTemp);
    dsNTemp = (*dsnComponentDSRequirement)->Prev;
  }
  DsNodeDelete(dsnComponentDSRequirement);
}

BOOL ResolveComponentDependency(siCD *siCDInDependency)
{
  int     dwIndex;
  siCD    *siCDepTemp = siCDInDependency;
  BOOL    bMoreToResolve = FALSE;
  DWORD   dwAttrib;

  if(siCDepTemp != NULL)
  {
    if((dwIndex = SiCNodeGetIndexRN(siCDepTemp->szReferenceName)) != -1)
    {
      dwAttrib = SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL);
      if(!(dwAttrib & SIC_SELECTED) && !(dwAttrib & SIC_DISABLED))
      {
        bMoreToResolve = TRUE;
        SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE, TRUE, AC_ALL);
      }
    }

    siCDepTemp = siCDepTemp->Next;
    while((siCDepTemp != NULL) && (siCDepTemp != siCDInDependency))
    {
      if((dwIndex = SiCNodeGetIndexRN(siCDepTemp->szReferenceName)) != -1)
      {
        dwAttrib = SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL);
        if(!(dwAttrib & SIC_SELECTED) && !(dwAttrib & SIC_DISABLED))
        {
          bMoreToResolve = TRUE;
          SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE, TRUE, AC_ALL);
        }
      }

      siCDepTemp = siCDepTemp->Next;
    }
  }
  return(bMoreToResolve);
}

BOOL ResolveDependencies(DWORD dwIndex)
{
  BOOL  bMoreToResolve  = FALSE;
  DWORD dwCount         = 0;
  siC   *siCTemp        = siComponents;

  if(siCTemp != NULL)
  {
    /* can resolve specific component or all components (-1) */
    if((dwIndex == dwCount) || (dwIndex == -1))
    {
      if(SiCNodeGetAttributes(dwCount, TRUE, AC_ALL) & SIC_SELECTED)
      {
         bMoreToResolve = ResolveComponentDependency(siCTemp->siCDDependencies);
         if(dwIndex == dwCount)
         {
           return(bMoreToResolve);
         }
      }
    }

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      /* can resolve specific component or all components (-1) */
      if((dwIndex == dwCount) || (dwIndex == -1))
      {
        if(SiCNodeGetAttributes(dwCount, TRUE, AC_ALL) & SIC_SELECTED)
        {
           bMoreToResolve = ResolveComponentDependency(siCTemp->siCDDependencies);
           if(dwIndex == dwCount)
           {
             return(bMoreToResolve);
           }
        }
      }

      ++dwCount;
      siCTemp = siCTemp->Next;
    }
  }
  return(bMoreToResolve);
}

BOOL ResolveComponentDependee(siCD *siCDInDependee)
{
  int     dwIndex;
  siCD    *siCDDependeeTemp   = siCDInDependee;
  BOOL    bAtLeastOneSelected = FALSE;

  if(siCDDependeeTemp != NULL)
  {
    if((dwIndex = SiCNodeGetIndexRN(siCDDependeeTemp->szReferenceName)) != -1)
    {
      if((SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL) & SIC_SELECTED) == TRUE)
      {
        bAtLeastOneSelected = TRUE;
      }
    }

    siCDDependeeTemp = siCDDependeeTemp->Next;
    while((siCDDependeeTemp != NULL) && (siCDDependeeTemp != siCDInDependee))
    {
      if((dwIndex = SiCNodeGetIndexRN(siCDDependeeTemp->szReferenceName)) != -1)
      {
        if((SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL) & SIC_SELECTED) == TRUE)
        {
          bAtLeastOneSelected = TRUE;
        }
      }

      siCDDependeeTemp = siCDDependeeTemp->Next;
    }
  }
  return(bAtLeastOneSelected);
}

ssi* SsiGetNode(LPSTR szDescription)
{
  ssi *ssiSiteSelectorTemp = ssiSiteSelector;

  do
  {
    if(ssiSiteSelectorTemp == NULL)
      break;

    if(lstrcmpi(ssiSiteSelectorTemp->szDescription, szDescription) == 0)
      return(ssiSiteSelectorTemp);

    ssiSiteSelectorTemp = ssiSiteSelectorTemp->Next;
  } while((ssiSiteSelectorTemp != NULL) && (ssiSiteSelectorTemp != ssiSiteSelector));

  return(NULL);
}

void ResolveDependees(LPSTR szToggledReferenceName)
{
  BOOL  bAtLeastOneSelected;
  BOOL  bMoreToResolve  = FALSE;
  siC   *siCTemp        = siComponents;
  DWORD dwIndex;
  DWORD dwAttrib;

  do
  {
    if(siCTemp == NULL)
      break;

    if((siCTemp->siCDDependees != NULL) &&
       (lstrcmpi(siCTemp->szReferenceName, szToggledReferenceName) != 0))
    {
      bAtLeastOneSelected = ResolveComponentDependee(siCTemp->siCDDependees);
      if(bAtLeastOneSelected == FALSE)
      {
        if((dwIndex = SiCNodeGetIndexRN(siCTemp->szReferenceName)) != -1)
        {
          dwAttrib = SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL);
          if((dwAttrib & SIC_SELECTED) && !(dwAttrib & SIC_DISABLED))
          {
            SiCNodeSetAttributes(dwIndex, SIC_SELECTED, FALSE, TRUE, AC_ALL);
            bMoreToResolve = TRUE;
          }
        }
      }
      else
      {
        if((dwIndex = SiCNodeGetIndexRN(siCTemp->szReferenceName)) != -1)
        {
          dwAttrib = SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL);
          if(!(dwAttrib & SIC_SELECTED) && !(dwAttrib & SIC_DISABLED))
          {
            SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE, TRUE, AC_ALL);
            bMoreToResolve = TRUE;
          }
        }
      }
    }

    siCTemp = siCTemp->Next;
  } while((siCTemp != NULL) && (siCTemp != siComponents));

  if(bMoreToResolve == TRUE)
    ResolveDependees(szToggledReferenceName);
}

void PrintUsage(void)
{
  char szBuf[MAX_BUF];
  char szUsageMsg[MAX_BUF];
  char szProcessFilename[MAX_BUF];

  /* -h: this help
   * -a [path]: Alternate archive search path
   * -n [filename]: setup's parent's process filename
   * -ma: run setup in Auto mode
   * -ms: run setup in Silent mode
   * -ira: ignore the [RunAppX] sections
   * -ispf: ignore the [Program FolderX] sections that show
   *        the Start Menu shortcut folder at the end of installation.
   */

  if(*sgProduct.szParentProcessFilename != '\0')
    lstrcpy(szProcessFilename, sgProduct.szParentProcessFilename);
  else
  {
    GetModuleFileName(NULL, szBuf, sizeof(szBuf));
    ParsePath(szBuf, szProcessFilename, sizeof(szProcessFilename), FALSE, PP_FILENAME_ONLY);
  }

  GetPrivateProfileString("Strings", "UsageMsg Usage", "", szBuf, sizeof(szBuf), szFileIniConfig);
  wsprintf(szUsageMsg, szBuf, szProcessFilename, "\n", "\n", "\n", "\n", "\n", "\n", "\n", "\n", "\n");

  PrintError(szUsageMsg, ERROR_CODE_HIDE);
}

DWORD ParseCommandLine(LPSTR lpszCmdLine)
{
  char  szArgVBuf[MAX_BUF];
  int   i;
  int   iArgC;

#ifdef XXX_DEBUG
  char  szBuf[MAX_BUF];
  char  szOutputStr[MAX_BUF];
#endif

  iArgC = GetArgC(lpszCmdLine);

#ifdef XXX_DEBUG
  wsprintf(szOutputStr, "ArgC: %d\n", iArgC);
#endif

  i = 0;
  while(i < iArgC)
  {
    GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));

    if(!lstrcmpi(szArgVBuf, "-h") || !lstrcmpi(szArgVBuf, "/h"))
    {
      PrintUsage();
      return(WIZ_ERROR_UNDEFINED);
    }
    else if(!lstrcmpi(szArgVBuf, "-a") || !lstrcmpi(szArgVBuf, "/a"))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      lstrcpy(sgProduct.szAlternateArchiveSearchPath, szArgVBuf);
    }
    else if(!lstrcmpi(szArgVBuf, "-n") || !lstrcmpi(szArgVBuf, "/n"))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      lstrcpy(sgProduct.szParentProcessFilename, szArgVBuf);
    }
    else if(!lstrcmpi(szArgVBuf, "-ma") || !lstrcmpi(szArgVBuf, "/ma"))
      SetSetupRunMode("AUTO");
    else if(!lstrcmpi(szArgVBuf, "-ms") || !lstrcmpi(szArgVBuf, "/ms"))
      SetSetupRunMode("SILENT");
    else if(!lstrcmpi(szArgVBuf, "-ira") || !lstrcmpi(szArgVBuf, "/ira"))
      /* ignore [RunAppX] sections */
      gbIgnoreRunAppX = TRUE;
    else if(!lstrcmpi(szArgVBuf, "-ispf") || !lstrcmpi(szArgVBuf, "/ispf"))
      /* ignore [Program FolderX] sections */
      gbIgnoreProgramFolderX = TRUE;

#ifdef XXX_DEBUG
    itoa(i, szBuf, 10);
    lstrcat(szOutputStr, "    ");
    lstrcat(szOutputStr, szBuf);
    lstrcat(szOutputStr, ": ");
    lstrcat(szOutputStr, szArgVBuf);
    lstrcat(szOutputStr, "\n");
#endif

    ++i;
  }

#ifdef XXX_DEBUG
  MessageBox(NULL, szOutputStr, "Output", MB_OK);
#endif
  return(WIZ_OK);
}

void GetAlternateArchiveSearchPath(LPSTR lpszCmdLine)
{
  char  szBuf[MAX_PATH];
  LPSTR lpszAASPath;
  LPSTR lpszEndPath;
  LPSTR lpszEndQuote;

  if(lstrcpy(szBuf, lpszCmdLine))
  {
    if((lpszAASPath = strstr(szBuf, "-a")) == NULL)
      return;
    else
      lpszAASPath += 2;

    if(*lpszAASPath == '\"')
    {
      lpszAASPath = lpszAASPath + 1;
      if((lpszEndQuote = strstr(lpszAASPath, "\"")) != NULL)
      {
        *lpszEndQuote = '\0';
      }
    }
    else if((lpszEndPath = strstr(lpszAASPath, " ")) != NULL)
    {
      *lpszEndPath = '\0';
    }

    lstrcpy(sgProduct.szAlternateArchiveSearchPath, lpszAASPath);
  }
}

BOOL CheckProcessWin95(NS_CreateSnapshot pCreateToolhelp32Snapshot, NS_ProcessWalk pProcessWalkFirst, NS_ProcessWalk pProcessWalkNext, LPSTR szProcessName)
{
  BOOL            bRv             = FALSE;
  HANDLE          hCreateSnapshot = NULL;
  PROCESSENTRY32  peProcessEntry;
  
  hCreateSnapshot = pCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if(hCreateSnapshot == (HANDLE)-1)
    return(bRv);

  peProcessEntry.dwSize = sizeof(PROCESSENTRY32);
  if(pProcessWalkFirst(hCreateSnapshot, &peProcessEntry))
  {
    do
    {
      char  szBuf[MAX_BUF];

      ParsePath(peProcessEntry.szExeFile, szBuf, sizeof(szBuf), FALSE, PP_FILENAME_ONLY);
      
      /* do process name string comparison here! */
      if(lstrcmpi(szBuf, szProcessName) == 0)
      {
        bRv = TRUE;
        break;
      }

    } while((bRv == FALSE) && pProcessWalkNext(hCreateSnapshot, &peProcessEntry));
  }

  CloseHandle(hCreateSnapshot);
  return(bRv);
}

BOOL CheckForProcess(LPSTR szProcessName, DWORD dwProcessName)
{
  HANDLE            hKernel                   = NULL;
  NS_CreateSnapshot pCreateToolhelp32Snapshot = NULL;
  NS_ProcessWalk    pProcessWalkFirst         = NULL;
  NS_ProcessWalk    pProcessWalkNext          = NULL;
  BOOL              bDoWin95Check             = TRUE;

  if((hKernel = GetModuleHandle("kernel32.dll")) == NULL)
    return(FALSE);

  pCreateToolhelp32Snapshot = (NS_CreateSnapshot)GetProcAddress(hKernel, "CreateToolhelp32Snapshot");
  pProcessWalkFirst         = (NS_ProcessWalk)GetProcAddress(hKernel,    "Process32First");
  pProcessWalkNext          = (NS_ProcessWalk)GetProcAddress(hKernel,    "Process32Next");

  if(pCreateToolhelp32Snapshot && pProcessWalkFirst && pProcessWalkNext)
    return(CheckProcessWin95(pCreateToolhelp32Snapshot, pProcessWalkFirst, pProcessWalkNext, szProcessName));
  else
    return(CheckProcessNT4(szProcessName, dwProcessName));
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
  ParsePath(szFile, szPath, sizeof(szPath), FALSE, PP_PATH_ONLY);

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
  char  szProcessName[MAX_BUF];
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

    GetPrivateProfileString(szSection, "Message", "", szMessage, sizeof(szMessage), szFileIniConfig);
    if(GetPrivateProfileString(szSection, "Process Name", "", szProcessName, sizeof(szProcessName), szFileIniConfig) != 0L)
    {
      if(*szProcessName != '\0')
      {
        /* If an instance is found, call PreCheckInstance first */
        if(CheckForProcess(szProcessName, sizeof(szProcessName)) == TRUE)
          PreCheckInstance(szSection, szFileIniConfig);

        if(CheckForProcess(szProcessName, sizeof(szProcessName)) == TRUE)
        {
          if(*szMessage != '\0')
          {
            switch(sgProduct.dwMode)
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

      /* Process Name= key existed, and has been processed, so continue looking for more */
      continue;
    }

    /* Process Name= key did not exist, so look for other keys */
    dwRv0 = GetPrivateProfileString(szSection, "Class Name",  "", szClassName,  sizeof(szClassName), szFileIniConfig);
    dwRv1 = GetPrivateProfileString(szSection, "Window Name", "", szWindowName, sizeof(szWindowName), szFileIniConfig);
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

      /* If an instance is found, call PreCheckInstance first */
      if((hwndFW = FindWindow(szCN, szWN)) != NULL)
        PreCheckInstance(szSection, szFileIniConfig);

      if((hwndFW = FindWindow(szCN, szWN)) != NULL)
      {
        if(*szMessage != '\0')
        {
          switch(sgProduct.dwMode)
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

int CRCCheckArchivesStartup(char *szCorruptedArchiveList, DWORD dwCorruptedArchiveListSize, BOOL bIncludeTempPath)
{
  DWORD dwIndex0;
  DWORD dwFileCounter;
  siC   *siCObject = NULL;
  char  szArchivePathWithFilename[MAX_BUF];
  char  szArchivePath[MAX_BUF];
  char  szMsgCRCCheck[MAX_BUF];
  char  szPartiallyDownloadedFilename[MAX_BUF];
  int   iRv;
  int   iResult;

  if(szCorruptedArchiveList != NULL)
    ZeroMemory(szCorruptedArchiveList, dwCorruptedArchiveListSize);

  GetSetupCurrentDownloadFile(szPartiallyDownloadedFilename,
                              sizeof(szPartiallyDownloadedFilename));
  GetPrivateProfileString("Strings", "Message Verifying Archives", "", szMsgCRCCheck, sizeof(szMsgCRCCheck), szFileIniConfig);
  ShowMessage(szMsgCRCCheck, TRUE);
  
  iResult           = WIZ_CRC_PASS;
  dwIndex0          = 0;
  dwFileCounter     = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    /* only download jars if not already in the local machine */
    iRv = LocateJar(siCObject, szArchivePath, sizeof(szArchivePath), bIncludeTempPath);
    if((iRv != AP_NOT_FOUND) &&
       (lstrcmpi(szPartiallyDownloadedFilename,
                 siCObject->szArchiveName) != 0))
    {
      if(lstrlen(szArchivePath) < sizeof(szArchivePathWithFilename))
        lstrcpy(szArchivePathWithFilename, szArchivePath);

      AppendBackSlash(szArchivePathWithFilename, sizeof(szArchivePathWithFilename));
      if((lstrlen(szArchivePathWithFilename) + lstrlen(siCObject->szArchiveName)) < sizeof(szArchivePathWithFilename))
        lstrcat(szArchivePathWithFilename, siCObject->szArchiveName);

      if(CheckForArchiveExtension(szArchivePathWithFilename))
      {
        /* Make sure that the Archive that failed is located in the TEMP
         * folder.  This means that it was downloaded at one point and not
         * simply uncompressed from the self-extracting exe file. */
        if(VerifyArchive(szArchivePathWithFilename) != ZIP_OK)
        {
          if(iRv == AP_TEMP_PATH)
            DeleteFile(szArchivePathWithFilename);
          else if(szCorruptedArchiveList != NULL)
          {
            iResult = WIZ_CRC_FAIL;
            if((DWORD)(lstrlen(szCorruptedArchiveList) + lstrlen(siCObject->szArchiveName + 1)) < dwCorruptedArchiveListSize)
            {
              lstrcat(szCorruptedArchiveList, "        ");
              lstrcat(szCorruptedArchiveList, siCObject->szArchiveName);
              lstrcat(szCorruptedArchiveList, "\n");
            }
            else
            {
              iResult = WIZ_OUT_OF_MEMORY;
              break;
            }
          }
        }
      }
    }

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }
  ShowMessage(szMsgCRCCheck, FALSE);
  return(iResult);
}

int StartupCheckArchives(void)
{
  int  iRv;
  char szBuf[MAX_BUF_SMALL];
  char szCorruptedArchiveList[MAX_BUF];

  iRv = CRCCheckArchivesStartup(szCorruptedArchiveList, sizeof(szCorruptedArchiveList), gbPreviousUnfinishedDownload);
  switch(iRv)
  {
    char szMsg[MAX_BUF];
    char szBuf2[MAX_BUF];
    char szTitle[MAX_BUF_TINY];

    case WIZ_CRC_FAIL:
      switch(sgProduct.dwMode)
      {
        case NORMAL:
          if(GetPrivateProfileString("Messages", "STR_MESSAGEBOX_TITLE", "", szBuf, sizeof(szBuf), szFileIniInstall))
            lstrcpy(szTitle, "Setup");
          else
            wsprintf(szTitle, szBuf, sgProduct.szProductName);

          GetPrivateProfileString("Strings", "Error Corrupted Archives Detected", "", szBuf, sizeof(szBuf), szFileIniConfig);
          if(*szBuf != '\0')
          {
            lstrcpy(szBuf2, "\n\n");
            lstrcat(szBuf2, szCorruptedArchiveList);
            lstrcat(szBuf2, "\n");
            wsprintf(szMsg, szBuf, szBuf2);
          }
          MessageBox(hWndMain, szMsg, szTitle, MB_OK | MB_ICONSTOP);
          break;

        case AUTO:
          GetPrivateProfileString("Strings", "Error Corrupted Archives Detected AUTO mode", "", szBuf, sizeof(szBuf), szFileIniConfig);
          ShowMessage(szBuf, TRUE);
          Delay(5);
          ShowMessage(szBuf, FALSE);
          break;
      }

      LogISComponentsFailedCRC(szCorruptedArchiveList, W_STARTUP);
      return(WIZ_CRC_FAIL);

    case WIZ_CRC_PASS:
      break;

    default:
      break;
  }
  LogISComponentsFailedCRC(NULL, W_STARTUP);
  return(iRv);
}

HRESULT ParseConfigIni(LPSTR lpszCmdLine)
{
  int  iRv;
  char szBuf[MAX_BUF];
  char szMsgInitSetup[MAX_BUF];
  char szPreviousPath[MAX_BUF];
  char szShowDialog[MAX_BUF];

  if(InitSetupGeneral())
    return(1);
  if(InitDlgWelcome(&diWelcome))
    return(1);
  if(InitDlgLicense(&diLicense))
    return(1);
  if(InitDlgSetupType(&diSetupType))
    return(1);
  if(InitDlgSelectComponents(&diSelectComponents, SM_SINGLE))
    return(1);
  if(InitDlgSelectComponents(&diSelectAdditionalComponents, SM_SINGLE))
    return(1);
  if(InitDlgWindowsIntegration(&diWindowsIntegration))
    return(1);
  if(InitDlgProgramFolder(&diProgramFolder))
    return(1);
  if(InitDlgDownloadOptions(&diDownloadOptions))
    return(1);
  if(InitDlgAdvancedSettings(&diAdvancedSettings))
    return(1);
  if(InitDlgQuickLaunch(&diQuickLaunch))
    return(1);
  if(InitDlgStartInstall(&diStartInstall))
    return(1);
  if(InitDlgDownload(&diDownload))
    return(1);
  if(InitDlgReboot(&diReboot))
    return(1);
  if(InitSDObject())
    return(1);
  if(InitSXpcomFile())
    return(1);
 
  /* get install Mode information */
  GetPrivateProfileString("General", "Run Mode", "", szBuf, sizeof(szBuf), szFileIniConfig);
  SetSetupRunMode(szBuf);
  if(ParseCommandLine(lpszCmdLine))
    return(1);

  if(CheckInstances())
    return(1);

  if(GetPrivateProfileString("Messages", "MSG_INIT_SETUP", "", szMsgInitSetup, sizeof(szMsgInitSetup), szFileIniInstall))
    ShowMessage(szMsgInitSetup, TRUE);

  /* get product name description */
  GetPrivateProfileString("General", "Company Name", "", sgProduct.szCompanyName, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Product Name", "", sgProduct.szProductName, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Uninstall Filename", "", sgProduct.szUninstallFilename, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "User Agent",   "", sgProduct.szUserAgent,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Sub Path",     "", sgProduct.szSubPath,     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Program Name", "", sgProduct.szProgramName, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Lock Path",    "", szBuf,                   sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    sgProduct.bLockPath = TRUE;
  
  gbRestrictedAccess = VerifyRestrictedAccess();
  if(gbRestrictedAccess)
  {
    /* Detected user does not have the appropriate
     * privileges on this system */
    char szTitle[MAX_BUF_TINY];
    int  iRvMB;

    switch(sgProduct.dwMode)
    {
      case NORMAL:
        if(!GetPrivateProfileString("Messages", "MB_WARNING_STR", "", szBuf, sizeof(szBuf), szFileIniInstall))
          lstrcpy(szTitle, "Setup");
        else
          wsprintf(szTitle, szBuf, sgProduct.szProductName);

        GetPrivateProfileString("Strings", "Message NORMAL Restricted Access", "", szBuf, sizeof(szBuf), szFileIniConfig);
        iRvMB = MessageBox(hWndMain, szBuf, szTitle, MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2);
        break;

      case AUTO:
        ShowMessage(szMsgInitSetup, FALSE);
        GetPrivateProfileString("Strings", "Message AUTO Restricted Access", "", szBuf, sizeof(szBuf), szFileIniConfig);
        ShowMessage(szBuf, TRUE);
        Delay(5);
        ShowMessage(szBuf, FALSE);
        iRvMB = IDNO;
        break;

      default:
        iRvMB = IDNO;
        break;
    }

    if(iRvMB == IDNO)
    {
      /* User chose not to continue with the lack of
       * appropriate access privileges */
      PostQuitMessage(1);
      return(1);
    }
  }

  /* get main install path */
  if(LocatePreviousPath("Locate Previous Product Path", szPreviousPath, sizeof(szPreviousPath)) == FALSE)
  {
    GetPrivateProfileString("General", "Path", "", szBuf, sizeof(szBuf), szFileIniConfig);
    DecryptString(sgProduct.szPath, szBuf);
  }
  else
  {
    /* If the previous path is located in the regsitry, then we need to check to see if the path from
     * the regsitry plus the Sub Path contains the Program Name file.  If it does, then we have the
     * correct path, so just use it.
     *
     * If it does not contain the Program Name file, then check the parent path (from the registry) +
     * SubPath.  If this path contains the Program Name file, then we found an older path format.  We
     * then need to use the parent path as the default path.
     *
     * Only do the older path format checking if the Sub Path= and Program Name= keys exist.  If
     * either are not set, then assume that the path from the registry is what we want.
     */
    if((*sgProduct.szSubPath != '\0') && (*sgProduct.szProgramName != '\0'))
    {
      /* If the Sub Path= and Program Name= keys exist, do extra parsing for the correct path */
      lstrcpy(szBuf, szPreviousPath);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, sgProduct.szSubPath);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, sgProduct.szProgramName);

      /* Check to see if PreviousPath + SubPath + ProgramName exists.  If it does, then we have the
       * correct path.
       */
      if(FileExists(szBuf))
      {
        lstrcpy(sgProduct.szPath, szPreviousPath);
      }
      else
      {
        /* If not, try parent of PreviousPath + SubPath + ProgramName.
         * If this exists, then we need to use the parent path of PreviousPath.
         */
        RemoveBackSlash(szPreviousPath);
        ParsePath(szPreviousPath, szBuf, sizeof(szBuf), FALSE, PP_PATH_ONLY);
        AppendBackSlash(szBuf, sizeof(szBuf));
        lstrcat(szBuf, sgProduct.szSubPath);
        AppendBackSlash(szBuf, sizeof(szBuf));
        lstrcat(szBuf, sgProduct.szProgramName);

        if(FileExists(szBuf))
        {
          RemoveBackSlash(szPreviousPath);
          ParsePath(szPreviousPath, szBuf, sizeof(szBuf), FALSE, PP_PATH_ONLY);
          lstrcpy(sgProduct.szPath, szBuf);
        }
        else
        {
          /* If we still can't locate ProgramName file, then use the default in the config.ini */
          GetPrivateProfileString("General", "Path", "", szBuf, sizeof(szBuf), szFileIniConfig);
          DecryptString(sgProduct.szPath, szBuf);
        }
      }
    }
    else
    {
      lstrcpy(sgProduct.szPath, szPreviousPath);
    }
  }
  RemoveBackSlash(sgProduct.szPath);

  /* make a copy of sgProduct.szPath to be used in the Setup Type dialog */
  lstrcpy(szTempSetupPath, sgProduct.szPath);
  
  /* get main program folder path */
  GetPrivateProfileString("General", "Program Folder Path", "", szBuf, sizeof(szBuf), szFileIniConfig);
  DecryptString(sgProduct.szProgramFolderPath, szBuf);
  
  /* get main program folder name */
  GetPrivateProfileString("General", "Program Folder Name", "", szBuf, sizeof(szBuf), szFileIniConfig);
  DecryptString(sgProduct.szProgramFolderName, szBuf);

  /* Welcome dialog */
  GetPrivateProfileString("Dialog Welcome",             "Show Dialog",     "", szShowDialog,                  sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Title",           "", diWelcome.szTitle,             MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Message0",        "", diWelcome.szMessage0,          MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Message1",        "", diWelcome.szMessage1,          MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Message2",        "", diWelcome.szMessage2,          MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diWelcome.bShowDialog = TRUE;

  /* License dialog */
  GetPrivateProfileString("Dialog License",             "Show Dialog",     "", szShowDialog,                  sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "Title",           "", diLicense.szTitle,             MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "License File",    "", diLicense.szLicenseFilename,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "Message0",        "", diLicense.szMessage0,          MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "Message1",        "", diLicense.szMessage1,          MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diLicense.bShowDialog = TRUE;

  /* Setup Type dialog */
  GetPrivateProfileString("Dialog Setup Type",          "Show Dialog",     "", szShowDialog,                  sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Title",           "", diSetupType.szTitle,           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Message0",        "", diSetupType.szMessage0,        MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Readme Filename", "", diSetupType.szReadmeFilename,  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Readme App",      "", diSetupType.szReadmeApp,       MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diSetupType.bShowDialog = TRUE;

  /* Get Setup Types info */
  GetPrivateProfileString("Setup Type0", "Description Short", "", diSetupType.stSetupType0.szDescriptionShort, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Setup Type0", "Description Long",  "", diSetupType.stSetupType0.szDescriptionLong,  MAX_BUF, szFileIniConfig);
  STSetVisibility(&diSetupType.stSetupType0);

  GetPrivateProfileString("Setup Type1", "Description Short", "", diSetupType.stSetupType1.szDescriptionShort, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Setup Type1", "Description Long",  "", diSetupType.stSetupType1.szDescriptionLong,  MAX_BUF, szFileIniConfig);
  STSetVisibility(&diSetupType.stSetupType1);

  GetPrivateProfileString("Setup Type2", "Description Short", "", diSetupType.stSetupType2.szDescriptionShort, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Setup Type2", "Description Long",  "", diSetupType.stSetupType2.szDescriptionLong,  MAX_BUF, szFileIniConfig);
  STSetVisibility(&diSetupType.stSetupType2);

  GetPrivateProfileString("Setup Type3", "Description Short", "", diSetupType.stSetupType3.szDescriptionShort, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Setup Type3", "Description Long",  "", diSetupType.stSetupType3.szDescriptionLong,  MAX_BUF, szFileIniConfig);
  STSetVisibility(&diSetupType.stSetupType3);

  /* remember the radio button that is considered the Custom type (the last radio button) */
  SetCustomType();

  /* Select Components dialog */
  GetPrivateProfileString("Dialog Select Components",   "Show Dialog",  "", szShowDialog,                    sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Select Components",   "Title",        "", diSelectComponents.szTitle,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Select Components",   "Message0",     "", diSelectComponents.szMessage0,   MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diSelectComponents.bShowDialog = TRUE;

  /* Select Additional Components dialog */
  GetPrivateProfileString("Dialog Select Additional Components",   "Show Dialog",  "", szShowDialog,                              sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Select Additional Components",   "Title",        "", diSelectAdditionalComponents.szTitle,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Select Additional Components",   "Message0",     "", diSelectAdditionalComponents.szMessage0,   MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diSelectAdditionalComponents.bShowDialog = TRUE;

  /* Windows Integration dialog */
  GetPrivateProfileString("Dialog Windows Integration", "Show Dialog",  "", szShowDialog,                    sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Windows Integration", "Title",        "", diWindowsIntegration.szTitle,    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Windows Integration", "Message0",     "", diWindowsIntegration.szMessage0, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Windows Integration", "Message1",     "", diWindowsIntegration.szMessage1, MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diWindowsIntegration.bShowDialog = TRUE;

  /* Program Folder dialog */
  GetPrivateProfileString("Dialog Program Folder",      "Show Dialog",  "", szShowDialog,                    sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Program Folder",      "Title",        "", diProgramFolder.szTitle,         MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Program Folder",      "Message0",     "", diProgramFolder.szMessage0,      MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diProgramFolder.bShowDialog = TRUE;

  /* Download Options dialog */
  GetPrivateProfileString("Dialog Download Options",       "Show Dialog",    "", szShowDialog,                     sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Download Options",       "Title",          "", diDownloadOptions.szTitle,        MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Download Options",       "Message0",       "", diDownloadOptions.szMessage0,     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Download Options",       "Message1",       "", diDownloadOptions.szMessage1,     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Download Options",       "Save Installer", "", szBuf,                            sizeof(szBuf), szFileIniConfig);

  if(lstrcmpi(szBuf, "TRUE") == 0)
    diDownloadOptions.bSaveInstaller = TRUE;

  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diDownloadOptions.bShowDialog = TRUE;

  /* Advanced Settings dialog */
  GetPrivateProfileString("Dialog Advanced Settings",       "Show Dialog",    "", szShowDialog,                     sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Title",          "", diAdvancedSettings.szTitle,       MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Message0",       "", diAdvancedSettings.szMessage0,    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Proxy Server",   "", diAdvancedSettings.szProxyServer, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Proxy Port",     "", diAdvancedSettings.szProxyPort,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Proxy User",     "", diAdvancedSettings.szProxyUser,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Proxy Password", "", diAdvancedSettings.szProxyPasswd, MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diAdvancedSettings.bShowDialog = TRUE;

  GetPrivateProfileString("Dialog Advanced Settings",       "Use Protocol",   "", szBuf,                            sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "HTTP") == 0)
    diDownloadOptions.dwUseProtocol = UP_HTTP;
  else
    diDownloadOptions.dwUseProtocol = UP_FTP;

  GetPrivateProfileString("Dialog Advanced Settings",       "Use Protocol Settings", "", szBuf,                     sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "DISABLED") == 0)
    diDownloadOptions.bUseProtocolSettings = FALSE;
  else
    diDownloadOptions.bUseProtocolSettings = TRUE;

  GetPrivateProfileString("Dialog Advanced Settings",
                          "Show Protocols",
                          "",
                          szBuf,
                          sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "FALSE") == 0)
    diDownloadOptions.bShowProtocols = FALSE;
  else
    diDownloadOptions.bShowProtocols = TRUE;

   /* Program Folder dialog */
  GetPrivateProfileString("Dialog Quick Launch",      "Show Dialog",  "", szShowDialog,                    sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Quick Launch",      "Title",        "", diQuickLaunch.szTitle,         MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Quick Launch",      "Message0",     "", diQuickLaunch.szMessage0,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Quick Launch",      "Message1",     "", diQuickLaunch.szMessage1,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Quick Launch",      "Message2",     "", diQuickLaunch.szMessage2,      MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diQuickLaunch.bShowDialog = TRUE;
  GetPrivateProfileString("Dialog Quick Launch",       "Turbo Mode",         "", szBuf,                          sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diQuickLaunch.bTurboMode = TRUE;   
  GetPrivateProfileString("Dialog Quick Launch",       "Turbo Mode Enabled","", szBuf,                          sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diQuickLaunch.bTurboModeEnabled = TRUE;
  else
    /* since turbo mode is disabled, make sure bTurboMode is FALSE */
    diQuickLaunch.bTurboMode = FALSE;

  /* Start Install dialog */
  GetPrivateProfileString("Dialog Start Install",       "Show Dialog",      "", szShowDialog,                     sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Start Install",       "Title",            "", diStartInstall.szTitle,           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Start Install",       "Message Install",  "", diStartInstall.szMessageInstall,  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Start Install",       "Message Download", "", diStartInstall.szMessageDownload, MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diStartInstall.bShowDialog = TRUE;
 
  /* Download dialog */
  GetPrivateProfileString("Dialog Download",       "Show Dialog",        "", szShowDialog,                   sizeof(szShowDialog),        szFileIniConfig);
  GetPrivateProfileString("Dialog Download",       "Title",              "", diDownload.szTitle,             MAX_BUF_TINY,   szFileIniConfig);
  GetPrivateProfileString("Dialog Download",       "Message Download0",  "", diDownload.szMessageDownload0,  MAX_BUF_MEDIUM, szFileIniConfig);
  GetPrivateProfileString("Dialog Download",       "Message Retry0",     "", diDownload.szMessageRetry0,     MAX_BUF_MEDIUM, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diDownload.bShowDialog = TRUE;

  /* Reboot dialog */
  GetPrivateProfileString("Dialog Reboot", "Show Dialog", "", szShowDialog, sizeof(szShowDialog), szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diReboot.dwShowDialog = TRUE;
  else if(lstrcmpi(szShowDialog, "AUTO") == 0)
    diReboot.dwShowDialog = AUTO;

  GetPrivateProfileString("Windows Integration-Item0", "CheckBoxState", "", szBuf,                                    sizeof(szBuf), szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item0", "Description",   "", diWindowsIntegration.wiCB0.szDescription, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item0", "Archive",       "", diWindowsIntegration.wiCB0.szArchive,     MAX_BUF, szFileIniConfig);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB0.szDescription != '\0')
    diWindowsIntegration.wiCB0.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB0.bCheckBoxState = TRUE;

  GetPrivateProfileString("Windows Integration-Item1", "CheckBoxState", "", szBuf,                           sizeof(szBuf), szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item1", "Description",   "", diWindowsIntegration.wiCB1.szDescription, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item1", "Archive",       "", diWindowsIntegration.wiCB1.szArchive, MAX_BUF, szFileIniConfig);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB1.szDescription != '\0')
    diWindowsIntegration.wiCB1.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB1.bCheckBoxState = TRUE;

  GetPrivateProfileString("Windows Integration-Item2", "CheckBoxState", "", szBuf,                           sizeof(szBuf), szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item2", "Description",   "", diWindowsIntegration.wiCB2.szDescription, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item2", "Archive",       "", diWindowsIntegration.wiCB2.szArchive, MAX_BUF, szFileIniConfig);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB2.szDescription != '\0')
    diWindowsIntegration.wiCB2.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB2.bCheckBoxState = TRUE;

  GetPrivateProfileString("Windows Integration-Item3", "CheckBoxState", "", szBuf,                           sizeof(szBuf), szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item3", "Description",   "", diWindowsIntegration.wiCB3.szDescription, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item3", "Archive",       "", diWindowsIntegration.wiCB3.szArchive, MAX_BUF, szFileIniConfig);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB3.szDescription != '\0')
    diWindowsIntegration.wiCB3.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB3.bCheckBoxState = TRUE;

  /* Read in the Site Selector Status */
  GetPrivateProfileString("Site Selector", "Status", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "HIDE") == 0)
    gdwSiteSelectorStatus = SS_HIDE;

  switch(sgProduct.dwMode)
  {
    case AUTO:
    case SILENT:
      diWelcome.bShowDialog                     = FALSE;
      diLicense.bShowDialog                     = FALSE;
      diSetupType.bShowDialog                   = FALSE;
      diSelectComponents.bShowDialog            = FALSE;
      diSelectAdditionalComponents.bShowDialog  = FALSE;
      diWindowsIntegration.bShowDialog          = FALSE;
      diProgramFolder.bShowDialog               = FALSE;
      diQuickLaunch.bShowDialog                 = FALSE;
      diDownloadOptions.bShowDialog             = FALSE;
      diAdvancedSettings.bShowDialog            = FALSE;
      diStartInstall.bShowDialog                = FALSE;
      diDownload.bShowDialog                    = FALSE;
      break;
  }

  InitSiComponents(szFileIniConfig);
  InitSiteSelector(szFileIniConfig);
  InitErrorMessageStream(szFileIniConfig);

  /* get Default Setup Type */
  GetPrivateProfileString("General", "Default Setup Type", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if((lstrcmpi(szBuf, "Setup Type 0") == 0) && diSetupType.stSetupType0.bVisible)
  {
    dwSetupType     = ST_RADIO0;
    dwTempSetupType = dwSetupType;
  }
  else if((lstrcmpi(szBuf, "Setup Type 1") == 0) && diSetupType.stSetupType1.bVisible)
  {
    dwSetupType     = ST_RADIO1;
    dwTempSetupType = dwSetupType;
  }
  else if((lstrcmpi(szBuf, "Setup Type 2") == 0) && diSetupType.stSetupType2.bVisible)
  {
    dwSetupType     = ST_RADIO2;
    dwTempSetupType = dwSetupType;
  }
  else if((lstrcmpi(szBuf, "Setup Type 3") == 0) && diSetupType.stSetupType3.bVisible)
  {
    dwSetupType     = ST_RADIO3;
    dwTempSetupType = dwSetupType;
  }
  else
  {
    if(diSetupType.stSetupType0.bVisible)
    {
      dwSetupType     = ST_RADIO0;
      dwTempSetupType = dwSetupType;
    }
    else if(diSetupType.stSetupType1.bVisible)
    {
      dwSetupType     = ST_RADIO1;
      dwTempSetupType = dwSetupType;
    }
    else if(diSetupType.stSetupType2.bVisible)
    {
      dwSetupType     = ST_RADIO2;
      dwTempSetupType = dwSetupType;
    }
    else if(diSetupType.stSetupType3.bVisible)
    {
      dwSetupType     = ST_RADIO3;
      dwTempSetupType = dwSetupType;
    }
  }
  SiCNodeSetItemsSelected(dwSetupType);

  /* get install size required in temp for component Xpcom.  Sould be in Kilobytes */
  GetPrivateProfileString("Core", "Install Size", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(*szBuf != '\0')
    siCFXpcomFile.ullInstallSize = _atoi64(szBuf);
  else
    siCFXpcomFile.ullInstallSize = 0;

  GetPrivateProfileString("SmartDownload-Netscape Install", "core_file",        "", siSDObject.szXpcomFile,       MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "core_file_path",   "", siSDObject.szXpcomFilePath,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "xpcom_dir",        "", siSDObject.szXpcomDir,        MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "no_ads",           "", siSDObject.szNoAds,           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "silent",           "", siSDObject.szSilent,          MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "execution",        "", siSDObject.szExecution,       MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "confirm_install",  "", siSDObject.szConfirmInstall,  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "extract_msg",      "", siSDObject.szExtractMsg,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Execution",        "exe",              "", siSDObject.szExe,             MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Execution",        "exe_param",        "", siSDObject.szExeParam,        MAX_BUF, szFileIniConfig);

  GetPrivateProfileString("Core",                           "Source",           "", szBuf,                        sizeof(szBuf), szFileIniConfig);
  DecryptString(siCFXpcomFile.szSource, szBuf);
  GetPrivateProfileString("Core",                           "Destination",      "", szBuf,                        sizeof(szBuf), szFileIniConfig);
  DecryptString(siCFXpcomFile.szDestination, szBuf);
  GetPrivateProfileString("Core",                           "Message",          "", siCFXpcomFile.szMessage,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Core",                           "Cleanup",          "", szBuf,                        sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "FALSE") == 0)
    siCFXpcomFile.bCleanup = FALSE;
  else
    siCFXpcomFile.bCleanup = TRUE;

  LogISProductInfo();
  LogMSProductInfo();
  CleanupXpcomFile();
  ShowMessage(szMsgInitSetup, FALSE);

  /* check the windows registry to see if a previous instance of setup finished downloading
   * all the required archives. */
  gbPreviousUnfinishedDownload = CheckForPreviousUnfinishedDownload();
  if(gbPreviousUnfinishedDownload)
  {
    char szTitle[MAX_BUF_TINY];

    switch(sgProduct.dwMode)
    {
      case NORMAL:
        if(!GetPrivateProfileString("Messages", "STR_MESSAGEBOX_TITLE", "", szBuf, sizeof(szBuf), szFileIniInstall))
          lstrcpy(szTitle, "Setup");
        else
          wsprintf(szTitle, szBuf, sgProduct.szProductName);

        GetPrivateProfileString("Strings", "Message Unfinished Download Restart", "", szBuf, sizeof(szBuf), szFileIniConfig);
        if(MessageBox(hWndMain, szBuf, szTitle, MB_YESNO | MB_ICONQUESTION) == IDNO)
        {
          UnsetSetupCurrentDownloadFile();
          UnsetDownloadState(); /* unset the download state so that the archives can be deleted */
          DeleteArchives(DA_ONLY_IF_NOT_IN_ARCHIVES_LST);
        }
        break;
    }
  }

  iRv = StartupCheckArchives();
  return(iRv);
}

HRESULT ParseInstallIni()
{
  LOGFONT lf;
  NONCLIENTMETRICS ncm;

  /* get system font */
  memset(&ncm, 0, sizeof(ncm));
  ncm.cbSize = sizeof(ncm);
  SystemParametersInfo( SPI_GETNONCLIENTMETRICS, 0, &ncm, 0);
  sgInstallGui.systemFont = CreateFontIndirect( &ncm.lfMessageFont ); 

  /* get defined font */
  GetPrivateProfileString("General", "FONTNAME", "", sgInstallGui.szFontName, sizeof(sgInstallGui.szFontName), szFileIniInstall);
  GetPrivateProfileString("General", "FONTSIZE", "", sgInstallGui.szFontSize, sizeof(sgInstallGui.szFontSize), szFileIniInstall);
  GetPrivateProfileString("General", "CHARSET", "", sgInstallGui.szCharSet, sizeof(sgInstallGui.szCharSet), szFileIniInstall);
  memset(&lf, 0, sizeof(lf));
  strcpy(lf.lfFaceName, sgInstallGui.szFontName);
  lf.lfHeight = -MulDiv(atoi(sgInstallGui.szFontSize), GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);
  lf.lfCharSet = atoi(sgInstallGui.szCharSet);
  sgInstallGui.definedFont = CreateFontIndirect( &lf ); 
  
  GetPrivateProfileString("General", "OK_", "", sgInstallGui.szOk_, sizeof(sgInstallGui.szOk_), szFileIniInstall);
  GetPrivateProfileString("General", "OK", "", sgInstallGui.szOk, sizeof(sgInstallGui.szOk), szFileIniInstall);
  GetPrivateProfileString("General", "CANCEL_", "", sgInstallGui.szCancel_, sizeof(sgInstallGui.szCancel_), szFileIniInstall);
  GetPrivateProfileString("General", "CANCEL", "", sgInstallGui.szCancel, sizeof(sgInstallGui.szCancel), szFileIniInstall);
  GetPrivateProfileString("General", "NEXT_", "", sgInstallGui.szNext_, sizeof(sgInstallGui.szNext_), szFileIniInstall);
  GetPrivateProfileString("General", "BACK_", "", sgInstallGui.szBack_, sizeof(sgInstallGui.szBack_), szFileIniInstall);
  GetPrivateProfileString("General", "PROXYSETTINGS_", "", sgInstallGui.szProxySettings_, sizeof(sgInstallGui.szProxySettings_), szFileIniInstall);
  GetPrivateProfileString("General", "PROXYSETTINGS", "", sgInstallGui.szProxySettings, sizeof(sgInstallGui.szProxySettings), szFileIniInstall);
  GetPrivateProfileString("General", "SERVER", "", sgInstallGui.szServer, sizeof(sgInstallGui.szServer), szFileIniInstall);
  GetPrivateProfileString("General", "PORT", "", sgInstallGui.szPort, sizeof(sgInstallGui.szPort), szFileIniInstall);
  GetPrivateProfileString("General", "USERID", "", sgInstallGui.szUserId, sizeof(sgInstallGui.szUserId), szFileIniInstall);
  GetPrivateProfileString("General", "PASSWORD", "", sgInstallGui.szPassword, sizeof(sgInstallGui.szPassword), szFileIniInstall);
  GetPrivateProfileString("General", "SELECTDIRECTORY", "", sgInstallGui.szSelectDirectory, sizeof(sgInstallGui.szSelectDirectory), szFileIniInstall);
  GetPrivateProfileString("General", "DIRECTORIES_", "", sgInstallGui.szDirectories_, sizeof(sgInstallGui.szDirectories_), szFileIniInstall);
  GetPrivateProfileString("General", "DRIVES_", "", sgInstallGui.szDrives_, sizeof(sgInstallGui.szDrives_), szFileIniInstall);
  GetPrivateProfileString("General", "STATUS", "", sgInstallGui.szStatus, sizeof(sgInstallGui.szStatus), szFileIniInstall);
  GetPrivateProfileString("General", "FILE", "", sgInstallGui.szFile, sizeof(sgInstallGui.szFile), szFileIniInstall);
  GetPrivateProfileString("General", "URL", "", sgInstallGui.szUrl, sizeof(sgInstallGui.szUrl), szFileIniInstall);
  GetPrivateProfileString("General", "TO", "", sgInstallGui.szTo, sizeof(sgInstallGui.szTo), szFileIniInstall);
  GetPrivateProfileString("General", "ACCEPT_", "", sgInstallGui.szAccept_, sizeof(sgInstallGui.szAccept_), szFileIniInstall);
  GetPrivateProfileString("General", "NO_", "", sgInstallGui.szNo_, sizeof(sgInstallGui.szNo_), szFileIniInstall);
  GetPrivateProfileString("General", "PROGRAMFOLDER_", "", sgInstallGui.szProgramFolder_, sizeof(sgInstallGui.szProgramFolder_), szFileIniInstall);
  GetPrivateProfileString("General", "EXISTINGFOLDERS_", "", sgInstallGui.szExistingFolder_, sizeof(sgInstallGui.szExistingFolder_), szFileIniInstall);
  GetPrivateProfileString("General", "SETUPMESSAGE", "", sgInstallGui.szSetupMessage, sizeof(sgInstallGui.szSetupMessage), szFileIniInstall);
  GetPrivateProfileString("General", "YESRESTART", "", sgInstallGui.szYesRestart, sizeof(sgInstallGui.szYesRestart), szFileIniInstall);
  GetPrivateProfileString("General", "NORESTART", "", sgInstallGui.szNoRestart, sizeof(sgInstallGui.szNoRestart), szFileIniInstall);
  GetPrivateProfileString("General", "ADDITIONALCOMPONENTS_", "", sgInstallGui.szAdditionalComponents_, sizeof(sgInstallGui.szAdditionalComponents_), szFileIniInstall);
  GetPrivateProfileString("General", "DESCRIPTION", "", sgInstallGui.szDescription, sizeof(sgInstallGui.szDescription), szFileIniInstall);
  GetPrivateProfileString("General", "TOTALDOWNLOADSIZE", "", sgInstallGui.szTotalDownloadSize, sizeof(sgInstallGui.szTotalDownloadSize), szFileIniInstall);
  GetPrivateProfileString("General", "SPACEAVAILABLE", "", sgInstallGui.szSpaceAvailable, sizeof(sgInstallGui.szSpaceAvailable), szFileIniInstall);
  GetPrivateProfileString("General", "COMPONENTS_", "", sgInstallGui.szComponents_, sizeof(sgInstallGui.szComponents_), szFileIniInstall);
  GetPrivateProfileString("General", "DESTINATIONDIRECTORY", "", sgInstallGui.szDestinationDirectory, sizeof(sgInstallGui.szDestinationDirectory), szFileIniInstall);
  GetPrivateProfileString("General", "BROWSE_", "", sgInstallGui.szBrowse_, sizeof(sgInstallGui.szBrowse_), szFileIniInstall);
  GetPrivateProfileString("General", "CURRENTSETTINGS", "", sgInstallGui.szCurrentSettings, sizeof(sgInstallGui.szCurrentSettings), szFileIniInstall);
  GetPrivateProfileString("General", "INSTALL_", "", sgInstallGui.szInstall_, sizeof(sgInstallGui.szInstall_), szFileIniInstall);
  GetPrivateProfileString("General", "DELETE_", "", sgInstallGui.szDelete_, sizeof(sgInstallGui.szDelete_), szFileIniInstall);
  GetPrivateProfileString("General", "EXTRACTING", "", sgInstallGui.szExtracting, sizeof(sgInstallGui.szExtracting), szFileIniInstall);
  GetPrivateProfileString("General", "README", "", sgInstallGui.szReadme_, sizeof(sgInstallGui.szReadme_), szFileIniInstall);
  GetPrivateProfileString("General", "PAUSE_", "", sgInstallGui.szPause_, sizeof(sgInstallGui.szPause_), szFileIniInstall);
  GetPrivateProfileString("General", "RESUME_", "", sgInstallGui.szResume_, sizeof(sgInstallGui.szResume_), szFileIniInstall);

  return(0);
}


BOOL LocatePreviousPath(LPSTR szMainSectionName, LPSTR szPath, DWORD dwPathSize)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szValue[MAX_BUF];
  BOOL  bFound;

  bFound  = FALSE;
  dwIndex = -1;
  while(!bFound)
  {
    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, szMainSectionName);
    lstrcat(szSection, szIndex);

    GetPrivateProfileString(szSection, "Key", "", szValue, sizeof(szValue), szFileIniConfig);
    if(*szValue != '\0')
      bFound = LocatePathNscpReg(szSection, szPath, dwPathSize);
    else
    {
      GetPrivateProfileString(szSection, "HKey", "", szValue, sizeof(szValue), szFileIniConfig);
      if(*szValue != '\0')
        bFound = LocatePathWinReg(szSection, szPath, dwPathSize);
      else
      {
        GetPrivateProfileString(szSection, "Path", "", szValue, sizeof(szValue), szFileIniConfig);
        if(*szValue != '\0')
          bFound = LocatePath(szSection, szPath, dwPathSize);
        else
          break;
      }
    }
  }

  return(bFound);
}

BOOL LocatePathNscpReg(LPSTR szSection, LPSTR szPath, DWORD dwPathSize)
{
  char  szKey[MAX_BUF];
  char  szContainsFilename[MAX_BUF];
  char  szBuf[MAX_BUF];
  BOOL  bReturn;

  bReturn = FALSE;
  GetPrivateProfileString(szSection, "Key", "", szKey, sizeof(szKey), szFileIniConfig);
  if(*szKey != '\0')
  {
    bReturn = FALSE;
    ZeroMemory(szPath, dwPathSize);

    VR_GetPath(szKey, MAX_BUF, szBuf);
    if(*szBuf != '\0')
    {
      GetPrivateProfileString(szSection, "Contains Filename", "", szContainsFilename, sizeof(szContainsFilename), szFileIniConfig);
      if(lstrcmpi(szContainsFilename, "TRUE") == 0)
        ParsePath(szBuf, szPath, dwPathSize, FALSE, PP_PATH_ONLY);
      else
        lstrcpy(szPath, szBuf);

      bReturn = TRUE;
    }
  }

  return(bReturn);
}

DWORD GetTotalArchivesToDownload()
{
  DWORD     dwIndex0;
  DWORD     dwTotalArchivesToDownload;
  siC       *siCObject = NULL;
  char      szIndex0[MAX_BUF];

  dwTotalArchivesToDownload = 0;
  dwIndex0                  = 0;
  itoa(dwIndex0,  szIndex0,  10);
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    if(siCObject->dwAttributes & SIC_SELECTED)
    {
      if(LocateJar(siCObject, NULL, 0, gbPreviousUnfinishedDownload) == AP_NOT_FOUND)
      {
        ++dwTotalArchivesToDownload;
      }
    }

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  return(dwTotalArchivesToDownload);
}

BOOL LocatePathWinReg(LPSTR szSection, LPSTR szPath, DWORD dwPathSize)
{
  char  szHKey[MAX_BUF];
  char  szHRoot[MAX_BUF];
  char  szName[MAX_BUF];
  char  szVerifyExistence[MAX_BUF];
  char  szBuf[MAX_BUF];
  BOOL  bDecryptKey;
  BOOL  bContainsFilename;
  BOOL  bReturn;
  HKEY  hkeyRoot;

  bReturn = FALSE;
  GetPrivateProfileString(szSection, "HKey", "", szHKey, sizeof(szHKey), szFileIniConfig);
  if(*szHKey != '\0')
  {
    bReturn = FALSE;
    ZeroMemory(szPath, dwPathSize);

    GetPrivateProfileString(szSection, "HRoot",        "", szHRoot, sizeof(szHRoot), szFileIniConfig);
    GetPrivateProfileString(szSection, "Name",         "", szName,  sizeof(szName),  szFileIniConfig);
    GetPrivateProfileString(szSection, "Decrypt HKey", "", szBuf,   sizeof(szBuf),   szFileIniConfig);
    if(lstrcmpi(szBuf, "FALSE") == 0)
      bDecryptKey = FALSE;
    else
      bDecryptKey = TRUE;

    /* check for both 'Verify Existance' and 'Verify Existence' */
    GetPrivateProfileString(szSection, "Verify Existence", "", szVerifyExistence, sizeof(szVerifyExistence), szFileIniConfig);
    if(*szVerifyExistence == '\0')
      GetPrivateProfileString(szSection, "Verify Existance", "", szVerifyExistence, sizeof(szVerifyExistence), szFileIniConfig);

    GetPrivateProfileString(szSection, "Contains Filename", "", szBuf, sizeof(szBuf), szFileIniConfig);
    if(lstrcmpi(szBuf, "TRUE") == 0)
      bContainsFilename = TRUE;
    else
      bContainsFilename = FALSE;

    hkeyRoot = ParseRootKey(szHRoot);
    if(bDecryptKey == TRUE)
    {
      DecryptString(szBuf, szHKey);
      lstrcpy(szHKey, szBuf);
    }

    GetWinReg(hkeyRoot, szHKey, szName, szBuf, sizeof(szBuf));
    if(*szBuf != '\0')
    {
      if(lstrcmpi(szVerifyExistence, "FILE") == 0)
      {
        if(FileExists(szBuf))
        {
          if(bContainsFilename == TRUE)
            ParsePath(szBuf, szPath, dwPathSize, FALSE, PP_PATH_ONLY);
          else
            lstrcpy(szPath, szBuf);

          bReturn = TRUE;
        }
        else
          bReturn = FALSE;
      }
      else if(lstrcmpi(szVerifyExistence, "PATH") == 0)
      {
        if(bContainsFilename == TRUE)
          ParsePath(szBuf, szPath, dwPathSize, FALSE, PP_PATH_ONLY);
        else
          lstrcpy(szPath, szBuf);

        if(FileExists(szPath))
          bReturn = TRUE;
        else
          bReturn = FALSE;
      }
      else
      {
        if(bContainsFilename == TRUE)
          ParsePath(szBuf, szPath, dwPathSize, FALSE, PP_PATH_ONLY);
        else
          lstrcpy(szPath, szBuf);

        bReturn = TRUE;
      }
    }
  }

  return(bReturn);
}

BOOL LocatePath(LPSTR szSection, LPSTR szPath, DWORD dwPathSize)
{
  char  szPathKey[MAX_BUF];
  BOOL  bReturn;

  bReturn = FALSE;
  GetPrivateProfileString(szSection, "Path", "", szPathKey, sizeof(szPathKey), szFileIniConfig);
  if(*szPathKey != '\0')
  {
    bReturn = FALSE;
    ZeroMemory(szPath, dwPathSize);

    DecryptString(szPath, szPathKey);
    bReturn = TRUE;
  }

  return(bReturn);
}

void SetCustomType()
{
  if(diSetupType.stSetupType3.bVisible == TRUE)
    sgProduct.dwCustomType = ST_RADIO3;
  else if(diSetupType.stSetupType2.bVisible == TRUE)
    sgProduct.dwCustomType = ST_RADIO2;
  else if(diSetupType.stSetupType1.bVisible == TRUE)
    sgProduct.dwCustomType = ST_RADIO1;
  else if(diSetupType.stSetupType0.bVisible == TRUE)
    sgProduct.dwCustomType = ST_RADIO0;
}

void STSetVisibility(st *stSetupType)
{
  if(*(stSetupType->szDescriptionShort) == '\0')
    stSetupType->bVisible = FALSE;
  else
    stSetupType->bVisible = TRUE;
}

HRESULT DecryptVariable(LPSTR szVariable, DWORD dwVariableSize)
{
  char szBuf[MAX_BUF];
  char szKey[MAX_BUF];
  char szName[MAX_BUF];
  char szValue[MAX_BUF];
  char szWRMSCurrentVersion[] = "Software\\Microsoft\\Windows\\CurrentVersion";
  char szWRMSShellFolders[]   = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders";
  char szWRMSMapGroup[]       = "Software\\Microsoft\\Windows\\CurrentVersion\\GrpConv\\MapGroup";


  /* zero out the memory allocations */
  ZeroMemory(szBuf,       sizeof(szBuf));
  ZeroMemory(szKey,       sizeof(szKey));
  ZeroMemory(szName,      sizeof(szName));
  ZeroMemory(szValue,     sizeof(szValue));

  if(lstrcmpi(szVariable, "PROGRAMFILESDIR") == 0)
  {
    /* parse for the "c:\Program Files" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, szWRMSCurrentVersion, "ProgramFilesDir", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "COMMONFILESDIR") == 0)
  {
    /* parse for the "c:\Program Files\Common Files" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, szWRMSCurrentVersion, "CommonFilesDir", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "MEDIAPATH") == 0)
  {
    /* parse for the "c:\Winnt40\Media" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, szWRMSCurrentVersion, "MediaPath", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "CONFIGPATH") == 0)
  {
    /* parse for the "c:\Windows\Config" directory */
    if(gSystemInfo.dwOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_LOCAL_MACHINE, szWRMSCurrentVersion, "ConfigPath", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "DEVICEPATH") == 0)
  {
    /* parse for the "c:\Winnt40\INF" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, szWRMSCurrentVersion, "DevicePath", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "COMMON_STARTUP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs\\Startup" directory */
    if(gSystemInfo.dwOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Startup", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, szWRMSShellFolders, "Common Startup", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs" directory */
    if((gSystemInfo.dwOSType & OS_WIN9x) || gbRestrictedAccess)
    {
      GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Programs", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, szWRMSShellFolders, "Common Programs", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "COMMON_PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs" directory */
    if(gSystemInfo.dwOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Programs", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, szWRMSShellFolders, "Common Programs", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "COMMON_STARTMENU") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu" directory */
    if(gSystemInfo.dwOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Start Menu", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, szWRMSShellFolders, "Common Start Menu", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "COMMON_DESKTOP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Desktop" directory */
    if(gSystemInfo.dwOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Desktop", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, szWRMSShellFolders, "Common Desktop", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "QUICK_LAUNCH") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Application Data\Microsoft\Internet Explorer\Quick Launch" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "AppData", szBuf, sizeof(szBuf));
    wsprintf(szVariable, "%s\\Microsoft\\Internet Explorer\\Quick Launch", szBuf);
    if(!FileExists(szVariable))
      GetWinReg(HKEY_CURRENT_USER, szWRMSMapGroup, "Quick Launch", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_STARTUP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu\Programs\Startup" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Startup", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu\Programs" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Programs", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_STARTMENU") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Start Menu", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_DESKTOP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Desktop" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Desktop", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_APPDATA") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Application Data" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "AppData", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_CACHE") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Temporary Internet Files" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Cache", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_COOKIES") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Cookies" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Cookies", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_FAVORITES") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Favorites" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Favorites", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_FONTS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Fonts" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Fonts", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_HISTORY") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\History" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "History", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_NETHOOD") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\NetHood" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "NetHood", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_PERSONAL") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Personal" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Personal", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_PRINTHOOD") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\PrintHood" directory */
    if(gSystemInfo.dwOSType & OS_NT)
    {
      GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "PrintHood", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "PERSONAL_RECENT") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Recent" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Recent", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_SENDTO") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\SendTo" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "SendTo", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_TEMPLATES") == 0)
  {
    /* parse for the "C:\WINNT40\ShellNew" directory */
    GetWinReg(HKEY_CURRENT_USER, szWRMSShellFolders, "Templates", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PROGRAMFOLDERNAME") == 0)
  {
    /* parse for the "Netscape Communicator" folder name */
    lstrcpy(szVariable, sgProduct.szProgramFolderName);
  }
  else if(lstrcmpi(szVariable, "PROGRAMFOLDERPATH") == 0)
  {
    /* parse for the "c:\Windows\profiles\All Users\Start Menu\Programs" path */
    lstrcpy(szVariable, sgProduct.szProgramFolderPath);
  }
  else if(lstrcmpi(szVariable, "PROGRAMFOLDERPATHNAME") == 0)
  {
    /* parse for the "c:\Windows\profiles\All Users\Start Menu\Programs\Netscape Communicator" path */
    lstrcpy(szVariable, sgProduct.szProgramFolderPath);
    lstrcat(szVariable, "\\");
    lstrcat(szVariable, sgProduct.szProgramFolderName);
  }
  else if(lstrcmpi(szVariable, "PROGRAMFOLDERPATH") == 0)
  {
    /* parse for the "c:\Windows\profiles\All Users\Start Menu\Programs" path */
    lstrcpy(szVariable, sgProduct.szProgramFolderPath);
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

      if(GetPrivateProfileString("Messages", "ERROR_GET_WINDOWS_DIRECTORY_FAILED", "", szEGetWinDirFailed, sizeof(szEGetWinDirFailed), szFileIniInstall))
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

      if(GetPrivateProfileString("Messages", "ERROR_GET_WINDOWS_DIRECTORY_FAILED", "", szEGetWinDirFailed, sizeof(szEGetWinDirFailed), szFileIniInstall))
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

      if(GetPrivateProfileString("Messages", "ERROR_GET_SYSTEM_DIRECTORY_FAILED", "", szEGetSysDirFailed, sizeof(szEGetSysDirFailed), szFileIniInstall))
        PrintError(szEGetSysDirFailed, ERROR_CODE_SHOW);

      exit(1);
    }
  }
  else if(lstrcmpi(szVariable, "JRE LIB PATH") == 0)
  {
    /* Locate the "c:\Program Files\JavaSoft\JRE\1.3\Bin" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\javaw.Exe", NULL, szVariable, dwVariableSize);
    if(*szVariable == '\0')
      return(FALSE);
    else
    {
      char szVarPathOnly[MAX_BUF];

      /* The path to javaw.exe is "...\jre\1.3.1\bin\javaw.exe", so we need to get it's
       * path only.  This should return:
       *
       *   ...\jre\1.3.1\bin\
       *
       * We remove the trailing backslash to get the following:
       *
       *   ...\jre\1.3.1\bin
       *
       * Then get the path again (the trailing backslash indicates that it's a
       * path, thus the lack of it indicates a file):
       *
       *   ...\jre\1.3.1\
       *
       * Now append 'lib' to it.  We are assuming that ...\jre\1.3.1\lib is at the
       * same dir level as ...\jre\1.3.1\bin:
       *
       *   ...\jre\1.3.1\lib
       */
      ParsePath(szVariable, szVarPathOnly, sizeof(szVarPathOnly), FALSE, PP_PATH_ONLY);
      RemoveBackSlash(szVarPathOnly);
      ParsePath(szVarPathOnly, szVariable, dwVariableSize, FALSE, PP_PATH_ONLY);
      AppendBackSlash(szVariable, dwVariableSize);
      lstrcat(szVariable, "lib");
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
      ParsePath(szVariable, szBuf, sizeof(szBuf), FALSE, PP_PATH_ONLY);
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
  else if(lstrcmpi(szVariable, "SETUP PATH") == 0)
  {
    lstrcpy(szVariable, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szVariable, dwVariableSize);
      lstrcat(szVariable, sgProduct.szSubPath);
    }
  }
  else if(lstrcmpi(szVariable, "Default Path") == 0)
  {
    lstrcpy(szVariable, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szVariable, dwVariableSize);
      lstrcat(szVariable, sgProduct.szSubPath);
    }
  }
  else if(lstrcmpi(szVariable, "SETUP STARTUP PATH") == 0)
  {
    lstrcpy(szVariable, szSetupDir);
  }
  else if(lstrcmpi(szVariable, "Default Folder") == 0)
  {
    lstrcpy(szVariable, sgProduct.szProgramFolderPath);
    AppendBackSlash(szVariable, dwVariableSize);
    lstrcat(szVariable, sgProduct.szProgramFolderName);
  }
  else if(lstrcmpi(szVariable, "Product CurrentVersion") == 0)
  {
    char szKey[MAX_BUF];

    wsprintf(szKey, "Software\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductName);

    /* parse for the current Netscape WinReg key */
    GetWinReg(HKEY_LOCAL_MACHINE, szKey, "CurrentVersion", szBuf, sizeof(szBuf));

    if(*szBuf == '\0')
      return(FALSE);

    wsprintf(szVariable, "Software\\%s\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductName, szBuf);
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
  char  szOutuptStrTemp[MAX_BUF];
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

    /* if Variable is "XPI PATH", do special processing */
    if(lstrcmpi(szVariable, "XPI PATH") == 0)
    {
      lstrcpy(szBuf, sgProduct.szAlternateArchiveSearchPath);
      RemoveBackSlash(szBuf);
      lstrcpy(szOutuptStrTemp, szPrepend);
      lstrcat(szOutuptStrTemp, szBuf);
      lstrcat(szOutuptStrTemp, szAppend);

      if((*sgProduct.szAlternateArchiveSearchPath != '\0') && FileExists(szOutuptStrTemp))
      {
        lstrcpy(szVariable, sgProduct.szAlternateArchiveSearchPath);
      }
      else
      {
        lstrcpy(szBuf, szSetupDir);
        RemoveBackSlash(szBuf);
        lstrcpy(szOutuptStrTemp, szPrepend);
        lstrcat(szOutuptStrTemp, szBuf);
        lstrcat(szOutuptStrTemp, szAppend);

        if(!FileExists(szOutuptStrTemp))
          lstrcpy(szVariable, szTempDir);
        else
          lstrcpy(szVariable, szSetupDir);
      }

      RemoveBackSlash(szVariable);
      bDecrypted = TRUE;
    }
    else
    {
      bDecrypted = DecryptVariable(szVariable, sizeof(szVariable));
    }

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

int ExtractDirEntries(char* directory, void* vZip)
{
  int   err;
  int   result;
  char  buf[512];  // XXX: need an XP "max path"

  int paths = 1;
  if(paths)
  {
    void* find = ZIP_FindInit(vZip, directory);

    if(find)
    {
      int prefix_length = 0;
      
      if(directory)
        prefix_length = lstrlen(directory) - 1;

      if(prefix_length >= sizeof(buf)-1)
        return ZIP_ERR_GENERAL;

      err = ZIP_FindNext( find, buf, sizeof(buf) );
      while ( err == ZIP_OK ) 
      {
        CreateDirectoriesAll(buf, FALSE);
        if(buf[lstrlen(buf) - 1] != '/')
          // only extract if it's a file
          result = ZIP_ExtractFile(vZip, buf, buf);
        err = ZIP_FindNext( find, buf, sizeof(buf) );
      }
      ZIP_FindFree( find );
    }
    else
      err = ZIP_ERR_GENERAL;

    if ( err == ZIP_ERR_FNF )
      return ZIP_OK;   // found them all
  }

  return ZIP_ERR_GENERAL;
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

BOOL NeedReboot()
{
   if(diReboot.dwShowDialog == AUTO)
     return(bReboot);
   else
     return(diReboot.dwShowDialog);
}

BOOL DeleteWGetLog(void)
{
  char  szFile[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFile, sizeof(szFile));

  lstrcpy(szFile, szTempDir);
  AppendBackSlash(szFile, sizeof(szFile));
  lstrcat(szFile, FILE_WGET_LOG);

  if(FileExists(szFile))
    bFileExists = TRUE;

  DeleteFile(szFile);
  return(bFileExists);
}

BOOL DeleteIdiGetConfigIni()
{
  char  szFileIdiGetConfigIni[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFileIdiGetConfigIni, sizeof(szFileIdiGetConfigIni));

  lstrcpy(szFileIdiGetConfigIni, szTempDir);
  AppendBackSlash(szFileIdiGetConfigIni, sizeof(szFileIdiGetConfigIni));
  lstrcat(szFileIdiGetConfigIni, FILE_IDI_GETCONFIGINI);
  if(FileExists(szFileIdiGetConfigIni))
  {
    bFileExists = TRUE;
  }
  DeleteFile(szFileIdiGetConfigIni);
  return(bFileExists);
}

BOOL DeleteInstallLogFile(char *szFile)
{
  char  szInstallLogFile[MAX_BUF];
  BOOL  bFileExists = FALSE;

  lstrcpy(szInstallLogFile, szTempDir);
  AppendBackSlash(szInstallLogFile, sizeof(szInstallLogFile));
  lstrcat(szInstallLogFile, szFile);

  if(FileExists(szInstallLogFile))
  {
    bFileExists = TRUE;
    DeleteFile(szInstallLogFile);
  }

  return(bFileExists);
}

BOOL DeleteIniRedirect()
{
  char  szFileIniRedirect[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFileIniRedirect, sizeof(szFileIniRedirect));

  lstrcpy(szFileIniRedirect, szTempDir);
  AppendBackSlash(szFileIniRedirect, sizeof(szFileIniRedirect));
  lstrcat(szFileIniRedirect, FILE_INI_REDIRECT);
  if(FileExists(szFileIniRedirect))
  {
    bFileExists = TRUE;
  }
  DeleteFile(szFileIniRedirect);
  return(bFileExists);
}

BOOL DeleteIdiGetRedirect()
{
  char  szFileIdiGetRedirect[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFileIdiGetRedirect, sizeof(szFileIdiGetRedirect));

  lstrcpy(szFileIdiGetRedirect, szTempDir);
  AppendBackSlash(szFileIdiGetRedirect, sizeof(szFileIdiGetRedirect));
  lstrcat(szFileIdiGetRedirect, FILE_IDI_GETREDIRECT);
  if(FileExists(szFileIdiGetRedirect))
  {
    bFileExists = TRUE;
  }
  DeleteFile(szFileIdiGetRedirect);
  return(bFileExists);
}

BOOL DeleteIdiGetArchives()
{
  char  szFileIdiGetArchives[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFileIdiGetArchives, sizeof(szFileIdiGetArchives));

  lstrcpy(szFileIdiGetArchives, szTempDir);
  AppendBackSlash(szFileIdiGetArchives, sizeof(szFileIdiGetArchives));
  lstrcat(szFileIdiGetArchives, FILE_IDI_GETARCHIVES);
  if(FileExists(szFileIdiGetArchives))
  {
    bFileExists = TRUE;
  }
  DeleteFile(szFileIdiGetArchives);
  return(bFileExists);
}

BOOL DeleteIdiFileIniConfig()
{
  char  szFileIniConfig[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFileIniConfig, sizeof(szFileIniConfig));

  lstrcpy(szFileIniConfig, szTempDir);
  AppendBackSlash(szFileIniConfig, sizeof(szFileIniConfig));
  lstrcat(szFileIniConfig, FILE_INI_CONFIG);
  if(FileExists(szFileIniConfig))
  {
    bFileExists = TRUE;
  }
  DeleteFile(szFileIniConfig);
  return(bFileExists);
}

BOOL DeleteIdiFileIniInstall()
{
  char  szFileIniInstall[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFileIniInstall, sizeof(szFileIniInstall));

  lstrcpy(szFileIniInstall, szTempDir);
  AppendBackSlash(szFileIniInstall, sizeof(szFileIniInstall));
  lstrcat(szFileIniInstall, FILE_INI_INSTALL);
  if(FileExists(szFileIniInstall))
  {
    bFileExists = TRUE;
  }
  DeleteFile(szFileIniInstall);
  return(bFileExists);
}

void DeleteArchives(DWORD dwDeleteCheck)
{
  DWORD dwIndex0;
  char  szArchiveName[MAX_BUF];
  siC   *siCObject = NULL;

  ZeroMemory(szArchiveName, sizeof(szArchiveName));

  if((!bSDUserCanceled) && (CheckForPreviousUnfinishedDownload() == FALSE))
  {
    dwIndex0 = 0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
    while(siCObject)
    {
      lstrcpy(szArchiveName, szTempDir);
      AppendBackSlash(szArchiveName, sizeof(szArchiveName));
      lstrcat(szArchiveName, siCObject->szArchiveName);

      switch(dwDeleteCheck)
      {
        case DA_ONLY_IF_IN_ARCHIVES_LST:
          if(IsInArchivesLst(siCObject, FALSE))
            DeleteFile(szArchiveName);
          break;

        case DA_ONLY_IF_NOT_IN_ARCHIVES_LST:
          if(!IsInArchivesLst(siCObject, FALSE))
            DeleteFile(szArchiveName);
          break;

        case DA_IGNORE_ARCHIVES_LST:
        default:
          DeleteFile(szArchiveName);
          break;
      }

      ++dwIndex0;
      siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
    }

    DeleteIniRedirect();
  }
}

void CleanTempFiles()
{
  DeleteIdiGetConfigIni();
  DeleteIdiGetArchives();
  DeleteIdiGetRedirect();

  /* do not delete config.ini file.
     if it was uncompressed from the self-extracting .exe file,
     then it will be deleted automatically.
     Right now the config.ini does not get downloaded, so we
     don't have to worry about that case
  */
  DeleteIdiFileIniConfig();
  DeleteIdiFileIniInstall();
  DeleteArchives(DA_IGNORE_ARCHIVES_LST);
  DeleteInstallLogFile(FILE_INSTALL_LOG);
  DeleteInstallLogFile(FILE_INSTALL_STATUS_LOG);
}

void SendErrorMessage(void)
{
  char szBuf[MAX_BUF];
  char *szPartialEscapedURL = NULL;

  /* append to the message stream the list of components that either had
   * network retries or crc retries */
  LogMSDownloadFileStatus();

  /* replace this function call with a call to xpnet */
  if((szPartialEscapedURL = nsEscape(gErrorMessageStream.szMessage,
                                     url_Path)) != NULL)
  {
    char  szWGetLog[MAX_BUF];
    char  szMsg[MAX_BUF];
    char  *szFullURL = NULL;
    DWORD dwSize;

    lstrcpy(szWGetLog, szTempDir);
    AppendBackSlash(szWGetLog, sizeof(szWGetLog));
    lstrcat(szWGetLog, FILE_WGET_LOG);

    /* take into account '?' and '\0' chars */
    dwSize = lstrlen(gErrorMessageStream.szURL) +
             lstrlen(szPartialEscapedURL) + 2;
    if((szFullURL = NS_GlobalAlloc(dwSize)) != NULL)
    {
      wsprintf(szFullURL,
               "%s?%s",
               gErrorMessageStream.szURL,
               szPartialEscapedURL);

      wsprintf(szMsg,
               "UnEscapedURL: %s?%s\nEscapedURL: %s",
               gErrorMessageStream.szURL,
               gErrorMessageStream.szMessage,
               szFullURL);

      if(gErrorMessageStream.bShowConfirmation &&
        (*gErrorMessageStream.szConfirmationMessage != '\0'))
      {
        char szConfirmationMessage[MAX_BUF];

        wsprintf(szBuf,
                 "\n\n  %s",
                 gErrorMessageStream.szMessage);
        wsprintf(szConfirmationMessage,
                 gErrorMessageStream.szConfirmationMessage,
                 szBuf);
        if(MessageBox(hWndMain,
                      szConfirmationMessage,
                      sgProduct.szProductName,
                      MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
        {
          //PrintError(szMsg, ERROR_CODE_HIDE);
          WGet(szFullURL,
               szWGetLog,
               diAdvancedSettings.szProxyServer,
               diAdvancedSettings.szProxyPort,
               diAdvancedSettings.szProxyUser,
               diAdvancedSettings.szProxyPasswd);
        }
      }
      else if(!gErrorMessageStream.bShowConfirmation)
      {
        //PrintError(szMsg, ERROR_CODE_HIDE);
        WGet(szFullURL,
             szWGetLog,
             diAdvancedSettings.szProxyServer,
             diAdvancedSettings.szProxyPort,
             diAdvancedSettings.szProxyUser,
             diAdvancedSettings.szProxyPasswd);
      }

      FreeMemory(&szFullURL);
    }

    FreeMemory(&szPartialEscapedURL);
  }
}

void DeInitialize()
{
  char szBuf[MAX_BUF];

  LogISTime(W_END);
  if(bCreateDestinationDir)
  {
    lstrcpy(szBuf, sgProduct.szPath);
    AppendBackSlash(szBuf, sizeof(szBuf));
    DirectoryRemove(szBuf, FALSE);
  }

  if(hbmpBoxChecked)
    DeleteObject(hbmpBoxChecked);
  if(hbmpBoxCheckedDisabled)
    DeleteObject(hbmpBoxCheckedDisabled);
  if(hbmpBoxUnChecked)
    DeleteObject(hbmpBoxUnChecked);

  DeleteWGetLog();
  CleanTempFiles();
  DirectoryRemove(szTempDir, FALSE);

  if(gErrorMessageStream.bEnabled && gErrorMessageStream.bSendMessage)
    SendErrorMessage();

  DeInitSiComponents(&siComponents);
  DeInitSXpcomFile();
  DeInitSDObject();
  DeInitDlgReboot(&diReboot);
  DeInitDlgDownload(&diDownload);
  DeInitDlgStartInstall(&diStartInstall);
  DeInitDlgDownloadOptions(&diDownloadOptions);
  DeInitDlgAdvancedSettings(&diAdvancedSettings);
  DeInitDlgProgramFolder(&diProgramFolder);
  DeInitDlgWindowsIntegration(&diWindowsIntegration);
  DeInitDlgSelectComponents(&diSelectAdditionalComponents);
  DeInitDlgSelectComponents(&diSelectComponents);
  DeInitDlgSetupType(&diSetupType);
  DeInitDlgWelcome(&diWelcome);
  DeInitDlgLicense(&diLicense);
  DeInitDlgQuickLaunch(&diQuickLaunch);
  DeInitSetupGeneral();
  DeInitDSNode(&gdsnComponentDSRequirement);
  DeInitErrorMessageStream();

  FreeMemory(&szTempDir);
  FreeMemory(&szOSTempDir);
  FreeMemory(&szSetupDir);
  FreeMemory(&szFileIniConfig);
  FreeMemory(&szFileIniInstall);
  FreeMemory(&szEGlobalAlloc);
  FreeMemory(&szEDllLoad);
  FreeMemory(&szEStringLoad);
  FreeMemory(&szEStringNull);
  DeleteObject(sgInstallGui.systemFont);
  DeleteObject(sgInstallGui.definedFont);

  FreeLibrary(hSetupRscInst);
}

char *GetSaveInstallerPath(char *szBuf, DWORD dwBufSize)
{
#ifdef XXX_INTL_HACK_WORKAROUND_FOR_NOW
  char szBuf2[MAX_BUF];
#endif

  /* determine the path to where the setup and downloaded files will be saved to */
  lstrcpy(szBuf, sgProduct.szPath);
  AppendBackSlash(szBuf, dwBufSize);
  if(*sgProduct.szSubPath != '\0')
  {
    lstrcat(szBuf, sgProduct.szSubPath);
    lstrcat(szBuf, " ");
  }

#ifdef XXX_INTL_HACK_WORKAROUND_FOR_NOW
/* Installer can't create the Save Installer Path if the word "Setup" is localized. */
  if(GetPrivateProfileString("Messages", "STR_SETUP", "", szBuf2, sizeof(szBuf2), szFileIniInstall))
    lstrcat(szBuf, szBuf2);
  else
#endif
    lstrcat(szBuf, "Setup");

  return(szBuf);
}

void SaveInstallerFiles()
{
  int       i;
  char      szBuf[MAX_BUF];
  char      szSource[MAX_BUF];
  char      szDestination[MAX_BUF];
  char      szMFN[MAX_BUF];
  char      szArchivePath[MAX_BUF];
  DWORD     dwIndex0;
  siC       *siCObject = NULL;

  GetSaveInstallerPath(szDestination, sizeof(szDestination));
  AppendBackSlash(szDestination, sizeof(szDestination));

  /* copy all files from the ns_temp dir to the install dir */
  CreateDirectoriesAll(szDestination, TRUE);

  /* copy the self extracting file that spawned setup.exe, if one exists */
  if((*sgProduct.szAlternateArchiveSearchPath != '\0') && (*sgProduct.szParentProcessFilename != '\0'))
  {
    lstrcpy(szSource, szSetupDir);
    AppendBackSlash(szSource, sizeof(szSource));
    lstrcat(szSource, "*.*");

    lstrcpy(szSource, sgProduct.szAlternateArchiveSearchPath);
    AppendBackSlash(szSource, sizeof(szSource));
    lstrcat(szSource, sgProduct.szParentProcessFilename);
    FileCopy(szSource, szDestination, FALSE, FALSE);
  }
  else
  {
    /* Else if self extracting file does not exist, copy the setup files */
    /* First get the current process' filename (in case it's not really named setup.exe */
    /* Then copy it to the install folder */
    GetModuleFileName(NULL, szBuf, sizeof(szBuf));
    ParsePath(szBuf, szMFN, sizeof(szMFN), FALSE, PP_FILENAME_ONLY);

    lstrcpy(szBuf, szSetupDir);
    AppendBackSlash(szBuf, sizeof(szBuf));
    lstrcat(szBuf, szMFN);
    FileCopy(szBuf, szDestination, FALSE, FALSE);

    /* now copy the rest of the setup files */
    i = 0;
    while(TRUE)
    {
      if(*SetupFileList[i] == '\0')
        break;

      lstrcpy(szBuf, szSetupDir);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, SetupFileList[i]);
      FileCopy(szBuf, szDestination, FALSE, FALSE);

      ++i;
    }
  }

  dwIndex0 = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    LocateJar(siCObject, szArchivePath, sizeof(szArchivePath), TRUE);
    if(*szArchivePath != '\0')
    {
      lstrcpy(szBuf, szArchivePath);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, siCObject->szArchiveName);
      FileCopy(szBuf, szDestination, FALSE, FALSE);
    }

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }
}






//
// The functions below were copied from MSDN 6.0:
//
//   \samples\vc98\sdk\sdktools\winnt\pviewer
//
// They were listed in the redist.txt file from the cd.
// Only CheckProcessNT4() was modified to accomodate the setup needs.
//
/******************************************************************************\
 * *       This is a part of the Microsoft Source Code Samples.
 * *       Copyright (C) 1993-1997 Microsoft Corporation.
 * *       All rights reserved.
\******************************************************************************/
BOOL CheckProcessNT4(LPSTR szProcessName, DWORD dwProcessNameSize)
{
  BOOL          bRv;
  HKEY          hKey;
  DWORD         dwType;
  DWORD         dwSize;
  DWORD         dwTemp;
  DWORD         dwTitleLastIdx;
  DWORD         dwIndex;
  DWORD         dwLen;
  DWORD         pDataSize = 50 * 1024;
  LPSTR         szCounterValueName;
  LPSTR         szTitle;
  LPSTR         *szTitleSz;
  LPSTR         szTitleBuffer;
  PPERF_DATA    pData;
  PPERF_OBJECT  pProcessObject;

  bRv   = FALSE;
  hKey  = NULL;

  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                  TEXT("Software\\Microsoft\\Windows NT\\CurrentVersion\\perflib"),
                  0,
                  KEY_READ,
                  &hKey) != ERROR_SUCCESS)
    return(bRv);

  dwSize = sizeof(dwTitleLastIdx);
  if(RegQueryValueEx(hKey, TEXT("Last Counter"), 0, &dwType, (LPBYTE)&dwTitleLastIdx, &dwSize) != ERROR_SUCCESS)
  {
    RegCloseKey(hKey);
    return(bRv);
  }
    

  dwSize = sizeof(dwTemp);
  if(RegQueryValueEx(hKey, TEXT("Version"), 0, &dwType, (LPBYTE)&dwTemp, &dwSize) != ERROR_SUCCESS)
  {
    RegCloseKey(hKey);
    szCounterValueName = TEXT("Counters");
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                    TEXT("Software\\Microsoft\\Windows NT\\CurrentVersion\\Perflib\\009"),
                    0,
                    KEY_READ,
                    &hKey) != ERROR_SUCCESS)
      return(bRv);
  }
  else
  {
    RegCloseKey(hKey);
    szCounterValueName = TEXT("Counter 009");
    hKey = HKEY_PERFORMANCE_DATA;
  }

  // Find out the size of the data.
  //
  dwSize = 0;
  if(RegQueryValueEx(hKey, szCounterValueName, 0, &dwType, 0, &dwSize) != ERROR_SUCCESS)
    return(bRv);


  // Allocate memory
  //
  szTitleBuffer = (LPTSTR)LocalAlloc(LMEM_FIXED, dwSize);
  if(!szTitleBuffer)
  {
    RegCloseKey(hKey);
    return(bRv);
  }

  szTitleSz = (LPTSTR *)LocalAlloc(LPTR, (dwTitleLastIdx + 1) * sizeof(LPTSTR));
  if(!szTitleSz)
  {
    if(szTitleBuffer != NULL)
    {
      LocalFree(szTitleBuffer);
      szTitleBuffer = NULL;
    }

    RegCloseKey(hKey);
    return(bRv);
  }

  // Query the data
  //
  if(RegQueryValueEx(hKey, szCounterValueName, 0, &dwType, (BYTE *)szTitleBuffer, &dwSize) != ERROR_SUCCESS)
  {
    RegCloseKey(hKey);
    if(szTitleSz)
      LocalFree(szTitleSz);
    if(szTitleBuffer)
      LocalFree(szTitleBuffer);

    return(bRv);
  }

  // Setup the TitleSz array of pointers to point to beginning of each title string.
  // TitleBuffer is type REG_MULTI_SZ.
  //
  szTitle = szTitleBuffer;
  while(dwLen = lstrlen(szTitle))
  {
    dwIndex = atoi(szTitle);
    szTitle = szTitle + dwLen + 1;

    if(dwIndex <= dwTitleLastIdx)
      szTitleSz[dwIndex] = szTitle;

    szTitle = szTitle + lstrlen(szTitle) + 1;
  }

  PX_PROCESS = GetTitleIdx (NULL, szTitleSz, dwTitleLastIdx, PN_PROCESS);
  PX_THREAD  = GetTitleIdx (NULL, szTitleSz, dwTitleLastIdx, PN_THREAD);
  wsprintf(INDEX_PROCTHRD_OBJ, TEXT("%ld %ld"), PX_PROCESS, PX_THREAD);
  pData = NULL;
  if(GetPerfData(HKEY_PERFORMANCE_DATA, INDEX_PROCTHRD_OBJ, &pData, &pDataSize) == ERROR_SUCCESS)
  {
    PPERF_INSTANCE pInst;
    DWORD          i = 0;

    pProcessObject = FindObject(pData, PX_PROCESS);
    if(pProcessObject)
    {
      LPSTR szPtrStr;
      int   iLen;
      char  szProcessNamePruned[MAX_BUF];
      char  szNewProcessName[MAX_BUF];

      if(sizeof(szProcessNamePruned) < (lstrlen(szProcessName) + 1))
      {
        if(hKey)
          RegCloseKey(hKey);
        if(szTitleSz)
          LocalFree(szTitleSz);
        if(szTitleBuffer)
          LocalFree(szTitleBuffer);

        return(bRv);
      }

      /* look for .exe and remove from end of string because the Process names
       * under NT are returned without the extension */
      lstrcpy(szProcessNamePruned, szProcessName);
      iLen = lstrlen(szProcessNamePruned);
      szPtrStr = &szProcessNamePruned[iLen - 4];
      if((lstrcmpi(szPtrStr, ".exe") == 0) || (lstrcmpi(szPtrStr, ".dll") == 0))
        *szPtrStr = '\0';

      pInst = FirstInstance(pProcessObject);
      for(i = 0; i < (DWORD)(pProcessObject->NumInstances); i++)
      {
        ZeroMemory(szNewProcessName, MAX_BUF);
        if(WideCharToMultiByte(CP_ACP,
                               0,
                               (LPCWSTR)((PCHAR)pInst + pInst->NameOffset),
                               -1,
                               szNewProcessName,
                               MAX_BUF,
                               NULL,
                               NULL) != 0)
        {
          if(lstrcmpi(szNewProcessName, szProcessNamePruned) == 0)
          {
            bRv = TRUE;
            break;
          }
        }

        pInst = NextInstance(pInst);
      }
    }
  }

  if(hKey)
    RegCloseKey(hKey);
  if(szTitleSz)
    LocalFree(szTitleSz);
  if(szTitleBuffer)
    LocalFree(szTitleBuffer);

  return(bRv);
}


//*********************************************************************
//
//  GetPerfData
//
//      Get a new set of performance data.
//
//      *ppData should be NULL initially.
//      This function will allocate a buffer big enough to hold the
//      data requested by szObjectIndex.
//
//      *pDataSize specifies the initial buffer size.  If the size is
//      too small, the function will increase it until it is big enough
//      then return the size through *pDataSize.  Caller should
//      deallocate *ppData if it is no longer being used.
//
//      Returns ERROR_SUCCESS if no error occurs.
//
//      Note: the trial and error loop is quite different from the normal
//            registry operation.  Normally if the buffer is too small,
//            RegQueryValueEx returns the required size.  In this case,
//            the perflib, since the data is dynamic, a buffer big enough
//            for the moment may not be enough for the next. Therefor,
//            the required size is not returned.
//
//            One should start with a resonable size to avoid the overhead
//            of reallocation of memory.
//
DWORD   GetPerfData    (HKEY        hPerfKey,
                        LPTSTR      szObjectIndex,
                        PPERF_DATA  *ppData,
                        DWORD       *pDataSize)
{
DWORD   DataSize;
DWORD   dwR;
DWORD   Type;


    if (!*ppData)
        *ppData = (PPERF_DATA) LocalAlloc (LMEM_FIXED, *pDataSize);


    do  {
        DataSize = *pDataSize;
        dwR = RegQueryValueEx (hPerfKey,
                               szObjectIndex,
                               NULL,
                               &Type,
                               (BYTE *)*ppData,
                               &DataSize);

        if (dwR == ERROR_MORE_DATA)
            {
            LocalFree (*ppData);
            *pDataSize += 1024;
            *ppData = (PPERF_DATA) LocalAlloc (LMEM_FIXED, *pDataSize);
            }

        if (!*ppData)
            {
            LocalFree (*ppData);
            return ERROR_NOT_ENOUGH_MEMORY;
            }

        } while (dwR == ERROR_MORE_DATA);

    return dwR;
}




//*********************************************************************
//
//  FirstInstance
//
//      Returns pointer to the first instance of pObject type.
//      If pObject is NULL then NULL is returned.
//
PPERF_INSTANCE   FirstInstance (PPERF_OBJECT pObject)
{
    if (pObject)
        return (PPERF_INSTANCE)((PCHAR) pObject + pObject->DefinitionLength);
    else
        return NULL;
}

//*********************************************************************
//
//  NextInstance
//
//      Returns pointer to the next instance following pInst.
//
//      If pInst is the last instance, bogus data maybe returned.
//      The caller should do the checking.
//
//      If pInst is NULL, then NULL is returned.
//
PPERF_INSTANCE   NextInstance (PPERF_INSTANCE pInst)
{
PERF_COUNTER_BLOCK *pCounterBlock;

    if (pInst)
        {
        pCounterBlock = (PERF_COUNTER_BLOCK *)((PCHAR) pInst + pInst->ByteLength);
        return (PPERF_INSTANCE)((PCHAR) pCounterBlock + pCounterBlock->ByteLength);
        }
    else
        return NULL;
}

//*********************************************************************
//
//  FirstObject
//
//      Returns pointer to the first object in pData.
//      If pData is NULL then NULL is returned.
//
PPERF_OBJECT FirstObject (PPERF_DATA pData)
{
    if (pData)
        return ((PPERF_OBJECT) ((PBYTE) pData + pData->HeaderLength));
    else
        return NULL;
}




//*********************************************************************
//
//  NextObject
//
//      Returns pointer to the next object following pObject.
//
//      If pObject is the last object, bogus data maybe returned.
//      The caller should do the checking.
//
//      If pObject is NULL, then NULL is returned.
//
PPERF_OBJECT NextObject (PPERF_OBJECT pObject)
{
    if (pObject)
        return ((PPERF_OBJECT) ((PBYTE) pObject + pObject->TotalByteLength));
    else
        return NULL;
}

//*********************************************************************
//
//  FindObject
//
//      Returns pointer to object with TitleIndex.  If not found, NULL
//      is returned.
//
PPERF_OBJECT FindObject (PPERF_DATA pData, DWORD TitleIndex)
{
PPERF_OBJECT pObject;
DWORD        i = 0;

    if (pObject = FirstObject (pData))
        while (i < pData->NumObjectTypes)
            {
            if (pObject->ObjectNameTitleIndex == TitleIndex)
                return pObject;

            pObject = NextObject (pObject);
            i++;
            }

    return NULL;
}

//********************************************************
//
//  GetTitleIdx
//
//      Searches Titles[] for Name.  Returns the index found.
//
DWORD GetTitleIdx(HWND hWnd, LPTSTR Title[], DWORD LastIndex, LPTSTR Name)
{
  DWORD Index;

  for(Index = 0; Index <= LastIndex; Index++)
    if(Title[Index])
      if(!lstrcmpi (Title[Index], Name))
        return(Index);

  MessageBox(hWnd, Name, TEXT("Pviewer cannot find index"), MB_OK);
  return 0;
}

