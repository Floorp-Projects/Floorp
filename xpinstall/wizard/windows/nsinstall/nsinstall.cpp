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
 */

#include <windows.h>

// stdlib.h and malloc.h are needed to build with WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include <malloc.h>
#include <ctype.h>

#include "resource.h"
#include "zlib.h"

#define BAR_MARGIN    1
#define BAR_SPACING   0
#define BAR_WIDTH     6
#define MAX_BUF       4096

/* Mode of Setup to run in */
#define NORMAL                          0
#define SILENT                          1
#define AUTO                            2

/* PP: Parse Path */
#define PP_FILENAME_ONLY                1
#define PP_PATH_ONLY                    2
#define PP_ROOT_ONLY                    3

#define CLASS_NAME_SETUP                "MozillaSetup"
#define CLASS_NAME_SETUP_DLG            "MozillaSetupDlg"

char      szTitle[MAX_BUF];
char      szCmdLineToSetup[MAX_BUF];
BOOL      gbUncompressOnly;
DWORD     dwMode;
HINSTANCE hInst;
char      gszWizTempDir[20] = "ns_temp";
char      gszFileToUncompress[MAX_BUF];
BOOL      gbAllowMultipleInstalls = FALSE;

/////////////////////////////////////////////////////////////////////////////
// Global Declarations

static DWORD	nTotalBytes = 0;  // sum of all the FILE resources

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
GetFullTempPathName(LPCTSTR lpszFileName, DWORD dwBufferLength, LPTSTR lpszBuffer)
{
	DWORD	dwLen;

	dwLen = GetTempPath(dwBufferLength, lpszBuffer);
	if (lpszBuffer[dwLen - 1] != '\\')
		strcat(lpszBuffer, "\\");
	strcat(lpszBuffer, gszWizTempDir);

  dwLen = lstrlen(lpszBuffer);
	if (lpszBuffer[dwLen - 1] != '\\')
		strcat(lpszBuffer, "\\");
	strcat(lpszBuffer, lpszFileName);

	return TRUE;
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

  if(lpszDest)
    *lpszDest = '\0';
  if(lpszBeginStr == NULL)
    return(NULL);

  lpszDestTemp = (char *)calloc(iDestSize, sizeof(char));
  if(lpszDestTemp == NULL)
  {
    MessageBox(NULL, "Out of memory", NULL, MB_OK | MB_ICONEXCLAMATION);
    exit(1);
  }

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

// this function appends a backslash at the end of a string,
// if one does not already exists.
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

// This function removes a directory and its subdirectories
HRESULT DirectoryRemove(LPSTR szDestination, BOOL bRemoveSubdirs)
{
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  char            szDestTemp[MAX_BUF];
  BOOL            bFound;

  if(GetFileAttributes(szDestination) == -1)
    return(0);

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
  return(0);
}

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

void ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwOutputSize, DWORD dwType)
{
  int   iCounter;
  DWORD dwCounter;
  DWORD dwInputLen;
  BOOL  bFound;

  if((szInput != NULL) && (szOutput != NULL))
  {
    bFound        = TRUE;
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

void ParseCommandLine(LPSTR lpszCmdLine)
{
  char  szArgVBuf[MAX_BUF];
  int   i;
  int   iArgC;

  *szCmdLineToSetup = '\0';
  *gszFileToUncompress = '\0';
  dwMode = NORMAL;
  gbUncompressOnly = FALSE;
  iArgC  = GetArgC(lpszCmdLine);
  i      = 0;
  while(i < iArgC)
  {
    GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
    if((lstrcmpi(szArgVBuf, "-ms") == 0) || (lstrcmpi(szArgVBuf, "/ms") == 0))
    {
      dwMode = SILENT;
    }
    else if((lstrcmpi(szArgVBuf, "-u") == 0) || (lstrcmpi(szArgVBuf, "/u") == 0))
    {
      gbUncompressOnly = TRUE;
      GetArgV(lpszCmdLine, i + 1, szArgVBuf, sizeof(szArgVBuf));
      if((*szArgVBuf != '\0') && (*szArgVBuf != '-'))
      {
        lstrcpy(gszFileToUncompress, szArgVBuf);
        ++i; // found filename to uncompress, update 'i' for the next iteration
      }
    }
    else if((lstrcmpi(szArgVBuf, "-mmi") == 0) || (lstrcmpi(szArgVBuf, "/mmi") == 0))
    {
      gbAllowMultipleInstalls = TRUE;
    }

    ++i;
  }

  lstrcpy(szCmdLineToSetup, " ");
  lstrcat(szCmdLineToSetup, lpszCmdLine);
}

// Centers the specified window over the desktop. Assumes the window is
// smaller both horizontally and vertically than the desktop
static void
CenterWindow(HWND hWndDlg)
{
	RECT	rect;
	int		iLeft, iTop;

	GetWindowRect(hWndDlg, &rect);
	iLeft = (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2;
	iTop = (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2;

	SetWindowPos(hWndDlg, NULL, iLeft, iTop, -1, -1,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/////////////////////////////////////////////////////////////////////////////
// Extract Files Dialog

// This routine processes windows messages that are in queue
void ProcessWindowsMessages()
{
  MSG msg;

  while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

// This routine updates the status string in the extracting dialog
static void
SetStatusLine(LPCTSTR lpszStatus)
{
	HWND	hWndLabel;

  if(dwMode != SILENT)
  {
	  hWndLabel = GetDlgItem(dlgInfo.hWndDlg, IDC_STATUS);
	  SetWindowText(hWndLabel, lpszStatus);
	  UpdateWindow(hWndLabel);
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
      HWND	hWndGauge = GetDlgItem(dlgInfo.hWndDlg, IDC_GAUGE);
      RECT	rect;

      // Update the gauge state before painting
      dlgInfo.nBars = nBars;

      // Only invalidate the part that needs updating
      GetClientRect(hWndGauge, &rect);
      InvalidateRect(hWndGauge, &rect, FALSE);
    
      // Update the whole extracting dialog. We do this because we don't
      // have a message loop to process WM_PAINT messages in case the
      // extracting dialog was exposed
      UpdateWindow(dlgInfo.hWndDlg);
    }
  }
}

// Window proc for dialog
BOOL APIENTRY
DialogProc(HWND hWndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if(dwMode != SILENT)
  {
    switch (msg) {
      case WM_INITDIALOG:
        // Center the dialog over the desktop
        CenterWindow(hWndDlg);
        return FALSE;

      case WM_COMMAND:
        DestroyWindow(hWndDlg);
        return TRUE;
    }
  }

	return FALSE;  // didn't handle the message
}

/////////////////////////////////////////////////////////////////////////////
// Resource Callback Functions

BOOL APIENTRY
DeleteTempFilesProc(HANDLE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG lParam)
{
	char	szTmpFile[MAX_PATH];

	// Get the path to the file in the temp directory
	GetFullTempPathName(lpszName, sizeof(szTmpFile), szTmpFile);

	// Delete the file
	DeleteFile(szTmpFile);
	return TRUE;
}

BOOL APIENTRY
SizeOfResourcesProc(HANDLE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG lParam)
{
	HRSRC	hResInfo;

	// Find the resource
	hResInfo = FindResource((HINSTANCE)hModule, lpszName, lpszType);

#ifdef _DEBUG
	if (!hResInfo) {
		char	buf[512];

		wsprintf(buf, "Error '%d' when loading FILE resource: %s", GetLastError(), lpszName);
		MessageBox(NULL, buf, szTitle, MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}
#endif

	// Add its size to the total size. Note that the return value is subject
	// to alignment rounding, but it's close enough for our purposes
	nTotalBytes += SizeofResource(NULL, hResInfo);

	// Release the resource
	FreeResource(hResInfo);
	return TRUE;  // keep enumerating
}

BOOL APIENTRY
ExtractFilesProc(HANDLE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG lParam)
{
	char	szTmpFile[MAX_PATH];
	char	szArcLstFile[MAX_PATH];
	HRSRC	hResInfo;
	HGLOBAL	hGlobal;
	LPBYTE	lpBytes;
	LPBYTE	lptr;
	LPBYTE	lpBytesUnCmp;
	HANDLE	hFile;
	char	szStatus[128];
	char	szText[4096];
	
	// Update the UI
	LoadString(hInst, IDS_STATUS_EXTRACTING, szText, sizeof(szText));
	wsprintf(szStatus, szText, lpszName);
	SetStatusLine(szStatus);

  if(gbUncompressOnly == TRUE)
    lstrcpy(szTmpFile, lpszName);
  else
  {
	  // Create a file in the temp directory
	  GetFullTempPathName(lpszName, sizeof(szTmpFile), szTmpFile);
    CreateDirectoriesAll(szTmpFile);
  }

  if((*gszFileToUncompress != '\0') && (lstrcmpi(lpszName, gszFileToUncompress) != 0))
    // We have a file to uncompress, but the one found is not the one we want,
    // so return TRUE to continue looking for the right one.
    return TRUE;

	// Extract the file
	hResInfo = FindResource((HINSTANCE)hModule, lpszName, lpszType);
	hGlobal = LoadResource((HINSTANCE)hModule, hResInfo);
	lpBytes = (LPBYTE)LockResource(hGlobal);

	// Create the file
	hFile = CreateFile(szTmpFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);

	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD	dwSize;
		DWORD	dwSizeUnCmp;
    DWORD dwTemp;

	  GetFullTempPathName("Archive.lst", sizeof(szArcLstFile), szArcLstFile);
    WritePrivateProfileString("Archives", lpszName, "TRUE", szArcLstFile);

    lptr = (LPBYTE)malloc((*(LPDWORD)(lpBytes + sizeof(DWORD))) + 1);
    if(!lptr)
    {
      char szBuf[512];

      LoadString(hInst, IDS_ERROR_OUT_OF_MEMORY, szBuf, sizeof(szBuf));
      MessageBox(NULL, szBuf, NULL, MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
    }

    lpBytesUnCmp = lptr;
    dwSizeUnCmp  = *(LPDWORD)(lpBytes + sizeof(DWORD));

		// Copy the file. The first DWORD specifies the size of the file
		dwSize = *(LPDWORD)lpBytes;
		lpBytes += (sizeof(DWORD) * 2);

    dwTemp = uncompress(lpBytesUnCmp, &dwSizeUnCmp, lpBytes, dwSize);

    while (dwSizeUnCmp > 0)
    {
			DWORD	dwBytesToWrite, dwBytesWritten;

      ProcessWindowsMessages();

			dwBytesToWrite = dwSizeUnCmp > 4096 ? 4096 : dwSizeUnCmp;
			if (!WriteFile(hFile, lpBytesUnCmp, dwBytesToWrite, &dwBytesWritten, NULL))
      {
				char szBuf[512];

      	LoadString(hInst, IDS_STATUS_EXTRACTING, szText, sizeof(szText));
				wsprintf(szBuf, szText, szTmpFile);
				MessageBox(NULL, szBuf, szTitle, MB_OK | MB_ICONEXCLAMATION);
				FreeResource(hResInfo);
        if(lptr)
          free(lptr);

				return FALSE;
			}

			dwSizeUnCmp -= dwBytesWritten;
			lpBytesUnCmp += dwBytesWritten;

			// Update the UI to reflect the total number of bytes written
			static DWORD	nBytesWritten = 0;

			nBytesWritten += dwBytesWritten;
			UpdateProgressBar(nBytesWritten * 100 / nTotalBytes);
		}

		CloseHandle(hFile);
	}

	// Release the resource
	FreeResource(hResInfo);
  if(lptr)
    free(lptr);

  if((*gszFileToUncompress != '\0') && (lstrcmpi(lpszName, gszFileToUncompress) == 0))
    // We have a file to uncompress, and we have uncompressed it,
    // so return FALSE to stop uncompressing any other file.
    return FALSE;

	return TRUE;  // keep enumerating
}

/////////////////////////////////////////////////////////////////////////////
// Progress bar

// Draws a recessed border around the gauge
static void
DrawGaugeBorder(HWND hWnd)
{
	HDC		hDC = GetWindowDC(hWnd);
	RECT	rect;
	int		cx, cy;
	HPEN	hShadowPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
	HGDIOBJ	hOldPen;

	GetWindowRect(hWnd, &rect);
	cx = rect.right - rect.left;
	cy = rect.bottom - rect.top;

	// Draw a dark gray line segment
	hOldPen = SelectObject(hDC, (HGDIOBJ)hShadowPen);
	MoveToEx(hDC, 0, cy - 1, NULL);
	LineTo(hDC, 0, 0);
	LineTo(hDC, cx - 1, 0);

	// Draw a white line segment
	SelectObject(hDC, GetStockObject(WHITE_PEN));
	MoveToEx(hDC, 0, cy - 1, NULL);
	LineTo(hDC, cx - 1, cy - 1);
	LineTo(hDC, cx - 1, 0);

	SelectObject(hDC, hOldPen);
	DeleteObject(hShadowPen);
	ReleaseDC(hWnd, hDC);
}

// Draws the blue progress bar
static void
DrawProgressBar(HWND hWnd)
{
	PAINTSTRUCT	ps;
	HDC			hDC = BeginPaint(hWnd, &ps);
	RECT		rect;
	HBRUSH		hBlueBrush = CreateSolidBrush(RGB(0, 0, 128));

	// Draw the bars
	GetClientRect(hWnd, &rect);
	rect.left = rect.top = BAR_MARGIN;
	rect.bottom -= BAR_MARGIN;
	rect.right = rect.left + BAR_WIDTH;

	for (int i = 0; i < dlgInfo.nBars; i++) {
		RECT	dest;

		if (IntersectRect(&dest, &ps.rcPaint, &rect))
			FillRect(hDC, &rect, hBlueBrush);
		OffsetRect(&rect, BAR_WIDTH + BAR_SPACING, 0);
	}

	DeleteObject(hBlueBrush);
	EndPaint(hWnd, &ps);
}

// Adjusts the width of the gauge based on the maximum number of bars
static void
SizeToFitGauge(HWND hWnd)
{
	RECT	rect;
	int		cx;

	// Get the window size in pixels
	GetWindowRect(hWnd, &rect);

	// Size the width to fit
	cx = 2 * GetSystemMetrics(SM_CXBORDER) + 2 * BAR_MARGIN +
		dlgInfo.nMaxBars * BAR_WIDTH + (dlgInfo.nMaxBars - 1) * BAR_SPACING;

	SetWindowPos(hWnd, NULL, -1, -1, cx, rect.bottom - rect.top,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Window proc for gauge
LRESULT APIENTRY
GaugeWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DWORD	dwStyle;
	RECT	rect;

	switch (msg) {
		case WM_NCCREATE:
			dwStyle = GetWindowLong(hWnd, GWL_STYLE);
			SetWindowLong(hWnd, GWL_STYLE, dwStyle | WS_BORDER);
			return TRUE;

		case WM_CREATE:
			// Figure out the maximum number of bars that can be displayed
			GetClientRect(hWnd, &rect);
			dlgInfo.nBars = 0;
			dlgInfo.nMaxBars = (rect.right - rect.left - 2 * BAR_MARGIN + BAR_SPACING) /
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

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

HRESULT FileExists(LPSTR szFile)
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

/////////////////////////////////////////////////////////////////////////////
// WinMain

static BOOL
RunInstaller()
{
  PROCESS_INFORMATION pi;
  STARTUPINFO         sti;
  char                szCmdLine[MAX_BUF];
  char                szSetupFile[MAX_BUF];
  char                szUninstallFile[MAX_BUF];
  char                szArcLstFile[MAX_BUF];
  BOOL                bRet;
  char                szText[256];
  char                szTempPath[MAX_BUF];
  char                szTmp[MAX_PATH];
  char                xpiDir[MAX_PATH];
  char                szFilename[MAX_BUF];
  char                szBuf[MAX_BUF];

  if(gbUncompressOnly == TRUE)
    return(TRUE);

  // Update UI
  UpdateProgressBar(100);
  LoadString(hInst, IDS_STATUS_LAUNCHING_SETUP, szText, sizeof(szText));
  SetStatusLine(szText);

  memset(&sti,0,sizeof(sti));
  sti.cb = sizeof(STARTUPINFO);

  // Setup program is in the directory specified for temporary files
  GetFullTempPathName("", MAX_BUF, szTempPath);
	GetFullTempPathName("Archive.lst",   sizeof(szArcLstFile),    szArcLstFile);
  GetFullTempPathName("SETUP.EXE",     sizeof(szSetupFile),     szSetupFile);
  GetFullTempPathName("uninstall.exe", sizeof(szUninstallFile), szUninstallFile);

  GetPrivateProfileString("Archives", "uninstall.exe", "", szBuf, sizeof(szBuf), szArcLstFile);
  if((FileExists(szUninstallFile) != FALSE) && (*szBuf != '\0'))
  {
    lstrcpy(szCmdLine, szUninstallFile);
  }
  else
  {
    lstrcpy(szCmdLine, szSetupFile);
    GetModuleFileName(NULL, szFilename, sizeof(szFilename));
    ParsePath(szFilename, xpiDir, sizeof(xpiDir), PP_PATH_ONLY);
    AppendBackSlash(xpiDir, sizeof(xpiDir));
    lstrcat(xpiDir, "xpi");
    if(FileExists(xpiDir))
    {
      GetShortPathName(xpiDir, szBuf, sizeof(szBuf));
      lstrcat(szCmdLine, " -a ");
      lstrcat(szCmdLine, szBuf);
    }
    lstrcat(szCmdLine, " -n ");
    lstrcat(szCmdLine, szFilename);
  }

  if(szCmdLine != NULL)
    lstrcat(szCmdLine, szCmdLineToSetup);

  // Launch the installer
  bRet = CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, 0, NULL, szTempPath, &sti, &pi);

  if (!bRet)
    return FALSE;

  CloseHandle(pi.hThread);

  // Wait for the InstallShield UI to appear before taking down the dialog box
  WaitForInputIdle(pi.hProcess, 3000);  // wait up to 3 seconds
  if(dwMode != SILENT)
  {
    DestroyWindow(dlgInfo.hWndDlg);
  }

  // Wait for the installer to complete
  WaitForSingleObject(pi.hProcess, INFINITE);
  CloseHandle(pi.hProcess);


  // Delete the files from the temp directory
  EnumResourceNames(NULL, "FILE", (ENUMRESNAMEPROC)DeleteTempFilesProc, 0);

  // delete archive.lst file in the temp directory
  GetFullTempPathName("Archive.lst", sizeof(szTmp), szTmp);
  DeleteFile(szTmp);
  GetFullTempPathName("xpcom.ns", sizeof(szTmp), szTmp);
  DirectoryRemove(szTmp, TRUE);
  DirectoryRemove(szTempPath, FALSE);
  return TRUE;
}

int APIENTRY
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  WNDCLASS  wc;
  HWND      hwndFW;

	hInst = hInstance;
	LoadString(hInst, IDS_TITLE, szTitle, MAX_BUF);

  // Parse the command line
  ParseCommandLine(lpCmdLine);
  
  /*  Allow multiple installer instances with the 
      provision that each instance is guaranteed 
      to have its own unique setup directory 
   */
  if(FindWindow("NSExtracting", "Extracting...") != NULL ||
    (hwndFW = FindWindow(CLASS_NAME_SETUP_DLG, NULL)) != NULL ||
    (hwndFW = FindWindow(CLASS_NAME_SETUP, NULL)) != NULL)
  {
    if (gbAllowMultipleInstalls)
    {
      char szTempPath[MAX_BUF];
      GetFullTempPathName("", MAX_BUF, szTempPath);
      DWORD dwLen = lstrlen(gszWizTempDir);

      for(int i = 1; i <= 100 && (FileExists(szTempPath) != FALSE); i++)
      {
        itoa(i, (gszWizTempDir + dwLen), 10);
        GetFullTempPathName("", MAX_BUF, szTempPath);
      }

      if (FileExists(szTempPath) != FALSE)
      {
        MessageBox(NULL, "Cannot create temp directory", NULL, MB_OK | MB_ICONEXCLAMATION);
        exit(1);
      }
    }
    else
    {
      if (hwndFW!=NULL)
      {
        ShowWindow(hwndFW, SW_RESTORE);
        SetForegroundWindow(hwndFW);
      }
      return(1);
    }
  }

	// Figure out the total size of the resources
	EnumResourceNames(NULL, "FILE", (ENUMRESNAMEPROC)SizeOfResourcesProc, 0);

  // Register a class for the gauge
  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc   = (WNDPROC)GaugeWndProc;
  wc.hInstance     = hInstance;
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszClassName = "NSGauge";
  RegisterClass(&wc);

  // Register a class for the main dialog
  memset(&wc, 0, sizeof(wc));
  wc.style         = CS_DBLCLKS | CS_SAVEBITS | CS_BYTEALIGNWINDOW;
  wc.lpfnWndProc   = DefDlgProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = DLGWINDOWEXTRA;
  wc.hInstance     = hInstance;
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = "NSExtracting";
  RegisterClass(&wc);

  if(dwMode != SILENT)
  {
	  // Display the dialog box
	  dlgInfo.hWndDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_EXTRACTING), NULL, (DLGPROC)DialogProc);
	  UpdateWindow(dlgInfo.hWndDlg);
  }

	// Extract the files
	EnumResourceNames(NULL, "FILE", (ENUMRESNAMEPROC)ExtractFilesProc, 0);
	
	// Launch the install program and wait for it to finish
	RunInstaller();
	return 0;  
}
