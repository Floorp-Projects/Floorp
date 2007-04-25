/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/*

  Implementation for a Related Links RDF data store.

 */

#include "nsRelatedLinksHandlerImpl.h"
#include "nsCRT.h"
#include "nsEnumeratorUtils.h"
#include "nsIInputStream.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFPurgeableDataSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIInputStream.h"
#include "nsIPref.h"
#include "nsRDFCID.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nscore.h"
#include "plhash.h"
#include "plstr.h"
#include "prio.h"
#include "prmem.h"
#include "prprf.h"
#include "rdf.h"

#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,   NS_RDFINMEMORYDATASOURCE_CID);

static const char kURINC_RelatedLinksRoot[] = "NC:RelatedLinks";

nsString              *RelatedLinksHandlerImpl::mRLServerURL = nsnull;
PRInt32               RelatedLinksHandlerImpl::gRefCnt = 0;
nsIRDFService    *RelatedLinksHandlerImpl::gRDFService;
nsIRDFResource        *RelatedLinksHandlerImpl::kNC_RelatedLinksRoot;
nsIRDFResource        *RelatedLinksHandlerImpl::kNC_Child;
nsIRDFResource        *RelatedLinksHandlerImpl::kRDF_type;
nsIRDFResource        *RelatedLinksHandlerImpl::kNC_RelatedLinksTopic;


////////////////////////////////////////////////////////////////////////
// RelatedLinksStreamListener
//
//   Until Netcenter produces valid RDF/XML, we'll use this kludge to
//   parse the crap they send us.
//

class RelatedLinksStreamListener : public nsIStreamListener
{
private:
	nsCOMPtr<nsIRDFDataSource> 	mDataSource;
	nsVoidArray			mParentArray;

	// pseudo-constants
	static PRInt32			gRefCnt;
	static nsIRDFService		*gRDFService;
	static nsIRDFResource		*kNC_Child;
	static nsIRDFResource		*kNC_Name;
	static nsIRDFResource		*kNC_URL;
	static nsIRDFResource		*kNC_loading;
	static nsIRDFResource		*kNC_RelatedLinksRoot;
	static nsIRDFResource		*kNC_BookmarkSeparator;
	static nsIRDFResource		*kNC_RelatedLinksTopic;
	static nsIRDFResource		*kRDF_type;
	static nsCOMPtr<nsIUnicodeDecoder>	mUnicodeDecoder;

	nsAutoString			mBuffer;

public:

	 NS_DECL_ISUPPORTS

			RelatedLinksStreamListener(nsIRDFDataSource *ds);
	 virtual	~RelatedLinksStreamListener();

	 NS_METHOD	Init();
     nsresult   Unescape(nsString &text);

	// nsIRequestObserver
	NS_DECL_NSIREQUESTOBSERVER

	// nsIStreamListener
	NS_DECL_NSISTREAMLISTENER
};



PRInt32			RelatedLinksStreamListener::gRefCnt;
nsIRDFService		*RelatedLinksStreamListener::gRDFService;

nsIRDFResource		*RelatedLinksStreamListener::kNC_Child;
nsIRDFResource		*RelatedLinksStreamListener::kNC_Name;
nsIRDFResource		*RelatedLinksStreamListener::kNC_URL;
nsIRDFResource		*RelatedLinksStreamListener::kNC_loading;
nsIRDFResource		*RelatedLinksStreamListener::kNC_RelatedLinksRoot;
nsIRDFResource		*RelatedLinksStreamListener::kNC_BookmarkSeparator;
nsIRDFResource		*RelatedLinksStreamListener::kNC_RelatedLinksTopic;
nsIRDFResource		*RelatedLinksStreamListener::kRDF_type;
nsCOMPtr<nsIUnicodeDecoder>	RelatedLinksStreamListener::mUnicodeDecoder;


////////////////////////////////////////////////////////////////////////



nsresult
NS_NewRelatedLinksStreamListener(nsIRDFDataSource* aDataSource,
				 nsIStreamListener** aResult)
{
	 RelatedLinksStreamListener* result =
		 new RelatedLinksStreamListener(aDataSource);

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



RelatedLinksStreamListener::RelatedLinksStreamListener(nsIRDFDataSource *aDataSource)
	 : mDataSource(aDataSource)
{
}



RelatedLinksStreamListener::~RelatedLinksStreamListener()
{
	 if (--gRefCnt == 0)
	 {
		 NS_IF_RELEASE(kNC_Child);
		 NS_IF_RELEASE(kNC_Name);
		 NS_IF_RELEASE(kNC_URL);
		 NS_IF_RELEASE(kNC_loading);
		 NS_IF_RELEASE(kNC_BookmarkSeparator);
		 NS_IF_RELEASE(kNC_RelatedLinksTopic);
		 NS_IF_RELEASE(kRDF_type);
		 NS_IF_RELEASE(kNC_RelatedLinksRoot);
		 mUnicodeDecoder = nsnull;

		 NS_IF_RELEASE(gRDFService);
	 }
}



NS_METHOD
RelatedLinksStreamListener::Init()
{
	if (gRefCnt++ == 0)
	{
		nsresult rv = CallGetService(kRDFServiceCID, &gRDFService);
		if (NS_FAILED(rv))
    {
      NS_ERROR("unable to get RDF service");
			return(rv);
    }

		nsICharsetConverterManager *charsetConv;
		rv = CallGetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &charsetConv);
		if (NS_SUCCEEDED(rv))
		{
			rv = charsetConv->GetUnicodeDecoderRaw("UTF-8",
												getter_AddRefs(mUnicodeDecoder));
			NS_RELEASE(charsetConv);
		}

		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "child"),
                             &kNC_Child);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Name"),
                             &kNC_Name);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "URL"),
                             &kNC_URL);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "loading"),
                             &kNC_loading);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "BookmarkSeparator"),
                             &kNC_BookmarkSeparator);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "RelatedLinksTopic"),
                             &kNC_RelatedLinksTopic);
		gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"),
                             &kRDF_type);
		gRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_RelatedLinksRoot),
                             &kNC_RelatedLinksRoot);
	 }

	 mParentArray.AppendElement(kNC_RelatedLinksRoot);
	 return(NS_OK);
}



// nsISupports interface
NS_IMPL_ISUPPORTS1(RelatedLinksStreamListener, nsIStreamListener)



// stream observer methods



NS_IMETHODIMP
RelatedLinksStreamListener::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
	 nsIRDFLiteral		*literal = nsnull;
	 nsresult		rv;
	 if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(NS_LITERAL_STRING("true").get(), &literal)))
	 {
		 mDataSource->Assert(kNC_RelatedLinksRoot, kNC_loading, literal, PR_TRUE);
		 NS_RELEASE(literal);
	 }
	 return(NS_OK);
}



NS_IMETHODIMP
RelatedLinksStreamListener::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                          nsresult status)
{
	 nsIRDFLiteral		*literal = nsnull;
	 nsresult		rv;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(NS_LITERAL_STRING("true").get(), &literal)))
	{
		mDataSource->Unassert(kNC_RelatedLinksRoot, kNC_loading, literal);
		NS_RELEASE(literal);
	}
	return(NS_OK);
}



// stream listener methods



NS_IMETHODIMP
RelatedLinksStreamListener::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
		nsIInputStream *aIStream, PRUint32 sourceOffset, PRUint32 aLength)
{
	nsresult	rv = NS_OK;

	if (aLength < 1)	return(rv);

	PRUint32	count;
	char		*buffer = new char[ aLength ];
	if (!buffer)	return(NS_ERROR_OUT_OF_MEMORY);

	if (NS_FAILED(rv = aIStream->Read(buffer, aLength, &count)) || count == 0)
	{
#ifdef	DEBUG
		printf("Related Links datasource read failure.\n");
#endif
		delete []buffer;
		return(rv);
	}
	if (count != aLength)
	{
#ifdef	DEBUG
		printf("Related Links datasource read # of bytes failure.\n");
#endif
		delete []buffer;
		return(NS_ERROR_UNEXPECTED);
	}

	if (mUnicodeDecoder)
	{
		char			*aBuffer = buffer;
		PRInt32			unicharBufLen = 0;
		mUnicodeDecoder->GetMaxLength(aBuffer, aLength, &unicharBufLen);
		PRUnichar		*unichars = new PRUnichar [ unicharBufLen+1 ];
		do
		{
			PRInt32		srcLength = aLength;
			PRInt32		unicharLength = unicharBufLen;
			rv = mUnicodeDecoder->Convert(aBuffer, &srcLength, unichars, &unicharLength);
			unichars[unicharLength]=0;  //add this since the unicode converters can't be trusted to do so.

			// Move the nsParser.cpp 00 -> space hack to here so it won't break UCS2 file

			// Hack Start
			for(PRInt32 i=0;i<unicharLength;i++)
				if(0x0000 == unichars[i])	unichars[i] = 0x0020;
			// Hack End

			mBuffer.Append(unichars, unicharLength);
			// if we failed, we consume one byte by replace it with U+FFFD
			// and try conversion again.
			if(NS_FAILED(rv))
			{
				mUnicodeDecoder->Reset();
				mBuffer.Append( (PRUnichar)0xFFFD);
				if(((PRUint32) (srcLength + 1)) > aLength)
					srcLength = aLength;
				else 
					srcLength++;
				aBuffer += srcLength;
				aLength -= srcLength;
			}
		} while (NS_FAILED(rv) && (aLength > 0));
		delete [] unichars;
		unichars = nsnull;
	}
	else
	{
		mBuffer.AppendWithConversion(buffer, aLength);
	}
	delete [] buffer;
	buffer = nsnull;

	// parse out any available lines
	while (PR_TRUE)
	{
		PRInt32 eol = mBuffer.FindCharInSet("\r\n");
		if (eol < 0)
		{
			break;
		}

		nsAutoString	oneLiner;
		if (eol >= 0)
		{
			mBuffer.Left(oneLiner, eol);
			mBuffer.Cut(0, eol+1);
		}
		if (oneLiner.IsEmpty())	break;

#if 0
		printf("RL: '%s'\n", NS_LossyConvertUTF16toASCII(oneLiner).get());
#endif

		// yes, very primitive RDF parsing follows

		nsAutoString	child, title;

		child.Truncate();
		title.Truncate();

		// get href
		PRInt32 theStart = oneLiner.Find("<child href=\"", PR_TRUE);
		if (theStart == 0)
		{
			// get child href
			theStart += PL_strlen("<child href=\"");
			oneLiner.Cut(0, theStart);
			PRInt32 theEnd = oneLiner.FindChar('"');
			if (theEnd > 0)
			{
				oneLiner.Mid(child, 0, theEnd);
			}
			// get child name
			theStart = oneLiner.Find("name=\"", PR_TRUE);
			if (theStart >= 0)
			{
				theStart += PL_strlen("name=\"");
				oneLiner.Cut(0, theStart);
				theEnd = oneLiner.FindChar('"');
				if (theEnd > 0)
				{
					oneLiner.Mid(title, 0, theEnd);
				}
			}
		}
		// check for separator
		else if ((theStart = oneLiner.Find("<child instanceOf=\"Separator1\"/>", PR_TRUE)) == 0)
		{
			nsCOMPtr<nsIRDFResource>	newSeparator;
			if (NS_SUCCEEDED(rv = gRDFService->GetAnonymousResource(getter_AddRefs(newSeparator))))
			{
				mDataSource->Assert(newSeparator, kRDF_type, kNC_BookmarkSeparator, PR_TRUE);

				nsIRDFResource	*parent = kNC_RelatedLinksRoot;
				PRInt32		numParents = mParentArray.Count();
				if (numParents > 0)
				{
					parent = (nsIRDFResource *)(mParentArray.ElementAt(numParents - 1));
				}
				mDataSource->Assert(parent, kNC_Child, newSeparator, PR_TRUE);
			}
		}
		else
		{
			theStart = oneLiner.Find("<Topic name=\"", PR_TRUE);
			if (theStart == 0)
			{
				// get topic name
				theStart += PL_strlen("<Topic name=\"");
				oneLiner.Cut(0, theStart);
				PRInt32 theEnd = oneLiner.FindChar('"');
				if (theEnd > 0)
				{
					oneLiner.Mid(title, 0, theEnd);
				}

				nsCOMPtr<nsIRDFResource>	newTopic;
				if (NS_SUCCEEDED(rv = gRDFService->GetAnonymousResource(getter_AddRefs(newTopic))))
				{
					mDataSource->Assert(newTopic, kRDF_type, kNC_RelatedLinksTopic, PR_TRUE);
					if (!title.IsEmpty())
					{
                        Unescape(title);

						const PRUnichar		*titleName = title.get();
						if (nsnull != titleName)
						{
							nsCOMPtr<nsIRDFLiteral> nameLiteral;
							if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(titleName, getter_AddRefs(nameLiteral))))
							{
								mDataSource->Assert(newTopic, kNC_Name, nameLiteral, PR_TRUE);
							}
						}
					}
					mParentArray.AppendElement(newTopic);
				}

			}
			else
			{
				theStart = oneLiner.Find("</Topic>", PR_TRUE);
				if (theStart == 0)
				{
					PRInt32		numParents = mParentArray.Count();
					if (numParents > 0)
					{
						nsIRDFResource	*aChild = (nsIRDFResource *)(mParentArray.ElementAt(numParents - 1));
						mParentArray.RemoveElementAt(numParents - 1);
						nsIRDFResource	*aParent = kNC_RelatedLinksRoot;
						if (numParents > 1)
						{
							aParent = (nsIRDFResource *)(mParentArray.ElementAt(numParents - 2));
						}
						mDataSource->Assert(aParent, kNC_Child, aChild, PR_TRUE);
					}
				}
			}
		}

		if (!child.IsEmpty())
		{

#if 0
			printf("RL: '%s'  -  '%s'\n", NS_LossyConvertUTF16toASCII(title).get(), NS_LossyConvertUTF16toASCII(child).get());
#endif
			const PRUnichar	*url = child.get();
			if (nsnull != url)
			{
				nsCOMPtr<nsIRDFResource>	relatedLinksChild;
				rv = gRDFService->GetAnonymousResource(getter_AddRefs(relatedLinksChild));
				if (NS_SUCCEEDED(rv))
				{
					title.Trim(" ");
					if (!title.IsEmpty())
					{
                        Unescape(title);

						const PRUnichar	*name = title.get();
						if (nsnull != name)
						{
							nsCOMPtr<nsIRDFLiteral>	nameLiteral;
							if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(name, getter_AddRefs(nameLiteral))))
							{
								mDataSource->Assert(relatedLinksChild, kNC_Name, nameLiteral, PR_TRUE);
							}
						}
					}

					// all related links are anonymous, so save off the "#URL" attribute
					nsCOMPtr<nsIRDFLiteral>	urlLiteral;
					if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(url, getter_AddRefs(urlLiteral))))
					{
						mDataSource->Assert(relatedLinksChild, kNC_URL, urlLiteral, PR_TRUE);
					}

					nsIRDFResource	*parent;
					PRInt32	numParents = mParentArray.Count();
					if (numParents > 0)
					{
						parent = (nsIRDFResource *)(mParentArray.ElementAt(numParents - 1));
					}
					else
					{
						parent = kNC_RelatedLinksRoot;
					}
					mDataSource->Assert(parent, kNC_Child, relatedLinksChild, PR_TRUE);
				}
			}
		}
	}
	return(rv);
}



nsresult
RelatedLinksStreamListener::Unescape(nsString &text)
{
	// convert some HTML-escaped (such as "&lt;") values back

	PRInt32		offset=0;

	while((offset = text.FindChar((PRUnichar('&')), offset)) >= 0)
	{
		if (Substring(text, offset, 4).LowerCaseEqualsLiteral("&lt;"))
		{
			text.Cut(offset, 4);
			text.Insert(PRUnichar('<'), offset);
		}
		else if (Substring(text, offset, 4).LowerCaseEqualsLiteral("&gt;"))
		{
			text.Cut(offset, 4);
			text.Insert(PRUnichar('>'), offset);
		}
		else if (Substring(text, offset, 5).LowerCaseEqualsLiteral("&amp;"))
		{
			text.Cut(offset, 5);
			text.Insert(PRUnichar('&'), offset);
		}
		else if (Substring(text, offset, 6).LowerCaseEqualsLiteral("&quot;"))
		{
			text.Cut(offset, 6);
			text.Insert(PRUnichar('\"'), offset);
		}

		++offset;
	}
	return(NS_OK);
}

RelatedLinksHandlerImpl::RelatedLinksHandlerImpl()
	: mRelatedLinksURL(nsnull)
{
}



RelatedLinksHandlerImpl::~RelatedLinksHandlerImpl()
{
	if (mRelatedLinksURL)
	{
		PL_strfree(mRelatedLinksURL);
		mRelatedLinksURL = nsnull;
	}

	if (--gRefCnt == 0)
	{
		delete mRLServerURL;
		mRLServerURL = nsnull;

		NS_IF_RELEASE(kNC_RelatedLinksRoot);
		NS_IF_RELEASE(kRDF_type);
		NS_IF_RELEASE(kNC_RelatedLinksTopic);
		NS_IF_RELEASE(kNC_Child);

		NS_IF_RELEASE(gRDFService);
	}
}



nsresult
RelatedLinksHandlerImpl::Init()
{
	nsresult	rv;

	if (gRefCnt++ == 0)
	{
		rv = CallGetService(kRDFServiceCID, &gRDFService);
		if (NS_FAILED(rv)) return rv;

		gRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_RelatedLinksRoot),
                             &kNC_RelatedLinksRoot);
		gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"),
                             &kRDF_type);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "RelatedLinksTopic"),
                             &kNC_RelatedLinksTopic);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "child"),
                             &kNC_Child);

		nsCOMPtr<nsIPref> prefServ(do_GetService(NS_PREF_CONTRACTID, &rv));
		mRLServerURL = new nsString();
		if (NS_SUCCEEDED(rv) && (prefServ))
		{
			char	*prefVal = nsnull;
			if (NS_SUCCEEDED(rv = prefServ->CopyCharPref("browser.related.provider",
				&prefVal)) && (prefVal))
			{
				mRLServerURL->AssignWithConversion(prefVal);
				nsCRT::free(prefVal);
				prefVal = nsnull;
			}
			else
			{
				// no preference, so fallback to a well-known URL
				mRLServerURL->AssignLiteral("http://www-rl.netscape.com/wtgn?");
			}
		}
	}

	mInner = do_CreateInstance(kRDFInMemoryDataSourceCID, &rv);
	return rv;
}


// nsISupports interface

NS_IMPL_CYCLE_COLLECTION_CLASS(RelatedLinksHandlerImpl)
NS_IMPL_CYCLE_COLLECTION_UNLINK_0(RelatedLinksHandlerImpl)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(RelatedLinksHandlerImpl)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mInner)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(RelatedLinksHandlerImpl,
                                          nsIRelatedLinksHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(RelatedLinksHandlerImpl,
                                           nsIRelatedLinksHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RelatedLinksHandlerImpl)
    NS_INTERFACE_MAP_ENTRY(nsIRelatedLinksHandler)
    NS_INTERFACE_MAP_ENTRY(nsIRDFDataSource)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRelatedLinksHandler)
NS_INTERFACE_MAP_END

// nsIRelatedLinksHandler interface

NS_IMETHODIMP
RelatedLinksHandlerImpl::GetURL(char** aURL)
{
	NS_PRECONDITION(aURL != nsnull, "null ptr");
	if (! aURL)
		return NS_ERROR_NULL_POINTER;

	if (mRelatedLinksURL)
	{
		*aURL = nsCRT::strdup(mRelatedLinksURL);
		return *aURL ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
	}
	else
	{
		*aURL = nsnull;
		return NS_OK;
	}
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::SetURL(const char* aURL)
{
	NS_PRECONDITION(aURL != nsnull, "null ptr");
	if (! aURL)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	if (mRelatedLinksURL)
		PL_strfree(mRelatedLinksURL);

	mRelatedLinksURL = PL_strdup(aURL);
	if (! mRelatedLinksURL)
		return NS_ERROR_OUT_OF_MEMORY;

	// Flush the old links. This'll force notifications to propagate, too.
	nsCOMPtr<nsIRDFPurgeableDataSource> purgeable = do_QueryInterface(mInner);
	NS_ASSERTION(purgeable, "uh oh, this datasource isn't purgeable!");
	if (! purgeable)
		return NS_ERROR_UNEXPECTED;

	rv = purgeable->Sweep();
	if (NS_FAILED(rv)) return rv;

	nsAutoString	relatedLinksQueryURL(*mRLServerURL);
	relatedLinksQueryURL.AppendWithConversion(mRelatedLinksURL);

	nsCOMPtr<nsIURI> url;
	rv = NS_NewURI(getter_AddRefs(url), relatedLinksQueryURL);

	if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIStreamListener> listener;
	rv = NS_NewRelatedLinksStreamListener(mInner, getter_AddRefs(listener));
	if (NS_FAILED(rv)) return rv;

	// XXX: Should there be a LoadGroup?
	rv = NS_OpenURI(listener, nsnull, url, nsnull);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}



// nsIRDFDataSource interface


NS_IMETHODIMP
RelatedLinksHandlerImpl::GetURI(char **aURI)
{
	NS_PRECONDITION(aURI != nsnull, "null ptr");
	if (! aURI)
		return NS_ERROR_NULL_POINTER;

	// XXX We could munge in the current URL that we're looking at I
	// suppose. Not critical because this datasource shouldn't be
	// registered with the RDF service.
	*aURI = nsCRT::strdup("rdf:related-links");
	if (! *aURI)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::GetSource(nsIRDFResource* aProperty,
				   nsIRDFNode* aTarget,
				   PRBool aTruthValue,
				   nsIRDFResource** aSource)
{
       	return mInner->GetSource(aProperty, aTarget, aTruthValue, aSource);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::GetSources(nsIRDFResource *aProperty,
				    nsIRDFNode *aTarget,
				    PRBool aTruthValue,
				    nsISimpleEnumerator **aSources)
{
       	return mInner->GetSources(aProperty, aTarget, aTruthValue, aSources);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::GetTarget(nsIRDFResource *aSource,
				   nsIRDFResource *aProperty,
				   PRBool aTruthValue,
				   nsIRDFNode **aTarget)
{
	return mInner->GetTarget(aSource, aProperty, aTruthValue, aTarget);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::GetTargets(nsIRDFResource* aSource,
				    nsIRDFResource* aProperty,
				    PRBool aTruthValue,
				    nsISimpleEnumerator** aTargets)
{
	return mInner->GetTargets(aSource, aProperty, aTruthValue, aTargets);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::Assert(nsIRDFResource *aSource,
				nsIRDFResource *aProperty,
				nsIRDFNode *aTarget,
				PRBool aTruthValue)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::Unassert(nsIRDFResource *aSource,
				  nsIRDFResource *aProperty,
				  nsIRDFNode *aTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::Change(nsIRDFResource* aSource,
								nsIRDFResource* aProperty,
								nsIRDFNode* aOldTarget,
								nsIRDFNode* aNewTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::Move(nsIRDFResource* aOldSource,
							  nsIRDFResource* aNewSource,
							  nsIRDFResource* aProperty,
							  nsIRDFNode* aTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::HasAssertion(nsIRDFResource *aSource,
				      nsIRDFResource *aProperty,
				      nsIRDFNode *aTarget,
				      PRBool aTruthValue,
				      PRBool *aResult)
{
	return mInner->HasAssertion(aSource, aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP 
RelatedLinksHandlerImpl::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
	return mInner->HasArcIn(aNode, aArc, result);
}

NS_IMETHODIMP 
RelatedLinksHandlerImpl::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
	nsresult	rv;

	PRBool	hasValueFlag = PR_FALSE;
	if (aArc == kNC_Child &&
		(aSource == kNC_RelatedLinksRoot) || 
		(NS_SUCCEEDED(rv = mInner->HasAssertion(aSource, kRDF_type,
												kNC_RelatedLinksTopic, PR_TRUE,
												&hasValueFlag))) &&
		(hasValueFlag == PR_TRUE))
	{
		*result = PR_TRUE;
	}
	else {
		*result = PR_FALSE;
	}
	return NS_OK;
}

NS_IMETHODIMP
RelatedLinksHandlerImpl::ArcLabelsIn(nsIRDFNode *aTarget,
				     nsISimpleEnumerator **aLabels)
{
	return mInner->ArcLabelsIn(aTarget, aLabels);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::ArcLabelsOut(nsIRDFResource *aSource,
				      nsISimpleEnumerator **aLabels)
{
	nsresult	rv;

	nsCOMPtr<nsISupportsArray> array;
	rv = NS_NewISupportsArray(getter_AddRefs(array));
	if (NS_FAILED(rv)) return rv;

	PRBool	hasValueFlag = PR_FALSE;
	if ((aSource == kNC_RelatedLinksRoot) || 
		(NS_SUCCEEDED(rv = mInner->HasAssertion(aSource, kRDF_type,
		kNC_RelatedLinksTopic, PR_TRUE, &hasValueFlag))) &&
		(hasValueFlag == PR_TRUE))
	{
		array->AppendElement(kNC_Child);
	}

	return NS_NewArrayEnumerator(aLabels, array);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::GetAllResources(nsISimpleEnumerator** aCursor)
{
	return mInner->GetAllResources(aCursor);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::AddObserver(nsIRDFObserver *aObserver)
{
	return mInner->AddObserver(aObserver);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::RemoveObserver(nsIRDFObserver *aObserver)
{
	return mInner->RemoveObserver(aObserver);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::GetAllCmds(nsIRDFResource* aSource,
					nsISimpleEnumerator/*<nsIRDFResource>*/** aCommands)
{
	return mInner->GetAllCmds(aSource, aCommands);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                               PRBool* aResult)
{
	return mInner->IsCommandEnabled(aSources, aCommand, aArguments, aResult);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	return mInner->DoCommand(aSources, aCommand, aArguments);
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::BeginUpdateBatch()
{
        return mInner->BeginUpdateBatch();
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::EndUpdateBatch()
{
        return mInner->EndUpdateBatch();
}
