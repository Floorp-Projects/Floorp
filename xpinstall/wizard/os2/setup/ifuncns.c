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
#include "shortcut.h"
#include "ifuncns.h"
#include "wizverreg.h"
#include "logging.h"
#include <logkeys.h>

HRESULT TimingCheck(DWORD dwTiming, LPSTR szSection, LPSTR szFile)
{
  char szBuf[MAX_BUF_TINY];

  GetPrivateProfileString(szSection, "Timing", "", szBuf, sizeof(szBuf), szFile);
  if(*szBuf != '\0')
  {
    switch(dwTiming)
    {
      case T_PRE_DOWNLOAD:
        if(stricmp(szBuf, "pre download") == 0)
          return(TRUE);
        break;

      case T_POST_DOWNLOAD:
        if(stricmp(szBuf, "post download") == 0)
          return(TRUE);
        break;

      case T_PRE_XPCOM:
        if(stricmp(szBuf, "pre xpcom") == 0)
          return(TRUE);
        break;

      case T_POST_XPCOM:
        if(stricmp(szBuf, "post xpcom") == 0)
          return(TRUE);
        break;

      case T_PRE_SMARTUPDATE:
        if(stricmp(szBuf, "pre smartupdate") == 0)
          return(TRUE);
        break;

      case T_POST_SMARTUPDATE:
        if(stricmp(szBuf, "post smartupdate") == 0)
          return(TRUE);
        break;

      case T_PRE_LAUNCHAPP:
        if(stricmp(szBuf, "pre launchapp") == 0)
          return(TRUE);
        break;

      case T_POST_LAUNCHAPP:
        if(stricmp(szBuf, "post launchapp") == 0)
          return(TRUE);
        break;

      case T_PRE_ARCHIVE:
        if(stricmp(szBuf, "pre archive") == 0)
          return(TRUE);
        break;

      case T_POST_ARCHIVE:
        if(stricmp(szBuf, "post archive") == 0)
          return(TRUE);
        break;

      case T_DEPEND_REBOOT:
        if(stricmp(szBuf, "depend reboot") == 0)
          return(TRUE);
        break;
    }
  }
  return(FALSE);
}

char *BuildNumberedString(DWORD dwIndex, char *szInputStringPrefix, char *szInputString, char *szOutBuf, DWORD dwOutBufSize)
{
  if((szInputStringPrefix) && (*szInputStringPrefix != '\0'))
    sprintf(szOutBuf, "%s-%s%d", szInputStringPrefix, szInputString, dwIndex);
  else
    sprintf(szOutBuf, "%s%d", szInputString, dwIndex);

  return(szOutBuf);
}

void GetUserAgentShort(char *szUserAgent, char *szOutUAShort, DWORD dwOutUAShortSize)
{
  char *ptrFirstSpace = NULL;

  memset(szOutUAShort, 0, dwOutUAShortSize);
  if((szUserAgent == NULL) || (*szUserAgent == '\0'))
    return;

  ptrFirstSpace = strstr(szUserAgent, " ");
  if(ptrFirstSpace != NULL)
  {
    *ptrFirstSpace = '\0';
    strcpy(szOutUAShort, szUserAgent);
    *ptrFirstSpace = ' ';
  }
}

void CleanupPreviousVersionINIKeys(void)
{
  char  szIndex[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szValue[MAX_BUF];
  char  szPath[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szDecrypt[MAX_BUF];
  char  szMainSectionName[] = "Cleanup Previous Product INIApps";
  char  szApp[MAX_BUF];
  char  szKey[MAX_BUF];
  char  szDirKey[MAX_BUF];
  char  szCleanupProduct[MAX_BUF];
  char  szCurrentVersion[MAX_BUF_TINY];
  char  szUserAgent[MAX_BUF];
  char  szIni[MAX_BUF];
  BOOL  bFound;
  ULONG ulAppsLength, ulIndex;
  CHAR* szApps;
  HINI  hini = HINI_USERPROFILE;

  strcpy(szPath, sgProduct.szPath);
  if(*sgProduct.szSubPath != '\0')
  {
    AppendBackSlash(szPath, sizeof(szPath));
    strcat(szPath, sgProduct.szSubPath);
  }
  AppendBackSlash(szPath, sizeof(szPath));

  bFound  = FALSE;
  ulIndex = -1;
  while(!bFound)
  {
    ++ulIndex;
    itoa(ulIndex, szIndex, 10);
    strcpy(szSection, szMainSectionName);
    strcat(szSection, szIndex);

    GetPrivateProfileString(szSection, "Product Name", "", szValue, sizeof(szValue), szFileIniConfig);
    if(*szValue != '\0') {
      GetPrivateProfileString(szSection, "Product Name", "", szCleanupProduct, sizeof(szCleanupProduct), szFileIniConfig);
      GetPrivateProfileString(szSection, "Current Version", "", szCurrentVersion, sizeof(szCurrentVersion), szFileIniConfig);

      GetPrivateProfileString(szSection, "Key",                "", szBuf,           sizeof(szBuf),           szFileIniConfig);
      GetPrivateProfileString(szSection, "Decrypt Key",        "", szDecrypt,       sizeof(szDecrypt),       szFileIniConfig);
      memset(szKey, 0, sizeof(szKey));
      if(stricmp(szDecrypt, "TRUE") == 0)
        DecryptString(szDirKey, szBuf);
      else
        strcpy(szDirKey, szBuf);

      sprintf(szUserAgent, "%s %s", szCleanupProduct, szCurrentVersion);

      PrfQueryProfileSize(HINI_USERPROFILE, NULL, NULL, &ulAppsLength);
      szApps = (char*)malloc(ulAppsLength+1);
      PrfQueryProfileString(HINI_USERPROFILE, NULL, NULL, NULL, szApps, ulAppsLength);
      szApps[ulAppsLength] = '\0';
      while (*szApps) {
        if (strncmp(szApps, szCleanupProduct, strlen(szCleanupProduct)) == 0) {
          if (strncmp(szApps, szUserAgent, strlen(szUserAgent)) != 0) {
            char szKey[MAX_BUF];
            PrfQueryProfileString(HINI_USERPROFILE, szApps, szDirKey, "", szKey, MAX_BUF);
            if (szKey[0]) {
              AppendBackSlash(szKey, sizeof(szKey));
              if (stricmp(szKey, szPath) == 0) {
                PrfWriteProfileString(HINI_USER, szApps, NULL, NULL);
              }
            }
          }
        }
        szApps = strchr(szApps, '\0')+1;
      }
    } else {
      GetPrivateProfileString(szSection, "App", "", szValue, sizeof(szValue), szFileIniConfig);
      if(*szValue != '\0') {
        GetPrivateProfileString(szSection, "App",                 "", szBuf,           sizeof(szBuf),          szFileIniConfig);
        GetPrivateProfileString(szSection, "Decrypt App",         "", szDecrypt,       sizeof(szDecrypt),      szFileIniConfig);
        memset(szApp, 0, sizeof(szApp));
        if(stricmp(szDecrypt, "TRUE") == 0)
          DecryptString(szApp, szBuf);
        else
          strcpy(szApp, szBuf);
  
        GetPrivateProfileString(szSection, "Key",                "", szBuf,           sizeof(szBuf),           szFileIniConfig);
        GetPrivateProfileString(szSection, "Decrypt Key",        "", szDecrypt,       sizeof(szDecrypt),       szFileIniConfig);
        memset(szDirKey, 0, sizeof(szKey));
        if(stricmp(szDecrypt, "TRUE") == 0)
          DecryptString(szDirKey, szBuf);
        else
          strcpy(szDirKey, szBuf);
  
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
  
        PrfQueryProfileString(hini, szApp, szDirKey, "", szKey, MAX_BUF);
        if (szKey[0]) {
          AppendBackSlash(szKey, sizeof(szKey));
          if (stricmp(szKey, szPath) == 0) {
            PrfWriteProfileString(hini, szApp, NULL, NULL);
          }
        }
        if (szIni[0]) {
          PrfCloseProfile(hini);
        }
      }
      else
      {
        break;
      }
    }
  }
}

void ProcessFileOps(DWORD dwTiming, char *szSectionPrefix)
{
  ProcessUncompressFile(dwTiming, szSectionPrefix);
  ProcessCreateDirectory(dwTiming, szSectionPrefix);
  ProcessMoveFile(dwTiming, szSectionPrefix);
  ProcessCopyFile(dwTiming, szSectionPrefix);
  ProcessCopyFileSequential(dwTiming, szSectionPrefix);
  ProcessDeleteFile(dwTiming, szSectionPrefix);
  ProcessRemoveDirectory(dwTiming, szSectionPrefix);
  if(!gbIgnoreRunAppX)
    ProcessRunApp(dwTiming, szSectionPrefix);
  ProcessOS2INI(dwTiming, szSectionPrefix);
  ProcessProgramFolder(dwTiming, szSectionPrefix);
  ProcessSetVersionRegistry(dwTiming, szSectionPrefix);
}

void ProcessFileOpsForSelectedComponents(DWORD dwTiming)
{
  DWORD dwIndex0;
  siC   *siCObject = NULL;

  dwIndex0  = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    if(siCObject->dwAttributes & SIC_SELECTED)
      /* Since the archive is selected, we need to process the file ops here */
      ProcessFileOps(dwTiming, siCObject->szReferenceName);

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  } /* while(siCObject) */
}

void ProcessFileOpsForAll(DWORD dwTiming)
{
  ProcessFileOps(dwTiming, NULL);
  ProcessFileOpsForSelectedComponents(dwTiming);
  ProcessCreateCustomFiles(dwTiming);
}

int VerifyArchive(LPSTR szArchive)
{
  void *vZip;
  int  iTestRv;

  /* Check for the existance of the from (source) file */
  if(!FileExists(szArchive))
    return(FO_ERROR_FILE_NOT_FOUND);

  if((iTestRv = ZIP_OpenArchive(szArchive, &vZip)) == ZIP_OK)
  {
    /* 1st parameter should be NULL or it will fail */
    /* It indicates extract the entire archive */
    iTestRv = ZIP_TestArchive(vZip);
    ZIP_CloseArchive(&vZip);
  }
  return(iTestRv);
}

HRESULT ProcessSetVersionRegistry(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD   dwIndex;
  BOOL    bIsDirectory;
  char    szBuf[MAX_BUF];
  char    szSection[MAX_BUF_TINY];
  char    szRegistryKey[MAX_BUF];
  char    szPath[MAX_BUF];
  char    szVersion[MAX_BUF_TINY];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Version Registry", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Registry Key", "", szRegistryKey, sizeof(szRegistryKey), szFileIniConfig);
  while(*szRegistryKey != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      GetPrivateProfileString(szSection, "Version", "", szVersion, sizeof(szVersion), szFileIniConfig);
      GetPrivateProfileString(szSection, "Path",    "", szBuf,     sizeof(szBuf),     szFileIniConfig);
      DecryptString(szPath, szBuf);
      if(FileExists(szPath) & FILE_DIRECTORY)
        bIsDirectory = TRUE;
      else
        bIsDirectory = FALSE;

      strcpy(szBuf, sgProduct.szPath);
      if(sgProduct.szSubPath != '\0')
      {
        AppendBackSlash(szBuf, sizeof(szBuf));
        strcat(szBuf, sgProduct.szSubPath);
      }

      VR_CreateRegistry(VR_DEFAULT_PRODUCT_NAME, szBuf, NULL);
      VR_Install(szRegistryKey, szPath, szVersion, bIsDirectory);
      VR_Close();
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Version Registry", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Registry Key", "", szRegistryKey, sizeof(szRegistryKey), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT FileUncompress(LPSTR szFrom, LPSTR szTo)
{
  char  szBuf[MAX_BUF];
  ULONG ulBuf, ulDiskNum, ulDriveMap;
  DWORD dwReturn;
  void  *vZip;
  APIRET rc;

  dwReturn = FO_SUCCESS;
  /* Check for the existance of the from (source) file */
  if(!FileExists(szFrom))
    return(FO_ERROR_FILE_NOT_FOUND);

  /* Check for the existance of the to (destination) path */
  dwReturn = FileExists(szTo);
  if(dwReturn && !(dwReturn & FILE_DIRECTORY))
  {
    /* found a file with the same name as the destination path */
    return(FO_ERROR_DESTINATION_CONFLICT);
  }
  else if(!dwReturn)
  {
    strcpy(szBuf, szTo);
    AppendBackSlash(szBuf, sizeof(szBuf));
    CreateDirectoriesAll(szBuf, FALSE);
  }

  ulBuf = MAX_BUF-3;
  rc = DosQueryCurrentDir(0, &szBuf[3], &ulBuf);
  // Directory does not start with 'x:\', so add it.
  rc = DosQueryCurrentDisk(&ulDiskNum, &ulDriveMap);

  // Follow the case of the first letter in the path.
  if (isupper(szBuf[3]))
     szBuf[0] = (char)('A' - 1 + ulDiskNum);
  else
     szBuf[0] = (char)('a' - 1 + ulDiskNum);
  szBuf[1] = ':';
  szBuf[2] = '\\';

  if (toupper(szBuf[0]) != toupper(szTo[0]))
     rc = DosSetDefaultDisk(toupper(szTo[0]) - 'A' + 1);
  if(DosSetCurrentDir(szTo) != NO_ERROR)
    return(FO_ERROR_CHANGE_DIR);

  if((dwReturn = ZIP_OpenArchive(szFrom, &vZip)) != ZIP_OK)
    return(dwReturn);

  /* 1st parameter should be NULL or it will fail */
  /* It indicates extract the entire archive */
  dwReturn = ExtractDirEntries(NULL, vZip);
  ZIP_CloseArchive(&vZip);

  if (toupper(szBuf[0]) != toupper(szTo[0]))
     DosSetDefaultDisk(toupper(szBuf[0]) -1 + 'A');
  if(DosSetCurrentDir(szBuf) != NO_ERROR)
    return(FO_ERROR_CHANGE_DIR);

  return(dwReturn);
}

HRESULT ProcessXpcomFile()
{
  char szSource[MAX_BUF];
  char szDestination[MAX_BUF];
  DWORD dwErr;

  if(*siCFXpcomFile.szMessage != '\0')
    ShowMessage(siCFXpcomFile.szMessage, TRUE);

  if((dwErr = FileUncompress(siCFXpcomFile.szSource, siCFXpcomFile.szDestination)) != FO_SUCCESS)
  {
    char szMsg[MAX_BUF];
    char szErrorString[MAX_BUF];

    if(*siCFXpcomFile.szMessage != '\0')
      ShowMessage(siCFXpcomFile.szMessage, FALSE);

    LogISProcessXpcomFile(LIS_FAILURE, dwErr);
    GetPrivateProfileString("Strings", "Error File Uncompress", "", szErrorString, sizeof(szErrorString), szFileIniConfig);
    sprintf(szMsg, szErrorString, siCFXpcomFile.szSource, dwErr);
    PrintError(szMsg, ERROR_CODE_HIDE);
    return(dwErr);
  }
  LogISProcessXpcomFile(LIS_SUCCESS, dwErr);

  /* copy msvcrt.dll and msvcirt.dll to the bin of the Xpcom temp dir:
   *   (c:\temp\Xpcom.ns\bin)
   * This is incase these files do not exist on the system */
  strcpy(szSource, siCFXpcomFile.szDestination);
  AppendBackSlash(szSource, sizeof(szSource));
  strcat(szSource, "ms*.dll");

  strcpy(szDestination, siCFXpcomFile.szDestination);
  AppendBackSlash(szDestination, sizeof(szDestination));
  strcat(szDestination, "bin");

  FileCopy(szSource, szDestination, TRUE, FALSE);

  if(*siCFXpcomFile.szMessage != '\0')
    ShowMessage(siCFXpcomFile.szMessage, FALSE);

  return(FO_SUCCESS);
}

HRESULT CleanupXpcomFile()
{
  if(siCFXpcomFile.bCleanup == TRUE)
    DirectoryRemove(siCFXpcomFile.szDestination, TRUE);

  return(FO_SUCCESS);
}

HRESULT CleanupArgsRegistry()
{
  char  szApp[MAX_BUF];

  sprintf(szApp, "%s %s", sgProduct.szProductNameInternal, sgProduct.szUserAgent);
  PrfWriteProfileString(HINI_USERPROFILE, szApp, "browserargs", NULL);
  return(FO_SUCCESS);
}

HRESULT ProcessUncompressFile(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD   dwIndex;
  BOOL    bOnlyIfExists;
  char    szBuf[MAX_BUF];
  char    szSection[MAX_BUF];
  char    szSource[MAX_BUF];
  char    szDestination[MAX_BUF];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Uncompress File", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szDestination, szBuf);
      GetPrivateProfileString(szSection, "Only If Exists", "", szBuf, sizeof(szBuf), szFileIniConfig);
      if(stricmp(szBuf, "TRUE") == 0)
        bOnlyIfExists = TRUE;
      else
        bOnlyIfExists = FALSE;

      if((!bOnlyIfExists) || (bOnlyIfExists && FileExists(szDestination)))
      {
        DWORD dwErr;

        GetPrivateProfileString(szSection, "Message",     "", szBuf, sizeof(szBuf), szFileIniConfig);
        ShowMessage(szBuf, TRUE);
        if((dwErr = FileUncompress(szSource, szDestination)) != FO_SUCCESS)
        {
          char szMsg[MAX_BUF];
          char szErrorString[MAX_BUF];

          ShowMessage(szBuf, FALSE);
          GetPrivateProfileString("Strings", "Error File Uncompress", "", szErrorString, sizeof(szErrorString), szFileIniConfig);
          sprintf(szMsg, szErrorString, szSource, dwErr);
          PrintError(szMsg, ERROR_CODE_HIDE);
          return(dwErr);
        }

        ShowMessage(szBuf, FALSE);
      }
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Uncompress File", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT FileMove(LPSTR szFrom, LPSTR szTo)
{
  HDIR            hFile;
  FILEFINDBUF3    fdFile;
  ULONG           ulFindCount;
  ULONG           ulAttrs;
  char            szFromDir[MAX_BUF];
  char            szFromTemp[MAX_BUF];
  char            szToTemp[MAX_BUF];
  char            szBuf[MAX_BUF];
  BOOL            bFound;

  /* From file path exists and To file path does not exist */
  if((FileExists(szFrom)) && (!FileExists(szTo)))
  {
    
    /* @MAK - need to handle OS/2 case where they are not the same drive*/
    DosMove(szFrom, szTo);

    /* log the file move command */
    sprintf(szBuf, "%s to %s", szFrom, szTo);
    UpdateInstallLog(KEY_MOVE_FILE, szBuf, FALSE);

    return(FO_SUCCESS);
  }
  /* From file path exists and To file path exists */
  if(FileExists(szFrom) && FileExists(szTo))
  {
    /* Since the To file path exists, assume it to be a directory and proceed.      */
    /* We don't care if it's a file.  If it is a file, then config.ini needs to be  */
    /* corrected to remove the file before attempting a MoveFile().                 */
    strcpy(szToTemp, szTo);
    AppendBackSlash(szToTemp, sizeof(szToTemp));
    ParsePath(szFrom, szBuf, sizeof(szBuf), FALSE, PP_FILENAME_ONLY);
    strcat(szToTemp, szBuf);
    DosMove(szFrom, szToTemp);

    /* log the file move command */
    sprintf(szBuf, "%s to %s", szFrom, szToTemp);
    UpdateInstallLog(KEY_MOVE_FILE, szBuf, FALSE);

    return(FO_SUCCESS);
  }

  ParsePath(szFrom, szFromDir, sizeof(szFromDir), FALSE, PP_PATH_ONLY);

  strcat(szFrom, "*.*");
  ulFindCount = 1;
  hFile = HDIR_CREATE;
  ulAttrs = FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_DIRECTORY | FILE_ARCHIVED;
  if((DosFindFirst(szFrom, &hFile, ulAttrs, &fdFile, sizeof(fdFile), &ulFindCount, FIL_STANDARD)) != NO_ERROR)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if((stricmp(fdFile.achName, ".") != 0) && (stricmp(fdFile.achName, "..") != 0))
    {
      /* create full path string including filename for source */
      strcpy(szFromTemp, szFromDir);
      AppendBackSlash(szFromTemp, sizeof(szFromTemp));
      strcat(szFromTemp, fdFile.achName);

      /* create full path string including filename for destination */
      strcpy(szToTemp, szTo);
      AppendBackSlash(szToTemp, sizeof(szToTemp));
      strcat(szToTemp, fdFile.achName);

      DosMove(szFromTemp, szToTemp);

      /* log the file move command */
      sprintf(szBuf, "%s to %s", szFromTemp, szToTemp);
      UpdateInstallLog(KEY_MOVE_FILE, szBuf, FALSE);
    }

    ulFindCount = 1;
    if (DosFindNext(hFile, &fdFile, sizeof(fdFile), &ulFindCount) == NO_ERROR) {
      bFound = TRUE;
    } else {
      bFound = FALSE;
    }
  }

  DosFindClose(hFile);
  return(FO_SUCCESS);
}

HRESULT ProcessMoveFile(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Move File", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szDestination, szBuf);
      FileMove(szSource, szDestination);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Move File", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT FileCopy(LPSTR szFrom, LPSTR szTo, BOOL bFailIfExists, BOOL bDnu)
{
  HDIR            hFile;
  FILEFINDBUF3    fdFile;
  ULONG           ulFindCount;
  ULONG           ulAttrs;
  char            szFromDir[MAX_BUF];
  char            szFromTemp[MAX_BUF];
  char            szToTemp[MAX_BUF];
  char            szBuf[MAX_BUF];
  BOOL            bFound;

  if(FileExists(szFrom))
  {
    /* The file in the From file path exists */
    ParsePath(szFrom, szBuf, sizeof(szBuf), FALSE, PP_FILENAME_ONLY);
    strcpy(szToTemp, szTo);
    AppendBackSlash(szToTemp, sizeof(szToTemp));
    strcat(szToTemp, szBuf);
    if (bFailIfExists) {
      DosCopy(szFrom, szToTemp, 0);
    } else {
      DosCopy(szFrom, szToTemp, DCPY_EXISTING);
    }
    sprintf(szBuf, "%s to %s", szFrom, szToTemp);
    UpdateInstallLog(KEY_COPY_FILE, szBuf, bDnu);

    return(FO_SUCCESS);
  }

  /* The file in the From file path does not exist.  Assume to contain wild args and */
  /* proceed acordingly.                                                             */
  ParsePath(szFrom, szFromDir, sizeof(szFromDir), FALSE, PP_PATH_ONLY);

  ulFindCount = 1;
  hFile = HDIR_CREATE;
  ulAttrs = FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_DIRECTORY | FILE_ARCHIVED;
  if((DosFindFirst(szFrom, &hFile, ulAttrs, &fdFile, sizeof(fdFile), &ulFindCount, FIL_STANDARD)) != NO_ERROR)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if((stricmp(fdFile.achName, ".") != 0) && (stricmp(fdFile.achName, "..") != 0))
    {
      /* create full path string including filename for source */
      strcpy(szFromTemp, szFromDir);
      AppendBackSlash(szFromTemp, sizeof(szFromTemp));
      strcat(szFromTemp, fdFile.achName);

      /* create full path string including filename for destination */
      strcpy(szToTemp, szTo);
      AppendBackSlash(szToTemp, sizeof(szToTemp));
      strcat(szToTemp, fdFile.achName);

      if (bFailIfExists) {
        DosCopy(szFromTemp, szToTemp, 0);
      } else {
        DosCopy(szFromTemp, szToTemp, DCPY_EXISTING);
      }

      /* log the file copy command */
      sprintf(szBuf, "%s to %s", szFromTemp, szToTemp);
      UpdateInstallLog(KEY_COPY_FILE, szBuf, bDnu);
    }

    ulFindCount = 1;
    if (DosFindNext(hFile, &fdFile, sizeof(fdFile), &ulFindCount) == NO_ERROR) {
      bFound = TRUE;
    } else {
      bFound = FALSE;
    }
  }

  DosFindClose(hFile);
  return(FO_SUCCESS);
}

HRESULT FileCopySequential(LPSTR szSourcePath, LPSTR szDestPath, LPSTR szFilename)
{
  int             iFilenameOnlyLen;
  char            szDestFullFilename[MAX_BUF];
  char            szSourceFullFilename[MAX_BUF];
  char            szSearchFilename[MAX_BUF];
  char            szSearchDestFullFilename[MAX_BUF];
  char            szFilenameOnly[MAX_BUF];
  char            szFilenameExtensionOnly[MAX_BUF];
  char            szNumber[MAX_BUF];
  long            dwNumber;
  long            dwMaxNumber;
  LPSTR           szDotPtr;
  HDIR            hFile;
  FILEFINDBUF3    fdFile;
  ULONG           ulFindCount;
  ULONG           ulAttrs;
  BOOL            bFound;

  strcpy(szSourceFullFilename, szSourcePath);
  AppendBackSlash(szSourceFullFilename, sizeof(szSourceFullFilename));
  strcat(szSourceFullFilename, szFilename);

  if(FileExists(szSourceFullFilename))
  {
    /* zero out the memory */
    memset(szSearchFilename, 0,        sizeof(szSearchFilename));
    memset(szFilenameOnly, 0,          sizeof(szFilenameOnly));
    memset(szFilenameExtensionOnly, 0, sizeof(szFilenameExtensionOnly));

    /* parse for the filename w/o extention and also only the extension */
    if((szDotPtr = strstr(szFilename, ".")) != NULL)
    {
      *szDotPtr = '\0';
      strcpy(szSearchFilename, szFilename);
      strcpy(szFilenameOnly, szFilename);
      strcpy(szFilenameExtensionOnly, &szDotPtr[1]);
      *szDotPtr = '.';
    }
    else
    {
      strcpy(szFilenameOnly, szFilename);
      strcpy(szSearchFilename, szFilename);
    }

    /* create the wild arg filename to search for in the szDestPath */
    strcat(szSearchFilename, "*.*");
    strcpy(szSearchDestFullFilename, szDestPath);
    AppendBackSlash(szSearchDestFullFilename, sizeof(szSearchDestFullFilename));
    strcat(szSearchDestFullFilename, szSearchFilename);

    iFilenameOnlyLen = strlen(szFilenameOnly);
    dwNumber         = 0;
    dwMaxNumber      = 0;

    /* find the largest numbered filename in the szDestPath */
    ulFindCount = 1;
    hFile = HDIR_CREATE;
    ulAttrs = FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_DIRECTORY | FILE_ARCHIVED;
    if((DosFindFirst(szSearchDestFullFilename, &hFile, ulAttrs, &fdFile, sizeof(fdFile), &ulFindCount, FIL_STANDARD)) != NO_ERROR)
      bFound = FALSE;
    else
      bFound = TRUE;

    while(bFound)
    {
      memset(szNumber, 0, sizeof(szNumber));
      if((stricmp(fdFile.achName, ".") != 0) && (stricmp(fdFile.achName, "..") != 0))
      {
        strcpy(szNumber, &fdFile.achName[iFilenameOnlyLen]);
        dwNumber = atoi(szNumber);
        if(dwNumber > dwMaxNumber)
          dwMaxNumber = dwNumber;
      }

      ulFindCount = 1;
      if (DosFindNext(hFile, &fdFile, sizeof(fdFile), &ulFindCount) == NO_ERROR) {
        bFound = TRUE;
      } else {
        bFound = FALSE;
      }
    }

    DosFindClose(hFile);

    strcpy(szDestFullFilename, szDestPath);
    AppendBackSlash(szDestFullFilename, sizeof(szDestFullFilename));
    strcat(szDestFullFilename, szFilenameOnly);
    _itoa(dwMaxNumber + 1, szNumber, 10);
    strcat(szDestFullFilename, szNumber);

    if(*szFilenameExtensionOnly != '\0')
    {
      strcat(szDestFullFilename, ".");
      strcat(szDestFullFilename, szFilenameExtensionOnly);
    }

    DosCopy(szSourceFullFilename, szDestFullFilename, 0);
  }

  return(FO_SUCCESS);
}

HRESULT ProcessCopyFile(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];
  BOOL  bFailIfExists;
  BOOL  bDnu;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Copy File", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szDestination, szBuf);

      GetPrivateProfileString(szSection, "Do Not Uninstall", "", szBuf, sizeof(szBuf), szFileIniConfig);
      if(stricmp(szBuf, "TRUE") == 0)
        bDnu = TRUE;
      else
        bDnu = FALSE;

      GetPrivateProfileString(szSection, "Fail If Exists", "", szBuf, sizeof(szBuf), szFileIniConfig);
      if(stricmp(szBuf, "TRUE") == 0)
        bFailIfExists = TRUE;
      else
        bFailIfExists = FALSE;

      FileCopy(szSource, szDestination, bFailIfExists, bDnu);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Copy File", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessCopyFileSequential(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];
  char  szFilename[MAX_BUF];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Copy File Sequential", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Filename", "", szFilename, sizeof(szFilename), szFileIniConfig);
  while(*szFilename != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szSource, szBuf);

      GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szDestination, szBuf);

      FileCopySequential(szSource, szDestination, szFilename);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Copy File Sequential", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Filename", "", szFilename, sizeof(szFilename), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

void UpdateInstallLog(PSZ szKey, PSZ szString, BOOL bDnu)
{
  FILE *fInstallLog;
  char szBuf[MAX_BUF];
  char szFileInstallLog[MAX_BUF];

  if(gbILUseTemp)
  {
    strcpy(szFileInstallLog, szTempDir);
    AppendBackSlash(szFileInstallLog, sizeof(szFileInstallLog));
  }
  else
  {
    strcpy(szFileInstallLog, sgProduct.szPath);
    AppendBackSlash(szFileInstallLog, sizeof(szFileInstallLog));
    strcat(szFileInstallLog, sgProduct.szSubPath);
    AppendBackSlash(szFileInstallLog, sizeof(szFileInstallLog));
  }

  CreateDirectoriesAll(szFileInstallLog, FALSE);
  strcat(szFileInstallLog, FILE_INSTALL_LOG);

  if((fInstallLog = fopen(szFileInstallLog, "a+")) != NULL)
  {
    if(bDnu)
      sprintf(szBuf, "     ** (*dnu*) %s%s\n", szKey, szString);
    else
      sprintf(szBuf, "     ** %s%s\n", szKey, szString);

    fwrite(szBuf, sizeof(char), strlen(szBuf), fInstallLog);
    fclose(fInstallLog);
  }
}

void UpdateInstallStatusLog(PSZ szString)
{
  FILE *fInstallLog;
  char szFileInstallStatusLog[MAX_BUF];

  if(gbILUseTemp)
  {
    strcpy(szFileInstallStatusLog, szTempDir);
    AppendBackSlash(szFileInstallStatusLog, sizeof(szFileInstallStatusLog));
  }
  else
  {
    strcpy(szFileInstallStatusLog, sgProduct.szPath);
    AppendBackSlash(szFileInstallStatusLog, sizeof(szFileInstallStatusLog));
    strcat(szFileInstallStatusLog, sgProduct.szSubPath);
    AppendBackSlash(szFileInstallStatusLog, sizeof(szFileInstallStatusLog));
  }

  CreateDirectoriesAll(szFileInstallStatusLog, FALSE);
  strcat(szFileInstallStatusLog, FILE_INSTALL_STATUS_LOG);

  if((fInstallLog = fopen(szFileInstallStatusLog, "a+")) != NULL)
  {
    fwrite(szString, sizeof(char), strlen(szString), fInstallLog);
    fclose(fInstallLog);
  }
}

void UpdateJSProxyInfo()
{
  FILE *fJSFile;
  char szBuf[MAX_BUF];
  char szJSFile[MAX_BUF];

  if((*diAdvancedSettings.szProxyServer != '\0') || (*diAdvancedSettings.szProxyPort != '\0'))
  {
    strcpy(szJSFile, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szJSFile, sizeof(szJSFile));
      strcat(szJSFile, sgProduct.szSubPath);
    }
    AppendBackSlash(szJSFile, sizeof(szJSFile));
    strcat(szJSFile, "defaults\\pref\\");
    CreateDirectoriesAll(szJSFile, TRUE);
    strcat(szJSFile, FILE_ALL_JS);

    if((fJSFile = fopen(szJSFile, "a+")) != NULL)
    {
      memset(szBuf, 0, sizeof(szBuf));
      if(*diAdvancedSettings.szProxyServer != '\0')
      {
        if(diAdditionalOptions.dwUseProtocol == UP_FTP)
          sprintf(szBuf,
                   "pref(\"network.proxy.ftp\", \"%s\");\n",
                   diAdvancedSettings.szProxyServer);
        else
          sprintf(szBuf,
                   "pref(\"network.proxy.http\", \"%s\");\n",
                   diAdvancedSettings.szProxyServer);
      }

      if(*diAdvancedSettings.szProxyPort != '\0')
      {
        if(diAdditionalOptions.dwUseProtocol == UP_FTP)
          sprintf(szBuf,
                   "pref(\"network.proxy.ftp_port\", %s);\n",
                   diAdvancedSettings.szProxyPort);
        else
          sprintf(szBuf,
                   "pref(\"network.proxy.http_port\", %s);\n",
                   diAdvancedSettings.szProxyPort);
      }

      strcat(szBuf, "pref(\"network.proxy.type\", 1);\n");

      fwrite(szBuf, sizeof(char), strlen(szBuf), fJSFile);
      fclose(fJSFile);
    }
  }
}

HRESULT CreateDirectoriesAll(char* szPath, BOOL bLogForUninstall)
{
  int     i;
  int     iLen = strlen(szPath);
  char    szCreatePath[MAX_BUF];
  HRESULT hrResult = 0;

  memset(szCreatePath, 0, MAX_BUF);
  memcpy(szCreatePath, szPath, iLen);
  for(i = 0; i < iLen; i++)
  {
    if((iLen > 1) &&
      ((i != 0) && ((szPath[i] == '\\') || (szPath[i] == '/'))) &&
      (!((szPath[0] == '\\') && (i == 1)) && !((szPath[1] == ':') && (i == 2))))
    {
      szCreatePath[i] = '\0';
      if(FileExists(szCreatePath) == FALSE)
      {
        APIRET rc = DosCreateDir(szCreatePath, NULL);  
        if (rc == NO_ERROR) {
          hrResult = 1;
        }

        if(bLogForUninstall)
          UpdateInstallLog(KEY_CREATE_FOLDER, szCreatePath, FALSE);
      }
      szCreatePath[i] = szPath[i];
    }
  }
  return(hrResult);
}

HRESULT ProcessCreateDirectory(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Create Directory", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      AppendBackSlash(szDestination, sizeof(szDestination));
      CreateDirectoriesAll(szDestination, TRUE);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Create Directory", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT FileDelete(LPSTR szDestination)
{
  HDIR            hFile;
  FILEFINDBUF3    fdFile;
  ULONG           ulFindCount;
  ULONG           ulAttrs;
  char            szBuf[MAX_BUF];
  char            szPathOnly[MAX_BUF];
  BOOL            bFound;

  if(FileExists(szDestination))
  {
    /* The file in the From file path exists */
    DosDelete(szDestination);
    return(FO_SUCCESS);
  }

  /* The file in the From file path does not exist.  Assume to contain wild args and */
  /* proceed acordingly.                                                             */
  ParsePath(szDestination, szPathOnly, sizeof(szPathOnly), FALSE, PP_PATH_ONLY);

  ulFindCount = 1;
  hFile = HDIR_CREATE;
  ulAttrs = FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_DIRECTORY | FILE_ARCHIVED;
  if((DosFindFirst(szDestination, &hFile, ulAttrs, &fdFile, sizeof(fdFile), &ulFindCount, FIL_STANDARD)) != NO_ERROR)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if(!(fdFile.attrFile & FILE_DIRECTORY))
    {
      strcpy(szBuf, szPathOnly);
      AppendBackSlash(szBuf, sizeof(szBuf));
      strcat(szBuf, fdFile.achName);

      DosDelete(szBuf);
    }

    ulFindCount = 1;
    if (DosFindNext(hFile, &fdFile, sizeof(fdFile), &ulFindCount) == NO_ERROR) {
      bFound = TRUE;
    } else {
      bFound = FALSE;
    }
  }

  DosFindClose(hFile);
  return(FO_SUCCESS);
}

HRESULT ProcessDeleteFile(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Delete File", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      FileDelete(szDestination);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Delete File", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT DirectoryRemove(LPSTR szDestination, BOOL bRemoveSubdirs)
{
  HDIR            hFile;
  FILEFINDBUF3    fdFile;
  ULONG           ulFindCount;
  ULONG           ulAttrs;
  char            szDestTemp[MAX_BUF];
  BOOL            bFound;

  if(!FileExists(szDestination))
    return(FO_SUCCESS);

  if(bRemoveSubdirs == TRUE)
  {
    strcpy(szDestTemp, szDestination);
    AppendBackSlash(szDestTemp, sizeof(szDestTemp));
    strcat(szDestTemp, "*");

    ulFindCount = 1;
    hFile = HDIR_CREATE;
    ulAttrs = FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_DIRECTORY | FILE_ARCHIVED;
    if((DosFindFirst(szDestTemp, &hFile, ulAttrs, &fdFile, sizeof(fdFile), &ulFindCount, FIL_STANDARD)) != NO_ERROR)
      bFound = FALSE;
    else
      bFound = TRUE;
    while(bFound == TRUE)
    {
      if((stricmp(fdFile.achName, ".") != 0) && (stricmp(fdFile.achName, "..") != 0))
      {
        /* create full path */
        strcpy(szDestTemp, szDestination);
        AppendBackSlash(szDestTemp, sizeof(szDestTemp));
        strcat(szDestTemp, fdFile.achName);

        if(fdFile.attrFile & FILE_DIRECTORY)
        {
          DirectoryRemove(szDestTemp, bRemoveSubdirs);
        }
        else
        {
          DosDelete(szDestTemp);
        }
      }

      ulFindCount = 1;
      if (DosFindNext(hFile, &fdFile, sizeof(fdFile), &ulFindCount) == NO_ERROR) {
        bFound = TRUE;
      } else {
        bFound = FALSE;
      }
    }

    DosFindClose(hFile);
  }
  
  DosDeleteDir(szDestination);
  return(FO_SUCCESS);
}

HRESULT ProcessRemoveDirectory(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];
  BOOL  bRemoveSubdirs;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Remove Directory", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      GetPrivateProfileString(szSection, "Remove subdirs", "", szBuf, sizeof(szBuf), szFileIniConfig);
      bRemoveSubdirs = FALSE;
      if(stricmp(szBuf, "TRUE") == 0)
        bRemoveSubdirs = TRUE;

      DirectoryRemove(szDestination, bRemoveSubdirs);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Remove Directory", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessRunApp(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szTarget[MAX_BUF];
  char  szParameters[MAX_BUF];
  char  szWorkingDir[MAX_BUF];
  BOOL  bRunApp;
  BOOL  bWait;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "RunApp", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Target", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szTarget, szBuf);
      GetPrivateProfileString(szSection, "Parameters", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szParameters, szBuf);

      // If we are given a criterion to test against, we expect also to be told whether we should run
      //    the app when that criterion is true or when it is false.  If we are not told, we assume that
      //    we are to run the app when the criterion is true.
      bRunApp = TRUE;
      GetPrivateProfileString(szSection, "Criterion ID", "", szBuf, sizeof(szBuf), szFileIniConfig);
      if(stricmp(szBuf, "RecaptureHP") == 0)
      {
        GetPrivateProfileString(szSection, "Run App If Criterion", "", szBuf, sizeof(szBuf), szFileIniConfig);
        if(stricmp(szBuf, "FALSE") == 0)
        {
          if(diAdditionalOptions.bRecaptureHomepage == TRUE)
             bRunApp = FALSE;
        }
        else
        {
          if(diAdditionalOptions.bRecaptureHomepage == FALSE)
             bRunApp = FALSE;
        }
      }

      GetPrivateProfileString(szSection, "WorkingDir", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szWorkingDir, szBuf);

      GetPrivateProfileString(szSection, "Wait", "", szBuf, sizeof(szBuf), szFileIniConfig);
      if(stricmp(szBuf, "FALSE") == 0)
        bWait = FALSE;
      else
        bWait = TRUE;

      if ((bRunApp == TRUE) && FileExists(szTarget))
      {
        if((dwTiming == T_DEPEND_REBOOT) && (NeedReboot() == TRUE))
        {
          strcat(szTarget, " ");
          strcat(szTarget, szParameters);
#ifdef OLDCODE /* @MAK */
          /* Add an object to the startup folder that points to szTarget and uses szParameters */
          /* and perhaps an xtra string that only we know (like deleteinstall)
          /* Then add code to nsNativeAppSupportOS2.cpp that when it sees this string, it */
          /* deletes the object from the startup folder */
          /* This is used to allow the app to run automatically on reboot, but clean up */
          /* after itself */
#endif
        }
        else
        {
          GetPrivateProfileString(szSection, "Message", "", szBuf, sizeof(szBuf), szFileIniConfig);
          if ( szBuf[0] != '\0' )
            ShowMessage(szBuf, TRUE);  
          WinSpawn(szTarget, szParameters, szWorkingDir, bWait);
          if ( szBuf[0] != '\0' )
            ShowMessage(szBuf, FALSE);  
        }
      }
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "RunApp", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Target", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessOS2INI(ULONG ulTiming, char *szSectionPrefix)
{
  char    szBuf[MAX_BUF];
  char    szApp[MAX_BUF];
  char    szKey[MAX_BUF];
  char    szValue[MAX_BUF];
  char    szDecrypt[MAX_BUF];
  char    szSection[MAX_BUF];
  BOOL    bDnu;
  ULONG   ulIndex;
  ULONG   ulSize;

  ulIndex = 0;
  BuildNumberedString(ulIndex, szSectionPrefix, "OS2 INI", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "App", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(ulTiming, szSection, szFileIniConfig))
    {
      GetPrivateProfileString(szSection, "App",                 "", szBuf,           sizeof(szBuf),          szFileIniConfig);
      GetPrivateProfileString(szSection, "Decrypt App",         "", szDecrypt,       sizeof(szDecrypt),      szFileIniConfig);
      memset(szApp, 0, sizeof(szApp));
      if(stricmp(szDecrypt, "TRUE") == 0)
        DecryptString(szApp, szBuf);
      else
        strcpy(szApp, szBuf);

      GetPrivateProfileString(szSection, "Key",                "", szBuf,           sizeof(szBuf),           szFileIniConfig);
      GetPrivateProfileString(szSection, "Decrypt Key",        "", szDecrypt,       sizeof(szDecrypt),       szFileIniConfig);
      memset(szKey, 0, sizeof(szKey));
      if(stricmp(szDecrypt, "TRUE") == 0)
        DecryptString(szKey, szBuf);
      else
        strcpy(szKey, szBuf);

      GetPrivateProfileString(szSection, "Key Value",          "", szBuf,           sizeof(szBuf), szFileIniConfig);
      GetPrivateProfileString(szSection, "Decrypt Key Value",  "", szDecrypt,       sizeof(szDecrypt), szFileIniConfig);
      memset(szValue, 0, sizeof(szValue));
      if(stricmp(szDecrypt, "TRUE") == 0)
        DecryptString(szValue, szBuf);
      else
        strcpy(szValue, szBuf);

      GetPrivateProfileString(szSection, "Size",                "", szBuf,           sizeof(szBuf), szFileIniConfig);
      if(*szBuf != '\0')
        ulSize = atoi(szBuf);
      else
        ulSize = 0;

      GetPrivateProfileString(szSection,
                              "Do Not Uninstall",
                              "",
                              szBuf,
                              sizeof(szBuf),
                              szFileIniConfig);
      if(stricmp(szBuf, "TRUE") == 0)
        bDnu = TRUE;
      else
        bDnu = FALSE;

      PrfWriteProfileString(HINI_USERPROFILE, szApp, szKey, szValue);
      /* log the win reg command */
      sprintf(szBuf, "%s [%s]", szApp, szKey);
      UpdateInstallLog(KEY_STORE_INI_ENTRY, szBuf, bDnu);
    }

    ++ulIndex;
    BuildNumberedString(ulIndex, szSectionPrefix, "OS2 INI", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "App", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

/* Helper function that moves a given hobject to the front of */
/* an association string in OS2.INI */

void MoveHOBJToFront(HOBJECT hobject, char* szApp, char* szKey)
{
  CHAR szhobj[CCHMAXPATH];
  CHAR *szOrigBuffer, *szNewBuffer, *szCurrNew, *szCurrOrig;
  ULONG ulSize;
  int i;

  sprintf(szhobj, "%d\0", hobject);
  PrfQueryProfileSize(HINI_USERPROFILE, szApp, szKey, &ulSize );
  szOrigBuffer = (char*)malloc(sizeof(char)*ulSize);
  szNewBuffer = (char*)malloc(sizeof(char)*ulSize);
  PrfQueryProfileData(HINI_USERPROFILE, szApp, szKey, szOrigBuffer, &ulSize);
  szCurrOrig = szOrigBuffer;
  szCurrNew = szNewBuffer;
  strncpy(szCurrNew, szhobj, 7);
  szCurrNew +=7;
  for (i=0; i<ulSize/7; i++) {
     if (strcmp(szCurrOrig, szhobj)) {
       strncpy(szCurrNew, szCurrOrig, 7);
       szCurrNew += 7;
     }
     szCurrOrig += 7;
  }
  PrfWriteProfileData(HINI_USERPROFILE, szApp, szKey, szNewBuffer, ulSize);
  free(szOrigBuffer);
  free(szNewBuffer);
}


HRESULT ProcessProgramFolder(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex0;
  DWORD dwIndex1;
  char  szIndex1[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szBuf2[MAX_BUF];
  char  szSection0[MAX_BUF];
  char  szSection1[MAX_BUF];
  char  szProgramFolder[MAX_BUF];

  char  szClassName[MAX_BUF];
  char  szTitle[MAX_BUF];
  char  szObjectID[MAX_BUF];
  char  szSetupString[MAX_BUF];
  char  szLocation[MAX_BUF];
  char  szAssocFilters[MAX_BUF];
  char  szAssocTypes[MAX_BUF];

  ULONG ulFlags = CO_REPLACEIFEXISTS;

  dwIndex0 = 0;
  BuildNumberedString(dwIndex0, szSectionPrefix, "Program Folder", szSection0, sizeof(szSection0));
  GetPrivateProfileString(szSection0, "Timing", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection0, szFileIniConfig))
    {
      DecryptString(szProgramFolder, szBuf);

      dwIndex1 = 0;
      _itoa(dwIndex1, szIndex1, 10);
      strcpy(szSection1, szSection0);
      strcat(szSection1, "-Object");
      strcat(szSection1, szIndex1);
      GetPrivateProfileString(szSection1, "Title", "", szBuf, sizeof(szBuf), szFileIniConfig);
      while(*szBuf != '\0')
      {
        *szSetupString = '\0';
        DecryptString(szTitle, szBuf);
        GetPrivateProfileString(szSection1, "Location",  "", szBuf, sizeof(szBuf), szFileIniConfig);
        DecryptString(szLocation, szBuf);
        GetPrivateProfileString(szSection1, "File",  "", szBuf, sizeof(szBuf), szFileIniConfig);
        DecryptString(szBuf2, szBuf);
        if (szBuf2[0]) {
          if (szSetupString[0]) {
            strcat(szSetupString, ";");
          }
          strcat(szSetupString, "EXENAME=");
          strcat(szSetupString, szBuf2);
        }
        GetPrivateProfileString(szSection1, "Parameters",  "", szBuf, sizeof(szBuf), szFileIniConfig);
        DecryptString(szBuf2, szBuf);
        if (szBuf2[0]) {
          if (szSetupString[0]) {
            strcat(szSetupString, ";");
          }
          strcat(szSetupString, "PARAMETERS=");
          strcat(szSetupString, szBuf2);
        }
        GetPrivateProfileString(szSection1, "Working Dir",  "", szBuf, sizeof(szBuf), szFileIniConfig);
        DecryptString(szBuf2, szBuf);
        if (szBuf2[0]) {
          if (szSetupString[0]) {
            strcat(szSetupString, ";");
          }
          strcat(szSetupString, "STARTUPDIR=");
          strcat(szSetupString, szBuf2);
        }
        GetPrivateProfileString(szSection1, "Object ID",  "", szObjectID, sizeof(szObjectID), szFileIniConfig);
        if (szObjectID[0]) {
          if (szSetupString[0]) {
            strcat(szSetupString, ";");
          }
          strcat(szSetupString, "OBJECTID=");
          strcat(szSetupString, szObjectID);
        }
        GetPrivateProfileString(szSection1, "Association Filters",  "", szAssocFilters, sizeof(szAssocFilters), szFileIniConfig);
        if (szAssocFilters[0]) {
          if (diOS2Integration.oiCBAssociateHTML.bCheckBoxState == TRUE) {
            if (szSetupString[0]) {
              strcat(szSetupString, ";");
            }
            strcat(szSetupString, "ASSOCFILTER=");
            strcat(szSetupString, szAssocFilters);
          }
        }
        GetPrivateProfileString(szSection1, "Association Types",  "", szAssocTypes, sizeof(szAssocTypes), szFileIniConfig);
        if (szAssocTypes[0]) {
          if (diOS2Integration.oiCBAssociateHTML.bCheckBoxState == TRUE) {
            if (szSetupString[0]) {
              strcat(szSetupString, ";");
            }
            strcat(szSetupString, "ASSOCTYPE=");
            strcat(szSetupString, szAssocTypes);
          }
        }
        GetPrivateProfileString(szSection1, "Setup String",  "", szBuf, sizeof(szBuf), szFileIniConfig);
        DecryptString(szBuf2, szBuf);
        if (szBuf2[0]) {
          if (szSetupString[0]) {
            strcat(szSetupString, ";");
          }
          strcat(szSetupString, szBuf2);
        }
        GetPrivateProfileString(szSection1, "ClassName", "", szBuf, sizeof(szBuf), szFileIniConfig);
        if (szBuf[0]) {
           strcpy(szClassName, szBuf);
        } else {
           strcpy(szClassName, "WPProgram");
        }

        GetPrivateProfileString(szSection1, "Attributes", "", szBuf, sizeof(szBuf), szFileIniConfig);
        if (szBuf[0]) {
          if (strcmp(szBuf, "UPDATE") == 0) {
            ulFlags = CO_UPDATEIFEXISTS;
          } else if (strcmp(szBuf, "FAIL") == 0) {
            ulFlags = CO_FAILIFEXISTS;
          }
        }

        WinCreateObject(szClassName, szTitle, szSetupString, szLocation, ulFlags);

        if (szObjectID[0]) {
          strcpy(szBuf, szObjectID);
        } else {
          strcpy(szBuf, szProgramFolder);
          AppendBackSlash(szBuf, sizeof(szBuf));
          strcat(szBuf, szTitle);
        }
        UpdateInstallLog(KEY_OS2_OBJECT, szBuf, FALSE);

        if ((diOS2Integration.oiCBAssociateHTML.bCheckBoxState == TRUE) &&
            (szAssocFilters[0] || szAssocTypes[0])) {
          HOBJECT hobj = WinQueryObject(szBuf);
          if (szAssocFilters[0]) {
            char *currFilter = strtok(szAssocFilters, ",");
            while (currFilter) {
              MoveHOBJToFront(hobj, "PMWP_ASSOC_FILTER", currFilter);
              currFilter = strtok(NULL, ",");
            }
          }
          if (szAssocTypes[0]) {
            char *currType = strtok(szAssocTypes, ",");
            while (currType) {
              MoveHOBJToFront(hobj, "PMWP_ASSOC_TYPE", currType);
              currType = strtok(NULL, ",");
            }
          }
        }

        ++dwIndex1;
        _itoa(dwIndex1, szIndex1, 10);
        strcpy(szSection1, szSection0);
        strcat(szSection1, "-Object");
        strcat(szSection1, szIndex1);
        GetPrivateProfileString(szSection1, "Title", "", szBuf, sizeof(szBuf), szFileIniConfig);
      }
    }

    ++dwIndex0;
    BuildNumberedString(dwIndex0, szSectionPrefix, "Program Folder", szSection0, sizeof(szSection0));
    GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessProgramFolderShowCmd()
{
  DWORD dwIndex0;
  int   iShowFolder;
  char  szBuf[MAX_BUF];
  char  szSection0[MAX_BUF];
  char  szProgramFolder[MAX_BUF];

  dwIndex0 = 0;
  BuildNumberedString(dwIndex0, NULL, "Program Folder", szSection0, sizeof(szSection0));
  GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    DecryptString(szProgramFolder, szBuf);
    GetPrivateProfileString(szSection0, "Show Folder", "", szBuf, sizeof(szBuf), szFileIniConfig);

#ifdef OLDCODE
    if(stricmp(szBuf, "HIDE") == 0)
      iShowFolder = SW_HIDE;
    else if(stricmp(szBuf, "MAXIMIZE") == 0)
      iShowFolder = SW_MAXIMIZE;
    else if(stricmp(szBuf, "MINIMIZE") == 0)
      iShowFolder = SW_MINIMIZE;
    else if(stricmp(szBuf, "RESTORE") == 0)
      iShowFolder = SW_RESTORE;
    else if(stricmp(szBuf, "SHOW") == 0)
      iShowFolder = SW_SHOW;
    else if(stricmp(szBuf, "SHOWMAXIMIZED") == 0)
      iShowFolder = SW_SHOWMAXIMIZED;
    else if(stricmp(szBuf, "SHOWMINIMIZED") == 0)
      iShowFolder = SW_SHOWMINIMIZED;
    else if(stricmp(szBuf, "SHOWMINNOACTIVE") == 0)
      iShowFolder = SW_SHOWMINNOACTIVE;
    else if(stricmp(szBuf, "SHOWNA") == 0)
      iShowFolder = SW_SHOWNA;
    else if(stricmp(szBuf, "SHOWNOACTIVATE") == 0)
      iShowFolder = SW_SHOWNOACTIVATE;
    else if(stricmp(szBuf, "SHOWNORMAL") == 0)
      iShowFolder = SW_SHOWNORMAL;

    if(iShowFolder != SW_HIDE)
      if(sgProduct.dwMode != SILENT)
        WinSpawn(szProgramFolder, NULL, NULL, TRUE);

#endif
    ++dwIndex0;
    BuildNumberedString(dwIndex0, NULL, "Program Folder", szSection0, sizeof(szSection0));
    GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessCreateCustomFiles(DWORD dwTiming)
{
  DWORD dwCompIndex;
  DWORD dwFileIndex;
  DWORD dwSectIndex;
  DWORD dwKVIndex;
  siC   *siCObject = NULL;
  char  szBufTiny[MAX_BUF_TINY];
  char  szSection[MAX_BUF_TINY];
  char  szBuf[MAX_BUF];
  char  szFileName[MAX_BUF];
  char  szDefinedSection[MAX_BUF]; 
  char  szDefinedKey[MAX_BUF]; 
  char  szDefinedValue[MAX_BUF];

  dwCompIndex   = 0;
  siCObject = SiCNodeGetObject(dwCompIndex, TRUE, AC_ALL);

  while(siCObject)
  {
    dwFileIndex   = 0;
    sprintf(szSection,"%s-Configuration File%d",siCObject->szReferenceName,dwFileIndex);
    siCObject = SiCNodeGetObject(++dwCompIndex, TRUE, AC_ALL);
    if(TimingCheck(dwTiming, szSection, szFileIniConfig) == FALSE)
    {
      continue;
    }

    GetPrivateProfileString(szSection, "FileName", "", szBuf, sizeof(szBuf), szFileIniConfig);
    while (*szBuf != '\0')
    {
      DecryptString(szFileName, szBuf);
      if(FileExists(szFileName))
      {
        DosDelete(szFileName);
      }

      /* TO DO - Support a File Type for something other than .ini */
      dwSectIndex = 0;
      sprintf(szBufTiny, "Section%d",dwSectIndex);
      GetPrivateProfileString(szSection, szBufTiny, "", szDefinedSection, sizeof(szDefinedSection), szFileIniConfig);
      while(*szDefinedSection != '\0')
      {  
        dwKVIndex =0;
        sprintf(szBufTiny,"Section%d-Key%d",dwSectIndex,dwKVIndex);
        GetPrivateProfileString(szSection, szBufTiny, "", szDefinedKey, sizeof(szDefinedKey), szFileIniConfig);
        while(*szDefinedKey != '\0')
        {
          sprintf(szBufTiny,"Section%d-Value%d",dwSectIndex,dwKVIndex);
          GetPrivateProfileString(szSection, szBufTiny, "", szBuf, sizeof(szBuf), szFileIniConfig);
          DecryptString(szDefinedValue, szBuf);
          if(WritePrivateProfileString(szDefinedSection, szDefinedKey, szDefinedValue, szFileName) == 0)
          {
            char szEWPPS[MAX_BUF];
            char szBuf[MAX_BUF];
            char szBuf2[MAX_BUF];
            if(GetPrivateProfileString("Messages", "ERROR_WRITEPRIVATEPROFILESTRING", "", szEWPPS, sizeof(szEWPPS), szFileIniInstall))
            {
              sprintf(szBuf, "%s\n    [%s]\n    %s=%s", szFileName, szDefinedSection, szDefinedKey, szDefinedValue);
              sprintf(szBuf2, szEWPPS, szBuf);
              PrintError(szBuf2, ERROR_CODE_SHOW);
            }
            return(FO_ERROR_WRITE);
          }
          sprintf(szBufTiny,"Section%d-Key%d",dwSectIndex,++dwKVIndex);
          GetPrivateProfileString(szSection, szBufTiny, "", szDefinedKey, sizeof(szDefinedKey), szFileIniConfig);
        } /* while(*szDefinedKey != '\0')  */

        sprintf(szBufTiny, "Section%d",++dwSectIndex);
        GetPrivateProfileString(szSection, szBufTiny, "", szDefinedSection, sizeof(szDefinedSection), szFileIniConfig);
      } /*       while(*szDefinedSection != '\0') */

      sprintf(szSection,"%s-Configuration File%d",siCObject->szReferenceName,++dwFileIndex);
      GetPrivateProfileString(szSection, "FileName", "", szBuf, sizeof(szBuf), szFileIniConfig);
    } /* while(*szBuf != '\0') */
  } /* while(siCObject) */
  return (FO_SUCCESS);
}

