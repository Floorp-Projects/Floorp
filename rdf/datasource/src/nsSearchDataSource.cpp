/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-file-style: "stroustrup" -*-
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
  Implementation for an internet search RDF data store.
 */

#include <ctype.h> // for toupper()
#include <stdio.h>
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIEnumerator.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsXPIDLString.h"
#include "nsRDFCID.h"
#include "rdfutil.h"
#include "nsIRDFService.h"
#include "xp_core.h"
#include "plhash.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "prio.h"
#include "rdf.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"
#include "nsEnumeratorUtils.h"

#include "nsIRDFRemoteDataSource.h"

#include "nsEscape.h"

#include "nsIURL.h"
#ifdef NECKO
#include "nsNeckoUtil.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIHTTPChannel.h"
#include "nsHTTPEnums.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#else
#include "nsIPostToServer.h"
#endif // NECKO
#include "nsIBuffer.h"
#include "nsIInputStream.h"
#include "nsIBufferInputStream.h"
#include "nsIStreamListener.h"
#include "nsIRDFSearch.h"

#ifdef	XP_MAC
#include "Files.h"
#endif

#ifdef	XP_WIN
#include "windef.h"
#include "winbase.h"
#endif


#define	POSTHEADER_PREFIX	"Content-type: application/x-www-form-urlencoded; charset=ISO-8859-1\r\nContent-Length: "
#define	POSTHEADER_SUFFIX	"\r\n\r\n"


static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,    NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);

static const char kURINC_SearchRoot[] = "NC:SearchEngineRoot";
static const char kURINC_SearchResultsSitesRoot[] = "NC:SearchResultsSitesRoot";
static const char kURINC_LastSearchRoot[] = "NC:LastSearchRoot";
static const char kURINC_SearchResultsAnonymous[] = "NC:SearchResultsAnonymous";



class SearchDataSourceCallback : public nsIStreamListener
{
private:
	nsIRDFDataSource	*mDataSource;
	nsIRDFResource		*mParent;
	nsIRDFResource		*mEngine;
	static PRInt32		gRefCnt;

    // pseudo-constants    
	static nsIRDFResource	*kNC_LastSearchRoot;
	static nsIRDFResource	*kNC_loading;
	static nsIRDFResource	*kNC_Child;
	static nsIRDFResource	*kNC_URL;
	static nsIRDFResource	*kNC_Name;
	static nsIRDFResource	*kNC_Data;
	static nsIRDFResource	*kNC_Relevance;
	static nsIRDFResource	*kNC_RelevanceSort;
	static nsIRDFResource	*kNC_Site;
	static nsIRDFResource	*kNC_Engine;
	static nsIRDFResource	*kNC_HTML;

	char			*mLine;

	nsresult	CreateAnonymousResource(nsCOMPtr<nsIRDFResource>* aResult);

public:

	NS_DECL_ISUPPORTS

			SearchDataSourceCallback(nsIRDFDataSource *ds, nsIRDFResource *parent, nsIRDFResource *engine);
	virtual		~SearchDataSourceCallback(void);

#ifdef NECKO
    // nsIStreamObserver methods:
    NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports *ctxt);
    NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg);
    // nsIStreamListener methods:
    NS_IMETHOD OnDataAvailable(nsIChannel* channel, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count);
#else
	// stream observer

	NS_IMETHOD	OnStartRequest(nsIURI *aURL, const char *aContentType);
	NS_IMETHOD	OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
	NS_IMETHOD	OnStatus(nsIURI* aURL, const PRUnichar* aMsg);
	NS_IMETHOD	OnStopRequest(nsIURI* aURL, nsresult aStatus, const PRUnichar* aMsg);

	// stream listener
	NS_IMETHOD	GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo);
	NS_IMETHOD	OnDataAvailable(nsIURI* aURL, nsIInputStream *aIStream, 
                               PRUint32 aLength);
#endif
};



class SearchDataSource : public nsIRDFSearchDataSource
{
private:
	static PRInt32		gRefCnt;

    // pseudo-constants
	static nsIRDFResource		*kNC_SearchRoot;
	static nsIRDFResource		*kNC_LastSearchRoot;
	static nsIRDFResource		*kNC_SearchResultsSitesRoot;
	static nsIRDFResource		*kNC_Ref;
	static nsIRDFResource		*kNC_Child;
	static nsIRDFResource		*kNC_Data;
	static nsIRDFResource		*kNC_Name;
	static nsIRDFResource		*kNC_URL;
	static nsIRDFResource		*kRDF_InstanceOf;
	static nsIRDFResource		*kRDF_type;

protected:
	static nsIRDFDataSource		*mInner;

public:

friend	class SearchDataSourceCallback;

	NS_DECL_ISUPPORTS

				SearchDataSource(void);
	virtual		~SearchDataSource(void);
	nsresult	Init();

	// nsIRDFDataSource methods

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
	NS_IMETHOD	Change(nsIRDFResource* aSource,
				nsIRDFResource* aProperty,
				nsIRDFNode* aOldTarget,
				nsIRDFNode* aNewTarget);
	NS_IMETHOD	Move(nsIRDFResource* aOldSource,
				nsIRDFResource* aNewSource,
				nsIRDFResource* aProperty,
				nsIRDFNode* aTarget);
	NS_IMETHOD	HasAssertion(nsIRDFResource *source,
				nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv,
				PRBool *hasAssertion /* out */);
	NS_IMETHOD	ArcLabelsIn(nsIRDFNode *node,
				nsISimpleEnumerator **labels /* out */);
	NS_IMETHOD	ArcLabelsOut(nsIRDFResource *source,
				nsISimpleEnumerator **labels /* out */);
	NS_IMETHOD	GetAllResources(nsISimpleEnumerator** aResult);
	NS_IMETHOD	AddObserver(nsIRDFObserver *n);
	NS_IMETHOD	RemoveObserver(nsIRDFObserver *n);
	NS_IMETHOD	GetAllCommands(nsIRDFResource* source,
				nsIEnumerator/*<nsIRDFResource>*/** commands);
	NS_IMETHOD	GetAllCmds(nsIRDFResource* source,
				nsISimpleEnumerator/*<nsIRDFResource>*/** commands);

	NS_IMETHOD	IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments,
				PRBool* aResult);

	NS_IMETHOD	DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments);


	// helper methods
static PRBool		isEngineURI(nsIRDFResource* aResource);
static PRBool		isSearchURI(nsIRDFResource* aResource);
static nsresult		BeginSearchRequest(nsIRDFResource *source, PRBool doNetworkRequest);
static nsresult		DoSearch(nsIRDFResource *source, nsIRDFResource *engine, nsString text);
static nsresult		GetSearchEngineList(nsFileSpec spec);
static nsresult		GetSearchFolder(nsFileSpec &spec);
static nsresult		ReadFileContents(nsFileSpec baseFilename, nsString & sourceContents);
static nsresult		GetData(nsString data, char *sectionToFind, char *attribToFind, nsString &value);
static nsresult		GetInputs(nsString data, nsString text, nsString &input);
static nsresult		GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult);
static PRBool		isVisible(const nsNativeFileSpec& file);

};



static	nsIRDFService		*gRDFService = nsnull;
static	SearchDataSource	*gSearchDataSource = nsnull;

PRInt32				SearchDataSource::gRefCnt;
nsIRDFDataSource		*SearchDataSource::mInner = nsnull;

nsIRDFResource			*SearchDataSource::kNC_SearchRoot;
nsIRDFResource			*SearchDataSource::kNC_LastSearchRoot;
nsIRDFResource			*SearchDataSource::kNC_SearchResultsSitesRoot;
nsIRDFResource			*SearchDataSource::kNC_Ref;
nsIRDFResource			*SearchDataSource::kNC_Child;
nsIRDFResource			*SearchDataSource::kNC_Data;
nsIRDFResource			*SearchDataSource::kNC_Name;
nsIRDFResource			*SearchDataSource::kNC_URL;
nsIRDFResource			*SearchDataSource::kRDF_InstanceOf;
nsIRDFResource			*SearchDataSource::kRDF_type;

PRInt32				SearchDataSourceCallback::gRefCnt;

nsIRDFResource			*SearchDataSourceCallback::kNC_LastSearchRoot;
nsIRDFResource			*SearchDataSourceCallback::kNC_Child;
nsIRDFResource			*SearchDataSourceCallback::kNC_URL;
nsIRDFResource			*SearchDataSourceCallback::kNC_Name;
nsIRDFResource			*SearchDataSourceCallback::kNC_Data;
nsIRDFResource			*SearchDataSourceCallback::kNC_Relevance;
nsIRDFResource			*SearchDataSourceCallback::kNC_RelevanceSort;
nsIRDFResource			*SearchDataSourceCallback::kNC_Site;
nsIRDFResource			*SearchDataSourceCallback::kNC_Engine;
nsIRDFResource			*SearchDataSourceCallback::kNC_loading;
nsIRDFResource			*SearchDataSourceCallback::kNC_HTML;

static const char		kEngineProtocol[] = "engine://";
static const char		kSearchProtocol[] = "internetsearch:";



PRBool
SearchDataSource::isEngineURI(nsIRDFResource *r)
{
	PRBool		isEngineURIFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kEngineProtocol, sizeof(kEngineProtocol) - 1)))
	{
		isEngineURIFlag = PR_TRUE;
	}
	return(isEngineURIFlag);
}



PRBool
SearchDataSource::isSearchURI(nsIRDFResource *r)
{
	PRBool		isSearchURIFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kSearchProtocol, sizeof(kSearchProtocol) - 1)))
	{
		isSearchURIFlag = PR_TRUE;
	}
	return(isSearchURIFlag);
}



SearchDataSource::SearchDataSource(void)
{
	NS_INIT_REFCNT();

	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
			nsIRDFService::GetIID(), (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(kURINC_SearchRoot,                   &kNC_SearchRoot);
		gRDFService->GetResource(kURINC_LastSearchRoot,               &kNC_LastSearchRoot);
		gRDFService->GetResource(kURINC_SearchResultsSitesRoot,       &kNC_SearchResultsSitesRoot);
		gRDFService->GetResource(NC_NAMESPACE_URI "ref",              &kNC_Ref);
		gRDFService->GetResource(NC_NAMESPACE_URI "child",            &kNC_Child);
		gRDFService->GetResource(NC_NAMESPACE_URI "data",             &kNC_Data);
		gRDFService->GetResource(NC_NAMESPACE_URI "Name",             &kNC_Name);
		gRDFService->GetResource(NC_NAMESPACE_URI "URL",              &kNC_URL);

		gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf",      &kRDF_InstanceOf);
		gRDFService->GetResource(RDF_NAMESPACE_URI "type",            &kRDF_type);

		gSearchDataSource = this;
	}
}



SearchDataSource::~SearchDataSource (void)
{
	if (--gRefCnt == 0)
	{
		NS_RELEASE(kNC_SearchRoot);
		NS_RELEASE(kNC_LastSearchRoot);
		NS_RELEASE(kNC_SearchResultsSitesRoot);
		NS_RELEASE(kNC_Ref);
		NS_RELEASE(kNC_Child);
		NS_RELEASE(kNC_Data);
		NS_RELEASE(kNC_Name);
		NS_RELEASE(kNC_URL);
		NS_RELEASE(kRDF_InstanceOf);
		NS_RELEASE(kRDF_type);

		NS_IF_RELEASE(mInner);

		gSearchDataSource = nsnull;
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
		gRDFService = nsnull;
	}
}



NS_IMPL_ISUPPORTS(SearchDataSource, nsIRDFDataSource::GetIID());



nsresult
SearchDataSource::Init()
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
		nsnull, nsIRDFDataSource::GetIID(), (void **)&mInner)))
		return(rv);

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this, PR_FALSE)))
		return(rv);

	// get available search engines
	nsFileSpec			nativeDir;
	if (NS_SUCCEEDED(rv = GetSearchFolder(nativeDir)))
	{
		rv = GetSearchEngineList(nativeDir);
	}
	return(rv);
}



NS_IMETHODIMP
SearchDataSource::GetURI(char **uri)
{
	NS_PRECONDITION(uri != nsnull, "null ptr");
	if (! uri)
		return NS_ERROR_NULL_POINTER;

	if ((*uri = nsXPIDLCString::Copy("rdf:search")) == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}



NS_IMETHODIMP
SearchDataSource::GetSource(nsIRDFResource* property,
                                nsIRDFNode* target,
                                PRBool tv,
                                nsIRDFResource** source /* out */)
{
	NS_PRECONDITION(property != nsnull, "null ptr");
	if (! property)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(target != nsnull, "null ptr");
	if (! target)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	*source = nsnull;
	return NS_RDF_NO_VALUE;
}



NS_IMETHODIMP
SearchDataSource::GetSources(nsIRDFResource *property,
                                 nsIRDFNode *target,
                                 PRBool tv,
                                 nsISimpleEnumerator **sources /* out */)
{
	NS_NOTYETIMPLEMENTED("write me");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
SearchDataSource::GetTarget(nsIRDFResource *source,
                                nsIRDFResource *property,
                                PRBool tv,
                                nsIRDFNode **target /* out */)
{
	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(property != nsnull, "null ptr");
	if (! property)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(target != nsnull, "null ptr");
	if (! target)
		return NS_ERROR_NULL_POINTER;

	nsresult		rv = NS_RDF_NO_VALUE;

	// we only have positive assertions in the internet search data source.
	if (! tv)
		return(rv);
	
	if (mInner)
	{
		rv = mInner->GetTarget(source, property, tv, target);
	}
	return(rv);
}



NS_IMETHODIMP
SearchDataSource::GetTargets(nsIRDFResource *source,
                           nsIRDFResource *property,
                           PRBool tv,
                           nsISimpleEnumerator **targets /* out */)
{
	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(property != nsnull, "null ptr");
	if (! property)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(targets != nsnull, "null ptr");
	if (! targets)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_RDF_NO_VALUE;

	// we only have positive assertions in the internet search data source.
	if (! tv)
		return(rv);

	if (mInner)
	{
		rv = mInner->GetTargets(source, property, tv, targets);
	}
	if (isSearchURI(source))
	{
		if (property == kNC_Child)
		{
			PRBool		doNetworkRequest = PR_TRUE;
			if (NS_SUCCEEDED(rv) && (targets))
			{
				// check and see if we already have data for the search in question;
				// if we do, BeginSearchRequest() won't bother doing the search again,
				// otherwise it will kickstart it
				PRBool		hasResults = PR_FALSE;
				if (NS_SUCCEEDED((*targets)->HasMoreElements(&hasResults)) && (hasResults == PR_TRUE))
				{
					doNetworkRequest = PR_FALSE;
				}
			}
			BeginSearchRequest(source, doNetworkRequest);
		}
	}
	return(rv);

#if 0
	if ((source == kNC_SearchRoot) || (source == kNC_LastSearchRoot))
	{
		if (property == kNC_Child)
		{
			if (mInner)
			{
				rv = mInner->GetTargets(source, property,tv, targets);
			}
			return(rv);
		}
		return(NS_RDF_NO_VALUE);
	}
	else if (isSearchURI(source))
	{
		if (property == kNC_Child)
		{
			if (mInner)
			{
				rv = mInner->GetTargets(source, property, tv, targets);
			}
			PRBool		doNetworkRequest = PR_TRUE;
			if (NS_SUCCEEDED(rv) && (targets))
			{
				// check and see if we already have data for the search in question;
				// if we do, don't bother doing the search again, otherwise kickstart it
				PRBool		hasResults = PR_FALSE;
				if (NS_SUCCEEDED((*targets)->HasMoreElements(&hasResults)) && (hasResults == PR_TRUE))
				{
					doNetworkRequest = PR_FALSE;
				}
			}
			BeginSearchRequest(source, doNetworkRequest);
//			return(rv);
		}
		return(NS_RDF_NO_VALUE);
	}
	if (mInner)
	{
		rv = mInner->GetTargets(source, property, tv, targets);
	}
	return NS_NewEmptyEnumerator(targets);
#endif

}



NS_IMETHODIMP
SearchDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
SearchDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
SearchDataSource::Change(nsIRDFResource* aSource,
			nsIRDFResource* aProperty,
			nsIRDFNode* aOldTarget,
			nsIRDFNode* aNewTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
SearchDataSource::Move(nsIRDFResource* aOldSource,
					   nsIRDFResource* aNewSource,
					   nsIRDFResource* aProperty,
					   nsIRDFNode* aTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
SearchDataSource::HasAssertion(nsIRDFResource *source,
                             nsIRDFResource *property,
                             nsIRDFNode *target,
                             PRBool tv,
                             PRBool *hasAssertion /* out */)
{
	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(property != nsnull, "null ptr");
	if (! property)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(target != nsnull, "null ptr");
	if (! target)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(hasAssertion != nsnull, "null ptr");
	if (! hasAssertion)
		return NS_ERROR_NULL_POINTER;

	// we only have positive assertions in the internet search data source.
	if (! tv)
	{
		*hasAssertion = PR_FALSE;
		return NS_OK;
        }
        nsresult	rv = NS_RDF_NO_VALUE;
        
        if (mInner)
        {
		rv = mInner->HasAssertion(source, property, target, tv, hasAssertion);
	}
        return(rv);
}



NS_IMETHODIMP
SearchDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
	NS_NOTYETIMPLEMENTED("write me");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
SearchDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsISimpleEnumerator **labels /* out */)
{
	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(labels != nsnull, "null ptr");
	if (! labels)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	if ((source == kNC_SearchRoot) || (source == kNC_LastSearchRoot) || isSearchURI(source))
	{
            nsCOMPtr<nsISupportsArray> array;
            rv = NS_NewISupportsArray(getter_AddRefs(array));
            if (NS_FAILED(rv)) return rv;

            array->AppendElement(kNC_Child);

            nsISimpleEnumerator* result = new nsArrayEnumerator(array);
            if (! result)
                return NS_ERROR_OUT_OF_MEMORY;

            NS_ADDREF(result);
            *labels = result;
            return(NS_OK);
	}
	
	if (mInner)
	{
		rv = mInner->ArcLabelsOut(source, labels);
		return(rv);
	}

	return NS_NewEmptyEnumerator(labels);
}



NS_IMETHODIMP
SearchDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	nsresult	rv = NS_RDF_NO_VALUE;

	if (mInner)
	{
		rv = mInner->GetAllResources(aCursor);
	}
	return(rv);
}



NS_IMETHODIMP
SearchDataSource::AddObserver(nsIRDFObserver *aObserver)
{
	nsresult	rv = NS_OK;

	if (mInner)
	{
		rv = mInner->AddObserver(aObserver);
	}
	return(rv);
}



NS_IMETHODIMP
SearchDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
	nsresult	rv = NS_OK;

	if (mInner)
	{
		rv = mInner->RemoveObserver(aObserver);
	}
	return(rv);
}



NS_IMETHODIMP
SearchDataSource::GetAllCommands(nsIRDFResource* source,
                                     nsIEnumerator/*<nsIRDFResource>*/** commands)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
SearchDataSource::GetAllCmds(nsIRDFResource* source,
                                     nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	return(NS_NewEmptyEnumerator(commands));
}



NS_IMETHODIMP
SearchDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                       nsIRDFResource*   aCommand,
                                       nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                       PRBool* aResult)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
SearchDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



nsresult
NS_NewRDFSearchDataSource(nsIRDFDataSource **result)
{
	if (!result)
		return NS_ERROR_NULL_POINTER;

	// only one search data source
	if (nsnull == gSearchDataSource)
	{
		if ((gSearchDataSource = new SearchDataSource()) == nsnull)
		{
			return NS_ERROR_OUT_OF_MEMORY;
		}

		nsresult rv = gSearchDataSource->Init();
		if (NS_FAILED(rv)) {
			delete gSearchDataSource;
			gSearchDataSource = nsnull;
			return rv;
		}
	}
	NS_ADDREF(gSearchDataSource);
	*result = gSearchDataSource;
	return NS_OK;
}



nsresult
SearchDataSource::BeginSearchRequest(nsIRDFResource *source, PRBool doNetworkRequest)
{
        nsresult		rv = NS_OK;
	char			*sourceURI = nsnull;

	if (NS_FAILED(rv = source->GetValue(&sourceURI)))
		return(rv);
	nsAutoString		uri(sourceURI);
	if (uri.Find("internetsearch:") != 0)
		return(NS_ERROR_FAILURE);

	// remember the last search query

	nsCOMPtr<nsIRDFDataSource>	localstore;
	rv = gRDFService->GetDataSource("rdf:local-store", getter_AddRefs(localstore));
	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFNode>	lastTarget;
		if (NS_SUCCEEDED(rv = localstore->GetTarget(kNC_LastSearchRoot, kNC_Ref,
			PR_TRUE, getter_AddRefs(lastTarget))))
		{
			if (rv != NS_RDF_NO_VALUE)
			{
#ifdef	DEBUG
				nsCOMPtr<nsIRDFLiteral>	lastLit = do_QueryInterface(lastTarget);
				if (lastLit)
				{
					const PRUnichar	*lastUni = nsnull;
					lastLit->GetValueConst(&lastUni);
					nsAutoString	lastStr(lastUni);
					char *lastC = lastStr.ToNewCString();
					if (lastC)
					{
						printf("\nLast Search: '%s'\n", lastC);
						delete [] lastC;
						lastC = nsnull;
					}
				}
#endif
				rv = localstore->Unassert(kNC_LastSearchRoot, kNC_Ref, lastTarget);
			}
		}
		if (uri.Length() > 0)
		{
			const PRUnichar	*uriUni = uri.GetUnicode();
			nsCOMPtr<nsIRDFLiteral>	uriLiteral;
			if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(uriUni, getter_AddRefs(uriLiteral))))
			{
				rv = localstore->Assert(kNC_LastSearchRoot, kNC_Ref, uriLiteral, PR_TRUE);
			}
		}
		
		// XXX Currently, need to flush localstore as its being leaked
		// and thus never written out to disk otherwise

		// gotta love the name "remoteLocalStore"
		nsCOMPtr<nsIRDFRemoteDataSource>	remoteLocalStore = do_QueryInterface(localstore);
		if (remoteLocalStore)
		{
			remoteLocalStore->Flush();
		}
	}

	// forget about any previous search results

	if (mInner)
	{
		nsCOMPtr<nsISimpleEnumerator>	arcs;
		if (NS_SUCCEEDED(rv = mInner->GetTargets(kNC_LastSearchRoot, kNC_Child, PR_TRUE, getter_AddRefs(arcs))))
		{
			PRBool			hasMore = PR_TRUE;
			while (hasMore == PR_TRUE)
			{
				if (NS_FAILED(arcs->HasMoreElements(&hasMore)) || (hasMore == PR_FALSE))
					break;
				nsCOMPtr<nsISupports>	arc;
				if (NS_FAILED(arcs->GetNext(getter_AddRefs(arc))))
					break;
				nsCOMPtr<nsIRDFResource>	child = do_QueryInterface(arc);
				if (child)
				{
					mInner->Unassert(kNC_LastSearchRoot, kNC_Child, child);
				}
			}
		}
	}

#if 0
		nsCOMPtr<nsIRDFNode>	lastTarget;
		if (NS_SUCCEEDED(rv = mInner->GetTarget(kNC_LastSearchRoot, kNC_Ref,
			PR_TRUE, getter_AddRefs(lastTarget))))
		{
			if (rv != NS_RDF_NO_VALUE)
			{
				rv = mInner->Unassert(kNC_LastSearchRoot, kNC_Ref, lastTarget);
			}
		}
		if (uri.Length() > 0)
		{
			const PRUnichar	*uriUni = uri.GetUnicode();
			nsCOMPtr<nsIRDFLiteral>	uriLiteral;
			if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(uriUni, getter_AddRefs(uriLiteral))))
			{
				rv = mInner->Assert(kNC_LastSearchRoot, kNC_Ref, uriLiteral, PR_TRUE);
			}
		}
	}
#endif

	// forget about any previous search sites

	if (mInner)
	{
		nsCOMPtr<nsISimpleEnumerator>	arcs;
		if (NS_SUCCEEDED(rv = mInner->GetTargets(kNC_SearchResultsSitesRoot, kNC_Child, PR_TRUE, getter_AddRefs(arcs))))
		{
			PRBool			hasMore = PR_TRUE;
			while (hasMore == PR_TRUE)
			{
				if (NS_FAILED(arcs->HasMoreElements(&hasMore)) || (hasMore == PR_FALSE))
					break;
				nsCOMPtr<nsISupports>	arc;
				if (NS_FAILED(arcs->GetNext(getter_AddRefs(arc))))
					break;
				nsCOMPtr<nsIRDFResource>	child = do_QueryInterface(arc);
				if (child)
				{
					mInner->Unassert(kNC_SearchResultsSitesRoot, kNC_Child, child);
				}
			}
		}
	}

	uri.Cut(0, strlen("internetsearch:"));

	nsVoidArray	*engineArray = new nsVoidArray;
	if (!engineArray)
		return(NS_ERROR_FAILURE);

	nsAutoString	text("");

	// parse up attributes

	while(uri.Length() > 0)
	{
		nsAutoString	item("");

		PRInt32 andOffset = uri.Find("&");
		if (andOffset >= 0)
		{
			uri.Left(item, andOffset);
			uri.Cut(0, andOffset + 1);
		}
		else
		{
			item = uri;
			uri.Truncate();
		}

		PRInt32 equalOffset = item.Find("=");
		if (equalOffset < 0)	break;
		
		nsAutoString	attrib(""), value("");
		item.Left(attrib, equalOffset);
		value = item;
		value.Cut(0, equalOffset + 1);
		
		if ((attrib.Length() > 0) && (value.Length() > 0))
		{
			if (attrib.EqualsIgnoreCase("engine"))
			{
				if (value.Find(kEngineProtocol) == 0)
				{
					char	*val = value.ToNewCString();
					if (val)
					{
						engineArray->AppendElement(val);
					}
				}
			}
			else if (attrib.EqualsIgnoreCase("text"))
			{
				text = value;
			}
		}
	}

	// loop over specified search engines
	while (engineArray->Count() > 0)
	{
		char *baseFilename = (char *)(engineArray->ElementAt(0));
		engineArray->RemoveElementAt(0);
		if (!baseFilename)	continue;

#ifdef	DEBUG
		printf("Search engine to query: '%s'\n", baseFilename);
#endif

		nsCOMPtr<nsIRDFResource>	engine;
		gRDFService->GetResource(baseFilename, getter_AddRefs(engine));
		delete [] baseFilename;
		baseFilename = nsnull;
		if (!engine)	continue;

		// mark this as a search site
		if (mInner)
		{
			mInner->Assert(kNC_SearchResultsSitesRoot, kNC_Child, engine, PR_TRUE);
		}

		if (doNetworkRequest == PR_TRUE)
		{
			DoSearch(source, engine, text);
		}
	}
	
	delete engineArray;
	engineArray = nsnull;

	return(rv);
}



nsresult
SearchDataSource::DoSearch(nsIRDFResource *source, nsIRDFResource *engine, nsString text)
{
	nsresult	rv;

	if (!source)
		return(NS_ERROR_NULL_POINTER);
	if (!engine)
		return(NS_ERROR_NULL_POINTER);

	if (!mInner)
	{
		return(NS_RDF_NO_VALUE);
	}

	// get data
	nsAutoString		data("");
	nsCOMPtr<nsIRDFNode>	dataTarget = nsnull;
	if (NS_SUCCEEDED((rv = mInner->GetTarget(engine, kNC_Data, PR_TRUE, getter_AddRefs(dataTarget)))) && (dataTarget))
	{
		nsCOMPtr<nsIRDFLiteral>	dataLiteral = do_QueryInterface(dataTarget);
		if (!dataLiteral)
			return(rv);
		PRUnichar	*dataUni;
		if (NS_FAILED(rv = dataLiteral->GetValue(&dataUni)))
			return(rv);
		data = dataUni;
	}
	else
	{
		const char	*engineURI = nsnull;
		if (NS_FAILED(rv = engine->GetValueConst(&engineURI)))
			return(rv);
		nsAutoString	engineStr(engineURI);
		if (engineStr.Find(kEngineProtocol) != 0)
			return(rv);
		engineStr.Cut(0, sizeof(kEngineProtocol) - 1);
		char	*baseFilename = engineStr.ToNewCString();
		if (!baseFilename)
			return(rv);
		baseFilename = nsUnescape(baseFilename);
		if (!baseFilename)
			return(rv);

		nsFileSpec	engineSpec(baseFilename);
		rv = ReadFileContents(engineSpec, data);
//		rv = NS_ERROR_UNEXPECTED;			// XXX rjc: fix this

		delete [] baseFilename;
		baseFilename = nsnull;
		if (NS_FAILED(rv))
		{
			return(rv);
		}

		// save file contents
		nsCOMPtr<nsIRDFLiteral>	dataLiteral;
		if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(data.GetUnicode(), getter_AddRefs(dataLiteral))))
		{
			if (mInner)
			{
				mInner->Assert(engine, kNC_Data, dataLiteral, PR_TRUE);
			}
		}
	}
	if (data.Length() < 1)
		return(NS_RDF_NO_VALUE);
	
	nsAutoString	action, method, input;

	if (NS_FAILED(rv = GetData(data, "search", "action", action)))
		return(rv);
	if (NS_FAILED(rv = GetData(data, "search", "method", method)))
		return(rv);
	if (NS_FAILED(rv = GetInputs(data, text, input)))
		return(rv);

#ifdef	DEBUG
	char *cAction = action.ToNewCString();
	char *cMethod = method.ToNewCString();
	char *cInput = input.ToNewCString();
	printf("Search Action: '%s'\n", cAction);
	printf("Search Method: '%s'\n", cMethod);
	printf(" Search Input: '%s'\n\n", cInput);
	if (cAction)
	{
		delete [] cAction;
		cAction = nsnull;
	}
	if (cMethod)
	{
		delete [] cMethod;
		cMethod = nsnull;
	}
	if (cInput)
	{
		delete [] cInput;
		cInput = nsnull;
	}
#endif

	if (input.Length() < 1)
		return(NS_ERROR_UNEXPECTED);

	if (method.EqualsIgnoreCase("get"))
	{
		// HTTP Get method support
		action += "?";
		action += input;
	}

	char	*searchURL = action.ToNewCString();
	if (!searchURL)
		return(NS_ERROR_NULL_POINTER);

#ifdef	NECKO
	SearchDataSourceCallback *callback = new SearchDataSourceCallback(mInner, source, engine);
	if (nsnull != callback)
	{
		nsCOMPtr<nsIURI>	url;
		if (NS_SUCCEEDED(rv = NS_NewURI(getter_AddRefs(url), (const char*) searchURL)))
		{
			if (method.EqualsIgnoreCase("post"))
			{
				nsCOMPtr<nsIChannel>	channel;
				if (NS_SUCCEEDED(rv = NS_OpenURI(getter_AddRefs(channel), url)))
				{
					nsCOMPtr<nsIHTTPChannel>	httpChannel = do_QueryInterface(channel);
					if (httpChannel)
					{
						httpChannel->SetRequestMethod(HM_POST);

				        	// construct post data to send
				        	nsAutoString	postStr(POSTHEADER_PREFIX);
				        	postStr.Append(input.Length(), 10);
				        	postStr += POSTHEADER_SUFFIX;
				        	postStr += input;
				        	
						char	*postData = postStr.ToNewCString();
						if (postData)
						{
							nsCOMPtr<nsIInputStream>	postDataStream;
							if (NS_SUCCEEDED(rv = NS_NewPostDataStream(PR_FALSE, postData,
								0, getter_AddRefs(postDataStream))))
							{
								httpChannel->SetPostDataStream(postDataStream);
								/* postData is now owned by the postDataStream instance... ? */
								rv = channel->AsyncRead(0, -1, nsnull, callback);
							}
							else
							{
								delete []postData;
								postData = nsnull;
							}
						}
					}
				}
			}
			else if (method.EqualsIgnoreCase("get"))
			{
				rv = NS_OpenURI(NS_STATIC_CAST(nsIStreamListener *, callback), nsnull, url);
			}
		}
	}
#endif

	delete [] searchURL;
	searchURL = nsnull;
	return(rv);
}



nsresult
SearchDataSource::GetSearchFolder(nsFileSpec &spec)
{
#ifdef	XP_MAC
	// on Mac, use system's search files
	nsSpecialSystemDirectory	searchSitesDir(nsSpecialSystemDirectory::Mac_InternetSearchDirectory);
#else
	// on other platforms, use our search files
	nsSpecialSystemDirectory	searchSitesDir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
	searchSitesDir += "res";
	searchSitesDir += "rdf";
	searchSitesDir += "datasets";
#endif
	spec = searchSitesDir;
	return(NS_OK);
}



nsresult
SearchDataSource::GetSearchEngineList(nsFileSpec nativeDir)
{
        nsresult			rv = NS_OK;

	if (!mInner)
	{
		return(NS_RDF_NO_VALUE);
	}

	for (nsDirectoryIterator i(nativeDir, PR_FALSE); i.Exists(); i++)
	{
		const nsFileSpec	fileSpec = (const nsFileSpec &)i;
		if (fileSpec.IsHidden())
		{
			continue;
		}

		const char		*childURL = fileSpec;

		if (fileSpec.IsDirectory())
		{
			GetSearchEngineList(fileSpec);
		}
		else
		if (childURL != nsnull)
		{
			nsAutoString	uri(childURL);
			PRInt32		len = uri.Length();
			if (len > 4)
			{
#ifdef	XP_MAC
				// be sure to resolve aliases in case we encounter one
				CInfoPBRec	cInfo;
				OSErr		err;
//				PRBool		wasAliased = PR_FALSE;
//				fileSpec.ResolveSymlink(wasAliased);
				err = fileSpec.GetCatInfo(cInfo);
				if ((!err) && (cInfo.hFileInfo.ioFlFndrInfo.fdType == 'issp') &&
					(cInfo.hFileInfo.ioFlFndrInfo.fdCreator == 'fndf'))
#else
				// else just check the extension
				nsAutoString	extension;
				if ((uri.Right(extension, 4) == 4) && (extension.EqualsIgnoreCase(".src")))
#endif
				{
					char	c;
#ifdef	XP_WIN
					c = '\\';
#elif	XP_MAC
					c = ':';
#else
					c = '/';
#endif
					PRInt32	separatorOffset = uri.RFindChar(PRUnichar(c));
					if (separatorOffset > 0)
					{
//						uri.Cut(0, separatorOffset+1);
						
						nsAutoString	searchURL(kEngineProtocol);

						char		*uriC = uri.ToNewCString();
						if (!uriC)	continue;
						char		*uriCescaped = nsEscape(uriC, url_Path);
						delete [] uriC;
						if (!uriCescaped)	continue;
//						searchURL += uri;
						searchURL += uriCescaped;
						delete [] uriCescaped;

/*
						char *baseFilename = uri.ToNewCString();
						if (!baseFilename)	continue;
						baseFilename = nsUnescape(baseFilename);
						if (!baseFilename)	continue;
*/
						nsAutoString	data("");
						rv = ReadFileContents(fileSpec, data);
//						delete [] baseFilename;
//						baseFilename = nsnull;
						if (NS_FAILED(rv))	continue;

						nsCOMPtr<nsIRDFResource>	searchRes;
						char		*searchURI = searchURL.ToNewCString();
						if (searchURI)
						{
							if (NS_SUCCEEDED(rv = gRDFService->GetResource(searchURI, getter_AddRefs(searchRes))))
							{
								mInner->Assert(kNC_SearchRoot, kNC_Child, searchRes, PR_TRUE);

								// save name of search engine (as specified in file)
								nsAutoString	nameValue;
								if (NS_SUCCEEDED(rv = GetData(data, "search", "name", nameValue)))
								{
									nsCOMPtr<nsIRDFLiteral>	nameLiteral;
									if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(nameValue.GetUnicode(), getter_AddRefs(nameLiteral))))
									{
										mInner->Assert(searchRes, kNC_Name, nameLiteral, PR_TRUE);
									}
								}
							}
							delete [] searchURI;
							searchURI = nsnull;
						}
//						delete [] baseFilename;
//						baseFilename = nsnull;
					}
				}
			}
		}
	}
	return(rv);
}



PRBool
SearchDataSource::isVisible(const nsNativeFileSpec& file)
{
	PRBool		isVisible = PR_TRUE;

#ifdef	XP_MAC
	CInfoPBRec	cInfo;
	OSErr		err;

	nsFileSpec	fileSpec(file);
	if (!(err = fileSpec.GetCatInfo(cInfo)))
	{
		if (cInfo.hFileInfo.ioFlFndrInfo.fdFlags & kIsInvisible)
		{
			isVisible = PR_FALSE;
		}
	}
#else
	char		*baseFilename = file.GetLeafName();
	if (nsnull != baseFilename)
	{
		if ((!strcmp(baseFilename, ".")) || (!strcmp(baseFilename, "..")))
		{
			isVisible = PR_FALSE;
		}
		nsCRT::free(baseFilename);
	}
#endif

	return(isVisible);
}



nsresult
SearchDataSource::ReadFileContents(nsFileSpec baseFilename, nsString& sourceContents)
{
	nsresult			rv = NS_OK;

/*
	nsFileSpec			searchEngine;
	if (NS_FAILED(rv = GetSearchFolder(searchEngine)))
	{
		return(rv);
	}
	searchEngine += baseFilename;

#ifdef	XP_MAC
	// be sure to resolve aliases in case we encounter one
	PRBool	wasAliased = PR_FALSE;
	searchEngine.ResolveSymlink(wasAliased);
#endif
	nsInputFileStream		searchFile(searchEngine);
*/

	nsInputFileStream		searchFile(baseFilename);

/*
#ifdef	XP_MAC
	if (!searchFile.is_open())
	{
		// on Mac, nsDirectoryIterator resolves aliases before returning them currently;
		// so, if we can't open the file directly, walk the directory and see if we then
		// find a match

		// on Mac, use system's search files
		nsSpecialSystemDirectory	searchSitesDir(nsSpecialSystemDirectory::Mac_InternetSearchDirectory);
		nsFileSpec 			nativeDir(searchSitesDir);
		for (nsDirectoryIterator i(nativeDir, PR_FALSE); i.Exists(); i++)
		{
			const nsFileSpec	fileSpec = (const nsFileSpec &)i;
			const char		*childURL = fileSpec;
			if (fileSpec.isHidden())
			{
				continue;
			}
			if (childURL != nsnull)
			{
				// be sure to resolve aliases in case we encounter one
				PRBool		wasAliased = PR_FALSE;
				fileSpec.ResolveSymlink(wasAliased);
				nsAutoString	childPath(childURL);
				PRInt32		separatorOffset = childPath.RFindChar(PRUnichar(':'));
				if (separatorOffset > 0)
				{
						childPath.Cut(0, separatorOffset+1);
				}
				if (childPath.EqualsIgnoreCase(baseFilename))
				{
					searchFile = fileSpec;
				}
			}
		}
	}
#endif
*/

	if (searchFile.is_open())
	{
		nsRandomAccessInputStream	stream(searchFile);
		char				buffer[1024];
		while (!stream.eof())
		{
			stream.readline(buffer, sizeof(buffer)-1 );
			sourceContents += buffer;
			sourceContents += "\n";
		}
	}
	else
	{
		rv = NS_ERROR_FAILURE;
	}
	return(rv);
}



nsresult
SearchDataSource::GetData(nsString data, char *sectionToFind, char *attribToFind, nsString &value)
{
	nsAutoString	buffer(data);	
	nsresult	rv = NS_RDF_NO_VALUE;
	PRBool		inSection = PR_FALSE;

	while(buffer.Length() > 0)
	{
		PRInt32 eol = buffer.FindCharInSet("\r\n", 0);
		if (eol < 0)	break;
		nsAutoString	line("");
		if (eol > 0)
		{
			buffer.Left(line, eol);
		}
		buffer.Cut(0, eol+1);
		if (line.Length() < 1)	continue;		// skip empty lines
		if (line[0] == PRUnichar('#'))	continue;	// skip comments
		line = line.Trim(" \t");
		if (inSection == PR_FALSE)
		{
			nsAutoString	section("<");
			section += sectionToFind;
			PRInt32	sectionOffset = line.Find(section, PR_TRUE);
			if (sectionOffset < 0)	continue;
			line.Cut(0, sectionOffset + section.Length() + 1);
			inSection = PR_TRUE;
			
		}
		line = line.Trim(" \t");
		PRInt32	len = line.Length();
		if (len > 0)
		{
			if (line[len-1] == PRUnichar('>'))
			{
				inSection = PR_FALSE;
				line.SetLength(len-1);
			}
		}
		PRInt32 equal = line.Find("=");
		if (equal < 0)	continue;			// skip lines with no equality
		
		nsAutoString	attrib("");
		if (equal > 0)
		{
			line.Left(attrib, equal /* - 1 */);
		}
		attrib = attrib.Trim(" \t");
		if (attrib.EqualsIgnoreCase(attribToFind))
		{
			line.Cut(0, equal+1);
			value = line.Trim(" \t");

			// strip of any enclosing quotes
			if (value[0] == PRUnichar('\"'))
			{
				value.Cut(0,1);
			}
			len = value.Length();
			if (len > 0)
			{
				if (value[len-1] == PRUnichar('\"'))
				{
					value.SetLength(len-1);
				}
			}
			rv = NS_OK;
			break;
		}
	}
	return(rv);
}



nsresult
SearchDataSource::GetInputs(nsString data, nsString text, nsString &input)
{
	nsAutoString	buffer(data);	// , eolStr("\r\n");	
	nsresult	rv = NS_OK;
	PRBool		inSection = PR_FALSE;

	while(buffer.Length() > 0)
	{
		PRInt32 eol = buffer.FindCharInSet("\r\n", 0);
		if (eol < 0)	break;
		nsAutoString	line("");
		if (eol > 0)
		{
			buffer.Left(line, eol);
		}
		buffer.Cut(0, eol+1);
		if (line.Length() < 1)	continue;		// skip empty lines
		if (line[0] == PRUnichar('#'))	continue;	// skip comments
		line = line.Trim(" \t");
		if (inSection == PR_FALSE)
		{
			nsAutoString	section("<");
			PRInt32	sectionOffset = line.Find(section, PR_TRUE);
			if (sectionOffset < 0)	continue;
			if (sectionOffset == 0)
			{
				line.Cut(0, sectionOffset + section.Length());
				inSection = PR_TRUE;
			}
		}
		PRInt32	len = line.Length();
		if (len > 0)
		{
			if (line[len-1] == PRUnichar('>'))
			{
				inSection = PR_FALSE;
				line.SetLength(len-1);
			}
		}
		if (inSection == PR_TRUE)	continue;

		// look for inputs
		if (line.Find("input", PR_TRUE) == 0)
		{
			line.Cut(0, 6);
			line = line.Trim(" \t");
			
			// first look for name attribute
			nsAutoString	nameAttrib("");

			PRInt32	nameOffset = line.Find("name", PR_TRUE);
			if (nameOffset >= 0)
			{
				PRInt32 equal = line.FindChar(PRUnichar('='), PR_TRUE, nameOffset);
				if (equal >= 0)
				{
					PRInt32	startQuote = line.FindChar(PRUnichar('\"'), PR_TRUE, equal + 1);
					if (startQuote >= 0)
					{
						PRInt32	endQuote = line.FindChar(PRUnichar('\"'), PR_TRUE, startQuote + 1);
						if (endQuote > 0)
						{
							line.Mid(nameAttrib, startQuote+1, endQuote-startQuote-1);
						}
					}
					else
					{
						nameAttrib = line;
						nameAttrib.Cut(0, equal+1);
						nameAttrib = nameAttrib.Trim(" \t");
						PRInt32 space = nameAttrib.FindCharInSet(" \t", 0);
						if (space > 0)
						{
							nameAttrib.Truncate(space);
						}
					}
				}
			}
			if (nameAttrib.Length() <= 0)	continue;

			// first look for value attribute
			nsAutoString	valueAttrib("");

			PRInt32	valueOffset = line.Find("value", PR_TRUE);
			if (valueOffset >= 0)
			{
				PRInt32 equal = line.FindChar(PRUnichar('='), PR_TRUE, valueOffset);
				if (equal >= 0)
				{
					PRInt32	startQuote = line.FindChar(PRUnichar('\"'), PR_TRUE, equal + 1);
					if (startQuote >= 0)
					{
						PRInt32	endQuote = line.FindChar(PRUnichar('\"'), PR_TRUE, startQuote + 1);
						if (endQuote >= 0)
						{
							line.Mid(valueAttrib, startQuote+1, endQuote-startQuote-1);
						}
					}
					else
					{
						// if value attribute's "value" isn't quoted, get the first word... ?
						valueAttrib = line;
						valueAttrib.Cut(0, equal+1);
						valueAttrib = valueAttrib.Trim(" \t");
						PRInt32 space = valueAttrib.FindCharInSet(" \t>", 0);
						if (space > 0)
						{
							valueAttrib.Truncate(space);
						}
					}
				}
			}
			else if (line.Find("user", PR_TRUE) >= 0)
			{
				valueAttrib = text;
			}
			
			// XXX should ignore if  mode=browser  is specified
			// XXX need to do this better
			if (line.RFind("mode=browser", PR_TRUE) >= 0)
				continue;

			if ((nameAttrib.Length() > 0) && (valueAttrib.Length() > 0))
			{
				if (input.Length() > 0)
				{
					input += "&";
				}
				input += nameAttrib;
				input += "=";
				input += valueAttrib;
			}
		}
	}
	return(rv);
}



nsresult
SearchDataSource::GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult)
{
        const char	*uri = nsnull;
	source->GetValueConst( &uri );
	nsAutoString	url(uri);
	nsIRDFLiteral	*literal;
	gRDFService->GetLiteral(url.GetUnicode(), &literal);
        *aResult = literal;
        return NS_OK;
}



// Search class for Netlib callback



SearchDataSourceCallback::SearchDataSourceCallback(nsIRDFDataSource *ds, nsIRDFResource *parent, nsIRDFResource *engine)
	: mDataSource(ds),
	  mParent(parent),
	  mEngine(engine),
	  mLine(nsnull)
{
	NS_INIT_REFCNT();
	NS_IF_ADDREF(mDataSource);
	NS_IF_ADDREF(mParent);
	NS_IF_ADDREF(mEngine);

	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   nsIRDFService::GetIID(),
                                                   (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(kURINC_LastSearchRoot, &kNC_LastSearchRoot);
		gRDFService->GetResource(NC_NAMESPACE_URI "child", &kNC_Child);
		gRDFService->GetResource(NC_NAMESPACE_URI "URL", &kNC_URL);
		gRDFService->GetResource(NC_NAMESPACE_URI "Name", &kNC_Name);
		gRDFService->GetResource(NC_NAMESPACE_URI "data", &kNC_Data);
		gRDFService->GetResource(NC_NAMESPACE_URI "Relevance", &kNC_Relevance);
		gRDFService->GetResource(NC_NAMESPACE_URI "Relevance?sort=true", &kNC_RelevanceSort);
		gRDFService->GetResource(NC_NAMESPACE_URI "Site", &kNC_Site);
		gRDFService->GetResource(NC_NAMESPACE_URI "Engine", &kNC_Engine);
		gRDFService->GetResource(NC_NAMESPACE_URI "loading", &kNC_loading);
		gRDFService->GetResource(NC_NAMESPACE_URI "HTML", &kNC_HTML);
	}
}



SearchDataSourceCallback::~SearchDataSourceCallback()
{
	NS_IF_RELEASE(mDataSource);
	NS_IF_RELEASE(mParent);
	NS_IF_RELEASE(mEngine);

	if (mLine)
	{
		delete [] mLine;
		mLine = nsnull;
	}

	if (--gRefCnt == 0)
	{
		NS_RELEASE(kNC_LastSearchRoot);
		NS_RELEASE(kNC_Child);
		NS_RELEASE(kNC_URL);
		NS_RELEASE(kNC_Name);
		NS_RELEASE(kNC_Data);
		NS_RELEASE(kNC_Relevance);
		NS_RELEASE(kNC_RelevanceSort);
		NS_RELEASE(kNC_Site);
		NS_RELEASE(kNC_Engine);
		NS_RELEASE(kNC_loading);
		NS_RELEASE(kNC_HTML);
	}
}



nsresult
SearchDataSourceCallback::CreateAnonymousResource(nsCOMPtr<nsIRDFResource>* aResult)
{
	static	PRInt32		gNext = 0;

	if (! gNext)
	{
		LL_L2I(gNext, PR_Now());
	}

	nsresult		rv;
	nsAutoString		uri(kURINC_SearchResultsAnonymous);
	uri.Append("#$");
	uri.Append(++gNext, 16);

	char			*uriC = uri.ToNewCString();
	if (uriC)
	{
		rv = gRDFService->GetResource(uriC, getter_AddRefs(*aResult));
		delete [] uriC;
	}
	else
	{
		rv = NS_ERROR_NULL_POINTER;
	}
	return(rv);
}



// stream observer methods



NS_IMETHODIMP
#ifdef NECKO
SearchDataSourceCallback::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
#else
SearchDataSourceCallback::OnStartRequest(nsIURI *aURL, const char *aContentType)
#endif
{
	nsAutoString		trueStr("true");
	nsIRDFLiteral		*literal = nsnull;
	nsresult		rv;

#ifdef	DEBUG
	printf("SearchDataSourceCallback::OnStartRequest entered.\n");
#endif

	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(trueStr.GetUnicode(), &literal)))
	{
		mDataSource->Assert(mParent, kNC_loading, literal, PR_TRUE);
		mDataSource->Assert(mEngine, kNC_loading, literal, PR_TRUE);
		NS_RELEASE(literal);
	}
	return(NS_OK);
}



#ifndef NECKO
NS_IMETHODIMP
SearchDataSourceCallback::OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax) 
{
	return(NS_OK);
}



NS_IMETHODIMP
SearchDataSourceCallback::OnStatus(nsIURI* aURL, const PRUnichar* aMsg)
{
	return(NS_OK);
}
#endif



NS_IMETHODIMP
#ifdef NECKO
SearchDataSourceCallback::OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
					nsresult status, const PRUnichar *errorMsg) 
#else
SearchDataSourceCallback::OnStopRequest(nsIURI* aURL, nsresult aStatus, const PRUnichar* aMsg) 
#endif
{
	nsAutoString		trueStr("true");
	nsIRDFLiteral		*literal = nsnull;
	nsresult		rv;
#ifdef NECKO
	nsCOMPtr<nsIURI> aURL;
	rv = channel->GetURI(getter_AddRefs(aURL));
	if (NS_FAILED(rv)) return rv;
#endif

	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(trueStr.GetUnicode(), &literal)))
	{
		mDataSource->Unassert(mParent, kNC_loading, literal);
		mDataSource->Unassert(mEngine, kNC_loading, literal);
		NS_RELEASE(literal);
	}

	if (!mLine)
	{
#ifdef	DEBUG
		printf(" *** SearchDataSourceCallback::OnStopRequest:  no data.\n\n");
#endif

		return(NS_OK);
	}

#if	0
	printf("\n\n%s\n\n", mLine);
#endif


	nsAutoString	htmlResults(mLine);
	delete [] mLine;
	mLine = nsnull;

	// get data out of graph
	nsAutoString		data("");
	nsCOMPtr<nsIRDFNode>	dataNode;
	if (NS_FAILED(rv = mDataSource->GetTarget(mEngine, kNC_Data, PR_TRUE, getter_AddRefs(dataNode))))
	{
		return(rv);
	}
	nsCOMPtr<nsIRDFLiteral>	dataLiteral = do_QueryInterface(dataNode);
	if (!dataLiteral)	return(NS_ERROR_NULL_POINTER);

	PRUnichar	*dataUni = nsnull;
	if (NS_FAILED(rv = dataLiteral->GetValue(&dataUni)))
		return(rv);
	if (!dataUni)	return(NS_ERROR_NULL_POINTER);
	data = dataUni;
	if (data.Length() < 1)
		return(rv);

	nsAutoString	resultListStartStr(""), resultListEndStr("");
	nsAutoString	resultItemStartStr(""), resultItemEndStr("");
	nsAutoString	relevanceStartStr(""), relevanceEndStr("");

	SearchDataSource::GetData(data, "interpret", "resultListStart", resultListStartStr);
	SearchDataSource::GetData(data, "interpret", "resultListEnd", resultListEndStr);
	SearchDataSource::GetData(data, "interpret", "resultItemStart", resultItemStartStr);
	SearchDataSource::GetData(data, "interpret", "resultItemEnd", resultItemEndStr);
	SearchDataSource::GetData(data, "interpret", "relevanceStart", relevanceStartStr);
	SearchDataSource::GetData(data, "interpret", "relevanceEnd", relevanceEndStr);

#if 0
	char *cStr;
	cStr = resultListStartStr.ToNewCString();
	if (cStr)
	{
		printf("resultListStart: '%s'\n", cStr);
		delete [] cStr;
		cStr = nsnull;
	}
	cStr = resultListEndStr.ToNewCString();
	if (cStr)
	{
		printf("resultListEnd: '%s'\n", cStr);
		delete [] cStr;
		cStr = nsnull;
	}
	cStr = resultItemStartStr.ToNewCString();
	if (cStr)
	{
		printf("resultItemStart: '%s'\n", cStr);
		delete [] cStr;
		cStr = nsnull;
	}
	cStr = resultItemEndStr.ToNewCString();
	if (cStr)
	{
		printf("resultItemEnd: '%s'\n", cStr);
		delete [] cStr;
		cStr = nsnull;
	}
	cStr = relevanceStartStr.ToNewCString();
	if (cStr)
	{
		printf("relevanceStart: '%s'\n", cStr);
		delete [] cStr;
		cStr = nsnull;
	}
	cStr = relevanceEndStr.ToNewCString();
	if (cStr)
	{
		printf("relevanceEnd: '%s'\n", cStr);
		delete [] cStr;
		cStr = nsnull;
	}
#endif

	if (resultListStartStr.Length() > 0)
	{
		PRInt32	resultListStart = htmlResults.Find(resultListStartStr, PR_TRUE);
		if (resultListStart >= 0)
		{
			htmlResults.Cut(0, resultListStart + resultListStartStr.Length());
		}
	}
	if (resultListEndStr.Length() > 0)
	{
		PRInt32	resultListEnd = htmlResults.Find(resultListEndStr, PR_TRUE);
		if (resultListEnd >= 0)
		{
			htmlResults.Truncate(resultListEnd);
		}
	}

	PRBool	trimItemEnd = PR_TRUE;
	// if resultItemEndStr is not specified, try making it the same as resultItemStartStr
	if (resultItemEndStr.Length() < 1)
	{
		resultItemEndStr = resultItemStartStr;
		trimItemEnd = PR_FALSE;
	}

	while(PR_TRUE)
	{
		PRInt32	resultItemStart;
		resultItemStart = htmlResults.Find(resultItemStartStr, PR_TRUE);
		if (resultItemStart < 0)	break;

		htmlResults.Cut(0, resultItemStart + resultItemStartStr.Length());

		PRInt32	resultItemEnd = htmlResults.Find(resultItemEndStr, PR_TRUE );
		if (resultItemEnd < 0)
		{
			resultItemEnd = htmlResults.Length()-1;
		}

		nsAutoString	resultItem("");
		htmlResults.Left(resultItem, resultItemEnd);

		if (resultItem.Length() < 1)	break;
		if (trimItemEnd == PR_TRUE)
		{
			htmlResults.Cut(0, resultItemEnd + resultItemEndStr.Length());
		}
		else
		{
			htmlResults.Cut(0, resultItemEnd);
		}

#if 0
		char	*results = resultItem.ToNewCString();
		if (results)
		{
			printf("\n----- Search result: '%s'\n\n", results);
			delete [] results;
			results = nsnull;
		}
#endif

		// look for href
		PRInt32	hrefOffset = resultItem.Find("HREF=", PR_TRUE);
		if (hrefOffset < 0)
		{
#ifdef	DEBUG
			printf("\n***** Unable to find HREF!\n\n");
#endif
			continue;
		}

		nsAutoString	hrefStr("");
		PRInt32		quoteStartOffset = resultItem.FindCharInSet("\"\'>", hrefOffset);
		PRInt32		quoteEndOffset;
		if (quoteStartOffset < hrefOffset)
		{
			// handle case where HREF isn't quoted
			quoteStartOffset = hrefOffset + nsCRT::strlen("HREF=");
			quoteEndOffset = resultItem.FindCharInSet(">", quoteStartOffset);
			if (quoteEndOffset < quoteStartOffset)	continue;
		}
		else
		{
			if (resultItem[quoteStartOffset] == PRUnichar('>'))
			{
				// handle case where HREF isn't quoted
				quoteEndOffset = quoteStartOffset;
				quoteStartOffset = hrefOffset + nsCRT::strlen("HREF=") -1;
			}
			else
			{
				quoteEndOffset = resultItem.FindCharInSet("\"\'", quoteStartOffset + 1);
				if (quoteEndOffset < hrefOffset)	continue;
			}
		}
		resultItem.Mid(hrefStr, quoteStartOffset + 1, quoteEndOffset - quoteStartOffset - 1);
		if (hrefStr.Length() < 1)	continue;

		// check to see if this needs to be an absolute URL
		if (hrefStr[0] == PRUnichar('/'))
		{
#ifdef NECKO
			char *host = nsnull, *protocol = nsnull;
#else
			const char	*host = nsnull, *protocol = nsnull;
#endif
			aURL->GetHost(&host);
#ifdef NECKO
			aURL->GetScheme(&protocol);
#else
			aURL->GetProtocol(&protocol);
#endif
			if (host && protocol)
			{
				nsAutoString	temp;
				temp += protocol;
				temp += "://";
				temp += host;
				temp += hrefStr;

				hrefStr = temp;
			}
#ifdef NECKO
			if (host) nsCRT::free(host);
			if (protocol) nsCRT::free(protocol);
#endif
		}
		
		char	*href = hrefStr.ToNewCString();
		if (!href)	continue;

		nsAutoString	site(href);

#if 0
		printf("HREF: '%s'\n", href);
#endif

		nsCOMPtr<nsIRDFResource>	res;

// #define	OLDWAY
#ifdef	OLDWAY
		rv = gRDFService->GetResource(href, getter_AddRefs(res));
#else
		const char			*parentURI = nsnull;
		mParent->GetValueConst(&parentURI);
		if (!parentURI)	break;
		
		// save HREF attribute as URL (crap)
		if (NS_SUCCEEDED(rv = CreateAnonymousResource(&res)))
		{
			const PRUnichar	*hrefUni = hrefStr.GetUnicode();
			if (hrefUni)
			{
				nsCOMPtr<nsIRDFLiteral>	hrefLiteral;
				if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(hrefUni, getter_AddRefs(hrefLiteral))))
				{
					mDataSource->Assert(res, kNC_URL, hrefLiteral, PR_TRUE);
				}
			}
		}
#endif

		delete [] href;
		href = nsnull;
		if (NS_FAILED(rv))	continue;
		
		// set HTML chunk
		const PRUnichar	*htmlUni = resultItem.GetUnicode();
		if (htmlUni)
		{
			nsCOMPtr<nsIRDFLiteral>	htmlLiteral;
			if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(htmlUni, getter_AddRefs(htmlLiteral))))
			{
				if (htmlLiteral)
				{
					mDataSource->Assert(res, kNC_HTML, htmlLiteral, PR_TRUE);
				}
			}
		}

		// look for Site (if it isn't already set)
		nsCOMPtr<nsIRDFNode>		oldSiteRes = nsnull;
		mDataSource->GetTarget(res, kNC_Site, PR_TRUE, getter_AddRefs(oldSiteRes));
		if (!oldSiteRes)
		{
			PRInt32	protocolOffset = site.FindCharInSet(":", 0);
			if (protocolOffset >= 0)
			{
				site.Cut(0, protocolOffset+1);
				while (site[0] == PRUnichar('/'))
				{
					site.Cut(0, 1);
				}
				PRInt32	slashOffset = site.FindCharInSet("/", 0);
				if (slashOffset >= 0)
				{
					site.Truncate(slashOffset);
				}
				if (site.Length() > 0)
				{
					const PRUnichar	*siteUni = site.GetUnicode();
					if (siteUni)
					{
						nsCOMPtr<nsIRDFLiteral>	siteLiteral;
						if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(siteUni, getter_AddRefs(siteLiteral))))
						{
							if (siteLiteral)
							{
								mDataSource->Assert(res, kNC_Site, siteLiteral, PR_TRUE);
							}
						}
					}
				}
			}
		}

		// look for name
		PRInt32	anchorEnd = resultItem.FindCharInSet(">", quoteEndOffset);
		if (anchorEnd < quoteEndOffset)
		{
#ifdef	DEBUG
			printf("\n\nSearch: Unable to find ending > when computing name.\n\n");
#endif
			continue;
		}
//		PRInt32	anchorStop = resultItem.FindChar(PRUnichar('<'), PR_TRUE, quoteEndOffset);
		PRInt32	anchorStop = resultItem.Find("</A>", PR_TRUE, quoteEndOffset);
		if (anchorStop < anchorEnd)
		{
#ifdef	DEBUG
			printf("\n\nSearch: Unable to find </A> tag to compute name.\n\n");
#endif
			continue;
		}
		
		nsAutoString	nameStr;
		resultItem.Mid(nameStr, anchorEnd + 1, anchorStop - anchorEnd - 1);

		// munge any "&quot;" in name
		PRInt32	quotOffset;
		while ((quotOffset = nameStr.Find("&quot;", PR_TRUE)) >= 0)
		{
			nameStr.Cut(quotOffset, strlen("&quot;"));
			nameStr.Insert(PRUnichar('\"'), quotOffset);
		}
		
		// munge any "&amp;" in name
		PRInt32	ampOffset;
		while ((ampOffset = nameStr.Find("&amp;", PR_TRUE)) >= 0)
		{
			nameStr.Cut(ampOffset, strlen("&amp;"));
			nameStr.Insert(PRUnichar('&'), ampOffset);
		}
		
		// munge any "&nbsp;" in name
		PRInt32	nbspOffset;
		while ((nbspOffset = nameStr.Find("&nbsp;", PR_TRUE)) >= 0)
		{
			nameStr.Cut(nbspOffset, strlen("&nbsp;"));
			nameStr.Insert(PRUnichar(' '), nbspOffset);
		}

		// munge any "&lt;" in name
		PRInt32	ltOffset;
		while ((ltOffset = nameStr.Find("&lt;", PR_TRUE)) >= 0)
		{
			nameStr.Cut(ltOffset, strlen("&lt;"));
			nameStr.Insert(PRUnichar('<'), ltOffset);
		}

		// munge any "&gt;" in name
		PRInt32	gtOffset;
		while ((gtOffset = nameStr.Find("&gt;", PR_TRUE)) >= 0)
		{
			nameStr.Cut(gtOffset, strlen("&gt;"));
			nameStr.Insert(PRUnichar('>'), gtOffset);
		}
		
		// munge out anything inside of HTML "<" / ">" tags
		PRInt32 tagStartOffset;
		while ((tagStartOffset = nameStr.FindCharInSet("<", 0)) >= 0)
		{
			PRInt32	tagEndOffset = nameStr.FindCharInSet(">", tagStartOffset);
			if (tagEndOffset <= tagStartOffset)	break;
			nameStr.Cut(tagStartOffset, tagEndOffset - tagStartOffset + 1);
		}

		// cut out any CRs or LFs
		PRInt32	eolOffset;
		while ((eolOffset = nameStr.FindCharInSet("\n\r", 0)) >= 0)
		{
			nameStr.Cut(eolOffset, 1);
		}
		// and trim name
		nameStr = nameStr.Trim(" \t");

		// look for Name (if it isn't already set)
		nsCOMPtr<nsIRDFNode>		oldNameRes = nsnull;
		mDataSource->GetTarget(res, kNC_Name, PR_TRUE, getter_AddRefs(oldNameRes));
		if (!oldNameRes)
		{
			if (nameStr.Length() > 0)
			{
				const PRUnichar	*nameUni = nameStr.GetUnicode();
				if (nameUni)
				{
					nsCOMPtr<nsIRDFLiteral>	nameLiteral;
					if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(nameUni, getter_AddRefs(nameLiteral))))
					{
						if (nameLiteral)
						{
							mDataSource->Assert(res, kNC_Name, nameLiteral, PR_TRUE);
						}
					}
				}
			}
		}

		// look for relevance
		nsAutoString	relItem("-");
		PRInt32		relStart;
		if ((relStart = resultItem.Find(relevanceStartStr, PR_TRUE)) >= 0)
		{
			PRInt32	relEnd = resultItem.Find(relevanceEndStr, PR_TRUE);
			if (relEnd > relStart)
			{
				resultItem.Mid(relItem, relStart + relevanceStartStr.Length(),
					relEnd - relStart - relevanceStartStr.Length());
			}
		}

		// look for Relevance (if it isn't already set)
		nsCOMPtr<nsIRDFNode>		oldRelRes = nsnull;
		mDataSource->GetTarget(res, kNC_Relevance, PR_TRUE, getter_AddRefs(oldRelRes));
		if (!oldRelRes)
		{
			if (relItem.Length() > 0)
			{
				// save real relevance
				const PRUnichar	*relUni = relItem.GetUnicode();
				if (relUni)
				{
					nsCOMPtr<nsIRDFLiteral>	relLiteral;
					if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(relUni, getter_AddRefs(relLiteral))))
					{
						if (relLiteral)
						{
							mDataSource->Assert(res, kNC_Relevance, relLiteral, PR_TRUE);
						}
					}
				}

				// If its a percentage, remove "%"
				if (relItem[relItem.Length()-1] == PRUnichar('%'))
				{
					relItem.Cut(relItem.Length()-1, 1);
				}

				// left-pad with "0"s and set special sorting value
				nsAutoString	zero("000");
				if (relItem.Length() < 3)
				{
					relItem.Insert(zero, 0, 3-relItem.Length()); 
				}

				const PRUnichar	*relSortUni = relItem.GetUnicode();
				if (relSortUni)
				{
					nsCOMPtr<nsIRDFLiteral>	relSortLiteral;
					if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(relSortUni, getter_AddRefs(relSortLiteral))))
					{
						if (relSortLiteral)
						{
							mDataSource->Assert(res, kNC_RelevanceSort, relSortLiteral, PR_TRUE);
						}
					}
				}
			}
		}

		// set reference to engine this came from (if it isn't already set)
		nsCOMPtr<nsIRDFNode>		oldEngineRes = nsnull;
		mDataSource->GetTarget(res, kNC_Engine, PR_TRUE, getter_AddRefs(oldEngineRes));
		if (!oldEngineRes)
		{
			nsAutoString	engineStr;
			if (NS_SUCCEEDED(rv = SearchDataSource::GetData(data, "search", "name", engineStr)))
			{
				const PRUnichar		*engineUni = engineStr.GetUnicode();
				nsCOMPtr<nsIRDFLiteral>	engineLiteral;
				if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(engineUni, getter_AddRefs(engineLiteral))))
				{
					if (engineLiteral)
					{
						mDataSource->Assert(res, kNC_Engine, engineLiteral, PR_TRUE);
					}
				}
			}
		}
#ifdef	OLDWAY
		// Note: always add in parent-child relationship last!  (if it isn't already set)
		PRBool		parentHasChildFlag = PR_FALSE;
		mDataSource->HasAssertion(mParent, kNC_Child, res, PR_TRUE, &parentHasChildFlag);
		if (parentHasChildFlag == PR_FALSE)
#endif
		{
			rv = mDataSource->Assert(mParent, kNC_Child, res, PR_TRUE);
		}

		// Persist this under kNC_LastSearchRoot
		if (mDataSource)
		{
			rv = mDataSource->Assert(kNC_LastSearchRoot, kNC_Child, res, PR_TRUE);
		}

	}
	return(NS_OK);
}



// stream listener methods



NS_IMPL_ISUPPORTS(SearchDataSourceCallback, nsIRDFSearchDataSourceCallback::GetIID());



#ifndef NECKO
NS_IMETHODIMP
SearchDataSourceCallback::GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo)
{
	return(NS_OK);
}
#endif



NS_IMETHODIMP
#ifdef NECKO
SearchDataSourceCallback::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt, nsIInputStream *aIStream,
					  PRUint32 sourceOffset, PRUint32 aLength)
#else
SearchDataSourceCallback::OnDataAvailable(nsIURI* aURL, nsIInputStream *aIStream, PRUint32 aLength)
#endif
{
	nsresult	rv = NS_OK;

	if (aLength > 0)
	{
		nsAutoString	line;
		if (mLine)
		{
			line += mLine;
			delete	[]mLine;
			mLine = nsnull;
		}

		char	buffer[257];
		while (aLength > 0)
		{
			PRUint32	count=0, numBytes = (aLength > sizeof(buffer)-1 ? sizeof(buffer)-1 : aLength);
			if (NS_FAILED(rv = aIStream->Read(buffer, numBytes, &count)))
			{
#ifdef	DEBUG			
				printf("Search datasource read failure.\n");
#endif
				break;
			}
			if (numBytes != count)
			{
#ifdef	DEBUG
				printf("Search datasource read # of bytes failure.\n");
#endif
				break;
			}
			buffer[count] = '\0';
			line += buffer;
			aLength -= count;
		}
		mLine = line.ToNewCString();
	}
	return(rv);
}
