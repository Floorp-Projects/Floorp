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
#define INCL_DOSERRORS

#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include "shortcut.h"
#include "ifuncns.h"
#include "wizverreg.h"
#include "logging.h"
#include <logkeys.h>

APIRET TimingCheck(ULONG dwTiming, PSZ szSection, PSZ szFile)
{
  char szBuf[MAX_BUF_TINY];
  HINI hiniFile;

  hiniFile = PrfOpenProfile((HBA)0, szFile);
  PrfQueryProfileString(hiniFile, szSection, "Timing", "", szBuf, sizeof(szBuf));
  PrfCloseProfile(hiniFile);
  if(*szBuf != '\0')
  {
    switch(dwTiming)
    {
      case T_PRE_DOWNLOAD:
        if(strcmpi(szBuf, "pre download") == 0)
          return(TRUE);
        break;

      case T_POST_DOWNLOAD:
        if(strcmpi(szBuf, "post download") == 0)
          return(TRUE);
        break;

      case T_PRE_XPCOM:
        if(strcmpi(szBuf, "pre xpcom") == 0)
          return(TRUE);
        break;

      case T_POST_XPCOM:
        if(strcmpi(szBuf, "post xpcom") == 0)
          return(TRUE);
        break;

      case T_PRE_SMARTUPDATE:
        if(strcmpi(szBuf, "pre smartupdate") == 0)
          return(TRUE);
        break;

      case T_POST_SMARTUPDATE:
        if(strcmpi(szBuf, "post smartupdate") == 0)
          return(TRUE);
        break;

      case T_PRE_LAUNCHAPP:
        if(strcmpi(szBuf, "pre launchapp") == 0)
          return(TRUE);
        break;

      case T_POST_LAUNCHAPP:
        if(strcmpi(szBuf, "post launchapp") == 0)
          return(TRUE);
        break;

      case T_PRE_ARCHIVE:
        if(strcmpi(szBuf, "pre archive") == 0)
          return(TRUE);
        break;

      case T_POST_ARCHIVE:
        if(strcmpi(szBuf, "post archive") == 0)
          return(TRUE);
        break;

      case T_DEPEND_REBOOT:
        if(strcmpi(szBuf, "depend reboot") == 0)
          return(TRUE);
        break;
    }
  }
  return(FALSE);
}

char *BuildNumberedString(ULONG dwIndex, char *szInputStringPrefix, char *szInputString, char *szOutBuf, ULONG dwOutBufSize)
{
  if((szInputStringPrefix) && (*szInputStringPrefix != '\0'))
    sprintf(szOutBuf, "%s-%s%d", szInputStringPrefix, szInputString, dwIndex);
  else
    sprintf(szOutBuf, "%s%d", szInputString, dwIndex);

  return(szOutBuf);
}

void GetUserAgentShort(char *szUserAgent, char *szOutUAShort, ULONG dwOutUAShortSize)
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

char *GetWinRegSubKeyProductPath(HINI hkRootKey, char *szInKey, char *szReturnSubKey, ULONG dwReturnSubKeySize, char *szInSubSubKey, char *szInName, char *szCompare)
{
  char      *szRv = NULL;
  char      szKey[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      szCurrentVersion[MAX_BUF_TINY];
  HINI      hkHandle;
  ULONG     dwIndex;
  ULONG     dwBufSize;
  ULONG     dwTotalSubKeys;
  ULONG     dwTotalValues;
  FILETIME  ftLastWriteFileTime;

  /* get the current version value for this product */
  GetWinReg(hkRootKey, szInKey, "CurrentVersion", szCurrentVersion, sizeof(szCurrentVersion));

  if(RegOpenKeyEx(hkRootKey, szInKey, 0, KEY_READ, &hkHandle) != ERROR_SUCCESS)
    return(szRv);

  dwTotalSubKeys = 0;
  dwTotalValues  = 0;
  RegQueryInfoKey(hkHandle, NULL, NULL, NULL, &dwTotalSubKeys, NULL, NULL, &dwTotalValues, NULL, NULL, NULL, NULL);
  for(dwIndex = 0; dwIndex < dwTotalSubKeys; dwIndex++)
  {
    dwBufSize = dwReturnSubKeySize;
    if(RegEnumKeyEx(hkHandle, dwIndex, szReturnSubKey, &dwBufSize, NULL, NULL, NULL, &ftLastWriteFileTime) == ERROR_SUCCESS)
    {
      if((*szCurrentVersion != '\0') && (strcmpi(szCurrentVersion, szReturnSubKey) != 0))
      {
        /* The key found is not the CurrentVersion (current UserAgent), so we can return it to be deleted.
         * We don't want to return the SubKey that is the same as the CurrentVersion because it might
         * have just been created by the current installation process.  So deleting it would be a
         * "Bad Thing" (TM).
         *
         * If it was not created by the current installation process, then it'll be left
         * around which is better than deleting something we will need later. To make sure this case is
         * not encountered, CleanupPreviousVersionRegKeys() should be called at the *end* of the
         * installation process (at least after all the .xpi files have been processed). */
        if(szInSubSubKey && (*szInSubSubKey != '\0'))
          sprintf(szKey, "%s\\%s\\%s", szInKey, szReturnSubKey, szInSubSubKey);
        else
          sprintf(szKey, "%s\\%s", szInKey, szReturnSubKey);

        GetWinReg(hkRootKey, szKey, szInName, szBuf, sizeof(szBuf));
        AppendBackSlash(szBuf, sizeof(szBuf));
        if(strcmpi(szBuf, szCompare) == 0)
        {
          szRv = szReturnSubKey;
          /* found one subkey. break out of the for() loop */
          break;
        }
      }
    }
  }

  RegCloseKey(hkHandle);
  return(szRv);
}

void CleanupPreviousVersionRegKeys(void)
{
  char  *szRvSubKey;
  char  szSubKeyFound[CCHMAXPATH + 1];
  char  szSubSubKey[] = "Main";
  char  szName[] = "Install Directory";
  char  szWRMSUninstall[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
  char  szPath[MAX_BUF];
  char  szUAShort[MAX_BUF_TINY];
  char  szKey[MAX_BUF];

  strcpy(szPath, sgProduct.szPath);
  if(*sgProduct.szSubPath != '\0')
  {
    AppendBackSlash(szPath, sizeof(szPath));
    strcat(szPath, sgProduct.szSubPath);
  }
  AppendBackSlash(szPath, sizeof(szPath));

  do
  {
    /* build prodyct key path here */
    sprintf(szKey, "Software\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductName);
    szRvSubKey = GetWinRegSubKeyProductPath(HKEY_LOCAL_MACHINE, szKey, szSubKeyFound, sizeof(szSubKeyFound), szSubSubKey, szName, szPath);
    if(szRvSubKey)
    {
      AppendBackSlash(szKey, sizeof(szKey));
      strcat(szKey, szSubKeyFound);
      DeleteWinRegKey(HKEY_LOCAL_MACHINE, szKey, TRUE);

      GetUserAgentShort(szSubKeyFound, szUAShort, sizeof(szUAShort));
      if(*szUAShort != '\0')
      {
        /* delete uninstall key that contains product name and its user agent in parenthesis, for
         * example:
         *     Mozilla (0.8)
         */
        sprintf(szKey, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s (%s)", sgProduct.szProductName, szUAShort);
        DeleteWinRegKey(HKEY_LOCAL_MACHINE, szKey, TRUE);

        /* delete uninstall key that contains product name and its user agent not in parenthesis,
         * for example:
         *     Mozilla 0.8
         */
        sprintf(szKey, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s %s", sgProduct.szProductName, szUAShort);
        DeleteWinRegKey(HKEY_LOCAL_MACHINE, szKey, TRUE);

        /* We are not looking to delete just the product name key, for example:
         *     Mozilla
         *
         * because it might have just been created by the current installation process, so
         * deleting this would be a "Bad Thing" (TM).  Besides, we shouldn't be deleting the
         * CurrentVersion key that might have just gotten created because GetWinRegSubKeyProductPath()
         * will not return the CurrentVersion key.
         */
      }
    }

  } while(szRvSubKey);
}

void ProcessFileOps(ULONG dwTiming, char *szSectionPrefix)
{
  ProcessUncompressFile(dwTiming, szSectionPrefix);
  ProcessCreateDirectory(dwTiming, szSectionPrefix);
  ProcessMoveFile(dwTiming, szSectionPrefix);
  ProcessCopyFile(dwTiming, szSectionPrefix);
  ProcessCopyFileSequential(dwTiming, szSectionPrefix);
  ProcessSelfRegisterFile(dwTiming, szSectionPrefix);
  ProcessDeleteFile(dwTiming, szSectionPrefix);
  ProcessRemoveDirectory(dwTiming, szSectionPrefix);
  if(!gbIgnoreRunAppX)
    ProcessRunApp(dwTiming, szSectionPrefix);
  ProcessWinReg(dwTiming, szSectionPrefix);
  ProcessProgramFolder(dwTiming, szSectionPrefix);
  ProcessSetVersionRegistry(dwTiming, szSectionPrefix);
}

int VerifyArchive(PSZ szArchive)
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

APIRET ProcessSetVersionRegistry(ULONG dwTiming, char *szSectionPrefix)
{
  ULONG   dwIndex;
  BOOL    bIsDirectory;
  char    szBuf[MAX_BUF];
  char    szSection[MAX_BUF_TINY];
  char    szRegistryKey[MAX_BUF];
  char    szPath[MAX_BUF];
  char    szVersion[MAX_BUF_TINY];
  HINI      hiniConfig;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Version Registry", szSection, sizeof(szSection));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Registry Key", "", szRegistryKey, sizeof(szRegistryKey));
  while(*szRegistryKey != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      PrfQueryProfileString(hiniConfig, szSection, "Version", "", szVersion, sizeof(szVersion));
      PrfQueryProfileString(hiniConfig, szSection, "Path",    "", szBuf,     sizeof(szBuf));
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
    PrfQueryProfileString(hiniConfig, szSection, "Registry Key", "", szRegistryKey, sizeof(szRegistryKey));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

APIRET FileUncompress(PSZ szFrom, PSZ szTo)
{
  char  szBuf[MAX_BUF];
  ULONG dwReturn;
  ULONG cbPathLen = 0;
  void  *vZip;

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
  cbPathLen = sizeof(szBuf);
  DosQueryCurrentDir(0, szBuf, &cbPathLen);
  if(DosSetCurrentDir(szTo) != NO_ERROR)
    return(FO_ERROR_CHANGE_DIR);

  if((dwReturn = ZIP_OpenArchive(szFrom, &vZip)) != ZIP_OK)
    return(dwReturn);

  /* 1st parameter should be NULL or it will fail */
  /* It indicates extract the entire archive */
  dwReturn = ExtractDirEntries(NULL, vZip);
  ZIP_CloseArchive(&vZip);

  if(DosSetCurrentDir(szBuf) != NO_ERROR)
    return(FO_ERROR_CHANGE_DIR);

  return(dwReturn);
}

APIRET ProcessXpcomFile()
{
  char szSource[MAX_BUF];
  char szDestination[MAX_BUF];
  ULONG dwErr;

  if(*siCFXpcomFile.szMessage != '\0')
    ShowMessage(siCFXpcomFile.szMessage, TRUE);

  if((dwErr = FileUncompress(siCFXpcomFile.szSource, siCFXpcomFile.szDestination)) != FO_SUCCESS)
  {
    char szMsg[MAX_BUF];
    char szErrorString[MAX_BUF];
    HINI   hiniConfig;

    if(*siCFXpcomFile.szMessage != '\0')
      ShowMessage(siCFXpcomFile.szMessage, FALSE);

    LogISProcessXpcomFile(LIS_FAILURE, dwErr);
    hiniConfig = PrfOPneProfile((HAB)0, szFileIniConfig);
    PrfQueryProfileString(hiniConfig, "Strings", "Error File Uncompress", "", szErrorString, sizeof(szErrorString));
    PrfCloseProfile(hiniConfig);
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

APIRET CleanupXpcomFile()
{
  if(siCFXpcomFile.bCleanup == TRUE)
    DirectoryRemove(siCFXpcomFile.szDestination, TRUE);

  return(FO_SUCCESS);
}

APIRET ProcessUncompressFile(ULONG dwTiming, char *szSectionPrefix)
{
  ULONG   dwIndex;
  BOOL    bOnlyIfExists;
  char    szBuf[MAX_BUF];
  char    szSection[MAX_BUF];
  char    szSource[MAX_BUF];
  char    szDestination[MAX_BUF];
  HINI      hiniConfig;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Uncompress File", szSection, sizeof(szSection));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Source", "", szBuf, sizeof(szBuf));
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
      DecryptString(szDestination, szBuf);
      PrfQueryProfileString(hiniConfig, szSection, "Only If Exists", "", szBuf, sizeof(szBuf));
      if(strcmpi(szBuf, "TRUE") == 0)
        bOnlyIfExists = TRUE;
      else
        bOnlyIfExists = FALSE;

      if((!bOnlyIfExists) || (bOnlyIfExists && FileExists(szDestination)))
      {
        ULONG dwErr;

        PrfQueryProfileString(hiniConfig, szSection, "Message",     "", szBuf, sizeof(szBuf));
        ShowMessage(szBuf, TRUE);
        if((dwErr = FileUncompress(szSource, szDestination)) != FO_SUCCESS)
        {
          char szMsg[MAX_BUF];
          char szErrorString[MAX_BUF];

          ShowMessage(szBuf, FALSE);
          PrfQueryProfileString(hiniConfig, "Strings", "Error File Uncompress", "", szErrorString, sizeof(szErrorString));
          sprintf(szMsg, szErrorString, szSource, dwErr);
          PrintError(szMsg, ERROR_CODE_HIDE);
          PrfCloseProfile(hiniConfig);
          return(dwErr);
        }

        ShowMessage(szBuf, FALSE);
      }
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Uncompress File", szSection, sizeof(szSection));
    PrfQueryProfileString(hiniConfig, szSection, "Source", "", szBuf, sizeof(szBuf));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

APIRET FileMove(PSZ szFrom, PSZ szTo)
{
  LHANDLE          hFile;
  FILEFINDBUF3    fdFile;
  char            szFromDir[MAX_BUF];
  char            szFromTemp[MAX_BUF];
  char            szToTemp[MAX_BUF];
  char            szBuf[MAX_BUF];
  BOOL            bFound;
  HDIR            hdirFindHandle = HDIR_CREATE;
  ULONG         ulBufLen = sizeof(FILEFINDBUF3);
  ULONG         ulFindCount = 1;
  APIRET        rc = NO_ERROR;

  /* From file path exists and To file path does not exist */
  if((FileExists(szFrom)) && (!FileExists(szTo)))
  {
    MoveFile(szFrom, szTo);

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
    MoveFile(szFrom, szToTemp);

    /* log the file move command */
    sprintf(szBuf, "%s to %s", szFrom, szToTemp);
    UpdateInstallLog(KEY_MOVE_FILE, szBuf, FALSE);

    return(FO_SUCCESS);
  }

  ParsePath(szFrom, szFromDir, sizeof(szFromDir), FALSE, PP_PATH_ONLY);

  rc = DosFindFirst(szFrom, &hdirFindHandle, FILE_NORMAL, &fdFile, ulBufLen, &ulFindCount, FIL_STANDARD);
  if(rc == ERROR_INVALID_HANDLE)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if((strcmpi(fdFile.achName, ".") != 0) && (strcmpi(fdFile.achName, "..") != 0))
    {
      /* create full path string including filename for source */
      strcpy(szFromTemp, szFromDir);
      AppendBackSlash(szFromTemp, sizeof(szFromTemp));
      strcat(szFromTemp, fdFile.achName);

      /* create full path string including filename for destination */
      strcpy(szToTemp, szTo);
      AppendBackSlash(szToTemp, sizeof(szToTemp));
     strcat(szToTemp, fdFile.achName);

      MoveFile(szFromTemp, szToTemp);

      /* log the file move command */
      sprintf(szBuf, "%s to %s", szFromTemp, szToTemp);
      UpdateInstallLog(KEY_MOVE_FILE, szBuf, FALSE);
    }
    rc = DosFindNext(hdirFindHandle, &fdFile, ulBufLen, &ulFindCount);
    if(rc == NO_ERROR)
        bFound = TRUE;
    else
        bFound = FALSE;
  }

  DosFindClose(hdirFindHandle);
  return(FO_SUCCESS);
}

APIRET ProcessMoveFile(ULONG dwTiming, char *szSectionPrefix)
{
  ULONG dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];
  HINI   hiniConfig;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Move File", szSection, sizeof(szSection));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Source", "", szBuf, sizeof(szBuf));
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
      DecryptString(szDestination, szBuf);
      FileMove(szSource, szDestination);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Move File", szSection, sizeof(szSection));
    PrfQueryProfileString(hiniConfig, szSection, "Source", "", szBuf, sizeof(szBuf));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

APIRET FileCopy(PSZ szFrom, PSZ szTo, BOOL bFailIfExists, BOOL bDnu)
{
  LHANDLE          hFile;
  FILEFINDBUF3 fdFile;
  char            szFromDir[MAX_BUF];
  char            szFromTemp[MAX_BUF];
  char            szToTemp[MAX_BUF];
  char            szBuf[MAX_BUF];
  BOOL            bFound;
  HDIR            hdirFindHandle = HDIR_CREATE;
  ULONG         ulBufLen = sizeof(FILEFINDBUF3);
  ULONG         ulFindCount = 1;
  APIRET        rc = NO_ERROR;

  if(FileExists(szFrom))
  {
    /* The file in the From file path exists */
    ParsePath(szFrom, szBuf, sizeof(szBuf), FALSE, PP_FILENAME_ONLY);
    strcpy(szToTemp, szTo);
    AppendBackSlash(szToTemp, sizeof(szToTemp));
    strcat(szToTemp, szBuf);
    CopyFile(szFrom, szToTemp, bFailIfExists);
    sprintf(szBuf, "%s to %s", szFrom, szToTemp);
    UpdateInstallLog(KEY_COPY_FILE, szBuf, bDnu);

    return(FO_SUCCESS);
  }

  /* The file in the From file path does not exist.  Assume to contain wild args and */
  /* proceed acordingly.                                                             */
  ParsePath(szFrom, szFromDir, sizeof(szFromDir), FALSE, PP_PATH_ONLY);

  rc = DosFindFirst(szFrom, &hdirFindHandle, FILE_NORMAL, &fdFile, ulBufLen, &ulFindCount, FIL_STANDARD);
  if(rc == ERROR_INVALID_HANDLE)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if((strcmpi(fdFile.achName, ".") != 0) && (strcmpi(fdFile.achName, "..") != 0))
    {
      /* create full path string including filename for source */
      strcpy(szFromTemp, szFromDir);
      AppendBackSlash(szFromTemp, sizeof(szFromTemp));
      strcat(szFromTemp, fdFile.achName);

      /* create full path string including filename for destination */
      strcpy(szToTemp, szTo);
      AppendBackSlash(szToTemp, sizeof(szToTemp));
      strcat(szToTemp, fdFile.achName);

      CopyFile(szFromTemp, szToTemp, bFailIfExists);

      /* log the file copy command */
      sprintf(szBuf, "%s to %s", szFromTemp, szToTemp);
      UpdateInstallLog(KEY_COPY_FILE, szBuf, bDnu);
    }

    rc = DosFindNext(hdirFindHandle, &fdFile, ulBufLen, &ulFindCount);
    if(rc == NO_ERROR)
        bFound = TRUE;
    else
        bFound = FALSE;
  }

  DosFindClose(hdirFindHandle);
  return(FO_SUCCESS);
}

APIRET FileCopySequential(PSZ szSourcePath, PSZ szDestPath, PSZ szFilename)
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
  PSZ           szDotPtr;
  LHANDLE          hFile;
  FILEFINDBUF3 fdFile;
  BOOL            bFound;
  HDIR            hdirFindHandle = HDIR_CREATE;
  ULONG         ulBufLen = sizeof(FILEFINDBUF3);
  ULONG         ulFindCount = 1;
  APIRET        rc = NO_ERROR;

  strcpy(szSourceFullFilename, szSourcePath);
  AppendBackSlash(szSourceFullFilename, sizeof(szSourceFullFilename));
  strcat(szSourceFullFilename, szFilename);

  if(FileExists(szSourceFullFilename))
  {
    /* zero out the memory */
    memset(szSearchFilename, 0, sizeof(szSearchFilename));
    memset(szFilenameOnly, 0, sizeof(szFilenameOnly));
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
    rc = DosFindFirst(szSearchDestFullFilename, &hdirFindHandle, FILE_NORMAL, &fdFile, ulBufLen, &ulFindCount, FIL_STANDARD);
    if(rc == ERROR_INVALID_HANDLE)
      bFound = FALSE;
    else
      bFound = TRUE;

    while(bFound)
    {
       memset(szNumber, 0, sizeof(szNumber));
      if((strcmpi(fdFile.achName, ".") != 0) && (strcmpi(fdFile.achName, "..") != 0))
      {
        strcpy(szNumber, &fdFile.achName[iFilenameOnlyLen]);
        dwNumber = atoi(szNumber);
        if(dwNumber > dwMaxNumber)
          dwMaxNumber = dwNumber;
      }

      rc = DosFindNext(hdirFindHandle, &fdFile, ulBufLen, &ulFindCount);
    if(rc == NO_ERROR)
        bFound = TRUE;
    else
        bFound = FALSE;
    }

    DosFindClose(hdirFindHandle);

    strcpy(szDestFullFilename, szDestPath);
    AppendBackSlash(szDestFullFilename, sizeof(szDestFullFilename));
    strcat(szDestFullFilename, szFilenameOnly);
    itoa(dwMaxNumber + 1, szNumber, 10);
    strcat(szDestFullFilename, szNumber);

    if(*szFilenameExtensionOnly != '\0')
    {
      strcat(szDestFullFilename, ".");
      strcat(szDestFullFilename, szFilenameExtensionOnly);
    }

    CopyFile(szSourceFullFilename, szDestFullFilename, TRUE);
  }

  return(FO_SUCCESS);
}

APIRET ProcessCopyFile(ULONG dwTiming, char *szSectionPrefix)
{
  ULONG dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];
  BOOL  bFailIfExists;
  BOOL  bDnu;
  HINI    hiniConfig;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Copy File", szSection, sizeof(szSection));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Source", "", szBuf, sizeof(szBuf));
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
      DecryptString(szDestination, szBuf);

      PrfQueryProfileString(hiniConfig, szSection, "Do Not Uninstall", "", szBuf, sizeof(szBuf));
      if(strcmpi(szBuf, "TRUE") == 0)
        bDnu = TRUE;
      else
        bDnu = FALSE;

      PrfQueryProfileString(hiniConfig, szSection, "Fail If Exists", "", szBuf, sizeof(szBuf));
      if(strcmpi(szBuf, "TRUE") == 0)
        bFailIfExists = TRUE;
      else
        bFailIfExists = FALSE;

      FileCopy(szSource, szDestination, bFailIfExists, bDnu);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Copy File", szSection, sizeof(szSection));
    PrfQueryProfileString(hiniConfig, szSection, "Source", "", szBuf, sizeof(szBuf));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

APIRET ProcessCopyFileSequential(ULONG dwTiming, char *szSectionPrefix)
{
  ULONG dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];
  char  szFilename[MAX_BUF];
  HINI   hiniConfig;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Copy File Sequential", szSection, sizeof(szSection));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig); 
  PrfQueryProfileString(hiniConfig, szSection, "Filename", "", szFilename, sizeof(szFilename));
  while(*szFilename != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      PrfQueryProfileString(hiniConfig, szSection, "Source", "", szBuf, sizeof(szBuf));
      DecryptString(szSource, szBuf);

      PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
      DecryptString(szDestination, szBuf);

      FileCopySequential(szSource, szDestination, szFilename);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Copy File Sequential", szSection, sizeof(szSection));
    PrfQueryProfileString(hiniConfig, szSection, "Filename", "", szFilename, sizeof(szFilename));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

int RegisterDll32(char *File)
{
  PFN   DllReg;
  HMODULE hLib;

  
  if(DosLoadModule(NULL, 0, File, &hLib) == NO_ERROR)
  {
    if((DosQueryProcAddr(hLib, 0, "DllRegisterServer", DllReg)) == NO_ERROR)
      DllReg();

    DosFreeModule(hLib);
    return(0);
  }

  return(1);
}


APIRET FileSelfRegister(PSZ szFilename, PSZ szDestination)
{
  char            szFullFilenamePath[MAX_BUF];
  ULONG           dwRv;
  LHANDLE          hFile;
  FILEFINDBUF3 fdFile;
  BOOL            bFound;
  HDIR            hdirFindHandle = HDIR_CREATE;
  ULONG         ulBufLen = sizeof(FILEFINDBUF3);
  ULONG         ulFindCount = 1;
  APIRET        rc = NO_ERROR;

  strcpy(szFullFilenamePath, szDestination);
  AppendBackSlash(szFullFilenamePath, sizeof(szFullFilenamePath));
  strcat(szFullFilenamePath, szFilename);

  /* From file path exists and To file path does not exist */
  if(FileExists(szFullFilenamePath))
  {
    RegisterDll32(szFullFilenamePath);
    return(FO_SUCCESS);
  }

  strcpy(szFullFilenamePath, szDestination);
  AppendBackSlash(szFullFilenamePath, sizeof(szFullFilenamePath));
  strcat(szFullFilenamePath, szFilename);

  rc = DosFindFirst(szFullFilenamePath, &hdirFindHandle, FILE_NORMAL, &fdFile, ulBufLen, &ulFindCount, FIL_STANDARD);
  if(rc == ERROR_INVALID_HANDLE)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if((strcmpi(fdFile.achName, ".") != 0) && (strcmpi(fdFile.achName, "..") != 0))
    {
      /* create full path string including filename for destination */
      strcpy(szFullFilenamePath, szDestination);
      AppendBackSlash(szFullFilenamePath, sizeof(szFullFilenamePath));
      strcat(szFullFilenamePath, fdFile.achName);

      if((dwRv = FileExists(szFullFilenamePath)) && (dwRv != FILE_DIRECTORY))
        RegisterDll32(szFullFilenamePath);
    }

    rc = DosFindNext(hdirFindHandle, &fdFile, ulBufLen, &ulFindCount);
    if(rc == NO_ERROR)
        bFound = TRUE;
    else
        bFound = FALSE;
  }

  DosFindClose(hdirFindHandle);
  return(FO_SUCCESS);
}

APIRET ProcessSelfRegisterFile(ULONG dwTiming, char *szSectionPrefix)
{
  ULONG dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szFilename[MAX_BUF];
  char  szDestination[MAX_BUF];
  HINI   hiniConfig;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Self Register File", szSection, sizeof(szSection));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      PrfQueryProfileString(hiniConfig, szSection, "Filename", "", szFilename, sizeof(szFilename));
      FileSelfRegister(szFilename, szDestination);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Self Register File", szSection, sizeof(szSection));
    PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
  }
  PrfCloseProfile(hiniConfig);
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

  if((fInstallLog = fopen(szFileInstallLog, "a+t")) != NULL)
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

  if((fInstallLog = fopen(szFileInstallStatusLog, "a+t")) != NULL)
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

    if((fJSFile = fopen(szJSFile, "a+t")) != NULL)
    {
      memset(szBuf, 0, sizeof(szBuf));
      if(*diAdvancedSettings.szProxyServer != '\0')
      {
        if(diDownloadOptions.dwUseProtocol == UP_FTP)
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
        if(diDownloadOptions.dwUseProtocol == UP_FTP)
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

APIRET CreateDirectoriesAll(char* szPath, BOOL bLogForUninstall)
{
  int     i;
  int     iLen = strlen(szPath);
  char    szCreatePath[MAX_BUF];
  APIRET hrResult = 0;

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
        hrResult = CreateDirectory(szCreatePath, NULL);

        if(bLogForUninstall)
          UpdateInstallLog(KEY_CREATE_FOLDER, szCreatePath, FALSE);
      }
      szCreatePath[i] = szPath[i];
    }
  }
  return(hrResult);
}

APIRET ProcessCreateDirectory(ULONG dwTiming, char *szSectionPrefix)
{
  ULONG dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];
  HINI   hiniConfig;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Create Directory", szSection, sizeof(szSection));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
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
    PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

APIRET FileDelete(PSZ szDestination)
{
  FILEFINDBUF3    fdFile;
  char            szBuf[MAX_BUF];
  char            szPathOnly[MAX_BUF];
  BOOL           bFound;
  HDIR            hdirFindHandle = HDIR_CREATE;
  ULONG         ulBufLen = sizeof(FILEFINDBUF3);
  ULONG         ulFindCount = 1;
  APIRET        rc = NO_ERROR;

  if(FileExists(szDestination))
  {
    /* The file in the From file path exists */
    DosDelete(szDestination);
    return(FO_SUCCESS);
  }

  /* The file in the From file path does not exist.  Assume to contain wild args and */
  /* proceed acordingly.                                                             */
  ParsePath(szDestination, szPathOnly, sizeof(szPathOnly), FALSE, PP_PATH_ONLY);

  rc = DosFindFirst(szDestination, &hdirFindHandle, FILE_NORMAL, &fdFile, ulBufLen, &ulFindCount, FIL_STANDARD);
  if(rc == ERROR_INVALID_HANDLE)
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

    rc = DosFindNext(hdirFindHandle, &fdFile, ulBufLen, &ulFindCount);
    if(rc == NO_ERROR)
        bFound = TRUE;
    else
        bFound = FALSE;
  }

  DosFindClose(hdirFindHandle);
  return(FO_SUCCESS);
}

APIRET ProcessDeleteFile(ULONG dwTiming, char *szSectionPrefix)
{
  ULONG dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];
  HINI   hiniConfig;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Delete File", szSection, sizeof(szSection));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      FileDelete(szDestination);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Delete File", szSection, sizeof(szSection));
    PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

APIRET DirectoryRemove(PSZ szDestination, BOOL bRemoveSubdirs)
{
  FILEFINDBUF3    fdFile;
  char            szDestTemp[MAX_BUF];
  BOOL            bFound;
  HDIR            hdirFindHandle = HDIR_CREATE;
  ULONG         ulBufLen = sizeof(FILEFINDBUF3);
  ULONG         ulFindCount = 1;
  APIRET        rc = NO_ERROR;

  if(!FileExists(szDestination))
    return(FO_SUCCESS);

  if(bRemoveSubdirs == TRUE)
  {
    strcpy(szDestTemp, szDestination);
    AppendBackSlash(szDestTemp, sizeof(szDestTemp));
    strcat(szDestTemp, "*");

    bFound = TRUE;
    rc = DosFindFirst(szDestTemp, &hdirFindHandle, FILE_NORMAL, &fdFile, ulBufLen, &ulFindCount, FIL_STANDARD);
    while((rc != ERROR_INVALID_HANDLE) && (bFound == TRUE))
    {
      if((strcmpi(fdFile.achName, ".") != 0) && (strcmpi(fdFile.achName, "..") != 0))
      {
        /* create full path */
        strcpy(szDestTemp, szDestination);
        AppendBackSlash(szDestTemp, sizeof(szDestTemp));
        strcat(szDestTemp, fdFile.achFileName);

        if(fdFile.attrFile & FILE_DIRECTORY)
        {
          DirectoryRemove(szDestTemp, bRemoveSubdirs);
        }
        else
        {
          DosDelete(szDestTemp);
        }
      }

      rc = DosFindNext(hdirFindHandle, &fdFile, ulBufLen, &ulFindCount);
    if(rc == NO_ERROR)
        bFound = TRUE;
    else
        bFound = FALSE;
    }

    DosFindClose(hdirFindHandle);
  }
  
  RemoveDirectory(szDestination);
  return(FO_SUCCESS);
}

APIRET ProcessRemoveDirectory(ULONG dwTiming, char *szSectionPrefix)
{
  ULONG dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];
  BOOL  bRemoveSubdirs;
  HINI    hiniConfig;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Remove Directory", szSection, sizeof(szSection));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      PrfQueryProfileString(hiniConfig, szSection, "Remove subdirs", "", szBuf, sizeof(szBuf));
      bRemoveSubdirs = FALSE;
      if(strcmpi(szBuf, "TRUE") == 0)
        bRemoveSubdirs = TRUE;

      DirectoryRemove(szDestination, bRemoveSubdirs);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Remove Directory", szSection, sizeof(szSection));
    PrfQueryProfileString(hiniConfig, szSection, "Destination", "", szBuf, sizeof(szBuf));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

APIRET ProcessRunApp(ULONG dwTiming, char *szSectionPrefix)
{
  ULONG dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szTarget[MAX_BUF];
  char  szParameters[MAX_BUF];
  char  szWorkingDir[MAX_BUF];
  BOOL  bWait;
  HINI   hiniConfig;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "RunApp", szSection, sizeof(szSection));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Target", "", szBuf, sizeof(szBuf));
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szTarget, szBuf);
      PrfQueryProfileString(hiniConfig, szSection, "Parameters", "", szBuf, sizeof(szBuf));
      DecryptString(szParameters, szBuf);
      PrfQueryProfileString(hiniConfig, szSection, "WorkingDir", "", szBuf, sizeof(szBuf));
      DecryptString(szWorkingDir, szBuf);
      PrfQueryProfileString(hiniConfig, szSection, "Wait", "", szBuf, sizeof(szBuf));

      if(strcmpi(szBuf, "FALSE") == 0)
        bWait = FALSE;
      else
        bWait = TRUE;

      if((dwTiming == T_DEPEND_REBOOT) && (NeedReboot() == TRUE))
      {
        strcat(szTarget, " ");
        strcat(szTarget, szParameters);
        SetWinReg(HKEY_CURRENT_USER,
                  "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
                  TRUE,
                  "Netscape",
                  TRUE,
                  REG_SZ,
                  szTarget,
                  strlen(szTarget),
                  FALSE,
                  FALSE);
      }
      else
        WinSpawn(szTarget, szParameters, szWorkingDir, SW_SHOWNORMAL, bWait);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "RunApp", szSection, sizeof(szSection));
    PrfQueryProfileString(hiniConfig, szSection, "Target", "", szBuf, sizeof(szBuf));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

ULONG ParseRestrictedAccessKey(PSZ szKey)
{
  ULONG dwKey;

  if(strcmpi(szKey, "ONLY_RESTRICTED") == 0)
    dwKey = RA_ONLY_RESTRICTED;
  else if(strcmpi(szKey, "ONLY_NONRESTRICTED") == 0)
    dwKey = RA_ONLY_NONRESTRICTED;
  else
    dwKey = RA_IGNORE;

  return(dwKey);
}

HINI ParseRootKey(PSZ szRootKey)
{
  HINI hkRootKey;

  if(strcmpi(szRootKey, "HKEY_CURRENT_CONFIG") == 0)
    hkRootKey = HKEY_CURRENT_CONFIG;
  else if(strcmpi(szRootKey, "HKEY_CURRENT_USER") == 0)
    hkRootKey = HKEY_CURRENT_USER;
  else if(strcmpi(szRootKey, "HKEY_LOCAL_MACHINE") == 0)
    hkRootKey = HKEY_LOCAL_MACHINE;
  else if(strcmpi(szRootKey, "HKEY_USERS") == 0)
    hkRootKey = HKEY_USERS;
  else if(strcmpi(szRootKey, "HKEY_PERFORMANCE_DATA") == 0)
    hkRootKey = HKEY_PERFORMANCE_DATA;
  else if(strcmpi(szRootKey, "HKEY_DYN_DATA") == 0)
    hkRootKey = HKEY_DYN_DATA;
  else /* HKEY_CLASSES_ROOT */
    hkRootKey = HKEY_CLASSES_ROOT;

  return(hkRootKey);
}

char *ParseRootKeyString(HINI hkKey, PSZ szRootKey, ULONG dwRootKeyBufSize)
{
  if(!szRootKey)
    return(NULL);

  memset(szRootKey, 0, dwRootKeyBufSize);
  if((hkKey == HKEY_CURRENT_CONFIG) &&
    ((long)dwRootKeyBufSize > strlen("HKEY_CURRENT_CONFIG")))
    strcpy(szRootKey, "HKEY_CURRENT_CONFIG");
  else if((hkKey == HKEY_CURRENT_USER) &&
         ((long)dwRootKeyBufSize > strlen("HKEY_CURRENT_USER")))
    strcpy(szRootKey, "HKEY_CURRENT_USER");
  else if((hkKey == HKEY_LOCAL_MACHINE) &&
         ((long)dwRootKeyBufSize > strlen("HKEY_LOCAL_MACHINE")))
    strcpy(szRootKey, "HKEY_LOCAL_MACHINE");
  else if((hkKey == HKEY_USERS) &&
         ((long)dwRootKeyBufSize > strlen("HKEY_USERS")))
    strcpy(szRootKey, "HKEY_USERS");
  else if((hkKey == HKEY_PERFORMANCE_DATA) &&
         ((long)dwRootKeyBufSize > strlen("HKEY_PERFORMANCE_DATA")))
    strcpy(szRootKey, "HKEY_PERFORMANCE_DATA");
  else if((hkKey == HKEY_DYN_DATA) &&
         ((long)dwRootKeyBufSize > strlen("HKEY_DYN_DATA")))
    strcpy(szRootKey, "HKEY_DYN_DATA");
  else if((long)dwRootKeyBufSize > strlen("HKEY_CLASSES_ROOT"))
    strcpy(szRootKey, "HKEY_CLASSES_ROOT");

  return(szRootKey);
}

BOOL ParseRegType(PSZ szType, ULONG *dwType)
{
  BOOL bSZ;

  if(strcmpi(szType, "REG_SZ") == 0)
  {
    /* Unicode NULL terminated string */
    *dwType = REG_SZ;
    bSZ     = TRUE;
  }
  else if(strcmpi(szType, "REG_EXPAND_SZ") == 0)
  {
    /* Unicode NULL terminated string
     * (with environment variable references) */
    *dwType = REG_EXPAND_SZ;
    bSZ     = TRUE;
  }
  else if(strcmpi(szType, "REG_BINARY") == 0)
  {
    /* Free form binary */
    *dwType = REG_BINARY;
    bSZ     = FALSE;
  }
  else if(strcmpi(szType, "REG_ULONG") == 0)
  {
    /* 32bit number */
    *dwType = REG_ULONG;
    bSZ     = FALSE;
  }
  else if(strcmpi(szType, "REG_ULONG_LITTLE_ENDIAN") == 0)
  {
    /* 32bit number
     * (same as REG_ULONG) */
    *dwType = REG_ULONG_LITTLE_ENDIAN;
    bSZ     = FALSE;
  }
  else if(strcmpi(szType, "REG_ULONG_BIG_ENDIAN") == 0)
  {
    /* 32bit number */
    *dwType = REG_ULONG_BIG_ENDIAN;
    bSZ     = FALSE;
  }
  else if(strcmpi(szType, "REG_LINK") == 0)
  {
    /* Symbolic link (unicode) */
    *dwType = REG_LINK;
    bSZ     = TRUE;
  }
  else if(strcmpi(szType, "REG_MULTI_SZ") == 0)
  {
    /* Multiple Unicode strings */
    *dwType = REG_MULTI_SZ;
    bSZ     = TRUE;
  }
  else /* Default is REG_NONE */
  {
    /* no value type */
    *dwType = REG_NONE;
    bSZ     = TRUE;
  }

  return(bSZ);
}

BOOL WinRegKeyExists(HINI hkRootKey, PSZ szKey)
{
  HINI  hkResult;
  ULONG dwErr;
  BOOL  bKeyExists = FALSE;

  if((dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult)) == ERROR_SUCCESS)
  {
    bKeyExists = TRUE;
    RegCloseKey(hkResult);
  }

  return(bKeyExists);
}

BOOL WinRegNameExists(HINI hkRootKey, PSZ szKey, PSZ szName)
{
  HINI  hkResult;
  ULONG dwErr;
  ULONG dwSize;
  char  szBuf[MAX_BUF];
  BOOL  bNameExists = FALSE;

  memset(szBuf, 0, sizeof(szBuf));
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

void DeleteWinRegKey(HINI hkRootKey, PSZ szKey, BOOL bAbsoluteDelete)
{
  HINI      hkResult;
  ULONG     dwErr;
  ULONG     dwTotalSubKeys;
  ULONG     dwTotalValues;
  ULONG     dwSubKeySize;
  FILETIME  ftLastWriteFileTime;
  char      szSubKey[MAX_BUF_TINY];
  char      szNewKey[MAX_BUF];
  long      lRv;

  dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_QUERY_VALUE, &hkResult);
  if(dwErr == ERROR_SUCCESS)
  {
    dwTotalSubKeys = 0;
    dwTotalValues  = 0;
    RegQueryInfoKey(hkResult, NULL, NULL, NULL, &dwTotalSubKeys, NULL, NULL, &dwTotalValues, NULL, NULL, NULL, NULL);
    RegCloseKey(hkResult);

    if(((dwTotalSubKeys == 0) && (dwTotalValues == 0)) || bAbsoluteDelete)
    {
      if(dwTotalSubKeys && bAbsoluteDelete)
      {
        do
        {
          dwSubKeySize = sizeof(szSubKey);
          lRv = 0;
          if(RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult) == ERROR_SUCCESS)
          {
            if((lRv = RegEnumKeyEx(hkResult, 0, szSubKey, &dwSubKeySize, NULL, NULL, NULL, &ftLastWriteFileTime)) == ERROR_SUCCESS)
            {
              RegCloseKey(hkResult);
              strcpy(szNewKey, szKey);
              AppendBackSlash(szNewKey, sizeof(szNewKey));
              strcat(szNewKey, szSubKey);
              DeleteWinRegKey(hkRootKey, szNewKey, bAbsoluteDelete);
            }
            else
              RegCloseKey(hkResult);
          }
        } while(lRv != ERROR_NO_MORE_ITEMS);
      }

      dwErr = RegDeleteKey(hkRootKey, szKey);
    }
  }
}

void DeleteWinRegValue(HINI hkRootKey, PSZ szKey, PSZ szName)
{
  HINI    hkResult;
  ULONG   dwErr;

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

ULONG GetWinReg(HINI hkRootKey, PSZ szKey, PSZ szName, PSZ szReturnValue, ULONG dwReturnValueSize)
{
  HINI  hkResult;
  ULONG dwErr;
  ULONG dwSize;
  ULONG dwType;
  char  szBuf[MAX_BUF];

  memset(szBuf, 0, sizeof(szBuf));
  memset(szReturnValue, 0, dwReturnValueSize);

  if((dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult)) == ERROR_SUCCESS)
  {
    dwSize = sizeof(szBuf);
    dwErr  = RegQueryValueEx(hkResult, szName, 0, &dwType, szBuf, &dwSize);

    if((dwType == REG_MULTI_SZ) && (*szBuf != '\0'))
    {
      ULONG dwCpSize;

      dwCpSize = dwReturnValueSize < dwSize ? (dwReturnValueSize - 1) : dwSize;
      memcpy(szReturnValue, szBuf, dwCpSize);
    }
    else if((*szBuf != '\0') && (dwErr == ERROR_SUCCESS))
      ExpandEnvironmentStrings(szBuf, szReturnValue, dwReturnValueSize);
    else
      *szReturnValue = '\0';

    RegCloseKey(hkResult);
  }

  return(dwType);
}

void SetWinReg(HINI hkRootKey,
               PSZ szKey,
               BOOL bOverwriteKey,
               PSZ szName,
               BOOL bOverwriteName,
               ULONG dwType,
               LPBYTE lpbData,
               ULONG dwSize,
               BOOL bLogForUninstall,
               BOOL bDnu)
{
  HINI    hkResult;
  ULONG   dwErr;
  ULONG   dwDisp;
  BOOL    bKeyExists;
  BOOL    bNameExists;
  char    szBuf[MAX_BUF];
  char    szRootKey[MAX_BUF_TINY];


  bKeyExists  = WinRegKeyExists(hkRootKey, szKey);
  bNameExists = WinRegNameExists(hkRootKey, szKey, szName);
  dwErr       = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_WRITE, &hkResult);

  if(dwErr != ERROR_SUCCESS)
  {
    dwErr = RegCreateKeyEx(hkRootKey, szKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkResult, &dwDisp);
    /* log the win reg command */
    if(bLogForUninstall &&
       ParseRootKeyString(hkRootKey, szRootKey, sizeof(szRootKey)))
    {
      sprintf(szBuf, "%s\\%s []", szRootKey, szKey);
      UpdateInstallLog(KEY_CREATE_REG_KEY, szBuf, bDnu);
    }
  }

  if(dwErr == ERROR_SUCCESS)
  {
    if((bNameExists == FALSE) ||
      ((bNameExists == TRUE) && (bOverwriteName == TRUE)))
    {
      dwErr = RegSetValueEx(hkResult, szName, 0, dwType, lpbData, dwSize);
      /* log the win reg command */
      if(bLogForUninstall &&
         ParseRootKeyString(hkRootKey, szRootKey, sizeof(szRootKey)))
      {
        if(ParseRegType(szBuf, &dwType))
        {
          sprintf(szBuf, "%s\\%s [%s]", szRootKey, szKey, szName);
          UpdateInstallLog(KEY_STORE_REG_STRING, szBuf, bDnu);
        }
        else
        {
          sprintf(szBuf, "%s\\%s [%s]", szRootKey, szKey, szName);
          UpdateInstallLog(KEY_STORE_REG_NUMBER, szBuf, bDnu);
        }
      }
    }

    RegCloseKey(hkResult);
  }
}

APIRET ProcessWinReg(ULONG dwTiming, char *szSectionPrefix)
{
  char    szBuf[MAX_BUF];
  char    szKey[MAX_BUF];
  char    szName[MAX_BUF];
  char    szValue[MAX_BUF];
  char    szDecrypt[MAX_BUF];
  char    szOverwriteKey[MAX_BUF];
  char    szOverwriteName[MAX_BUF];
  char    szSection[MAX_BUF];
  HINI    hRootKey;
  BOOL    bDnu;
  BOOL    bOverwriteKey;
  BOOL    bOverwriteName;
  ULONG   dwIndex;
  ULONG   dwType;
  ULONG   dwSize;
  __int64 iiNum;
  HINI       hiniConfig;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Windows Registry", szSection, sizeof(szSection));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection, "Root Key", "", szBuf, sizeof(szBuf));
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      hRootKey = ParseRootKey(szBuf);

      PrfQueryProfileString(hiniConfig, szSection, "Key",                 "", szBuf,           sizeof(szBuf));
      PrfQueryProfileString(hiniConfig, szSection, "Decrypt Key",         "", szDecrypt,       sizeof(szDecrypt));
      PrfQueryProfileString(hiniConfig, szSection, "Overwrite Key",       "", szOverwriteKey,  sizeof(szOverwriteKey));

      if(strcmpi(szDecrypt, "TRUE") == 0)
        DecryptString(szKey, szBuf);
      else
        strcpy(szKey, szBuf);

      if(strcmpi(szOverwriteKey, "FALSE") == 0)
        bOverwriteKey = FALSE;
      else
        bOverwriteKey = TRUE;

      PrfQueryProfileString(hiniConfig, szSection, "Name",                "", szBuf,           sizeof(szBuf));
      PrfQueryProfileString(hiniConfig, szSection, "Decrypt Name",        "", szDecrypt,       sizeof(szDecrypt));
      PrfQueryProfileString(hiniConfig, szSection, "Overwrite Name",      "", szOverwriteName, sizeof(szOverwriteName));

      if(strcmpi(szDecrypt, "TRUE") == 0)
        DecryptString(szName, szBuf);
      else
        strcpy(szName, szBuf);

      if(strcmpi(szOverwriteName, "FALSE") == 0)
        bOverwriteName = FALSE;
      else
        bOverwriteName = TRUE;

      PrfQueryProfileString(hiniConfig, szSection, "Name Value",          "", szBuf,           sizeof(szBuf));
      PrfQueryProfileString(hiniConfig, szSection, "Decrypt Name Value",  "", szDecrypt,       sizeof(szDecrypt));
      if(strcmpi(szDecrypt, "TRUE") == 0)
        DecryptString(szValue, szBuf);
      else
        strcpy(szValue, szBuf);

      PrfQueryProfileString(hiniConfig, szSection, "Size",                "", szBuf,           sizeof(szBuf));
      if(*szBuf != '\0')
        dwSize = atoi(szBuf);
      else
        dwSize = 0;

      PrfQueryProfileString(hiniConfig, szSection,
                              "Do Not Uninstall",
                              "",
                              szBuf,
                              sizeof(szBuf));
      if(strcmpi(szBuf, "TRUE") == 0)
        bDnu = TRUE;
      else
        bDnu = FALSE;

      PrfQueryProfileString(hiniConfig, szSection, "Type",                "", szBuf,           sizeof(szBuf));
      if(ParseRegType(szBuf, &dwType))
      {
        /* create/set windows registry key here (string value)! */
        SetWinReg(hRootKey, szKey, bOverwriteKey, szName, bOverwriteName,
                  dwType, (CONST LPBYTE)szValue, strlen(szValue), TRUE, bDnu);
      }
      else
      {
        iiNum = _atoi64(szValue);
        /* create/set windows registry key here (binary/dword value)! */
        SetWinReg(hRootKey, szKey, bOverwriteKey, szName, bOverwriteName,
                  dwType, (CONST LPBYTE)&iiNum, dwSize, TRUE, bDnu);
      }
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Windows Registry", szSection, sizeof(szSection));
    PrfQueryProfileString(hiniConfig, szSection, "Root Key", "", szBuf, sizeof(szBuf));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

APIRET ProcessProgramFolder(ULONG dwTiming, char *szSectionPrefix)
{
  ULONG dwIndex0;
  ULONG dwIndex1;
  ULONG dwIconId;
  ULONG dwRestrictedAccess;
  char  szIndex1[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szSection0[MAX_BUF];
  char  szSection1[MAX_BUF];
  char  szProgramFolder[MAX_BUF];
  char  szFile[MAX_BUF];
  char  szArguments[MAX_BUF];
  char  szWorkingDir[MAX_BUF];
  char  szDescription[MAX_BUF];
  char  szIconPath[MAX_BUF];
  HINI   hiniConfig;

  dwIndex0 = 0;
  BuildNumberedString(dwIndex0, szSectionPrefix, "Program Folder", szSection0, sizeof(szSection0));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection0, "Program Folder", "", szBuf, sizeof(szBuf));
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection0, szFileIniConfig))
    {
      DecryptString(szProgramFolder, szBuf);

      dwIndex1 = 0;
      itoa(dwIndex1, szIndex1, 10);
      strcpy(szSection1, szSection0);
      strcat(szSection1, "-Shortcut");
      strcat(szSection1, szIndex1);
      PrfQueryProfileString(hiniConfig, szSection1, "File", "", szBuf, sizeof(szBuf));
      while(*szBuf != '\0')
      {
        DecryptString(szFile, szBuf);
        PrfQueryProfileString(hiniConfig, szSection1, "Arguments",    "", szBuf, sizeof(szBuf));
        DecryptString(szArguments, szBuf);
        PrfQueryProfileString(hiniConfig, szSection1, "Working Dir",  "", szBuf, sizeof(szBuf));
        DecryptString(szWorkingDir, szBuf);
        PrfQueryProfileString(hiniConfig, szSection1, "Description",  "", szBuf, sizeof(szBuf));
        DecryptString(szDescription, szBuf);
        PrfQueryProfileString(hiniConfig, szSection1, "Icon Path",    "", szBuf, sizeof(szBuf));
        DecryptString(szIconPath, szBuf);
        PrfQueryProfileString(hiniConfig, szSection1, "Icon Id",      "", szBuf, sizeof(szBuf));
        if(*szBuf != '\0')
          dwIconId = atol(szBuf);
        else
          dwIconId = 0;

        PrfQueryProfileString(hiniConfig, szSection1, "Restricted Access",    "", szBuf, sizeof(szBuf));
        dwRestrictedAccess = ParseRestrictedAccessKey(szBuf);
        if((dwRestrictedAccess == RA_IGNORE) ||
          ((dwRestrictedAccess == RA_ONLY_RESTRICTED) && gbRestrictedAccess) ||
          ((dwRestrictedAccess == RA_ONLY_NONRESTRICTED) && !gbRestrictedAccess))
        {
          CreateALink(szFile, szProgramFolder, szDescription, szWorkingDir, szArguments, szIconPath, dwIconId);
          strcpy(szBuf, szProgramFolder);
          AppendBackSlash(szBuf, sizeof(szBuf));
          strcat(szBuf, szDescription);
          UpdateInstallLog(KEY_WINDOWS_SHORTCUT, szBuf, FALSE);
        }

        ++dwIndex1;
        itoa(dwIndex1, szIndex1, 10);
        strcpy(szSection1, szSection0);
        strcat(szSection1, "-Shortcut");
        strcat(szSection1, szIndex1);
        PrfQueryProfileString(hiniConfig, szSection1, "File", "", szBuf, sizeof(szBuf));
      }
    }

    ++dwIndex0;
    BuildNumberedString(dwIndex0, szSectionPrefix, "Program Folder", szSection0, sizeof(szSection0));
    PrfQueryProfileString(hiniConfig, szSection0, "Program Folder", "", szBuf, sizeof(szBuf));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

APIRET ProcessProgramFolderShowCmd()
{
  ULONG dwIndex0;
  int   iShowFolder;
  char  szBuf[MAX_BUF];
  char  szSection0[MAX_BUF];
  char  szProgramFolder[MAX_BUF];
  HINI   hiniConfig;

  dwIndex0 = 0;
  BuildNumberedString(dwIndex0, NULL, "Program Folder", szSection0, sizeof(szSection0));
  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, szSection0, "Program Folder", "", szBuf, sizeof(szBuf));
  while(*szBuf != '\0')
  {
    DecryptString(szProgramFolder, szBuf);
    PrfQueryProfileString(hiniConfig, szSection0, "Show Folder", "", szBuf, sizeof(szBuf));

    if(strcmpi(szBuf, "HIDE") == 0)
      iShowFolder = SW_HIDE;
    else if(strcmpi(szBuf, "MAXIMIZE") == 0)
      iShowFolder = SW_MAXIMIZE;
    else if(strcmpi(szBuf, "MINIMIZE") == 0)
      iShowFolder = SW_MINIMIZE;
    else if(strcmpi(szBuf, "RESTORE") == 0)
      iShowFolder = SW_RESTORE;
    else if(strcmpi(szBuf, "SHOW") == 0)
      iShowFolder = SW_SHOW;
    else if(strcmpi(szBuf, "SHOWMAXIMIZED") == 0)
      iShowFolder = SW_SHOWMAXIMIZED;
    else if(strcmpi(szBuf, "SHOWMINIMIZED") == 0)
      iShowFolder = SW_SHOWMINIMIZED;
    else if(strcmpi(szBuf, "SHOWMINNOACTIVE") == 0)
      iShowFolder = SW_SHOWMINNOACTIVE;
    else if(strcmpi(szBuf, "SHOWNA") == 0)
      iShowFolder = SW_SHOWNA;
    else if(strcmpi(szBuf, "SHOWNOACTIVATE") == 0)
      iShowFolder = SW_SHOWNOACTIVATE;
    else if(strcmpi(szBuf, "SHOWNORMAL") == 0)
      iShowFolder = SW_SHOWNORMAL;

    if(iShowFolder != SW_HIDE)
      if(sgProduct.dwMode != SILENT)
        WinSpawn(szProgramFolder, NULL, NULL, iShowFolder, TRUE);

    ++dwIndex0;
    BuildNumberedString(dwIndex0, NULL, "Program Folder", szSection0, sizeof(szSection0));
    PrfQueryProfileString(hiniConfig, szSection0, "Program Folder", "", szBuf, sizeof(szBuf));
  }
  PrfCloseProfile(hiniConfig);
  return(FO_SUCCESS);
}

