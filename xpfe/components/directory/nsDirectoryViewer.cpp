/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*

  A directory viewer object. Parses "application/http-index-format"
  per Lou Montulli's original spec:

    http://www.area.com/~roeber/file_format.html

*/

#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsNeckoUtil.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerContainer.h"
#include "nsIDocument.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentViewer.h"
#include "nsIEnumerator.h"
#include "nsIGenericFactory.h"
#include "nsIHTTPIndex.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsIXPConnect.h"
#include "nsEnumeratorUtils.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "rdf.h"


////////////////////////////////////////////////////////////////////////
// Common CIDs

// {82776710-5690-11d3-BE36-00104BDE6048}
#define NS_DIRECTORYVIEWERFACTORY_CID \
{ 0x82776710, 0x5690, 0x11d3, { 0xbe, 0x36, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }

static NS_DEFINE_CID(kComponentManagerCID,       NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kDirectoryViewerFactoryCID, NS_DIRECTORYVIEWERFACTORY_CID);
static NS_DEFINE_CID(kGenericFactoryCID,         NS_GENERICFACTORY_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);

#define HTTPINDEX_NAMESPACE_URI "urn:http-index-format:"

////////////////////////////////////////////////////////////////////////
// nsHTTPIndex

class nsHTTPIndex : public nsIHTTPIndex
{
protected:
  // We grab a reference to the content viewer container (which
  // indirectly owns us) so that we can insert ourselves as a global
  // in the script context _after_ the XUL doc has been embedded into
  // content viewer. We'll know that this has happened once we receive
  // an OnStartRequest() notification
  nsIContentViewerContainer* mContainer; // [WEAK]

  char* mBaseURL;

  nsCOMPtr<nsIRDFDataSource> mDataSource;

  nsHTTPIndex(nsIContentViewerContainer* aContainer);
  nsresult Init(nsIURI* aBaseURL);
  virtual ~nsHTTPIndex();

public:

  static nsresult Create(nsIURI* aBaseURI, nsIContentViewerContainer* aContainer, nsIHTTPIndex** aResult);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIHTTPIndex interface
  NS_IMETHOD GetBaseURL(char** _result);
  NS_IMETHOD GetDataSource(nsIRDFDataSource** _result);
  NS_IMETHOD CreateListener(nsIStreamListener** _result);
};


////////////////////////////////////////////////////////////////////////
// nsHTTPIndexParser
//

class nsHTTPIndexParser : public nsIStreamListener
{
protected:
  static nsrefcnt gRefCnt;
  static nsIRDFService* gRDF;
  static nsIRDFResource* kHTTPIndex_Comment;
  static nsIRDFResource* kHTTPIndex_Filename;

  struct Field {
    const char*     mName;
    nsIRDFResource* mResource;
  };

  static Field gFieldTable[];

  nsHTTPIndex* mHTTPIndex; // [WEAK]

  nsCOMPtr<nsIRDFDataSource> mDataSource;

  nsCOMPtr<nsIURI> mDirectoryURI;
  nsCOMPtr<nsIRDFResource> mDirectory;
  nsCOMPtr<nsIRDFContainer> mDirectoryContainer;

  nsCAutoString mLine;
  nsresult ProcessOneLine();
  nsresult ParseFormat(const char* aFormatStr);
  nsresult ParseData(const char* aDataStr);
  char URLUnescape(const char** aStr);

  nsAutoString mComment;

  nsVoidArray mFormat;

  nsHTTPIndexParser(nsHTTPIndex* aHTTPIndex, nsIContentViewerContainer* aContainer);
  nsresult Init();

  virtual ~nsHTTPIndexParser();

  // If this is set, then we need to bind the nsIHTTPIndex object to
  // the global object when we get an OnStartRequest() notification
  // (at this point, we'll know that the XUL document has been
  // embedded and the global object won't get clobbered.
  nsIContentViewerContainer* mContainer;

public:
  static nsresult Create(nsHTTPIndex* aHTTPIndex,
                         nsIContentViewerContainer* aContainer,
                         nsIStreamListener** aResult);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIStreamObserver interface
  NS_IMETHOD OnStartRequest(nsIChannel* aChannel, nsISupports* aContext);

  NS_IMETHOD OnStopRequest(nsIChannel* aChannel,
                           nsISupports* aContext,
                           nsresult aStatus,
                           const PRUnichar* aErrorMsg);

  // nsIStreamListener interface
  NS_IMETHOD OnDataAvailable(nsIChannel* aChannel,
                             nsISupports* aContext,
                             nsIInputStream* aStream,
                             PRUint32 aSourceOffset,
                             PRUint32 aCount);

};


nsrefcnt nsHTTPIndexParser::gRefCnt = 0;
nsIRDFService* nsHTTPIndexParser::gRDF;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Comment;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Filename;

nsHTTPIndexParser::Field
nsHTTPIndexParser::gFieldTable[] = {
  { "Filename",       nsnull },
  { "Content-Length", nsnull },
  { "Last-Modified",  nsnull },
  { "Content-Type",   nsnull },
  { "File-Type",      nsnull },
  { "Permissions",    nsnull },
  { nsnull,           nsnull },
};

nsHTTPIndexParser::nsHTTPIndexParser(nsHTTPIndex* aHTTPIndex, nsIContentViewerContainer* aContainer)
  : mHTTPIndex(aHTTPIndex),
    mContainer(aContainer)
{
  NS_INIT_REFCNT();
}


nsresult
nsHTTPIndexParser::Init()
{
  NS_PRECONDITION(mHTTPIndex != nsnull, "not initialized");
  if (! mHTTPIndex)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;
  rv = mHTTPIndex->GetDataSource(getter_AddRefs(mDataSource));
  if (NS_FAILED(rv)) return rv;

  if (gRefCnt++ == 0) {
    rv = nsServiceManager::GetService("component://netscape/rdf/rdf-service",
                                      nsCOMTypeInfo<nsIRDFService>::GetIID(),
                                      NS_REINTERPRET_CAST(nsISupports**, &gRDF));
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Comment",  &kHTTPIndex_Comment);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Filename", &kHTTPIndex_Filename);
    if (NS_FAILED(rv)) return rv;

    for (Field* field = gFieldTable; field->mName; ++field) {
      nsCAutoString str;
      str += HTTPINDEX_NAMESPACE_URI;
      str += field->mName;

      rv = gRDF->GetResource(str, &field->mResource);
      if (NS_FAILED(rv)) return rv;
    }
  }
  return NS_OK;
}

nsHTTPIndexParser::~nsHTTPIndexParser()
{
  if (--gRefCnt == 0) {
    NS_IF_RELEASE(kHTTPIndex_Comment);
    NS_IF_RELEASE(kHTTPIndex_Filename);

    for (Field* field = gFieldTable; field->mName; ++field) {
      NS_IF_RELEASE(field->mResource);
    }

    nsServiceManager::ReleaseService("component://netscape/rdf/rdf-service", gRDF);
  }
}


nsresult
nsHTTPIndexParser::Create(nsHTTPIndex* aHTTPIndex,
                          nsIContentViewerContainer* aContainer,
                          nsIStreamListener** aResult)
{
  NS_PRECONDITION(aHTTPIndex != nsnull, "null ptr");
  if (! aHTTPIndex)
    return NS_ERROR_NULL_POINTER;

  nsHTTPIndexParser* result = new nsHTTPIndexParser(aHTTPIndex, aContainer);
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = result->Init();
  if (NS_FAILED(rv)) {
    delete result;
    return rv;
  }

  NS_ADDREF(result);
  *aResult = result;
  return NS_OK;
}


NS_IMPL_ADDREF(nsHTTPIndexParser);
NS_IMPL_RELEASE(nsHTTPIndexParser);

NS_IMETHODIMP
nsHTTPIndexParser::QueryInterface(REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_OK;

  if (aIID.Equals(nsCOMTypeInfo<nsIStreamListener>::GetIID()) ||
      aIID.Equals(nsCOMTypeInfo<nsIStreamObserver>::GetIID()) ||
      aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aResult = NS_STATIC_CAST(nsIStreamListener*, this);
    NS_ADDREF(this);
    return NS_OK;
  }
  else {
    *aResult = nsnull;
    return NS_NOINTERFACE;
  }
}


NS_IMETHODIMP
nsHTTPIndexParser::OnStartRequest(nsIChannel* aChannel, nsISupports* aContext)
{
  nsresult rv;

  if (mContainer) {
    // Now get the content viewer container's script object.
    nsCOMPtr<nsIScriptContextOwner> contextowner;
    rv = mContainer->QueryCapability(nsCOMTypeInfo<nsIScriptContextOwner>::GetIID(),
                                     getter_AddRefs(contextowner));

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get script context owner from document");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIScriptContext> context;
    rv = contextowner->GetScriptContext(getter_AddRefs(context));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get script context");
    if (NS_FAILED(rv)) return rv;

    JSContext* jscontext = NS_REINTERPRET_CAST(JSContext*, context->GetNativeContext());

    // Using XPConnect, wrap the HTTP index object...
    static NS_DEFINE_CID(kXPConnectCID, NS_XPCONNECT_CID);
    NS_WITH_SERVICE(nsIXPConnect, xpc, kXPConnectCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
    rv = xpc->WrapNative(jscontext,                       
                         mHTTPIndex,
                         nsCOMTypeInfo<nsIHTTPIndex>::GetIID(),
                         getter_AddRefs(wrapper));

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to xpconnect-wrap http-index");
    if (NS_FAILED(rv)) return rv;

    JSObject* jsobj;
    rv = wrapper->GetJSObject(&jsobj);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get jsobj from xpconnect wrapper");
    if (NS_FAILED(rv)) return rv;

    jsval jslistener = OBJECT_TO_JSVAL(jsobj);

    // ...and stuff it into the global context
    PRBool ok;
    JSObject* global = JS_GetGlobalObject(jscontext);
    ok = JS_SetProperty(jscontext, global, "HTTPIndex", &jslistener);

    NS_ASSERTION(ok, "unable to set Listener property");
    if (! ok)
      return NS_ERROR_FAILURE;
  }

  // Save off some information about the stream we're about to parse.
  rv = aChannel->GetURI(getter_AddRefs(mDirectoryURI));
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString uristr;
  rv = mDirectoryURI->GetSpec(getter_Copies(uristr));
  if (NS_FAILED(rv)) return rv;

  rv = gRDF->GetResource(uristr, getter_AddRefs(mDirectory));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE(nsIRDFContainerUtils, rdfc, "component://netscape/rdf/container-utils", &rv);
  if (NS_FAILED(rv)) return rv;

  rv = rdfc->MakeSeq(mDataSource, mDirectory, getter_AddRefs(mDirectoryContainer));
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

NS_IMETHODIMP
nsHTTPIndexParser::OnStopRequest(nsIChannel* aChannel,
                                 nsISupports* aContext,
                                 nsresult aStatus,
                                 const PRUnichar* aErrorMsg)
{
  if (mLine.Length() > 0)
    ProcessOneLine();

  nsresult rv;

  nsCOMPtr<nsIRDFLiteral> comment;
  rv = gRDF->GetLiteral(mComment.GetUnicode(), getter_AddRefs(comment));
  if (NS_FAILED(rv)) return rv;

  rv = mDataSource->Assert(mDirectory, kHTTPIndex_Comment, comment, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}


NS_IMETHODIMP
nsHTTPIndexParser::OnDataAvailable(nsIChannel* aChannel,
                                   nsISupports* aContext,
                                   nsIInputStream* aStream,
                                   PRUint32 aSourceOffset,
                                   PRUint32 aCount)
{
  nsresult rv;

  while (aCount > 0) {
    char buf[128];
    PRUint32 count = (aCount > sizeof(buf)) ? sizeof(buf) : aCount;
    PRUint32 nread;

    rv = aStream->Read(buf, count, &nread);
    if (NS_FAILED(rv)) return rv;

    char* p = buf;
    while (nread != 0) {
      mLine.Append(*p);

      if (*p == '\n') {
        rv = ProcessOneLine();
        NS_ASSERTION(NS_SUCCEEDED(rv), "error processing http-index line");
        if (NS_FAILED(rv)) return rv;

        mLine.Truncate();
      }

      ++p;
      --nread;
    }
  }

  return NS_OK;
}


nsresult
nsHTTPIndexParser::ProcessOneLine()
{
  if (mLine.Length() >= 4) {
    nsresult rv;
    const char* buf = mLine;

    if (buf[0] == '1') {
      if (buf[1] == '0') {
        if (buf[2] == '0' && buf[3] == ':') {
          // 100. Human-readable comment line. Ignore
        }
        else if (buf[2] == '1' && buf[3] == ':') {
          // 101. Human-readable information line.
          mComment += (buf + 4);
        }
        else if (buf[2] == '2' && buf[3] == ':') {
          // 102. Human-readable information line, HTML.
          mComment += (buf + 4);
        }
      }
    }
    else if (buf[0] == '2') {
      if (buf[1] == '0') {
        if (buf[2] == '0' && buf[3] == ':') {
          // 200. Define field names
          rv = ParseFormat(buf + 4);
          if (NS_FAILED(rv)) return rv;
        }
        else if (buf[2] == '1' && buf[3] == ':') {
          // 201. Field data
          rv = ParseData(buf + 4);
          if (NS_FAILED(rv)) return rv;
        }
      }
    }
    else if (buf[0] == '3') {
      if (buf[1] == '0') {
        if (buf[2] == '0' && buf[3] == ':') {
          // 300. Self-referring URL
        }
      }
    }
  }

  return NS_OK;
}


nsresult
nsHTTPIndexParser::ParseFormat(const char* aFormatStr)
{
  // Parse a "200" format line, and remember the fields and their
  // ordering in mFormat. Multiple 200 lines stomp on each other.
  mFormat.Clear();

  do {
    while (*aFormatStr && nsString::IsSpace(PRUnichar(*aFormatStr)))
      ++aFormatStr;

    if (! aFormatStr)
      break;

    nsAutoString name;
    while (*aFormatStr && !nsString::IsSpace(PRUnichar(*aFormatStr))) {
      name.Append(URLUnescape(&aFormatStr));
    }

    nsIRDFResource* resource = nsnull;
    for (Field* field = gFieldTable; field->mName; ++field) {
      if (name.EqualsIgnoreCase(field->mName)) {
        resource = field->mResource;
        break;
      }
    }

    mFormat.AppendElement(resource);
  } while (*aFormatStr);

  return NS_OK;
}


#define MAX_AUTO_VALUES 8

nsresult
nsHTTPIndexParser::ParseData(const char* aDataStr)
{
  // Parse a "201" data line, using the field ordering specified in
  // mFormat.

  if (mFormat.Count() == 0) {
    // Ignore if we haven't seen a format yet.
    return NS_OK;
  }

  // First, we'll iterate through the values and remember each (using
  // an array of autostrings allocated on the stack, if possible). We
  // have to do this, because we don't know up-front the filename for
  // which this 201 refers.
  nsAutoString autovalues[MAX_AUTO_VALUES];
  nsAutoString* values = autovalues;

  if (mFormat.Count() > MAX_AUTO_VALUES) {
    values = new nsAutoString[mFormat.Count()];
    if (! values)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> entry;

  for (PRInt32 field = 0; field < mFormat.Count(); ++field) {
    // If we've exhausted the data before we run out of fields, just
    // bail.
    if (! *aDataStr)
      break;

    while (*aDataStr && nsString::IsSpace(PRUnichar(*aDataStr)))
      ++aDataStr;

    nsAutoString value;

    if (*aDataStr == '"' || *aDataStr == '\'') {
      // it's a quoted string. snarf everything up to the next quote character
      const char quotechar = *(aDataStr++);
      while (*aDataStr && *aDataStr != quotechar)
        value.Append(URLUnescape(&aDataStr));

      if (! aDataStr) {
        NS_WARNING("quoted value not terminated");
      }
    }
    else {
      // it's unquoted. snarf until we see whitespace.
      while (*aDataStr && !nsString::IsSpace(*aDataStr))
        value.Append(URLUnescape(&aDataStr));
    }

    values[field] = value;

    if (mFormat.ElementAt(field) == kHTTPIndex_Filename) {
      // we found the filename; construct a resource for its entry
      nsXPIDLCString base;
      rv = mDirectory->GetValue(getter_Copies(base));
      if (NS_FAILED(rv)) break;

      nsAutoString entryuri;
      rv = NS_MakeAbsoluteURI(value, mDirectoryURI, entryuri);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable make absolute URI");
      if (NS_FAILED(rv)) break;

      rv = gRDF->GetResource(nsCAutoString(entryuri), getter_AddRefs(entry));
    }
  }

  // At this point, we'll (hopefully) have found the filename and
  // constructed a resource for it, stored in entry. So now take a
  // second pass through the values and add as statements to the RDF
  // datasource.
  if (entry && NS_SUCCEEDED(rv)) {
    for (PRInt32 indx = 0; indx < mFormat.Count(); ++indx) {
      nsIRDFResource* property = NS_REINTERPRET_CAST(nsIRDFResource*, mFormat.ElementAt(indx));
      if (! property)
        continue;

      nsCOMPtr<nsIRDFLiteral> value;
      rv = gRDF->GetLiteral(values[indx].GetUnicode(), getter_AddRefs(value));
      if (NS_FAILED(rv)) break;

      rv = mDataSource->Assert(entry, property, value, PR_TRUE);
      if (NS_FAILED(rv)) break;
    }

    rv = mDirectoryContainer->AppendElement(entry);
  }

  // If we needed to spill values onto the heap, make sure we clean up
  // here.
  if (values != autovalues)
    delete[] values;

  return rv;
}



char
nsHTTPIndexParser::URLUnescape(const char** aStr)
{
  // This funky little function will URL unescape (per RFC1738) the
  // contents pointed to at aStr, and then move aStr forward an
  // appropriate number of characters to deal with the unescaping.
  //
  // For example,
  //
  //   char* p = "%20foo";
  //   char c = URLUnescape(p);
  //   /* now c = ' ' and p points to "foo" */
  //
  static const char kHex[] = "0123456789abcdef";

  char c = **aStr;
  if (c == '%') {
    const char* p = *aStr;
    char c2 = 0;

    for (PRInt32 i = 0; i < 2; ++i) {
      char digit;

      digit = *(++p);

      // Illegal escape at the end of the string; e.g., "%". Bail.
      if (! digit)
        goto done;

      if (digit >= 'A' && digit <= 'F')
        digit = digit - 'A' + 'a';

      char* offset = PL_strchr(kHex, digit);

      // Character isn't in [0-9a-f]. Bail.
      if (! offset)
        goto done;

      c2 *= 16;
      c2 += char(offset - kHex);
    }

    (*aStr) += 3;
    return c2;
  }


done:
  (*aStr)++;
  return c;
}


////////////////////////////////////////////////////////////////////////
// nsHTTPIndex implementation

nsHTTPIndex::nsHTTPIndex(nsIContentViewerContainer* aContainer)
  : mContainer(aContainer)
{
  NS_INIT_REFCNT();
}


nsresult
nsHTTPIndex::Init(nsIURI* aBaseURL)
{
  NS_PRECONDITION(aBaseURL != nsnull, "null ptr");
  if (! aBaseURL)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;

  rv = aBaseURL->GetSpec(&mBaseURL);
  if (NS_FAILED(rv)) return rv;

  static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
  rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID, this,
                                          nsCOMTypeInfo<nsIRDFDataSource>::GetIID(),
                                          getter_AddRefs(mDataSource));
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}


nsresult
nsHTTPIndex::Create(nsIURI* aBaseURL, nsIContentViewerContainer* aContainer, nsIHTTPIndex** aResult)
{
  nsHTTPIndex* result = new nsHTTPIndex(aContainer);
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = result->Init(aBaseURL);
  if (NS_FAILED(rv)) {
    delete result;
    return rv;
  }

  NS_ADDREF(result);
  *aResult = result;
  return NS_OK;
}

nsHTTPIndex::~nsHTTPIndex()
{
  if (mBaseURL)
    nsAllocator::Free(mBaseURL);
}

NS_IMPL_ADDREF(nsHTTPIndex);

NS_IMETHODIMP_(nsrefcnt)
nsHTTPIndex::Release()
{
  --mRefCnt;
  if (mRefCnt == 1 && mDataSource) {
    mDataSource = nsnull; /* nsCOMPtr triggers Release() */
  }
  else if (mRefCnt == 0) {
    delete this;
  }
  return mRefCnt;
}

NS_IMETHODIMP
nsHTTPIndex::QueryInterface(REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_OK;

  if (aIID.Equals(nsCOMTypeInfo<nsIHTTPIndex>::GetIID()) ||
      aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aResult = NS_STATIC_CAST(nsIHTTPIndex*, this);
    NS_ADDREF(this);
    return NS_OK;
  }
  else {
    *aResult = nsnull;
    return NS_NOINTERFACE;
  }
}

NS_IMETHODIMP
nsHTTPIndex::GetBaseURL(char** _result)
{
  *_result = nsXPIDLCString::Copy(mBaseURL);
  if (! *_result)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}


NS_IMETHODIMP
nsHTTPIndex::GetDataSource(nsIRDFDataSource** _result)
{
  *_result = mDataSource;
  NS_IF_ADDREF(*_result);
  return NS_OK;
}

NS_IMETHODIMP
nsHTTPIndex::CreateListener(nsIStreamListener** _result)
{
  nsresult rv;
  rv = nsHTTPIndexParser::Create(this, mContainer, _result);
  if (NS_FAILED(rv)) return rv;

  // Clear this: we only need to pass it through once.
  mContainer = nsnull;

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsDirectoryViewerFactory
//

class nsDirectoryViewerFactory : public nsIDocumentLoaderFactory
{
protected:
  nsDirectoryViewerFactory();
  virtual ~nsDirectoryViewerFactory();

public:
  // constructor
  static NS_IMETHODIMP
  Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIDocumentLoaderFactory interface
  NS_IMETHOD CreateInstance(const char *aCommand,
                            nsIChannel* aChannel,
                            nsILoadGroup* aLoadGroup,
                            const char* aContentType, 
                            nsIContentViewerContainer* aContainer,
                            nsISupports* aExtraInfo,
                            nsIStreamListener** aDocListenerResult,
                            nsIContentViewer** aDocViewerResult);

  NS_IMETHOD CreateInstanceForDocument(nsIContentViewerContainer* aContainer,
                                       nsIDocument* aDocument,
                                       const char *aCommand,
                                       nsIContentViewer** aDocViewerResult);
};

#ifdef CONSTRUCT_XUL_DOCUMENT_SYNCHRONOUSLY
NS_IMPL_ISUPPORTS(nsDirectoryViewerFactory::ProxyStream, nsCOMTypeInfo<nsIInputStream>::GetIID());
#endif


nsDirectoryViewerFactory::nsDirectoryViewerFactory()
{
  NS_INIT_REFCNT();
}

nsDirectoryViewerFactory::~nsDirectoryViewerFactory()
{
}


NS_IMETHODIMP
nsDirectoryViewerFactory::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aOuter == nsnull, "no aggregation");
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsDirectoryViewerFactory* result
    = new nsDirectoryViewerFactory();

  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result); // stabilize
  nsresult rv = result->QueryInterface(aIID, aResult);
  NS_RELEASE(result);

  return rv;
}


NS_IMPL_ADDREF(nsDirectoryViewerFactory);
NS_IMPL_RELEASE(nsDirectoryViewerFactory);

NS_IMETHODIMP
nsDirectoryViewerFactory::QueryInterface(REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(nsCOMTypeInfo<nsIDocumentLoaderFactory>::GetIID()) ||
      aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aResult = NS_STATIC_CAST(nsIDocumentLoaderFactory*, this);
  }
  else {
    *aResult = nsnull;
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF(this);
  return NS_OK;
}

NS_IMETHODIMP
nsDirectoryViewerFactory::CreateInstance(const char *aCommand,
                                         nsIChannel* aChannel,
                                         nsILoadGroup* aLoadGroup,
                                         const char* aContentType, 
                                         nsIContentViewerContainer* aContainer,
                                         nsISupports* aExtraInfo,
                                         nsIStreamListener** aDocListenerResult,
                                         nsIContentViewer** aDocViewerResult)
{
  nsresult rv;

  // Create a dummy loader that will load a stub XUL document.
  nsCOMPtr<nsIDocumentLoaderFactory> factory;
  rv = nsComponentManager::CreateInstance(NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "view/text/xul",
                                          nsnull,
                                          nsCOMTypeInfo<nsIDocumentLoaderFactory>::GetIID(),
                                          getter_AddRefs(factory));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), "chrome://directory/content/");
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIChannel> channel;
  rv = NS_OpenURI(getter_AddRefs(channel), uri);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStreamListener> listener;
  rv = factory->CreateInstance("view", channel, aLoadGroup, "text/xul",
                               aContainer, aExtraInfo, getter_AddRefs(listener),
                               aDocViewerResult);
  if (NS_FAILED(rv)) return rv;

  rv = channel->AsyncRead(0, -1, nsnull, listener);
  if (NS_FAILED(rv)) return rv;

  // Create an HTTPIndex object so that we can stuff it into the script context
  nsCOMPtr<nsIURI> baseuri;
  rv = aChannel->GetURI(getter_AddRefs(baseuri));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIHTTPIndex> httpindex;
  rv = nsHTTPIndex::Create(baseuri, aContainer, getter_AddRefs(httpindex));
  if (NS_FAILED(rv)) return rv;

  // Now shanghai the stream into our http-index parsing datasource
  // wrapper beastie.
  rv = httpindex->CreateListener(aDocListenerResult);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

NS_IMETHODIMP
nsDirectoryViewerFactory::CreateInstanceForDocument(nsIContentViewerContainer* aContainer,
                                                    nsIDocument* aDocument,
                                                    const char *aCommand,
                                                    nsIContentViewer** aDocViewerResult)
{
  NS_NOTYETIMPLEMENTED("didn't expect to get here");
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// Component Exports

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServiceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  NS_PRECONDITION(aFactory != nsnull, "null ptr");
  if (! aFactory)
    return NS_ERROR_NULL_POINTER;

  nsIGenericFactory::ConstructorProcPtr constructor;

  if (aClass.Equals(kDirectoryViewerFactoryCID)) {
    constructor = nsDirectoryViewerFactory::Create;
  }
  else {
    *aFactory = nsnull;
    return NS_NOINTERFACE; // XXX
  }

  nsresult rv;
  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServiceMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIGenericFactory> factory;
  rv = compMgr->CreateInstance(kGenericFactoryCID,
                               nsnull,
                               nsIGenericFactory::GetIID(),
                               getter_AddRefs(factory));

  if (NS_FAILED(rv)) return rv;

  rv = factory->SetConstructor(constructor);
  if (NS_FAILED(rv)) return rv;

  *aFactory = factory;
  NS_ADDREF(*aFactory);
  return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kDirectoryViewerFactoryCID, "Global History",
                                  NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "view/application/http-index-format",
                                  aPath, PR_TRUE, PR_TRUE);

  return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kDirectoryViewerFactoryCID, aPath);

  return NS_OK;
}


