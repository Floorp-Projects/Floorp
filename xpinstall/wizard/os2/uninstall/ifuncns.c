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

#include "uninstall.h"
#include "extra.h"
#include "ifuncns.h"

BOOL SearchForUninstallKeys(char *szStringToMatch)
{
#ifdef OLDCODE
  char      szBuf[MAX_BUF];
  char      szStringToMatchLowerCase[MAX_BUF];
  char      szBufKey[MAX_BUF];
  char      szSubKey[MAX_BUF];
  HKEY      hkHandle;
  BOOL      bFound;
  DWORD     dwIndex;
  DWORD     dwSubKeySize;
  DWORD     dwTotalSubKeys;
  DWORD     dwTotalValues;
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
#endif
}

HRESULT FileMove(PSZ szFrom, PSZ szTo)
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
    DosMove(szFrom, szToTemp);
    return(FO_SUCCESS);
  }

  ParsePath(szFrom, szFromDir, MAX_BUF, PP_PATH_ONLY);

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

HRESULT FileCopy(PSZ szFrom, PSZ szTo, BOOL bFailIfExists)
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
    ParsePath(szFrom, szBuf, MAX_BUF, PP_FILENAME_ONLY);
    strcpy(szToTemp, szTo);
    AppendBackSlash(szToTemp, sizeof(szToTemp));
    strcat(szToTemp, szBuf);
    if (bFailIfExists) {
      DosCopy(szFrom, szToTemp, 0);
    } else {
      DosCopy(szFrom, szToTemp, DCPY_EXISTING);
    }

    return(FO_SUCCESS);
  }

  /* The file in the From file path does not exist.  Assume to contain wild args and */
  /* proceed acordingly.                                                             */
  ParsePath(szFrom, szFromDir, MAX_BUF, PP_PATH_ONLY);

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

HRESULT CreateDirectoriesAll(char* szPath)
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
      }
      szCreatePath[i] = szPath[i];
    }
  }
  return(hrResult);
}

HRESULT FileDelete(PSZ szDestination)
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
  ParsePath(szDestination, szPathOnly, MAX_BUF, PP_PATH_ONLY);

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

HRESULT DirectoryRemove(PSZ szDestination, BOOL bRemoveSubdirs)
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

