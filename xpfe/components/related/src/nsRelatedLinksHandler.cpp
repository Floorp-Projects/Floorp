/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

  Implementation for a Related Links RDF data store.

 */

#include "nsCOMPtr.h"
#include "nsEnumeratorUtils.h"
#include "nsIGenericFactory.h"
#include "nsIInputStream.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFPurgeableDataSource.h"
#include "nsIRDFService.h"
#include "nsIRelatedLinksHandler.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nscore.h"
#include "plhash.h"
#include "plstr.h"
#include "prio.h"
#include "prmem.h"
#include "prprf.h"
#include "rdf.h"
#include "xp_core.h"
#include <ctype.h> // for toupper()
#include <stdio.h>

static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRelatedLinksHandlerCID,   NS_RELATEDLINKSHANDLER_CID);
static NS_DEFINE_CID(kComponentManagerCID,      NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID,        NS_GENERICFACTORY_CID);

static const char kURINC_RelatedLinksRoot[] = "NC:RelatedLinks";


////////////////////////////////////////////////////////////////////////
// RelatedLinksStreamListener
//
//   Until Netcenter produces valid RDF/XML, we'll use this kludge to
//   parse the crap they send us.
//

class RelatedLinksStreamListener : public nsIStreamListener
{
private:
	 nsCOMPtr<nsIRDFDataSource> mDataSource;
	 nsVoidArray		mParentArray;

    // pseudo-constants
	 static PRInt32		gRefCnt;
	 static nsIRDFService   *gRDFService;
	 static nsIRDFResource	*kNC_Child;
	 static nsIRDFResource	*kNC_Name;
	 static nsIRDFResource	*kNC_loading;
	 static nsIRDFResource	*kNC_RelatedLinksRoot;
	 static nsIRDFResource	*kNC_BookmarkSeparator;
	 static nsIRDFResource	*kRDF_type;

	 char			*mLine;

public:

	 NS_DECL_ISUPPORTS

			RelatedLinksStreamListener(nsIRDFDataSource *ds);
	 virtual	~RelatedLinksStreamListener(void);

	 NS_METHOD	Init();
	 NS_METHOD	CreateAnonymousResource(const nsString& aPrefixURI, nsCOMPtr<nsIRDFResource>* aResult);

	 // stream observer

	 NS_IMETHOD	OnStartBinding(nsIURL *aURL, const char *aContentType);
	 NS_IMETHOD	OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
	 NS_IMETHOD	OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
	 NS_IMETHOD	OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

	 // stream listener
	 NS_IMETHOD	GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo);
	 NS_IMETHOD	OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, 
				PRUint32 aLength);
};

PRInt32			RelatedLinksStreamListener::gRefCnt;
nsIRDFService		*RelatedLinksStreamListener::gRDFService;

nsIRDFResource		*RelatedLinksStreamListener::kNC_Child;
nsIRDFResource		*RelatedLinksStreamListener::kNC_Name;
nsIRDFResource		*RelatedLinksStreamListener::kNC_loading;
nsIRDFResource		*RelatedLinksStreamListener::kNC_RelatedLinksRoot;
nsIRDFResource		*RelatedLinksStreamListener::kNC_BookmarkSeparator;
nsIRDFResource		*RelatedLinksStreamListener::kRDF_type;


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
	 : mDataSource(dont_QueryInterface(aDataSource)),
	   mLine(nsnull)
{
	 NS_INIT_REFCNT();
}



RelatedLinksStreamListener::~RelatedLinksStreamListener()
{
	 if (mLine) {
		 delete [] mLine;
		 mLine = nsnull;
	 }

	 if (--gRefCnt == 0)
	 {
		 NS_RELEASE(kNC_Child);
		 NS_RELEASE(kNC_Name);
		 NS_RELEASE(kNC_loading);
		 NS_RELEASE(kNC_BookmarkSeparator);
		 NS_RELEASE(kRDF_type);
		 NS_RELEASE(kNC_RelatedLinksRoot);

		 nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
	 }
}


NS_METHOD
RelatedLinksStreamListener::Init()
{
	 if (gRefCnt++ == 0) {
		 nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
							    nsIRDFService::GetIID(),
							    (nsISupports**) &gRDFService);

		 NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
		 if (NS_FAILED(rv))
			 return rv;

		 gRDFService->GetResource(NC_NAMESPACE_URI "child", &kNC_Child);
		 gRDFService->GetResource(NC_NAMESPACE_URI "Name",  &kNC_Name);
		 gRDFService->GetResource(NC_NAMESPACE_URI "loading", &kNC_loading);
		 gRDFService->GetResource(NC_NAMESPACE_URI "BookmarkSeparator", &kNC_BookmarkSeparator);
		 gRDFService->GetResource(RDF_NAMESPACE_URI "type", &kRDF_type);
		 gRDFService->GetResource(kURINC_RelatedLinksRoot, &kNC_RelatedLinksRoot);
	 }

	 mParentArray.AppendElement(kNC_RelatedLinksRoot);
	 return NS_OK;
}


NS_METHOD
RelatedLinksStreamListener::CreateAnonymousResource(const nsString& aPrefixURI,
						     nsCOMPtr<nsIRDFResource>* aResult)
{
	 static PRInt32 gCounter;

	 nsAutoString uri(aPrefixURI);
	 uri.Append('#');
	 uri.Append(gCounter++, 10);

	 return gRDFService->GetUnicodeResource(uri.GetUnicode(), getter_AddRefs(*aResult));
}

// nsISupports interface
NS_IMPL_ISUPPORTS(RelatedLinksStreamListener, nsIStreamListener::GetIID());



// stream observer methods



NS_IMETHODIMP
RelatedLinksStreamListener::OnStartBinding(nsIURL *aURL, const char *aContentType)
{
	 nsAutoString		trueStr("true");
	 nsIRDFLiteral		*literal = nsnull;
	 nsresult		rv;
	 if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(trueStr.GetUnicode(), &literal)))
	 {
		 mDataSource->Assert(kNC_RelatedLinksRoot, kNC_loading, literal, PR_TRUE);
		 NS_RELEASE(literal);
	 }
	 return(NS_OK);
}



NS_IMETHODIMP
RelatedLinksStreamListener::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax) 
{
	 return(NS_OK);
}



NS_IMETHODIMP
RelatedLinksStreamListener::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
	 return(NS_OK);
}



NS_IMETHODIMP
RelatedLinksStreamListener::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg) 
{
	 nsAutoString		trueStr("true");
	 nsIRDFLiteral		*literal = nsnull;
	 nsresult		rv;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(trueStr.GetUnicode(), &literal)))
	{
		mDataSource->Unassert(kNC_RelatedLinksRoot, kNC_loading, literal);
		NS_RELEASE(literal);
	}
	return(NS_OK);
}



// stream listener methods



NS_IMETHODIMP
RelatedLinksStreamListener::GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo)
{
	return(NS_OK);
}



NS_IMETHODIMP
RelatedLinksStreamListener::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
{
	nsresult	rv = NS_OK;

	if (aLength > 0)
	{
		nsString	line;
		if (mLine)	line += mLine;

		char		c;
		for (PRUint32 loop=0; loop<aLength; loop++)
		{
			PRUint32	count;
			if (NS_FAILED(rv = aIStream->Read(&c, 1, &count)))
			{
				printf("Related Links datasource read failure.\n");
				break;
			}

			if (count != 1)	break;
			line += c;
		}

		while (PR_TRUE)
		{
			PRInt32 eol = line.FindCharInSet("\r\n");
			if (eol < 0)
			{
				if (mLine)	delete []mLine;
				mLine = line.ToNewCString();
				break;
			}

			nsAutoString	oneLiner("");
			if (eol > 0)
			{
				line.Left(oneLiner, eol);
			}
			line.Cut(0, eol+1);
			if (mLine)	delete []mLine;
			mLine = line.ToNewCString();
			if (oneLiner.Length() < 1)	return(rv);


//			printf("RL: '%s'\n", oneLiner.ToNewCString());


			// yes, very primitive RDF parsing follows

			nsAutoString	child(""), title("");

			// get href
			PRInt32 theStart = oneLiner.Find("<child href=\"");
			if (theStart == 0)
			{
				// get child href
				theStart += PL_strlen("<child href=\"");
				oneLiner.Cut(0, theStart);
				PRInt32 theEnd = oneLiner.Find("\"");
				if (theEnd > 0)
				{
					oneLiner.Mid(child, 0, theEnd);
				}
				// get child name
				theStart = oneLiner.Find("name=\"");
				if (theStart >= 0)
				{
					theStart += PL_strlen("name=\"");
					oneLiner.Cut(0, theStart);
					PRInt32 theEnd = oneLiner.Find("\"");
					if (theEnd > 0)
					{
						oneLiner.Mid(title, 0, theEnd);
					}
				}
			}
			// check for separator
			else if ((theStart = oneLiner.Find("<child instanceOf=\"Separator1\"/>")) == 0)
			{
				nsCOMPtr<nsIRDFResource>	newSeparator;
				nsAutoString			rlRoot(kURINC_RelatedLinksRoot);
				if (NS_SUCCEEDED(rv = CreateAnonymousResource(rlRoot, &newSeparator)))
				{
					mDataSource->Assert(newSeparator, kRDF_type, kNC_BookmarkSeparator, PR_TRUE);

					PRInt32		numParents = mParentArray.Count();
					if (numParents > 0)
					{
						nsIRDFResource	*parent = (nsIRDFResource *)(mParentArray.ElementAt(numParents - 1));
						mDataSource->Assert(parent, kNC_Child, newSeparator, PR_TRUE);
					}
				}
			}
			else
			{
				theStart = oneLiner.Find("<Topic name=\"");
				if (theStart == 0)
				{
					// get topic name
					theStart += PL_strlen("<Topic name=\"");
					oneLiner.Cut(0, theStart);
					PRInt32 theEnd = oneLiner.Find("\"");
					if (theEnd > 0)
					{
						oneLiner.Mid(title, 0, theEnd);
					}

					nsCOMPtr<nsIRDFResource>	newTopic;
					nsAutoString			rlRoot(kURINC_RelatedLinksRoot);
					if (NS_SUCCEEDED(rv = CreateAnonymousResource(rlRoot, &newTopic)))
					{
						if (title.Length() > 0)
						{
							char *name = title.ToNewCString();
							if (nsnull != name)
							{
								nsAutoString	theName(name);
								nsCOMPtr<nsIRDFLiteral> nameLiteral;
								if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(theName.GetUnicode(), getter_AddRefs(nameLiteral))))
								{
									mDataSource->Assert(newTopic, kNC_Name, nameLiteral, PR_TRUE);
								}
#ifdef	DEBUG
								for (PRInt32 i=0; i<mParentArray.Count(); i++)	printf("    ");
								printf("Topic: '%s'\n", name);
#endif
								delete [] name;
							}
						}
						PRInt32		numParents = mParentArray.Count();
						if (numParents > 0)
						{
							nsIRDFResource	*parent = (nsIRDFResource *)(mParentArray.ElementAt(numParents - 1));
							mDataSource->Assert(parent, kNC_Child, newTopic, PR_TRUE);
						}
						mParentArray.AppendElement(newTopic);
					}

				}
				else
				{
					theStart = oneLiner.Find("</Topic>");
					if (theStart == 0)
					{
						PRInt32		numParents = mParentArray.Count();
						if (numParents > 0)
						{
							mParentArray.RemoveElementAt(numParents - 1);
						}
#ifdef	DEBUG
						for (PRInt32 i=0; i<mParentArray.Count(); i++)	printf("    ");
						printf("Topic ends.\n");
#endif
					}
				}
			}

			if (child.Length() > 0)
			{
				char	*url = child.ToNewCString();
				if (nsnull != url)
				{
					nsCOMPtr<nsIRDFResource>	relatedLinksChild;
					PRInt32		numParents = mParentArray.Count();
					if (numParents > 1)
					{
						nsAutoString	rlRoot(kURINC_RelatedLinksRoot);
						rv = CreateAnonymousResource(rlRoot, &relatedLinksChild);
					}
					else
					{
						rv = gRDFService->GetResource(url, getter_AddRefs(relatedLinksChild));
					}
					if (NS_SUCCEEDED(rv))
					{					
						if (title.Length() > 0)
						{
							char *name = title.ToNewCString();
							if (nsnull != name)
							{
								nsAutoString		theName(name);
								nsCOMPtr<nsIRDFLiteral>	nameLiteral;
								if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(theName.GetUnicode(), getter_AddRefs(nameLiteral))))
								{
									mDataSource->Assert(relatedLinksChild, kNC_Name, nameLiteral, PR_TRUE);
								}
#ifdef	DEBUG
								for (PRInt32 i=0; i<mParentArray.Count(); i++)	printf("    ");
								printf("Name: '%s'  URL: '%s'\n", name, url);
#endif
								delete [] name;
							}
						}
						if (numParents > 0)
						{
							nsIRDFResource	*parent = (nsIRDFResource *)(mParentArray.ElementAt(numParents - 1));
							mDataSource->Assert(parent, kNC_Child, relatedLinksChild, PR_TRUE);
						}
					}
					delete []url;
				}
			}
		}
	}
	return(rv);
}


////////////////////////////////////////////////////////////////////////
// RelatedLinksHanlderImpl

class RelatedLinksHandlerImpl : public nsIRelatedLinksHandler,
				public nsIRDFDataSource
{
private:
	char			*mURI;
	char			*mRelatedLinksURL;
	PRBool			mPerformQuery;

   // pseudo-constants
	static PRInt32		gRefCnt;
	static nsIRDFService    *gRDFService;
	static nsIRDFResource	*kNC_RelatedLinksRoot;
	static nsIRDFResource	*kNC_Child;
	static nsIRDFResource	*kNC_Name;
	static nsIRDFResource	*kRDF_type;

	nsCOMPtr<nsIRDFDataSource> mInner;

			RelatedLinksHandlerImpl(void);
	virtual		~RelatedLinksHandlerImpl(void);

	friend NS_IMETHODIMP
	NS_NewRelatedLinksHandler(nsISupports* aOuter, REFNSIID aIID, void** aResult);

public:

	NS_DECL_ISUPPORTS

	// nsIRelatedLinksHandler methods
	NS_IMETHOD GetURL(char * *aURL);
	NS_IMETHOD SetURL(char * aURL);

	// nsIRDFDataSource methods

	NS_IMETHOD	Init(const char *uri);
	NS_IMETHOD	GetURI(char **uri);
	NS_IMETHOD	GetSource(nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv,
				nsIRDFResource **source /* out */);
	NS_IMETHOD	GetSources(nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv,
				nsISimpleEnumerator **sources /* out */);
	NS_IMETHOD	GetTarget(nsIRDFResource *source,
				nsIRDFResource *property,
				PRBool tv,
				nsIRDFNode **target /* out */);
	NS_IMETHOD	GetTargets(nsIRDFResource *source,
				nsIRDFResource *property,
				PRBool tv,
				nsISimpleEnumerator **targets /* out */);
	NS_IMETHOD	Assert(nsIRDFResource *source,
				nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv);
	NS_IMETHOD	Unassert(nsIRDFResource *source,
				nsIRDFResource *property,
				nsIRDFNode *target);
	NS_IMETHOD	HasAssertion(nsIRDFResource *source,
				nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv,
				PRBool *hasAssertion /* out */);
	NS_IMETHOD	ArcLabelsIn(nsIRDFNode *node,
				nsISimpleEnumerator **labels /* out */);
	NS_IMETHOD	ArcLabelsOut(nsIRDFResource *source,
				nsISimpleEnumerator **labels /* out */);
	NS_IMETHOD	GetAllResources(nsISimpleEnumerator** aCursor);
	NS_IMETHOD	AddObserver(nsIRDFObserver *n);
	NS_IMETHOD	RemoveObserver(nsIRDFObserver *n);
	NS_IMETHOD	Flush();
	NS_IMETHOD	GetAllCommands(nsIRDFResource* source,
				nsIEnumerator/*<nsIRDFResource>*/** commands);
	NS_IMETHOD	IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments,
               PRBool* aResult);
	NS_IMETHOD	DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments);
};



PRInt32			RelatedLinksHandlerImpl::gRefCnt;
nsIRDFService		*RelatedLinksHandlerImpl::gRDFService;

nsIRDFResource		*RelatedLinksHandlerImpl::kNC_RelatedLinksRoot;
nsIRDFResource		*RelatedLinksHandlerImpl::kNC_Child;
nsIRDFResource		*RelatedLinksHandlerImpl::kNC_Name;
nsIRDFResource		*RelatedLinksHandlerImpl::kRDF_type;




RelatedLinksHandlerImpl::RelatedLinksHandlerImpl(void)
	: mURI(nsnull),
	  mRelatedLinksURL(nsnull),
	  mPerformQuery(PR_FALSE)
{
	NS_INIT_REFCNT();
}



RelatedLinksHandlerImpl::~RelatedLinksHandlerImpl (void)
{
	if (mRelatedLinksURL)
	{
		PL_strfree(mRelatedLinksURL);
		mRelatedLinksURL = nsnull;
	}

	if (--gRefCnt == 0)
	{
		NS_RELEASE(kNC_RelatedLinksRoot);
		NS_RELEASE(kNC_Child);
		NS_RELEASE(kNC_Name);
		NS_RELEASE(kRDF_type);

		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
		gRDFService = nsnull;
	}
}



NS_IMETHODIMP
NS_NewRelatedLinksHandler(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
	NS_PRECONDITION(aResult != nsnull, "null ptr");
	if (! aResult)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(aOuter == nsnull, "no aggregation");
	if (aOuter)
		return NS_ERROR_NO_AGGREGATION;

	nsresult rv = NS_OK;

	RelatedLinksHandlerImpl* result = new RelatedLinksHandlerImpl();
	if (! result)
		return NS_ERROR_OUT_OF_MEMORY;

	rv = result->Init("rdf:relatedlinks");
	if (NS_SUCCEEDED(rv)) {
		rv = result->QueryInterface(aIID, aResult);
	}

	if (NS_FAILED(rv)) {
		delete result;
		*aResult = nsnull;
		return rv;
	}

	return rv;
}


// nsISupports interface

NS_IMPL_ADDREF(RelatedLinksHandlerImpl);
NS_IMPL_RELEASE(RelatedLinksHandlerImpl);


NS_IMETHODIMP
RelatedLinksHandlerImpl::QueryInterface(REFNSIID aIID, void** aResult)
{
	NS_PRECONDITION(aResult != nsnull, "null ptr");
	if (! aResult)
		return NS_ERROR_NULL_POINTER;

	static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

	if (aIID.Equals(nsIRelatedLinksHandler::GetIID()) ||
	    aIID.Equals(kISupportsIID))
	{
		*aResult = NS_STATIC_CAST(nsIRelatedLinksHandler*, this);
	}
	else if (aIID.Equals(nsIRDFDataSource::GetIID())) {
		*aResult = NS_STATIC_CAST(nsIRDFDataSource*, this);
	}
	else
	{
		*aResult = nsnull;
		return NS_NOINTERFACE;
	}

	// If we get here, we know the QI succeeded
	NS_ADDREF(this);
	return NS_OK;
}


// nsIRelatedLinksHandler interface

NS_IMETHODIMP
RelatedLinksHandlerImpl::GetURL(char** aURL)
{
	NS_PRECONDITION(aURL != nsnull, "null ptr");
	if (! aURL)
		return NS_ERROR_NULL_POINTER;

	if (mRelatedLinksURL) {
		*aURL = nsXPIDLCString::Copy(mRelatedLinksURL);
		return *aURL ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
	}
	else {
		*aURL = nsnull;
		return NS_OK;
	}
}


NS_IMETHODIMP
RelatedLinksHandlerImpl::SetURL(char* aURL)
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

	// Flush the old links. This'll force notifications to propogate, too.
	nsCOMPtr<nsIRDFPurgeableDataSource> purgeable = do_QueryInterface(mInner);
	NS_ASSERTION(purgeable, "uh oh, this datasource isn't purgeable!");
	if (! purgeable)
		return NS_ERROR_UNEXPECTED;

	rv = purgeable->Sweep();
	if (NS_FAILED(rv)) return rv;

	// XXX This should be a parameter
	nsAutoString	relatedLinksQueryURL("http://www-rl.netscape.com/wtgn?");
	relatedLinksQueryURL += mRelatedLinksURL;

	char *queryURL = relatedLinksQueryURL.ToNewCString();
	if (! queryURL)
		return NS_ERROR_OUT_OF_MEMORY;

	nsCOMPtr<nsIURL> url;
	rv = NS_NewURL(getter_AddRefs(url), (const char*) queryURL);

	delete[] queryURL;

	if (NS_FAILED(rv)) return rv;

	// XXX When is this destroyed?
	nsCOMPtr<nsIStreamListener> listener;
	rv = NS_NewRelatedLinksStreamListener(mInner, getter_AddRefs(listener));
	if (NS_FAILED(rv)) return rv;

	rv = NS_OpenURL(url, listener);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}


// nsIRDFDataSource interface

NS_IMETHODIMP
RelatedLinksHandlerImpl::Init(const char *aURI)
{
	NS_PRECONDITION(aURI != nsnull, "null ptr");
	if (! aURI)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	if (gRefCnt++ == 0)
	{
		rv = nsServiceManager::GetService(kRDFServiceCID,
                                                  nsIRDFService::GetIID(),
                                                  (nsISupports**) &gRDFService);
		if (NS_FAILED(rv)) return rv;

		gRDFService->GetResource(kURINC_RelatedLinksRoot, &kNC_RelatedLinksRoot);
		gRDFService->GetResource(NC_NAMESPACE_URI  "child", &kNC_Child);
		gRDFService->GetResource(NC_NAMESPACE_URI  "Name", &kNC_Name);
		gRDFService->GetResource(RDF_NAMESPACE_URI "type", &kRDF_type);
	}

	rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
						nsnull,
						nsIRDFDataSource::GetIID(),
						getter_AddRefs(mInner));
	if (NS_FAILED(rv)) return rv;

	rv = mInner->Init(mURI);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::GetURI(char **aURI)
{
	return mInner->GetURI(aURI);
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
RelatedLinksHandlerImpl::HasAssertion(nsIRDFResource *aSource,
				      nsIRDFResource *aProperty,
				      nsIRDFNode *aTarget,
				      PRBool aTruthValue,
				      PRBool *aResult)
{
	return mInner->HasAssertion(aSource, aProperty, aTarget, aTruthValue, aResult);
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
	return mInner->ArcLabelsOut(aSource, aLabels);
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
RelatedLinksHandlerImpl::Flush()
{
	return mInner->Flush();
}



NS_IMETHODIMP
RelatedLinksHandlerImpl::GetAllCommands(nsIRDFResource* aSource,
					nsIEnumerator/*<nsIRDFResource>*/** aCommands)
{
	return mInner->GetAllCommands(aSource, aCommands);
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

	if (aClass.Equals(kRelatedLinksHandlerCID)) {
		constructor = NS_NewRelatedLinksHandler;
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

	rv = compMgr->RegisterComponent(kRelatedLinksHandlerCID, "Related Links Handler",
					NS_RELATEDLINKSHANDLER_PROGID,
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

	rv = compMgr->UnregisterComponent(kRelatedLinksHandlerCID, aPath);

	return NS_OK;
}


