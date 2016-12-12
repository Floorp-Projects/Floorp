/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRDFXMLParser.h"

#include "nsIComponentManager.h"
#include "nsIParser.h"
#include "nsCharsetSource.h"
#include "nsIRDFContentSink.h"
#include "nsParserCIID.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"
#include "nsNullPrincipal.h"

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);

NS_IMPL_ISUPPORTS(nsRDFXMLParser, nsIRDFXMLParser)

nsresult
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
}

nsRDFXMLParser::~nsRDFXMLParser()
{
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

    parser->SetDocumentCharset(NS_LITERAL_CSTRING("UTF-8"),
                               kCharsetFromDocTypeDefault);
    parser->SetContentSink(sink);

    rv = parser->Parse(aBaseURI);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(parser, aResult);
}

NS_IMETHODIMP
nsRDFXMLParser::ParseString(nsIRDFDataSource* aSink, nsIURI* aBaseURI, const nsACString& aString)
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

    parser->SetDocumentCharset(NS_LITERAL_CSTRING("UTF-8"),
                               kCharsetFromOtherComponent);
    parser->SetContentSink(sink);

    rv = parser->Parse(aBaseURI);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListener> listener =
        do_QueryInterface(parser);

    if (! listener)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewCStringInputStream(getter_AddRefs(stream), aString);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIPrincipal> nullPrincipal = nsNullPrincipal::Create();

    // The following channel is never openend, so it does not matter what
    // securityFlags we pass; let's follow the principle of least privilege.
    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewInputStreamChannel(getter_AddRefs(channel),
                                  aBaseURI,
                                  stream,
                                  nullPrincipal,
                                  nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
                                  nsIContentPolicy::TYPE_OTHER,
                                  NS_LITERAL_CSTRING("text/xml"));
    if (NS_FAILED(rv)) return rv;

    listener->OnStartRequest(channel, nullptr);
    listener->OnDataAvailable(channel, nullptr, stream, 0, aString.Length());
    listener->OnStopRequest(channel, nullptr, NS_OK);

    return NS_OK;
}
