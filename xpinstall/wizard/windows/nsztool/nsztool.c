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

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <stdlib.h>
#include <ctype.h>
#include "nsztool.h"
#include "zlib.h"
#include "math.h"

#define DEFAULT_SEA_FILE "nsinstall.exe"

/* Function to show the usage for this application */
void ShowUsage(char *name)
{
    char szBuf[MAX_BUF];

    wsprintf(szBuf, "Usage: %s <output sea name> <files [file1] [file2]...>\n", name);
    lstrcat(szBuf, "\n");
    lstrcat(szBuf, "    output sea name: name to use for the self-extracting executable\n");
    lstrcat(szBuf, "    files: one or more files to add to the self-extracing executable\n");
    MessageBox(NULL, szBuf, "Usage", MB_OK);
}

/* Function to print error message with/without error code */
void PrintError(LPSTR szMsg, DWORD dwErrorCodeSH)
{
  DWORD dwErr;
  char  szErrorString[MAX_BUF];

  if(dwErrorCodeSH == ERROR_CODE_SHOW)
  {
    dwErr = GetLastError();
    wsprintf(szErrorString, "%d : %s", dwErr, szMsg);
  }
  else
    wsprintf(szErrorString, "%s", szMsg);

  MessageBox(NULL, szErrorString, NULL, MB_ICONEXCLAMATION);
}

/* Function to remove quotes from a string */
void RemoveQuotes(LPSTR lpszSrc, LPSTR lpszDest, int iDestSize)
{
  char *lpszBegin;

  if(lstrlen(lpszSrc) > iDestSize)
    return;

  if(*lpszSrc == '\"')
    lpszBegin = &lpszSrc[1];
  else
    lpszBegin = lpszSrc;

  lstrcpy(lpszDest, lpszBegin);

  if(lpszDest[lstrlen(lpszDest) - 1] == '\"')
    lpszDest[lstrlen(lpszDest) - 1] = '\0';
}

/* Function to remove the last backslash from a path string */
void RemoveBackSlash(LPSTR szInput)
{
  int   iCounter;
  DWORD dwInputLen;

  if(szInput != NULL)
  {
    dwInputLen = lstrlen(szInput);

    for(iCounter = dwInputLen -1; iCounter >= 0 ; iCounter--)
    {
      if(szInput[iCounter] == '\\')
        szInput[iCounter] = '\0';
      else
        break;
    }
  }
}

/* Function to append a backslash to a path string */
void AppendBackSlash(LPSTR szInput, DWORD dwInputSize)
{
  if(szInput != NULL)
  {
    if(szInput[strlen(szInput) - 1] != '\\')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "\\");
      }
    }
  }
}

/* Function to parse a path string for one of three parts of a path:
 *   Filename only
 *   Path only
 *   drive only */
void ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwOutputSize, DWORD dwType)
{
  int   iCounter;
  DWORD dwCounter;
  DWORD dwInputLen;
  BOOL  bFound;

  if((szInput != NULL) && (szOutput != NULL))
  {
    bFound        = FALSE;
    dwInputLen    = lstrlen(szInput);
    ZeroMemory(szOutput, dwOutputSize);

    if(dwInputLen < dwOutputSize)
    {
      switch(dwType)
      {
        case PP_FILENAME_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              lstrcpy(szOutput, &szInput[iCounter + 1]);
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            lstrcpy(szOutput, szInput);

          break;

        case PP_PATH_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              lstrcpy(szOutput, szInput);
              szOutput[iCounter + 1] = '\0';
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            lstrcpy(szOutput, szInput);

          break;

        case PP_ROOT_ONLY:
          if(szInput[1] == ':')
          {
            szOutput[0] = szInput[0];
            szOutput[1] = szInput[1];
            AppendBackSlash(szOutput, dwOutputSize);
          }
          else if(szInput[1] == '\\')
          {
            int iFoundBackSlash = 0;
            for(dwCounter = 0; dwCounter < dwInputLen; dwCounter++)
            {
              if(szInput[dwCounter] == '\\')
              {
                szOutput[dwCounter] = szInput[dwCounter];
                ++iFoundBackSlash;
              }

              if(iFoundBackSlash == 3)
                break;
            }

            if(iFoundBackSlash != 0)
              AppendBackSlash(szOutput, dwOutputSize);
          }
          break;
      }
    }
  }
}

/* Function to check to see if a file exists.
 * If it does, return it's attributes */
long FileExists(LPSTR szFile)
{
  DWORD rv;

  if((rv = GetFileAttributes(szFile)) == -1)
  {
    return(FALSE);
  }
  else
  {
    return(rv);
  }
}

/* Function to locate the first non space character in a string,
 * and return a pointer to it. */
LPSTR GetFirstNonSpace(LPSTR lpszString)
{
  int   i;
  int   iStrLength;

  iStrLength = lstrlen(lpszString);

  for(i = 0; i < iStrLength; i++)
  {
    if(!isspace(lpszString[i]))
      return(&lpszString[i]);
  }

  return(NULL);
}

/* Function to return the argument count given a command line input
 * format string */
int GetArgC(LPSTR lpszCommandLine)
{
  int   i;
  int   iArgCount;
  int   iStrLength;
  LPSTR lpszBeginStr;
  BOOL  bFoundQuote;
  BOOL  bFoundSpace;

  iArgCount    = 0;
  lpszBeginStr = GetFirstNonSpace(lpszCommandLine);

  if(lpszBeginStr == NULL)
    return(iArgCount);

  iStrLength   = lstrlen(lpszBeginStr);
  bFoundQuote  = FALSE;
  bFoundSpace  = TRUE;

  for(i = 0; i < iStrLength; i++)
  {
    if(lpszCommandLine[i] == '\"')
    {
      if(bFoundQuote == FALSE)
      {
        ++iArgCount;
        bFoundQuote = TRUE;
      }
      else
      {
        bFoundQuote = FALSE;
      }
    }
    else if(bFoundQuote == FALSE)
    {
      if(!isspace(lpszCommandLine[i]) && (bFoundSpace == TRUE))
      {
        ++iArgCount;
        bFoundSpace = FALSE;
      }
      else if(isspace(lpszCommandLine[i]))
      {
        bFoundSpace = TRUE;
      }
    }
  }

  return(iArgCount);
}

/* Function to return a specific argument parameter from a given command line input
 * format string. */
LPSTR GetArgV(LPSTR lpszCommandLine, int iIndex, LPSTR lpszDest, int iDestSize)
{
  int   i;
  int   j;
  int   iArgCount;
  int   iStrLength;
  LPSTR lpszBeginStr;
  LPSTR lpszDestTemp;
  BOOL  bFoundQuote;
  BOOL  bFoundSpace;

  iArgCount    = 0;
  lpszBeginStr = GetFirstNonSpace(lpszCommandLine);

  if(lpszBeginStr == NULL)
    return(NULL);

  lpszDestTemp = (char *)calloc(iDestSize, sizeof(char));
  if(lpszDestTemp == NULL)
  {
    PrintError("Out of memory", ERROR_CODE_HIDE);
    exit(1);
  }

  ZeroMemory(lpszDest, iDestSize);
  iStrLength    = lstrlen(lpszBeginStr);
  bFoundQuote   = FALSE;
  bFoundSpace   = TRUE;
  j             = 0;

  for(i = 0; i < iStrLength; i++)
  {
    if(lpszCommandLine[i] == '\"')
    {
      if(bFoundQuote == FALSE)
      {
        ++iArgCount;
        bFoundQuote = TRUE;
      }
      else
      {
        bFoundQuote = FALSE;
      }
    }
    else if(bFoundQuote == FALSE)
    {
      if(!isspace(lpszCommandLine[i]) && (bFoundSpace == TRUE))
      {
        ++iArgCount;
        bFoundSpace = FALSE;
      }
      else if(isspace(lpszCommandLine[i]))
      {
        bFoundSpace = TRUE;
      }
    }

    if((iIndex == (iArgCount - 1)) &&
      ((bFoundQuote == TRUE) || (bFoundSpace == FALSE) ||
      ((bFoundQuote == FALSE) && (lpszCommandLine[i] == '\"'))))
    {
      if(j < iDestSize)
      {
        lpszDestTemp[j] = lpszCommandLine[i];
        ++j;
      }
      else
      {
        lpszDestTemp[j] = '\0';
      }
    }
  }

  RemoveQuotes(lpszDestTemp, lpszDest, iDestSize);
  free(lpszDestTemp);
  return(lpszDest);
}

/* Function to add a file to a self-extracting .exe file.
 * It compresses the file, then adds it as a resource type "FILE". */
void AddFile(HANDLE hExe, LPSTR lpszFile)
{
  char        szBuf[MAX_BUF];
  char        szResourceName[MAX_BUF];
  HANDLE      hInputFile;
  DWORD       dwBytesRead;
  DWORD       dwFileSize;
  DWORD       dwFileSizeCmp;
  LPBYTE      lpBuf;
  LPBYTE      lpBufCmp;

  if(!hExe)
    exit(1);

  ParsePath(lpszFile, szResourceName, sizeof(szResourceName), PP_FILENAME_ONLY);
  strupr(szResourceName);
  hInputFile = CreateFile(lpszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if(hInputFile == INVALID_HANDLE_VALUE)
  {
    PrintError("CreateFile() failed", ERROR_CODE_SHOW);
    exit(1);
  }

  dwFileSize    = GetFileSize(hInputFile, NULL);
  dwFileSizeCmp = ((long)ceil(dwFileSize * 0.001)) + dwFileSize + 12;
  lpBuf         = (LPBYTE)malloc(dwFileSize);
  lpBufCmp      = (LPBYTE)malloc(dwFileSizeCmp + (sizeof(DWORD) * 2));
  if((lpBuf == NULL) || (lpBufCmp == NULL))
  {
    PrintError("Out of memory", ERROR_CODE_HIDE);
    exit(1);
  }

  ReadFile(hInputFile, lpBuf, dwFileSize, &dwBytesRead, NULL);
  if(dwBytesRead != dwFileSize)
  {
    wsprintf(szBuf, "Error reading file: %s", lpszFile);
    PrintError(szBuf, ERROR_CODE_HIDE);
    exit(1);
  }

  if(compress((lpBufCmp + (sizeof(DWORD) * 2)), &dwFileSizeCmp, (const Bytef*)lpBuf, dwFileSize) != Z_OK)
  {
    PrintError("Error occurred during compression of archives!", ERROR_CODE_HIDE);
    exit(1);
  }

  *(LPDWORD)lpBufCmp = dwFileSizeCmp;
  *(LPDWORD)(lpBufCmp + sizeof(DWORD)) = dwFileSize;

  if(!UpdateResource(hExe, "FILE", szResourceName, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                     lpBufCmp, dwFileSizeCmp + (sizeof(DWORD) * 2)))
  {
    PrintError("UpdateResource() failed", ERROR_CODE_SHOW);
    exit(1);
  }

  free(lpBuf);
  if(!CloseHandle(hInputFile))
  {
    PrintError("CloseHandle() failed", ERROR_CODE_SHOW);
    exit(1);
  }
}

/* Function to extract a resourced file from a .exe file.
 * It also uncompresss the file after extracting it. */
BOOL APIENTRY ExtractFilesProc(HANDLE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG lParam)
{
  char    szBuf[MAX_BUF];
  HRSRC   hResInfo;
  LPSTR   lpszSeaExe = (LPSTR)lParam;
  HANDLE  hFile;
  LPBYTE  lpBytes;
  LPBYTE  lpBytesUnCmp;
  HGLOBAL hGlobal;

  // Extract the file
  hResInfo = FindResource((HINSTANCE)hModule, lpszName, lpszType);
  hGlobal  = LoadResource((HINSTANCE)hModule, hResInfo);
  lpBytes  = (LPBYTE)LockResource(hGlobal);

  // Create the file
  hFile = CreateFile(lpszSeaExe, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                     FILE_ATTRIBUTE_TEMPORARY, NULL);

  if(hFile != INVALID_HANDLE_VALUE)
  {
    DWORD dwSize;
    DWORD dwSizeUnCmp;

    lpBytesUnCmp = (LPBYTE)malloc((*(LPDWORD)(lpBytes + sizeof(DWORD))) + 1);
    dwSizeUnCmp  = *(LPDWORD)(lpBytes + sizeof(DWORD));

    // Copy the file. The first DWORD specifies the size of the file
    dwSize = *(LPDWORD)lpBytes;
    lpBytes += (sizeof(DWORD) * 2);

    if(uncompress(lpBytesUnCmp, &dwSizeUnCmp, lpBytes, dwSize) != Z_OK)
    {
      wsprintf(szBuf, "Error occurred during uncompression of %s: ", lpszSeaExe);
      PrintError(szBuf, ERROR_CODE_HIDE);
      CloseHandle(hFile);
      FreeResource(hResInfo);
      return FALSE;
    }

    while(dwSizeUnCmp > 0)
    {
      DWORD dwBytesToWrite;
      DWORD dwBytesWritten;

      dwBytesToWrite = dwSizeUnCmp > 4096 ? 4096 : dwSizeUnCmp;
      if(!WriteFile(hFile, lpBytesUnCmp, dwBytesToWrite, &dwBytesWritten, NULL))
      {
        wsprintf(szBuf, "%s: write error", lpszSeaExe);
        MessageBox(NULL, szBuf, "Error", MB_OK | MB_ICONEXCLAMATION);
        CloseHandle(hFile);
        FreeResource(hResInfo);
        return FALSE;
      }

      dwSizeUnCmp -= dwBytesWritten;
      lpBytesUnCmp += dwBytesWritten;
    }

    CloseHandle(hFile);
  }

  // Release the resource
  FreeResource(hResInfo);

  return TRUE;  // keep enumerating
}

HANDLE InitResource(LPSTR lpszSeaExe)
{
  HANDLE hExe;

  if((hExe = BeginUpdateResource(lpszSeaExe, FALSE)) == NULL)
  {
    DWORD dwErr;

    dwErr = GetLastError();
    if(dwErr == ERROR_CALL_NOT_IMPLEMENTED)
    {
      MessageBox(NULL, "This application does not run under this OS", NULL, MB_ICONEXCLAMATION);
      exit(0);
    }
    else
    {
      PrintError("BeginUpdateResource() error", ERROR_CODE_SHOW);
      exit(1);
    }
  }
  return(hExe);
}

void DeInitResource(HANDLE hExe)
{
  if(!EndUpdateResource(hExe, FALSE))
  {
    PrintError("EndUpdateResource() failed", ERROR_CODE_SHOW);
    exit(1);
  }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
  int               i;
  int               iArgC;
  char              szBuf[MAX_BUF];
  char              szArgVBuf[MAX_BUF];
  char              szFileFullPath[MAX_BUF];
  char              szSeaExe[MAX_BUF];
  char              szNsZipName[MAX_BUF];
  char              szAppName[MAX_BUF];
  char              szAppPath[MAX_BUF];

#ifdef SSU_DEBUG
  char              szOutputStr[MAX_BUF];
#endif

  WIN32_FIND_DATA   findFileData;
  HANDLE            hExe;
  HANDLE            hFindFile;
  BOOL              bSelfUpdate;

  if(GetModuleFileName(NULL, szAppName, sizeof(szAppName)) == 0L)
  {
    PrintError("GetModuleFileName() failed", ERROR_CODE_SHOW);
    exit(1);
  }
  ParsePath(szAppName, szBuf, sizeof(szBuf), PP_FILENAME_ONLY);
  ParsePath(szAppPath, szBuf, sizeof(szBuf), PP_PATH_ONLY);

  if(*lpszCmdLine == '\0')
  {
    ShowUsage(szBuf);
    exit(1);
  }

  iArgC = GetArgC(lpszCmdLine);
  bSelfUpdate = FALSE;

#ifdef SSU_DEBUG
  wsprintf(szOutputStr, "ArgC: %d\n", iArgC);
  for(i = 0; i < iArgC; i++)
  {
    GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
    itoa(i, szBuf, 10);
    lstrcat(szOutputStr, "    ");
    lstrcat(szOutputStr, szBuf);
    lstrcat(szOutputStr, ": ");
    lstrcat(szOutputStr, szArgVBuf);
    lstrcat(szOutputStr, "\n");
  }
  MessageBox(NULL, szOutputStr, "Output", MB_OK);
#endif

  /* Get the first parameter */
  GetArgV(lpszCmdLine, 0, szSeaExe, sizeof(szSeaExe));
  if(lstrcmpi(szSeaExe, "-g") == 0)
  {
    /* The first parameter is "-g".
     * Create a new nszip that contains nsinstall.exe in itself */

    GetArgV(lpszCmdLine, 1, szNsZipName, sizeof(szNsZipName));
    GetArgV(lpszCmdLine, 2, szSeaExe, sizeof(szSeaExe));
    if(!FileExists(szSeaExe))
    {
      wsprintf(szBuf, "file not found: %s", szSeaExe);
      PrintError(szBuf, ERROR_CODE_HIDE);
      exit(1);
    }

    if(GetModuleFileName(NULL, szAppName, sizeof(szAppName)) == 0L)
    {
      PrintError("GetModuleFileName() failed", ERROR_CODE_SHOW);
      exit(1);
    }
    if(!FileExists(szAppName))
    {
      wsprintf(szBuf, "File not found: %s", szAppName);
      PrintError(szBuf, ERROR_CODE_HIDE);
      exit(1);
    }

    if(CopyFile(szAppName, szNsZipName, FALSE) == FALSE)
    {
      wsprintf(szBuf, "Error creating %s", szNsZipName);
      PrintError(szBuf, ERROR_CODE_SHOW);
      exit(1);
    }

    hExe = InitResource(szNsZipName);
    AddFile(hExe, szSeaExe);
    DeInitResource(hExe);
    return(0);
  }
  else
  {
    /* The first parameter is not "-g".  Assume that it's the name of the
     * self-extracting .exe to be saved as.  So lets create it only if it does not exist. */

    if(!FileExists(szSeaExe))
    {
      EnumResourceNames(NULL, "FILE", (ENUMRESNAMEPROC)ExtractFilesProc, (LONG)szSeaExe);

      // if still does not exist, copy nsinstall.exe to szSeaExe
      if(!FileExists(szSeaExe))
      {
        if(FileExists(DEFAULT_SEA_FILE))
          CopyFile(DEFAULT_SEA_FILE, szSeaExe, FALSE);
        else
        {
          wsprintf(szBuf, "file not found: %s", DEFAULT_SEA_FILE);
          PrintError(szBuf, ERROR_CODE_HIDE);
          exit(1);
        }
      }
    }

    if(!FileExists(szSeaExe))
    {
      wsprintf(szBuf, "file not found: %s", szSeaExe);
      PrintError(szBuf, ERROR_CODE_HIDE);
      exit(1);
    }
  }

  hExe = InitResource(szSeaExe);
  for(i = 1; i < iArgC; i++)
  {
    GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
    GetFullPathName(szArgVBuf, sizeof(szFileFullPath), szFileFullPath, NULL);
    hFindFile = FindFirstFile(szFileFullPath, &findFileData);

    if(hFindFile == INVALID_HANDLE_VALUE)
    {
      wsprintf(szBuf, "file not found: %s", szArgVBuf);
      PrintError(szBuf, ERROR_CODE_HIDE);
      exit(1);
    }

    do
    {
      char szFile[MAX_BUF];

      if(!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      {
        // We need to pass to AddFile() whatever kind of path we were passed,
        // e.g. simple, relative, full
        strcpy(szFile, szFileFullPath);
        strcpy(strrchr(szFile, '\\') + 1, findFileData.cFileName);
        AddFile(hExe, szFile);
      }
    } while(FindNextFile(hFindFile, &findFileData));

    FindClose(hFindFile);
  }

  DeInitResource(hExe);
  return(0);
}

