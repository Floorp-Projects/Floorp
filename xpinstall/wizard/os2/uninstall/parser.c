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
#include "logkeys.h"
#include "extra.h"
#include "parser.h"
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
    DosReplaceModule(szFile, NULL, NULL);
    FileDelete(szFile);
  }

  return TRUE;
}

void RemoveUninstaller(PSZ szUninstallFilename)
{
  char      szUninstallFile[MAX_BUF];

  strcpy(szUninstallFile, ugUninstall.szLogPath);
  strcat(szUninstallFile, "\\");
  strcat(szUninstallFile, szUninstallFilename);
  DeleteOrDelayUntilReboot(szUninstallFile);
  DirectoryRemove(ugUninstall.szLogPath, FALSE);
}

sil *InitSilNodes(char *szInFile)
{
  FILE      *ifp;
  char      szLineRead[MAX_BUF];
  sil       *silTemp;
  sil       *silHead;
  unsigned long long ullLineCount;

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

void ParseForFile(PSZ szString, PSZ szKeyStr, PSZ szFile, ULONG ulShortFilenameBufSize)
{
  int     iLen;
  PSZ     szFirstNonSpace;
  char    szBuf[MAX_BUF];

  if((szFirstNonSpace = GetFirstNonSpace(&(szString[strlen(szKeyStr)]))) != NULL)
  {
    iLen = strlen(szFirstNonSpace);
    if(szFirstNonSpace[iLen - 1] == '\n')
      szFirstNonSpace[iLen -1] = '\0';

    strcpy(szFile, szFirstNonSpace);
  }
}

void ParseForCopyFile(PSZ szString, PSZ szKeyStr, PSZ szFile, ULONG ulShortFilenameBufSize)
{
  int     iLen;
  PSZ     szFirstNonSpace;
  PSZ     szSubStr = NULL;
  char    szBuf[MAX_BUF];

  if((szSubStr = strstr(szString, " to ")) != NULL)
  {
    if((szFirstNonSpace = GetFirstNonSpace(&(szSubStr[strlen(" to ")]))) != NULL)
    {
      iLen = strlen(szFirstNonSpace);
      if(szFirstNonSpace[iLen - 1] == '\n')
        szFirstNonSpace[iLen -1] = '\0';

    strcpy(szFile, szFirstNonSpace);
    }
  }
}

void ParseForOS2INIInfo(PSZ szString, PSZ szKeyStr, PSZ szApp, ULONG ulAppBufSize, PSZ szKey, ULONG ulKeyBufSize)
{
  int     i;
  int     iLen;
  int     iBrackets;
  char    szStrCopy[MAX_BUF];
  PSZ     szFirstNonSpace;
  PSZ     szFirstBackSlash;
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
          strcpy(szKey, &(szFirstNonSpace[i + 1]));
          bFoundName = TRUE;
        }
      }
      else
      {
        /* locate the first non space to the left of the last '[' */
        if(!isspace(szFirstNonSpace[i]))
        {
          szFirstNonSpace[i + 1] = '\0';
          strcpy(szApp, szFirstNonSpace);
          break;
        }
      }
    }
  }
}

ULONG Uninstall(sil* silInstallLogHead)
{
  sil   *silInstallLogTemp;
  PSZ   szSubStr;
  char  szLCLine[MAX_BUF];
  char  szApp[MAX_BUF];
  char  szKey[MAX_BUF];
  char  szFile[MAX_BUF];

  if(silInstallLogHead != NULL)
  {
    silInstallLogTemp = silInstallLogHead;
    do
    {
      silInstallLogTemp = silInstallLogTemp->Prev;
      strcpy(szLCLine, silInstallLogTemp->szLine);
      strlwr(szLCLine);

      if(((szSubStr = strstr(szLCLine, KEY_INSTALLING)) != NULL) &&
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
      else if(((szSubStr = strstr(silInstallLogTemp->szLine, KEY_STORE_INI_ENTRY)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        /* check for "Store INI Entry: " string and get the app/key pair */
        ParseForOS2INIInfo(szSubStr, KEY_STORE_INI_ENTRY, szApp, sizeof(szKey), szKey, sizeof(szKey));
        PrfWriteProfileString(HINI_USER, szApp, szKey, NULL);
      }
      else if(((szSubStr = strstr(silInstallLogTemp->szLine, KEY_OS2_OBJECT)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        /* check for "OS2 object" string and delete the object */
        HOBJECT hObj = NULLHANDLE;
        ParseForFile(szSubStr, KEY_OS2_OBJECT, szFile, sizeof(szFile));
        hObj = WinQueryObject(szFile);
        if (hObj) {
           WinDestroyObject(hObj);
        }
      }
      else if(((szSubStr = strstr(szLCLine, KEY_CREATE_FOLDER)) != NULL) &&
               (strstr(szLCLine, KEY_DO_NOT_UNINSTALL) == NULL))
      {
        ParseForFile(szSubStr, KEY_CREATE_FOLDER, szFile, sizeof(szFile));
        DirectoryRemove(szFile, FALSE);
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
  long            ulNumber;
  long            ulMaxNumber;
  PSZ             szDotPtr;
  HDIR            hFile;
  FILEFINDBUF3    fdFile;
  ULONG           ulFindCount;
  ULONG           ulAttrs;
  BOOL            bFound;

  if(FileExists(szTargetPath))
  {
    /* zero out the memory */
    memset(szOutBuf,                0, dwOutBufSize);
    memset(szSearchFilename,        0, sizeof(szSearchFilename));
    memset(szFilenameOnly,          0, sizeof(szFilenameOnly));
    memset(szFilenameExtensionOnly, 0, sizeof(szFilenameExtensionOnly));

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
    ulNumber         = 0;
    ulMaxNumber      = -1;

    /* find the largest numbered filename in the szTargetPath */
    ulFindCount = 1;
    hFile = HDIR_CREATE;
    ulAttrs = FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_DIRECTORY | FILE_ARCHIVED;
    if((DosFindFirst(szSearchTargetFullFilename, &hFile, ulAttrs, &fdFile, sizeof(fdFile), &ulFindCount, FIL_STANDARD)) != NO_ERROR)
      bFound = FALSE;
    else
      bFound = TRUE;

    while(bFound)
    {
       memset(szNumber, 0, sizeof(szNumber));
      if((stricmp(fdFile.achName, ".") != 0) && (stricmp(fdFile.achName, "..") != 0))
      {
        strcpy(szNumber, &fdFile.achName[iFilenameOnlyLen]);
        ulNumber = atoi(szNumber);
        if(ulNumber > ulMaxNumber)
          ulMaxNumber = ulNumber;
      }

      ulFindCount = 1;
      if (DosFindNext(hFile, &fdFile, sizeof(fdFile), &ulFindCount) == NO_ERROR) {
        bFound = TRUE;
      } else {
        bFound = FALSE;
      }
    }

    DosFindClose(hFile);

    strcpy(szOutBuf, szTargetPath);
    AppendBackSlash(szOutBuf, dwOutBufSize);
    strcat(szOutBuf, szFilenameOnly);
    _itoa(ulMaxNumber, szNumber, 10);
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

