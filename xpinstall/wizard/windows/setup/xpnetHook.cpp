/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Navigator.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape Communications Corp. are
 * Copyright (C) 1998, 1999, 2000, 2001 Netscape Communications Corp.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Sean Su <ssu@netscape.com>
 */

#include <windows.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include "xpnetHook.h"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#include "nsFTPConn.h"
#include "nsHTTPConn.h"

const int  kProxySrvrLen = 1024;
const char kHTTP[8]      = "http://";
const char kFTP[7]       = "ftp://";
const char kLoclFile[7]  = "zzzFTP";

static long             glLastBytesSoFar;
static long             glAbsoluteBytesSoFar;
static long             glTotalKb;
char                    gszStrCopyingFile[MAX_BUF_MEDIUM];
char                    gszCurrentDownloadPath[MAX_BUF];
char                    gszCurrentDownloadFilename[MAX_BUF_TINY];
char                    gszCurrentDownloadFileDescription[MAX_BUF_TINY];
char                    gszUrl[MAX_BUF];
char                    *gszConfigIniFile;
BOOL                    gbDlgDownloadMinimized;
BOOL                    gbDlgDownloadJustMinimized;
BOOL                    gbUrlChanged;
BOOL                    gbCancelDownload;
BOOL                    gbShowDownloadRetryMsg;
int                     giIndex;
int                     giTotalArchivesToDownload;

double GetPercentSoFar(void);

static void UpdateGaugeFileProgressBar(double value);
       int  ProgressCB(int aBytesSoFar, int aTotalFinalSize);
       void InitDownloadDlg(void);
       void DeInitDownloadDlg();

struct DownloadFileInfo
{
  char szUrl[MAX_BUF];
  char szFile[MAX_BUF_TINY];
} dlFileInfo;

struct ExtractFilesDlgInfo
{
	HWND	hWndDlg;
	int		nMaxFileBars;	    // maximum number of bars that can be displayed
	int		nMaxArchiveBars;	// maximum number of bars that can be displayed
	int   nFileBars;	      // current number of bars to display
	int		nArchiveBars;		  // current number of bars to display
} dlgInfo;


#ifdef SSU_TEST

void ParseURLServerAndPath(char *szInURL, char *szOutServer, DWORD dwOutServerBufSize, char *szOutPath, DWORD dwOutPathBufSize)
{
  DWORD dwOutServerLen;
  char  cDelimiter   = '/';
  char  *ptrChar     = NULL;
  char  *ptrLastChar = NULL;

  lstrcpy(szOutServer, szInURL);
  ZeroMemory(szOutPath, dwOutPathBufSize);
  dwOutServerLen = lstrlen(szOutServer);
  ptrLastChar    = CharPrev(szOutServer, &szOutServer[dwOutServerLen]);
  ptrChar        = CharNext(szOutServer);

  iFoundDelimiter = 0;
  ptrChar = szOutput;
  while(!bDone)
  {
    if(*ptrChar == cDelimiter)
      ++iFoundDelimiter;

    if(iFoundDelimiter == 3)
    {
      lstrcpy(szOutPath, CharNext(prtChar));
      *CharNext(ptrChar) = '\0';
      bDone = TRUE;
    }
    else if(ptrChar == ptrLastChar)
      bDone = TRUE;
    else
      ptrChar = CharNext(ptrChar);
  }
}

void TruncateURLString(HWND hWnd, LPSTR szInURL, DWORD dwInURLBufSize, LPSTR szOutString, DWORD dwOutStringBufSize)
{
  HDC           hdcWnd;
  LOGFONT       logFont;
  HFONT         hfontTmp;
  HFONT         hfontOld;
  RECT          rWndRect;
  SIZE          sizeString;
  BOOL          bDone;
  char          szFilename[MAX_BUF];
  char          szURLServerPart[MAX_BUF];
  char          szURLPath[MAX_BUF];
  char          *ptrCharPrev1;
  char          *ptrCharPrev2;
  char          *ptrCharPrev3;
  char          *ptrCharPrev4;
  char          cCharPrev2;
  char          cCharPrev3;
  char          cCharPrev4;

  ZeroMemory(szOutString, dwOutStringBufSize);
  if(dwInURLBufSize > dwOutStringBufSize)
    return;

  if(lstrlen(szInURL) == 0)
    return;

  ParsePath(szInURL, szFilename, sizeof(szFilename), TRUE, PP_FILENAME_ONLY);
  ParsePath(szInURL, szURLwithoutFilename, sizeof(szURLwithoutFilename), TRUE, PP_PATH_ONLY);
  ParseURLServerAndPath(szURLwithoutFilename, szURLServerPart, sizeof(szURLServerPart), szURLPath, sizeof(szURLPath));
  

  lstrcpy(szOutString, szInURL);
  hdcWnd = GetWindowDC(hWnd);
  GetClientRect(hWnd, &rWndRect);
  SystemParametersInfo(SPI_GETICONTITLELOGFONT,
                       sizeof(logFont),
                       (PVOID)&logFont,
                       0);

  hfontTmp = CreateFontIndirect(&logFont);

  if(hfontTmp)
    hfontOld = (HFONT)SelectObject(hdcWnd, hfontTmp);

  /* make this its own function so that we can call it for szURLServerPath, szURLPath, and szFilename */
  /** From here **/
  GetTextExtentPoint32(hdcWnd, szOutString, lstrlen(szOutString), &sizeString);
  if(sizeString.cx > rWndRect.right)
  {
    bDone = FALSE;
    while(!bDone)
    {
      ptrCharPrev1 = CharPrev(szOutString, &szOutString[lstrlen(szOutString)]);
      ptrCharPrev2 = CharPrev(szOutString, ptrCharPrev1);
      ptrCharPrev3 = CharPrev(szOutString, ptrCharPrev2);
      ptrCharPrev4 = CharPrev(szOutString, ptrCharPrev3);
      cCharPrev2 = *ptrCharPrev2;
      cCharPrev3 = *ptrCharPrev3;
      cCharPrev4 = *ptrCharPrev4;

      *ptrCharPrev1 = '\0';
      *ptrCharPrev2 = '.';
      *ptrCharPrev3 = '.';
      *ptrCharPrev4 = '.';


      GetTextExtentPoint32(hdcWnd, szOutString, lstrlen(szOutString), &sizeString);
      if(sizeString.cx > rWndRect.right)
      {
        *ptrCharPrev2 = cCharPrev2;
        *ptrCharPrev3 = cCharPrev3;
        *ptrCharPrev4 = cCharPrev4;
      }
      else
        bDone = TRUE;
    }
  }
  /** To here **/

  SelectObject(hdcWnd, hfontOld);
  DeleteObject(hfontTmp);
  ReleaseDC(hWnd, hdcWnd);
}
#endif

void SetStatusStatus(void)
{
  char szStatusStatusLine[MAX_BUF_MEDIUM];
  char szCurrentStatusInfo[MAX_BUF_MEDIUM];

  if(!gbShowDownloadRetryMsg)
  {
    GetPrivateProfileString("Strings", "Status Download", "", szStatusStatusLine, sizeof(szStatusStatusLine), szFileIniConfig);
    if(*szStatusStatusLine != '\0')
      wsprintf(szCurrentStatusInfo, szStatusStatusLine, (giIndex + 1), giTotalArchivesToDownload, (glAbsoluteBytesSoFar / 1024), glTotalKb, (int)GetPercentSoFar());
    else
      wsprintf(szCurrentStatusInfo, "Downloading file %d of %d (%uKB of %uKB total - %d%%)", (giIndex + 1), giTotalArchivesToDownload, (glAbsoluteBytesSoFar / 1024), glTotalKb, (int)GetPercentSoFar());
  }
  else
  {
    GetPrivateProfileString("Strings", "Status Retry", "", szStatusStatusLine, sizeof(szStatusStatusLine), szFileIniConfig);
    if(*szStatusStatusLine != '\0')
      wsprintf(szCurrentStatusInfo, szStatusStatusLine, (giIndex + 1), giTotalArchivesToDownload, (glAbsoluteBytesSoFar / 1024), glTotalKb, (int)GetPercentSoFar());
    else
      wsprintf(szCurrentStatusInfo, "Redownloading file %d of %d (%uKB of %uKB total - %d%%)", (giIndex + 1), giTotalArchivesToDownload, (glAbsoluteBytesSoFar / 1024), glTotalKb, (int)GetPercentSoFar());
  }

  /* Set the download dialog title */
  SetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS_STATUS, szCurrentStatusInfo);
}

void SetStatusFile(void)
{
  /* Set the download dialog status*/
  SetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS_FILE, gszCurrentDownloadFileDescription);
  SetStatusStatus();
}

void SetStatusUrl(void)
{
  /* display the current url being processed */
  if(gbUrlChanged)
  {
    char szUrlPathBuf[MAX_BUF];
    HWND hStatusUrl = NULL;

    hStatusUrl = GetDlgItem(dlgInfo.hWndDlg, IDC_STATUS_URL);
    if(hStatusUrl)
      TruncateString(hStatusUrl, gszUrl, sizeof(gszUrl), szUrlPathBuf, sizeof(szUrlPathBuf));
    else
      lstrcpy(szUrlPathBuf, gszUrl);

    SetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS_URL, szUrlPathBuf);
    SetStatusFile();
    gbUrlChanged = FALSE;
  }
}

double GetPercentSoFar(void)
{
  return((double)(((double)(glAbsoluteBytesSoFar / 1024) / (double)glTotalKb) * (double)100));
}

void SetMinimizedDownloadTitle(DWORD dwPercentSoFar)
{
  static DWORD dwLastPercentSoFar = 0;
  char szDownloadTitle[MAX_BUF_MEDIUM];
  char gszCurrentDownloadInfo[MAX_BUF_MEDIUM];

  GetPrivateProfileString("Strings", "Dialog Download Title Minimized", "", szDownloadTitle, sizeof(szDownloadTitle), szFileIniConfig);

  if(*szDownloadTitle != '\0')
    wsprintf(gszCurrentDownloadInfo, szDownloadTitle, dwPercentSoFar, gszCurrentDownloadFilename);
  else
    wsprintf(gszCurrentDownloadInfo, "%d%% for all files", dwPercentSoFar);

  /* check and save the current percent so far to minimize flickering */
  if((dwLastPercentSoFar != dwPercentSoFar) || gbDlgDownloadJustMinimized)
  {
    /* When the dialog is has just been minimized, the title is not set
     * until the percentage changes, which when downloading via modem,
     * could take several seconds.  This variable allows us to tell when
     * the dialog had *just* been minimized regardless if the percentage
     * had changed or not. */
    gbDlgDownloadJustMinimized = FALSE;

    /* Set the download dialog title */
    SetWindowText(dlgInfo.hWndDlg, gszCurrentDownloadInfo);
    dwLastPercentSoFar = dwPercentSoFar;
  }
}

void SetRestoredDownloadTitle(void)
{
  /* Set the download dialog title */
  SetWindowText(dlgInfo.hWndDlg, diDownload.szTitle);
}

void GetTotalArchivesToDownload(int *iTotalArchivesToDownload, DWORD *dwTotalEstDownloadSize)
{
  int  iIndex = 0;
  char szIndex[MAX_ITOA];
  char szUrl[MAX_BUF];
  char szSection[MAX_INI_SK];
  char szDownloadSize[MAX_ITOA];

  *dwTotalEstDownloadSize = 0;
  iIndex = 0;
  itoa(iIndex, szIndex, 10);
  lstrcpy(szSection, "File");
  lstrcat(szSection, szIndex);
  GetPrivateProfileString(szSection, "url", "", szUrl, sizeof(szUrl), gszConfigIniFile);
  while(*szUrl != '\0')
  {
    GetPrivateProfileString(szSection, "size", "", szDownloadSize, sizeof(szDownloadSize), gszConfigIniFile);
    itoa(iIndex, szIndex, 10);

    if((lstrlen(szDownloadSize) < 32) && (*szDownloadSize != '\0'))
      /* size will be in kb.  31 bits (int minus the signed bit) of precision should sufice */
      *dwTotalEstDownloadSize += atoi(szDownloadSize);

    ++iIndex;
    itoa(iIndex, szIndex, 10);
    lstrcpy(szSection, "File");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "url", "", szUrl, sizeof(szUrl), gszConfigIniFile);
  }
  *iTotalArchivesToDownload = iIndex;
}

int DownloadViaProxy(char *szUrl, char *szProxyServer, char *szProxyPort, char *szProxyUser, char *szProxyPasswd)
{
  int  rv;
  char proxyURL[kProxySrvrLen];

  rv = nsHTTPConn::OK;
  memset(proxyURL, 0, kProxySrvrLen);
  wsprintf(proxyURL, "http://%s:%s", szProxyServer, szProxyPort);

  nsHTTPConn *conn = new nsHTTPConn(proxyURL);
  if(conn == NULL)
  {
    char szBuf[MAX_BUF_TINY];

    GetPrivateProfileString("Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf), szFileIniConfig);
    PrintError(szBuf, ERROR_CODE_HIDE);

    return(WIZ_OUT_OF_MEMORY);
  }

  if((szProxyUser != NULL) && (*szProxyUser != '\0') &&
     (szProxyPasswd != NULL) && (*szProxyPasswd != '\0'))
    conn->SetProxyInfo(szUrl, szProxyUser, szProxyPasswd);
  else
    conn->SetProxyInfo(szUrl, NULL, NULL);

  if((rv = conn->Open()) != WIZ_OK)
    return(rv);

  rv = conn->Get(ProgressCB, NULL); // use leaf from URL
  conn->Close();

  if(conn)
    delete(conn);

  return(rv);
}

int DownloadViaHTTP(char *szUrl)
{
  int rv;

  rv = nsHTTPConn::OK;
  nsHTTPConn *conn = new nsHTTPConn(szUrl);
  if(conn == NULL)
  {
    char szBuf[MAX_BUF_TINY];

    GetPrivateProfileString("Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf), gszConfigIniFile);
    PrintError(szBuf, ERROR_CODE_HIDE);

    return(WIZ_OUT_OF_MEMORY);
  }
  
  if((rv = conn->Open()) != WIZ_OK)
    return(rv);

  rv = conn->Get(ProgressCB, NULL);
  conn->Close();

  if(conn)
    delete(conn);

  return(rv);
}

int DownloadViaFTP(char *szUrl)
{
  char *host = 0, *path = 0, *file = (char*) kLoclFile;
  int port = 21;
  int rv;

  rv = nsHTTPConn::ParseURL(kFTP, szUrl, &host, &port, &path);

  nsFTPConn *conn = new nsFTPConn(host);
  if(conn == NULL)
  {
    char szBuf[MAX_BUF_TINY];

    GetPrivateProfileString("Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf), szFileIniConfig);
    PrintError(szBuf, ERROR_CODE_HIDE);

    return(WIZ_OUT_OF_MEMORY);
  }

  if((rv = conn->Open()) != WIZ_OK)
    return(rv);

  if(strrchr(path, '/') != (path + strlen(path)))
    file = strrchr(path, '/') + 1; // set to leaf name

  rv = conn->Get(path, file, nsFTPConn::BINARY, 1, ProgressCB);
  conn->Close();

  if(host)
    free(host);
  if(path)
    free(path);
  if(conn)
    delete(conn);

  return(rv);
}

int DownloadFiles(char *szInputIniFile, char *szDownloadDir, char *szProxyServer, char *szProxyPort, char *szProxyUser, char *szProxyPasswd, BOOL bShowRetryMsg, BOOL bIgnoreNetworkError)
{
  char      szIndex[MAX_ITOA];
  char      szSection[MAX_INI_SK];
  char      szSavedCwd[MAX_BUF_MEDIUM];
  int       rv;
  int       iFileDownloadRetries;
  DWORD     dwTotalEstDownloadSize;

  if(szInputIniFile == NULL)
    return(WIZ_ERROR_UNDEFINED);

  GetCurrentDirectory(sizeof(szSavedCwd), szSavedCwd);
  SetCurrentDirectory(szDownloadDir);

  rv                        = WIZ_OK;
  dwTotalEstDownloadSize    = 0;
  giTotalArchivesToDownload = 0;
  glLastBytesSoFar          = 0;
  glAbsoluteBytesSoFar      = 0;
  gbUrlChanged              = TRUE;
  gbDlgDownloadMinimized    = FALSE;
  gbDlgDownloadJustMinimized = FALSE;
  gbCancelDownload          = FALSE;
  gbShowDownloadRetryMsg    = bShowRetryMsg;
  gszConfigIniFile          = szInputIniFile;

  GetTotalArchivesToDownload(&giTotalArchivesToDownload, &dwTotalEstDownloadSize);
  glTotalKb                 = dwTotalEstDownloadSize;

  InitDownloadDlg();

  for(giIndex = 0; giIndex < giTotalArchivesToDownload; giIndex++)
  {
    gbUrlChanged = TRUE;
    itoa(giIndex, szIndex, 10);
    lstrcpy(szSection, "File");
    lstrcat(szSection, szIndex);
    GetPrivateProfileString(szSection, "url", "", gszUrl, sizeof(gszUrl), gszConfigIniFile);
    if(*gszUrl == '\0')
      continue;

    GetPrivateProfileString(szSection, "desc", "", gszCurrentDownloadFileDescription, sizeof(gszCurrentDownloadFileDescription), gszConfigIniFile);

    if(gbDlgDownloadMinimized)
      SetMinimizedDownloadTitle((int)GetPercentSoFar());
    else
    {
      SetStatusUrl();
      SetRestoredDownloadTitle();
    }

    iFileDownloadRetries = 0;
    do
    {
      /* Download starts here */
      if((szProxyServer != NULL) && (szProxyPort != NULL) &&
         (*szProxyServer != '\0') && (*szProxyPort != '\0'))
        /* If proxy info is provided, use HTTP proxy */
        rv = DownloadViaProxy(gszUrl, szProxyServer, szProxyPort, szProxyUser, szProxyPasswd);
      else
      {
        /* is this an HTTP URL? */
        if(strncmp(gszUrl, kHTTP, lstrlen(kHTTP)) == 0)
          rv = DownloadViaHTTP(gszUrl);
        /* or is this an FTP URL? */
        else if(strncmp(gszUrl, kFTP, lstrlen(kFTP)) == 0)
          rv = DownloadViaFTP(gszUrl);
      }

      if(rv == nsFTPConn::E_USER_CANCEL)
      {
        char szBuf[MAX_BUF_MEDIUM];
        char szFilename[MAX_BUF_TINY];

        ParsePath(gszUrl, szFilename, sizeof(szFilename), TRUE, PP_FILENAME_ONLY);
        lstrcpy(szBuf, szDownloadDir);
        AppendBackSlash(szBuf, sizeof(szBuf));
        lstrcat(szBuf, szFilename);

        if(FileExists(szBuf))
          DeleteFile(szBuf);

        break;
      }
    } while((rv != nsFTPConn::E_USER_CANCEL) && (rv != nsFTPConn::OK) && (iFileDownloadRetries++ < MAX_FILE_DOWNLOAD_RETRIES));

    if(rv == nsFTPConn::E_USER_CANCEL)
      /* break out of for() loop */
      break;

    if((rv != nsFTPConn::OK) && (iFileDownloadRetries >= MAX_FILE_DOWNLOAD_RETRIES) && !bIgnoreNetworkError)
    {
      /* too many retries from failed downloads */
      char szMsg[MAX_BUF];

      GetPrivateProfileString("Strings", "Error Too Many Network Errors", "", szMsg, sizeof(szMsg), szFileIniConfig);
      if(*szMsg != '\0')
        PrintError(szMsg, ERROR_CODE_HIDE);

      /* Set return value and break out of for() loop.
       * We don't want to continue if there were too
       * many network errors on any file. */
      rv = WIZ_TOO_MANY_NETWORK_ERRORS;
      break;
    }
  }

  DeInitDownloadDlg();
  SetCurrentDirectory(szSavedCwd);
  return(rv);
}

int ProgressCB(int aBytesSoFar, int aTotalFinalSize)
{
  long lBytesDiffSoFar;
  double dPercentSoFar;
  int  iRv = nsFTPConn::OK;

  if(sgProduct.dwMode != SILENT)
  {
    SetStatusUrl();

    if(glTotalKb == 0)
      glTotalKb = aTotalFinalSize;

    /* Calculate the difference between the last set of bytes read against
     * the current set of bytes read.  If the value is negative, that means
     * that it started a new file, so reset lBytesDiffSoFar to aBytesSoFar */
    lBytesDiffSoFar = ((aBytesSoFar - glLastBytesSoFar) < 1) ? aBytesSoFar : (aBytesSoFar - glLastBytesSoFar);

    /* Save the current bytes read as the last set of bytes read */
    glLastBytesSoFar = aBytesSoFar;
    glAbsoluteBytesSoFar += lBytesDiffSoFar;

    dPercentSoFar = GetPercentSoFar();
    if(gbDlgDownloadMinimized)
      SetMinimizedDownloadTitle((int)dPercentSoFar);

    UpdateGaugeFileProgressBar(dPercentSoFar);
    SetStatusStatus();

    if(gbCancelDownload)
      iRv = nsFTPConn::E_USER_CANCEL;
  }

  ProcessWindowsMessages();
  return(iRv);
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
DownloadDlgProc(HWND hWndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
  {
		case WM_INITDIALOG:
      DisableSystemMenuItems(hWndDlg, FALSE);
			CenterWindow(hWndDlg);
      if(gbShowDownloadRetryMsg)
        SetDlgItemText(hWndDlg, IDC_MESSAGE0, diDownload.szMessageRetry0);
      else
        SetDlgItemText(hWndDlg, IDC_MESSAGE0, diDownload.szMessageDownload0);

			return FALSE;

		case WM_SIZE:
      switch(wParam)
      {
        case SIZE_MINIMIZED:
          SetMinimizedDownloadTitle((int)GetPercentSoFar());
          gbDlgDownloadMinimized = TRUE;
          gbDlgDownloadJustMinimized = TRUE;
          break;

        case SIZE_RESTORED:
          SetStatusUrl();
          SetRestoredDownloadTitle();
          gbDlgDownloadMinimized = FALSE;
          break;
      }
      return(FALSE);

		case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDCANCEL:
          if(AskCancelDlg(hWndDlg))
            gbCancelDownload = TRUE;

          break;

        default:
          break;
      }
			return(TRUE);
	}

	return(FALSE);  // didn't handle the message
}

// This routine will update the File Gauge progress bar to the specified percentage
// (value between 0 and 100)
static void
UpdateGaugeFileProgressBar(double value)
{
	int	nBars;

  if(sgProduct.dwMode != SILENT)
  {
    // Figure out how many bars should be displayed
    nBars = (int)(dlgInfo.nMaxFileBars * value / 100);

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
	  rect.left     = rect.top = BAR_LIBXPNET_MARGIN;
	  rect.bottom  -= BAR_LIBXPNET_MARGIN;
	  rect.right    = rect.left + BAR_LIBXPNET_WIDTH;

	  for(i = 0; i < nBars; i++)
    {
		  RECT	dest;

		  if(IntersectRect(&dest, &ps.rcPaint, &rect))
			  FillRect(hDC, &rect, hBrush);

      OffsetRect(&rect, BAR_LIBXPNET_WIDTH + BAR_LIBXPNET_SPACING, 0);
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
	cx = 2 * GetSystemMetrics(SM_CXBORDER) + 2 * BAR_LIBXPNET_MARGIN +
		nMaxBars * BAR_LIBXPNET_WIDTH + (nMaxBars - 1) * BAR_LIBXPNET_SPACING;

	SetWindowPos(hWnd, NULL, -1, -1, cx, rect.bottom - rect.top,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Window proc for Download gauge
BOOL CALLBACK
GaugeDownloadWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
			dlgInfo.nMaxFileBars = (rect.right - rect.left - 2 * BAR_LIBXPNET_MARGIN + BAR_LIBXPNET_SPACING) / (BAR_LIBXPNET_WIDTH + BAR_LIBXPNET_SPACING);

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

void InitDownloadDlg(void)
{
	WNDCLASS	wc;

  if(sgProduct.dwMode != SILENT)
  {
    memset(&wc, 0, sizeof(wc));
    wc.style          = CS_GLOBALCLASS;
    wc.hInstance      = hInst;
    wc.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpfnWndProc    = (WNDPROC)GaugeDownloadWndProc;
    wc.lpszClassName  = "GaugeFile";
    RegisterClass(&wc);

    // Display the dialog box
    dlgInfo.hWndDlg = CreateDialog(hSetupRscInst, MAKEINTRESOURCE(DLG_DOWNLOADING), hWndMain, (DLGPROC)DownloadDlgProc);
    UpdateWindow(dlgInfo.hWndDlg);
    UpdateGaugeFileProgressBar(0);
  }
}

void DeInitDownloadDlg()
{
  if(sgProduct.dwMode != SILENT)
  {
    DestroyWindow(dlgInfo.hWndDlg);
    UnregisterClass("GaugeFile", hInst);
  }
}
