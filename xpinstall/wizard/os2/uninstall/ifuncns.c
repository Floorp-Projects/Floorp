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

#include "ifuncns.h"
#include "extra.h"

BOOL SearchForUninstallKeys(char *szStringToMatch)
{
  char      szBuf[MAX_BUF];
  char      szStringToMatchLowerCase[MAX_BUF];
  char      szBufKey[MAX_BUF];
  char      szSubKey[MAX_BUF];
  HINI      hkHandle;
  BOOL      bFound;
  ULONG     dwIndex;
  ULONG     dwSubKeySize;
  ULONG     dwTotalSubKeys;
  ULONG     dwTotalValues;
  FILETIME  ftLastWriteFileTime;
  char      szWRMSUninstallKeyPath[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
  char      szWRMSUninstallName[] =  "UninstallString";

  strcpyn(szStringToMatchLowerCase, szStringToMatch, sizeof(szStringToMatchLowerCase));
  CharLower(szStringToMatchLowerCase);

  bFound = FALSE;
  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, szWRMSUninstallKeyPath, 0, KEY_READ, &hkHandle) != ERROR_SUCCESS)
    return(bFound);

  dwTotalSubKeys = 0;
  dwTotalValues  = 0;
  RegQueryInfoKey(hkHandle, NULL, NULL, NULL, &dwTotalSubKeys, NULL, NULL, &dwTotalValues, NULL, NULL, NULL, NULL);
  for(dwIndex = 0; dwIndex < dwTotalSubKeys; dwIndex++)
  {
    dwSubKeySize = sizeof(szSubKey);
    if(RegEnumKeyEx(hkHandle, dwIndex, szSubKey, &dwSubKeySize, NULL, NULL, NULL, &ftLastWriteFileTime) == ERROR_SUCCESS)
    {
      wsprintf(szBufKey, "%s\\%s", szWRMSUninstallKeyPath, szSubKey);
      GetWinReg(HKEY_LOCAL_MACHINE, szBufKey, szWRMSUninstallName, szBuf, sizeof(szBuf));
      CharLower(szBuf);
      if(strstr(szBuf, szStringToMatchLowerCase) != NULL)
      {
        bFound = TRUE;

        /* found one subkey. break out of the for() loop */
        break;
      }
    }
  }

  RegCloseKey(hkHandle);
  return(bFound);
}

APIRET FileMove(PSZ szFrom, PSZ szTo)
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


  /* From file path exists and To file path does not exist */
  if((FileExists(szFrom)) && (!FileExists(szTo)))
  {
    MoveFile(szFrom, szTo);
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
    ParsePath(szFrom, szBuf, MAX_BUF, PP_FILENAME_ONLY);
    strcat(szToTemp, szBuf);
    MoveFile(szFrom, szToTemp);
    return(FO_SUCCESS);
  }

  ParsePath(szFrom, szFromDir, MAX_BUF, PP_PATH_ONLY);

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

APIRET FileCopy(PSZ szFrom, PSZ szTo, BOOL bFailIfExists)
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
    ParsePath(szFrom, szBuf, MAX_BUF, PP_FILENAME_ONLY);
    strcpy(szToTemp, szTo);
    AppendBackSlash(szToTemp, sizeof(szToTemp));
    strcat(szToTemp, szBuf);
    CopyFile(szFrom, szToTemp, bFailIfExists);
    return(FO_SUCCESS);
  }

  /* The file in the From file path does not exist.  Assume to contain wild args and */
  /* proceed acordingly.                                                             */
  ParsePath(szFrom, szFromDir, MAX_BUF, PP_PATH_ONLY);

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

APIRET CreateDirectoriesAll(char* szPath)
{
  int     i;
  int     iLen = strlen(szPath);
  char    szCreatePath[MAX_BUF];
  APIRET hrResult;

  memset(szCreatePath, 0, MAX_BUF);
  memcpy(szCreatePath, szPath, iLen);
  for(i = 0; i < iLen; i++)
  {
    if((iLen > 1) &&
      ((i != 0) && ((szPath[i] == '\\') || (szPath[i] == '/'))) &&
      (!((szPath[0] == '\\') && (i == 1)) && !((szPath[1] == ':') && (i == 2))))
    {
      szCreatePath[i] = '\0';
      hrResult        = CreateDirectory(szCreatePath, NULL);
      szCreatePath[i] = szPath[i];
    }
  }
  return(hrResult);
}

APIRET FileDelete(PSZ szDestination)
{
  LHANDLE          hFile;
  FILEFINDBUF3    fdFile;
  char            szBuf[MAX_BUF];
  char            szPathOnly[MAX_BUF];
  BOOL            bFound;
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
  ParsePath(szDestination, szPathOnly, MAX_BUF, PP_PATH_ONLY);

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

APIRET DirectoryRemove(PSZ szDestination, BOOL bRemoveSubdirs)
{
  LHANDLE          hFile;
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

PSZ GetStringRootKey(HINI hkRootKey, PSZ szString, ULONG dwStringSize)
{
  if(hkRootKey == HKEY_CURRENT_CONFIG)
  {
    if(sizeof("HKEY_CURRENT_CONFIG") <= dwStringSize)
      strcpy(szString, "HKEY_CURRENT_CONFIG");
    else
      return(NULL);
  }
  else if(hkRootKey == HKEY_CURRENT_USER)
  {
    if(sizeof("HKEY_CURRENT_USER") <= dwStringSize)
      strcpy(szString, "HKEY_CURRENT_USER");
    else
      return(NULL);
  }
  else if(hkRootKey == HKEY_LOCAL_MACHINE)
  {
    if(sizeof("HKEY_LOCAL_MACHINE") <= dwStringSize)
      strcpy(szString, "HKEY_LOCAL_MACHINE");
    else
      return(NULL);
  }
  else if(hkRootKey == HKEY_USERS)
  {
    if(sizeof("HKEY_USERS") <= dwStringSize)
      strcpy(szString, "HKEY_USERS");
    else
      return(NULL);
  }
  else if(hkRootKey == HKEY_PERFORMANCE_DATA)
  {
    if(sizeof("HKEY_PERFORMANCE_DATA") <= dwStringSize)
      strcpy(szString, "HKEY_PERFORMANCE_DATA");
    else
      return(NULL);
  }
  else if(hkRootKey == HKEY_DYN_DATA)
  {
    if(sizeof("HKEY_DYN_DATA") <= dwStringSize)
      strcpy(szString, "HKEY_DYN_DATA");
    else
      return(NULL);
  }
  else
  {
    if(sizeof("HKEY_CLASSES_ROOT") <= dwStringSize)
      strcpy(szString, "HKEY_CLASSES_ROOT");
    else
      return(NULL);
  }

  return(szString);
}

