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
 * Copyright (C) 2001 Netscape Communications Corp.  All Rights
 * Reserved.
 * 
 * Contributor(s): 
 *   Sean Su <ssu@netscape.com>
 */

#include "extern.h"
#include "logging.h"
#include "extra.h"
#include "ifuncns.h"
#include "xpi.h"

#define E_USER_CANCEL         -813

int AppendToGlobalMessageStream(char *szInfo)
{
  DWORD dwInfoLen = lstrlen(szInfo);
  DWORD dwMessageLen;

  if(gErrorMessageStream.bEnabled && gErrorMessageStream.szMessage)
  {
    dwMessageLen = lstrlen(gErrorMessageStream.szMessage);
    if((dwInfoLen + dwMessageLen) >= gErrorMessageStream.dwMessageBufSize)
    {
      if(NS_GlobalReAlloc(&gErrorMessageStream.szMessage,
                          gErrorMessageStream.dwMessageBufSize,
                          dwInfoLen + dwMessageLen + MAX_BUF_TINY) == NULL)
        return(WIZ_OUT_OF_MEMORY);

      gErrorMessageStream.dwMessageBufSize = dwInfoLen +
                                             dwMessageLen +
                                             MAX_BUF_TINY;
    }

    lstrcat(gErrorMessageStream.szMessage, szInfo);
  }

  return(WIZ_OK);
}

void LogISTime(int iType)
{
  char       szBuf[MAX_BUF];
  char       szTime[MAX_BUF_TINY];
  char       szDate[MAX_BUF_TINY];
  SYSTEMTIME stLocalTime;

  GetLocalTime(&stLocalTime);
  GetTimeFormat(LOCALE_NEUTRAL,
                LOCALE_NOUSEROVERRIDE,
                &stLocalTime,
                NULL,
                szTime,
                sizeof(szTime));
  GetDateFormat(LOCALE_NEUTRAL,
                LOCALE_NOUSEROVERRIDE,
                &stLocalTime,
                NULL,
                szDate,
                sizeof(szDate));

  if(iType == W_START)
    wsprintf(szBuf, "Start Log: %s - %s\n", szDate, szTime);
  else
    wsprintf(szBuf, "End Log: %s - %s\n", szDate, szTime);

  UpdateInstallStatusLog(szBuf);
}

void LogISProductInfo(void)
{
  char szBuf[MAX_BUF];

  wsprintf(szBuf, "\n    Product Info:\n");
  UpdateInstallStatusLog(szBuf);

  switch(sgProduct.dwMode)
  {
    case SILENT:
      wsprintf(szBuf, "        Install mode: Silent\n");
      break;
    case AUTO:
      wsprintf(szBuf, "        Install mode: Auto\n");
      break;
    default:
      wsprintf(szBuf, "        Install mode: Normal\n");
      break;
  }
  UpdateInstallStatusLog(szBuf);

  wsprintf(szBuf, "        Company name: %s\n        Product name: %s\n        Uninstall Filename: %s\n        UserAgent: %s\n        Alternate search path: %s\n",
           sgProduct.szCompanyName,
           sgProduct.szProductName,
           sgProduct.szUninstallFilename,
           sgProduct.szUserAgent,
           sgProduct.szAlternateArchiveSearchPath);
  UpdateInstallStatusLog(szBuf);
}

void LogISDestinationPath(void)
{
  char szBuf[MAX_BUF];

  wsprintf(szBuf,
           "\n    Destination Path:\n        Main: %s\n        SubPath: %s\n",
           sgProduct.szPath,
           sgProduct.szSubPath);
  UpdateInstallStatusLog(szBuf);
}

void LogISSetupType(void)
{
  char szBuf[MAX_BUF_TINY];

  switch(dwSetupType)
  {
    case ST_RADIO3:
      wsprintf(szBuf, "\n    Setup Type: %s\n",
               diSetupType.stSetupType3.szDescriptionShort);
      break;

    case ST_RADIO2:
      wsprintf(szBuf, "\n    Setup Type: %s\n",
               diSetupType.stSetupType2.szDescriptionShort);
      break;

    case ST_RADIO1:
      wsprintf(szBuf, "\n    Setup Type: %s\n",
               diSetupType.stSetupType1.szDescriptionShort);
      break;

    default:
      wsprintf(szBuf, "\n    Setup Type: %s\n",
               diSetupType.stSetupType0.szDescriptionShort);
      break;
  }

  UpdateInstallStatusLog(szBuf);
}

void LogISComponentsSelected(void)
{
  char szBuf[MAX_BUF_TINY];
  siC  *siCNode;
  BOOL bFoundComponentSelected;

  wsprintf(szBuf, "\n    Components selected:\n");
  UpdateInstallStatusLog(szBuf);

  bFoundComponentSelected = FALSE;
  siCNode = siComponents;
  do
  {
    if(siCNode == NULL)
      break;

    if(siCNode->dwAttributes & SIC_SELECTED)
    {
      if(!siCNode->bForceUpgrade)
        wsprintf(szBuf,
                 "        %s\n",
                 siCNode->szDescriptionShort);
      else
        wsprintf(szBuf,
                 "        %s (Required)\n",
                 siCNode->szDescriptionShort);

      UpdateInstallStatusLog(szBuf);
      bFoundComponentSelected = TRUE;
    }

    siCNode = siCNode->Next;
  } while((siCNode != NULL) && (siCNode != siComponents));

  if(!bFoundComponentSelected)
  {
    wsprintf(szBuf, "        none\n");
    UpdateInstallStatusLog(szBuf);
  }
}

void LogISComponentsToDownload(void)
{
  char szBuf[MAX_BUF_TINY];
  char szArchivePath[MAX_BUF_MEDIUM];
  siC  *siCNode;
  BOOL bFoundComponentSelected;
  BOOL bFoundComponentsToDownload;

  wsprintf(szBuf, "\n    Components to download:\n");
  UpdateInstallStatusLog(szBuf);

  bFoundComponentSelected = FALSE;
  bFoundComponentsToDownload = FALSE;
  siCNode = siComponents;
  do
  {
    if(siCNode == NULL)
      break;

    if(siCNode->dwAttributes & SIC_SELECTED)
    {

      if(LocateJar(siCNode,
                   szArchivePath,
                   sizeof(szArchivePath),
                   gbPreviousUnfinishedDownload) == AP_NOT_FOUND)
      {
        wsprintf(szBuf,
                 "        %s will be downloaded\n",
                 siCNode->szDescriptionShort);
        bFoundComponentsToDownload = TRUE;
      }
      else
        wsprintf(szBuf,
                 "        %s found: %s\n",
                 siCNode->szDescriptionShort,
                 szArchivePath);

      UpdateInstallStatusLog(szBuf);
      bFoundComponentSelected = TRUE;
    }

    siCNode = siCNode->Next;
  } while((siCNode != NULL) && (siCNode != siComponents));

  if(!bFoundComponentSelected)
  {
    wsprintf(szBuf, "        none\n");
    UpdateInstallStatusLog(szBuf);
  }
  if(!bFoundComponentsToDownload)
  {
    wsprintf(szBuf, "        **\n        ** All components have been found locally.  No components will be downloaded.\n        **\n");
    UpdateInstallStatusLog(szBuf);
  }
}

void LogISDownloadStatus(char *szStatus, char *szFailedFile)
{
  char szBuf[MAX_BUF];

  if(szFailedFile)
    wsprintf(szBuf,
             "\n    Download status:\n        %s\n          file: %s\n",
             szStatus,
             szFailedFile);
  else
    wsprintf(szBuf,
             "\n    Download status:\n        %s\n",
             szStatus);
  UpdateInstallStatusLog(szBuf);
}

void LogISComponentsFailedCRC(char *szList, int iWhen)
{
  char szBuf[MAX_BUF];

  if(iWhen == W_STARTUP)
  {
    if(szList && (*szList != '\0'))
      wsprintf(szBuf,
               "\n    Components corrupted (startup):\n%s",
               szList);
    else
      wsprintf(szBuf,
               "\n    Components corrupted (startup):\n        none\n");
  }
  else
  {
    if(szList && (*szList != '\0'))
      wsprintf(szBuf,
               "\n    Components corrupted (download):\n%s",
               szList);
    else
      wsprintf(szBuf,
               "\n    Components corrupted (download):\n        none\n");
  }

  UpdateInstallStatusLog(szBuf);
}

void LogISXPInstall(int iWhen)
{
  char szBuf[MAX_BUF];

  if(iWhen == W_START)
    wsprintf(szBuf, "\n    XPInstall Start\n");
  else
    wsprintf(szBuf, "    XPInstall End\n");

  UpdateInstallStatusLog(szBuf);
}

void LogISXPInstallComponent(char *szComponentName)
{
  char szBuf[MAX_BUF];

  wsprintf(szBuf, "        %s", szComponentName);
  UpdateInstallStatusLog(szBuf);
}

void LogISXPInstallComponentResult(DWORD dwErrorNumber)
{
  char szBuf[MAX_BUF];
  char szErrorString[MAX_BUF];

  GetErrorString(dwErrorNumber, szErrorString, sizeof(szErrorString));
  wsprintf(szBuf, ": %d %s\n", dwErrorNumber, szErrorString);
  UpdateInstallStatusLog(szBuf);
}

void LogISLaunchApps(int iWhen)
{
  char szBuf[MAX_BUF];

  if(iWhen == W_START)
    wsprintf(szBuf, "\n    Launch Apps Start\n");
  else
    wsprintf(szBuf, "    Launch Apps End\n");

  UpdateInstallStatusLog(szBuf);
}

void LogISLaunchAppsComponent(char *szComponentName)
{
  char szBuf[MAX_BUF];

  wsprintf(szBuf, "        %s\n", szComponentName);
  UpdateInstallStatusLog(szBuf);
}

void LogISProcessXpcomFile(int iStatus, int iResult)
{
  char szBuf[MAX_BUF];

  if(iStatus == LIS_SUCCESS)
    wsprintf(szBuf, "\n    Uncompressing Xpcom Succeeded: %d\n", iResult);
  else
    wsprintf(szBuf, "\n    Uncompressing Xpcom Failed: %d\n", iResult);

  UpdateInstallStatusLog(szBuf);
}

void LogISDiskSpace(dsN *dsnComponentDSRequirement)
{
  ULONGLONG ullDSAvailable;
  dsN       *dsnTemp = NULL;
  char      szBuf[MAX_BUF];
  char      szDSRequired[MAX_BUF_TINY];
  char      szDSAvailable[MAX_BUF_TINY];

  if(dsnComponentDSRequirement != NULL)
  {
    wsprintf(szBuf, "\n    Disk Space Info:\n");
    UpdateInstallStatusLog(szBuf);
    dsnTemp = dsnComponentDSRequirement;
    do
    {
      if(!dsnTemp)
        break;

      ullDSAvailable = GetDiskSpaceAvailable(dsnTemp->szVDSPath);
      _ui64toa(ullDSAvailable, szDSAvailable, 10);
      _ui64toa(dsnTemp->ullSpaceRequired, szDSRequired, 10);
      wsprintf(szBuf,
               "             Path: %s\n         Required: %sKB\n        Available: %sKB\n",
               dsnTemp->szVDSPath,
               szDSRequired,
               szDSAvailable);
      UpdateInstallStatusLog(szBuf);

      dsnTemp = dsnTemp->Next;
    } while((dsnTemp != NULL) && (dsnTemp != dsnComponentDSRequirement));
  }
}

void LogMSProductInfo(void)
{
  char szBuf[MAX_BUF];
  char szOSType[MAX_BUF_TINY];

  GetOSTypeString(szOSType, sizeof(szOSType));
  if(*gSystemInfo.szExtraString != '\0')
    wsprintf(szBuf, "UserAgent=%s/%s (%s,%d.%d.%d,%s)",
             sgProduct.szProductName,
             sgProduct.szUserAgent,
             szOSType,
             gSystemInfo.dwMajorVersion,
             gSystemInfo.dwMinorVersion,
             gSystemInfo.dwBuildNumber,
             gSystemInfo.szExtraString);
  else
    wsprintf(szBuf, "UserAgent=%s/%s (%s,%d.%d.%d)",
             sgProduct.szProductName,
             sgProduct.szUserAgent,
             szOSType,
             gSystemInfo.dwMajorVersion,
             gSystemInfo.dwMinorVersion,
             gSystemInfo.dwBuildNumber);

  AppendToGlobalMessageStream(szBuf);
}

void LogMSDownloadStatus(int iDownloadStatus)
{
  char szMessageStream[MAX_BUF_TINY];

  wsprintf(szMessageStream, "&DownloadStatus=%d", iDownloadStatus);
  AppendToGlobalMessageStream(szMessageStream);

  if(((iDownloadStatus == E_USER_CANCEL) &&
       !gErrorMessageStream.bShowConfirmation) ||
     ((iDownloadStatus != E_USER_CANCEL) &&
      (iDownloadStatus != WIZ_OK)))
    gErrorMessageStream.bSendMessage = TRUE;
}

void LogMSDownloadFileStatus(void)
{
  siC   *siCObject = NULL;
  DWORD dwIndex;
  char  szMessageStream[MAX_BUF];

  ZeroMemory(szMessageStream, sizeof(szMessageStream));
  dwIndex = 0;
  siCObject = SiCNodeGetObject(dwIndex, TRUE, AC_ALL);
  while(siCObject)
  {
    if(siCObject->iNetRetries || siCObject->iCRCRetries)
    {
      char szFileInfo[MAX_BUF_SMALL];

      wsprintf(szFileInfo,
               "&%s=%d,%d",
               siCObject->szArchiveName,
               siCObject->iNetRetries,
               siCObject->iCRCRetries);

      lstrcat(szMessageStream, szFileInfo);
    }
    ++dwIndex;
    siCObject = SiCNodeGetObject(dwIndex, TRUE, AC_ALL);
  }

  if(*szMessageStream != '\0')
    AppendToGlobalMessageStream(szMessageStream);
}

void LogMSXPInstallStatus(char *szFile, int iErr)
{
  char szMessageStream[MAX_BUF];

  if(szFile)
  {
    wsprintf(szMessageStream, "&XPInstallStatus=%d&File=%s", iErr, szFile);
    AppendToGlobalMessageStream(szMessageStream);
    if((iErr != E_REBOOT) &&
      (((iErr == E_USER_CANCEL) &&
         !gErrorMessageStream.bShowConfirmation) ||
       ((iErr != E_USER_CANCEL) &&
        (iErr != WIZ_OK))))
      gErrorMessageStream.bSendMessage = TRUE;
  }
}

