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

#define INCL_GPI
#define INCL_WIN
#define INCL_PM
#define INCL_DOSERRORS
#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#define INCL_DOSNLS

#include <os2.h>
#include <stdio.h>
#include <io.h>
#include <stdlib.h>
#include <ctype.h>
#include "nsztool.h"
#include "zlib.h"
#include "math.h"

#define DEFAULT_SEA_FILE "nsinstall.exe"

//Global Variables
HAB	hab;		//Anchor block handle



/* Function to show the usage for this application */
void ShowUsage(char *name)
{
    char szBuf[MAX_BUF];

    sprintf(szBuf, "Usage: %s <output sea name> <files [file1] [file2]...>\n", name);
    strcat(szBuf, "\n");
    strcat(szBuf, "    output sea name: name to use for the self-extracting executable\n");
    strcat(szBuf, "    files: one or more files to add to the self-extracing executable\n");
	WinMessageBox(HWND_DESKTOP, NULL, szBuf, "Usage", NULL, MB_OK);
}

/* Function to print error message with/without error code */
void PrintError(PSZ szMsg, ULONG dwErrorCodeSH)
{
  ULONG		ulErrorClass = 0,
				ulErrorAction = 0,             /* Action warranted by error   */
				ulErrorLoc    = 0;
  APIRET	rc = NO_ERROR;
  ULONG dwErr;
  char  szErrorString[MAX_BUF];

  rc = DosError(FERR_DISABLEHARDERR);

  if(dwErrorCodeSH == ERROR_CODE_SHOW)
  {
    dwErr = WinGetLastError(hab); 
    sprintf(szErrorString, "%d : %s", dwErr, szMsg);
  }
  else
    sprintf(szErrorString, "%s", szMsg);

  WinMessageBox(HWND_DESKTOP, NULL, szErrorString, "Usage", NULL, MB_ICONEXCLAMATION);
}

/* Function to remove quotes from a string */
void RemoveQuotes(PSZ lpszSrc, PSZ lpszDest, int iDestSize)
{
  char *lpszBegin;

  if(strlen(lpszSrc) > iDestSize)
    return;

  if(*lpszSrc == '\"')
    lpszBegin = &lpszSrc[1];
  else
    lpszBegin = lpszSrc;

  strcpy(lpszDest, lpszBegin);

  if(lpszDest[strlen(lpszDest) - 1] == '\"')
    lpszDest[strlen(lpszDest) - 1] = '\0';
}

/* Function to remove the last backslash from a path string */
void RemoveBackSlash(PSZ szInput)
{
  int   iCounter;
  ULONG dwInputLen;

  if(szInput != NULL)
  {
    dwInputLen = strlen(szInput);

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
void AppendBackSlash(PSZ szInput, ULONG dwInputSize)
{
  if(szInput != NULL)
  {
    if(szInput[strlen(szInput) - 1] != '\\')
    {
      if(((ULONG)strlen(szInput) + 1) < dwInputSize)
      {
        strcat(szInput, "\\");
      }
    }
  }
}

/* Function to parse a path string for one of three parts of a path:
 *   Filename only
 *   Path only
 *   drive only */
void ParsePath(PSZ szInput, PSZ szOutput, ULONG dwOutputSize, ULONG dwType)
{
  int   iCounter;
  ULONG dwCounter;
  ULONG dwInputLen;
  BOOL  bFound;

  if((szInput != NULL) && (szOutput != NULL))
  {
    bFound        = FALSE;
    dwInputLen    = strlen(szInput);
    memset(szOutput, 0, dwOutputSize);

    if(dwInputLen < dwOutputSize)
    {
      switch(dwType)
      {
        case PP_FILENAME_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              strcpy(szOutput, &szInput[iCounter + 1]);
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            strcpy(szOutput, szInput);

          break;

        case PP_PATH_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              strcpy(szOutput, szInput);
              szOutput[iCounter + 1] = '\0';
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            strcpy(szOutput, szInput);

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
APIRET FileExists(PSZ szFile)
{
  APIRET				rc = NO_ERROR;
  FILEFINDBUF3	fdFile;
  ULONG				ulResultBufLen = sizeof(FILEFINDBUF3);


  if((rc = DosQueryPathInfo(szFile, FIL_STANDARD, &fdFile, ulResultBufLen) != NO_ERROR))
  {
    return(FALSE);
  }
  else
  {
    return(rc);
  }
}

/* Function to locate the first non space character in a string,
 * and return a pointer to it. */
PSZ GetFirstNonSpace(PSZ lpszString)
{
  int   i;
  int   iStrLength;

  iStrLength = strlen(lpszString);

  for(i = 0; i < iStrLength; i++)
  {
    if(!isspace(lpszString[i]))
      return(&lpszString[i]);
  }

  return(NULL);
}

/* Function to return the argument count given a command line input
 * format string */
int GetArgC(PSZ lpszCommandLine)
{
  int   i;
  int   iArgCount;
  int   iStrLength;
  PSZ lpszBeginStr;
  BOOL  bFoundQuote;
  BOOL  bFoundSpace;

  iArgCount    = 0;
  lpszBeginStr = GetFirstNonSpace(lpszCommandLine);

  if(lpszBeginStr == NULL)
    return(iArgCount);

  iStrLength   = strlen(lpszBeginStr);
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
PSZ GetArgV(PSZ lpszCommandLine, int iIndex, PSZ lpszDest, int iDestSize)
{
  int   i;
  int   j;
  int   iArgCount;
  int   iStrLength;
  PSZ lpszBeginStr;
  PSZ lpszDestTemp;
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

  memset(lpszDest, 0, iDestSize);
  iStrLength    = strlen(lpszBeginStr);
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
void AddFile(LHANDLE hExe, PSZ lpszFile)
{
  char        szBuf[MAX_BUF];
  char        szResourceName[MAX_BUF];
  LHANDLE      hInputFile;
  ULONG       dwBytesRead;
  ULONG       dwFileSize;
  ULONG       dwFileSizeCmp;
  ULONG		ulAction       = 0;
  Bytef      *lpBuf;
  Bytef      *lpBufCmp;
  APIRET	rc = NO_ERROR;

  FILESTATUS3	fsts3FileInfo = {{0}};
  ULONG				ulBufSize     = sizeof(FILESTATUS3);

  if(!hExe)
    exit(1);

  ParsePath(lpszFile, szResourceName, sizeof(szResourceName), PP_FILENAME_ONLY);
  strupr(szResourceName);
  rc = DosOpen(lpszFile, &hInputFile, &ulAction, 0, FILE_NORMAL, 
			OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS, 
			OPEN_SHARE_DENYWRITE|OPEN_ACCESS_READONLY, 0);

  if(hInputFile == ERROR_INVALID_HANDLE)
  {
    PrintError("CreateFile() failed", ERROR_CODE_SHOW);
    exit(1);
  }

  //dwFileSize    = GetFileSize(hInputFile, NULL);
  rc = DosQueryFileInfo(hInputFile,   /* Handle of file                  */
                          FIL_STANDARD,  /* Request standard (Level 1) info */
                          &fsts3FileInfo, /* Buffer for file information  */
                          ulBufSize);    /* Size of buffer                  */
  dwFileSize = fsts3FileInfo.cbFile;

  dwFileSizeCmp = ((long)ceil(dwFileSize * 0.001)) + dwFileSize + 12;
  lpBuf         = (Bytef *)malloc(dwFileSize);
  lpBufCmp      = (Byte )malloc(dwFileSizeCmp + (sizeof(ULONG) * 2));
  if((lpBuf == NULL) || (lpBufCmp == NULL))
  {
    PrintError("Out of memory", ERROR_CODE_HIDE);
    exit(1);
  }

  DosRead(hInputFile, lpBuf, dwFileSize, &dwBytesRead);
  if(dwBytesRead != dwFileSize)
  {
    sprintf(szBuf, "Error reading file: %s", lpszFile);
    PrintError(szBuf, ERROR_CODE_HIDE);
    exit(1);
  }

  if(compress((lpBufCmp + (sizeof(ULONG) * 2)), &dwFileSizeCmp, (const Bytef*)lpBuf, dwFileSize) != Z_OK)
  {
    PrintError("Error occurred during compression of archives!", ERROR_CODE_HIDE);
    exit(1);
  }

  *(PULONG)lpBufCmp = dwFileSizeCmp;
  *(PULONG)(lpBufCmp + sizeof(ULONG)) = dwFileSize;

//  if(!UpdateResource(hExe, "FILE", szResourceName, 850,
//                     lpBufCmp, dwFileSizeCmp + (sizeof(ULONG) * 2)))
  {
    PrintError("UpdateResource() failed", ERROR_CODE_SHOW);
    exit(1);
  }

  free(lpBuf);
  if(!DosClose(hInputFile))
  {
    PrintError("DosClose() failed", ERROR_CODE_SHOW);
    exit(1);
  }
}

/* Function to extract a resourced file from a .exe file.
 * It also uncompresss the file after extracting it. */
BOOL APIENTRY ExtractFilesProc(LHANDLE hModule, PSZ lpszType, PSZ lpszName, LONG lParam)
{
  char    szBuf[MAX_BUF];
  UCHAR		LoadError[MAX_BUF];
  PPVOID   hResInfo;
  PSZ   lpszSeaExe = (PSZ)lParam;
  LHANDLE  hFile;
  Bytef  *lpBytes;
  Bytef  *lpBytesUnCmp;
  LHANDLE hGlobal;
  ULONG	ulAction = 0;
  APIRET	rc = NO_ERROR;

  // Extract the file
  //hResInfo = FindResource(hModule, lpszName, lpszType);
  rc = DosLoadModule(LoadError, sizeof(LoadError), lpszName, &hModule);
  rc = DosGetResource(hModule, lpszType, lpszName, hResInfo);

  //hGlobal  = LoadResource(hModule, hResInfo);
  //lpBytes  = (Bytef *)LockResource(hGlobal);
  lpBytes  = (Bytef *)hResInfo;

  // Create the file
  rc = DosOpen(lpszSeaExe, &hFile, &ulAction, 0, FILE_NORMAL, OPEN_ACTION_CREATE_IF_NEW, OPEN_ACCESS_WRITEONLY, 0);

  if(hFile != ERROR_INVALID_HANDLE)
  {
    ULONG	dwSize;
    uLongf	dwSizeUnCmp;

    lpBytesUnCmp = (Bytef *)malloc((*(PULONG)(lpBytes + sizeof(ULONG))) + 1);
    dwSizeUnCmp  = *(PULONG)(lpBytes + sizeof(ULONG));

    // Copy the file. The first ULONG specifies the size of the file
    dwSize = *(PULONG)lpBytes;
    lpBytes += (sizeof(ULONG) * 2);

    if(uncompress(lpBytesUnCmp, &dwSizeUnCmp, lpBytes, dwSize) != Z_OK)
    {
      sprintf(szBuf, "Error occurred during uncompression of %s: ", lpszSeaExe);
      PrintError(szBuf, ERROR_CODE_HIDE);
      DosClose(hFile);
      DosFreeResource(hResInfo);
      return FALSE;
    }

    while(dwSizeUnCmp > 0)
    {
      ULONG dwBytesToWrite;
      ULONG dwBytesWritten;

      dwBytesToWrite = dwSizeUnCmp > 4096 ? 4096 : dwSizeUnCmp;
      if(!DosWrite(hFile, lpBytesUnCmp, dwBytesToWrite, &dwBytesWritten))
      {
        sprintf(szBuf, "%s: write error", lpszSeaExe);
		WinMessageBox(HWND_DESKTOP, NULL, szBuf, "Error", NULL, MB_OK|MB_ICONEXCLAMATION);
        DosClose(hFile);
        DosFreeResource(hResInfo);
        return FALSE;
      }

      dwSizeUnCmp -= dwBytesWritten;
      lpBytesUnCmp += dwBytesWritten;
    }

    DosClose(hFile);
  }

  // Release the resource
  DosFreeResource(hResInfo);

  return TRUE;  // keep enumerating
}

LHANDLE InitResource(PSZ lpszSeaExe)
{
  LHANDLE hExe;

  if((DosQueryModuleHandle(lpszSeaExe, hExe)) != NO_ERROR)
  {
    ULONG dwErr;

    dwErr = WinGetLastError(hab);
    if(dwErr == ERROR_CALL_NOT_IMPLEMENTED)
    {
	  WinMessageBox(HWND_DESKTOP, NULL, "This application does not run under this OS", NULL, NULL, MB_ICONEXCLAMATION);
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

void DeInitResource(LHANDLE hExe)
{
  if(DosFreeModule(hExe) != NO_ERROR)
  {
    PrintError("EndUpdateResource() failed", ERROR_CODE_SHOW);
    exit(1);
  }
}

//HINSTANCE hInstance, HINSTANCE hPrevInstance, PSZ lpszCmdLine, int nCmdShow
int APIENTRY main(HOBJECT hInstance, HOBJECT hPrevInstance, PSZ lpszCmdLine, int nCmdShow)
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
  HDIR				hdirFindHandle = HDIR_CREATE;
  FILEFINDBUF3	findFileData;
  ULONG				ulResultBufLen = sizeof(FILEFINDBUF3);
  ULONG				ulFindCount    = 1;
  LHANDLE			hExe;
  LHANDLE			hFindFile;
  BOOL				bSelfUpdate;
  APIRET			rc = NO_ERROR;

  hab = WinInitialize(0);

  if(DosQueryModuleName(NULL, sizeof(szAppName), szAppName) != NO_ERROR)
  {
    PrintError("DosQueryModuleName() failed", ERROR_CODE_SHOW);
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
  sprintf(szOutputStr, "ArgC: %d\n", iArgC);
  for(i = 0; i < iArgC; i++)
  {
    GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
    itoa(i, szBuf, 10);
    strcat(szOutputStr, "    ");
    strcat(szOutputStr, szBuf);
    strcat(szOutputStr, ": ");
    strcat(szOutputStr, szArgVBuf);
    strcat(szOutputStr, "\n");
  }
  WinMessageBox(HWND_DESKTOP, NULL, szOutputStr, "Output", NULL, MB_OK);

#endif

  /* Get the first parameter */
  GetArgV(lpszCmdLine, 0, szSeaExe, sizeof(szSeaExe));
  if(strcmpi(szSeaExe, "-g") == 0)
  {
    /* The first parameter is "-g".
     * Create a new nszip that contains nsinstall.exe in itself */

    GetArgV(lpszCmdLine, 1, szNsZipName, sizeof(szNsZipName));
    GetArgV(lpszCmdLine, 2, szSeaExe, sizeof(szSeaExe));
    if(!FileExists(szSeaExe))
    {
      sprintf(szBuf, "file not found: %s", szSeaExe);
      PrintError(szBuf, ERROR_CODE_HIDE);
      exit(1);
    }

    if(DosQueryModuleName(NULL, sizeof(szAppName), szAppName) != NO_ERROR)
    {
      PrintError("DosQueryModuleName() failed", ERROR_CODE_SHOW);
      exit(1);
    }
    if(!FileExists(szAppName))
    {
      sprintf(szBuf, "File not found: %s", szAppName);
      PrintError(szBuf, ERROR_CODE_HIDE);
      exit(1);
    }

    if(DosCopy(szAppName, szNsZipName, DCPY_EXISTING) != NO_ERROR)
    {
      sprintf(szBuf, "Error creating %s", szNsZipName);
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
      //EnumResourceNames(NULL, "FILE", ExtractFilesProc, (LONG)szSeaExe);

      // if still does not exist, copy nsinstall.exe to szSeaExe
      if(!FileExists(szSeaExe))
      {
        if(FileExists(DEFAULT_SEA_FILE))
          DosCopy(DEFAULT_SEA_FILE, szSeaExe, DCPY_EXISTING);
        else
        {
          sprintf(szBuf, "file not found: %s", DEFAULT_SEA_FILE);
          PrintError(szBuf, ERROR_CODE_HIDE);
          exit(1);
        }
      }
    }

    if(!FileExists(szSeaExe))
    {
      sprintf(szBuf, "file not found: %s", szSeaExe);
      PrintError(szBuf, ERROR_CODE_HIDE);
      exit(1);
    }
  }

  hExe = InitResource(szSeaExe);
  for(i = 1; i < iArgC; i++)
  {
    GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
    DosQueryPathInfo(szArgVBuf, FIL_STANDARD, &findFileData, sizeof(findFileData));
    strcpy(szFileFullPath, findFileData.achName, sizeof(findFileData.achName));
   	rc = DosFindFirst(szFileFullPath, &hdirFindHandle, FILE_NORMAL, 
					&hFindFile, ulResultBufLen, &ulFindCount, FIL_STANDARD);


    if(hFindFile == ERROR_INVALID_HANDLE)
    {
      sprintf(szBuf, "file not found: %s", szArgVBuf);
      PrintError(szBuf, ERROR_CODE_HIDE);
      exit(1);
    }

    do
    {
      char szFile[MAX_BUF];

      if(!(findFileData.attrFile & FILE_DIRECTORY))
      {
        // We need to pass to AddFile() whatever kind of path we were passed,
        // e.g. simple, relative, full
        strcpy(szFile, szFileFullPath);
        strcpy(strrchr(szFile, '\\') + 1, findFileData.achName);
        AddFile(hExe, szFile);
      }
    } while(DosFindNext(hFindFile, &findFileData, ulResultBufLen, &ulFindCount) != NO_ERROR);

    DosFindClose(hFindFile);
  }

  DeInitResource(hExe);
  return(0);
}

