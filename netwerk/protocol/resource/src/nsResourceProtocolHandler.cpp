/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsResourceProtocolHandler.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIURL.h"
#include "prmem.h"
#include "prprf.h"
#include "prenv.h"
#include "nsSpecialSystemDirectory.h"

#ifdef XP_PC
#include <windows.h>
static HINSTANCE g_hInst = NULL;
#endif

#ifdef XP_BEOS
#include <OS.h>
#include <image.h>
#endif

static NS_DEFINE_CID(kStandardURLCID,            NS_STANDARDURL_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsResourceProtocolHandler::nsResourceProtocolHandler()
{
    NS_INIT_REFCNT();
}

nsresult
nsResourceProtocolHandler::Init()
{
    return NS_OK;
}

nsResourceProtocolHandler::~nsResourceProtocolHandler()
{
}

NS_IMPL_ISUPPORTS(nsResourceProtocolHandler, nsCOMTypeInfo<nsIProtocolHandler>::GetIID());

NS_METHOD
nsResourceProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsResourceProtocolHandler* ph = new nsResourceProtocolHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = ph->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(ph);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsResourceProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("resource");
    if (*result == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsResourceProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for resource: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsResourceProtocolHandler::MakeAbsolute(const char* aSpec,
                                        nsIURI* aBaseURI,
                                        char* *result)
{
    // XXX optimize this to not needlessly construct the URL

    nsresult rv;
    nsIURI* url;
    rv = NewURI(aSpec, aBaseURI, &url);
    if (NS_FAILED(rv)) return rv;

    rv = url->GetSpec(result);
    NS_RELEASE(url);
    return rv;
}

NS_IMETHODIMP
nsResourceProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                                  nsIURI **result)
{
    nsresult rv;

    // Resource: URLs (currently) have no additional structure beyond that provided by standard
    // URLs, so there is no "outer" given to CreateInstance 

    nsIURI* url;
    if (aBaseURI) {
        rv = aBaseURI->Clone(&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetRelativePath(aSpec);
    }
    else {
        rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                                nsCOMTypeInfo<nsIURI>::GetIID(),
                                                (void**)&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetSpec((char*)aSpec);
    }

    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

/*
 * Rewrite "resource:/" URLs into file: URLs with the path of the 
 * executable prepended to the file path...
 */
static char *
MangleResourceIntoFileURL(const char* aResourceFileName) 
{
    // XXX For now, resources are not in jar files 
    // Find base path name to the resource file
    char* resourceBase;

#ifdef XP_PC
    // XXX For now, all resources are relative to the .exe file
    resourceBase = (char *)PR_Malloc(_MAX_PATH);
    DWORD mfnLen = GetModuleFileName(g_hInst, resourceBase, _MAX_PATH);
    // Truncate the executable name from the rest of the path...
    char* cp = strrchr(resourceBase, '\\');
    if (nsnull != cp) {
        *cp = '\0';
    }
    // Change the first ':' into a '|'
    cp = PL_strchr(resourceBase, ':');
    if (nsnull != cp) {
        *cp = '|';
    }
#endif /* XP_PC */

#ifdef XP_UNIX

    //
    // Obtain the resource: url base from the environment variable
    //
    // MOZILLA_FIVE_HOME
    //
    // Which is the standard place where mozilla stores global (ie, not
    // user specific) data
    //

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024 // A good guess, i suppose
#endif

#define MOZILLA_FIVE_HOME "MOZILLA_FIVE_HOME"

    static char * nsUnixMozillaHomePath = nsnull;

    if (nsnull == nsUnixMozillaHomePath)
    {
        nsUnixMozillaHomePath = PR_GetEnv(MOZILLA_FIVE_HOME);
    }
    if (nsnull == nsUnixMozillaHomePath)
    {
        static char homepath[MAXPATHLEN];
        FILE* pp;
        if (!(pp = popen("pwd", "r"))) {
#ifdef DEBUG
            printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 1\n");
#endif
            return(nsnull);
        }
        if (fgets(homepath, MAXPATHLEN, pp)) {
            homepath[PL_strlen(homepath)-1] = 0;
        }
        else {
#ifdef DEBUG
            printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 2\n");
#endif
            pclose(pp);
            return(nsnull);
        }
        pclose(pp);
        nsUnixMozillaHomePath = homepath;
    }

    resourceBase = nsCRT::strdup(nsUnixMozillaHomePath);
#ifdef DEBUG
    {
        static PRBool firstTime = PR_TRUE;
        if (firstTime) {
            firstTime = PR_FALSE;
            printf("Using '%s' as the resource: base\n", resourceBase);
        }
    }
#endif

#endif /* XP_UNIX */

#ifdef XP_BEOS
    char *moz5 = getenv("MOZILLA_FIVE_HOME");
    if (moz5)
      resourceBase = nsCRT::strdup(moz5);
    else
    {
      static char buf[MAXPATHLEN];
      int32 cookie = 0;
      image_info info;
      char *p;
      *buf = 0;
      if(get_next_image_info(0, &cookie, &info) == B_OK)
      {
        strcpy(buf, info.name);
        if((p = strrchr(buf, '/')) != 0)
        {
          *p = 0;
          resourceBase = nsCRT::strdup(buf);
        }
        else
          return nsnull;
      }
      else
        return nsnull;
    }
#endif

#ifdef XP_MAC
   // resourceBase = nsCRT::strdup("usr/local/netscape/bin");
    nsSpecialSystemDirectory netscapeDir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
    nsFilePath netscapePath(netscapeDir);
    resourceBase = nsCRT::strdup(1+(const char *)netscapePath);

#endif /* XP_MAC */

    // Join base path to resource name
    if (aResourceFileName[0] == '/') {
        aResourceFileName++;
    }
    PRInt32 baseLen = PL_strlen(resourceBase);
    PRInt32 resLen = PL_strlen(aResourceFileName);
    PRInt32 totalLen = 8 + baseLen + 1 + resLen + 1;
    char* fileName = (char *)PR_Malloc(totalLen);
    PR_snprintf(fileName, totalLen, "file:///%s/%s", resourceBase, aResourceFileName);

#ifdef XP_PC
    // Change any backslashes into foreward slashes...
    while ((cp = PL_strchr(fileName, '\\')) != 0) {
        *cp = '/';
        cp++;
    }
#endif /* XP_PC */

    PR_Free(resourceBase);

    return fileName;
}

NS_IMETHODIMP
nsResourceProtocolHandler::NewChannel(const char* verb, nsIURI* uri,
                                      nsIEventSinkGetter* eventSinkGetter,
                                      nsIEventQueue* eventQueue,
                                      nsIChannel* *result)
{
    nsresult rv;

    // XXX Later we're going to change resource URLs to do something 
    // else, like look in a jar file or something.

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // Here's where we translate the resource: URL into a file: URL:
    char* path;
    rv = uri->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    char* filePath;
    filePath = MangleResourceIntoFileURL(path);
    nsCRT::free(path);
    if (filePath == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsIChannel* channel;
    rv = serv->NewChannel(verb, filePath, uri, eventSinkGetter, &channel);
    nsCRT::free(filePath);
    if (NS_FAILED(rv)) return rv;

    nsIURL* url;
    rv = uri->QueryInterface(nsCOMTypeInfo<nsIURL>::GetIID(), (void**)&url);
    if (NS_SUCCEEDED(rv)) {
        char* query;
        rv = url->GetQuery(&query);
        if (NS_SUCCEEDED(rv)) {
            nsIURI* fileURI;
            rv = channel->GetURI(&fileURI);
            if (NS_SUCCEEDED(rv)) {
                nsIURL* fileURL;
                rv = fileURI->QueryInterface(nsCOMTypeInfo<nsIURL>::GetIID(), (void**)&fileURL);
                if (NS_SUCCEEDED(rv)) {
                    (void)fileURL->SetQuery(query);
                    NS_RELEASE(fileURL);
                }
                NS_RELEASE(fileURI);
            }
            nsCRT::free(query);
        }
        NS_RELEASE(url);
    }
        
    *result = channel;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
