/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

  A directory viewer object. Parses "application/http-index-format"
  per Lou Montulli's original spec:

    http://www.area.com/~roeber/file_format.html

*/

#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDocumentViewer.h"
#include "nsIEnumerator.h"
#include "nsIGenericFactory.h"
#include "nsIHTTPIndex.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFDataSource.h"
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
#include "nsIInterfaceRequestor.h"
#include "iostream.h"
#include "nsIIOService.h"

//----------------------------------------------------------------------
//
// Common CIDs
//

// {82776710-5690-11d3-BE36-00104BDE6048}
#define NS_DIRECTORYVIEWERFACTORY_CID \
{ 0x82776710, 0x5690, 0x11d3, { 0xbe, 0x36, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }

static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

#define HTTPINDEX_NAMESPACE_URI "urn:http-index-format:"

//----------------------------------------------------------------------
//
// nsHTTPIndex
//

class nsHTTPIndex : public nsIHTTPIndex
{
protected:
  // We grab a reference to the content viewer container (which
  // indirectly owns us) so that we can insert ourselves as a global
  // in the script context _after_ the XUL doc has been embedded into
  // content viewer. We'll know that this has happened once we receive
  // an OnStartRequest() notification
  nsISupports* mContainer; // [WEAK]

  nsCString mBaseURL;

  nsCOMPtr<nsIRDFDataSource> mDataSource;

  nsHTTPIndex(nsISupports* aContainer);
  nsresult Init(nsIURI* aBaseURL);
  virtual ~nsHTTPIndex();

public:

  static nsresult Create(nsIURI* aBaseURI, nsISupports* aContainer, nsIHTTPIndex** aResult);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIHTTPIndex interface
  NS_IMETHOD GetBaseURL(char** _result);
  NS_IMETHOD GetDataSource(nsIRDFDataSource** _result);
  NS_IMETHOD CreateListener(nsIStreamListener** _result);
};


//----------------------------------------------------------------------
//
// nsHTTPIndexParser
//

class nsHTTPIndexParser : public nsIStreamListener
{
protected:
  static nsrefcnt gRefCnt;
  static nsIRDFService* gRDF;
  static nsIRDFResource* kHTTPIndex_Comment;
  static nsIRDFResource* kHTTPIndex_Filename;
  static nsIRDFResource* kHTTPIndex_Loading;
  static nsIRDFLiteral*  kTrueLiteral;

  static nsresult ParseLiteral(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
  static nsresult ParseDate(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
  static nsresult ParseInt(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);

  struct Field {
    const char*     mName;
    nsresult        (*mParse)(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
    nsIRDFResource* mProperty;
  };

  static Field gFieldTable[];

  nsHTTPIndex* mHTTPIndex; // [WEAK]

  nsCOMPtr<nsIRDFDataSource> mDataSource;

  nsCOMPtr<nsIURI> mDirectoryURI;
  nsCOMPtr<nsIRDFResource> mDirectory;
  nsCOMPtr<nsIRDFContainer> mDirectoryContainer;

  nsCString mBuf;
  nsresult ProcessData();
  nsresult ParseFormat(const char* aFormatStr);
  nsresult ParseData(const char* aDataStr);

  nsAutoString mComment;

  nsVoidArray mFormat;

  nsHTTPIndexParser(nsHTTPIndex* aHTTPIndex, nsISupports* aContainer);
  nsresult Init();

  virtual ~nsHTTPIndexParser();

  // If this is set, then we need to bind the nsIHTTPIndex object to
  // the global object when we get an OnStartRequest() notification
  // (at this point, we'll know that the XUL document has been
  // embedded and the global object won't get clobbered.
  nsISupports* mContainer;

public:
  static nsresult Create(nsHTTPIndex* aHTTPIndex,
                         nsISupports* aContainer,
                         nsIStreamListener** aResult);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSISTREAMLISTENER
};


nsrefcnt nsHTTPIndexParser::gRefCnt = 0;
nsIRDFService* nsHTTPIndexParser::gRDF;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Comment;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Filename;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Loading;
nsIRDFLiteral*  nsHTTPIndexParser::kTrueLiteral;

// This table tells us how to parse the fields in the HTTP-index
// stream into an RDF graph.
nsHTTPIndexParser::Field
nsHTTPIndexParser::gFieldTable[] = {
  { "Filename",       nsHTTPIndexParser::ParseLiteral, nsnull },
  { "Content-Length", nsHTTPIndexParser::ParseInt,     nsnull },
  { "Last-Modified",  nsHTTPIndexParser::ParseDate,    nsnull },
  { "Content-Type",   nsHTTPIndexParser::ParseLiteral, nsnull },
  { "File-Type",      nsHTTPIndexParser::ParseLiteral, nsnull },
  { "Permissions",    nsHTTPIndexParser::ParseLiteral, nsnull },
  { nsnull,           nsHTTPIndexParser::ParseLiteral, nsnull },
};

nsHTTPIndexParser::nsHTTPIndexParser(nsHTTPIndex* aHTTPIndex, nsISupports* aContainer)
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

  // No datasource. Uh oh. We won't be much use then.
  if (! mDataSource)
    return NS_ERROR_UNEXPECTED;

  if (gRefCnt++ == 0) {
    rv = nsServiceManager::GetService("component://netscape/rdf/rdf-service",
                                      NS_GET_IID(nsIRDFService),
                                      NS_REINTERPRET_CAST(nsISupports**, &gRDF));
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Comment",  &kHTTPIndex_Comment);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Filename", &kHTTPIndex_Filename);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Loading",  &kHTTPIndex_Loading);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetLiteral(nsAutoString("true").GetUnicode(), &kTrueLiteral);
    if (NS_FAILED(rv)) return rv;

    for (Field* field = gFieldTable; field->mName; ++field) {
      nsCAutoString str(HTTPINDEX_NAMESPACE_URI);
      str += field->mName;

      rv = gRDF->GetResource(str, &field->mProperty);
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
    NS_IF_RELEASE(kHTTPIndex_Loading);
    NS_IF_RELEASE(kTrueLiteral);

    for (Field* field = gFieldTable; field->mName; ++field) {
      NS_IF_RELEASE(field->mProperty);
    }

    nsServiceManager::ReleaseService("component://netscape/rdf/rdf-service", gRDF);
  }
}



nsresult
nsHTTPIndexParser::Create(nsHTTPIndex* aHTTPIndex,
                          nsISupports* aContainer,
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

NS_IMPL_THREADSAFE_ISUPPORTS2(nsHTTPIndexParser, nsIStreamListener, nsIStreamObserver);


NS_IMETHODIMP
nsHTTPIndexParser::OnStartRequest(nsIChannel* aChannel, nsISupports* aContext)
{
  nsresult rv;

  // This should only run once...
  if (mContainer) {
    // We need to undo the AddRef() on the nsHTTPIndex object that
    // happened in nsDirectoryViewerFactory::CreateInstance(). We'll
    // stuff it into an nsCOMPtr (because we _know_ it'll get release
    // if any errors occur)...
    nsCOMPtr<nsIHTTPIndex> httpindex = do_QueryInterface(mHTTPIndex);

    // ...and then _force_ a release here
    mHTTPIndex->Release();

    // Now get the content viewer container's script object.
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_GetInterface(mContainer));
    NS_ENSURE_TRUE(scriptGlobal, NS_ERROR_FAILURE);

    nsCOMPtr<nsIScriptContext> context;
    rv = scriptGlobal->GetContext(getter_AddRefs(context));
    NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);

    JSContext* jscontext = NS_REINTERPRET_CAST(JSContext*, context->GetNativeContext());
    JSObject* global = JS_GetGlobalObject(jscontext);

    // Using XPConnect, wrap the HTTP index object...
    static NS_DEFINE_CID(kXPConnectCID, NS_XPCONNECT_CID);
    NS_WITH_SERVICE(nsIXPConnect, xpc, kXPConnectCID, &rv);
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
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get jsobj from xpconnect wrapper");
    if (NS_FAILED(rv)) return rv;

    jsval jslistener = OBJECT_TO_JSVAL(jsobj);

    // ...and stuff it into the global context
    PRBool ok;
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

  // Mark the directory as "loading"
  rv = mDataSource->Assert(mDirectory, kHTTPIndex_Loading, kTrueLiteral, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}



NS_IMETHODIMP
nsHTTPIndexParser::OnStopRequest(nsIChannel* aChannel,
                                 nsISupports* aContext,
                                 nsresult aStatus,
                                 const PRUnichar* aErrorMsg)
{
  // If mDirectory isn't set, then we should just bail. Either an
  // error occurred and OnStartRequest() never got called, or
  // something exploded in OnStartRequest().
  if (! mDirectory)
    return NS_OK;

  // XXX Should we do anything different if aStatus != NS_OK?

  // Clean up any remaining data
  if (mBuf.Length() > 0)
    ProcessData();

  nsresult rv;

  nsCOMPtr<nsIRDFLiteral> comment;
  rv = gRDF->GetLiteral(mComment.GetUnicode(), getter_AddRefs(comment));
  if (NS_FAILED(rv)) return rv;

  rv = mDataSource->Assert(mDirectory, kHTTPIndex_Comment, comment, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // Remove the 'loading' annotation.
  rv = mDataSource->Unassert(mDirectory, kHTTPIndex_Loading, kTrueLiteral);
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

  return ProcessData();
}



nsresult
nsHTTPIndexParser::ProcessData()
{
	while(PR_TRUE)
	{
		PRInt32		eol = mBuf.FindCharInSet("\n\r");
		if (eol < 0)	break;
		
		nsCAutoString	line;
		mBuf.Left(line, eol);
		mBuf.Cut(0, eol + 1);

		if (line.Length() >= 4)
		{
			nsresult	rv;
			const char	*buf = line;

			if (buf[0] == '1')
			{
				if (buf[1] == '0')
				{
					if (buf[2] == '0' && buf[3] == ':')
					{
						// 100. Human-readable comment line. Ignore
					}
					else if (buf[2] == '1' && buf[3] == ':')
					{
						// 101. Human-readable information line.
						mComment += (buf + 4);
					}
					else if (buf[2] == '2' && buf[3] == ':')
					{
						// 102. Human-readable information line, HTML.
						mComment += (buf + 4);
					}
				}
			}
			else if (buf[0] == '2')
			{
				if (buf[1] == '0')
				{
					if (buf[2] == '0' && buf[3] == ':')
					{
						// 200. Define field names
						rv = ParseFormat(buf + 4);
						if (NS_FAILED(rv)) return rv;
					}
					else if (buf[2] == '1' && buf[3] == ':')
					{
						// 201. Field data
						rv = ParseData(buf + 4);
						if (NS_FAILED(rv)) return rv;
					}
				}
			}
			else if (buf[0] == '3')
			{
				if (buf[1] == '0')
				{
					if (buf[2] == '0' && buf[3] == ':')
					{
						// 300. Self-referring URL
					}
				}
			}
		}
	}
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



nsresult
nsHTTPIndexParser::ParseData(const char* aDataStr)
{
  // Parse a "201" data line, using the field ordering specified in
  // mFormat.

  if (mFormat.Count() == 0) {
    // Ignore if we haven't seen a format yet.
    return NS_OK;
  }

  // Because the 'file:' protocol won't do this for us, we need to
  // make sure that the mDirectoryURI ends with a '/' before
  // concatenating.
  nsresult rv;
  NS_WITH_SERVICE(nsIIOService, ioServ, kIOServiceCID, &rv);
  nsCOMPtr<nsIURI> realbase;

  {
      nsXPIDLCString orig;
      rv = mDirectoryURI->GetSpec(getter_Copies(orig));
      if (NS_FAILED(rv)) return rv;

      nsCAutoString basestr(orig);
      if (basestr.Last() != '/')
        basestr.Append('/');

      rv = NS_NewURI(getter_AddRefs(realbase), (const char*) basestr);
      if (NS_FAILED(rv)) return rv;
  }


  // First, we'll iterate through the values and remember each (using
  // an array of autostrings allocated on the stack, if possible). We
  // have to do this, because we don't know up-front the filename for
  // which this 201 refers.
#define MAX_AUTO_VALUES 8

  nsAutoString autovalues[MAX_AUTO_VALUES];
  nsAutoString* values = autovalues;

  if (mFormat.Count() > MAX_AUTO_VALUES) {
    // Yeah, we really -do- want to create nsAutoStrings in the heap
    // here, because most of the fields will be l.t. 32 characters:
    // this avoids an extra allocation for the nsString's buffer.
    values = new nsAutoString[mFormat.Count()];
    if (! values)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = NS_OK;
  nsCOMPtr<nsIRDFResource> entry;

  for (PRInt32 i = 0; i < mFormat.Count(); ++i) {
    // If we've exhausted the data before we run out of fields, just
    // bail.
    if (! *aDataStr)
      break;

    while (*aDataStr && nsCRT::IsAsciiSpace(PRUnichar(*aDataStr)))
      ++aDataStr;

    nsCAutoString	value;
    PRInt32		len;

    if (*aDataStr == '"' || *aDataStr == '\'') {
      // it's a quoted string. snarf everything up to the next quote character
      const char quotechar = *(aDataStr++);
      len = 0;
      while (aDataStr[len] && (aDataStr[len] != quotechar))
      	++len;
      value.SetCapacity(len + 1);
      value.Append(aDataStr, len);
      aDataStr += len;

      if (! aDataStr) {
        NS_WARNING("quoted value not terminated");
      }
    }
    else {
      // it's unquoted. snarf until we see whitespace.
      len = 0;
      while (aDataStr[len] && !nsCRT::IsAsciiSpace(aDataStr[len]))
        ++len;
      value.SetCapacity(len + 1);
      value.Append(aDataStr, len);
      aDataStr += len;
    }

    // Monkey with the nsStr, because we're bold.
    value.mLength = nsUnescapeCount(value.mStr);

    values[i] = value;

    Field* field = NS_STATIC_CAST(Field*, mFormat.ElementAt(i));
    if (field && field->mProperty == kHTTPIndex_Filename) {
      // we found the filename; construct a resource for its entry
      nsAutoString entryuri;
      char* result = nsnull;
      rv = ioServ->Escape(value.mStr, nsIIOService::url_FileBaseName +
                          nsIIOService::url_Forced, &result);
      rv = NS_MakeAbsoluteURI(result, realbase, entryuri);
      CRTFREEIF(result);
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
      Field* field = NS_REINTERPRET_CAST(Field*, mFormat.ElementAt(indx));
      if (! field)
        continue;

      nsCOMPtr<nsIRDFNode> value;
      rv = (*field->mParse)(field->mProperty, values[indx], getter_AddRefs(value));
      if (NS_FAILED(rv)) break;
      if (value)
      {
        rv = mDataSource->Assert(entry, field->mProperty, value, PR_TRUE);
        if (NS_FAILED(rv)) break;
      }
    }

    rv = mDirectoryContainer->AppendElement(entry);
  }

  // If we needed to spill values onto the heap, make sure we clean up
  // here.
  if (values != autovalues)
    delete[] values;

  return rv;
}



nsresult
nsHTTPIndexParser::ParseLiteral(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult)
{
  nsresult rv;

  if (arc == kHTTPIndex_Filename)
  {
	// strip off trailing slash(s) from directory names
  	PRInt32	len = aValue.Length();
  	if (len > 0)
  	{
  		if (aValue[len - 1] == '/')
  		{
  			aValue.SetLength(len - 1);
  		}
  	}
  }

  nsCOMPtr<nsIRDFLiteral> result;
  rv = gRDF->GetLiteral(aValue.GetUnicode(), getter_AddRefs(result));
  if (NS_FAILED(rv)) return rv;

  return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}



nsresult
nsHTTPIndexParser::ParseDate(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult)
{
  PRTime tm;
  PRStatus err = PR_ParseTimeString(nsCAutoString(aValue), PR_FALSE, &tm);
  if (err != PR_SUCCESS)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIRDFDate> result;
  rv = gRDF->GetDateLiteral(tm, getter_AddRefs(result));
  if (NS_FAILED(rv)) return rv;

  return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}



nsresult
nsHTTPIndexParser::ParseInt(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult)
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

nsHTTPIndex::nsHTTPIndex(nsISupports* aContainer)
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

  nsXPIDLCString url;
  rv = aBaseURL->GetSpec(getter_Copies(url));
  if (NS_FAILED(rv)) return rv;

  mBaseURL.Assign(url);

  static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
  rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID, nsnull,
                                          NS_GET_IID(nsIRDFDataSource),
                                          getter_AddRefs(mDataSource));
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}



nsresult
nsHTTPIndex::Create(nsIURI* aBaseURL, nsISupports* aContainer, nsIHTTPIndex** aResult)
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
}


NS_IMPL_ISUPPORTS1(nsHTTPIndex, nsIHTTPIndex);


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


//----------------------------------------------------------------------
//
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
                            nsISupports* aContainer,
                            nsISupports* aExtraInfo,
                            nsIStreamListener** aDocListenerResult,
                            nsIContentViewer** aDocViewerResult);

  NS_IMETHOD CreateInstanceForDocument(nsISupports* aContainer,
                                       nsIDocument* aDocument,
                                       const char *aCommand,
                                       nsIContentViewer** aDocViewerResult);
};



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
  rv = nsComponentManager::CreateInstance(NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "view/text/xul",
                                          nsnull,
                                          NS_GET_IID(nsIDocumentLoaderFactory),
                                          getter_AddRefs(factory));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), "chrome://directory/content/");
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIChannel> channel;
  rv = NS_OpenURI(getter_AddRefs(channel), uri, aLoadGroup);
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

  // addref the nsIHTTPIndex so that it'll live long enough to get
  // added to the embedder's script global object; this happens when
  // the stream's OnStartRequest() is called. (We can feel safe that
  // this'll happen because of the flow-of-control in nsWebShell.cpp)
  nsIHTTPIndex* dummy = httpindex;
  NS_ADDREF(dummy);

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


////////////////////////////////////////////////////////////////////////
// Component Exports

// The list of components we register
static nsModuleComponentInfo components[] = {
    { "Directory Viewer", NS_DIRECTORYVIEWERFACTORY_CID,
      NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "view/application/http-index-format",
      nsDirectoryViewerFactory::Create,
    },
};

NS_IMPL_NSGETMODULE("nsDirectoryViewerModule", components)
