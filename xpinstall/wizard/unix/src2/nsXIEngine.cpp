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

#include "nsFTPConn.h"
#include "nsHTTPConn.h"
#include "nsXIEngine.h"

#include <errno.h>

#define CORE_LIB_COUNT 11

const char kHTTPProto[] = "http://";
const char kFTPProto[] = "ftp://";
const char kDLMarkerPath[] = "./xpi/.current_download";

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
EventPumpCB(void)
{
    return 0;
}

int     
nsXIEngine::Download(int aCustom, nsComponentList *aComps)
{
    DUMP("Download");

    if (!aComps)
        return E_PARAM;

    int err = OK;
    nsComponent *currComp = aComps->GetHead(), *markedComp = NULL;
    char *currURL = NULL;
    char *currHost = NULL;
    char *currPath = NULL;
    char localPath[MAXPATHLEN];
    char *srvPath = NULL;
    char *proxyURL = NULL;
    char *qualURL = NULL;
    int i;
    int currPort;
    struct stat stbuf;
    int resPos = 0;
    int fileSize = 0;
    int currCompNum = 1, markedCompNum = 0;
    int numToDL = 0; // num xpis to download
    
    err = GetDLMarkedComp(aComps, aCustom, &markedComp, &markedCompNum);
    if (err == OK && markedComp)
    {
        currComp = markedComp;
        currCompNum = markedCompNum;
        sprintf(localPath, "%s/%s", XPI_DIR, currComp->GetArchive());
        currComp->SetResumePos(GetFileSize(localPath));
    }
    else
    {
        // if all .xpis exist in the ./xpi dir (blob/CD) 
        // we don't need to download
        if (ExistAllXPIs(aCustom, aComps, &mTotalComps))
            return OK; 
    }

    // check if ./xpi dir exists else create it
    if (0 != stat(XPI_DIR, &stbuf))
    {
        if (0 != mkdir(XPI_DIR, 0755))
            return E_MKDIR_FAIL;
    }

    numToDL = TotalToDownload(aCustom, aComps);

    while (currComp)
    {
        if ( (aCustom == TRUE && currComp->IsSelected()) || (aCustom == FALSE) )
        {
            // in case we are resuming inter- or intra-installer session
            if (currComp->IsDownloaded())
            {
                currComp = currComp->GetNext();
                continue;
            }

            SetDLMarker(currComp->GetArchive());

            for (i = 0; i < MAX_URLS; i++)
            {
                currURL = currComp->GetURL(i);
                if (!currURL) break;
                
                nsInstallDlg::SetDownloadComp(currComp, i, 
                    currCompNum, numToDL);

                // restore resume position
                resPos = currComp->GetResumePos();

                // has a proxy server been specified?
                if (gCtx->opt->mProxyHost && gCtx->opt->mProxyPort)
                {
                    // URL of the proxy server
                    proxyURL = (char *) malloc(strlen(kHTTPProto) + 
                                        strlen(gCtx->opt->mProxyHost) + 1 +
                                        strlen(gCtx->opt->mProxyPort) + 1);
                    if (!proxyURL)
                    {
                        err = E_MEM;
                        break;
                    }

                    sprintf(proxyURL, "%s%s:%s", kHTTPProto,
                            gCtx->opt->mProxyHost, gCtx->opt->mProxyPort);

                    nsHTTPConn *conn = new nsHTTPConn(proxyURL, EventPumpCB);
                    if (!conn)
                    {
                        err = E_MEM;
                        break;
                    }

                    // URL of the actual file to download
                    qualURL = (char *) malloc(strlen(currURL) + 
                                       strlen(currComp->GetArchive()) + 1);
                    if (!qualURL)
                    {
                        err = E_MEM;
                        break;
                    }
                    sprintf(qualURL, "%s%s", currURL, currComp->GetArchive());

                    conn->SetProxyInfo(qualURL, gCtx->opt->mProxyUser,
                                                gCtx->opt->mProxyPswd);
                    err = conn->Open();
                    if (err == nsHTTPConn::OK)
                    {
                        sprintf(localPath, "%s/%s", XPI_DIR,
                            currComp->GetArchive());
                        err = conn->Get(nsInstallDlg::DownloadCB, localPath,
                                        resPos);
                        conn->Close();
                    }
                    
                    XI_IF_FREE(proxyURL);
                    XI_IF_FREE(qualURL);
                    XI_IF_DELETE(conn);
                }
            
                // is this an HTTP URL?
                else if (strncmp(currURL, kHTTPProto, strlen(kHTTPProto)) == 0)
                {
                    // URL of the actual file to download
                    qualURL = (char *) malloc(strlen(currURL) + 
                                       strlen(currComp->GetArchive()) + 1);
                    if (!qualURL)
                    {
                        err = E_MEM;
                        break;
                    }
                    sprintf(qualURL, "%s%s", currURL, currComp->GetArchive());

                    nsHTTPConn *conn = new nsHTTPConn(qualURL, EventPumpCB);
                    if (!conn)
                    {
                        err = E_MEM;
                        break;
                    }
    
                    err = conn->Open();
                    if (err == nsHTTPConn::OK)
                    {
                        sprintf(localPath, "%s/%s", XPI_DIR,
                            currComp->GetArchive());
                        err = conn->Get(nsInstallDlg::DownloadCB, localPath,
                                        resPos);
                        conn->Close();
                    }

                    XI_IF_FREE(qualURL);
                    XI_IF_DELETE(conn);
                }

                // or is this an FTP URL? 
                else if (strncmp(currURL, kFTPProto, strlen(kFTPProto)) == 0)
                {
                    err = nsHTTPConn::ParseURL(kFTPProto, currURL, &currHost, 
                            &currPort, &currPath);
                    if (err != nsHTTPConn::OK)
                        break;
    
                    // path on the remote server
                    srvPath = (char *) malloc(strlen(currPath) +
                                        strlen(currComp->GetArchive()) + 1);
                    if (!srvPath)
                    {
                        err = E_MEM;
                        break;
                    }
                    sprintf(srvPath, "%s%s", currPath, currComp->GetArchive());

                    nsFTPConn *conn = new nsFTPConn(currHost, EventPumpCB);
                    if (!conn)
                    {
                        err = E_MEM;
                        break;
                    }
                
                    err = conn->Open();
                    if (err == nsFTPConn::OK)
                    {
                        sprintf(localPath, "%s/%s", XPI_DIR,
                            currComp->GetArchive());
                        err = conn->Get(srvPath, localPath, nsFTPConn::BINARY, 
                            resPos, 1, nsInstallDlg::DownloadCB);
                        conn->Close();
                    }

                    XI_IF_FREE(currHost);
                    XI_IF_FREE(currPath);
                    XI_IF_FREE(srvPath);
                    XI_IF_DELETE(conn);
                }

                // else error: malformed URL
                else
                {
                    err = nsHTTPConn::E_MALFORMED_URL;
                }

                if (err == nsHTTPConn::E_USER_CANCEL)
                    err = nsInstallDlg::CancelOrPause();

                // user hit pause and subsequently resumed
                if (err == nsInstallDlg::E_DL_PAUSE)
                {
                    currComp->SetResumePos(GetFileSize(localPath));
                    return err;
                }

                // user cancelled during download
                else if (err == nsInstallDlg::E_DL_CANCEL)
                    return err;

                // user didn't cancel or pause: some other dl error occured
                else if (err != OK)
                {
                    fileSize = GetFileSize(localPath);

                    if (fileSize > 0)
                    {
                        // assume dropped connection if file size > 0
                        currComp->SetResumePos(fileSize);
                        return nsInstallDlg::E_DL_DROP_CXN;
                    }
                    else
                    {
                        // failover
                        continue;
                    }
                }

                nsInstallDlg::ClearRateLabel(); // clean after ourselves

                if (err == OK) 
                {
                    currComp->SetDownloaded();
                    currCompNum++;
                    break;  // no need to failover
                }
            }
        }
        
        currComp = currComp->GetNext();
    }
    
    // download complete: remove marker
    DelDLMarker();
    return OK;
}

int     
nsXIEngine::Extract(nsComponent *aXPIEngine)
{
    int rv;

    if (!aXPIEngine)
        return E_PARAM;

    mTmp = NULL;
    rv = MakeUniqueTmpDir();
    if (!mTmp || rv != OK)
        return E_DIR_CREATE;

    nsZipExtractor *unzip = new nsZipExtractor(XPI_DIR, mTmp);
    rv = unzip->Extract(aXPIEngine, CORE_LIB_COUNT);
    XI_IF_DELETE(unzip);

    return rv;
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
#ifdef SOLARIS
    sprintf(new_LD_LIBRARY_PATH, "LD_LIBRARY_PATH=%s/bin:.", mTmp);
#else
    sprintf(new_LD_LIBRARY_PATH, "%s/bin:.", mTmp);
#endif
    DUMP(new_LD_LIBRARY_PATH);
    old_LD_LIBRARY_PATH = getenv("LD_LIBRARY_PATH");
#ifdef SOLARIS
    putenv(new_LD_LIBRARY_PATH);
#else
    setenv("LD_LIBRARY_PATH", new_LD_LIBRARY_PATH, 1);
#endif 
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
#ifdef SOLARIS
    char old_LD_env[MAXPATHLEN];

    sprintf(old_LD_env, "LD_LIBRARY_PATH=%s", old_LD_LIBRARY_PATH);
    putenv(old_LD_env);
#else
    setenv("LD_LIBRARY_PATH", old_LD_LIBRARY_PATH, 1);
#endif

    return err;
}

int
nsXIEngine::MakeUniqueTmpDir()
{
    int err = OK;
    int i;
    char buf[MAXPATHLEN];
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
    mkdir(mTmp, 0755);

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

    if (!aStub || !aXPI || !mOriginalDir)
        return E_PARAM;

    sprintf(xpipath, "%s/%s/%s", mOriginalDir, XPI_DIR, aXPI->GetArchive());
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
     *      apparently safe on Linux/Unix).
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
nsXIEngine::DeleteXPIs(int aCustom, nsComponentList *aComps)
{
    int err = OK;
    nsComponent *currComp = aComps->GetHead();
    char currXPIPath[MAXPATHLEN];

    if (!aComps || !mOriginalDir)
        return E_PARAM;

    while (currComp)
    {
        if ( (aCustom == TRUE && currComp->IsSelected()) || (aCustom == FALSE) )
        {
            sprintf(currXPIPath, "%s/%s/%s", mOriginalDir, XPI_DIR, 
                    currComp->GetArchive());
            
            // delete the xpi
            err = unlink(currXPIPath);

#ifdef DEBUG
            printf("%s %d: unlink %s returned: %d\n", __FILE__, __LINE__, 
                currXPIPath, err); 
#endif
        }

        currComp = currComp->GetNext();
    }

    // all xpi should be deleted so delete the ./xpi dir
    sprintf(currXPIPath, "%s/xpi", mOriginalDir);
    err = rmdir(currXPIPath);

#ifdef DEBUG
    printf("%s %d: rmdir %s returned: %d\n", __FILE__, __LINE__, 
        currXPIPath, err);
#endif
    
    return err;
}

int
nsXIEngine::GetFileSize(char *aPath)
{
    struct stat stbuf;

    if (!aPath)
        return 0;

    if (0 == stat(aPath, &stbuf))
    {
        return stbuf.st_size;
    }

    return 0;
}

int
nsXIEngine::SetDLMarker(char *aCompName)
{
    int rv = OK;
    FILE *dlMarkerFD;
    int compNameLen;

    if (!aCompName)
        return E_PARAM;

    // open the marker file
    dlMarkerFD = fopen(kDLMarkerPath, "w");
    if (!dlMarkerFD)
        return E_OPEN_MKR;

    // write out the current comp name
    compNameLen = strlen(aCompName);
    if (compNameLen > 0)
    {
        rv = fwrite((void *) aCompName, sizeof(char), compNameLen, dlMarkerFD);
        if (rv != compNameLen)
            rv = E_WRITE_MKR;
        else
            rv = OK;
    }

    // close the marker file
    fclose(dlMarkerFD);
        
#ifdef DEBUG
    printf("%s %d: SetDLMarker rv = %d\n", __FILE__, __LINE__, rv);
#endif
    return rv;
}

int
nsXIEngine::GetDLMarkedComp(nsComponentList *aComps, int aCustom, 
                            nsComponent **aOutComp, int *aOutCompNum)
{
    int rv = OK;
    FILE *dlMarkerFD = NULL;
    struct stat stbuf;
    char *compNameInFile = NULL;
    int compNum = 1;
    nsComponent *currComp = NULL;

    if (!aComps || !aOutComp || !aOutCompNum)
        return E_PARAM;

    *aOutComp = NULL;
    currComp = aComps->GetHead();

    // open the marker file
    dlMarkerFD = fopen(kDLMarkerPath, "r");
    if (!dlMarkerFD)
        return E_OPEN_MKR;

    // find it's length 
    if (0 != stat(kDLMarkerPath, &stbuf))
    {
        rv = E_STAT;
        goto BAIL;
    }
    if (stbuf.st_size <= 0)
    {
        rv = E_FIND_COMP;
        goto BAIL;
    }

    // allocate a buffer the length of the file
    compNameInFile = (char *) malloc(sizeof(char) * (stbuf.st_size + 1));
    if (!compNameInFile)
    {
        rv = E_MEM;
        goto BAIL;
    }
    memset(compNameInFile, 0 , (stbuf.st_size + 1));

    // read in the file contents
    rv = fread((void *) compNameInFile, sizeof(char), 
               stbuf.st_size, dlMarkerFD);
    if (rv != stbuf.st_size)
        rv = E_READ_MKR;
    else
        rv = OK; 

    if (rv == OK)
    {
        // compare the comp name read in with all those in the components list
        while (currComp)
        {
            if ( (aCustom == TRUE && currComp->IsSelected()) || 
                 (aCustom == FALSE) )
            {
                if (strcmp(currComp->GetArchive(), compNameInFile) == 0)
                {
                    *aOutComp = currComp;
                    break;
                }

                compNum++;
            }
            
            currComp = currComp->GetNext();
        }
    }

    *aOutCompNum = compNum;

BAIL:
    if (dlMarkerFD)
        fclose(dlMarkerFD);

    XI_IF_FREE(compNameInFile);

    return rv;
}

int
nsXIEngine::DelDLMarker()
{
    return unlink(kDLMarkerPath);
}

int
nsXIEngine::TotalToDownload(int aCustom, nsComponentList *aComps)
{
    int total = 0;
    nsComponent *currComp;

    if (!aComps)
        return 0;

    currComp = aComps->GetHead();
    while (currComp)
    {
        if ( (aCustom == TRUE && currComp->IsSelected()) || (aCustom == FALSE) )
        {
            if (!currComp->IsDownloaded())
                total++;
        }
        currComp = currComp->GetNext();
    }

    return total;
}
