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
 * The Original Code is Mozilla Navigator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
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

#include <windows.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

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
#include "nsSocket.h"

#define UPDATE_INTERVAL_STATUS          1
#define UPDATE_INTERVAL_PROGRESS_BAR    1

/* Cancel Status */
#define CS_NONE         0x00000000
#define CS_CANCEL       0x00000001
#define CS_PAUSE        0x00000002
#define CS_RESUME       0x00000003

const int  kProxySrvrLen = 1024;
const char kHTTP[8]      = "http://";
const char kFTP[7]       = "ftp://";
const char kLoclFile[7]  = "zzzFTP";
const int  kModTimeOutValue = 3;

static nsHTTPConn       *connHTTP = NULL;
static nsFTPConn        *connFTP = NULL;
static long             glLastBytesSoFar;
static long             glAbsoluteBytesSoFar;
static long             glBytesResumedFrom;
static long             glTotalKb;
char                    gszStrCopyingFile[MAX_BUF_MEDIUM];
char                    gszCurrentDownloadPath[MAX_BUF];
char                    gszCurrentDownloadFilename[MAX_BUF_TINY];
char                    gszCurrentDownloadFileDescription[MAX_BUF_TINY];
char                    gszUrl[MAX_BUF];
char                    gszTo[MAX_BUF];
char                    gszFileInfo[MAX_BUF];
char                    *gszConfigIniFile;
BOOL                    gbDlgDownloadMinimized;
BOOL                    gbDlgDownloadJustMinimized;
BOOL                    gbUrlChanged;
BOOL                    gbShowDownloadRetryMsg;
DWORD                   gdwDownloadDialogStatus;
int                     giIndex;
int                     giTotalArchivesToDownload;
DWORD                   gdwTickStart;
BOOL                    gbStartTickCounter;

double GetPercentSoFar(void);

static void UpdateGaugeFileProgressBar(double value);
       int  ProgressCB(int aBytesSoFar, int aTotalFinalSize);
       void InitDownloadDlg(void);
       void DeInitDownloadDlg();

/* local prototypes */
siC *GetObjectFromArchiveName(char *szArchiveName);

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

struct TickInfo
{
  DWORD dwTickBegin;
  DWORD dwTickEnd;
  DWORD dwTickDif;
  BOOL  bTickStarted;
  BOOL  bTickDownloadResumed;
} gtiPaused;

BOOL CheckInterval(long *lModLastValue, int iInterval)
{
  BOOL bRv = FALSE;
  long lModCurrentValue;

  if(iInterval == 1)
  {
    lModCurrentValue = time(NULL);
    if(lModCurrentValue != *lModLastValue)
      bRv = TRUE;
  }
  else
  {
    lModCurrentValue = time(NULL) % iInterval;
    if((lModCurrentValue == 0) && (*lModLastValue != 0))
      bRv = TRUE;
  }

  *lModLastValue = lModCurrentValue;
  return(bRv);
}

char *GetTimeLeft(DWORD dwTimeLeft,
                  char *szTimeString,
                  DWORD dwTimeStringBufSize)
{
  DWORD      dwTimeLeftPP;
  SYSTEMTIME stTime;

  ZeroMemory(&stTime, sizeof(stTime));
  dwTimeLeftPP         = dwTimeLeft + 1;
  stTime.wHour         = (unsigned)(dwTimeLeftPP / 60 / 60);
  stTime.wMinute       = (unsigned)((dwTimeLeftPP / 60) % 60);
  stTime.wSecond       = (unsigned)(dwTimeLeftPP % 60);

  ZeroMemory(szTimeString, dwTimeStringBufSize);
  /* format time string using user's local time format information */
  GetTimeFormat(LOCALE_USER_DEFAULT,
                TIME_NOTIMEMARKER|TIME_FORCE24HOURFORMAT,
                &stTime,
                NULL,
                szTimeString,
                dwTimeStringBufSize);

  return(szTimeString);
}

DWORD AddToTick(DWORD dwTick, DWORD dwTickMoreToAdd)
{
  DWORD dwTickLeftTillWrap = 0;

  /* Since GetTickCount() is the number of milliseconds since the system
   * has been on and the return value is a DWORD, this value will wrap
   * every 49.71 days or 0xFFFFFFFF milliseconds. */
  dwTickLeftTillWrap = 0xFFFFFFFF - dwTick;
  if(dwTickMoreToAdd > dwTickLeftTillWrap)
    dwTick = dwTickMoreToAdd - dwTickLeftTillWrap;
  else
    dwTick = dwTick + dwTickMoreToAdd;

  return(dwTick);
}

DWORD GetTickDif(DWORD dwTickEnd, DWORD dwTickStart)
{
  DWORD dwTickDif;

  /* Since GetTickCount() is the number of milliseconds since the system
   * has been on and the return value is a DWORD, this value will wrap
   * every 49.71 days or 0xFFFFFFFF milliseconds. 
   *
   * Assumption: dwTickEnd has not wrapped _and_ passed dwTickStart */
  if(dwTickEnd < dwTickStart)
    dwTickDif = 0xFFFFFFFF - dwTickStart + dwTickEnd;
  else
    dwTickDif = dwTickEnd - dwTickStart;

  return(dwTickDif);
}

void InitTickInfo(void)
{
  gtiPaused.dwTickBegin          = 0;
  gtiPaused.dwTickEnd            = 0;
  gtiPaused.dwTickDif            = 0;
  gtiPaused.bTickStarted         = FALSE;
  gtiPaused.bTickDownloadResumed = FALSE;
}

DWORD RoundDouble(double dValue)
{
  if(0.5 <= (dValue - (DWORD)dValue))
    return((DWORD)dValue + 1);
  else
    return((DWORD)dValue);
}

void SetStatusStatus(void)
{
  char        szStatusStatusLine[MAX_BUF_MEDIUM];
  char        szCurrentStatusInfo[MAX_BUF_MEDIUM];
  char        szPercentString[MAX_BUF_MEDIUM];
  char        szPercentageCompleted[MAX_BUF_MEDIUM];
  static long lModLastValue = 0;
  double        dRate;
  static double dRateCounter;
  DWORD         dwTickNow;
  DWORD         dwTickDif;
  DWORD         dwKBytesSoFar;
  DWORD         dwRoundedRate;
  char          szTimeLeft[MAX_BUF_TINY];

  /* If the user just clicked on the Resume button, then the time lapsed
   * between gdwTickStart and when the Resume button was clicked needs to
   * be subtracted taken into account when calculating dwTickDif.  So
   * "this" lapsed time needs to be added to gdwTickStart. */
  if(gtiPaused.bTickDownloadResumed)
  {
    gdwTickStart = AddToTick(gdwTickStart, gtiPaused.dwTickDif);
    InitTickInfo();
  }

  /* GetTickCount() returns time in milliseconds.  This is more accurate,
   * which will allow us to get at a 2 decimal precision value for the
   * download rate. */
  dwTickNow = GetTickCount();
  if((gdwTickStart == 0) && gbStartTickCounter)
    dwTickNow = gdwTickStart = GetTickCount();

  dwTickDif = GetTickDif(dwTickNow, gdwTickStart);

  /* Only update the UI every UPDATE_INTERVAL_STATUS interval,
   * which is currently set to 1 sec. */
  if(!CheckInterval(&lModLastValue, UPDATE_INTERVAL_STATUS))
    return;

  if(glAbsoluteBytesSoFar == 0)
    dRateCounter = 0.0;
  else
    dRateCounter = dwTickDif / 1000;

  if(dRateCounter == 0.0)
    dRate = 0.0;
  else
    dRate = (glAbsoluteBytesSoFar - glBytesResumedFrom) / dRateCounter / 1024;

  dwKBytesSoFar = glAbsoluteBytesSoFar / 1024;

  /* Use a rate that is rounded to the nearest integer.  If dRate used directly,
   * the "Time Left" will jump around quite a bit due to the rate usually 
   * varying up and down by quite a bit. The rounded rate give a "more linear"
   * count down of the "Time Left". */
  dwRoundedRate = RoundDouble(dRate);
  if(dwRoundedRate > 0)
    GetTimeLeft((glTotalKb - dwKBytesSoFar) / dwRoundedRate,
                 szTimeLeft,
                 sizeof(szTimeLeft));
  else
    lstrcpy(szTimeLeft, "00:00:00");

  if(!gbShowDownloadRetryMsg)
  {
    GetPrivateProfileString("Strings",
                            "Status Download",
                            "",
                            szStatusStatusLine,
                            sizeof(szStatusStatusLine),
                            szFileIniConfig);
    if(*szStatusStatusLine != '\0')
      sprintf(szCurrentStatusInfo,
              szStatusStatusLine,
              szTimeLeft,
              dRate,
              dwKBytesSoFar,
              glTotalKb);
    else
      sprintf(szCurrentStatusInfo,
              "%s at %.2fKB/sec (%uKB of %uKB downloaded)",
              szTimeLeft,
              dRate,
              dwKBytesSoFar,
              glTotalKb);
  }
  else
  {
    GetPrivateProfileString("Strings",
                            "Status Retry",
                            "",
                            szStatusStatusLine,
                            sizeof(szStatusStatusLine),
                            szFileIniConfig);
    if(*szStatusStatusLine != '\0')
      sprintf(szCurrentStatusInfo,
              szStatusStatusLine,
              szTimeLeft,
              dRate,
              dwKBytesSoFar,
              glTotalKb);
    else
      sprintf(szCurrentStatusInfo,
              "%s at %.2KB/sec (%uKB of %uKB downloaded)",
              szTimeLeft,
              dRate,
              dwKBytesSoFar,
              glTotalKb);
  }

  GetPrivateProfileString("Strings",
                          "Status Percentage Completed",
                          "",
                          szPercentageCompleted,
                          sizeof(szPercentageCompleted),
                          szFileIniConfig);
  wsprintf(szPercentString, szPercentageCompleted, (int)GetPercentSoFar());

  /* Set the download dialog title */
  SetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS_STATUS, szCurrentStatusInfo);
  SetDlgItemText(dlgInfo.hWndDlg, IDC_PERCENTAGE, szPercentString);
}

void SetStatusFile(void)
{
  char szString[MAX_BUF];

  /* Set the download dialog status*/
  wsprintf(szString, gszFileInfo, gszCurrentDownloadFileDescription);
  SetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS_FILE, szString);
  SetStatusStatus();
}

void SetStatusUrl(void)
{
  /* display the current url being processed */
  if(gbUrlChanged)
  {
    char szUrlPathBuf[MAX_BUF];
    char szToPathBuf[MAX_BUF];
    HWND hStatusUrl = NULL;
    HWND hStatusTo  = NULL;

    hStatusUrl = GetDlgItem(dlgInfo.hWndDlg, IDC_STATUS_URL);
    if(hStatusUrl)
      TruncateString(hStatusUrl, gszUrl, szUrlPathBuf, sizeof(szUrlPathBuf));
    else
      lstrcpy(szUrlPathBuf, gszUrl);

    hStatusTo = GetDlgItem(dlgInfo.hWndDlg, IDC_STATUS_TO);
    if(hStatusTo)
      TruncateString(hStatusTo, gszTo, szToPathBuf, sizeof(szToPathBuf));
    else
      lstrcpy(szToPathBuf, gszTo);

    SetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS_URL, szUrlPathBuf);
    SetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS_TO,  szToPathBuf);
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
  char szUrl[MAX_BUF];
  char szSection[MAX_INI_SK];
  char szDownloadSize[MAX_ITOA];

  *dwTotalEstDownloadSize = 0;
  iIndex = 0;
  wsprintf(szSection, "File%d", iIndex);
  GetPrivateProfileString(szSection,
                          "url0",
                          "",
                          szUrl,
                          sizeof(szUrl),
                          gszConfigIniFile);
  while(*szUrl != '\0')
  {
    GetPrivateProfileString(szSection, "size", "", szDownloadSize, sizeof(szDownloadSize), gszConfigIniFile);
    if((lstrlen(szDownloadSize) < 32) && (*szDownloadSize != '\0'))
      /* size will be in kb.  31 bits (int minus the signed bit) of precision should sufice */
      *dwTotalEstDownloadSize += atoi(szDownloadSize);

    ++iIndex;
    wsprintf(szSection, "File%d", iIndex);
    GetPrivateProfileString(szSection,
                            "url0",
                            "",
                            szUrl,
                            sizeof(szUrl),
                            gszConfigIniFile);
  }
  *iTotalArchivesToDownload = iIndex;
}

/* 
 * Name: ProcessWndMsgCB
 *
 * Arguments: None
 *
 * Description: Callback function invoked by socket code and by FTP and HTTP layers
 *				to give the UI a chance to breath while we are in a look processing
 *				incoming data, or looping in select()
 *
 * Author: syd@netscape.com 5/11/2001
 *
*/

int
ProcessWndMsgCB()
{
  int iRv = nsFTPConn::OK;

	ProcessWindowsMessages();
  if((gdwDownloadDialogStatus == CS_CANCEL) ||
     (gdwDownloadDialogStatus == CS_PAUSE))
    iRv = nsFTPConn::E_USER_CANCEL;

	return(iRv);
}

/* Function used only to send the message stream error */
int WGet(char *szUrl,
         char *szFile,
         char *szProxyServer,
         char *szProxyPort,
         char *szProxyUser,
         char *szProxyPasswd)
{
  int        rv;
  char       proxyURL[MAX_BUF];
  nsHTTPConn *conn = NULL;

  if((szProxyServer != NULL) && (szProxyPort != NULL) &&
     (*szProxyServer != '\0') && (*szProxyPort != '\0'))
  {
    /* detected proxy information, let's use it */
    memset(proxyURL, 0, sizeof(proxyURL));
    wsprintf(proxyURL, "http://%s:%s", szProxyServer, szProxyPort);

    conn = new nsHTTPConn(proxyURL);
    if(conn == NULL)
      return(WIZ_OUT_OF_MEMORY);

    if((szProxyUser != NULL) && (*szProxyUser != '\0') &&
       (szProxyPasswd != NULL) && (*szProxyPasswd != '\0'))
      /* detected user and password info */
      conn->SetProxyInfo(szUrl, szProxyUser, szProxyPasswd);
    else
      conn->SetProxyInfo(szUrl, NULL, NULL);
  }
  else
  {
    /* no proxy information supplied. set up normal http object */
    conn = new nsHTTPConn(szUrl, ProcessWndMsgCB);
    if(conn == NULL)
      return(WIZ_OUT_OF_MEMORY);
  }
  
  rv = conn->Open();
  if(rv == WIZ_OK)
  {
    rv = conn->Get(NULL, szFile);
    conn->Close();
  }

  if(conn)
    delete(conn);

  return(rv);
}

int DownloadViaProxyOpen(char *szUrl, char *szProxyServer, char *szProxyPort, char *szProxyUser, char *szProxyPasswd)
{
  int  rv;
  char proxyURL[kProxySrvrLen];

  if((!szUrl) || (*szUrl == '\0'))
    return nsHTTPConn::E_PARAM;

  rv = nsHTTPConn::OK;
  memset(proxyURL, 0, kProxySrvrLen);
  wsprintf(proxyURL, "http://%s:%s", szProxyServer, szProxyPort);

  connHTTP = new nsHTTPConn(proxyURL, ProcessWndMsgCB);
  if(connHTTP == NULL)
  {
    char szBuf[MAX_BUF_TINY];

    GetPrivateProfileString("Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf), szFileIniConfig);
    PrintError(szBuf, ERROR_CODE_HIDE);

    return(WIZ_OUT_OF_MEMORY);
  }

  if((szProxyUser != NULL) && (*szProxyUser != '\0') &&
     (szProxyPasswd != NULL) && (*szProxyPasswd != '\0'))
    connHTTP->SetProxyInfo(szUrl, szProxyUser, szProxyPasswd);
  else
    connHTTP->SetProxyInfo(szUrl, NULL, NULL);

  rv = connHTTP->Open();
  return(rv);
}

void DownloadViaProxyClose(void)
{
  gbStartTickCounter = FALSE;
  if(connHTTP)
  {
    connHTTP->Close();
    delete(connHTTP);
    connHTTP = NULL;
  }
}

int DownloadViaProxy(char *szUrl, char *szProxyServer, char *szProxyPort, char *szProxyUser, char *szProxyPasswd)
{
  int  rv;
  char *file = NULL;

  rv = nsHTTPConn::OK;
  if((!szUrl) || (*szUrl == '\0'))
    return nsHTTPConn::E_PARAM;

  if(connHTTP == NULL)
  {
    rv = DownloadViaProxyOpen(szUrl,
                              szProxyServer,
                              szProxyPort,
                              szProxyUser,
                              szProxyPasswd);

    if(rv != nsHTTPConn::OK)
    {
      DownloadViaProxyClose();
      return(rv);
    }
  }

  if(connHTTP == NULL)
  {
    char szBuf[MAX_BUF_TINY];

    GetPrivateProfileString("Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf), szFileIniConfig);
    PrintError(szBuf, ERROR_CODE_HIDE);

    return(WIZ_OUT_OF_MEMORY);
  }

  if(strrchr(szUrl, '/') != (szUrl + strlen(szUrl)))
    file = strrchr(szUrl, '/') + 1; // set to leaf name

  gbStartTickCounter = TRUE;
  rv = connHTTP->Get(ProgressCB, file); // use leaf from URL
  DownloadViaProxyClose();
  return(rv);
}

int DownloadViaHTTPOpen(char *szUrl)
{
  int  rv;

  if((!szUrl) || (*szUrl == '\0'))
    return nsHTTPConn::E_PARAM;

  rv = nsHTTPConn::OK;
  connHTTP = new nsHTTPConn(szUrl, ProcessWndMsgCB);
  if(connHTTP == NULL)
  {
    char szBuf[MAX_BUF_TINY];

    GetPrivateProfileString("Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf), gszConfigIniFile);
    PrintError(szBuf, ERROR_CODE_HIDE);

    return(WIZ_OUT_OF_MEMORY);
  }
  
  rv = connHTTP->Open();
  return(rv);
}

void DownloadViaHTTPClose(void)
{
  gbStartTickCounter = FALSE;
  if(connHTTP)
  {
    connHTTP->Close();
    delete(connHTTP);
    connHTTP = NULL;
  }
}

int DownloadViaHTTP(char *szUrl)
{
  int  rv;
  char *file = NULL;

  if((!szUrl) || (*szUrl == '\0'))
    return nsHTTPConn::E_PARAM;

  if(connHTTP == NULL)
  {
    rv = DownloadViaHTTPOpen(szUrl);
    if(rv != nsHTTPConn::OK)
    {
      DownloadViaHTTPClose();
      return(rv);
    }
  }

  if(connHTTP == NULL)
  {
    char szBuf[MAX_BUF_TINY];

    GetPrivateProfileString("Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf), gszConfigIniFile);
    PrintError(szBuf, ERROR_CODE_HIDE);

    return(WIZ_OUT_OF_MEMORY);
  }
  
  rv = nsHTTPConn::OK;
  if(strrchr(szUrl, '/') != (szUrl + strlen(szUrl)))
    file = strrchr(szUrl, '/') + 1; // set to leaf name

  gbStartTickCounter = TRUE;
  rv = connHTTP->Get(ProgressCB, file);
  DownloadViaHTTPClose();
  return(rv);
}

int DownloadViaFTPOpen(char *szUrl)
{
  char *host = 0, *path = 0, *file = (char*) kLoclFile;
  int port = 21;
  int rv;

  if((!szUrl) || (*szUrl == '\0'))
    return nsFTPConn::E_PARAM;

  rv = nsHTTPConn::ParseURL(kFTP, szUrl, &host, &port, &path);

  connFTP = new nsFTPConn(host, ProcessWndMsgCB);
  if(connFTP == NULL)
  {
    char szBuf[MAX_BUF_TINY];

    GetPrivateProfileString("Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf), szFileIniConfig);
    PrintError(szBuf, ERROR_CODE_HIDE);

    return(WIZ_OUT_OF_MEMORY);
  }

  rv = connFTP->Open();
  if(host)
    free(host);
  if(path)
    free(path);

  return(rv);
}

void DownloadViaFTPClose(void)
{
  gbStartTickCounter = FALSE;
  if(connFTP)
  {
    connFTP->Close();
    delete(connFTP);
    connFTP = NULL;
  }
}

int DownloadViaFTP(char *szUrl)
{
  char *host = 0, *path = 0, *file = (char*) kLoclFile;
  int port = 21;
  int rv;

  if((!szUrl) || (*szUrl == '\0'))
    return nsFTPConn::E_PARAM;

  if(connFTP == NULL)
  {
    rv = DownloadViaFTPOpen(szUrl);
    if(rv != nsFTPConn::OK)
    {
      DownloadViaFTPClose();
      return(rv);
    }
  }

  if(connFTP == NULL)
  {
    char szBuf[MAX_BUF_TINY];

    GetPrivateProfileString("Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf), szFileIniConfig);
    PrintError(szBuf, ERROR_CODE_HIDE);

    return(WIZ_OUT_OF_MEMORY);
  }

  rv = nsHTTPConn::ParseURL(kFTP, szUrl, &host, &port, &path);

  if(strrchr(path, '/') != (path + strlen(path)))
    file = strrchr(path, '/') + 1; // set to leaf name

  gbStartTickCounter = TRUE;
  rv = connFTP->Get(path, file, nsFTPConn::BINARY, TRUE, ProgressCB);

  if(host)
    free(host);
  if(path)
    free(path);

  return(rv);
}

void PauseTheDownload(int rv, int *iFileDownloadRetries)
{
  if(rv != nsFTPConn::E_USER_CANCEL)
  {
    SendMessage(dlgInfo.hWndDlg, WM_COMMAND, IDPAUSE, 0);
    --*iFileDownloadRetries;
  }

  while(gdwDownloadDialogStatus == CS_PAUSE)
  {
    SleepEx(200, FALSE);
    ProcessWindowsMessages();
  }
}

void CloseSocket(char *szProxyServer, char *szProxyPort)
{
  /* Close the socket connection from the first attempt. */
  if((szProxyServer != NULL) && (szProxyPort != NULL) &&
     (*szProxyServer != '\0') && (*szProxyPort != '\0'))
      DownloadViaProxyClose();
  else
  {
    /* is this an HTTP URL? */
    if(strncmp(gszUrl, kHTTP, lstrlen(kHTTP)) == 0)
      DownloadViaHTTPClose();
    /* or is this an FTP URL? */
    else if(strncmp(gszUrl, kFTP, lstrlen(kFTP)) == 0)
      DownloadViaFTPClose();
  }
}

siC *GetObjectFromArchiveName(char *szArchiveName)
{
  DWORD dwIndex;
  siC   *siCObject = NULL;
  siC   *siCNode   = NULL;

  dwIndex = 0;
  siCObject = SiCNodeGetObject(dwIndex, TRUE, AC_ALL);
  while(siCObject)
  {
    if(lstrcmpi(szArchiveName, siCObject->szArchiveName) == 0)
    {
      siCNode = siCObject;
      break;
    }

    ++dwIndex;
    siCObject = SiCNodeGetObject(dwIndex, TRUE, AC_ALL);
  }

  return(siCNode);
}

int DownloadFiles(char *szInputIniFile,
                  char *szDownloadDir,
                  char *szProxyServer,
                  char *szProxyPort,
                  char *szProxyUser,
                  char *szProxyPasswd,
                  BOOL bShowRetryMsg,
                  BOOL bIgnoreAllNetworkErrors,
                  char *szFailedFile,
                  DWORD dwFailedFileSize)
{
  char      szBuf[MAX_BUF];
  char      szCurrentFile[MAX_BUF];
  char      szSection[MAX_INI_SK];
  char      szKey[MAX_INI_SK];
  char      szSavedCwd[MAX_BUF_MEDIUM];
  int       iCounter;
  int       rv;
  int       iFileDownloadRetries;
  int       iIgnoreFileNetworkError;
  int       iLocalTimeOutCounter;
  DWORD     dwTotalEstDownloadSize;
  char      szPartiallyDownloadedFilename[MAX_BUF];
  BOOL      bDownloadInitiated;
  char      szTempURL[MAX_BUF];
  char      szWorkingURLPathOnly[MAX_BUF];
  siC       *siCCurrentFileObj = NULL;

  ZeroMemory(szTempURL, sizeof(szTempURL));
  ZeroMemory(szWorkingURLPathOnly, sizeof(szWorkingURLPathOnly));
  if(szInputIniFile == NULL)
    return(WIZ_ERROR_UNDEFINED);

  if(szFailedFile)
    ZeroMemory(szFailedFile, dwFailedFileSize);

  InitTickInfo();
  GetCurrentDirectory(sizeof(szSavedCwd), szSavedCwd);
  SetCurrentDirectory(szDownloadDir);

  rv                        = WIZ_OK;
  dwTotalEstDownloadSize    = 0;
  giTotalArchivesToDownload = 0;
  glLastBytesSoFar          = 0;
  glAbsoluteBytesSoFar      = 0;
  glBytesResumedFrom        = 0;
  gdwTickStart              = 0; /* Initialize the counter used to
                                  * calculate download rate */
  gbStartTickCounter        = FALSE; /* used to determine when to start
                                      * the tick counter used to calculate
                                      * the download rate */
  gbUrlChanged              = TRUE;
  gbDlgDownloadMinimized    = FALSE;
  gbDlgDownloadJustMinimized = FALSE;
  gdwDownloadDialogStatus   = CS_NONE;
  gbShowDownloadRetryMsg    = bShowRetryMsg;
  gszConfigIniFile          = szInputIniFile;
  bDownloadInitiated        = FALSE;

  GetTotalArchivesToDownload(&giTotalArchivesToDownload,
                             &dwTotalEstDownloadSize);
  glTotalKb                 = dwTotalEstDownloadSize;
  GetSetupCurrentDownloadFile(szPartiallyDownloadedFilename,
                              sizeof(szPartiallyDownloadedFilename));

  ShowMessage(NULL, FALSE);
  InitDownloadDlg();

  for(giIndex = 0; giIndex < giTotalArchivesToDownload; giIndex++)
  {
    /* set (or reset) the counter to 0 in order to read the
     * next files's 0'th url from the .idi file */
    iCounter     = 0;
    gbUrlChanged = TRUE; /* Update the download dialog with new URL */
    wsprintf(szSection, "File%d", giIndex);
    wsprintf(szKey,     "url%d",  iCounter);
    GetPrivateProfileString(szSection,
                            szKey,
                            "",
                            szTempURL,
                            sizeof(szTempURL),
                            gszConfigIniFile);

    if(*szTempURL == '\0')
      continue;

    if(!bDownloadInitiated)
    {
      ParsePath(szTempURL,
                szWorkingURLPathOnly,
                sizeof(szWorkingURLPathOnly),
                TRUE, //use '/' as the path delimiter
                PP_PATH_ONLY);
    }

    GetPrivateProfileString(szSection,
                            "desc",
                            "",
                            gszCurrentDownloadFileDescription,
                            sizeof(gszCurrentDownloadFileDescription),
                            gszConfigIniFile);
    iIgnoreFileNetworkError = GetPrivateProfileInt(szSection,
                            "Ignore File Network Error",
                            0,
                            gszConfigIniFile);

    /* save the file name to be downloaded */
    ParsePath(szTempURL,
              szCurrentFile,
              sizeof(szCurrentFile),
              TRUE, //use '/' as the path delimiter
              PP_FILENAME_ONLY);

    RemoveSlash(szWorkingURLPathOnly);
    wsprintf(gszUrl, "%s/%s", szWorkingURLPathOnly, szCurrentFile);

    /* retrieve the file's data structure */
    siCCurrentFileObj = GetObjectFromArchiveName(szCurrentFile);

    if((*szPartiallyDownloadedFilename != 0) &&
       (lstrcmpi(szPartiallyDownloadedFilename, szCurrentFile) == 0))
    {
      struct stat statBuf;

      if(stat(szPartiallyDownloadedFilename, &statBuf) != -1)
      {
        glAbsoluteBytesSoFar += statBuf.st_size;
        glBytesResumedFrom    = statBuf.st_size;
      }
    }

    lstrcpy(gszTo, szDownloadDir);
    AppendBackSlash(gszTo, sizeof(gszTo));
    lstrcat(gszTo, szCurrentFile);

    if(gbDlgDownloadMinimized)
      SetMinimizedDownloadTitle((int)GetPercentSoFar());
    else
    {
      SetStatusUrl();
      SetRestoredDownloadTitle();
    }

    SetSetupCurrentDownloadFile(szCurrentFile);
    iFileDownloadRetries = 0;
    iLocalTimeOutCounter = 0;
    do
    {
      ProcessWindowsMessages();
      /* Download starts here */
      if((szProxyServer != NULL) && (szProxyPort != NULL) &&
         (*szProxyServer != '\0') && (*szProxyPort != '\0'))
        /* If proxy info is provided, use HTTP proxy */
        rv = DownloadViaProxy(gszUrl,
                              szProxyServer,
                              szProxyPort,
                              szProxyUser,
                              szProxyPasswd);
      else
      {
        /* is this an HTTP URL? */
        if(strncmp(gszUrl, kHTTP, lstrlen(kHTTP)) == 0)
          rv = DownloadViaHTTP(gszUrl);
        /* or is this an FTP URL? */
        else if(strncmp(gszUrl, kFTP, lstrlen(kFTP)) == 0)
          rv = DownloadViaFTP(gszUrl);
      }

      bDownloadInitiated = TRUE;
      if((rv == nsFTPConn::E_USER_CANCEL) ||
         (gdwDownloadDialogStatus == CS_PAUSE))
      {
        if(gdwDownloadDialogStatus == CS_PAUSE)
        {
          CloseSocket(szProxyServer, szProxyPort);

          /* rv needs to be set to something
           * other than E_USER_CANCEL or E_OK */
          rv = nsFTPConn::E_CMD_UNEXPECTED;

          PauseTheDownload(rv, &iFileDownloadRetries);
          bDownloadInitiated = FALSE; /* restart the download using
                                       * new socket connection */
        }
        else
        {
          /* user canceled; break out of the do loop */
          break;
        }
      }
      else if((rv != nsFTPConn::OK) &&
              (rv != nsFTPConn::E_CMD_FAIL) &&
              (rv != nsSocket::E_BIND) &&
              (rv != nsHTTPConn::E_HTTP_RESPONSE) &&
              (gdwDownloadDialogStatus != CS_CANCEL))
      {
        /* We timed out.  No response from the server, or 
         * we somehow lost connection. */

        char szTitle[MAX_BUF_SMALL];
        char szMsgDownloadPaused[MAX_BUF];

        /* Incrememt the time out counter on E_TIMEOUT */
        if(rv == nsSocket::E_TIMEOUT)
        {
          ++siCCurrentFileObj->iNetTimeOuts;
          ++iLocalTimeOutCounter;
        }

        CloseSocket(szProxyServer, szProxyPort);

        /* If the number of timeouts is %3 == 0, then let's pause
         * the download process.  Otherwise, just close the
         * connection and open a new one to see if the download
         * can be restarted automatically. */
        if((rv != nsSocket::E_TIMEOUT) ||
           (rv == nsSocket::E_TIMEOUT) && ((iLocalTimeOutCounter % kModTimeOutValue) == 0))
        {
          /* Start the pause tick counter here because we don't know how
           * long before the user will dismiss the MessageBox() */
          if(!gtiPaused.bTickStarted)
          {
            gtiPaused.dwTickBegin          = GetTickCount();
            gtiPaused.bTickStarted         = TRUE;
            gtiPaused.bTickDownloadResumed = FALSE;
          }

          /* The connection unexepectedly dropped for some reason, so inform
           * the user that the download will be Paused, and then update the
           * Download dialog to show the Paused state. */
          GetPrivateProfileString("Messages",
                                  "MB_WARNING_STR",
                                  "",
                                  szTitle,
                                  sizeof(szTitle),
                                  szFileIniInstall);
          GetPrivateProfileString("Strings",
                                  "Message Download Paused",
                                  "",
                                  szMsgDownloadPaused,
                                  sizeof(szMsgDownloadPaused),
                                  szFileIniConfig);
          MessageBox(dlgInfo.hWndDlg,
                     szMsgDownloadPaused,
                     szTitle,
                     MB_ICONEXCLAMATION);

          /* Let's make sure we're in a paused state */
          gdwDownloadDialogStatus = CS_PAUSE;
          PauseTheDownload(rv, &iFileDownloadRetries);
        }
        else
          /* Let's make sure we're _not_ in a paused state */
          gdwDownloadDialogStatus = CS_NONE;
      }

      /* We don't count time outs as normal failures.  We're
       * keeping track of time outs differently. */
      if(rv != nsSocket::E_TIMEOUT)
        ++iFileDownloadRetries;

      if((iFileDownloadRetries > MAX_FILE_DOWNLOAD_RETRIES) &&
         (rv != nsFTPConn::E_USER_CANCEL) &&
         (gdwDownloadDialogStatus != CS_CANCEL))
      {
        /* since the download retries maxed out, increment the counter
         * to read the next url for the current file */
        ++iCounter;
        wsprintf(szKey, "url%d",  iCounter);
        GetPrivateProfileString(szSection,
                                szKey,
                                "",
                                szTempURL,
                                sizeof(szTempURL),
                                gszConfigIniFile);
        if(*szTempURL != '\0')
        {
          /* Found more urls to download from for the current file.
           * Update the dialog to show the new url and reset the
           * file download retries to 0 since it's a new url. */
          gbUrlChanged = TRUE;
          iFileDownloadRetries = 0;
          bDownloadInitiated = FALSE; // restart the download using new socket connection
          CloseSocket(szProxyServer, szProxyPort);
          ParsePath(szTempURL,
                    szWorkingURLPathOnly,
                    sizeof(szWorkingURLPathOnly),
                    TRUE, //use '/' as the path delimiter
                    PP_PATH_ONLY);
          RemoveSlash(szWorkingURLPathOnly);
          wsprintf(gszUrl, "%s/%s", szWorkingURLPathOnly, szCurrentFile);
          SetStatusUrl();
        }
      }
    } while((rv != nsFTPConn::E_USER_CANCEL) &&
            (rv != nsFTPConn::OK) &&
            (gdwDownloadDialogStatus != CS_CANCEL) &&
            (iFileDownloadRetries <= MAX_FILE_DOWNLOAD_RETRIES));

    /* Save the number of retries for each file */
    siCCurrentFileObj->iNetRetries = iFileDownloadRetries < 1 ? 0:iFileDownloadRetries - 1;

    if((rv == nsFTPConn::E_USER_CANCEL) ||
       (gdwDownloadDialogStatus == CS_CANCEL))
    {
      /* make sure rv is E_USER_CANCEL when gdwDownloadDialogStatus
       * is CS_CANCEL */
      rv = nsFTPConn::E_USER_CANCEL;

      if(szFailedFile && ((DWORD)lstrlen(szCurrentFile) <= dwFailedFileSize))
        lstrcpy(szFailedFile, gszCurrentDownloadFileDescription);

      /* break out of for() loop */
      break;
    }

    if((rv != nsFTPConn::OK) &&
       (iFileDownloadRetries > MAX_FILE_DOWNLOAD_RETRIES) &&
       !bIgnoreAllNetworkErrors &&
       !iIgnoreFileNetworkError)
    {
      /* too many retries from failed downloads */
      char szMsg[MAX_BUF];

      if(szFailedFile && ((DWORD)lstrlen(szCurrentFile) <= dwFailedFileSize))
        lstrcpy(szFailedFile, gszCurrentDownloadFileDescription);

      GetPrivateProfileString("Strings",
                              "Error Too Many Network Errors",
                              "",
                              szMsg,
                              sizeof(szMsg),
                              szFileIniConfig);
      if(*szMsg != '\0')
      {
        wsprintf(szBuf, szMsg, szCurrentFile);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }

      iFileDownloadRetries = 0; // reset the file download retries counter since
                                // we'll be restarting the download again.
      bDownloadInitiated = FALSE; // restart the download using new socket connection
      CloseSocket(szProxyServer, szProxyPort);
      --giIndex; // Decrement the file index counter because we'll be trying to
                 // download the same file again.  We don't want to go to the next
                 // file just yet.

      /* Let's make sure we're in a paused state. */
      /* The pause state will be unset by DownloadDlgProc(). */
      gdwDownloadDialogStatus = CS_PAUSE;
      PauseTheDownload(rv, &iFileDownloadRetries);
    }
    else if(bIgnoreAllNetworkErrors || iIgnoreFileNetworkError)
      rv = nsFTPConn::OK;

    UnsetSetupCurrentDownloadFile();
  }

  CloseSocket(szProxyServer, szProxyPort);
  DeInitDownloadDlg();
  SetCurrentDirectory(szSavedCwd);
  return(rv);
}

int ProgressCB(int aBytesSoFar, int aTotalFinalSize)
{
  long   lBytesDiffSoFar;
  double dPercentSoFar;
  int    iRv = nsFTPConn::OK;

  if(sgProduct.mode != SILENT)
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

    if((gdwDownloadDialogStatus == CS_CANCEL) ||
       (gdwDownloadDialogStatus == CS_PAUSE))
      iRv = nsFTPConn::E_USER_CANCEL;
  }

  ProcessWindowsMessages();
  return(iRv);
}

// Window proc for dialog
LRESULT CALLBACK
DownloadDlgProc(HWND hWndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
    case WM_INITDIALOG:
      GetPrivateProfileString("Strings",
                              "Status File Info",
                              "",
                              gszFileInfo,
                              sizeof(gszFileInfo),
                              szFileIniConfig);
      DisableSystemMenuItems(hWndDlg, FALSE);
      if(gbShowDownloadRetryMsg)
        SetDlgItemText(hWndDlg, IDC_MESSAGE0, diDownload.szMessageRetry0);
      else
        SetDlgItemText(hWndDlg, IDC_MESSAGE0, diDownload.szMessageDownload0);

      EnableWindow(GetDlgItem(hWndDlg, IDRESUME), FALSE);
      SetDlgItemText(hWndDlg, IDC_STATIC1, sgInstallGui.szStatus);
      SetDlgItemText(hWndDlg, IDC_STATIC2, sgInstallGui.szFile);
      SetDlgItemText(hWndDlg, IDC_STATIC4, sgInstallGui.szTo);
      SetDlgItemText(hWndDlg, IDC_STATIC3, sgInstallGui.szUrl);
      SetDlgItemText(hWndDlg, IDCANCEL, sgInstallGui.szCancel_);
      SetDlgItemText(hWndDlg, IDPAUSE, sgInstallGui.szPause_);
      SetDlgItemText(hWndDlg, IDRESUME, sgInstallGui.szResume_);
      SendDlgItemMessage (hWndDlg, IDC_STATIC1, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hWndDlg, IDC_STATIC2, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hWndDlg, IDC_STATIC3, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hWndDlg, IDCANCEL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hWndDlg, IDPAUSE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hWndDlg, IDRESUME, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hWndDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hWndDlg, IDC_STATUS_STATUS, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hWndDlg, IDC_STATUS_FILE, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hWndDlg, IDC_STATUS_URL, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      SendDlgItemMessage (hWndDlg, IDC_STATUS_TO, WM_SETFONT, (WPARAM)sgInstallGui.definedFont, 0L);
      RepositionWindow(hWndDlg, BANNER_IMAGE_DOWNLOAD);
      ClosePreviousDialog();
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
            gdwDownloadDialogStatus = CS_CANCEL;
          break;

        case IDPAUSE:
          if(!gtiPaused.bTickStarted)
          {
            gtiPaused.dwTickBegin          = GetTickCount();
            gtiPaused.bTickStarted         = TRUE;
            gtiPaused.bTickDownloadResumed = FALSE;
          }

          EnableWindow(GetDlgItem(hWndDlg, IDPAUSE),  FALSE);
          EnableWindow(GetDlgItem(hWndDlg, IDRESUME), TRUE);
          gdwDownloadDialogStatus = CS_PAUSE;
          break;

        case IDRESUME:
          gtiPaused.dwTickEnd = GetTickCount();
          gtiPaused.dwTickDif = GetTickDif(gtiPaused.dwTickEnd,
                                           gtiPaused.dwTickBegin);
          gtiPaused.bTickDownloadResumed = TRUE;

          EnableWindow(GetDlgItem(hWndDlg, IDRESUME), FALSE);
          EnableWindow(GetDlgItem(hWndDlg, IDPAUSE),  TRUE);
          gdwDownloadDialogStatus = CS_NONE;
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
	int	        nBars;
  static long lModLastValue = 0;

  if(sgProduct.mode != SILENT)
  {
    if(!CheckInterval(&lModLastValue, UPDATE_INTERVAL_PROGRESS_BAR))
      return;

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

  if(sgProduct.mode != SILENT)
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
  if(sgProduct.mode != SILENT)
  {
    SaveWindowPosition(dlgInfo.hWndDlg);
    DestroyWindow(dlgInfo.hWndDlg);
    UnregisterClass("GaugeFile", hInst);
  }
}
