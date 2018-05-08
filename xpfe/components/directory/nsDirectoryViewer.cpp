/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A directory viewer object. Parses "application/http-index-format"
  per Lou Montulli's original spec:

  http://www.mozilla.org/projects/netlib/dirindexformat.html

  One added change is for a description entry, for when the
  target does not match the filename

*/

#include "nsDirectoryViewer.h"
#include "nsArray.h"
#include "nsArrayUtils.h"
#include "nsIDirIndex.h"
#include "nsIDocShell.h"
#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsEnumeratorUtils.h"
#include "nsEscape.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsIXPConnect.h"
#include "nsEnumeratorUtils.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsITextToSubURI.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIFTPChannel.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIProgressEventSink.h"
#include "nsIDOMWindow.h"
#include "nsIStreamConverterService.h"
#include "nsICategoryManager.h"
#include "nsXPCOMCID.h"
#include "nsIDocument.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

using namespace mozilla;

static const int FORMAT_XUL = 3;

//----------------------------------------------------------------------
//
// Common CIDs
//

static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);

// Various protocols we have to special case
static const char               kFTPProtocol[] = "ftp://";

//----------------------------------------------------------------------
//
// nsHTTPIndex
//

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsHTTPIndex)
    NS_INTERFACE_MAP_ENTRY(nsIHTTPIndex)
    NS_INTERFACE_MAP_ENTRY(nsIRDFDataSource)
    NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIDirIndexListener)
    NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsIFTPEventSink)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIHTTPIndex)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(nsHTTPIndex, mInner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsHTTPIndex)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsHTTPIndex)

NS_IMETHODIMP
nsHTTPIndex::GetInterface(const nsIID &anIID, void **aResult )
{
    if (anIID.Equals(NS_GET_IID(nsIFTPEventSink))) {
        // If we don't have a container to store the logged data
        // then don't report ourselves back to the caller

        if (!mRequestor)
          return NS_ERROR_NO_INTERFACE;
        *aResult = static_cast<nsIFTPEventSink*>(this);
        NS_ADDREF(this);
        return NS_OK;
    }

    if (anIID.Equals(NS_GET_IID(nsIPrompt))) {

        if (!mRequestor)
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsPIDOMWindowOuter> aDOMWindow = do_GetInterface(mRequestor);
        if (!aDOMWindow)
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));

        return wwatch->GetNewPrompter(aDOMWindow, (nsIPrompt**)aResult);
    }

    if (anIID.Equals(NS_GET_IID(nsIAuthPrompt))) {

        if (!mRequestor)
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsPIDOMWindowOuter> aDOMWindow = do_GetInterface(mRequestor);
        if (!aDOMWindow)
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));

        return wwatch->GetNewAuthPrompter(aDOMWindow, (nsIAuthPrompt**)aResult);
    }

    if (anIID.Equals(NS_GET_IID(nsIProgressEventSink))) {

        if (!mRequestor)
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIProgressEventSink> sink = do_GetInterface(mRequestor);
        if (!sink)
            return NS_ERROR_NO_INTERFACE;

        *aResult = sink;
        NS_ADDREF((nsISupports*)*aResult);
        return NS_OK;
    }

    return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsHTTPIndex::OnFTPControlLog(bool server, const char *msg)
{
    NS_ENSURE_TRUE(mRequestor, NS_OK);

    nsCOMPtr<nsIGlobalObject> globalObject = do_GetInterface(mRequestor);
    NS_ENSURE_TRUE(globalObject, NS_OK);

    // We're going to run script via JS_CallFunctionName, so we need an
    // AutoEntryScript. This is Gecko specific and not in any spec.
    dom::AutoEntryScript aes(globalObject,
                             "nsHTTPIndex OnFTPControlLog");
    JSContext* cx = aes.cx();

    JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
    NS_ENSURE_TRUE(global, NS_OK);

    nsString unicodeMsg;
    CopyASCIItoUTF16(msg, unicodeMsg);
    JSString* jsMsgStr = JS_NewUCStringCopyZ(cx, unicodeMsg.get());
    NS_ENSURE_TRUE(jsMsgStr, NS_ERROR_OUT_OF_MEMORY);

    JS::AutoValueArray<2> params(cx);
    params[0].setBoolean(server);
    params[1].setString(jsMsgStr);

    JS::Rooted<JS::Value> val(cx);
    JS_CallFunctionName(cx,
                        global,
                        "OnFTPControlLog",
                        params,
                        &val);
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPIndex::SetEncoding(const char *encoding)
{
    mEncoding = encoding;
    return(NS_OK);
}

NS_IMETHODIMP
nsHTTPIndex::GetEncoding(char **encoding)
{
  NS_PRECONDITION(encoding, "null ptr");
  if (! encoding)
    return(NS_ERROR_NULL_POINTER);

  *encoding = ToNewCString(mEncoding);
  if (!*encoding)
    return(NS_ERROR_OUT_OF_MEMORY);

  return(NS_OK);
}

NS_IMETHODIMP
nsHTTPIndex::OnStartRequest(nsIRequest *request, nsISupports* aContext)
{
  nsresult rv;

  mParser = do_CreateInstance(NS_DIRINDEXPARSER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = mParser->SetEncoding(mEncoding.get());
  if (NS_FAILED(rv)) return rv;

  rv = mParser->SetListener(this);
  if (NS_FAILED(rv)) return rv;

  rv = mParser->OnStartRequest(request,aContext);
  if (NS_FAILED(rv)) return rv;

  // This should only run once...
  // Unless we don't have a container to start with
  // (ie called from bookmarks as an rdf datasource)
  if (mBindToGlobalObject && mRequestor) {
    mBindToGlobalObject = false;

    nsCOMPtr<nsIGlobalObject> globalObject = do_GetInterface(mRequestor);
    NS_ENSURE_TRUE(globalObject, NS_ERROR_FAILURE);

    // We might run script via JS_SetProperty, so we need an AutoEntryScript.
    // This is Gecko specific and not in any spec.
    dom::AutoEntryScript aes(globalObject,
                             "nsHTTPIndex set HTTPIndex property");
    JSContext* cx = aes.cx();

    JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));

    // Using XPConnect, wrap the HTTP index object...
    static NS_DEFINE_CID(kXPConnectCID, NS_XPCONNECT_CID);
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(kXPConnectCID, &rv));
    if (NS_FAILED(rv)) return rv;

    JS::Rooted<JSObject*> jsobj(cx);
    rv = xpc->WrapNative(cx,
                         global,
                         static_cast<nsIHTTPIndex*>(this),
                         NS_GET_IID(nsIHTTPIndex),
                         jsobj.address());

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to xpconnect-wrap http-index");
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(jsobj,
                 "unable to get jsobj from xpconnect wrapper");
    if (!jsobj) return NS_ERROR_UNEXPECTED;

    JS::Rooted<JS::Value> jslistener(cx, JS::ObjectValue(*jsobj));

    // ...and stuff it into the global context
    bool ok = JS_SetProperty(cx, global, "HTTPIndex", jslistener);
    NS_ASSERTION(ok, "unable to set Listener property");
    if (!ok)
      return NS_ERROR_FAILURE;
  }

  if (!aContext) {
    nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));
    NS_ASSERTION(channel, "request should be a channel");

    // lets hijack the notifications:
    channel->SetNotificationCallbacks(this);

    // now create the top most resource
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));

    nsAutoCString entryuriC;
    rv = uri->GetSpec(entryuriC);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> entry;
    rv = mDirRDF->GetResource(entryuriC, getter_AddRefs(entry));

    NS_ConvertUTF8toUTF16 uriUnicode(entryuriC);

    nsCOMPtr<nsIRDFLiteral> URLVal;
    rv = mDirRDF->GetLiteral(uriUnicode.get(), getter_AddRefs(URLVal));

    Assert(entry, kNC_URL, URLVal, true);
    mDirectory = do_QueryInterface(entry);
  }
  else
  {
    // Get the directory from the context
    mDirectory = do_QueryInterface(aContext);
  }

  if (!mDirectory) {
      request->Cancel(NS_BINDING_ABORTED);
      return NS_BINDING_ABORTED;
  }

  // Mark the directory as "loading"
  rv = Assert(mDirectory, kNC_Loading,
                           kTrueLiteral, true);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}


NS_IMETHODIMP
nsHTTPIndex::OnStopRequest(nsIRequest *request,
                           nsISupports* aContext,
                           nsresult aStatus)
{
  // If mDirectory isn't set, then we should just bail. Either an
  // error occurred and OnStartRequest() never got called, or
  // something exploded in OnStartRequest().
  if (! mDirectory)
    return NS_BINDING_ABORTED;

  mParser->OnStopRequest(request,aContext,aStatus);

  nsresult rv;

  nsCString commentStr;
  mParser->GetComment(getter_Copies(commentStr));

  nsCOMPtr<nsIRDFLiteral> comment;
  rv = mDirRDF->GetLiteral(NS_ConvertASCIItoUTF16(commentStr).get(), getter_AddRefs(comment));
  if (NS_FAILED(rv)) return rv;

  rv = Assert(mDirectory, kNC_Comment, comment, true);
  if (NS_FAILED(rv)) return rv;

  // hack: Remove the 'loading' annotation (ignore errors)
  AddElement(mDirectory, kNC_Loading, kTrueLiteral);

  return NS_OK;
}


NS_IMETHODIMP
nsHTTPIndex::OnDataAvailable(nsIRequest *request,
                             nsISupports* aContext,
                             nsIInputStream* aStream,
                             uint64_t aSourceOffset,
                             uint32_t aCount)
{
  // If mDirectory isn't set, then we should just bail. Either an
  // error occurred and OnStartRequest() never got called, or
  // something exploded in OnStartRequest().
  if (! mDirectory)
    return NS_BINDING_ABORTED;

  return mParser->OnDataAvailable(request, mDirectory, aStream, aSourceOffset, aCount);
}


nsresult
nsHTTPIndex::OnIndexAvailable(nsIRequest* aRequest, nsISupports *aContext,
                              nsIDirIndex* aIndex)
{
  nsCOMPtr<nsIRDFResource>	parentRes = do_QueryInterface(aContext);
  if (!parentRes) {
    NS_ERROR("Could not obtain parent resource");
    return(NS_ERROR_UNEXPECTED);
  }

  const char* baseStr;
  parentRes->GetValueConst(&baseStr);
  if (! baseStr) {
    NS_ERROR("Could not reconstruct base uri");
    return NS_ERROR_UNEXPECTED;
  }

  // we found the filename; construct a resource for its entry
  nsAutoCString entryuriC(baseStr);

  nsCString filename;
  nsresult rv = aIndex->GetLocation(getter_Copies(filename));
  if (NS_FAILED(rv)) return rv;
  entryuriC.Append(filename);

  // if its a directory, make sure it ends with a trailing slash.
  uint32_t type;
  rv = aIndex->GetType(&type);
  if (NS_FAILED(rv))
    return rv;

  bool isDirType = (type == nsIDirIndex::TYPE_DIRECTORY);
  if (isDirType && entryuriC.Last() != '/') {
      entryuriC.Append('/');
  }

  nsCOMPtr<nsIRDFResource> entry;
  rv = mDirRDF->GetResource(entryuriC, getter_AddRefs(entry));

  // At this point, we'll (hopefully) have found the filename and
  // constructed a resource for it, stored in entry. So now take a
  // second pass through the values and add as statements to the RDF
  // datasource.

  if (entry && NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIRDFLiteral> lit;
    nsString str;

    CopyASCIItoUTF16(entryuriC, str);

    rv = mDirRDF->GetLiteral(str.get(), getter_AddRefs(lit));

    if (NS_SUCCEEDED(rv)) {
      rv = Assert(entry, kNC_URL, lit, true);
      if (NS_FAILED(rv)) return rv;

      nsString xpstr;

      // description
      rv = aIndex->GetDescription(getter_Copies(xpstr));
      if (NS_FAILED(rv)) return rv;
      if (xpstr.Last() == '/')
        xpstr.Truncate(xpstr.Length() - 1);

      rv = mDirRDF->GetLiteral(xpstr.get(), getter_AddRefs(lit));
      if (NS_FAILED(rv)) return rv;
      rv = Assert(entry, kNC_Description, lit, true);
      if (NS_FAILED(rv)) return rv;

      // contentlength
      int64_t size;
      rv = aIndex->GetSize(&size);
      if (NS_FAILED(rv)) return rv;
      int64_t minus1 = UINT64_MAX;
      if (size != minus1) {
        int32_t intSize = int32_t(size);
        // XXX RDF should support 64 bit integers (bug 240160)
        nsCOMPtr<nsIRDFInt> val;
        rv = mDirRDF->GetIntLiteral(intSize, getter_AddRefs(val));
        if (NS_FAILED(rv)) return rv;
        rv = Assert(entry, kNC_ContentLength, val, true);
        if (NS_FAILED(rv)) return rv;
      }

      // lastmodified
      PRTime tm;
      rv = aIndex->GetLastModified(&tm);
      if (NS_FAILED(rv)) return rv;
      if (tm != -1) {
        nsCOMPtr<nsIRDFDate> val;
        rv = mDirRDF->GetDateLiteral(tm, getter_AddRefs(val));
        if (NS_FAILED(rv)) return rv;
        rv = Assert(entry, kNC_LastModified, val, true);
      }

      // filetype
      uint32_t type;
      rv = aIndex->GetType(&type);
      switch (type) {
      case nsIDirIndex::TYPE_UNKNOWN:
        rv = mDirRDF->GetLiteral(u"UNKNOWN", getter_AddRefs(lit));
        break;
      case nsIDirIndex::TYPE_DIRECTORY:
        rv = mDirRDF->GetLiteral(u"DIRECTORY", getter_AddRefs(lit));
        break;
      case nsIDirIndex::TYPE_FILE:
        rv = mDirRDF->GetLiteral(u"FILE", getter_AddRefs(lit));
        break;
      case nsIDirIndex::TYPE_SYMLINK:
        rv = mDirRDF->GetLiteral(u"SYMLINK", getter_AddRefs(lit));
        break;
      }

      if (NS_FAILED(rv)) return rv;
      rv = Assert(entry, kNC_FileType, lit, true);
      if (NS_FAILED(rv)) return rv;
    }

    // Since the definition of a directory depends on the protocol, we would have
    // to do string comparisons all the time.
    // But we're told if we're a container right here - so save that fact
    if (isDirType)
      Assert(entry, kNC_IsContainer, kTrueLiteral, true);
    else
      Assert(entry, kNC_IsContainer, kFalseLiteral, true);

//   instead of
//       rv = Assert(parentRes, kNC_Child, entry, true);
//       if (NS_FAILED(rv)) return rv;
//   defer insertion onto a timer so that the UI isn't starved
    AddElement(parentRes, kNC_Child, entry);
  }

  return rv;
}

nsresult
nsHTTPIndex::OnInformationAvailable(nsIRequest *aRequest,
                                  nsISupports *aCtxt,
                                  const nsAString& aInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
//
// nsHTTPIndex implementation
//

nsHTTPIndex::nsHTTPIndex()
  : mBindToGlobalObject(true),
    mRequestor(nullptr)
{
}


nsHTTPIndex::nsHTTPIndex(nsIInterfaceRequestor* aRequestor)
  : mBindToGlobalObject(true),
    mRequestor(aRequestor)
{
}


nsHTTPIndex::~nsHTTPIndex()
{
  // note: these are NOT statics due to the native of nsHTTPIndex
  // where it may or may not be treated as a singleton

    if (mTimer)
    {
        // be sure to cancel the timer, as it holds a
        // weak reference back to nsHTTPIndex
        mTimer->Cancel();
        mTimer = nullptr;
    }

    mConnectionList = nullptr;
    mNodeList = nullptr;

    if (mDirRDF)
      {
        // UnregisterDataSource() may fail; just ignore errors
        mDirRDF->UnregisterDataSource(this);
      }
}



nsresult
nsHTTPIndex::CommonInit()
{
    nsresult	rv = NS_OK;

    // set initial/default encoding to windows-1252 (not UTF-8)
    mEncoding = "windows-1252";

    mDirRDF = do_GetService(kRDFServiceCID, &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
    if (NS_FAILED(rv)) {
      return(rv);
    }

    mInner = do_CreateInstance("@mozilla.org/rdf/datasource;1?name=in-memory-datasource", &rv);

    if (NS_FAILED(rv))
      return rv;

    mDirRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "child"),
                         getter_AddRefs(kNC_Child));
    mDirRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "loading"),
                         getter_AddRefs(kNC_Loading));
    mDirRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Comment"),
                         getter_AddRefs(kNC_Comment));
    mDirRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "URL"),
                         getter_AddRefs(kNC_URL));
    mDirRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Name"),
                         getter_AddRefs(kNC_Description));
    mDirRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Content-Length"),
                         getter_AddRefs(kNC_ContentLength));
    mDirRDF->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastModifiedDate"),
                         getter_AddRefs(kNC_LastModified));
    mDirRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Content-Type"),
                         getter_AddRefs(kNC_ContentType));
    mDirRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "File-Type"),
                         getter_AddRefs(kNC_FileType));
    mDirRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "IsContainer"),
                         getter_AddRefs(kNC_IsContainer));

    rv = mDirRDF->GetLiteral(u"true", getter_AddRefs(kTrueLiteral));
    if (NS_FAILED(rv)) return(rv);
    rv = mDirRDF->GetLiteral(u"false", getter_AddRefs(kFalseLiteral));
    if (NS_FAILED(rv)) return(rv);

    mConnectionList = nsArray::Create();

    // note: don't register DS here
    return rv;
}


nsresult
nsHTTPIndex::Init()
{
	nsresult	rv;

	// set initial/default encoding to windows-1252 (not UTF-8)
	mEncoding = "windows-1252";

	rv = CommonInit();
	if (NS_FAILED(rv))	return(rv);

	// (do this last) register this as a named data source with the RDF service
	rv = mDirRDF->RegisterDataSource(this, false);
	if (NS_FAILED(rv)) return(rv);

	return(NS_OK);
}



nsresult
nsHTTPIndex::Init(nsIURI* aBaseURL)
{
  NS_PRECONDITION(aBaseURL != nullptr, "null ptr");
  if (! aBaseURL)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;

  rv = CommonInit();
  if (NS_FAILED(rv))	return(rv);

  // note: don't register DS here (singleton case)

  rv = aBaseURL->GetSpec(mBaseURL);
  if (NS_FAILED(rv)) return rv;

  // Mark the base url as a container
  nsCOMPtr<nsIRDFResource> baseRes;
  mDirRDF->GetResource(mBaseURL, getter_AddRefs(baseRes));
  Assert(baseRes, kNC_IsContainer, kTrueLiteral, true);

  return NS_OK;
}



nsresult
nsHTTPIndex::Create(nsIURI* aBaseURL, nsIInterfaceRequestor* aRequestor,
                    nsIHTTPIndex** aResult)
{
  *aResult = nullptr;

  nsHTTPIndex* result = new nsHTTPIndex(aRequestor);
  nsresult rv = result->Init(aBaseURL);
  if (NS_SUCCEEDED(rv))
  {
    NS_ADDREF(result);
    *aResult = result;
  }
  else
  {
    delete result;
  }
  return rv;
}

NS_IMETHODIMP
nsHTTPIndex::GetBaseURL(char** _result)
{
  *_result = ToNewCString(mBaseURL);
  if (! *_result)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsHTTPIndex::GetDataSource(nsIRDFDataSource** _result)
{
  NS_ADDREF(*_result = this);
  return NS_OK;
}

// This function finds the destination when following a given nsIRDFResource
// If the resource has a URL attribute, we use that. If not, just use
// the uri.
//
// Do NOT try to get the destination of a uri in any other way
void nsHTTPIndex::GetDestination(nsIRDFResource* r, nsACString& dest) {
  // First try the URL attribute
  nsCOMPtr<nsIRDFNode> node;

  GetTarget(r, kNC_URL, true, getter_AddRefs(node));
  nsCOMPtr<nsIRDFLiteral> url;

  if (node)
    url = do_QueryInterface(node);

  if (!url) {
     const char* temp;
     r->GetValueConst(&temp);
     dest.Adopt(temp ? strdup(temp) : 0);
  } else {
    const char16_t* uri;
    url->GetValueConst(&uri);
    dest.Adopt(ToNewUTF8String(nsDependentString(uri)));
  }
}

// rjc: isWellknownContainerURI() decides whether a URI is a container for which,
// when asked (say, by the template builder), we'll make a network connection
// to get its contents. For the moment, all we speak is ftp:// URLs, even though
//    a) we can get "http-index" mimetypes for really anything
//    b) we could easily handle file:// URLs here
//         Q: Why don't we?
//         A: The file system datasource ("rdf:file"); at some point, the two
//            should be perhaps united.  Until then, we can't aggregate both
//            "rdf:file" and "http-index" (such as with bookmarks) because we'd
//            get double the # of answers we really want... also, "rdf:file" is
//            less expensive in terms of both memory usage as well as speed



// We use an rdf attribute to mark if this is a container or not.
// Note that we still have to do string comparisons as a fallback
// because stuff like the personal toolbar and bookmarks check whether
// a URL is a container, and we have no attribute in that case.
bool
nsHTTPIndex::isWellknownContainerURI(nsIRDFResource *r)
{
  nsCOMPtr<nsIRDFNode> node;
  GetTarget(r, kNC_IsContainer, true, getter_AddRefs(node));
  if (node) {
    bool isContainerFlag;
    if (NS_SUCCEEDED(node->EqualsNode(kTrueLiteral, &isContainerFlag)))
      return isContainerFlag;
  }

  nsCString uri;
  GetDestination(r, uri);
  return StringBeginsWith(uri, nsDependentCString(kFTPProtocol)) &&
         (uri.Last() == '/');
}


NS_IMETHODIMP
nsHTTPIndex::GetURI(nsACString& aURI)
{
  aURI.AssignLiteral("rdf:httpindex");
  return NS_OK;
}

NS_IMETHODIMP
nsHTTPIndex::GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, bool aTruthValue,
			nsIRDFResource **_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;

	*_retval = nullptr;

	if (mInner)
	{
		rv = mInner->GetSource(aProperty, aTarget, aTruthValue, _retval);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, bool aTruthValue,
			nsISimpleEnumerator **_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;

	if (mInner)
	{
		rv = mInner->GetSources(aProperty, aTarget, aTruthValue, _retval);
	}
	else
	{
		rv = NS_NewEmptyEnumerator(_retval);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, bool aTruthValue,
			nsIRDFNode **_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;

	*_retval = nullptr;

        if ((aTruthValue) && (aProperty == kNC_Child) && isWellknownContainerURI(aSource))
	{
		// fake out the generic builder (i.e. return anything in this case)
		// so that search containers never appear to be empty
		NS_IF_ADDREF(aSource);
		*_retval = aSource;
		return(NS_OK);
	}

	if (mInner)
	{
		rv = mInner->GetTarget(aSource, aProperty, aTruthValue, _retval);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, bool aTruthValue,
			nsISimpleEnumerator **_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;

	if (mInner)
	{
		rv = mInner->GetTargets(aSource, aProperty, aTruthValue, _retval);
	}
	else
	{
		rv = NS_NewEmptyEnumerator(_retval);
	}

	if ((aProperty == kNC_Child) && isWellknownContainerURI(aSource))
	{
		bool		doNetworkRequest = true;
		if (NS_SUCCEEDED(rv) && (_retval))
		{
			// check and see if we already have data for the search in question;
			// if we do, don't bother doing the search again
			bool hasResults;
			if (NS_SUCCEEDED((*_retval)->HasMoreElements(&hasResults)) &&
			    hasResults)
			  doNetworkRequest = false;
		}

        // Note: if we need to do a network request, do it out-of-band
        // (because the XUL template builder isn't re-entrant)
        // by using a global connection list and an immediately-firing timer
		if (doNetworkRequest && mConnectionList)
		{
		    uint32_t connectionIndex;
                    nsresult idx_rv = mConnectionList->IndexOf(0, aSource, &connectionIndex);
		    if (NS_FAILED(idx_rv))
		    {
    		    // add aSource into list of connections to make
	    	    mConnectionList->AppendElement(aSource);

                // if we don't have a timer about to fire, create one
                // which should fire as soon as possible (out-of-band)
            	if (!mTimer)
            	{
                    rv = NS_NewTimerWithFuncCallback(getter_AddRefs(mTimer),
                                                     nsHTTPIndex::FireTimer,
                                                     this,
                                                     1,
                                                     nsITimer::TYPE_ONE_SHOT,
                                                     "nsHTTPIndex::GetTargets");
                    // Note: don't addref "this" as we'll cancel the
                    // timer in the httpIndex destructor
            		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a timer");
            	}
	    	}
		}
	}

	return(rv);
}


nsresult
nsHTTPIndex::AddElement(nsIRDFResource *parent, nsIRDFResource *prop, nsIRDFNode *child)
{

    if (!mNodeList)
    {
        mNodeList = nsArray::Create();
    }

    // order required: parent, prop, then child
    mNodeList->AppendElement(parent);
    mNodeList->AppendElement(prop);
    mNodeList->AppendElement(child);

	if (!mTimer)
	{
        return NS_NewTimerWithFuncCallback(getter_AddRefs(mTimer),
                                           nsHTTPIndex::FireTimer,
                                           this,
                                           1,
                                           nsITimer::TYPE_ONE_SHOT,
                                           "nsHTTPIndex::AddElement");
        // Note: don't addref "this" as we'll cancel the
        // timer in the httpIndex destructor
    }

    return(NS_OK);
}

void
nsHTTPIndex::FireTimer(nsITimer* aTimer, void* aClosure)
{
  nsHTTPIndex *httpIndex = static_cast<nsHTTPIndex *>(aClosure);
  if (!httpIndex)
    return;

  // don't return out of this loop as mTimer may need to be cancelled afterwards
  uint32_t numItems = 0;
  if (httpIndex->mConnectionList)
  {
    httpIndex->mConnectionList->GetLength(&numItems);
    if (numItems > 0)
    {
      nsCOMPtr<nsIRDFResource> source =
          do_QueryElementAt(httpIndex->mConnectionList, 0);
      httpIndex->mConnectionList->RemoveElementAt(0);

      nsCString uri = VoidCString();
      if (source) {
        httpIndex->GetDestination(source, uri);
      }

      if (uri.IsVoid()) {
        NS_ERROR("Could not reconstruct uri");
        return;
      }

      nsresult rv = NS_OK;
      nsCOMPtr<nsIURI>	url;

      rv = NS_NewURI(getter_AddRefs(url), uri.get());
      nsCOMPtr<nsIChannel> channel;
      if (NS_SUCCEEDED(rv) && (url)) {
        rv = NS_NewChannel(getter_AddRefs(channel),
            url,
            nsContentUtils::GetSystemPrincipal(),
            nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
            nsIContentPolicy::TYPE_OTHER);
      }
      if (NS_SUCCEEDED(rv) && (channel)) {
        channel->SetNotificationCallbacks(httpIndex);
        rv = channel->AsyncOpen2(httpIndex);
      }
    }
  }

  if (httpIndex->mNodeList)
  {
    httpIndex->mNodeList->GetLength(&numItems);
    if (numItems > 0)
    {
      // account for order required: src, prop, then target
      numItems /=3;
      if (numItems > 10)
        numItems = 10;

      int32_t loop;
      for (loop=0; loop<(int32_t)numItems; loop++)
      {
        nsCOMPtr<nsIRDFResource> src = do_QueryElementAt(httpIndex->mNodeList, 0);
        httpIndex->mNodeList->RemoveElementAt(0);

        nsCOMPtr<nsIRDFResource> prop = do_QueryElementAt(httpIndex->mNodeList, 0);
        httpIndex->mNodeList->RemoveElementAt(0);

        nsCOMPtr<nsIRDFNode> target = do_QueryElementAt(httpIndex->mNodeList, 0);
        httpIndex->mNodeList->RemoveElementAt(0);

        if (src && prop && target)
        {
          if (prop.get() == httpIndex->kNC_Loading)
          {
            httpIndex->Unassert(src, prop, target);
          }
          else
          {
            httpIndex->Assert(src, prop, target, true);
          }
        }
      }
    }
  }

  bool refireTimer = false;
  // check both lists to see if the timer needs to continue firing
  if (httpIndex->mConnectionList)
  {
    httpIndex->mConnectionList->GetLength(&numItems);
    if (numItems > 0)
    {
      refireTimer = true;
    }
    else
    {
      httpIndex->mConnectionList->Clear();
    }
  }

  if (httpIndex->mNodeList)
  {
    httpIndex->mNodeList->GetLength(&numItems);
    if (numItems > 0)
    {
      refireTimer = true;
    }
    else
    {
      httpIndex->mNodeList->Clear();
    }
  }

  // be sure to cancel the timer, as it holds a
  // weak reference back to nsHTTPIndex
  httpIndex->mTimer->Cancel();
  httpIndex->mTimer = nullptr;

  // after firing off any/all of the connections be sure
  // to cancel the timer if we don't need to refire it
  if (refireTimer)
  {
    NS_NewTimerWithFuncCallback(getter_AddRefs(httpIndex->mTimer),
                                nsHTTPIndex::FireTimer,
                                aClosure,
                                10,
                                nsITimer::TYPE_ONE_SHOT,
                                "nsHTTPIndex::FireTimer");
    // Note: don't addref "this" as we'll cancel the
    // timer in the httpIndex destructor
  }
}

NS_IMETHODIMP
nsHTTPIndex::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget,
			bool aTruthValue)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->Unassert(aSource, aProperty, aTarget);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::Change(nsIRDFResource *aSource, nsIRDFResource *aProperty,
			nsIRDFNode *aOldTarget, nsIRDFNode *aNewTarget)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::Move(nsIRDFResource *aOldSource, nsIRDFResource *aNewSource,
			nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty,
			nsIRDFNode *aTarget, bool aTruthValue, bool *_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->HasAssertion(aSource, aProperty, aTarget, aTruthValue, _retval);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::AddObserver(nsIRDFObserver *aObserver)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->AddObserver(aObserver);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::RemoveObserver(nsIRDFObserver *aObserver)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->RemoveObserver(aObserver);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, bool *result)
{
  if (!mInner) {
    *result = false;
    return NS_OK;
  }
  return mInner->HasArcIn(aNode, aArc, result);
}

NS_IMETHODIMP
nsHTTPIndex::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, bool *result)
{
    if (aArc == kNC_Child && isWellknownContainerURI(aSource)) {
      *result = true;
      return NS_OK;
    }

    if (mInner) {
      return mInner->HasArcOut(aSource, aArc, result);
    }

    *result = false;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPIndex::ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->ArcLabelsIn(aNode, _retval);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
	*_retval = nullptr;

	nsCOMPtr<nsISimpleEnumerator> child, anonArcs;
	if (isWellknownContainerURI(aSource))
	{
		NS_NewSingletonEnumerator(getter_AddRefs(child), kNC_Child);
	}

	if (mInner)
	{
		mInner->ArcLabelsOut(aSource, getter_AddRefs(anonArcs));
	}

	return NS_NewUnionEnumerator(_retval, child, anonArcs);
}

NS_IMETHODIMP
nsHTTPIndex::GetAllResources(nsISimpleEnumerator **_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->GetAllResources(_retval);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::IsCommandEnabled(nsISupports *aSources, nsIRDFResource *aCommand,
				nsISupports *aArguments, bool *_retval)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPIndex::DoCommand(nsISupports *aSources, nsIRDFResource *aCommand,
				nsISupports *aArguments)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPIndex::BeginUpdateBatch()
{
        return mInner->BeginUpdateBatch();
}

NS_IMETHODIMP
nsHTTPIndex::EndUpdateBatch()
{
        return mInner->EndUpdateBatch();
}

NS_IMETHODIMP
nsHTTPIndex::GetAllCmds(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->GetAllCmds(aSource, _retval);
	}
	return(rv);
}


//----------------------------------------------------------------------
//
// nsDirectoryViewerFactory
//
nsDirectoryViewerFactory::nsDirectoryViewerFactory()
{
}



nsDirectoryViewerFactory::~nsDirectoryViewerFactory()
{
}


NS_IMPL_ISUPPORTS(nsDirectoryViewerFactory, nsIDocumentLoaderFactory)



NS_IMETHODIMP
nsDirectoryViewerFactory::CreateInstance(const char *aCommand,
                                         nsIChannel* aChannel,
                                         nsILoadGroup* aLoadGroup,
                                         const nsACString& aContentType,
                                         nsIDocShell* aContainer,
                                         nsISupports* aExtraInfo,
                                         nsIStreamListener** aDocListenerResult,
                                         nsIContentViewer** aDocViewerResult)
{
  nsresult rv;

  bool viewSource = FindInReadable(NS_LITERAL_CSTRING("view-source"),
                                   aContentType);

  if (!viewSource &&
      Preferences::GetInt("network.dir.format", FORMAT_XUL) == FORMAT_XUL) {
    // ... and setup the original channel's content type
    (void)aChannel->SetContentType(NS_LITERAL_CSTRING("application/vnd.mozilla.xul+xml"));

    // This is where we shunt the HTTP/Index stream into our datasource,
    // and open the directory viewer XUL file as the content stream to
    // load in its place.

    // Create a dummy loader that will load a stub XUL document.
    nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;
    nsCString contractID;
    rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", "application/vnd.mozilla.xul+xml",
                                  getter_Copies(contractID));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIDocumentLoaderFactory>
      factory(do_GetService(contractID.get(), &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), "chrome://communicator/content/directory/directory.xul");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel),
                       uri,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER,
                       nullptr, // PerformanceStorage
                       aLoadGroup);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListener> listener;
    rv = factory->CreateInstance(aCommand, channel, aLoadGroup,
                                 NS_LITERAL_CSTRING("application/vnd.mozilla.xul+xml"),
                                 aContainer, aExtraInfo, getter_AddRefs(listener),
                                 aDocViewerResult);
    if (NS_FAILED(rv)) return rv;

    rv = channel->AsyncOpen2(listener);
    if (NS_FAILED(rv)) return rv;

    // Create an HTTPIndex object so that we can stuff it into the script context
    nsCOMPtr<nsIURI> baseuri;
    rv = aChannel->GetURI(getter_AddRefs(baseuri));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInterfaceRequestor> requestor = do_QueryInterface(aContainer,&rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIHTTPIndex> httpindex;
    rv = nsHTTPIndex::Create(baseuri, requestor, getter_AddRefs(httpindex));
    if (NS_FAILED(rv)) return rv;

    // Now shanghai the stream into our http-index parsing datasource
    // wrapper beastie.
    listener = do_QueryInterface(httpindex,&rv);
    *aDocListenerResult = listener.get();
    NS_ADDREF(*aDocListenerResult);

    return NS_OK;
  }

  // setup the original channel's content type
  (void)aChannel->SetContentType(NS_LITERAL_CSTRING("text/html"));

  // Otherwise, lets use the html listing
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  nsCString contractID;
  rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", "text/html",
                                getter_Copies(contractID));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDocumentLoaderFactory>
    factory(do_GetService(contractID.get(), &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStreamListener> listener;

  if (viewSource) {
    rv = factory->CreateInstance("view-source", aChannel, aLoadGroup,
                                 NS_LITERAL_CSTRING("text/html; x-view-type=view-source"),
                                 aContainer, aExtraInfo, getter_AddRefs(listener),
                                 aDocViewerResult);
  } else {
    rv = factory->CreateInstance("view", aChannel, aLoadGroup,
                                 NS_LITERAL_CSTRING("text/html"),
                                 aContainer, aExtraInfo, getter_AddRefs(listener),
                                 aDocViewerResult);
  }

  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStreamConverterService> scs = do_GetService("@mozilla.org/streamConverters;1", &rv);
  if (NS_FAILED(rv)) return rv;

  rv = scs->AsyncConvertData("application/http-index-format",
                             "text/html",
                             listener,
                             nullptr,
                             aDocListenerResult);

  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}



NS_IMETHODIMP
nsDirectoryViewerFactory::CreateInstanceForDocument(nsISupports* aContainer,
                                                    nsIDocument* aDocument,
                                                    const char *aCommand,
                                                    nsIContentViewer** aDocViewerResult)
{
  MOZ_ASSERT_UNREACHABLE("didn't expect to get here");
  return NS_ERROR_NOT_IMPLEMENTED;
}
