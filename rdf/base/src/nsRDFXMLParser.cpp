/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
                               kCharsetFromDocTypeDefault);
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

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewInputStreamChannel(getter_AddRefs(channel), aBaseURI, stream,
                                  NS_LITERAL_CSTRING("text/xml"),
                                  EmptyCString());
    if (NS_FAILED(rv)) return rv;

    listener->OnStartRequest(channel, nsnull);
    listener->OnDataAvailable(channel, nsnull, stream, 0, aString.Length());
    listener->OnStopRequest(channel, nsnull, NS_OK);

    return NS_OK;
}
