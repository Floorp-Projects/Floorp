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
#include "dialogs.h"
#include "extra.h"
#include "xpistub.h"
#include "xpi.h"

static XpiInit          pfnXpiInit;
static XpiInstall       pfnXpiInstall;
static XpiExit          pfnXpiExit;

static long             lFileCounter;
static DWORD            dwCurrentArchive;
static DWORD            dwTotalArchives;

static void UpdateGaugeFileProgressBar(unsigned value);
static void UpdateGaugeArchiveProgressBar(unsigned value);
static void UpdateFileStatus(const char *szFullFilename);

struct ExtractFilesDlgInfo
{
	HWND	hWndDlg;
	int		nMaxFileBars;	    // maximum number of bars that can be displayed
	int		nMaxArchiveBars;	// maximum number of bars that can be displayed
	int		nFileBars;		    // current number of bars to display
	int		nArchiveBars;		  // current number of bars to display
} dlgInfo;

HRESULT InitializeXPIStub()
{
  char szBuf[MAX_BUF];
  char szXPIStubFile[MAX_BUF];
  char szEGetProcAddress[MAX_BUF];

  hXPIStubInst = NULL;

  if(NS_LoadString(hSetupRscInst, IDS_ERROR_GETPROCADDRESS, szEGetProcAddress, MAX_BUF) != WIZ_OK)
    return(1);

  /* change current directory to where xpistub.dll */
  lstrcpy(szBuf, siCFCoreFile.szDestination);
  AppendBackSlash(szBuf, sizeof(szBuf));
  lstrcat(szBuf, "x86rel");
  chdir(szBuf);

  /* build full path to xpistub.dll */
  lstrcpy(szXPIStubFile, szBuf);
  AppendBackSlash(szXPIStubFile, sizeof(szXPIStubFile));
  lstrcat(szXPIStubFile, "xpistub.dll");

  if(FileExists(szXPIStubFile) == FALSE)
    return(2);

  /* load xpistub.dll */
  if((hXPIStubInst = LoadLibraryEx(szXPIStubFile, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) == NULL)
  {
    wsprintf(szBuf, szEDllLoad, szXPIStubFile);
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(((FARPROC)pfnXpiInit = GetProcAddress(hXPIStubInst, "XPI_Init")) == NULL)
  {
    wsprintf(szBuf, szEGetProcAddress, "XPI_Init");
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(((FARPROC)pfnXpiInstall = GetProcAddress(hXPIStubInst, "XPI_Install")) == NULL)
  {
    wsprintf(szBuf, szEGetProcAddress, "XPI_Install");
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(((FARPROC)pfnXpiExit = GetProcAddress(hXPIStubInst, "XPI_Exit")) == NULL)
  {
    wsprintf(szBuf, szEGetProcAddress, "XPI_Exit");
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }

  return(0);
}

HRESULT DeInitializeXPIStub()
{
  pfnXpiInit    = NULL;
  pfnXpiInstall = NULL;
  pfnXpiExit    = NULL;

  if(hXPIStubInst)
    FreeLibrary(hXPIStubInst);

  chdir(szSetupDir);
  return(0);
}

void GetTotalArchivesToInstall(void)
{
  DWORD     dwIndex0;
  siC       *siCObject = NULL;

  dwIndex0        = 0;
  dwTotalArchives = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE);
  while(siCObject)
  {
    if((siCObject->dwAttributes & SIC_SELECTED) && !(siCObject->dwAttributes & SIC_LAUNCHAPP))
      ++dwTotalArchives;

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE);
  }
}

HRESULT SmartUpdateJars()
{
  DWORD     dwIndex0;
  siC       *siCObject = NULL;
  HRESULT   hrResult;
  char      szBuf[MAX_BUF];
  char      szArchive[MAX_BUF];
  char      szMsgSmartUpdateStart[MAX_BUF];

  if(NS_LoadString(hSetupRscInst, IDS_MSG_SMARTUPDATE_START, szMsgSmartUpdateStart, MAX_BUF) != WIZ_OK)
    return(1);

  ShowMessage(szMsgSmartUpdateStart, TRUE);
  if(InitializeXPIStub() == WIZ_OK)
  {
    hrResult = pfnXpiInit(sgProduct.szPath, cbXPIStart, cbXPIProgress, cbXPIFinal);

    ShowMessage(szMsgSmartUpdateStart, FALSE);
    InitProgressDlg();
    GetTotalArchivesToInstall();

    dwIndex0          = 0;
    dwCurrentArchive  = 0;
    dwTotalArchives  *= 2;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE);
    while(siCObject)
    {
      /* launch smartupdate engine for earch jar to be installed */
      if((siCObject->dwAttributes & SIC_SELECTED) && !(siCObject->dwAttributes & SIC_LAUNCHAPP))
      {
        lFileCounter      = 0;
			  dlgInfo.nFileBars = 0;
        UpdateGaugeFileProgressBar(0);

        lstrcpy(szArchive, sgProduct.szAlternateArchiveSearchPath);
        AppendBackSlash(szArchive, sizeof(szArchive));
        lstrcat(szArchive, siCObject->szArchiveName);
        if(!FileExists(szArchive))
        {
          lstrcpy(szArchive, szSetupDir);
          AppendBackSlash(szArchive, sizeof(szArchive));
          lstrcat(szArchive, siCObject->szArchiveName);
          if(!FileExists(szArchive))
          {
            lstrcpy(szArchive, szTempDir);
            AppendBackSlash(szArchive, sizeof(szArchive));
            lstrcat(szArchive, siCObject->szArchiveName);
            if(!FileExists(szArchive))
            {
              char szEFileNotFound[MAX_BUF];

              if(NS_LoadString(hSetupRscInst, IDS_ERROR_FILE_NOT_FOUND, szEFileNotFound, MAX_BUF) == WIZ_OK)
              {
                wsprintf(szBuf, szEFileNotFound, szArchive);
                PrintError(szBuf, ERROR_CODE_HIDE);
              }
              return(1);
            }
          }
        }
        hrResult = pfnXpiInstall(szArchive, "", 0xFFFF);

        ++dwCurrentArchive;
        UpdateGaugeArchiveProgressBar((unsigned)(((double)(dwCurrentArchive)/(double)dwTotalArchives)*(double)100));
        ProcessWindowsMessages();
      }

      ++dwIndex0;
      siCObject = SiCNodeGetObject(dwIndex0, TRUE);
    }

    pfnXpiExit();
    DeInitProgressDlg();
  }
  else
  {
    ShowMessage(szMsgSmartUpdateStart, FALSE);
  }

  DeInitializeXPIStub();

  return(0);
}

void cbXPIStart(const char *URL, const char *UIName)
{
//  MessageBox(NULL, UIName, "XpiStub is running", MB_ICONEXCLAMATION);
}

void cbXPIProgress(const char* msg, PRInt32 val, PRInt32 max)
{
  if(max == 0)
  {
    UpdateFileStatus(msg);
  }
  else
  {
    UpdateGaugeFileProgressBar((unsigned)(((double)(val+1)/(double)max)*(double)100));
    if((dwCurrentArchive % 2) == 0)
    {
      ++dwCurrentArchive;
      UpdateGaugeArchiveProgressBar((unsigned)(((double)(dwCurrentArchive)/(double)dwTotalArchives)*(double)100));
    }
  }

  ProcessWindowsMessages();
}

void cbXPIFinal(const char *URL, PRInt32 finalStatus)
{
}



/////////////////////////////////////////////////////////////////////////////
// Progress bar

// Centers the specified window over the desktop. Assumes the window is
// smaller both horizontally and vertically than the desktop
static void
CenterWindow(HWND hWndDlg)
{
	RECT	rect;
	int		iLeft, iTop;

	GetWindowRect(hWndDlg, &rect);
	iLeft = (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2;
	iTop  = (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2;

	SetWindowPos(hWndDlg, NULL, iLeft, iTop, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Window proc for dialog
LRESULT CALLBACK
ProgressDlgProc(HWND hWndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
  char szStrFileNumber[MAX_BUF];
  char szStrFilename[MAX_BUF];

	switch (msg)
  {
		case WM_INITDIALOG:
      if(NS_LoadString(hSetupRscInst, IDS_STR_FILE_NUMBER, szStrFileNumber, MAX_BUF) != WIZ_OK)
        exit(1);
      if(NS_LoadString(hSetupRscInst, IDS_STR_FILENAME, szStrFilename, MAX_BUF) != WIZ_OK)
        exit(1);

			// Center the dialog over the desktop
			CenterWindow(hWndDlg);
      SetDlgItemText(hWndDlg, IDC_STATUS0, szStrFileNumber);
      SetDlgItemText(hWndDlg, IDC_STATUS3, szStrFilename);
			return FALSE;

		case WM_COMMAND:
//			DestroyWindow(hWndDlg);
			return TRUE;
	}

	return FALSE;  // didn't handle the message
}

static void
UpdateFileStatus(const char *szFullFilename)
{
  char szFileCounter[MAX_BUF];
  char szFilename[MAX_BUF];

  ParsePath((char *)szFullFilename, szFilename, sizeof(szFilename), PP_FILENAME_ONLY);
  ++lFileCounter;
  ltoa(lFileCounter, szFileCounter, 10);
  SetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS1, szFileCounter);
  SetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS2, szFilename);
}

// This routine will update the File Gauge progress bar to the specified percentage
// (value between 0 and 100)
static void
UpdateGaugeFileProgressBar(unsigned value)
{
	int	nBars;

	// Figure out how many bars should be displayed
	nBars = dlgInfo.nMaxFileBars * value / 100;

	// Only paint if we need to display more bars
	if((nBars > dlgInfo.nFileBars) || (dlgInfo.nFileBars == 0))
  {
		HWND	hWndGauge = GetDlgItem(dlgInfo.hWndDlg, IDC_GAUGE_FILE);
		RECT	rect;

		// Update the gauge state before painting
		dlgInfo.nFileBars = nBars;

		// Only invalidate the part that needs updating
		GetClientRect(hWndGauge, &rect);
		InvalidateRect(hWndGauge, &rect, FALSE);
	
		// Update the whole extracting dialog. We do this because we don't
		// have a message loop to process WM_PAINT messages in case the
		// extracting dialog was exposed
		UpdateWindow(dlgInfo.hWndDlg);
	}
}

// This routine will update the Archive Gauge progress bar to the specified percentage
// (value between 0 and 100)
static void
UpdateGaugeArchiveProgressBar(unsigned value)
{
	int	nBars;

	// Figure out how many bars should be displayed
	nBars = dlgInfo.nMaxArchiveBars * value / 100;

	// Only paint if we need to display more bars
	if (nBars > dlgInfo.nArchiveBars)
  {
		HWND	hWndGauge = GetDlgItem(dlgInfo.hWndDlg, IDC_GAUGE_ARCHIVE);
		RECT	rect;

		// Update the gauge state before painting
		dlgInfo.nArchiveBars = nBars;

		// Only invalidate the part that needs updating
		GetClientRect(hWndGauge, &rect);
		InvalidateRect(hWndGauge, &rect, FALSE);
	
		// Update the whole extracting dialog. We do this because we don't
		// have a message loop to process WM_PAINT messages in case the
		// extracting dialog was exposed
		UpdateWindow(dlgInfo.hWndDlg);
	}
}

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
DrawProgressBar(HWND hWnd, int nBars)
{
  int         i;
	PAINTSTRUCT	ps;
	HDC         hDC;
	RECT        rect;
	HBRUSH      hBrush;

  hDC = BeginPaint(hWnd, &ps);
	GetClientRect(hWnd, &rect);
  if(nBars <= 0)
  {
    /* clear the bars */
    hBrush = CreateSolidBrush(GetSysColor(COLOR_MENU));
    FillRect(hDC, &rect, hBrush);
  }
  else
  {
  	// Draw the bars
    hBrush = CreateSolidBrush(RGB(0, 0, 128));
	  rect.left     = rect.top = BAR_MARGIN;
	  rect.bottom  -= BAR_MARGIN;
	  rect.right    = rect.left + BAR_WIDTH;

	  for(i = 0; i < nBars; i++)
    {
		  RECT	dest;

		  if(IntersectRect(&dest, &ps.rcPaint, &rect))
			  FillRect(hDC, &rect, hBrush);

      OffsetRect(&rect, BAR_WIDTH + BAR_SPACING, 0);
	  }
  }

	DeleteObject(hBrush);
	EndPaint(hWnd, &ps);
}

// Adjusts the width of the gauge based on the maximum number of bars
static void
SizeToFitGauge(HWND hWnd, int nMaxBars)
{
	RECT	rect;
	int		cx;

	// Get the window size in pixels
	GetWindowRect(hWnd, &rect);

	// Size the width to fit
	cx = 2 * GetSystemMetrics(SM_CXBORDER) + 2 * BAR_MARGIN +
		nMaxBars * BAR_WIDTH + (nMaxBars - 1) * BAR_SPACING;

	SetWindowPos(hWnd, NULL, -1, -1, cx, rect.bottom - rect.top,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Window proc for file gauge
LRESULT CALLBACK
GaugeFileWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DWORD	dwStyle;
	RECT	rect;

	switch(msg)
  {
		case WM_NCCREATE:
			dwStyle = GetWindowLong(hWnd, GWL_STYLE);
			SetWindowLong(hWnd, GWL_STYLE, dwStyle | WS_BORDER);
			return(TRUE);

		case WM_CREATE:
			// Figure out the maximum number of bars that can be displayed
			GetClientRect(hWnd, &rect);
			dlgInfo.nFileBars = 0;
			dlgInfo.nMaxFileBars = (rect.right - rect.left - 2 * BAR_MARGIN + BAR_SPACING) / (BAR_WIDTH + BAR_SPACING);

			// Size the gauge to exactly fit the maximum number of bars
			SizeToFitGauge(hWnd, dlgInfo.nMaxFileBars);
			return(FALSE);

		case WM_NCPAINT:
			DrawGaugeBorder(hWnd);
			return(FALSE);

		case WM_PAINT:
			DrawProgressBar(hWnd, dlgInfo.nFileBars);
			return(FALSE);
	}

	return(DefWindowProc(hWnd, msg, wParam, lParam));
}

// Window proc for file gauge
LRESULT CALLBACK
GaugeArchiveWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DWORD	dwStyle;
	RECT	rect;

	switch(msg)
  {
		case WM_NCCREATE:
			dwStyle = GetWindowLong(hWnd, GWL_STYLE);
			SetWindowLong(hWnd, GWL_STYLE, dwStyle | WS_BORDER);
			return(TRUE);

		case WM_CREATE:
			// Figure out the maximum number of bars that can be displayed
			GetClientRect(hWnd, &rect);
			dlgInfo.nArchiveBars = 0;
			dlgInfo.nMaxArchiveBars = (rect.right - rect.left - 2 * BAR_MARGIN + BAR_SPACING) / (BAR_WIDTH + BAR_SPACING);

			// Size the gauge to exactly fit the maximum number of bars
			SizeToFitGauge(hWnd, dlgInfo.nMaxArchiveBars);
			return(FALSE);

		case WM_NCPAINT:
			DrawGaugeBorder(hWnd);
			return(FALSE);

		case WM_PAINT:
			DrawProgressBar(hWnd, dlgInfo.nArchiveBars);
			return(FALSE);
	}

	return(DefWindowProc(hWnd, msg, wParam, lParam));
}

void InitProgressDlg()
{
	WNDCLASS	wc;

	memset(&wc, 0, sizeof(wc));
  wc.style          = CS_GLOBALCLASS;
	wc.hInstance      = hInst;
	wc.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpfnWndProc    = (WNDPROC)GaugeFileWndProc;
	wc.lpszClassName  = "GaugeFile";
	RegisterClass(&wc);

  wc.lpfnWndProc    = (WNDPROC)GaugeArchiveWndProc;
	wc.lpszClassName  = "GaugeArchive";
	RegisterClass(&wc);

	// Display the dialog box
	dlgInfo.hWndDlg = CreateDialog(hSetupRscInst, MAKEINTRESOURCE(DLG_EXTRACTING), hWndMain, (WNDPROC)ProgressDlgProc);
	UpdateWindow(dlgInfo.hWndDlg);
}

void DeInitProgressDlg()
{
  DestroyWindow(dlgInfo.hWndDlg);
  UnregisterClass("GaugeFile", hInst);
  UnregisterClass("GaugeArchive", hInst);
}
