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

#include "nsNeckoUtil.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

NS_NET nsresult
NS_NewURI(nsIURI* *result, const char* spec, nsIURI* baseURI)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    rv = serv->NewURI(spec, baseURI, result);
    return rv;
}

NS_NET nsresult
NS_OpenURI(nsIURI* uri, nsIInputStream* *result)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsIChannel* channel;
    rv = serv->NewChannelFromURI("load", uri, nsnull, &channel);
    if (NS_FAILED(rv)) return rv;

    nsIInputStream* inStr;
    rv = channel->OpenInputStream(0, -1, &inStr);
    NS_RELEASE(channel);
    if (NS_FAILED(rv)) return rv;

    *result = inStr;
    return rv;
}

NS_NET nsresult
NS_OpenURI(nsIURL* uri, nsIStreamListener* aConsumer)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsIChannel* channel;
    rv = serv->NewChannelFromURI("load", uri, nsnull, &channel);
    if (NS_FAILED(rv)) return rv;

    rv = channel->AsyncRead(0, -1, uri, // uri used as context
                            nsnull,     // XXX need event queue of current thread
                            aConsumer);
    NS_RELEASE(channel);
    return rv;
}

NS_NET nsresult
NS_MakeAbsoluteURI(const char* spec, nsIURI* base, char* *result)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    return serv->MakeAbsolute(spec, base, result);
}

////////////////////////////////////////////////////////////////////////////////

NS_NET nsresult
NS_NewURL(nsIURL* *result, const char* spec, nsIURL* baseURL)
{
    nsresult rv;
    nsIURI* uri;
    rv = NS_NewURI(&uri, spec, baseURL);
    if (NS_FAILED(rv)) return rv;

    rv = uri->QueryInterface(nsIURL::GetIID(), (void**)result);
    NS_RELEASE(uri);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
