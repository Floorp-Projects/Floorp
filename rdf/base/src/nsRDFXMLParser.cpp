/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributor(s): 
 *   Chris Waterson <waterson@netscape.com>
 */

#include "nsRDFXMLParser.h"

#include "nsIComponentManager.h"
#include "nsIParser.h"
#include "nsIRDFContentSink.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);

NS_IMPL_ISUPPORTS1(nsRDFXMLParser, nsIRDFXMLParser)

NS_IMETHODIMP
nsRDFXMLParser::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsRDFXMLParser* result = new nsRDFXMLParser();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    NS_ADDREF(result);
    rv = result->QueryInterface(aIID, aResult);
    NS_RELEASE(result);
    return rv;
}

nsRDFXMLParser::nsRDFXMLParser()
{
    MOZ_COUNT_CTOR(nsRDFXMLParser);
}

nsRDFXMLParser::~nsRDFXMLParser()
{
    MOZ_COUNT_DTOR(nsRDFXMLParser);
}

NS_IMETHODIMP
nsRDFXMLParser::ParseAsync(nsIRDFDataSource* aSink, nsIURI* aBaseURI, nsIStreamListener** aResult)
{
    nsresult rv;

    nsCOMPtr<nsIRDFContentSink> sink =
        do_CreateInstance("@mozilla.org/rdf/content-sink;1", &rv);

    if (NS_FAILED(rv)) return rv;

    rv = sink->Init(aBaseURI);
    if (NS_FAILED(rv)) return rv;

    // We set the content sink's data source directly to our in-memory
    // store. This allows the initial content to be generated "directly".
    rv = sink->SetDataSource(aSink);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID, &rv);
    if (NS_FAILED(rv)) return rv;

    parser->SetDocumentCharset(NS_LITERAL_STRING("UTF-8"),
                               kCharsetFromDocTypeDefault);
    parser->SetContentSink(sink);

    rv = parser->Parse(aBaseURI);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(parser, aResult);
}

NS_IMETHODIMP
nsRDFXMLParser::ParseString(nsIRDFDataSource* aSink, nsIURI* aBaseURI, const nsAString& aString)
{
    nsresult rv;

    nsCOMPtr<nsIRDFContentSink> sink =
        do_CreateInstance("@mozilla.org/rdf/content-sink;1", &rv);

    if (NS_FAILED(rv)) return rv;

    rv = sink->Init(aBaseURI);
    if (NS_FAILED(rv)) return rv;

    // We set the content sink's data source directly to our in-memory
    // store. This allows the initial content to be generated "directly".
    rv = sink->SetDataSource(aSink);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID, &rv);
    if (NS_FAILED(rv)) return rv;

    parser->SetDocumentCharset(NS_LITERAL_STRING("UTF-8"),
                               kCharsetFromDocTypeDefault);
    parser->SetContentSink(sink);

    rv = parser->Parse(aBaseURI);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListener> listener =
        do_QueryInterface(parser);

    if (! listener)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewStringInputStream(getter_AddRefs(stream), aString);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewInputStreamChannel(getter_AddRefs(channel), aBaseURI, stream,
                                  NS_LITERAL_CSTRING("text/xml"),
                                  NS_LITERAL_CSTRING(""));
    if (NS_FAILED(rv)) return rv;

    listener->OnStartRequest(channel, nsnull);
    listener->OnDataAvailable(channel, nsnull, stream, 0, aString.Length());
    listener->OnStopRequest(channel, nsnull, NS_OK);

    return NS_OK;
}
