/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsJARProtocolHandler.h"
#include "nsIIOService.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsJARURI.h"
#include "nsIURL.h"
#include "nsJARChannel.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsNetCID.h"
#include "nsIMIMEService.h"
#include "nsMimeTypes.h"

static NS_DEFINE_CID(kZipReaderCacheCID, NS_ZIPREADERCACHE_CID);

#define NS_JAR_CACHE_SIZE 32

//-----------------------------------------------------------------------------

nsJARProtocolHandler *gJarHandler = nsnull;

nsJARProtocolHandler::nsJARProtocolHandler()
{
    gJarHandler = this;
}

nsJARProtocolHandler::~nsJARProtocolHandler()
{
    gJarHandler = nsnull;
}

nsresult
nsJARProtocolHandler::Init()
{
    nsresult rv;

    mJARCache = do_CreateInstance(kZipReaderCacheCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = mJARCache->Init(NS_JAR_CACHE_SIZE);
    return rv;
}

nsIMIMEService * 
nsJARProtocolHandler::MimeService()
{
    if (!mMimeService)
        mMimeService = do_GetService("@mozilla.org/mime;1");

    return mMimeService.get();
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsJARProtocolHandler,
                              nsIJARProtocolHandler,
                              nsIProtocolHandler,
                              nsISupportsWeakReference)

NS_METHOD
nsJARProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsJARProtocolHandler* ph = new nsJARProtocolHandler();
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

NS_IMETHODIMP
nsJARProtocolHandler::GetJARCache(nsIZipReaderCache* *result)
{
    *result = mJARCache;
    NS_ADDREF(*result);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsJARProtocolHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral("jar");
    return NS_OK;
}

NS_IMETHODIMP
nsJARProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for JAR: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsJARProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_NORELATIVE | URI_NOAUTH;
    /* Although jar uris have their own concept of relative urls
       it is very different from the standard behaviour, so we
       have to say norelative here! */
    return NS_OK;
}

NS_IMETHODIMP
nsJARProtocolHandler::NewURI(const nsACString &aSpec,
                             const char *aCharset,
                             nsIURI *aBaseURI,
                             nsIURI **result)
{
    nsresult rv = NS_OK;
    nsIURI* url;

    nsJARURI *jarURI = new nsJARURI();
    if (!jarURI)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(url = jarURI);

    rv = jarURI->Init(aCharset);
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    if (aBaseURI) {
        nsCAutoString aResolvedURI;
        rv = aBaseURI->Resolve(aSpec, aResolvedURI);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetSpec(aResolvedURI);
    }
    else
        rv = url->SetSpec(aSpec);

    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsJARProtocolHandler::NewChannel(nsIURI *uri, nsIChannel **result)
{
    nsJARChannel *chan = new nsJARChannel();
    if (!chan)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(chan);

    nsresult rv = chan->Init(uri);
    if (NS_FAILED(rv)) {
        NS_RELEASE(chan);
        return rv;
    }

    *result = chan;
    return NS_OK;
}


NS_IMETHODIMP
nsJARProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.
    *_retval = PR_FALSE;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
