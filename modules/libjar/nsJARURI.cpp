/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsJARURI.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIJAR.h"
 
static NS_DEFINE_CID(kIOServiceCID,     NS_IOSERVICE_CID);
 
////////////////////////////////////////////////////////////////////////////////
 
nsJARURI::nsJARURI()
{
    NS_INIT_REFCNT();
}
 
nsJARURI::~nsJARURI()
{
}

NS_IMPL_ISUPPORTS2(nsJARURI, nsIJARURI, nsIURI)

NS_METHOD
nsJARURI::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsJARURI* uri = new nsJARURI();

    if (uri == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(uri);
    nsresult rv = uri->Init();

    if (NS_SUCCEEDED(rv)) {
        rv = uri->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(uri);

    return rv;
}
 
nsresult
nsJARURI::Init()
{
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsURI methods:

#define NS_JAR_SCHEME           "jar:"
#define NS_JAR_DELIMITER        "!/"

NS_IMETHODIMP
nsJARURI::GetSpec(char* *aSpec)
{
    nsresult rv;
    char* jarFileSpec;
    rv = mJARFile->GetSpec(&jarFileSpec);
    if (NS_FAILED(rv)) return rv;

    nsCString spec(NS_JAR_SCHEME);
    spec += jarFileSpec;
    nsCRT::free(jarFileSpec);
    spec += NS_JAR_DELIMITER;
    spec += mJAREntry;
    
    *aSpec = nsCRT::strdup(spec);
    return *aSpec ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJARURI::SetSpec(const char * aSpec)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    PRUint32 startPos, endPos;
    rv = serv->ExtractScheme(aSpec, &startPos, &endPos, nsnull);
    if (NS_FAILED(rv)) return rv;

    if (nsCRT::strncmp("jar", &aSpec[startPos], endPos - startPos - 1) != 0)
        return NS_ERROR_FAILURE;

    // Search backward from the end for the "!/" delimiter. Remember, jar URLs
    // can nest, e.g.:
    //    jar:jar:http://www.foo.com/bar.jar!/a.jar!/b.html
    // This gets the b.html document from out of the a.jar file, that's 
    // contained within the bar.jar file.

    nsCAutoString jarPath(aSpec);
    PRInt32 pos = jarPath.RFind(NS_JAR_DELIMITER);
    if (pos == -1 || endPos + 1 > (PRUint32)pos)
        return NS_ERROR_MALFORMED_URI;

    jarPath.Cut(pos, jarPath.Length());
    jarPath.Cut(0, endPos);

    rv = serv->NewURI(jarPath, nsnull, getter_AddRefs(mJARFile));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString entry(aSpec);
    entry.Cut(0, pos + 2);      // 2 == strlen(NS_JAR_DELIMITER)

    mJAREntry = nsCRT::strdup(entry);
    if (mJAREntry == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::GetScheme(char * *aScheme)
{
    *aScheme = nsCRT::strdup("jar");
    return *aScheme ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJARURI::SetScheme(const char * aScheme)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARURI::GetPreHost(char * *aPreHost)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::SetPreHost(const char * aPreHost)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetHost(char * *aHost)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::SetHost(const char * aHost)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetPort(PRInt32 *aPort)
{
    return NS_ERROR_FAILURE;
}
 
NS_IMETHODIMP
nsJARURI::SetPort(PRInt32 aPort)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetPath(char * *aPath)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::SetPath(const char * aPath)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::Equals(nsIURI *other, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARURI::Clone(nsIURI **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARURI::SetRelativePath(const char *i_RelativePath)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARURI::Resolve(const char *relativePath, char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIJARUri methods:

NS_IMETHODIMP
nsJARURI::GetJARFile(nsIURI* *rootURI)
{
    *rootURI = mJARFile;
    NS_ADDREF(*rootURI);
    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::SetJARFile(nsIURI* rootURI)
{
    mJARFile = rootURI;
    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::GetJAREntry(char* *relativeURI)
{
    *relativeURI = nsCRT::strdup(mJAREntry);
    return *relativeURI ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJARURI::SetJAREntry(const char* relativeURI)
{
    if (mJAREntry)
        nsCRT::free(mJAREntry);
    mJAREntry = nsCRT::strdup(relativeURI);
    return mJAREntry ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

////////////////////////////////////////////////////////////////////////////////
