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
#include <logkeys.h>

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

      case T_PRE_XPCOM:
        if(lstrcmpi(szBuf, "pre xpcom") == 0)
          return(TRUE);
        break;

      case T_POST_XPCOM:
        if(lstrcmpi(szBuf, "post xpcom") == 0)
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
  ProcessCopyFileSequential(dwTiming);
  ProcessSelfRegisterFile(dwTiming);
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
    CreateDirectoriesAll(szBuf, FALSE);
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

HRESULT ProcessXpcomFile()
{
  char szSource[MAX_BUF];
  char szDestination[MAX_BUF];

  if(*siCFXpcomFile.szMessage != '\0')
    ShowMessage(siCFXpcomFile.szMessage, TRUE);

  FileUncompress(siCFXpcomFile.szSource, siCFXpcomFile.szDestination);

  /* copy msvcrt.dll and msvcirt.dll to the bin of the Xpcom temp dir:
   *   (c:\temp\Xpcom.ns\bin)
   * This is incase these files do not exist on the system */
  lstrcpy(szSource, siCFXpcomFile.szDestination);
  AppendBackSlash(szSource, sizeof(szSource));
  lstrcat(szSource, "ms*.dll");

  lstrcpy(szDestination, siCFXpcomFile.szDestination);
  AppendBackSlash(szDestination, sizeof(szDestination));
  lstrcat(szDestination, "bin");

  FileCopy(szSource, szDestination, TRUE);

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

    /* log the file move command */
    lstrcpy(szBuf, szFrom);
    lstrcat(szBuf, " to ");
    lstrcat(szBuf, szTo);
    UpdateInstallLog(KEY_MOVE_FILE, szBuf);

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

    /* log the file move command */
    lstrcpy(szBuf, szFrom);
    lstrcat(szBuf, " to ");
    lstrcat(szBuf, szToTemp);
    UpdateInstallLog(KEY_MOVE_FILE, szBuf);

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

      /* log the file move command */
      lstrcpy(szBuf, szFromTemp);
      lstrcat(szBuf, " to ");
      lstrcat(szBuf, szToTemp);
      UpdateInstallLog(KEY_MOVE_FILE, szBuf);
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

    /* log the file copy command */
    lstrcpy(szBuf, szFrom);
    lstrcat(szBuf, " to ");
    lstrcat(szBuf, szToTemp);
    UpdateInstallLog(KEY_COPY_FILE, szBuf);

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

      /* log the file copy command */
      lstrcpy(szBuf, szFromTemp);
      lstrcat(szBuf, " to ");
      lstrcat(szBuf, szToTemp);
      UpdateInstallLog(KEY_COPY_FILE, szBuf);
    }

    bFound = FindNextFile(hFile, &fdFile);
  }

  FindClose(hFile);
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
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  BOOL            bFound;

  lstrcpy(szSourceFullFilename, szSourcePath);
  AppendBackSlash(szSourceFullFilename, sizeof(szSourceFullFilename));
  lstrcat(szSourceFullFilename, szFilename);

  if(FileExists(szSourceFullFilename))
  {
    /* zero out the memory */
    ZeroMemory(szSearchFilename,        sizeof(szSearchFilename));
    ZeroMemory(szFilenameOnly,          sizeof(szFilenameOnly));
    ZeroMemory(szFilenameExtensionOnly, sizeof(szFilenameExtensionOnly));

    /* parse for the filename w/o extention and also only the extension */
    if((szDotPtr = strstr(szFilename, ".")) != NULL)
    {
      *szDotPtr = '\0';
      lstrcpy(szSearchFilename, szFilename);
      lstrcpy(szFilenameOnly, szFilename);
      lstrcpy(szFilenameExtensionOnly, &szDotPtr[1]);
      *szDotPtr = '.';
    }
    else
    {
      lstrcpy(szFilenameOnly, szFilename);
      lstrcpy(szSearchFilename, szFilename);
    }

    /* create the wild arg filename to search for in the szDestPath */
    lstrcat(szSearchFilename, "*.*");
    lstrcpy(szSearchDestFullFilename, szDestPath);
    AppendBackSlash(szSearchDestFullFilename, sizeof(szSearchDestFullFilename));
    lstrcat(szSearchDestFullFilename, szSearchFilename);

    iFilenameOnlyLen = lstrlen(szFilenameOnly);
    dwNumber         = 0;
    dwMaxNumber      = 0;

    /* find the largest numbered filename in the szDestPath */
    if((hFile = FindFirstFile(szSearchDestFullFilename, &fdFile)) == INVALID_HANDLE_VALUE)
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

    lstrcpy(szDestFullFilename, szDestPath);
    AppendBackSlash(szDestFullFilename, sizeof(szDestFullFilename));
    lstrcat(szDestFullFilename, szFilenameOnly);
    itoa(dwMaxNumber + 1, szNumber, 10);
    lstrcat(szDestFullFilename, szNumber);

    if(*szFilenameExtensionOnly != '\0')
    {
      lstrcat(szDestFullFilename, ".");
      lstrcat(szDestFullFilename, szFilenameExtensionOnly);
    }

    CopyFile(szSourceFullFilename, szDestFullFilename, TRUE);
  }

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

HRESULT ProcessCopyFileSequential(DWORD dwTiming)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];
  char  szFilename[MAX_BUF];

  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szSection, "Copy File Sequential");
  lstrcat(szSection, szIndex);
  GetPrivateProfileString(szSection, "Filename", "", szFilename, MAX_BUF, szFileIniConfig);
  while(*szFilename != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      GetPrivateProfileString(szSection, "Source", "", szBuf, MAX_BUF, szFileIniConfig);
      DecryptString(szSource, szBuf);

      GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
      DecryptString(szDestination, szBuf);

      FileCopySequential(szSource, szDestination, szFilename);
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, "Copy File Sequential");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "Filename", "", szFilename, MAX_BUF, szFileIniConfig);
  }
  return(FO_SUCCESS);
}

int RegisterDll32(char *File)
{
  FARPROC   DllReg;
  HINSTANCE hLib;

  if((hLib = LoadLibraryEx(File, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) != NULL)
  {
    if((DllReg = GetProcAddress(hLib, "DllRegisterServer")) != NULL)
      DllReg();

    FreeLibrary(hLib);
    return(0);
  }

  return(1);
}


HRESULT FileSelfRegister(LPSTR szFilename, LPSTR szDestination)
{
  char            szFullFilenamePath[MAX_BUF];
  DWORD           dwRv;
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  BOOL            bFound;

  lstrcpy(szFullFilenamePath, szDestination);
  AppendBackSlash(szFullFilenamePath, sizeof(szFullFilenamePath));
  lstrcat(szFullFilenamePath, szFilename);

  /* From file path exists and To file path does not exist */
  if(FileExists(szFullFilenamePath))
  {
    RegisterDll32(szFullFilenamePath);
    return(FO_SUCCESS);
  }

  lstrcpy(szFullFilenamePath, szDestination);
  AppendBackSlash(szFullFilenamePath, sizeof(szFullFilenamePath));
  lstrcat(szFullFilenamePath, szFilename);

  if((hFile = FindFirstFile(szFullFilenamePath, &fdFile)) == INVALID_HANDLE_VALUE)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
    {
      /* create full path string including filename for destination */
      lstrcpy(szFullFilenamePath, szDestination);
      AppendBackSlash(szFullFilenamePath, sizeof(szFullFilenamePath));
      lstrcat(szFullFilenamePath, fdFile.cFileName);

      if((dwRv = FileExists(szFullFilenamePath)) && (dwRv != FILE_ATTRIBUTE_DIRECTORY))
        RegisterDll32(szFullFilenamePath);
    }

    bFound = FindNextFile(hFile, &fdFile);
  }

  FindClose(hFile);
  return(FO_SUCCESS);
}

HRESULT ProcessSelfRegisterFile(DWORD dwTiming)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szFilename[MAX_BUF];
  char  szDestination[MAX_BUF];

  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szSection, "Self Register File");
  lstrcat(szSection, szIndex);
  GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      GetPrivateProfileString(szSection, "Filename", "", szFilename, MAX_BUF, szFileIniConfig);
      FileSelfRegister(szFilename, szDestination);
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, "Self Register File");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "Destination", "", szBuf, MAX_BUF, szFileIniConfig);
  }
  return(FO_SUCCESS);
}

void UpdateInstallLog(LPSTR szKey, LPSTR szDir)
{
  FILE *fInstallLog;
  char szBuf[MAX_BUF];
  char szFileInstallLog[MAX_BUF];

  if(gbILUseTemp)
  {
    lstrcpy(szFileInstallLog, szTempDir);
    AppendBackSlash(szFileInstallLog, sizeof(szFileInstallLog));
  }
  else
  {
    lstrcpy(szFileInstallLog, sgProduct.szPath);
    AppendBackSlash(szFileInstallLog, sizeof(szFileInstallLog));
    lstrcat(szFileInstallLog, sgProduct.szSubPath);
    AppendBackSlash(szFileInstallLog, sizeof(szFileInstallLog));
  }

  CreateDirectoriesAll(szFileInstallLog, FALSE);
  lstrcat(szFileInstallLog, FILE_INSTALL_LOG);

  if((fInstallLog = fopen(szFileInstallLog, "a+t")) != NULL)
  {
    lstrcpy(szBuf, "     ** ");
    lstrcat(szBuf, szKey);
    lstrcat(szBuf, szDir);
    lstrcat(szBuf, "\n");
    fwrite(szBuf, sizeof(char), lstrlen(szBuf), fInstallLog);
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
    lstrcpy(szJSFile, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szJSFile, sizeof(szJSFile));
      lstrcat(szJSFile, sgProduct.szSubPath);
    }
    AppendBackSlash(szJSFile, sizeof(szJSFile));
    lstrcat(szJSFile, "defaults\\pref\\");
    CreateDirectoriesAll(szJSFile, TRUE);
    lstrcat(szJSFile, FILE_ALL_JS);

    if((fJSFile = fopen(szJSFile, "a+t")) != NULL)
    {
      ZeroMemory(szBuf, sizeof(szBuf));
      if(*diAdvancedSettings.szProxyServer != '\0')
      {
        lstrcpy(szBuf, "user_pref(\"network.proxy.ftp\", \"");
        lstrcat(szBuf, diAdvancedSettings.szProxyServer);
        lstrcat(szBuf, "\");\n");
      }

      if(*diAdvancedSettings.szProxyPort != '\0')
      {
        lstrcat(szBuf, "user_pref(\"network.proxy.ftp_port\", ");
        lstrcat(szBuf, diAdvancedSettings.szProxyPort);
        lstrcat(szBuf, ");\n");
      }
      lstrcat(szBuf, "user_pref(\"network.proxy.type\", 1);\n");

      fwrite(szBuf, sizeof(char), lstrlen(szBuf), fJSFile);
      fclose(fJSFile);
    }
  }
}

HRESULT CreateDirectoriesAll(char* szPath, BOOL bLogForUninstall)
{
  int     i;
  int     iLen = lstrlen(szPath);
  char    szCreatePath[MAX_BUF];
  HRESULT hrResult = 0;

  ZeroMemory(szCreatePath, MAX_BUF);
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
          UpdateInstallLog(KEY_CREATE_FOLDER, szCreatePath);
      }
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
      CreateDirectoriesAll(szDestination, TRUE);
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

BOOL ParseRegType(LPSTR szType, DWORD *dwType)
{
  BOOL bSZ;

  if(lstrcmpi(szType, "REG_SZ") == 0)
  {
    /* Unicode NULL terminated string */
    *dwType = REG_SZ;
    bSZ     = TRUE;
  }
  else if(lstrcmpi(szType, "REG_EXPAND_SZ") == 0)
  {
    /* Unicode NULL terminated string
     * (with environment variable references) */
    *dwType = REG_EXPAND_SZ;
    bSZ     = TRUE;
  }
  else if(lstrcmpi(szType, "REG_BINARY") == 0)
  {
    /* Free form binary */
    *dwType = REG_BINARY;
    bSZ     = FALSE;
  }
  else if(lstrcmpi(szType, "REG_DWORD") == 0)
  {
    /* 32bit number */
    *dwType = REG_DWORD;
    bSZ     = FALSE;
  }
  else if(lstrcmpi(szType, "REG_DWORD_LITTLE_ENDIAN") == 0)
  {
    /* 32bit number
     * (same as REG_DWORD) */
    *dwType = REG_DWORD_LITTLE_ENDIAN;
    bSZ     = FALSE;
  }
  else if(lstrcmpi(szType, "REG_DWORD_BIG_ENDIAN") == 0)
  {
    /* 32bit number */
    *dwType = REG_DWORD_BIG_ENDIAN;
    bSZ     = FALSE;
  }
  else if(lstrcmpi(szType, "REG_LINK") == 0)
  {
    /* Symbolic link (unicode) */
    *dwType = REG_LINK;
    bSZ     = TRUE;
  }
  else if(lstrcmpi(szType, "REG_MULTI_SZ") == 0)
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

void SetWinReg(HKEY hkRootKey, LPSTR szKey, BOOL bOverwriteKey, LPSTR szName, BOOL bOverwriteName, DWORD dwType, LPBYTE lpbData, DWORD dwSize)
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
      dwErr = RegSetValueEx(hkResult, szName, 0, dwType, lpbData, dwSize);

    RegCloseKey(hkResult);
  }
}

HRESULT ProcessWinReg(DWORD dwTiming)
{
  char    szIndex[MAX_BUF];
  char    szBuf[MAX_BUF];
  char    szKey[MAX_BUF];
  char    szName[MAX_BUF];
  char    szValue[MAX_BUF];
  char    szDecrypt[MAX_BUF];
  char    szOverwriteKey[MAX_BUF];
  char    szOverwriteName[MAX_BUF];
  char    szSection[MAX_BUF];
  HKEY    hRootKey;
  BOOL    bOverwriteKey;
  BOOL    bOverwriteName;
  DWORD   dwIndex;
  DWORD   dwType;
  DWORD   dwSize;
  __int64 iiNum;

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

      GetPrivateProfileString(szSection, "Size",                "", szBuf,           MAX_BUF, szFileIniConfig);
      if(*szBuf != '\0')
        dwSize = atoi(szBuf);
      else
        dwSize = 0;

      GetPrivateProfileString(szSection, "Type",                "", szBuf,           MAX_BUF, szFileIniConfig);
      if(ParseRegType(szBuf, &dwType))
      {
        /* create/set windows registry key here (string value)! */
        SetWinReg(hRootKey, szKey, bOverwriteKey, szName, bOverwriteName, dwType, (CONST LPBYTE)szValue, lstrlen(szValue));
      }
      else
      {
        iiNum = _atoi64(szValue);
        /* create/set windows registry key here (binary/dword value)! */
        SetWinReg(hRootKey, szKey, bOverwriteKey, szName, bOverwriteName, dwType, (CONST LPBYTE)&iiNum, dwSize);
      }
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
