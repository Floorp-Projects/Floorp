/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
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
  if(*siCFCoreFile.szMessage != '\0')
    ShowMessage(siCFCoreFile.szMessage, TRUE);

  FileUncompress(siCFCoreFile.szSource, siCFCoreFile.szDestination);

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
      DecriptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
      DecriptString(szDestination, szBuf);

      GetPrivateProfileString(szSection, "Message",     "", szBuf, MAX_BUF, szFileIniConfig);
      ShowMessage(szBuf, TRUE);
      FileUncompress(szSource, szDestination);
      ShowMessage(szBuf, FALSE);
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
      DecriptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
      DecriptString(szDestination, szBuf);
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
      DecriptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
      DecriptString(szDestination, szBuf);

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
      DecriptString(szDestination, szBuf);
      CreateDirectoriesAll(szBuf);
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
      DecriptString(szDestination, szBuf);
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
      DecriptString(szDestination, szBuf);
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
      DecriptString(szTarget, szBuf);
      GetPrivateProfileString(szSection, "Parameters", "", szBuf, MAX_BUF, szFileIniConfig);
      DecriptString(szParameters, szBuf);
      GetPrivateProfileString(szSection, "WorkingDir", "", szBuf, MAX_BUF, szFileIniConfig);
      DecriptString(szWorkingDir, szBuf);
      GetPrivateProfileString(szSection, "Wait", "", szBuf, MAX_BUF, szFileIniConfig);
      if(lstrcmpi(szBuf, "FALSE") == 0)
      {
        bWait = FALSE;
      }
      else
      {
        bWait = TRUE;
      }

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
      DecriptString(szProgramFolder, szBuf);

      dwIndex1 = 0;
      itoa(dwIndex1, szIndex1, 10);
      lstrcpy(szSection1, szSection0);
      lstrcat(szSection1, "-Shortcut");
      lstrcat(szSection1, szIndex1);
      GetPrivateProfileString(szSection1, "File", "", szBuf, MAX_BUF, szFileIniConfig);
      while(*szBuf != '\0')
      {
        DecriptString(szFile, szBuf);
        GetPrivateProfileString(szSection1, "Arguments",    "", szBuf, MAX_BUF, szFileIniConfig);
        DecriptString(szArguments, szBuf);
        GetPrivateProfileString(szSection1, "Working Dir",  "", szBuf, MAX_BUF, szFileIniConfig);
        DecriptString(szWorkingDir, szBuf);
        GetPrivateProfileString(szSection1, "Description",  "", szBuf, MAX_BUF, szFileIniConfig);
        DecriptString(szDescription, szBuf);
        GetPrivateProfileString(szSection1, "Icon Path",    "", szBuf, MAX_BUF, szFileIniConfig);
        DecriptString(szIconPath, szBuf);
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
    DecriptString(szProgramFolder, szBuf);
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

    WinSpawn(szProgramFolder, NULL, NULL, iShowFolder, TRUE);

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    lstrcpy(szSection0, "Program Folder");
    lstrcat(szSection0, szIndex0);
    GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, MAX_BUF, szFileIniConfig);
  }
  return(FO_SUCCESS);
}
