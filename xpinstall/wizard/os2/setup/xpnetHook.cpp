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
 *     IBM Corp. 
 */

#include <os2.h>
#include <string.h>
#include <time.h>
#include <sys\stat.h>

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
ULONG                   gdwDownloadDialogStatus;
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

struct TickInfo
{
  ULONG dwTickBegin;
  ULONG dwTickEnd;
  ULONG dwTickDif;
  BOOL  bTickStarted;
  BOOL  bTickDownloadResumed;
};

struct TickInfo gtiPaused;

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

char *GetTimeLeft(ULONG dwTimeLeft,
                  char *szTimeString,
                  ULONG dwTimeStringBufSize)
{
  ULONG      dwTimeLeftPP;
  SYSTEMTIME stTime;

  memset(&stTime, 0, sizeof(stTime));
  dwTimeLeftPP         = dwTimeLeft + 1;
  stTime.wHour         = (unsigned)(dwTimeLeftPP / 60 / 60);
  stTime.wMinute       = (unsigned)((dwTimeLeftPP / 60) % 60);
  stTime.wSecond       = (unsigned)(dwTimeLeftPP % 60);

  memset(szTimeString, 0, dwTimeStringBufSize);
  /* format time string using user's local time format information */
  GetTimeFormat(LOCALE_USER_DEFAULT,
                TIME_NOTIMEMARKER|TIME_FORCE24HOURFORMAT,
                &stTime,
                NULL,
                szTimeString,
                dwTimeStringBufSize);

  return(szTimeString);
}

ULONG AddToTick(ULONG dwTick, ULONG dwTickMoreToAdd)
{
  ULONG dwTickLeftTillWrap = 0;

  /* Since GetTickCount() is the number of milliseconds since the system
   * has been on and the return value is a ULONG, this value will wrap
   * every 49.71 days or 0xFFFFFFFF milliseconds. */
  dwTickLeftTillWrap = 0xFFFFFFFF - dwTick;
  if(dwTickMoreToAdd > dwTickLeftTillWrap)
    dwTick = dwTickMoreToAdd - dwTickLeftTillWrap;
  else
    dwTick = dwTick + dwTickMoreToAdd;

  return(dwTick);
}

ULONG GetTickDif(ULONG dwTickEnd, ULONG dwTickStart)
{
  ULONG dwTickDif;

  /* Since GetTickCount() is the number of milliseconds since the system
   * has been on and the return value is a ULONG, this value will wrap
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

ULONG RoundDouble(double dValue)
{
  if(0.5 <= (dValue - (ULONG)dValue))
    return((ULONG)dValue + 1);
  else
    return((ULONG)dValue);
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
  static ULONG  dwTickStart = 0;
  ULONG         dwTickNow;
  ULONG         dwTickDif;
  ULONG         dwKBytesSoFar;
  ULONG         dwRoundedRate;
  char          szTimeLeft[MAX_BUF_TINY];
  HINI          hiniConfig;

  /* If the user just clicked on the Resume button, then the time lapsed
   * between dwTickStart and when the Resume button was clicked needs to
   * be subtracted taken into account when calculating dwTickDif.  So
   * "this" lapsed time needs to be added to dwTickStart. */
  if(gtiPaused.bTickDownloadResumed)
  {
    dwTickStart = AddToTick(dwTickStart, gtiPaused.dwTickDif);
    InitTickInfo();
  }

  /* GetTickCount() returns time in milliseconds.  This is more accurate,
   * which will allow us to get at a 2 decimal precision value for the
   * download rate. */
  dwTickNow = GetTickCount();
  if(dwTickStart == 0)
    dwTickNow = dwTickStart = GetTickCount();

  dwTickDif = GetTickDif(dwTickNow, dwTickStart);

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
    strcpy(szTimeLeft, "00:00:00");

  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  if(!gbShowDownloadRetryMsg)
  {
    PrfQueryProfileString(hiniConfig, "Strings",
                            "Status Download",
                            "",
                            szStatusStatusLine,
                            sizeof(szStatusStatusLine));
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
    PrfQueryProfileString(hiniConfig, "Strings",
                            "Status Retry",
                            "",
                            szStatusStatusLine,
                            sizeof(szStatusStatusLine));
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

  PrfQueryProfileString("Strings",
                          "Status Percentage Completed",
                          "",
                          szPercentageCompleted,
                          sizeof(szPercentageCompleted));
  sprintf(szPercentString, szPercentageCompleted, (int)GetPercentSoFar());

  /* Set the download dialog title */
  WinSetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS_STATUS, szCurrentStatusInfo);
  WinSetDlgItemText(dlgInfo.hWndDlg, IDC_PERCENTAGE, szPercentString);
  PrfCloseProfile(hiniConfig);
}

void SetStatusFile(void)
{
  char szString[MAX_BUF];

  /* Set the download dialog status*/
  sprintf(szString, gszFileInfo, gszCurrentDownloadFileDescription);
  WinSetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS_FILE, szString);
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

    hStatusUrl = WinWindowFromID(dlgInfo.hWndDlg, IDC_STATUS_URL);
    if(hStatusUrl)
      TruncateString(hStatusUrl, gszUrl, szUrlPathBuf, sizeof(szUrlPathBuf));
    else
      strcpy(szUrlPathBuf, gszUrl);

    hStatusTo = WinWindowFromID(dlgInfo.hWndDlg, IDC_STATUS_TO);
    if(hStatusTo)
      TruncateString(hStatusTo, gszTo, szToPathBuf, sizeof(szToPathBuf));
    else
      strcpy(szToPathBuf, gszTo);

    WinSetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS_URL, szUrlPathBuf);
    WinSetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS_TO,  szToPathBuf);
    SetStatusFile();
    gbUrlChanged = FALSE;
  }
}

double GetPercentSoFar(void)
{
  return((double)(((double)(glAbsoluteBytesSoFar / 1024) / (double)glTotalKb) * (double)100));
}

void SetMinimizedDownloadTitle(ULONG dwPercentSoFar)
{
  static ULONG dwLastPercentSoFar = 0;
  char szDownloadTitle[MAX_BUF_MEDIUM];
  char gszCurrentDownloadInfo[MAX_BUF_MEDIUM];
  HINI   hiniConfig;

  hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
  PrfQueryProfileString(hiniConfig, "Strings", "Dialog Download Title Minimized", "", szDownloadTitle, sizeof(szDownloadTitle));
  PrfCloseProfile(hiniConfig);

  if(*szDownloadTitle != '\0')
    sprintf(gszCurrentDownloadInfo, szDownloadTitle, dwPercentSoFar, gszCurrentDownloadFilename);
  else
    sprintf(gszCurrentDownloadInfo, "%d%% for all files", dwPercentSoFar);

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
    WinSetWindowText(dlgInfo.hWndDlg, gszCurrentDownloadInfo);
    dwLastPercentSoFar = dwPercentSoFar;
  }
}

void SetRestoredDownloadTitle(void)
{
  /* Set the download dialog title */
  WinSetWindowText(dlgInfo.hWndDlg, diDownload.szTitle);
}

void GetTotalArchivesToDownload(int *iTotalArchivesToDownload, ULONG *dwTotalEstDownloadSize)
{
  int  iIndex = 0;
  char szUrl[MAX_BUF];
  char szSection[MAX_INI_SK];
  char szDownloadSize[MAX_ITOA];
  HINI  hiniConfigIniFile;

  *dwTotalEstDownloadSize = 0;
  iIndex = 0;
  sprintf(szSection, "File%d", iIndex);
  hiniConfigIniFile = PrfOpenProfile((HAB)0, gszConfigIniFile);
  PrfQueryProfileString(hiniConfigIniFile, szSection,
                          "url0",
                          "",
                          szUrl,
                          sizeof(szUrl));
  while(*szUrl != '\0')
  {
    PrfQueryProfileString(hiniConfigIniFile, szSection, "size", "", szDownloadSize, sizeof(szDownloadSize));
    if((strlen(szDownloadSize) < 32) && (*szDownloadSize != '\0'))
      /* size will be in kb.  31 bits (int minus the signed bit) of precision should sufice */
      *dwTotalEstDownloadSize += atoi(szDownloadSize);

    ++iIndex;
    sprintf(szSection, "File%d", iIndex);
    PrfQueryProfileString(hiniConfigIniFile, szSection,
                            "url0",
                            "",
                            szUrl,
                            sizeof(szUrl));
  }
  *iTotalArchivesToDownload = iIndex;
  PrfCloseProfile(hiniConfigIni);
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
	ProcessWindowsMessages();
	return 0;
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
    sprintf(proxyURL, "http://%s:%s", szProxyServer, szProxyPort);

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

int DownloadViaProxy(char *szUrl, char *szProxyServer, char *szProxyPort, char *szProxyUser, char *szProxyPasswd)
{
  int  rv;
  char proxyURL[kProxySrvrLen];
  char *file = NULL;
  HINI  hiniConfig;

  if((!szUrl) || (*szUrl == '\0'))
    return nsHTTPConn::E_PARAM;

  rv = nsHTTPConn::OK;
  memset(proxyURL, 0, kProxySrvrLen);
  sprintf(proxyURL, "http://%s:%s", szProxyServer, szProxyPort);

  nsHTTPConn *conn = new nsHTTPConn(proxyURL, ProcessWndMsgCB);
  if(conn == NULL)
  {
    char szBuf[MAX_BUF_TINY];
    hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
    PrfQueryProfileString(hiniConfig, "Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf));
    PrfCloseProfile(hiniConfig);
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

  if(strrchr(szUrl, '/') != (szUrl + strlen(szUrl)))
    file = strrchr(szUrl, '/') + 1; // set to leaf name

  rv = conn->Get(ProgressCB, file); // use leaf from URL
  conn->Close();

  if(conn)
    delete(conn);

  return(rv);
}

int DownloadViaHTTP(char *szUrl)
{
  int  rv;
  char *file = NULL;
  HINI  hiniConfigIni;

  if((!szUrl) || (*szUrl == '\0'))
    return nsHTTPConn::E_PARAM;

  rv = nsHTTPConn::OK;
  nsHTTPConn *conn = new nsHTTPConn(szUrl, ProcessWndMsgCB);
  if(conn == NULL)
  {
    char szBuf[MAX_BUF_TINY];
    PrfOpenProfile((HAB)0, gszConfigIniFile);
    PrfQueryProfileString(hiniConfigIni, "Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf));
    PrfCloseProfile(hiniConfigIni);
    PrintError(szBuf, ERROR_CODE_HIDE);

    return(WIZ_OUT_OF_MEMORY);
  }
  
  if((rv = conn->Open()) != WIZ_OK)
    return(rv);

  if(strrchr(szUrl, '/') != (szUrl + strlen(szUrl)))
    file = strrchr(szUrl, '/') + 1; // set to leaf name

  rv = conn->Get(ProgressCB, file);
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
  HINI    hiniConfig;

  if((!szUrl) || (*szUrl == '\0'))
    return nsFTPConn::E_PARAM;

  rv = nsHTTPConn::ParseURL(kFTP, szUrl, &host, &port, &path);

  nsFTPConn *conn = new nsFTPConn(host, ProcessWndMsgCB);
  if(conn == NULL)
  {
    char szBuf[MAX_BUF_TINY];
    hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
    PrfQueryProfileString(hiniConfig, "Strings", "Error Out Of Memory", "", szBuf, sizeof(szBuf));
    PrfCloseProfile(hiniConfig);
    PrintError(szBuf, ERROR_CODE_HIDE);

    return(WIZ_OUT_OF_MEMORY);
  }

  if((rv = conn->Open()) != WIZ_OK)
    return(rv);

  if(strrchr(path, '/') != (path + strlen(path)))
    file = strrchr(path, '/') + 1; // set to leaf name

  rv = conn->Get(path, file, nsFTPConn::BINARY, TRUE, ProgressCB);
  conn->Close();

  if(host)
    free(host);
  if(path)
    free(path);
  if(conn)
    delete(conn);

  return(rv);
}

void PauseTheDownload(int rv, int *iFileDownloadRetries)
{
  if(rv != nsFTPConn::E_USER_CANCEL)
  {
    WinSendMsg(dlgInfo.hWndDlg, WM_COMMAND, IDPAUSE, 0);
    --*iFileDownloadRetries;
  }

  while(gdwDownloadDialogStatus == CS_PAUSE)
  {
    DosSleep(200);
    ProcessWindowsMessages();
  }
}

int DownloadFiles(char *szInputIniFile,
                  char *szDownloadDir,
                  char *szProxyServer,
                  char *szProxyPort,
                  char *szProxyUser,
                  char *szProxyPasswd,
                  BOOL bShowRetryMsg,
                  int *iNetRetries,
                  BOOL bIgnoreAllNetworkErrors,
                  char *szFailedFile,
                  ULONG dwFailedFileSize)
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
  ULONG     dwTotalEstDownloadSize;
  char      szPartiallyDownloadedFilename[MAX_BUF];
  ULONG ulCwdLength = sizeof(szSavedCwd);
  HINI      hiniInstall;
  HINI      hiniConfig;
  HINI      hiniConfigIniFile;

  if(szInputIniFile == NULL)
    return(WIZ_ERROR_UNDEFINED);

  if(szFailedFile)
    memset(szFailedFile, 0, dwFailedFileSize);

  InitTickInfo();
  DosQueryCurrentDir(0, szSavedCwd, &ulCwdLength);
  DosSetCurrentDir(szDownloadDir);

  rv                        = WIZ_OK;
  dwTotalEstDownloadSize    = 0;
  giTotalArchivesToDownload = 0;
  glLastBytesSoFar          = 0;
  glAbsoluteBytesSoFar      = 0;
  glBytesResumedFrom        = 0;
  gbUrlChanged              = TRUE;
  gbDlgDownloadMinimized    = FALSE;
  gbDlgDownloadJustMinimized = FALSE;
  gdwDownloadDialogStatus   = CS_NONE;
  gbShowDownloadRetryMsg    = bShowRetryMsg;
  gszConfigIniFile          = szInputIniFile;

  GetTotalArchivesToDownload(&giTotalArchivesToDownload,
                             &dwTotalEstDownloadSize);
  glTotalKb                 = dwTotalEstDownloadSize;
  GetSetupCurrentDownloadFile(szPartiallyDownloadedFilename,
                              sizeof(szPartiallyDownloadedFilename));

  InitDownloadDlg();

  hiniConfigIniFile = PrfOpenProfile((HAB)0, gszConfigIniFile);
  hiniConfig    = PrfOpenProfile((HAB)0, szFileIniConfig);
  hiniInstall   = PrfOpenProfile((HAB)0, szFileIniInstall);

  for(giIndex = 0; giIndex < giTotalArchivesToDownload; giIndex++)
  {
    /* set (or reset) the counter to 0 in order to read the
     * next files's 0'th url from the .idi file */
    iCounter     = 0;
    gbUrlChanged = TRUE; /* Update the download dialog with new URL */
    sprintf(szSection, "File%d", giIndex);
    sprintf(szKey,     "url%d",  iCounter);
    PrfQueryProfileString(hiniConfigIniFile, szSection,
                            szKey,
                            "",
                            gszUrl,
                            sizeof(gszUrl));
    if(*gszUrl == '\0')
      continue;

    PrfQueryProfileString(hiniConfigIniFile, szSection,
                            "desc",
                            "",
                            gszCurrentDownloadFileDescription,
                            sizeof(gszCurrentDownloadFileDescription));
    iIgnoreFileNetworkError = PrfQueryProfileInt(hiniConfigIni, szSection,
                            "Ignore File Network Error",
                            0);

    /* save the file name to be downloaded */
    ParsePath(gszUrl,
              szCurrentFile,
              sizeof(szCurrentFile),
              TRUE,
              PP_FILENAME_ONLY);

    if((*szPartiallyDownloadedFilename != 0) &&
       (strcmpi(szPartiallyDownloadedFilename, szCurrentFile) == 0))
    {
      struct stat statBuf;

      if(stat(szPartiallyDownloadedFilename, &statBuf) != -1)
      {
        glAbsoluteBytesSoFar += statBuf.st_size;
        glBytesResumedFrom    = statBuf.st_size;
      }
    }

    strcpy(gszTo, szDownloadDir);
    AppendBackSlash(gszTo, sizeof(gszTo));
    strcat(gszTo, szCurrentFile);

    if(gbDlgDownloadMinimized)
      SetMinimizedDownloadTitle((int)GetPercentSoFar());
    else
    {
      SetStatusUrl();
      SetRestoredDownloadTitle();
    }

    SetSetupCurrentDownloadFile(szCurrentFile);
    iFileDownloadRetries = 0;
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
        if(strncmp(gszUrl, kHTTP, strlen(kHTTP)) == 0)
          rv = DownloadViaHTTP(gszUrl);
        /* or is this an FTP URL? */
        else if(strncmp(gszUrl, kFTP, strlen(kFTP)) == 0)
          rv = DownloadViaFTP(gszUrl);
      }

      if((rv == nsFTPConn::E_USER_CANCEL) ||
         (gdwDownloadDialogStatus == CS_PAUSE))
      {
        if(gdwDownloadDialogStatus == CS_PAUSE)
        {
          PauseTheDownload(rv, &iFileDownloadRetries);

          /* rv needs to be set to something
           * other than E_USER_CANCEL or E_OK */
          rv = nsFTPConn::E_CMD_UNEXPECTED;
        }
        else
        {
          /* break out of the do loop */
          break;
        }
      }
      else if((rv != nsFTPConn::OK) &&
              (rv != nsFTPConn::E_CMD_FAIL) &&
              (gdwDownloadDialogStatus != CS_CANCEL))
      {
        char szTitle[MAX_BUF_SMALL];
        char szMsgDownloadPaused[MAX_BUF];

        /* Start the pause tick counter here because we don't know how
         * long before the user will dismiss the MessageBox() */
        if(!gtiPaused.bTickStarted)
        {
          gtiPaused.dwTickBegin          = GetTickCount();
          gtiPaused.bTickStarted         = TRUE;
          gtiPaused.bTickDownloadResumed = FALSE;
        }

        /* The connection exepectedly dropped for some reason, so inform
         * the user that the download will be Paused, and then update the
         * Download dialog to show the Paused state. */
        PrfQueryProfileString(hiniInstall, "Messages",
                                "MB_WARNING_STR",
                                "",
                                szTitle,
                                sizeof(szTitle));
        PrfQueryProfileString(hiniConfig, "Strings",
                                "Message Download Paused",
                                "",
                                szMsgDownloadPaused,
                                sizeof(szMsgDownloadPaused));
        WinMessageBox(HWND_DESKTOP, dlgInfo.hWndDlg,
                   szMsgDownloadPaused,
                   szTitle,
                   0,
                   MB_ICONEXCLAMATION);
        PauseTheDownload(rv, &iFileDownloadRetries);
      }

      ++iFileDownloadRetries;
      if((iFileDownloadRetries > MAX_FILE_DOWNLOAD_RETRIES) &&
         (rv != nsFTPConn::E_USER_CANCEL) &&
         (gdwDownloadDialogStatus != CS_CANCEL))
      {
        /* since the download retries maxed out, increment the counter
         * to read the next url for the current file */
        ++iCounter;
        sprintf(szKey, "url%d",  iCounter);
        PrfQueryProfileString(hiniConfigIniFile, szSection,
                                szKey,
                                "",
                                gszUrl,
                                sizeof(gszUrl));
        if(*gszUrl != '\0')
        {
          /* Found more urls to download from for the current file.
           * Update the dialog to show the new url and reset the
           * file download retries to 0 since it's a new url. */
          gbUrlChanged = TRUE;
          iFileDownloadRetries = 0;
          SetStatusUrl();
        }
      }
    } while((rv != nsFTPConn::E_USER_CANCEL) &&
            (rv != nsFTPConn::OK) &&
            (gdwDownloadDialogStatus != CS_CANCEL) &&
            (iFileDownloadRetries <= MAX_FILE_DOWNLOAD_RETRIES));

    if((iFileDownloadRetries > MAX_FILE_DOWNLOAD_RETRIES) && iNetRetries)
      /* Keep track of how many retries for only the file that had
       * too many retries.
       * We don't care about retries unless it's reached the limit
       * because each retry should not count as a new download. */
      *iNetRetries = iFileDownloadRetries - 1;

    if((rv == nsFTPConn::E_USER_CANCEL) ||
       (gdwDownloadDialogStatus == CS_CANCEL))
    {
      /* make sure rv is E_USER_CANCEL when gdwDownloadDialogStatus
       * is CS_CANCEL */
      rv = nsFTPConn::E_USER_CANCEL;

      if(szFailedFile && ((ULONG)strlen(szCurrentFile) <= dwFailedFileSize))
        strcpy(szFailedFile, gszCurrentDownloadFileDescription);

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

      if(szFailedFile && ((ULONG)strlen(szCurrentFile) <= dwFailedFileSize))
        strcpy(szFailedFile, gszCurrentDownloadFileDescription);

      PrfQueryProfileString(hiniConfig, "Strings",
                              "Error Too Many Network Errors",
                              "",
                              szMsg,
                              sizeof(szMsg));
      if(*szMsg != '\0')
      {
        sprintf(szBuf, szMsg, szCurrentFile);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }

      /* Set return value and break out of for() loop.
       * We don't want to continue if there were too
       * many network errors on any file. */
      rv = WIZ_TOO_MANY_NETWORK_ERRORS;
      break;
    }
    else if(bIgnoreAllNetworkErrors || iIgnoreFileNetworkError)
      rv = nsFTPConn::OK;

    UnsetSetupCurrentDownloadFile();
  }

  DeInitDownloadDlg();
  DosSetCurrentDir(szSavedCwd);
  PrfCloseProfile(hiniInstall);
  PrfCloseProfile(hiniConfig);
  PrfCloseProfile(hiniConfigIni);
  return(rv);
}

int ProgressCB(int aBytesSoFar, int aTotalFinalSize)
{
  long   lBytesDiffSoFar;
  double dPercentSoFar;
  int    iRv = nsFTPConn::OK;

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

    if((gdwDownloadDialogStatus == CS_CANCEL) ||
       (gdwDownloadDialogStatus == CS_PAUSE))
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
	RECTL	rect;
	int		iLeft, iTop;

	WinQueryWindowRect(hWndDlg, &rect);
	iLeft = (WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN) - (rect.xRight - rect.xLeft)) / 2;
	iTop  = (WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - (rect.yBottom - rect.yTop)) / 2;

	WinSetWindowPos(hWndDlg, NULL, iLeft, iTop, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Window proc for dialog
LRESULT CALLBACK
DownloadDlgProc(HWND hWndDlg, UINT msg, MPARAM wParam, MPARAM lParam)
{
  HINI  hiniConfig;
  PSZ pszFontNameSize;
  ULONG ulFontNameSizeLength;

  switch (msg)
  {
    case WM_INITDIALOG:
      hiniConfig = PrfOpenProfile((HAB)0, szFileIniConfig);
      PrfQueryProfileString(hiniConfig, "Strings",
                              "Status File Info",
                              "",
                              gszFileInfo,
                              sizeof(gszFileInfo));
      PrfCloseProfile(hiniConfig);
      DisableSystemMenuItems(hWndDlg, FALSE);
      CenterWindow(hWndDlg);
      if(gbShowDownloadRetryMsg)
        WinSetDlgItemText(hWndDlg, IDC_MESSAGE0, diDownload.szMessageRetry0);
      else
        WinSetDlgItemText(hWndDlg, IDC_MESSAGE0, diDownload.szMessageDownload0);

      pszFontNameSize = myGetSysFont();
      ulFontNameSizeLength = sizeof(pszFontNameSize) + 1;

      WinEnableWindow(WinWindowFromID(hWndDlg, IDRESUME), FALSE);
      WinSetDlgItemText(hWndDlg, IDC_STATIC1, sgInstallGui.szStatus);
      WinSetDlgItemText(hWndDlg, IDC_STATIC2, sgInstallGui.szFile);
      WinSetDlgItemText(hWndDlg, IDC_STATIC4, sgInstallGui.szTo);
      WinSetDlgItemText(hWndDlg, IDC_STATIC3, sgInstallGui.szUrl);
      WinSetDlgItemText(hWndDlg, MBID_CANCEL, sgInstallGui.szCancel_);
      WinSetDlgItemText(hWndDlg, MBID_PAUSE, sgInstallGui.szPause_);
      WinSetDlgItemText(hWndDlg, MBID_RESUME, sgInstallGui.szResume_);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC2), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC13), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_PAUSE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_RESUME), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATUS_STATUS), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATUS_FILE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATUS_URL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATUS_TO), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);

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

          WinEnableWindow(GetDlgItem(hWndDlg, IDPAUSE),  FALSE);
          WinEnableWindow(GetDlgItem(hWndDlg, IDRESUME), TRUE);
          gdwDownloadDialogStatus = CS_PAUSE;
          break;

        case IDRESUME:
          gtiPaused.dwTickEnd = GetTickCount();
          gtiPaused.dwTickDif = GetTickDif(gtiPaused.dwTickEnd,
                                           gtiPaused.dwTickBegin);
          gtiPaused.bTickDownloadResumed = TRUE;

          WinEnableWindow(GetDlgItem(hWndDlg, IDRESUME), FALSE);
          WinEnableWindow(GetDlgItem(hWndDlg, IDPAUSE),  TRUE);
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

  if(sgProduct.dwMode != SILENT)
  {
    if(!CheckInterval(&lModLastValue, UPDATE_INTERVAL_PROGRESS_BAR))
      return;

    // Figure out how many bars should be displayed
    nBars = (int)(dlgInfo.nMaxFileBars * value / 100);

    // Only paint if we need to display more bars
    if((nBars > dlgInfo.nFileBars) || (dlgInfo.nFileBars == 0))
    {
      HWND	hWndGauge = WinWindowFormID(dlgInfo.hWndDlg, IDC_GAUGE_FILE);
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

// Draws a recessed border around the gauge
static void
DrawGaugeBorder(HWND hWnd)
{
	HDC		hDC = GetWindowDC(hWnd);
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
}

// Draws the blue progress bar
static void
DrawProgressBar(HWND hWnd, int nBars)
{
  int         i;
	HPS         hPS;
	RECTL        rect;

  WinQueryWindowRect(hWnd, &rect);
  hPS = WinBeginPaint(hWnd, NULLHANDLE, &rect);
  if(nBars <= 0)
  {
    /* clear the bars */
    WinFillRect(hPS, &rect, WinQuerySysColor(HWND_DESKTOP, SYSCLR_MENU, 0L));
  }
  else
  {
  	// Draw the bars
	  rect.xLeft     = rect.yTop = BAR_LIBXPNET_MARGIN;
	  rect.yBottom  -= BAR_LIBXPNET_MARGIN;
	  rect.xRight    = rect.xLeft + BAR_LIBXPNET_WIDTH;

	  for(i = 0; i < nBars; i++)
    {
		  RECTL	dest;

		  if(WinIntersectRect(hab, &dest, &ps.rcPaint, &rect))
			  WinFillRect(hPS, &rect, CLR_BLUE);

      WinOffsetRect(hab, &rect, BAR_LIBXPNET_WIDTH + BAR_LIBXPNET_SPACING, 0);
	  }
  }

	EndPaint(hPS);
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
	cx = 2 * WinQuerySysValue(SV_CXBORDER) + 2 * BAR_LIBXPNET_MARGIN +
		nMaxBars * BAR_LIBXPNET_WIDTH + (nMaxBars - 1) * BAR_LIBXPNET_SPACING;

	WinSetWindowPos(hWnd, NULL, -1, -1, cx, rect.yBottom - rect.yTop,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Window proc for Download gauge
BOOL CALLBACK
GaugeDownloadWndProc(HWND hWnd, UINT msg, MPARAM wParam, MPARAM lParam)
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
			dlgInfo.nMaxFileBars = (rect.xRight - rect.xLeft - 2 * BAR_LIBXPNET_MARGIN + BAR_LIBXPNET_SPACING) / (BAR_LIBXPNET_WIDTH + BAR_LIBXPNET_SPACING);

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
    memset(wc, 0, sizeof(wc));
    wc.style          = CS_GLOBALCLASS;
    wc.hInstance      = hInst;
    wc.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpfnWndProc    = (WNDPROC)GaugeDownloadWndProc;
    wc.lpszClassName  = "GaugeFile";
    RegisterClass(&wc);

    // Display the dialog box
    //dlgInfo.hWndDlg = CreateDialog(hSetupRscInst, MAKEINTRESOURCE(DLG_DOWNLOADING), hWndMain, (DLGPROC)DownloadDlgProc);
    dlgInfo.hWndDlg = WinCreateDlg(HWND_DESKTOP, hWndMain, (PFNWP)DownloadDlgProc, DLG_DOWNLOADING, NULL);
    WinUpdateWindow(dlgInfo.hWndDlg);
    UpdateGaugeFileProgressBar(0);
  }
}

void DeInitDownloadDlg()
{
  if(sgProduct.dwMode != SILENT)
  {
    WinDestroyWindow(dlgInfo.hWndDlg);
    UnregisterClass("GaugeFile", hInst);
  }
}
