/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
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

#ifndef _NS_XIENGINE_H_
#define _NS_XIENGINE_H_

#include "XIDefines.h"
#include "nsXInstaller.h"
#include "nsComponent.h"
#include "nsComponentList.h"
#include "nsInstallDlg.h"
#include "nsZipExtractor.h"

#include "xpistub.h"

#define STANDALONE 1
#include "zipfile.h"
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>

/*------------------------------------------------------------------------*
 *   XPI Stub Glue
 *------------------------------------------------------------------------*/
typedef nsresult (*pfnXPI_Init) 
                 (const char *aProgramDir, const char *aLogName,
                  pfnXPIProgress progressCB);
typedef nsresult (*pfnXPI_Install) 
                 (const char *file, const char *args, long flags);
typedef void     (*pfnXPI_Exit)();

typedef struct _xpistub_t 
{
    const char      *name;
    void            *handle;
    pfnXPI_Init     fn_init;
    pfnXPI_Install  fn_install;
    pfnXPI_Exit     fn_exit;
} xpistub_t;

#define TYPE_UNDEF 0
#define TYPE_PROXY 1
#define TYPE_HTTP 2
#define TYPE_FTP 3

typedef struct _conn
{
  unsigned char type; // TYPE_UNDEF, etc.
  char *URL;      // URL this connection is for
  void *conn;     // pointer to open connection
} CONN;

/*------------------------------------------------------------------------*
 *   nsXIEngine
 *------------------------------------------------------------------------*/
class nsXIEngine
{
public:
    nsXIEngine();
    ~nsXIEngine();

    int     Download(int aCustom, nsComponentList *aComps);
    int     Extract(nsComponent *aXPIEngine);
    int     Install(int aCustom, nsComponentList *aComps, char *aDestination);
    int     DeleteXPIs(int aCustom, nsComponentList *aComps);

    static void ProgressCallback(const char* aMsg, PRInt32 aVal, PRInt32 aMax);
    static int  ExistAllXPIs(int aCustom, nsComponentList *aComps, int *aTotal);

    enum
    {
        OK          = 0,
        E_PARAM     = -1201,
        E_MEM       = -1202,
        E_OPEN_MKR  = -1203,
        E_WRITE_MKR = -1204,
        E_READ_MKR  = -1205,
        E_FIND_COMP = -1206,
        E_STAT      = -1207
    };

private:
    int     MakeUniqueTmpDir();
    int     LoadXPIStub(xpistub_t *aStub, char *aDestionation);
    int     InstallXPI(nsComponent *aComp, xpistub_t *aStub);
    int     UnloadXPIStub(xpistub_t *aStub);
    int     GetFileSize(char *aPath);
    int     SetDLMarker(char *aCompName);
    int     GetDLMarkedComp(nsComponentList *aComps, int aCustom,
                            nsComponent **aOutComp, int *aOutCompNum);
    int     DelDLMarker();
    int     TotalToDownload(int aCustom, nsComponentList *aComps);
    PRBool  CRCCheckDownloadedArchives(char *dlPath, short dlPathLen, 
              nsComponent *currComp, int count, int aCustom);
    PRBool  IsArchiveFile(char *path);
    int     VerifyArchive(char *szArchive);
    PRBool  CheckConn( char *URL, int type, CONN *myConn, PRBool force );
    char    *mTmp;
    int     mTotalComps;
    char    *mOriginalDir;
};

#endif /* _NS_XIENGINE_H_ */
