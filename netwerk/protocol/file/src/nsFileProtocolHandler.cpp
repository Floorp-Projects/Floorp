/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFileChannel.h"
#include "nsFileProtocolHandler.h"
#include "nsIURL.h"
#include "nsIURLParser.h"
#include "nsIStandardURL.h"
#include "nsIFileURL.h"
#include "nsIPref.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsIThread.h"
#include "nsIThreadPool.h"
#include "nsISupportsArray.h"
#include "nsXPIDLString.h"
#include "nsStandardURL.h"
#include "nsNetCID.h"
#include "nsURLHelper.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

////////////////////////////////////////////////////////////////////////////////

nsFileProtocolHandler::nsFileProtocolHandler()
{
    NS_INIT_ISUPPORTS();
    mGenerateHTMLContent = PR_FALSE;
}

nsresult
nsFileProtocolHandler::Init()
{
    nsresult rv;
    nsCOMPtr<nsIPref> pPref(do_GetService(kPrefCID, &rv)); 
    if (NS_SUCCEEDED(rv) || pPref) { 
        pPref->GetBoolPref("network.dir.generate_html", &mGenerateHTMLContent);
    }

    return NS_OK;
}

nsFileProtocolHandler::~nsFileProtocolHandler()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsFileProtocolHandler,
                              nsIFileProtocolHandler,
                              nsIProtocolHandler,
                              nsISupportsWeakReference);

NS_METHOD
nsFileProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsFileProtocolHandler* ph = new nsFileProtocolHandler();
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
nsFileProtocolHandler::GetScheme(nsACString &result)
{
    result = "file";
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for file: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_NOAUTH;
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::NewURI(const nsACString &aSpec,
                              const char *aCharset,
                              nsIURI *aBaseURI,
                              nsIURI **result)
{
    nsresult rv;

    nsCOMPtr<nsIStandardURL> url = new nsStandardURL(PR_TRUE);
    if (!url)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = url->Init(nsIStandardURL::URLTYPE_NO_AUTHORITY, -1, aSpec, aCharset, aBaseURI);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(url, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
    nsresult rv;
    
    nsFileChannel* channel;
    rv = nsFileChannel::Create(nsnull, NS_GET_IID(nsIFileChannel), (void**)&channel);
    if (NS_FAILED(rv)) return rv;

    rv = channel->Init(-1,      // ioFlags unspecified
                       -1,      // permissions unspecified
                       url,
                       mGenerateHTMLContent);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = channel;
    return NS_OK;
}

NS_IMETHODIMP 
nsFileProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIFileProtocolHandler methods:

NS_IMETHODIMP
nsFileProtocolHandler::NewFileURI(nsIFile *file, nsIURI **result)
{
    nsresult rv;

    nsCOMPtr<nsIFileURL> url = new nsStandardURL(PR_TRUE);
    if (!url)
        return NS_ERROR_OUT_OF_MEMORY;

    // XXX shouldn't we set nsIURI::originCharset ??

    rv = url->SetFile(file);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(url, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::GetURLSpecFromFile(nsIFile *aFile, nsACString &result)
{
    return net_GetURLSpecFromFile(aFile, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::GetFileFromURLSpec(const nsACString &aURL, nsIFile **result)
{
    return net_GetFileFromURLSpec(aURL, result);
}
