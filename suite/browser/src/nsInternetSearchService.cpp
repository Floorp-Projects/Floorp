/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-file-style: "stroustrup" -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
//#include "rdfutil.h"
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
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIHTTPChannel.h"
#include "nsHTTPEnums.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsISearchService.h"

#ifdef	XP_MAC
#include "Files.h"
#endif

#ifdef	XP_WIN
#include "windef.h"
#include "winbase.h"
#endif

#ifdef	DEBUG
// #define	DEBUG_SEARCH_OUTPUT	1
#endif



#define	POSTHEADER_PREFIX	"Content-type: application/x-www-form-urlencoded; charset=ISO-8859-1\r\nContent-Length: "
#define	POSTHEADER_SUFFIX	"\r\n\r\n"



static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,    NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,         NS_RDFXMLDATASOURCE_CID);

static const char kURINC_SearchEngineRoot[]           = "NC:SearchEngineRoot";
static const char kURINC_SearchResultsSitesRoot[]     = "NC:SearchResultsSitesRoot";
static const char kURINC_LastSearchRoot[]             = "NC:LastSearchRoot";
static const char kURINC_SearchCategoryRoot[]         = "NC:SearchCategoryRoot";
static const char kURINC_SearchCategoryPrefix[]       = "NC:SearchCategory?category=";
static const char kURINC_SearchCategoryEnginePrefix[] = "NC:SearchCategory?engine=";
static const char kURINC_SearchResultsAnonymous[]     = "NC:SearchResultsAnonymous";



class	InternetSearchContext : public nsIInternetSearchContext
{
private:
	nsCOMPtr<nsIRDFResource>	mParent;
	nsCOMPtr<nsIRDFResource>	mEngine;
	nsAutoString			mBuffer;

public:
			InternetSearchContext(nsIRDFResource *aParent, nsIRDFResource *aEngine);
	virtual		~InternetSearchContext(void);
	NS_METHOD	Init();

	NS_DECL_ISUPPORTS
	NS_DECL_NSIINTERNETSEARCHCONTEXT
};



InternetSearchContext::~InternetSearchContext(void)
{
}



InternetSearchContext::InternetSearchContext(nsIRDFResource *aParent, nsIRDFResource *aEngine)
	: mParent(aParent), mEngine(aEngine)
{
	NS_INIT_ISUPPORTS();
}



NS_IMETHODIMP
InternetSearchContext::Init()
{
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::GetEngine(nsIRDFResource **node)
{
	*node = mEngine;
	NS_IF_ADDREF(*node);
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::GetParent(nsIRDFResource **node)
{
	*node = mParent;
	NS_IF_ADDREF(*node);
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::AppendBytes(const char *buffer, PRInt32 numBytes)
{
	mBuffer.Append(buffer, numBytes);
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::GetBuffer(PRUnichar **buffer)
{
	*buffer = NS_CONST_CAST(PRUnichar *, mBuffer.GetUnicode());
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::Truncate()
{
	mBuffer.Truncate();
	return(NS_OK);
}



NS_IMPL_ISUPPORTS(InternetSearchContext, nsIInternetSearchContext::GetIID());



nsresult
NS_NewInternetSearchContext(nsIRDFResource *aParent, nsIRDFResource *aEngine,
				nsIInternetSearchContext **aResult)
{
	 InternetSearchContext *result =
		 new InternetSearchContext(aParent, aEngine);

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



class InternetSearchDataSource : public nsIInternetSearchService,
				 public nsIRDFDataSource, nsIStreamListener
{
private:
	static PRInt32			gRefCnt;
	PRBool				mEngineListBuilt;
	nsCOMPtr<nsISupportsArray>	mConnections;

    // pseudo-constants
	static nsIRDFResource		*kNC_SearchEngineRoot;
	static nsIRDFResource		*kNC_LastSearchRoot;
	static nsIRDFResource		*kNC_SearchCategoryRoot;
	static nsIRDFResource		*kNC_SearchResultsSitesRoot;
	static nsIRDFResource		*kNC_Ref;
	static nsIRDFResource		*kNC_Child;
	static nsIRDFResource		*kNC_Data;
	static nsIRDFResource		*kNC_Name;
	static nsIRDFResource		*kNC_URL;
	static nsIRDFResource		*kRDF_InstanceOf;
	static nsIRDFResource		*kRDF_type;
	static nsIRDFResource		*kNC_loading;
	static nsIRDFResource		*kNC_HTML;
	static nsIRDFResource		*kNC_Icon;
	static nsIRDFResource		*kNC_StatusIcon;

	static nsIRDFResource		*kNC_Banner;
	static nsIRDFResource		*kNC_Site;
	static nsIRDFResource		*kNC_Relevance;
	static nsIRDFResource		*kNC_RelevanceSort;
	static nsIRDFResource		*kNC_Engine;

protected:
	static nsIRDFDataSource		*mInner;

	static nsCOMPtr<nsIRDFDataSource>	categoryDataSource;

friend	NS_IMETHODIMP	NS_NewInternetSearchService(nsISupports* aOuter, REFNSIID aIID, void** aResult);

	// helper methods
	PRBool		isEngineURI(nsIRDFResource* aResource);
	PRBool		isSearchURI(nsIRDFResource* aResource);
	PRBool		isSearchCategoryURI(nsIRDFResource* aResource);
	PRBool		isSearchCategoryEngineURI(nsIRDFResource* aResource);
	nsresult	resolveSearchCategoryEngineURI(nsIRDFResource *source, nsIRDFResource **trueEngine);
	nsresult	BeginSearchRequest(nsIRDFResource *source, PRBool doNetworkRequest);
	nsresult	DoSearch(nsIRDFResource *source, nsIRDFResource *engine, nsString text);
	nsresult	GetSearchEngineList(nsFileSpec spec);
	nsresult	GetCategoryList();
	nsresult	GetSearchFolder(nsFileSpec &spec);
	nsresult	ReadFileContents(nsFileSpec baseFilename, nsString & sourceContents);
static	nsresult	GetData(nsString data, char *sectionToFind, char *attribToFind, nsString &value);
	nsresult	GetInputs(nsString data, nsString text, nsString &input);
	nsresult	GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult);
	nsresult	CreateAnonymousResource(nsCOMPtr<nsIRDFResource>* aResult);


			InternetSearchDataSource(void);
	virtual		~InternetSearchDataSource(void);
	NS_METHOD	Init();

public:

	NS_DECL_ISUPPORTS

	NS_DECL_NSIINTERNETSEARCHSERVICE

	// nsIStreamObserver methods:
	NS_DECL_NSISTREAMOBSERVER

	// nsIStreamListener methods:
	NS_DECL_NSISTREAMLISTENER

	// nsIRDFDataSource methods
	NS_DECL_NSIRDFDATASOURCE
};



static	nsIRDFService		*gRDFService = nsnull;

PRInt32				InternetSearchDataSource::gRefCnt;
nsIRDFDataSource		*InternetSearchDataSource::mInner = nsnull;
nsCOMPtr<nsIRDFDataSource>	InternetSearchDataSource::categoryDataSource;

nsIRDFResource			*InternetSearchDataSource::kNC_SearchEngineRoot;
nsIRDFResource			*InternetSearchDataSource::kNC_LastSearchRoot;
nsIRDFResource			*InternetSearchDataSource::kNC_SearchCategoryRoot;
nsIRDFResource			*InternetSearchDataSource::kNC_SearchResultsSitesRoot;
nsIRDFResource			*InternetSearchDataSource::kNC_Ref;
nsIRDFResource			*InternetSearchDataSource::kNC_Child;
nsIRDFResource			*InternetSearchDataSource::kNC_Data;
nsIRDFResource			*InternetSearchDataSource::kNC_Name;
nsIRDFResource			*InternetSearchDataSource::kNC_URL;
nsIRDFResource			*InternetSearchDataSource::kRDF_InstanceOf;
nsIRDFResource			*InternetSearchDataSource::kRDF_type;
nsIRDFResource			*InternetSearchDataSource::kNC_loading;
nsIRDFResource			*InternetSearchDataSource::kNC_HTML;
nsIRDFResource			*InternetSearchDataSource::kNC_Icon;
nsIRDFResource			*InternetSearchDataSource::kNC_StatusIcon;
nsIRDFResource			*InternetSearchDataSource::kNC_Banner;
nsIRDFResource			*InternetSearchDataSource::kNC_Site;
nsIRDFResource			*InternetSearchDataSource::kNC_Relevance;
nsIRDFResource			*InternetSearchDataSource::kNC_RelevanceSort;
nsIRDFResource			*InternetSearchDataSource::kNC_Engine;



static const char		kEngineProtocol[] = "engine://";
static const char		kSearchProtocol[] = "internetsearch:";



InternetSearchDataSource::InternetSearchDataSource(void)
{
	NS_INIT_REFCNT();

	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
			nsIRDFService::GetIID(), (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

		rv = NS_NewISupportsArray(getter_AddRefs(mConnections));

		gRDFService->GetResource(kURINC_SearchEngineRoot,                &kNC_SearchEngineRoot);
		gRDFService->GetResource(kURINC_LastSearchRoot,                  &kNC_LastSearchRoot);
		gRDFService->GetResource(kURINC_SearchResultsSitesRoot,          &kNC_SearchResultsSitesRoot);
		gRDFService->GetResource(NC_NAMESPACE_URI "SearchCategoryRoot",  &kNC_SearchCategoryRoot);
		gRDFService->GetResource(NC_NAMESPACE_URI "ref",                 &kNC_Ref);
		gRDFService->GetResource(NC_NAMESPACE_URI "child",               &kNC_Child);
		gRDFService->GetResource(NC_NAMESPACE_URI "data",                &kNC_Data);
		gRDFService->GetResource(NC_NAMESPACE_URI "Name",                &kNC_Name);
		gRDFService->GetResource(NC_NAMESPACE_URI "URL",                 &kNC_URL);
		gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf",         &kRDF_InstanceOf);
		gRDFService->GetResource(RDF_NAMESPACE_URI "type",               &kRDF_type);
		gRDFService->GetResource(NC_NAMESPACE_URI "loading",             &kNC_loading);
		gRDFService->GetResource(NC_NAMESPACE_URI "HTML",                &kNC_HTML);
		gRDFService->GetResource(NC_NAMESPACE_URI "Icon",                &kNC_Icon);
		gRDFService->GetResource(NC_NAMESPACE_URI "StatusIcon",          &kNC_StatusIcon);
		gRDFService->GetResource(NC_NAMESPACE_URI "Banner",              &kNC_Banner);
		gRDFService->GetResource(NC_NAMESPACE_URI "Site",                &kNC_Site);
		gRDFService->GetResource(NC_NAMESPACE_URI "Relevance",           &kNC_Relevance);
		gRDFService->GetResource(NC_NAMESPACE_URI "Relevance?sort=true", &kNC_RelevanceSort);
		gRDFService->GetResource(NC_NAMESPACE_URI "Engine",              &kNC_Engine);
	}
}



InternetSearchDataSource::~InternetSearchDataSource (void)
{
	if (--gRefCnt == 0)
	{
		NS_IF_RELEASE(kNC_SearchEngineRoot);
		NS_IF_RELEASE(kNC_LastSearchRoot);
		NS_IF_RELEASE(kNC_SearchCategoryRoot);
		NS_IF_RELEASE(kNC_SearchResultsSitesRoot);
		NS_IF_RELEASE(kNC_Ref);
		NS_IF_RELEASE(kNC_Child);
		NS_IF_RELEASE(kNC_Data);
		NS_IF_RELEASE(kNC_Name);
		NS_IF_RELEASE(kNC_URL);
		NS_IF_RELEASE(kRDF_InstanceOf);
		NS_IF_RELEASE(kRDF_type);
		NS_IF_RELEASE(kNC_loading);
		NS_IF_RELEASE(kNC_HTML);
		NS_IF_RELEASE(kNC_Icon);
		NS_IF_RELEASE(kNC_StatusIcon);
		NS_IF_RELEASE(kNC_Banner);
		NS_IF_RELEASE(kNC_Site);
		NS_IF_RELEASE(kNC_Relevance);
		NS_IF_RELEASE(kNC_RelevanceSort);
		NS_IF_RELEASE(kNC_Engine);

		NS_IF_RELEASE(mInner);

		if (gRDFService)
		{
			nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
			gRDFService = nsnull;
		}
	}
}



PRBool
InternetSearchDataSource::isEngineURI(nsIRDFResource *r)
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
InternetSearchDataSource::isSearchURI(nsIRDFResource *r)
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



PRBool
InternetSearchDataSource::isSearchCategoryURI(nsIRDFResource *r)
{
	PRBool		isSearchCategoryURIFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kURINC_SearchCategoryPrefix, sizeof(kURINC_SearchCategoryPrefix) - 1)))
	{
		isSearchCategoryURIFlag = PR_TRUE;
	}
	return(isSearchCategoryURIFlag);
}



PRBool
InternetSearchDataSource::isSearchCategoryEngineURI(nsIRDFResource *r)
{
	PRBool		isSearchCategoryEngineURIFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kURINC_SearchCategoryEnginePrefix, sizeof(kURINC_SearchCategoryEnginePrefix) - 1)))
	{
		isSearchCategoryEngineURIFlag = PR_TRUE;
	}
	return(isSearchCategoryEngineURIFlag);
}



nsresult
InternetSearchDataSource::resolveSearchCategoryEngineURI(nsIRDFResource *engine, nsIRDFResource **trueEngine)
{
	*trueEngine = nsnull;

	if ((!categoryDataSource) || (!mInner))	return(NS_ERROR_UNEXPECTED);

	nsresult		rv;
	nsCOMPtr<nsIRDFNode>	catNode;
	rv = categoryDataSource->GetTarget(engine, kNC_Name, PR_TRUE, getter_AddRefs(catNode));
	if (NS_FAILED(rv))	return(rv);

	nsCOMPtr<nsIRDFLiteral>	catName = do_QueryInterface(catNode);
	if (!catName)		return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIRDFResource>	catSrc;
	rv = mInner->GetSource(kNC_Name, catName, PR_TRUE, getter_AddRefs(catSrc));
	if (NS_FAILED(rv))	return(rv);
	if (!catSrc)		return(NS_ERROR_UNEXPECTED);
	
	*trueEngine = catSrc;
	NS_IF_ADDREF(*trueEngine);
	return(NS_OK);
}



////////////////////////////////////////////////////////////////////////



NS_IMPL_ISUPPORTS3(InternetSearchDataSource, nsIInternetSearchService, nsIRDFDataSource, nsIStreamListener);



NS_METHOD
InternetSearchDataSource::Init()
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
		nsnull, nsIRDFDataSource::GetIID(), (void **)&mInner)))
		return(rv);

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this, PR_FALSE)))
		return(rv);

	mEngineListBuilt = PR_FALSE;

	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::GetURI(char **uri)
{
	NS_PRECONDITION(uri != nsnull, "null ptr");
	if (! uri)
		return NS_ERROR_NULL_POINTER;

	if ((*uri = nsXPIDLCString::Copy("rdf:internetsearch")) == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}



NS_IMETHODIMP
InternetSearchDataSource::GetSource(nsIRDFResource* property,
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
InternetSearchDataSource::GetSources(nsIRDFResource *property,
                                 nsIRDFNode *target,
                                 PRBool tv,
                                 nsISimpleEnumerator **sources /* out */)
{
	NS_NOTYETIMPLEMENTED("write me");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
InternetSearchDataSource::GetTarget(nsIRDFResource *source,
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

	if ((isSearchCategoryURI(source)) && (categoryDataSource))
	{
		const char	*uri = nsnull;
		source->GetValueConst(&uri);
		if (!uri)	return(NS_ERROR_UNEXPECTED);
		nsAutoString	catURI(uri);

		nsCOMPtr<nsIRDFResource>	category;
		if (NS_FAILED(rv = gRDFService->GetResource(nsCAutoString(catURI),
			getter_AddRefs(category))))
			return(rv);

		rv = categoryDataSource->GetTarget(category, property, tv, target);
		return(rv);
	}

	if (isSearchCategoryEngineURI(source))
	{
		nsCOMPtr<nsIRDFResource>	trueEngine;
		rv = resolveSearchCategoryEngineURI(source, getter_AddRefs(trueEngine));
		if (NS_FAILED(rv))	return(rv);
		source = trueEngine;
	}

	if (mInner)
	{
		rv = mInner->GetTarget(source, property, tv, target);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::GetTargets(nsIRDFResource *source,
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

	if ((isSearchCategoryURI(source)) && (categoryDataSource))
	{
		const char	*uri = nsnull;
		source->GetValueConst(&uri);
		if (!uri)	return(NS_ERROR_UNEXPECTED);
		nsAutoString	catURI(uri);

		nsCOMPtr<nsIRDFResource>	category;
		if (NS_FAILED(rv = gRDFService->GetResource(nsCAutoString(catURI),
			getter_AddRefs(category))))
			return(rv);

		rv = categoryDataSource->GetTargets(category, property, tv, targets);
		return(rv);
	}

	if (isSearchCategoryEngineURI(source))
	{
		nsCOMPtr<nsIRDFResource>	trueEngine;
		rv = resolveSearchCategoryEngineURI(source, getter_AddRefs(trueEngine));
		if (NS_FAILED(rv))	return(rv);
		source = trueEngine;
	}

	if (mInner)
	{	
		rv = mInner->GetTargets(source, property, tv, targets);

		// defer search engine discovery until needed; small startup time improvement
		if (((source == kNC_SearchEngineRoot) || isSearchURI(source)) && (property == kNC_Child)
			&& (mEngineListBuilt == PR_FALSE))
		{
			mEngineListBuilt = PR_TRUE;

			// get available search engines
			nsFileSpec			nativeDir;
			if (NS_SUCCEEDED(rv = GetSearchFolder(nativeDir)))
			{
				rv = GetSearchEngineList(nativeDir);
				
				// read in category list
				rv = GetCategoryList();

#ifdef	XP_MAC
				// on Mac, use system's search files too
				nsSpecialSystemDirectory	searchSitesDir(nsSpecialSystemDirectory::Mac_InternetSearchDirectory);
				nativeDir = searchSitesDir;
				rv = GetSearchEngineList(nativeDir);
#endif
			}
		}
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
}



nsresult
InternetSearchDataSource::GetCategoryList()
{
	nsIRDFDataSource	*ds = nsnull;
	nsresult rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
			nsnull, NS_GET_IID(nsIRDFDataSource), (void**) &ds);
	if (NS_FAILED(rv))	return(rv);
	if (!ds)		return(NS_ERROR_UNEXPECTED);

	categoryDataSource = do_QueryInterface(ds);
	NS_RELEASE(ds);
	ds = nsnull;
	if (!categoryDataSource)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIRDFRemoteDataSource>	remoteCategoryDataSource = do_QueryInterface(categoryDataSource);
	if (!remoteCategoryDataSource)	return(NS_ERROR_UNEXPECTED);

	// XXX should check in user's profile directory first,
	//     and fallback to the default file

	nsFileSpec			searchDir;
	if (NS_FAILED(rv = GetSearchFolder(searchDir)))	return(rv);
	searchDir += "category.rdf";

	nsFileURL	fileURL(searchDir);
	if (NS_FAILED(rv = remoteCategoryDataSource->Init(fileURL.GetURLString())))	return(rv);

	// synchronous read
	rv = remoteCategoryDataSource->Refresh(PR_TRUE);
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
InternetSearchDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
InternetSearchDataSource::Change(nsIRDFResource* aSource,
			nsIRDFResource* aProperty,
			nsIRDFNode* aOldTarget,
			nsIRDFNode* aNewTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
InternetSearchDataSource::Move(nsIRDFResource* aOldSource,
					   nsIRDFResource* aNewSource,
					   nsIRDFResource* aProperty,
					   nsIRDFNode* aTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
InternetSearchDataSource::HasAssertion(nsIRDFResource *source,
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
InternetSearchDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
	NS_NOTYETIMPLEMENTED("write me");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
InternetSearchDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsISimpleEnumerator **labels /* out */)
{
	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(labels != nsnull, "null ptr");
	if (! labels)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	if ((source == kNC_SearchEngineRoot) || (source == kNC_LastSearchRoot) || isSearchURI(source))
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

	if ((isSearchCategoryURI(source)) && (categoryDataSource))
	{
		const char	*uri = nsnull;
		source->GetValueConst(&uri);
		if (!uri)	return(NS_ERROR_UNEXPECTED);
		nsAutoString	catURI(uri);

		nsCOMPtr<nsIRDFResource>	category;
		if (NS_FAILED(rv = gRDFService->GetResource(nsCAutoString(catURI),
			getter_AddRefs(category))))
			return(rv);

		rv = categoryDataSource->ArcLabelsOut(category, labels);
		return(rv);
	}

	if (isSearchCategoryEngineURI(source))
	{
		nsCOMPtr<nsIRDFResource>	trueEngine;
		rv = resolveSearchCategoryEngineURI(source, getter_AddRefs(trueEngine));
		if (NS_FAILED(rv) || (!trueEngine))	return(rv);
		source = trueEngine;
	}

	if (mInner)
	{
		rv = mInner->ArcLabelsOut(source, labels);
		return(rv);
	}

	return NS_NewEmptyEnumerator(labels);
}



NS_IMETHODIMP
InternetSearchDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	nsresult	rv = NS_RDF_NO_VALUE;

	if (mInner)
	{
		rv = mInner->GetAllResources(aCursor);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::AddObserver(nsIRDFObserver *aObserver)
{
	nsresult	rv = NS_OK;

	if (mInner)
	{
		rv = mInner->AddObserver(aObserver);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
	nsresult	rv = NS_OK;

	if (mInner)
	{
		rv = mInner->RemoveObserver(aObserver);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::GetAllCommands(nsIRDFResource* source,
                                     nsIEnumerator/*<nsIRDFResource>*/** commands)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
InternetSearchDataSource::GetAllCmds(nsIRDFResource* source,
                                     nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	return(NS_NewEmptyEnumerator(commands));
}



NS_IMETHODIMP
InternetSearchDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                       nsIRDFResource*   aCommand,
                                       nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                       PRBool* aResult)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
InternetSearchDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
NS_NewInternetSearchService(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
	NS_PRECONDITION(aResult != nsnull, "null ptr");
	if (! aResult)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(aOuter == nsnull, "no aggregation");
	if (aOuter)
		return NS_ERROR_NO_AGGREGATION;

	nsresult rv = NS_OK;

	InternetSearchDataSource* result = new InternetSearchDataSource();
	if (! result)
		return NS_ERROR_OUT_OF_MEMORY;

	rv = result->Init();
	if (NS_SUCCEEDED(rv))
		rv = result->QueryInterface(aIID, aResult);

	if (NS_FAILED(rv)) {
		delete result;
		*aResult = nsnull;
		return rv;
	}

	return rv;
}



NS_IMETHODIMP
InternetSearchDataSource::ClearResultSearchSites(void)
{
	// forget about any previous search sites

	if (mInner)
	{
		nsresult			rv;
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
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchDataSource::GetCategoryDataSource(nsIRDFDataSource **ds)
{
	nsresult	rv;

	if (!categoryDataSource)
	{
		if (NS_FAILED(rv = GetCategoryList()))
		{
			*ds = nsnull;
			return(rv);
		}
	}
	if (categoryDataSource)
	{
		*ds = categoryDataSource.get();
		NS_IF_ADDREF(*ds);
		return(NS_OK);
	}
	*ds = nsnull;
	return(NS_ERROR_FAILURE);
}



NS_IMETHODIMP
InternetSearchDataSource::Stop()
{
	// cancel any outstanding connections
	if (mConnections)
	{
		PRUint32	count=0, loop;
		mConnections->Count(&count);
		for (loop=0; loop < count; loop++)
		{
			nsCOMPtr<nsISupports>	iSupports = mConnections->ElementAt(loop);
			if (!iSupports)	continue;
			nsCOMPtr<nsIChannel>	channel = do_QueryInterface(iSupports);
			if (channel)
			{
				channel->Cancel();
			}
		}

		mConnections->Clear();
	}

	// remove any loading icons
	nsresult		rv;
	nsCOMPtr<nsIRDFLiteral>	trueLiteral;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(nsAutoString("true").GetUnicode(),
		getter_AddRefs(trueLiteral))))
	{
		nsCOMPtr<nsISimpleEnumerator>	arcs;
		if (NS_SUCCEEDED(rv = mInner->GetSources(kNC_loading, trueLiteral, PR_TRUE,
			getter_AddRefs(arcs))))
		{
			PRBool			hasMore = PR_TRUE;
			while (hasMore == PR_TRUE)
			{
				if (NS_FAILED(arcs->HasMoreElements(&hasMore)) || (hasMore == PR_FALSE))
					break;
				nsCOMPtr<nsISupports>	arc;
				if (NS_FAILED(arcs->GetNext(getter_AddRefs(arc))))
					break;
				nsCOMPtr<nsIRDFResource>	src = do_QueryInterface(arc);
				if (src)
				{
					mInner->Unassert(src, kNC_loading, trueLiteral);
				}
			}
		}
	}

	return(NS_OK);
}



nsresult
InternetSearchDataSource::BeginSearchRequest(nsIRDFResource *source, PRBool doNetworkRequest)
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
#ifdef	DEBUG_SEARCH_OUTPUT
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
						nsCRT::free(lastC);
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

	// forget about any previous search sites
	ClearResultSearchSites();

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
				if ((value.Find(kEngineProtocol) == 0) ||
					(value.Find(kURINC_SearchCategoryEnginePrefix) == 0))
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

	nsCOMPtr<nsIRDFLiteral>	trueLiteral;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(nsAutoString("true").GetUnicode(),
		getter_AddRefs(trueLiteral))))
	{
		mInner->Assert(source, kNC_loading, trueLiteral, PR_TRUE);
	}

	// loop over specified search engines
	while (engineArray->Count() > 0)
	{
		char *baseFilename = (char *)(engineArray->ElementAt(0));
		engineArray->RemoveElementAt(0);
		if (!baseFilename)	continue;

#ifdef	DEBUG_SEARCH_OUTPUT
		printf("Search engine to query: '%s'\n", baseFilename);
#endif

		nsCOMPtr<nsIRDFResource>	engine;
		gRDFService->GetResource(baseFilename, getter_AddRefs(engine));
		nsCRT::free(baseFilename);
		baseFilename = nsnull;
		if (!engine)	continue;

		// if its a engine from a search category, then get its "#Name",
		// and map from that back to the real engine reference; if an
		// error occurs, finish processing the rest of the engines,
		// don't just break/return out
		if (isSearchCategoryEngineURI(engine))
		{
			nsCOMPtr<nsIRDFResource>	trueEngine;
			rv = resolveSearchCategoryEngineURI(engine, getter_AddRefs(trueEngine));
			if (NS_FAILED(rv) || (!trueEngine))	continue;			
			engine = trueEngine;
		}

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

	// if we weren't able to establish any connections, remove the loading attribute
	// here; otherwise, it will be done when the last connection completes
	if (mConnections)
	{
		PRUint32	count=0;
		mConnections->Count(&count);
		if (count < 1)
		{
			mInner->Unassert(source, kNC_loading, trueLiteral);
		}
	}

	return(rv);
}



nsresult
InternetSearchDataSource::DoSearch(nsIRDFResource *source, nsIRDFResource *engine, nsString text)
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

		nsCRT::free(baseFilename);
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

#ifdef	DEBUG_SEARCH_OUTPUT
	char *cAction = action.ToNewCString();
	char *cMethod = method.ToNewCString();
	char *cInput = input.ToNewCString();
	printf("Search Action: '%s'\n", cAction);
	printf("Search Method: '%s'\n", cMethod);
	printf(" Search Input: '%s'\n\n", cInput);
	if (cAction)
	{
		nsCRT::free(cAction);
		cAction = nsnull;
	}
	if (cMethod)
	{
		nsCRT::free(cMethod);
		cMethod = nsnull;
	}
	if (cInput)
	{
		nsCRT::free(cInput);
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

	nsCOMPtr<nsIInternetSearchContext>	context;
	if (NS_FAILED(rv = NS_NewInternetSearchContext(source, engine, getter_AddRefs(context))))
		return(rv);
	if (!context)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIURI>	url;
	if (NS_SUCCEEDED(rv = NS_NewURI(getter_AddRefs(url), action)))
	{
		nsCOMPtr<nsIChannel>	channel;
		// XXX: Null LoadGroup ?
		if (NS_SUCCEEDED(rv = NS_OpenURI(getter_AddRefs(channel), url, nsnull)))
		{
			if (method.EqualsIgnoreCase("post"))
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
			        	
					nsCOMPtr<nsIInputStream>	postDataStream;
					if (NS_SUCCEEDED(rv = NS_NewPostDataStream(PR_FALSE, nsCAutoString(postStr),
						0, getter_AddRefs(postDataStream))))
					{
						httpChannel->SetPostDataStream(postDataStream);
					}
				}
			}

			if (NS_SUCCEEDED(rv = channel->AsyncRead(0, -1, context, this)))
			{
				if (mConnections)
				{
					mConnections->AppendElement(channel);
				}
			}
		}
	}

	// dispose of any last HTML results page
	if (mInner)
	{
		nsCOMPtr<nsIRDFNode>	htmlNode;
		if (NS_SUCCEEDED(rv = mInner->GetTarget(engine, kNC_HTML, PR_TRUE, getter_AddRefs(htmlNode)))
			&& (rv != NS_RDF_NO_VALUE))
		{
			rv = mInner->Unassert(engine, kNC_HTML, htmlNode);
		}
	}

	// start "loading" animation for this search engine
	if (NS_SUCCEEDED(rv) && (mInner))
	{
		// remove status icon so that loading icon style can be used
		nsCOMPtr<nsIRDFNode>		engineIconNode = nsnull;
		mInner->GetTarget(engine, kNC_StatusIcon, PR_TRUE, getter_AddRefs(engineIconNode));
		if (engineIconNode)
		{
			rv = mInner->Unassert(engine, kNC_StatusIcon, engineIconNode);
		}

		nsCOMPtr<nsIRDFLiteral>	literal;
		if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(nsAutoString("true").GetUnicode(), getter_AddRefs(literal))))
		{
			mInner->Assert(engine, kNC_loading, literal, PR_TRUE);
		}
	}

	return(rv);
}



nsresult
InternetSearchDataSource::GetSearchFolder(nsFileSpec &spec)
{
	nsSpecialSystemDirectory	searchSitesDir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
	searchSitesDir += "res";
	searchSitesDir += "rdf";
	searchSitesDir += "datasets";
	spec = searchSitesDir;
	return(NS_OK);
}



nsresult
InternetSearchDataSource::GetSearchEngineList(nsFileSpec nativeDir)
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
				// check for various icons
				nsAutoString	iconURL;
				PRInt32		extensionOffset;
				extensionOffset = uri.RFind(".src", PR_TRUE);
				if ((extensionOffset >= 0) && (extensionOffset == uri.Length()-4))
				{
					nsAutoString	temp;

					uri.Left(temp, uri.Length()-4);
					temp += ".gif";
					const	nsFileSpec	gifIconFile(temp);
					if (gifIconFile.IsFile())
					{
						nsFileURL	gifIconFileURL(gifIconFile);
						iconURL = gifIconFileURL.GetURLString();
					}
					uri.Left(temp, uri.Length()-4);
					temp += ".jpg";
					const	nsFileSpec	jpgIconFile(temp);
					if (jpgIconFile.IsFile())
					{
						nsFileURL	jpgIconFileURL(jpgIconFile);
						iconURL = jpgIconFileURL.GetURLString();
					}
					uri.Left(temp, uri.Length()-4);
					temp += ".jpeg";
					const	nsFileSpec	jpegIconFile(temp);
					if (jpegIconFile.IsFile())
					{
						nsFileURL	jpegIconFileURL(jpegIconFile);
						iconURL = jpegIconFileURL.GetURLString();
					}
					uri.Left(temp, uri.Length()-4);
					temp += ".png";
					const	nsFileSpec	pngIconFile(temp);
					if (pngIconFile.IsFile())
					{
						nsFileURL	pngIconFileURL(pngIconFile);
						iconURL = pngIconFileURL.GetURLString();
					}
				}

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
						nsCRT::free(uriC);
						if (!uriCescaped)	continue;
//						searchURL += uri;
						searchURL += uriCescaped;
						nsCRT::free(uriCescaped);

/*
						char *baseFilename = uri.ToNewCString();
						if (!baseFilename)	continue;
						baseFilename = nsUnescape(baseFilename);
						if (!baseFilename)	continue;
*/
						nsAutoString	data;
						rv = ReadFileContents(fileSpec, data);
//						nsCRT::free(baseFilename);
//						baseFilename = nsnull;
						if (NS_FAILED(rv))	continue;

						nsCOMPtr<nsIRDFResource>	searchRes;
						char		*searchURI = searchURL.ToNewCString();
						if (searchURI)
						{
							if (NS_SUCCEEDED(rv = gRDFService->GetResource(searchURI, getter_AddRefs(searchRes))))
							{
								// save name of search engine (as specified in file)
								nsAutoString	nameValue;
								if (NS_SUCCEEDED(rv = GetData(data, "search", "name", nameValue)))
								{
									nsCOMPtr<nsIRDFLiteral>	nameLiteral;
									if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(nameValue.GetUnicode(),
											getter_AddRefs(nameLiteral))))
									{
										mInner->Assert(searchRes, kNC_Name, nameLiteral, PR_TRUE);
									}
								}
								
								// save icon url (if we have one)
								if (iconURL.Length() > 0)
								{
									nsCOMPtr<nsIRDFLiteral>	iconLiteral;
									if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(iconURL.GetUnicode(),
											getter_AddRefs(iconLiteral))))
									{
										mInner->Assert(searchRes, kNC_Icon, iconLiteral, PR_TRUE);
									}
								}

								// Note: add the child relationship last
								mInner->Assert(kNC_SearchEngineRoot, kNC_Child, searchRes, PR_TRUE);
							}
							nsCRT::free(searchURI);
							searchURI = nsnull;
						}
//						nsCRT::free(baseFilename);
//						baseFilename = nsnull;
					}
				}
			}
		}
	}
	return(rv);
}



nsresult
InternetSearchDataSource::ReadFileContents(nsFileSpec baseFilename, nsString& sourceContents)
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
InternetSearchDataSource::GetData(nsString data, char *sectionToFind, char *attribToFind, nsString &value)
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
			PRUnichar	quoteChar;
			PRBool		foundQuoteChar = PR_FALSE;

			if ((value[0] == PRUnichar('\"')) || (value[0] == PRUnichar('\'')))
			{
				foundQuoteChar = PR_TRUE;
				quoteChar = value[0];
				value.Cut(0,1);
			}
			len = value.Length();
			if ((len > 0) && (foundQuoteChar == PR_TRUE))
			{
				if (value[len-1] == quoteChar)
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
InternetSearchDataSource::GetInputs(nsString data, nsString text, nsString &input)
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
InternetSearchDataSource::GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult)
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



nsresult
InternetSearchDataSource::CreateAnonymousResource(nsCOMPtr<nsIRDFResource>* aResult)
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

	rv = gRDFService->GetResource(nsCAutoString(uri), getter_AddRefs(*aResult));
	return(rv);
}



// stream observer methods



NS_IMETHODIMP
InternetSearchDataSource::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
{
#ifdef	DEBUG_SEARCH_OUTPUT
	printf("InternetSearchDataSourceCallback::OnStartRequest entered.\n");
#endif
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchDataSource::OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
					nsresult status, const PRUnichar *errorMsg) 
{
	if (mConnections)
	{
		mConnections->RemoveElement(channel);
	}

	nsCOMPtr<nsIInternetSearchContext>	context = do_QueryInterface(ctxt);
	if (!ctxt)	return(NS_ERROR_NO_INTERFACE);

	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	mParent;
	if (NS_FAILED(rv = context->GetParent(getter_AddRefs(mParent))))	return(rv);
	if (!mParent)	return(NS_ERROR_NO_INTERFACE);

	nsCOMPtr<nsIRDFResource>	mEngine;
	if (NS_FAILED(rv = context->GetEngine(getter_AddRefs(mEngine))))	return(rv);
	if (!mEngine)	return(NS_ERROR_NO_INTERFACE);

	nsCOMPtr<nsIURI> aURL;
	rv = channel->GetURI(getter_AddRefs(aURL));
	if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIRDFLiteral>	trueLiteral;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(nsAutoString("true").GetUnicode(), getter_AddRefs(trueLiteral))))
	{
		mInner->Unassert(mEngine, kNC_loading, trueLiteral);

		if (mConnections)
		{
			PRUint32	count=0;
			mConnections->Count(&count);
			if (count <= 1)
			{
				mInner->Unassert(mParent, kNC_loading, trueLiteral);
			}
		}
	}

	// copy the engine's icon reference (if it has one) onto the result node
	nsCOMPtr<nsIRDFNode>		engineIconStatusNode = nsnull;
	mInner->GetTarget(mEngine, kNC_Icon, PR_TRUE, getter_AddRefs(engineIconStatusNode));
	if (engineIconStatusNode)
	{
		rv = mInner->Assert(mEngine, kNC_StatusIcon, engineIconStatusNode, PR_TRUE);
	}

	PRUnichar	*uniBuf = nsnull;
	if (NS_FAILED(rv = context->GetBuffer(&uniBuf)))	return(rv);
	if (!uniBuf)	return(NS_ERROR_UNEXPECTED);

	nsAutoString	htmlResults(uniBuf);

	if (htmlResults.Length() < 1)
	{
#ifdef	DEBUG_SEARCH_OUTPUT
		printf(" *** InternetSearchDataSourceCallback::OnStopRequest:  no data.\n\n");
#endif
		return(NS_OK);
	}

	if (NS_FAILED(rv = context->Truncate()))	return(rv);

	// save HTML result page for this engine
	const PRUnichar	*htmlUni = htmlResults.GetUnicode();
	if (htmlUni)
	{
		nsCOMPtr<nsIRDFLiteral>	htmlLiteral;
		if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(htmlUni, getter_AddRefs(htmlLiteral))))
		{
			rv = mInner->Assert(mEngine, kNC_HTML, htmlLiteral, PR_TRUE);
		}
	}

	// get data out of graph
	nsAutoString		data("");
	nsCOMPtr<nsIRDFNode>	dataNode;
	if (NS_FAILED(rv = mInner->GetTarget(mEngine, kNC_Data, PR_TRUE, getter_AddRefs(dataNode))))
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
	nsAutoString	bannerStartStr(""), bannerEndStr(""), skiplocalStr("");
	PRBool		skipLocalFlag = PR_FALSE;

	InternetSearchDataSource::GetData(data, "interpret", "resultListStart", resultListStartStr);
	InternetSearchDataSource::GetData(data, "interpret", "resultListEnd", resultListEndStr);
	InternetSearchDataSource::GetData(data, "interpret", "resultItemStart", resultItemStartStr);
	InternetSearchDataSource::GetData(data, "interpret", "resultItemEnd", resultItemEndStr);
	InternetSearchDataSource::GetData(data, "interpret", "relevanceStart", relevanceStartStr);
	InternetSearchDataSource::GetData(data, "interpret", "relevanceEnd", relevanceEndStr);
	InternetSearchDataSource::GetData(data, "interpret", "bannerStart", bannerStartStr);
	InternetSearchDataSource::GetData(data, "interpret", "bannerEnd", bannerEndStr);
	InternetSearchDataSource::GetData(data, "interpret", "skiplocal", skiplocalStr);
	if (skiplocalStr.EqualsIgnoreCase("true"))
	{
		skipLocalFlag = PR_TRUE;
	}

#ifdef	DEBUG_SEARCH_OUTPUT
	char *cStr;
	cStr = resultListStartStr.ToNewCString();
	if (cStr)
	{
		printf("resultListStart: '%s'\n", cStr);
		nsCRT::free(cStr);
		cStr = nsnull;
	}
	cStr = resultListEndStr.ToNewCString();
	if (cStr)
	{
		printf("resultListEnd: '%s'\n", cStr);
		nsCRT::free(cStr);
		cStr = nsnull;
	}
	cStr = resultItemStartStr.ToNewCString();
	if (cStr)
	{
		printf("resultItemStart: '%s'\n", cStr);
		nsCRT::free(cStr);
		cStr = nsnull;
	}
	cStr = resultItemEndStr.ToNewCString();
	if (cStr)
	{
		printf("resultItemEnd: '%s'\n", cStr);
		nsCRT::free(cStr);
		cStr = nsnull;
	}
	cStr = relevanceStartStr.ToNewCString();
	if (cStr)
	{
		printf("relevanceStart: '%s'\n", cStr);
		nsCRT::free(cStr);
		cStr = nsnull;
	}
	cStr = relevanceEndStr.ToNewCString();
	if (cStr)
	{
		printf("relevanceEnd: '%s'\n", cStr);
		nsCRT::free(cStr);
		cStr = nsnull;
	}
#endif

	// pre-compute server path (we'll discard URLs that match this)
	nsAutoString	serverPathStr;
	char *serverPath = nsnull;
	aURL->GetPath(&serverPath);
	if (serverPath)
	{
		serverPathStr = serverPath;
		nsCRT::free(serverPath);
		serverPath = nsnull;

		PRInt32 serverOptionsOffset = serverPathStr.FindChar(PRUnichar('?'));
		if (serverOptionsOffset >= 0)	serverPathStr.Truncate(serverOptionsOffset);
	}

	// look for banner once in entire document
	nsCOMPtr<nsIRDFLiteral>	bannerLiteral;
	if ((bannerStartStr.Length() > 0) && (bannerEndStr.Length() > 0))
	{
		PRInt32	bannerStart = htmlResults.Find(bannerStartStr, PR_TRUE);
		if (bannerStart >= 0)
		{
			nsAutoString	htmlCopy(htmlResults);
			htmlCopy.Cut(0, bannerStart + bannerStartStr.Length());
			
			PRInt32	bannerEnd = htmlCopy.Find(bannerEndStr, PR_TRUE);
			if (bannerEnd > 0)
			{
				htmlCopy.Truncate(bannerEnd);
				if (htmlCopy.Length() > 0)
				{
					const PRUnichar	*bannerUni = htmlCopy.GetUnicode();
					if (bannerUni)
					{
						gRDFService->GetLiteral(bannerUni, getter_AddRefs(bannerLiteral));
					}
				}
			}
		}
	}

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
		// rjc note: use RFind to find the LAST occurrence of resultListEndStr
		PRInt32	resultListEnd = htmlResults.RFind(resultListEndStr, PR_TRUE);
		if (resultListEnd >= 0)
		{
			htmlResults.Truncate(resultListEnd);
		}
	}

	PRBool	trimItemStart = PR_TRUE;
	PRBool	trimItemEnd = PR_FALSE;		// rjc note: testing shows we should NEVER trim end???

	// if resultItemStartStr is not specified, try making it just be "HREF="
	if (resultItemStartStr.Length() < 1)
	{
		resultItemStartStr = "HREF=";
		trimItemStart = PR_FALSE;
	}

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

		PRInt32	resultItemEnd;
		if (trimItemStart == PR_TRUE)
		{
			htmlResults.Cut(0, resultItemStart + resultItemStartStr.Length());
			resultItemEnd = htmlResults.Find(resultItemEndStr, PR_TRUE );
		}
		else
		{
			htmlResults.Cut(0, resultItemStart);
			resultItemEnd = htmlResults.Find(resultItemEndStr, PR_TRUE, resultItemStartStr.Length() );
		}

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

#ifdef	DEBUG_SEARCH_OUTPUT
		char	*results = resultItem.ToNewCString();
		if (results)
		{
			printf("\n----- Search result: '%s'\n\n", results);
			nsCRT::free(results);
			results = nsnull;
		}
#endif

		// look for href
		PRInt32	hrefOffset = resultItem.Find("HREF=", PR_TRUE);
		if (hrefOffset < 0)
		{
#ifdef	DEBUG_SEARCH_OUTPUT
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
			if (skipLocalFlag == PR_TRUE)	continue;

			char *host = nsnull, *protocol = nsnull;
			aURL->GetHost(&host);
			aURL->GetScheme(&protocol);
			if (host && protocol)
			{
				nsAutoString	temp;
				temp += protocol;
				temp += "://";
				temp += host;
				temp += hrefStr;

				hrefStr = temp;
			}
			if (host) nsCRT::free(host);
			if (protocol) nsCRT::free(protocol);
		}
		else if (serverPathStr.Length() > 0)
		{
			// prune out any URLs that reference the search engine site
			nsAutoString	tempHREF = hrefStr;
			tempHREF.Insert("/", 0);

			PRInt32	optionsOffset = tempHREF.FindChar(PRUnichar('?'));
			if (optionsOffset >= 0)	tempHREF.Truncate(optionsOffset);

			if (tempHREF.EqualsIgnoreCase(serverPathStr))	continue;
		}
		
		char	*href = hrefStr.ToNewCString();
		if (!href)	continue;

		nsAutoString	site(href);

#ifdef	DEBUG_SEARCH_OUTPUT
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
		
		// save HREF attribute as URL
		if (NS_SUCCEEDED(rv = CreateAnonymousResource(&res)))
		{
			const PRUnichar	*hrefUni = hrefStr.GetUnicode();
			if (hrefUni)
			{
				nsCOMPtr<nsIRDFLiteral>	hrefLiteral;
				if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(hrefUni, getter_AddRefs(hrefLiteral))))
				{
					mInner->Assert(res, kNC_URL, hrefLiteral, PR_TRUE);
				}
			}
		}
#endif

		nsCRT::free(href);
		href = nsnull;
		if (NS_FAILED(rv))	continue;
		
		// set HTML response chunk
		const PRUnichar	*htmlResponseUni = resultItem.GetUnicode();
		if (htmlResponseUni)
		{
			nsCOMPtr<nsIRDFLiteral>	htmlResponseLiteral;
			if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(htmlResponseUni, getter_AddRefs(htmlResponseLiteral))))
			{
				if (htmlResponseLiteral)
				{
					mInner->Assert(res, kNC_HTML, htmlResponseLiteral, PR_TRUE);
				}
			}
		}
		
		// set banner (if we have one)
		if (bannerLiteral)
		{
			mInner->Assert(res, kNC_Banner, bannerLiteral, PR_TRUE);
		}

		// look for Site (if it isn't already set)
		nsCOMPtr<nsIRDFNode>		oldSiteRes = nsnull;
		mInner->GetTarget(res, kNC_Site, PR_TRUE, getter_AddRefs(oldSiteRes));
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
								mInner->Assert(res, kNC_Site, siteLiteral, PR_TRUE);
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
#ifdef	DEBUG_SEARCH_OUTPUT
			printf("\n\nSearch: Unable to find ending > when computing name.\n\n");
#endif
			continue;
		}
//		PRInt32	anchorStop = resultItem.FindChar(PRUnichar('<'), PR_TRUE, quoteEndOffset);
		PRInt32	anchorStop = resultItem.Find("</A>", PR_TRUE, quoteEndOffset);
		if (anchorStop < anchorEnd)
		{
#ifdef	DEBUG_SEARCH_OUTPUT
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
		mInner->GetTarget(res, kNC_Name, PR_TRUE, getter_AddRefs(oldNameRes));
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
							mInner->Assert(res, kNC_Name, nameLiteral, PR_TRUE);
						}
					}
				}
			}
		}

		// look for relevance
		nsAutoString	relItem;
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
		mInner->GetTarget(res, kNC_Relevance, PR_TRUE, getter_AddRefs(oldRelRes));
		if (!oldRelRes)
		{
			if (relItem.Length() > 0)
			{
				// save real relevance
				const PRUnichar	*relUni = relItem.GetUnicode();
				if (relUni)
				{
					nsAutoString	relStr(relUni);
					// take out any characters that aren't numeric or "%"
					PRInt32	len = relStr.Length();
					for (PRInt32 x=len-1; x>=0; x--)
					{
						PRUnichar	ch;
						ch = relStr.CharAt(x);
						if ((ch != PRUnichar('%')) &&
							((ch < PRUnichar('0')) || (ch > PRUnichar('9'))))
						{
							relStr.Cut(x, 1);
						}
					}
					// make sure it ends with a "%"
					len = relStr.Length();
					if (len > 0)
					{
						PRUnichar	ch;
						ch = relStr.CharAt(len - 1);
						if (ch != PRUnichar('%'))
						{
							relStr += PRUnichar('%');
						}
						relItem = relStr;
					}
					else
					{
						relItem.Truncate();
					}
				}
			}
			if (relItem.Length() < 1)
			{
				relItem = "-";
			}

			const PRUnichar *relItemUni = relItem.GetUnicode();
			nsCOMPtr<nsIRDFLiteral>	relLiteral;
			if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(relItemUni, getter_AddRefs(relLiteral))))
			{
				if (relLiteral)
				{
					mInner->Assert(res, kNC_Relevance, relLiteral, PR_TRUE);
				}
			}

			if ((relItem.Length() > 0) && (!relItem.Equals("-")))
			{
				// If its a percentage, remove "%"
				if (relItem[relItem.Length()-1] == PRUnichar('%'))
				{
					relItem.Cut(relItem.Length()-1, 1);
				}

				// left-pad with "0"s and set special sorting value
				nsAutoString	zero("000");
				if (relItem.Length() < 3)
				{
					relItem.Insert(zero, 0, zero.Length() - relItem.Length()); 
				}
			}
			else
			{
				relItem = "000";
			}

			const PRUnichar	*relSortUni = relItem.GetUnicode();
			if (relSortUni)
			{
				nsCOMPtr<nsIRDFLiteral>	relSortLiteral;
				if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(relSortUni, getter_AddRefs(relSortLiteral))))
				{
					if (relSortLiteral)
					{
						mInner->Assert(res, kNC_RelevanceSort, relSortLiteral, PR_TRUE);
					}
				}
			}
		}

		// set reference to engine this came from (if it isn't already set)
		nsCOMPtr<nsIRDFNode>		oldEngineRes = nsnull;
		mInner->GetTarget(res, kNC_Engine, PR_TRUE, getter_AddRefs(oldEngineRes));
		if (!oldEngineRes)
		{
			nsAutoString	engineStr;
			if (NS_SUCCEEDED(rv = InternetSearchDataSource::GetData(data, "search", "name", engineStr)))
			{
				const PRUnichar		*engineUni = engineStr.GetUnicode();
				nsCOMPtr<nsIRDFLiteral>	engineLiteral;
				if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(engineUni, getter_AddRefs(engineLiteral))))
				{
					if (engineLiteral)
					{
						mInner->Assert(res, kNC_Engine, engineLiteral, PR_TRUE);
					}
				}
			}
		}

		// copy the engine's icon reference (if it has one) onto the result node
		nsCOMPtr<nsIRDFNode>		engineIconNode = nsnull;
		mInner->GetTarget(mEngine, kNC_Icon, PR_TRUE, getter_AddRefs(engineIconNode));
		if (engineIconNode)
		{
			rv = mInner->Assert(res, kNC_Icon, engineIconNode, PR_TRUE);
		}

#ifdef	OLDWAY
		// Note: always add in parent-child relationship last!  (if it isn't already set)
		PRBool		parentHasChildFlag = PR_FALSE;
		mInner->HasAssertion(mParent, kNC_Child, res, PR_TRUE, &parentHasChildFlag);
		if (parentHasChildFlag == PR_FALSE)
#endif
		{
			rv = mInner->Assert(mParent, kNC_Child, res, PR_TRUE);
		}

		// Persist this under kNC_LastSearchRoot
		if (mInner)
		{
			rv = mInner->Assert(kNC_LastSearchRoot, kNC_Child, res, PR_TRUE);
		}

	}
	return(NS_OK);
}



// stream listener methods



NS_IMETHODIMP
InternetSearchDataSource::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt,
				nsIInputStream *aIStream, PRUint32 sourceOffset, PRUint32 aLength)
{
	nsCOMPtr<nsIInternetSearchContext>	context = do_QueryInterface(ctxt);
	if (!ctxt)	return(NS_ERROR_NO_INTERFACE);

	nsresult	rv = NS_OK;

	if (aLength < 1)	return(rv);

	PRUint32	count;
	char		*buffer = new char[ aLength ];
	if (!buffer)	return(NS_ERROR_OUT_OF_MEMORY);

	if (NS_FAILED(rv = aIStream->Read(buffer, aLength, &count)) || count == 0)
	{
#ifdef	DEBUG
		printf("Search datasource read failure.\n");
#endif
		delete []buffer;
		return(rv);
	}
	if (count != aLength)
	{
#ifdef	DEBUG
		printf("Search datasource read # of bytes failure.\n");
#endif
		delete []buffer;
		return(NS_ERROR_UNEXPECTED);
	}

	context->AppendBytes(buffer, aLength);

	delete [] buffer;
	buffer = nsnull;
	return(rv);
}
