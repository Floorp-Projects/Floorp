/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsAboutBlank.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIStringStream.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

NS_IMPL_ISUPPORTS(nsAboutBlank, NS_GET_IID(nsIAboutModule));

static const char kBlankPage[] = "";

NS_IMETHODIMP
nsAboutBlank::NewChannel(nsIURI *aURI, nsIChannel **result)
{
    nsresult rv;
    nsIChannel* channel;
    nsISupports* s;
    rv = NS_NewCStringInputStream(&s, nsCAutoString(kBlankPage));
    if (NS_FAILED(rv)) return rv;
    nsIInputStream* in;

    rv = CallQueryInterface(s, &in);
    NS_RELEASE(s);
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewInputStreamChannel(&channel, aURI, in, "text/html", 
                                  nsCRT::strlen(kBlankPage));
    NS_RELEASE(in);
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    return rv;
}

NS_METHOD
nsAboutBlank::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsAboutBlank* about = new nsAboutBlank();
    if (about == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(about);
    nsresult rv = about->QueryInterface(aIID, aResult);
    NS_RELEASE(about);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
