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

#include "extern.h"
#include "logkeys.h"
#include "parser.h"
#include "extra.h"
#include "ifuncns.h"
#include "dialogs.h"

#define KEY_SHARED_DLLS "Software\\Microsoft\\Windows\\CurrentVersion\\SharedDlls"

BOOL DeleteOrDelayUntilReboot(LPSTR szFile)
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

      lstrcpy(szWininitFile, szWinDir);
      AppendBackSlash(szWininitFile, sizeof(szWininitFile));
      lstrcat(szWininitFile, "wininit.ini");

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

void RemoveUninstaller(LPSTR szUninstallFilename)
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

  lstrcpy(szBuf, szWinDir);
  AppendBackSlash(szBuf, sizeof(szBuf));
  lstrcat(szBuf, szUninstallFilename);
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

void DeleteWinRegKey(HKEY hkRootKey, LPSTR szKey, BOOL bAbsoluteDelete)
{
  HKEY      hkResult;
  DWORD     dwErr;
  DWORD     dwTotalSubKeys;
  DWORD     dwTotalValues;
  DWORD     dwSubKeySize;
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
              lstrcpy(szNewKey, szKey);
              AppendBackSlash(szNewKey, sizeof(szNewKey));
              lstrcat(szNewKey, szSubKey);
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

void ParseForUninstallCommand(LPSTR szString, LPSTR szKeyStr, LPSTR szFile, DWORD dwFileBufSize, LPSTR szParam, DWORD dwParamBufSize)
{
  LPSTR   szFirstNonSpace;
  LPSTR   szBeginParamStr;
  LPSTR   szEndOfFilePath;
  LPSTR   szEndQuote;
  char    *cmdStart;
  int     length;

  ZeroMemory(szFile, dwFileBufSize);
  ZeroMemory(szParam, dwParamBufSize);

  length = lstrlen(szString);
  if(szString[length - 1] == '\n')
    szString[length - 1] = '\0';

  // get to the beginning of the command given an install log string
  cmdStart = szString + lstrlen(szKeyStr);
  if((szFirstNonSpace = GetFirstNonSpace(cmdStart)) != NULL)
  {
    if(*szFirstNonSpace == '\"')
    {
      ++szFirstNonSpace;
      // found a beginning quote, look for the ending quote now
      if((szEndQuote = MozStrChar(szFirstNonSpace, '\"')) != NULL)
      {
        // found ending quote. copy file path string *not* including the quotes
        *szEndQuote = '\0';
        MozCopyStr(szFirstNonSpace, szFile, dwFileBufSize);

        // get the params substring now
        if((szBeginParamStr = GetFirstNonSpace(++szEndQuote)) != NULL)
        {
          // the params string should be the first non space char after the
          // quoted file path string to the end of the string.
          MozCopyStr(szBeginParamStr, szParam, dwParamBufSize);
        }
      }
      else
      {
        // could not find the ending quote.  assume the _entire_ string is the file path
        MozCopyStr(szFirstNonSpace, szFile, dwFileBufSize);
      }
    }
    else
    {
      // no beginning quote found.  file path is up to the first space character found
      if((szEndOfFilePath = GetFirstSpace(szFirstNonSpace)) != NULL)
      {
        // found the first space char
        *szEndOfFilePath = '\0';
        MozCopyStr(szFirstNonSpace, szFile, dwFileBufSize);

        // get the params substring now
        if((szBeginParamStr = GetFirstNonSpace(++szEndOfFilePath)) != NULL)
        {
          // the params string should be the first non space char after the
          // quoted file path string to the end of the string.
          MozCopyStr(szBeginParamStr, szParam, dwParamBufSize);
        }
      }
      else
      {
        // no space char found. assume the _entire_ string is the file path
        MozCopyStr(szFirstNonSpace, szFile, dwFileBufSize);
      }
    }
  }
}

void ParseForFile(LPSTR szString, LPSTR szKeyStr, LPSTR szFile, DWORD dwShortFilenameBufSize)
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

    lstrcpy(szFile, szFirstNonSpace);
  }
}

void ParseForCopyFile(LPSTR szString, LPSTR szKeyStr, LPSTR szFile, DWORD dwShortFilenameBufSize)
{
  int     iLen;
  LPSTR   szFirstNonSpace;
  LPSTR   szSubStr = NULL;
  char    szBuf[MAX_BUF];

  if((szSubStr = strstr(szString, " to ")) != NULL)
  {
    if((szFirstNonSpace = GetFirstNonSpace(&(szSubStr[lstrlen(" to ")]))) != NULL)
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

    lstrcpy(szFile, szFirstNonSpace);
    }
  }
}

HRESULT ParseForWinRegInfo(LPSTR szString, LPSTR szKeyStr, LPSTR szRootKey, DWORD dwRootKeyBufSize, LPSTR szKey, DWORD dwKeyBufSize, LPSTR szName, DWORD dwNameBufSize)
{
  int     i;
  int     iLen;
  int     iBrackets;
  char    szStrCopy[MAX_BUF];
  LPSTR   szFirstNonSpace;
  LPSTR   szFirstBackSlash;
  BOOL    bFoundOpenBracket;
  BOOL    bFoundName;

  *szRootKey = '\0';
  *szKey = '\0';
  *szName = '\0';

  lstrcpy(szStrCopy, szString);
  if((szFirstNonSpace = GetFirstNonSpace(&(szStrCopy[lstrlen(szKeyStr)]))) != NULL)
  {
    iLen = lstrlen(szFirstNonSpace);
    if(szFirstNonSpace[iLen - 1] == '\n')
    {
      szFirstNonSpace[--iLen] = '\0';
    }

    szFirstBackSlash = strstr(szFirstNonSpace, "\\");
    if(!szFirstBackSlash)
      return(WIZ_ERROR_PARSING_UNINST_STRS);

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
  return(WIZ_OK);
}

DWORD DecrementSharedFileCounter(char *file)
{
  HKEY     keyHandle = 0;
  LONG     result;
  DWORD    type      = REG_DWORD;
  DWORD    valbuf    = 0;
  DWORD    valbufsize;
  DWORD    rv        = 0;

  valbufsize = sizeof(DWORD);
  result     = RegOpenKeyEx(HKEY_LOCAL_MACHINE, KEY_SHARED_DLLS, 0, KEY_READ | KEY_WRITE, &keyHandle);
  if(ERROR_SUCCESS == result)
  {
    result = RegQueryValueEx(keyHandle, file, NULL, &type, (LPBYTE)&valbuf, (LPDWORD)&valbufsize);
    if((ERROR_SUCCESS == result) && (type == REG_DWORD))
      rv = valbuf = ((long)valbuf - 1) < 0 ? 0 : (valbuf - 1);

    RegSetValueEx(keyHandle, file, 0, REG_DWORD, (LPBYTE)&valbuf, valbufsize);
    RegCloseKey(keyHandle);
  }

  return(rv);
}

int GetSharedFileCount(char *file)
{
  HKEY     keyHandle = 0;
  LONG     result;
  DWORD    type      = REG_DWORD;
  DWORD    valbuf    = 0;
  DWORD    valbufsize;
  int      rv        = -999;

  valbufsize = sizeof(DWORD);
  result     = RegOpenKeyEx(HKEY_LOCAL_MACHINE, KEY_SHARED_DLLS, 0, KEY_READ, &keyHandle);
  if(ERROR_SUCCESS == result)
  {
    result = RegQueryValueEx(keyHandle, file, NULL, &type, (LPBYTE)&valbuf, (LPDWORD)&valbufsize);
    if((ERROR_SUCCESS == result) && (type == REG_DWORD))
      rv = valbuf;

    RegCloseKey(keyHandle);
  }

  return(rv);
}

BOOL UnregisterServer(char *file)
{
  FARPROC   DllUnReg;
  HINSTANCE hLib;
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

BOOL DetermineUnRegisterServer(sil *silInstallLogHead, LPSTR szFile)
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
    lstrcpy(szLCFile, szFile);
    CharLowerBuff(szLCFile, sizeof(szLCLine));

    do
    {
      silInstallLogTemp = silInstallLogTemp->Prev;
      lstrcpy(szLCLine, silInstallLogTemp->szLine);
      CharLowerBuff(szLCLine, sizeof(szLCLine));

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

DWORD Uninstall(sil* silInstallLogHead)
{
  sil   *silInstallLogTemp;
  LPSTR szSubStr;
  char  szLCLine[MAX_BUF];
  char  szKey[MAX_BUF];
  char  szRootKey[MAX_BUF];
  char  szName[MAX_BUF];
  char  szFile[MAX_BUF];
  char  szParams[MAX_BUF];
  HKEY  hkRootKey;
  int   rv;

  if(silInstallLogHead != NULL)
  {
    silInstallLogTemp = silInstallLogHead;
    do
    {
      silInstallLogTemp = silInstallLogTemp->Prev;
      lstrcpy(szLCLine, silInstallLogTemp->szLine);
      CharLowerBuff(szLCLine, sizeof(szLCLine));

      if(((szSubStr = strstr(szLCLine, KEY_WINDOWS_REGISTER_SERVER)) != NULL) &&
          (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        ParseForFile(szSubStr, KEY_WINDOWS_REGISTER_SERVER, szFile, sizeof(szFile));
        if(DetermineUnRegisterServer(silInstallLogHead, szFile) == TRUE)
          UnregisterServer(szFile);
      }
      else if((((szSubStr = strstr(szLCLine, KEY_INSTALLING_SHARED_FILE)) != NULL) ||
               ((szSubStr = strstr(szLCLine, KEY_REPLACING_SHARED_FILE)) != NULL)) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        /* check for "Installing Shared File: " string and delete the file */
        ParseForFile(szSubStr, KEY_INSTALLING_SHARED_FILE, szFile, sizeof(szFile));
        if(DecrementSharedFileCounter(szFile) == 0)
        {
          if((gdwWhatToDo != WTD_NO_TO_ALL) && (gdwWhatToDo != WTD_YES_TO_ALL) && FileExists(szFile))
          {
            MessageBeep(MB_ICONEXCLAMATION);
            gdwWhatToDo = DialogBoxParam(hInst, MAKEINTRESOURCE(DLG_WHAT_TO_DO), hDlgUninstall, DlgProcWhatToDo, (LPARAM)szFile);
          }

          if((gdwWhatToDo == WTD_YES) || (gdwWhatToDo == WTD_YES_TO_ALL) || !FileExists(szFile))
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
        rv = ParseForWinRegInfo(szSubStr, KEY_STORE_REG_STRING, szRootKey, sizeof(szRootKey), szKey, sizeof(szKey), szName, sizeof(szName));
        if(WIZ_OK == rv)
        {
          hkRootKey = ParseRootKey(szRootKey);
          DeleteWinRegValue(hkRootKey, szKey, szName);
        }
      }
      else if(((szSubStr = strstr(szLCLine, KEY_STORE_REG_NUMBER)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        /* check for "Store Registry Value Number: " string and remove the key */
        rv = ParseForWinRegInfo(szSubStr, KEY_STORE_REG_NUMBER, szRootKey, sizeof(szRootKey), szKey, sizeof(szKey), szName, sizeof(szName));
        if(WIZ_OK == rv)
        {
          hkRootKey = ParseRootKey(szRootKey);
          DeleteWinRegValue(hkRootKey, szKey, szName);
        }
      }
      else if(((szSubStr = strstr(szLCLine, KEY_CREATE_REG_KEY)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        rv = ParseForWinRegInfo(szSubStr, KEY_CREATE_REG_KEY, szRootKey, sizeof(szRootKey), szKey, sizeof(szKey), szName, sizeof(szName));
        if(WIZ_OK == rv)
        {
          hkRootKey = ParseRootKey(szRootKey);
          DeleteWinRegKey(hkRootKey, szKey, FALSE);
        }
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
      else if(((szSubStr = strstr(szLCLine, KEY_UNINSTALL_COMMAND)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        ParseForUninstallCommand(szSubStr, KEY_UNINSTALL_COMMAND, szFile, sizeof(szFile), szParams, sizeof(szParams));
        //execute szFile with szParams here!
        if(FileExists(szFile))
            WinSpawn(szFile, szParams, NULL, SW_HIDE, TRUE);
      }

      ProcessWindowsMessages();
    } while(silInstallLogTemp != silInstallLogHead);
  }

  return(0);
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

