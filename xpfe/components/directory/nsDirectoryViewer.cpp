/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson           <waterson@netscape.com>
 *   Robert John Churchill    <rjc@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Bradley Baetz <bbaetz@student.usyd.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  A directory viewer object. Parses "application/http-index-format"
  per Lou Montulli's original spec:

    http://www.area.com/~roeber/file_format.html

  One added change is for a description entry, for when the
  target does not match the filename (ie gopher)

*/

#include "nsDirectoryViewer.h"
#include "nsIDirIndex.h"
#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentViewer.h"
#include "nsIEnumerator.h"
#include "nsIRDFService.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsIXPConnect.h"
#include "nsEnumeratorUtils.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "rdf.h"
#include "nsITextToSubURI.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIFTPChannel.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIProgressEventSink.h"
#include "nsIContent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIPref.h"
#include "nsIStreamConverterService.h"

//----------------------------------------------------------------------
//
// Common CIDs
//

static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);

// Various protocols we have to special case
static const char               kFTPProtocol[] = "ftp://";
static const char               kGopherProtocol[] = "gopher://";

//----------------------------------------------------------------------
//
// nsHTTPIndex
//

NS_IMPL_THREADSAFE_ISUPPORTS7(nsHTTPIndex,
                              nsIHTTPIndex,
                              nsIRDFDataSource,
                              nsIStreamListener,
                              nsIDirIndexListener,
                              nsIRequestObserver,
                              nsIInterfaceRequestor,
                              nsIFTPEventSink);

NS_IMETHODIMP
nsHTTPIndex::GetInterface(const nsIID &anIID, void **aResult ) 
{
    if (anIID.Equals(NS_GET_IID(nsIFTPEventSink))) {
        // If we don't have a container to store the logged data
        // then don't report ourselves back to the caller

        if (!mRequestor)
          return NS_ERROR_NO_INTERFACE;
        *aResult = NS_STATIC_CAST(nsIFTPEventSink*, this);
        NS_ADDREF(this);
        return NS_OK;
    }

    if (anIID.Equals(NS_GET_IID(nsIPrompt))) {
        
        if (!mRequestor) 
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIDOMWindow> aDOMWindow = do_GetInterface(mRequestor);
        if (!aDOMWindow) 
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
        
        return wwatch->GetNewPrompter(aDOMWindow, (nsIPrompt**)aResult);
    }  

    if (anIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
        
        if (!mRequestor) 
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIDOMWindow> aDOMWindow = do_GetInterface(mRequestor);
        if (!aDOMWindow) 
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
        
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
nsHTTPIndex::OnFTPControlLog(PRBool server, const char *msg)
{
    NS_ENSURE_TRUE(mRequestor, NS_OK);

    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_GetInterface(mRequestor));
    NS_ENSURE_TRUE(scriptGlobal, NS_OK);

    nsCOMPtr<nsIScriptContext> context;
    nsresult rv = scriptGlobal->GetContext(getter_AddRefs(context));
    NS_ENSURE_SUCCESS(rv, NS_OK);

    JSContext* jscontext = NS_REINTERPRET_CAST(JSContext*,
                                               context->GetNativeContext());

    JSObject* global = JS_GetGlobalObject(jscontext);

    if (!jscontext || !global) return NS_OK;
    
    jsval params[2];

    nsString unicodeMsg;
    unicodeMsg.AssignWithConversion(msg);
    JSString* jsMsgStr = JS_NewUCStringCopyZ(jscontext, (jschar*) unicodeMsg.get());

    params[0] = BOOLEAN_TO_JSVAL(server);
    params[1] = STRING_TO_JSVAL(jsMsgStr);
    
    jsval val;
    JS_CallFunctionName(jscontext, 
                        global, 
                        "OnFTPControlLog",
                        2, 
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
    mBindToGlobalObject = PR_FALSE;

    // Now get the content viewer container's script object.
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_GetInterface(mRequestor));
    NS_ENSURE_TRUE(scriptGlobal, NS_ERROR_FAILURE);

    nsCOMPtr<nsIScriptContext> context;
    rv = scriptGlobal->GetContext(getter_AddRefs(context));
    NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);

    JSContext* jscontext = NS_REINTERPRET_CAST(JSContext*,
                                               context->GetNativeContext());
    JSObject* global = JS_GetGlobalObject(jscontext);

    // Using XPConnect, wrap the HTTP index object...
    static NS_DEFINE_CID(kXPConnectCID, NS_XPCONNECT_CID);
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(kXPConnectCID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
    rv = xpc->WrapNative(jscontext,
                         global,
                         NS_STATIC_CAST(nsIHTTPIndex*, this),
                         NS_GET_IID(nsIHTTPIndex),
                         getter_AddRefs(wrapper));

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to xpconnect-wrap http-index");
    if (NS_FAILED(rv)) return rv;

    JSObject* jsobj;
    rv = wrapper->GetJSObject(&jsobj);
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "unable to get jsobj from xpconnect wrapper");
    if (NS_FAILED(rv)) return rv;

    jsval jslistener = OBJECT_TO_JSVAL(jsobj);

    // ...and stuff it into the global context
    PRBool ok;
    ok = JS_SetProperty(jscontext, global, "HTTPIndex", &jslistener);

    NS_ASSERTION(ok, "unable to set Listener property");
    if (! ok)
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
      
    nsXPIDLCString entryuriC;
    uri->GetSpec(getter_Copies(entryuriC));

    nsCOMPtr<nsIRDFResource> entry;
    rv = mDirRDF->GetResource(entryuriC, getter_AddRefs(entry));
    
    nsString uriUnicode;
    uriUnicode.AssignWithConversion(entryuriC);

    nsCOMPtr<nsIRDFLiteral> URLVal;
    rv = mDirRDF->GetLiteral(uriUnicode.get(), getter_AddRefs(URLVal));

    Assert(entry, kNC_URL, URLVal, PR_TRUE);
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
                           kTrueLiteral, PR_TRUE);
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

  nsXPIDLCString commentStr;
  mParser->GetComment(getter_Copies(commentStr));

  nsCOMPtr<nsIRDFLiteral> comment;
  rv = mDirRDF->GetLiteral(NS_ConvertASCIItoUCS2(commentStr).get(), getter_AddRefs(comment));
  if (NS_FAILED(rv)) return rv;

  rv = Assert(mDirectory, kNC_Comment, comment, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // hack: Remove the 'loading' annotation (ignore errors)
  AddElement(mDirectory, kNC_Loading, kTrueLiteral);

  return NS_OK;
}


NS_IMETHODIMP
nsHTTPIndex::OnDataAvailable(nsIRequest *request,
                             nsISupports* aContext,
                             nsIInputStream* aStream,
                             PRUint32 aSourceOffset,
                             PRUint32 aCount)
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
    NS_ERROR("Could not reconstruct base uri\n");
    return NS_ERROR_UNEXPECTED;
  }

  // we found the filename; construct a resource for its entry
  nsCAutoString entryuriC(baseStr);
  
  // gopher resources don't point to an entry in the same directory
  // like ftp uris. So the entryuriC is just a unique string, while
  // the URL attribute is the destination of this element
  // The naming scheme for the attributes is taken from the bookmarks
  nsXPIDLCString filename;
  nsresult rv = aIndex->GetLocation(getter_Copies(filename));
  if (NS_FAILED(rv)) return rv;
  entryuriC.Append(filename.get());
  
  // if its a directory, make sure it ends with a trailing slash.
  // This doesn't matter for gopher, (where directories don't have
  // to end in a trailing /), because the filename is used for the URL
  // attribute.
  PRUint32 type;
  rv = aIndex->GetType(&type);
  if (NS_FAILED(rv))
    return rv;

  PRBool isDirType = (type == nsIDirIndex::TYPE_DIRECTORY);

  if (isDirType) {
      entryuriC.Append('/');
  }
  
  nsCOMPtr<nsIRDFResource> entry;
  rv = mDirRDF->GetResource(entryuriC.get(), getter_AddRefs(entry));

  // At this point, we'll (hopefully) have found the filename and
  // constructed a resource for it, stored in entry. So now take a
  // second pass through the values and add as statements to the RDF
  // datasource.

  if (entry && NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIRDFLiteral> lit;
    nsString str;

    // For gopher, the target is the filename. We still have to do all
    // the above string manipulation though, because we need the entryuric
    // as the key for the RDF data source
    if (!strncmp(entryuriC.get(), kGopherProtocol, sizeof(kGopherProtocol)-1))
      str.AssignWithConversion(filename);
    else {
      str.AssignWithConversion(entryuriC.get());
    }

    rv = mDirRDF->GetLiteral(str.get(), getter_AddRefs(lit));

    if (NS_SUCCEEDED(rv)) {
      rv = Assert(entry, kNC_URL, lit, PR_TRUE);
      if (NS_FAILED(rv)) return rv;
      
      nsXPIDLString xpstr;

      // description
      rv = aIndex->GetDescription(getter_Copies(xpstr));
      if (NS_FAILED(rv)) return rv;

      rv = mDirRDF->GetLiteral(xpstr.get(), getter_AddRefs(lit));
      if (NS_FAILED(rv)) return rv;
      rv = Assert(entry, kNC_Description, lit, PR_TRUE);
      if (NS_FAILED(rv)) return rv;
      
      // contentlength
      PRUint32 size;
      rv = aIndex->GetSize(&size);
      if (NS_FAILED(rv)) return rv;
      if (size != PRUint32(-1)) {
        nsCOMPtr<nsIRDFInt> val;
        rv = mDirRDF->GetIntLiteral(size, getter_AddRefs(val));
        if (NS_FAILED(rv)) return rv;
        rv = Assert(entry, kNC_ContentLength, val, PR_TRUE);
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
        rv = Assert(entry, kNC_LastModified, val, PR_TRUE);
      }

      // filetype
      PRUint32 type;
      rv = aIndex->GetType(&type);
      switch (type) {
      case nsIDirIndex::TYPE_UNKNOWN:
        rv = mDirRDF->GetLiteral(NS_LITERAL_STRING("UNKNOWN").get(), getter_AddRefs(lit));
        break;
      case nsIDirIndex::TYPE_DIRECTORY:
        rv = mDirRDF->GetLiteral(NS_LITERAL_STRING("DIRECTORY").get(), getter_AddRefs(lit));
        break;
      case nsIDirIndex::TYPE_FILE:
        rv = mDirRDF->GetLiteral(NS_LITERAL_STRING("FILE").get(), getter_AddRefs(lit));
        break;
      case nsIDirIndex::TYPE_SYMLINK:
        rv = mDirRDF->GetLiteral(NS_LITERAL_STRING("SYMLINK").get(), getter_AddRefs(lit));
        break;
      }
      
      if (NS_FAILED(rv)) return rv;
      rv = Assert(entry, kNC_FileType, lit, PR_TRUE);
      if (NS_FAILED(rv)) return rv;
    }

    // Since the definition of a directory depends on the protocol, we would have
    // to do string comparisons all the time.
    // But we're told if we're a container right here - so save that fact
    if (isDirType)
      Assert(entry, kNC_IsContainer, kTrueLiteral, PR_TRUE);
    else
      Assert(entry, kNC_IsContainer, kFalseLiteral, PR_TRUE);
    
//   instead of
//       rv = Assert(parentRes, kNC_Child, entry, PR_TRUE);
//       if (NS_FAILED(rv)) return rv;
//   defer insertion onto a timer so that the UI isn't starved
    AddElement(parentRes, kNC_Child, entry);
  }

  return rv;
}

//----------------------------------------------------------------------
//
// nsHTTPIndex implementation
//

nsHTTPIndex::nsHTTPIndex()
  : mBindToGlobalObject(PR_TRUE),
    mRequestor(nsnull)
{
	NS_INIT_REFCNT();
}


nsHTTPIndex::nsHTTPIndex(nsIInterfaceRequestor* aRequestor)
  : mBindToGlobalObject(PR_TRUE),
    mRequestor(aRequestor)
{
	NS_INIT_REFCNT();
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
        mTimer = nsnull;
    }

    mConnectionList = nsnull;
    mNodeList = nsnull;
    
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

    // set initial/default encoding to ISO-8859-1 (not UTF-8)
    mEncoding = "ISO-8859-1";

    mDirRDF = do_GetService(kRDFServiceCID, &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
    if (NS_FAILED(rv)) {
      return(rv);
    }

    mInner = do_CreateInstance("@mozilla.org/rdf/datasource;1?name=in-memory-datasource", &rv);

    if (NS_FAILED(rv))
      return rv;

    mDirRDF->GetResource(NC_NAMESPACE_URI "child",   getter_AddRefs(kNC_Child));
    mDirRDF->GetResource(NC_NAMESPACE_URI "loading", getter_AddRefs(kNC_Loading));
    mDirRDF->GetResource(NC_NAMESPACE_URI "Comment", getter_AddRefs(kNC_Comment));
    mDirRDF->GetResource(NC_NAMESPACE_URI "URL", getter_AddRefs(kNC_URL));
    mDirRDF->GetResource(NC_NAMESPACE_URI "Name", getter_AddRefs(kNC_Description));
    mDirRDF->GetResource(NC_NAMESPACE_URI "Content-Length", getter_AddRefs(kNC_ContentLength));
    mDirRDF->GetResource("http://home.netscape.com/WEB-rdf#LastModifiedDate",
                         getter_AddRefs(kNC_LastModified));
    mDirRDF->GetResource(NC_NAMESPACE_URI "Content-Type", getter_AddRefs(kNC_ContentType));
    mDirRDF->GetResource(NC_NAMESPACE_URI "File-Type", getter_AddRefs(kNC_FileType));
    mDirRDF->GetResource(NC_NAMESPACE_URI "IsContainer", getter_AddRefs(kNC_IsContainer));

    rv = mDirRDF->GetLiteral(NS_LITERAL_STRING("true").get(), getter_AddRefs(kTrueLiteral));
    if (NS_FAILED(rv)) return(rv);
    rv = mDirRDF->GetLiteral(NS_LITERAL_STRING("false").get(), getter_AddRefs(kFalseLiteral));
    if (NS_FAILED(rv)) return(rv);

    rv = NS_NewISupportsArray(getter_AddRefs(mConnectionList));
    if (NS_FAILED(rv)) return(rv);

    // note: don't register DS here
    return rv;
}


nsresult
nsHTTPIndex::Init()
{
	nsresult	rv;

	// set initial/default encoding to ISO-8859-1 (not UTF-8)
	mEncoding = "ISO-8859-1";

	rv = CommonInit();
	if (NS_FAILED(rv))	return(rv);

	// (do this last) register this as a named data source with the RDF service
	rv = mDirRDF->RegisterDataSource(this, PR_FALSE);
	if (NS_FAILED(rv)) return(rv);

	return(NS_OK);
}



nsresult
nsHTTPIndex::Init(nsIURI* aBaseURL)
{
  NS_PRECONDITION(aBaseURL != nsnull, "null ptr");
  if (! aBaseURL)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;

  rv = CommonInit();
  if (NS_FAILED(rv))	return(rv);

  // note: don't register DS here (singleton case)

  nsXPIDLCString url;
  rv = aBaseURL->GetSpec(getter_Copies(url));
  if (NS_FAILED(rv)) return rv;

  mBaseURL.Assign(url);
  
  // Mark the base url as a container
  nsCOMPtr<nsIRDFResource> baseRes;
  mDirRDF->GetResource(mBaseURL.get(), getter_AddRefs(baseRes));
  Assert(baseRes, kNC_IsContainer, kTrueLiteral, PR_TRUE);

  return NS_OK;
}



nsresult
nsHTTPIndex::Create(nsIURI* aBaseURL, nsIInterfaceRequestor* aRequestor,
                    nsIHTTPIndex** aResult)
{
  *aResult = nsnull;

  nsHTTPIndex* result = new nsHTTPIndex(aRequestor);
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

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
void nsHTTPIndex::GetDestination(nsIRDFResource* r, nsXPIDLCString& dest) {
  // First try the URL attribute
  nsCOMPtr<nsIRDFNode> node;
  
  GetTarget(r, kNC_URL, PR_TRUE, getter_AddRefs(node));
  nsCOMPtr<nsIRDFLiteral> url;
  
  if (node)
    url = do_QueryInterface(node);

  if (!url) {
     const char* temp;
     r->GetValueConst(&temp);
     dest.Adopt(temp ? nsCRT::strdup(temp) : 0);
  } else {
    const PRUnichar* uri;
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

// We also handle gopher now


// We use an rdf attribute to mark if this is a container or not.
// Note that we still have to do string comparisons as a fallback
// because stuff like the personal toolbar and bookmarks check whether
// a URL is a container, and we have no attribute in that case.
PRBool
nsHTTPIndex::isWellknownContainerURI(nsIRDFResource *r)
{
  nsCOMPtr<nsIRDFNode> node;
  GetTarget(r, kNC_IsContainer, PR_TRUE, getter_AddRefs(node));

  PRBool isContainerFlag = PR_FALSE;

  if (node && NS_SUCCEEDED(node->EqualsNode(kTrueLiteral, &isContainerFlag))) {
    return isContainerFlag;
  } else {
    nsXPIDLCString uri;
    
    // For gopher, we need to follow the URL attribute to get the
    // real destination
    GetDestination(r,uri);

    if ((uri.get()) && (!strncmp(uri, kFTPProtocol, sizeof(kFTPProtocol) - 1))) {
      if (uri.Last() == '/') {
        isContainerFlag = PR_TRUE;
      }
    }

    // A gopher url is of the form:
    // gopher://example.com/xFileNameToGet
    // where x is a single character representing the type of file
    // 1 is a directory, and 7 is a search.
    // Searches will cause a dialog to be popped up (asking the user what
    // to search for), and so even though searches return a directory as a
    // result, don't treat it as a directory here.

    // The isContainerFlag test above will correctly handle this when a
    // search url is passed in as the baseuri
    if ((uri.get()) &&
        (!strncmp(uri,kGopherProtocol, sizeof(kGopherProtocol)-1))) {
      char* pos = PL_strchr(uri+sizeof(kGopherProtocol)-1, '/');
      if (!pos || pos[1] == '\0' || pos[1] == '1')
        isContainerFlag = PR_TRUE;
    }
  }
  return isContainerFlag;
}


NS_IMETHODIMP
nsHTTPIndex::GetURI(char * *uri)
{
	NS_PRECONDITION(uri != nsnull, "null ptr");
	if (! uri)
		return(NS_ERROR_NULL_POINTER);

	if ((*uri = nsCRT::strdup("rdf:httpindex")) == nsnull)
		return(NS_ERROR_OUT_OF_MEMORY);

	return(NS_OK);
}



NS_IMETHODIMP
nsHTTPIndex::GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue,
			nsIRDFResource **_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;

	*_retval = nsnull;

	if (mInner)
	{
		rv = mInner->GetSource(aProperty, aTarget, aTruthValue, _retval);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue,
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
nsHTTPIndex::GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue,
			nsIRDFNode **_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;

	*_retval = nsnull;

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
nsHTTPIndex::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue,
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
		PRBool		doNetworkRequest = PR_TRUE;
		if (NS_SUCCEEDED(rv) && (_retval))
		{
			// check and see if we already have data for the search in question;
			// if we do, don't bother doing the search again
			PRBool		hasResults = PR_FALSE;
			if (NS_SUCCEEDED((*_retval)->HasMoreElements(&hasResults)) &&
			    (hasResults == PR_TRUE))
			{
				doNetworkRequest = PR_FALSE;
			}
		}

        // Note: if we need to do a network request, do it out-of-band
        // (because the XUL template builder isn't re-entrant)
        // by using a global connection list and an immediately-firing timer

		if ((doNetworkRequest == PR_TRUE) && (mConnectionList))
		{
		    PRInt32 connectionIndex = mConnectionList->IndexOf(aSource);
		    if (connectionIndex < 0)
		    {
    		    // add aSource into list of connections to make
	    	    mConnectionList->AppendElement(aSource);

                // if we don't have a timer about to fire, create one
                // which should fire as soon as possible (out-of-band)
            	if (!mTimer)
            	{
            		mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
            		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a timer");
            		if (NS_SUCCEEDED(rv))
            		{
                		mTimer->Init(nsHTTPIndex::FireTimer, this, 1,
                		    NS_PRIORITY_LOWEST, NS_TYPE_ONE_SHOT);
                		// Note: don't addref "this" as we'll cancel the
                		// timer in the httpIndex destructor
            		}
            	}
	    	}
		}
	}

	return(rv);
}


nsresult
nsHTTPIndex::AddElement(nsIRDFResource *parent, nsIRDFResource *prop, nsIRDFNode *child)
{
    nsresult    rv;

    if (!mNodeList)
    {
        rv = NS_NewISupportsArray(getter_AddRefs(mNodeList));
        if (NS_FAILED(rv)) return(rv);
    }

    // order required: parent, prop, then child
    mNodeList->AppendElement(parent);
    mNodeList->AppendElement(prop);
    mNodeList->AppendElement(child);

	if (!mTimer)
	{
		mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a timer");
		if (NS_FAILED(rv))  return(rv);

		mTimer->Init(nsHTTPIndex::FireTimer, this, 1,
		    NS_PRIORITY_LOWEST, NS_TYPE_ONE_SHOT);
		// Note: don't addref "this" as we'll cancel the
		// timer in the httpIndex destructor
	}

    return(NS_OK);
}

void
nsHTTPIndex::FireTimer(nsITimer* aTimer, void* aClosure)
{
  nsHTTPIndex *httpIndex = NS_STATIC_CAST(nsHTTPIndex *, aClosure);
  if (!httpIndex)	return;
  
  // don't return out of this loop as mTimer may need to be cancelled afterwards
  PRBool      refireTimer = PR_FALSE;
  
  PRUint32    numItems = 0;
  if (httpIndex->mConnectionList)
  {
        httpIndex->mConnectionList->Count(&numItems);
        if (numItems > 0)
        {
          nsCOMPtr<nsISupports>   isupports;
          httpIndex->mConnectionList->GetElementAt((PRUint32)0, getter_AddRefs(isupports));
          httpIndex->mConnectionList->RemoveElementAt((PRUint32)0);
          
          nsCOMPtr<nsIRDFResource>    aSource;
          if (isupports)  aSource = do_QueryInterface(isupports);
          
          nsXPIDLCString uri;
          if (aSource) {
            httpIndex->GetDestination(aSource, uri);
          }
          
          if (!uri) {
            NS_ERROR("Could not reconstruct uri");
            return;
          }
          
          nsresult            rv = NS_OK;
          nsCOMPtr<nsIURI>	url;
          
          rv = NS_NewURI(getter_AddRefs(url), uri.get());
          nsCOMPtr<nsIChannel>	channel;
          if (NS_SUCCEEDED(rv) && (url)) {
            rv = NS_OpenURI(getter_AddRefs(channel), url, nsnull, nsnull);
          }
          if (NS_SUCCEEDED(rv) && (channel)) {
            channel->SetNotificationCallbacks(httpIndex);
            rv = channel->AsyncOpen(httpIndex, aSource);
          }
        }
    }
    if (httpIndex->mNodeList)
    {
        httpIndex->mNodeList->Count(&numItems);
        if (numItems > 0)
        {
            // account for order required: src, prop, then target
            numItems /=3;
            if (numItems > 10)  numItems = 10;
          
            PRInt32 loop;
            for (loop=0; loop<(PRInt32)numItems; loop++)
            {
                nsCOMPtr<nsISupports>   isupports;
                httpIndex->mNodeList->GetElementAt((PRUint32)0, getter_AddRefs(isupports));
                httpIndex->mNodeList->RemoveElementAt((PRUint32)0);
                nsCOMPtr<nsIRDFResource>    src;
                if (isupports)  src = do_QueryInterface(isupports);
                httpIndex->mNodeList->GetElementAt((PRUint32)0, getter_AddRefs(isupports));
                httpIndex->mNodeList->RemoveElementAt((PRUint32)0);
                nsCOMPtr<nsIRDFResource>    prop;
                if (isupports)  prop = do_QueryInterface(isupports);
                
                httpIndex->mNodeList->GetElementAt((PRUint32)0, getter_AddRefs(isupports));
                httpIndex->mNodeList->RemoveElementAt((PRUint32)0);
                nsCOMPtr<nsIRDFNode>    target;
                if (isupports)  target = do_QueryInterface(isupports);
                
                if (src && prop && target)
                {
                    if (prop.get() == httpIndex->kNC_Loading)
                    {
                        httpIndex->Unassert(src, prop, target);
                    }
                    else
                    {
                        httpIndex->Assert(src, prop, target, PR_TRUE);
                    }
                }
            }                
        }
    }
    
    // check both lists to see if the timer needs to continue firing
    if (httpIndex->mConnectionList)
    {
        httpIndex->mConnectionList->Count(&numItems);
        if (numItems > 0)
        {
            refireTimer = PR_TRUE;
        }
        else
        {
            httpIndex->mConnectionList->Clear();
        }
    }
    if (httpIndex->mNodeList)
    {
        httpIndex->mNodeList->Count(&numItems);
        if (numItems > 0)
        {
            refireTimer = PR_TRUE;
        }
        else
        {
            httpIndex->mNodeList->Clear();
        }
    }

    // be sure to cancel the timer, as it holds a
    // weak reference back to nsHTTPIndex
    httpIndex->mTimer->Cancel();
    httpIndex->mTimer = nsnull;
    
    // after firing off any/all of the connections be sure
    // to cancel the timer if we don't need to refire it
    if (refireTimer)
    {
      httpIndex->mTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (httpIndex->mTimer)
      {
        httpIndex->mTimer->Init(nsHTTPIndex::FireTimer, aClosure, 10,
                                NS_PRIORITY_LOWEST, NS_TYPE_ONE_SHOT);
        // Note: don't addref "this" as we'll cancel the
        // timer in the httpIndex destructor
      }
    }
}

NS_IMETHODIMP
nsHTTPIndex::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget,
			PRBool aTruthValue)
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
			nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval)
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
nsHTTPIndex::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  if (!mInner) {
    *result = PR_FALSE;
    return NS_OK;
  }
  return mInner->HasArcIn(aNode, aArc, result);
}

NS_IMETHODIMP 
nsHTTPIndex::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
    if (aArc == kNC_Child && isWellknownContainerURI(aSource)) {
      *result = PR_TRUE;
      return NS_OK;
    }

    if (mInner) {
      return mInner->HasArcOut(aSource, aArc, result);
    }

    *result = PR_FALSE;
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
	nsresult	rv = NS_ERROR_UNEXPECTED;

	*_retval = nsnull;

	nsCOMPtr<nsISupportsArray> array;
	rv = NS_NewISupportsArray(getter_AddRefs(array));
	if (NS_FAILED(rv)) return rv;

	if (isWellknownContainerURI(aSource))
	{
		array->AppendElement(kNC_Child);
	}

	if (mInner)
	{
		nsCOMPtr<nsISimpleEnumerator>	anonArcs;
		rv = mInner->ArcLabelsOut(aSource, getter_AddRefs(anonArcs));
		PRBool	hasResults = PR_TRUE;
		while (NS_SUCCEEDED(rv) &&
                       NS_SUCCEEDED(anonArcs->HasMoreElements(&hasResults)) &&
                       hasResults == PR_TRUE)
		{
			nsCOMPtr<nsISupports>	anonArc;
			if (NS_FAILED(anonArcs->GetNext(getter_AddRefs(anonArc))))
				break;
			array->AppendElement(anonArc);
		}
	}

	nsISimpleEnumerator* result = new nsArrayEnumerator(array);
	if (! result)
	return NS_ERROR_OUT_OF_MEMORY;

	NS_ADDREF(result);
	*_retval = result;
	return(NS_OK);
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
nsHTTPIndex::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->GetAllCommands(aSource, _retval);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::IsCommandEnabled(nsISupportsArray *aSources, nsIRDFResource *aCommand,
				nsISupportsArray *aArguments, PRBool *_retval)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->IsCommandEnabled(aSources, aCommand, aArguments, _retval);
	}
	return(rv);
}

NS_IMETHODIMP
nsHTTPIndex::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand,
				nsISupportsArray *aArguments)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;
	if (mInner)
	{
		rv = mInner->DoCommand(aSources, aCommand, aArguments);
	}
	return(rv);
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
  NS_INIT_REFCNT();
}



nsDirectoryViewerFactory::~nsDirectoryViewerFactory()
{
}


NS_IMPL_ISUPPORTS1(nsDirectoryViewerFactory, nsIDocumentLoaderFactory);



NS_IMETHODIMP
nsDirectoryViewerFactory::CreateInstance(const char *aCommand,
                                         nsIChannel* aChannel,
                                         nsILoadGroup* aLoadGroup,
                                         const char* aContentType, 
                                         nsISupports* aContainer,
                                         nsISupports* aExtraInfo,
                                         nsIStreamListener** aDocListenerResult,
                                         nsIContentViewer** aDocViewerResult)
{
  nsresult rv;

  // OK - are we going to be using the html listing or not?
  nsCOMPtr<nsIPref> prefSrv = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  PRBool useHtml;
  rv = prefSrv->GetBoolPref("network.dir.generate_html", &useHtml);

  // We need to disable html mode for file:///, at least for the moment
  // The charset coding isn't quite right for non ASCII systems
  // XXX - This is a temporary hack
  nsCOMPtr<nsIURI> uri;
  rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_SUCCEEDED(rv)) {
    PRBool isFile;
    if (NS_SUCCEEDED(uri->SchemeIs("file", &isFile)) && isFile)
      useHtml = PR_FALSE;
  }

  PRBool viewSource = (PL_strstr(aContentType,"view-source") != 0);

  if ((NS_FAILED(rv) || !useHtml) && !viewSource) {
    
    // This is where we shunt the HTTP/Index stream into our datasource,
    // and open the directory viewer XUL file as the content stream to
    // load in its place.
    
    // Create a dummy loader that will load a stub XUL document.
    nsCOMPtr<nsIDocumentLoaderFactory> factory;

    rv = nsComponentManager::CreateInstance(NS_DOCUMENT_LOADER_FACTORY_CONTRACTID_PREFIX "view;1?type=application/vnd.mozilla.xul+xml",
                                            nsnull,
                                            NS_GET_IID(nsIDocumentLoaderFactory),
                                            getter_AddRefs(factory));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), "chrome://communicator/content/directory/directory.xul");
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIChannel> channel;
    rv = NS_OpenURI(getter_AddRefs(channel), uri, nsnull, aLoadGroup);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIStreamListener> listener;
    rv = factory->CreateInstance(aCommand, channel, aLoadGroup, "application/vnd.mozilla.xul+xml",
                                 aContainer, aExtraInfo, getter_AddRefs(listener),
                                 aDocViewerResult);
    if (NS_FAILED(rv)) return rv;
    
    rv = channel->AsyncOpen(listener, nsnull);
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

    // ... and set the original channel's content type up
    (void)aChannel->SetContentType("application/vnd.mozilla.xul+xml");
    
    return NS_OK;
  }
  
  // Otherwise, lets use the html listing
  nsCOMPtr<nsIDocumentLoaderFactory> factory;

  if (viewSource) {
    rv = nsComponentManager::CreateInstance(NS_DOCUMENT_LOADER_FACTORY_CONTRACTID_PREFIX "view;1?type=text/html; x-view-type=view-source",
                                            nsnull,
                                            NS_GET_IID(nsIDocumentLoaderFactory),
                                            getter_AddRefs(factory));
  } else {
    rv = nsComponentManager::CreateInstance(NS_DOCUMENT_LOADER_FACTORY_CONTRACTID_PREFIX "view;1?type=text/html",
                                            nsnull,
                                            NS_GET_IID(nsIDocumentLoaderFactory),
                                            getter_AddRefs(factory));
  }
  
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIStreamListener> listener;

  if (viewSource) {
    rv = factory->CreateInstance("view-source", aChannel, aLoadGroup, "text/html; x-view-type=view-source",
                                 aContainer, aExtraInfo, getter_AddRefs(listener),
                                 aDocViewerResult);
  } else {
    rv = factory->CreateInstance("view", aChannel, aLoadGroup, "text/html",
                                 aContainer, aExtraInfo, getter_AddRefs(listener),
                                 aDocViewerResult);
  }

  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStreamConverterService> scs = do_GetService("@mozilla.org/streamConverters;1", &rv);
  if (NS_FAILED(rv)) return rv;

  rv = scs->AsyncConvertData(NS_LITERAL_STRING("application/http-index-format").get(),
                             NS_LITERAL_STRING("text/html").get(),
                             listener,
                             nsnull,
                             aDocListenerResult);

  if (NS_FAILED(rv)) return rv;

  NS_ADDREF(*aDocListenerResult);

  // ... and set the original channel's content type up
  (void)aChannel->SetContentType("text/html");

  return NS_OK;
}



NS_IMETHODIMP
nsDirectoryViewerFactory::CreateInstanceForDocument(nsISupports* aContainer,
                                                    nsIDocument* aDocument,
                                                    const char *aCommand,
                                                    nsIContentViewer** aDocViewerResult)
{
  NS_NOTYETIMPLEMENTED("didn't expect to get here");
  return NS_ERROR_NOT_IMPLEMENTED;
}
