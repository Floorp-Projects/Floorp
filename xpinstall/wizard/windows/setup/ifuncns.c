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
#include "extra.h"
#include "dialogs.h"
#include "shortcut.h"
#include "ifuncns.h"

HRESULT TimingCheck(DWORD dwTiming, LPSTR szSection, LPSTR szFile)
{
  char szBuf[MAX_BUF];

  GetPrivateProfileString(szSection, "Timing", "", szBuf, MAX_BUF, szFile);
  if(*szBuf != '\0')
  {
    switch(dwTiming)
    {
      case T_PRE_DOWNLOAD:
        if(lstrcmpi(szBuf, "pre download") == 0)
          return(TRUE);
        break;

      case T_POST_DOWNLOAD:
        if(lstrcmpi(szBuf, "post download") == 0)
          return(TRUE);
        break;

      case T_PRE_CORE:
        if(lstrcmpi(szBuf, "pre core") == 0)
          return(TRUE);
        break;

      case T_POST_CORE:
        if(lstrcmpi(szBuf, "post core") == 0)
          return(TRUE);
        break;

      case T_PRE_SMARTUPDATE:
        if(lstrcmpi(szBuf, "pre smartupdate") == 0)
          return(TRUE);
        break;

      case T_POST_SMARTUPDATE:
        if(lstrcmpi(szBuf, "post smartupdate") == 0)
          return(TRUE);
        break;

      case T_PRE_LAUNCHAPP:
        if(lstrcmpi(szBuf, "pre launchapp") == 0)
          return(TRUE);
        break;

      case T_POST_LAUNCHAPP:
        if(lstrcmpi(szBuf, "post launchapp") == 0)
          return(TRUE);
        break;

      case T_DEPEND_REBOOT:
        if(lstrcmpi(szBuf, "depend reboot") == 0)
          return(TRUE);
        break;
    }
  }
  return(FALSE);
}

void ProcessFileOps(DWORD dwTiming)
{
  ProcessUncompressFile(dwTiming);
  ProcessCreateDirectory(dwTiming);
  ProcessMoveFile(dwTiming);
  ProcessCopyFile(dwTiming);
  ProcessDeleteFile(dwTiming);
  ProcessRemoveDirectory(dwTiming);
  ProcessRunApp(dwTiming);
  ProcessWinReg(dwTiming);
  ProcessProgramFolder(dwTiming);
}

HRESULT FileUncompress(LPSTR szFrom, LPSTR szTo)
{
  char  szBuf[MAX_BUF];
  DWORD dwReturn;
  void  *vZip;

  /* Check for the existance of the from (source) file */
  if(!FileExists(szFrom))
    return(FO_ERROR_FILE_NOT_FOUND);

  /* Check for the existance of the to (destination) path */
  dwReturn = FileExists(szTo);
  if(dwReturn && !(dwReturn & FILE_ATTRIBUTE_DIRECTORY))
  {
    /* found a file with the same name as the destination path */
    return(FO_ERROR_DESTINATION_CONFLICT);
  }
  else if(!dwReturn)
  {
    lstrcpy(szBuf, szTo);
    AppendBackSlash(szBuf, sizeof(szBuf));
    CreateDirectoriesAll(szBuf);
  }

  GetCurrentDirectory(MAX_BUF, szBuf);
  if(SetCurrentDirectory(szTo) == FALSE)
    return(FO_ERROR_CHANGE_DIR);

  ZIP_OpenArchive(szFrom, &vZip);
  /* 1st parameter should be NULL or it will fail */
  /* It indicates extract the entire archive */
  ExtractDirEntries(NULL, vZip);
  ZIP_CloseArchive(&vZip);

  if(SetCurrentDirectory(szBuf) == FALSE)
    return(FO_ERROR_CHANGE_DIR);

  return(FO_SUCCESS);
}

HRESULT ProcessCoreFile()
{
  char szSource[MAX_BUF];
  char szDestination[MAX_BUF];

  if(*siCFCoreFile.szMessage != '\0')
    ShowMessage(siCFCoreFile.szMessage, TRUE);

  FileUncompress(siCFCoreFile.szSource, siCFCoreFile.szDestination);

  /* copy msvcrt.dll and msvcirt.dll to the bin of the core temp dir:
   *   (c:\temp\core.ns\bin)
   * This is incase these files do not exist on the system */
  lstrcpy(szSource, siCFCoreFile.szDestination);
  AppendBackSlash(szSource, sizeof(szSource));
  lstrcat(szSource, "ms*.dll");

  lstrcpy(szDestination, siCFCoreFile.szDestination);
  AppendBackSlash(szDestination, sizeof(szDestination));
  lstrcat(szDestination, "bin");

  FileCopy(szSource, szDestination, TRUE);

  if(*siCFCoreFile.szMessage != '\0')
    ShowMessage(siCFCoreFile.szMessage, FALSE);

  return(FO_SUCCESS);
}

HRESULT CleanupCoreFile()
{
  if(siCFCoreFile.bCleanup == TRUE)
    DirectoryRemove(siCFCoreFile.szDestination, TRUE);

  return(FO_SUCCESS);
}

HRESULT ProcessUncompressFile(DWORD dwTiming)
{
  DWORD   dwIndex;
  BOOL    bOnlyIfExists;
  char    szIndex[MAX_BUF];
  char    szBuf[MAX_BUF];
  char    szSection[MAX_BUF];
  char    szSource[MAX_BUF];
  char    szDestination[MAX_BUF];

  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szSection, "Uncompress File");
  lstrcat(szSection, szIndex);
  GetPrivateProfileString(szSection, "Source", "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
      DecryptString(szDestination, szBuf);
      GetPrivateProfileString(szSection, "Only If Exists", "", szBuf, MAX_BUF, szFileIniConfig);
      if(lstrcmpi(szBuf, "TRUE") == 0)
        bOnlyIfExists = TRUE;
      else
        bOnlyIfExists = FALSE;

      if((!bOnlyIfExists) || (bOnlyIfExists && FileExists(szDestination)))
      {
        GetPrivateProfileString(szSection, "Message",     "", szBuf, MAX_BUF, szFileIniConfig);
        ShowMessage(szBuf, TRUE);
        FileUncompress(szSource, szDestination);
        ShowMessage(szBuf, FALSE);
      }
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, "Uncompress File");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "Source", "", szBuf, MAX_BUF, szFileIniConfig);
  }
  return(FO_SUCCESS);
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

HRESULT ProcessMoveFile(DWORD dwTiming)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];

  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szSection, "Move File");
  lstrcat(szSection, szIndex);
  GetPrivateProfileString(szSection, "Source", "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
      DecryptString(szDestination, szBuf);
      FileMove(szSource, szDestination);
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, "Move File");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "Source", "", szBuf, MAX_BUF, szFileIniConfig);
  }
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

HRESULT ProcessCopyFile(DWORD dwTiming)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];
  BOOL  bFailIfExists;

  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szSection, "Copy File");
  lstrcat(szSection, szIndex);
  GetPrivateProfileString(szSection, "Source", "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
      DecryptString(szDestination, szBuf);

      GetPrivateProfileString(szSection, "Fail If Exists", "", szBuf, MAX_BUF, szFileIniConfig);
      if(lstrcmpi(szBuf, "TRUE") == 0)
        bFailIfExists = TRUE;
      else
        bFailIfExists = FALSE;

      FileCopy(szSource, szDestination, bFailIfExists);
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, "Copy File");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "Source", "", szBuf, MAX_BUF, szFileIniConfig);
  }
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

HRESULT ProcessCreateDirectory(DWORD dwTiming)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];

  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szSection, "Create Directory");
  lstrcat(szSection, szIndex);
  GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      AppendBackSlash(szDestination, sizeof(szDestination));
      CreateDirectoriesAll(szDestination);
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, "Create Directory");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
  }
  return(FO_SUCCESS);
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

HRESULT ProcessDeleteFile(DWORD dwTiming)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];

  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szSection, "Delete File");
  lstrcat(szSection, szIndex);
  GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      FileDelete(szDestination);
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, "Delete File");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
  }
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

HRESULT ProcessRemoveDirectory(DWORD dwTiming)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];
  BOOL  bRemoveSubdirs;

  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szSection, "Remove Directory");
  lstrcat(szSection, szIndex);
  GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      GetPrivateProfileString(szSection, "Remove subdirs", "", szBuf, MAX_BUF, szFileIniConfig);
      bRemoveSubdirs = FALSE;
      if(lstrcmpi(szBuf, "TRUE") == 0)
        bRemoveSubdirs = TRUE;

      DirectoryRemove(szDestination, bRemoveSubdirs);
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, "Remove Directory");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessRunApp(DWORD dwTiming)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szTarget[MAX_BUF];
  char  szParameters[MAX_BUF];
  char  szWorkingDir[MAX_BUF];
  BOOL  bWait;

  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szSection, "RunApp");
  lstrcat(szSection, szIndex);
  GetPrivateProfileString(szSection, "Target", "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szTarget, szBuf);
      GetPrivateProfileString(szSection, "Parameters", "", szBuf, MAX_BUF, szFileIniConfig);
      DecryptString(szParameters, szBuf);
      GetPrivateProfileString(szSection, "WorkingDir", "", szBuf, MAX_BUF, szFileIniConfig);
      DecryptString(szWorkingDir, szBuf);
      GetPrivateProfileString(szSection, "Wait", "", szBuf, MAX_BUF, szFileIniConfig);

      if(lstrcmpi(szBuf, "FALSE") == 0)
        bWait = FALSE;
      else
        bWait = TRUE;

      if((dwTiming == T_DEPEND_REBOOT) && (NeedReboot() == TRUE))
      {
        lstrcat(szTarget, " ");
        lstrcat(szTarget, szParameters);
        SetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", TRUE, "Netscape", TRUE, REG_SZ, szTarget, lstrlen(szTarget));
      }
      else
        WinSpawn(szTarget, szParameters, szWorkingDir, SW_SHOWNORMAL, bWait);
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, "RunApp");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "Target", "", szBuf, MAX_BUF, szFileIniConfig);
  }
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

DWORD ParseRegType(LPSTR szType)
{
  DWORD dwType;

  if(lstrcmpi(szType, "REG_SZ") == 0)
    /* Unicode NULL terminated string */
    dwType = REG_SZ;
  else if(lstrcmpi(szType, "REG_EXPAND_SZ") == 0)
    /* Unicode NULL terminated string
     * (with environment variable references) */
    dwType = REG_EXPAND_SZ;
  else if(lstrcmpi(szType, "REG_BINARY") == 0)
    /* Free form binary */
    dwType = REG_BINARY;
  else if(lstrcmpi(szType, "REG_DWORD") == 0)
    /* 32bit number */
    dwType = REG_DWORD;
  else if(lstrcmpi(szType, "REG_DWORD_LITTLE_ENDIAN") == 0)
    /* 32bit number
     * (same as REG_DWORD) */
    dwType = REG_DWORD_LITTLE_ENDIAN;
  else if(lstrcmpi(szType, "REG_DWORD_BIG_ENDIAN") == 0)
    /* 32bit number */
    dwType = REG_DWORD_BIG_ENDIAN;
  else if(lstrcmpi(szType, "REG_LINK") == 0)
    /* Symbolic link (unicode) */
    dwType = REG_LINK;
  else if(lstrcmpi(szType, "REG_MULTI_SZ") == 0)
    /* Multiple Unicode strings */
    dwType = REG_MULTI_SZ;
  else /* Default is REG_NONE */
    /* no value type */
    dwType = REG_NONE;

  return(dwType);
}

BOOL WinRegKeyExists(HKEY hkRootKey, LPSTR szKey)
{
  HKEY  hkResult;
  DWORD dwErr;
  BOOL  bKeyExists = FALSE;

  if((dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult)) == ERROR_SUCCESS)
  {
    bKeyExists = TRUE;
    RegCloseKey(hkResult);
  }

  return(bKeyExists);
}

BOOL WinRegNameExists(HKEY hkRootKey, LPSTR szKey, LPSTR szName)
{
  HKEY  hkResult;
  DWORD dwErr;
  DWORD dwSize;
  char  szBuf[MAX_BUF];
  BOOL  bNameExists = FALSE;

  ZeroMemory(szBuf, sizeof(szBuf));
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

void GetWinReg(HKEY hkRootKey, LPSTR szKey, LPSTR szName, LPSTR szReturnValue, DWORD dwReturnValueSize)
{
  HKEY  hkResult;
  DWORD dwErr;
  DWORD dwSize;
  char  szBuf[MAX_BUF];

  ZeroMemory(szBuf, sizeof(szBuf));
  ZeroMemory(szReturnValue, dwReturnValueSize);

  if((dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult)) == ERROR_SUCCESS)
  {
    dwSize = sizeof(szBuf);
    dwErr  = RegQueryValueEx(hkResult, szName, 0, NULL, szBuf, &dwSize);

    if((*szBuf != '\0') && (dwErr == ERROR_SUCCESS))
      ExpandEnvironmentStrings(szBuf, szReturnValue, dwReturnValueSize);
    else
      *szReturnValue = '\0';

    RegCloseKey(hkResult);
  }
}

void SetWinReg(HKEY hkRootKey, LPSTR szKey, BOOL bOverwriteKey, LPSTR szName, BOOL bOverwriteName, DWORD dwType, LPSTR szData, DWORD dwSize)
{
  HKEY    hkResult;
  DWORD   dwErr;
  DWORD   dwDisp;
  BOOL    bKeyExists;
  BOOL    bNameExists;

  bKeyExists  = WinRegKeyExists(hkRootKey, szKey);
  bNameExists = WinRegNameExists(hkRootKey, szKey, szName);
  dwErr       = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_WRITE, &hkResult);

  if(dwErr != ERROR_SUCCESS)
    dwErr = RegCreateKeyEx(hkRootKey, szKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkResult, &dwDisp);

  if(dwErr == ERROR_SUCCESS)
  {
    if((bNameExists == FALSE) ||
      ((bNameExists == TRUE) && (bOverwriteName == TRUE)))
      dwErr = RegSetValueEx(hkResult, szName, 0, dwType, szData, dwSize);

    RegCloseKey(hkResult);
  }
}

HRESULT ProcessWinReg(DWORD dwTiming)
{
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szKey[MAX_BUF];
  char  szName[MAX_BUF];
  char  szValue[MAX_BUF];
  char  szDecrypt[MAX_BUF];
  char  szOverwriteKey[MAX_BUF];
  char  szOverwriteName[MAX_BUF];
  char  szSection[MAX_BUF];
  HKEY  hRootKey;
  BOOL  bOverwriteKey;
  BOOL  bOverwriteName;
  DWORD dwIndex;
  DWORD dwType;

  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szSection, "Windows Registry");
  lstrcat(szSection, szIndex);
  GetPrivateProfileString(szSection, "Root Key", "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      hRootKey = ParseRootKey(szBuf);

      GetPrivateProfileString(szSection, "Key",                 "", szBuf,           MAX_BUF, szFileIniConfig);
      GetPrivateProfileString(szSection, "Decrypt Key",         "", szDecrypt,       MAX_BUF, szFileIniConfig);
      GetPrivateProfileString(szSection, "Overwrite Key",       "", szOverwriteKey,  MAX_BUF, szFileIniConfig);

      if(lstrcmpi(szDecrypt, "TRUE") == 0)
        DecryptString(szKey, szBuf);
      else
        lstrcpy(szKey, szBuf);

      if(lstrcmpi(szOverwriteKey, "FALSE") == 0)
        bOverwriteKey = FALSE;
      else
        bOverwriteKey = TRUE;

      GetPrivateProfileString(szSection, "Name",                "", szBuf,           MAX_BUF, szFileIniConfig);
      GetPrivateProfileString(szSection, "Decrypt Name",        "", szDecrypt,       MAX_BUF, szFileIniConfig);
      GetPrivateProfileString(szSection, "Overwrite Name",      "", szOverwriteName, MAX_BUF, szFileIniConfig);

      if(lstrcmpi(szDecrypt, "TRUE") == 0)
        DecryptString(szName, szBuf);
      else
        lstrcpy(szName, szBuf);

      if(lstrcmpi(szOverwriteName, "FALSE") == 0)
        bOverwriteName = FALSE;
      else
        bOverwriteName = TRUE;

      GetPrivateProfileString(szSection, "Name Value",          "", szBuf,           MAX_BUF, szFileIniConfig);
      GetPrivateProfileString(szSection, "Decrypt Name Value",  "", szDecrypt,       MAX_BUF, szFileIniConfig);
      if(lstrcmpi(szDecrypt, "TRUE") == 0)
        DecryptString(szValue, szBuf);
      else
        lstrcpy(szValue, szBuf);

      GetPrivateProfileString(szSection, "Type",                "", szBuf,           MAX_BUF, szFileIniConfig);
      dwType = ParseRegType(szBuf);

      /* create/set windows registry key here! */
      SetWinReg(hRootKey, szKey, bOverwriteKey, szName, bOverwriteName, dwType, szValue, lstrlen(szValue));
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, "Windows Registry");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "Root Key", "", szBuf, MAX_BUF, szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessProgramFolder(DWORD dwTiming)
{
  DWORD dwIndex0;
  DWORD dwIndex1;
  DWORD dwIconId;
  char  szIndex0[MAX_BUF];
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

  dwIndex0 = 0;
  itoa(dwIndex0, szIndex0, 10);
  lstrcpy(szSection0, "Program Folder");
  lstrcat(szSection0, szIndex0);
  GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection0, szFileIniConfig))
    {
      DecryptString(szProgramFolder, szBuf);

      dwIndex1 = 0;
      itoa(dwIndex1, szIndex1, 10);
      lstrcpy(szSection1, szSection0);
      lstrcat(szSection1, "-Shortcut");
      lstrcat(szSection1, szIndex1);
      GetPrivateProfileString(szSection1, "File", "", szBuf, MAX_BUF, szFileIniConfig);
      while(*szBuf != '\0')
      {
        DecryptString(szFile, szBuf);
        GetPrivateProfileString(szSection1, "Arguments",    "", szBuf, MAX_BUF, szFileIniConfig);
        DecryptString(szArguments, szBuf);
        GetPrivateProfileString(szSection1, "Working Dir",  "", szBuf, MAX_BUF, szFileIniConfig);
        DecryptString(szWorkingDir, szBuf);
        GetPrivateProfileString(szSection1, "Description",  "", szBuf, MAX_BUF, szFileIniConfig);
        DecryptString(szDescription, szBuf);
        GetPrivateProfileString(szSection1, "Icon Path",    "", szBuf, MAX_BUF, szFileIniConfig);
        DecryptString(szIconPath, szBuf);
        GetPrivateProfileString(szSection1, "Icon Id",      "", szBuf, MAX_BUF, szFileIniConfig);
        if(*szBuf != '\0')
          dwIconId = atol(szBuf);
        else
          dwIconId = 0;

        CreateALink(szFile, szProgramFolder, szDescription, szWorkingDir, szArguments, szIconPath, dwIconId);

        ++dwIndex1;
        itoa(dwIndex1, szIndex1, 10);
        lstrcpy(szSection1, szSection0);
        lstrcat(szSection1, "-Shortcut");
        lstrcat(szSection1, szIndex1);
        GetPrivateProfileString(szSection1, "File", "", szBuf, MAX_BUF, szFileIniConfig);
      }
    }

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    lstrcpy(szSection0, "Program Folder");
    lstrcat(szSection0, szIndex0);
    GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, MAX_BUF, szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessProgramFolderShowCmd()
{
  DWORD dwIndex0;
  int   iShowFolder;
  char  szIndex0[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szSection0[MAX_BUF];
  char  szProgramFolder[MAX_BUF];

  dwIndex0 = 0;
  itoa(dwIndex0, szIndex0, 10);
  lstrcpy(szSection0, "Program Folder");
  lstrcat(szSection0, szIndex0);
  GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    DecryptString(szProgramFolder, szBuf);
    GetPrivateProfileString(szSection0, "Show Folder", "", szBuf, MAX_BUF, szFileIniConfig);

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
    itoa(dwIndex0, szIndex0, 10);
    lstrcpy(szSection0, "Program Folder");
    lstrcat(szSection0, szIndex0);
    GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, MAX_BUF, szFileIniConfig);
  }
  return(FO_SUCCESS);
}
