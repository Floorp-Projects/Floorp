/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *     Samir Gehani <sgehani@netscape.com>
 */

#ifndef _NS_XIENGINE_H_
#define _NS_XIENGINE_H_

#include "XIDefines.h"
#include "nsXInstaller.h"
#include "nsComponent.h"
#include "nsComponentList.h"
#include "nsInstallDlg.h"

#include "xpistub.h"

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
                 (const char *aProgramDir, pfnXPIProgress progressCB);
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

    static void ProgressCallback(const char* aMsg, PRInt32 aMax, PRInt32 aVal);

private:
    int     MakeUniqueTmpDir();
    int     ParseURL(char *aURL, char **aHost, char **aDir);
    int     FTPAnonGet(char *aHost, char *aDir, char *aAcrhive);
    int     LoadXPIStub(xpistub_t *aStub, char *aDestionation);
    int     InstallXPI(nsComponent *aComp, xpistub_t *aStub);
    int     UnloadXPIStub(xpistub_t *aStub);

    char            *mTmp;
};

#define CORE_LIB_COUNT 9
static char sCoreLibs[ CORE_LIB_COUNT * 2 ][ 32 ] = 
{
/*      Archive Subdir      File                            */
/*      --------------      ----                            */
		"bin/", 			"libjsdom.so", 
		"bin/", 			"libmozjs.so",
		"bin/", 			"libnspr4.so",
		"bin/",				"libplc4.so",
		"bin/", 			"libplds4.so",
		"bin/",				"libxpcom.so",
		"bin/",				"libxpistub.so",
		"bin/components/",	"libxpinstall.so",
		"bin/components/",	"libjar50.so"
};

#endif /* _NS_XIENGINE_H_ */
