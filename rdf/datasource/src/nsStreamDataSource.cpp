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

  A data source that can read itself from and write itself to an
  RDF/XML stream.

  TO DO
  -----

  1) Right now, the only kind of stream data sources that are writable
     are "file:" URIs. (In fact, <em>all</em> "file:" URIs are
     writable, modulo flie system permissions; this may lead to some
     surprising behavior.) Eventually, it'd be great if we could open
     an arbitrary nsIOutputStream on *any* URL, and Netlib could just
     do the magic.

 */

#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsIDTD.h"
#include "nsINameSpaceManager.h"
#include "nsIOutputStream.h"
#include "nsIParser.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFXMLSource.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsLayoutCID.h" // for NS_NAMESPACEMANAGER_CID.
#include "nsParserCIID.h"
#include "nsRDFCID.h"
#include "plstr.h"
#include "prio.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIDTDIID,               NS_IDTD_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,  NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIOutputStreamIID,      NS_IOUTPUTSTREAM_IID);
static NS_DEFINE_IID(kIParserIID,            NS_IPARSER_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,     NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFContentSinkIID,    NS_IRDFCONTENTSINK_IID);
static NS_DEFINE_IID(kIRDFXMLSourceIID,      NS_IRDFXMLSOURCE_IID);
static NS_DEFINE_IID(kIStreamListenerIID,    NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID,          NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kParserCID,                NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFSimpleContentSinkCID,  NS_RDFSIMPLECONTENTSINK_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,         NS_WELLFORMEDDTD_CID);

////////////////////////////////////////////////////////////////////////
// FileOutputStreamImpl

class FileOutputStreamImpl : public nsIOutputStream
{
private:
    nsOutputFileStream mStream;

public:
    FileOutputStreamImpl(const nsFilePath& path)
        : mStream(path) {}

    virtual ~FileOutputStreamImpl(void) {
        Close();
    }

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIBaseStream interface
    NS_IMETHOD Close(void) {
        mStream.close();
        return NS_OK;
    }

    // nsIOutputStream interface
    NS_IMETHOD Write(const char* aBuf,
                     PRUint32 aOffset,
                     PRUint32 aCount,
                     PRUint32 *aWriteCount)
    {
        PRInt32 written = mStream.write(aBuf + aOffset, aCount);
        if (written == -1) {
            *aWriteCount = 0;
            return NS_ERROR_FAILURE; // XXX right error code?
        }
        else {
            *aWriteCount = written;
            return NS_OK;
        }
    }
};

NS_IMPL_ISUPPORTS(FileOutputStreamImpl, kIOutputStreamIID);


////////////////////////////////////////////////////////////////////////
// StreamDataSourceImpl

class StreamDataSourceImpl : public nsIRDFDataSource,
                             public nsIRDFXMLSource
{
protected:
    nsIRDFDataSource* mInner;
    PRBool            mIsWritable;
    PRBool            mIsDirty;

public:
    StreamDataSourceImpl(void);
    virtual ~StreamDataSourceImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource
    NS_IMETHOD Init(const char* uri);

    NS_IMETHOD GetURI(const char* *uri) const {
        return mInner->GetURI(uri);
    }

    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source) {
        return mInner->GetSource(property, target, tv, source);
    }

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFAssertionCursor** sources) {
        return mInner->GetSources(property, target, tv, sources);
    }

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target) {
        return mInner->GetTarget(source, property, tv, target);
    }

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsIRDFAssertionCursor** targets) {
        return mInner->GetTargets(source, property, tv, targets);
    }

    NS_IMETHOD Assert(nsIRDFResource* source, 
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv);

    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target);

    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion) {
        return mInner->HasAssertion(source, property, target, tv, hasAssertion);
    }

    NS_IMETHOD AddObserver(nsIRDFObserver* n) {
        return mInner->AddObserver(n);
    }

    NS_IMETHOD RemoveObserver(nsIRDFObserver* n) {
        return mInner->RemoveObserver(n);
    }

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsIRDFArcsInCursor** labels) {
        return mInner->ArcLabelsIn(node, labels);
    }

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsIRDFArcsOutCursor** labels) {
        return mInner->ArcLabelsOut(source, labels);
    }

    NS_IMETHOD Flush(void);

    // nsIRDFXMLSource interface
    NS_IMETHOD Serialize(nsIOutputStream* aStream);
};


////////////////////////////////////////////////////////////////////////


StreamDataSourceImpl::StreamDataSourceImpl(void)
    : mIsWritable(PR_FALSE)
{
}


StreamDataSourceImpl::~StreamDataSourceImpl(void)
{
    Flush();
}


NS_IMPL_ADDREF(StreamDataSourceImpl);
NS_IMPL_RELEASE(StreamDataSourceImpl);

NS_IMETHODIMP
StreamDataSourceImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kISupportsIID) ||
        iid.Equals(kIRDFDataSourceIID)) {
        *result = NS_STATIC_CAST(nsIRDFDataSource*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(kIRDFXMLSourceIID)) {
        *result = NS_STATIC_CAST(nsIRDFXMLSource*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
}

const char* kFileURIPrefix = "file:";
const PRInt32 kFileURIPrefixLen = 5;

NS_IMETHODIMP
StreamDataSourceImpl::Init(const char* uri)
{
    nsresult rv;

    // XXX this is a hack: any "file:" URI is considered writable. All
    // others are considered read-only.
    if (PL_strncmp(uri, kFileURIPrefix, kFileURIPrefixLen) == 0)
        mIsWritable = PR_TRUE;

    nsINameSpaceManager* ns = nsnull;
    nsIRDFContentSink* sink = nsnull;
    nsIParser* parser       = nsnull;
    nsIDTD* dtd             = nsnull;
    nsIStreamListener* lsnr = nsnull;
    nsIURL* url             = nsnull;

    if (NS_FAILED(rv = NS_NewURL(&url, uri)))
        goto done;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFInMemoryDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &mInner)))
        goto done;

    if (NS_FAILED(rv = mInner->Init(uri)))
        goto done;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kNameSpaceManagerCID,
                                                    nsnull,
                                                    kINameSpaceManagerIID,
                                                    (void**) &ns)))
        goto done;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFSimpleContentSinkCID,
                                                    nsnull,
                                                    kIRDFContentSinkIID,
                                                    (void**) &sink)))
        goto done;

    if (NS_FAILED(sink->Init(url, ns)))
        goto done;

    // We set the content sink's data source directly to our in-memory
    // store. We _always_ fail asserts because they're not allowed.
    if (NS_FAILED(rv = sink->SetDataSource(mInner)))
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

    if (NS_FAILED(parser->Parse(url)))
        goto done;

    if (NS_FAILED(NS_OpenURL(url, lsnr)))
        goto done;

done:
    NS_IF_RELEASE(lsnr);
    NS_IF_RELEASE(dtd);
    NS_IF_RELEASE(parser);
    NS_IF_RELEASE(sink);
    NS_IF_RELEASE(url);
    return rv;
}


NS_IMETHODIMP
StreamDataSourceImpl::Assert(nsIRDFResource* source, 
                             nsIRDFResource* property, 
                             nsIRDFNode* target,
                             PRBool tv)
{
    if (! mIsWritable)
        return NS_ERROR_FAILURE; // XXX right error code?

    nsresult rv;
    if (NS_SUCCEEDED(rv = mInner->Assert(source, property, target, tv)))
        mIsDirty = PR_TRUE;

    return rv;
}


NS_IMETHODIMP
StreamDataSourceImpl::Unassert(nsIRDFResource* source, 
                               nsIRDFResource* property, 
                               nsIRDFNode* target)
{
    if (! mIsWritable)
        return NS_ERROR_FAILURE; // XXX right error code?

    nsresult rv;
    if (NS_SUCCEEDED(rv = mInner->Unassert(source, property, target)))
        mIsDirty = PR_TRUE;

    return rv;
}


NS_IMETHODIMP
StreamDataSourceImpl::Flush(void)
{
    if (!mIsWritable || !mIsDirty)
        return NS_OK;

    nsresult rv;

    const char* uri;
    if (NS_FAILED(rv = mInner->GetURI(&uri)))
        return rv;

    nsFileURL url(uri);
    nsFilePath path(url);

    FileOutputStreamImpl out(path);
    if (NS_FAILED(rv = Serialize(&out)))
        return rv;

    mIsDirty = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
StreamDataSourceImpl::Serialize(nsIOutputStream* stream)
{
    nsresult rv;
    nsIRDFXMLSource* source;
    if (NS_SUCCEEDED(rv = mInner->QueryInterface(kIRDFXMLSourceIID, (void**) &source))) {
        rv = source->Serialize(stream);
        NS_RELEASE(source);
    }
    return rv;
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

////////////////////////////////////////////////////////////////////////

