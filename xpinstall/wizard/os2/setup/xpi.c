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

#include "extern.h"
#include "dialogs.h"
#include "extra.h"
#include "xpistub.h"
#include "xpi.h"
#include "xperr.h"
#include "logging.h"
#include "ifuncns.h"

#define BDIR_RIGHT 1
#define BDIR_LEFT  2

typedef APIRET (_cdecl *XpiInit)(const char *, const char *aLogName, pfnXPIProgress);
typedef APIRET (_cdecl *XpiInstall)(const char *, const char *, long);
typedef void    (_cdecl *XpiExit)(void);

static XpiInit          pfnXpiInit;
static XpiInstall       pfnXpiInstall;
static XpiExit          pfnXpiExit;

static long             lFileCounter;
static long             lBarberCounter;
static BOOL             bBarberBar;
static ULONG            dwBarberDirection;
static ULONG            dwCurrentArchive;
static ULONG            dwTotalArchives;
char                    szStrProcessingFile[MAX_BUF];
char                    szStrCopyingFile[MAX_BUF];
char                    szStrInstalling[MAX_BUF];

static void UpdateGaugeFileProgressBar(unsigned value);
static void UpdateGaugeArchiveProgressBar(unsigned value);
static void UpdateGaugeFileBarber(void);

struct ExtractFilesDlgInfo
{
	HWND	hWndDlg;
	int		nMaxFileBars;	    // maximum number of bars that can be displayed
	int		nMaxArchiveBars;	// maximum number of bars that can be displayed
	int		nFileBars;		    // current number of bars to display
	int		nArchiveBars;		  // current number of bars to display
} dlgInfo;

APIRET InitializeXPIStub()
{
  char szBuf[MAX_BUF];
  char szXPIStubFile[MAX_BUF];
  char szEGetProcAddress[MAX_BUF];
  HINI  hiniInstall;

  hXPIStubInst = NULL;
  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  if(!PrfQueryProfileString(hiniInstall, "Messages", "ERROR_GETPROCADDRESS", "", szEGetProcAddress, sizeof(szEGetProcAddress)))
  {
    PrfCloseProfile(hiniInstall);
    return(1);
  }
  PrfCloseProfile(hiniInstall);
  /* change current directory to where xpistub.dll */
  lstrcpy(szBuf, siCFXpcomFile.szDestination);
  AppendBackSlash(szBuf, sizeof(szBuf));
  lstrcat(szBuf, "bin");
  chdir(szBuf);

  /* build full path to xpistub.dll */
  lstrcpy(szXPIStubFile, szBuf);
  AppendBackSlash(szXPIStubFile, sizeof(szXPIStubFile));
  lstrcat(szXPIStubFile, "xpistub.dll");

  if(FileExists(szXPIStubFile) == FALSE)
    return(2);

  /* load xpistub.dll */
  
  if(DosLoadModule(NULL, 0, szXPIStubFile, hXPIStubInst) != NO_ERROR)
  {
    sprintf(szBuf, szEDllLoad, szXPIStubFile);
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(DosQueryProcAddr(hXPIstubInst, 0, "XPI_Init", pfnXpiInit) != NO_ERROR)
  {
    sprintf(szBuf, szEGetProcAddress, "XPI_Init");
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(DosQueryProcAddr(hXPIstubInst, 0, "XPI_Install", pfnXpiInstall) != NO_ERROR)
  {
    sprintf(szBuf, szEGetProcAddress, "XPI_Install");
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(DosQueryProcAddr(hXPIstubInst, 0, "XPI_Exit", pfnXpiExit) != NO_ERROR)
  {
    sprintf(szBuf, szEGetProcAddress, "XPI_Exit");
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }

  return(0);
}

APIRET DeInitializeXPIStub()
{
  pfnXpiInit    = NULL;
  pfnXpiInstall = NULL;
  pfnXpiExit    = NULL;

  if(hXPIStubInst)
    DosFreeModule(hXPIStubInst);

  chdir(szSetupDir);
  return(0);
}

void GetTotalArchivesToInstall(void)
{
  ULONG     dwIndex0;
  siC       *siCObject = NULL;

  dwIndex0        = 0;
  dwTotalArchives = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    if((siCObject->dwAttributes & SIC_SELECTED) && !(siCObject->dwAttributes & SIC_LAUNCHAPP))
      ++dwTotalArchives;

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }
}

char *GetErrorString(ULONG dwError, char *szErrorString, ULONG dwErrorStringSize)
{
  int  i = 0;
  char szErrorNumber[MAX_BUF];

  memset(szErrorString, 0, dwErrorStringSize);
  itoa(dwError, szErrorNumber, 10);

  /* map the error value to a string */
  while(TRUE)
  {
    if(*XpErrorList[i] == '\0')
      break;

    if(lstrcmpi(szErrorNumber, XpErrorList[i]) == 0)
    {
      if(*XpErrorList[i + 1] != '\0')
        lstrcpy(szErrorString, XpErrorList[i + 1]);

      break;
    }

    ++i;
  }

  return(szErrorString);
}

APIRET SmartUpdateJars()
{
  ULONG     dwIndex0;
  siC       *siCObject = NULL;
  APIRET   hrResult;
  char      szBuf[MAX_BUF];
  char      szEXpiInstall[MAX_BUF];
  char      szArchive[MAX_BUF];
  char      szMsgSmartUpdateStart[MAX_BUF];
  char      szDlgExtractingTitle[MAX_BUF];
  HINI       hiniInstall;

  hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
  if(!PrfQueryProfileString(hiniInstall, "Messages", "MSG_SMARTUPDATE_START", "", szMsgSmartUpdateStart, sizeof(szMsgSmartUpdateStart)))
  {
    PrfCloseProfile(hiniInstall);
    return(1);
  }
  if(!PrfQueryProfileString(hiniInstall, "Messages", "DLG_EXTRACTING_TITLE", "", szDlgExtractingTitle, sizeof(szDlgExtractingTitle)))
  {
    PrfCloseProfile(hiniInstall);
    return(1);
  }
  if(!PrfQueryProfileString(hiniInstall, "Messages", "STR_PROCESSINGFILE", "", szStrProcessingFile, sizeof(szStrProcessingFile)))
  {
    PrfCloseProfile(hiniInstall);
    exit(1);
  }
  if(!PrfQueryProfileString(hiniInstall, "Messages", "STR_INSTALLING", "", szStrInstalling, sizeof(szStrInstalling)))
  {
    PrfCloseProfile(hiniInstall);
    exit(1);
  }
  if(!PrfQueryProfileString(hiniInstall, "Messages", "STR_COPYINGFILE", "", szStrCopyingFile, sizeof(szStrCopyingFile)))
  {
    PrfCloseProfile(hiniInstall);
    exit(1);
  }

  ShowMessage(szMsgSmartUpdateStart, TRUE);
  if(InitializeXPIStub() == WIZ_OK)
  {
    LogISXPInstall(W_START);
    lstrcpy(szBuf, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, sgProduct.szSubPath);
    }
    hrResult = pfnXpiInit(szBuf, FILE_INSTALL_LOG, cbXPIProgress);

    ShowMessage(szMsgSmartUpdateStart, FALSE);
    InitProgressDlg();
    GetTotalArchivesToInstall();
    WinSetWindowText(dlgInfo.hWndDlg, szDlgExtractingTitle);

    dwIndex0          = 0;
    dwCurrentArchive  = 0;
    dwTotalArchives   = (dwTotalArchives * 2) + 1;
    bBarberBar        = FALSE;
    siCObject         = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
    while(siCObject)
    {
      if(siCObject->dwAttributes & SIC_SELECTED)
        /* Since the archive is selected, we need to process the file ops here */
         ProcessFileOps(T_PRE_ARCHIVE, siCObject->szReferenceName);

      /* launch smartupdate engine for earch jar to be installed */
      if((siCObject->dwAttributes & SIC_SELECTED)   &&
        !(siCObject->dwAttributes & SIC_LAUNCHAPP) &&
        !(siCObject->dwAttributes & SIC_DOWNLOAD_ONLY))
      {
        lFileCounter      = 0;
        lBarberCounter    = 0;
        dwBarberDirection = BDIR_RIGHT;
			  dlgInfo.nFileBars = 0;
        UpdateGaugeFileProgressBar(0);

        lstrcpy(szArchive, sgProduct.szAlternateArchiveSearchPath);
        AppendBackSlash(szArchive, sizeof(szArchive));
        lstrcat(szArchive, siCObject->szArchiveName);
        if((*sgProduct.szAlternateArchiveSearchPath == '\0') || (!FileExists(szArchive)))
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

              if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_FILE_NOT_FOUND", "", szEFileNotFound, sizeof(szEFileNotFound)))
              {
                sprintf(szBuf, szEFileNotFound, szArchive);
                PrintError(szBuf, ERROR_CODE_HIDE);
              }
              PrfCloseProfile(hiniInstall);
              return(1);
            }
          }
        }

        if(dwCurrentArchive == 0)
        {
          ++dwCurrentArchive;
          UpdateGaugeArchiveProgressBar((unsigned)(((double)(dwCurrentArchive)/(double)dwTotalArchives)*(double)100));
        }

        sprintf(szBuf, szStrInstalling, siCObject->szDescriptionShort);
        WinSetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS0, szBuf);
        LogISXPInstallComponent(siCObject->szDescriptionShort);

        hrResult = pfnXpiInstall(szArchive, "", 0xFFFF);
        if(hrResult == E_REBOOT)
          bReboot = TRUE;
        else if((hrResult != WIZ_OK) &&
               !(siCObject->dwAttributes & SIC_IGNORE_XPINSTALL_ERROR))
        {
          LogMSXPInstallStatus(siCObject->szArchiveName, hrResult);
          LogISXPInstallComponentResult(hrResult);
          if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_XPI_INSTALL", "", szEXpiInstall, sizeof(szEXpiInstall)))
          {
            char szErrorString[MAX_BUF];

            GetErrorString(hrResult, szErrorString, sizeof(szErrorString));
            sprintf(szBuf, "%s - %s: %d %s", szEXpiInstall, siCObject->szDescriptionShort, hrResult, szErrorString);
            PrintError(szBuf, ERROR_CODE_HIDE);
          }

          /* break out of the siCObject while loop */
          break;
        }

        ++dwCurrentArchive;
        UpdateGaugeArchiveProgressBar((unsigned)(((double)(dwCurrentArchive)/(double)dwTotalArchives)*(double)100));
        ProcessWindowsMessages();
        LogISXPInstallComponentResult(hrResult);

        if((hrResult != WIZ_OK) &&
          (siCObject->dwAttributes & SIC_IGNORE_XPINSTALL_ERROR))
          /* reset the result to WIZ_OK if there was an error and the
           * component's attributes contains SIC_IGNORE_XPINSTALL_ERROR.
           * This should be done after LogISXPInstallComponentResult()
           * because we still should log the error value. */
          hrResult = WIZ_OK;
      }

      if(siCObject->dwAttributes & SIC_SELECTED)
        /* Since the archive is selected, we need to do the file ops here */
         ProcessFileOps(T_POST_ARCHIVE, siCObject->szReferenceName);

      ++dwIndex0;
      siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
    } /* while(siCObject) */

    LogMSXPInstallStatus(NULL, hrResult);
    pfnXpiExit();
    DeInitProgressDlg();
  }
  else
  {
    ShowMessage(szMsgSmartUpdateStart, FALSE);
  }

  DeInitializeXPIStub();
  LogISXPInstall(W_END);
  PrfCloseProfile(hiniInstall);
  return(hrResult);
}

void cbXPIStart(const char *URL, const char *UIName)
{
}

void cbXPIProgress(const char* msg, PRInt32 val, PRInt32 max)
{
  char szFilename[MAX_BUF];
  char szStrProcessingFileBuf[MAX_BUF];
  char szStrCopyingFileBuf[MAX_BUF];

  if(sgProduct.dwMode != SILENT)
  {
    ParsePath((char *)msg, szFilename, sizeof(szFilename), FALSE, PP_FILENAME_ONLY);

    if(max == 0)
    {
      sprintf(szStrProcessingFileBuf, szStrProcessingFile, szFilename);
      WinSetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS3, szStrProcessingFileBuf);
      bBarberBar = TRUE;
      UpdateGaugeFileBarber();
    }
    else
    {
      if(bBarberBar == TRUE)
      {
        dlgInfo.nFileBars = 0;
        ++dwCurrentArchive;
        UpdateGaugeArchiveProgressBar((unsigned)(((double)(dwCurrentArchive)/(double)dwTotalArchives)*(double)100));
        bBarberBar = FALSE;
      }

      sprintf(szStrCopyingFileBuf, szStrCopyingFile, szFilename);
      WinSetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS3, szStrCopyingFileBuf);
      UpdateGaugeFileProgressBar((unsigned)(((double)(val+1)/(double)max)*(double)100));
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
	RECTL	rect;
	int		iLeft, iTop;

	WinQueryWindowRect(hWndDlg, &rect);
	iLeft = (WinQuerySysValue(SV_CXSCREEN) - (rect.xRight - rect.xLeft)) / 2;
	iTop  = (WinQuerySysValue(SV_CYSCREEN) - (rect.yBottom - rect.yTop)) / 2;

	WinSetWindowPos(hWndDlg, NULL, iLeft, iTop, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Window proc for dialog
MRESULT EXPENTRY
ProgressDlgProc(HWND hWndDlg, UINT msg, WPARAM wParam, WPARAM lParam)
{
   PSZ pszFontNameSize;
   ULONG ulFontNameSizeLength;
	switch (msg)
  {
		case WM_INITDIALOG:
      DisableSystemMenuItems(hWndDlg, TRUE);
			CenterWindow(hWndDlg);
      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATUS0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_GAUGE_ARCHIVE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATUS3), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_GAUGE_FILE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
			return FALSE;

		case WM_COMMAND:
			return TRUE;
	}

	return FALSE;  // didn't handle the message
}

// This routine will update the File Gauge progress bar to the specified percentage
// (value between 0 and 100)
static void
UpdateGaugeFileBarber()
{
	int	  nBars;
	HWND	hWndGauge;
	RECTL	rect;

  if(sgProduct.dwMode != SILENT)
  {
	  hWndGauge = WinWindowFromID(dlgInfo.hWndDlg, IDC_GAUGE_FILE);
    if(dwBarberDirection == BDIR_RIGHT)
    {
      if(lBarberCounter < 151)
        ++lBarberCounter;
      else
        dwBarberDirection = BDIR_LEFT;
    }
    else if(dwBarberDirection == BDIR_LEFT)
    {
      if(lBarberCounter > 0)
        --lBarberCounter;
      else
        dwBarberDirection = BDIR_RIGHT;
    }

    // Figure out how many bars should be displayed
    nBars = (dlgInfo.nMaxFileBars * lBarberCounter / 100);

    // Update the gauge state before painting
    dlgInfo.nFileBars = nBars;

    // Only invalidate the part that needs updating
    WinQueryWindowRect(hWndGauge, &rect);
    WinInvalidateRect(hWndGauge, &rect, FALSE);

    // Update the whole extracting dialog. We do this because we don't
    // have a message loop to process WM_PAINT messages in case the
    // extracting dialog was exposed
    WinUpdateWindow(dlgInfo.hWndDlg);
  }
}

// This routine will update the File Gauge progress bar to the specified percentage
// (value between 0 and 100)
static void
UpdateGaugeFileProgressBar(unsigned value)
{
	int	nBars;

  if(sgProduct.dwMode != SILENT)
  {
    // Figure out how many bars should be displayed
    nBars = dlgInfo.nMaxFileBars * value / 100;

    // Only paint if we need to display more bars
    if((nBars > dlgInfo.nFileBars) || (dlgInfo.nFileBars == 0))
    {
      HWND	hWndGauge = WinWindowFromID(dlgInfo.hWndDlg, IDC_GAUGE_FILE);
      RECTL	rect;

      // Update the gauge state before painting
      dlgInfo.nFileBars = nBars;

      // Only invalidate the part that needs updating
      WinQueryWindowRect(hWndGauge, &rect);
      WinInvalidateRect(hWndGauge, &rect, FALSE);
    
      // Update the whole extracting dialog. We do this because we don't
      // have a message loop to process WM_PAINT messages in case the
      // extracting dialog was exposed
      WinUpdateWindow(dlgInfo.hWndDlg);
    }
  }
}

// This routine will update the Archive Gauge progress bar to the specified percentage
// (value between 0 and 100)
static void
UpdateGaugeArchiveProgressBar(unsigned value)
{
	int	nBars;

  if(sgProduct.dwMode != SILENT)
  {
    // Figure out how many bars should be displayed
    nBars = dlgInfo.nMaxArchiveBars * value / 100;

    // Only paint if we need to display more bars
    if (nBars > dlgInfo.nArchiveBars)
    {
      HWND	hWndGauge = WinWindowFromID(dlgInfo.hWndDlg, IDC_GAUGE_ARCHIVE);
      RECTL	rect;

      // Update the gauge state before painting
      dlgInfo.nArchiveBars = nBars;

      // Only invalidate the part that needs updating
      WinQueryWindowRect(hWndGauge, &rect);
      WinInvalidateRect(hWndGauge, &rect, FALSE);
    
      // Update the whole extracting dialog. We do this because we don't
      // have a message loop to process WM_PAINT messages in case the
      // extracting dialog was exposed
      WinUpdateWindow(dlgInfo.hWndDlg);
    }
  }
}

// Draws a recessed border around the gauge
static void
DrawGaugeBorder(HWND hWnd)
{
	HDC		hDC = GetWindowDC(hWnd);
    HPS       hPS = WinGetPS(hWnd);
	RECTL	rect;
	int		cx, cy;
	HPEN	hShadowPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
	HGDIOBJ	hOldPen;

	WinQueryWindowRect(hWnd, &rect);
	cx = rect.xRight - rect.xLeft;
	cy = rect.yBottom - rect.yTop;

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
    WinReleasePS(hPS);
}

// Draws the blue progress bar
static void
DrawProgressBar(HWND hWnd, int nBars)
{
  int         i;
	RECTL        rect;
	HBRUSH      hBrush;
    HPS           hPS;

  hPS = WinBeginPaint(hWnd, NULLHANDLE, &rect);
	WinQueryWindowRect(hWnd, &rect);
  if(nBars <= 0)
  {
    /* clear the bars */
    WinFillRect(hPS, &rect, SYSCLR_MENU);
  }
  else
  {
  	// Draw the bars
	  rect.xLeft     = rect.yTop = BAR_MARGIN;
	  rect.yBottom  -= BAR_MARGIN;
	  rect.xRight    = rect.xLeft + BAR_WIDTH;

	  for(i = 0; i < nBars; i++)
    {
		  RECTL	dest;

		  if(WinIntersectRect((HAB)0, &dest, &ps.rcPaint, &rect))
			  WinFillRect(hPS, &rect, CLR_DARKBLUE);

      WinOffsetRect((HAB)0, &rect, BAR_WIDTH + BAR_SPACING, 0);
	  }
  }

	WinEndPaint(hPS);
}

// Draws the blue progress bar
static void
DrawBarberBar(HWND hWnd, int nBars)
{
  int         i;
	PAINTSTRUCT	ps;
	HDC         hDC;
    HPS         hPS;
	RECTL        rect;
	HBRUSH      hBrush      = NULL;
	HBRUSH      hBrushClear = NULL;

  hPS = WinBeginPaint(hWnd, NULLHANDLE, &rect);
	WinQueryWindowRect(hWnd, &rect);
  if(nBars <= 0)
  {
    /* clear the bars */
    WinFillRect(hPS, &rect, SYSCLR_MENU);
  }
  else
  {
  	// Draw the bars
	  rect.xLeft     = rect.yTop = BAR_MARGIN;
	  rect.yBottom  -= BAR_MARGIN;
	  rect.xRight    = rect.xLeft + BAR_WIDTH;

	  for(i = 0; i < (nBars + 1); i++)
    {
		  RECTL	dest;

		  if(WinIntersectRect((HAB)0, &dest, &ps.rcPaint, &rect))
      {
        if((i >= (nBars - 15)) && (i < nBars))
			    WinFillRect(hPS, &rect, CLR_DARKBLUE);
        else
			    WinFillRect(hPS, &rect, SYSCLR_MENU);
      }

      WinOffsetRect((HAB)0, &rect, BAR_WIDTH + BAR_SPACING, 0);
	  }
  }
	WinEndPaint(hPS);
}

// Adjusts the width of the gauge based on the maximum number of bars
static void
SizeToFitGauge(HWND hWnd, int nMaxBars)
{
	RECTL	rect;
	int		cx;

	// Get the window size in pixels
	WinQueryWindowRect(hWnd, &rect);

	// Size the width to fit
	cx = 2 * WinQuerySysValue(SV_CXBORDER) + 2 * BAR_MARGIN +
		nMaxBars * BAR_WIDTH + (nMaxBars - 1) * BAR_SPACING;

	WinSetWindowPos(hWnd, NULL, -1, -1, cx, rect.yBottom - rect.yTop,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Window proc for file gauge
MRESULT EXPENTRY
GaugeFileWndProc(HWND hWnd, UINT msg, WPARAM wParam, WPARAM lParam)
{
	ULONG	dwStyle;
	RECTL	rect;

	switch(msg)
  {
		case WM_NCCREATE:
			dwStyle = WinQueryWindowULong(hWnd, GWL_STYLE);
			WinSetWindowULong(hWnd, GWL_STYLE, dwStyle | WS_BORDER);
			return(TRUE);

		case WM_CREATE:
			// Figure out the maximum number of bars that can be displayed
			WinQueryWindowRect(hWnd, &rect);
			dlgInfo.nFileBars = 0;
			dlgInfo.nMaxFileBars = (rect.xRight - rect.xLeft - 2 * BAR_MARGIN + BAR_SPACING) / (BAR_WIDTH + BAR_SPACING);

			// Size the gauge to exactly fit the maximum number of bars
			SizeToFitGauge(hWnd, dlgInfo.nMaxFileBars);
			return(FALSE);

		case WM_NCPAINT:
			DrawGaugeBorder(hWnd);
			return(FALSE);

		case WM_PAINT:
      if(bBarberBar == TRUE)
			  DrawBarberBar(hWnd, dlgInfo.nFileBars);
      else
			  DrawProgressBar(hWnd, dlgInfo.nFileBars);

			return(FALSE);
	}

	return(DefWindowProc(hWnd, msg, wParam, lParam));
}

// Window proc for file gauge
MRESULT EXPENTRY
GaugeArchiveWndProc(HWND hWnd, UINT msg, WPARAM wParam, WPARAM lParam)
{
	ULONG	dwStyle;
	RECTL	rect;

	switch(msg)
  {
		case WM_NCCREATE:
			dwStyle = WinQueryWindowULong(hWnd, GWL_STYLE);
			WinSetWindowULong(hWnd, GWL_STYLE, dwStyle | WS_BORDER);
			return(TRUE);

		case WM_CREATE:
			// Figure out the maximum number of bars that can be displayed
			WinQueryWindowRect(hWnd, &rect);
			dlgInfo.nArchiveBars = 0;
			dlgInfo.nMaxArchiveBars = (rect.xRight - rect.xLeft - 2 * BAR_MARGIN + BAR_SPACING) / (BAR_WIDTH + BAR_SPACING);

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

  if(sgProduct.dwMode != SILENT)
  {
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
    dlgInfo.hWndDlg = WinCreateDlg(hWndMain, NULLHANDLE, MAKEINTRESOURCE(DLG_EXTRACTING), (PFNWP)ProgressDlgProc, MAKEINTRESOURCE(DLG_EXTRACTING), NULL);
    WinUpdateWindow(dlgInfo.hWndDlg);
  }
}

void DeInitProgressDlg()
{
  if(sgProduct.dwMode != SILENT)
  {
    WinDestroyWindow(dlgInfo.hWndDlg);
    UnregisterClass("GaugeFile", hInst);
    UnregisterClass("GaugeArchive", hInst);
  }
}
