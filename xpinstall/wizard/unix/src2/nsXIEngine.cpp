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

#include "nsXIEngine.h"

#define CORE_LIB_COUNT 10
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
		"bin/",				"libzlib.so",
		"bin/components/",	"libxpinstall.so",
		"bin/components/",	"libjar50.so"
};

nsXIEngine::nsXIEngine() :
    mTmp(NULL),
    mTotalComps(0),
    mOriginalDir(NULL)
{
}

nsXIEngine::~nsXIEngine()   
{
    DUMP("~nsXIEngine");

    // reset back to original directory
    chdir(mOriginalDir);

    XI_IF_FREE(mTmp);
    XI_IF_FREE(mOriginalDir);
}

int     
nsXIEngine::Download(int aCustom, nsComponentList *aComps)
{
    DUMP("Download");

    if (!aComps)
        return E_PARAM;

    int err = OK;
    nsComponent *currComp = aComps->GetHead();
    char *currURL = NULL;
    char *currHost = NULL;
    char *currDir = NULL;
    int i;
    
    mTmp = NULL;
    err = MakeUniqueTmpDir();
    if (!mTmp || err != OK)
        return E_DIR_CREATE;

    // if all .xpis exist in the ./xpi dir (blob/CD) we don't need to download
    if (ExistAllXPIs(aCustom, aComps, &mTotalComps))
        return CopyToTmp(aCustom, aComps);

    int currCompNum = 1;
    while (currComp)
    {
        if ( (aCustom == TRUE && currComp->IsSelected()) || (aCustom == FALSE) )
        {
            for (i = 0; i < MAX_URLS; i++)
            {
                currURL = currComp->GetURL(i);
                if (!currURL) break;
                
                currHost = NULL;
                currDir = NULL;
                err = ParseURL(currURL, &currHost, &currDir);
                if (err == OK)
                {
                    // update UI
                    nsInstallDlg::MajorProgressCB(currComp->GetDescShort(),
                        currCompNum, mTotalComps, nsInstallDlg::ACT_DOWNLOAD);
            
                    err = FTPAnonGet(currHost, currDir, currComp->GetArchive());
                    if (err == OK) 
                    {
                        currCompNum++;
                        break;  // no need to failover
                    }
                }
            }
        }
        
        currComp = currComp->GetNext();
    }
    
    return OK;
}

int     
nsXIEngine::Extract(nsComponent *aXPIEngine)
{
    DUMP("Extract");
   
    char path[MAXPATHLEN];
    char bindir[512];
    char unzipcmd[512];
    struct stat dummy;
    int i;

    if (!aXPIEngine || !(aXPIEngine->GetArchive()))
        return E_PARAM;

    sprintf(path, "%s/%s", mTmp, aXPIEngine->GetArchive());
    if (-1 == stat(path, &dummy))
        return E_NO_DOWNLOAD;

    for (i = 0; i < CORE_LIB_COUNT*2; i++)
    {
        // update UI
        nsInstallDlg::MajorProgressCB(sCoreLibs[i+1], ((i+2)/2),
            CORE_LIB_COUNT, nsInstallDlg::ACT_EXTRACT);

        sprintf(unzipcmd, "unzip %s -d %s %s%s > /dev/null", 
                path, mTmp, sCoreLibs[i], sCoreLibs[i+1]);
        i++;
        system(unzipcmd);
    }
    
    sprintf(bindir, "%s/%s", mTmp, TMP_EXTRACT_SUBDIR);
    if (-1 == stat(bindir, &dummy))
        return E_EXTRACTION;

    return OK;
}

int     
nsXIEngine::Install(int aCustom, nsComponentList *aComps, char *aDestination)
{
    DUMP("Install");

    int err = OK;
    xpistub_t stub;
    char *old_LD_LIBRARY_PATH = NULL;
    char new_LD_LIBRARY_PATH[MAXPATHLEN];
    int i;
    int compNum = 1;
    nsComponent *currComp = NULL;

    if (!aComps || !aDestination)
        return E_PARAM;

    // handle LD_LIBRARY_PATH settings
    sprintf(new_LD_LIBRARY_PATH, "%s/bin:.", mTmp);
    DUMP(new_LD_LIBRARY_PATH);
    old_LD_LIBRARY_PATH = getenv("LD_LIBRARY_PATH");
    setenv("LD_LIBRARY_PATH", new_LD_LIBRARY_PATH, 1);
 
    currComp = aComps->GetHead();
    err = LoadXPIStub(&stub, aDestination);
    if (err == OK)
    {
        for (i = 0; i < MAX_COMPONENTS; i++)
        {
            if (!currComp)
                break;

            if (  (aCustom && currComp->IsSelected()) ||
                  (!aCustom)  )
            {
                nsInstallDlg::MajorProgressCB(currComp->GetDescShort(),
                    compNum, mTotalComps, nsInstallDlg::ACT_INSTALL);
                err = InstallXPI(currComp, &stub);
                if (err != OK)
                    ErrorHandler(err); // handle and continue
                compNum++;
            }

            currComp = currComp->GetNext();
        }
    }

    UnloadXPIStub(&stub);

    // restore LD_LIBRARY_PATH settings
    setenv("LD_LIBRARY_PATH", old_LD_LIBRARY_PATH, 1);

    return err;
}

int
nsXIEngine::MakeUniqueTmpDir()
{
    int err = OK;
    int i;
    char buf[MAXPATHLEN];
    char cmd[1030];
    struct stat dummy;
    mTmp = NULL;

    for (i = 0; i < MAX_TMP_DIRS; i++)
    {
        sprintf(buf, TMP_DIR_TEMPLATE, i);
        if (-1 == stat(buf, &dummy))
            break; 
    }

    mTmp = (char *) malloc( (strlen(buf) * sizeof(char)) + 1 );
    if (!mTmp) return E_MEM;

    sprintf(mTmp, "%s", buf);
    sprintf(cmd, "mkdir %s", mTmp);
    system(cmd);

    return err;
}

int
nsXIEngine::ParseURL(char *aURL, char **aHost, char **aDir)
{
    int err = OK;
    char *host = NULL;
    char *hostTerminator = NULL;
    char *dirTerminator = NULL;

    if (!aURL || !aHost || !aDir)
        return E_PARAM;

    if (0 != strncmp(aURL, "ftp://", 6)) return E_BAD_FTP_URL;

    host = aURL + 6;
    if (!host) return E_BAD_FTP_URL;
    hostTerminator = strchr(host, '/'); 
    if (!hostTerminator) return E_BAD_FTP_URL;
    
    *aHost = (char *) malloc(sizeof(char) * (hostTerminator - host + 1));
    memset(*aHost, 0, (hostTerminator - host + 1));
    strncpy(*aHost, host, hostTerminator - host);

    dirTerminator = strrchr(hostTerminator + 1, '/');
    if (!dirTerminator)
    {
        // no dir == root dir
        *aDir = (char *) malloc(2);
        sprintf(*aDir, "/");
    }
    else
    {
        *aDir = (char *) malloc(sizeof(char) * 
                         (dirTerminator - hostTerminator + 2));
        memset(*aDir, 0, (dirTerminator - hostTerminator + 2));
        strncpy(*aDir, hostTerminator, dirTerminator - hostTerminator + 1);
    }
    
    return err;
}

int
nsXIEngine::FTPAnonGet(char *aHost, char *aDir, char *aArchive)
{
    int err = OK;
    char qualifiedPath[MAXPATHLEN];
    char basename[256];
    char ftpcmds[1024];
    struct stat dummy;

    if (!aHost || !aDir || !aArchive)
        return E_PARAM;

    sprintf(qualifiedPath, "%s%s", aDir, aArchive);
    sprintf(basename, "%s/%s", mTmp, aArchive);
    sprintf(ftpcmds, "\
\
ftp -n %s > /dev/null 2>&1 <<EndFTP\n\
user anonymous dev@null.edu\n\
binary\n\
passive\n\
get %s %s\n\
EndFTP\n\
\
", aHost, qualifiedPath, basename);

    system(ftpcmds);

    if (-1 == stat(basename, &dummy))
        err = E_NO_DOWNLOAD;

    return err;
}

int
nsXIEngine::LoadXPIStub(xpistub_t *aStub, char *aDestination)
{
    int err = OK;

    char libpath[MAXPATHLEN];
    char libloc[MAXPATHLEN];
	char *dlerr;
    nsresult rv = 0;

	DUMP("LoadXPIStub");

    /* param check */
    if (!aStub || !aDestination)
        return E_PARAM;

    /* save original directory to reset it after installing */
    mOriginalDir = (char *) malloc(MAXPATHLEN * sizeof(char));
    getcwd(mOriginalDir, MAXPATHLEN);

    /* chdir to library location for dll deps resolution */
    sprintf(libloc, "%s/bin", mTmp);
    chdir(libloc);
    
	/* open the library */
    getcwd(libpath, MAXPATHLEN);
    sprintf(libpath, "%s/%s", libpath, XPISTUB);

#ifdef DEBUG
printf("DEBUG: libpath = >>%s<<\n", libpath);
#endif

	aStub->handle = NULL;
	aStub->handle = dlopen(libpath, RTLD_LAZY);
	if (!aStub->handle)
	{
		dlerr = dlerror();
		DUMP(dlerr);
		return E_LIB_OPEN;
	}
	DUMP("xpistub opened");
	
	/* read and store symbol addresses */
	aStub->fn_init    = (pfnXPI_Init) dlsym(aStub->handle, FN_INIT);
	aStub->fn_install = (pfnXPI_Install) dlsym(aStub->handle, FN_INSTALL);
	aStub->fn_exit    = (pfnXPI_Exit) dlsym(aStub->handle, FN_EXIT);
	if (!aStub->fn_init || !aStub->fn_install || !aStub->fn_exit)
	{
		dlerr = dlerror();
		DUMP(dlerr);
		err = E_LIB_SYM;
        goto BAIL;
	}
    DUMP("xpistub symbols loaded");

    rv = aStub->fn_init(aDestination, NULL, ProgressCallback);

#ifdef DEBUG
printf("DEBUG: XPI_Init returned 0x%.8X\n", rv);
#endif

    DUMP("XPI_Init called");
	if (NS_FAILED(rv))
	{
		err = E_XPI_FAIL;
        goto BAIL;
	}

    return err;

BAIL:
    return err;
}

int
nsXIEngine::InstallXPI(nsComponent *aXPI, xpistub_t *aStub)
{
    int err = OK;
    char xpipath[MAXPATHLEN];
    nsresult rv = 0;

    if (!aStub || !aXPI)
        return E_PARAM;

    sprintf(xpipath, "../%s", aXPI->GetArchive());
    DUMP(xpipath);

#define XPI_NO_NEW_THREAD 0x1000

    rv = aStub->fn_install(xpipath, "", XPI_NO_NEW_THREAD);

#ifdef DEBUG
printf("DEBUG: XPI_Install %s returned %d\n", aXPI->GetArchive(), rv);
#endif

    if (!NS_SUCCEEDED(rv))
        err = E_INSTALL;

    return err;
}

int
nsXIEngine::UnloadXPIStub(xpistub_t *aStub)
{
    int err = OK;

    /* param check */
    if (!aStub)
        return E_PARAM;

	/* release XPCOM and XPInstall */
    XI_ASSERT(aStub->fn_exit, "XPI_Exit is NULL and wasn't called!");
	if (aStub->fn_exit)
	{
		aStub->fn_exit();
		DUMP("XPI_Exit called");
	}

#if 0
    /* NOTE:
     * ----
     *      Don't close the stub: it'll be released on exit.
     *      This fixes the seg fault on exit bug,
     *      Apparently, the global destructors are no longer
     *      around when the app exits (since xpcom etc. was 
     *      unloaded when the stub was unloaded).  To get 
     *      around this we don't close the stub (which is 
     *      apparently safe on Linux/Unix.
     */

	/* close xpistub library */
	if (aStub->handle)
	{
		dlclose(aStub->handle);
		DUMP("xpistub closed");
	}
#endif

    return err;
}

void
nsXIEngine::ProgressCallback(const char* aMsg, PRInt32 aVal, PRInt32 aMax)
{
    // DUMP("ProgressCallback");
    
    nsInstallDlg::XPIProgressCB(aMsg, (int)aVal, (int)aMax);
}

int 
nsXIEngine::ExistAllXPIs(int aCustom, nsComponentList *aComps, int *aTotal)
{
    // param check
    if (!aComps || !aTotal)
        return E_PARAM;
    
    int bAllExist = TRUE;
    nsComponent *currComp = aComps->GetHead();
    char currArchivePath[256];
    struct stat dummy;

    while (currComp)
    {
        if ( (aCustom == TRUE && currComp->IsSelected()) || (aCustom == FALSE) )
        {
            sprintf(currArchivePath, "%s/%s", XPI_DIR, currComp->GetArchive());
            DUMP(currArchivePath);
            
            if (0 != stat(currArchivePath, &dummy))
                bAllExist = FALSE;

            (*aTotal)++;
        }
        
        currComp = currComp->GetNext();
    }

    return bAllExist;
}

int 
nsXIEngine::CopyToTmp(int aCustom, nsComponentList *aComps)
{
    int err = OK;
    nsComponent *currComp = aComps->GetHead();
    char cmd[256];

    int currCompNum = 1;
    while (currComp)
    {
        if ( (aCustom == TRUE && currComp->IsSelected()) || (aCustom == FALSE) )
        {
            sprintf(cmd, "cp %s/%s %s", XPI_DIR, currComp->GetArchive(), mTmp); 
            DUMP(cmd);

            // update UI
            nsInstallDlg::MajorProgressCB(currComp->GetDescShort(),
                currCompNum, mTotalComps, nsInstallDlg::ACT_DOWNLOAD);

            system(cmd);
            currCompNum++;
        }
        
        currComp = currComp->GetNext();
    }
    
    return err;
}
