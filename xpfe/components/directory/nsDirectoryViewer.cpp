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
 * Original Author(s):
 *   Chris Waterson           <waterson@netscape.com>
 *   Robert John Churchill    <rjc@netscape.com>
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

  A directory viewer object. Parses "application/http-index-format"
  per Lou Montulli's original spec:

    http://www.area.com/~roeber/file_format.html

*/

#include "nsDirectoryViewer.h"
#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentViewer.h"
#include "nsIEnumerator.h"
#include "nsIGenericFactory.h"
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
#include "nsITextToSubURI.h"

//----------------------------------------------------------------------
//
// Common CIDs
//

static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kTextToSubURICID,           NS_TEXTTOSUBURI_CID);

// Note: due to aggregation, the HTTPINDEX namespace should match
// what's used in the rest of the application
#define HTTPINDEX_NAMESPACE_URI 	NC_NAMESPACE_URI

//----------------------------------------------------------------------
//
// nsHTTPIndex
//

//----------------------------------------------------------------------
//
// nsHTTPIndexParser
//

class nsHTTPIndexParser : public nsIStreamListener
{
protected:
  static nsrefcnt gRefCntParser;
  static nsIRDFService* gRDF;
  static nsITextToSubURI* gTextToSubURI;
  static nsIRDFResource* kHTTPIndex_Comment;
  static nsIRDFResource* kHTTPIndex_Filename;
  static nsIRDFResource* kHTTPIndex_Filetype;
  static nsIRDFResource* kHTTPIndex_Loading;
  static nsIRDFResource* kNC_Child;
  static nsIRDFLiteral*  kTrueLiteral;
  static nsIRDFLiteral*  kFalseLiteral;

  static nsresult ParseLiteral(nsIRDFResource *arc, const nsString& aValue, nsIRDFNode** aResult);
  static nsresult ParseDate(nsIRDFResource *arc, const nsString& aValue, nsIRDFNode** aResult);
  static nsresult ParseInt(nsIRDFResource *arc, const nsString& aValue, nsIRDFNode** aResult);

  struct Field {
    const char      *mName;
    const char      *mResName;
    nsresult        (*mParse)(nsIRDFResource *arc, const nsString& aValue, nsIRDFNode** aResult);
    nsIRDFResource* mProperty;
  };

  static Field gFieldTable[];

  nsIHTTPIndex* mHTTPIndex; // [WEAK]

  nsCOMPtr<nsIRDFDataSource> mDataSource;
  nsCOMPtr<nsIRDFResource> mDirectory;

  nsCString mURI;
  nsCString mBuf;
  PRInt32   mLineStart;

  nsresult ProcessData(nsISupports *context);
  nsresult ParseFormat(const char* aFormatStr);
  nsresult ParseData(nsString* values, const char *baseStr, const char *encodingStr, char* aDataStr, nsIRDFResource *parentRes);

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

nsrefcnt nsHTTPIndexParser::gRefCntParser = 0;
nsIRDFService* nsHTTPIndexParser::gRDF;
nsITextToSubURI *nsHTTPIndexParser::gTextToSubURI;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Comment;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Filename;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Filetype;
nsIRDFResource* nsHTTPIndexParser::kHTTPIndex_Loading;
nsIRDFResource* nsHTTPIndexParser::kNC_Child;
nsIRDFLiteral*  nsHTTPIndexParser::kTrueLiteral;
nsIRDFLiteral*  nsHTTPIndexParser::kFalseLiteral;



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
    mLineStart(0),
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

    rv = nsServiceManager::GetService(NS_ITEXTTOSUBURI_PROGID,
                                      NS_GET_IID(nsITextToSubURI),
                                      NS_REINTERPRET_CAST(nsISupports**, &gTextToSubURI));
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Comment",  &kHTTPIndex_Comment);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "Name", &kHTTPIndex_Filename);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "File-Type", &kHTTPIndex_Filetype);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(HTTPINDEX_NAMESPACE_URI "loading",  &kHTTPIndex_Loading);
    if (NS_FAILED(rv)) return rv;

	rv = gRDF->GetResource(NC_NAMESPACE_URI "child",   &kNC_Child);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2("true").GetUnicode(), &kTrueLiteral);
    if (NS_FAILED(rv)) return rv;
    rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2("false").GetUnicode(), &kFalseLiteral);
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
    NS_IF_RELEASE(kNC_Child);
    NS_IF_RELEASE(kTrueLiteral);
    NS_IF_RELEASE(kFalseLiteral);

    for (Field* field = gFieldTable; field->mName; ++field) {
      NS_IF_RELEASE(field->mProperty);
    }

    if (gRDF)
    {
        nsServiceManager::ReleaseService("component://netscape/rdf/rdf-service", gRDF);
        gRDF = nsnull;
    }
    if (gTextToSubURI)
    {
        nsServiceManager::ReleaseService(NS_ITEXTTOSUBURI_PROGID, gTextToSubURI);
        gTextToSubURI = nsnull;
    }
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
  nsCOMPtr<nsIURI> mDirectoryURI;
  rv = aChannel->GetURI(getter_AddRefs(mDirectoryURI));
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString uristr;
  rv = mDirectoryURI->GetSpec(getter_Copies(uristr));
  if (NS_FAILED(rv)) return rv;

  // we know that this is a directory (due to being a HTTP-INDEX response)
  // so ensure it ends with a slash if its a FTP URL
  mURI.Assign(uristr);
  if ((mURI.Find("ftp://", PR_TRUE) == 0) && (mURI.Last() != (PRUnichar('/'))))
  {
  	mURI.Append('/');
  }

  rv = gRDF->GetResource(mURI, getter_AddRefs(mDirectory));
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
  if (mBuf.Length() > (PRUint32) mLineStart)
  {
    ProcessData(aContext);
  }

  // free up buffer after processing all the data
  mBuf.Truncate();

  nsresult rv;

  nsCOMPtr<nsIRDFLiteral> comment;
  rv = gRDF->GetLiteral(mComment.GetUnicode(), getter_AddRefs(comment));
  if (NS_FAILED(rv)) return rv;

  rv = mDataSource->Assert(mDirectory, kHTTPIndex_Comment, comment, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // hack: Remove the 'loading' annotation (ignore errors)
  mHTTPIndex->AddElement(mDirectory, kHTTPIndex_Loading, kTrueLiteral);

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
    nsXPIDLCString  encodingStr;
	if (mHTTPIndex)
	{
	    mHTTPIndex->GetEncoding(getter_Copies(encodingStr));
	}

    nsCOMPtr<nsIRDFResource>	parentRes = do_QueryInterface(context);
    if (!parentRes)
    {
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

	while(PR_TRUE)
	{
	    ++numItems;

		PRInt32		eol = mBuf.FindCharInSet("\n\r", mLineStart);
		if (eol < 0)	break;
		mBuf.SetCharAt(PRUnichar('\0'), eol);

		const char  *line = &mBuf.mStr[mLineStart];
        PRInt32     lineLen = eol - mLineStart;
		mLineStart = eol + 1;

		if (lineLen >= 4)
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
						mComment.AppendWithConversion(buf + 4);
					}
					else if (buf[2] == '2' && buf[3] == ':')
					{
						// 102. Human-readable information line, HTML.
						mComment.AppendWithConversion(buf + 4);
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
						if (NS_FAILED(rv))
						{
						    return rv;
						}
					}
					else if (buf[2] == '1' && buf[3] == ':')
					{
						// 201. Field data
						rv = ParseData(values, mURI, encodingStr, ((char *)buf) + 4, parentRes);
						if (NS_FAILED(rv))
						{
						    return rv;
						}
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
	NS_PRECONDITION(encoding != nsnull, "null ptr");
	if (! encoding)
		return(NS_ERROR_NULL_POINTER);

	if ((*encoding = nsXPIDLCString::Copy(mEncoding)) == nsnull)
		return(NS_ERROR_OUT_OF_MEMORY);

	return(NS_OK);
}






nsresult
nsHTTPIndexParser::ParseData(nsString* values, const char *baseStr, const char *encodingStr,
    register char* aDataStr, nsIRDFResource *parentRes)
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
                if (nsCRT::strlen(result) > 0)
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
      entryuriC.Append(filename);

      // if its a directory, make sure it ends with a trailing slash
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
    for (PRInt32 indx = 0; indx < numFormats; ++indx) {
      Field* field = NS_STATIC_CAST(Field*, mFormat.ElementAt(indx));
//      Field* field = NS_REINTERPRET_CAST(Field*, mFormat.ElementAt(indx));
      if (! field)
        continue;

      nsCOMPtr<nsIRDFNode> nodeValue;
      rv = (*field->mParse)(field->mProperty, values[indx], getter_AddRefs(nodeValue));
      if (NS_FAILED(rv)) break;
      if (nodeValue)
      {
        rv = mDataSource->Assert(entry, field->mProperty, nodeValue, PR_TRUE);
        if (NS_FAILED(rv)) break;
      }
    }
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
    nsresult                  rv;
    nsCOMPtr<nsIRDFLiteral>   result;

    if (arc == kHTTPIndex_Filename)
    {
        // strip off trailing slash(s) from directory names
        PRInt32	len = aValue.Length();
        if (len > 0)
        {
            if (aValue[len - 1] == '/')
            {
                nsAutoString  temp(aValue);
                temp.SetLength(len - 1);
                rv = gRDF->GetLiteral(temp.GetUnicode(), getter_AddRefs(result));
            }
        }
    }

    if (!result)
    {
        rv = gRDF->GetLiteral(aValue.GetUnicode(), getter_AddRefs(result));
    }
    if (NS_FAILED(rv)) return rv;

    return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}



nsresult
nsHTTPIndexParser::ParseDate(nsIRDFResource *arc, const nsString& aValue, nsIRDFNode** aResult)
{
  PRTime tm;
  nsCAutoString avalueC;
  avalueC.AssignWithConversion(aValue);
  PRStatus err = PR_ParseTimeString(avalueC, PR_FALSE, &tm);
  if (err != PR_SUCCESS)
    return NS_ERROR_FAILURE;

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

	if (gRDF)
	{
		// this may fail; just ignore errors
		gRDF->UnregisterDataSource(this);
	}

	if (gRDF)
	{
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDF);
		gRDF = nsnull;
	}
}



nsresult
nsHTTPIndex::CommonInit()
{
	nsresult	rv = NS_OK;

	// set initial/default encoding to ISO-8859-1 (not UTF-8)
	mEncoding = "ISO-8859-1";

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

    rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2("true").GetUnicode(), &kTrueLiteral);
    if (NS_FAILED(rv)) return(rv);
    rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2("false").GetUnicode(), &kFalseLiteral);
    if (NS_FAILED(rv)) return(rv);

    rv = NS_NewISupportsArray(getter_AddRefs(mConnectionList));
    if (NS_FAILED(rv)) return(rv);

	// note: don't register DS here

	return(rv);
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
	rv = gRDF->RegisterDataSource(this, PR_FALSE);
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
nsHTTPIndex::GetURI(char * *uri)
{
	NS_PRECONDITION(uri != nsnull, "null ptr");
	if (! uri)
		return(NS_ERROR_NULL_POINTER);

	if ((*uri = nsXPIDLCString::Copy("rdf:httpindex")) == nsnull)
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

	if (mInner)
	{
		rv = mInner->GetTargets(aSource, aProperty, aTruthValue, _retval);
	}
	else
	{
		rv = NS_NewEmptyEnumerator(_retval);
	}

	if (isWellknownContainerURI(aSource) && (aProperty == kNC_Child))
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
            		mTimer = do_CreateInstance("component://netscape/timer", &rv);
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
		mTimer = do_CreateInstance("component://netscape/timer", &rv);
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a timer");
		if (NS_FAILED(rv))  return(rv);

		mTimer->Init(nsHTTPIndex::FireTimer, this, 100,
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
    PRInt32     loop;
    if (httpIndex->mConnectionList)
    {
        httpIndex->mConnectionList->Count(&numItems);
        if (numItems > 0)
        {
            // Note: process ALL outstanding connection requests;
            for (loop=((PRInt32)numItems)-1; loop>=0; loop--)
            {
                nsCOMPtr<nsISupports>   isupports;
                httpIndex->mConnectionList->GetElementAt((PRUint32)loop, getter_AddRefs(isupports));
                httpIndex->mConnectionList->RemoveElementAt((PRUint32)loop);

                nsCOMPtr<nsIRDFResource>    aSource;
                if (isupports)  aSource = do_QueryInterface(isupports);
    			const char	*uri = nsnull;
                if (aSource)    aSource->GetValueConst(&uri);
                nsresult            rv = NS_OK;
    			nsCOMPtr<nsIURI>	url;
                if (uri)
                {
        			rv = NS_NewURI(getter_AddRefs(url), uri);
        		}
				nsCOMPtr<nsIChannel>	channel;
        		if (NS_SUCCEEDED(rv) && (url))
        		{
    				rv = NS_OpenURI(getter_AddRefs(channel), url, nsnull, nsnull);
        		}
				nsCOMPtr<nsIStreamListener>	listener;
        		if (NS_SUCCEEDED(rv) && (channel))
        		{
					rv = httpIndex->CreateListener(getter_AddRefs(listener));
        		}
        		if (NS_SUCCEEDED(rv) && (listener))
        		{
					rv = channel->AsyncRead(listener, aSource);
        		}
            }
        }
        httpIndex->mConnectionList->Clear();
    }

    if (httpIndex->mNodeList)
    {
        httpIndex->mNodeList->Count(&numItems);
        if (numItems > 0)
        {
            // account for order required: src, prop, then target
            numItems /=3;
            if (numItems > 10)  numItems = 10;
            
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

    // after firing off any/all of the connections be sure
    // to cancel the timer if we don't need to refire it
	if (refireTimer == PR_TRUE)
	{
		httpIndex->mTimer->Init(nsHTTPIndex::FireTimer, httpIndex, 100,
		    NS_PRIORITY_LOWEST, NS_TYPE_ONE_SHOT);
		// Note: don't addref "this" as we'll cancel the
		// timer in the httpIndex destructor
	}
	else
	{
		// be sure to cancel the timer, as it holds a
		// weak reference back to nsHTTPIndex
		httpIndex->mTimer->Cancel();
		httpIndex->mTimer = nsnull;
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
  rv = nsComponentManager::CreateInstance(NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "view/text/xul",
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
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDirectoryViewerFactory)

// The list of components we register
static nsModuleComponentInfo components[] = {
    { "Directory Viewer", NS_DIRECTORYVIEWERFACTORY_CID,
      NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "view/application/http-index-format",
      nsDirectoryViewerFactoryConstructor,
    },
    { "Directory Viewer", NS_HTTPINDEX_SERVICE_CID, NS_HTTPINDEX_SERVICE_PROGID,
      nsHTTPIndexConstructor },
    { "Directory Viewer", NS_HTTPINDEX_SERVICE_CID, NS_HTTPINDEX_DATASOURCE_PROGID,
      nsHTTPIndexConstructor },
};

NS_IMPL_NSGETMODULE("nsDirectoryViewerModule", components)
