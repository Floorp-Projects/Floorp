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
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
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
//----------------------------------------------------------------------
//
// Common CIDs
//

static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);

// Note: due to aggregation, the HTTPINDEX namespace should match
// what's used in the rest of the application
#define HTTPINDEX_NAMESPACE_URI 	NC_NAMESPACE_URI

// Various protocols we have to special case
static const char               kFTPProtocol[] = "ftp://";
static const char               kGopherProtocol[] = "gopher://";

//----------------------------------------------------------------------
//
// nsHTTPIndex
//

//----------------------------------------------------------------------
//
// nsHTTPIndexParser
//

class nsHTTPIndexParser : public nsIStreamListener, public nsIInterfaceRequestor, 
                          public nsIFTPEventSink
{
protected:
  static nsrefcnt gRefCntParser;
  static nsIRDFService* gRDF;
  static nsITextToSubURI* gTextToSubURI;
  static nsIRDFResource* kHTTPIndex_Comment;
  static nsIRDFResource* kHTTPIndex_Filename;
  static nsIRDFResource* kHTTPIndex_Description;
  static nsIRDFResource* kHTTPIndex_Filetype;
  static nsIRDFResource* kHTTPIndex_Loading;
  static nsIRDFResource* kHTTPIndex_URL;
  static nsIRDFResource* kHTTPIndex_IsContainer;
  static nsIRDFResource* kNC_Child;
  static nsIRDFLiteral*  kTrueLiteral;
  static nsIRDFLiteral*  kFalseLiteral;

  static nsresult ParseLiteral(nsIRDFResource *arc, const nsString& aValue,
                               nsIRDFNode** aResult);
  static nsresult ParseDate(nsIRDFResource *arc, const nsString& aValue,
                            nsIRDFNode** aResult);
  static nsresult ParseInt(nsIRDFResource *arc, const nsString& aValue,
                           nsIRDFNode** aResult);

  struct Field {
    const char      *mName;
    const char      *mResName;
    nsresult        (*mParse)(nsIRDFResource *arc, const nsString& aValue,
                              nsIRDFNode** aResult);
    nsIRDFResource* mProperty;
  };

  static Field gFieldTable[];

  nsIHTTPIndex* mHTTPIndex; // [WEAK]

  nsCOMPtr<nsIRDFDataSource> mDataSource;
  nsCOMPtr<nsIRDFResource> mDirectory;
  
  nsCString mBuf;
  PRInt32   mLineStart;
  PRBool    mHasDescription; // Is there a description entry?
  
  nsresult ProcessData(nsISupports *context);
  nsresult ParseFormat(const char* aFormatStr);
  nsresult ParseData(nsString* values, const char *encodingStr, char* aDataStr,
                     nsIRDFResource *parentRes);

  nsAutoString mComment;

  nsVoidArray mFormat;

  nsHTTPIndexParser(nsHTTPIndex* aHTTPIndex, nsISupports* aContainer);
  nsresult Init();

  virtual ~nsHTTPIndexParser();

  // If this is set, then we need to bind the nsIHTTPIndex object to
  // the global object when we get an OnStartRequest() notification
  // (at this point, we'll know that the XUL document has been
  // embedded and the global object won't get clobbered.
  PRBool mBindToGlobalObject;
  nsCOMPtr<nsISupports> mContainer;

public:
  static nsresult Create(nsHTTPIndex* aHTTPIndex,
                         nsISupports* aContainer,
                         nsIStreamListener** aResult);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIFTPEVENTSINK

};



nsrefcnt nsHTTPIndexParser::gRefCntParser = 0;
nsIRDFService* nsHTTPIndexParser::gRDF;
nsITextToSubURI *nsHTTPIndexParser::gTextToSubURI;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Comment;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Filename;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Description;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Filetype;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Loading;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_URL;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_IsContainer;
nsIRDFResource* nsHTTPIndexParser::kNC_Child;
nsIRDFLiteral*  nsHTTPIndexParser::kTrueLiteral;
nsIRDFLiteral*  nsHTTPIndexParser::kFalseLiteral;


// This table tells us how to parse the fields in the HTTP-index
// stream into an RDF graph.
nsHTTPIndexParser::Field
nsHTTPIndexParser::gFieldTable[] = {
  { "Filename",       "http://home.netscape.com/NC-rdf#Name",
    nsHTTPIndexParser::ParseLiteral, nsnull },
  { "Description",    "http://home.netscape.com/NC-rdf#Description",
    nsHTTPIndexParser::ParseLiteral, nsnull },
  { "Content-Length", "http://home.netscape.com/NC-rdf#Content-Length",
    nsHTTPIndexParser::ParseInt,     nsnull },
  { "Last-Modified",  "http://home.netscape.com/WEB-rdf#LastModifiedDate",
    nsHTTPIndexParser::ParseDate,    nsnull },
  { "Content-Type",   "http://home.netscape.com/NC-rdf#Content-Type",
    nsHTTPIndexParser::ParseLiteral, nsnull },
  { "File-Type",      "http://home.netscape.com/NC-rdf#File-Type",
    nsHTTPIndexParser::ParseLiteral, nsnull },
  { "Permissions",    "http://home.netscape.com/NC-rdf#Permissions",
    nsHTTPIndexParser::ParseLiteral, nsnull },
  { nsnull,           "",
    nsHTTPIndexParser::ParseLiteral, nsnull },
};

nsHTTPIndexParser::nsHTTPIndexParser(nsHTTPIndex* aHTTPIndex,
                                     nsISupports* aContainer)
  : mHTTPIndex(aHTTPIndex),
    mLineStart(0),
    mHasDescription(PR_FALSE),
    mBindToGlobalObject(PR_TRUE),
    mContainer(aContainer)
{
  NS_ASSERTION(aContainer, "Need a containter");
  NS_INIT_REFCNT();
}



nsresult
nsHTTPIndexParser::Init()
{
  NS_PRECONDITION(mHTTPIndex, "not initialized");
  if (! mHTTPIndex)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;
  mDataSource = do_QueryInterface(mHTTPIndex, &rv);
  if (NS_FAILED(rv)) return rv;

  // No datasource. Uh oh. We won't be much use then.
  if (! mDataSource)
    return NS_ERROR_UNEXPECTED;

  if (gRefCntParser++ == 0) {
    rv = nsServiceManager::GetService("@mozilla.org/rdf/rdf-service;1",
                                      NS_GET_IID(nsIRDFService),
                                      NS_REINTERPRET_CAST(nsISupports**, &gRDF));
    if (NS_FAILED(rv)) return rv;

    rv = nsServiceManager::GetService(NS_ITEXTTOSUBURI_CONTRACTID,
                                      NS_GET_IID(nsITextToSubURI),
                                      NS_REINTERPRET_CAST(nsISupports**, &gTextToSubURI));
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Comment",
                           &kHTTPIndex_Comment);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Name",
                           &kHTTPIndex_Filename);
    if (NS_FAILED(rv)) return rv;
    
    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Description",
                           &kHTTPIndex_Description);
    if (NS_FAILED(rv)) return rv;    

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "File-Type",
                           &kHTTPIndex_Filetype);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "loading",
                           &kHTTPIndex_Loading);
    if (NS_FAILED(rv)) return rv;
    
    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "URL",
                           &kHTTPIndex_URL);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "IsContainer",
                           &kHTTPIndex_IsContainer);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(NC_NAMESPACE_URI "child",
                           &kNC_Child);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetLiteral(NS_LITERAL_STRING("true").get(),
                          &kTrueLiteral);
    if (NS_FAILED(rv)) return rv;
    rv = gRDF->GetLiteral(NS_LITERAL_STRING("false").get(),
                          &kFalseLiteral);
    if (NS_FAILED(rv)) return rv;

    for (Field* field = gFieldTable; field->mName; ++field) {
      nsCAutoString str(field->mResName);

      rv = gRDF->GetResource(str, &field->mProperty);
      if (NS_FAILED(rv)) return rv;
    }
  }
  return NS_OK;
}


nsHTTPIndexParser::~nsHTTPIndexParser()
{
  if (--gRefCntParser == 0) {
    NS_IF_RELEASE(kHTTPIndex_Comment);
    NS_IF_RELEASE(kHTTPIndex_Filename);
    NS_IF_RELEASE(kHTTPIndex_Description);
    NS_IF_RELEASE(kHTTPIndex_Filetype);
    NS_IF_RELEASE(kHTTPIndex_Loading);
    NS_IF_RELEASE(kHTTPIndex_URL);
    NS_IF_RELEASE(kHTTPIndex_IsContainer);
    NS_IF_RELEASE(kNC_Child);
    NS_IF_RELEASE(kTrueLiteral);
    NS_IF_RELEASE(kFalseLiteral);

    for (Field* field = gFieldTable; field->mName; ++field) {
      NS_IF_RELEASE(field->mProperty);
    }

    if (gRDF)
    {
        nsServiceManager::ReleaseService("@mozilla.org/rdf/rdf-service;1", gRDF);
        gRDF = nsnull;
    }
    if (gTextToSubURI)
    {
        nsServiceManager::ReleaseService(NS_ITEXTTOSUBURI_CONTRACTID, gTextToSubURI);
        gTextToSubURI = nsnull;
    }
  }
}



nsresult
nsHTTPIndexParser::Create(nsHTTPIndex* aHTTPIndex,
                          nsISupports* aContainer,
                          nsIStreamListener** aResult)
{
  NS_PRECONDITION(aHTTPIndex, "null ptr");
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

NS_IMPL_THREADSAFE_ISUPPORTS4(nsHTTPIndexParser,
                              nsIStreamListener,
                              nsIRequestObserver,
                              nsIInterfaceRequestor,
                              nsIFTPEventSink);

NS_IMETHODIMP
nsHTTPIndexParser::GetInterface(const nsIID &anIID, void **aResult ) 
{
    if (anIID.Equals(NS_GET_IID(nsIFTPEventSink))) {
        // If we don't have a container to store the logged data
        // then don't report ourselves back to the caller
        if (!mContainer)
          return NS_ERROR_NO_INTERFACE;
        *aResult = NS_STATIC_CAST(nsIFTPEventSink*, this);
        NS_ADDREF(this);
        return NS_OK;
    }

    if (anIID.Equals(NS_GET_IID(nsIPrompt))) {
        
        nsCOMPtr<nsIInterfaceRequestor> requestor = do_QueryInterface(mContainer);
        if (!requestor) 
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIDOMWindow> aDOMWindow = do_GetInterface(requestor);
        if (!aDOMWindow) 
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
        
        return wwatch->GetNewPrompter(aDOMWindow, (nsIPrompt**)aResult);
    }  

    if (anIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
        
        nsCOMPtr<nsIInterfaceRequestor> requestor = do_QueryInterface(mContainer);
        if (!requestor) 
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIDOMWindow> aDOMWindow = do_GetInterface(requestor);
        if (!aDOMWindow) 
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
        
        return wwatch->GetNewAuthPrompter(aDOMWindow, (nsIAuthPrompt**)aResult);
    }  

    if (anIID.Equals(NS_GET_IID(nsIProgressEventSink))) {

        nsCOMPtr<nsIInterfaceRequestor> requestor = do_QueryInterface(mContainer);
        if (!requestor) 
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIProgressEventSink> sink = do_GetInterface(requestor);
        if (!sink) 
            return NS_ERROR_NO_INTERFACE;
        
        *aResult = sink;
        NS_ADDREF((nsISupports*)*aResult);
        return NS_OK;
    }

    return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP 
nsHTTPIndexParser::OnFTPControlLog(PRBool server, const char *msg)
{
    NS_ENSURE_TRUE(mContainer, NS_OK);

    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_GetInterface(mContainer));
    NS_ENSURE_TRUE(scriptGlobal, NS_OK);

    nsCOMPtr<nsIScriptContext> context;
    nsresult rv = scriptGlobal->GetContext(getter_AddRefs(context));
    NS_ENSURE_TRUE(context, NS_OK);

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
nsHTTPIndexParser::OnStartRequest(nsIRequest *request, nsISupports* aContext)
{
  nsresult rv;

  // This should only run once...
  // Unless we don't have a container to start with
  // (ie called from bookmarks as an rdf datasource)
  if (mBindToGlobalObject && mContainer.get()) {
    mBindToGlobalObject = PR_FALSE;

    nsCOMPtr<nsIHTTPIndex> httpindex = do_QueryInterface(mHTTPIndex);

    // Now get the content viewer container's script object.
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_GetInterface(mContainer));
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
                         httpindex,
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
    rv = gRDF->GetResource(entryuriC, getter_AddRefs(entry));
    
    nsString uriUnicode;
    uriUnicode.AssignWithConversion(entryuriC);

    nsCOMPtr<nsIRDFLiteral> URLVal;
    rv = gRDF->GetLiteral(uriUnicode.get(), getter_AddRefs(URLVal));

    mDataSource->Assert(entry, kHTTPIndex_URL, URLVal, PR_TRUE);
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
  rv = mDataSource->Assert(mDirectory, kHTTPIndex_Loading,
                           kTrueLiteral, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}



NS_IMETHODIMP
nsHTTPIndexParser::OnStopRequest(nsIRequest *request,
                                 nsISupports* aContext,
                                 nsresult aStatus)
{
  // If mDirectory isn't set, then we should just bail. Either an
  // error occurred and OnStartRequest() never got called, or
  // something exploded in OnStartRequest().
  if (! mDirectory)
    return NS_BINDING_ABORTED;

  // XXX Should we do anything different if aStatus != NS_OK?

  // Clean up any remaining data
  if (mBuf.Length() > (PRUint32) mLineStart)
  {
    ProcessData(mDirectory);
  }

  // free up buffer after processing all the data
  mBuf.Truncate();

  nsresult rv;

  nsCOMPtr<nsIRDFLiteral> comment;
  rv = gRDF->GetLiteral(mComment.get(), getter_AddRefs(comment));
  if (NS_FAILED(rv)) return rv;

  rv = mDataSource->Assert(mDirectory, kHTTPIndex_Comment, comment, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // hack: Remove the 'loading' annotation (ignore errors)
  mHTTPIndex->AddElement(mDirectory, kHTTPIndex_Loading, kTrueLiteral);

  return NS_OK;
}


NS_IMETHODIMP
nsHTTPIndexParser::OnDataAvailable(nsIRequest *request,
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

  // Make sure there's some data to process...
  if (aCount < 1)
    return NS_OK;

  PRInt32 len = mBuf.Length();

  // Ensure that our mBuf has capacity to hold the data we're about to
  // read.
  mBuf.SetCapacity(len + aCount + 1);
  if (! mBuf.mStr)
    return NS_ERROR_OUT_OF_MEMORY;

  // Now read the data into our buffer.
  nsresult rv;
  PRUint32 count;
  rv = aStream->Read(mBuf.mStr + len, aCount, &count);
  if (NS_FAILED(rv)) return rv;

  // Set the string's length according to the amount of data we've read.
  //
  // XXX You'd think that mBuf.SetLength() would do this, but it
  // doesn't. It calls Truncate(), so you can't set the length to
  // something longer.
  mBuf.mLength = len + count;
  AddNullTerminator(mBuf);

  return ProcessData(mDirectory);
}



nsresult
nsHTTPIndexParser::ProcessData(nsISupports *context)
{
    nsXPIDLCString  encodingStr;
	if (mHTTPIndex)
	{
	    mHTTPIndex->GetEncoding(getter_Copies(encodingStr));
	}

    nsCOMPtr<nsIRDFResource>	parentRes = do_QueryInterface(context);
    if (!parentRes) {
      NS_ERROR("Could not obtain parent resource");
      return(NS_ERROR_UNEXPECTED);
    }

  // First, we'll iterate through the values and remember each (using
  // an array of autostrings allocated on the stack, if possible). We
  // have to do this, because we don't know up-front the filename for
  // which this 201 refers.
#define MAX_AUTO_VALUES 8

  nsString autovalues[MAX_AUTO_VALUES];
  nsString* values = autovalues;

  if (mFormat.Count() > MAX_AUTO_VALUES) {
    // Yeah, we really -do- want to create nsAutoStrings in the heap
    // here, because most of the fields will be l.t. 32 characters:
    // this avoids an extra allocation for the nsString's buffer.
    values = new nsString[mFormat.Count()];
    if (! values)
    {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  PRInt32     numItems = 0;
  
  while(PR_TRUE) {
    ++numItems;
    
    PRInt32		eol = mBuf.FindCharInSet("\n\r", mLineStart);
    if (eol < 0)	break;
    mBuf.SetCharAt(PRUnichar('\0'), eol);
    
    const char  *line = &mBuf.mStr[mLineStart];
    
    PRInt32 lineLen = eol - mLineStart;
    mLineStart = eol + 1;
    
    if (lineLen >= 4) {
      nsresult	rv;
      const char	*buf = line;
      
      if (buf[0] == '1') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 100. Human-readable comment line. Ignore
          } else if (buf[2] == '1' && buf[3] == ':') {
            // 101. Human-readable information line.
              mComment.AppendWithConversion(buf + 4);
          } else if (buf[2] == '2' && buf[3] == ':') {
            // 102. Human-readable information line, HTML.
            mComment.AppendWithConversion(buf + 4);
          }
        }
      } else if (buf[0] == '2') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 200. Define field names
            rv = ParseFormat(buf + 4);
            if (NS_FAILED(rv)) {
              return rv;
            }
          } else if (buf[2] == '1' && buf[3] == ':') {
            // 201. Field data
            rv = ParseData(values, encodingStr, ((char *)buf) + 4, parentRes);
            if (NS_FAILED(rv)) {
              return rv;
            }
          }
        }
      } else if (buf[0] == '3') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 300. Self-referring URL
          }
        }
      }
    }
  }
  
  // If we needed to spill values onto the heap, make sure we clean up
  // here.
  if (values != autovalues)
    delete[] values;
  
  return(NS_OK);
}

nsresult
nsHTTPIndexParser::ParseFormat(const char* aFormatStr)
{
  // Parse a "200" format line, and remember the fields and their
  // ordering in mFormat. Multiple 200 lines stomp on each other.
  mFormat.Clear();

  do {
    while (*aFormatStr && nsCRT::IsAsciiSpace(PRUnichar(*aFormatStr)))
      ++aFormatStr;

    if (! *aFormatStr)
      break;

    nsCAutoString name;
    PRInt32	len = 0;
    while (aFormatStr[len] && !nsCRT::IsAsciiSpace(PRUnichar(aFormatStr[len])))
    	++len;
    name.SetCapacity(len + 1);
    name.Append(aFormatStr, len);
    aFormatStr += len;

    // Okay, we're gonna monkey with the nsStr. Bold!
    name.mLength = nsUnescapeCount(name.mStr);

    // All tokens are case-insensitive - http://www.area.com/~roeber/file_format.html
    if (name.EqualsIgnoreCase("description"))
        mHasDescription = PR_TRUE;

    Field* field = nsnull;
    for (Field* i = gFieldTable; i->mName; ++i) {
      if (name.EqualsIgnoreCase(i->mName)) {
        field = i;
        break;
      }
    }

    mFormat.AppendElement(field);
  } while (*aFormatStr);

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

nsresult
nsHTTPIndexParser::ParseData(nsString* values, const char *encodingStr,
                             char* aDataStr, nsIRDFResource *parentRes)
{
  // Parse a "201" data line, using the field ordering specified in
  // mFormat.

  PRInt32   numFormats = mFormat.Count();

  if (numFormats == 0) {
    // Ignore if we haven't seen a format yet.
    return NS_OK;
  }

  nsresult rv = NS_OK;

  nsCAutoString	filename;
  PRBool    isDirType = PR_FALSE;

  const char* baseStr = nsnull;
  parentRes->GetValueConst(&baseStr);
  if (! baseStr) {
    NS_ERROR("Could not reconstruct base uri\n");
    return NS_ERROR_UNEXPECTED;
  }

  for (PRInt32 i = 0; i < numFormats; ++i) {
    // If we've exhausted the data before we run out of fields, just
    // bail.
    if (! *aDataStr)
      break;

    while (*aDataStr && nsCRT::IsAsciiSpace(*aDataStr))
      ++aDataStr;

    char    *value = aDataStr;

    if (*aDataStr == '"' || *aDataStr == '\'') {
      // it's a quoted string. snarf everything up to the next quote character
      const char quotechar = *(aDataStr++);
      ++value;
      while (*aDataStr && *aDataStr != quotechar)
      	++aDataStr;
      *aDataStr++ = '\0';

      if (! aDataStr) {
        NS_WARNING("quoted value not terminated");
      }
    }
    else {
      // it's unquoted. snarf until we see whitespace.
      value = aDataStr;
      while (*aDataStr && (!nsCRT::IsAsciiSpace(*aDataStr)))
        ++aDataStr;
      *aDataStr++ = '\0';
    }

    Field* field = NS_STATIC_CAST(Field*, mFormat.ElementAt(i));
    if (field && field->mProperty == kHTTPIndex_Filename)
    {
        // don't unescape at this point, so that UnEscapeAndConvert() can
        filename = value;

        PRBool  success = PR_FALSE;

        nsAutoString entryuri;

        if (gTextToSubURI)
        {
            PRUnichar	*result = nsnull;
            if (NS_SUCCEEDED(rv = gTextToSubURI->UnEscapeAndConvert(encodingStr, filename,
                                                                    &result)) && (result))
            {
                if (result[0])
                {
                    values[i].Assign(result);
                    success = PR_TRUE;
                }
                Recycle(result);
            }
            else
            {
                NS_WARNING("UnEscapeAndConvert error");
            }
        }

        if (success == PR_FALSE)
        {
            // if unsuccessfully at charset conversion, then
            // just fallback to unescape'ing in-place
            nsUnescape(value);
            values[i].AssignWithConversion(value);
        }

    }
    else
    {
        // unescape in-place
        nsUnescape(value);
        values[i].AssignWithConversion(value);

        if (field && field->mProperty == kHTTPIndex_Filetype)
        {
            if (!nsCRT::strcasecmp(value, "directory"))
            {
                isDirType = PR_TRUE;
            }
        }
    }
  }

  // we found the filename; construct a resource for its entry
  nsCAutoString entryuriC(baseStr);
  
  // gopher resource don't point to an entry in the same directory
  // like ftp uris. So the entryuriC is just a unique string, while
  // the URL attribute is the destination of this element
  // The naming scheme for the attributes is taken from the bookmarks
  entryuriC.Append(filename);
  
  // if its a directory, make sure it ends with a trailing slash.
  // This doesn't matter for gopher, (where directories don't have
  // to end in a trailing /), because the filename is used for the URL
  // attribute.
  if (isDirType == PR_TRUE)
    {
      entryuriC.Append('/');
    }
  
  nsCOMPtr<nsIRDFResource> entry;
  rv = gRDF->GetResource(entryuriC, getter_AddRefs(entry));

  // At this point, we'll (hopefully) have found the filename and
  // constructed a resource for it, stored in entry. So now take a
  // second pass through the values and add as statements to the RDF
  // datasource.

  if (entry && NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIRDFLiteral> URLVal;
    nsString url;

    // For gopher, the target is the filename. We still have to do all
    // the above string manipulation though, because we need the entryuric
    // as the key for the RDF data source
    if (!strncmp(entryuriC, kGopherProtocol, sizeof(kGopherProtocol)-1))
      url.AssignWithConversion(filename);
    else
      url.AssignWithConversion(entryuriC);

    rv = gRDF->GetLiteral(url.get(), getter_AddRefs(URLVal));

    if (NS_SUCCEEDED(rv)) {
      mDataSource->Assert(entry, kHTTPIndex_URL, URLVal, PR_TRUE);

      for (PRInt32 indx = 0; indx < numFormats; ++indx) {
        Field* field = NS_STATIC_CAST(Field*, mFormat.ElementAt(indx));
        if (! field)
          continue;
        
        if (mHasDescription && field->mProperty == kHTTPIndex_Filename)
          continue;
        
        nsCOMPtr<nsIRDFNode> nodeValue;
        rv = (*field->mParse)(field->mProperty, values[indx], getter_AddRefs(nodeValue));
        if (NS_FAILED(rv)) break;
        if (nodeValue) {
          // If there is a description entry, prefer that over the filename
          if (mHasDescription && field->mProperty == kHTTPIndex_Description) {
            rv = mDataSource->Assert(entry, kHTTPIndex_Filename, nodeValue, PR_TRUE);
            if (NS_FAILED(rv)) break;
          }

          rv = mDataSource->Assert(entry, field->mProperty, nodeValue, PR_TRUE);
          if (NS_FAILED(rv)) break;
        }
      }
    }

    // Since the definition of a directory depends on the protocol, we would have
    // to do string comparisons all the time.
    // But we're told if we're a container right here - so save that fact
    if (isDirType)
      mDataSource->Assert(entry, kHTTPIndex_IsContainer, kTrueLiteral, PR_TRUE);
    else
      mDataSource->Assert(entry, kHTTPIndex_IsContainer, kFalseLiteral, PR_TRUE);
    
//   instead of
//       rv = mDataSource->Assert(parentRes, kNC_Child, entry, PR_TRUE);
//       if (NS_FAILED(rv)) return rv;
//   defer insertion onto a timer so that the UI isn't starved
    mHTTPIndex->AddElement(parentRes, kNC_Child, entry);
  }

  return rv;
}



nsresult
nsHTTPIndexParser::ParseLiteral(nsIRDFResource *arc, const nsString& aValue, nsIRDFNode** aResult)
{
    nsresult                  rv = NS_OK;
    nsCOMPtr<nsIRDFLiteral>   result;

    if (arc == kHTTPIndex_Filename)
    {
        // strip off trailing slash(s) from directory names - but not gopher
        PRInt32	len = aValue.Length();
        if (len > 0)
        {
          if (aValue[len - 1] == '/' && aValue.EqualsIgnoreCase(kGopherProtocol,
                                                                sizeof(kGopherProtocol)))
            {
                nsAutoString  temp(aValue);
                temp.SetLength(len - 1);
                rv = gRDF->GetLiteral(temp.get(), getter_AddRefs(result));
            }
        }
    }

    if (!result)
    {
        rv = gRDF->GetLiteral(aValue.get(), getter_AddRefs(result));
    }
    if (NS_FAILED(rv)) return rv;

    return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}



nsresult
nsHTTPIndexParser::ParseDate(nsIRDFResource *arc, const nsString& aValue, nsIRDFNode** aResult)
{
  *aResult = nsnull;
  PRTime tm;
  nsCAutoString avalueC;
  avalueC.AssignWithConversion(aValue);
  PRStatus err = PR_ParseTimeString(avalueC, PR_FALSE, &tm);
  if (err != PR_SUCCESS)
    return NS_OK;   // if unable to parse the date/time string, that's OK, just return no value

  nsresult rv;
  nsCOMPtr<nsIRDFDate> result;
  rv = gRDF->GetDateLiteral(tm, getter_AddRefs(result));
  if (NS_FAILED(rv)) return rv;

  return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}



nsresult
nsHTTPIndexParser::ParseInt(nsIRDFResource *arc, const nsString& aValue, nsIRDFNode** aResult)
{
  PRInt32 err;
  PRInt32 i = aValue.ToInteger(&err);
  if (nsresult(err) != NS_OK)
    return NS_ERROR_FAILURE;

  if (i == 0)
  {
  	// disregard "zero" values
  	*aResult = nsnull;
  	return(NS_OK);
  }

  nsresult rv;
  nsCOMPtr<nsIRDFInt> result;
  rv = gRDF->GetIntLiteral(i, getter_AddRefs(result));
  if (NS_FAILED(rv)) return rv;

  return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}



//----------------------------------------------------------------------
//
// nsHTTPIndex implementation
//

nsHTTPIndex::nsHTTPIndex()
  : mContainer(nsnull)
{
	NS_INIT_REFCNT();
}


nsHTTPIndex::nsHTTPIndex(nsISupports* aContainer)
  : mContainer(aContainer)
{
	NS_INIT_REFCNT();
}


nsHTTPIndex::~nsHTTPIndex()
{
  // note: these are NOT statics due to the native of nsHTTPIndex
  // where it may or may not be treated as a singleton
  
  NS_IF_RELEASE(kNC_Child);
  NS_IF_RELEASE(kNC_loading);
  NS_IF_RELEASE(kNC_URL);
  NS_IF_RELEASE(kNC_IsContainer);
  NS_IF_RELEASE(kTrueLiteral);
  NS_IF_RELEASE(kFalseLiteral);
  
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

    mDirRDF->GetResource(NC_NAMESPACE_URI "child",   &kNC_Child);
    mDirRDF->GetResource(NC_NAMESPACE_URI "loading", &kNC_loading);
    mDirRDF->GetResource(NC_NAMESPACE_URI "URL", &kNC_URL);
    mDirRDF->GetResource(NC_NAMESPACE_URI "IsContainer", &kNC_IsContainer);

    rv = mDirRDF->GetLiteral(NS_LITERAL_STRING("true").get(), &kTrueLiteral);
    if (NS_FAILED(rv)) return(rv);
    rv = mDirRDF->GetLiteral(NS_LITERAL_STRING("false").get(), &kFalseLiteral);
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
nsHTTPIndex::Create(nsIURI* aBaseURL, nsISupports* aContainer, nsIHTTPIndex** aResult)
{
  *aResult = nsnull;

  nsHTTPIndex* result = new nsHTTPIndex(aContainer);
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

NS_IMPL_THREADSAFE_ISUPPORTS2(nsHTTPIndex, nsIHTTPIndex, nsIRDFDataSource);

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

NS_IMETHODIMP
nsHTTPIndex::CreateListener(nsIStreamListener** _result)
{
  return nsHTTPIndexParser::Create(this, mContainer, _result);
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



NS_IMETHODIMP
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
          if (aSource)
            httpIndex->GetDestination(aSource, uri);
          
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
          nsCOMPtr<nsIStreamListener>	listener;
          if (NS_SUCCEEDED(rv) && (channel)) {
            rv = httpIndex->CreateListener(getter_AddRefs(listener));
          }
          if (NS_SUCCEEDED(rv) && (listener)) {
            nsCOMPtr<nsIInterfaceRequestor> callbacks = do_QueryInterface(listener);
            if (callbacks)
              channel->SetNotificationCallbacks(callbacks);
            rv = channel->AsyncOpen(listener, aSource);
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
                    if (prop.get() == httpIndex->kNC_loading)
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
  // This is where we shunt the HTTP/Index stream into our datasource,
  // and open the directory viewer XUL file as the content stream to
  // load in its place.
  nsresult rv;

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
  rv = factory->CreateInstance("view", channel, aLoadGroup, "application/vnd.mozilla.xul+xml",
                               aContainer, aExtraInfo, getter_AddRefs(listener),
                               aDocViewerResult);
  if (NS_FAILED(rv)) return rv;

  rv = channel->AsyncOpen(listener, nsnull);
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

  return rv;
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
