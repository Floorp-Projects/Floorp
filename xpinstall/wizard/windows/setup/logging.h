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

#ifndef _LOGGING_H_
#define _LOGGING_H_

int               AppendToGlobalMessageStream(char *szInfo);
void              LogISTime(int iType);
void              LogISProductInfo(void);
void              LogISDestinationPath(void);
void              LogISSetupType(void);
void              LogISComponentsSelected(void);
void              LogISComponentsToDownload(void);
void              LogISComponentsFailedCRC(char *szList, int iWhen);
void              LogISDownloadStatus(char *szStatus, char *szFailedFile);
void              LogISDownloadProtocol(DWORD dwProtocolType);
void              LogISXPInstall(int iWhen);
void              LogISXPInstallComponent(char *szComponentName);
void              LogISXPInstallComponentResult(DWORD dwErrorNumber);
void              LogISLaunchApps(int iWhen);
void              LogISLaunchAppsComponent(char *szComponentName);
void              LogISLaunchAppsComponentUncompress(char *szComponentName,
                                                     DWORD dwErr);
void              LogISProcessXpcomFile(int iStatus, int iResult);
void              LogISShared(void);
void              LogISDiskSpace(dsN *dsnComponentDSRequirement);
void              LogISTurboMode(BOOL bTurboMode);
void              LogMSProductInfo(void);
void              LogMSDownloadFileStatus(void);
void              LogMSDownloadStatus(int iDownloadStatus);
void              LogMSDownloadProtocol(DWORD dwProtocolType);
void              LogMSXPInstallStatus(char *szFile, int iErr);
void              LogMSTurboMode(BOOL bTurboMode);
void              DeleteExitStatusFile(void);
void              LogExitStatus(LPSTR status);
void              GetGreSetupExitStatus(LPSTR aStatus, DWORD aStatusBufSize);

#endif /* _LOGGING_H_ */

