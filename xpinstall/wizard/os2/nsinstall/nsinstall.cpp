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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Troy Chevalier <troy@netscape.com>
 *     Sean Su <ssu@netscape.com>
 *     IBM Corp.
 */

#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#define INCL_DOSMISC
#define INCL_DOSMODULEMGR
#define INCL_DOSPROCESS
#define INCL_DOSRESOURCES
#define INCL_DOS
#define INCL_WIN
#define INCL_PM
#define INCL_WINRECTANGLES
#define INCL_WINSHELLDATA
#define INCL_WINSYS
#define INCL_WINACCELERATORS
#define INCL_WINDESKTOP

#include <os2.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unidef.h>
#include "resource.h"
#include "zlib.h"

#define BAR_MARGIN    	1
#define BAR_SPACING   	0
#define BAR_WIDTH		6
#define MAX_BUF			4096
#define WIZ_TEMP_DIR  	"ns_temp"

/* Mode of Setup to run in */
#define NORMAL				0
#define SILENT				1
#define AUTO					2

/* PP: Parse Path */
#define PP_FILENAME_ONLY                1
#define PP_PATH_ONLY                    2
#define PP_ROOT_ONLY                    3

#define CLASS_NAME_SETUP_DLG            "MozillaSetupDlg"

char      szTitle[MAX_BUF];
char      szCmdLineToSetup[MAX_BUF];
BOOL      gbUncompressOnly;
ULONG     dwMode;
HWND hInst;

/////////////////////////////////////////////////////////////////////////////
// Global Declarations

static ULONG	nTotalBytes = 0;  // sum of all the FILE resources

HAB hab;

struct ExtractFilesDlgInfo {
	HWND	hWndDlg;
	int		nMaxBars;	// maximum number of bars that can be displayed
	int		nBars;		// current number of bars to display
} dlgInfo;

/////////////////////////////////////////////////////////////////////////////
// Utility Functions

// This function is similar to GetFullPathName except instead of
// using the current drive and directory it uses the path of the
// directory designated for temporary files
static BOOL
GetFullTempPathName(PSZ lpszFileName, ULONG dwBufferLength, PSZ lpszBuffer)
{
	ULONG	dwLen;
	APIRET	rc = NO_ERROR;

	//dwLen = GetTempPath(dwBufferLength, lpszBuffer);
	rc = DosScanEnv("TMP", (PCSZ *)lpszBuffer);
	dwLen = sizeof(lpszBuffer);
	if (lpszBuffer[dwLen - 1] != '\\')
		strcat(lpszBuffer, "\\");
	strcat(lpszBuffer, WIZ_TEMP_DIR);

  dwLen = strlen(lpszBuffer);
	if (lpszBuffer[dwLen - 1] != '\\')
		strcat(lpszBuffer, "\\");
	strcat(lpszBuffer, lpszFileName);

	return TRUE;
}

/* Function to remove quotes from a string */
void RemoveQuotes(PSZ lpszSrc, PSZ  lpszDest, int iDestSize)
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

/* Function to locate the first non space character in a string,
 * and return a pointer to it. */
PSZ  GetFirstNonSpace(PSZ  lpszString)
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
int GetArgC(PSZ  lpszCommandLine)
{
  int   i;
  int   iArgCount;
  int   iStrLength;
  PSZ  lpszBeginStr;
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
PCH  GetArgV(PSZ  lpszCommandLine, int iIndex, PSZ  lpszDest, int iDestSize)
{
  int   i;
  int   j;
  int   iArgCount;
  int   iStrLength;
  PSZ  lpszBeginStr;
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
    WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, "Out of memory", NULL, NULL, MB_OK | MB_ICONEXCLAMATION);
    DosExit(EXIT_PROCESS, 1);
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

// this function appends a backslash at the end of a string,
// if one does not already exists.
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

APIRET CreateDirectoriesAll(char* szPath)
{
  int     i;
  int     iLen = strlen(szPath);
  char    szCreatePath[MAX_BUF];
  APIRET hrResult;

  memset(szCreatePath, 0, MAX_BUF);
  memcpy(szCreatePath, szPath, iLen);
  for(i = 0; i < iLen; i++)
  {
    if((iLen > 1) &&
      ((i != 0) && ((szPath[i] == '\\') || (szPath[i] == '/'))) &&
      (!((szPath[0] == '\\') && (i == 1)) && !((szPath[1] == ':') && (i == 2))))
    {
      szCreatePath[i] = '\0';
      hrResult        = DosCreateDir(szCreatePath, NULL);
      szCreatePath[i] = szPath[i];
    }
  }
  return(hrResult);
}

// This function removes a directory and its subdirectories
APIRET DirectoryRemove(PSZ szDestination, BOOL bRemoveSubdirs)
{
  HDIR				hdirFindHandle = HDIR_CREATE;
  HFILE				hFile;
  FILEFINDBUF3	fdFile;
  ULONG				ulResultBufLen = sizeof(FILEFINDBUF3);
  ULONG				ulFindCount    = 1;
  APIRET			rc = NO_ERROR;
  char				szDestTemp[MAX_BUF];
  BOOL	 			bFound;

  if(DosQueryPathInfo(szDestination, FIL_STANDARD, &fdFile, ulResultBufLen) != NO_ERROR)
    return(0);

  if(bRemoveSubdirs == TRUE)
  {
    strcpy(szDestTemp, szDestination);
    AppendBackSlash(szDestTemp, sizeof(szDestTemp));
    strcat(szDestTemp, "*");

    bFound = TRUE;
    rc = DosFindFirst(szDestTemp, &hdirFindHandle, FILE_NORMAL, 
					&fdFile, ulResultBufLen, &ulFindCount, FIL_STANDARD);
    while((rc != ERROR_INVALID_HANDLE) && (bFound == TRUE))
    {
      if((strcmpi(fdFile.achName, ".") != 0) && (strcmpi(fdFile.achName, "..") != 0))
      {
        /* create full path */
        strcpy(szDestTemp, szDestination);
        AppendBackSlash(szDestTemp, sizeof(szDestTemp));
        strcat(szDestTemp, fdFile.achName);

        if(fdFile.attrFile & MUST_HAVE_DIRECTORY)
        {
          DirectoryRemove(szDestTemp, bRemoveSubdirs);
        }
        else
        {
          DosDelete(szDestTemp);
        }
      }

      rc = DosFindNext(hdirFindHandle, &fdFile, ulResultBufLen, &ulFindCount);
	  if(rc != NO_ERROR && rc != ERROR_NO_MORE_FILES)
			bFound = FALSE;
    }
	DosFindClose(hdirFindHandle);
    DosClose(hFile);
  }
  
  DosDeleteDir(szDestination);
  return(0);
}

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

void ParsePath(PSZ szInput, PSZ szOutput, ULONG dwOutputSize, ULONG dwType)
{
  int   iCounter;
  ULONG dwCounter;
  ULONG dwInputLen;
  BOOL  bFound;

  if((szInput != NULL) && (szOutput != NULL))
  {
    bFound        = TRUE;
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

void ParseCommandLine(PSZ lpszCmdLine)
{
  char  szArgVBuf[MAX_BUF];
  int   i;
  int   iArgC;

  memset(szCmdLineToSetup, 0, MAX_BUF);
  dwMode = NORMAL;
  gbUncompressOnly = FALSE;
  iArgC  = GetArgC(lpszCmdLine);
  i      = 0;
  while(i < iArgC)
  {
    GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
    if((strcmpi(szArgVBuf, "-ms") == 0) || (strcmpi(szArgVBuf, "/ms") == 0))
    {
      dwMode = SILENT;
    }
    else if((strcmpi(szArgVBuf, "-u") == 0) || (strcmpi(szArgVBuf, "/u") == 0))
    {
      gbUncompressOnly = TRUE;
    }

    ++i;
  }

  strcpy(szCmdLineToSetup, " ");
  strcat(szCmdLineToSetup, lpszCmdLine);
}

// Centers the specified window over the desktop. Assumes the window is
// smaller both horizontally and vertically than the desktop
static void
CenterWindow(HWND hWndDlg)
{
    SWP		swp;
	int		iLeft, iTop;

    WinQueryWindowPos(hWndDlg, (PSWP)&swp);
	iLeft = (WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN) - swp.cx) / 2;
	iTop = (WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - swp.cy) / 2;

	WinSetWindowPos(hWndDlg, NULL, -1, -1, iLeft, iTop, SWP_MOVE);
        //SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}


/////////////////////////////////////////////////////////////////////////////
// Extract Files Dialog

// This routine processes windows messages that are in queue
void ProcessWindowsMessages()
{
  HACCEL	haccelAccel;
  PQMSG	pqmsg;
  HWND		hwndFilter;

  while(WinPeekMsg(hab, pqmsg, hwndFilter, 0, 0, PM_REMOVE))
  {
    //TranslateMessage(&msg);
	haccelAccel = WinQueryAccelTable(hab, hwndFilter);
	WinTranslateAccel(hab, hwndFilter, haccelAccel, pqmsg);
    WinDispatchMsg(hab, pqmsg);
  }
}

// This routine updates the status string in the extracting dialog
static void
SetStatusLine(PCSZ lpszStatus)
{
	HWND	hWndLabel;
    HWND    hWndChild;
    HENUM  henum;
    BOOL  fSuccess;
  if(dwMode != SILENT)
  {  
      henum = WinBeginEnumWindows(dlgInfo.hWndDlg);
      hWndChild = WinGetNextWindow(henum);
	  hWndLabel = WinEnumDlgItem(dlgInfo.hWndDlg, hWndChild, IDC_STATUS);
	  WinSetWindowText(hWndLabel, lpszStatus);
	  WinUpdateWindow(hWndLabel);
      fSuccess = WinEndEnumWindows (henum);
  }
}

// This routine will update the progress bar to the specified percentage
// (value between 0 and 100)
static void
UpdateProgressBar(unsigned value)
{
	int	nBars;

  if(dwMode != SILENT)
  {
    // Figure out how many bars should be displayed
    nBars = dlgInfo.nMaxBars * value / 100;

    // Only paint if we need to display more bars
    if (nBars > dlgInfo.nBars)
    {
      HENUM henum = WinBeginEnumWindows(dlgInfo.hWndDlg);
      HWND hWndChild = WinGetNextWindow(henum);

      HWND	hWndGauge = WinEnumDlgItem(dlgInfo.hWndDlg, hWndChild, IDC_GAUGE);
      RECTL	rect;

      // Update the gauge state before painting
      dlgInfo.nBars = nBars;

      // Only invalidate the part that needs updating
  	  WinQueryWindowRect(hWndGauge, &rect);
      WinInvalidateRect(hWndGauge, &rect, FALSE);
    
      // Update the whole extracting dialog. We do this because we don't
      // have a message loop to process WM_PAINT messages in case the
      // extracting dialog was exposed
      WinUpdateWindow(dlgInfo.hWndDlg);
      BOOL fSuccess = WinEndEnumWindows (henum);
    }
  }
}

// Window proc for dialog
BOOL APIENTRY
DialogProc(HWND hWndDlg, USHORT msg, MPARAM wParam, MPARAM lParam)
{
  if(dwMode != SILENT)
  {
    switch (msg) {
      case WM_INITDLG:
        // Center the dialog over the desktop
        CenterWindow(hWndDlg);
        return FALSE;

      case WM_COMMAND:
        WinDestroyWindow(hWndDlg);
        return TRUE;
    }
  }

	return FALSE;  // didn't handle the message
}

/////////////////////////////////////////////////////////////////////////////
// Resource Callback Functions

BOOL APIENTRY
DeleteTempFilesProc(LHANDLE hModule, PSZ lpszType, PSZ lpszName, LONG lParam)
{ 
	char	szTmpFile[CCHMAXPATH];

	// Get the path to the file in the temp directory
	GetFullTempPathName(lpszName, sizeof(szTmpFile), szTmpFile);

	// Delete the file
	DosDelete(szTmpFile);
	return TRUE;
}

BOOL APIENTRY
SizeOfResourcesProc(LHANDLE hModule, ULONG lpszType, ULONG lpszName, LONG lParam)
{
	PPVOID		hResInfo;
	APIRET		rc = NO_ERROR;
	ULONG		Size = 0;

	// Find the resource
	rc = DosGetResource(hModule, lpszType, lpszName, hResInfo);

#ifdef _DEBUG
	if (rc != NO_ERROR) {
		char	buf[512];

		vsprintf(buf, "Error '%d' when loading FILE resource: %s", GetLastError(), lpszName);
		WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, buf, szTitle, NULL, MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}
#endif

	// Add its size to the total size. Note that the return value is subject
	// to alignment rounding, but it's close enough for our purposes
	rc = DosQueryResourceSize(hModule, lpszType, lpszName, &Size);
	nTotalBytes += Size;

	// Release the resource
	DosFreeResource(hResInfo);
	return TRUE;  // keep enumerating
}
/*
BOOL APIENTRY
ExtractFilesProc(LHANDLE hModule, ULONG lpszType, PSZ lpszName, LONG lParam)
{
	char	szTmpFile[CCHMAXPATH];
	char	szArcLstFile[CCHMAXPATH];
	PPVOID	hResInfo;
	LHANDLE	hGlobal;
	Bytef	*lpBytes;
	Bytef	*lptr;
	Bytef	*lpBytesUnCmp;
	LHANDLE	hFile;
	char	szStatus[128];
	char	szText[4096];
	ULONG  ulAction = 0;
	APIRET	rc = NO_ERROR;
    HINI   hiniLstFile;
	
	// Update the UI
	WinLoadString(hab, hModule, IDS_STATUS_EXTRACTING, sizeof(szText), szText);
	vsprintf(szStatus, szText, lpszName);
	SetStatusLine(szStatus);

  if(gbUncompressOnly == TRUE)
    strcpy(szTmpFile, lpszName);
  else
  {
	  // Create a file in the temp directory
	  GetFullTempPathName(lpszName, sizeof(szTmpFile), szTmpFile);
    CreateDirectoriesAll(szTmpFile);
  }

	// Extract the file
	// XXX - Must get the ULONG idName
	rc = DosGetResource(hModule, lpszType, lpszName, hResInfo);
	hGlobal = LoadResource(hModule, hResInfo);
	lpBytes = (PBYTE)LockResource(hGlobal);

	// Create the file
	rc = DosOpen(szTmpFile, &hFile, &ulAction, 0, FILE_NORMAL, OPEN_ACTION_CREATE_IF_NEW, OPEN_ACCESS_WRITEONLY, 0);

	if (hFile != ERROR_INVALID_HANDLE) {
		ULONG	dwSize;
		ULONG	dwSizeUnCmp;
    	ULONG dwTemp;

	  GetFullTempPathName("Archive.lst", sizeof(szArcLstFile), szArcLstFile);
    //WritePrivateProfileString("Archives", lpszName, "TRUE", szArcLstFile);
    hiniLstFile = PrfOpenProfile((HAB)0, szArcLstFile);
	PrfWriteProfileString(hiniLstFile, (PSZ)"Archives", (PSZ)lpszName, "TRUE");
    PrfCloseProfile(hini);

    lptr = (Bytef *)malloc((*(PULONG)(lpBytes + sizeof(ULONG))) + 1);
    if(!lptr)
    {
      char szBuf[512];

  	  WinLoadString((HAB)0, hInst, IDS_ERROR_OUT_OF_MEMORY, sizeof(szBuf), szBuf);
	  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, szBuf, NULL, NULL, MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
    }

    lpBytesUnCmp = lptr;
    dwSizeUnCmp  = *(PULONG)(lpBytes + sizeof(ULONG));

		// Copy the file. The first ULONG specifies the size of the file
		dwSize = *(PULONG)lpBytes;
		lpBytes += (sizeof(ULONG) * 2);

    dwTemp = uncompress(lpBytesUnCmp, &dwSizeUnCmp, lpBytes, dwSize);

    while (dwSizeUnCmp > 0)
    {
			ULONG	dwBytesToWrite, dwBytesWritten;

      ProcessWindowsMessages();

			dwBytesToWrite = dwSizeUnCmp > 4096 ? 4096 : dwSizeUnCmp;
			if (!DosWrite(hFile, lpBytesUnCmp, dwBytesToWrite, &dwBytesWritten))
      {
				char szBuf[512];

   		WinLoadString((HAB)0, hInst, IDS_STATUS_EXTRACTING, sizeof(szText), szText);
		vsprintf(szBuf, szText, szTmpFile);
		WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, szBuf, szTitle, NULL, MB_OK | MB_ICONEXCLAMATION);
		DosFreeResource(hResInfo);

        if(lptr)
          free(lptr);

				return FALSE;
			}

			dwSizeUnCmp -= dwBytesWritten;
			lpBytesUnCmp += dwBytesWritten;

			// Update the UI to reflect the total number of bytes written
			static ULONG	nBytesWritten = 0;

			nBytesWritten += dwBytesWritten;
			UpdateProgressBar(nBytesWritten * 100 / nTotalBytes);
		}

		DosClose(hFile);
	}

	// Release the resource
	DosFreeResource(hResInfo);
  if(lptr)
    free(lptr);

	return TRUE;  // keep enumerating
}
*/
/////////////////////////////////////////////////////////////////////////////
// Progress bar

// Draws a recessed border around the gauge
static void
DrawGaugeBorder(HWND hWnd)
{
   	HPS		hps;
	HDC		hDC = WinQueryWindowDC(hWnd);
    SWP      swp;

	RECTL rect;
	POINTL ptl;

    WinQueryWindowPos( hWnd, (PSWP)&swp );
	hps = WinBeginPaint(hWnd, (HPS) NULL, &rect);

	rect.xLeft = 0;
	rect.xRight = swp.cx - 1;
	rect.yBottom = 0;
	rect.yTop = swp.cy - 1;

	ptl.x = 0;
	ptl.y = swp.cy - 1;
	GpiMove(hps, &ptl);
	WinFillRect(hps, &rect, SYSCLR_SHADOW);

	// Draw a white line segment

	rect.xLeft = swp.cx - 1;
	rect.xRight = swp.cx - 1;
	rect.yBottom = 0;
	rect.yTop = swp.cy - 1;

	ptl.x = 0;
	ptl.y = swp.cy - 1;
	GpiMove(hps, &ptl);
	WinFillRect(hps, &rect, CLR_WHITE);

	WinReleasePS(hps);
}

// Draws the blue progress bar
static void
DrawProgressBar(HWND hWnd)
{
	HAB			hab;
	HPS			hps;
	RECTL		rect;
	RECTL		rect1;

	hps = WinBeginPaint(hWnd, (HPS) NULL, &rect);
	rect1.xLeft = rect.xLeft;
	rect1.xRight = rect.xRight;
	rect1.yBottom = rect.yBottom;
	rect1.yTop = rect.yTop;

	WinQueryWindowRect(hWnd, &rect);

	rect.xLeft = rect.yTop = BAR_MARGIN;
	rect.yBottom -= BAR_MARGIN;
	rect.xRight = rect.xLeft + BAR_WIDTH;

	for (int i = 0; i < dlgInfo.nBars; i++) {
		RECTL	dest;

		if (WinIntersectRect(hab, &dest, &rect1, &rect))
			WinFillRect(hps, &rect, CLR_BLUE);
		WinOffsetRect(hab, &rect, BAR_WIDTH + BAR_SPACING, 0);
	}

	WinEndPaint(hps);
}

// Adjusts the width of the gauge based on the maximum number of bars
static void
SizeToFitGauge(HWND hWnd)
{
    SWP		swp;
	int		cx;

	WinQueryWindowPos(hWnd, (PSWP)&swp); 

	// Size the width to fit
	cx = 2 * WinQuerySysValue(HWND_DESKTOP, SV_CXSIZEBORDER) + 2 * BAR_MARGIN +
		dlgInfo.nMaxBars * BAR_WIDTH + (dlgInfo.nMaxBars - 1) * BAR_SPACING;

	WinSetWindowPos(hWnd, NULL, -1, -1, cx, swp.cy, SWP_SIZE);
}

// Window proc for gauge
MRESULT EXPENTRY
GaugeWndProc(HWND hWnd, USHORT msg, MPARAM wParam, MPARAM lParam)
{
	ULONG	dwStyle;
	RECTL	rect;
/*
	switch (msg) {
		case WM_NCCREATE:
			dwStyle = WinQueryWindowULong(hWnd, GWL_STYLE);
			WinSetWindowULong(hWnd, GWL_STYLE, dwStyle | WS_BORDER);
			return TRUE;

		case WM_CREATE:
			// Figure out the maximum number of bars that can be displayed
			WinQueryWindowRect(hWnd, &rect);
			dlgInfo.nBars = 0;
			dlgInfo.nMaxBars = (rect.xRight - rect.xLeft - 2 * BAR_MARGIN + BAR_SPACING) /
				(BAR_WIDTH + BAR_SPACING);

			// Size the gauge to exactly fit the maximum number of bars
			SizeToFitGauge(hWnd);
			return TRUE;

		case WM_NCPAINT:
			DrawGaugeBorder(hWnd);
			return TRUE;

		case WM_PAINT:
			DrawProgressBar(hWnd);
			return TRUE;
	}
*/
	return WinDefWindowProc(hWnd, msg, wParam, lParam);
}

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

/////////////////////////////////////////////////////////////////////////////
// WinMain

static BOOL
RunInstaller()
{
  PIB					pi;
  STARTDATA	sti;
  char                szCmdLine[MAX_BUF];
  char                szSetupFile[MAX_BUF];
  char                szUninstallFile[MAX_BUF];
  char                szArcLstFile[MAX_BUF];
  BOOL               bRet;
  char                szText[256];
  PSZ				szTempPath = "";
  char                szTmp[CCHMAXPATH];
  char                szCurrentDirectory[CCHMAXPATH];
  char                szFilename[MAX_BUF];
  char                szBuf[MAX_BUF];
  ULONG				dwLen;

  UCHAR				uchLoadError[CCHMAXPATH] = {0};
  RESULTCODES	ChildRC           = {0};
  PID					pidChild          = 0;
  APIRET			rc                = NO_ERROR;
  HINI                 hiniLstFile;



  if(gbUncompressOnly == TRUE)
    return(TRUE);

  // Update UI
  UpdateProgressBar(100) ;
  WinLoadString(hab, hInst, IDS_STATUS_LAUNCHING_SETUP, sizeof(szText), szText);
  // XXX OS2TODO
  //SetStatusLine(szText);

  memset(&sti,0,sizeof(sti));
  sti.Length = sizeof(STARTDATA);

  //dwLen = GetTempPath(4096, szTempPath);

 
  rc = DosScanEnv("TMP", (PCSZ *)szTempPath);
  dwLen = sizeof(szTempPath);
  if (szTempPath[dwLen - 1] != '\\')
    strcat(szTempPath, "\\");
  strcat(szTempPath, WIZ_TEMP_DIR);

  // Setup program is in the directory specified for temporary files
  GetFullTempPathName("Archive.lst",   sizeof(szArcLstFile),    szArcLstFile);
  GetFullTempPathName("SETUP.EXE",     sizeof(szSetupFile),     szSetupFile);
  GetFullTempPathName("uninstall.exe", sizeof(szUninstallFile), szUninstallFile);

  hiniLstFile = PrfOpenProfile((HAB)0, szArcLstFile);
  PrfQueryProfileString(hiniLstFile, (PSZ)"Archives", (PSZ)"uninstall.exe", NULL, (PVOID)szBuf, (LONG)sizeof(szBuf));
  PrfCloseProfile(hiniLstFile);

  if((FileExists(szUninstallFile) != FALSE) && (*szBuf != '\0'))
  {
    strcpy(szCmdLine, szUninstallFile);
  }
  else
  {
    strcpy(szCmdLine, szSetupFile);
	
    DosQueryModuleName(NULL, sizeof(szBuf), szBuf);
    ParsePath(szBuf, szFilename, sizeof(szFilename), PP_FILENAME_ONLY);
    ParsePath(szBuf, szCurrentDirectory, sizeof(szCurrentDirectory), PP_PATH_ONLY);
    RemoveBackSlash(szCurrentDirectory);
   // GetShortPathName(szCurrentDirectory, szBuf, sizeof(szBuf));

    strcat(szCmdLine, " -a ");
    strcat(szCmdLine, szBuf);
    strcat(szCmdLine, " -n ");
    strcat(szCmdLine, szFilename);
  }

  if(szCmdLine != NULL)
    strcat(szCmdLine, szCmdLineToSetup);

  // Launch the installer
  //bRet = CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, 0, NULL, szTempPath, &sti, &pi);
  rc = DosExecPgm(sti.ObjectBuffer, sti.ObjectBuffLen, EXEC_ASYNCRESULT, sti.PgmInputs, sti.Environment, &ChildRC, szFilename);

  if (!bRet)
    return FALSE;

  //CloseHandle(pi.hThread);

  // Wait for the InstallShield UI to appear before taking down the dialog box
  DosSleep(3000);
  if(dwMode != SILENT)
  {
    WinDestroyWindow(dlgInfo.hWndDlg);
  }

  // Wait for the installer to complete
  rc = DosWaitChild(DCWA_PROCESS, DCWW_WAIT, &ChildRC, &pidChild, ChildRC.codeTerminate);

 // CloseHandle(pi.hProcess);


  // Delete the files from the temp directory
  //EnumResourceNames(NULL, "FILE", (ENUMRESNAMEPROC)DeleteTempFilesProc, 0);

  // delete archive.lst file in the temp directory
  GetFullTempPathName("Archive.lst", sizeof(szTmp), szTmp);
  DosDelete(szTmp);
  GetFullTempPathName("xpcom.ns", sizeof(szTmp), szTmp);
  DirectoryRemove(szTmp, TRUE);
  DirectoryRemove(szTempPath, FALSE);


  return TRUE;
}

//HINSTANCE hInstance, HINSTANCE hPrevInstance, PSZ lpCmdLine, int nCmdShow

int main(HOBJECT hInstance, HOBJECT hPrevInstance, PSZ lpCmdLine, int nCmdShow)
{
	HWND      hwndFW;

    hab = WinInitialize(0);
	hInst = hInstance;
	WinLoadString(hab, hInst, IDS_TITLE, sizeof(szTitle), szTitle);

  /* Allow only one instance of nsinstall to run.
   * Detect a previous instance of nsinstall, bring it to the 
   * foreground, and quit current instance */
  //if(FindWindow("NSExtracting", "Extracting...") != NULL)
    //return(1);

  /* Allow only one instance of Setup to run.
   * Detect a previous instance of Setup, and quit */
//  if((hwndFW = FindWindow(CLASS_NAME_SETUP_DLG, NULL)) != NULL)
//  {
    SWP swp;
    WinQueryWindowPos(hwndFW, &swp);
    WinShowWindow(hwndFW, TRUE);
    WinSetWindowPos(hwndFW, HWND_TOP, swp.x, swp.y, swp.cx, swp.cy, SWP_MOVE|SWP_ZORDER);
    return(1);
//  }


  // Parse the command line
  ParseCommandLine(lpCmdLine);

	// Figure out the total size of the resources
	//EnumResourceNames(NULL, "FILE", (ENUMRESNAMEPROC)SizeOfResourcesProc, 0);

  // Register a class for the gauge
/*  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc   = (WNDPROC)GaugeWndProc;
  wc.hInstance     = hInstance;
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszClassName = "NSGauge"; */
  //RegisterClass(&wc);
  WinRegisterClass(hab, "NSGauge", (PFNWP)GaugeWndProc, 0L, 0);

  // Register a class for the main dialog
 /* memset(&wc, 0, sizeof(wc));
  wc.style         = CS_DBLCLKS | CS_SAVEBITS | CS_BYTEALIGNWINDOW;
  wc.lpfnWndProc   = DefDlgProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = DLGWINDOWEXTRA;
  wc.hInstance     = hInstance;
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = "NSExtracting"; */
  //RegisterClass(&wc);
  WinRegisterClass(hab, "NSExtracting", (PFNWP)WinDefDlgProc, CS_SAVEBITS , 0); 
        //CS_DBLCLKS | CS_SAVEBITS | CS_BYTEALIGNWINDOW, 0);

  if(dwMode != SILENT)
  {
	  // Display the dialog box
	  dlgInfo.hWndDlg = WinCreateDlg(HWND_DESKTOP, hwndFW, (PFNWP)DialogProc, NULL, (MPARAM)IDD_EXTRACTING);
	  WinUpdateWindow(dlgInfo.hWndDlg);
  }

	// Extract the files
	//EnumResourceNames(NULL, "FILE", (ENUMRESNAMEPROC)ExtractFilesProc, 0);
	
	// Launch the install program and wait for it to finish
	RunInstaller();
	return 0;  
}
