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

#include "nsAboutBlank.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIStringStream.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

NS_IMPL_ISUPPORTS(nsAboutBlank, nsCOMTypeInfo<nsIAboutModule>::GetIID());

static const char kBlankPage[] = "<html><body></body></html>";

NS_IMETHODIMP
nsAboutBlank::NewChannel(const char *verb,
                         nsIURI *aURI,
                         nsIEventSinkGetter *eventSinkGetter,
                         nsIChannel **result)
{
    nsresult rv;
    nsIChannel* channel;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsISupports* s;
    rv = NS_NewStringInputStream(&s, kBlankPage);
    if (NS_FAILED(rv)) return rv;
    nsIInputStream* in;

    rv = CallQueryInterface(s, &in);
    NS_RELEASE(s);
    if (NS_FAILED(rv)) return rv;

    rv = serv->NewInputStreamChannel(aURI, "text/html", in, &channel);
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
