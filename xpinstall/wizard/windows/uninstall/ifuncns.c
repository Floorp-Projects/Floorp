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

#include "ifuncns.h"
#include "extra.h"

BOOL SearchForUninstallKeys(char *szStringToMatch)
{
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

  lstrcpyn(szStringToMatchLowerCase, szStringToMatch, sizeof(szStringToMatchLowerCase));
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

HRESULT FileMove(LPSTR szFrom, LPSTR szTo)
{
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  char            szFromDir[MAX_BUF];
  char            szFromTemp[MAX_BUF];
  char            szToTemp[MAX_BUF];
  char            szBuf[MAX_BUF];
  BOOL            bFound;

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
    lstrcpy(szToTemp, szTo);
    AppendBackSlash(szToTemp, sizeof(szToTemp));
    ParsePath(szFrom, szBuf, MAX_BUF, PP_FILENAME_ONLY);
    lstrcat(szToTemp, szBuf);
    MoveFile(szFrom, szToTemp);
    return(FO_SUCCESS);
  }

  ParsePath(szFrom, szFromDir, MAX_BUF, PP_PATH_ONLY);

  if((hFile = FindFirstFile(szFrom, &fdFile)) == INVALID_HANDLE_VALUE)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
    {
      /* create full path string including filename for source */
      lstrcpy(szFromTemp, szFromDir);
      AppendBackSlash(szFromTemp, sizeof(szFromTemp));
      lstrcat(szFromTemp, fdFile.cFileName);

      /* create full path string including filename for destination */
      lstrcpy(szToTemp, szTo);
      AppendBackSlash(szToTemp, sizeof(szToTemp));
      lstrcat(szToTemp, fdFile.cFileName);

      MoveFile(szFromTemp, szToTemp);
    }

    bFound = FindNextFile(hFile, &fdFile);
  }

  FindClose(hFile);
  return(FO_SUCCESS);
}

HRESULT FileCopy(LPSTR szFrom, LPSTR szTo, BOOL bFailIfExists)
{
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  char            szFromDir[MAX_BUF];
  char            szFromTemp[MAX_BUF];
  char            szToTemp[MAX_BUF];
  char            szBuf[MAX_BUF];
  BOOL            bFound;

  if(FileExists(szFrom))
  {
    /* The file in the From file path exists */
    ParsePath(szFrom, szBuf, MAX_BUF, PP_FILENAME_ONLY);
    lstrcpy(szToTemp, szTo);
    AppendBackSlash(szToTemp, sizeof(szToTemp));
    lstrcat(szToTemp, szBuf);
    CopyFile(szFrom, szToTemp, bFailIfExists);
    return(FO_SUCCESS);
  }

  /* The file in the From file path does not exist.  Assume to contain wild args and */
  /* proceed acordingly.                                                             */
  ParsePath(szFrom, szFromDir, MAX_BUF, PP_PATH_ONLY);

  if((hFile = FindFirstFile(szFrom, &fdFile)) == INVALID_HANDLE_VALUE)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
    {
      /* create full path string including filename for source */
      lstrcpy(szFromTemp, szFromDir);
      AppendBackSlash(szFromTemp, sizeof(szFromTemp));
      lstrcat(szFromTemp, fdFile.cFileName);

      /* create full path string including filename for destination */
      lstrcpy(szToTemp, szTo);
      AppendBackSlash(szToTemp, sizeof(szToTemp));
      lstrcat(szToTemp, fdFile.cFileName);

      CopyFile(szFromTemp, szToTemp, bFailIfExists);
    }

    bFound = FindNextFile(hFile, &fdFile);
  }

  FindClose(hFile);
  return(FO_SUCCESS);
}

HRESULT CreateDirectoriesAll(char* szPath)
{
  int     i;
  int     iLen = lstrlen(szPath);
  char    szCreatePath[MAX_BUF];
  HRESULT hrResult;

  ZeroMemory(szCreatePath, MAX_BUF);
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

HRESULT FileDelete(LPSTR szDestination)
{
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  char            szBuf[MAX_BUF];
  char            szPathOnly[MAX_BUF];
  BOOL            bFound;

  if(FileExists(szDestination))
  {
    /* The file in the From file path exists */
    DeleteFile(szDestination);
    return(FO_SUCCESS);
  }

  /* The file in the From file path does not exist.  Assume to contain wild args and */
  /* proceed acordingly.                                                             */
  ParsePath(szDestination, szPathOnly, MAX_BUF, PP_PATH_ONLY);

  if((hFile = FindFirstFile(szDestination, &fdFile)) == INVALID_HANDLE_VALUE)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if(!(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
      lstrcpy(szBuf, szPathOnly);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, fdFile.cFileName);

      DeleteFile(szBuf);
    }

    bFound = FindNextFile(hFile, &fdFile);
  }

  FindClose(hFile);
  return(FO_SUCCESS);
}

HRESULT DirectoryRemove(LPSTR szDestination, BOOL bRemoveSubdirs)
{
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  char            szDestTemp[MAX_BUF];
  BOOL            bFound;

  if(!FileExists(szDestination))
    return(FO_SUCCESS);

  if(bRemoveSubdirs == TRUE)
  {
    lstrcpy(szDestTemp, szDestination);
    AppendBackSlash(szDestTemp, sizeof(szDestTemp));
    lstrcat(szDestTemp, "*");

    bFound = TRUE;
    hFile = FindFirstFile(szDestTemp, &fdFile);
    while((hFile != INVALID_HANDLE_VALUE) && (bFound == TRUE))
    {
      if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
      {
        /* create full path */
        lstrcpy(szDestTemp, szDestination);
        AppendBackSlash(szDestTemp, sizeof(szDestTemp));
        lstrcat(szDestTemp, fdFile.cFileName);

        if(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
          DirectoryRemove(szDestTemp, bRemoveSubdirs);
        }
        else
        {
          DeleteFile(szDestTemp);
        }
      }

      bFound = FindNextFile(hFile, &fdFile);
    }

    FindClose(hFile);
  }
  
  RemoveDirectory(szDestination);
  return(FO_SUCCESS);
}

HKEY ParseRootKey(LPSTR szRootKey)
{
  HKEY hkRootKey;

  if(lstrcmpi(szRootKey, "HKEY_CURRENT_CONFIG") == 0)
    hkRootKey = HKEY_CURRENT_CONFIG;
  else if(lstrcmpi(szRootKey, "HKEY_CURRENT_USER") == 0)
    hkRootKey = HKEY_CURRENT_USER;
  else if(lstrcmpi(szRootKey, "HKEY_LOCAL_MACHINE") == 0)
    hkRootKey = HKEY_LOCAL_MACHINE;
  else if(lstrcmpi(szRootKey, "HKEY_USERS") == 0)
    hkRootKey = HKEY_USERS;
  else if(lstrcmpi(szRootKey, "HKEY_PERFORMANCE_DATA") == 0)
    hkRootKey = HKEY_PERFORMANCE_DATA;
  else if(lstrcmpi(szRootKey, "HKEY_DYN_DATA") == 0)
    hkRootKey = HKEY_DYN_DATA;
  else /* HKEY_CLASSES_ROOT */
    hkRootKey = HKEY_CLASSES_ROOT;

  return(hkRootKey);
}

LPSTR GetStringRootKey(HKEY hkRootKey, LPSTR szString, DWORD dwStringSize)
{
  if(hkRootKey == HKEY_CURRENT_CONFIG)
  {
    if(sizeof("HKEY_CURRENT_CONFIG") <= dwStringSize)
      lstrcpy(szString, "HKEY_CURRENT_CONFIG");
    else
      return(NULL);
  }
  else if(hkRootKey == HKEY_CURRENT_USER)
  {
    if(sizeof("HKEY_CURRENT_USER") <= dwStringSize)
      lstrcpy(szString, "HKEY_CURRENT_USER");
    else
      return(NULL);
  }
  else if(hkRootKey == HKEY_LOCAL_MACHINE)
  {
    if(sizeof("HKEY_LOCAL_MACHINE") <= dwStringSize)
      lstrcpy(szString, "HKEY_LOCAL_MACHINE");
    else
      return(NULL);
  }
  else if(hkRootKey == HKEY_USERS)
  {
    if(sizeof("HKEY_USERS") <= dwStringSize)
      lstrcpy(szString, "HKEY_USERS");
    else
      return(NULL);
  }
  else if(hkRootKey == HKEY_PERFORMANCE_DATA)
  {
    if(sizeof("HKEY_PERFORMANCE_DATA") <= dwStringSize)
      lstrcpy(szString, "HKEY_PERFORMANCE_DATA");
    else
      return(NULL);
  }
  else if(hkRootKey == HKEY_DYN_DATA)
  {
    if(sizeof("HKEY_DYN_DATA") <= dwStringSize)
      lstrcpy(szString, "HKEY_DYN_DATA");
    else
      return(NULL);
  }
  else
  {
    if(sizeof("HKEY_CLASSES_ROOT") <= dwStringSize)
      lstrcpy(szString, "HKEY_CLASSES_ROOT");
    else
      return(NULL);
  }

  return(szString);
}

BOOL WinRegKeyExists(HKEY hkRootKey, LPSTR szKey)
{
  HKEY  hkResult;
  BOOL  bKeyExists = FALSE;

  if(RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult) == ERROR_SUCCESS)
  {
    bKeyExists = TRUE;
    RegCloseKey(hkResult);
  }

  return(bKeyExists);
}

