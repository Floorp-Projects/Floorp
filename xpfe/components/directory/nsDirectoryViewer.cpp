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
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);

// Note: due to aggregation, the HTTPINDEX namespace should match
// what's used in the rest of the application
#define HTTPINDEX_NAMESPACE_URI 	NC_NAMESPACE_URI

//----------------------------------------------------------------------
//
// nsHTTPIndex
//

class nsHTTPIndex : public nsIHTTPIndex, public nsIRDFDataSource
{
private:
	static nsIRDFResource		*kNC_Child;
	static nsIRDFResource		*kNC_loading;

protected:
	// We grab a reference to the content viewer container (which
	// indirectly owns us) so that we can insert ourselves as a global
	// in the script context _after_ the XUL doc has been embedded into
	// content viewer. We'll know that this has happened once we receive
	// an OnStartRequest() notification

	nsCOMPtr<nsIRDFDataSource>	mInner;
	nsISupports			*mContainer;	// [WEAK]
	nsCString			mBaseURL;

			nsHTTPIndex(nsISupports* aContainer);
	nsresult 	Init(nsIURI* aBaseURL);
	PRBool		isWellknownContainerURI(nsIRDFResource *r);

public:
			nsHTTPIndex();
	virtual		~nsHTTPIndex();
	nsresult	Init(void);
static	nsresult	Create(nsIURI* aBaseURI, nsISupports* aContainer, nsIHTTPIndex** aResult);

	// nsIHTTPIndex interface
	NS_DECL_NSIHTTPINDEX

	// NSIRDFDataSource interface
	NS_DECL_NSIRDFDATASOURCE

	// nsISupports interface
	NS_DECL_ISUPPORTS
};


//----------------------------------------------------------------------
//
// nsHTTPIndexParser
//

class nsHTTPIndexParser : public nsIStreamListener
{
protected:
  static nsrefcnt gRefCntParser;
  static nsIRDFService* gRDF;
  static nsIRDFResource* kHTTPIndex_Comment;
  static nsIRDFResource* kHTTPIndex_Filename;
  static nsIRDFResource* kHTTPIndex_Filetype;
  static nsIRDFResource* kHTTPIndex_Loading;
  static nsIRDFLiteral*  kTrueLiteral;

  static nsresult ParseLiteral(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
  static nsresult ParseDate(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
  static nsresult ParseInt(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);

  struct Field {
    const char      *mName;
    const char      *mResName;
    nsresult        (*mParse)(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
    nsIRDFResource* mProperty;
  };

  static Field gFieldTable[];

  nsIHTTPIndex* mHTTPIndex; // [WEAK]

  nsCOMPtr<nsIRDFDataSource> mDataSource;

  nsCOMPtr<nsIURI> mDirectoryURI;
  nsCOMPtr<nsIRDFResource> mDirectory;
  nsCOMPtr<nsIRDFContainer> mDirectoryContainer;

  nsCString mBuf;
  nsresult ProcessData(nsISupports *context);
  nsresult ParseFormat(const char* aFormatStr);
  nsresult ParseData(const char* aDataStr, nsISupports *context);

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



nsIRDFService		*gRDF = nsnull;
PRInt32			gRefCnt = 0;

nsIRDFResource	*nsHTTPIndex::kNC_Child;
nsIRDFResource	*nsHTTPIndex::kNC_loading;

nsrefcnt nsHTTPIndexParser::gRefCntParser = 0;
nsIRDFService* nsHTTPIndexParser::gRDF;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Comment;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Filename;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Filetype;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Loading;
nsIRDFLiteral*  nsHTTPIndexParser::kTrueLiteral;



// This table tells us how to parse the fields in the HTTP-index
// stream into an RDF graph.
nsHTTPIndexParser::Field
nsHTTPIndexParser::gFieldTable[] = {
  { "Filename",       "http://home.netscape.com/NC-rdf#Name",              nsHTTPIndexParser::ParseLiteral, nsnull },
  { "Content-Length", "http://home.netscape.com/NC-rdf#Content-Length",    nsHTTPIndexParser::ParseInt,     nsnull },
  { "Last-Modified",  "http://home.netscape.com/WEB-rdf#LastModifiedDate", nsHTTPIndexParser::ParseDate,    nsnull },
  { "Content-Type",   "http://home.netscape.com/NC-rdf#Content-Type",      nsHTTPIndexParser::ParseLiteral, nsnull },
  { "File-Type",      "http://home.netscape.com/NC-rdf#File-Type",         nsHTTPIndexParser::ParseLiteral, nsnull },
  { "Permissions",    "http://home.netscape.com/NC-rdf#Permissions",       nsHTTPIndexParser::ParseLiteral, nsnull },
  { nsnull,           "",                                                  nsHTTPIndexParser::ParseLiteral, nsnull },
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

  if (gRefCntParser++ == 0) {
    rv = nsServiceManager::GetService("component://netscape/rdf/rdf-service",
                                      NS_GET_IID(nsIRDFService),
                                      NS_REINTERPRET_CAST(nsISupports**, &gRDF));
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Comment",  &kHTTPIndex_Comment);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Name", &kHTTPIndex_Filename);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "File-Type", &kHTTPIndex_Filetype);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "loading",  &kHTTPIndex_Loading);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetLiteral(nsAutoString("true").GetUnicode(), &kTrueLiteral);
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
    NS_IF_RELEASE(kHTTPIndex_Filetype);
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
    ProcessData(aContext);

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

  return ProcessData(aContext);
}



nsresult
nsHTTPIndexParser::ProcessData(nsISupports *context)
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
						rv = ParseData(buf + 4, context);
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
nsHTTPIndexParser::ParseData(const char* aDataStr, nsISupports *context)
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

	nsCAutoString	filename, filetype;

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
      filename = value.mStr;
    }
    else if (field && field->mProperty == kHTTPIndex_Filetype) {
      filetype = value.mStr;
    }
  }

      // we found the filename; construct a resource for its entry
      nsAutoString entryuri;
      char* result = nsnull;
      rv = ioServ->Escape(filename, nsIIOService::url_FileBaseName +
                          nsIIOService::url_Forced, &result);
      rv = NS_MakeAbsoluteURI(entryuri, result, realbase);
      CRTFREEIF(result);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable make absolute URI");
      if (filetype.EqualsIgnoreCase("directory"))
      {
            entryuri.Append(PRUnichar('/'));
      }
      rv = gRDF->GetResource(nsCAutoString(entryuri), getter_AddRefs(entry));

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

	if (!mDirectoryContainer)
	{
		nsCOMPtr<nsIRDFResource>	parentRes = do_QueryInterface(context);
		if (parentRes)
		{
			NS_WITH_SERVICE(nsIRDFContainerUtils, rdfc, "component://netscape/rdf/container-utils", &rv);
			if (NS_FAILED(rv)) return rv;

			rv = rdfc->MakeSeq(mDataSource, parentRes, getter_AddRefs(mDirectoryContainer));
			if (NS_FAILED(rv)) return rv;
		}
	}

	if (mDirectoryContainer)
	{
	    rv = mDirectoryContainer->AppendElement(entry);
	}
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

nsHTTPIndex::nsHTTPIndex()
	: mContainer(nsnull), mInner(nsnull)
{
	NS_INIT_REFCNT();
}



nsHTTPIndex::nsHTTPIndex(nsISupports* aContainer)
  : mContainer(aContainer), mInner(nsnull)
{
	NS_INIT_REFCNT();
}



nsHTTPIndex::~nsHTTPIndex()
{
	if (--gRefCnt == 0)
	{
		NS_IF_RELEASE(kNC_Child);
		NS_IF_RELEASE(kNC_loading);

		if (gRDF)
		{
			nsServiceManager::ReleaseService(kRDFServiceCID, gRDF);
			gRDF = nsnull;
		}
	}
}



nsresult
nsHTTPIndex::Init()
{
	nsresult	rv;

	if (gRefCnt++ == 0)
	{
		rv = nsServiceManager::GetService(kRDFServiceCID,
			NS_GET_IID(nsIRDFService), (nsISupports**) &gRDF);
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
		if (NS_FAILED(rv)) return rv;

		if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
				nsnull, NS_GET_IID(nsIRDFDataSource), (void**) getter_AddRefs(mInner))))
		{
			return(rv);
		}

		gRDF->GetResource(NC_NAMESPACE_URI "child",   &kNC_Child);
		gRDF->GetResource(NC_NAMESPACE_URI "loading", &kNC_loading);

		// register this as a named data source with the RDF service
		rv = gRDF->RegisterDataSource(this, PR_FALSE);
		if (NS_FAILED(rv)) return rv;
	}
	return(NS_OK);
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

  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}



nsresult
nsHTTPIndex::Create(nsIURI* aBaseURL, nsISupports* aContainer, nsIHTTPIndex** aResult)
{
  nsHTTPIndex* result = new nsHTTPIndex(aContainer);
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = result->Init();
  if (NS_FAILED(rv)) {
    delete result;
    return rv;
  }

  rv = result->Init(aBaseURL);
  if (NS_FAILED(rv)) {
    delete result;
    return rv;
  }

  NS_ADDREF(result);
  *aResult = result;
  return NS_OK;
}



NS_IMPL_ISUPPORTS2(nsHTTPIndex, nsIHTTPIndex, nsIRDFDataSource);



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
  *_result = this;
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

static const char		kFTPProtocol[] = "ftp://";

PRBool
nsHTTPIndex::isWellknownContainerURI(nsIRDFResource *r)
{
	PRBool		isContainerFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kFTPProtocol, sizeof(kFTPProtocol) - 1)))
	{
		if (uri[strlen(uri)-1] == '/')
		{
			isContainerFlag = PR_TRUE;
		}
	}
	return(isContainerFlag);
}



NS_IMETHODIMP
nsHTTPIndex::GetURI(char * *aURI)
{
	nsresult	rv = NS_ERROR_UNEXPECTED;

	*aURI = nsnull;

	if (mInner)
	{
		rv = mInner->GetURI(aURI);
	}
	return(rv);
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

	if (isWellknownContainerURI(aSource) && (aProperty == kNC_Child) && (aTruthValue))
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

	if (isWellknownContainerURI(aSource) && (aProperty == kNC_Child))
	{
		PRBool		doNetworkRequest = PR_TRUE;
		if (NS_SUCCEEDED(rv) && (_retval))
		{
			// check and see if we already have data for the search in question;
			// if we do, BeginSearchRequest() won't bother doing the search again,
			// otherwise it will kickstart it
			PRBool		hasResults = PR_FALSE;
			if (NS_SUCCEEDED((*_retval)->HasMoreElements(&hasResults)) && (hasResults == PR_TRUE))
			{
				doNetworkRequest = PR_FALSE;
			}
		}
		if (doNetworkRequest == PR_TRUE)
		{
			const char	*uri = nsnull;
			aSource->GetValueConst(&uri);

			nsCOMPtr<nsIURI>	url;
			if (NS_SUCCEEDED(rv = NS_NewURI(getter_AddRefs(url), uri)))
			{
				nsCOMPtr<nsIChannel>	channel;
				if (NS_SUCCEEDED(rv = NS_OpenURI(getter_AddRefs(channel), url, nsnull, nsnull)))
				{
					nsCOMPtr<nsIStreamListener>	listener;
					if (NS_SUCCEEDED(rv = CreateListener(getter_AddRefs(listener))))
					{
						if (NS_SUCCEEDED(rv = channel->AsyncRead(listener, aSource)))
						{
							nsCOMPtr<nsIRDFLiteral>	trueLiteral;
							gRDF->GetLiteral(nsAutoString("true").GetUnicode(),
								getter_AddRefs(trueLiteral));
							rv = mInner->Assert(aSource, kNC_loading, trueLiteral, PR_TRUE);
						}
					}
				}
			}
		}
	}

	if (mInner)
	{
		rv = mInner->GetTargets(aSource, aProperty, aTruthValue, _retval);
	}
	else
	{
		rv = NS_NewEmptyEnumerator(_retval);
	}
	return(rv);
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
		while (NS_SUCCEEDED(rv) && (anonArcs->HasMoreElements(&hasResults) &&
			(hasResults == PR_TRUE)))
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
  rv = NS_OpenURI(getter_AddRefs(channel), uri, nsnull, aLoadGroup);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStreamListener> listener;
  rv = factory->CreateInstance("view", channel, aLoadGroup, "text/xul",
                               aContainer, aExtraInfo, getter_AddRefs(listener),
                               aDocViewerResult);
  if (NS_FAILED(rv)) return rv;

  rv = channel->AsyncRead(listener, nsnull);
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

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHTTPIndex, Init)

// The list of components we register
static nsModuleComponentInfo components[] = {
    { "Directory Viewer", NS_DIRECTORYVIEWERFACTORY_CID,
      NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "view/application/http-index-format",
      nsDirectoryViewerFactory::Create,
    },
    { "Directory Viewer", NS_HTTPINDEX_SERVICE_CID, NS_HTTPINDEX_SERVICE_PROGID,
      nsHTTPIndexConstructor },
    { "Directory Viewer", NS_HTTPINDEX_SERVICE_CID, NS_HTTPINDEX_DATASOURCE_PROGID,
      nsHTTPIndexConstructor },
};

NS_IMPL_NSGETMODULE("nsDirectoryViewerModule", components)
