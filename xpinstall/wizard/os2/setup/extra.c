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
 *     Curt Patrick <curt@netscape.com>
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

char *ArchiveExtensions[] = {"zip",
                             "xpi",
                             "jar",
                             ""};

BOOL InitApplication()
{
  CLASSINFO classinfo;
  WinQueryClassInfo((HAB)0, WC_FRAME, &classinfo);
  return (WinRegisterClass((HAB)0, CLASS_NAME_SETUP_DLG, WinDefDlgProc,
                           CS_SAVEBITS, classinfo.cbWindowData));
}

BOOL InitInstance()
{
  gSystemInfo.lScreenX = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
  gSystemInfo.lScreenY = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
  gSystemInfo.lDlgFrameX = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  gSystemInfo.lDlgFrameY = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  gSystemInfo.lTitleBarY = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);

  hWndMain = NULL;

  return(TRUE);
}

void PrintError(PSZ szMsg, ULONG ulErrorCodeSH)
{
  ERRORID erridErrorCode;
  char  szErrorString[MAX_BUF];

  if(ulErrorCodeSH == ERROR_CODE_SHOW)
  {
    erridErrorCode = WinGetLastError((HAB)0);
    sprintf(szErrorString, "%d : %s", erridErrorCode, szMsg);
  }
  else
    sprintf(szErrorString, "%s", szMsg);

  if((sgProduct.ulMode != SILENT) && (sgProduct.ulMode != AUTO))
  {
    WinMessageBox(HWND_DESKTOP, hWndMain, szErrorString, NULL, 0, MB_ICONEXCLAMATION);
  }
  else if(sgProduct.ulMode == AUTO)
  {
    ShowMessage(szErrorString, TRUE);
    DosSleep(5000);
    ShowMessage(szErrorString, FALSE);
  }
}

/* Windows API does offer a GlobalReAlloc() routine, but it kept
 * returning an out of memory error for subsequent calls to it
 * after the first time, thus the reason for this routine. */
void *NS_GlobalReAlloc(void **hgMemory,
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

void *NS_GlobalAlloc(ULONG ulMaxBuf)
{
  void *vBuf = NULL;

  if ((vBuf = calloc(1, ulMaxBuf)) == NULL)
  {     
    if((szEGlobalAlloc == NULL) || (*szEGlobalAlloc == '\0'))
      PrintError("Memory allocation error.", ERROR_CODE_HIDE);
    else
      PrintError(szEGlobalAlloc, ERROR_CODE_SHOW);

    return(NULL);
  }
  else
  {
    return(vBuf);
  }
}

void FreeMemory(void **vPointer)
{
  if(*vPointer != NULL)
    free(*vPointer);
  *vPointer = NULL;
}

HRESULT NS_LoadStringAlloc(HMODULE hInstance, ULONG ulID, PSZ *szStringBuf, ULONG ulStringBuf)
{
  char szBuf[MAX_BUF];

  if((*szStringBuf = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  
  if(!WinLoadString((HAB)0, hInstance, ulID, ulStringBuf, *szStringBuf))
  {
    if((szEStringLoad == NULL) ||(*szEStringLoad == '\0'))
      sprintf(szBuf, "Could not load string resource ID %d", ulID);
    else
      sprintf(szBuf, szEStringLoad, ulID);

    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  return(0);
}

HRESULT NS_LoadString(HMODULE hInstance, ULONG ulID, PSZ szStringBuf, ULONG ulStringBuf)
{
  char szBuf[MAX_BUF];

  if(!WinLoadString((HAB)0, hInstance, ulID, ulStringBuf, szStringBuf))
  {
    if((szEStringLoad == NULL) ||(*szEStringLoad == '\0'))
      sprintf(szBuf, "Could not load string resource ID %d", ulID);
    else
      sprintf(szBuf, szEStringLoad, ulID);

    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  return(WIZ_OK);
}

void UnsetSetupState(void)
{
  char szApp[MAX_BUF_TINY];

  sprintf(szApp,
           "%s %s",
           sgProduct.szProductNameInternal,
           sgProduct.szUserAgent);
  PrfWriteProfileString(HINI_USERPROFILE, szApp, "Setup State", NULL);
}

void SetSetupState(char *szState)
{
  char szApp[MAX_BUF_TINY];

  sprintf(szApp,
           "%s %s",
           sgProduct.szProductNameInternal,
           sgProduct.szUserAgent);

  PrfWriteProfileString(HINI_USERPROFILE, szApp, "Setup State", szState);
}

DWORD GetPreviousUnfinishedState(void)
{
  char szBuf[MAX_BUF_TINY];
  char szApp[MAX_BUF_TINY];
  DWORD dwRv = PUS_NONE;

  if(sgProduct.szProductNameInternal &&
     sgProduct.szUserAgent)
  {
    sprintf(szApp,
             "%s %s",
             sgProduct.szProductNameInternal,
             sgProduct.szUserAgent);

    PrfQueryProfileString(HINI_USERPROFILE, szApp, "Setup State", "", szBuf, sizeof(szBuf));
    if(stricmp(szBuf, SETUP_STATE_DOWNLOAD) == 0)
      dwRv = PUS_DOWNLOAD;
    else if(stricmp(szBuf, SETUP_STATE_UNPACK_XPCOM) == 0)
      dwRv = PUS_UNPACK_XPCOM;
    else if(stricmp(szBuf, SETUP_STATE_INSTALL_XPI) == 0)
      dwRv = PUS_INSTALL_XPI;
  }

  return(dwRv);
}

void UnsetSetupCurrentDownloadFile(void)
{
  char szApp[MAX_BUF];

  sprintf(szApp,
           "%s %s",
           sgProduct.szProductNameInternal,
           sgProduct.szUserAgent);
  PrfWriteProfileString(HINI_USERPROFILE, szApp, "Setup Current Download", NULL);
}

void SetSetupCurrentDownloadFile(char *szCurrentFilename)
{
  char szApp[MAX_BUF];

  sprintf(szApp,
           "%s %s",
           sgProduct.szProductNameInternal,
           sgProduct.szUserAgent);
  PrfWriteProfileString(HINI_USERPROFILE, szApp, "Setup State", szCurrentFilename);
}

char *GetSetupCurrentDownloadFile(char *szCurrentDownloadFile,
                                  ULONG ulCurrentDownloadFileBufSize)
{
  char szApp[MAX_BUF];

  if(!szCurrentDownloadFile)
    return(NULL);

  memset(szCurrentDownloadFile, 0, ulCurrentDownloadFileBufSize);
  if(sgProduct.szProductNameInternal &&
     sgProduct.szUserAgent)
  {
    sprintf(szApp,
             "%s %s",
             sgProduct.szProductNameInternal,
             sgProduct.szUserAgent);
    PrfQueryProfileString(HINI_USERPROFILE, szApp, "Setup Current Download", "",
                          szCurrentDownloadFile, ulCurrentDownloadFileBufSize);
  }

  return(szCurrentDownloadFile);
}

BOOL UpdateFile(char *szInFilename, char *szOutFilename, char *szIgnoreStr)
{
  FILE *ifp;
  FILE *ofp;
  char szLineRead[MAX_BUF];
  char szLCIgnoreLongStr[MAX_BUF];
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
    strlwr(szLCIgnoreLongStr);
  }

  while(fgets(szLineRead, sizeof(szLineRead), ifp) != NULL)
  {
    strcpy(szLCLineRead, szLineRead);
    strlwr(szLCLineRead);
    if(!strstr(szLCLineRead, szLCIgnoreLongStr))
      fputs(szLineRead, ofp);
    else
      bFoundIgnoreStr = TRUE;
  }
  fclose(ifp);
  fclose(ofp);

  return(bFoundIgnoreStr);
}

HRESULT Initialize(HMODULE hInstance, PSZ szAppName)
{
  char szBuf[MAX_BUF];
  char szCurrentProcessDir[MAX_BUF];
  char *tempEnvVar = NULL;

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

  ParsePath(szAppName, szCurrentProcessDir,
            sizeof(szCurrentProcessDir),
            FALSE,
            PP_PATH_ONLY);

  if(DosLoadModule(NULL, 0, "SETUPRSC", &hSetupRscInst) != NO_ERROR)
  {
    char szFullFilename[MAX_BUF];

    strcpy(szFullFilename, szCurrentProcessDir);
    AppendBackSlash(szFullFilename, sizeof(szFullFilename));
    strcat(szFullFilename, "Setuprsc.dll");
    if(DosLoadModule(NULL, 0, szFullFilename, &hSetupRscInst) != NO_ERROR)
    {
      sprintf(szBuf, szEDllLoad, szFullFilename);
      PrintError(szBuf, ERROR_CODE_HIDE);
      return(1);
    }
  }

  ulWizardState         = DLG_NONE;
  ulTempSetupType       = ulWizardState;
  siComponents          = NULL;
  bCreateDestinationDir = FALSE;
  bReboot               = FALSE;
  gulUpgradeValue       = UG_NONE;
  gulSiteSelectorStatus = SS_SHOW;
  gbILUseTemp           = TRUE;
  gbIgnoreRunAppX        = FALSE;
  gbIgnoreProgramFolderX = FALSE;

  if((szSetupDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  strcpy(szSetupDir, szCurrentProcessDir);

  if((szTempDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szOSTempDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szFileIniConfig = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szFileIniInstall = NS_GlobalAlloc(MAX_BUF)) == NULL)
  return(1);

  // determine the system's TEMP path
  tempEnvVar = getenv("TMP");
  if ((tempEnvVar) && (!(isFAT(tempEnvVar)))) {
    strcpy(szTempDir, tempEnvVar);
  }
  else
  {
    tempEnvVar = getenv("TEMP");
    if (tempEnvVar)
      strcpy(szTempDir, tempEnvVar);
  }
  if ((!tempEnvVar) || (isFAT(tempEnvVar)))
  {
    ULONG ulBootDrive = 0;
    APIRET rc;
    char  buffer[] = " :\\OS2\\";
    DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                    &ulBootDrive, sizeof(ulBootDrive));
    buffer[0] = 'A' - 1 + ulBootDrive;
    if (isFAT(buffer)) {
       /* Try current disk if boot drive is FAT */
       ULONG ulDriveNum;
       ULONG ulDriveMap;
       strcpy(buffer, " :\\");
       DosQueryCurrentDisk(&ulDriveNum, &ulDriveMap);
       buffer[0] = 'A' - 1 + ulDriveNum;
       if (isFAT(buffer)) {
         int i;
         for (i = 2; i < 26; i++) {
           if ((ulDriveMap<<(31-i)) >> 31) {
             buffer[0] = 'A' + i;
             if (!(isFAT(buffer))) {
                break;
             }
           }
         }
         if (i == 26) {
            char szBuf[MAX_BUF];
            if(NS_LoadString(hSetupRscInst, IDS_ERROR_NO_LONG_FILENAMES, szBuf, MAX_BUF) == WIZ_OK)
              WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, szBuf, NULL, 0, MB_ICONEXCLAMATION);
            return(1);
         }
       }
    }
    strcpy(szTempDir, buffer);
    strcat(szTempDir, "TEMP");
  }
  strcpy(szOSTempDir, szTempDir);
  AppendBackSlash(szTempDir, MAX_BUF);
  strcat(szTempDir, WIZ_TEMP_DIR);

  if(!FileExists(szTempDir))
  {
    AppendBackSlash(szTempDir, MAX_BUF);
    CreateDirectoriesAll(szTempDir, FALSE);
    RemoveBackSlash(szTempDir);
    if(!FileExists(szTempDir))
    {
      char szECreateTempDir[MAX_BUF];

      if(GetPrivateProfileString("Messages", "ERROR_CREATE_TEMP_DIR", "",
                                 szECreateTempDir, sizeof(szECreateTempDir),
                                 szFileIniInstall))
      {
        sprintf(szBuf, szECreateTempDir, szTempDir);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }
      return(1);
    }
  }

  DeleteIdiGetConfigIni();
  bIdiArchivesExists = DeleteIdiGetArchives();
  DeleteIdiGetRedirect();
  DeleteInstallLogFile(FILE_INSTALL_LOG);
  DeleteInstallLogFile(FILE_INSTALL_STATUS_LOG);
  LogISTime(W_START);
  DetermineOSVersionEx();

  gSystemInfo.bScreenReader = TRUE;

  return(0);
}

/* Function to remove quotes from a string */
void RemoveQuotes(LPSTR lpszSrc, LPSTR lpszDest, int iDestSize)
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
LPSTR GetFirstNonSpace(LPSTR lpszString)
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

BOOL IsInArchivesLst(siC *siCObject, BOOL bModify)
{
  char *szBufPtr;
  char szBuf[MAX_BUF];
  char szArchiveLstFile[MAX_BUF_MEDIUM];
  BOOL bRet = FALSE;

  strcpy(szArchiveLstFile, szTempDir);
  AppendBackSlash(szArchiveLstFile, sizeof(szArchiveLstFile));
  strcat(szArchiveLstFile, "Archive.lst");
  GetPrivateProfileString("Archives", NULL, "", szBuf, sizeof(szBuf), szArchiveLstFile);
  if(*szBuf != '\0')
  {
    szBufPtr = szBuf;
    while(*szBufPtr != '\0')
    {
      if(stricmp(siCObject->szArchiveName, szBufPtr) == 0)
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

HRESULT ParseSetupIni()
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

  DosCopy(szFileIniSetup, szFileIdiGetConfigIni, DCPY_EXISTING);

  if(!FileExists(szFileIdiGetConfigIni))
  {
    char szEFileNotFound[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_FILE_NOT_FOUND", "", szEFileNotFound, sizeof(szEFileNotFound), szFileIniInstall))
    {
      sprintf(szBuf, szEFileNotFound, szFileIdiGetConfigIni);
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

    if(GetPrivateProfileString("Messages", "ERROR_FILE_NOT_FOUND", "", szEFileNotFound, sizeof(szEFileNotFound), szFileIniInstall))
      {
        sprintf(szBuf, szEFileNotFound, FILE_INI_CONFIG);
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
    
  strcpy(szFileIniTempDir, szTempDir);
  AppendBackSlash(szFileIniTempDir, sizeof(szFileIniTempDir));
  strcat(szFileIniTempDir, FILE_INI_INSTALL);

  /* set default value for szFileIniInstall here */
  strcpy(szFileIniInstall, szFileIniTempDir);

  strcpy(szFileIniSetupDir, szSetupDir);
  AppendBackSlash(szFileIniSetupDir, sizeof(szFileIniSetupDir));
  strcat(szFileIniSetupDir, FILE_INI_INSTALL);

  /* if install.ini exists, then use it, else download install.ini from the net */
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

    if(stricmp(szTempDirTemp, szSetupDirTemp) == 0)
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

void SwapFTPAndHTTP(char *szInUrl, DWORD dwInUrlSize)
{
  char szTmpBuf[MAX_BUF];
  char *ptr       = NULL;
  char szFtp[]    = "ftp://";
  char szHttp[]   = "http://";

  if((!szInUrl) || !diAdditionalOptions.bUseProtocolSettings)
    return;

  memset(szTmpBuf, 0, sizeof(szTmpBuf));
  switch(diAdditionalOptions.dwUseProtocol)
  {
    case UP_HTTP:
      if((strncmp(szInUrl, szFtp, (sizeof(szFtp) - 1)) == 0) &&
         ((int)dwInUrlSize > strlen(szInUrl) + 1))
      {
        ptr = szInUrl + (sizeof(szFtp) - 1);
        memmove(ptr + 1, ptr, strlen(ptr) + 1);
        memcpy(szInUrl, szHttp, (sizeof(szHttp) - 1));
      }
      break;

    case UP_FTP:
    default:
      if((strncmp(szInUrl, szHttp, (sizeof(szHttp) - 1)) == 0) &&
         ((int)dwInUrlSize > strlen(szInUrl) + 1))
      {
        ptr = szInUrl + (sizeof(szHttp) - 1);
        memmove(ptr - 1, ptr, strlen(ptr) + 1);
        memcpy(szInUrl, szFtp, (sizeof(szFtp) - 1));
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
  sprintf(szUrl, "%s/%s", szPartialUrl, siCObject->szArchiveName);
  if(WritePrivateProfileString(szSection,
                               szKey,
                               szUrl,
                               szFileIdiGetArchives) == 0)
  {
    char szEWPPS[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_WRITEPRIVATEPROFILESTRING", "", szEWPPS, sizeof(szEWPPS), szFileIniInstall))
    {
      sprintf(szBufTemp,
               "%s\n    [%s]\n    url=%s",
               szFileIdiGetArchives,
               szSection,
               szUrl);
      sprintf(szBuf, szEWPPS, szBufTemp);
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
  _itoa(siCObject->ulInstallSizeArchive, szArchiveSize, 10);
  WritePrivateProfileString(szSection,
                            "size",
                            szArchiveSize,
                            szFileIdiGetArchives);
  _itoa(siCObject->dwAttributes & SIC_IGNORE_DOWNLOAD_ERROR, szBuf, 10);
  WritePrivateProfileString(szSection,
                            "Ignore File Network Error",
                            szBuf,
                            szFileIdiGetArchives);

  strcpy(szFile, szTempDir);
  AppendBackSlash(szFile, sizeof(szFile));
  strcat(szFile, FILE_INI_REDIRECT);

  memset(szIdentifier, 0, sizeof(szIdentifier));
  ssiSiteSelectorTemp = SsiGetNode(szSiteSelectorDescription);

  GetPrivateProfileString("Redirect",
                          "Status",
                          "",
                          szBuf,
                          sizeof(szBuf),
                          szFileIniConfig);
  if(stricmp(szBuf, "ENABLED") != 0)
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
    GetPrivateProfileString("General",
                            "url",
                            "",
                            szUrl,
                            sizeof(szUrl),
                            szFileIniConfig);
    if(*szUrl != 0)
    {
      sprintf(szKey, "url%d", iIndex);
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
      sprintf(szKey, "url%d", iIndex);
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
      sprintf(szKey, "url%d", iIndex);
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
      sprintf(szKey, "url%d", iIndex);
      UpdateIdiFile(szUrl, sizeof(szUrl), siCObject, szSection, szKey, szFileIdiGetArchives);
    }
  }

  return(0);
}

void SetSetupRunMode(LPSTR szMode)
{
  if(stricmp(szMode, "NORMAL") == 0)
    sgProduct.ulMode = NORMAL;
  if(stricmp(szMode, "AUTO") == 0)
    sgProduct.ulMode = AUTO;
  if(stricmp(szMode, "SILENT") == 0)
    sgProduct.ulMode = SILENT;
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
    if(stricmp(szExtension, ArchiveExtensions[i]) == 0)
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

  strcpy(szFileIniRedirect, szTempDir);
  AppendBackSlash(szFileIniRedirect, sizeof(szFileIniRedirect));
  strcat(szFileIniRedirect, FILE_INI_REDIRECT);

  if(FileExists(szFileIniRedirect))
    DosDelete(szFileIniRedirect);

  GetPrivateProfileString("Redirect", "Status", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "ENABLED") != 0)
    return(0);

  ssiSiteSelectorTemp = SsiGetNode(szSiteSelectorDescription);
  if(ssiSiteSelectorTemp != NULL)
  {
    if(ssiSiteSelectorTemp->szDomain != NULL)
      strcpy(szBufUrl, ssiSiteSelectorTemp->szDomain);
  }
  else
    /* No domain to download the redirect.ini file from.
     * Assume that it does not exist.
     * This should trigger the backup/alternate url. */
    return(0);

  strcpy(szFileIdiGetRedirect, szTempDir);
  AppendBackSlash(szFileIdiGetRedirect, sizeof(szFileIdiGetRedirect));
  strcat(szFileIdiGetRedirect, FILE_IDI_GETREDIRECT);

  GetPrivateProfileString("Redirect", "Description", "", szBuf, sizeof(szBuf), szFileIniConfig);
  WritePrivateProfileString("File0", "desc", szBuf, szFileIdiGetRedirect);
  GetPrivateProfileString("Redirect", "Server Path", "", szBuf, sizeof(szBuf), szFileIniConfig);
  AppendSlash(szBufUrl, sizeof(szBufUrl));
  strcat(szBufUrl, szBuf);
  SwapFTPAndHTTP(szBufUrl, sizeof(szBufUrl));
  if(WritePrivateProfileString("File0", "url", szBufUrl, szFileIdiGetRedirect) == 0)
  {
    char szEWPPS[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_WRITEPRIVATEPROFILESTRING", "", szEWPPS, sizeof(szEWPPS), szFileIniInstall))
    {
      sprintf(szBufTemp, "%s\n    [%s]\n    %s=%s", szFileIdiGetRedirect, "File0", szIndex0, szBufUrl);
      sprintf(szBuf, szEWPPS, szBufTemp);
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
    DosDelete(szFileIdiGetArchives);

  if(szCorruptedArchiveList != NULL)
    memset(szCorruptedArchiveList, 0, dwCorruptedArchiveListSize);

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
              if((DWORD)(strlen(szCorruptedArchiveList) + strlen(siCObject->szArchiveName + 1)) < dwCorruptedArchiveListSize)
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

  memset(szCorruptedArchiveList, 0, sizeof(szCorruptedArchiveList));
  strcpy(szFileIdiGetArchives, szTempDir);
  AppendBackSlash(szFileIdiGetArchives, sizeof(szFileIdiGetArchives));
  strcat(szFileIdiGetArchives, FILE_IDI_GETARCHIVES);
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
         (stricmp(szPartiallyDownloadedFilename,
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
    sprintf(szBuf, "failed: %d", lResult);
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

void RemoveBackSlash(PSZ szInput)
{
  char  *ptrChar = NULL;

  if(!szInput)
    return;

  ptrChar = WinPrevChar(0, 0, 0, szInput, szInput + strlen(szInput));
  if (*ptrChar == '\\') {
    *ptrChar = '\0';
  }
}

void AppendBackSlash(PSZ szInput, ULONG ulInputSize)
{
  ULONG ulInputLen = strlen(szInput);

  if(szInput)
  {
    if(*szInput == '\0')
    {
      if((ulInputLen + 1) < ulInputSize)
      {
        strcat(szInput, "\\");
      }
    }
    else if(*WinPrevChar(0, 0, 0, szInput, &szInput[ulInputLen]) != '\\')
    {
      if((ulInputLen + 1) < ulInputSize)
      {
        strcat(szInput, "\\");
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
    dwInputLen = strlen(szInput);
    bDone = FALSE;
    ptrChar = &szInput[dwInputLen];
    while(!bDone)
    {
      ptrChar = WinPrevChar(0, 0, 0, szInput, ptrChar);
      if(*ptrChar == '/')
        *ptrChar = '\0';
      else
        bDone = TRUE;
    }
  }
}

void AppendSlash(LPSTR szInput, DWORD dwInputSize)
{
  DWORD dwInputLen = strlen(szInput);

  if(szInput)
  {
    if(*szInput == '\0')
    {
      if((dwInputLen + 1) < dwInputSize)
      {
        strcat(szInput, "/");
      }
    }
    else if(*WinPrevChar(0, 0, 0, szInput, &szInput[dwInputLen]) != '/')
    {
      if((dwInputLen + 1) < dwInputSize)
      {
        strcat(szInput, "/");
      }
    }
  }
}

void ParsePath(PSZ szInput, PSZ szOutput, ULONG ulOutputSize, BOOL bURLPath, ULONG ulType)
{
  int   iFoundDelimiter;
  ULONG ulInputLen;
  ULONG ulOutputLen;
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
    ulInputLen    = strlen(szInput);
    memset(szOutput, 0, ulOutputSize);

    if(ulInputLen < ulOutputSize)
    {
      switch(ulType)
      {
        case PP_FILENAME_ONLY:
          ptrChar = &szInput[ulInputLen];
          bDone = FALSE;
          while(!bDone)
          {
            ptrChar = WinPrevChar(0, 0, 0, szInput, ptrChar);
            if(*ptrChar == cDelimiter)
            {
              strcpy(szOutput, WinNextChar(0, 0, 0, ptrChar));
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
          ulOutputLen = strlen(szOutput);
          ptrChar = &szOutput[ulOutputLen];
          bDone = FALSE;
          while(!bDone)
          {
            ptrChar = WinPrevChar(0, 0, 0, szOutput, ptrChar);
            if(*ptrChar == cDelimiter)
            {
              *WinNextChar(0, 0, 0, ptrChar) = '\0';
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
          ptrChar = WinPrevChar(0, 0, 0, szInput, &szInput[ulInputLen]);
          if(*ptrChar == '.')
            break;

          bDone = FALSE;
          while(!bDone)
          {
            ptrChar = WinPrevChar(0, 0, 0, szInput, ptrChar);
            if(*ptrChar == cDelimiter)
              /* found path delimiter before '.' */
              bDone = TRUE;
            else if(*ptrChar == '.')
            {
              strcpy(szOutput, WinNextChar(0, 0, 0, ptrChar));
              bDone = TRUE;
            }
            else if(ptrChar == szInput)
              bDone = TRUE;
          }
          break;

        case PP_ROOT_ONLY:
          strcpy(szOutput, szInput);
          ulOutputLen = strlen(szOutput);
          ptrLastChar = WinPrevChar(0, 0, 0, szOutput, &szOutput[ulOutputLen]);
          ptrChar     = WinNextChar(0, 0, 0, szOutput);
          if(*ptrChar == ':')
          {
            ptrChar = WinNextChar(0, 0, 0, ptrChar);
            *ptrChar = cDelimiter;
            *WinNextChar(0, 0, 0, ptrChar) = '\0';
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
                *WinNextChar(0, 0, 0, ptrChar) = '\0';
                bDone = TRUE;
              }
              else if(ptrChar == ptrLastChar)
                bDone = TRUE;
              else
                ptrChar = WinNextChar(0, 0, 0, ptrChar);
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
        char szSpawnFile[MAX_BUF];
        char szMessageString[MAX_BUF];
        DWORD dwErr = FO_SUCCESS;

        sprintf(szMessageString, szMsg, siCObject->szDescriptionShort);
        ShowMessage(szMessageString, TRUE);
        DecryptString(szParameterBuf, siCObject->szParameter);

        strcpy(szSpawnFile, szArchive);
        if(siCObject->dwAttributes & SIC_UNCOMPRESS)
        {
          if((dwErr = FileUncompress(szArchive, szTempDir)) == FO_SUCCESS)
          {
            strcpy(szSpawnFile, szTempDir);
            AppendBackSlash(szSpawnFile, sizeof(szSpawnFile));
            strcat(szSpawnFile, siCObject->szArchiveNameUncompressed);
          }

          LogISLaunchAppsComponentUncompress(siCObject->szDescriptionShort, dwErr);
          if(dwErr != FO_SUCCESS)
          {
            ShowMessage(szMessageString, FALSE);
            continue;
          }
        }

        LogISLaunchAppsComponent(siCObject->szDescriptionShort);
        WinSpawn(szSpawnFile, szParameterBuf, szTempDir, TRUE);

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

HRESULT ProcessOS2Integration()
{
  if (diOS2Integration.oiCBMakeDefaultBrowser.bCheckBoxState == TRUE) {
    HOBJECT hObjURL = NULLHANDLE;
    CHAR    szSetupString[1000];
    CHAR    szTemp[CCHMAXPATH];
    strcpy(szSetupString, "OBJECTID=<MOZTEMPCONVERSIONURL>");
    sprintf(szTemp, "%s\\%s", sgProduct.szPath, sgProduct.szProgramName);
    strcat(szSetupString, ";DEFAULTBROWSER=");
    strcat(szSetupString, szTemp) ;
    PrfWriteProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultBrowserExe",
                          szTemp ) ;

    strcat(szSetupString, ";DEFAULTWORKINGDIR=");
    strcat(szSetupString, sgProduct.szPath);
    PrfWriteProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultWorkingDir",
                          sgProduct.szPath);

    if (hObjURL = WinCreateObject("WPUrl",
                                  "Temporary URL",
                                  szSetupString,
                                  "<WP_NOWHERE>",
                                  CO_REPLACEIFEXISTS))
    {
      WinDestroyObject(hObjURL);
    }
  }
  return(0);
}

char *GetOSTypeString(char *szOSType, ULONG ulOSTypeBufSize)
{
  memset(szOSType, 0, ulOSTypeBufSize);

  if(gSystemInfo.ulOSType & OS_WARP3)
    strcpy(szOSType, "Warp 3");
  else
    strcpy(szOSType, "Warp 4");

  return(szOSType);
}

void DetermineOSVersionEx()
{
  char          szBuf[MAX_BUF];
  char          szOSType[MAX_BUF];
  ULONG         aulSysInfo[QSV_MAX+1];
  APIRET        rc;
  ULONG TotPhysMem,TotResMem,TotAvailMem;

  rc = DosQuerySysInfo(1,
                       QSV_MAX,
                       (PVOID)&aulSysInfo[1],
                       sizeof(ULONG)*QSV_MAX);

  if(rc != NO_ERROR)
  {
    /* DosQuerySysInfo() failed for some reason.  It's not fatal, but could cause
     * some complications during installation */
    char szEMsg[MAX_BUF_TINY];

    if(GetPrivateProfileString("Messages", "ERROR_GETVERSION", "", szEMsg, sizeof(szEMsg), szFileIniInstall))
      PrintError(szEMsg, ERROR_CODE_SHOW);
  }

  gSystemInfo.ulOSType = OS_WARP4; /* @MAK - should detect more stuff here */
  gSystemInfo.ulMajorVersion = aulSysInfo[QSV_VERSION_MAJOR];
  gSystemInfo.ulMinorVersion = aulSysInfo[QSV_VERSION_MINOR];
  gSystemInfo.ulBuildNumber  = aulSysInfo[QSV_VERSION_REVISION];
  memset(gSystemInfo.szExtraString, 0, sizeof(gSystemInfo.szExtraString));
  strcpy(gSystemInfo.szExtraString, "Seriously should be something"); /* @MAK */

  gSystemInfo.ulMemoryTotalPhysical = aulSysInfo[QSV_TOTPHYSMEM]/1024;
  gSystemInfo.ulMemoryAvailablePhysical = aulSysInfo[QSV_TOTAVAILMEM]/1024;

  GetOSTypeString(szOSType, sizeof(szOSType));
  sprintf(szBuf,
"    System Info:\n\
        OS Type: %s\n\
        Major Version: %d\n\
        Minor Version: %d\n\
        Build Number: %d\n\
        Extra String: %s\n\
        Total Physical Memory: %dKB\n\
        Total Available Physical Memory: %dKB\n",
           szOSType,
           gSystemInfo.ulMajorVersion,
           gSystemInfo.ulMinorVersion,
           gSystemInfo.ulBuildNumber,
           gSystemInfo.szExtraString,
           gSystemInfo.ulMemoryTotalPhysical,
           gSystemInfo.ulMemoryAvailablePhysical);

  UpdateInstallStatusLog(szBuf);
}

HRESULT WinSpawn(LPSTR szClientName, LPSTR szParameters, LPSTR szCurrentDir, BOOL bWait)
{
  STARTDATA startdata;
  PID       pid, endpid;
  ULONG     ulSessID;
  APIRET rc;
  RESULTCODES resultcodes;
  ULONG     ulFlags;
  
  rc = DosQueryAppType(szClientName, &ulFlags);
  if (rc == NO_ERROR) {
    memset(&startdata, 0, sizeof(STARTDATA));
    startdata.Length  = sizeof(STARTDATA);
    startdata.PgmName = szClientName;
    startdata.PgmInputs = szParameters;
    rc = DosStartSession(&startdata, &ulSessID, &pid);
    if (rc == NO_ERROR) {
      if (bWait) {
        DosWaitChild(DCWA_PROCESS, DCWW_NOWAIT, &resultcodes, &endpid, pid);
      }
      return (TRUE);
    }
  } else {
    CHAR szBuf[CCHMAXPATH];
    HOBJECT hobject;
    strcpy(szBuf, szCurrentDir);
    strcat(szBuf, szClientName);
    hobject = WinQueryObject(szBuf);
    WinSetFocus(HWND_DESKTOP, HWND_DESKTOP);
    WinOpenObject(hobject, 0, TRUE); // 0 = OPEN_DEFAULT
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

HRESULT InitDlgOS2Integration(diOI *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((diDialog->szMessage1 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((diDialog->szHomeDirectory = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);

  diDialog->oiCBMakeDefaultBrowser.bEnabled = FALSE;
  diDialog->oiCBAssociateHTML.bEnabled = FALSE;
  diDialog->oiCBUpdateCONFIGSYS.bEnabled = FALSE;

  diDialog->oiCBMakeDefaultBrowser.bCheckBoxState = FALSE;
  diDialog->oiCBAssociateHTML.bCheckBoxState = FALSE;
  diDialog->oiCBUpdateCONFIGSYS.bCheckBoxState = FALSE;

  if((diDialog->oiCBMakeDefaultBrowser.szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->oiCBAssociateHTML.szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->oiCBUpdateCONFIGSYS.szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgOS2Integration(diOI *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szMessage1));
  FreeMemory(&(diDialog->szHomeDirectory));

  FreeMemory(&(diDialog->oiCBMakeDefaultBrowser.szDescription));
  FreeMemory(&(diDialog->oiCBAssociateHTML.szDescription));
  FreeMemory(&(diDialog->oiCBUpdateCONFIGSYS.szDescription));
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

ULONG InitDlgReboot(diR *diDialog)
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

  sgProduct.ulMode               = NORMAL;
  sgProduct.ulCustomType         = ST_RADIO0;
  sgProduct.ulNumberOfComponents = 0;
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

  if(GetPrivateProfileString("Messages", "CB_DEFAULT", "", szBuf, sizeof(szBuf), szFileIniInstall))
    strcpy(szSiteSelectorDescription, szBuf);

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
  FreeMemory(&(szTempSetupPath));
  FreeMemory(&(szSiteSelectorDescription));
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
  siCFXpcomFile.ulInstallSize   = 0;
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
  siCNode->ulInstallSize           = 0;
  siCNode->ulInstallSizeSystem     = 0;
  siCNode->ulInstallSizeArchive    = 0;
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
  strcpy(szAttribute, szBuf);
  strupr(szAttribute);

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
  strcpy(szAttribute, szBuf);
  strupr(szAttribute);

  if(strstr(szAttribute, "ADDITIONAL") && !strstr(szAttribute, "NOTADDITIONAL"))
    siCNode->dwAttributes |= SIC_ADDITIONAL;
  else
    siCNode->dwAttributes &= ~SIC_ADDITIONAL;
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

  strcpy(szSTSection, "Setup Type");
  _itoa(dwSetupType, szBuf, 10);
  strcat(szSTSection, szBuf);

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
    ResolveSupersede(siCNode);
    siCNode = siCNode->Next;
  } while((siCNode != NULL) && (siCNode != siComponents));

  /* for each component in a setup type, set its ATTRIBUTES to the right values */
  dwIndex0 = 0;
  sprintf(szComponentKey, "C%d", dwIndex0);
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

      sprintf(szOverrideSection, "%s-Override-%s", siCNode->szReferenceName, szSTSection);
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
      else if(sgProduct.ulCustomType != dwSetupType)
      {
        /* Setup Type other than custom detected, so
         * make sure all components from this Setup Type
         * is selected (regardless if it's DISABLED or not). 
         * Don't select components that are Superseded */
        if(!siCNode->bSupersede)
          siCNode->dwAttributes |= SIC_SELECTED;

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
    sprintf(szComponentKey, "C%d", dwIndex0);
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

unsigned long long SiCNodeGetInstallSize(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
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
        return(siCTemp->ulInstallSize);

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
          return(siCTemp->ulInstallSize);
      
        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(0L);
}

unsigned long long SiCNodeGetInstallSizeSystem(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
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
        return(siCTemp->ulInstallSizeSystem);

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
          return(siCTemp->ulInstallSizeSystem);
      
        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(0L);
}

unsigned long long SiCNodeGetInstallSizeArchive(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
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
        return(siCTemp->ulInstallSizeArchive);

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
          return(siCTemp->ulInstallSizeArchive);
      
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
    if(stricmp(szInDescriptionShort, siCTemp->szDescriptionShort) == 0)
      return(dwCount);

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(stricmp(szInDescriptionShort, siCTemp->szDescriptionShort) == 0)
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
    if(stricmp(szInReferenceName, siCTemp->szReferenceName) == 0)
      return(dwCount);

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(stricmp(szInReferenceName, siCTemp->szReferenceName) == 0)
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

  dsNode->ulSpaceRequired = 0;

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

/* returns the value in kilobytes */
unsigned long long GetDiskSpaceRequired(DWORD dwType)
{
  unsigned long long ullTotalSize = 0;
  siC       *siCTemp     = siComponents;

  if(siCTemp != NULL)
  {
    if(siCTemp->dwAttributes & SIC_SELECTED)
    {
      switch(dwType)
      {
        case DSR_DESTINATION:
          ullTotalSize += siCTemp->ulInstallSize;
          break;

        case DSR_SYSTEM:
          ullTotalSize += siCTemp->ulInstallSizeSystem;
          break;

        case DSR_TEMP:
        case DSR_DOWNLOAD_SIZE:
          if((LocateJar(siCTemp, NULL, 0, gbPreviousUnfinishedDownload) == AP_NOT_FOUND) ||
             (dwType == DSR_DOWNLOAD_SIZE))
            ullTotalSize += siCTemp->ulInstallSizeArchive;
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
            ullTotalSize += siCTemp->ulInstallSize;
            break;

          case DSR_SYSTEM:
            ullTotalSize += siCTemp->ulInstallSizeSystem;
            break;

          case DSR_TEMP:
          case DSR_DOWNLOAD_SIZE:
            if((LocateJar(siCTemp, NULL, 0, gbPreviousUnfinishedDownload) == AP_NOT_FOUND) ||
               (dwType == DSR_DOWNLOAD_SIZE))
              ullTotalSize += siCTemp->ulInstallSizeArchive;
            break;
        }
      }

      siCTemp = siCTemp->Next;
    }
  }

  /* add the amount of disk space it will take for the 
     xpinstall engine in the TEMP area */
  if(dwType == DSR_TEMP)
    ullTotalSize += siCFXpcomFile.ulInstallSize;

  return(ullTotalSize);
}

int LocateExistingPath(char *szPath, char *szExistingPath, DWORD dwExistingPathSize)
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
unsigned long long GetDiskSpaceAvailable(LPSTR szPath)
{
  char szBuf[MAX_BUF];
  char szBuf2[MAX_BUF];
  FSALLOCATE fsAllocate;
  unsigned long long nBytes = 0;
  APIRET rc;
  ULONG ulDriveNo;

  ulDriveNo = toupper(szPath[0]) + 1 - 'A';
  rc = DosQueryFSInfo(ulDriveNo,
                      FSIL_ALLOC,
                      &fsAllocate,
                      sizeof(fsAllocate));

  if (rc != NO_ERROR) {
    char szEDeterminingDiskSpace[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_DETERMINING_DISK_SPACE", "", szEDeterminingDiskSpace, sizeof(szEDeterminingDiskSpace), szFileIniInstall))
    {
      strcpy(szBuf2, "\n    ");
      strcat(szBuf2, szPath);
      sprintf(szBuf, szEDeterminingDiskSpace, szBuf2);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
  }

  nBytes = fsAllocate.cUnitAvail;
  nBytes *= fsAllocate.cSectorUnit;
  nBytes *= fsAllocate.cbSector;
  if (nBytes > 1024)
     nBytes /= 1024;
  else
     nBytes = 0;

  return nBytes;
}

HRESULT ErrorMsgDiskSpace(unsigned long long ullDSAvailable, unsigned long long ullDSRequired, LPSTR szPath, BOOL bCrutialMsg)
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
    if(!GetPrivateProfileString("Messages", "DLG_DISK_SPACE_CHECK_CRUCIAL_MSG", "", szDlgDiskSpaceCheckMsg, sizeof(szDlgDiskSpaceCheckMsg), szFileIniInstall))
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
  strcpy(szBuf0, szPath);
  RemoveBackSlash(szBuf0);

  _itoa(ullDSAvailable, szDSAvailable, 10);
  _itoa(ullDSRequired, szDSRequired, 10);

  sprintf(szBuf1, "\n\n    %s\n\n    ", szBuf0);
  sprintf(szBuf2, "%s KB\n    ",        szDSRequired);
  sprintf(szBuf3, "%s KB\n\n",          szDSAvailable);
  sprintf(szBufMsg, szDlgDiskSpaceCheckMsg, szBufRootPath, szBuf1, szBuf2, szBuf3);

  if((sgProduct.ulMode != SILENT) && (sgProduct.ulMode != AUTO))
  {
    return(WinMessageBox(HWND_DESKTOP, hWndMain, szBufMsg, szDlgDiskSpaceCheckTitle, 0, dwDlgType | MB_ICONEXCLAMATION | MB_DEFBUTTON2 | MB_APPLMODAL));
  }
  else if(sgProduct.ulMode == AUTO)
  {
    ShowMessage(szBufMsg, TRUE);
    DosSleep(5000);
    ShowMessage(szBufMsg, FALSE);
    exit(1);
  }

  return(MBID_CANCEL);
}

void UpdatePathDiskSpaceRequired(LPSTR szPath, unsigned long long ullSize, dsN **dsnComponentDSRequirement)
{
  BOOL  bFound = FALSE;
  dsN   *dsnTemp = *dsnComponentDSRequirement;
  char  szReparsePath[MAX_BUF];
  char  szVDSPath[MAX_BUF];
  char  szRootPath[MAX_BUF];

  if(ullSize > 0)
  {
    ParsePath(szPath, szRootPath, sizeof(szRootPath), FALSE, PP_ROOT_ONLY);

    if(GetDiskSpaceAvailable(szRootPath) == GetDiskSpaceAvailable(szPath))
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
        dsnTemp->ulSpaceRequired = ullSize;
        bFound = TRUE;
      }
      else if(stricmp(dsnTemp->szVDSPath, szVDSPath) == 0)
      {
        dsnTemp->ulSpaceRequired += ullSize;
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
      dsnTemp->ulSpaceRequired = ullSize;
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
  char      szBufTempPath[MAX_BUF];

  ParsePath(szTempDir, szBufTempPath, sizeof(szBufTempPath), FALSE, PP_ROOT_ONLY);
  AppendBackSlash(szBufTempPath, sizeof(szBufTempPath));

  dwIndex0 = 0;
  _itoa(dwIndex0, szIndex0, 10);
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
      UpdatePathDiskSpaceRequired(szBuf, siCObject->ulInstallSize, dsnComponentDSRequirement);

      if(*szTempDir != '\0')
        UpdatePathDiskSpaceRequired(szTempDir, siCObject->ulInstallSizeArchive, dsnComponentDSRequirement);
    }

    ++dwIndex0;
    _itoa(dwIndex0, szIndex0, 10);
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  /* take the uncompressed size of Xpcom into account */
  if(*szTempDir != '\0')
    UpdatePathDiskSpaceRequired(szTempDir, siCFXpcomFile.ulInstallSize, dsnComponentDSRequirement);

  return(hResult);
}

HRESULT VerifyDiskSpace()
{
  unsigned long long ullDSAvailable;
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
        if(ullDSAvailable < dsnTemp->ulSpaceRequired)
        {
          hRetValue = ErrorMsgDiskSpace(ullDSAvailable, dsnTemp->ulSpaceRequired, dsnTemp->szPath, FALSE);
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
ULONG ParseOSType(char *szOSType)
{
  char  szBuf[MAX_BUF];
  ULONG ulOSType = 0;

  strcpy(szBuf, szOSType);
  strupr(szBuf);

  if(strstr(szBuf, "WARP4"))
    ulOSType |= OS_WARP4;
  if(strstr(szBuf, "WARP3"))
    ulOSType |= OS_WARP3;

  return(ulOSType);
}

HRESULT ParseComponentAttributes(char *szAttribute, DWORD dwAttributes, BOOL bOverride)
{
  char  szBuf[MAX_BUF];

  strcpy(szBuf, szAttribute);
  strupr(szBuf);

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

    if(stricmp(siCNode->szReferenceName, szInReferenceName) == 0)
      return(siCNode);

    siCNode = siCNode->Next;
  } while((siCNode != NULL) && (siCNode != siCHeadNode));

  return(NULL);
}

BOOL ResolveSupersede(siC *siCObject)
{
  DWORD dwIndex;
  char  szFilePath[MAX_BUF];
  char  szSupersedeFile[MAX_BUF];
  char  szSupersedeVersion[MAX_BUF];
  char  szType[MAX_BUF_TINY];
  char  szKey[MAX_BUF_TINY];

  siCObject->bSupersede = FALSE;
  if(siCObject->dwAttributes & SIC_SUPERSEDE)
  {
    dwIndex = 0;
    GetPrivateProfileString(siCObject->szReferenceName, "SupersedeType", "", szType, sizeof(szType), szFileIniConfig);
    if(*szType !='\0')
    {
      if(stricmp(szType, "File Exists") == 0)
      {
        sprintf(szKey, "SupersedeFile%d", dwIndex);        
        GetPrivateProfileString(siCObject->szReferenceName, szKey, "", szSupersedeFile, sizeof(szSupersedeFile), szFileIniConfig);
        while(*szSupersedeFile != '\0')
        {
          DecryptString(szFilePath, szSupersedeFile);
          if(FileExists(szFilePath))
          {
            /* The file exists.  set Supersede */
            siCObject->bSupersede = TRUE;
            break;  /* Found at least one file, so break out of while loop */
          }
          sprintf(szKey, "SupersedeFile%d", ++dwIndex);        
          GetPrivateProfileString(siCObject->szReferenceName, szKey, "", szSupersedeFile, sizeof(szSupersedeFile), szFileIniConfig);
        }
      }
    }

    if(siCObject->bSupersede)
    {
      siCObject->dwAttributes &= ~SIC_SELECTED;
      siCObject->dwAttributes |= SIC_DISABLED;
      siCObject->dwAttributes |= SIC_INVISIBLE;
    }
    else
      /* Make sure to unset the DISABLED bit.  If the Setup Type is other than
       * Custom, then we don't care if it's DISABLED or not because this flag
       * is only used in the Custom dialogs.
       *
       * If the Setup Type is Custom and this component is DISABLED by default
       * via the config.ini, it's default value will be restored in the
       * SiCNodeSetItemsSelected() function that called ResolveSupersede(). */
      siCObject->dwAttributes &= ~SIC_DISABLED;
  }
  return(siCObject->bSupersede);
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
    strcpy(szSTSection, "Setup Type");
    _itoa(iCurrentLoop, szBuf, 10);
    strcat(szSTSection, szBuf);

    /* read in each component given a setup type */
    dwIndex0 = 0;
    _itoa(dwIndex0, szIndex0, 10);
    strcpy(szComponentKey, "C");
    strcat(szComponentKey, szIndex0);
    GetPrivateProfileString(szSTSection, szComponentKey, "", szComponentSection, sizeof(szComponentSection), szFileIni);
    while(*szComponentSection != '\0')
    {
      GetPrivateProfileString(szComponentSection, "Archive", "", szBuf, sizeof(szBuf), szFileIni);
      if((*szBuf != '\0') && (SiCNodeFind(siComponents, szComponentSection) == NULL))
      {
        /* create and initialize empty node */
        siCTemp = CreateSiCNode();

        /* store name of archive for component */
        strcpy(siCTemp->szArchiveName, szBuf);

        /* store name of the uncompressed archive for the component */
        GetPrivateProfileString(szComponentSection,
                                "Archive Uncompressed",
                                "",
                                siCTemp->szArchiveNameUncompressed,
                                sizeof(szBuf),
                                szFileIni);
        
        /* get short description of component */
        GetPrivateProfileString(szComponentSection, "Description Short", "", szBuf, sizeof(szBuf), szFileIni);
        strcpy(siCTemp->szDescriptionShort, szBuf);

        /* get long description of component */
        GetPrivateProfileString(szComponentSection, "Description Long", "", szBuf, sizeof(szBuf), szFileIni);
        strcpy(siCTemp->szDescriptionLong, szBuf);

        /* get commandline parameter for component */
        GetPrivateProfileString(szComponentSection, "Parameter", "", siCTemp->szParameter, MAX_BUF, szFileIni);

        /* set reference name for component */
        strcpy(siCTemp->szReferenceName, szComponentSection);

        /* get install size required in destination for component.  Sould be in Kilobytes */
        GetPrivateProfileString(szComponentSection, "Install Size", "", szBuf, sizeof(szBuf), szFileIni);
        if(*szBuf != '\0')
          siCTemp->ulInstallSize = atoi(szBuf);
        else
          siCTemp->ulInstallSize = 0;

        /* get install size required in system for component.  Sould be in Kilobytes */
        GetPrivateProfileString(szComponentSection, "Install Size System", "", szBuf, sizeof(szBuf), szFileIni);
        if(*szBuf != '\0')
          siCTemp->ulInstallSizeSystem = atoi(szBuf);
        else
          siCTemp->ulInstallSizeSystem = 0;

        /* get install size required in temp for component.  Sould be in Kilobytes */
        GetPrivateProfileString(szComponentSection, "Install Size Archive", "", szBuf, sizeof(szBuf), szFileIni);
        if(*szBuf != '\0')
          siCTemp->ulInstallSizeArchive = atoi(szBuf);
        else
          siCTemp->ulInstallSizeArchive = 0;

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
        _itoa(dwIndex1, szIndex1, 10);
        strcpy(szDependency, "Dependency");
        strcat(szDependency, szIndex1);
        GetPrivateProfileString(szComponentSection, szDependency, "", szBuf, sizeof(szBuf), szFileIni);
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
          _itoa(dwIndex1, szIndex1, 10);
          strcpy(szDependency, "Dependency");
          strcat(szDependency, szIndex1);
          GetPrivateProfileString(szComponentSection, szDependency, "", szBuf, sizeof(szBuf), szFileIni);
        }

        /* get all dependees for this component */
        dwIndex1 = 0;
        _itoa(dwIndex1, szIndex1, 10);
        strcpy(szDependee, "Dependee");
        strcat(szDependee, szIndex1);
        GetPrivateProfileString(szComponentSection, szDependee, "", szBuf, sizeof(szBuf), szFileIni);
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
          _itoa(dwIndex1, szIndex1, 10);
          strcpy(szDependee, "Dependee");
          strcat(szDependee, szIndex1);
          GetPrivateProfileString(szComponentSection, szDependee, "", szBuf, sizeof(szBuf), szFileIni);
        }

        // locate previous path if necessary
        strcpy(szDPSection, szComponentSection);
        strcat(szDPSection, "-Destination Path");
        if(LocatePreviousPath(szDPSection, siCTemp->szDestinationPath, MAX_PATH) == FALSE)
          memset(siCTemp->szDestinationPath, 0, MAX_PATH);

        /* inserts the newly created component into the global component list */
        SiCNodeInsert(&siComponents, siCTemp);
      }

      ProcessWindowsMessages();
      ++dwIndex0;
      _itoa(dwIndex0, szIndex0, 10);
      strcpy(szComponentKey, "C");
      strcat(szComponentKey, szIndex0);
      GetPrivateProfileString(szSTSection, szComponentKey, "", szComponentSection, sizeof(szComponentSection), szFileIni);
    }
  }

  sgProduct.ulNumberOfComponents = dwIndex0;
}

void ResetComponentAttributes(char *szFileIni)
{
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szComponentItem[MAX_BUF];
  siC   *siCTemp;
  DWORD dwCounter;

  for(dwCounter = 0; dwCounter < sgProduct.ulNumberOfComponents; dwCounter++)
  {
    _itoa(dwCounter, szIndex, 10);
    strcpy(szComponentItem, "Component");
    strcat(szComponentItem, szIndex);

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

  strcpy(szFileIniRedirect, szTempDir);
  AppendBackSlash(szFileIniRedirect, sizeof(szFileIniRedirect));
  strcat(szFileIniRedirect, FILE_INI_REDIRECT);

  if(FileExists(szFileIniRedirect) == FALSE)
    return;

  /* get all dependees for this component */
  dwIndex = 0;
  _itoa(dwIndex, szIndex, 10);
  strcpy(szKDescription, "Description");
  strcpy(szKDomain,      "Domain");
  strcat(szKDescription, szIndex);
  strcat(szKDomain,      szIndex);
  GetPrivateProfileString("Site Selector", szKDescription, "", szDescription, sizeof(szDescription), szFileIniRedirect);
  while(*szDescription != '\0')
  {
    if(stricmp(szDescription, szSiteSelectorDescription) == 0)
    {
      GetPrivateProfileString("Site Selector", szKDomain, "", szDomain, sizeof(szDomain), szFileIniRedirect);
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
    _itoa(dwIndex, szIndex, 10);
    strcpy(szKDescription, "Description");
    strcpy(szKDomain,      "Domain");
    strcat(szKDescription, szIndex);
    strcat(szKDomain,      szIndex);
    memset(szDescription, 0, sizeof(szDescription));
    memset(szDomain, 0,      sizeof(szDomain));
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
  _itoa(dwIndex, szIndex, 10);
  strcpy(szKDescription, "Description");
  strcpy(szKDomain,      "Domain");
  strcpy(szKIdentifier,  "Identifier");
  strcat(szKDescription, szIndex);
  strcat(szKDomain,      szIndex);
  strcat(szKIdentifier,  szIndex);
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

      strcpy(ssiSiteSelectorNewNode->szDescription, szDescription);
      strcpy(ssiSiteSelectorNewNode->szDomain,      szDomain);
      strcpy(ssiSiteSelectorNewNode->szIdentifier,  szIdentifier);

      /* inserts the newly created node into the global node list */
      SsiSiteSelectorNodeInsert(&(ssiSiteSelector), ssiSiteSelectorNewNode);
    }

    ProcessWindowsMessages();
    ++dwIndex;
    _itoa(dwIndex, szIndex, 10);
    strcpy(szKDescription, "Description");
    strcpy(szKDomain,      "Domain");
    strcpy(szKIdentifier,  "Identifier");
    strcat(szKDescription, szIndex);
    strcat(szKDomain,      szIndex);
    strcat(szKIdentifier,  szIndex);
    memset(szDescription, 0, sizeof(szDescription));
    memset(szDomain, 0,      sizeof(szDomain));
    memset(szIdentifier, 0,  sizeof(szIdentifier));
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

  if(stricmp(szBuf, "disabled") == 0)
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
  if(stricmp(szBuf, "FALSE") == 0)
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
    DWORD dwIndex;

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

    if(stricmp(ssiSiteSelectorTemp->szDescription, szDescription) == 0)
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
       (stricmp(siCTemp->szReferenceName, szToggledReferenceName) != 0))
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

void PrintUsage(PSZ szAppName)
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
    strcpy(szProcessFilename, sgProduct.szParentProcessFilename);
  else
  {
    strcpy(szBuf, szAppName);
    ParsePath(szBuf, szProcessFilename, sizeof(szProcessFilename), FALSE, PP_FILENAME_ONLY);
  }

  GetPrivateProfileString("Strings", "UsageMsg Usage", "", szBuf, sizeof(szBuf), szFileIniConfig);
  sprintf(szUsageMsg, szBuf, szProcessFilename, "\n", "\n", "\n", "\n", "\n", "\n", "\n", "\n", "\n");

  PrintError(szUsageMsg, ERROR_CODE_HIDE);
}

ULONG ParseCommandLine(int argc, char *argv[])
{
  char  szArgVBuf[MAX_BUF];
  int   i;
  int   iArgC;

#ifdef XXX_DEBUG
  char  szBuf[MAX_BUF];
  char  szOutputStr[MAX_BUF];
#endif

#ifdef XXX_DEBUG
  sprintf(szOutputStr, "ArgC: %d\n", iArgC);
#endif

  i = 0;
  while(i < argc)
  {
    if(!stricmp(argv[i], "-h") || !stricmp(argv[i], "/h"))
    {
      PrintUsage(argv[0]);
      return(WIZ_ERROR_UNDEFINED);
    }
    else if(!stricmp(argv[i], "-a") || !stricmp(argv[i], "/a"))
    {
      ++i;
      strcpy(sgProduct.szAlternateArchiveSearchPath, argv[i]);
    }
    else if(!stricmp(argv[i], "-n") || !stricmp(argv[i], "/n"))
    {
      ++i;
      strcpy(sgProduct.szParentProcessFilename, argv[i]);
    }
    else if(!stricmp(argv[i], "-ma") || !stricmp(argv[i], "/ma"))
      SetSetupRunMode("AUTO");
    else if(!stricmp(argv[i], "-ms") || !stricmp(argv[i], "/ms"))
      SetSetupRunMode("SILENT");
    else if(!stricmp(argv[i], "-ira") || !stricmp(argv[i], "/ira"))
      /* ignore [RunAppX] sections */
      gbIgnoreRunAppX = TRUE;
    else if(!stricmp(argv[i], "-ispf") || !stricmp(argv[i], "/ispf"))
      /* ignore [Program FolderX] sections */
      gbIgnoreProgramFolderX = TRUE;

#ifdef XXX_DEBUG
    _itoa(i, szBuf, 10);
    strcat(szOutputStr, "    ");
    strcat(szOutputStr, szBuf);
    strcat(szOutputStr, ": ");
    strcat(szOutputStr, argv[i]);
    strcat(szOutputStr, "\n");
#endif

    ++i;
  }

#ifdef XXX_DEBUG
  WinMessageBox(HWND_DESKTOP, NULL, szOutputStr, "Output", 0, MB_OK);
#endif
  return(WIZ_OK);
}

void GetAlternateArchiveSearchPath(LPSTR lpszCmdLine)
{
  char  szBuf[MAX_PATH];
  LPSTR lpszAASPath;
  LPSTR lpszEndPath;
  LPSTR lpszEndQuote;

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

#define BUFMIN	8*1024
#define BUFMAX	256*1024
#define BUFDEFAULT 32*1024

BOOL CheckForProcess(PID pid, LPSTR szProcessName, DWORD dwProcessName, PSZ szFQProcessName, DWORD dwFQProcessName)
{
/* Only compile this code if we have the new toolkit */
#ifdef QS_PROCESS
  ULONG bufsize = BUFDEFAULT;
  QSPTRREC* pbh;
  APIRET rc = 0;
  CHAR szUpperAppName[CCHMAXPATH] = {0};

  /* Can't call with both - only one or the other */
  if (pid && szProcessName) {
    return FALSE;
  }
  if (szProcessName) {
    strcpy(szUpperAppName, szProcessName);
    strupr(szUpperAppName);
  }
  do {
    pbh = (QSPTRREC*) malloc(bufsize);
    if(!pbh) {
      if(bufsize <= BUFMIN)
        rc = ERROR_NOT_ENOUGH_MEMORY;
      else if(rc != ERROR_BUFFER_OVERFLOW)
        bufsize /= 2;
    } else {
      rc = DosQuerySysState(QS_PROCESS | QS_MTE, 0, 0, 0, pbh, bufsize);
      if(rc == ERROR_BUFFER_OVERFLOW) {
        if(bufsize < BUFMAX) {
          free(pbh);
          bufsize *= 2;
        } else {
          rc = ERROR_TOO_MANY_NAMES;    // give up.
        }
      }
    }
  } while(rc == ERROR_BUFFER_OVERFLOW);

  if(rc == NO_ERROR) {
    QSPREC* ppiLocal = pbh->pProcRec;
    while(ppiLocal->RecType == QS_PROCESS) {
      QSLREC* pmi = pbh->pLibRec;
      while (pmi && pmi->hmte != ppiLocal->hMte)
        pmi = (QSLREC*)pmi->pNextRec;
      if(pmi) {
        if ((szUpperAppName[0] && strstr((char*)pmi->pName, szUpperAppName)) ||
           (ppiLocal->pid == pid)) {
            if (szFQProcessName)
              strcpy(szFQProcessName, (char*)pmi->pName);
            if (pbh)
              free(pbh);
            return TRUE;
        }
      }
      ppiLocal=(QSPREC*)(ppiLocal->pThrdRec+ppiLocal->cTCB);
    }
  }
  if(pbh)
    free(pbh);
#endif
  return FALSE;
}

int PreCheckInstance(char *szSection, char *szIniFile, char *szFQProcessName)
{
  char  szParameter[MAX_BUF];
  char  szPath[MAX_BUF];
  ULONG ulCounter = 0;
  BOOL  bContinue = TRUE;
  char  szExtraCmd[] = "Extra Cmd";
  char  szExtraCmdParameter[MAX_BUF];

  do
  {
    /* Read the win reg key path */
    sprintf(szExtraCmdParameter, "%s%d Parameter", szExtraCmd, ulCounter);
    GetPrivateProfileString(szSection,
                            szExtraCmdParameter,
                            "",
                            szParameter,
                            sizeof(szParameter),
                            szIniFile);
    if(*szParameter == '\0')
    {
      bContinue = FALSE;
      continue;
    }

    ParsePath(szFQProcessName, szPath, sizeof(szPath), FALSE, PP_PATH_ONLY);

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
    WinSpawn(szFQProcessName, szParameter, szPath, TRUE);

    /* Even though WinSpawn is suppose to wait for the app to finish, this
     * does not really work that way for trying to quit the browser when
     * it's in turbo mode, so we wait 2 secs for it to complete. */
    DosSleep(2000);
    
    ++ulCounter;
  } while(bContinue);

  return(WIZ_OK);
}

ULONG CloseAllWindowsOfWindowHandle(HWND hwndWindow)
{
  HENUM henum;
  HWND hwnd;
  PID mainpid, pid;
  TID tid;

  WinQueryWindowProcess(hwndWindow, &mainpid, &tid);

  henum = WinBeginEnumWindows(HWND_DESKTOP);
  while ((hwnd = WinGetNextWindow(henum)) != NULLHANDLE)
  {
    WinQueryWindowProcess(hwnd, &pid, &tid);
    if (pid == mainpid) {
      if (WinIsWindowVisible(hwnd)) {
        MRESULT rc = WinSendMsg(hwnd, WM_CLOSE, 0, 0);
        printf("rc = %x\n", rc);
      }
    }
  }
  WinEndEnumWindows(henum);
  /* The windows don't close quick enough, so we need to wait a bit */
  DosSleep(2500);

  return(WIZ_OK);
}

HRESULT CheckInstances()
{
  char  szSection[MAX_BUF];
  char  szProcessName[CCHMAXPATH];
  char  szFQProcessName[CCHMAXPATH];
  char  szClassName[MAX_BUF];
  char  szCloseAllWindows[MAX_BUF];
  char  szAttention[MAX_BUF];
  char  szMessage[MAX_BUF];
  char  szMessageFulInstaller[MAX_BUF];
  char  szIndex[MAX_BUF];
  int   iIndex;
  BOOL  bContinue;
  BOOL  bCloseAllWindows;
  HWND  hwndFW;
  LPSTR szCN;
  DWORD dwRv0;
  DWORD dwRv1;

  bContinue = TRUE;
  iIndex    = -1;
  while(bContinue)
  {
    memset(szClassName,            0, sizeof(szClassName));
    memset(szMessage,              0, sizeof(szMessage));
    memset(szMessageFulInstaller, 0, sizeof(szMessageFulInstaller));

    ++iIndex;
    _itoa(iIndex, szIndex, 10);
    strcpy(szSection, "Check Instance");
    strcat(szSection, szIndex);

    GetPrivateProfileString("Messages", "MB_ATTENTION_STR", "", szAttention, sizeof(szAttention), szFileIniInstall);
    GetPrivateProfileString(szSection, "Message", "", szMessage, sizeof(szMessage), szFileIniConfig);
    GetPrivateProfileString(szSection, "Message Full Installer", "", szMessageFulInstaller, sizeof(szMessageFulInstaller), szFileIniConfig);
    if(!gbDownloadTriggered && !gbPreviousUnfinishedDownload && (*szMessageFulInstaller != '\0'))
      strcpy(szMessage, szMessageFulInstaller);

    if(GetPrivateProfileString(szSection, "Process Name", "", szProcessName, sizeof(szProcessName), szFileIniConfig) != 0L)
    {
      if(*szProcessName != '\0')
      {
        /* If an instance is found, call PreCheckInstance first */
        if(CheckForProcess(0, szProcessName, sizeof(szProcessName), szFQProcessName, sizeof(szFQProcessName)) == TRUE)
          PreCheckInstance(szSection, szFileIniConfig, szFQProcessName);

        if(CheckForProcess(0, szProcessName, sizeof(szProcessName), NULL, 0) == TRUE)
        {
          if(*szMessage != '\0')
          {
            switch(sgProduct.ulMode)
            {
              case NORMAL:
                switch(WinMessageBox(HWND_DESKTOP, hWndMain, szMessage, szAttention, 0, MB_ICONEXCLAMATION | MB_OKCANCEL))
                {
                  case MBID_CANCEL:
                    /* User selected to cancel Setup */
                    return(TRUE);

                  case MBID_RETRY:
                  case MBID_OK:
                    /* User selected to retry.  Reset counter */
                    iIndex = -1;
                    break;
                }
                break;

              case AUTO:
                ShowMessage(szMessage, TRUE);
                DosSleep(5000);
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

    GetPrivateProfileString(szSection, "Close All Process Windows", "", szCloseAllWindows, sizeof(szCloseAllWindows), szFileIniConfig);
    if(stricmp(szCloseAllWindows, "TRUE") == 0)
      bCloseAllWindows = TRUE;
    else
      bCloseAllWindows = FALSE;

    /* Process Name= key did not exist, so look for other keys */
    dwRv0 = GetPrivateProfileString(szSection, "Class Name",  "", szClassName,  sizeof(szClassName), szFileIniConfig);
    if (dwRv0 == 0L)
    {
      bContinue = FALSE;
    }
    else if(*szClassName != '\0')
    {
      if(*szClassName == '\0')
        szCN = NULL;
      else
        szCN = szClassName;

      /* If an instance is found, call PreCheckInstance first */
      if((hwndFW = FindWindow(szCN)) != NULL) {
        PID pid;
        TID tid;
        WinQueryWindowProcess(hwndFW, &pid, &tid);
        CheckForProcess(pid, NULL, 0, szFQProcessName, sizeof(szFQProcessName));
        PreCheckInstance(szSection, szFileIniConfig, szFQProcessName);
      }

      if((hwndFW = FindWindow(szCN)) != NULL)
      {
        if(*szMessage != '\0')
        {
          switch(sgProduct.ulMode)
          {
            case NORMAL:
              switch(WinMessageBox(HWND_DESKTOP, hWndMain, szMessage, szAttention, 0, MB_ICONEXCLAMATION | MB_OKCANCEL))
              {
                case MBID_CANCEL:
                  /* User selected to cancel Setup */
                  return(TRUE);

                case MBID_RETRY:
                case MBID_OK:
                  /* User selected to retry.  Reset counter */
                  if(bCloseAllWindows)
                      CloseAllWindowsOfWindowHandle(hwndFW);

                  iIndex = -1;
                  break;
              }
              break;

            case AUTO:
              /* Setup mode is AUTO.  Show message, timeout, then auto close
               * all the windows associated with the process */

              ShowMessage(szMessage, TRUE);
              DosSleep(5000);
              ShowMessage(szMessage, FALSE);

              if(bCloseAllWindows)
                CloseAllWindowsOfWindowHandle(hwndFW);

              return(TRUE);

            case SILENT:
              /* Setup mode is SILENT.  Just auto close
               * all the windows associated with the process */

              if(bCloseAllWindows)
                CloseAllWindowsOfWindowHandle(hwndFW);

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
    memset(szCorruptedArchiveList, 0, dwCorruptedArchiveListSize);

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
       (stricmp(szPartiallyDownloadedFilename,
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
            if((DWORD)(strlen(szCorruptedArchiveList) + strlen(siCObject->szArchiveName + 1)) < dwCorruptedArchiveListSize)
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
      switch(sgProduct.ulMode)
      {
        case NORMAL:
          if(GetPrivateProfileString("Messages", "STR_MESSAGEBOX_TITLE", "", szBuf, sizeof(szBuf), szFileIniInstall))
            strcpy(szTitle, "Setup");
          else
            sprintf(szTitle, szBuf, sgProduct.szProductName);

          GetPrivateProfileString("Strings", "Error Corrupted Archives Detected", "", szBuf, sizeof(szBuf), szFileIniConfig);
          if(*szBuf != '\0')
          {
            strcpy(szBuf2, "\n\n");
            strcat(szBuf2, szCorruptedArchiveList);
            strcat(szBuf2, "\n");
            sprintf(szMsg, szBuf, szBuf2);
          }
          WinMessageBox(HWND_DESKTOP, hWndMain, szMsg, szTitle, 0, MB_OK | MB_ERROR);
          break;

        case AUTO:
          GetPrivateProfileString("Strings", "Error Corrupted Archives Detected AUTO mode", "", szBuf, sizeof(szBuf), szFileIniConfig);
          ShowMessage(szBuf, TRUE);
          DosSleep(5000);
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

HRESULT ParseConfigIni(int argc, char *argv[])
{
  int  iRv;
  char szBuf[MAX_BUF];
  char szMsgInitSetup[MAX_BUF];
  char szPreviousPath[MAX_BUF];
  char szShowDialog[MAX_BUF];
  DWORD dwPreviousUnfinishedState;

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
  if(InitDlgOS2Integration(&diOS2Integration))
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
  if(InitSXpcomFile())
    return(1);
 
  /* get install Mode information */
  GetPrivateProfileString("General", "Run Mode", "", szBuf, sizeof(szBuf), szFileIniConfig);
  SetSetupRunMode(szBuf);
  if(ParseCommandLine(argc, argv))
    return(1);

  if(GetPrivateProfileString("Messages", "MSG_INIT_SETUP", "", szMsgInitSetup, sizeof(szMsgInitSetup), szFileIniInstall))
    ShowMessage(szMsgInitSetup, TRUE);

  /* get product name description */
  GetPrivateProfileString("General", "Company Name", "", sgProduct.szCompanyName, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Product Name", "", sgProduct.szProductName, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Product Name Internal", "", sgProduct.szProductNameInternal, MAX_BUF, szFileIniConfig);
  if (sgProduct.szProductNameInternal[0] == 0)
    strcpy(sgProduct.szProductNameInternal, sgProduct.szProductName);
  GetPrivateProfileString("General", "Product Name Previous", "", sgProduct.szProductNamePrevious, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Uninstall Filename", "", sgProduct.szUninstallFilename, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "User Agent",   "", sgProduct.szUserAgent,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Sub Path",     "", sgProduct.szSubPath,     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Program Name", "", sgProduct.szProgramName, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Lock Path",    "", szBuf,                   sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "TRUE") == 0)
    sgProduct.bLockPath = TRUE;

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
          GetPrivateProfileString("General", "Path", "", szBuf, sizeof(szBuf), szFileIniConfig);
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
  if(stricmp(szShowDialog, "TRUE") == 0)
    diWelcome.bShowDialog = TRUE;

  /* License dialog */
  GetPrivateProfileString("Dialog License",             "Show Dialog",     "", szShowDialog,                  sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "Title",           "", diLicense.szTitle,             MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "License File",    "", diLicense.szLicenseFilename,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "Message0",        "", diLicense.szMessage0,          MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "Message1",        "", diLicense.szMessage1,          MAX_BUF, szFileIniConfig);
  if(stricmp(szShowDialog, "TRUE") == 0)
    diLicense.bShowDialog = TRUE;

  /* Setup Type dialog */
  GetPrivateProfileString("Dialog Setup Type",          "Show Dialog",     "", szShowDialog,                  sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Title",           "", diSetupType.szTitle,           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Message0",        "", diSetupType.szMessage0,        MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Readme Filename", "", diSetupType.szReadmeFilename,  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Readme App",      "", diSetupType.szReadmeApp,       MAX_BUF, szFileIniConfig);
  if(stricmp(szShowDialog, "TRUE") == 0)
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
  if(stricmp(szShowDialog, "TRUE") == 0)
    diSelectComponents.bShowDialog = TRUE;

  /* Select Additional Components dialog */
  GetPrivateProfileString("Dialog Select Additional Components",   "Show Dialog",  "", szShowDialog,                              sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Select Additional Components",   "Title",        "", diSelectAdditionalComponents.szTitle,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Select Additional Components",   "Message0",     "", diSelectAdditionalComponents.szMessage0,   MAX_BUF, szFileIniConfig);
  if(stricmp(szShowDialog, "TRUE") == 0)
    diSelectAdditionalComponents.bShowDialog = TRUE;

  /* OS/2 Integration dialog */
  GetPrivateProfileString("Dialog OS/2 Integration", "Show Dialog",  "", szShowDialog,                    sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog OS/2 Integration", "Title",        "", diOS2Integration.szTitle,    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog OS/2 Integration", "Message0",     "", diOS2Integration.szMessage0, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog OS/2 Integration", "Message1",     "", diOS2Integration.szMessage1, MAX_BUF, szFileIniConfig);
  if(stricmp(szShowDialog, "TRUE") == 0)
    diOS2Integration.bShowDialog = TRUE;

  /* Additional Options dialog */
  GetPrivateProfileString("Dialog Additional Options",       "Show Dialog",    "", szShowDialog,                     sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Additional Options",       "Title",          "", diAdditionalOptions.szTitle,        MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Additional Options",       "Message0",       "", diAdditionalOptions.szMessage0,     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Additional Options",       "Message1",       "", diAdditionalOptions.szMessage1,     MAX_BUF, szFileIniConfig);

  GetPrivateProfileString("Dialog Additional Options",       "Save Installer", "", szBuf,                            sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "TRUE") == 0)
    diAdditionalOptions.bSaveInstaller = TRUE;

  GetPrivateProfileString("Dialog Additional Options",       "Recapture Homepage", "", szBuf,                            sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "TRUE") == 0)
    diAdditionalOptions.bRecaptureHomepage = TRUE;

  GetPrivateProfileString("Dialog Additional Options",       "Show Homepage Option", "", szBuf,                            sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "TRUE") == 0)
    diAdditionalOptions.bShowHomepageOption = TRUE;

  if(stricmp(szShowDialog, "TRUE") == 0)
    diAdditionalOptions.bShowDialog = TRUE;

  /* Advanced Settings dialog */
  GetPrivateProfileString("Dialog Advanced Settings",       "Show Dialog",    "", szShowDialog,                     sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Title",          "", diAdvancedSettings.szTitle,       MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Message0",       "", diAdvancedSettings.szMessage0,    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Proxy Server",   "", diAdvancedSettings.szProxyServer, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Proxy Port",     "", diAdvancedSettings.szProxyPort,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Proxy User",     "", diAdvancedSettings.szProxyUser,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Proxy Password", "", diAdvancedSettings.szProxyPasswd, MAX_BUF, szFileIniConfig);
  if(stricmp(szShowDialog, "TRUE") == 0)
    diAdvancedSettings.bShowDialog = TRUE;

  GetPrivateProfileString("Dialog Advanced Settings",       "Use Protocol",   "", szBuf,                            sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "HTTP") == 0)
    diAdditionalOptions.dwUseProtocol = UP_HTTP;
  else
    diAdditionalOptions.dwUseProtocol = UP_FTP;

  GetPrivateProfileString("Dialog Advanced Settings",       "Use Protocol Settings", "", szBuf,                     sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "DISABLED") == 0)
    diAdditionalOptions.bUseProtocolSettings = FALSE;
  else
    diAdditionalOptions.bUseProtocolSettings = TRUE;

  GetPrivateProfileString("Dialog Advanced Settings",
                          "Show Protocols",
                          "",
                          szBuf,
                          sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "FALSE") == 0)
    diAdditionalOptions.bShowProtocols = FALSE;
  else
    diAdditionalOptions.bShowProtocols = TRUE;

   /* Quick Launch dialog */
  GetPrivateProfileString("Dialog Quick Launch",      "Show Dialog",  "", szShowDialog,                    sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Quick Launch",      "Title",        "", diQuickLaunch.szTitle,         MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Quick Launch",      "Message0",     "", diQuickLaunch.szMessage0,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Quick Launch",      "Message1",     "", diQuickLaunch.szMessage1,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Quick Launch",      "Message2",     "", diQuickLaunch.szMessage2,      MAX_BUF, szFileIniConfig);
  if(stricmp(szShowDialog, "TRUE") == 0)
    diQuickLaunch.bShowDialog = TRUE;
  GetPrivateProfileString("Dialog Quick Launch",       "Turbo Mode",         "", szBuf,                          sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "TRUE") == 0)
    diQuickLaunch.bTurboMode = TRUE;   
  GetPrivateProfileString("Dialog Quick Launch",       "Turbo Mode Enabled","", szBuf,                          sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "TRUE") == 0)
    diQuickLaunch.bTurboModeEnabled = TRUE;
  else
    /* since turbo mode is disabled, make sure bTurboMode is FALSE */
    diQuickLaunch.bTurboMode = FALSE;

  /* Start Install dialog */
  GetPrivateProfileString("Dialog Start Install",       "Show Dialog",      "", szShowDialog,                     sizeof(szShowDialog), szFileIniConfig);
  GetPrivateProfileString("Dialog Start Install",       "Title",            "", diStartInstall.szTitle,           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Start Install",       "Message Install",  "", diStartInstall.szMessageInstall,  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Start Install",       "Message Download", "", diStartInstall.szMessageDownload, MAX_BUF, szFileIniConfig);
  if(stricmp(szShowDialog, "TRUE") == 0)
    diStartInstall.bShowDialog = TRUE;
 
  /* Download dialog */
  GetPrivateProfileString("Dialog Download",       "Show Dialog",        "", szShowDialog,                   sizeof(szShowDialog),        szFileIniConfig);
  GetPrivateProfileString("Dialog Download",       "Title",              "", diDownload.szTitle,             MAX_BUF_TINY,   szFileIniConfig);
  GetPrivateProfileString("Dialog Download",       "Message Download0",  "", diDownload.szMessageDownload0,  MAX_BUF_MEDIUM, szFileIniConfig);
  GetPrivateProfileString("Dialog Download",       "Message Retry0",     "", diDownload.szMessageRetry0,     MAX_BUF_MEDIUM, szFileIniConfig);
  if(stricmp(szShowDialog, "TRUE") == 0)
    diDownload.bShowDialog = TRUE;

  /* Reboot dialog */
  GetPrivateProfileString("Dialog Reboot", "Show Dialog", "", szShowDialog, sizeof(szShowDialog), szFileIniConfig);
  if(stricmp(szShowDialog, "TRUE") == 0)
    diReboot.dwShowDialog = TRUE;
  else if(stricmp(szShowDialog, "AUTO") == 0)
    diReboot.dwShowDialog = AUTO;

  GetPrivateProfileString("OS/2 Integration-Item0", "CheckBoxState", "", szBuf,                                    sizeof(szBuf), szFileIniConfig);
  GetPrivateProfileString("OS/2 Integration-Item0", "Description",   "", diOS2Integration.oiCBMakeDefaultBrowser.szDescription, MAX_BUF, szFileIniConfig);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diOS2Integration.oiCBMakeDefaultBrowser.szDescription != '\0')
    diOS2Integration.oiCBMakeDefaultBrowser.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(stricmp(szBuf, "TRUE") == 0)
    diOS2Integration.oiCBMakeDefaultBrowser.bCheckBoxState = TRUE;

  GetPrivateProfileString("OS/2 Integration-Item1", "CheckBoxState", "", szBuf,                           sizeof(szBuf), szFileIniConfig);
  GetPrivateProfileString("OS/2 Integration-Item1", "Description",   "", diOS2Integration.oiCBAssociateHTML.szDescription, MAX_BUF, szFileIniConfig);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diOS2Integration.oiCBAssociateHTML.szDescription != '\0')
    diOS2Integration.oiCBAssociateHTML.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(stricmp(szBuf, "TRUE") == 0)
    diOS2Integration.oiCBAssociateHTML.bCheckBoxState = TRUE;

  GetPrivateProfileString("OS/2 Integration-Item2", "CheckBoxState", "", szBuf,                           sizeof(szBuf), szFileIniConfig);
  GetPrivateProfileString("OS/2 Integration-Item2", "Description",   "", diOS2Integration.oiCBUpdateCONFIGSYS.szDescription, MAX_BUF, szFileIniConfig);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diOS2Integration.oiCBUpdateCONFIGSYS.szDescription != '\0')
    diOS2Integration.oiCBUpdateCONFIGSYS.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(stricmp(szBuf, "TRUE") == 0)
    diOS2Integration.oiCBUpdateCONFIGSYS.bCheckBoxState = TRUE;

  /* Read in the Site Selector Status */
  GetPrivateProfileString("Site Selector", "Status", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "HIDE") == 0)
    gulSiteSelectorStatus = SS_HIDE;

  switch(sgProduct.ulMode)
  {
    case AUTO:
    case SILENT:
      diWelcome.bShowDialog                     = FALSE;
      diLicense.bShowDialog                     = FALSE;
      diSetupType.bShowDialog                   = FALSE;
      diSelectComponents.bShowDialog            = FALSE;
      diSelectAdditionalComponents.bShowDialog  = FALSE;
      diOS2Integration.bShowDialog          = FALSE;
      diProgramFolder.bShowDialog               = FALSE;
      diQuickLaunch.bShowDialog                 = FALSE;
      diAdditionalOptions.bShowDialog             = FALSE;
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
  if((stricmp(szBuf, "Setup Type 0") == 0) && diSetupType.stSetupType0.bVisible)
  {
    ulSetupType     = ST_RADIO0;
    ulTempSetupType = ulSetupType;
  }
  else if((stricmp(szBuf, "Setup Type 1") == 0) && diSetupType.stSetupType1.bVisible)
  {
    ulSetupType     = ST_RADIO1;
    ulTempSetupType = ulSetupType;
  }
  else if((stricmp(szBuf, "Setup Type 2") == 0) && diSetupType.stSetupType2.bVisible)
  {
    ulSetupType     = ST_RADIO2;
    ulTempSetupType = ulSetupType;
  }
  else if((stricmp(szBuf, "Setup Type 3") == 0) && diSetupType.stSetupType3.bVisible)
  {
    ulSetupType     = ST_RADIO3;
    ulTempSetupType = ulSetupType;
  }
  else
  {
    if(diSetupType.stSetupType0.bVisible)
    {
      ulSetupType     = ST_RADIO0;
      ulTempSetupType = ulSetupType;
    }
    else if(diSetupType.stSetupType1.bVisible)
    {
      ulSetupType     = ST_RADIO1;
      ulTempSetupType = ulSetupType;
    }
    else if(diSetupType.stSetupType2.bVisible)
    {
      ulSetupType     = ST_RADIO2;
      ulTempSetupType = ulSetupType;
    }
    else if(diSetupType.stSetupType3.bVisible)
    {
      ulSetupType     = ST_RADIO3;
      ulTempSetupType = ulSetupType;
    }
  }
  SiCNodeSetItemsSelected(ulSetupType);

  /* get install size required in temp for component Xpcom.  Sould be in Kilobytes */
  GetPrivateProfileString("Core", "Install Size", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(*szBuf != '\0')
    siCFXpcomFile.ulInstallSize = atoi(szBuf);
  else
    siCFXpcomFile.ulInstallSize = 0;

  GetPrivateProfileString("Core",                           "Source",           "", szBuf,                        sizeof(szBuf), szFileIniConfig);
  DecryptString(siCFXpcomFile.szSource, szBuf);
  GetPrivateProfileString("Core",                           "Destination",      "", szBuf,                        sizeof(szBuf), szFileIniConfig);
  DecryptString(siCFXpcomFile.szDestination, szBuf);
  GetPrivateProfileString("Core",                           "Message",          "", siCFXpcomFile.szMessage,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Core",                           "Cleanup",          "", szBuf,                        sizeof(szBuf), szFileIniConfig);
  if(stricmp(szBuf, "FALSE") == 0)
    siCFXpcomFile.bCleanup = FALSE;
  else
    siCFXpcomFile.bCleanup = TRUE;

  LogISProductInfo();
  LogMSProductInfo();
  CleanupXpcomFile();
  ShowMessage(szMsgInitSetup, FALSE);

  /* check the windows registry to see if a previous instance of setup finished downloading
   * all the required archives. */
  dwPreviousUnfinishedState = GetPreviousUnfinishedState();
  gbPreviousUnfinishedDownload = dwPreviousUnfinishedState == PUS_DOWNLOAD;
  if(gbPreviousUnfinishedDownload)
  {
    char szTitle[MAX_BUF_TINY];

    switch(sgProduct.ulMode)
    {
      case NORMAL:
        if(!GetPrivateProfileString("Messages", "STR_MESSAGEBOX_TITLE", "", szBuf, sizeof(szBuf), szFileIniInstall))
          strcpy(szTitle, "Setup");
        else
          sprintf(szTitle, szBuf, sgProduct.szProductName);

        GetPrivateProfileString("Strings", "Message Unfinished Download Restart", "", szBuf, sizeof(szBuf), szFileIniConfig);
        if(WinMessageBox(HWND_DESKTOP, hWndMain, szBuf, szTitle, 0, MB_YESNO | MB_ICONQUESTION) == MBID_NO)
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
    switch(sgProduct.ulMode)
    {
      case NORMAL:
        if(!GetPrivateProfileString("Messages", "STR_MESSAGEBOX_TITLE", "", szBuf, sizeof(szBuf), szFileIniInstall))
          strcpy(szTitle, "Setup");
        else
          sprintf(szTitle, szBuf, sgProduct.szProductName);

        GetPrivateProfileString("Strings", "Message Unfinished Install Xpi Restart", "", szBuf, sizeof(szBuf), szFileIniConfig);
        if(WinMessageBox(HWND_DESKTOP, hWndMain, szBuf, szTitle, 0, MB_YESNO | MB_ICONQUESTION) == MBID_NO)
        {
          UnsetSetupCurrentDownloadFile();
          UnsetSetupState(); /* unset the installing xpis state so that the archives can be deleted */
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
  /* get system font */
  /* @MAK - we need to query this from somewhere */
  strcpy(sgInstallGui.szDefinedFont, "9");
  strcat(sgInstallGui.szDefinedFont, ".");
  strcat(sgInstallGui.szDefinedFont, "WarpSans");

  /* get defined font */
  GetPrivateProfileString("General", "FONTNAME", "", sgInstallGui.szFontName, sizeof(sgInstallGui.szFontName), szFileIniInstall);
  GetPrivateProfileString("General", "FONTSIZE", "", sgInstallGui.szFontSize, sizeof(sgInstallGui.szFontSize), szFileIniInstall);
  GetPrivateProfileString("General", "CHARSET", "", sgInstallGui.szCharSet, sizeof(sgInstallGui.szCharSet), szFileIniInstall);
  strcpy(sgInstallGui.szDefinedFont, sgInstallGui.szFontSize);
  strcat(sgInstallGui.szDefinedFont, ".");
  strcat(sgInstallGui.szDefinedFont, sgInstallGui.szFontName);

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
  GetPrivateProfileString("General", "DECLINE_", "", sgInstallGui.szDecline_, sizeof(sgInstallGui.szDecline_), szFileIniInstall);
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


BOOL LocatePreviousPath(PSZ szMainSectionName, PSZ szPath, ULONG ulPathSize)
{
  ULONG ulIndex;
  char  szIndex[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szValue[MAX_BUF];
  BOOL  bFound;

  bFound  = FALSE;
  ulIndex = -1;
  while(!bFound)
  {
    ++ulIndex;
    _itoa(ulIndex, szIndex, 10);
    strcpy(szSection, szMainSectionName);
    strcat(szSection, szIndex);

    GetPrivateProfileString(szSection, "App", "", szValue, sizeof(szValue), szFileIniConfig);
    if(*szValue != '\0')
      bFound = LocatePathOS2INI(szSection, szPath, ulPathSize);
    else
      break;
  }

  return(bFound);
}

DWORD GetTotalArchivesToDownload()
{
  DWORD     dwIndex0;
  DWORD     dwTotalArchivesToDownload;
  siC       *siCObject = NULL;
  char      szIndex0[MAX_BUF];

  dwTotalArchivesToDownload = 0;
  dwIndex0                  = 0;
  _itoa(dwIndex0,  szIndex0,  10);
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
    _itoa(dwIndex0, szIndex0, 10);
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  return(dwTotalArchivesToDownload);
}

BOOL LocatePathOS2INI(PSZ szSection, PSZ szPath, ULONG ulPathSize)
{
  char  szApp[MAX_BUF];
  char  szHRoot[MAX_BUF];
  char  szName[MAX_BUF];
  char  szVerifyExistence[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szIni[MAX_BUF];
  BOOL  bDecryptKey;
  BOOL  bContainsFilename;
  BOOL  bReturn;
  HINI  hini = HINI_USERPROFILE;

  bReturn = FALSE;
  GetPrivateProfileString(szSection, "App", "", szApp, sizeof(szApp), szFileIniConfig);
  if(*szApp != '\0')
  {
    bReturn = FALSE;
    memset(szPath, 0, ulPathSize);

    GetPrivateProfileString(szSection, "Key",         "", szName,  sizeof(szName),  szFileIniConfig);
    GetPrivateProfileString(szSection, "Decrypt App", "", szBuf,   sizeof(szBuf),   szFileIniConfig);
    if(stricmp(szBuf, "FALSE") == 0)
      bDecryptKey = FALSE;
    else
      bDecryptKey = TRUE;

    /* check for both 'Verify Existance' and 'Verify Existence' */
    GetPrivateProfileString(szSection, "Verify Existence", "", szVerifyExistence, sizeof(szVerifyExistence), szFileIniConfig);
    if(*szVerifyExistence == '\0')
      GetPrivateProfileString(szSection, "Verify Existance", "", szVerifyExistence, sizeof(szVerifyExistence), szFileIniConfig);

    GetPrivateProfileString(szSection, "Contains Filename", "", szBuf, sizeof(szBuf), szFileIniConfig);
    if(stricmp(szBuf, "TRUE") == 0)
      bContainsFilename = TRUE;
    else
      bContainsFilename = FALSE;

    if(bDecryptKey == TRUE)
    {
      DecryptString(szBuf, szApp);
      strcpy(szApp, szBuf);
    }

    GetPrivateProfileString(szSection, "INI", "", szIni,  sizeof(szIni),  szFileIniConfig);
    if (szIni[0]) {
      BOOL bDecryptINI;
      GetPrivateProfileString(szSection, "Decrypt INI", "", szBuf,   sizeof(szBuf),   szFileIniConfig);
      if(stricmp(szBuf, "FALSE")) {
        DecryptString(szBuf, szIni);
        strcpy(szIni, szBuf);
      }
      hini = PrfOpenProfile((HAB)0, szIni);
    }

    PrfQueryProfileString(hini, szApp, szName, "", szBuf, sizeof(szBuf));
    if (szIni[0]) {
      PrfCloseProfile(hini);
    }
    if(*szBuf != '\0')
    {
      if(stricmp(szVerifyExistence, "FILE") == 0)
      {
        if(FileExists(szBuf))
        {
          if(bContainsFilename == TRUE)
            ParsePath(szBuf, szPath, ulPathSize, FALSE, PP_PATH_ONLY);
          else
            strcpy(szPath, szBuf);

          bReturn = TRUE;
        }
        else
          bReturn = FALSE;
      }
      else if(stricmp(szVerifyExistence, "PATH") == 0)
      {
        if(bContainsFilename == TRUE)
          ParsePath(szBuf, szPath, ulPathSize, FALSE, PP_PATH_ONLY);
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
          ParsePath(szBuf, szPath, ulPathSize, FALSE, PP_PATH_ONLY);
        else
          strcpy(szPath, szBuf);

        bReturn = TRUE;
      }
    }
  }

  return(bReturn);
}

void SetCustomType()
{
  if(diSetupType.stSetupType3.bVisible == TRUE)
    sgProduct.ulCustomType = ST_RADIO3;
  else if(diSetupType.stSetupType2.bVisible == TRUE)
    sgProduct.ulCustomType = ST_RADIO2;
  else if(diSetupType.stSetupType1.bVisible == TRUE)
    sgProduct.ulCustomType = ST_RADIO1;
  else if(diSetupType.stSetupType0.bVisible == TRUE)
    sgProduct.ulCustomType = ST_RADIO0;
}

void STSetVisibility(st *stSetupType)
{
  if(*(stSetupType->szDescriptionShort) == '\0')
    stSetupType->bVisible = FALSE;
  else
    stSetupType->bVisible = TRUE;
}

HRESULT DecryptVariable(PSZ szVariable, ULONG ulVariableSize)
{
  char szBuf[MAX_BUF];
  char szBuf2[MAX_BUF];
  char szKey[MAX_BUF];
  char szName[MAX_BUF];
  char szValue[MAX_BUF];
  char szLookupSection[MAX_BUF];
  HKEY hkeyRoot;

  /* zero out the memory allocations */
  memset(szBuf,           0, sizeof(szBuf));
  memset(szKey,           0, sizeof(szKey));
  memset(szName,          0, sizeof(szName));
  memset(szValue,         0, sizeof(szValue));
  memset(szBuf2,          0, sizeof(szBuf2));
  memset(szLookupSection, 0, sizeof(szLookupSection));

  if(stricmp(szVariable, "PROGRAMFILESDIR") == 0)
  {
    /* @MAK Needed for install */
  }
  else if(stricmp(szVariable, "INSTALLDRIVE") == 0)
  {
    /* parse for "C:" */
    szVariable[0] = sgProduct.szPath[0];
    szVariable[1] = sgProduct.szPath[1];
    szVariable[2] = '\0';
  }
  else if(stricmp(szVariable, "STARTUP") == 0)
  {
    HOBJECT hobj;
    hobj = WinQueryObject("<WP_STARTUP>");
    WinQueryObjectPath(hobj, szVariable, ulVariableSize);
  }
  else if(stricmp(szVariable, "DESKTOP") == 0)
  {
    HOBJECT hobj;
    hobj = WinQueryObject("<WP_DESKTOP>");
    WinQueryObjectPath(hobj, szVariable, ulVariableSize);
  }
  else if(stricmp(szVariable, "WARPCENTER") == 0)
  {
    HOBJECT hobj;
    hobj = WinQueryObject("<WP_WARPCENTER????>");
    WinQueryObjectPath(hobj, szVariable, ulVariableSize);
  }
  else if(stricmp(szVariable, "WIZTEMP") == 0)
  {
    /* parse for the "c:\Temp" path */
    strcpy(szVariable, szTempDir);
    if(szVariable[strlen(szVariable) - 1] == '\\')
      szVariable[strlen(szVariable) - 1] = '\0';
  }
  else if(stricmp(szVariable, "TEMP") == 0)
  {
    /* parse for the "c:\Temp" path */
    strcpy(szVariable, szOSTempDir);
    if(szVariable[strlen(szVariable) - 1] == '\\')
      szVariable[strlen(szVariable) - 1] = '\0';
  }
  else if(stricmp(szVariable, "OS2DISK") == 0)
  {
    /* Locate the drive that OS/2 is installed on, and only use the drive letter and the ':' character (C:). */
    ULONG ulBootDrive = 0;
    memset(szVariable, '\0', MAX_BUF);
    DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                    &ulBootDrive, sizeof(ulBootDrive));
    szVariable[0] = 'A' - 1 + ulBootDrive;
    szVariable[1] = ':';
  }
  else if(stricmp(szVariable, "OS2DIR") == 0)
  {
    /* Locate the "OS2" directory */
    ULONG ulBootDrive = 0;
    APIRET rc;
    char  buffer[] = " :\\OS2";
    DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                    &ulBootDrive, sizeof(ulBootDrive));
    buffer[0] = 'A' - 1 + ulBootDrive;
    strcpy(szVariable, buffer);
  }
  else if(stricmp(szVariable, "OS2SYSDIR") == 0)
  {
    /* Locate the "OS2\SYSTEM" directory */
    ULONG ulBootDrive = 0;
    APIRET rc;
    char  buffer[] = " :\\OS2\\SYSTEM";
    DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                    &ulBootDrive, sizeof(ulBootDrive));
    buffer[0] = 'A' - 1 + ulBootDrive;
    strcpy(szVariable, buffer);
  }
  else if(  (stricmp(szVariable, "JRE LIB PATH") == 0)
         || (stricmp(szVariable, "JRE BIN PATH") == 0) )
  {
    /* @MAK - we could do this */
  }
  else if(stricmp(szVariable, "JRE PATH") == 0)
  {
    /* @MAK - we could do this */
  }
  else if(stricmp(szVariable, "SETUP PATH") == 0)
  {
    strcpy(szVariable, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szVariable, ulVariableSize);
      strcat(szVariable, sgProduct.szSubPath);
    }
  }
  else if(stricmp(szVariable, "Default Path") == 0)
  {
    strcpy(szVariable, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szVariable, ulVariableSize);
      strcat(szVariable, sgProduct.szSubPath);
    }
  }
  else if(stricmp(szVariable, "SETUP STARTUP PATH") == 0)
  {
    strcpy(szVariable, szSetupDir);
  }
  else if(stricmp(szVariable, "Default Folder") == 0)
  {
    strcpy(szVariable, sgProduct.szProgramFolderPath);
    AppendBackSlash(szVariable, ulVariableSize);
    strcat(szVariable, sgProduct.szProgramFolderName);
  }
  else if(stricmp(szVariable, "Product CurrentVersion") == 0)
  {
    char szApp[MAX_BUF];

    sprintf(szApp, "%s", sgProduct.szProductNameInternal);

    /* parse for the current Netscape INI entry */
    PrfQueryProfileString(HINI_USERPROFILE, szApp, "CurrentVersion", "",
                          szBuf, sizeof(szBuf));

    if(*szBuf == '\0')
      return(FALSE);

    strcpy(szVariable, szBuf);
  }
  else if(stricmp(szVariable, "Product PreviousVersion") == 0)
  {
    char szApp[MAX_BUF];

    sprintf(szApp, "%s", sgProduct.szProductNamePrevious);

    /* parse for the current Netscape INI entry */
    PrfQueryProfileString(HINI_USERPROFILE, szApp, "CurrentVersion", "",
                          szBuf, sizeof(szBuf));

    if(*szBuf == '\0')
      return(FALSE);

    sprintf(szVariable, "%s %s", sgProduct.szProductNamePrevious, szBuf);
  }
  else
    return(FALSE);

  return(TRUE);
}

HRESULT DecryptString(PSZ szOutputStr, PSZ szInputStr)
{
  ULONG ulLenInputStr;
  ULONG ulCounter;
  ULONG ulVar;
  ULONG ulPrepend;
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
  ulLenInputStr = strlen(szInputStr);
  bBeginParse   = FALSE;
  bFoundVar     = FALSE;

  for(ulCounter = 0; ulCounter < ulLenInputStr; ulCounter++)
  {
    if((szInputStr[ulCounter] == ']') && bBeginParse)
      break;

    if(bBeginParse)
      szVariable[ulVar++] = szInputStr[ulCounter];

    if((szInputStr[ulCounter] == '[') && !bBeginParse)
    {
      ulVar        = 0;
      ulPrepend    = ulCounter;
      bBeginParse  = TRUE;
    }
  }

  if(ulCounter == ulLenInputStr)
    /* did not find anything to expand. */
    ulCounter = 0;
  else
  {
    bFoundVar = TRUE;
    ++ulCounter;
  }

  if(bFoundVar)
  {
    strcpy(szAppend, &szInputStr[ulCounter]);

    szPrepend[ulPrepend] = '\0';

    /* if Variable is "XPI PATH", do special processing */
    if(stricmp(szVariable, "XPI PATH") == 0)
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

#define S_IFMT (S_IFDIR | S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

HRESULT FileExists(PSZ szFile)
{
  struct stat st;
  int statrv;

  statrv = stat(szFile, &st);
  if (statrv == 0)
     if (S_ISDIR(st.st_mode))
        return FILE_DIRECTORY;
     else
        return(TRUE);
  else if ((strlen(szFile) == 2) && (szFile[1] == ':'))
  {
     char temp[4] = {0};
     strcpy(temp, szFile);
     strcat(temp, "\\");
     statrv = stat(temp, &st);
     if (statrv == 0)
        return FILE_DIRECTORY;
  }
  return (FALSE);
}

BOOL isFAT(char* szPath)
{
  APIRET rc;
  ULONG ulSize;
  PFSQBUFFER2 pfsqbuf2;
  CHAR szDrive[3];

  ulSize = sizeof(FSQBUFFER2) + 3 * CCHMAXPATH;
  pfsqbuf2 = (PFSQBUFFER2)malloc(ulSize);
  strncpy(szDrive, szPath, 2);
  szDrive[2] = '\0';

  DosError(FERR_DISABLEHARDERR);
  rc = DosQueryFSAttach(szDrive, 0, FSAIL_QUERYNAME,
                        pfsqbuf2, &ulSize);
  DosError(FERR_ENABLEHARDERR);

  if (rc == NO_ERROR) {
    if (strcmp(pfsqbuf2->szFSDName + pfsqbuf2->cbName, "FAT") != 0)
      return FALSE;
  }

  return TRUE;
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

  memset(szFileIniConfig, 0,sizeof(szFileIniConfig));

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

void DeleteArchives(DWORD dwDeleteCheck)
{
  DWORD dwIndex0;
  char  szArchiveName[MAX_BUF];
  siC   *siCObject = NULL;

  memset(szArchiveName, 0, sizeof(szArchiveName));

  if((!bSDUserCanceled) && (GetPreviousUnfinishedState() == PUS_NONE))
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
                         sgProduct.szProductName, 0,
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

void DeInitialize()
{
  char szBuf[MAX_BUF];

  LogISTime(W_END);
  if(bCreateDestinationDir)
  {
    strcpy(szBuf, sgProduct.szPath);
    AppendBackSlash(szBuf, sizeof(szBuf));
    DirectoryRemove(szBuf, FALSE);
  }

  DeleteWGetLog();
  CleanTempFiles();
  DirectoryRemove(szTempDir, FALSE);

  if(gErrorMessageStream.bEnabled && gErrorMessageStream.bSendMessage)
    SendErrorMessage();

  DeInitSiComponents(&siComponents);
  DeInitSXpcomFile();
  DeInitDlgReboot(&diReboot);
  DeInitDlgDownload(&diDownload);
  DeInitDlgStartInstall(&diStartInstall);
  DeInitDlgAdditionalOptions(&diAdditionalOptions);
  DeInitDlgAdvancedSettings(&diAdvancedSettings);
  DeInitDlgProgramFolder(&diProgramFolder);
  DeInitDlgOS2Integration(&diOS2Integration);
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

  DosFreeModule(hSetupRscInst);
}

char *GetSaveInstallerPath(char *szBuf, DWORD dwBufSize)
{
#ifdef XXX_INTL_HACK_WORKAROUND_FOR_NOW
  char szBuf2[MAX_BUF];
#endif

  /* determine the path to where the setup and downloaded files will be saved to */
  strcpy(szBuf, sgProduct.szPath);
  AppendBackSlash(szBuf, dwBufSize);
  if(*sgProduct.szSubPath != '\0')
  {
    strcat(szBuf, sgProduct.szSubPath);
    strcat(szBuf, " ");
  }

#ifdef XXX_INTL_HACK_WORKAROUND_FOR_NOW
/* Installer can't create the Save Installer Path if the word "Setup" is localized. */
  if(GetPrivateProfileString("Messages", "STR_SETUP", "", szBuf2, sizeof(szBuf2), szFileIniInstall))
    strcat(szBuf, szBuf2);
  else
#endif
    strcat(szBuf, "Setup");

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
  PPIB      ppib;
  PTIB      ptib;

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
    char buffer[CCHMAXPATH];
    DosGetInfoBlocks( &ptib, &ppib);
    DosQueryModuleName( ppib->pib_hmte, sizeof(szBuf), szBuf);
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

BOOL ShowAdditionalOptionsDialog(void)
{
  if(diAdditionalOptions.bShowDialog == FALSE)
    return(FALSE);

  if( (diAdditionalOptions.bShowHomepageOption == FALSE) && (GetTotalArchivesToDownload() < 1) )
    return(FALSE);

  return(TRUE);
}

HWND FindWindow(PCSZ pszAtomString)
{
  ATOM atom;
  HENUM henum;
  HWND hwndClient, hwnd = NULLHANDLE;


  atom = WinFindAtom(WinQuerySystemAtomTable(), pszAtomString);
  if (atom) {
    henum = WinBeginEnumWindows(HWND_DESKTOP);
    while ((hwnd = WinGetNextWindow(henum)) != NULLHANDLE)
    {
      ULONG ulWindowWord;
      ulWindowWord = WinQueryWindowULong(hwnd, QWL_USER);
      if (ulWindowWord == atom) {
        break;
      } else {
        /* Try the class name method to support older browsers */
        HWND hwndClient;
        CHAR szClassName[MAX_BUF];
        hwndClient = WinWindowFromID(hwnd, FID_CLIENT);
        WinQueryClassName(hwndClient ? hwndClient : hwnd, MAX_BUF, szClassName);
        if (strcmp(szClassName, pszAtomString) == 0) {
           break;
        }
      }
    }
    WinEndEnumWindows(henum);
  }
  if (!hwnd) {
     /* Try the object windows just in case, but only for the classname */
    henum = WinBeginEnumWindows(HWND_OBJECT);
    while ((hwnd = WinGetNextWindow(henum)) != NULLHANDLE)
    {
      /* Try the class name method to support older browsers */
      HWND hwndClient;
      CHAR szClassName[MAX_BUF];
      hwndClient = WinWindowFromID(hwnd, FID_CLIENT);
      WinQueryClassName(hwndClient ? hwndClient : hwnd, MAX_BUF, szClassName);
      if (strcmp(szClassName, pszAtomString) == 0) {
         break;
      }
    }
  }
  return  hwnd;
}

