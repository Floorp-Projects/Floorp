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
#include "parser.h"
#include "extra.h"
#include "ifuncns.h"
#include "dialogs.h"

void RemoveUninstaller(LPSTR szUninstallFilename)
{
  FILE      *ofp;
  char      szBuf[MAX_BUF];
  char      szWinDir[MAX_BUF];
  char      szUninstallFile[MAX_BUF];
  char      szWininitFile[MAX_BUF];
  BOOL      bWriteRenameSection;

  if(GetWindowsDirectory(szWinDir, sizeof(szWinDir)) == 0)
    return;

  lstrcpy(szBuf, szWinDir);
  AppendBackSlash(szBuf, sizeof(szBuf));
  lstrcat(szBuf, szUninstallFilename);
  GetShortPathName(szBuf, szUninstallFile, sizeof(szUninstallFile));

  if(ulOSType & OS_NT)
  {
    MoveFileEx(szUninstallFile, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
  }
  else
  {
    lstrcpy(szWininitFile, szWinDir);
    AppendBackSlash(szWininitFile, sizeof(szUninstallFile));
    lstrcat(szWininitFile, "wininit.ini");

    if(FileExists(szWininitFile) == FALSE)
      bWriteRenameSection = TRUE;
    else
      bWriteRenameSection = FALSE;

    if((ofp = fopen(szWininitFile, "a+")) == NULL)
      return;

    if(bWriteRenameSection == TRUE)
      fprintf(ofp, "[RENAME]\n");

    fprintf(ofp, "NUL=%s\n", szUninstallFile);
    fclose(ofp);
  }
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
    lstrcpy(silTemp->szLine, szLineRead);
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

void DeleteWinRegKey(HKEY hkRootKey, LPSTR szKey)
{
  HKEY      hkResult;
  DWORD     dwErr;
  DWORD     dwTotalSubKeys;
  DWORD     dwTotalValues;

  dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_QUERY_VALUE, &hkResult);
  if(dwErr == ERROR_SUCCESS)
  {
    dwTotalSubKeys = 0;
    dwTotalValues  = 0;
    RegQueryInfoKey(hkResult, NULL, NULL, NULL, &dwTotalSubKeys, NULL, NULL, &dwTotalValues, NULL, NULL, NULL, NULL);
    RegCloseKey(hkResult);

    if((dwTotalSubKeys == 0) && (dwTotalValues == 0))
      dwErr = RegDeleteKey(hkRootKey, szKey);
  }
}

void DeleteWinRegValue(HKEY hkRootKey, LPSTR szKey, LPSTR szName)
{
  HKEY    hkResult;
  DWORD   dwErr;

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

void ParseForFile(LPSTR szString, LPSTR szKeyStr, LPSTR szShortFilename, DWORD dwShortFilenameBufSize)
{
  int     iLen;
  LPSTR   szFirstNonSpace;
  char    szBuf[MAX_BUF];

  if((szFirstNonSpace = GetFirstNonSpace(&(szString[lstrlen(szKeyStr)]))) != NULL)
  {
    iLen = lstrlen(szFirstNonSpace);
    if(szFirstNonSpace[iLen - 1] == '\n')
      szFirstNonSpace[iLen -1] = '\0';

    if(lstrcmpi(szKeyStr, KEY_WINDOWS_SHORTCUT) == 0)
    {
      lstrcpy(szBuf, szFirstNonSpace);
      lstrcat(szBuf, ".lnk");
      szFirstNonSpace = szBuf;
    }

    GetShortPathName(szFirstNonSpace, szShortFilename, dwShortFilenameBufSize);
  }
}

void ParseForWinRegInfo(LPSTR szString, LPSTR szKeyStr, LPSTR szRootKey, DWORD dwRootKeyBufSize, LPSTR szKey, DWORD dwKeyBufSize, LPSTR szName, DWORD dwNameBufSize)
{
  int     i;
  int     iLen;
  int     iBrackets;
  char    szStrCopy[MAX_BUF];
  LPSTR   szFirstNonSpace;
  LPSTR   szFirstBackSlash;
  BOOL    bFoundOpenBracket;
  BOOL    bFoundName;

  lstrcpy(szStrCopy, szString);
  if((szFirstNonSpace = GetFirstNonSpace(&(szStrCopy[lstrlen(szKeyStr)]))) != NULL)
  {
    iLen = lstrlen(szFirstNonSpace);
    if(szFirstNonSpace[iLen - 1] == '\n')
    {
      szFirstNonSpace[--iLen] = '\0';
    }

    szFirstBackSlash = strstr(szFirstNonSpace, "\\");
    szFirstBackSlash[0] = '\0';
    lstrcpy(szRootKey, szFirstNonSpace);
    szFirstNonSpace = &(szFirstBackSlash[1]);
    iLen = lstrlen(szFirstNonSpace);

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
          lstrcpy(szName, &(szFirstNonSpace[i + 1]));
          bFoundName = TRUE;
        }
      }
      else
      {
        /* locate the first non space to the left of the last '[' */
        if(!isspace(szFirstNonSpace[i]))
        {
          szFirstNonSpace[i + 1] = '\0';
          lstrcpy(szKey, szFirstNonSpace);
          break;
        }
      }
    }
  }
}

void Uninstall(sil* silFile)
{
  sil   *silTemp;
  LPSTR szSubStr;
  char  szLCLine[MAX_BUF];
  char  szKey[MAX_BUF];
  char  szRootKey[MAX_BUF];
  char  szName[MAX_BUF];
  char  szShortFilename[MAX_BUF];
  HKEY  hkRootKey;

  if(silFile != NULL)
  {
    silTemp = silFile;
    do
    {
      silTemp = silTemp->Prev;
      lstrcpy(szLCLine, silTemp->szLine);
      _strlwr(szLCLine);
      if((szSubStr = strstr(szLCLine, KEY_INSTALLING)) != NULL)
      {
        /* check for "Installing: " string and delete the file */
        ParseForFile(szSubStr, KEY_INSTALLING, szShortFilename, sizeof(szShortFilename));
        printf("%I64u: %s\n", silTemp->ullLineNumber, szShortFilename);
        FileDelete(szShortFilename);
      }
      else if((szSubStr = strstr(szLCLine, KEY_REPLACING)) != NULL)
      {
        /* check for "Replacing: " string and delete the file */
        ParseForFile(szSubStr, KEY_REPLACING, szShortFilename, sizeof(szShortFilename));
        printf("%I64u: %s\n", silTemp->ullLineNumber, szShortFilename);
        FileDelete(szShortFilename);
      }
      else if((szSubStr = strstr(szLCLine, KEY_STORE_REG_STRING)) != NULL)
      {
        /* check for "Store Registry Value String: " string and remove the key */
        ParseForWinRegInfo(szSubStr, KEY_STORE_REG_STRING, szRootKey, sizeof(szRootKey), szKey, sizeof(szKey), szName, sizeof(szName));
        printf("%I64u: Root: %s\n", silTemp->ullLineNumber, szRootKey);
        printf("%I64u: Key : %s\n", silTemp->ullLineNumber, szKey);
        printf("%I64u: Name: %s\n", silTemp->ullLineNumber, szName);
        hkRootKey = ParseRootKey(szRootKey);
        DeleteWinRegValue(hkRootKey, szKey, szName);
      }
      else if((szSubStr = strstr(szLCLine, KEY_STORE_REG_NUMBER)) != NULL)
      {
        /* check for "Store Registry Value Number: " string and remove the key */
        ParseForWinRegInfo(szSubStr, KEY_STORE_REG_NUMBER, szRootKey, sizeof(szRootKey), szKey, sizeof(szKey), szName, sizeof(szName));
        printf("%I64u: Root: %s\n", silTemp->ullLineNumber, szRootKey);
        printf("%I64u: Key : %s\n", silTemp->ullLineNumber, szKey);
        printf("%I64u: Name: %s\n", silTemp->ullLineNumber, szName);
        hkRootKey = ParseRootKey(szRootKey);
        DeleteWinRegValue(hkRootKey, szKey, szName);
      }
      else if((szSubStr = strstr(szLCLine, KEY_CREATE_REG_KEY)) != NULL)
      {
        ParseForWinRegInfo(szSubStr, KEY_CREATE_REG_KEY, szRootKey, sizeof(szRootKey), szKey, sizeof(szKey), szName, sizeof(szName));
        printf("%I64u: Root: %s\n", silTemp->ullLineNumber, szRootKey);
        printf("%I64u: Key : %s\n", silTemp->ullLineNumber, szKey);
        printf("%I64u: Name: %s\n", silTemp->ullLineNumber, szName);
        hkRootKey = ParseRootKey(szRootKey);
        DeleteWinRegKey(hkRootKey, szKey);
      }
      else if((szSubStr = strstr(szLCLine, KEY_CREATE_FOLDER)) != NULL)
      {
        ParseForFile(szSubStr, KEY_CREATE_FOLDER, szShortFilename, sizeof(szShortFilename));
        printf("%I64u: %s\n", silTemp->ullLineNumber, szShortFilename);
        DirectoryRemove(szShortFilename, FALSE);
      }
      else if((szSubStr = strstr(szLCLine, KEY_WINDOWS_SHORTCUT)) != NULL)
      {
        ParseForFile(szSubStr, KEY_WINDOWS_SHORTCUT, szShortFilename, sizeof(szShortFilename));
        printf("%I64u: %s\n", silTemp->ullLineNumber, szShortFilename);
        FileDelete(szShortFilename);
      }

      ProcessWindowsMessages();
    }while(silTemp != silFile);
  }
}

DWORD GetLogFile(LPSTR szTargetPath, LPSTR szInFilename, LPSTR szOutBuf, DWORD dwOutBufSize)
{
  int             iFilenameOnlyLen;
  char            szSearchFilename[MAX_BUF];
  char            szSearchTargetFullFilename[MAX_BUF];
  char            szFilenameOnly[MAX_BUF];
  char            szFilenameExtensionOnly[MAX_BUF];
  char            szNumber[MAX_BUF];
  long            dwNumber;
  long            dwMaxNumber;
  LPSTR           szDotPtr;
  HANDLE          hFile;
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
      lstrcpy(szSearchFilename, szInFilename);
      lstrcpy(szFilenameOnly, szInFilename);
      lstrcpy(szFilenameExtensionOnly, &szDotPtr[1]);
      *szDotPtr = '.';
    }
    else
    {
      lstrcpy(szFilenameOnly, szInFilename);
      lstrcpy(szSearchFilename, szInFilename);
    }

    /* create the wild arg filename to search for in the szTargetPath */
    lstrcat(szSearchFilename, "*.*");
    lstrcpy(szSearchTargetFullFilename, szTargetPath);
    AppendBackSlash(szSearchTargetFullFilename, sizeof(szSearchTargetFullFilename));
    lstrcat(szSearchTargetFullFilename, szSearchFilename);

    iFilenameOnlyLen = lstrlen(szFilenameOnly);
    dwNumber         = 0;
    dwMaxNumber      = -1;

    /* find the largest numbered filename in the szTargetPath */
    if((hFile = FindFirstFile(szSearchTargetFullFilename, &fdFile)) == INVALID_HANDLE_VALUE)
      bFound = FALSE;
    else
      bFound = TRUE;

    while(bFound)
    {
       ZeroMemory(szNumber, sizeof(szNumber));
      if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
      {
        lstrcpy(szNumber, &fdFile.cFileName[iFilenameOnlyLen]);
        dwNumber = atoi(szNumber);
        if(dwNumber > dwMaxNumber)
          dwMaxNumber = dwNumber;
      }

      bFound = FindNextFile(hFile, &fdFile);
    }

    FindClose(hFile);

    lstrcpy(szOutBuf, szTargetPath);
    AppendBackSlash(szOutBuf, dwOutBufSize);
    lstrcat(szOutBuf, szFilenameOnly);
    itoa(dwMaxNumber, szNumber, 10);
    lstrcat(szOutBuf, szNumber);

    if(*szFilenameExtensionOnly != '\0')
    {
      lstrcat(szOutBuf, ".");
      lstrcat(szOutBuf, szFilenameExtensionOnly);
    }
  }
  else
    return(0);

  return(FileExists(szOutBuf));
}

