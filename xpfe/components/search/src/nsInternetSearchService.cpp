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
 *   Robert John Churchill	<rjc@netscape.com>
 *   Pierre Phaneuf		<pp@ludusdesign.com>
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
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsITextToSubURI.h"
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
static NS_DEFINE_CID(kRDFXMLDataSourceCID,         NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID,  NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kTextToSubURICID,             NS_TEXTTOSUBURI_CID);

static const char kURINC_SearchEngineRoot[]           = "NC:SearchEngineRoot";
static const char kURINC_SearchResultsSitesRoot[]     = "NC:SearchResultsSitesRoot";
static const char kURINC_LastSearchRoot[]             = "NC:LastSearchRoot";
static const char kURINC_SearchCategoryRoot[]         = "NC:SearchCategoryRoot";
static const char kURINC_SearchCategoryPrefix[]       = "NC:SearchCategory?category=";
static const char kURINC_SearchCategoryEnginePrefix[] = "NC:SearchCategory?engine=";



class	InternetSearchContext : public nsIInternetSearchContext
{
private:
	nsCOMPtr<nsIRDFResource>	mParent;
	nsCOMPtr<nsIRDFResource>	mEngine;
	nsCOMPtr<nsIUnicodeDecoder>	mUnicodeDecoder;
	nsAutoString			mBuffer;

public:
			InternetSearchContext(nsIRDFResource *aParent, nsIRDFResource *aEngine, nsIUnicodeDecoder *aUnicodeDecoder);
	virtual		~InternetSearchContext(void);
	NS_METHOD	Init();

	NS_DECL_ISUPPORTS
	NS_DECL_NSIINTERNETSEARCHCONTEXT
};



InternetSearchContext::~InternetSearchContext(void)
{
}



InternetSearchContext::InternetSearchContext(nsIRDFResource *aParent, nsIRDFResource *aEngine,
						nsIUnicodeDecoder *aUnicodeDecoder)
	: mParent(aParent), mEngine(aEngine), mUnicodeDecoder(aUnicodeDecoder)
{
	NS_INIT_ISUPPORTS();
}



NS_IMETHODIMP
InternetSearchContext::Init()
{
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::GetUnicodeDecoder(nsIUnicodeDecoder **decoder)
{
	*decoder = mUnicodeDecoder;
	NS_IF_ADDREF(*decoder);
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
InternetSearchContext::AppendUnicodeBytes(const PRUnichar *buffer, PRInt32 numUniBytes)
{
	mBuffer.Append(buffer, numUniBytes);
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



NS_IMPL_ISUPPORTS(InternetSearchContext, NS_GET_IID(nsIInternetSearchContext));



nsresult
NS_NewInternetSearchContext(nsIRDFResource *aParent, nsIRDFResource *aEngine,
	nsIUnicodeDecoder *aUnicodeDecoder, nsIInternetSearchContext **aResult)
{
	 InternetSearchContext *result =
		 new InternetSearchContext(aParent, aEngine, aUnicodeDecoder);

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
				 public nsIRDFDataSource, public nsIStreamListener
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
	static nsIRDFResource		*kNC_Description;
	static nsIRDFResource		*kNC_LastText;
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
	static nsIRDFResource		*kNC_Price;
	static nsIRDFResource		*kNC_PriceSort;
	static nsIRDFResource		*kNC_Availability;

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
	nsresult	FindData(nsIRDFResource *engine, nsString &data);
	nsresult	DoSearch(nsIRDFResource *source, nsIRDFResource *engine, const nsString &fullURL, const nsString &text);
	nsresult	MapEncoding(const nsString &numericEncoding, nsString &stringEncoding);
	nsresult	GetSearchEngineList(nsFileSpec spec, PRBool checkMacFileType);
	nsresult	GetCategoryList();
	nsresult	GetSearchFolder(nsFileSpec &spec);
	nsresult	ReadFileContents(nsFileSpec baseFilename, nsString & sourceContents);
static	nsresult	GetData(nsString &data, const char *sectionToFind, const char *attribToFind, nsString &value);
	nsresult	GetInputs(const nsString &data, nsString &userVar, const nsString &text, nsString &input);
	nsresult	GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult);
	nsresult	ParseHTML(nsIURI *aURL, nsIRDFResource *mParent, nsIRDFResource *engine, const nsString &htmlPage, PRBool useAllHREFsFlag, PRUint32 &numResults);
	nsresult	SetHint(nsIRDFResource *mParent, nsIRDFResource *hintRes);
	nsresult	ConvertEntities(nsString &str, PRBool removeHTMLFlag = PR_TRUE, PRBool removeCRLFsFlag = PR_TRUE, PRBool trimWhiteSpaceFlag = PR_TRUE);

			InternetSearchDataSource(void);
	virtual		~InternetSearchDataSource(void);
	NS_METHOD	Init();
	NS_METHOD	DeferredInit();

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
nsIRDFResource			*InternetSearchDataSource::kNC_Description;
nsIRDFResource			*InternetSearchDataSource::kNC_LastText;
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
nsIRDFResource			*InternetSearchDataSource::kNC_Price;
nsIRDFResource			*InternetSearchDataSource::kNC_PriceSort;
nsIRDFResource			*InternetSearchDataSource::kNC_Availability;


static const char		kEngineProtocol[] = "engine://";
static const char		kSearchProtocol[] = "internetsearch:";



InternetSearchDataSource::InternetSearchDataSource(void)
{
	NS_INIT_REFCNT();

	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
			NS_GET_IID(nsIRDFService), (nsISupports**) &gRDFService);

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
		gRDFService->GetResource(NC_NAMESPACE_URI "Description",         &kNC_Description);
		gRDFService->GetResource(NC_NAMESPACE_URI "LastText",            &kNC_LastText);
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
		gRDFService->GetResource(NC_NAMESPACE_URI "Price",               &kNC_Price);
		gRDFService->GetResource(NC_NAMESPACE_URI "Price?sort=true",     &kNC_PriceSort);
		gRDFService->GetResource(NC_NAMESPACE_URI "Availability",        &kNC_Availability);
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
		NS_IF_RELEASE(kNC_Description);
		NS_IF_RELEASE(kNC_LastText);
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
		NS_IF_RELEASE(kNC_Price);
		NS_IF_RELEASE(kNC_PriceSort);
		NS_IF_RELEASE(kNC_Availability);

		NS_IF_RELEASE(mInner);

		if (gRDFService)
		{
			nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
			gRDFService = nsnull;
		}
		categoryDataSource = NULL;		
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
		nsnull, NS_GET_IID(nsIRDFDataSource), (void **)&mInner)))
		return(rv);

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this, PR_FALSE)))
		return(rv);

	mEngineListBuilt = PR_FALSE;

	return(rv);
}



NS_METHOD
InternetSearchDataSource::DeferredInit()
{
	nsresult	rv = NS_OK;

	if (mEngineListBuilt == PR_FALSE)
	{
		mEngineListBuilt = PR_TRUE;

		// get available search engines
		nsFileSpec			nativeDir;
		if (NS_SUCCEEDED(rv = GetSearchFolder(nativeDir)))
		{
			rv = GetSearchEngineList(nativeDir, PR_FALSE);
			
			// read in category list
			rv = GetCategoryList();

#ifdef	XP_MAC
			// on Mac, use system's search files too
			nsSpecialSystemDirectory	searchSitesDir(nsSpecialSystemDirectory::Mac_InternetSearchDirectory);
			nativeDir = searchSitesDir;
			rv = GetSearchEngineList(nativeDir, PR_TRUE);
#endif
		}
	}
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
	nsresult	rv = NS_RDF_NO_VALUE;

	if (mInner)
	{
		rv = mInner->GetSources(property, target, tv, sources);
	}
	else
	{
		rv = NS_NewEmptyEnumerator(sources);
	}
	return(rv);
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

	if (isSearchURI(source) && (property == kNC_Child))
	{
		// fake out the generic builder (i.e. return anything in this case)
		// so that search containers never appear to be empty
		*target = source;
		NS_ADDREF(source);
		return(NS_OK);
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
			DeferredInit();
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
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	// we only have positive assertions in the internet search data source.
	if (! tv)
		return(rv);

	if (mInner)
	{
		rv = mInner->Assert(source, property, target, tv);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (mInner)
	{
		rv = mInner->Unassert(source, property, target);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::Change(nsIRDFResource *source,
			nsIRDFResource *property,
			nsIRDFNode *oldTarget,
			nsIRDFNode *newTarget)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (mInner)
	{
		rv = mInner->Change(source, property, oldTarget, newTarget);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::Move(nsIRDFResource *oldSource,
					   nsIRDFResource *newSource,
					   nsIRDFResource *property,
					   nsIRDFNode *target)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (mInner)
	{
		rv = mInner->Move(oldSource, newSource, property, target);
	}
	return(rv);
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

	*hasAssertion = PR_FALSE;

	// we only have positive assertions in the internet search data source.
	if (! tv)
	{
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
	nsresult	rv;

	if (mInner)
	{
		rv = mInner->ArcLabelsIn(node, labels);
		return(rv);
	}
	else
	{
		rv = NS_NewEmptyEnumerator(labels);
	}
	return(rv);
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
InternetSearchDataSource::GetInternetSearchURL(const char *searchEngineURI,
	const PRUnichar *searchStr, char **resultURL)
{
	if (!resultURL)	return(NS_ERROR_NULL_POINTER);
	*resultURL = nsnull;

	// if we haven't already, load in the engines
	if (mEngineListBuilt == PR_FALSE)	DeferredInit();

	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	engine;
	if (NS_FAILED(rv = gRDFService->GetResource(searchEngineURI, getter_AddRefs(engine))))
		return(rv);
	if (!engine)	return(NS_ERROR_UNEXPECTED);

	// if its a engine from a search category, then get its "#Name",
	// and try to map from that back to the real engine reference
	if (isSearchCategoryEngineURI(engine))
	{
		nsCOMPtr<nsIRDFResource>	trueEngine;
		rv = resolveSearchCategoryEngineURI(engine, getter_AddRefs(trueEngine));
		if (NS_FAILED(rv) || (!trueEngine))	return(NS_ERROR_UNEXPECTED);			
		engine = trueEngine;
	}

	nsAutoString	data;
	if (NS_FAILED(rv = FindData(engine, data)))	return(rv);
	if (data.Length() < 1)				return(NS_ERROR_UNEXPECTED);

	nsAutoString	 text(searchStr), encodingStr, queryEncodingStr;

	GetData(data, "search", "queryEncoding", encodingStr);		// decimal string values
	MapEncoding(encodingStr, queryEncodingStr);
	if (queryEncodingStr.Length() > 0)
	{
		// convert from escaped-UTF_8, to unicode, and then to
		// the charset indicated by the dataset in question

		char	*utf8data = text.ToNewUTF8String();
		if (utf8data)
		{
			NS_WITH_SERVICE(nsITextToSubURI, textToSubURI,
				kTextToSubURICID, &rv);
			if (NS_SUCCEEDED(rv) && (textToSubURI))
			{
				PRUnichar	*uni = nsnull;
				if (NS_SUCCEEDED(rv = textToSubURI->UnEscapeAndConvert("UTF-8", utf8data, &uni)) && (uni))
				{
					char	*charsetData = nsnull;
					if (NS_SUCCEEDED(rv = textToSubURI->ConvertAndEscape(nsCAutoString(queryEncodingStr), uni, &charsetData)) && (charsetData))
					{
						text = charsetData;
						Recycle(charsetData);
					}
					Recycle(uni);
				}
			}
			Recycle(utf8data);
		}
	}
	
	nsAutoString	action, input, method, userVar;
	if (NS_FAILED(rv = GetData(data, "search", "action", action)))	return(rv);
	if (NS_FAILED(rv = GetData(data, "search", "method", method)))	return(rv);
	if (NS_FAILED(rv = GetInputs(data, userVar, text, input)))	return(rv);
	if (input.Length() < 1)				return(NS_ERROR_UNEXPECTED);

	// we can only handle HTTP GET
	if (!method.EqualsIgnoreCase("get"))	return(NS_ERROR_UNEXPECTED);
	// HTTP Get method support
	action += "?";
	action += input;

	// return a copy of the resulting search URL
	*resultURL = action.ToNewCString();
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchDataSource::RememberLastSearchText(const PRUnichar *escapedSearchStr)
{
	nsresult		rv;
	nsCOMPtr<nsIRDFLiteral>	textLiteral;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(escapedSearchStr, getter_AddRefs(textLiteral))))
	{
		nsCOMPtr<nsIRDFNode>	textNode;
		if (NS_SUCCEEDED(rv = mInner->GetTarget(kNC_LastSearchRoot, kNC_LastText, PR_TRUE,
			getter_AddRefs(textNode))) && (rv != NS_RDF_NO_VALUE))
		{
			Change(kNC_LastSearchRoot, kNC_LastText, textNode, textLiteral);
		}
		else
		{
			Assert(kNC_LastSearchRoot, kNC_LastText, textLiteral, PR_TRUE);
		}
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::FindInternetSearchResults(const char *url, PRBool *searchInProgress)
{
	*searchInProgress = PR_FALSE;

	if (!mInner)	return(NS_OK);

	// if the url doesn't look like a HTTP GET query, just return,
	// otherwise strip off the query data
	nsAutoString		shortURL(url);
	PRInt32			optionsOffset;
	if ((optionsOffset = shortURL.FindChar(PRUnichar('?'))) < 0)	return(NS_OK);
	shortURL.Truncate(optionsOffset);

	// if we haven't already, load in the engines
	if (mEngineListBuilt == PR_FALSE)	DeferredInit();

	// look in available engines to see if any of them appear
	// to match this url (minus the GET options)
	PRBool				foundEngine = PR_FALSE;
	nsresult			rv;
	nsAutoString			data;
	nsCOMPtr<nsIRDFResource>	engine;
	nsCOMPtr<nsISimpleEnumerator>	arcs;
	if (NS_SUCCEEDED(rv = mInner->GetTargets(kNC_SearchEngineRoot, kNC_Child,
		PR_TRUE, getter_AddRefs(arcs))))
	{
		PRBool			hasMore = PR_TRUE;
		while (hasMore == PR_TRUE)
		{
			if (NS_FAILED(arcs->HasMoreElements(&hasMore)) || (hasMore == PR_FALSE))
				break;
			nsCOMPtr<nsISupports>	arc;
			if (NS_FAILED(arcs->GetNext(getter_AddRefs(arc))))
				break;

			engine = do_QueryInterface(arc);
			if (!engine)	continue;

			if (NS_FAILED(rv = FindData(engine, data)))	continue;
			if (data.Length() < 1)				continue;

			nsAutoString		action;
			if (NS_FAILED(rv = GetData(data, "search", "action", action)))	continue;
			if (shortURL.EqualsIgnoreCase(action))
			{
				foundEngine = PR_TRUE;
				break;
			}

			// extension for engines which can have multiple "actions"
			if (NS_FAILED(rv = GetData(data, "browser", "alsomatch", action)))	continue;
			if (shortURL.EqualsIgnoreCase(action))
			{
				foundEngine = PR_TRUE;
				break;
			}
		}
	}
	if (foundEngine == PR_TRUE)
	{
		nsAutoString	searchURL(url);

		// look for query option which is the string the user is searching for
		nsAutoString	userVar, inputUnused;
		if (NS_FAILED(rv = GetInputs(data, userVar, "", inputUnused)))	return(rv);
		if (userVar.Length() < 1)	return(NS_RDF_NO_VALUE);

		nsAutoString	queryStr("?");
		queryStr += userVar;
		queryStr += "=";

		PRInt32		queryOffset;
		if ((queryOffset = searchURL.Find(queryStr, PR_TRUE )) < 0)
		{
			queryStr = "&";
			queryStr += userVar;
			queryStr += "=";
			if ((queryOffset = searchURL.Find(queryStr, PR_TRUE )) < 0)
				return(NS_RDF_NO_VALUE);
		}
		
		nsAutoString	searchText;
		PRInt32		andOffset;
		searchURL.Right(searchText, searchURL.Length() - queryOffset - queryStr.Length());

		if ((andOffset = searchText.FindChar(PRUnichar('&'))) >= 0)
		{
			searchText.Truncate(andOffset);
		}
		if (searchText.Length() < 1)	return(NS_RDF_NO_VALUE);

		// remember the text of the last search
		RememberLastSearchText(searchText.GetUnicode());

#ifdef	DEBUG_SEARCH_OUTPUT
		char	*engineMatch = searchText.ToNewCString();
		if (engineMatch)
		{
			printf("FindInternetSearchResults: found a search engine match: '%s'\n\n", engineMatch);
			delete [] engineMatch;
			engineMatch = nsnull;
		}
#endif

		// forget about any previous search results
		ClearResults();

		// do the search
		DoSearch(nsnull, engine, searchURL, nsAutoString(""));

		*searchInProgress = PR_TRUE;
	}

	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchDataSource::ClearResults(void)
{
	if (mInner)
	{
		nsresult			rv;
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
	return(NS_OK);
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
	ClearResults();

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
			DoSearch(source, engine, nsAutoString(""), text);
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
InternetSearchDataSource::FindData(nsIRDFResource *engine, nsString &data)
{
	data.Truncate();

	if (!engine)	return(NS_ERROR_NULL_POINTER);
	if (!mInner)	return(NS_RDF_NO_VALUE);

	nsresult		rv;

	nsCOMPtr<nsIRDFNode>	dataTarget = nsnull;
	if (NS_SUCCEEDED((rv = mInner->GetTarget(engine, kNC_Data, PR_TRUE,
		getter_AddRefs(dataTarget)))) && (dataTarget))
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
	return(rv);
}



struct	encodings
{
	char		*numericEncoding;
	char		*stringEncoding;
};



nsresult
InternetSearchDataSource::MapEncoding(const nsString &numericEncoding, nsString &stringEncoding)
{
	// XXX we need to have a fully table of numeric --> string conversions

	struct	encodings	encodingList[] =
	{
		{	"2336",	"EUC-JP"	},	// Japanese
		{	"2352", "GB2312"	},	// Simplified Chinese
		{	"2368", "EUC-KR"	},	// Korean
		{	"2561", "Shift_JIS"	},	// Japanese
		{	"2562", "KOI8-R"	},	// Russian
		{	"2563", "Big5"		},	// Traditional Chinese

		{	nsnull, nsnull		}
	};

	stringEncoding.Truncate();

	PRUint32	loop = 0;
	while (encodingList[loop].numericEncoding != nsnull)
	{
		if (numericEncoding.Equals(encodingList[loop].numericEncoding))
		{
			stringEncoding = encodingList[loop].stringEncoding;
			break;
		}
		++loop;
	}
	return(NS_OK);
}



nsresult
InternetSearchDataSource::DoSearch(nsIRDFResource *source, nsIRDFResource *engine,
				const nsString &fullURL, const nsString &text)
{
	nsresult	rv;
	nsAutoString	textTemp(text);

//	Note: source can be null
//	if (!source)	return(NS_ERROR_NULL_POINTER);

	if (!engine)	return(NS_ERROR_NULL_POINTER);
	if (!mInner)	return(NS_RDF_NO_VALUE);

	nsCOMPtr<nsIUnicodeDecoder>	unicodeDecoder;
	nsAutoString			action, data, method, input, userVar;

	if (NS_FAILED(rv = FindData(engine, data)))	return(rv);
	if (data.Length() < 1)				return(NS_RDF_NO_VALUE);

	if (fullURL.Length() > 0)
	{
		action = fullURL;
		method = "get";
	}
	else
	{
		if (NS_FAILED(rv = GetData(data, "search", "action", action)))	return(rv);
		if (NS_FAILED(rv = GetData(data, "search", "method", method)))	return(rv);
	}

	nsAutoString	encodingStr, queryEncodingStr, resultEncodingStr;

	GetData(data, "interpret", "resultEncoding", encodingStr);	// decimal string values
	MapEncoding(encodingStr, resultEncodingStr);
	// rjc note: ignore "interpret/resultTranslationEncoding" as well as
	// "interpret/resultTranslationFont" since we always convert results to Unicode
	if (resultEncodingStr.Length() > 0)
	{
		NS_WITH_SERVICE(nsICharsetConverterManager, charsetConv,
			kCharsetConverterManagerCID, &rv);
		if (NS_SUCCEEDED(rv) && (charsetConv))
		{
			rv = charsetConv->GetUnicodeDecoder(&resultEncodingStr,
				getter_AddRefs(unicodeDecoder));
		}
	}

	GetData(data, "search", "queryEncoding", encodingStr);		// decimal string values
	MapEncoding(encodingStr, queryEncodingStr);
	if (queryEncodingStr.Length() > 0)
	{
		// convert from escaped-UTF_8, to unicode, and then to
		// the charset indicated by the dataset in question

		char	*utf8data = textTemp.ToNewUTF8String();
		if (utf8data)
		{
			NS_WITH_SERVICE(nsITextToSubURI, textToSubURI,
				kTextToSubURICID, &rv);
			if (NS_SUCCEEDED(rv) && (textToSubURI))
			{
				PRUnichar	*uni = nsnull;
				if (NS_SUCCEEDED(rv = textToSubURI->UnEscapeAndConvert("UTF-8", utf8data, &uni)) && (uni))
				{
					char	*charsetData = nsnull;
					if (NS_SUCCEEDED(rv = textToSubURI->ConvertAndEscape(nsCAutoString(queryEncodingStr), uni, &charsetData)) && (charsetData))
					{
						textTemp = charsetData;
						Recycle(charsetData);
					}
					Recycle(uni);
				}
			}
			Recycle(utf8data);
		}
	}

	if (fullURL.Length() > 0)
	{
	}
	else if (method.EqualsIgnoreCase("get"))
	{
		if (NS_FAILED(rv = GetInputs(data, userVar, textTemp, input)))	return(rv);
		if (input.Length() < 1)				return(NS_ERROR_UNEXPECTED);

		// HTTP Get method support
		action += "?";
		action += input;
	}

	nsCOMPtr<nsIInternetSearchContext>	context;
	if (NS_FAILED(rv = NS_NewInternetSearchContext(source, engine, unicodeDecoder, getter_AddRefs(context))))
		return(rv);
	if (!context)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIURI>	url;
	if (NS_SUCCEEDED(rv = NS_NewURI(getter_AddRefs(url), action)))
	{
		nsCOMPtr<nsIChannel>	channel;
		// XXX: Null LoadGroup ?
		if (NS_SUCCEEDED(rv = NS_OpenURI(getter_AddRefs(channel), url, nsnull)))
		{

			// send a "MultiSearch" header
			nsCOMPtr<nsIHTTPChannel>	httpMultiChannel = do_QueryInterface(channel);
			if (httpMultiChannel)
			{
				nsCOMPtr<nsIAtom>	multiSearchAtom = getter_AddRefs(NS_NewAtom("MultiSearch"));
				if (multiSearchAtom)
				{
					httpMultiChannel->SetRequestHeader(multiSearchAtom, "true");
				}
			}

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
InternetSearchDataSource::GetSearchEngineList(nsFileSpec nativeDir, PRBool checkMacFileType)
{
        nsresult			rv = NS_OK;

	if (!mInner)
	{
		return(NS_RDF_NO_VALUE);
	}

	for (nsDirectoryIterator i(nativeDir, PR_TRUE); i.Exists(); i++)
	{
		const nsFileSpec	fileSpec = (const nsFileSpec &)i;
		if (fileSpec.IsHidden())
		{
			continue;
		}

		const char		*childURL = fileSpec;

		if (fileSpec.IsDirectory())
		{
			GetSearchEngineList(fileSpec, checkMacFileType);
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
				if (checkMacFileType == PR_TRUE)
				{
					CInfoPBRec	cInfo;
					OSErr		err;

					err = fileSpec.GetCatInfo(cInfo);
					if ((err) || (cInfo.hFileInfo.ioFlFndrInfo.fdType != 'issp') ||
						(cInfo.hFileInfo.ioFlFndrInfo.fdCreator != 'fndf'))
						continue;
				}
#endif

				// check the extension
				nsAutoString	extension;
				if ((uri.Right(extension, 4) == 4) && (extension.EqualsIgnoreCase(".src")))
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

								// save description of search engine (if specified)
								nsAutoString	descValue;
								if (NS_SUCCEEDED(rv = GetData(data, "search", "description", descValue)))
								{
									nsCOMPtr<nsIRDFLiteral>	descLiteral;
									if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(descValue.GetUnicode(),
											getter_AddRefs(descLiteral))))
									{
										mInner->Assert(searchRes, kNC_Description, descLiteral, PR_TRUE);
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
InternetSearchDataSource::ReadFileContents(nsFileSpec fileSpec, nsString& sourceContents)
{
	nsresult			rv = NS_ERROR_FAILURE;
	PRUint32			contentsLen;
	char				*contents;

	sourceContents.Truncate();

	contentsLen = fileSpec.GetFileSize();
	if (contentsLen > 0)
	{
		contents = new char [contentsLen + 1];
		if (contents)
		{
			nsInputFileStream inputStream(fileSpec);	// defaults to read only
		       	PRInt32 howMany = inputStream.read(contents, contentsLen);
		        if (PRUint32(howMany) == contentsLen)
		        {
				contents[contentsLen] = '\0';
				sourceContents = contents;
				rv = NS_OK;
		        }
	        	delete [] contents;
	        	contents = nsnull;
		}
	}
	return(rv);
}



nsresult
InternetSearchDataSource::GetData(nsString &data, const char *sectionToFind, const char *attribToFind, nsString &value)
{
	nsAutoString	buffer(data);	
	nsresult	rv = NS_RDF_NO_VALUE;
	PRBool		inSection = PR_FALSE;

	nsAutoString	section("<");
	section += sectionToFind;

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
		PRInt32 equal = line.FindChar(PRUnichar('='));
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
InternetSearchDataSource::GetInputs(const nsString &data, nsString &userVar,
					const nsString &text, nsString &input)
{
	nsAutoString	buffer(data);
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
			if (line[0] != PRUnichar('<'))	continue;
			line.Cut(0, 1);
			inSection = PR_TRUE;
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
				userVar = nameAttrib;
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
InternetSearchDataSource::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt,
				nsIInputStream *aIStream, PRUint32 sourceOffset, PRUint32 aLength)
{
	if (!ctxt)	return(NS_ERROR_NO_INTERFACE);
	nsCOMPtr<nsIInternetSearchContext>	context = do_QueryInterface(ctxt);
	if (!context)	return(NS_ERROR_NO_INTERFACE);

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

	nsCOMPtr<nsIUnicodeDecoder>	decoder;
	context->GetUnicodeDecoder(getter_AddRefs(decoder));
	if (decoder)
	{
		char			*aBuffer = buffer;
		PRInt32			unicharBufLen = 0;
		decoder->GetMaxLength(aBuffer, aLength, &unicharBufLen);
		PRUnichar		*unichars = new PRUnichar [ unicharBufLen+1 ];
		do
		{
			PRInt32		srcLength = aLength;
			PRInt32		unicharLength = unicharBufLen;
			rv = decoder->Convert(aBuffer, &srcLength, unichars, &unicharLength);
			unichars[unicharLength]=0;  //add this since the unicode converters can't be trusted to do so.

			// Move the nsParser.cpp 00 -> space hack to here so it won't break UCS2 file

			// Hack Start
			for(PRInt32 i=0;i<unicharLength;i++)
				if(0x0000 == unichars[i])	unichars[i] = 0x0020;
			// Hack End

			context->AppendUnicodeBytes(unichars, unicharLength);
			// if we failed, we consume one byte by replace it with U+FFFD
			// and try conversion again.
			if(NS_FAILED(rv))
			{
				decoder->Reset();
				unsigned char	smallBuf[2];
				smallBuf[0] = 0xFF;
				smallBuf[1] = 0xFD;
				context->AppendBytes( (const char *)&smallBuf, 2L);
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
		context->AppendBytes(buffer, aLength);
	}

	delete [] buffer;
	buffer = nsnull;
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
					nsresult status, const PRUnichar *errorMsg) 
{
	if (!mInner)	return(NS_OK);

	if (mConnections)
	{
		mConnections->RemoveElement(channel);
	}

	nsCOMPtr<nsIInternetSearchContext>	context = do_QueryInterface(ctxt);
	if (!ctxt)	return(NS_ERROR_NO_INTERFACE);

	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	mParent;
	if (NS_FAILED(rv = context->GetParent(getter_AddRefs(mParent))))	return(rv);
//	Note: mParent can be null
//	if (!mParent)	return(NS_ERROR_NO_INTERFACE);

	nsCOMPtr<nsIRDFResource>	mEngine;
	if (NS_FAILED(rv = context->GetEngine(getter_AddRefs(mEngine))))	return(rv);
	if (!mEngine)	return(NS_ERROR_NO_INTERFACE);

	nsCOMPtr<nsIURI> aURL;
	rv = channel->GetURI(getter_AddRefs(aURL));
	if (NS_FAILED(rv)) return rv;

	// copy the engine's icon reference (if it has one) onto the result node
	nsCOMPtr<nsIRDFNode>		engineIconStatusNode = nsnull;
	mInner->GetTarget(mEngine, kNC_Icon, PR_TRUE, getter_AddRefs(engineIconStatusNode));
	if (engineIconStatusNode)
	{
		rv = mInner->Assert(mEngine, kNC_StatusIcon, engineIconStatusNode, PR_TRUE);
	}

	PRUnichar	*uniBuf = nsnull;
	if (NS_SUCCEEDED(rv = context->GetBuffer(&uniBuf)) && (uniBuf))
	{
		// Note: don't free uniBuf, its really a const *

		nsAutoString	htmlResults(uniBuf);
		context->Truncate();
		if (htmlResults.Length() > 0)
		{
			if (mParent)
			{
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
			}

			// parse up HTML results
			PRUint32	numResults = 0;
			rv = ParseHTML(aURL, mParent, mEngine, htmlResults, PR_FALSE, numResults);
			if (NS_SUCCEEDED(rv) && (!mParent) && (numResults == 0))
			{
				rv = ParseHTML(aURL, mParent, mEngine, htmlResults, PR_TRUE, numResults);
			}
		}
		else
		{
#ifdef	DEBUG_SEARCH_OUTPUT
			printf(" *** InternetSearchDataSourceCallback::OnStopRequest:  no data.\n\n");
#endif
		}
	}

	// (do this last) potentially remove the loading attribute
	nsCOMPtr<nsIRDFLiteral>	trueLiteral;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(nsAutoString("true").GetUnicode(), getter_AddRefs(trueLiteral))))
	{
		mInner->Unassert(mEngine, kNC_loading, trueLiteral);

		if (mConnections)
		{
			PRUint32	count=0;
			mConnections->Count(&count);
			if (count < 1)
			{
				if (mParent)
				{
					mInner->Unassert(mParent, kNC_loading, trueLiteral);
				}
			}
		}
	}

	return(NS_OK);
}



nsresult
InternetSearchDataSource::ParseHTML(nsIURI *aURL, nsIRDFResource *mParent, nsIRDFResource *mEngine,
	const nsString &htmlPage, PRBool useAllHREFsFlag, PRUint32 &numResults)
{
	nsAutoString	htmlResults(htmlPage);
	nsAutoString	data, engineStr;
	nsAutoString	resultListStartStr, resultListEndStr;
	nsAutoString	resultItemStartStr, resultItemEndStr;
	nsAutoString	relevanceStartStr, relevanceEndStr;
	nsAutoString	bannerStartStr, bannerEndStr, skiplocalStr;
	nsAutoString	priceStartStr, priceEndStr, availStartStr, availEndStr;
	PRBool		skipLocalFlag = PR_FALSE;
	nsresult	rv;

	numResults = 0;

	if (useAllHREFsFlag == PR_TRUE)
	{
		skipLocalFlag = PR_TRUE;
	}
	else
	{
		// get data out of graph
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

		GetData(data, "interpret", "resultListStart", resultListStartStr);
		GetData(data, "interpret", "resultListEnd", resultListEndStr);
		GetData(data, "interpret", "resultItemStart", resultItemStartStr);
		GetData(data, "interpret", "resultItemEnd", resultItemEndStr);
		GetData(data, "interpret", "relevanceStart", relevanceStartStr);
		GetData(data, "interpret", "relevanceEnd", relevanceEndStr);
		GetData(data, "interpret", "bannerStart", bannerStartStr);
		GetData(data, "interpret", "bannerEnd", bannerEndStr);
		GetData(data, "interpret", "skiplocal", skiplocalStr);
		if (skiplocalStr.EqualsIgnoreCase("true"))
		{
			skipLocalFlag = PR_TRUE;
		}

		GetData(data, "search", "name", engineStr);

		GetData(data, "interpret", "priceStart", priceStartStr);
		GetData(data, "interpret", "priceEnd", priceEndStr);
		GetData(data, "interpret", "availStart", availStartStr);
		GetData(data, "interpret", "availEnd", availEndStr);

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
	}

	// pre-compute host (we might discard URLs that match this)
	nsAutoString	hostStr;
	char		*hostName = nsnull;
	aURL->GetHost(&hostName);
	if (hostName)
	{
		hostStr = hostName;
		nsCRT::free(hostName);
		hostName = nsnull;
	}

	// pre-compute server path (we might discard URLs that match this)
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
			PRInt32	bannerEnd = htmlResults.Find(bannerEndStr, PR_TRUE, bannerStart + bannerStartStr.Length());
			if (bannerEnd > bannerStart)
			{
				nsAutoString	htmlBanner;
				htmlResults.Mid(htmlBanner, bannerStart, bannerEnd - bannerStart - 1);
				if (htmlBanner.Length() > 0)
				{
					const PRUnichar	*bannerUni = htmlBanner.GetUnicode();
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

	PRBool	hasPriceFlag = PR_FALSE, hasAvailabilityFlag = PR_FALSE, hasRelevanceFlag = PR_FALSE;

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

		nsAutoString	resultItem;
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

		nsAutoString	hrefStr;
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

		char		*absURIStr = nsnull;
		if (NS_SUCCEEDED(rv = NS_MakeAbsoluteURI(nsCAutoString(hrefStr),
			aURL, &absURIStr)) && (absURIStr))
		{
			hrefStr = absURIStr;

			nsCOMPtr<nsIURI>	absURI;
			rv = NS_NewURI(getter_AddRefs(absURI), absURIStr);
			nsCRT::free(absURIStr);
			absURIStr = nsnull;

			if (absURI)
			{
				char	*absPath = nsnull;
				absURI->GetPath(&absPath);
				if (absPath)
				{
					nsAutoString	absPathStr(absPath);
					nsCRT::free(absPath);
					PRInt32 pathOptionsOffset = absPathStr.FindChar(PRUnichar('?'));
					if (pathOptionsOffset >= 0)
						absPathStr.Truncate(pathOptionsOffset);
					PRBool	pathsMatchFlag = serverPathStr.EqualsIgnoreCase(absPathStr);
					if (pathsMatchFlag == PR_TRUE)	continue;
				}

				if ((hostStr.Length() > 0) && (skipLocalFlag == PR_TRUE))
				{
					char		*absHost = nsnull;
					absURI->GetHost(&absHost);
					if (absHost)
					{
						PRBool	hostsMatchFlag = hostStr.EqualsIgnoreCase(absHost);
						nsCRT::free(absHost);
						if (hostsMatchFlag == PR_TRUE)	continue;
					}
				}
			}
		}

		nsAutoString	site(hrefStr);

#ifdef	DEBUG_SEARCH_OUTPUT
		printf("HREF: '%s'\n", href);
#endif

		nsCOMPtr<nsIRDFResource>	res;

// #define	OLDWAY
#ifdef	OLDWAY
		rv = gRDFService->GetResource(nsCAutoString(hrefStr), getter_AddRefs(res));
#else		
		// save HREF attribute as URL
		if (NS_SUCCEEDED(rv = gRDFService->GetAnonymousResource(getter_AddRefs(res))))
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
		ConvertEntities(nameStr);

		// look for Name (if it isn't already set)
		nsCOMPtr<nsIRDFNode>		oldNameRes = nsnull;
#ifdef	OLDWAY
		mInner->GetTarget(res, kNC_Name, PR_TRUE, getter_AddRefs(oldNameRes));
#endif
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

		// look for price
		nsAutoString	priceItem;
		PRInt32		priceStart;
		if ((priceStart = resultItem.Find(priceStartStr, PR_TRUE)) >= 0)
		{
			PRInt32	priceEnd = resultItem.Find(priceEndStr, PR_TRUE, priceStart + priceStartStr.Length());
			if (priceEnd > priceStart)
			{
				resultItem.Mid(priceItem, priceStart + priceStartStr.Length(),
					priceEnd - priceStart - priceStartStr.Length());

				ConvertEntities(priceItem);
			}
		}
		if (priceItem.Length() > 0)
		{
			const PRUnichar		*priceUni = priceItem.GetUnicode();
			nsCOMPtr<nsIRDFLiteral>	priceLiteral;
			if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(priceUni, getter_AddRefs(priceLiteral))))
			{
				if (priceLiteral)
				{
					mInner->Assert(res, kNC_Price, priceLiteral, PR_TRUE);
					hasPriceFlag = PR_TRUE;
				}
			}

			PRInt32	priceCharStartOffset = priceItem.FindCharInSet("1234567890");
			if (priceCharStartOffset >= 0)
			{
				priceItem.Cut(0, priceCharStartOffset);
				PRInt32	priceErr;
				float	val = priceItem.ToFloat(&priceErr);
				if (priceItem.FindChar(PRUnichar('.')) >= 0)	val *= 100;

				nsCOMPtr<nsIRDFInt>	priceSortLiteral;
				if (NS_SUCCEEDED(rv = gRDFService->GetIntLiteral((PRInt32)val, getter_AddRefs(priceSortLiteral))))
				{
					if (priceSortLiteral)
					{
						mInner->Assert(res, kNC_PriceSort, priceSortLiteral, PR_TRUE);
					}
				}
			}
			
		}

		// look for availability
		nsAutoString	availItem;
		PRInt32		availStart;
		if ((availStart = resultItem.Find(availStartStr, PR_TRUE)) >= 0)
		{
			PRInt32	availEnd = resultItem.Find(availEndStr, PR_TRUE, availStart + availStartStr.Length());
			if (availEnd > availStart)
			{
				resultItem.Mid(availItem, availStart + availStartStr.Length(),
					availEnd - availStart - availStartStr.Length());

				ConvertEntities(availItem);
			}
		}
		if (availItem.Length() > 0)
		{
			const PRUnichar		*availUni = availItem.GetUnicode();
			nsCOMPtr<nsIRDFLiteral>	availLiteral;
			if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(availUni, getter_AddRefs(availLiteral))))
			{
				if (availLiteral)
				{
					mInner->Assert(res, kNC_Availability, availLiteral, PR_TRUE);
					hasAvailabilityFlag = PR_TRUE;
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
#ifdef	OLDWAY
		mInner->GetTarget(res, kNC_Relevance, PR_TRUE, getter_AddRefs(oldRelRes));
#endif
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
						hasRelevanceFlag = PR_TRUE;
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
#ifdef	OLDWAY
		mInner->GetTarget(res, kNC_Engine, PR_TRUE, getter_AddRefs(oldEngineRes));
#endif
		if ((!oldEngineRes) && (data.Length() > 0))
		{
			if (engineStr.Length() > 0)
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
		if (mParent)
		{
			mInner->HasAssertion(mParent, kNC_Child, res, PR_TRUE, &parentHasChildFlag);
		}
		if (parentHasChildFlag == PR_FALSE)
#endif
		{
			if (mParent)
			{
				rv = mInner->Assert(mParent, kNC_Child, res, PR_TRUE);
			}
		}

		// Persist this under kNC_LastSearchRoot
		if (mInner)
		{
			rv = mInner->Assert(kNC_LastSearchRoot, kNC_Child, res, PR_TRUE);
		}

		++numResults;

	}

	// set hints so that the appropriate columns can be displayed
	if (mParent)
	{
		if (hasPriceFlag == PR_TRUE)		SetHint(mParent, kNC_Price);
		if (hasAvailabilityFlag == PR_TRUE)	SetHint(mParent, kNC_Availability);
		if (hasRelevanceFlag == PR_TRUE)	SetHint(mParent, kNC_Relevance);
	}

	return(NS_OK);
}



nsresult
InternetSearchDataSource::SetHint(nsIRDFResource *mParent, nsIRDFResource *hintRes)
{
	if (!mInner)	return(NS_OK);

	nsresult		rv;
	nsCOMPtr<nsIRDFLiteral>	trueLiteral;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(nsAutoString("true").GetUnicode(), getter_AddRefs(trueLiteral))))
	{
		PRBool		hasAssertionFlag = PR_FALSE;
		if (NS_SUCCEEDED(rv = mInner->HasAssertion(mParent, hintRes, trueLiteral, PR_TRUE, &hasAssertionFlag))
			&& (hasAssertionFlag == PR_FALSE))
		{
			rv = mInner->Assert(mParent, hintRes, trueLiteral, PR_TRUE);
		}
	}
	return(rv);
}



nsresult
InternetSearchDataSource::ConvertEntities(nsString &nameStr, PRBool removeHTMLFlag,
					PRBool removeCRLFsFlag, PRBool trimWhiteSpaceFlag)
{
	PRInt32	startOffset = 0, ampOffset, semiOffset, offset;

	// do this before converting entities
	if (removeHTMLFlag == PR_TRUE)
	{
		// munge out anything inside of HTML "<" / ">" tags
		while ((offset = nameStr.FindChar(PRUnichar('<'), PR_FALSE, 0)) >= 0)
		{
			PRInt32	offsetEnd = nameStr.FindChar(PRUnichar('>'), PR_FALSE, offset+1);
			if (offsetEnd <= offset)	break;
			nameStr.Cut(offset, offsetEnd - offset + 1);
		}
	}
	while ((ampOffset = nameStr.FindChar(PRUnichar('&'), PR_FALSE, startOffset)) >= 0)
	{
		if ((semiOffset = nameStr.FindChar(PRUnichar(';'), PR_FALSE, ampOffset+1)) <= ampOffset)
			break;

		nsAutoString	entityStr;
		nameStr.Mid(entityStr, ampOffset, semiOffset-ampOffset+1);
		nameStr.Cut(ampOffset, semiOffset-ampOffset+1);

		PRUnichar	entityChar = 0;
		if (entityStr.EqualsIgnoreCase("&quot;"))	entityChar = PRUnichar('\"');
		if (entityStr.EqualsIgnoreCase("&amp;"))	entityChar = PRUnichar('&');
		if (entityStr.EqualsIgnoreCase("&nbsp;"))	entityChar = PRUnichar(' ');
		if (entityStr.EqualsIgnoreCase("&lt;"))		entityChar = PRUnichar('<');
		if (entityStr.EqualsIgnoreCase("&gt;"))		entityChar = PRUnichar('>');
		if (entityStr.EqualsIgnoreCase("&pound;"))	entityChar = PRUnichar(163);

		startOffset = ampOffset;
		if (entityChar != 0)
		{
			nameStr.Insert(entityChar, ampOffset);
			++startOffset;
		}
	}

	if (removeCRLFsFlag == PR_TRUE)
	{
		// cut out any CRs or LFs
		while ((offset = nameStr.FindCharInSet("\n\r", 0)) >= 0)
		{
			nameStr.Cut(offset, 1);
		}
	}

	if (trimWhiteSpaceFlag == PR_TRUE)
	{
		// trim name
		nameStr = nameStr.Trim(" \t");
	}

	return(NS_OK);
}
