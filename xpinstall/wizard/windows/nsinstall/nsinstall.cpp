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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Troy Chevalier <troy@netscape.com>
 *     Sean Su <ssu@netscape.com>
 */

#include <windows.h>
#include "resource.h"
#include "zlib.h"

#define BAR_MARGIN    1
#define BAR_SPACING   2
#define BAR_WIDTH     6
#define MAX_BUF       4096

char szTitle[4096];
HINSTANCE hInst;

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
	strcat(lpszBuffer, lpszFileName);
	return TRUE;
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

// This routine updates the status string in the extracting dialog
static void
SetStatusLine(LPCTSTR lpszStatus)
{
	HWND	hWndLabel = GetDlgItem(dlgInfo.hWndDlg, IDC_STATUS);

	SetWindowText(hWndLabel, lpszStatus);
	UpdateWindow(hWndLabel);
}

// This routine will update the progress bar to the specified percentage
// (value between 0 and 100)
static void
UpdateProgressBar(unsigned value)
{
	int	nBars;

	// Figure out how many bars should be displayed
	nBars = dlgInfo.nMaxBars * value / 100;

	// Only paint if we need to display more bars
	if (nBars > dlgInfo.nBars) {
		HWND	hWndGauge = GetDlgItem(dlgInfo.hWndDlg, IDC_GAUGE);
		RECT	rect;

		// Update the gauge state before painting
		dlgInfo.nBars = nBars;

		// Only invalidate the part that needs updating
		GetClientRect(hWndGauge, &rect);
		rect.left = BAR_MARGIN + (nBars - 1) * (BAR_WIDTH + BAR_SPACING);
		InvalidateRect(hWndGauge, &rect, FALSE);
	
		// Update the whole extracting dialog. We do this because we don't
		// have a message loop to process WM_PAINT messages in case the
		// extracting dialog was exposed
		UpdateWindow(dlgInfo.hWndDlg);
	}
}

// Window proc for dialog
BOOL APIENTRY
DialogProc(HWND hWndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
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
	LPBYTE	lpBytesUnCmp;
	HANDLE	hFile;
	char	szStatus[128];
	char	szText[4096];
	
	// Update the UI
	LoadString(hInst, IDS_STATUS_EXTRACTING, szText, sizeof(szText));
	wsprintf(szStatus, szText, lpszName);
	SetStatusLine(szStatus);

	// Create a file in the temp directory
	GetFullTempPathName(lpszName, sizeof(szTmpFile), szTmpFile);

	// Extract the file
	hResInfo = FindResource((HINSTANCE)hModule, lpszName, lpszType);
	hGlobal = LoadResource((HINSTANCE)hModule, hResInfo);
	lpBytes = (LPBYTE)LockResource(hGlobal);

	// Create the file
	hFile = CreateFile(szTmpFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_TEMPORARY, NULL);

	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD	dwSize;
		DWORD	dwSizeUnCmp;
    DWORD dwTemp;

	  GetFullTempPathName("Archive.lst", sizeof(szArcLstFile), szArcLstFile);
    WritePrivateProfileString("Archives", lpszName, "TRUE", szArcLstFile);

    lpBytesUnCmp = (LPBYTE)malloc((*(LPDWORD)(lpBytes + sizeof(DWORD))) + 1);
    dwSizeUnCmp  = *(LPDWORD)(lpBytes + sizeof(DWORD));

		// Copy the file. The first DWORD specifies the size of the file
		dwSize = *(LPDWORD)lpBytes;
		lpBytes += (sizeof(DWORD) * 2);

    dwTemp = uncompress(lpBytesUnCmp, &dwSizeUnCmp, lpBytes, dwSize);

    while (dwSizeUnCmp > 0)
    {
			DWORD	dwBytesToWrite, dwBytesWritten;

			dwBytesToWrite = dwSizeUnCmp > 4096 ? 4096 : dwSizeUnCmp;
			if (!WriteFile(hFile, lpBytesUnCmp, dwBytesToWrite, &dwBytesWritten, NULL))
      {
				char szBuf[512];

      	LoadString(hInst, IDS_STATUS_EXTRACTING, szText, sizeof(szText));
				wsprintf(szBuf, szText, szTmpFile);
				MessageBox(NULL, szBuf, szTitle, MB_OK | MB_ICONEXCLAMATION);
				FreeResource(hResInfo);
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

/////////////////////////////////////////////////////////////////////////////
// WinMain

static BOOL
RunInstaller(LPSTR lpCmdLine)
{
    PROCESS_INFORMATION pi;
    STARTUPINFO         sti;
    char                szCmdLine[MAX_PATH];
    BOOL                bRet;
	  char                szText[256];
    char                szTempPath[4096];
    char                szTmp[MAX_PATH];
    char                szCurrentDirectory[MAX_PATH];
    char                szBuf[MAX_PATH];

	// Update UI
	UpdateProgressBar(100);
	LoadString(hInst, IDS_STATUS_LAUNCHING_SETUP, szText, sizeof(szText));
	SetStatusLine(szText);

    memset(&sti,0,sizeof(sti));
    sti.cb = sizeof(STARTUPINFO);

    // Setup program is in the directory specified for temporary files
	  GetFullTempPathName("SETUP.EXE", sizeof(szCmdLine), szCmdLine);
	  GetTempPath(4096, szTempPath);
    GetCurrentDirectory(MAX_PATH, szCurrentDirectory);
    GetShortPathName(szCurrentDirectory, szBuf, MAX_PATH);

    lstrcat(szCmdLine, " -a");
    lstrcat(szCmdLine, szBuf);

    if((lpCmdLine != NULL) && (*lpCmdLine != '\0'))
    {
      lstrcat(szCmdLine, " ");
      lstrcat(szCmdLine, lpCmdLine);
    }

	  // Launch the installer
    bRet = CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, 0, NULL, szTempPath, &sti, &pi);

    if (!bRet)
        return FALSE;

   	CloseHandle(pi.hThread);

	// Wait for the InstallShield UI to appear before taking down the dialog box
	WaitForInputIdle(pi.hProcess, 3000);  // wait up to 3 seconds
	DestroyWindow(dlgInfo.hWndDlg);

	// Wait for the installer to complete
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);

	// That was just the installer bootstrapper. Now we need to wait for the
	// installer itself. We can find the process ID by looking for a window of
	// class ISINSTALLSCLASS
	HWND	hWnd = FindWindow("ISINSTALLSCLASS", NULL);

	if (hWnd) {
		DWORD	dwProcessId;
		HANDLE	hProcess;

		// Get the associated process handle and wait for it to terminate
		GetWindowThreadProcessId(hWnd, &dwProcessId);

		// We need the process handle to use WaitForSingleObject
		hProcess = OpenProcess(STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE, FALSE, dwProcessId);

		if (hProcess) {
			WaitForSingleObject(hProcess, INFINITE);
			CloseHandle(hProcess);
		}
	}
	// Delete the files from the temp directory
  EnumResourceNames(NULL, "FILE", (ENUMRESNAMEPROC)DeleteTempFilesProc, 0);

  // delete archive.lst file in the temp directory
	GetFullTempPathName("Archive.lst", sizeof(szTmp), szTmp);
	DeleteFile(szTmp);
	GetFullTempPathName("core.ns", sizeof(szTmp), szTmp);
  DirectoryRemove(szTmp, TRUE);
	return TRUE;
}

int APIENTRY
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASS	wc;

	hInst = hInstance;
	LoadString(hInst, IDS_TITLE, szTitle, sizeof(szTitle));

	// Figure out the total size of the resources
	EnumResourceNames(NULL, "FILE", (ENUMRESNAMEPROC)SizeOfResourcesProc, 0);

	// Register a class for the gauge
	memset(&wc, 0, sizeof(wc));
	wc.lpfnWndProc = (WNDPROC)GaugeWndProc;
	wc.hInstance = hInstance;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.lpszClassName = "NSGauge";
	RegisterClass(&wc);

	// Display the dialog box
	dlgInfo.hWndDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_EXTRACTING),
		NULL, (DLGPROC)DialogProc);
	UpdateWindow(dlgInfo.hWndDlg);

	// Extract the files
	EnumResourceNames(NULL, "FILE", (ENUMRESNAMEPROC)ExtractFilesProc, 0);
	
	// Launch the install program and wait for it to finish
	RunInstaller(lpCmdLine);
	return 0;  
}
