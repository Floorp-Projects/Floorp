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

#include "extern.h"
#include "logkeys.h"
#include "parser.h"
#include "extra.h"
#include "ifuncns.h"
#include "dialogs.h"

#define KEY_SHARED_DLLS "Software\\Microsoft\\Windows\\CurrentVersion\\SharedDlls"


BOOL DeleteOrDelayUntilReboot(PSZ szFile)
{
  FILE      *ofp;
  char      szWinDir[MAX_BUF];
  char      szWininitFile[MAX_BUF];
  BOOL      bDelayDelete = FALSE;
  BOOL      bWriteRenameSection;

  FileDelete(szFile);
  if(FileExists(szFile))
  {
    bDelayDelete = TRUE;
    if(ulOSType & OS_NT)
      MoveFileEx(szFile, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    else
    {
      if(GetWindowsDirectory(szWinDir, sizeof(szWinDir)) == 0)
        return(FALSE);

      strcpy(szWininitFile, szWinDir);
      AppendBackSlash(szWininitFile, sizeof(szWininitFile));
      strcat(szWininitFile, "wininit.ini");

      if(FileExists(szWininitFile) == FALSE)
        bWriteRenameSection = TRUE;
      else
        bWriteRenameSection = FALSE;

      if((ofp = fopen(szWininitFile, "a+")) == NULL)
        return(FALSE);

      if(bWriteRenameSection == TRUE)
        fprintf(ofp, "[RENAME]\n");

      fprintf(ofp, "NUL=%s\n", szFile);
      fclose(ofp);
    }
  }
  else
    bDelayDelete = FALSE;

  return(bDelayDelete);
}

void RemoveUninstaller(PSZ szUninstallFilename)
{
  char      szBuf[MAX_BUF];
  char      szWinDir[MAX_BUF];
  char      szUninstallFile[MAX_BUF];

  if(SearchForUninstallKeys(szUninstallFilename))
    /* Found the uninstall file name in the windows registry uninstall
     * key sections.  We should not try to delete ourselves. */
    return;

  if(GetWindowsDirectory(szWinDir, sizeof(szWinDir)) == 0)
    return;

  strcpy(szBuf, szWinDir);
  AppendBackSlash(szBuf, sizeof(szBuf));
  strcat(szBuf, szUninstallFilename);
  GetShortPathName(szBuf, szUninstallFile, sizeof(szUninstallFile));
  DeleteOrDelayUntilReboot(szUninstallFile);
}

sil *InitSilNodes(char *szInFile)
{
  FILE      *ifp;
  char      szLineRead[MAX_BUF];
  sil       *silTemp;
  sil       *silHead;
  ULONGLONG ullLineCount;

  if(FileExists(szInFile) == FALSE)
    return(NULL);

  ullLineCount = 0;
  silHead      = NULL;
  if((ifp = fopen(szInFile, "r")) == NULL)
    exit(1);

  while(fgets(szLineRead, MAX_BUF, ifp) != NULL)
  {
    silTemp = CreateSilNode();

    silTemp->ullLineNumber = ++ullLineCount;
    strcpy(silTemp->szLine, szLineRead);
    if(silHead == NULL)
    {
      silHead = silTemp;
    }
    else
    {
      SilNodeInsert(silHead, silTemp);
    }

    ProcessWindowsMessages();
  }
  fclose(ifp);
  return(silHead);
}

void DeInitSilNodes(sil **silHead)
{
  sil   *silTemp;
  
  if(*silHead == NULL)
  {
    return;
  }
  else if(((*silHead)->Prev == NULL) || ((*silHead)->Prev == *silHead))
  {
    SilNodeDelete(*silHead);
    return;
  }
  else
  {
    silTemp = (*silHead)->Prev;
  }

  while(silTemp != *silHead)
  {
    SilNodeDelete(silTemp);
    silTemp = (*silHead)->Prev;

    ProcessWindowsMessages();
  }
  SilNodeDelete(silTemp);
}

void DeleteWinRegKey(HINI hkRootKey, PSZ szKey, BOOL bAbsoluteDelete)
{
  HINI      hkResult;
  ULONG     dwErr;
  ULONG     dwTotalSubKeys;
  ULONG     dwTotalValues;
  ULONG     dwSubKeySize;
  FILETIME  ftLastWriteFileTime;
  char      szSubKey[MAX_BUF];
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

void ParseForFile(PSZ szString, PSZ szKeyStr, PSZ szFile, ULONG dwShortFilenameBufSize)
{
  int     iLen;
  PSZ   szFirstNonSpace;
  char    szBuf[MAX_BUF];

  if((szFirstNonSpace = GetFirstNonSpace(&(szString[strlen(szKeyStr)]))) != NULL)
  {
    iLen = strlen(szFirstNonSpace);
    if(szFirstNonSpace[iLen - 1] == '\n')
      szFirstNonSpace[iLen -1] = '\0';

    if(strcmpi(szKeyStr, KEY_WINDOWS_SHORTCUT) == 0)
    {
      strcpy(szBuf, szFirstNonSpace);
      strcat(szBuf, ".lnk");
      szFirstNonSpace = szBuf;
    }

    strcpy(szFile, szFirstNonSpace);
  }
}

void ParseForCopyFile(PSZ szString, PSZ szKeyStr, PSZ szFile, ULONG dwShortFilenameBufSize)
{
  int     iLen;
  PSZ   szFirstNonSpace;
  PSZ   szSubStr = NULL;
  char    szBuf[MAX_BUF];

  if((szSubStr = strstr(szString, " to ")) != NULL)
  {
    if((szFirstNonSpace = GetFirstNonSpace(&(szSubStr[strlen(" to ")]))) != NULL)
    {
      iLen = strlen(szFirstNonSpace);
      if(szFirstNonSpace[iLen - 1] == '\n')
        szFirstNonSpace[iLen -1] = '\0';

      if(strcmpi(szKeyStr, KEY_WINDOWS_SHORTCUT) == 0)
      {
        strcpy(szBuf, szFirstNonSpace);
        strcat(szBuf, ".lnk");
        szFirstNonSpace = szBuf;
      }

    strcpy(szFile, szFirstNonSpace);
    }
  }
}

void ParseForWinRegInfo(PSZ szString, PSZ szKeyStr, PSZ szRootKey, ULONG dwRootKeyBufSize, PSZ szKey, ULONG dwKeyBufSize, PSZ szName, ULONG dwNameBufSize)
{
  int     i;
  int     iLen;
  int     iBrackets;
  char    szStrCopy[MAX_BUF];
  PSZ   szFirstNonSpace;
  PSZ   szFirstBackSlash;
  BOOL    bFoundOpenBracket;
  BOOL    bFoundName;

  strcpy(szStrCopy, szString);
  if((szFirstNonSpace = GetFirstNonSpace(&(szStrCopy[strlen(szKeyStr)]))) != NULL)
  {
    iLen = strlen(szFirstNonSpace);
    if(szFirstNonSpace[iLen - 1] == '\n')
    {
      szFirstNonSpace[--iLen] = '\0';
    }

    szFirstBackSlash = strstr(szFirstNonSpace, "\\");
    szFirstBackSlash[0] = '\0';
    strcpy(szRootKey, szFirstNonSpace);
    szFirstNonSpace = &(szFirstBackSlash[1]);
    iLen = strlen(szFirstNonSpace);

    iBrackets         = 0;
    bFoundName        = FALSE;
    bFoundOpenBracket = FALSE;
    for(i = iLen - 1; i >= 0; i--)
    {
      if(bFoundName == FALSE)
      {
        /* Find the Name created in the Windows registry key.
         * Since the Name can contain '[' and ']', we have to
         * parse for the brackets that denote the Name string in
         * szFirstNonSpace.  It parses from right to left.
         */
        if(szFirstNonSpace[i] == ']')
        {
          if(iBrackets == 0)
            szFirstNonSpace[i] = '\0';

          ++iBrackets;
        }
        else if(szFirstNonSpace[i] == '[')
        {
          bFoundOpenBracket = TRUE;
          --iBrackets;
        }

        if((bFoundOpenBracket) && (iBrackets == 0))
        {
          strcpy(szName, &(szFirstNonSpace[i + 1]));
          bFoundName = TRUE;
        }
      }
      else
      {
        /* locate the first non space to the left of the last '[' */
        if(!isspace(szFirstNonSpace[i]))
        {
          szFirstNonSpace[i + 1] = '\0';
          strcpy(szKey, szFirstNonSpace);
          break;
        }
      }
    }
  }
}

ULONG DecrementSharedFileCounter(char *file)
{
  HINI     keyHandle = 0;
  LONG     result;
  ULONG    type      = REG_ULONG;
  ULONG    valbuf    = 0;
  ULONG    valbufsize;
  ULONG    rv        = 0;

  valbufsize = sizeof(ULONG);
  result     = RegOpenKeyEx(HKEY_LOCAL_MACHINE, KEY_SHARED_DLLS, 0, KEY_READ | KEY_WRITE, &keyHandle);
  if(ERROR_SUCCESS == result)
  {
    result = RegQueryValueEx(keyHandle, file, NULL, &type, (LPBYTE)&valbuf, (LPULONG)&valbufsize);
    if((ERROR_SUCCESS == result) && (type == REG_ULONG))
    {
      rv = --valbuf;
    }

    RegSetValueEx(keyHandle, file, 0, REG_ULONG, (LPBYTE)&valbuf, valbufsize);
    RegCloseKey(keyHandle);
  }

  return(rv);
}

int GetSharedFileCount(char *file)
{
  HINI     keyHandle = 0;
  LONG     result;
  ULONG    type      = REG_ULONG;
  ULONG    valbuf    = 0;
  ULONG    valbufsize;
  int      rv        = -999;

  valbufsize = sizeof(ULONG);
  result     = RegOpenKeyEx(HKEY_LOCAL_MACHINE, KEY_SHARED_DLLS, 0, KEY_READ, &keyHandle);
  if(ERROR_SUCCESS == result)
  {
    result = RegQueryValueEx(keyHandle, file, NULL, &type, (LPBYTE)&valbuf, (LPULONG)&valbufsize);
    if((ERROR_SUCCESS == result) && (type == REG_ULONG))
      rv = valbuf;

    RegCloseKey(keyHandle);
  }

  return(rv);
}

BOOL UnregisterServer(char *file)
{
  PFNWP   DllUnReg;
  HOBJECT hLib;
  BOOL      bFailed = FALSE;

  if(file != NULL)
  {
    if((hLib = LoadLibraryEx(file, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) != NULL)
    {
      if((DllUnReg = GetProcAddress(hLib, "DllUnregisterServer")) != NULL)
        DllUnReg();
      else
        bFailed = TRUE;

      FreeLibrary(hLib);
    }
    else
      bFailed = TRUE;
  }
  else
    bFailed = TRUE;

  return(bFailed);
}

BOOL DetermineUnRegisterServer(sil *silInstallLogHead, PSZ szFile)
{
  sil   *silInstallLogTemp;
  int   iSharedFileCount;
  char  szLCLine[MAX_BUF];
  char  szLCFile[MAX_BUF];
  BOOL  bRv;

  bRv = FALSE;
  if(silInstallLogHead != NULL)
  {
    silInstallLogTemp = silInstallLogHead;
    iSharedFileCount  = GetSharedFileCount(szFile);
    strcpy(szLCFile, szFile);
    _strlwr(szLCFile);

    do
    {
      silInstallLogTemp = silInstallLogTemp->Prev;
      strcpy(szLCLine, silInstallLogTemp->szLine);
      _strlwr(szLCLine);

      if((strstr(szLCLine, szLCFile) != NULL) &&
         (strstr(szLCLine, KEY_INSTALLING_SHARED_FILE) != NULL) &&
         (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        --iSharedFileCount;
      }
      else if((strstr(szLCLine, szLCFile) != NULL) &&
              (strstr(szLCLine, KEY_INSTALLING) != NULL) &&
              (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        bRv = TRUE;
        break;
      }
      else if((strstr(szLCLine, szLCFile) != NULL) &&
              (strstr(szLCLine, KEY_REPLACING_SHARED_FILE) != NULL) &&
              (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        --iSharedFileCount;
      }
      else if((strstr(szLCLine, szLCFile) != NULL) &&
              (strstr(szLCLine, KEY_REPLACING) != NULL) &&
              (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        bRv = TRUE;
        break;
      }
      else if((strstr(szLCLine, szLCFile) != NULL) &&
              (strstr(szLCLine, KEY_COPY_FILE) != NULL) &&
              (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        bRv = TRUE;
        break;
      }

      ProcessWindowsMessages();
    } while(silInstallLogTemp != silInstallLogHead);
  }

  if((iSharedFileCount <= 0) && (iSharedFileCount != -999))
    bRv = TRUE;

  return(bRv);
}

ULONG Uninstall(sil* silInstallLogHead)
{
  sil   *silInstallLogTemp;
  PSZ szSubStr;
  char  szLCLine[MAX_BUF];
  char  szKey[MAX_BUF];
  char  szRootKey[MAX_BUF];
  char  szName[MAX_BUF];
  char  szFile[MAX_BUF];
  HINI  hkRootKey;

  if(silInstallLogHead != NULL)
  {
    silInstallLogTemp = silInstallLogHead;
    do
    {
      silInstallLogTemp = silInstallLogTemp->Prev;
      strcpy(szLCLine, silInstallLogTemp->szLine);
      _strlwr(szLCLine);

      if(((szSubStr = strstr(szLCLine, KEY_WINDOWS_REGISTER_SERVER)) != NULL) &&
          (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        ParseForFile(szSubStr, KEY_WINDOWS_REGISTER_SERVER, szFile, sizeof(szFile));
        if(DetermineUnRegisterServer(silInstallLogHead, szFile) == TRUE)
          UnregisterServer(szFile);
      }
      else if(((szSubStr = strstr(szLCLine, KEY_INSTALLING_SHARED_FILE)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        /* check for "Installing Shared File: " string and delete the file */
        ParseForFile(szSubStr, KEY_INSTALLING_SHARED_FILE, szFile, sizeof(szFile));
        if(DecrementSharedFileCounter(szFile) == 0)
        {
          if((gdwWhatToDo != WTD_NO_TO_ALL) && (gdwWhatToDo != WTD_YES_TO_ALL))
          {
            MessageBeep(MB_ICONEXCLAMATION);
            gdwWhatToDo = DialogBoxParam(hInst, MAKEINTRESOURCE(DLG_WHAT_TO_DO), hDlgUninstall, DlgProcWhatToDo, (LPARAM)szFile);
          }

          if((gdwWhatToDo == WTD_YES) || (gdwWhatToDo == WTD_YES_TO_ALL))
          {
            DeleteWinRegValue(HKEY_LOCAL_MACHINE, KEY_SHARED_DLLS, szFile);
            DeleteOrDelayUntilReboot(szFile);
          }
          else if(gdwWhatToDo == WTD_CANCEL)
            return(WTD_CANCEL);
        }
      }
      else if(((szSubStr = strstr(szLCLine, KEY_INSTALLING)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        /* check for "Installing: " string and delete the file */
        ParseForFile(szSubStr, KEY_INSTALLING, szFile, sizeof(szFile));
        DeleteOrDelayUntilReboot(szFile);
      }
      else if(((szSubStr = strstr(szLCLine, KEY_REPLACING_SHARED_FILE)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        /* check for "Replacing Shared File: " string and delete the file */
        ParseForFile(szSubStr, KEY_REPLACING_SHARED_FILE, szFile, sizeof(szFile));
        if(DecrementSharedFileCounter(szFile) == 0)
        {
          if((gdwWhatToDo != WTD_NO_TO_ALL) && (gdwWhatToDo != WTD_YES_TO_ALL))
            gdwWhatToDo = DialogBoxParam(hInst, MAKEINTRESOURCE(DLG_WHAT_TO_DO), hDlgUninstall, DlgProcWhatToDo, (LPARAM)szFile);

          if((gdwWhatToDo == WTD_YES) || (gdwWhatToDo == WTD_YES_TO_ALL))
          {
            DeleteWinRegValue(HKEY_LOCAL_MACHINE, KEY_SHARED_DLLS, szFile);
            DeleteOrDelayUntilReboot(szFile);
          }
          else if(gdwWhatToDo == WTD_CANCEL)
            return(WTD_CANCEL);
        }
      }
      else if(((szSubStr = strstr(szLCLine, KEY_REPLACING)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        /* check for "Replacing: " string and delete the file */
        ParseForFile(szSubStr, KEY_REPLACING, szFile, sizeof(szFile));
        DeleteOrDelayUntilReboot(szFile);
      }
      else if(((szSubStr = strstr(szLCLine, KEY_STORE_REG_STRING)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        /* check for "Store Registry Value String: " string and remove the key */
        ParseForWinRegInfo(szSubStr, KEY_STORE_REG_STRING, szRootKey, sizeof(szRootKey), szKey, sizeof(szKey), szName, sizeof(szName));
        hkRootKey = ParseRootKey(szRootKey);
        DeleteWinRegValue(hkRootKey, szKey, szName);
      }
      else if(((szSubStr = strstr(szLCLine, KEY_STORE_REG_NUMBER)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        /* check for "Store Registry Value Number: " string and remove the key */
        ParseForWinRegInfo(szSubStr, KEY_STORE_REG_NUMBER, szRootKey, sizeof(szRootKey), szKey, sizeof(szKey), szName, sizeof(szName));
        hkRootKey = ParseRootKey(szRootKey);
        DeleteWinRegValue(hkRootKey, szKey, szName);
      }
      else if(((szSubStr = strstr(szLCLine, KEY_CREATE_REG_KEY)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        ParseForWinRegInfo(szSubStr, KEY_CREATE_REG_KEY, szRootKey, sizeof(szRootKey), szKey, sizeof(szKey), szName, sizeof(szName));
        hkRootKey = ParseRootKey(szRootKey);
        DeleteWinRegKey(hkRootKey, szKey, FALSE);
      }
      else if(((szSubStr = strstr(szLCLine, KEY_CREATE_FOLDER)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        ParseForFile(szSubStr, KEY_CREATE_FOLDER, szFile, sizeof(szFile));
        DirectoryRemove(szFile, FALSE);
      }
      else if(((szSubStr = strstr(szLCLine, KEY_WINDOWS_SHORTCUT)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        ParseForFile(szSubStr, KEY_WINDOWS_SHORTCUT, szFile, sizeof(szFile));
        DeleteOrDelayUntilReboot(szFile);
      }
      else if(((szSubStr = strstr(szLCLine, KEY_COPY_FILE)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        /* check for "copy file: " string and delete the file */
        ParseForCopyFile(szSubStr, KEY_COPY_FILE, szFile, sizeof(szFile));
        DeleteOrDelayUntilReboot(szFile);
      }

      ProcessWindowsMessages();
    } while(silInstallLogTemp != silInstallLogHead);
  }

  return(0);
}

ULONG GetLogFile(PSZ szTargetPath, PSZ szInFilename, PSZ szOutBuf, ULONG dwOutBufSize)
{
  int             iFilenameOnlyLen;
  char            szSearchFilename[MAX_BUF];
  char            szSearchTargetFullFilename[MAX_BUF];
  char            szFilenameOnly[MAX_BUF];
  char            szFilenameExtensionOnly[MAX_BUF];
  char            szNumber[MAX_BUF];
  long            dwNumber;
  long            dwMaxNumber;
  PSZ           szDotPtr;
  LHANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  BOOL            bFound;

  if(FileExists(szTargetPath))
  {
    /* zero out the memory */
    ZeroMemory(szOutBuf,                dwOutBufSize);
    ZeroMemory(szSearchFilename,        sizeof(szSearchFilename));
    ZeroMemory(szFilenameOnly,          sizeof(szFilenameOnly));
    ZeroMemory(szFilenameExtensionOnly, sizeof(szFilenameExtensionOnly));

    /* parse for the filename w/o extention and also only the extension */
    if((szDotPtr = strstr(szInFilename, ".")) != NULL)
    {
      *szDotPtr = '\0';
      strcpy(szSearchFilename, szInFilename);
      strcpy(szFilenameOnly, szInFilename);
      strcpy(szFilenameExtensionOnly, &szDotPtr[1]);
      *szDotPtr = '.';
    }
    else
    {
      strcpy(szFilenameOnly, szInFilename);
      strcpy(szSearchFilename, szInFilename);
    }

    /* create the wild arg filename to search for in the szTargetPath */
    strcat(szSearchFilename, "*.*");
    strcpy(szSearchTargetFullFilename, szTargetPath);
    AppendBackSlash(szSearchTargetFullFilename, sizeof(szSearchTargetFullFilename));
    strcat(szSearchTargetFullFilename, szSearchFilename);

    iFilenameOnlyLen = strlen(szFilenameOnly);
    dwNumber         = 0;
    dwMaxNumber      = -1;

    /* find the largest numbered filename in the szTargetPath */
    if((hFile = FindFirstFile(szSearchTargetFullFilename, &fdFile)) == ERROR_INVALID_HANDLE)
      bFound = FALSE;
    else
      bFound = TRUE;

    while(bFound)
    {
       ZeroMemory(szNumber, sizeof(szNumber));
      if((strcmpi(fdFile.cFileName, ".") != 0) && (strcmpi(fdFile.cFileName, "..") != 0))
      {
        strcpy(szNumber, &fdFile.cFileName[iFilenameOnlyLen]);
        dwNumber = atoi(szNumber);
        if(dwNumber > dwMaxNumber)
          dwMaxNumber = dwNumber;
      }

      bFound = FindNextFile(hFile, &fdFile);
    }

    FindClose(hFile);

    strcpy(szOutBuf, szTargetPath);
    AppendBackSlash(szOutBuf, dwOutBufSize);
    strcat(szOutBuf, szFilenameOnly);
    itoa(dwMaxNumber, szNumber, 10);
    strcat(szOutBuf, szNumber);

    if(*szFilenameExtensionOnly != '\0')
    {
      strcat(szOutBuf, ".");
      strcat(szOutBuf, szFilenameExtensionOnly);
    }
  }
  else
    return(0);

  return(FileExists(szOutBuf));
}

