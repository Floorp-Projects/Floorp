/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*

  A data source that can read itself from and write itself to a stream.

 */

#include "nsIDTD.h"
#include "nsINameSpaceManager.h"
#include "nsIParser.h"
#include "nsIRDFContentSink.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsMemoryDataSource.h"
#include "nsParserCIID.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIDTDIID,            NS_IDTD_IID);
static NS_DEFINE_IID(kIParserIID,         NS_IPARSER_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);

static NS_DEFINE_CID(kParserCID,          NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kWellFormedDTDCID,   NS_WELLFORMEDDTD_CID);


////////////////////////////////////////////////////////////////////////

class StreamDataSourceImpl : public nsMemoryDataSource
{
protected:
    nsIURL* mURL;

public:
    StreamDataSourceImpl(void);
    virtual ~StreamDataSourceImpl(void);

    // nsIDataSource methods that are overloaded from nsMemoryDataSource
    NS_IMETHOD Init(const nsString& uri);
    NS_IMETHOD Flush(void);
};


////////////////////////////////////////////////////////////////////////


StreamDataSourceImpl::StreamDataSourceImpl(void)
    : mURL(nsnull)
{
}


StreamDataSourceImpl::~StreamDataSourceImpl(void)
{
    NS_IF_RELEASE(mURL);
}


NS_IMETHODIMP
StreamDataSourceImpl::Init(const nsString& uri)
{
    nsresult rv;

    if (NS_FAILED(rv = NS_NewURL(&mURL, uri)))
        return rv;

    nsINameSpaceManager* ns = nsnull;
    nsIRDFContentSink* sink = nsnull;
    nsIParser* parser       = nsnull;
    nsIDTD* dtd             = nsnull;
    nsIStreamListener* lsnr = nsnull;

    // XXX Get a namespace manager *here*
    PR_ASSERT(0);

    if (NS_FAILED(rv = NS_NewRDFSimpleContentSink(&sink, mURL, ns)))
        goto done;

    if (NS_FAILED(rv = sink->SetDataSource(this)))
        goto done;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kParserCID,
                                                    nsnull,
                                                    kIParserIID,
                                                    (void**) &parser)))
        goto done;

    parser->SetContentSink(sink);

    if (NS_FAILED(rv = nsRepository::CreateInstance(kWellFormedDTDCID,
                                                    nsnull,
                                                    kIDTDIID,
                                                    (void**) &dtd)))
        goto done;

    parser->RegisterDTD(dtd);

    if (NS_FAILED(rv = parser->QueryInterface(kIStreamListenerIID, (void**) &lsnr)))
        goto done;

    // XXX This _can't_ be all I need to do -- how do I start the parser?
    if (NS_FAILED(parser->Parse(mURL)))
        goto done;

    if (NS_FAILED(NS_OpenURL(mURL, lsnr)))
        goto done;

done:
    NS_IF_RELEASE(lsnr);
    NS_IF_RELEASE(dtd);
    NS_IF_RELEASE(parser);
    NS_IF_RELEASE(sink);
    return rv;
}


NS_IMETHODIMP
StreamDataSourceImpl::Flush(void)
{
    // XXX walk through all the resources, writing <RDF:Description>
    // elements with properties for each.
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFStreamDataSource(nsIRDFDataSource** result)
{
    StreamDataSourceImpl* ds = new StreamDataSourceImpl();
    if (! ds)
        return NS_ERROR_NULL_POINTER;

    *result = ds;
    NS_ADDREF(*result);
    return NS_OK;
}
