/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsIDTD.h"
#include "nsIParser.h"
#include "nsIStreamListener.h"
#include "nsIWebShell.h"
#include "nsIURL.h"
#include "nsParserCIID.h"
#include "nsRDFContentSink.h"
#include "nsRDFDocument.h"
#include "nsWellFormedDTD.h"

#include "nsRDFCID.h"
#include "nsIRDFDataBase.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"

#include "nsIHTMLStyleSheet.h" // for basic styles

static NS_DEFINE_IID(kIDocumentIID,      NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIParserIID,        NS_IPARSER_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,   NS_IRDFDOCUMENT_IID);
static NS_DEFINE_IID(kIWebShellIID,      NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIXMLDocumentIID,   NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFDataBaseIID,   NS_IRDFDATABASE_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID, NS_IRDFDATASOURCE_IID);

static NS_DEFINE_IID(kParserCID,              NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kRDFSimpleDataBaseCID,   NS_RDFSIMPLEDATABASE_CID);
static NS_DEFINE_CID(kRDFMemoryDataSourceCID, NS_RDFMEMORYDATASOURCE_CID);

struct NameSpaceStruct {
    nsIAtom* mPrefix;
    nsString mURI;
}; 

////////////////////////////////////////////////////////////////////////

nsRDFDocument::nsRDFDocument()
    : mParser(NULL),
      mNameSpaces(NULL),
      mDB(NULL)
{
}

nsRDFDocument::~nsRDFDocument()
{
    NS_IF_RELEASE(mParser);
    if (mNameSpaces) {
        for (PRInt32 i = 0; i < mNameSpaces->Count(); ++i) {
            NameSpaceStruct* ns =
                static_cast<NameSpaceStruct*>(mNameSpaces->ElementAt(i));

            if (! ns)
                continue;

            NS_IF_RELEASE(ns->mPrefix);
            delete ns;
        }
        delete mNameSpaces;
        mNameSpaces = NULL;
    }
    NS_IF_RELEASE(mDB);
}

NS_IMETHODIMP 
nsRDFDocument::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kIRDFDocumentIID) ||
        iid.Equals(kIXMLDocumentIID)) {
        *result = static_cast<nsIRDFDocument*>(this);
        AddRef();
        return NS_OK;
    }
    return nsMarkupDocument::QueryInterface(iid, result);
}

nsrefcnt nsRDFDocument::AddRef()
{
    return nsDocument::AddRef();
}

nsrefcnt nsRDFDocument::Release()
{
    return nsDocument::Release();
}


NS_IMETHODIMP 
nsRDFDocument::StartDocumentLoad(nsIURL *aUrl, 
                                 nsIContentViewerContainer* aContainer,
                                 nsIStreamListener **aDocListener,
                                 const char* aCommand)
{
    nsresult rv = nsDocument::StartDocumentLoad(aUrl, aContainer, aDocListener);
    if (NS_FAILED(rv))
        return rv;

    rv = nsRepository::CreateInstance(kParserCID, 
                                      NULL,
                                      kIParserIID, 
                                      (void**) &mParser);

    if (NS_FAILED(rv))
        return rv;

    rv = nsRepository::CreateInstance(kRDFSimpleDataBaseCID,
                                      NULL,
                                      kIRDFDataBaseIID,
                                      (void**) &mDB);

    if (NS_FAILED(rv))
        return rv;

    nsIWebShell* webShell;
    aContainer->QueryInterface(kIWebShellIID, (void**)&webShell);

    nsIRDFContentSink* sink;
    rv = NS_NewRDFDocumentContentSink(&sink, this, aUrl, webShell);

    NS_IF_RELEASE(webShell);

    if (NS_FAILED(rv))
        return rv;

    nsIRDFDataSource* ds;
    rv = nsRepository::CreateInstance(kRDFMemoryDataSourceCID,
                                      NULL,
                                      kIRDFDataSourceIID,
                                      (void**) &ds);
    if (NS_SUCCEEDED(rv)) {
        sink->SetDataSource(ds);
        mDB->AddDataSource(ds);

        // For the HTML content within a document
        nsIHTMLStyleSheet* mAttrStyleSheet;
        if (NS_SUCCEEDED(rv = NS_NewHTMLStyleSheet(&mAttrStyleSheet, aUrl, this))) {
            AddStyleSheet(mAttrStyleSheet); // tell the world about our new style sheet
        }
      
        // Set the parser as the stream listener for the document loader...
        static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
        rv = mParser->QueryInterface(kIStreamListenerIID, (void**)aDocListener);
        if (NS_SUCCEEDED(rv)) {
            nsIDTD* theDTD=0;

            // XXX For now, we'll use the "well formed" DTD
            NS_NewWellFormed_DTD(&theDTD);
            mParser->RegisterDTD(theDTD);
            mParser->SetCommand(aCommand);
            mParser->SetContentSink(sink);
            mParser->Parse(aUrl);
        }
    }

    NS_RELEASE(sink);
    return rv;
}

NS_IMETHODIMP 
nsRDFDocument::EndLoad()
{
  NS_IF_RELEASE(mParser);
  return nsDocument::EndLoad();
}


////////////////////////////////////////////////////////////////////////
// nsIXMLDocument interface

NS_IMETHODIMP
nsRDFDocument::RegisterNameSpace(nsIAtom* aPrefix, const nsString& aURI, 
                                 PRInt32& aNameSpaceId)
{
    if (! mNameSpaces) {
        mNameSpaces = new nsVoidArray();
        if (! mNameSpaces)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    NameSpaceStruct* ns = new NameSpaceStruct;
    if (! ns)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_IF_ADDREF(aPrefix);
    ns->mPrefix = aPrefix;
    ns->mURI    = aURI;

    aNameSpaceId = mNameSpaces->Count(); // just an index into the array
    mNameSpaces->AppendElement(static_cast<void*>(ns));
    return NS_OK;
}


NS_IMETHODIMP
nsRDFDocument::GetNameSpaceURI(PRInt32 aNameSpaceId, nsString& aURI)
{
    if (! mNameSpaces)
        return NS_ERROR_NULL_POINTER;

    if (aNameSpaceId < 0 || aNameSpaceId >= mNameSpaces->Count())
        return NS_ERROR_INVALID_ARG;

    NameSpaceStruct* ns =
        static_cast<NameSpaceStruct*>(mNameSpaces->ElementAt(aNameSpaceId));

    if (! ns)
        return NS_ERROR_NULL_POINTER;

    aURI = ns->mURI;
    return NS_OK;
}

NS_IMETHODIMP
nsRDFDocument::GetNameSpacePrefix(PRInt32 aNameSpaceId, nsIAtom*& aPrefix)
{
    if (! mNameSpaces)
        return NS_ERROR_NULL_POINTER;

    if (aNameSpaceId < 0 || aNameSpaceId >= mNameSpaces->Count())
        return NS_ERROR_INVALID_ARG;

    NameSpaceStruct* ns =
        static_cast<NameSpaceStruct*>(mNameSpaces->ElementAt(aNameSpaceId));

    if (! ns)
        return NS_ERROR_NULL_POINTER;

    aPrefix = ns->mPrefix;
    NS_IF_ADDREF(aPrefix);

    return NS_OK;
}


NS_IMETHODIMP
nsRDFDocument::PrologElementAt(PRInt32 aOffset, nsIContent** aContent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFDocument::PrologCount(PRInt32* aCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFDocument::AppendToProlog(nsIContent* aContent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFDocument::EpilogElementAt(PRInt32 aOffset, nsIContent** aContent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFDocument::EpilogCount(PRInt32* aCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFDocument::AppendToEpilog(nsIContent* aContent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}




////////////////////////////////////////////////////////////////////////
// nsIRDFDocument interface

NS_IMETHODIMP
nsRDFDocument::GetDataBase(nsIRDFDataBase*& result)
{
    if (! mDB)
        return NS_ERROR_NOT_INITIALIZED;

    result = mDB;
    result->AddRef();
    return NS_OK;
}
