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
 *   Curt Patrick <curt@netscape.com>
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
#include "process.h"
#include "dialogs.h"
#include "ifuncns.h"
#include "xpnetHook.h"
#include "time.h"
#include "xpi.h"
#include "logging.h"
#include "nsEscape.h"
#include <logkeys.h>
#include <winnls.h>
#include <winver.h>

// shellapi.h is needed to build with WIN32_LEAN_AND_MEAN
#include <shellapi.h>

#define HIDWORD(l)   ((DWORD) (((ULONG) (l) >> 32) & 0xFFFF))
#define LODWORD(l)   ((DWORD) (l))

#define DEFAULT_ICON_SIZE   32

#define FTPSTR_LEN (sizeof(szFtp) - 1)
#define HTTPSTR_LEN (sizeof(szHttp) - 1)

ULONG  (PASCAL *NS_GetDiskFreeSpace)(LPCTSTR, LPDWORD, LPDWORD, LPDWORD, LPDWORD);
ULONG  (PASCAL *NS_GetDiskFreeSpaceEx)(LPCTSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);
HRESULT InitGre(greInfo *gre);
void    DeInitGre(greInfo *gre);
void    UpdateGreInstallerCmdLine(greInfo *aGre, char *aParameter, DWORD aParameterBufSize, BOOL forExistingGre);
void    LaunchExistingGreInstaller(greInfo *gre);
HRESULT GetInstalledGreConfigIni(greInfo *aGre, char *aGreConfigIni, DWORD aGreConfigIniBufSize);
HRESULT GetInfoFromInstalledGreConfigIni(greInfo *aGre);
HRESULT DetermineGreComponentDestinationPath(char *aInPath, char *aOutPath, DWORD aOutPathBufSize);
BOOL    IsOkToRemoveFileOrDirname(char *aFileOrDirname, char **aListToIgnore,
int     aListToIgnoreLength, char **aListProfileObjects, int aListProfileLength);
int     GetTotalNumKeys(char *aSectionName, char *aBaseKeyName);
void    CleanupArrayList(char **aList, int aListLength);
char    **BuildArrayList(char *aSectionName, char *aBaseKeyName, int *aArrayLength);
BOOL    IsDirAProfileDir(char *aParentPath, char **aListProfileObjects, int aListLength);

static greInfo gGre;

char *ArchiveExtensions[] = {"zip",
                             "xpi",
                             "jar",
                             ""};

// Path and filename to the GRE's uninstaller.  This is used to cleanup
// old versions of GRE that have been orphaned when upgrading mozilla.
#define GRE_UNINSTALLER_FILE "[WINDIR]\\GREUninstall.exe"

#define GRE_REG_KEY "Software\\mozilla.org\\GRE"
#define SETUP_STATE_REG_KEY "Software\\%s\\%s\\%s\\Setup"
#define APP_PATHS_KEY "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%s"

//GRE app installer progress bar integration
typedef LONG (CALLBACK* LPFNDLLFUNC)(LPCTSTR,INT);

#define GRE_APP_INSTALLER_PROXY_DLL      "ProgUpd.dll"
#define GRE_PROXY_UPD_FUNC               "StdUpdateProgress"
#define GRE_INSTALLER_ID                 "gre"

#define FOR_EXISTING_GRE                 TRUE
#define FOR_NEW_GRE                      FALSE

LPSTR szProxyDLLPath;
LPFNDLLFUNC lpfnProgressUpd;
HINSTANCE hGREAppInstallerProxyDLL;

BOOL gGreInstallerHasRun = FALSE;

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
  BOOL     bRv;
  WNDCLASS wc;

  wc.style         = CS_HREDRAW | CS_VREDRAW | CS_PARENTDC | CS_SAVEBITS;
  wc.lpfnWndProc   = DefWindowProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = hInstance;
  wc.hIcon         = LoadIcon(hSetupRscInst, MAKEINTRESOURCE(IDI_SETUP));
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_ACTIVECAPTION + 1);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = CLASS_NAME_SETUP;

  bRv = RegisterClass(&wc);
  if(bRv == FALSE)
    return(bRv);

  return(InitDialogClass(hInstance, hSetupRscInst));
}

BOOL InitInstance(HINSTANCE hInstance, DWORD dwCmdShow)
{
  HWND hWnd;

  gSystemInfo.dwScreenX = GetSystemMetrics(SM_CXSCREEN);
  gSystemInfo.dwScreenY = GetSystemMetrics(SM_CYSCREEN);

  gSystemInfo.lastWindowPosCenterX = gSystemInfo.dwScreenX / 2;
  gSystemInfo.lastWindowPosCenterY = gSystemInfo.dwScreenY / 2;

  hInst = hInstance;

  /* This window is only for the purpose of allowing the self-extracting .exe
   * code to detect that a setup.exe is currenly running and do the appropriate
   * action given certain conditions.  This window is created and left in the
   * invisible state.
   * There's no other purpose for this window at this time.
   */
  hWnd = CreateWindow(CLASS_NAME_SETUP,
                      DEFAULT_SETUP_WINDOW_NAME,
                      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                      -150,
                      -50,
                      1,
                      1,
                      NULL,
                      NULL,
                      hInstance,
                      NULL);

  if(!hWnd)
    return(FALSE);

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

  if((sgProduct.mode != SILENT) && (sgProduct.mode != AUTO))
  {
    MessageBox(hWndMain, szErrorString, NULL, MB_ICONEXCLAMATION);
  }
  else if(sgProduct.mode == AUTO)
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
    return(vBuf);
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

void SetInstallFilesVar(LPSTR szProdDir)
{
  char szProgramPath[MAX_BUF];

  if(szProdDir[lstrlen(szProdDir) - 1] == '\\')
    wsprintf(szProgramPath, "%s%s", szProdDir, sgProduct.szProgramName);
  else
    wsprintf(szProgramPath, "%s\\%s", szProdDir, sgProduct.szProgramName);

  if((!gbForceInstall) && FileExists(szProgramPath))
    sgProduct.bInstallFiles = FALSE;
}

void UnsetSetupState(void)
{
  char szKey[MAX_BUF_TINY];

  wsprintf(szKey,
           SETUP_STATE_REG_KEY,
           sgProduct.szCompanyName,
           sgProduct.szProductNameInternal,
           sgProduct.szUserAgent);
  DeleteWinRegValue(HKEY_CURRENT_USER, szKey, "Setup State");
}

void SetSetupState(char *szState)
{
  char szKey[MAX_BUF_TINY];

  wsprintf(szKey,
           SETUP_STATE_REG_KEY,
           sgProduct.szCompanyName,
           sgProduct.szProductNameInternal,
           sgProduct.szUserAgent);

  SetWinReg(HKEY_CURRENT_USER, szKey, TRUE, "Setup State", TRUE,
            REG_SZ, szState, lstrlen(szState), TRUE, FALSE);
}

DWORD GetPreviousUnfinishedState(void)
{
  char szBuf[MAX_BUF_TINY];
  char szKey[MAX_BUF_TINY];
  DWORD dwRv = PUS_NONE;

  if(sgProduct.szCompanyName &&
     sgProduct.szProductNameInternal &&
     sgProduct.szUserAgent)
  {
    wsprintf(szKey,
             SETUP_STATE_REG_KEY,
             sgProduct.szCompanyName,
             sgProduct.szProductNameInternal,
             sgProduct.szUserAgent);
    GetWinReg(HKEY_CURRENT_USER, szKey, "Setup State", szBuf, sizeof(szBuf));
    if(lstrcmpi(szBuf, SETUP_STATE_DOWNLOAD) == 0)
      dwRv = PUS_DOWNLOAD;
    else if(lstrcmpi(szBuf, SETUP_STATE_UNPACK_XPCOM) == 0)
      dwRv = PUS_UNPACK_XPCOM;
    else if(lstrcmpi(szBuf, SETUP_STATE_INSTALL_XPI) == 0)
      dwRv = PUS_INSTALL_XPI;
  }

  return(dwRv);
}

void UnsetSetupCurrentDownloadFile(void)
{
  char szKey[MAX_BUF];

  wsprintf(szKey,
           SETUP_STATE_REG_KEY,
           sgProduct.szCompanyName,
           sgProduct.szProductNameInternal,
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
           sgProduct.szProductNameInternal,
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
     sgProduct.szProductNameInternal &&
     sgProduct.szUserAgent)
  {
    wsprintf(szKey,
             SETUP_STATE_REG_KEY,
             sgProduct.szCompanyName,
             sgProduct.szProductNameInternal,
             sgProduct.szUserAgent);
    GetWinReg(HKEY_CURRENT_USER,
              szKey,
              "Setup Current Download", 
              szCurrentDownloadFile,
              dwCurrentDownloadFileBufSize);
  }

  return(szCurrentDownloadFile);
}

void UpdateGREAppInstallerProgress(int percent)
{
  if (lpfnProgressUpd)
    lpfnProgressUpd(GRE_INSTALLER_ID, percent);
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

/* Function: RemoveDelayedDeleteFileEntries()
 *
 *       in: const char *aPathToMatch - path to match against
 *
 *  purpose: To remove windows registry entries (normally set by the uninstaller)
 *           that dictates what files to remove at system remove.
 *           The windows registry key that will be parsed is:
 *
 *             key : HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager
 *             name: PendingFileRenameOperations
 *
 *           This will not remove any entries that are set to be 'renamed'
 *           at system remove, only to be 'deleted'.
 *
 *           This function is multibyte safe.
 *
 *           To see what format the value of the var is in, look up the win32 API:
 *             MoveFileEx()
 */
void RemoveDelayedDeleteFileEntries(const char *aPathToMatch)
{
  HKEY  hkResult;
  DWORD dwErr;
  DWORD dwType = REG_NONE;
  DWORD oldMaxValueLen = 0;
  DWORD newMaxValueLen = 0;
  DWORD lenToEnd = 0;
  char  *multiStr = NULL;
  const char key[] = "SYSTEM\\CurrentControlSet\\Control\\Session Manager";
  const char name[] = "PendingFileRenameOperations";
  char  *pathToMatch;
  char  *lcName;
  char  *pName;
  char  *pRename;
  int   nameLen, renameLen;

  assert(aPathToMatch);

  /* if not NT systems (win2k, winXP) return */
  if (!(gSystemInfo.dwOSType & OS_NT))
    return;

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_READ|KEY_WRITE, &hkResult) != ERROR_SUCCESS)
    return;

  dwErr = RegQueryValueEx(hkResult, name, 0, &dwType, NULL, &oldMaxValueLen);
  if (dwErr != ERROR_SUCCESS || oldMaxValueLen == 0 || dwType != REG_MULTI_SZ)
  {
    /* no value, no data, or wrong type */
    return;
  }

  multiStr = calloc(oldMaxValueLen, sizeof(BYTE));
  if (!multiStr)
    return;

  pathToMatch = strdup(aPathToMatch);
  if (!pathToMatch)
  {
      free(multiStr);
      return;
  }

  if (RegQueryValueEx(hkResult, name, 0, NULL, multiStr, &oldMaxValueLen) == ERROR_SUCCESS)
  {
      // The registry value consists of name/newname pairs of null-terminated
      // strings, with a final extra null termination. We're only interested
      // in files to be deleted, which are indicated by a null newname.
      CharLower(pathToMatch);
      lenToEnd = newMaxValueLen = oldMaxValueLen;
      pName = multiStr;
      while(*pName && lenToEnd > 0)
      {
          // find the locations and lengths of the current pair. Count the
          // nulls,  we need to know how much data to skip or move
          nameLen = strlen(pName) + 1;
          pRename = pName + nameLen;
          renameLen = strlen(pRename) + 1;

          // How much remains beyond the current pair
          lenToEnd -= (nameLen + renameLen);

          if (*pRename == '\0')
          {
              // No new name, it's a delete. Is it the one we want?
              lcName = strdup(pName);
              if (lcName)
              {
                  CharLower(lcName);
                  if (strstr(lcName, pathToMatch))
              {
                  // It's a match--
                  // delete this pair by moving the remainder on top
                  memmove(pName, pRename + renameLen, lenToEnd);

                  // update the total length to reflect the missing pair
                  newMaxValueLen -= (nameLen + renameLen);

                  // next pair is in place, continue w/out moving pName
                      free(lcName);
                  continue;
              }
                  free(lcName);
              }
          }
          // on to the next pair
          pName = pRename + renameLen;
      }

      if (newMaxValueLen != oldMaxValueLen)
      {
          // We've deleted something, save the changed data
          RegSetValueEx(hkResult, name, 0, REG_MULTI_SZ, multiStr, newMaxValueLen);
          RegFlushKey(hkResult);
      }
  }

  RegCloseKey(hkResult);
  free(multiStr);
  free(pathToMatch);
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
  char  szLCUninstallFilenameLongBuf[MAX_BUF];
  char  szLCUninstallFilenameShortBuf[MAX_BUF];
  char  szWinInitFile[MAX_BUF];
  char  szTempInitFile[MAX_BUF];
  char  szWinDir[MAX_BUF];

  if(!GetWindowsDirectory(szWinDir, sizeof(szWinDir)))
    return;

  wsprintf(szLCUninstallFilenameLongBuf, "%s\\%s", szWinDir, sgProduct.szUninstallFilename);
  GetShortPathName(szLCUninstallFilenameLongBuf, szLCUninstallFilenameShortBuf, sizeof(szLCUninstallFilenameShortBuf));

  if(gSystemInfo.dwOSType & OS_NT)
  {
    RemoveDelayedDeleteFileEntries(szLCUninstallFilenameShortBuf);
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

  bSDUserCanceled        = FALSE;
  hDlgMessage            = NULL;

  /* load strings from setup.exe */
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_GLOBALALLOC, &szEGlobalAlloc, MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_STRING_LOAD, &szEStringLoad,  MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_DLL_LOAD,    &szEDllLoad,     MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_STRING_NULL, &szEStringNull,  MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_OUTOFMEMORY, &szEOutOfMemory, MAX_BUF))
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
      return(WIZ_ERROR_LOADING_RESOURCE_LIB);
    }
  }

  dwWizardState          = DLG_NONE;
  dwTempSetupType        = dwWizardState;
  siComponents           = NULL;
  bCreateDestinationDir  = FALSE;
  bReboot                = FALSE;
  gdwUpgradeValue        = UG_NONE;
  gdwSiteSelectorStatus  = SS_SHOW;
  gbILUseTemp            = TRUE;
  gbIgnoreRunAppX        = FALSE;
  gbIgnoreProgramFolderX = FALSE;

  if((szSetupDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  lstrcpy(szSetupDir, szCurrentProcessDir);

  if((szTempDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szOSTempDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szProxyDLLPath = NS_GlobalAlloc(MAX_BUF)) == NULL)
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

  /*  if multiple installer instances are allowed; 
      each instance requires a unique temp directory
   */
  if(gbAllowMultipleInstalls)
  {
    DWORD dwLen = lstrlen(szTempDir);

    if (strncmp(szSetupDir, szTempDir, dwLen) == 0)
    {
      lstrcpy(szTempDir, szSetupDir);
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
    CreateDirectoriesAll(szTempDir, DO_NOT_ADD_TO_UNINSTALL_LOG);
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

  /* Check if we need to load GRE App installer proxy DLL;
     this DLL lives in the Windows temp directory when the 
     external GRE app installer is running. If found, it 
     will be loaded and fed with incremental progress updates.
  */  

  GetTempPath(MAX_BUF, szProxyDLLPath);
  AppendBackSlash(szProxyDLLPath, MAX_BUF);
  strcat(szProxyDLLPath, GRE_APP_INSTALLER_PROXY_DLL);
  
  if (FileExists(szProxyDLLPath) != FALSE)
    hGREAppInstallerProxyDLL = LoadLibrary(szProxyDLLPath);
  
  if (hGREAppInstallerProxyDLL != NULL)
    lpfnProgressUpd = (LPFNDLLFUNC) GetProcAddress(hGREAppInstallerProxyDLL, GRE_PROXY_UPD_FUNC);
                             

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

  SystemParametersInfo(SPI_GETSCREENREADER, 0, &(gSystemInfo.bScreenReader), 0);

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
  DWORD length;

  assert(szSrc);
  assert(szDest);

  length = lstrlen(szSrc) + 1;
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

/* Function to locate the first character c in lpszString,
 * and return a pointer to it. */
LPSTR MozStrChar(LPSTR lpszString, char c)
{
  char* p = lpszString;

  if(!p)
    return NULL;

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

  if((!szInUrl) || !diAdditionalOptions.bUseProtocolSettings)
    return;

  ZeroMemory(szTmpBuf, sizeof(szTmpBuf));
  switch(diAdditionalOptions.dwUseProtocol)
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
  /* Check to see if mode has already been set.  If so,
   * then do not override it.
   * We don't want to override this value because it could have been
   * set via the command line argument as opposed to the config.ini
   * having been parse.  Command line arguments take precedence.
   */
  if(sgProduct.mode != NOT_SET)
    return;

  if(lstrcmpi(szMode, "NORMAL") == 0)
    sgProduct.mode = NORMAL;
  if(lstrcmpi(szMode, "AUTO") == 0)
    sgProduct.mode = AUTO;
  if(lstrcmpi(szMode, "SILENT") == 0)
    sgProduct.mode = SILENT;
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

  gbDownloadTriggered= FALSE;
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

  SetSetupState(SETUP_STATE_DOWNLOAD);

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
      gbDownloadTriggered = TRUE;
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
    if(gbDownloadTriggered)
      LogISComponentsFailedCRC(NULL, W_DOWNLOAD);
  }

  LogISDownloadProtocol(diAdditionalOptions.dwUseProtocol);
  LogMSDownloadProtocol(diAdditionalOptions.dwUseProtocol);

  if(lResult == WIZ_OK)
  {
    LogISDownloadStatus("ok", NULL);
  }
  else if(gbDownloadTriggered)
  {
    wsprintf(szBuf, "failed: %d", lResult);
    LogISDownloadStatus(szBuf, szFailedFile);
  }

  /* We want to log the download status regardless if we downloaded or not. */
  LogMSDownloadStatus(lResult);

  if(lResult == WIZ_OK)
  {
    UnsetSetupCurrentDownloadFile();
    UnsetSetupState();
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

/* Function: UpdateGreInstallerCmdLine()
 *       in: greInfo *aGre - contains infor needed by this function
 *           BOOL forExistingGre - to determine if the caller is needing the
 *               aParameter to be used by an existing GRE installer or by
 *               a new GRE installer.
 *   in/out: char *aParameter.
 *  purpose: To update the default GRE installer's command line parameters
 *           with new defaults depending on config.ini or cmdline arguments
 *           to this app's installer.
 *           It will also check to make sure GRE's default destination path
 *           is writable.  If not, it will change the GRE's destination path
 *           to [product path]\GRE\[gre ver].
 */
void UpdateGreInstallerCmdLine(greInfo *aGre, char *aParameter, DWORD aParameterBufSize, BOOL forExistingGre)
{
  char productPath[MAX_BUF];

  MozCopyStr(sgProduct.szPath, productPath, sizeof(productPath));
  RemoveBackSlash(productPath);

  /* Decide GRE installer's run mode.  Default should be -ma (AUTO).
   * The only other possibility is -ms (SILENT).  We don't want to allow
   * the GRE installer to run in NORMAL mode because we don't want to
   * let the user see more dialogs than they need to. */
  if(sgProduct.mode == SILENT)
    lstrcat(aParameter, " -ms");
  else
    lstrcat(aParameter, " -ma");

  /* Force the install of GRE if '-greForce' is passed or if GRE
   * is to be local.
   *
   * Passing '-f' to GRE's installer will force it to install
   * regardless of version found on system.  If '-f' is already
   * present in the parameter, it will do no harm to pass it again. */
  if(gbForceInstallGre || (sgProduct.greType == GRE_LOCAL))
    lstrcat(aParameter, " -f");

  /* If GRE is to be local, then instruct the GRE's installer to
   * install to this application's destination path stored in
   * sgProduct.szPath.
   *
   * We need to also instruct the GRE's installer to create a
   * private windows registry GRE key instead of the default one, so
   * that other apps attempting to use the global GRE will not find
   * this private, local copy.  They should not find this copy! */
  if(sgProduct.greType == GRE_LOCAL)
  {
    char buf[MAX_BUF];

    wsprintf(buf, " -dd \"%s\" -reg_path %s", productPath, sgProduct.grePrivateKey);
    lstrcat(aParameter, buf);
  }
  else if(!forExistingGre)
  {
    char buf[MAX_BUF];

    assert(aGre);
    assert(*aGre->homePath);
    assert(*aGre->userAgent);

    /* Append a backslash to the path so CreateDirectoriesAll() will see the
     * the last directory name as a directory instead of a file. */
    AppendBackSlash(aGre->homePath, sizeof(aGre->homePath));

    /* Attempt to create the GRE destination directory.  If it fails, we don't
     * have sufficient access to the default path.  We then need to install
     * GRE to:
     *   [product path]\GRE\[gre id]
     *
     * This path should be guaranteed to be writable because the user had
     * already created the parent path ([product path]). */
    if(DirHasWriteAccess(aGre->homePath) != WIZ_OK)
    {
      int rv = WIZ_OK;

      /* Update the homePath to the new destination path of where GRE will be
       * installed to. homePath is used elsewhere and it needs to be
       * referencing the new path, else things will break. */
      _snprintf(aGre->homePath, sizeof(aGre->homePath), "%s\\GRE\\%s\\", productPath, aGre->userAgent);
      aGre->homePath[sizeof(aGre->homePath) - 1] = '\0';

      /* CreateDirectoriesAll() is guaranteed to succeed using the new GRE
       * destination path because it is a subdir of this product's destination
       * path that has already been created successfully. */
      rv = CreateDirectoriesAll(aGre->homePath, ADD_TO_UNINSTALL_LOG);
      assert(rv == WIZ_OK);

      /* When using the -dd option to override the GRE's destination path, the
       * path must be a path which includes the GRE ID, ie:
       *   C:\Program Files\mozilla.org\Mozilla\GRE\1.4_0000000000
       */
      _snprintf(buf, sizeof(buf), " -dd \"%s\"", aGre->homePath);
      buf[sizeof(buf) - 1] = '\0';
      lstrcat(aParameter, buf);
    }
  }
}

/* Function: GetInstalledGreConfigIni()
 *       in: greInfo *aGre: gre class containing the location of GRE
 *           already installed.
 *      out: char *aGreConfigIni, DWORD aGreConfigIniBufSize: contains
 *           the full path to the installed GRE installer's config.ini
 *           file.
 *  purpose: To get the full path to the installed GRE installer's
 *           config.ini file.
 */
HRESULT GetInstalledGreConfigIni(greInfo *aGre, char *aGreConfigIni, DWORD aGreConfigIniBufSize)
{
  char buf[MAX_BUF];

  if(!aGre || !aGreConfigIni || (*aGre->homePath == '\0'))
    return(WIZ_ERROR_UNDEFINED);

  *aGreConfigIni = '\0';
  MozCopyStr(aGre->homePath, buf, sizeof(buf));
  AppendBackSlash(buf, sizeof(buf));
  wsprintf(aGreConfigIni, "%s%s\\%s", buf, GRE_SETUP_DIR_NAME, FILE_INI_CONFIG);
  return(WIZ_OK);
}

/* Function: GetInfoFromInstalledGreConfigIni()
 *       in: greInfo *aGre: gre class to fill the info for.
 *      out: returns info in aGre.
 *  purpose: To retrieve the GRE uninstaller file (full path) from
 *           the installed GRE's config.ini file.
 *           GetInfoFromGreInstaller() retrieves information from
 *           the (to be run) GRE installer's config.ini, not from the
 *           installed GRE installer's config.ini file.
 */
HRESULT GetInfoFromInstalledGreConfigIni(greInfo *aGre)
{
  HRESULT rv = WIZ_ERROR_UNDEFINED;
  char    greConfigIni[MAX_BUF];
  char    buf[MAX_BUF];
  char    greUninstallerFilename[MAX_BUF];

  if(!aGre)
    return(rv);

  *aGre->uninstallerAppPath = '\0';
  rv = GetInstalledGreConfigIni(aGre, greConfigIni, sizeof(greConfigIni));
  if(WIZ_OK == rv)
  {
    GetPrivateProfileString("General", "Uninstall Filename", "", greUninstallerFilename, sizeof(greUninstallerFilename), greConfigIni);
    wsprintf(buf, "[WINDIR]\\%s", greUninstallerFilename);
    DecryptString(aGre->uninstallerAppPath, buf);
    GetPrivateProfileString("General", "User Agent", "", aGre->userAgent, sizeof(aGre->userAgent), greConfigIni);
  }
  return(rv);
}

/* Function: GetInfoFromGreInstaller()
 *       in: char    *aGreInstallerFile: full path to the GRE installer.
 *           greInfo *aGre: gre class to fill the homePath for.
 *      out: returns homePath in aGre.
 *  purpose: To retrieve the GRE home path from the GRE installer.
 */
void GetInfoFromGreInstaller(char *aGreInstallerFile, greInfo *aGre)
{
  char szBuf[MAX_BUF];
  char extractedConfigFile[MAX_BUF];
  int  size = 0;

  if(!aGre)
    return;

  /* If GRE is to be installed locally, then set it to the
   * application's destination path. */
  if(sgProduct.greType == GRE_LOCAL)
  {
    MozCopyStr(sgProduct.szPath, aGre->homePath, sizeof(aGre->homePath));
    return;
  }

  *aGre->homePath = '\0';

  /* uncompress GRE installer's config.ini file in order to parse for:
   *   [General]
   *   Path=
   *   User Agent=
   */
  wsprintf(szBuf, "-mmi -ms -u %s", FILE_INI_CONFIG);
  WinSpawn(aGreInstallerFile, szBuf, szOSTempDir, SW_SHOWNORMAL, WS_WAIT);
  MozCopyStr(szOSTempDir, extractedConfigFile, sizeof(extractedConfigFile));
  AppendBackSlash(extractedConfigFile, sizeof(extractedConfigFile));
  lstrcat(extractedConfigFile, FILE_INI_CONFIG);
  GetPrivateProfileString("General", "Path", "", szBuf, sizeof(szBuf), extractedConfigFile);
  DecryptString(aGre->homePath, szBuf);
  GetPrivateProfileString("General", "User Agent", "", aGre->userAgent, sizeof(aGre->userAgent), extractedConfigFile);
  DeleteFile(extractedConfigFile);
}

/* Function: CleanupOrphanedGREs()
 *       in: none.
 *      out: none.
 *  purpose: To clean up GREs that were left on the system by previous
 *           installations of this product only (not by any other product).
 */
HRESULT CleanupOrphanedGREs()
{
  HKEY      greIDKeyHandle;
  HKEY      greAppListKeyHandle;
  DWORD     totalGreIDSubKeys;
  DWORD     totalGREAppListSubKeys;
  char      **greIDListToClean = NULL;
  char      **greAppListToClean = NULL;
  char      greKeyID[MAX_BUF_MEDIUM];
  char      greRegKey[MAX_BUF];
  char      greKeyAppList[MAX_BUF_MEDIUM];
  char      greAppListKeyPath[MAX_BUF];
  char      greUninstaller[MAX_BUF];
  DWORD     idKeySize;
  DWORD     appListKeySize;
  DWORD     indexOfGREID;
  DWORD     indexOfGREAppList;
  int       rv = WIZ_OK;
  int       regRv = WIZ_OK;
  char      buf[MAX_BUF];

  DecryptString(greUninstaller, GRE_UNINSTALLER_FILE);

  _snprintf(buf, sizeof(buf), "    GRE Cleanup Orphans: %s\n", sgProduct.greCleanupOrphans ? "true" : "false");
  buf[sizeof(buf) - 1] = '\0';
  UpdateInstallStatusLog(buf);

  _snprintf(buf, sizeof(buf), "    GRE Uninstaller Path: %s\n", greUninstaller);
  buf[sizeof(buf) - 1] = '\0';
  UpdateInstallStatusLog(buf);

  _snprintf(buf, sizeof(buf), "    GRE Uninstaller found: %s\n", FileExists(greUninstaller) ? "true" : "false");
  buf[sizeof(buf) - 1] = '\0';
  UpdateInstallStatusLog(buf);

  // If greCleanupOrphans is false (set either in config.ini or passed in
  // thru the cmdline) or GRE's uninstaller file does not exist, then
  // simply do nothing and return.
  if(!sgProduct.greCleanupOrphans || !FileExists(greUninstaller))
  {
    _snprintf(buf, sizeof(buf), "    Cleanup Orphaned GRE premature return: %d\n", WIZ_OK);
    buf[sizeof(buf) - 1] = '\0';
    UpdateInstallStatusLog(buf);
    return(WIZ_OK);
  }

  // If GRE is installed locally, then use the private GRE key, else
  // use the default global GRE key in the windows registry.
  if(sgProduct.greType == GRE_LOCAL)
    DecryptString(greRegKey, sgProduct.grePrivateKey);
  else
    MozCopyStr(GRE_REG_KEY, greRegKey, sizeof(greRegKey));

  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, greRegKey, 0, KEY_READ, &greIDKeyHandle) != ERROR_SUCCESS)
  {
    _snprintf(buf, sizeof(buf), "    Cleanup Orphaned GRE premature return (RegOpenKeyEx: %s): %d\n", greRegKey, WIZ_ERROR_UNDEFINED);
    buf[sizeof(buf) - 1] = '\0';
    UpdateInstallStatusLog(buf);
    return(WIZ_ERROR_UNDEFINED);
  }

  // Build the list of GRE's given greRegKey.  For example, if greRegKey is:
  //
  //   HKLM\Software\mozilla.org\GRE
  //
  // then build a list of the GRE IDs inside this key.
  totalGreIDSubKeys = 0;
  regRv = RegQueryInfoKey(greIDKeyHandle, NULL, NULL, NULL, &totalGreIDSubKeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  if((regRv != ERROR_SUCCESS) || (totalGreIDSubKeys == 0))
  {
    rv = WIZ_ERROR_UNDEFINED;
    _snprintf(buf, sizeof(buf), "    Cleanup Orphaned GRE premature return (RegQueryInfoKey: all keys): %d\n", rv);
    buf[sizeof(buf) - 1] = '\0';
    UpdateInstallStatusLog(buf);
    return(rv);
  }

  if((rv == WIZ_OK) && (greIDListToClean = NS_GlobalAlloc(sizeof(char *) * totalGreIDSubKeys)) == NULL)
  {
    RegCloseKey(greIDKeyHandle);
    _snprintf(buf, sizeof(buf), "    Cleanup Orphaned GRE premature return.  Failed to allocate memory for greIDListToClean: %d\n", WIZ_OUT_OF_MEMORY);
    buf[sizeof(buf) - 1] = '\0';
    UpdateInstallStatusLog(buf);
    return(WIZ_OUT_OF_MEMORY);
  }

  // Show message that orphaned GREs are being cleaned up
  if(*sgProduct.greCleanupOrphansMessage != '\0');
    ShowMessage(sgProduct.greCleanupOrphansMessage, TRUE);

  if(rv == WIZ_OK)
  {
    // Initialize the array of pointers (of GRE ID keys) to NULL
    for(indexOfGREID = 0; indexOfGREID < totalGreIDSubKeys; indexOfGREID++)
      greIDListToClean[indexOfGREID] = NULL;

    // Enumerate the GRE ID list and save the GRE ID to each of its array element
    for(indexOfGREID = 0; indexOfGREID < totalGreIDSubKeys; indexOfGREID++)
    {
      idKeySize = sizeof(greKeyID);
      if(RegEnumKeyEx(greIDKeyHandle, indexOfGREID, greKeyID, &idKeySize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
      {
        if((greIDListToClean[indexOfGREID] = NS_GlobalAlloc(sizeof(char) * (idKeySize + 1))) == NULL)
        {
          RegCloseKey(greIDKeyHandle);
          rv = WIZ_OUT_OF_MEMORY;
          _snprintf(buf, sizeof(buf), "    Cleanup Orphaned GRE premature return.  Failed to add %s to greIDListToClean[]: %d\n", greKeyID, rv);
          buf[sizeof(buf) - 1] = '\0';
          UpdateInstallStatusLog(buf);
          break;
        }
        MozCopyStr(greKeyID, greIDListToClean[indexOfGREID], idKeySize + 1);
      }
    }
    RegCloseKey(greIDKeyHandle);

    if(rv == WIZ_OK)
    {
      // Enumerate the GRE ID list built from above to get to each of it's AppList key.
      // The list that we need to build now is from the following key:
      //
      //   HKLM\Software\mozilla.org\GRE\[GRE ID]\AppList
      for(indexOfGREID = 0; indexOfGREID < totalGreIDSubKeys; indexOfGREID++)
      {
        // If we find the same GRE version as what we're trying to install,
        // then we don't want to process it because if we do, it will
        // uninstall it.  Which means that it'll reinstall it again.  We don't
        // want to have reinstall when we don't need to.
        //
        // szUserAgent is the GRE unique id if this instance of the installer is
        // installing GRE.
        if(strcmpi(sgProduct.szUserAgent, greIDListToClean[indexOfGREID]) == 0)
          continue;

        _snprintf(greAppListKeyPath, sizeof(greAppListKeyPath), "%s\\%s\\AppList",
            GRE_REG_KEY, greIDListToClean[indexOfGREID]);
        greAppListKeyPath[sizeof(greAppListKeyPath) - 1] = '\0';
        if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, greAppListKeyPath, 0, KEY_READ, &greAppListKeyHandle) != ERROR_SUCCESS)
        {
          rv = WIZ_ERROR_UNDEFINED;
          _snprintf(buf, sizeof(buf), "    Cleanup Orphaned GRE premature return.  Failed to open key %s: %d\n", greAppListKeyPath, rv);
          buf[sizeof(buf) - 1] = '\0';
          UpdateInstallStatusLog(buf);
          break;
        }

        totalGREAppListSubKeys = 0;
        if(RegQueryInfoKey(greAppListKeyHandle, NULL, NULL, NULL, &totalGREAppListSubKeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
        {
          RegCloseKey(greAppListKeyHandle);
          rv = WIZ_ERROR_UNDEFINED;
          _snprintf(buf, sizeof(buf), "    Cleanup Orphaned GRE premature return.  Failed to regquery for all keys in %s: %d\n", greAppListKeyPath, rv);
          buf[sizeof(buf) - 1] = '\0';
          UpdateInstallStatusLog(buf);
          break;
        }
    
        if((rv == WIZ_OK) && (greAppListToClean = NS_GlobalAlloc(sizeof(char *) * totalGREAppListSubKeys)) == NULL)
        {
          RegCloseKey(greAppListKeyHandle);
          rv = WIZ_OUT_OF_MEMORY;
          _snprintf(buf, sizeof(buf), "    Cleanup Orphaned GRE premature return.  Failed to allocate memory for greAppListToClean: %d\n", rv);
          buf[sizeof(buf) - 1] = '\0';
          UpdateInstallStatusLog(buf);
          break;
        }

        // Initialize the GREAppList elements to NULL.
        for(indexOfGREAppList = 0; indexOfGREAppList < totalGREAppListSubKeys; indexOfGREAppList++)
          greAppListToClean[indexOfGREAppList] = NULL;

        // enumerate the GRE AppList key and build a list.
        for(indexOfGREAppList = 0; indexOfGREAppList < totalGREAppListSubKeys; indexOfGREAppList++)
        {
          appListKeySize = sizeof(greKeyAppList);
          if(RegEnumKeyEx(greAppListKeyHandle, indexOfGREAppList, greKeyAppList, &appListKeySize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
          {
            if((greAppListToClean[indexOfGREAppList] = NS_GlobalAlloc(sizeof(char) * (appListKeySize + 1))) == NULL)
            {
              RegCloseKey(greAppListKeyHandle);
              rv = WIZ_OUT_OF_MEMORY;
              _snprintf(buf, sizeof(buf), "    Cleanup Orphaned GRE premature return.  Failed to add %s to greAppListToClean[]: %d\n", greKeyAppList, rv);
              buf[sizeof(buf) - 1] = '\0';
              UpdateInstallStatusLog(buf);
              break;
            }
            MozCopyStr(greKeyAppList, greAppListToClean[indexOfGREAppList], appListKeySize + 1);
          }
        }
        RegCloseKey(greAppListKeyHandle);

        if(rv != WIZ_OK)
          break;

        UpdateInstallStatusLog("\n    Cleanup Orphaned GRE's uninstall commandline:\n");
        // Enumerate the saved GREAppList and start calling GREUninstall.exe
        // to remove them if appropriate.
        // GREUninstall.exe will take care of determining if the particular
        // GRE should be fully removed or not.
        // We need to enumerate the list again instead of doing this in the
        // save loop as above because deleting keys while at the same time
        // enumerating thru its parent key is a Bad Thing(TM).
        for(indexOfGREAppList = 0; indexOfGREAppList < totalGREAppListSubKeys; indexOfGREAppList++)
        {
          char programNamePath[MAX_BUF];
          char greUninstallParam[MAX_BUF];

          DecryptString(programNamePath, sgProduct.szAppPath);
          _snprintf(greUninstallParam, sizeof(greUninstallParam), "-mmi -ms -ua \"%s\" -app \"%s\" -app_path \"%s\"",
              greIDListToClean[indexOfGREID], greAppListToClean[indexOfGREAppList], programNamePath);
          greUninstallParam[sizeof(greUninstallParam) - 1] = '\0';
          UpdateInstallStatusLog("        ");
          UpdateInstallStatusLog(greUninstallParam);
          UpdateInstallStatusLog("\n");
          WinSpawn(greUninstaller, greUninstallParam, szTempDir, SW_SHOWNORMAL, WS_WAIT);
        }

        // Cleanup allocated memory
        for(indexOfGREAppList = 0; indexOfGREAppList < totalGREAppListSubKeys; indexOfGREAppList++)
          FreeMemory(&greAppListToClean[indexOfGREAppList]);
        if(greAppListToClean)
          GlobalFree(greAppListToClean);
      }
    }
  }

  //
  // Cleanup allocated memory
  CleanupArrayList(greIDListToClean, totalGreIDSubKeys);
  if(greIDListToClean)
    GlobalFree(greIDListToClean);

  // Hide message that orphaned GREs are being cleaned up
  if(*sgProduct.greCleanupOrphansMessage != '\0');
    ShowMessage(sgProduct.greCleanupOrphansMessage, FALSE);

  return(rv);
}

void LaunchOneComponent(siC *siCObject, greInfo *aGre)
{
  BOOL      bArchiveFound;
  char      szArchive[MAX_BUF];
  char      szMsg[MAX_BUF];

  if(!siCObject)
    /* nothing to do return */
    return;

  GetPrivateProfileString("Messages", "MSG_CONFIGURING", "", szMsg, sizeof(szMsg), szFileIniInstall);

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

      *szMessageString = '\0';
      if(*szMsg != '\0')
      {
        wsprintf(szMessageString, szMsg, siCObject->szDescriptionShort);
        ShowMessage(szMessageString, TRUE);
      }

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
          if(*szMessageString != '\0')
            ShowMessage(szMessageString, FALSE);
          return;
        }
      }

      if(aGre)
      {
        GetInfoFromGreInstaller(szSpawnFile, aGre);
        UpdateGreInstallerCmdLine(aGre, szParameterBuf, sizeof(szParameterBuf), FOR_NEW_GRE);
      }

      LogISLaunchAppsComponent(siCObject->szDescriptionShort);
      WinSpawn(szSpawnFile, szParameterBuf, szTempDir, SW_SHOWNORMAL, WS_WAIT);

      if(siCObject->dwAttributes & SIC_UNCOMPRESS)
        FileDelete(szSpawnFile);

      if(*szMessageString != '\0')
        ShowMessage(szMessageString, FALSE);
    }
  }
}

void LaunchExistingGreInstaller(greInfo *aGre)
{
  char      szMsg[MAX_BUF];
  char      szParameterBuf[MAX_BUF];
  char      szMessageString[MAX_BUF];
  siC       *siCObject = aGre->siCGreComponent;

  if((!siCObject) || !FileExists(aGre->installerAppPath))
    /* nothing to do return */
    return;

  GetPrivateProfileString("Messages", "MSG_CONFIGURING", "", szMsg, sizeof(szMsg), szFileIniInstall);

  *szMessageString = '\0';
  if(*szMsg != '\0')
  {
    wsprintf(szMessageString, szMsg, siCObject->szDescriptionShort);
    ShowMessage(szMessageString, TRUE);
  }

  DecryptString(szParameterBuf, siCObject->szParameter);
  UpdateGreInstallerCmdLine(NULL, szParameterBuf, sizeof(szParameterBuf), FOR_EXISTING_GRE);
  LogISLaunchAppsComponent(siCObject->szDescriptionShort);
  WinSpawn(aGre->installerAppPath, szParameterBuf, szTempDir, SW_SHOWNORMAL, WS_WAIT);
  if(*szMessageString != '\0')
    ShowMessage(szMessageString, FALSE);
}

HRESULT LaunchApps()
{
  DWORD     dwIndex0;
  siC       *siCObject = NULL;

  LogISLaunchApps(W_START);
  dwIndex0 = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    /* launch 3rd party executable */
    LaunchOneComponent(siCObject, NULL);
    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  LogISLaunchApps(W_END);
  return(0);
}

/* Function: IsPathWithinWindir()
 *       in: char *aTargetPath
 *      out: returns a BOOL type indicating whether the install path chosen
 *           by the user is within the %windir% or not.
 *  purpose: To see if aTargetPath is within the %windir% path.
 */
BOOL IsPathWithinWindir(char *aTargetPath)
{
  char windir[MAX_PATH];
  char targetPath[MAX_PATH];

  assert(aTargetPath);

  if(GetWindowsDirectory(windir, sizeof(windir)))
  {
    MozCopyStr(aTargetPath, targetPath, sizeof(targetPath));
    AppendBackSlash(targetPath, sizeof targetPath);
    CharUpperBuff(targetPath, sizeof(targetPath));
    AppendBackSlash(windir, sizeof(windir));
    CharUpperBuff(windir, sizeof(windir));
    if(strstr(targetPath, windir) == targetPath)
      return(TRUE);
  }
  else
  {
    /* If we can't get the windows path, just show error message and assume
     * the install path is not within the windows dir. */
    char szEGetWindirFailed[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_GET_WINDOWS_DIRECTORY_FAILED", "", szEGetWindirFailed, sizeof(szEGetWindirFailed), szFileIniInstall))
      PrintError(szEGetWindirFailed, ERROR_CODE_SHOW);
  }
  return(FALSE);
}

/* Function: CleanupArrayList()
 *       in: char **aList - array to cleanup
 *           int  aListLength - length of array
 *      out: none.
 *  purpose: Cleans up memory.
 */
void CleanupArrayList(char **aList, int aListLength)
{
  int index = 0;

  // Free allocated memory
  for(index = 0; index < aListLength; index++)
    FreeMemory(&aList[index]);
}

/* Function: GetTotalNumKeys()
 *       in: char *aSectionName - name of section to get info on
 *           char *aBaseKeyName - base name of key to count
 *      out: none.
 *  purpose: To get the total number of 'aBaseKeyName's in aSection.
 *           A base key name is just the name of the key without the
 *           numbers, ie:
 *             base key name          : Object
 *             actual key in .ini file: Object0, Object1, etc...
 */
int GetTotalNumKeys(char *aSectionName, char *aBaseKeyName)
{
  int index = 0;
  char keyName[MAX_BUF_TINY];
  char buf[MAX_BUF];

  assert(aSectionName);
  assert(aBaseKeyName);

  _snprintf(keyName, sizeof(keyName), "%s%d", aBaseKeyName, index);
  keyName[sizeof(keyName) - 1] = '\0';
  GetPrivateProfileString(aSectionName, keyName, "", buf, sizeof(buf), szFileIniConfig);
  while(*buf != '\0')
  {
    ++index;
    _snprintf(keyName, sizeof(keyName), "%s%d", aBaseKeyName, index);
    keyName[sizeof(keyName) - 1] = '\0';
    GetPrivateProfileString(aSectionName, keyName, "", buf, sizeof(buf), szFileIniConfig);
  }

  return(index);
}

/* Function: BuildArrayList()
 *       in: char *aSectionName - section name to look for info to build array
 *           char *aBaseKeyName - base key name to use to build array from
 *      out: returns char ** - array list built
 *           int *aArrayLength - length of array built
 *  purpose: To build an Array list given the aSectionName and aBaseKeyName.
 *           Caller is responsible for cleaning up of allocated memory done
 *           in this function!
 */
char **BuildArrayList(char *aSectionName, char *aBaseKeyName, int *aArrayLength)
{
  int index = 0;
  int totalKeys = 0;
  char **list;
  char keyName[MAX_BUF_TINY];
  char buf[MAX_BUF];

  *aArrayLength = 0;
  totalKeys = GetTotalNumKeys(aSectionName, aBaseKeyName);

  // if totalKeys is <= 0, then there's nothing to do.
  if(totalKeys <= 0)
    return(NULL);

  if((list = NS_GlobalAlloc(sizeof(char *) * totalKeys)) == NULL)
    return(NULL);

  // Create and initialize the array of pointers
  for(index = 0; index < totalKeys; index++)
  {
    // build the actual key name to use.
    _snprintf(keyName, sizeof(keyName), "%s%d", aBaseKeyName, index);
    keyName[sizeof(keyName) - 1] = '\0';
    GetPrivateProfileString(aSectionName, keyName, "", buf, sizeof(buf), szFileIniConfig);
    if(*buf != '\0')
    {
      if((list[index] = NS_GlobalAlloc(sizeof(char) * (lstrlen(buf) + 1))) == NULL)
      {
        // couldn't allocate memory for an array, cleanup the array list that
        // has been currently built, and return NULL.
        CleanupArrayList(list, index);
        if(list)
          GlobalFree(list);

        return(NULL);
      }
      MozCopyStr(buf, list[index], lstrlen(buf) + 1);
    }
    else
      list[index] = NULL;
  }
  *aArrayLength = index;

  // caller is responsible for garbage cleanup of list.
  return(list);
}

/* Function: IsDirAProfileDir()
 *       in: char *aParentPath - path to start the check.
 *           char **aListProfileObjects - array list of profile files/dirs to
 *                  use to determine if a dir is a profile dir or not.
 *           int  aListLength
 *      out: BOOL
 *  purpose: To check to see if a dir is a profile dir or not.  If any of it's
 *           subdirectories is a profile dir, then the initial aParentPath is
 *           considered to be a profile dir.
 */
BOOL IsDirAProfileDir(char *aParentPath, char **aListProfileObjects, int aListLength)
{
  HANDLE          fileHandle;
  WIN32_FIND_DATA fdFile;
  char            destPathTemp[MAX_BUF];
  BOOL            found;
  BOOL            isProfileDir;
  int             index;

  if((aListLength <= 0) || !aListProfileObjects || !*aListProfileObjects)
    return(FALSE);

  /* check the initial aParentPath to see if the dir contains the
   * files/dirs that contitute a profile dir */
  isProfileDir = TRUE;
  for(index = 0; index < aListLength; index++)
  {
    /* create full path */
    _snprintf(destPathTemp, sizeof(destPathTemp), "%s\\%s", aParentPath, aListProfileObjects[index]);
    destPathTemp[sizeof(destPathTemp) - 1] = '\0';

    /* if any of the files/dirs that make up a profile dir is missing,
     * then it's not a profile dir */
    if(!FileExists(destPathTemp))
    {
      isProfileDir = FALSE;
      break;
    }
  }

  /* If it's determined to be a profile dir, then return immediately.  If it's
   * not, then continue searching the other dirs to see if any constitute a
   * profile dir. */
  if(isProfileDir)
    return(TRUE);

  _snprintf(destPathTemp, sizeof(destPathTemp), "%s\\*", aParentPath);
  destPathTemp[sizeof(destPathTemp) - 1] = '\0';

  found = TRUE;
  fileHandle = FindFirstFile(destPathTemp, &fdFile);
  while((fileHandle != INVALID_HANDLE_VALUE) && found)
  {
    if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
    {
      // if it's a directory, there we need to traverse it to see if there are
      // dirs that are profile dirs.
      if(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
        _snprintf(destPathTemp, sizeof(destPathTemp), "%s\\%s", aParentPath, fdFile.cFileName);
        destPathTemp[sizeof(destPathTemp) - 1] = '\0';
        isProfileDir = IsDirAProfileDir(destPathTemp, aListProfileObjects, aListLength);
        if(isProfileDir)
          break;
          
      }
    }
    found = FindNextFile(fileHandle, &fdFile);
  }

  FindClose(fileHandle);
  return(isProfileDir);
}

/* Function: IsOkToRemoveFileOrDir()
 *       in: char *aFileOrDirname
 *      out: bool return type
 *  purpose: To check if the file or dirname is not in the aListToIgnore.
 */
BOOL IsOkToRemoveFileOrDirname(char *aFileOrDirname, char **aListToIgnore,
    int aListToIgnoreLength, char **aListProfileObjects, int aListProfileLength)
{
  int  i;
  char filename[MAX_BUF];

  if(!aListToIgnore || !*aListToIgnore)
    return(TRUE);

  ParsePath(aFileOrDirname, filename, sizeof(filename), FALSE, PP_FILENAME_ONLY);

  // check to see if this is a profile folder
  if(FileExists(aFileOrDirname) & FILE_ATTRIBUTE_DIRECTORY)
    if(IsDirAProfileDir(aFileOrDirname, aListProfileObjects, aListProfileLength))
      return(FALSE);

  for(i = 0; i < aListToIgnoreLength; i++)
  {
    if(lstrcmpi(filename, aListToIgnore[i]) == 0)
      return(FALSE);
  }
  return(TRUE);
}

/* Function: CleanupOnUpgrade()
 *       in: none.
 *      out: none.
 *  purpose: To cleanup/remove files and dirs within the user chosen
 *           installation path, excluding the list in
 *           aListToIgnore.
 */
void CleanupOnUpgrade()
{
  HANDLE          fileHandle;
  WIN32_FIND_DATA fdFile;
  char            destPathTemp[MAX_BUF];
  char            targetPath[MAX_BUF];
  char            buf[MAX_BUF];
  BOOL            found;
  char            **listObjectsToIgnore;
  int             listTotalObjectsToIgnore;
  char            **listProfileObjects;
  int             listTotalProfileObjects;
  int             index;

  MozCopyStr(sgProduct.szPath, targetPath, sizeof(targetPath));
  RemoveBackSlash(targetPath);

  if(!FileExists(targetPath))
    return;

  UpdateInstallStatusLog("\n    Files/Dirs deleted on upgrade:\n");

  /* Check to see if the installation path is within the %windir%.  If so,
   * warn the user and do not delete any file! */
  if(IsPathWithinWindir(targetPath))
  {
    if(sgProduct.mode == NORMAL)
    {
      GetPrivateProfileString("Strings", "Message Cleanup On Upgrade Windir", "",
          buf, sizeof(buf), szFileIniConfig);
      MessageBox(hWndMain, buf, NULL, MB_ICONEXCLAMATION);
    }

    _snprintf(buf, sizeof(buf), "        None.  Installation path is within %windir%:\n            %s\n", targetPath);
    buf[sizeof(buf) - 1] = '\0';
    UpdateInstallStatusLog(buf);
    return;
  }

  // param1: section name
  // param2: base key name within section name
  //         ie: ObjectToIgnore0=
  //             ObjectToIgnore1=
  //             ObjectToIgnore2=
  listObjectsToIgnore = BuildArrayList("Cleanup On Upgrade", "ObjectToIgnore", &listTotalObjectsToIgnore);
  listProfileObjects  = BuildArrayList("Profile Dir Object List", "Object", &listTotalProfileObjects);

  _snprintf(destPathTemp, sizeof(destPathTemp), "%s\\*", targetPath);
  destPathTemp[sizeof(destPathTemp) - 1] = '\0';

  found = TRUE;
  fileHandle = FindFirstFile(destPathTemp, &fdFile);
  while((fileHandle != INVALID_HANDLE_VALUE) && found)
  {
    if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
    {
      /* create full path */
      _snprintf(destPathTemp, sizeof(destPathTemp), "%s\\%s", targetPath, fdFile.cFileName);
      destPathTemp[sizeof(destPathTemp) - 1] = '\0';

      if(IsOkToRemoveFileOrDirname(destPathTemp, listObjectsToIgnore,
            listTotalObjectsToIgnore, listProfileObjects, listTotalProfileObjects))
      {
        if(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
          AppendBackSlash(destPathTemp, sizeof(destPathTemp));
          DirectoryRemove(destPathTemp, TRUE);
        }
        else
          DeleteFile(destPathTemp);

        /* Check to see if the file/dir was deleted successfully */
        if(!FileExists(destPathTemp))
          _snprintf(buf, sizeof(buf), "        ok: %s\n", destPathTemp);
        else
          _snprintf(buf, sizeof(buf), "    failed: %s\n", destPathTemp);
      }
      else
        _snprintf(buf, sizeof(buf), "   skipped: %s\n", destPathTemp);

      buf[sizeof(buf) - 1] = '\0';
      UpdateInstallStatusLog(buf);
    }

    found = FindNextFile(fileHandle, &fdFile);
  }

  FindClose(fileHandle);

  // Log the array lists to make sure it matches the list of files/dirs that
  // were processed.
  UpdateInstallStatusLog("\n    Files/Dirs list to ignore:\n");
  for(index = 0; index < listTotalObjectsToIgnore; index++)
  {
    _snprintf(buf, sizeof(buf), "        ObjectToIgnore%d: %s\n", index + 1, listObjectsToIgnore[index]);
    buf[sizeof(buf) - 1] = '\0';
    UpdateInstallStatusLog(buf);
  }

  CleanupArrayList(listObjectsToIgnore, listTotalObjectsToIgnore);
  if(listObjectsToIgnore)
    GlobalFree(listObjectsToIgnore);

  UpdateInstallStatusLog("\n    Files/Dirs list to verify dir is a Profile dir:\n");
  for(index = 0; index < listTotalProfileObjects; index++)
  {
    _snprintf(buf, sizeof(buf), "        Object%d: %s\n", index + 1, listProfileObjects[index]);
    buf[sizeof(buf) - 1] = '\0';
    UpdateInstallStatusLog(buf);
  }

  CleanupArrayList(listProfileObjects, listTotalProfileObjects);
  if(listProfileObjects)
    GlobalFree(listProfileObjects);

}

/* Function: ProcessGre()
 *       in: none.
 *      out: path to where GRE is installed at.
 *  purpose: To install GRE on the system, so this installer can use it's
 *           xpinstall engine.  It uses gre->homePath to see if GRE is already
 *           installed on the system.  If GRE is already present on the system,
 *           gre->homePath is guaranteed to contain it's destination path.
 */
HRESULT ProcessGre(greInfo *aGre)
{
  char greUninstallCommand[MAX_BUF];

  if(!aGre)
    return(WIZ_OK);

  /* If aGre->homePath does not exist, it means that a compatible version of GRE was
   * not found on the system, so we need to install one.  We also need to get the
   * path to where it's going to install the GRE for in case we need to use it
   * as the xpinstall engine.
   *
   * If gbForceInstallGre is true, then force an installation of GRE regardless
   * if it's already on the system.  Also save the GRE's destination path to
   * aGre->homePath.
   *
   * If aGre->homePath does exist, then we simply call LaunchExistingGreInstaller()
   * to run the existing GRE's installer app to register mozilla.
   */
  UpdateInstallStatusLog("\n");
  if((*aGre->homePath == '\0') || (gbForceInstallGre))
    LaunchOneComponent(aGre->siCGreComponent, aGre);
  else
    LaunchExistingGreInstaller(aGre);

  /* XXX instead of unsetting the SELECTED attribute, set the DOWNLOAD_ONLY attribute
   * in the config.ini file.
   */
  /* Unset "Component GRE"'s SELECTED attribute so it doesn't get spawned again later from LaunchApps() */
  gGreInstallerHasRun = TRUE;
  if(aGre->siCGreComponent)
    aGre->siCGreComponent->dwAttributes &= ~SIC_SELECTED;

  /* Log the GRE uninstall command to call in order for this app's uninstaller to
   * uninstall GRE.
   */
  if(GetInfoFromInstalledGreConfigIni(aGre) == WIZ_OK)
  {
    /* If were installing GRE locally, then update the app's uninstall command
     * to pass the local/private windows GRE key path to GRE's uninstaller
     * during uninstall. */
    if(sgProduct.greType == GRE_LOCAL)
      wsprintf(greUninstallCommand,
               "\"%s\" -mmi -ms -app \"%s %s\" -ua \"%s\" -reg_path %s",
               aGre->uninstallerAppPath,
               sgProduct.szProductNameInternal,
               sgProduct.szUserAgent,
               aGre->userAgent,
               sgProduct.grePrivateKey);
    else
      wsprintf(greUninstallCommand,
               "\"%s\" -mmi -ms -app \"%s %s\" -ua \"%s\"",
               aGre->uninstallerAppPath,
               sgProduct.szProductNameInternal,
               sgProduct.szUserAgent,
               aGre->userAgent);
    UpdateInstallLog(KEY_UNINSTALL_COMMAND, greUninstallCommand, DNU_UNINSTALL);
  }
  return(WIZ_OK);
}

/* Function: GetXpinstallPath()
 *       in: none.
 *      out: path to where xpinstall engine is at.
 *  purpose: To retrieve the full path of where a valid xpinstall engine is
 *           installed at.
 *           * If we're using xpcom.xpi, then append 'bin' to the returned
 *             path because when xpcom.xpi is uncompressed, xpinstall will be
 *             within a 'bin' subdir.
 *           * If we're using GRE, then simply just return the GRE
 *             destination path.
 */
void GetXpinstallPath(char *aPath, int aPathBufSize)
{
  if(siCFXpcomFile.bStatus == STATUS_ENABLED)
  {
    MozCopyStr(siCFXpcomFile.szDestination, aPath, aPathBufSize);
    AppendBackSlash(aPath, aPathBufSize);
    lstrcat(aPath, "bin");
  }
  else
    MozCopyStr(gGre.homePath, aPath, aPathBufSize);
}

/* Function: ProcessXpinstallEngine()
 *       in: none.
 *      out: none.
 *  purpose: To setup the xpinstall engine either by:
 *             1) uncompressing xpcom.xpi into the temp dir.
 *             2) locating existing compatible version of GRE.
 *             3) installing GRE that was downloaded with this product.
 *
 *           Since GRE installer uses the same native installer as the Mozilla
 *           installer, xpcom.xpi should be used only by the GRE installer.
 *           All other installers should be using GRE (either finding one on
 *           the local system, or installer its own copy).
 */
HRESULT ProcessXpinstallEngine()
{
  HRESULT rv = WIZ_OK;

  /* If product to be installed is _not_ GRE, then call ProcessGRE.
   * ProcessGre() will either install GRE or simply run the existing
   * GRE's installer that's already on the system.  This will setup
   * GRE so it can be used as the xpinstall engine (if it's needed).
   */
  if(!IsInstallerProductGRE())
    rv = ProcessGre(&gGre);

  if(*siCFXpcomFile.szMessage != '\0')
    ShowMessage(siCFXpcomFile.szMessage, TRUE);

  if((WIZ_OK == rv) && (siCFXpcomFile.bStatus == STATUS_ENABLED))
    rv = ProcessXpcomFile();

  if(*siCFXpcomFile.szMessage != '\0')
    ShowMessage(siCFXpcomFile.szMessage, FALSE);

  return(rv);
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

HRESULT InitDlgAdditionalOptions(diDO *diDialog)
{
  diDialog->bShowDialog           = FALSE;
  diDialog->bSaveInstaller        = FALSE;
  diDialog->bRecaptureHomepage    = FALSE;
  diDialog->bShowHomepageOption   = FALSE;
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

void DeInitDlgAdditionalOptions(diDO *diDialog)
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
  gSystemInfo.bRefreshIcons      = FALSE; 
  sgProduct.mode                 = NOT_SET;
  sgProduct.bSharedInst          = FALSE;
  sgProduct.bInstallFiles        = TRUE;
  sgProduct.checkCleanupOnUpgrade = FALSE;
  sgProduct.doCleanupOnUpgrade    = FALSE;
  sgProduct.greType              = GRE_TYPE_NOT_SET;
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
  if((sgProduct.szProductNameInternal         = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProductNamePrevious         = NS_GlobalAlloc(MAX_BUF)) == NULL)
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
  if((sgProduct.szAppID                       = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szAppPath                     = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szRegPath                     = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  sgProduct.greCleanupOrphans = FALSE;
  *sgProduct.greCleanupOrphansMessage = '\0';
  *sgProduct.greID         = '\0';
  *sgProduct.grePrivateKey = '\0';
  return(0);
}

void DeInitSetupGeneral()
{
  FreeMemory(&(sgProduct.szPath));
  FreeMemory(&(sgProduct.szSubPath));
  FreeMemory(&(sgProduct.szProgramName));
  FreeMemory(&(sgProduct.szCompanyName));
  FreeMemory(&(sgProduct.szProductName));
  FreeMemory(&(sgProduct.szProductNameInternal));
  FreeMemory(&(sgProduct.szProductNamePrevious));
  FreeMemory(&(sgProduct.szUninstallFilename));
  FreeMemory(&(sgProduct.szUserAgent));
  FreeMemory(&(sgProduct.szProgramFolderName));
  FreeMemory(&(sgProduct.szProgramFolderPath));
  FreeMemory(&(sgProduct.szAlternateArchiveSearchPath));
  FreeMemory(&(sgProduct.szParentProcessFilename));
  FreeMemory(&(sgProduct.szAppID));
  FreeMemory(&(sgProduct.szAppPath));
  FreeMemory(&(sgProduct.szRegPath));
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
  siCFXpcomFile.bStatus          = STATUS_ENABLED;
  siCFXpcomFile.ullInstallSize   = 0;
  return(0);
}

void DeInitSXpcomFile()
{
  FreeMemory(&(siCFXpcomFile.szSource));
  FreeMemory(&(siCFXpcomFile.szDestination));
  FreeMemory(&(siCFXpcomFile.szMessage));
}

/* Function: InitGre()
 *       in: gre structure.
 *      out: gre structure.
 *  purpose: To initialize a newly created greInfo structure.
 */
HRESULT InitGre(greInfo *gre)
{
  gre->homePath[0]           = '\0';
  gre->installerAppPath[0]   = '\0';
  gre->uninstallerAppPath[0] = '\0';
  gre->userAgent[0]          = '\0';
  gre->minVersion.ullMajor   = 0;
  gre->minVersion.ullMinor   = 0;
  gre->minVersion.ullRelease = 0;
  gre->minVersion.ullBuild   = 0;
  gre->maxVersion.ullMajor   = 0;
  gre->maxVersion.ullMinor   = 0;
  gre->maxVersion.ullRelease = 0;
  gre->maxVersion.ullBuild   = 0;
  gre->greSupersedeList      = NULL;
  gre->greInstalledList      = NULL;
  gre->siCGreComponent       = NULL;
  return(WIZ_OK);
}

/* Function: DeInitGre()
 *       in: gre structure.
 *      out: gre structure.
 *  purpose: To cleanup allocated memory to a greInfo structure.
 */
void DeInitGre(greInfo *gre)
{
  DeleteGverList(gre->greSupersedeList);
  DeleteGverList(gre->greInstalledList);
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

void SiCNodeSetAttributes(DWORD dwIndex, DWORD dwAttributes, BOOL bSet, BOOL bIncludeInvisible, DWORD dwACFlag, HWND hwndListBox)
{
  DWORD dwCount        = 0;
  DWORD dwVisibleIndex = 0;
  siC   *siCTemp       = siComponents;

  LPSTR szTmpString;
  TCHAR tchBuffer[MAX_BUF];

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

        if((!(siCTemp->dwAttributes & SIC_INVISIBLE))
           && (dwAttributes == SIC_SELECTED) 
           && gSystemInfo.bScreenReader 
           && hwndListBox)
        {
          szTmpString = SiCNodeGetDescriptionShort(dwVisibleIndex, FALSE, dwACFlag);
          lstrcpy(tchBuffer, szTmpString);
          lstrcat(tchBuffer, " - ");
          if(bSet)
            lstrcat(tchBuffer, sgInstallGui.szChecked);
          else
            lstrcat(tchBuffer, sgInstallGui.szUnchecked);

          //We've got to actually change the text in the listbox for the screen reader to see it.
          SendMessage(hwndListBox, LB_INSERTSTRING, 0, (LPARAM)tchBuffer);
          SendMessage(hwndListBox, LB_DELETESTRING, 1, 0);
        }
      }

      ++dwCount;
      if(!(siCTemp->dwAttributes & SIC_INVISIBLE))
        ++dwVisibleIndex;
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

          if((!(siCTemp->dwAttributes & SIC_INVISIBLE))
             && (dwAttributes == SIC_SELECTED) 
             && gSystemInfo.bScreenReader 
             && hwndListBox)
          {
            szTmpString = SiCNodeGetDescriptionShort(dwVisibleIndex, FALSE, dwACFlag);
            lstrcpy(tchBuffer, szTmpString);
            lstrcat(tchBuffer, " - ");
            if(bSet)
              lstrcat(tchBuffer, sgInstallGui.szChecked);
            else
              lstrcat(tchBuffer, sgInstallGui.szUnchecked);

            //We've got to actually change the text in the listbox for the screen reader to see it.
            SendMessage(hwndListBox, LB_INSERTSTRING, dwVisibleIndex, (LPARAM)tchBuffer);
            SendMessage(hwndListBox, LB_DELETESTRING, dwVisibleIndex+1, 0);
          }
        }

        ++dwCount;
        if(!(siCTemp->dwAttributes & SIC_INVISIBLE))
          ++dwVisibleIndex;
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

void RestoreInvisibleFlag(siC *siCNode)
{
  char szBuf[MAX_BUF_TINY];
  char szAttribute[MAX_BUF_TINY];

  GetPrivateProfileString(siCNode->szReferenceName, "Attributes", "", szBuf, sizeof(szBuf), szFileIniConfig);
  lstrcpy(szAttribute, szBuf);
  CharUpperBuff(szAttribute, sizeof(szAttribute));

  if(strstr(szAttribute, "INVISIBLE") || siCNode->bSupersede)
    siCNode->dwAttributes |= SIC_INVISIBLE;
  else
    siCNode->dwAttributes &= ~SIC_INVISIBLE;
}

void RestoreAdditionalFlag(siC *siCNode)
{
  char szBuf[MAX_BUF_TINY];
  char szAttribute[MAX_BUF_TINY];

  GetPrivateProfileString(siCNode->szReferenceName, "Attributes", "", szBuf, sizeof(szBuf), szFileIniConfig);
  lstrcpy(szAttribute, szBuf);
  CharUpperBuff(szAttribute, sizeof(szAttribute));

  if(strstr(szAttribute, "ADDITIONAL") && !strstr(szAttribute, "NOTADDITIONAL"))
    siCNode->dwAttributes |= SIC_ADDITIONAL;
  else
    siCNode->dwAttributes &= ~SIC_ADDITIONAL;
}

void RestoreEnabledFlag(siC *siCNode)
{
  char szBuf[MAX_BUF_TINY];
  char szAttribute[MAX_BUF_TINY];

  GetPrivateProfileString(siCNode->szReferenceName, "Attributes", "", szBuf, sizeof(szBuf), szFileIniConfig);
  lstrcpy(szAttribute, szBuf);
  CharUpperBuff(szAttribute, sizeof(szAttribute));

  if(strstr(szAttribute, "ENABLED") && !strstr(szAttribute, "DISABLED"))
    siCNode->dwAttributes |= SIC_DISABLED;
  else
    siCNode->dwAttributes &= ~SIC_DISABLED;
}

//  This function:
//  - Zeros the SELECTED and ADDITIONAL attributes of all components.
//  - Set all attributes as specified for the specific Setup Type
//  - Overrides attributes designated in any [Component XXX-Overwrite-Setup TypeX]
//    sections for the specific Setup Type.
void SiCNodeSetItemsSelected(DWORD dwSetupType)
{
  siC  *siCNode;
  char szBuf[MAX_BUF];
  char szSTSection[MAX_BUF];
  char szComponentKey[MAX_BUF];
  char szComponentSection[MAX_BUF];
  char szOverrideSection[MAX_BUF];
  char szOverrideAttributes[MAX_BUF];
  DWORD dwIndex0;

  lstrcpy(szSTSection, "Setup Type");
  itoa(dwSetupType, szBuf, 10);
  lstrcat(szSTSection, szBuf);

  // For each component in the global list unset its SELECTED and ADDITIONAL attributes
  siCNode = siComponents;
  do
  {
    if(siCNode == NULL)
      break;

    /* clear these flags for all components so they can be
     * reset later if they belong to the setup type the user
     * selected */
    siCNode->dwAttributes &= ~SIC_SELECTED;
    siCNode->dwAttributes &= ~SIC_ADDITIONAL;
    siCNode->dwAttributes |= SIC_INVISIBLE;

    /* Force Upgrade needs to be performed here because the user has just
     * selected the destination path for the product.  The destination path is
     * critical to the checking of the Force Upgrade. */
    ResolveForceUpgrade(siCNode);
    ResolveSupersede(siCNode, &gGre);
    siCNode = siCNode->Next;
  } while((siCNode != NULL) && (siCNode != siComponents));

  /* for each component in a setup type, set its ATTRIBUTES to the right values */
  dwIndex0 = 0;
  wsprintf(szComponentKey, "C%d", dwIndex0);
  GetPrivateProfileString(szSTSection, szComponentKey, "", szComponentSection, sizeof(szComponentSection), szFileIniConfig);
  while(*szComponentSection != '\0')
  {
    if((siCNode = SiCNodeFind(siComponents, szComponentSection)) != NULL)
    {
      /* Component is in the Setup Type the user selected, so we now need to
       * reset the INVISIBLE and ADDITIONAL flags because they were unset
       * above and also are not reset anywhere else. */
      RestoreInvisibleFlag(siCNode);
      RestoreAdditionalFlag(siCNode);

      wsprintf(szOverrideSection, "%s-Override-%s", siCNode->szReferenceName, szSTSection);
      GetPrivateProfileString(szOverrideSection, "Attributes", "", szOverrideAttributes, sizeof(szOverrideAttributes), szFileIniConfig);

      if((siCNode->lRandomInstallPercentage != 0) &&
         (siCNode->lRandomInstallPercentage <= siCNode->lRandomInstallValue) &&
         !(siCNode->dwAttributes & SIC_DISABLED))
      {
        /* Random Install Percentage check passed *and* the component
         * is not DISABLED */
        if(*szOverrideAttributes != '\0')
          siCNode->dwAttributes = ParseComponentAttributes(szOverrideAttributes, siCNode->dwAttributes, TRUE);
        siCNode->dwAttributes &= ~SIC_SELECTED;
      }
      else if(sgProduct.dwCustomType != dwSetupType)
      {
        /* Setup Type other than custom detected, so
         * make sure all components from this Setup Type
         * is selected (regardless if it's DISABLED or not). 
         * Don't select components that are Superseded */
        if(!siCNode->bSupersede)
          siCNode->dwAttributes |= SIC_SELECTED;

        /* We need to restore the ENBLED/DISABLED flag for this component
         * from config.ini because it could have been altered in
         * ResolveForceUpgrade().  We need to restore it here so the override
         * attribute can override it here. */
        RestoreEnabledFlag(siCNode);
        if(*szOverrideAttributes != '\0')
          siCNode->dwAttributes = ParseComponentAttributes(szOverrideAttributes, siCNode->dwAttributes, TRUE);
      }
      else if(!(siCNode->dwAttributes & SIC_DISABLED) && !siCNode->bForceUpgrade && !siCNode->bSupersede)
      {
        /* Custom setup type detected and the component is
         * not DISABLED and FORCE_UPGRADE.  Reset the component's 
         * attribute to default.  If the component is DISABLED and
         * happens not be SELECTED in the config.ini file, we leave
         * it as is.  The user will see the component in the Options
         * Dialogs as not selected and DISABLED.  Not sure why we
         * want this, but marketing might find it useful someday. */

        GetPrivateProfileString(siCNode->szReferenceName, "Attributes", "", szBuf, sizeof(szBuf), szFileIniConfig);
        siCNode->dwAttributes = ParseComponentAttributes(szBuf, 0, FALSE);
        if(*szOverrideAttributes != '\0')
          siCNode->dwAttributes = ParseComponentAttributes(szOverrideAttributes, siCNode->dwAttributes, TRUE);
      }
    }

    ++dwIndex0;
    wsprintf(szComponentKey, "C%d", dwIndex0);
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
    if(siCFXpcomFile.bStatus == STATUS_ENABLED)
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

  if(NS_GetDiskFreeSpaceEx != NULL)
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
  else if(NS_GetDiskFreeSpace != NULL)
  {
    ParsePath(szPath, szTempPath, MAX_BUF, FALSE, PP_ROOT_ONLY);
    NS_GetDiskFreeSpace(szTempPath, 
                        &dwSectorsPerCluster,
                        &dwBytesPerSector,
                        &dwNumberOfFreeClusters,
                        &dwTotalNumberOfClusters);
    ullReturn = ((ULONGLONG)dwBytesPerSector * (ULONGLONG)dwSectorsPerCluster * (ULONGLONG)dwNumberOfFreeClusters);
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

  if((sgProduct.mode != SILENT) && (sgProduct.mode != AUTO))
  {
    return(MessageBox(hWndMain, szBufMsg, szDlgDiskSpaceCheckTitle, dwDlgType | MB_ICONEXCLAMATION | MB_DEFBUTTON2 | MB_APPLMODAL | MB_SETFOREGROUND));
  }
  else if(sgProduct.mode == AUTO)
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

/* Function: DetermineGreComponentDestinationPath()
 *
 *       in: char *aInPath - original path to where the 'Component GRE' is to
 *               be installed to.
 *           char *aOutPath - out buffer of what the new destinaton path will
 *               be for 'Component GRE'.  This can be the same as aInPath.
 *
 *  purpose: To figure determine if the original path is writable or not.  If
 *           not, then use following as the new destination path:
 *
 *               [product path]\GRE\[gre id]
 *
 *           This path is guaranteed to work because the user chose it.  There
 *           is logic to make sure we have write access to this new path thru
 *           the Setup Type dialog.
 *           This path is also the same path that will be passed on to the GRE
 *           installer in it's -dd command line parameter.
 */
HRESULT DetermineGreComponentDestinationPath(char *aInPath, char *aOutPath, DWORD aOutPathBufSize)
{
  int  rv = WIZ_OK;
  char inPath[MAX_BUF];

  MozCopyStr(aInPath, inPath, sizeof(inPath));
  AppendBackSlash(inPath, sizeof(inPath));
  if(DirHasWriteAccess(inPath) != WIZ_OK)
  {
    char productPath[MAX_BUF];

    MozCopyStr(sgProduct.szPath, productPath, sizeof(productPath));
    RemoveBackSlash(productPath);

    assert(*sgProduct.greID);
    _snprintf(aOutPath, aOutPathBufSize, "%s\\GRE\\%s", productPath, sgProduct.greID);
    aOutPath[aOutPathBufSize - 1] = '\0';
  }
  else
    MozCopyStr(aInPath, aOutPath, aOutPathBufSize);

  return(rv);
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
      else if((lstrcmpi(siCObject->szReferenceName, "Component GRE") == 0) &&
              !IsInstallerProductGRE())
        /* We found 'Component GRE' and this product is not 'GRE'.  The GRE
         * product happens to also have a 'Component GRE', but we don't
         * care about that one. */
        DetermineGreComponentDestinationPath(siCObject->szDestinationPath, szBuf, sizeof(szBuf));
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
    if(siCFXpcomFile.bStatus == STATUS_ENABLED)
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
  CharUpperBuff(szBuf, sizeof(szBuf));

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

HRESULT ParseComponentAttributes(char *szAttribute, DWORD dwAttributes, BOOL bOverride)
{
  char  szBuf[MAX_BUF];

  lstrcpy(szBuf, szAttribute);
  CharUpperBuff(szBuf, sizeof(szBuf));

  if(bOverride != TRUE)
  {
    if(strstr(szBuf, "LAUNCHAPP"))
      dwAttributes |= SIC_LAUNCHAPP;
    if(strstr(szBuf, "DOWNLOAD_ONLY"))
      dwAttributes |= SIC_DOWNLOAD_ONLY;
    if(strstr(szBuf, "FORCE_UPGRADE"))
      dwAttributes |= SIC_FORCE_UPGRADE;
    if(strstr(szBuf, "IGNORE_DOWNLOAD_ERROR"))
      dwAttributes |= SIC_IGNORE_DOWNLOAD_ERROR;
    if(strstr(szBuf, "IGNORE_XPINSTALL_ERROR"))
      dwAttributes |= SIC_IGNORE_XPINSTALL_ERROR;
    if(strstr(szBuf, "UNCOMPRESS"))
      dwAttributes |= SIC_UNCOMPRESS;
  }

  if(strstr(szBuf, "UNSELECTED"))
    dwAttributes &= ~SIC_SELECTED;
  else if(strstr(szBuf, "SELECTED"))
    dwAttributes |= SIC_SELECTED;

  if(strstr(szBuf, "INVISIBLE"))
    dwAttributes |= SIC_INVISIBLE;
  else if(strstr(szBuf, "VISIBLE"))
    dwAttributes &= ~SIC_INVISIBLE;

  if(strstr(szBuf, "DISABLED"))
    dwAttributes |= SIC_DISABLED;
  if(strstr(szBuf, "ENABLED"))
    dwAttributes &= ~SIC_DISABLED;

  if(strstr(szBuf, "NOTADDITIONAL"))
    dwAttributes &= ~SIC_ADDITIONAL;
  else if(strstr(szBuf, "ADDITIONAL"))
    dwAttributes |= SIC_ADDITIONAL;

  if(strstr(szBuf, "NOTSUPERSEDE"))
    dwAttributes &= ~SIC_SUPERSEDE;
  else if(strstr(szBuf, "SUPERSEDE"))
    dwAttributes |= SIC_SUPERSEDE;
   

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
    if(!sgProduct.doCleanupOnUpgrade)
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
    }

    if(siCObject->bForceUpgrade)
    {
      siCObject->dwAttributes |= SIC_SELECTED;
      siCObject->dwAttributes |= SIC_DISABLED;
    }
    else
      /* Make sure to unset the DISABLED bit.  If the Setup Type is other than
       * Custom, then we don't care if it's DISABLED or not because this flag
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
        siCTemp->dwAttributes = ParseComponentAttributes(szBuf, 0, FALSE);

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
    siCTemp->dwAttributes = ParseComponentAttributes(szBuf, 0, FALSE);
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

BOOL ResolveComponentDependency(siCD *siCDInDependency, HWND hwndListBox)
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
        SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE, TRUE, AC_ALL, hwndListBox);
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
          SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE, TRUE, AC_ALL, hwndListBox);
        }
      }

      siCDepTemp = siCDepTemp->Next;
    }
  }
  return(bMoreToResolve);
}

BOOL ResolveDependencies(DWORD dwIndex, HWND hwndListBox)
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
         bMoreToResolve = ResolveComponentDependency(siCTemp->siCDDependencies, hwndListBox);
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
           bMoreToResolve = ResolveComponentDependency(siCTemp->siCDDependencies, hwndListBox);
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

void ResolveDependees(LPSTR szToggledReferenceName, HWND hwndListBox)
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
            SiCNodeSetAttributes(dwIndex, SIC_SELECTED, FALSE, TRUE, AC_ALL, hwndListBox);
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
            SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE, TRUE, AC_ALL, hwndListBox);
            bMoreToResolve = TRUE;
          }
        }
      }
    }

    siCTemp = siCTemp->Next;
  } while((siCTemp != NULL) && (siCTemp != siComponents));

  if(bMoreToResolve == TRUE)
    ResolveDependees(szToggledReferenceName, hwndListBox);
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

void PrintUsage(void)
{
  char szBuf[MAX_BUF];
  char szUsageMsg[MAX_BUF];
  char szProcessFilename[MAX_BUF];

  /* -h: this help
   * -a [path]: Alternate archive search path
   * -app: ID of application which is launching the installer  (Shared installs)
   * -app_path: Points to representative file of app (Shared installs)
   * -dd [path]: Suggested install destination directory. (Shared installs)
   * -greLocal: Forces GRE to be installed into the application dir.
   * -greShared: Forces GRE to be installed into a global, shared dir (normally
   *    c:\program files\common files\mozilla.org\GRE
   * -reg_path [path]: Where to make entries in the Windows registry. (Shared installs)
   * -f: Force install of GRE installer (Shared installs), though it'll work
   *    for non GRE installers too.
   * -greForce: Force 'Component GRE' to be downloaded, run, and installed.  This
   *    bypasses GRE's logic of determining when to install by running its
   *    installer with a '-f' flag.
   * -n [filename]: setup's parent's process filename
   * -ma: run setup in Auto mode
   * -mmi: Allow multiple installer instances. (Shared installs)
   * -showBanner: Show the banner in the download and install process dialogs.
   *              This will override config.ini
   * -hideBanner: Hides the banner in the download and install process dialogs.
   *              This will override config.ini
   * -ms: run setup in Silent mode
   * -ira: ignore the [RunAppX] sections
   * -ispf: ignore the [Program FolderX] sections that show
   *        the Start Menu shortcut folder at the end of installation.
   */

  if(sgProduct.szParentProcessFilename && *sgProduct.szParentProcessFilename != '\0')
    ParsePath(sgProduct.szParentProcessFilename, szProcessFilename, sizeof(szProcessFilename), FALSE, PP_FILENAME_ONLY);
  else
  {
    GetModuleFileName(NULL, szBuf, sizeof(szBuf));
    ParsePath(szBuf, szProcessFilename, sizeof(szProcessFilename), FALSE, PP_FILENAME_ONLY);
  }

  GetPrivateProfileString("Strings", "UsageMsg Usage", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if (lstrlen(szBuf) > 0)
  {
    char strUsage[MAX_BUF];

    ReplacePrivateProfileStrCR(szBuf);
    _snprintf(szUsageMsg, sizeof(szUsageMsg), szBuf, szProcessFilename);
    szUsageMsg[sizeof(szUsageMsg) - 1] = '\0';
    GetPrivateProfileString("Messages", "DLG_USAGE_TITLE", "", strUsage, sizeof(strUsage), szFileIniInstall);
    MessageBox(hWndMain, szUsageMsg, strUsage, MB_ICONEXCLAMATION);
  }
}

/* Function: ParseForStartupOptions()
 *       in: aCmdLine
 *  purpose: Parses for options that affect the initialization of setup.exe,
 *           such as -ms, -ma, -mmi.
 *           This is required to be parsed this early because setup needs to
 *           know if dialogs need to be shown (-ms, -ma) and also where to
 *           create the temp directory for temporary items to be placed at
 *           (-mmi).
 */
DWORD ParseForStartupOptions(LPSTR aCmdLine)
{
  char  szArgVBuf[MAX_BUF];
  int   i;
  int   iArgC;

#ifdef XXX_DEBUG
  char  szBuf[MAX_BUF];
  char  szOutputStr[MAX_BUF];
#endif

  iArgC = GetArgC(aCmdLine);

#ifdef XXX_DEBUG
  wsprintf(szOutputStr, "ArgC: %d\n", iArgC);
#endif

  i = 0;
  while(i < iArgC)
  {
    GetArgV(aCmdLine, i, szArgVBuf, sizeof(szArgVBuf));

    if(!lstrcmpi(szArgVBuf, "-mmi") || !lstrcmpi(szArgVBuf, "/mmi"))
    {
      gbAllowMultipleInstalls = TRUE;    
    }
    else if(!lstrcmpi(szArgVBuf, "-ma") || !lstrcmpi(szArgVBuf, "/ma"))
      SetSetupRunMode("AUTO");
    else if(!lstrcmpi(szArgVBuf, "-ms") || !lstrcmpi(szArgVBuf, "/ms"))
      SetSetupRunMode("SILENT");

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

DWORD ParseCommandLine(LPSTR aMessageToClose, LPSTR lpszCmdLine)
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
      ShowMessage(aMessageToClose, FALSE);
      PrintUsage();
      return(WIZ_ERROR_UNDEFINED);
    }
    else if(!lstrcmpi(szArgVBuf, "-a") || !lstrcmpi(szArgVBuf, "/a"))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      lstrcpy(sgProduct.szAlternateArchiveSearchPath, szArgVBuf);
    }
    else if(!lstrcmpi(szArgVBuf, "-greForce") || !lstrcmpi(szArgVBuf, "/greForce"))
    {
      gbForceInstallGre = TRUE;
    }
    else if(!lstrcmpi(szArgVBuf, "-greLocal") || !lstrcmpi(szArgVBuf, "/greLocal"))
    {
      sgProduct.greType = GRE_LOCAL;
    }
    else if(!lstrcmpi(szArgVBuf, "-greShared") || !lstrcmpi(szArgVBuf, "/greShared"))
    {
      sgProduct.greType = GRE_SHARED;
    }
    else if(!lstrcmpi(szArgVBuf, "-f") || !lstrcmpi(szArgVBuf, "/f"))
    {
      gbForceInstall = TRUE;
    }
    else if(!lstrcmpi(szArgVBuf, "-n") || !lstrcmpi(szArgVBuf, "/n"))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      lstrcpy(sgProduct.szParentProcessFilename, szArgVBuf);
    }
    else if(!lstrcmpi(szArgVBuf, "-ira") || !lstrcmpi(szArgVBuf, "/ira"))
      /* ignore [RunAppX] sections */
      gbIgnoreRunAppX = TRUE;
    else if(!lstrcmpi(szArgVBuf, "-ispf") || !lstrcmpi(szArgVBuf, "/ispf"))
      /* ignore [Program FolderX] sections */
      gbIgnoreProgramFolderX = TRUE;
    else if(!lstrcmpi(szArgVBuf, "-dd") || !lstrcmpi(szArgVBuf, "/dd"))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      lstrcpy(sgProduct.szPath, szArgVBuf);
    }
    // identifies the app which is installing for shared installs
    else if(!lstrcmpi(szArgVBuf, "-app") || !lstrcmpi(szArgVBuf, "/app"))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      lstrcpy(sgProduct.szAppID, szArgVBuf);
    }
    // points to a file belonging to the app which can be searched to determine
    //    if the app is installed
    else if(!lstrcmpi(szArgVBuf, "-app_path") || !lstrcmpi(szArgVBuf, "/app_path"))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      lstrcpy(sgProduct.szAppPath, szArgVBuf);
    }
    // alternative path in Windows registry, for private sharable installations
    else if(!lstrcmpi(szArgVBuf, "-reg_path") || !lstrcmpi(szArgVBuf, "/reg_path"))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      lstrcpy(sgProduct.szRegPath, szArgVBuf);
    }
    else if(!lstrcmpi(szArgVBuf, "-showBanner") || !lstrcmpi(szArgVBuf, "/showBanner"))
      gShowBannerImage = TRUE;
    else if(!lstrcmpi(szArgVBuf, "-hideBanner") || !lstrcmpi(szArgVBuf, "/hideBanner"))
      gShowBannerImage = FALSE;
    else if(!lstrcmpi(szArgVBuf, "-cleanupOnUpgrade") || !lstrcmpi(szArgVBuf, "/cleanupOnUpgrade"))
      sgProduct.checkCleanupOnUpgrade = TRUE;
    else if(!lstrcmpi(szArgVBuf, "-noCleanupOnUpgrade") || !lstrcmpi(szArgVBuf, "/noCleanupOnUpgrade"))
      sgProduct.checkCleanupOnUpgrade = FALSE;
    else if(!lstrcmpi(szArgVBuf, "-greCleanupOrphans") || !lstrcmpi(szArgVBuf, "/greCleanupOrphans"))
      sgProduct.greCleanupOrphans = TRUE;
    else if(!lstrcmpi(szArgVBuf, "-greNoCleanupOrphans") || !lstrcmpi(szArgVBuf, "/greNoCleanupOrphans"))
      sgProduct.greCleanupOrphans = FALSE;

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
    ParsePath(szFile, szPath, sizeof(szPath), FALSE, PP_PATH_ONLY);

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
  char messageFullInstaller[MAX_BUF];

  *aMessage = '\0';
  *messageFullInstaller = '\0';

  GetPrivateProfileString(aSection, "Message", "", aMessage, aMessageBufSize, szFileIniConfig);
  GetPrivateProfileString(aSection, "Message Full Installer", "", messageFullInstaller, sizeof(messageFullInstaller), szFileIniConfig);
  if(!gbDownloadTriggered && !gbPreviousUnfinishedDownload && (*messageFullInstaller != '\0'))
    MozCopyStr(messageFullInstaller, aMessage, aMessageBufSize);

  return(WIZ_OK);
}

HRESULT ShowMessageAndQuitProcess(HWND aHwndFW, char *aMsgQuitProcess, char *aMsgWait, BOOL aCloseAllWindows, char *aProcessName, BOOL aForceQuit)
{
  switch(sgProduct.mode)
  {
    case NORMAL:
    {
      char msgTitleStr[MAX_BUF];
      GetPrivateProfileString("Messages", "MB_ATTENTION_STR", "", msgTitleStr, sizeof(msgTitleStr), szFileIniInstall);
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

  GetPrivateProfileString("Messages", "MB_ATTENTION_STR", "", msgTitleStr, sizeof(msgTitleStr), szFileIniInstall);
  bContinue = TRUE;
  index    = -1;
  while(bContinue)
  {
    *className  = '\0';
    *windowName = '\0';
    *message    = '\0';

    wsprintf(section, "Check Instance%d", ++index);
    GetPrivateProfileString(section, "Process Name", "", processName, sizeof(processName), szFileIniConfig);
    GetPrivateProfileString(section, "Pretty Name", "", prettyName, sizeof(prettyName), szFileIniConfig);
    GetPrivateProfileString(section, "Message Wait", "", msgWait, sizeof(msgWait), szFileIniConfig);
    GetPrivateProfileString(section, "Close All Process Windows", "", closeAllWindows, sizeof(closeAllWindows), szFileIniConfig);
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
        GetPrivateProfileString("Messages", "MSG_FORCE_QUIT_PROCESS", "", message, sizeof(message), szFileIniInstall);
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
      GetPrivateProfileString("Messages", "MSG_FORCE_QUIT_PROCESS_FAILED", "", message, sizeof(message), szFileIniInstall);
      if(*message != '\0')
      {
        wsprintf(buf, message, prettyName, processName, prettyName);
        switch(sgProduct.mode)
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

    dwRv0 = GetPrivateProfileString(section, "Class Name",  "", className,  sizeof(className), szFileIniConfig);
    dwRv1 = GetPrivateProfileString(section, "Window Name", "", windowName, sizeof(windowName), szFileIniConfig);
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
        PreCheckInstance(section, szFileIniConfig);

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

void RefreshIcons()
{
  char subKey[MAX_BUF];
  char iconConstraint[MAX_BUF];
  BYTE iconConstraintNew[MAX_BUF];
  HKEY hkResult;
  DWORD dwValueType;
  long iconConstraintSize;
  long rv;

  // Get the size of the icon from the windows registry.
  // When this registry value is changed, the OS can flush the
  // icon cache and thus update the icons.
  iconConstraintSize = sizeof(iconConstraint);
  strcpy(subKey, "Control Panel\\Desktop\\WindowMetrics");
  RegOpenKeyEx(HKEY_CURRENT_USER, subKey, 0, KEY_ALL_ACCESS, &hkResult);
  rv = RegQueryValueEx(hkResult, "Shell Icon Size", 0, &dwValueType, iconConstraint, &iconConstraintSize);
  if(rv != ERROR_SUCCESS || iconConstraintSize == 0)
  {
    // Key not found or value not found, so use default OS value.
    int iIconSize = GetSystemMetrics(SM_CXICON);
    if(iIconSize == 0)
      // Getting default OS value failed, use hard coded value
      iIconSize = DEFAULT_ICON_SIZE;

    sprintf(iconConstraint, "%d", iIconSize);
  }

  // decrease the size of the icon by 1
  // and tell the system to refresh the icons
  sprintf(iconConstraintNew, "%d", atoi(iconConstraint) - 1);
  iconConstraintSize = lstrlen(iconConstraintNew);
  RegSetValueEx(hkResult, "Shell Icon Size", 0, dwValueType, iconConstraintNew, iconConstraintSize);
  SendMessageTimeout(HWND_BROADCAST,
                     WM_SETTINGCHANGE,
                     SPI_SETNONCLIENTMETRICS,
                     (LPARAM)"WindowMetrics",
                     SMTO_NORMAL|SMTO_ABORTIFHUNG, 
                     10000, NULL); 

  // reset the original size of the icon
  // and tell the system to refresh the icons
  iconConstraintSize = lstrlen(iconConstraint);
  RegSetValueEx(hkResult, "Shell Icon Size", 0, dwValueType, iconConstraint, iconConstraintSize);
  SendMessageTimeout(HWND_BROADCAST,
                     WM_SETTINGCHANGE,
                     SPI_SETNONCLIENTMETRICS,
                     (LPARAM)"WindowMetrics",
                     SMTO_NORMAL|SMTO_ABORTIFHUNG, 
                     10000, NULL); 
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
      switch(sgProduct.mode)
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
  DWORD dwPreviousUnfinishedState;

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
  if(InitDlgAdditionalOptions(&diAdditionalOptions))
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
  if(InitGre(&gGre))
    return(WIZ_ERROR_INIT);
 
  /* get install Mode information */
  GetPrivateProfileString("General", "Run Mode", "", szBuf, sizeof(szBuf), szFileIniConfig);
  SetSetupRunMode(szBuf);

  /* find out if we are doing a shared install */
  GetPrivateProfileString("General", "Shared Install", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    sgProduct.bSharedInst = TRUE;

  /* find out if we need to cleanup previous installation on upgrade
   * (installing ontop of - not related to cleaning up olde GRE's
   *  installed elsewhere) */
  GetPrivateProfileString("Cleanup On Upgrade", "Cleanup", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    sgProduct.checkCleanupOnUpgrade = TRUE;

  /* this is a default value so don't change it if it has already been set */
  if(*sgProduct.szAppID == '\0')
    GetPrivateProfileString("General", "Default AppID",   "", sgProduct.szAppID, MAX_BUF, szFileIniConfig);

  if(GetPrivateProfileString("Messages", "MSG_INIT_SETUP", "", szMsgInitSetup, sizeof(szMsgInitSetup), szFileIniInstall))
    ShowMessage(szMsgInitSetup, TRUE);

  /* get product name description */
  GetPrivateProfileString("General", "Company Name", "", sgProduct.szCompanyName, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Product Name", "", sgProduct.szProductName, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Product Name Internal", "", sgProduct.szProductNameInternal, MAX_BUF, szFileIniConfig);
  if (sgProduct.szProductNameInternal[0] == 0)
    lstrcpy(sgProduct.szProductNameInternal, sgProduct.szProductName);
 
  GetPrivateProfileString("General", "Product Name Previous", "", sgProduct.szProductNamePrevious, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Uninstall Filename", "", sgProduct.szUninstallFilename, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "User Agent",   "", sgProduct.szUserAgent,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Sub Path",     "", sgProduct.szSubPath,     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Program Name", "", sgProduct.szProgramName, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Lock Path",    "", szBuf,                   sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    sgProduct.bLockPath = TRUE;
  
  /* this is a default value so don't change it if it has already been set */
  if (sgProduct.szRegPath[0] == 0)
    wsprintf(sgProduct.szRegPath, "Software\\%s\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductNameInternal, sgProduct.szUserAgent);

  gbRestrictedAccess = VerifyRestrictedAccess();

  /* get main install path */
  if(LocatePreviousPath("Locate Previous Product Path", szPreviousPath, sizeof(szPreviousPath)) == FALSE)
  {
    GetPrivateProfileString("General", "Path", "", szBuf, sizeof(szBuf), szFileIniConfig);
    DecryptString(sgProduct.szPath, szBuf);
  }
  else
  {
    /* If the previous path is located in the registry, then we need to check to see if the path from
     * the registry plus the Sub Path contains the Program Name file.  If it does, then we have the
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

  /* get main program folder path */
  GetPrivateProfileString("General", "Program Folder Path", "", szBuf, sizeof(szBuf), szFileIniConfig);
  DecryptString(sgProduct.szProgramFolderPath, szBuf);
  
  /* get main program folder name */
  GetPrivateProfileString("General", "Program Folder Name", "", szBuf, sizeof(szBuf), szFileIniConfig);
  DecryptString(sgProduct.szProgramFolderName, szBuf);

  GetPrivateProfileString("General", "Refresh Icons", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    gSystemInfo.bRefreshIcons = TRUE;

  /* Set default value for greType if not already set. If already set,
   * it was set via cmdline argumen. Do not override the setting. */
  if(sgProduct.greType == GRE_TYPE_NOT_SET)
  {
    sgProduct.greType = GRE_SHARED; /* Always default to installing GRE to global area. */
    GetPrivateProfileString("General", "GRE Type", "", szBuf, sizeof(szBuf), szFileIniConfig);
    if(lstrcmpi(szBuf, "Local") == 0)
      sgProduct.greType = GRE_LOCAL;
    else
      sgProduct.greType = GRE_SHARED;
  }

  GetPrivateProfileString("General", "GRE Cleanup Orphans", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    sgProduct.greCleanupOrphans = TRUE;

  GetPrivateProfileString("General", "GRE Cleanup Orphans Message", "", sgProduct.greCleanupOrphansMessage, sizeof(sgProduct.greCleanupOrphansMessage), szFileIniConfig);
  GetPrivateProfileString("General", "GRE ID", "", sgProduct.greID, sizeof(sgProduct.greID), szFileIniConfig);
  GetPrivateProfileString("General", "GRE Private Key", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(*szBuf != '\0')
    DecryptString(sgProduct.grePrivateKey, szBuf);

  GetPrivateProfileString("General", "Show Banner Image", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "FALSE") == 0)
    gShowBannerImage = FALSE;

  iRv = ParseCommandLine(szMsgInitSetup, lpszCmdLine);
  if(iRv)
    return(iRv);

  if(gbRestrictedAccess && !gbForceInstall)
  {
    // User does not have the appropriate privileges on this system
    // (-f "force install" option overrides this check)
    char szTitle[MAX_BUF_TINY];
    int  iRvMB;

    switch(sgProduct.mode)
    {
      case NORMAL:
        if(!GetPrivateProfileString("Messages", "MB_WARNING_STR", "", szBuf, sizeof(szBuf), szFileIniInstall))
          lstrcpy(szTitle, "Setup");
        else
          wsprintf(szTitle, szBuf, sgProduct.szProductName);

        GetPrivateProfileString("Strings", "Message NORMAL Restricted Access", "", szBuf, sizeof(szBuf), szFileIniConfig);
        iRvMB = MessageBox(hWndMain, szBuf, szTitle, MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2);

        /* force a local GRE to avoid problems with non-admin installs */
        sgProduct.greType = GRE_LOCAL;
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

  /* make a copy of sgProduct.szPath to be used in the Setup Type dialog */
  lstrcpy(szTempSetupPath, sgProduct.szPath);
  
  // check to see if files need to be installed for share installations
  if(sgProduct.bSharedInst == TRUE)
    SetInstallFilesVar(sgProduct.szPath);

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

  /* Additional Options dialog */
  GetPrivateProfileString("Dialog Additional Options",       "Show Dialog",    "", szShowDialog,                     sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Additional Options",       "Title",          "", diAdditionalOptions.szTitle,        MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Additional Options",       "Message0",       "", diAdditionalOptions.szMessage0,     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Additional Options",       "Message1",       "", diAdditionalOptions.szMessage1,     MAX_BUF, szFileIniConfig);

  GetPrivateProfileString("Dialog Additional Options",       "Save Installer", "", szBuf,                            sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diAdditionalOptions.bSaveInstaller = TRUE;

  GetPrivateProfileString("Dialog Additional Options",       "Recapture Homepage", "", szBuf,                            sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diAdditionalOptions.bRecaptureHomepage = TRUE;

  GetPrivateProfileString("Dialog Additional Options",       "Show Homepage Option", "", szBuf,                            sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diAdditionalOptions.bShowHomepageOption = TRUE;

  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diAdditionalOptions.bShowDialog = TRUE;

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
    diAdditionalOptions.dwUseProtocol = UP_HTTP;
  else
    diAdditionalOptions.dwUseProtocol = UP_FTP;

  GetPrivateProfileString("Dialog Advanced Settings",       "Use Protocol Settings", "", szBuf,                     sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "DISABLED") == 0)
    diAdditionalOptions.bUseProtocolSettings = FALSE;
  else
    diAdditionalOptions.bUseProtocolSettings = TRUE;

  GetPrivateProfileString("Dialog Advanced Settings",
                          "Show Protocols",
                          "",
                          szBuf,
                          sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "FALSE") == 0)
    diAdditionalOptions.bShowProtocols = FALSE;
  else
    diAdditionalOptions.bShowProtocols = TRUE;

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

  switch(sgProduct.mode)
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
      diAdditionalOptions.bShowDialog           = FALSE;
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
  GetPrivateProfileString("Core",                           "Status",           "", szBuf,                        sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "DISABLED") == 0)
    siCFXpcomFile.bStatus = STATUS_DISABLED;
  else
    siCFXpcomFile.bStatus = STATUS_ENABLED;

  LogISProductInfo();
  LogMSProductInfo();
  CleanupXpcomFile();
  ShowMessage(szMsgInitSetup, FALSE);

  /* check the windows registry to see if a previous instance of setup finished downloading
   * all the required archives. */
  dwPreviousUnfinishedState = GetPreviousUnfinishedState();

  // Delete archives from the previous state *if* the user did not cancel
  // out of the download state.
  if(dwPreviousUnfinishedState == PUS_NONE)
    DeleteArchives(DA_ONLY_IF_NOT_IN_ARCHIVES_LST);

  gbPreviousUnfinishedDownload = dwPreviousUnfinishedState == PUS_DOWNLOAD;
  if(gbPreviousUnfinishedDownload)
  {
    char szTitle[MAX_BUF_TINY];

    switch(sgProduct.mode)
    {
      case NORMAL:
        if(!GetPrivateProfileString("Messages", "STR_MESSAGEBOX_TITLE", "", szBuf, sizeof(szBuf), szFileIniInstall))
          lstrcpy(szTitle, "Setup");
        else
          wsprintf(szTitle, szBuf, sgProduct.szProductName);

        GetPrivateProfileString("Strings", "Message Unfinished Download Restart", "", szBuf, sizeof(szBuf), szFileIniConfig);
        if(MessageBox(hWndMain, szBuf, szTitle, MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND) == IDNO)
        {
          UnsetSetupCurrentDownloadFile();
          UnsetSetupState(); /* unset the download state so that the archives can be deleted */
          DeleteArchives(DA_ONLY_IF_NOT_IN_ARCHIVES_LST);
        }
        break;
    }
  }
  else if((dwPreviousUnfinishedState == PUS_UNPACK_XPCOM) || (dwPreviousUnfinishedState == PUS_INSTALL_XPI))
  {
    char szTitle[MAX_BUF_TINY];

    // need to set this var to true even though the previous state was not the
    // download state.  This is because it is used for disk space calculation
    // wrt saved downloaded files and making sure CRC checks are performed on
    // them.
    gbPreviousUnfinishedDownload = TRUE;
    switch(sgProduct.mode)
    {
      case NORMAL:
        if(!GetPrivateProfileString("Messages", "STR_MESSAGEBOX_TITLE", "", szBuf, sizeof(szBuf), szFileIniInstall))
          lstrcpy(szTitle, "Setup");
        else
          wsprintf(szTitle, szBuf, sgProduct.szProductName);

        GetPrivateProfileString("Strings", "Message Unfinished Install Xpi Restart", "", szBuf, sizeof(szBuf), szFileIniConfig);
        if(MessageBox(hWndMain, szBuf, szTitle, MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND) == IDNO)
        {
          UnsetSetupCurrentDownloadFile();
          UnsetSetupState(); /* unset the installing xpis state so that the archives can be deleted */
          DeleteArchives(DA_ONLY_IF_NOT_IN_ARCHIVES_LST);
        }
        break;
    }
  }

  /* clean up previous exit status log file */
  DeleteExitStatusFile();

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
  GetPrivateProfileString("General", "IGNORE_", "", sgInstallGui.szIgnore_, sizeof(sgInstallGui.szIgnore_), szFileIniInstall);
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
  GetPrivateProfileString("General", "DECLINE_", "", sgInstallGui.szDecline_, sizeof(sgInstallGui.szDecline_), szFileIniInstall);
  GetPrivateProfileString("General", "PROGRAMFOLDER_", "", sgInstallGui.szProgramFolder_, sizeof(sgInstallGui.szProgramFolder_), szFileIniInstall);
  GetPrivateProfileString("General", "EXISTINGFOLDERS_", "", sgInstallGui.szExistingFolder_, sizeof(sgInstallGui.szExistingFolder_), szFileIniInstall);
  GetPrivateProfileString("General", "SETUPMESSAGE", "", sgInstallGui.szSetupMessage, sizeof(sgInstallGui.szSetupMessage), szFileIniInstall);
  GetPrivateProfileString("General", "RESTART", "", sgInstallGui.szRestart, sizeof(sgInstallGui.szRestart), szFileIniInstall);
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
  GetPrivateProfileString("General", "CONTINUE_", "", sgInstallGui.szContinue_, sizeof(sgInstallGui.szContinue_), szFileIniInstall);
  GetPrivateProfileString("General", "SKIP_", "", sgInstallGui.szSkip_, sizeof(sgInstallGui.szSkip_), szFileIniInstall);
  GetPrivateProfileString("General", "EXTRACTING", "", sgInstallGui.szExtracting, sizeof(sgInstallGui.szExtracting), szFileIniInstall);
  GetPrivateProfileString("General", "README", "", sgInstallGui.szReadme_, sizeof(sgInstallGui.szReadme_), szFileIniInstall);
  GetPrivateProfileString("General", "PAUSE_", "", sgInstallGui.szPause_, sizeof(sgInstallGui.szPause_), szFileIniInstall);
  GetPrivateProfileString("General", "RESUME_", "", sgInstallGui.szResume_, sizeof(sgInstallGui.szResume_), szFileIniInstall);
  GetPrivateProfileString("General", "CHECKED", "",   sgInstallGui.szChecked,   sizeof(sgInstallGui.szChecked),   szFileIniInstall);
  GetPrivateProfileString("General", "UNCHECKED", "", sgInstallGui.szUnchecked, sizeof(sgInstallGui.szUnchecked), szFileIniInstall);
  GetPrivateProfileString("Messages", "CB_DEFAULT", "", szSiteSelectorDescription, MAX_BUF, szFileIniInstall);

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
  char szBuf2[MAX_BUF];
  char szKey[MAX_BUF];
  char szName[MAX_BUF];
  char szValue[MAX_BUF];
  char szLookupSection[MAX_BUF];
  char szWRMSCurrentVersion[] = "Software\\Microsoft\\Windows\\CurrentVersion";
  char szWRMSShellFolders[]   = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders";
  char szWRMSMapGroup[]       = "Software\\Microsoft\\Windows\\CurrentVersion\\GrpConv\\MapGroup";
  HKEY hkeyRoot;

  /* zero out the memory allocations */
  ZeroMemory(szBuf,           sizeof(szBuf));
  ZeroMemory(szKey,           sizeof(szKey));
  ZeroMemory(szName,          sizeof(szName));
  ZeroMemory(szValue,         sizeof(szValue));
  ZeroMemory(szBuf2,          sizeof(szBuf2));
  ZeroMemory(szLookupSection, sizeof(szLookupSection));

  if(lstrcmpi(szVariable, "PROGRAMFILESDIR") == 0)
  {
    /* parse for the "c:\Program Files" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, szWRMSCurrentVersion, "ProgramFilesDir", szVariable, dwVariableSize);
    /* see if that failed */
    if (*szVariable == 0)
    {
      char* backslash;
      /* Use the parent of the windows directory */
      GetWindowsDirectory(szVariable, dwVariableSize);
      backslash = strrchr(szVariable, '\\');
      if (backslash)
        *backslash = 0;
    }
  }
  else if(lstrcmpi(szVariable, "PROGRAMFILESPATH") == 0)
  {
    /* parse for the "\Program Files" directory -- NOTE does not include the drive letter */
    GetWinReg(HKEY_LOCAL_MACHINE, szWRMSCurrentVersion, "ProgramFilesDir", szBuf, sizeof(szBuf));
    /* see if that failed */
    if (*szBuf == 0)
    {
      char* backslash;
      /* Use the parent of the windows directory */
      GetWindowsDirectory(szBuf, sizeof(szBuf));
      backslash = strrchr(szBuf, '\\');
      if (backslash)
        *backslash = 0;
    }
    lstrcpy(szVariable, szBuf+2);
  }
  else if(lstrcmpi(szVariable, "INSTALLDRIVE") == 0)
  {
    /* parse for "C:" */
    szVariable[0] = sgProduct.szPath[0];
    szVariable[1] = sgProduct.szPath[1];
    szVariable[2] = '\0';
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
    if(gSystemInfo.dwOSType & OS_WIN9x || gbRestrictedAccess)
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
    if(gSystemInfo.dwOSType & OS_WIN9x || gbRestrictedAccess)
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
    if(gSystemInfo.dwOSType & OS_WIN9x || gbRestrictedAccess)
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
    if(gSystemInfo.dwOSType & OS_WIN9x || gbRestrictedAccess)
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
    /* parse for the "c:\Temp\ns_temp" path */
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
  else if(  (lstrcmpi(szVariable, "JRE LIB PATH") == 0)
         || (lstrcmpi(szVariable, "JRE BIN PATH") == 0) )
  {
    /* Locate the "c:\Program Files\JavaSoft\JRE\1.3\Bin" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\javaw.Exe", NULL, szBuf, dwVariableSize);
    if(*szBuf == '\0')
    {
      *szVariable = '\0';
      return(FALSE);
    }
      
    ParsePath(szBuf, szBuf2, sizeof(szBuf2), FALSE, PP_PATH_ONLY);
    if(lstrcmpi(szVariable, "JRE LIB PATH") == 0)
    {
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
      RemoveBackSlash(szBuf2);
      ParsePath(szBuf2, szVariable, dwVariableSize, FALSE, PP_PATH_ONLY);
      AppendBackSlash(szVariable, dwVariableSize);
      lstrcat(szVariable, "lib");
    }
    else
      lstrcpy(szVariable, szBuf2);
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

    wsprintf(szKey, "Software\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductNameInternal);

    /* parse for the current Netscape WinReg key */
    GetWinReg(HKEY_LOCAL_MACHINE, szKey, "CurrentVersion", szBuf, sizeof(szBuf));

    if(*szBuf == '\0')
      return(FALSE);

    wsprintf(szVariable, "Software\\%s\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductNameInternal, szBuf);
  }
  else if(lstrcmpi(szVariable, "Product PreviousVersion") == 0)
  {
    char szKey[MAX_BUF];

    wsprintf(szKey, "Software\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductNamePrevious);

    /* parse for the current Netscape WinReg key */
    GetWinReg(HKEY_LOCAL_MACHINE, szKey, "CurrentVersion", szBuf, sizeof(szBuf));

    if(*szBuf == '\0')
      return(FALSE);

    wsprintf(szVariable, "Software\\%s\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductNamePrevious, szBuf);
  }
  else if(lstrcmpi(szVariable, "APP_ID") == 0)
  {
    lstrcpy(szVariable, sgProduct.szAppID);
  }
  else if(lstrcmpi(szVariable, "PATH_TO_APP") == 0)
  {
    lstrcpy(szVariable, sgProduct.szAppPath);
  }
  else if(lstrcmpi(szVariable, "REGPATH") == 0)
  {
    lstrcpy(szVariable, sgProduct.szRegPath);
  }
  else if(szVariable[0] == '$')
  {
    // the $ indicates that there's another section that defines a lookup for this string.
    // find that section to get the registry information, lookup the proper value and 
    // stick it into szVariable
    wsprintf(szLookupSection,"Path Lookup %s",szVariable);

    GetPrivateProfileString(szLookupSection, "Path Reg Key Root", "", szBuf, sizeof(szBuf), szFileIniConfig);
    if(*szBuf == '\0')
      return(FALSE); 
    hkeyRoot = ParseRootKey(szBuf);

    GetPrivateProfileString(szLookupSection, "Path Reg Key", "", szKey, sizeof(szKey), szFileIniConfig);

    GetPrivateProfileString(szLookupSection, "Path Reg Name", "", szName, sizeof(szName), szFileIniConfig);

    GetWinReg(hkeyRoot, szKey, szName, szBuf, sizeof(szBuf));
    if(*szBuf == '\0')
      return(FALSE);

    GetPrivateProfileString(szLookupSection, "Strip Filename", "", szBuf2, sizeof(szBuf2), szFileIniConfig);
    if(lstrcmpi(szBuf2, "TRUE") == 0)
    {
      ParsePath(szBuf, szVariable, dwVariableSize, FALSE, PP_PATH_ONLY);
      RemoveBackSlash(szVariable);
    }
    else
    {
      lstrcpy(szVariable,szBuf);
    }
  }
  else if(lstrcmpi(szVariable, "GRE PATH") == 0)
  {
    if(*gGre.homePath != '\0')
      lstrcpy(szVariable, gGre.homePath);
  }
  else
    return(FALSE);

  return(TRUE);
}

void CollateBackslashes(LPSTR szInputOutputStr)
{
  // search for "\\" in the output string, and replace it with "\"
  LPSTR pSearch          = szInputOutputStr;
  LPSTR pSearchEnd       = szInputOutputStr + lstrlen(szInputOutputStr);
  LPSTR pPreviousSearch  = NULL;

  while (pSearch < pSearchEnd)
  {
    if ('\\' == *pSearch)
    {
      if (pPreviousSearch && ('\\' == *pPreviousSearch))
      { // found a second one -> remove it
        memmove(pPreviousSearch, pSearch, pSearchEnd-pSearch+1);

        // our string is shorter now ...
        pSearchEnd -= pSearch - pPreviousSearch;

        // no increment of pSearch here - we want to continue with the same character,
        // again (just for the weird case of yet another backslash...)
        continue;
      }
    }

    pPreviousSearch = pSearch;
    pSearch = CharNext(pSearch);
  }
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
      CollateBackslashes(szOutputStr);
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
        CreateDirectoriesAll(buf, DO_NOT_ADD_TO_UNINSTALL_LOG);
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
  if(GreInstallerNeedsReboot())
    return(TRUE);
  if(diReboot.dwShowDialog == AUTO)
    return(bReboot);
  else
    return(diReboot.dwShowDialog);
}

/* Function: IsInstallerProductGRE()
 *       in: none.
 *      out: Boolean value on whether or not the installer will be installing
 *           the GRE.
 *  purpose: The check to see if the product the installer will be installing
 *           is GRE or not.
 */
BOOL IsInstallerProductGRE()
{
  return(lstrcmpi(sgProduct.szProductNameInternal, "GRE") == 0 ? TRUE : FALSE);
}

/* Function: GreInstallerNeedsReboot()
 *       in: none.
 *      out: Boolean value on whether or not GRE installer needed a
 *           when it ran last.
 *  purpose: To check if this is not the GRE installer and that the GRE installer
 *           has been run needed a reboot.
 */
BOOL GreInstallerNeedsReboot()
{
  BOOL greReboot = FALSE;

  /* if this setup is not installing GRE *and* the GRE Setup has been run, then
   * check for GRE setup's exit value, if one exists */
  if(!IsInstallerProductGRE() && gGreInstallerHasRun)
  {
    char status[MAX_BUF];

    GetGreSetupExitStatus(status, sizeof(status));
    /* if a reboot is detected from the GRE setup run, then
     * simply return TRUE for reboot is needed. */
    if(lstrcmpi(status, "Reboot") == 0)
      greReboot = TRUE;
  }
  return(greReboot);
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

  if((!bSDUserCanceled) && (GetPreviousUnfinishedState() == PUS_NONE))
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
  DeInitGre(&gGre);
  DeInitSXpcomFile();
  DeInitSDObject();
  DeInitDlgReboot(&diReboot);
  DeInitDlgDownload(&diDownload);
  DeInitDlgStartInstall(&diStartInstall);
  DeInitDlgAdditionalOptions(&diAdditionalOptions);
  DeInitDlgAdvancedSettings(&diAdvancedSettings);
  DeInitDlgProgramFolder(&diProgramFolder);
  DeInitDlgWindowsIntegration(&diWindowsIntegration);
  DeInitDlgSelectComponents(&diSelectAdditionalComponents);
  DeInitDlgSelectComponents(&diSelectComponents);
  DeInitDlgSetupType(&diSetupType);
  DeInitDlgWelcome(&diWelcome);
  DeInitDlgLicense(&diLicense);
  DeInitDlgQuickLaunch(&diQuickLaunch);
  DeInitDSNode(&gdsnComponentDSRequirement);
  DeInitErrorMessageStream();

  FreeMemory(&szTempDir);
  FreeMemory(&szOSTempDir);
  FreeMemory(&szProxyDLLPath);
  FreeMemory(&szSetupDir);
  FreeMemory(&szFileIniConfig);
  FreeMemory(&szFileIniInstall);
  FreeMemory(&szEGlobalAlloc);
  FreeMemory(&szEOutOfMemory);
  FreeMemory(&szEDllLoad);
  FreeMemory(&szEStringLoad);
  FreeMemory(&szEStringNull);
  DeleteObject(sgInstallGui.systemFont);
  DeleteObject(sgInstallGui.definedFont);

  FreeLibrary(hSetupRscInst);
  if (hGREAppInstallerProxyDLL != NULL) 
    FreeLibrary(hGREAppInstallerProxyDLL);
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

  /* We need to have the product name be part of the Setup directory name.
   * This is because if GRE is installed ontop of this app, GRE's saved files
   * will overwrite files of the same name for this app. */
  if(*sgProduct.szProductNameInternal != '\0')
  {
    lstrcat(szBuf, " ");
    lstrcat(szBuf, sgProduct.szProductNameInternal);
  }

  return(szBuf);
}

void SaveInstallerFiles()
{
  int       i;
  char      szBuf[MAX_BUF];
  char      destInstallDir[MAX_BUF];
  char      destInstallXpiDir[MAX_BUF];
  char      szMFN[MAX_BUF];
  char      szArchivePath[MAX_BUF];
  DWORD     dwIndex0;
  siC       *siCObject = NULL;

  GetSaveInstallerPath(destInstallDir, sizeof(destInstallDir));
  AppendBackSlash(destInstallDir, sizeof(destInstallDir));
  CreateDirectoriesAll(destInstallDir, ADD_TO_UNINSTALL_LOG);

  /* copy the self extracting file that spawned setup.exe, if one exists */
  if((*sgProduct.szAlternateArchiveSearchPath != '\0') && (*sgProduct.szParentProcessFilename != '\0'))
  {
    FileCopy(sgProduct.szParentProcessFilename, destInstallDir, FALSE, FALSE);

    /* The dir for xpi files is .\xpi because the self-extracting
     * .exe file will automatically look for the .xpi files in a xpi subdir
     * off of the current working dir. */
    _snprintf(destInstallXpiDir, sizeof(destInstallXpiDir), "%sxpi\\", destInstallDir);
    destInstallXpiDir[sizeof(destInstallXpiDir) - 1] = '\0';
    CreateDirectoriesAll(destInstallXpiDir, ADD_TO_UNINSTALL_LOG);
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
    FileCopy(szBuf, destInstallDir, FALSE, FALSE);

    /* now copy the rest of the setup files */
    i = 0;
    while(TRUE)
    {
      if(*SetupFileList[i] == '\0')
        break;

      lstrcpy(szBuf, szSetupDir);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, SetupFileList[i]);
      FileCopy(szBuf, destInstallDir, FALSE, FALSE);

      ++i;
    }
    /* copy the license file */
    if(*diLicense.szLicenseFilename != '\0')
      FileCopy(diLicense.szLicenseFilename, destInstallDir, FALSE, FALSE);
    /* copy the readme file */
    if(*diSetupType.szReadmeFilename != '\0')
      FileCopy(diLicense.szLicenseFilename, destInstallDir, FALSE, FALSE);

    /* The dir for xpi files is just "." as opposed to ".\xpi"
     * because the setup.exe (not the self-extracting .exe) will look for the
     * .xpi files in the cwd. */
    MozCopyStr(destInstallDir, destInstallXpiDir, sizeof(destInstallXpiDir));
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
      FileCopy(szBuf, destInstallXpiDir, FALSE, FALSE);
    }

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }
}

BOOL ShowAdditionalOptionsDialog(void)
{
  if(diAdditionalOptions.bShowDialog == FALSE)
    return(FALSE);

  if( (diAdditionalOptions.bShowHomepageOption == FALSE) && (GetTotalArchivesToDownload() < 1) )
    return(FALSE);

  return(TRUE);
}
