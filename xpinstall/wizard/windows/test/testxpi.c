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

#include "testxpi.h"

//#define SSU_DEBUG

HANDLE hXPIStubInst;

/* Function to show the usage for this application */
void ShowUsage(char *name)
{
    char szBuf[MAX_BUF];

    wsprintf(szBuf, "Usage: %s <output sea name> <files [file1] [file2]...>\n", name);
    lstrcat(szBuf, "\n");
    lstrcat(szBuf, "    output sea name: name to use for the self-extracting executable\n");
    lstrcat(szBuf, "    files: one or more files to add to the self-extracing executable\n");
}

/* Function to print error message with/without error code */
void PrintError(LPSTR szMsg, DWORD dwErrorCodeSH, int iExitCode)
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

  printf("%s\n", szErrorString);
  printf("exit code: %d\n", iExitCode);
  if(iExitCode == 0)
    printf("\n** Success! **\n");

  DeInitializeXPIStub();
  exit(iExitCode);
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
    PrintError("Out of memory", ERROR_CODE_HIDE, 1);

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

int main(void)
{
  HRESULT           hrResult;
  char              szBuf[MAX_BUF];
  char              szAppName[MAX_BUF];
  char              szAppPath[MAX_BUF];
  char              *listArchive[] = {"e:\\cmonkey\\mozilla\\dist\\win32_d.obj\\bin\\test1.xpi",
                                      "e:\\cmonkey\\mozilla\\dist\\win32_d.obj\\bin\\test2.xpi",
                                      "\0"};

  if(GetModuleFileName(NULL, szBuf, sizeof(szBuf)) == 0L)
    PrintError("GetModuleFileName() failed", ERROR_CODE_SHOW, 1);

  ParsePath(szBuf, szAppPath, sizeof(szAppPath), PP_PATH_ONLY);
  ParsePath(szBuf, szAppName, sizeof(szAppName), PP_FILENAME_ONLY);

  hrResult = SmartUpdateJars(szAppName, szAppPath, listArchive);

  if(hrResult == 999)
    wsprintf(szBuf, "%s done\nReboot required\n", szAppName);
  else
    wsprintf(szBuf, "%s done\n", szAppName);

  PrintError(szBuf, ERROR_CODE_SHOW, hrResult);
  return(0);
}

