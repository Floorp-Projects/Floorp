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

#define INCL_DOSNLS
#define INCL_DOSERRORS
#define INCL_DOSMISC

#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"
#include "xpnetHook.h"
#include "time.h"
#include "xpi.h"
#include "logging.h"
#include "nsEscape.h"
//#include <winnls.h>
//#include <winver.h>
//#include <tlhelp32.h>
//#include <winperf.h>

#define HIDWORD(l)   ((ULONG) (((ULONG) (l) >> 32) & 0xFFFF))
#define LODWORD(l)   ((ULONG) (l))

#define INDEX_STR_LEN       10
#define PN_PROCESS          TEXT("Process")
#define PN_THREAD           TEXT("Thread")

#define FTPSTR_LEN (sizeof(szFtp) - 1)
#define HTTPSTR_LEN (sizeof(szHttp) - 1)

ULONG  (EXPENTRY *NS_GetDiskFreeSpace)(PCSZ, PULONG, PULONG, PULONG, PULONG);
ULONG  (EXPENTRY *NS_GetDiskFreeSpaceEx)(PCSZ, PULONG, PULONG, PULONG);

typedef BOOL   (EXPENTRY *NS_ProcessWalk)(LHANDLE hSnapshot, LPPROCESSENTRY32 lppe);
typedef LHANDLE (EXPENTRY *NS_CreateSnapshot)(ULONG dwFlags, ULONG th32ProcessID);
//typedef PERF_DATA_BLOCK             PERF_DATA,      *PPERF_DATA;
//typedef PERF_OBJECT_TYPE            PERF_OBJECT,    *PPERF_OBJECT;
//typedef PERF_INSTANCE_DEFINITION    PERF_INSTANCE,  *PPERF_INSTANCE;
char *ArchiveExtensions[] = {"zip",
                             "xpi",
                             "jar",
                             ""};
CHAR   INDEX_PROCTHRD_OBJ[2*INDEX_STR_LEN];
ULONG   PX_PROCESS;
ULONG   PX_THREAD;


#define SETUP_STATE_REG_KEY "Software\\%s\\%s\\%s\\Setup"

typedef struct structVer
{
  ULONGLONG ullMajor;
  ULONGLONG ullMinor;
  ULONGLONG ullRelease;
  ULONGLONG ullBuild;
} verBlock;

void  TranslateVersionStr(PSZ szVersion, verBlock *vbVersion);
BOOL  GetFileVersion(PSZ szFile, verBlock *vbVersion);
int   CompareVersion(verBlock vbVersionOld, verBlock vbVersionNew);
BOOL CheckProcessNT4(PSZ szProcessName, ULONG dwProcessNameSize);
ULONG GetTitleIdx(HWND hWnd, PSZ Title[], ULONG LastIndex, PSZ Name);
//PPERF_OBJECT FindObject (PPERF_DATA pData, ULONG TitleIndex);
//PPERF_OBJECT NextObject (PPERF_OBJECT pObject);
//PPERF_OBJECT FirstObject (PPERF_DATA pData);
//PPERF_INSTANCE   NextInstance (PPERF_INSTANCE pInst);
//PPERF_INSTANCE   FirstInstance (PPERF_OBJECT pObject);
//ULONG   GetPerfData    (HINI        hPerfKey,
//                        PSZ      szObjectIndex,
//                        PPERF_DATA  *ppData,
//                        ULONG       *pDataSize);

BOOL InitDialogClass(HOBJECT hInstance, HOBJECT hSetupRscInst)
{
  APIRET rc = NO_ERROR;
/*  WNDCLASS  wc;

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
*/
  rc = WinRegisterClass((HAB)0, (PSZ)CLASS_NAME_SETUP_DLG, (PFNWP)WinDefDlgProc, CS_SAVEBITS, 0);
  return(rc);
}

BOOL InitApplication(HOBJECT hInstance, HOBJECT hSetupRscInst)
{
  return(InitDialogClass(hInstance, hSetupRscInst));
}

BOOL InitInstance(HOBJECT hInstance, ULONG dwCmdShow)
{
  gSystemInfo.dwScreenX = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
  gSystemInfo.dwScreenY = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);

  hInst = hInstance;
  hWndMain = NULL;

  return(TRUE);
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

  if((sgProduct.dwMode != SILENT) && (sgProduct.dwMode != AUTO))
  {
    WinMessageBox(HWND_DESKTOP, hWndMain, szErrorString, NULL, 0, MB_ICONEXCLAMATION);
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
void *NS_GlobalReAlloc(HPOINTER *hgMemory,
                       ULONG dwMemoryBufSize,
                       ULONG dwNewSize)
{
  HPOINTER hgPtr = NULL;

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

void *NS_GlobalAlloc(ULONG dwMaxBuf)
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
    memset(vBuf, 0, dwMaxBuf);
    return(vBuf);
  }
}

void FreeMemory(void **vPointer)
{
  if(*vPointer != NULL)
        DosFreeMem(*vPointer);
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

BOOL VerifyRestrictedAccess(void)
{
  char  szSubKey[MAX_BUF];
  char  szSubKeyToTest[] = "Software\\%s - Test Key";
  BOOL  bRv;
  ULONG dwDisp = 0;
  ULONG dwErr;
  HKEY  hkRv;

  sprintf(szSubKey, szSubKeyToTest, sgProduct.szCompanyName);
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

  sprintf(szKey,
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

  sprintf(szKey,
           SETUP_STATE_REG_KEY,
           sgProduct.szCompanyName,
           sgProduct.szProductName,
           sgProduct.szUserAgent);
  strcpy(szValue, "downloading");

  SetWinReg(HKEY_CURRENT_USER, szKey, TRUE, "Setup State", TRUE,
            REG_SZ, szValue, strlen(szValue), TRUE, FALSE);
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
    sprintf(szKey,
             SETUP_STATE_REG_KEY,
             sgProduct.szCompanyName,
             sgProduct.szProductName,
             sgProduct.szUserAgent);
    PrfQueryProfileString(HINI_USERPROFILE, szKey, "Setup State", "", szBuf, sizeof(szBuf));
    if(strcmpi(szBuf, "downloading") == 0)
      bRv = TRUE;
  }

  return(bRv);
}

void UnsetSetupCurrentDownloadFile(void)
{
  char szKey[MAX_BUF];

  sprintf(szKey,
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

  sprintf(szKey,
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
            strlen(szCurrentFilename),
            TRUE,
            FALSE);
}

char *GetSetupCurrentDownloadFile(char *szCurrentDownloadFile,
                                  ULONG dwCurrentDownloadFileBufSize)
{
  char szKey[MAX_BUF];

  if(!szCurrentDownloadFile)
    return(NULL);

  memset(szCurrentDownloadFile, 0, dwCurrentDownloadFileBufSize);
  if(sgProduct.szCompanyName &&
     sgProduct.szProductName &&
     sgProduct.szUserAgent)
  {
    sprintf(szKey,
             SETUP_STATE_REG_KEY,
             sgProduct.szCompanyName,
             sgProduct.szProductName,
             sgProduct.szUserAgent);
    PrfQueryProfileString(HINI_USERPROFILE, szKey, "Setup Current Download", "", szCurrentDownloadFile, dwCurrentDownloadFileBuf);
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

  if(strlen(szIgnoreStr) < sizeof(szLCIgnoreLongStr))
  {
    strcpy(szLCIgnoreLongStr, szIgnoreStr);
    CharLower(szLCIgnoreLongStr);
  }
  if(strlen(szIgnoreStr) < sizeof(szLCIgnoreShortStr))
  {
    GetShortPathName(szIgnoreStr, szLCIgnoreShortStr, sizeof(szLCIgnoreShortStr));
    CharLower(szLCIgnoreShortStr);
  }

  while(fgets(szLineRead, sizeof(szLineRead), ifp) != NULL)
  {
    strcpy(szLCLineRead, szLineRead);
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
  ULONG dwOutMultiStrLen;
  ULONG dwType;
  BOOL  bFoundUninstaller = FALSE;

  if(!GetWindowsDirectory(szWinDir, sizeof(szWinDir)))
    return;

  sprintf(szLCUninstallFilenameLongBuf, "%s\\%s", szWinDir, sgProduct.szUninstallFilename);
  GetShortPathName(szLCUninstallFilenameLongBuf, szLCUninstallFilenameShortBuf, sizeof(szLCUninstallFilenameShortBuf));
  CharLower(szLCUninstallFilenameLongBuf);
  CharLower(szLCUninstallFilenameShortBuf);

  if(gSystemInfo.dwOSType & OS_NT)
  {
    memset(szInMultiStr, 0, sizeof(szInMultiStr));
    memset(szOutMultiStr, 0, sizeof(szOutMultiStr));

//    dwType = GetWinReg(HKEY_LOCAL_MACHINE,
//                       "System\\CurrentControlSet\\Control\\Session Manager",
//                       "PendingFileRenameOperations",
//                       szInMultiStr,
//                       sizeof(szInMultiStr));
    PrfQueryProfileString(HINI_SYSTEMPROFILE,
                        "System\\CurrentControlSet\\Control\\Session Manager",
                        "PendingFileRenameOperations",
                        "",
                        szInMultiStr,
                        sizeof(szInMultiStr));
//    if((dwType == REG_MULTI_SZ) && (szInMultiStr != '\0'))
//    {
//      szPtrIn          = szInMultiStr;
//      szPtrOut         = szOutMultiStr;
//      dwOutMultiStrLen = 0;
//      do
//      {
//        strcpy(szLCKeyBuf, szPtrIn);
//        CharLower(szLCKeyBuf);
//        if(!strstr(szLCKeyBuf, szLCUninstallFilenameLongBuf) && !strstr(szLCKeyBuf, szLCUninstallFilenameShortBuf))
//        {
//          if((dwOutMultiStrLen + strlen(szPtrIn) + 3) <= sizeof(szOutMultiStr))
//          {
            /* uninstaller not found, so copy the szPtrIn string to szPtrOut buffer */
//            strcpy(szPtrOut, szPtrIn);
//            dwOutMultiStrLen += strlen(szPtrIn) + 2;            /* there are actually 2 NULL bytes between the strings */
//            szPtrOut          = &szPtrOut[strlen(szPtrIn) + 2]; /* there are actually 2 NULL bytes between the strings */
//          }
//          else
//          {
//            bFoundUninstaller = FALSE;
            /* not enough memory; break out of while loop. */
//            break;
//          }
//        }
//        else
//          bFoundUninstaller = TRUE;

//        szPtrIn = &szPtrIn[strlen(szPtrIn) + 2];              /* there are actually 2 NULL bytes between the strings */
//      }while(*szPtrIn != '\0');
//    }

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
    sprintf(szWinInitFile, "%s\\wininit.ini", szWinDir);
    sprintf(szTempInitFile, "%s\\wininit.moz", szWinDir);
    if(FileExists(szWinInitFile))
    {
      if(UpdateFile(szWinInitFile, szTempInitFile, szLCUninstallFilenameLongBuf))
        CopyFile(szTempInitFile, szWinInitFile, FALSE);

      DosDelete(szTempInitFile);
    }
  }
}

APIRET Initialize(HOBJECT hInstance)
{
  char szBuf[MAX_BUF];

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

  hAccelTable = LoadAccelerators(hInstance, CLASS_NAME_SETUP_DLG);

  if(DosLoadModule(NULL, 0, "Setuprsc.dll", hSetupRscInst) != NO_ERROR)
  {
    char szFullFilename[MAX_BUF];

    DosQueryModuleName(NULL, sizeof(szBuf), szBuf);
    ParsePath(szBuf, szFullFilename,
              sizeof(szFullFilename), FALSE, PP_PATH_ONLY);
    AppendBackSlash(szFullFilename, sizeof(szFullFilename));
    strcat(szFullFilename, "Setuprsc.dll");

    if(DosLoadModule(NULL, 0, szFullFilename, hSetupRscInst) != NO_ERROR)
    {
      sprintf(szBuf, szEDllLoad, szFullFilename);
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

  DosQueryCurrentDir(0, szSetupDir, MAX_BUF);

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

      HINI hini = PrfOpenProfile((HAB)0, szFileIniInstall);
      if(PrfQueryProfileString(hini, "Messages", "ERROR_GET_WINDOWS_DIRECTORY_FAILED", "", szEGetWinDirFailed, sizeof(szEGetWinDirFailed)))
        PrintError(szEGetWinDirFailed, ERROR_CODE_SHOW);

      PrfCloseProfile(hini);
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
    CreateDirectoriesAll(szTempDir, FALSE);
    if(!FileExists(szTempDir))
    {
      char szECreateTempDir[MAX_BUF];

      HINI hini = PrfOpenProfile((HAB)0, szFileIniInstall);
      if(PrfQueryProfileString(hini, "Messages", "ERROR_CREATE_TEMP_DIR", "", szECreateTempDir, sizeof(szECreateTempDir)))
      {
        sprintf(szBuf, szECreateTempDir, szTempDir);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }
      PrfCloseProfile(hini);
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

BOOL IsInArchivesLst(siC *siCObject, BOOL bModify)
{
  char *szBufPtr;
  char szBuf[MAX_BUF];
  char szArchiveLstFile[MAX_BUF_MEDIUM];
  BOOL bRet = FALSE;
  HINI   hini;

  strcpy(szArchiveLstFile, szTempDir);
  AppendBackSlash(szArchiveLstFile, sizeof(szArchiveLstFile));
  strcat(szArchiveLstFile, "Archive.lst");
  hini = PrfOpenProfile((HAB)0, szArchiveLstFile); 
  WinQueryProfileString(hini, "Archives", NULL, "", szBuf, sizeof(szBuf));
  PrfCloseProfile(hini);
  if(*szBuf != '\0')
  {
    szBufPtr = szBuf;
    while(*szBufPtr != '\0')
    {
      if(strcmpi(siCObject->szArchiveName, szBufPtr) == 0)
      {
        if(bModify)
        {
          /* jar file found.  Unset attribute to download from the net */
          siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
          /* save the path of where jar was found at */
          strcpy(siCObject->szArchivePath, szTempDir);
          AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
        }
        bRet = TRUE;

        /* found what we're looking for.  No need to continue */
        break;
      }
      szBufPtr += strlen(szBufPtr) + 1;
    }
  }
  return(bRet);
}

APIRET ParseSetupIni()
{
  char szBuf[MAX_BUF];
  char szFileIniSetup[MAX_BUF];
  char szFileIdiGetConfigIni[MAX_BUF];

  strcpy(szFileIdiGetConfigIni, szTempDir);
  AppendBackSlash(szFileIdiGetConfigIni, sizeof(szFileIdiGetConfigIni));
  strcat(szFileIdiGetConfigIni, FILE_IDI_GETCONFIGINI);

  strcpy(szFileIniSetup, szSetupDir);
  AppendBackSlash(szFileIniSetup, sizeof(szFileIniSetup));
  strcat(szFileIniSetup, FILE_INI_SETUP);

  CopyFile(szFileIniSetup, szFileIdiGetConfigIni, FALSE);

  if(!FileExists(szFileIdiGetConfigIni))
  {
    char szEFileNotFound[MAX_BUF];

    HINI hini = PrfOpenProfile((HAB)0, szFileIniInstall);
    if(WinQueryProfileString(hini, "Messages", "ERROR_FILE_NOT_FOUND", "", szEFileNotFound, sizeof(szEFileNotFound)))
    {
      sprintf(szBuf, szEFileNotFound, szFileIdiGetConfigIni);
      PrintError(szBuf, ERROR_CODE_HIDE);
    }
    PrfCloseProfile(hini);
    return(1);
  }

  return(0);
}

APIRET GetConfigIni()
{
  char    szFileIniTempDir[MAX_BUF];
  char    szFileIniSetupDir[MAX_BUF];
  char    szMsgRetrieveConfigIni[MAX_BUF];
  char    szBuf[MAX_BUF];
  APIRET hResult = 0;

  HINI hini = PrfOpenProfile((HAB)0, szFileIniInstall);
  if(!WinQueryProfileString(hini, "Messages", "MSG_RETRIEVE_CONFIGINI", "", szMsgRetrieveConfigIni, sizeof(szMsgRetrieveConfigIni)))
  {
    PrfCloseProfile(hini);
    return(1);
  }
    
  strcpy(szFileIniTempDir, szTempDir);
  AppendBackSlash(szFileIniTempDir, sizeof(szFileIniTempDir));
  strcat(szFileIniTempDir, FILE_INI_CONFIG);

  /* set default value for szFileIniConfig here */
  strcpy(szFileIniConfig, szFileIniTempDir);

  strcpy(szFileIniSetupDir, szSetupDir);
  AppendBackSlash(szFileIniSetupDir, sizeof(szFileIniSetupDir));
  strcat(szFileIniSetupDir, FILE_INI_CONFIG);

  /* if config.ini exists, then use it, else download config.ini from the net */
  if(!FileExists(szFileIniTempDir))
  {
    if(FileExists(szFileIniSetupDir))
    {
      strcpy(szFileIniConfig, szFileIniSetupDir);
      hResult = 0;
    }
    else
    {
      char szEFileNotFound[MAX_BUF];

    if(WinQueryProfileString(hini, "Messages", "ERROR_FILE_NOT_FOUND", "", szEFileNotFound, sizeof(szEFileNotFound)))
      {
        sprintf(szBuf, szEFileNotFound, FILE_INI_CONFIG);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }
      hResult = 1;
    }
  }
  else
    hResult = 0;

  PrfCloseProfile(hini);
  return(hResult);
}

APIRET GetInstallIni()
{
  char    szFileIniTempDir[MAX_BUF];
  char    szFileIniSetupDir[MAX_BUF];
  char    szMsgRetrieveInstallIni[MAX_BUF];
  char    szBuf[MAX_BUF];
  APIRET hResult = 0;

  if(NS_LoadString(hSetupRscInst, IDS_MSG_RETRIEVE_INSTALLINI, szMsgRetrieveInstallIni, MAX_BUF) != WIZ_OK)
    return(1);
    
  strcpy(szFileIniTempDir, szTempDir);
  AppendBackSlash(szFileIniTempDir, sizeof(szFileIniTempDir));
  strcat(szFileIniTempDir, FILE_INI_INSTALL);

  /* set default value for szFileIniInstall here */
  strcpy(szFileIniInstall, szFileIniTempDir);

  strcpy(szFileIniSetupDir, szSetupDir);
  AppendBackSlash(szFileIniSetupDir, sizeof(szFileIniSetupDir));
  strcat(szFileIniSetupDir, FILE_INI_INSTALL);

  /* if installer.ini exists, then use it, else download installer.ini from the net */
  if(!FileExists(szFileIniTempDir))
  {
    if(FileExists(szFileIniSetupDir))
    {
      strcpy(szFileIniInstall, szFileIniSetupDir);
      hResult = 0;
    }
    else
    {
      char szEFileNotFound[MAX_BUF];

      if(NS_LoadString(hSetupRscInst, IDS_ERROR_FILE_NOT_FOUND, szEFileNotFound, MAX_BUF) == WIZ_OK)
      {
        sprintf(szBuf, szEFileNotFound, FILE_INI_INSTALL);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }
      hResult = 1;
    }
  }
  else
    hResult = 0;

  return(hResult);
}

int LocateJar(siC *siCObject, PSZ szPath, int dwPathSize, BOOL bIncludeTempDir)
{
  BOOL bRet;
  char szBuf[MAX_BUF * 2];
  char szSEADirTemp[MAX_BUF];
  char szSetupDirTemp[MAX_BUF];
  char szTempDirTemp[MAX_BUF];

  /* initialize default behavior */
  bRet = AP_NOT_FOUND;
  if(szPath != NULL)
    memset(szPath, 0, dwPathSize);
  siCObject->dwAttributes |= SIC_DOWNLOAD_REQUIRED;

  strcpy(szSEADirTemp, sgProduct.szAlternateArchiveSearchPath);
  AppendBackSlash(szSEADirTemp, sizeof(szSEADirTemp));
  strcat(szSEADirTemp, siCObject->szArchiveName);

  /* XXX_QUICK_FIX 
   * checking sgProduct.szAlternateArchiveSearchPath for empty string
   * should be done prior to AppendBackSlash() above.
   * This is a quick fix for the time frame that we are currently in. */
  if((*sgProduct.szAlternateArchiveSearchPath != '\0') && (FileExists(szSEADirTemp)))
  {
    /* jar file found.  Unset attribute to download from the net */
    siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
    /* save the path of where jar was found at */
    strcpy(siCObject->szArchivePath, sgProduct.szAlternateArchiveSearchPath);
    AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
    bRet = AP_ALTERNATE_PATH;

    /* save path where archive is located */
    if((szPath != NULL) && (strlen(sgProduct.szAlternateArchiveSearchPath) < dwPathSize))
      strcpy(szPath, sgProduct.szAlternateArchiveSearchPath);
  }
  else
  {
    strcpy(szSetupDirTemp, szSetupDir);
    AppendBackSlash(szSetupDirTemp, sizeof(szSetupDirTemp));

    strcpy(szTempDirTemp,  szTempDir);
    AppendBackSlash(szTempDirTemp, sizeof(szTempDirTemp));

    if(strcmpi(szTempDirTemp, szSetupDirTemp) == 0)
    {
      /* check the temp dir for the .xpi file */
      strcpy(szBuf, szTempDirTemp);
      AppendBackSlash(szBuf, sizeof(szBuf));
      strcat(szBuf, siCObject->szArchiveName);

      if(FileExists(szBuf))
      {
        if(bIncludeTempDir == TRUE)
        {
          /* jar file found.  Unset attribute to download from the net */
          siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
          /* save the path of where jar was found at */
          strcpy(siCObject->szArchivePath, szTempDirTemp);
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
        if((szPath != NULL) && (strlen(szTempDirTemp) < dwPathSize))
          strcpy(szPath, szTempDirTemp);
      }
    }
    else
    {
      /* check the setup dir for the .xpi file */
      strcpy(szBuf, szSetupDirTemp);
      AppendBackSlash(szBuf, sizeof(szBuf));
      strcat(szBuf, siCObject->szArchiveName);

      if(FileExists(szBuf))
      {
        /* jar file found.  Unset attribute to download from the net */
        siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
        /* save the path of where jar was found at */
        strcpy(siCObject->szArchivePath, szSetupDirTemp);
        AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
        bRet = AP_SETUP_PATH;

        /* save path where archive is located */
        if((szPath != NULL) && (strlen(sgProduct.szAlternateArchiveSearchPath) < dwPathSize))
          strcpy(szPath, szSetupDirTemp);
      }
      else
      {
        /* check the ns_temp dir for the .xpi file */
        strcpy(szBuf, szTempDirTemp);
        AppendBackSlash(szBuf, sizeof(szBuf));
        strcat(szBuf, siCObject->szArchiveName);

        if(FileExists(szBuf))
        {
          if(bIncludeTempDir == TRUE)
          {
            /* jar file found.  Unset attribute to download from the net */
            siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
            /* save the path of where jar was found at */
            strcpy(siCObject->szArchivePath, szTempDirTemp);
            AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
            bRet = AP_TEMP_PATH;
          }

          /* save path where archive is located */
          if((szPath != NULL) && (strlen(sgProduct.szAlternateArchiveSearchPath) < dwPathSize))
            strcpy(szPath, szTempDirTemp);
        }
      }
    }
  }
  return(bRet);
}

void SwapFTPAndHTTP(char *szInUrl, ULONG dwInUrlSize)
{
  char szTmpBuf[MAX_BUF];
  char *ptr       = NULL;
  char szFtp[]    = "ftp://";
  char szHttp[]   = "http://";

  if((!szInUrl) || !diDownloadOptions.bUseProtocolSettings)
    return;

  memset(szTmpBuf, 0, sizeof(szTmpBuf));
  switch(diDownloadOptions.dwUseProtocol)
  {
    case UP_HTTP:
      if((strncmp(szInUrl, szFtp, FTPSTR_LEN) == 0) &&
         ((int)dwInUrlSize > strlen(szInUrl) + 1))
      {
        ptr = szInUrl + FTPSTR_LEN;
        memmove(ptr + 1, ptr, strlen(ptr) + 1);
        memcpy(szInUrl, szHttp, HTTPSTR_LEN);
      }
      break;

    case UP_FTP:
    default:
      if((strncmp(szInUrl, szHttp, HTTPSTR_LEN) == 0) &&
         ((int)dwInUrlSize > strlen(szInUrl) + 1))
      {
        ptr = szInUrl + HTTPSTR_LEN;
        memmove(ptr - 1, ptr, strlen(ptr) + 1);
        memcpy(szInUrl, szFtp, FTPSTR_LEN);
      }
      break;
  }
}

int UpdateIdiFile(char  *szPartialUrl,
                  ULONG dwPartialUrlBufSize,
                  siC   *siCObject,
                  char  *szSection,
                  char  *szKey,
                  char  *szFileIdiGetArchives)
{
  char      szUrl[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      szBufTemp[MAX_BUF];
  HINI       hiniArchives;

  SwapFTPAndHTTP(szPartialUrl, dwPartialUrlBufSize);
  RemoveSlash(szPartialUrl);
  sprintf(szUrl, "%s/%s", szPartialUrl, siCObject->szArchiveName);
  hiniArchives = PrfOpenProfile((HAB)0, szFileIdiGetArchives); 
  if(PrfWriteProfileString(hiniArchives, szSection,
                               szKey,
                               szUrl) == 0)
  {
    char szEWPPS[MAX_BUF];
    
    HINI hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
    if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_WRITEPRIVATEPROFILESTRING", "", szEWPPS, sizeof(szEWPPS)))
    {
      sprintf(szBufTemp,
               "%s\n    [%s]\n    url=%s",
               szFileIdiGetArchives,
               szSection,
               szUrl);
      sprintf(szBuf, szEWPPS, szBufTemp);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    PrfCloseProfile(hiniInstall);
    PrfCloseProfile(hiniArchives);
    return(1);
  }
  PrfCloseProfile(hiniArchives);
  return(0);
}

APIRET AddArchiveToIdiFile(siC *siCObject,
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
  HINI      hiniConfig;

  HINI hiniArchives = PrfOpenProfile((HAB)0, szFileIdiGetArchives);
  PrfWriteProfileString(hiniArchives, szSection,
                            "desc",
                            siCObject->szDescriptionShort);
  _ui64toa(siCObject->ullInstallSizeArchive, szArchiveSize, 10);
  PrfWriteProfileString(hiniArchives, szSection,
                            "size",
                            szArchiveSize);
  itoa(siCObject->dwAttributes & SIC_IGNORE_DOWNLOAD_ERROR, szBuf, 10);
  PrfWriteProfileString(hiniArchives, szSection,
                            "Ignore File Network Error",
                            szBuf);

  strcpy(szFile, szTempDir);
  AppendBackSlash(szFile, sizeof(szFile));
  strcat(szFile, FILE_INI_REDIRECT);

  memset(szIdentifier, 0, sizeof(szIdentifier));
  ssiSiteSelectorTemp = SsiGetNode(szSiteSelectorDescription);

  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, "Redirect",
                          "Status",
                          "",
                          szBuf,
                          sizeof(szBuf));
  if(strcmpi(szBuf, "ENABLED") != 0)
  {
    /* redirect.ini is *not* enabled, so use the url from the
     * config.ini's [Site Selector] section */
    if(*ssiSiteSelectorTemp->szDomain != '\0')
    {
      sprintf(szKey, "url%d", iIndex);
      strcpy(szUrl, ssiSiteSelectorTemp->szDomain);
      UpdateIdiFile(szUrl, sizeof(szUrl), siCObject, szSection, szKey, szFileIdiGetArchives);
      ++iIndex;
    }

    /* use the url from the config.ini's [General] section as well */
    PrfQueryProfileString(hiniConfig, "General",
                            "url",
                            "",
                            szUrl,
                            sizeof(szUrl));
    if(*szUrl != 0)
    {
      sprintf(szKey, "url%d", iIndex);
      UpdateIdiFile(szUrl, sizeof(szUrl), siCObject, szSection, szKey, szFileIdiGetArchives);
    }
  }
  else if(FileExists(szFile))
  {
    /* redirect.ini is enabled *and* it exists */
    HINI hiniFile = PrfOpenProfile((HAB)0, szFile);
    GetPrivateProfileString(hiniFile, "Site Selector",
                            ssiSiteSelectorTemp->szIdentifier,
                            "",
                            szUrl,
                            sizeof(szUrl));
    PrfCloseProfile(hiniFile);
    if(*szUrl != '\0')
    {
      sprintf(szKey, "url%d", iIndex);
      UpdateIdiFile(szUrl, sizeof(szUrl), siCObject, szSection, szKey, szFileIdiGetArchives);
      ++iIndex;
    }

    /* use the url from the config.ini's [General] section as well */
    PrfQueryProfileString(hiniConfig, "General",
                            "url",
                            "",
                            szUrl,
                            sizeof(szUrl));
    if(*szUrl != 0)
    {
      sprintf(szKey, "url%d", iIndex);
      UpdateIdiFile(szUrl, sizeof(szUrl), siCObject, szSection, szKey, szFileIdiGetArchives);
    }
  }
  else
  {
    /* redirect.ini is enabled, but the file does not exist,
     * so fail over to the url from the config.ini's [General] section */
    PrfQueryProfileString(hiniConfig, "General",
                            "url",
                            "",
                            szUrl,
                            sizeof(szUrl));
    if(*szUrl != 0)
    {
      sprintf(szKey, "url%d", iIndex);
      UpdateIdiFile(szUrl, sizeof(szUrl), siCObject, szSection, szKey, szFileIdiGetArchives);
    }
  }

  PrfCloseProfile(hiniArchives);
  PrfCloseProfile(hiniConfig);
  return(0);
}

void SetSetupRunMode(PSZ szMode)
{
  if(strcmpi(szMode, "NORMAL") == 0)
    sgProduct.dwMode = NORMAL;
  if(strcmpi(szMode, "AUTO") == 0)
    sgProduct.dwMode = AUTO;
  if(strcmpi(szMode, "SILENT") == 0)
    sgProduct.dwMode = SILENT;
}

BOOL CheckForArchiveExtension(PSZ szFile)
{
  int  i;
  BOOL bRv = FALSE;
  char szExtension[MAX_BUF_TINY];

  /* if there is no extension in szFile, szExtension will be zero'ed out */
  ParsePath(szFile, szExtension, sizeof(szExtension), FALSE, PP_EXTENSION_ONLY);
  i = 0;
  while(*ArchiveExtensions[i] != '\0')
  {
    if(strcmpi(szExtension, ArchiveExtensions[i]) == 0)
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
  HINI      hiniConfig;
  HINI      hiniRedirect;

  if(GetTotalArchivesToDownload() == 0)
    return(0);

  strcpy(szFileIniRedirect, szTempDir);
  AppendBackSlash(szFileIniRedirect, sizeof(szFileIniRedirect));
  strcat(szFileIniRedirect, FILE_INI_REDIRECT);

  if(FileExists(szFileIniRedirect))
    DosDelete(szFileIniRedirect);

  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, "Redirect", "Status", "", szBuf, sizeof(szBuf));
  if(strcmpi(szBuf, "ENABLED") != 0)
  { PrfCloseProfile(hiniConfig);
    return(0);
  }

  ssiSiteSelectorTemp = SsiGetNode(szSiteSelectorDescription);
  if(ssiSiteSelectorTemp != NULL)
  {
    if(ssiSiteSelectorTemp->szDomain != NULL)
      strcpy(szBufUrl, ssiSiteSelectorTemp->szDomain);
  }
  else{
    /* No domain to download the redirect.ini file from.
     * Assume that it does not exist.
     * This should trigger the backup/alternate url. */
    PrfCloseProfile(hiniConfig);
    return(0);
  }

  strcpy(szFileIdiGetRedirect, szTempDir);
  AppendBackSlash(szFileIdiGetRedirect, sizeof(szFileIdiGetRedirect));
  strcat(szFileIdiGetRedirect, FILE_IDI_GETREDIRECT);

  PrfQueryProfileString(hiniConfig, "Redirect", "Description", "", szBuf, sizeof(szBuf));
  WritePrivateProfileString("File0", "desc", szBuf, szFileIdiGetRedirect);
  PrfQueryProfileString(hiniConfig, "Redirect", "Server Path", "", szBuf, sizeof(szBuf));
  AppendSlash(szBufUrl, sizeof(szBufUrl));
  strcat(szBufUrl, szBuf);
  SwapFTPAndHTTP(szBufUrl, sizeof(szBufUrl));
  hiniRedirect = PrfOpenProfile((HAB)0, szFileIdiGetRedirect);
  if(PrfWriteProfileString(hiniRedirect, "File0", "url", szBufUrl) == 0)
  {
    char szEWPPS[MAX_BUF];
    HINI hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
    if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_WRITEPRIVATEPROFILESTRING", "", szEWPPS, sizeof(szEWPPS)))
    {
      sprintf(szBufTemp, "%s\n    [%s]\n    %s=%s", szFileIdiGetRedirect, "File0", szIndex0, szBufUrl);
      sprintf(szBuf, szEWPPS, szBufTemp);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    PrfCloseProfile(hiniInstall);
    PrfCloseProfile(hiniRedirect);
    PrfCloseProfile(hiniConfig);
    return(1);
  }

  lResult = DownloadFiles(szFileIdiGetRedirect,               /* input idi file to parse                 */
                          szTempDir,                          /* download directory                      */
                          diAdvancedSettings.szProxyServer,   /* proxy server name                       */
                          diAdvancedSettings.szProxyPort,     /* proxy server port                       */
                          diAdvancedSettings.szProxyUser,     /* proxy server user (optional)            */
                          diAdvancedSettings.szProxyPasswd,   /* proxy password (optional)               */
                          FALSE,                              /* show retry message                      */
                          NULL,                               /* receive the number of network retries   */
                          TRUE,                               /* ignore network error                    */
                          NULL,                               /* buffer to store the name of failed file */
                          0);                                 /* size of failed file name buffer         */
  PrfCloseProfile(hiniRedirect);
  PrfCloseProfile(hiniConfig);
  return(lResult);
}

int CRCCheckDownloadedArchives(char *szCorruptedArchiveList,
                               ULONG dwCorruptedArchiveListSize,
                               char *szFileIdiGetArchives)
{
  ULONG dwIndex0;
  ULONG dwFileCounter;
  siC   *siCObject = NULL;
  char  szArchivePathWithFilename[MAX_BUF];
  char  szArchivePath[MAX_BUF];
  char  szMsgCRCCheck[MAX_BUF];
  char  szSection[MAX_INI_SK];
  int   iRv;
  int   iResult;
  HINI  hiniInstall;

  /* delete the getarchives.idi file because it needs to be recreated
   * if there are any files that fails the CRC check */
  if(szFileIdiGetArchives)
    DosDelete(szFileIdiGetArchives);

  if(szCorruptedArchiveList != NULL)
    memset(szCorruptedArchiveList, 0, dwCorruptedArchiveListSize);

  hiniInstall =  PrfOpenProfile((HAB)0, szFileIniInstall);
  PrfQueryProfileString(hiniInstall, "Strings", "Message Verifying Archives", "", szMsgCRCCheck, sizeof(szMsgCRCCheck));
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

       if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_FILE_NOT_FOUND", "", szEFileNotFound, sizeof(szEFileNotFound)))
        {
          sprintf(szBuf, szEFileNotFound, siCObject->szArchiveName);
          PrintError(szBuf, ERROR_CODE_HIDE);
        }
        iResult = WIZ_ARCHIVES_MISSING; // not all .xpi files were downloaded
        break;
      }

      if(strlen(szArchivePath) < sizeof(szArchivePathWithFilename))
        strcpy(szArchivePathWithFilename, szArchivePath);

      AppendBackSlash(szArchivePathWithFilename, sizeof(szArchivePathWithFilename));
      if((strlen(szArchivePathWithFilename) + strlen(siCObject->szArchiveName)) < sizeof(szArchivePathWithFilename))
        strcat(szArchivePathWithFilename, siCObject->szArchiveName);

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
            DosDelete(szArchivePathWithFilename);
            sprintf(szSection, "File%d", dwFileCounter);
            ++dwFileCounter;
            if(szFileIdiGetArchives)
              if((AddArchiveToIdiFile(siCObject,
                                      szSection,
                                      szFileIdiGetArchives)) != 0)
                return(WIZ_ERROR_UNDEFINED);

            ++siCObject->iCRCRetries;
            if(szCorruptedArchiveList != NULL)
            {
              if((ULONG)(strlen(szCorruptedArchiveList) + strlen(siCObject->szArchiveName + 1)) < dwCorruptedArchiveListSize)
              {
                strcat(szCorruptedArchiveList, "        ");
                strcat(szCorruptedArchiveList, siCObject->szArchiveName);
                strcat(szCorruptedArchiveList, "\n");
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
  PrfCloseProfile(hiniInstall);
  return(iResult);
}

long RetrieveArchives()
{
  ULONG     dwIndex0;
  ULONG     dwFileCounter;
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
  int       iNetRetries;
  int       iRv;
  HINI      hiniConfig;

  /* retrieve the redirect.ini file */
  RetrieveRedirectFile();

  memset(szCorruptedArchiveList, 0, sizeof(szCorruptedArchiveList));
  strcpy(szFileIdiGetArchives, szTempDir);
  AppendBackSlash(szFileIdiGetArchives, sizeof(szFileIdiGetArchives));
  strcat(szFileIdiGetArchives, FILE_IDI_GETARCHIVES);
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
         (strcmpi(szPartiallyDownloadedFilename,
                   siCObject->szArchiveName) == 0))
      {
        sprintf(szSection, "File%d", dwFileCounter);
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
  iNetRetries = 0;
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
                              &iNetRetries,                       /* receive the number of network retries   */
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
        int iComponentIndex = SiCNodeGetIndexDS(szFailedFile);
        siC *siCObject      = SiCNodeGetObject(iComponentIndex, TRUE, AC_ALL);

        ++siCObject->iNetRetries;

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
    hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
    PrfQueryProfileString(hiniConfig, "Strings", "Error Too Many CRC Failures", "", szMsg, sizeof(szMsg));
    PrfCloseProfile(hiniConfig);
    if(*szMsg != '\0')
      PrintError(szMsg, ERROR_CODE_HIDE);

    lResult = WIZ_CRC_FAIL;
  }
  else
  {
    if(bDownloadTriggered)
      LogISComponentsFailedCRC(NULL, W_DOWNLOAD);
  }

  if(lResult == WIZ_OK)
  {
    if(bDownloadTriggered)
      LogISDownloadStatus("ok", NULL);

    UnsetSetupCurrentDownloadFile();
    UnsetDownloadState();
  }
  else if(bDownloadTriggered)
  {
    sprintf(szBuf, "failed: %d", lResult);
    LogISDownloadStatus(szBuf, szFailedFile);
  }

  LogMSDownloadStatus(lResult);
  return(lResult);
}

void RemoveBackSlash(PSZ szInput)
{
  ULONG dwInputLen;
  BOOL  bDone;
  char  *ptrChar = NULL;

  if(szInput)
  {
    dwInputLen = strlen(szInput);
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

void AppendBackSlash(PSZ szInput, ULONG dwInputSize)
{
  ULONG dwInputLen = strlen(szInput);

  if(szInput)
  {
    if(*szInput == '\0')
    {
      if((dwInputLen + 1) < dwInputSize)
      {
        strcat(szInput, "\\");
      }
    }
    else if(*CharPrev(szInput, &szInput[dwInputLen]) != '\\')
    {
      if((dwInputLen + 1) < dwInputSize)
      {
        strcat(szInput, "\\");
      }
    }
  }
}

void RemoveSlash(PSZ szInput)
{
  ULONG dwInputLen;
  BOOL  bDone;
  char  *ptrChar = NULL;

  if(szInput)
  {
    dwInputLen = strlen(szInput);
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

void AppendSlash(PSZ szInput, ULONG dwInputSize)
{
  ULONG dwInputLen = strlen(szInput);

  if(szInput)
  {
    if(*szInput == '\0')
    {
      if((dwInputLen + 1) < dwInputSize)
      {
        strcat(szInput, "/");
      }
    }
    else if(*CharPrev(szInput, &szInput[dwInputLen]) != '/')
    {
      if((dwInputLen + 1) < dwInputSize)
      {
        strcat(szInput, "/");
      }
    }
  }
}

void ParsePath(PSZ szInput, PSZ szOutput, ULONG dwOutputSize, BOOL bURLPath, ULONG dwType)
{
  int   iFoundDelimiter;
  ULONG dwInputLen;
  ULONG dwOutputLen;
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
    dwInputLen    = strlen(szInput);
    memset(szOutput, 0, dwOutputSize);

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
              strcpy(szOutput, CharNext(ptrChar));
              bDone = TRUE;
            }
            else if(ptrChar == szInput)
            {
              /* we're at the beginning of the string and still
               * nothing found.  So just return the input string. */
              strcpy(szOutput, szInput);
              bDone = TRUE;
            }
          }
          break;

        case PP_PATH_ONLY:
          strcpy(szOutput, szInput);
          dwOutputLen = strlen(szOutput);
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
              strcpy(szOutput, CharNext(ptrChar));
              bDone = TRUE;
            }
            else if(ptrChar == szInput)
              bDone = TRUE;
          }
          break;

        case PP_ROOT_ONLY:
          strcpy(szOutput, szInput);
          dwOutputLen = strlen(szOutput);
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

APIRET LaunchApps()
{
  ULONG     dwIndex0;
  BOOL      bArchiveFound;
  siC       *siCObject = NULL;
  char      szArchive[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      szMsg[MAX_BUF];
  HINI       hiniInstall;

  LogISLaunchApps(W_START);
  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  if(!PrfQueryProfileString(hiniInstall, "Messages", "MSG_CONFIGURING", "", szMsg, sizeof(szMsg)))
  {
    PrfCloseProfile(hiniInstall);
    return(1);
  }

  dwIndex0 = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    /* launch 3rd party executable */
    if((siCObject->dwAttributes & SIC_SELECTED) && (siCObject->dwAttributes & SIC_LAUNCHAPP))
    {
      bArchiveFound = TRUE;
      strcpy(szArchive, sgProduct.szAlternateArchiveSearchPath);
      AppendBackSlash(szArchive, sizeof(szArchive));
      strcat(szArchive, siCObject->szArchiveName);
      if((*sgProduct.szAlternateArchiveSearchPath == '\0') || !FileExists(szArchive))
      {
        strcpy(szArchive, szSetupDir);
        AppendBackSlash(szArchive, sizeof(szArchive));
        strcat(szArchive, siCObject->szArchiveName);
        if(!FileExists(szArchive))
        {
          strcpy(szArchive, szTempDir);
          AppendBackSlash(szArchive, sizeof(szArchive));
          strcat(szArchive, siCObject->szArchiveName);
          if(!FileExists(szArchive))
          {
            bArchiveFound = FALSE;
          }
        }
      }

      if(bArchiveFound)
      {
        char szParameterBuf[MAX_BUF];

        LogISLaunchAppsComponent(siCObject->szDescriptionShort);
        sprintf(szBuf, szMsg, siCObject->szDescriptionShort);
        ShowMessage(szBuf, TRUE);
        DecryptString(szParameterBuf, siCObject->szParameter);
        WinSpawn(szArchive, szParameterBuf, szTempDir, WM_SHOW, TRUE);
        ShowMessage(szBuf, FALSE);
      }
    }
    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  LogISLaunchApps(W_END);
  PrfCloseProfile(hiniInstall);
  return(0);
}

char *GetOSTypeString(char *szOSType, ULONG dwOSTypeBufSize)
{
  memset(szOSType, 0, dwOSTypeBufSize);

  if(gSystemInfo.dwOSType & OS_WIN95_DEBUTE)
    strcpy(szOSType, "Win95 debute");
  else if(gSystemInfo.dwOSType & OS_WIN95)
    strcpy(szOSType, "Win95");
  else if(gSystemInfo.dwOSType & OS_WIN98)
    strcpy(szOSType, "Win98");
  else if(gSystemInfo.dwOSType & OS_NT3)
    strcpy(szOSType, "NT3");
  else if(gSystemInfo.dwOSType & OS_NT4)
    strcpy(szOSType, "NT4");
  else
    strcpy(szOSType, "NT5");

  return(szOSType);
}

void DetermineOSVersionEx()
{
  BOOL          bIsWin95Debute;
  char          szBuf[MAX_BUF];
  char          szOSType[MAX_BUF];
  char          szESetupRequirement[MAX_BUF];
  ULONG         dwStrLen;
  ULONG       aulSysInfo[QSV_MAX] = {0};
//  OSVERSIONINFO osVersionInfo;
//  MEMORYSTATUS  msMemoryInfo;
  HINI            hiniInstall;

  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  gSystemInfo.dwOSType = 0;
//  osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
//  if(!GetVersionEx(&osVersionInfo))
//  {
    /* GetVersionEx() failed for some reason.  It's not fatal, but could cause
     * some complications during installation */
//    char szEMsg[MAX_BUF_TINY];

//    if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_GETVERSION", "", szEMsg, sizeof(szEMsg)))
//      PrintError(szEMsg, ERROR_CODE_SHOW);
//  }

  bIsWin95Debute  = IsWin95Debute();
/*  switch(osVersionInfo.dwPlatformId)
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
          break;
      }
      break;

    default:
      if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_SETUP_REQUIREMENT", "", szESetupRequirement, sizeof(szESetupRequirement)))
        PrintError(szESetupRequirement, ERROR_CODE_HIDE);
      break;
  }
*/
//  gSystemInfo.dwMajorVersion = osVersionInfo.dwMajorVersion;
//  gSystemInfo.dwMinorVersion = osVersionInfo.dwMinorVersion;
//  gSystemInfo.dwBuildNumber  = (ULONG)(LOWORD(osVersionInfo.dwBuildNumber));

//  dwStrLen = sizeof(gSystemInfo.szExtraString) >
//                    strlen(osVersionInfo.szCSDVersion) ?
//                    strlen(osVersionInfo.szCSDVersion) :
//                    sizeof(gSystemInfo.szExtraString) - 1;
  memset(gSystemInfo.szExtraString, 0, sizeof(gSystemInfo.szExtraString));
//  strncpy(gSystemInfo.szExtraString, osVersionInfo.szCSDVersion, dwStrLen);

//  msMemoryInfo.dwLength = sizeof(MEMORYSTATUS);
//  GlobalMemoryStatus(&msMemoryInfo);
//  gSystemInfo.dwMemoryTotalPhysical = msMemoryInfo.dwTotalPhys/1024;
//  gSystemInfo.dwMemoryAvailablePhysical = msMemoryInfo.dwAvailPhys/1024;

  DosQuerySysInfo(1L, QSV_MAX, (PVOID)aulSysInfo, sizeof(ULONG)*QSV_MAX);

  GetOSTypeString(szOSType, sizeof(szOSType));
  sprintf(szBuf,
"    System Info:\n\
//        OS Type: %s\n\
        Major Version: %d\n\
        Minor Version: %d\n\
//        Build Number: %d\n\
//        Extra String: %s\n\
        Total Physical Memory: %dKB\n\
        Total Available Physical Memory: %dKB\n",
//           szOSType,
           aulSysInfo[QSV_VERSION_MAJOR-1],
           aulSysInfo[QSV_VERSION_MINOR-1],
//           gSystemInfo.dwBuildNumber,
//           gSystemInfo.szExtraString,
           aulSysInfo[QSV_TOTPHYSMEM-1]/1024,
           aulSysInfo[QSV_TOTAVAILMEM-1]/1024);

  UpdateInstallStatusLog(szBuf);
  PrfCloseProfile(hiniInstall);
}


APIRET WinSpawn(PSZ szClientName, PSZ szParameters, PSZ szCurrentDir, int iShowCmd, BOOL bWait)
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

APIRET InitDlgWelcome(diW *diDialog)
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

APIRET InitDlgLicense(diL *diDialog)
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

APIRET InitDlgSetupType(diST *diDialog)
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

APIRET InitDlgSelectComponents(diSC *diDialog, ULONG dwSM)
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

APIRET InitDlgWindowsIntegration(diWI *diDialog)
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

APIRET InitDlgProgramFolder(diPF *diDialog)
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

APIRET InitDlgDownloadOptions(diDO *diDialog)
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

APIRET InitDlgAdvancedSettings(diAS *diDialog)
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

APIRET InitDlgStartInstall(diSI *diDialog)
{
  diDialog->bShowDialog = FALSE;
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

APIRET InitDlgDownload(diD *diDialog)
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

ULONG InitDlgReboot(diR *diDialog)
{
  HINI hiniInstall;
  diDialog->dwShowDialog = FALSE;
  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  if(!PrfQueryProfileString(hiniInstall, "Messages", "DLG_REBOOT_TITLE", "", diDialog->szTitle, MAX_BUF))
  {
    PrfCloseProfile(hiniInstall);
    return(1);
  }
  PrfCloseProfile(hiniInstall);
  return(0);
}

void DeInitDlgReboot(diR *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
}

APIRET InitSetupGeneral()
{
  char szBuf[MAX_BUF];
  HINI hiniInstall;

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

  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  if(PrfQueryProfileString(hiniInstall, "Messages", "CB_DEFAULT", "", szBuf, sizeof(szBuf)))
    strcpy(szSiteSelectorDescription, szBuf);

  PrfCloseProfile(hiniInstall);
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

APIRET InitSDObject()
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

APIRET InitSXpcomFile()
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

APIRET SiCNodeGetAttributes(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag)
{
  ULONG dwCount = 0;
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

void SiCNodeSetAttributes(ULONG dwIndex, ULONG dwAttributes, BOOL bSet, BOOL bIncludeInvisible, ULONG dwACFlag)
{
  ULONG dwCount  = 0;
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

BOOL IsInList(ULONG dwCurrentItem, ULONG dwItems, ULONG *dwItemsSelected)
{
  ULONG i;

  for(i = 0; i < dwItems; i++)
  {
    if(dwItemsSelected[i] == dwCurrentItem)
      return(TRUE);
  }

  return(FALSE);
}

void SiCNodeSetItemsSelected(ULONG dwSetupType)
{
  siC  *siCNode;
  char szBuf[MAX_BUF];
  char szIndex0[MAX_BUF];
  char szSTSection[MAX_BUF];
  char szComponentKey[MAX_BUF];
  char szComponentSection[MAX_BUF];
  ULONG dwIndex0;
  HINI hiniConfig;

  strcpy(szSTSection, "Setup Type");
  itoa(dwSetupType, szBuf, 10);
  strcat(szSTSection, szBuf);

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
  strcpy(szComponentKey, "C");
  strcat(szComponentKey, szIndex0);
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSTSection, szComponentKey, "", szComponentSection, sizeof(szComponentSection));
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
        PrfQueryProfileString(hiniConfig, siCNode->szReferenceName, "Attributes", "", szBuf, sizeof(szBuf));
        siCNode->dwAttributes = ParseComponentAttributes(szBuf);
      }
    }

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    strcpy(szComponentKey, "C");
    strcat(szComponentKey, szIndex0);
    PrfQueryProfileString(hiniConfig, szSTSection, szComponentKey, "", szComponentSection, sizeof(szComponentSection));
  }
  PrfCloseProfile(hiniConfig);
}

char *SiCNodeGetReferenceName(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag)
{
  ULONG dwCount = 0;
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

char *SiCNodeGetDescriptionShort(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag)
{
  ULONG dwCount = 0;
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

char *SiCNodeGetDescriptionLong(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag)
{
  ULONG dwCount = 0;
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

ULONGLONG SiCNodeGetInstallSize(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag)
{
  ULONG dwCount   = 0;
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

ULONGLONG SiCNodeGetInstallSizeSystem(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag)
{
  ULONG dwCount   = 0;
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

ULONGLONG SiCNodeGetInstallSizeArchive(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag)
{
  ULONG dwCount   = 0;
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
  ULONG dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(strcmpi(szInDescriptionShort, siCTemp->szDescriptionShort) == 0)
      return(dwCount);

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(strcmpi(szInDescriptionShort, siCTemp->szDescriptionShort) == 0)
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
  ULONG dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(strcmpi(szInReferenceName, siCTemp->szReferenceName) == 0)
      return(dwCount);

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(strcmpi(szInReferenceName, siCTemp->szReferenceName) == 0)
        return(dwCount);
      
      ++dwCount;
      siCTemp = siCTemp->Next;
    }
  }
  return(-1);
}

siC *SiCNodeGetObject(ULONG dwIndex, BOOL bIncludeInvisibleObjs, ULONG dwACFlag)
{
  ULONG dwCount = -1;
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

ULONG GetAdditionalComponentsCount()
{
  ULONG dwCount  = 0;
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
  HMODULE hLib;
  BOOL      bIsWin95Debute;
  

  bIsWin95Debute = FALSE;
  
  if(DosLoadModule(NULL, 0, "kernell32.dll", &hLib) == NO_ERROR)
  {
    if(DosQueryProcAddr(hLib, 1L, "GetDiskFreeSpaceExA", &NS_GetDiskFreeSpaceEx) != NO_ERROR)
    {
      DosQueryProcAddr(hLib, 1L, "GetDiskFreeSpaceA", &NS_GetDiskFreeSpace);
      bIsWin95Debute = TRUE;
    }

    DosFreeModule(hLib);
  }
  return(bIsWin95Debute);
}

/* returns the value in kilobytes */
ULONGLONG GetDiskSpaceRequired(ULONG dwType)
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

int LocateExistingPath(char *szPath, char *szExistingPath, ULONG dwExistingPathSize)
{
  char szBuf[MAX_BUF];

  strcpy(szExistingPath, szPath);
  AppendBackSlash(szExistingPath, dwExistingPathSize);
  while((FileExists(szExistingPath) == FALSE))
  {
    RemoveBackSlash(szExistingPath);
    ParsePath(szExistingPath, szBuf, sizeof(szBuf), FALSE, PP_PATH_ONLY);
    strcpy(szExistingPath, szBuf);
    AppendBackSlash(szExistingPath, dwExistingPathSize);
  }
  return(WIZ_OK);
}

/* returns the value in bytes */
ULONGLONG GetDiskSpaceAvailable(PSZ szPath)
{
  char            szTempPath[MAX_BUF];
  char            szBuf[MAX_BUF];
  char            szExistingPath[MAX_BUF];
  char            szBuf2[MAX_BUF];
  ULARGE_INTEGER  uliFreeBytesAvailableToCaller;
  ULARGE_INTEGER  uliTotalNumberOfBytesToCaller;
  ULARGE_INTEGER  uliTotalNumberOfFreeBytes;
  ULONGLONG       ullReturn = 0;
  ULONG           dwSectorsPerCluster;
  ULONG           dwBytesPerSector;
  ULONG           dwNumberOfFreeClusters;
  ULONG           dwTotalNumberOfClusters;
  HINI               hiniInstall;

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
    if(NS_GetDiskFreeSpaceEx(szExistingPath,
                             &uliFreeBytesAvailableToCaller,
                             &uliTotalNumberOfBytesToCaller,
                             &uliTotalNumberOfFreeBytes) == FALSE)
    {
      char szEDeterminingDiskSpace[MAX_BUF];
      hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall); 
      if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_DETERMINING_DISK_SPACE", "", szEDeterminingDiskSpace, sizeof(szEDeterminingDiskSpace)))
      {
        strcpy(szBuf2, "\n    ");
        strcat(szBuf2, szPath);
        sprintf(szBuf, szEDeterminingDiskSpace, szBuf2);
        PrintError(szBuf, ERROR_CODE_SHOW);
      }
      PrfCloseProfile(hiniInstall);
    }
    ullReturn = uliFreeBytesAvailableToCaller.QuadPart;
  }

  if(ullReturn > 1024)
    ullReturn /= 1024;
  else
    ullReturn = 0;

  return(ullReturn);
}

APIRET ErrorMsgDiskSpace(ULONGLONG ullDSAvailable, ULONGLONG ullDSRequired, PSZ szPath, BOOL bCrutialMsg)
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
  ULONG     dwDlgType;
  HINI        hiniInstall;

  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  if(!PrfQueryProfileString(hiniInstall, "Messages", "DLG_DISK_SPACE_CHECK_TITLE", "", szDlgDiskSpaceCheckTitle, sizeof(szDlgDiskSpaceCheckTitle)))
  {
    PrfCloseProfile(hiniInstall);
    exit(1);
  }

  if(bCrutialMsg)
  {
    dwDlgType = MB_RETRYCANCEL;
    if(!PrfQueryProfileString(hiniInstall, "Messages", "DLG_DISK_SPACE_CHECK_CRUTIAL_MSG", "", szDlgDiskSpaceCheckMsg, sizeof(szDlgDiskSpaceCheckMsg)))
    {
      PrfCloseProfile(hiniInstall);
      exit(1);
    }
  }
  else
  {
    dwDlgType = MB_OK;
    if(!PrfQueryProfileString(hiniInstall, "Messages", "DLG_DISK_SPACE_CHECK_MSG", "", szDlgDiskSpaceCheckMsg, sizeof(szDlgDiskSpaceCheckMsg)))
    {
      PrfCloseProfile(hiniInstall);
      exit(1);
    }
  }
  PrfCloseProfile(hiniInstall);
  ParsePath(szPath, szBufRootPath, sizeof(szBufRootPath), FALSE, PP_ROOT_ONLY);
  RemoveBackSlash(szBufRootPath);
  strcpy(szBuf0, szPath);
  RemoveBackSlash(szBuf0);

  _ui64toa(ullDSAvailable, szDSAvailable, 10);
  _ui64toa(ullDSRequired, szDSRequired, 10);

  sprintf(szBuf1, "\n\n    %s\n\n    ", szBuf0);
  sprintf(szBuf2, "%s KB\n    ",        szDSRequired);
  sprintf(szBuf3, "%s KB\n\n",          szDSAvailable);
  sprintf(szBufMsg, szDlgDiskSpaceCheckMsg, szBufRootPath, szBuf1, szBuf2, szBuf3);

  if((sgProduct.dwMode != SILENT) && (sgProduct.dwMode != AUTO))
  {
    return(WinMessageBox(HWND_DESKTOP, hWndMain, szBufMsg, szDlgDiskSpaceCheckTitle, 0, dwDlgType | MB_ICONEXCLAMATION | MB_DEFBUTTON2 | MB_APPLMODAL));
  }
  else if(sgProduct.dwMode == AUTO)
  {
    ShowMessage(szBufMsg, TRUE);
    Delay(5);
    ShowMessage(szBufMsg, FALSE);
    exit(1);
  }

  return(MBID_CANCEL);
}

BOOL ContainsReparseTag(char *szPath, char *szReparsePath, ULONG dwReparsePathSize)
{
  BOOL  bFoundReparseTag  = FALSE;
  ULONG dwOriginalLen     = strlen(szPath);
  ULONG dwLen             = dwOriginalLen;
  char  *szPathTmp        = NULL;
  char  *szBuf            = NULL;

  if((szPathTmp = NS_GlobalAlloc(dwLen + 1)) == NULL)
    exit(1);
  if((szBuf = NS_GlobalAlloc(dwLen + 1)) == NULL)
    exit(1);

  strcpy(szPathTmp, szPath);
  while((szPathTmp[dwLen - 2] != ':') && (szPathTmp[dwLen - 2] != '\\'))
  {
    if(FileExists(szPathTmp) & FILE_ATTRIBUTE_REPARSE_POINT)
    {
      if((ULONG)strlen(szPathTmp) <= dwReparsePathSize)
        strcpy(szReparsePath, szPathTmp);
      else
        memset(szReparsePath, 0, dwReparsePathSize);

      bFoundReparseTag = TRUE;
      break;
    }

    strcpy(szBuf, szPathTmp);
    RemoveBackSlash(szBuf);
    ParsePath(szBuf, szPathTmp, dwOriginalLen + 1, FALSE, PP_PATH_ONLY);
    dwLen = strlen(szPathTmp);
  }

  FreeMemory(&szBuf);
  FreeMemory(&szPathTmp);
  return(bFoundReparseTag);
}

void UpdatePathDiskSpaceRequired(PSZ szPath, ULONGLONG ullSize, dsN **dsnComponentDSRequirement)
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
      strcpy(szVDSPath, szRootPath);
    else if((gSystemInfo.dwOSType & OS_NT5) &&
             ContainsReparseTag(szPath, szReparsePath, sizeof(szReparsePath)))
    {
      // check for Reparse Tag (mount points under NT5 only)
      if(*szReparsePath == '\0')
      {
        // szReparsePath is not big enough to hold the Reparse path.  This is a
        // non fatal error.  Keep going using szPath instead.
        strcpy(szVDSPath, szPath);
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
        strcpy(szVDSPath, szReparsePath);
      else
        strcpy(szVDSPath, szPath);
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
      strcpy(szVDSPath, szRootPath);
    else
      strcpy(szVDSPath, szPath);

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
      else if(strcmpi(dsnTemp->szVDSPath, szVDSPath) == 0)
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

APIRET InitComponentDiskSpaceInfo(dsN **dsnComponentDSRequirement)
{
  ULONG     dwIndex0;
  siC       *siCObject = NULL;
  APIRET   hResult    = 0;
  char      szBuf[MAX_BUF];
  char      szIndex0[MAX_BUF];
  char      szSysPath[MAX_BUF];
  char      szBufSysPath[MAX_BUF];
  char      szBufTempPath[MAX_BUF];

  if(GetSystemDirectory(szSysPath, MAX_BUF) == 0)
  {
    memset(szSysPath, 0, MAX_BUF);
    memset(szBufSysPath, 0, MAX_BUF);
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
        strcpy(szBuf, sgProduct.szPath);
      else
        strcpy(szBuf, siCObject->szDestinationPath);

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

APIRET VerifyDiskSpace()
{
  ULONGLONG ullDSAvailable;
  APIRET   hRetValue = FALSE;
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

APIRET ParseComponentAttributes(char *szAttribute)
{
  char  szBuf[MAX_BUF];
  ULONG dwAttributes = 0;

  strcpy(szBuf, szAttribute);
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

    if(strcmpi(siCNode->szReferenceName, szInReferenceName) == 0)
      return(siCNode);

    siCNode = siCNode->Next;
  } while((siCNode != NULL) && (siCNode != siCHeadNode));

  return(NULL);
}

BOOL ResolveForceUpgrade(siC *siCObject)
{
  ULONG dwIndex;
  char  szFilePath[MAX_BUF];
  char  szKey[MAX_BUF_TINY];
  char  szForceUpgradeFile[MAX_BUF];
  HINI    hiniInstall;
  HINI    hiniConfig;

  siCObject->bForceUpgrade = FALSE;
  if(siCObject->dwAttributes & SIC_FORCE_UPGRADE)
  {
    dwIndex = 0;
    BuildNumberedString(dwIndex, NULL, "Force Upgrade File", szKey, sizeof(szKey));
    hiniInstall = PrfOpenProfile((HAB)0, szFileIniConfig);
    PrfQueryProfileString(hiniInstall, siCObject->szReferenceName, szKey, "", szForceUpgradeFile, sizeof(szForceUpgradeFile));
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
      hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
      PrfQueryProfileString(hiniConfig, siCObject->szReferenceName, szKey, "", szForceUpgradeFile, sizeof(szForceUpgradeFile));
      PrfCloseProfile(hiniConfig);
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

  PrfCloseProfile(hiniInstall);
  }
  return(siCObject->bForceUpgrade);
}

void InitSiComponents(char *szFileIni)
{
  ULONG dwIndex0;
  ULONG dwIndex1;
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
  HINI    hiniIni;

  hiniIni = PrfOpenProfile((HAB)0, szFileIni);
  /* clean up the list before reading new components given the Setup Type */
  DeInitSiComponents(&siComponents);

  /* Parse the Setup Type sections in reverse order because
   * the Custom Setup Type is always last.  It needs to be parsed
   * first because the component list it has will be shown in its
   * other custom dialogs.  Order matters! */
  for(iCurrentLoop = 3; iCurrentLoop >= 0; iCurrentLoop--)
  {
    strcpy(szSTSection, "Setup Type");
    itoa(iCurrentLoop, szBuf, 10);
    strcat(szSTSection, szBuf);

    /* read in each component given a setup type */
    dwIndex0 = 0;
    itoa(dwIndex0, szIndex0, 10);
    strcpy(szComponentKey, "C");
    strcat(szComponentKey, szIndex0);
    PrfQueryProfileString(hiniIni, szSTSection, szComponentKey, "", szComponentSection, sizeof(szComponentSection));
    while(*szComponentSection != '\0')
    {
      PrfQueryProfileString(hiniIni, szComponentSection, "Archive", "", szBuf, sizeof(szBuf));
      if((*szBuf != '\0') && (SiCNodeFind(siComponents, szComponentSection) == NULL))
      {
        /* create and initialize empty node */
        siCTemp = CreateSiCNode();

        /* store name of archive for component */
        strcpy(siCTemp->szArchiveName, szBuf);
        
        /* get short description of component */
        PrfQueryProfileString(hiniIni, szComponentSection, "Description Short", "", szBuf, sizeof(szBuf));
        strcpy(siCTemp->szDescriptionShort, szBuf);

        /* get long description of component */
        PrfQueryProfileString(hiniIni, szComponentSection, "Description Long", "", szBuf, sizeof(szBuf));
        strcpy(siCTemp->szDescriptionLong, szBuf);

        /* get commandline parameter for component */
        PrfQueryProfileString(hiniIni, szComponentSection, "Parameter", "", siCTemp->szParameter, MAX_BUF);

        /* set reference name for component */
        strcpy(siCTemp->szReferenceName, szComponentSection);

        /* get install size required in destination for component.  Sould be in Kilobytes */
        PrfQueryProfileString(hiniIni, szComponentSection, "Install Size", "", szBuf, sizeof(szBuf));
        if(*szBuf != '\0')
          siCTemp->ullInstallSize = _atoi64(szBuf);
        else
          siCTemp->ullInstallSize = 0;

        /* get install size required in system for component.  Sould be in Kilobytes */
        PrfQueryProfileString(hiniIni, szComponentSection, "Install Size System", "", szBuf, sizeof(szBuf));
        if(*szBuf != '\0')
          siCTemp->ullInstallSizeSystem = _atoi64(szBuf);
        else
          siCTemp->ullInstallSizeSystem = 0;

        /* get install size required in temp for component.  Sould be in Kilobytes */
        PrfQueryProfileString(hiniIni, szComponentSection, "Install Size Archive", "", szBuf, sizeof(szBuf));
        if(*szBuf != '\0')
          siCTemp->ullInstallSizeArchive = _atoi64(szBuf);
        else
          siCTemp->ullInstallSizeArchive = 0;

        /* get attributes of component */
        PrfQueryProfileString(hiniIni, szComponentSection, "Attributes", "", szBuf, sizeof(szBuf));
        siCTemp->dwAttributes = ParseComponentAttributes(szBuf);

        /* get the random percentage value and select or deselect the component (by default) for
         * installation */
        PrfQueryProfileString(hiniIni, szComponentSection, "Random Install Percentage", "", szBuf, sizeof(szBuf));
        if(*szBuf != '\0')
        {
          siCTemp->lRandomInstallPercentage = atol(szBuf);
          if(siCTemp->lRandomInstallPercentage != 0)
            siCTemp->lRandomInstallValue = RandomSelect();
        }

        /* get all dependencies for this component */
        dwIndex1 = 0;
        itoa(dwIndex1, szIndex1, 10);
        strcpy(szDependency, "Dependency");
        strcat(szDependency, szIndex1);
        PrfQueryProfileString(hiniIni, szComponentSection, szDependency, "", szBuf, sizeof(szBuf));
        while(*szBuf != '\0')
        {
          /* create and initialize empty node */
          siCDepTemp = CreateSiCDepNode();

          /* store name of archive for component */
          strcpy(siCDepTemp->szReferenceName, szBuf);

          /* inserts the newly created component into the global component list */
          SiCDepNodeInsert(&(siCTemp->siCDDependencies), siCDepTemp);

          ProcessWindowsMessages();
          ++dwIndex1;
          itoa(dwIndex1, szIndex1, 10);
          strcpy(szDependency, "Dependency");
          strcat(szDependency, szIndex1);
          PrfQueryProfileString(hiniIni, szComponentSection, szDependency, "", szBuf, sizeof(szBuf));
        }

        /* get all dependees for this component */
        dwIndex1 = 0;
        itoa(dwIndex1, szIndex1, 10);
        strcpy(szDependee, "Dependee");
        strcat(szDependee, szIndex1);
        PrfQueryProfileString(hiniIni, szComponentSection, szDependee, "", szBuf, sizeof(szBuf));
        while(*szBuf != '\0')
        {
          /* create and initialize empty node */
          siCDDependeeTemp = CreateSiCDepNode();

          /* store name of archive for component */
          strcpy(siCDDependeeTemp->szReferenceName, szBuf);

          /* inserts the newly created component into the global component list */
          SiCDepNodeInsert(&(siCTemp->siCDDependees), siCDDependeeTemp);

          ProcessWindowsMessages();
          ++dwIndex1;
          itoa(dwIndex1, szIndex1, 10);
          strcpy(szDependee, "Dependee");
          strcat(szDependee, szIndex1);
          PrfQueryProfileString(hiniIni, szComponentSection, szDependee, "", szBuf, sizeof(szBuf));
        }

        // locate previous path if necessary
        strcpy(szDPSection, szComponentSection);
        strcat(szDPSection, "-Destination Path");
        if(LocatePreviousPath(szDPSection, siCTemp->szDestinationPath, CCHMAXPATH) == FALSE)
          memset(siCTemp->szDestinationPath, 0, CCHMAXPATH);

        /* inserts the newly created component into the global component list */
        SiCNodeInsert(&siComponents, siCTemp);
      }

      ProcessWindowsMessages();
      ++dwIndex0;
      itoa(dwIndex0, szIndex0, 10);
      strcpy(szComponentKey, "C");
      strcat(szComponentKey, szIndex0);
      PrfQueryProfileString(hiniIni, szSTSection, szComponentKey, "", szComponentSection, sizeof(szComponentSection));
    }
  }
  PrfCloseProfile(hiniIni);
  sgProduct.dwNumberOfComponents = dwIndex0;
}

void ResetComponentAttributes(char *szFileIni)
{
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szComponentItem[MAX_BUF];
  siC   *siCTemp;
  ULONG dwCounter;
  HINI   hiniIni;

  hiniIni = PrfOpenProfile((HAB)0, szFileIni);
  for(dwCounter = 0; dwCounter < sgProduct.dwNumberOfComponents; dwCounter++)
  {
    itoa(dwCounter, szIndex, 10);
    strcpy(szComponentItem, "Component");
    strcat(szComponentItem, szIndex);

    siCTemp = SiCNodeGetObject(dwCounter, TRUE, AC_ALL);
    PrfQueryProfileString(hiniIni, szComponentItem, "Attributes", "", szBuf, sizeof(szBuf));
    siCTemp->dwAttributes = ParseComponentAttributes(szBuf);
  }
  PrfCloseProfile(hiniIni);
}

void UpdateSiteSelector()
{
  ULONG dwIndex;
  char  szIndex[MAX_BUF];
  char  szKDescription[MAX_BUF];
  char  szDescription[MAX_BUF];
  char  szKDomain[MAX_BUF];
  char  szDomain[MAX_BUF];
  char  szFileIniRedirect[MAX_BUF];
  ssi   *ssiSiteSelectorTemp;
  HINI    hiniRedirect;

  strcpy(szFileIniRedirect, szTempDir);
  AppendBackSlash(szFileIniRedirect, sizeof(szFileIniRedirect));
  strcat(szFileIniRedirect, FILE_INI_REDIRECT);

  if(FileExists(szFileIniRedirect) == FALSE)
    return;

  /* get all dependees for this component */
  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  strcpy(szKDescription, "Description");
  strcpy(szKDomain,      "Domain");
  strcat(szKDescription, szIndex);
  strcat(szKDomain,      szIndex);
  hiniRedirect = PrfOpenProfile((HAB)0, szFileIniRedirect);
  PrfQueryProfileString(hiniRedirect, "Site Selector", szKDescription, "", szDescription, sizeof(szDescription));
  while(*szDescription != '\0')
  {
    if(strcmpi(szDescription, szSiteSelectorDescription) == 0)
    {
      PrfQueryProfileString(hiniRedirect, "Site Selector", szKDomain, "", szDomain, sizeof(szDomain));
      if(*szDomain != '\0')
      {
        ssiSiteSelectorTemp = SsiGetNode(szDescription);
        if(ssiSiteSelectorTemp != NULL)
        {
          strcpy(ssiSiteSelectorTemp->szDomain, szDomain);
        }
        else
        {
          /* no match found for the given domain description, so assume there's nothing
           * to change. just return. */
          PrfCloseProfile(hiniRedirect);
          return;
        }
      }
      else
      {
        /* found matched description, but domain was not set, so assume there's no
         * redirect required, just return. */
        PrfCloseProfile(hiniRedirect);
        return;
      }
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    strcpy(szKDescription, "Description");
    strcpy(szKDomain,      "Domain");
    strcat(szKDescription, szIndex);
    strcat(szKDomain,      szIndex);
    memset(szDescription, 0, sizeof(szDescription));
    memset(szDomain, 0, sizeof(szDomain));
    PrfQueryProfileString(hiniRedirect, "Site Selector", szKDescription, "", szDescription, sizeof(szDescription));
  }
  PrfCloseProfile(hiniRedirect);
}

void InitSiteSelector(char *szFileIni)
{
  ULONG dwIndex;
  char  szIndex[MAX_BUF];
  char  szKDescription[MAX_BUF];
  char  szDescription[MAX_BUF];
  char  szKDomain[MAX_BUF];
  char  szDomain[MAX_BUF];
  char  szKIdentifier[MAX_BUF];
  char  szIdentifier[MAX_BUF];
  ssi   *ssiSiteSelectorNewNode;
  HINI   hiniIni;

  ssiSiteSelector = NULL;

  /* get all dependees for this component */
  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  strcpy(szKDescription, "Description");
  strcpy(szKDomain,      "Domain");
  strcpy(szKIdentifier,  "Identifier");
  strcat(szKDescription, szIndex);
  strcat(szKDomain,      szIndex);
  strcat(szKIdentifier,  szIndex);
  hiniIni = PrfOpenProfile((HAB)0, szFileIni);
  PrfQueryProfileString(hiniIni, "Site Selector", szKDescription, "", szDescription, sizeof(szDescription));
  while(*szDescription != '\0')
  {
    /* if the Domain and Identifier are not set, then skip */
    PrfQueryProfileString(hiniIni, "Site Selector", szKDomain,     "", szDomain,     sizeof(szDomain));
    PrfQueryProfileString(hiniIni, "Site Selector", szKIdentifier, "", szIdentifier, sizeof(szIdentifier));
    if((*szDomain != '\0') && (*szIdentifier != '\0'))
    {
      /* create and initialize empty node */
      ssiSiteSelectorNewNode = CreateSsiSiteSelectorNode();

      strcpy(ssiSiteSelectorNewNode->szDescription, szDescription);
      strcpy(ssiSiteSelectorNewNode->szDomain,      szDomain);
      strcpy(ssiSiteSelectorNewNode->szIdentifier,  szIdentifier);

      /* inserts the newly created node into the global node list */
      SsiSiteSelectorNodeInsert(&(ssiSiteSelector), ssiSiteSelectorNewNode);
    }

    ProcessWindowsMessages();
    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    strcpy(szKDescription, "Description");
    strcpy(szKDomain,      "Domain");
    strcpy(szKIdentifier,  "Identifier");
    strcat(szKDescription, szIndex);
    strcat(szKDomain,      szIndex);
    strcat(szKIdentifier,  szIndex);
    memset(szDescription, 0, sizeof(szDescription));
    memset(szDomain, 0, sizeof(szDomain));
    memset(szIdentifier, 0, sizeof(szIdentifier));
    PrfQueryProfileString(hiniIni, "Site Selector", szKDescription, "", szDescription, sizeof(szDescription));
  }
  PrfCloseProfile(hiniIni);
}

void InitErrorMessageStream(char *szFileIni)
{
  char szBuf[MAX_BUF_TINY];
  HINI  hiniIni;

  hiniIni = PrfOpenProfile((HAB)0, szFileIni);
  PrfQueryProfileString(hiniIni, "Message Stream",
                          "Status",
                          "",
                          szBuf,
                          sizeof(szBuf));

  if(strcmpi(szBuf, "disabled") == 0)
    gErrorMessageStream.bEnabled = FALSE;
  else
    gErrorMessageStream.bEnabled = TRUE;

  PrfQueryProfileString(hiniIni, "Message Stream",
                          "url",
                          "",
                          gErrorMessageStream.szURL,
                          sizeof(gErrorMessageStream.szURL));

  PrfQueryProfileString(hiniIni, "Message Stream",
                          "Show Confirmation",
                          "",
                          szBuf,
                          sizeof(szBuf));
  if(strcmpi(szBuf, "FALSE") == 0)
    gErrorMessageStream.bShowConfirmation = FALSE;
  else
    gErrorMessageStream.bShowConfirmation = TRUE;

  PrfQueryProfileString(hiniIni, "Message Stream",
                          "Confirmation Message",
                          "",
                          gErrorMessageStream.szConfirmationMessage,
                          sizeof(szBuf));

  gErrorMessageStream.bSendMessage = FALSE;
  gErrorMessageStream.dwMessageBufSize = MAX_BUF;
  gErrorMessageStream.szMessage = NS_GlobalAlloc(gErrorMessageStream.dwMessageBufSize);
  PrfCloseProfile(hiniIni);
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
    ULONG dwIndex;

    strcpy(szIndentationPadding, szIndentation);
    strcat(szIndentationPadding, "    ");

    do
    {
      strcat(szBuffer, szIndentationPadding);
      strcat(szBuffer, siCDependencyTemp->szReferenceName);
      strcat(szBuffer, "::");

      if((dwIndex = SiCNodeGetIndexRN(siCDependencyTemp->szReferenceName)) != -1)
        strcat(szBuffer, SiCNodeGetDescriptionShort(dwIndex, TRUE, AC_ALL));
      else
        strcat(szBuffer, "Component does not exist");

      strcat(szBuffer, ":");
      strcat(szBuffer, "\n");

      if(dwIndex != -1)
      {
        if((siCNodeTemp = SiCNodeGetObject(dwIndex, TRUE, AC_ALL)) != NULL)
          ViewSiComponentsDependency(szBuffer, szIndentationPadding, siCNodeTemp);
        else
          strcat(szBuffer, "Node not found");
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
    ULONG dwIndex;

    strcpy(szIndentationPadding, szIndentation);
    strcat(szIndentationPadding, "    ");

    do
    {
      strcat(szBuffer, szIndentationPadding);
      strcat(szBuffer, siCDependeeTemp->szReferenceName);
      strcat(szBuffer, "::");

      if((dwIndex = SiCNodeGetIndexRN(siCDependeeTemp->szReferenceName)) != -1)
        strcat(szBuffer, SiCNodeGetDescriptionShort(dwIndex, TRUE, AC_ALL));
      else
        strcat(szBuffer, "Component does not exist");

      strcat(szBuffer, ":");
      strcat(szBuffer, "\n");

      if(dwIndex != -1)
      {
        if((siCNodeTemp = SiCNodeGetObject(dwIndex, TRUE, AC_ALL)) != NULL)
          ViewSiComponentsDependency(szBuffer, szIndentationPadding, siCNodeTemp);
        else
          strcat(szBuffer, "Node not found");
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
  memset(szBuf, 0, sizeof(szBuf));
  strcpy(szBuf, "Dependency:\n");

  do
  {
    if(siCTemp == NULL)
      break;

    strcat(szBuf, "    ");
    strcat(szBuf, siCTemp->szReferenceName);
    strcat(szBuf, "::");
    strcat(szBuf, siCTemp->szDescriptionShort);
    strcat(szBuf, ":\n");

    ViewSiComponentsDependency(szBuf, "    ", siCTemp);

    siCTemp = siCTemp->Next;
  } while((siCTemp != NULL) && (siCTemp != siComponents));

  WinMessageBox(HWND_DESKTOP, hWndMain, szBuf, NULL, 0, MB_ICONEXCLAMATION);

  // build dependee list
  memset(szBuf, 0, sizeof(szBuf));
  strcpy(szBuf, "Dependee:\n");

  do
  {
    if(siCTemp == NULL)
      break;

    strcat(szBuf, "    ");
    strcat(szBuf, siCTemp->szReferenceName);
    strcat(szBuf, "::");
    strcat(szBuf, siCTemp->szDescriptionShort);
    strcat(szBuf, ":\n");

    ViewSiComponentsDependee(szBuf, "    ", siCTemp);

    siCTemp = siCTemp->Next;
  } while((siCTemp != NULL) && (siCTemp != siComponents));

  WinMessageBox(HWND_DESKTOP, hWndMain, szBuf, NULL, 0, MB_ICONEXCLAMATION);
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
  ULONG   dwAttrib;

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

BOOL ResolveDependencies(ULONG dwIndex)
{
  BOOL  bMoreToResolve  = FALSE;
  ULONG dwCount         = 0;
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

ssi* SsiGetNode(PSZ szDescription)
{
  ssi *ssiSiteSelectorTemp = ssiSiteSelector;

  do
  {
    if(ssiSiteSelectorTemp == NULL)
      break;

    if(strcmpi(ssiSiteSelectorTemp->szDescription, szDescription) == 0)
      return(ssiSiteSelectorTemp);

    ssiSiteSelectorTemp = ssiSiteSelectorTemp->Next;
  } while((ssiSiteSelectorTemp != NULL) && (ssiSiteSelectorTemp != ssiSiteSelector));

  return(NULL);
}

void ResolveDependees(PSZ szToggledReferenceName)
{
  BOOL  bAtLeastOneSelected;
  BOOL  bMoreToResolve  = FALSE;
  siC   *siCTemp        = siComponents;
  ULONG dwIndex;
  ULONG dwAttrib;

  do
  {
    if(siCTemp == NULL)
      break;

    if((siCTemp->siCDDependees != NULL) &&
       (strcmpi(siCTemp->szReferenceName, szToggledReferenceName) != 0))
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
  HINI  hiniConfig;

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
    strcpy(szProcessFilename, sgProduct.szParentProcessFilename);
  else
  {
    DosQueryModuleName(NULL, sizeof(szBuf), szBuf);
    ParsePath(szBuf, szProcessFilename, sizeof(szProcessFilename), FALSE, PP_FILENAME_ONLY);
  }
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, "Strings", "UsageMsg Usage", "", szBuf, sizeof(szBuf));
  sprintf(szUsageMsg, szBuf, szProcessFilename, "\n", "\n", "\n", "\n", "\n", "\n", "\n", "\n", "\n");

  PrintError(szUsageMsg, ERROR_CODE_HIDE);
  PrfCloseProfile(hiniConfig);
}

ULONG ParseCommandLine(PSZ lpszCmdLine)
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
  sprintf(szOutputStr, "ArgC: %d\n", iArgC);
#endif

  i = 0;
  while(i < iArgC)
  {
    GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));

    if(!strcmpi(szArgVBuf, "-h") || !strcmpi(szArgVBuf, "/h"))
    {
      PrintUsage();
      return(WIZ_ERROR_UNDEFINED);
    }
    else if(!strcmpi(szArgVBuf, "-a") || !strcmpi(szArgVBuf, "/a"))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      strcpy(sgProduct.szAlternateArchiveSearchPath, szArgVBuf);
    }
    else if(!strcmpi(szArgVBuf, "-n") || !strcmpi(szArgVBuf, "/n"))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      strcpy(sgProduct.szParentProcessFilename, szArgVBuf);
    }
    else if(!strcmpi(szArgVBuf, "-ma") || !strcmpi(szArgVBuf, "/ma"))
      SetSetupRunMode("AUTO");
    else if(!strcmpi(szArgVBuf, "-ms") || !strcmpi(szArgVBuf, "/ms"))
      SetSetupRunMode("SILENT");
    else if(!strcmpi(szArgVBuf, "-ira") || !strcmpi(szArgVBuf, "/ira"))
      /* ignore [RunAppX] sections */
      gbIgnoreRunAppX = TRUE;
    else if(!strcmpi(szArgVBuf, "-ispf") || !strcmpi(szArgVBuf, "/ispf"))
      /* ignore [Program FolderX] sections */
      gbIgnoreProgramFolderX = TRUE;

#ifdef XXX_DEBUG
    itoa(i, szBuf, 10);
    strcat(szOutputStr, "    ");
    strcat(szOutputStr, szBuf);
    strcat(szOutputStr, ": ");
    strcat(szOutputStr, szArgVBuf);
    strcat(szOutputStr, "\n");
#endif

    ++i;
  }

#ifdef XXX_DEBUG
  WinMessageBox(HWND_DESKTOP, NULL, szOutputStr, "Output", 0, MB_OK);
#endif
  return(WIZ_OK);
}

void GetAlternateArchiveSearchPath(PSZ lpszCmdLine)
{
  char  szBuf[CCHMAXPATH];
  PSZ lpszAASPath;
  PSZ lpszEndPath;
  PSZ lpszEndQuote;

  if(strcpy(szBuf, lpszCmdLine))
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

    strcpy(sgProduct.szAlternateArchiveSearchPath, lpszAASPath);
  }
}

BOOL CheckProcessWin95(NS_CreateSnapshot pCreateToolhelp32Snapshot, NS_ProcessWalk pProcessWalkFirst, NS_ProcessWalk pProcessWalkNext, PSZ szProcessName)
{
  BOOL            bRv             = FALSE;
  LHANDLE          hCreateSnapshot = NULL;
  PROCESSENTRY32  peProcessEntry;
  
  hCreateSnapshot = pCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if(hCreateSnapshot == (LHANDLE)-1)
    return(bRv);

  peProcessEntry.dwSize = sizeof(PROCESSENTRY32);
  if(pProcessWalkFirst(hCreateSnapshot, &peProcessEntry))
  {
    do
    {
      char  szBuf[MAX_BUF];

      ParsePath(peProcessEntry.szExeFile, szBuf, sizeof(szBuf), FALSE, PP_FILENAME_ONLY);
      
      /* do process name string comparison here! */
      if(strcmpi(szBuf, szProcessName) == 0)
      {
        bRv = TRUE;
        break;
      }

    } while((bRv == FALSE) && pProcessWalkNext(hCreateSnapshot, &peProcessEntry));
  }

  CloseHandle(hCreateSnapshot);
  return(bRv);
}

BOOL CheckForProcess(PSZ szProcessName, ULONG dwProcessName)
{
  HMODULE            hKernel                   = NULLHANDLE;
  NS_CreateSnapshot pCreateToolhelp32Snapshot = NULL;
  NS_ProcessWalk    pProcessWalkFirst         = NULL;
  NS_ProcessWalk    pProcessWalkNext          = NULL;
  BOOL              bDoWin95Check             = TRUE;
    
  if(DosQueryModuleHandle("kernel32.dll", hKernel) != NO_ERROR)
    return(FALSE);

  pCreateToolhelp32Snapshot = (NS_CreateSnapshot)GetProcAddress(hKernel, "CreateToolhelp32Snapshot");
  pProcessWalkFirst         = (NS_ProcessWalk)GetProcAddress(hKernel,    "Process32First");
  pProcessWalkNext          = (NS_ProcessWalk)GetProcAddress(hKernel,    "Process32Next");

  if(pCreateToolhelp32Snapshot && pProcessWalkFirst && pProcessWalkNext)
    return(CheckProcessWin95(pCreateToolhelp32Snapshot, pProcessWalkFirst, pProcessWalkNext, szProcessName));
  else
    return(CheckProcessNT4(szProcessName, dwProcessName));
}

APIRET CheckInstances()
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
  PSZ szWN;
  PSZ szCN;
  ULONG dwRv0;
  ULONG dwRv1;
  HINI  hiniConfig;

  bContinue = TRUE;
  iIndex    = -1;
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  while(bContinue)
  {
    memset(szClassName, 0, sizeof(szClassName));
    memset(szWindowName, 0, sizeof(szWindowName));
    memset(szMessage, 0, sizeof(szMessage));

    ++iIndex;
    itoa(iIndex, szIndex, 10);
    strcpy(szSection, "Check Instance");
    strcat(szSection, szIndex);

    PrfQueryProfileString(hiniConfig, szSection, "Message", "", szMessage, sizeof(szMessage));
    if(PrfQueryProfileString(hiniConfig, szSection, "Process Name", "", szProcessName, sizeof(szProcessName)) != 0L)
    {
      if(*szProcessName != '\0')
      {
        if(CheckForProcess(szProcessName, sizeof(szProcessName)) == TRUE)
        {
          if(*szMessage != '\0')
          {
            switch(sgProduct.dwMode)
            {
              case NORMAL:
                switch(WinMessageBox(HWND_DESKTOP, hWndMain, szMessage, NULL, 0, MB_ICONEXCLAMATION | MB_RETRYCANCEL))
                {
                  case MBID_CANCEL:
                    /* User selected to cancel Setup */
                    PrfCloseProfile(hiniConfig);
                    return(TRUE);

                  case MBID_RETRY:
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
                PrfCloseProfile(hiniConfig);
                return(TRUE);

              case SILENT:
                PrfCloseProfile(hiniConfig);
                return(TRUE);
            }
          }
          else
          {
            /* No message to display.  Assume cancel because we can't allow user to continue */
            PrfCloseProfile(hiniConfig);
            return(TRUE);
          }
        }
      }

      /* Process Name= key existed, and has been processed, so continue looking for more */
      continue;
    }

    /* Process Name= key did not exist, so look for other keys */
    dwRv0 = PrfQueryProfileString(hiniConfig, szSection, "Class Name",  "", szClassName,  sizeof(szClassName));
    dwRv1 = PrfQueryProfileString(hiniConfig, szSection, "Window Name", "", szWindowName, sizeof(szWindowName));
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
          switch(sgProduct.dwMode)
          {
            case NORMAL:
              switch(WinMessageBox(HWND_DESKTOP, hWndMain, szMessage, NULL, 0, MB_ICONEXCLAMATION | MB_RETRYCANCEL))
              {
                case MBID_CANCEL:
                  /* User selected to cancel Setup */
                  PrfCloseProfile(hiniConfig);
                  return(TRUE);

                case MBID_RETRY:
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
              PrfCloseProfile(hiniConfig);
              return(TRUE);

            case SILENT:
              PrfCloseProfile(hiniConfig);
              return(TRUE);
          }
        }
        else
        {
          /* No message to display.  Assume cancel because we can't allow user to continue */
          PrfCloseProfile(hiniConfig);
          return(TRUE);
        }
      }
    }
  }
  PrfCloseProfile(hiniConfig);
  return(FALSE);
}

BOOL GetFileVersion(PSZ szFile, verBlock *vbVersion)
{
  UINT              uLen;
  UINT              dwLen;
  BOOL              bRv;
  ULONG             dwHandle;
  PVOID            lpData;
  PVOID            lpBuffer;
//  VS_FIXEDFILEINFO  *lpBuffer2;

//  vbVersion->ullMajor   = 0;
//  vbVersion->ullMinor   = 0;
//  vbVersion->ullRelease = 0;
//  vbVersion->ullBuild   = 0;
  if(FileExists(szFile))
  {
    bRv    = TRUE;
//    dwLen  = GetFileVersionInfoSize(szFile, &dwHandle);
//    lpData = (PVOID)malloc(sizeof(long)*dwLen);
//    uLen   = 0;

//    if(GetFileVersionInfo(szFile, dwHandle, dwLen, lpData) != 0)
//    {
//      if(VerQueryValue(lpData, "\\", &lpBuffer, &uLen) != 0)
//      {
//        lpBuffer2             = (VS_FIXEDFILEINFO *)lpBuffer;
//        vbVersion->ullMajor   = HIWORD(lpBuffer2->dwFileVersionMS);
//        vbVersion->ullMinor   = LOWORD(lpBuffer2->dwFileVersionMS);
//        vbVersion->ullRelease = HIWORD(lpBuffer2->dwFileVersionLS);
//        vbVersion->ullBuild   = LOWORD(lpBuffer2->dwFileVersionLS);
//      }
//    }
//    free(lpData);
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

int CRCCheckArchivesStartup(char *szCorruptedArchiveList, ULONG dwCorruptedArchiveListSize, BOOL bIncludeTempPath)
{
  ULONG dwIndex0;
  ULONG dwFileCounter;
  siC   *siCObject = NULL;
  char  szArchivePathWithFilename[MAX_BUF];
  char  szArchivePath[MAX_BUF];
  char  szMsgCRCCheck[MAX_BUF];
  char  szPartiallyDownloadedFilename[MAX_BUF];
  int   iRv;
  int   iResult;
  HINI  hiniConfig;

  if(szCorruptedArchiveList != NULL)
    memset(szCorruptedArchiveList, 0, dwCorruptedArchiveListSize);

  GetSetupCurrentDownloadFile(szPartiallyDownloadedFilename,
                              sizeof(szPartiallyDownloadedFilename));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, "Strings", "Message Verifying Archives", "", szMsgCRCCheck, sizeof(szMsgCRCCheck));
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
       (strcmpi(szPartiallyDownloadedFilename,
                 siCObject->szArchiveName) != 0))
    {
      if(strlen(szArchivePath) < sizeof(szArchivePathWithFilename))
        strcpy(szArchivePathWithFilename, szArchivePath);

      AppendBackSlash(szArchivePathWithFilename, sizeof(szArchivePathWithFilename));
      if((strlen(szArchivePathWithFilename) + strlen(siCObject->szArchiveName)) < sizeof(szArchivePathWithFilename))
        strcat(szArchivePathWithFilename, siCObject->szArchiveName);

      if(CheckForArchiveExtension(szArchivePathWithFilename))
      {
        /* Make sure that the Archive that failed is located in the TEMP
         * folder.  This means that it was downloaded at one point and not
         * simply uncompressed from the self-extracting exe file. */
        if(VerifyArchive(szArchivePathWithFilename) != ZIP_OK)
        {
          if(iRv == AP_TEMP_PATH)
            DosDelete(szArchivePathWithFilename);
          else if(szCorruptedArchiveList != NULL)
          {
            iResult = WIZ_CRC_FAIL;
            if((ULONG)(strlen(szCorruptedArchiveList) + strlen(siCObject->szArchiveName + 1)) < dwCorruptedArchiveListSize)
            {
              strcat(szCorruptedArchiveList, "        ");
              strcat(szCorruptedArchiveList, siCObject->szArchiveName);
              strcat(szCorruptedArchiveList, "\n");
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
  PrfCloseProfile(hiniConfig);
  return(iResult);
}

int StartupCheckArchives(void)
{
  int  iRv;
  char szBuf[MAX_BUF_SMALL];
  char szCorruptedArchiveList[MAX_BUF];
  HINI  hiniInstall;
  HINI  hiniConfig;

  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
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
          if(PrfQueryProfileString(hiniInstall, "Messages", "STR_MESSAGEBOX_TITLE", "", szBuf, sizeof(szBuf)))
            strcpy(szTitle, "Setup");
          else
            sprintf(szTitle, szBuf, sgProduct.szProductName);

          PrfQueryProfileString(hiniConfig, "Strings", "Error Corrupted Archives Detected", "", szBuf, sizeof(szBuf));
          if(*szBuf != '\0')
          {
            strcpy(szBuf2, "\n\n");
            strcat(szBuf2, szCorruptedArchiveList);
            strcat(szBuf2, "\n");
            sprintf(szMsg, szBuf, szBuf2);
          }
          WinMessageBox(HWND_DESKTOP, hWndMain, szMsg, szTitle, 0, MB_OK | MB_ICONHAND);
          break;

        case AUTO:
          GetPrivateProfileString("Strings", "Error Corrupted Archives Detected AUTO mode", "", szBuf, sizeof(szBuf), szFileIniConfig);
          ShowMessage(szBuf, TRUE);
          Delay(5);
          ShowMessage(szBuf, FALSE);
          break;
      }

      LogISComponentsFailedCRC(szCorruptedArchiveList, W_STARTUP);
      PrfCloseProfile(hiniInstall);
      PrfCloseProfile(hiniConfig);
      return(WIZ_CRC_FAIL);

    case WIZ_CRC_PASS:
      break;

    default:
      break;
  }
  LogISComponentsFailedCRC(NULL, W_STARTUP);
  PrfCloseProfile(hiniInstall);
  PrfCloseProfile(hiniConfig);
  return(iRv);
}

APIRET ParseConfigIni(PSZ lpszCmdLine)
{
  int  iRv;
  char szBuf[MAX_BUF];
  char szMsgInitSetup[MAX_BUF];
  char szPreviousPath[MAX_BUF];
  char szShowDialog[MAX_BUF];
  HINI  hiniInstall;
  HINI  hiniConfig;

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
  if(InitDlgStartInstall(&diStartInstall))
    return(1);
  if(InitDlgDownload(&diDownload))
    return(1);
  if(InitSDObject())
    return(1);
  if(InitSXpcomFile())
    return(1);
 
  /* get install Mode information */
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, "General", "Run Mode", "", szBuf, sizeof(szBuf));
  SetSetupRunMode(szBuf);
  if(ParseCommandLine(lpszCmdLine))
  {
    PrfCloseProfile(hiniConfig);
    return(1);
  }

  if(CheckInstances())
  {
    PrfCloseProfile(hiniConfig);
    return(1);
  }

  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  if(PrfQueryProfileString(hiniInstall, "Messages", "MSG_INIT_SETUP", "", szMsgInitSetup, sizeof(szMsgInitSetup)))
    ShowMessage(szMsgInitSetup, TRUE);

  /* get product name description */
  PrfQueryProfileString(hiniConfig, "General", "Company Name", "", sgProduct.szCompanyName, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "General", "Product Name", "", sgProduct.szProductName, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "General", "Uninstall Filename", "", sgProduct.szUninstallFilename, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "General", "User Agent",   "", sgProduct.szUserAgent,   MAX_BUF);
  PrfQueryProfileString(hiniConfig, "General", "Sub Path",     "", sgProduct.szSubPath,     MAX_BUF);
  PrfQueryProfileString(hiniConfig, "General", "Program Name", "", sgProduct.szProgramName, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "General", "Lock Path",    "", szBuf,                   sizeof(szBuf));
  if(strcmpi(szBuf, "TRUE") == 0)
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
        if(!PrfQueryProfileString(hiniInstall, "Messages", "MB_WARNING_STR", "", szBuf, sizeof(szBuf)))
          strcpy(szTitle, "Setup");
        else
          sprintf(szTitle, szBuf, sgProduct.szProductName);

        PrfQueryProfileString(hiniConfig, "Strings", "Message NORMAL Restricted Access", "", szBuf, sizeof(szBuf));
        iRvMB = WinMessageBox(HWND_DESKTOP, hWndMain, szBuf, szTitle, 0, MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2);
        break;

      case AUTO:
        ShowMessage(szMsgInitSetup, FALSE);
        PrfQueryProfileString(hiniConfig, "Strings", "Message AUTO Restricted Access", "", szBuf, sizeof(szBuf));
        ShowMessage(szBuf, TRUE);
        Delay(5);
        ShowMessage(szBuf, FALSE);
        iRvMB = MBID_NO;
        break;

      default:
        iRvMB = MBID_NO;
        break;
    }

    if(iRvMB == MBID_NO)
    {
      /* User chose not to continue with the lack of
       * appropriate access privileges */
      PostQuitMessage(1);
      PrfCloseProfile(hiniInstall);
      PrfCloseProfile(hiniConfig);
      return(1);
    }
  }

  /* get main install path */
  if(LocatePreviousPath("Locate Previous Product Path", szPreviousPath, sizeof(szPreviousPath)) == FALSE)
  {
    PrfQueryProfileString(hiniConfig, "General", "Path", "", szBuf, sizeof(szBuf));
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
      strcpy(szBuf, szPreviousPath);
      AppendBackSlash(szBuf, sizeof(szBuf));
      strcat(szBuf, sgProduct.szSubPath);
      AppendBackSlash(szBuf, sizeof(szBuf));
      strcat(szBuf, sgProduct.szProgramName);

      /* Check to see if PreviousPath + SubPath + ProgramName exists.  If it does, then we have the
       * correct path.
       */
      if(FileExists(szBuf))
      {
        strcpy(sgProduct.szPath, szPreviousPath);
      }
      else
      {
        /* If not, try parent of PreviousPath + SubPath + ProgramName.
         * If this exists, then we need to use the parent path of PreviousPath.
         */
        RemoveBackSlash(szPreviousPath);
        ParsePath(szPreviousPath, szBuf, sizeof(szBuf), FALSE, PP_PATH_ONLY);
        AppendBackSlash(szBuf, sizeof(szBuf));
        strcat(szBuf, sgProduct.szSubPath);
        AppendBackSlash(szBuf, sizeof(szBuf));
        strcat(szBuf, sgProduct.szProgramName);

        if(FileExists(szBuf))
        {
          RemoveBackSlash(szPreviousPath);
          ParsePath(szPreviousPath, szBuf, sizeof(szBuf), FALSE, PP_PATH_ONLY);
          strcpy(sgProduct.szPath, szBuf);
        }
        else
        {
          /* If we still can't locate ProgramName file, then use the default in the config.ini */
          PrfQueryProfileString(hiniConfig, "General", "Path", "", szBuf, sizeof(szBuf));
          DecryptString(sgProduct.szPath, szBuf);
        }
      }
    }
    else
    {
      strcpy(sgProduct.szPath, szPreviousPath);
    }
  }
  RemoveBackSlash(sgProduct.szPath);

  /* make a copy of sgProduct.szPath to be used in the Setup Type dialog */
  strcpy(szTempSetupPath, sgProduct.szPath);
  
  /* get main program folder path */
  PrfQueryProfileString(hiniConfig, "General", "Program Folder Path", "", szBuf, sizeof(szBuf));
  DecryptString(sgProduct.szProgramFolderPath, szBuf);
  
  /* get main program folder name */
  PrfQueryProfileString(hiniConfig, "General", "Program Folder Name", "", szBuf, sizeof(szBuf));
  DecryptString(sgProduct.szProgramFolderName, szBuf);

  /* Welcome dialog */
  PrfQueryProfileString(hiniConfig, "Dialog Welcome",             "Show Dialog",     "", szShowDialog,                  sizeof(szShowDialog));
  PrfQueryProfileString(hiniConfig, "Dialog Welcome",             "Title",           "", diWelcome.szTitle,             MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Welcome",             "Message0",        "", diWelcome.szMessage0,          MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Welcome",             "Message1",        "", diWelcome.szMessage1,          MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Welcome",             "Message2",        "", diWelcome.szMessage2,          MAX_BUF);
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diWelcome.bShowDialog = TRUE;

  /* License dialog */
  PrfQueryProfileString(hiniConfig, "Dialog License",             "Show Dialog",     "", szShowDialog,                  sizeof(szShowDialog));
  PrfQueryProfileString(hiniConfig, "Dialog License",             "Title",           "", diLicense.szTitle,             MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog License",             "License File",    "", diLicense.szLicenseFilename,   MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog License",             "Message0",        "", diLicense.szMessage0,          MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog License",             "Message1",        "", diLicense.szMessage1,          MAX_BUF);
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diLicense.bShowDialog = TRUE;

  /* Setup Type dialog */
  PrfQueryProfileString(hiniConfig, "Dialog Setup Type",          "Show Dialog",     "", szShowDialog,                  sizeof(szShowDialog));
  PrfQueryProfileString(hiniConfig, "Dialog Setup Type",          "Title",           "", diSetupType.szTitle,           MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Setup Type",          "Message0",        "", diSetupType.szMessage0,        MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Setup Type",          "Readme Filename", "", diSetupType.szReadmeFilename,  MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Setup Type",          "Readme App",      "", diSetupType.szReadmeApp,       MAX_BUF);
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diSetupType.bShowDialog = TRUE;

  /* Get Setup Types info */
  PrfQueryProfileString(hiniConfig, "Setup Type0", "Description Short", "", diSetupType.stSetupType0.szDescriptionShort, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Setup Type0", "Description Long",  "", diSetupType.stSetupType0.szDescriptionLong,  MAX_BUF);
  STSetVisibility(&diSetupType.stSetupType0);

  PrfQueryProfileString(hiniConfig, "Setup Type1", "Description Short", "", diSetupType.stSetupType1.szDescriptionShort, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Setup Type1", "Description Long",  "", diSetupType.stSetupType1.szDescriptionLong,  MAX_BUF);
  STSetVisibility(&diSetupType.stSetupType1);

  PrfQueryProfileString(hiniConfig, "Setup Type2", "Description Short", "", diSetupType.stSetupType2.szDescriptionShort, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Setup Type2", "Description Long",  "", diSetupType.stSetupType2.szDescriptionLong,  MAX_BUF);
  STSetVisibility(&diSetupType.stSetupType2);

  PrfQueryProfileString(hiniConfig, "Setup Type3", "Description Short", "", diSetupType.stSetupType3.szDescriptionShort, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Setup Type3", "Description Long",  "", diSetupType.stSetupType3.szDescriptionLong,  MAX_BUF);
  STSetVisibility(&diSetupType.stSetupType3);

  /* remember the radio button that is considered the Custom type (the last radio button) */
  SetCustomType();

  /* Select Components dialog */
  PrfQueryProfileString(hiniConfig, "Dialog Select Components",   "Show Dialog",  "", szShowDialog,                    sizeof(szShowDialog));
  PrfQueryProfileString(hiniConfig, "Dialog Select Components",   "Title",        "", diSelectComponents.szTitle,      MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Select Components",   "Message0",     "", diSelectComponents.szMessage0,   MAX_BUF);
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diSelectComponents.bShowDialog = TRUE;

  /* Select Additional Components dialog */
  PrfQueryProfileString(hiniConfig, "Dialog Select Additional Components",   "Show Dialog",  "", szShowDialog,                              sizeof(szShowDialog));
  PrfQueryProfileString(hiniConfig, "Dialog Select Additional Components",   "Title",        "", diSelectAdditionalComponents.szTitle,      MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Select Additional Components",   "Message0",     "", diSelectAdditionalComponents.szMessage0,   MAX_BUF);
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diSelectAdditionalComponents.bShowDialog = TRUE;

  /* Windows Integration dialog */
  PrfQueryProfileString(hiniConfig, "Dialog Windows Integration", "Show Dialog",  "", szShowDialog,                    sizeof(szShowDialog));
  PrfQueryProfileString(hiniConfig, "Dialog Windows Integration", "Title",        "", diWindowsIntegration.szTitle,    MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Windows Integration", "Message0",     "", diWindowsIntegration.szMessage0, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Windows Integration", "Message1",     "", diWindowsIntegration.szMessage1, MAX_BUF);
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diWindowsIntegration.bShowDialog = TRUE;

  /* Program Folder dialog */
  PrfQueryProfileString(hiniConfig, "Dialog Program Folder",      "Show Dialog",  "", szShowDialog,                    sizeof(szShowDialog));
  PrfQueryProfileString(hiniConfig, "Dialog Program Folder",      "Title",        "", diProgramFolder.szTitle,         MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Program Folder",      "Message0",     "", diProgramFolder.szMessage0,      MAX_BUF);
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diProgramFolder.bShowDialog = TRUE;

  /* Download Options dialog */
  PrfQueryProfileString(hiniConfig, "Dialog Download Options",       "Show Dialog",    "", szShowDialog,                     sizeof(szShowDialog));
  PrfQueryProfileString(hiniConfig, "Dialog Download Options",       "Title",          "", diDownloadOptions.szTitle,        MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Download Options",       "Message0",       "", diDownloadOptions.szMessage0,     MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Download Options",       "Message1",       "", diDownloadOptions.szMessage1,     MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Download Options",       "Save Installer", "", szBuf,                            sizeof(szBuf));

  if(strcmpi(szBuf, "TRUE") == 0)
    diDownloadOptions.bSaveInstaller = TRUE;

  if(strcmpi(szShowDialog, "TRUE") == 0)
    diDownloadOptions.bShowDialog = TRUE;

  /* Advanced Settings dialog */
  PrfQueryProfileString(hiniConfig, "Dialog Advanced Settings",       "Show Dialog",    "", szShowDialog,                     sizeof(szShowDialog));
  PrfQueryProfileString(hiniConfig, "Dialog Advanced Settings",       "Title",          "", diAdvancedSettings.szTitle,       MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Advanced Settings",       "Message0",       "", diAdvancedSettings.szMessage0,    MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Advanced Settings",       "Proxy Server",   "", diAdvancedSettings.szProxyServer, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Advanced Settings",       "Proxy Port",     "", diAdvancedSettings.szProxyPort,   MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Advanced Settings",       "Proxy User",     "", diAdvancedSettings.szProxyUser,   MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Advanced Settings",       "Proxy Password", "", diAdvancedSettings.szProxyPasswd, MAX_BUF);
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diAdvancedSettings.bShowDialog = TRUE;

  PrfQueryProfileString(hiniConfig, "Dialog Advanced Settings",       "Use Protocol",   "", szBuf,                            sizeof(szBuf));
  if(strcmpi(szBuf, "HTTP") == 0)
    diDownloadOptions.dwUseProtocol = UP_HTTP;
  else
    diDownloadOptions.dwUseProtocol = UP_FTP;

  PrfQueryProfileString(hiniConfig, "Dialog Advanced Settings",       "Use Protocol Settings", "", szBuf,                     sizeof(szBuf));
  if(strcmpi(szBuf, "DISABLED") == 0)
    diDownloadOptions.bUseProtocolSettings = FALSE;
  else
    diDownloadOptions.bUseProtocolSettings = TRUE;

  PrfQueryProfileString(hiniConfig, "Dialog Advanced Settings",
                          "Show Protocols",
                          "",
                          szBuf,
                          sizeof(szBuf));
  if(strcmpi(szBuf, "FALSE") == 0)
    diDownloadOptions.bShowProtocols = FALSE;
  else
    diDownloadOptions.bShowProtocols = TRUE;

  /* Start Install dialog */
  PrfQueryProfileString(hiniConfig, "Dialog Start Install",       "Show Dialog",      "", szShowDialog,                     sizeof(szShowDialog));
  PrfQueryProfileString(hiniConfig, "Dialog Start Install",       "Title",            "", diStartInstall.szTitle,           MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Start Install",       "Message Install",  "", diStartInstall.szMessageInstall,  MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Dialog Start Install",       "Message Download", "", diStartInstall.szMessageDownload, MAX_BUF);
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diStartInstall.bShowDialog = TRUE;

  /* Download dialog */
  PrfQueryProfileString(hiniConfig, "Dialog Download",       "Show Dialog",        "", szShowDialog,                   sizeof(szShowDialog));
  PrfQueryProfileString(hiniConfig, "Dialog Download",       "Title",              "", diDownload.szTitle,             MAX_BUF_TINY);
  PrfQueryProfileString(hiniConfig, "Dialog Download",       "Message Download0",  "", diDownload.szMessageDownload0,  MAX_BUF_MEDIUM);
  PrfQueryProfileString(hiniConfig, "Dialog Download",       "Message Retry0",     "", diDownload.szMessageRetry0,     MAX_BUF_MEDIUM);
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diDownload.bShowDialog = TRUE;

  /* Reboot dialog */
  PrfQueryProfileString(hiniConfig, "Dialog Reboot", "Show Dialog", "", szShowDialog, sizeof(szShowDialog));
  if(strcmpi(szShowDialog, "TRUE") == 0)
    diReboot.dwShowDialog = TRUE;
  else if(strcmpi(szShowDialog, "AUTO") == 0)
    diReboot.dwShowDialog = AUTO;

  PrfQueryProfileString(hiniConfig, "Windows Integration-Item0", "CheckBoxState", "", szBuf,                                    sizeof(szBuf));
  PrfQueryProfileString(hiniConfig, "Windows Integration-Item0", "Description",   "", diWindowsIntegration.wiCB0.szDescription, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Windows Integration-Item0", "Archive",       "", diWindowsIntegration.wiCB0.szArchive,     MAX_BUF);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB0.szDescription != '\0')
    diWindowsIntegration.wiCB0.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(strcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB0.bCheckBoxState = TRUE;

  PrfQueryProfileString(hiniConfig, "Windows Integration-Item1", "CheckBoxState", "", szBuf,                           sizeof(szBuf));
  PrfQueryProfileString(hiniConfig, "Windows Integration-Item1", "Description",   "", diWindowsIntegration.wiCB1.szDescription, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Windows Integration-Item1", "Archive",       "", diWindowsIntegration.wiCB1.szArchive, MAX_BUF);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB1.szDescription != '\0')
    diWindowsIntegration.wiCB1.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(strcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB1.bCheckBoxState = TRUE;

  PrfQueryProfileString(hiniConfig, "Windows Integration-Item2", "CheckBoxState", "", szBuf,                           sizeof(szBuf));
  PrfQueryProfileString(hiniConfig, "Windows Integration-Item2", "Description",   "", diWindowsIntegration.wiCB2.szDescription, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Windows Integration-Item2", "Archive",       "", diWindowsIntegration.wiCB2.szArchive, MAX_BUF);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB2.szDescription != '\0')
    diWindowsIntegration.wiCB2.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(strcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB2.bCheckBoxState = TRUE;

  PrfQueryProfileString(hiniConfig, "Windows Integration-Item3", "CheckBoxState", "", szBuf,                           sizeof(szBuf));
  PrfQueryProfileString(hiniConfig, "Windows Integration-Item3", "Description",   "", diWindowsIntegration.wiCB3.szDescription, MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Windows Integration-Item3", "Archive",       "", diWindowsIntegration.wiCB3.szArchive, MAX_BUF);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB3.szDescription != '\0')
    diWindowsIntegration.wiCB3.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(strcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB3.bCheckBoxState = TRUE;

  /* Read in the Site Selector Status */
  PrfQueryProfileString(hiniConfig, "Site Selector", "Status", "", szBuf, sizeof(szBuf));
  if(strcmpi(szBuf, "HIDE") == 0)
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
  PrfQueryProfileString(hiniConfig, "General", "Default Setup Type", "", szBuf, sizeof(szBuf));
  if((strcmpi(szBuf, "Setup Type 0") == 0) && diSetupType.stSetupType0.bVisible)
  {
    dwSetupType     = ST_RADIO0;
    dwTempSetupType = dwSetupType;
  }
  else if((strcmpi(szBuf, "Setup Type 1") == 0) && diSetupType.stSetupType1.bVisible)
  {
    dwSetupType     = ST_RADIO1;
    dwTempSetupType = dwSetupType;
  }
  else if((strcmpi(szBuf, "Setup Type 2") == 0) && diSetupType.stSetupType2.bVisible)
  {
    dwSetupType     = ST_RADIO2;
    dwTempSetupType = dwSetupType;
  }
  else if((strcmpi(szBuf, "Setup Type 3") == 0) && diSetupType.stSetupType3.bVisible)
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
  PrfQueryProfileString(hiniConfig, "Core", "Install Size", "", szBuf, sizeof(szBuf));
  if(*szBuf != '\0')
    siCFXpcomFile.ullInstallSize = _atoi64(szBuf);
  else
    siCFXpcomFile.ullInstallSize = 0;

  PrfQueryProfileString(hiniConfig, "SmartDownload-Netscape Install", "core_file",        "", siSDObject.szXpcomFile,       MAX_BUF);
  PrfQueryProfileString(hiniConfig, "SmartDownload-Netscape Install", "core_file_path",   "", siSDObject.szXpcomFilePath,   MAX_BUF);
  PrfQueryProfileString(hiniConfig, "SmartDownload-Netscape Install", "xpcom_dir",        "", siSDObject.szXpcomDir,        MAX_BUF);
  PrfQueryProfileString(hiniConfig, "SmartDownload-Netscape Install", "no_ads",           "", siSDObject.szNoAds,           MAX_BUF);
  PrfQueryProfileString(hiniConfig, "SmartDownload-Netscape Install", "silent",           "", siSDObject.szSilent,          MAX_BUF);
  PrfQueryProfileString(hiniConfig, "SmartDownload-Netscape Install", "execution",        "", siSDObject.szExecution,       MAX_BUF);
  PrfQueryProfileString(hiniConfig, "SmartDownload-Netscape Install", "confirm_install",  "", siSDObject.szConfirmInstall,  MAX_BUF);
  PrfQueryProfileString(hiniConfig, "SmartDownload-Netscape Install", "extract_msg",      "", siSDObject.szExtractMsg,      MAX_BUF);
  PrfQueryProfileString(hiniConfig, "SmartDownload-Execution",        "exe",              "", siSDObject.szExe,             MAX_BUF);
  PrfQueryProfileString(hiniConfig, "SmartDownload-Execution",        "exe_param",        "", siSDObject.szExeParam,        MAX_BUF);

  PrfQueryProfileString(hiniConfig, "Core",                           "Source",           "", szBuf,                        sizeof(szBuf));
  DecryptString(siCFXpcomFile.szSource, szBuf);
  PrfQueryProfileString(hiniConfig, "Core",                           "Destination",      "", szBuf,                        sizeof(szBuf));
  DecryptString(siCFXpcomFile.szDestination, szBuf);
  PrfQueryProfileString(hiniConfig, "Core",                           "Message",          "", siCFXpcomFile.szMessage,      MAX_BUF);
  PrfQueryProfileString(hiniConfig, "Core",                           "Cleanup",          "", szBuf,                        sizeof(szBuf));
  if(strcmpi(szBuf, "FALSE") == 0)
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
        if(!PrfQueryProfileString(hiniInstall, "Messages", "STR_MESSAGEBOX_TITLE", "", szBuf, sizeof(szBuf)))
          strcpy(szTitle, "Setup");
        else
          sprintf(szTitle, szBuf, sgProduct.szProductName);

        PrfQueryProfileString(hiniConfig, "Strings", "Message Unfinished Download Restart", "", szBuf, sizeof(szBuf));
        if(WinMessageBox(HWND_DESKTOP, hWndMain, szBuf, szTitle, 0, MB_YESNO | MB_ICONQUESTION) == MBID_NO)
        {
          UnsetSetupCurrentDownloadFile();
          UnsetDownloadState(); /* unset the download state so that the archives can be deleted */
          DeleteArchives(DA_ONLY_IF_NOT_IN_ARCHIVES_LST);
        }
        break;
    }
  }

  iRv = StartupCheckArchives();
  PrfCloseProfile(hiniConfig);
  PrfCloseProfile(hiniInstall);
  return(iRv);
}

APIRET ParseInstallIni()
{
  HINI  hiniInstall;
  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  PrfQueryProfileString(hiniInstall, "General", "OK_", "", sgInstallGui.szOk_, sizeof(sgInstallGui.szOk_));
  PrfQueryProfileString(hiniInstall, "General", "OK", "", sgInstallGui.szOk, sizeof(sgInstallGui.szOk));
  PrfQueryProfileString(hiniInstall, "General", "CANCEL_", "", sgInstallGui.szCancel_, sizeof(sgInstallGui.szCancel_));
  PrfQueryProfileString(hiniInstall, "General", "CANCEL", "", sgInstallGui.szCancel, sizeof(sgInstallGui.szCancel));
  PrfQueryProfileString(hiniInstall, "General", "NEXT_", "", sgInstallGui.szNext_, sizeof(sgInstallGui.szNext_));
  PrfQueryProfileString(hiniInstall, "General", "BACK_", "", sgInstallGui.szBack_, sizeof(sgInstallGui.szBack_));
  PrfQueryProfileString(hiniInstall, "General", "PROXYSETTINGS_", "", sgInstallGui.szProxySettings_, sizeof(sgInstallGui.szProxySettings_));
  PrfQueryProfileString(hiniInstall, "General", "PROXYSETTINGS", "", sgInstallGui.szProxySettings, sizeof(sgInstallGui.szProxySettings));
  PrfQueryProfileString(hiniInstall, "General", "SERVER", "", sgInstallGui.szServer, sizeof(sgInstallGui.szServer));
  PrfQueryProfileString(hiniInstall, "General", "PORT", "", sgInstallGui.szPort, sizeof(sgInstallGui.szPort));
  PrfQueryProfileString(hiniInstall, "General", "USERID", "", sgInstallGui.szUserId, sizeof(sgInstallGui.szUserId));
  PrfQueryProfileString(hiniInstall, "General", "PASSWORD", "", sgInstallGui.szPassword, sizeof(sgInstallGui.szPassword));
  PrfQueryProfileString(hiniInstall, "General", "SELECTDIRECTORY", "", sgInstallGui.szSelectDirectory, sizeof(sgInstallGui.szSelectDirectory));
  PrfQueryProfileString(hiniInstall, "General", "DIRECTORIES_", "", sgInstallGui.szDirectories_, sizeof(sgInstallGui.szDirectories_));
  PrfQueryProfileString(hiniInstall, "General", "DRIVES_", "", sgInstallGui.szDrives_, sizeof(sgInstallGui.szDrives_));
  PrfQueryProfileString(hiniInstall, "General", "STATUS", "", sgInstallGui.szStatus, sizeof(sgInstallGui.szStatus));
  PrfQueryProfileString(hiniInstall, "General", "FILE", "", sgInstallGui.szFile, sizeof(sgInstallGui.szFile));
  PrfQueryProfileString(hiniInstall, "General", "URL", "", sgInstallGui.szUrl, sizeof(sgInstallGui.szUrl));
  PrfQueryProfileString(hiniInstall, "General", "TO", "", sgInstallGui.szTo, sizeof(sgInstallGui.szTo));
  PrfQueryProfileString(hiniInstall, "General", "ACCEPT_", "", sgInstallGui.szAccept_, sizeof(sgInstallGui.szAccept_));
  PrfQueryProfileString(hiniInstall, "General", "NO_", "", sgInstallGui.szNo_, sizeof(sgInstallGui.szNo_));
  PrfQueryProfileString(hiniInstall, "General", "PROGRAMFOLDER_", "", sgInstallGui.szProgramFolder_, sizeof(sgInstallGui.szProgramFolder_));
  PrfQueryProfileString(hiniInstall, "General", "EXISTINGFOLDERS_", "", sgInstallGui.szExistingFolder_, sizeof(sgInstallGui.szExistingFolder_));
  PrfQueryProfileString(hiniInstall, "General", "SETUPMESSAGE", "", sgInstallGui.szSetupMessage, sizeof(sgInstallGui.szSetupMessage));
  PrfQueryProfileString(hiniInstall, "General", "YESRESTART", "", sgInstallGui.szYesRestart, sizeof(sgInstallGui.szYesRestart));
  PrfQueryProfileString(hiniInstall, "General", "NORESTART", "", sgInstallGui.szNoRestart, sizeof(sgInstallGui.szNoRestart));
  PrfQueryProfileString(hiniInstall, "General", "ADDITIONALCOMPONENTS_", "", sgInstallGui.szAdditionalComponents_, sizeof(sgInstallGui.szAdditionalComponents_));
  PrfQueryProfileString(hiniInstall, "General", "DESCRIPTION", "", sgInstallGui.szDescription, sizeof(sgInstallGui.szDescription));
  PrfQueryProfileString(hiniInstall, "General", "TOTALDOWNLOADSIZE", "", sgInstallGui.szTotalDownloadSize, sizeof(sgInstallGui.szTotalDownloadSize));
  PrfQueryProfileString(hiniInstall, "General", "SPACEAVAILABLE", "", sgInstallGui.szSpaceAvailable, sizeof(sgInstallGui.szSpaceAvailable));
  PrfQueryProfileString(hiniInstall, "General", "COMPONENTS_", "", sgInstallGui.szComponents_, sizeof(sgInstallGui.szComponents_));
  PrfQueryProfileString(hiniInstall, "General", "DESTINATIONDIRECTORY", "", sgInstallGui.szDestinationDirectory, sizeof(sgInstallGui.szDestinationDirectory));
  PrfQueryProfileString(hiniInstall, "General", "BROWSE_", "", sgInstallGui.szBrowse_, sizeof(sgInstallGui.szBrowse_));
  PrfQueryProfileString(hiniInstall, "General", "CURRENTSETTINGS", "", sgInstallGui.szCurrentSettings, sizeof(sgInstallGui.szCurrentSettings));
  PrfQueryProfileString(hiniInstall, "General", "INSTALL_", "", sgInstallGui.szInstall_, sizeof(sgInstallGui.szInstall_));
  PrfQueryProfileString(hiniInstall, "General", "DELETE_", "", sgInstallGui.szDelete_, sizeof(sgInstallGui.szDelete_));
  PrfQueryProfileString(hiniInstall, "General", "EXTRACTING", "", sgInstallGui.szExtracting, sizeof(sgInstallGui.szExtracting));
  PrfQueryProfileString(hiniInstall, "General", "README", "", sgInstallGui.szReadme_, sizeof(sgInstallGui.szReadme_));
  PrfQueryProfileString(hiniInstall, "General", "PAUSE_", "", sgInstallGui.szPause_, sizeof(sgInstallGui.szPause_));
  PrfQueryProfileString(hiniInstall, "General", "RESUME_", "", sgInstallGui.szResume_, sizeof(sgInstallGui.szResume_));

  PrfCloseProfile(hiniInstall);
  return(0);
}


BOOL LocatePreviousPath(PSZ szMainSectionName, PSZ szPath, ULONG dwPathSize)
{
  ULONG dwIndex;
  char  szIndex[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szValue[MAX_BUF];
  BOOL  bFound;
  HINI   hiniConfig;

  bFound  = FALSE;
  dwIndex = -1;
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  while(!bFound)
  {
    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    strcpy(szSection, szMainSectionName);
    strcat(szSection, szIndex);

    PrfQueryProfileString(hiniConfig, szSection, "Key", "", szValue, sizeof(szValue));
    if(*szValue != '\0')
      bFound = LocatePathNscpReg(szSection, szPath, dwPathSize);
    else
    {
      PrfQueryProfileString(hiniConfig, szSection, "HKey", "", szValue, sizeof(szValue));
      if(*szValue != '\0')
        bFound = LocatePathWinReg(szSection, szPath, dwPathSize);
      else
      {
        PrfQueryProfileString(hiniConfig, szSection, "Path", "", szValue, sizeof(szValue));
        if(*szValue != '\0')
          bFound = LocatePath(szSection, szPath, dwPathSize);
        else
          break;
      }
    }
  }
  PrfCloseProfile(hiniConfig);
  return(bFound);
}


BOOL LocatePathNscpReg(PSZ szSection, PSZ szPath, ULONG dwPathSize)
{
  char  szKey[MAX_BUF];
  char  szContainsFilename[MAX_BUF];
  char  szBuf[MAX_BUF];
  BOOL  bReturn;
  HINI    hiniConfig;

  bReturn = FALSE;
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Key", "", szKey, sizeof(szKey));
  if(*szKey != '\0')
  {
    bReturn = FALSE;
    memset(szPath, 0, dwPathSize);

    VR_GetPath(szKey, MAX_BUF, szBuf);
    if(*szBuf != '\0')
    {
      PrfQueryProfileString(hiniConfig, szSection, "Contains Filename", "", szContainsFilename, sizeof(szContainsFilename));
      if(strcmpi(szContainsFilename, "TRUE") == 0)
        ParsePath(szBuf, szPath, dwPathSize, FALSE, PP_PATH_ONLY);
      else
        strcpy(szPath, szBuf);

      bReturn = TRUE;
    }
  }
  PrfClose(hiniConfig);
  return(bReturn);
}

ULONG GetTotalArchivesToDownload()
{
  ULONG     dwIndex0;
  ULONG     dwTotalArchivesToDownload;
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


BOOL LocatePathWinReg(PSZ szSection, PSZ szPath, ULONG dwPathSize)
{
  char  szHKey[MAX_BUF];
  char  szHRoot[MAX_BUF];
  char  szName[MAX_BUF];
  char  szVerifyExistence[MAX_BUF];
  char  szBuf[MAX_BUF];
  BOOL  bDecryptKey;
  BOOL  bContainsFilename;
  BOOL  bReturn;
  HINI    hiniConfig;
//  HKEY  hkeyRoot;

  bReturn = FALSE;
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "HKey", "", szHKey, sizeof(szHKey));
  if(*szHKey != '\0')
  {
    bReturn = FALSE;
    memset(szPath, 0, dwPathSize);

    PrfQueryProfileString(hiniConfig, szSection, "HRoot",        "", szHRoot, sizeof(szHRoot));
    PrfQueryProfileString(hiniConfig, szSection, "Name",         "", szName,  sizeof(szName));
    PrfQueryProfileString(hiniConfig, szSection, "Decrypt HKey", "", szBuf,   sizeof(szBuf));
    if(strcmpi(szBuf, "FALSE") == 0)
      bDecryptKey = FALSE;
    else
      bDecryptKey = TRUE;

    /* check for both 'Verify Existance' and 'Verify Existence' */
    PrfQueryProfileString(hiniConfig, szSection, "Verify Existence", "", szVerifyExistence, sizeof(szVerifyExistence));
    if(*szVerifyExistence == '\0')
      PrfQueryProfileString(hiniConfig, szSection, "Verify Existance", "", szVerifyExistence, sizeof(szVerifyExistence));

    PrfQueryProfileString(hiniConfig, szSection, "Contains Filename", "", szBuf, sizeof(szBuf));
    if(strcmpi(szBuf, "TRUE") == 0)
      bContainsFilename = TRUE;
    else
      bContainsFilename = FALSE;

//    hkeyRoot = ParseRootKey(szHRoot);
    if(bDecryptKey == TRUE)
    {
      DecryptString(szBuf, szHKey);
      strcpy(szHKey, szBuf);
    }

//    GetWinReg(hkeyRoot, szHKey, szName, szBuf, sizeof(szBuf));
    if(*szBuf != '\0')
    {
      if(strcmpi(szVerifyExistence, "FILE") == 0)
      {
        if(FileExists(szBuf))
        {
          if(bContainsFilename == TRUE)
            ParsePath(szBuf, szPath, dwPathSize, FALSE, PP_PATH_ONLY);
          else
            strcpy(szPath, szBuf);

          bReturn = TRUE;
        }
        else
          bReturn = FALSE;
      }
      else if(strcmpi(szVerifyExistence, "PATH") == 0)
      {
        if(bContainsFilename == TRUE)
          ParsePath(szBuf, szPath, dwPathSize, FALSE, PP_PATH_ONLY);
        else
          strcpy(szPath, szBuf);

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
          strcpy(szPath, szBuf);

        bReturn = TRUE;
      }
    }
  }
  PrfCloseProfile(hiniConfig);
  return(bReturn);
}


BOOL LocatePath(PSZ szSection, PSZ szPath, ULONG dwPathSize)
{
  char  szPathKey[MAX_BUF];
  BOOL  bReturn;
  HINI    hiniConfig;

  bReturn = FALSE;
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Path", "", szPathKey, sizeof(szPathKey));
  if(*szPathKey != '\0')
  {
    bReturn = FALSE;
    memset(szPath, 0, dwPathSize);

    DecryptString(szPath, szPathKey);
    bReturn = TRUE;
  }
  PrfCloseProfile(hiniConfig);
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

APIRET DecryptVariable(PSZ szVariable, ULONG dwVariableSize)
{
  char szBuf[MAX_BUF];
  char szKey[MAX_BUF];
  char szName[MAX_BUF];
  char szValue[MAX_BUF];
  char szWRMSCurrentVersion[] = "Software\\Microsoft\\Windows\\CurrentVersion";
  char szWRMSShellFolders[]   = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders";
  HINI  hiniInstall;

  /* zero out the memory allocations */
  memset(szBuf, 0, sizeof(szBuf));
  memset(szKey, 0, sizeof(szKey));
  memset(szName, 0, sizeof(szName));
  memset(szValue, 0, sizeof(szValue));

  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  if(strcmpi(szVariable, "PROGRAMFILESDIR") == 0)
  {
    /* parse for the "c:\Program Files" directory */
    PrfQueryProfileString(HINI_SYSTEMPROFILE, szWRMSCurrentVersion, "ProgramFilesDir", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "COMMONFILESDIR") == 0)
  {
    /* parse for the "c:\Program Files\Common Files" directory */
    PrfQueryProfileString(HINI_SYSTEMPROFILE, szWRMSCurrentVersion, "CommonFilesDir", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "MEDIAPATH") == 0)
  {
    /* parse for the "c:\Winnt40\Media" directory */
    PrfQueryProfileString(HINI_SYSTEMPROFILE, szWRMSCurrentVersion, "MediaPath", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "CONFIGPATH") == 0)
  {
    /* parse for the "c:\Windows\Config" directory */
    if(gSystemInfo.dwOSType & OS_WIN9x)
    {
        PrfQueryProfileString(HINI_SYSTEMPROFILE, szWRMSCurrentVersion, "ConfigPath", "", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "DEVICEPATH") == 0)
  {
    /* parse for the "c:\Winnt40\INF" directory */
    PrfQueryProfileString(HINI_SYSTEMPROFILE, szWRMSCurrentVersion, "DevicePath", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "COMMON_STARTUP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs\\Startup" directory */
    if(gSystemInfo.dwOSType & OS_WIN9x)
    {
        PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Startup", "", szVariable, dwVariableSize);
    }
    else
    {
        PrfQueryProfileString(HINI_SYSTEMPROFILE, szWRMSShellFolders, "Common Startup", "", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs" directory */
    if((gSystemInfo.dwOSType & OS_WIN9x) || gbRestrictedAccess)
    {
        PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Programs", "", szVariable, dwVariableSize);
    }
    else
    {
        PrfQueryProfileString(HINI_SYSTEMPROFILE, szWRMSShellFolders, "Common Programs", "", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "COMMON_PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs" directory */
    if(gSystemInfo.dwOSType & OS_WIN9x)
    {
        PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Programs", "", szVariable, dwVariableSize);
    }
    else
    {
        PrfQueryProfileString(HINI_SYSTEMPROFILE, szWRMSShellFolders, "Common Programs", "", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "COMMON_STARTMENU") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu" directory */
    if(gSystemInfo.dwOSType & OS_WIN9x)
    {
        PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Start Menu", "", szVariable, dwVariableSize);
    }
    else
    {
        PrfQueryProfileString(HINI_SYSTEMPROFILE, szWRMSShellFolders, "Common Start Menu", "", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "COMMON_DESKTOP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Desktop" directory */
    if(gSystemInfo.dwOSType & OS_WIN9x)
    {
        PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Desktop", "", szVariable, dwVariableSize);
    }
    else
    {
        PrfQueryProfileString(HINI_SYSTEMPROFILE, szWRMSShellFolders, "Common Desktop", "", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "PERSONAL_STARTUP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu\Programs\Startup" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Startup", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu\Programs" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Programs", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_STARTMENU") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Start Menu", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_DESKTOP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Desktop" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Desktop", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_APPDATA") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Application Data" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "AppData", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_CACHE") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Temporary Internet Files" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Cache", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_COOKIES") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Cookies" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Cookies", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_FAVORITES") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Favorites" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Favorites", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_FONTS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Fonts" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Fonts", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_HISTORY") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\History" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "History", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_NETHOOD") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\NetHood" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "NetHood", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_PERSONAL") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Personal" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Personal", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_PRINTHOOD") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\PrintHood" directory */
    if(gSystemInfo.dwOSType & OS_NT)
    {
        PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "PrintHood", "", szVariable, dwVariableSize);
    }
  }
  else if(strcmpi(szVariable, "PERSONAL_RECENT") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Recent" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Recent", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_SENDTO") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\SendTo" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "SendTo", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PERSONAL_TEMPLATES") == 0)
  {
    /* parse for the "C:\WINNT40\ShellNew" directory */
    PrfQueryProfileString(HINI_USERPROFILE, szWRMSShellFolders, "Templates", "", szVariable, dwVariableSize);
  }
  else if(strcmpi(szVariable, "PROGRAMFOLDERNAME") == 0)
  {
    /* parse for the "Netscape Communicator" folder name */
    strcpy(szVariable, sgProduct.szProgramFolderName);
  }
  else if(strcmpi(szVariable, "PROGRAMFOLDERPATH") == 0)
  {
    /* parse for the "c:\Windows\profiles\All Users\Start Menu\Programs" path */
    strcpy(szVariable, sgProduct.szProgramFolderPath);
  }
  else if(strcmpi(szVariable, "PROGRAMFOLDERPATHNAME") == 0)
  {
    /* parse for the "c:\Windows\profiles\All Users\Start Menu\Programs\Netscape Communicator" path */
    strcpy(szVariable, sgProduct.szProgramFolderPath);
    strcat(szVariable, "\\");
    strcat(szVariable, sgProduct.szProgramFolderName);
  }
  else if(strcmpi(szVariable, "PROGRAMFOLDERPATH") == 0)
  {
    /* parse for the "c:\Windows\profiles\All Users\Start Menu\Programs" path */
    strcpy(szVariable, sgProduct.szProgramFolderPath);
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

      if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_GET_WINDOWS_DIRECTORY_FAILED", "", szEGetWinDirFailed, sizeof(szEGetWinDirFailed)))
        PrintError(szEGetWinDirFailed, ERROR_CODE_SHOW);
      
      PrfCloseProfile(hiniInstall);
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

      if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_GET_WINDOWS_DIRECTORY_FAILED", "", szEGetWinDirFailed, sizeof(szEGetWinDirFailed)))
        PrintError(szEGetWinDirFailed, ERROR_CODE_SHOW);
      PrfCloseProfile(hiniInstall);
      exit(1);
    }
  }
  else if(strcmpi(szVariable, "WINSYSDIR") == 0)
  {
    /* Locate the "c:\Windows\System" (for Win95/Win98) or "c:\Windows\System32" (for NT) directory */
    if(GetSystemDirectory(szVariable, dwVariableSize) == 0)
    {
      char szEGetSysDirFailed[MAX_BUF];

      if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_GET_SYSTEM_DIRECTORY_FAILED", "", szEGetSysDirFailed, sizeof(szEGetSysDirFailed)))
        PrintError(szEGetSysDirFailed, ERROR_CODE_SHOW);

      PrfCloseProfile(hiniInstall);
      exit(1);
    }
  }
  else if(strcmpi(szVariable, "JRE BIN PATH") == 0)
  {
    /* Locate the "c:\Program Files\JavaSoft\JRE\1.3\Bin" directory */
    PrfQueryProfileString(HINI_SYSTEMPROFILE, "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\javaw.Exe", NULL, "", szVariable, dwVariableSize);
    if(*szVariable == '\0')
      return(FALSE);
    else
    {
      ParsePath(szVariable, szBuf, sizeof(szBuf), FALSE, PP_PATH_ONLY);
      strcpy(szVariable, szBuf);
    }
  }
  else if(strcmpi(szVariable, "JRE PATH") == 0)
  {
    /* Locate the "c:\Program Files\JavaSoft\JRE\1.3" directory */
    PrfQueryProfileString(HINI_SYSTEMPROFILE, "Software\\JavaSoft\\Java Plug-in\\1.3", "JavaHome", "", szVariable, dwVariableSize);
    if(*szVariable == '\0')
      return(FALSE);
  }
  else if(strcmpi(szVariable, "SETUP PATH") == 0)
  {
    strcpy(szVariable, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szVariable, dwVariableSize);
      strcat(szVariable, sgProduct.szSubPath);
    }
  }
  else if(strcmpi(szVariable, "Default Path") == 0)
  {
    strcpy(szVariable, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szVariable, dwVariableSize);
      strcat(szVariable, sgProduct.szSubPath);
    }
  }
  else if(strcmpi(szVariable, "SETUP STARTUP PATH") == 0)
  {
    strcpy(szVariable, szSetupDir);
  }
  else if(strcmpi(szVariable, "Default Folder") == 0)
  {
    strcpy(szVariable, sgProduct.szProgramFolderPath);
    AppendBackSlash(szVariable, dwVariableSize);
    strcat(szVariable, sgProduct.szProgramFolderName);
  }
  else if(strcmpi(szVariable, "Product CurrentVersion") == 0)
  {
    char szKey[MAX_BUF];

    sprintf(szKey, "Software\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductName);

    /* parse for the current Netscape WinReg key */
    PrfQueryProfileString(HINI_SYSTEMPROFILE, szKey, "CurrentVersion", "", szBuf, sizeof(szBuf));
    if(*szBuf == '\0')
      return(FALSE);

    sprintf(szVariable, "Software\\%s\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductName, szBuf);
  }
  else
  {
    PrfCloseProfile(hiniInstall);
    return(FALSE);
  }
  PrfCloseProfile(hiniInstall);
  return(TRUE);
}

APIRET DecryptString(PSZ szOutputStr, PSZ szInputStr)
{
  ULONG dwLenInputStr;
  ULONG dwCounter;
  ULONG dwVar;
  ULONG dwPrepend;
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

    /* if Variable is "XPI PATH", do special processing */
    if(strcmpi(szVariable, "XPI PATH") == 0)
    {
      strcpy(szBuf, sgProduct.szAlternateArchiveSearchPath);
      RemoveBackSlash(szBuf);
      strcpy(szOutuptStrTemp, szPrepend);
      strcat(szOutuptStrTemp, szBuf);
      strcat(szOutuptStrTemp, szAppend);

      if((*sgProduct.szAlternateArchiveSearchPath != '\0') && FileExists(szOutuptStrTemp))
      {
        strcpy(szVariable, sgProduct.szAlternateArchiveSearchPath);
      }
      else
      {
        strcpy(szBuf, szSetupDir);
        RemoveBackSlash(szBuf);
        strcpy(szOutuptStrTemp, szPrepend);
        strcat(szOutuptStrTemp, szBuf);
        strcat(szOutuptStrTemp, szAppend);

        if(!FileExists(szOutuptStrTemp))
          strcpy(szVariable, szTempDir);
        else
          strcpy(szVariable, szSetupDir);
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
        prefix_length = strlen(directory) - 1;

      if(prefix_length >= sizeof(buf)-1)
        return ZIP_ERR_GENERAL;

      err = ZIP_FindNext( find, buf, sizeof(buf) );
      while ( err == ZIP_OK ) 
      {
        CreateDirectoriesAll(buf, FALSE);
        if(buf[strlen(buf) - 1] != '/')
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

  memset(szFile, 0, sizeof(szFile));

  strcpy(szFile, szTempDir);
  AppendBackSlash(szFile, sizeof(szFile));
  strcat(szFile, FILE_WGET_LOG);

  if(FileExists(szFile))
    bFileExists = TRUE;

  DosDelete(szFile);
  return(bFileExists);
}

BOOL DeleteIdiGetConfigIni()
{
  char  szFileIdiGetConfigIni[MAX_BUF];
  BOOL  bFileExists = FALSE;

  memset(szFileIdiGetConfigIni, 0, sizeof(szFileIdiGetConfigIni));

  strcpy(szFileIdiGetConfigIni, szTempDir);
  AppendBackSlash(szFileIdiGetConfigIni, sizeof(szFileIdiGetConfigIni));
  strcat(szFileIdiGetConfigIni, FILE_IDI_GETCONFIGINI);
  if(FileExists(szFileIdiGetConfigIni))
  {
    bFileExists = TRUE;
  }
  DosDelete(szFileIdiGetConfigIni);
  return(bFileExists);
}

BOOL DeleteInstallLogFile(char *szFile)
{
  char  szInstallLogFile[MAX_BUF];
  BOOL  bFileExists = FALSE;

  strcpy(szInstallLogFile, szTempDir);
  AppendBackSlash(szInstallLogFile, sizeof(szInstallLogFile));
  strcat(szInstallLogFile, szFile);

  if(FileExists(szInstallLogFile))
  {
    bFileExists = TRUE;
    DosDelete(szInstallLogFile);
  }

  return(bFileExists);
}

BOOL DeleteIniRedirect()
{
  char  szFileIniRedirect[MAX_BUF];
  BOOL  bFileExists = FALSE;

  memset(szFileIniRedirect, 0, sizeof(szFileIniRedirect));

  strcpy(szFileIniRedirect, szTempDir);
  AppendBackSlash(szFileIniRedirect, sizeof(szFileIniRedirect));
  strcat(szFileIniRedirect, FILE_INI_REDIRECT);
  if(FileExists(szFileIniRedirect))
  {
    bFileExists = TRUE;
  }
  DosDelete(szFileIniRedirect);
  return(bFileExists);
}

BOOL DeleteIdiGetRedirect()
{
  char  szFileIdiGetRedirect[MAX_BUF];
  BOOL  bFileExists = FALSE;

  memset(szFileIdiGetRedirect, 0, sizeof(szFileIdiGetRedirect));

  strcpy(szFileIdiGetRedirect, szTempDir);
  AppendBackSlash(szFileIdiGetRedirect, sizeof(szFileIdiGetRedirect));
  strcat(szFileIdiGetRedirect, FILE_IDI_GETREDIRECT);
  if(FileExists(szFileIdiGetRedirect))
  {
    bFileExists = TRUE;
  }
  DosDelete(szFileIdiGetRedirect);
  return(bFileExists);
}

BOOL DeleteIdiGetArchives()
{
  char  szFileIdiGetArchives[MAX_BUF];
  BOOL  bFileExists = FALSE;

  memset(szFileIdiGetArchives, 0, sizeof(szFileIdiGetArchives));

  strcpy(szFileIdiGetArchives, szTempDir);
  AppendBackSlash(szFileIdiGetArchives, sizeof(szFileIdiGetArchives));
  strcat(szFileIdiGetArchives, FILE_IDI_GETARCHIVES);
  if(FileExists(szFileIdiGetArchives))
  {
    bFileExists = TRUE;
  }
  DosDelete(szFileIdiGetArchives);
  return(bFileExists);
}

BOOL DeleteIdiFileIniConfig()
{
  char  szFileIniConfig[MAX_BUF];
  BOOL  bFileExists = FALSE;

  memset(szFileIniConfig, 0, sizeof(szFileIniConfig));

  strcpy(szFileIniConfig, szTempDir);
  AppendBackSlash(szFileIniConfig, sizeof(szFileIniConfig));
  strcat(szFileIniConfig, FILE_INI_CONFIG);
  if(FileExists(szFileIniConfig))
  {
    bFileExists = TRUE;
  }
  DosDelete(szFileIniConfig);
  return(bFileExists);
}

BOOL DeleteIdiFileIniInstall()
{
  char  szFileIniInstall[MAX_BUF];
  BOOL  bFileExists = FALSE;

  memset(szFileIniInstall, 0, sizeof(szFileIniInstall));

  strcpy(szFileIniInstall, szTempDir);
  AppendBackSlash(szFileIniInstall, sizeof(szFileIniInstall));
  strcat(szFileIniInstall, FILE_INI_INSTALL);
  if(FileExists(szFileIniInstall))
  {
    bFileExists = TRUE;
  }
  DosDelete(szFileIniInstall);
  return(bFileExists);
}

void DeleteArchives(ULONG dwDeleteCheck)
{
  ULONG dwIndex0;
  char  szArchiveName[MAX_BUF];
  siC   *siCObject = NULL;

  memset(szArchiveName, 0, sizeof(szArchiveName));

  if((!bSDUserCanceled) && (CheckForPreviousUnfinishedDownload() == FALSE))
  {
    dwIndex0 = 0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
    while(siCObject)
    {
      strcpy(szArchiveName, szTempDir);
      AppendBackSlash(szArchiveName, sizeof(szArchiveName));
      strcat(szArchiveName, siCObject->szArchiveName);

      switch(dwDeleteCheck)
      {
        case DA_ONLY_IF_IN_ARCHIVES_LST:
          if(IsInArchivesLst(siCObject, FALSE))
            DosDelete(szArchiveName);
          break;

        case DA_ONLY_IF_NOT_IN_ARCHIVES_LST:
          if(!IsInArchivesLst(siCObject, FALSE))
            DosDelete(szArchiveName);
          break;

        case DA_IGNORE_ARCHIVES_LST:
        default:
          DosDelete(szArchiveName);
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

void DeInitialize()
{
  char szBuf[MAX_BUF];
  
  if(gErrorMessageStream.bEnabled && gErrorMessageStream.bSendMessage)
  {
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
      ULONG dwSize;

      strcpy(szWGetLog, szTempDir);
      AppendBackSlash(szWGetLog, sizeof(szWGetLog));
      strcat(szWGetLog, FILE_WGET_LOG);

      /* take into account '?' and '\0' chars */
      dwSize = strlen(gErrorMessageStream.szURL) +
               strlen(szPartialEscapedURL) + 2;
      if((szFullURL = NS_GlobalAlloc(dwSize)) != NULL)
      {
        sprintf(szFullURL,
                 "%s?%s",
                 gErrorMessageStream.szURL,
                 szPartialEscapedURL);

        sprintf(szMsg,
                 "UnEscapedURL: %s?%s\nEscapedURL: %s",
                 gErrorMessageStream.szURL,
                 gErrorMessageStream.szMessage,
                 szFullURL);

        if(gErrorMessageStream.bShowConfirmation &&
          (*gErrorMessageStream.szConfirmationMessage != '\0'))
        {
          char szConfirmationMessage[MAX_BUF];

          sprintf(szBuf,
                   "\n\n  %s",
                   gErrorMessageStream.szMessage);
          sprintf(szConfirmationMessage,
                   gErrorMessageStream.szConfirmationMessage,
                   szBuf);
          if(WinMessageBox(HWND_DESKTOP, hWndMain,
                        szConfirmationMessage,
                        sgProduct.szProductName,
                        0,
                        MB_OKCANCEL | MB_ICONQUESTION) == MBID_OK)
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

  LogISTime(W_END);
  if(bCreateDestinationDir)
  {
    strcpy(szBuf, sgProduct.szPath);
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
  DeInitDlgLicense(&diLicense);
  DeInitDlgWelcome(&diWelcome);
  DeInitSetupGeneral();
  DeInitDSNode(&gdsnComponentDSRequirement);
  DeInitErrorMessageStream();

  FreeMemory(&szTempDir);
  FreeMemory(&szOSTempDir);
  FreeMemory(&szSetupDir);
  FreeMemory(&szFileIniConfig);
  FreeMemory(&szEGlobalAlloc);
  FreeMemory(&szEDllLoad);
  FreeMemory(&szEStringLoad);
  FreeMemory(&szEStringNull);

  DosFreeModule(hSetupRscInst);
}

char *GetSaveInstallerPath(char *szBuf, ULONG dwBufSize)
{
#ifdef XXX_INTL_HACK_WORKAROUND_FOR_NOW
  char szBuf2[MAX_BUF];
#endif
  HINI  hiniInstall;

  /* determine the path to where the setup and downloaded files will be saved to */
  strcpy(szBuf, sgProduct.szPath);
  AppendBackSlash(szBuf, dwBufSize);
  if(*sgProduct.szSubPath != '\0')
  {
    strcat(szBuf, sgProduct.szSubPath);
    strcat(szBuf, " ");
  }

  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
#ifdef XXX_INTL_HACK_WORKAROUND_FOR_NOW
/* Installer can't create the Save Installer Path if the word "Setup" is localized. */
  if(PrfQueryProfileString(hiniInstall, "Messages", "STR_SETUP", "", szBuf2, sizeof(szBuf2)))
    strcat(szBuf, szBuf2);
  else
#endif
    strcat(szBuf, "Setup");
  PrfCloseProfile(hiniInstall);
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
  ULONG     dwIndex0;
  siC       *siCObject = NULL;

  GetSaveInstallerPath(szDestination, sizeof(szDestination));
  AppendBackSlash(szDestination, sizeof(szDestination));

  /* copy all files from the ns_temp dir to the install dir */
  CreateDirectoriesAll(szDestination, TRUE);

  /* copy the self extracting file that spawned setup.exe, if one exists */
  if((*sgProduct.szAlternateArchiveSearchPath != '\0') && (*sgProduct.szParentProcessFilename != '\0'))
  {
    strcpy(szSource, szSetupDir);
    AppendBackSlash(szSource, sizeof(szSource));
    strcat(szSource, "*.*");

    strcpy(szSource, sgProduct.szAlternateArchiveSearchPath);
    AppendBackSlash(szSource, sizeof(szSource));
    strcat(szSource, sgProduct.szParentProcessFilename);
    FileCopy(szSource, szDestination, FALSE, FALSE);
  }
  else
  {
    /* Else if self extracting file does not exist, copy the setup files */
    /* First get the current process' filename (in case it's not really named setup.exe */
    /* Then copy it to the install folder */
    DosQueryModuleName(NULL, sizeof(szBuf), szBuf);
    ParsePath(szBuf, szMFN, sizeof(szMFN), FALSE, PP_FILENAME_ONLY);

    strcpy(szBuf, szSetupDir);
    AppendBackSlash(szBuf, sizeof(szBuf));
    strcat(szBuf, szMFN);
    FileCopy(szBuf, szDestination, FALSE, FALSE);

    /* now copy the rest of the setup files */
    i = 0;
    while(TRUE)
    {
      if(*SetupFileList[i] == '\0')
        break;

      strcpy(szBuf, szSetupDir);
      AppendBackSlash(szBuf, sizeof(szBuf));
      strcat(szBuf, SetupFileList[i]);
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
      strcpy(szBuf, szArchivePath);
      AppendBackSlash(szBuf, sizeof(szBuf));
      strcat(szBuf, siCObject->szArchiveName);
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
BOOL CheckProcessNT4(PSZ szProcessName, ULONG dwProcessNameSize)
{
  BOOL          bRv;
  HKEY          hKey;
  ULONG         dwType;
  ULONG         dwSize;
  ULONG         dwTemp;
  ULONG         dwTitleLastIdx;
  ULONG         dwIndex;
  ULONG         dwLen;
  ULONG         pDataSize = 50 * 1024;
  PSZ         szCounterValueName;
  PSZ         szTitle;
  PSZ         *szTitleSz;
  PSZ         szTitleBuffer;
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
  szTitleBuffer = (PSZ)LocalAlloc(LMEM_FIXED, dwSize);
  if(!szTitleBuffer)
  {
    RegCloseKey(hKey);
    return(bRv);
  }

  szTitleSz = (PSZ *)LocalAlloc(LPTR, (dwTitleLastIdx + 1) * sizeof(PSZ));
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
  while(dwLen = strlen(szTitle))
  {
    dwIndex = atoi(szTitle);
    szTitle = szTitle + dwLen + 1;

    if(dwIndex <= dwTitleLastIdx)
      szTitleSz[dwIndex] = szTitle;

    szTitle = szTitle + strlen(szTitle) + 1;
  }

  PX_PROCESS = GetTitleIdx (NULL, szTitleSz, dwTitleLastIdx, PN_PROCESS);
  PX_THREAD  = GetTitleIdx (NULL, szTitleSz, dwTitleLastIdx, PN_THREAD);
  sprintf(INDEX_PROCTHRD_OBJ, TEXT("%ld %ld"), PX_PROCESS, PX_THREAD);
  pData = NULL;
  if(GetPerfData(HKEY_PERFORMANCE_DATA, INDEX_PROCTHRD_OBJ, &pData, &pDataSize) == ERROR_SUCCESS)
  {
    PPERF_INSTANCE pInst;
    ULONG          i = 0;

    pProcessObject = FindObject(pData, PX_PROCESS);
    if(pProcessObject)
    {
      PSZ szPtrStr;
      int   iLen;
      char  szProcessNamePruned[MAX_BUF];
      char  szNewProcessName[MAX_BUF];

      if(sizeof(szProcessNamePruned) < (strlen(szProcessName) + 1))
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
      strcpy(szProcessNamePruned, szProcessName);
      iLen = strlen(szProcessNamePruned);
      szPtrStr = &szProcessNamePruned[iLen - 4];
      if((strcmpi(szPtrStr, ".exe") == 0) || (strcmpi(szPtrStr, ".dll") == 0))
        *szPtrStr = '\0';

      pInst = FirstInstance(pProcessObject);
      for(i = 0; i < (ULONG)(pProcessObject->NumInstances); i++)
      {
        memset(szNewProcessName, 0, MAX_BUF);
        if(WideCharToMultiByte(CP_ACP,
                               0,
                               (LPCWSTR)((PCHAR)pInst + pInst->NameOffset),
                               -1,
                               szNewProcessName,
                               MAX_BUF,
                               NULL,
                               NULL) != 0)
        {
          if(strcmpi(szNewProcessName, szProcessNamePruned) == 0)
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
ULONG   GetPerfData    (HKEY        hPerfKey,
                        PSZ      szObjectIndex,
                        PPERF_DATA  *ppData,
                        ULONG       *pDataSize)
{
ULONG   DataSize;
ULONG   dwR;
ULONG   Type;


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
PPERF_OBJECT FindObject (PPERF_DATA pData, ULONG TitleIndex)
{
PPERF_OBJECT pObject;
ULONG        i = 0;

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
ULONG GetTitleIdx(HWND hWnd, PSZ Title[], ULONG LastIndex, PSZ Name)
{
  ULONG Index;

  for(Index = 0; Index <= LastIndex; Index++)
    if(Title[Index])
      if(!strcmpi (Title[Index], Name))
        return(Index);

  WinMessageBox(HWND_DESKTOP, hWnd, Name, TEXT("Pviewer cannot find index"), 0, MB_OK);
  return 0;
}

