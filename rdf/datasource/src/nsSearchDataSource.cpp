/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsEscape.h"

#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsIRDFSearch.h"

#ifdef	XP_WIN
#include "windef.h"
#include "winbase.h"
#endif



static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,    NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);

static const char kURINC_SearchRoot[] = "NC:SearchEngineRoot";



class SearchDataSourceCallback : public nsIStreamListener
{
private:
	nsIRDFDataSource	*mDataSource;
	nsIRDFResource		*mParent;
	static PRInt32		gRefCnt;

    // pseudo-constants
	static nsIRDFResource	*kNC_loading;
	static nsIRDFResource	*kNC_Child;

	char			*mLine;

public:

	NS_DECL_ISUPPORTS

			SearchDataSourceCallback(nsIRDFDataSource *ds, nsIRDFResource *parent);
	virtual		~SearchDataSourceCallback(void);

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

class SearchDataSource : public nsIRDFSearchDataSource
{
private:
	char			*mURI;
	nsVoidArray		*mObservers;

	static PRInt32		gRefCnt;

    // pseudo-constants
	static nsIRDFResource		*kNC_SearchRoot;
	static nsIRDFResource		*kNC_Child;
	static nsIRDFResource		*kNC_Name;
	static nsIRDFResource		*kNC_URL;
	static nsIRDFResource		*kRDF_InstanceOf;
	static nsIRDFResource		*kRDF_type;

protected:
	static nsIRDFDataSource		*mInner;

public:

	NS_DECL_ISUPPORTS

			SearchDataSource(void);
	virtual		~SearchDataSource(void);

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
	NS_IMETHOD	GetAllResources(nsISimpleEnumerator** aResult);
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


	// helper methods
static PRBool		isEngineURI(nsIRDFResource* aResource);
static PRBool		isSearchURI(nsIRDFResource* aResource);
static nsresult		BeginSearchRequest(nsIRDFResource *source);
static nsresult		DoSearch(nsIRDFResource *source, nsString text, nsString data);
static nsresult		GetSearchEngineList(nsISimpleEnumerator **aResult);
static nsresult		ReadFileContents(char *basename, nsString & sourceContents);
static nsresult		GetData(nsString data, char *sectionToFind, char *attribToFind, nsString &value);
static nsresult		GetInputs(nsString data, nsString text, nsString &input);
static nsresult		GetName(nsIRDFResource *source, nsIRDFLiteral** aResult);
static nsresult		GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult);
static PRBool		isVisible(const nsNativeFileSpec& file);

};



static	nsIRDFService		*gRDFService = nsnull;
static	SearchDataSource	*gSearchDataSource = nsnull;

PRInt32				SearchDataSource::gRefCnt;
nsIRDFDataSource		*SearchDataSource::mInner;

nsIRDFResource			*SearchDataSource::kNC_SearchRoot;
nsIRDFResource			*SearchDataSource::kNC_Child;
nsIRDFResource			*SearchDataSource::kNC_Name;
nsIRDFResource			*SearchDataSource::kNC_URL;
nsIRDFResource			*SearchDataSource::kRDF_InstanceOf;
nsIRDFResource			*SearchDataSource::kRDF_type;

PRInt32				SearchDataSourceCallback::gRefCnt;

nsIRDFResource			*SearchDataSourceCallback::kNC_Child;
nsIRDFResource			*SearchDataSourceCallback::kNC_loading;



PRBool
SearchDataSource::isEngineURI(nsIRDFResource *r)
{
	PRBool		isEngineURIFlag = PR_FALSE;
        nsXPIDLCString	uri;
	
	r->GetValue( getter_Copies(uri) );
	if (!strncmp(uri, "engine://", 9))
	{
		isEngineURIFlag = PR_TRUE;
	}
	return(isEngineURIFlag);
}



PRBool
SearchDataSource::isSearchURI(nsIRDFResource *r)
{
	PRBool		isSearchURIFlag = PR_FALSE;
        nsXPIDLCString	uri;
	
	r->GetValue( getter_Copies(uri) );
	if (!strncmp(uri, "search://", 9))
	{
		isSearchURIFlag = PR_TRUE;
	}
	return(isSearchURIFlag);
}



SearchDataSource::SearchDataSource(void)
	: mURI(nsnull),
	  mObservers(nsnull)
{
	NS_INIT_REFCNT();

	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
			nsIRDFService::GetIID(), (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(kURINC_SearchRoot,                   &kNC_SearchRoot);
		gRDFService->GetResource(NC_NAMESPACE_URI "child",            &kNC_Child);
		gRDFService->GetResource(NC_NAMESPACE_URI "Name",             &kNC_Name);
		gRDFService->GetResource(NC_NAMESPACE_URI "URL",              &kNC_URL);

		gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf",      &kRDF_InstanceOf);
		gRDFService->GetResource(RDF_NAMESPACE_URI "type",            &kRDF_type);

		gSearchDataSource = this;
	}
}



SearchDataSource::~SearchDataSource (void)
{
	gRDFService->UnregisterDataSource(this);

	PL_strfree(mURI);

	delete mObservers; // we only hold a weak ref to each observer

	if (--gRefCnt == 0)
	{
		NS_RELEASE(kNC_SearchRoot);
		NS_RELEASE(kNC_Child);
		NS_RELEASE(kNC_Name);
		NS_RELEASE(kNC_URL);
		NS_RELEASE(kRDF_InstanceOf);
		NS_RELEASE(kRDF_type);

		NS_RELEASE(mInner);

		gSearchDataSource = nsnull;
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
		gRDFService = nsnull;
	}
}



NS_IMPL_ISUPPORTS(SearchDataSource, nsIRDFDataSource::GetIID());



NS_IMETHODIMP
SearchDataSource::Init(const char *uri)
{
	NS_PRECONDITION(uri != nsnull, "null ptr");
	if (! uri)
		return NS_ERROR_NULL_POINTER;

	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
		nsnull, nsIRDFDataSource::GetIID(), (void **)&mInner)))
		return rv;
	if (NS_FAILED(rv = mInner->Init(uri)))
		return rv;

	if ((mURI = PL_strdup(uri)) == nsnull)
		return rv;

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this, PR_FALSE)))
		return rv;
	return NS_OK;
}



NS_IMETHODIMP
SearchDataSource::GetURI(char **uri)
{
	NS_PRECONDITION(uri != nsnull, "null ptr");
	if (! uri)
		return NS_ERROR_NULL_POINTER;

	if ((*uri = nsXPIDLCString::Copy(mURI)) == nsnull)
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

	// we only have positive assertions in the file system data source.
	if (! tv)
		return(rv);

	if (source == kNC_SearchRoot)
	{
	}
	else if (isEngineURI(source))
	{
		if (property == kNC_Name)
		{
			nsCOMPtr<nsIRDFLiteral> name;
			rv = GetName(source, getter_AddRefs(name));
			if (NS_FAILED(rv)) return rv;
			if (!name)	return(NS_RDF_NO_VALUE);
			return name->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
		}
		else if (property == kNC_URL)
		{
			nsCOMPtr<nsIRDFLiteral> url;
			rv = GetURL(source, getter_AddRefs(url));
			if (NS_FAILED(rv)) return rv;
			if (!url)	return(NS_RDF_NO_VALUE);
			return url->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
		}
	}
	else if (isSearchURI(source))
	{
		// XXX to do
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

	// we only have positive assertions in the file system data source.
	if (! tv)
		return NS_RDF_NO_VALUE;

	nsresult rv;

	if (source == kNC_SearchRoot)
	{
		if (property == kNC_Child)
		{
			return GetSearchEngineList(targets);
		}
	}
	else if (isEngineURI(source))
	{
		if (property == kNC_Name)
		{
			nsCOMPtr<nsIRDFLiteral> name;
			rv = GetName(source, getter_AddRefs(name));
			if (NS_FAILED(rv)) return rv;

			nsISimpleEnumerator* result = new nsSingletonEnumerator(name);
			if (! result)
				return NS_ERROR_OUT_OF_MEMORY;

			NS_ADDREF(result);
			*targets = result;
			return NS_OK;
		}
		else if (property == kNC_URL)
		{
			nsCOMPtr<nsIRDFLiteral> url;
			rv = GetURL(source, getter_AddRefs(url));
			if (NS_FAILED(rv)) return rv;

			nsISimpleEnumerator* result = new nsSingletonEnumerator(url);
			if (! result)
				return NS_ERROR_OUT_OF_MEMORY;

			NS_ADDREF(result);
			*targets = result;
			return NS_OK;
		}
	}
	else if (isSearchURI(source))
	{
		if (property == kNC_Child)
		{
			BeginSearchRequest(source);
			// Note: initially fall through to empty
		}
	}
	return NS_NewEmptyEnumerator(targets);
}



NS_IMETHODIMP
SearchDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
//	PR_ASSERT(0);
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
SearchDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
//	PR_ASSERT(0);
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

	// we only have positive assertions in the file system data source.
	if (! tv)
	{
		*hasAssertion = PR_FALSE;
		return NS_OK;
        }
#if 0
	if ((source == kNC_SearchRoot) || isEngineURI(source))
	{
		if (property == kRDF_type)
		{
			nsCOMPtr<nsIRDFResource> resource( do_QueryInterface(target) );
			if (resource.get() == kRDF_type)
			{
				*hasAssertion = PR_TRUE;
				return NS_OK;
			}
		}
	}
#endif
	*hasAssertion = PR_FALSE;
	return NS_OK;
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

	if ((source == kNC_SearchRoot) || isSearchURI(source))
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
            return NS_OK;
	}

        return NS_NewEmptyEnumerator(labels);
}



NS_IMETHODIMP
SearchDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	NS_NOTYETIMPLEMENTED("sorry!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
SearchDataSource::AddObserver(nsIRDFObserver *n)
{
    NS_PRECONDITION(n != nsnull, "null ptr");
    if (! n)
        return NS_ERROR_NULL_POINTER;

	if (nsnull == mObservers)
	{
		if ((mObservers = new nsVoidArray()) == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
	}
	mObservers->AppendElement(n);
	return NS_OK;
}



NS_IMETHODIMP
SearchDataSource::RemoveObserver(nsIRDFObserver *n)
{
	NS_PRECONDITION(n != nsnull, "null ptr");
	if (! n)
		return NS_ERROR_NULL_POINTER;

	if (nsnull == mObservers)
		return NS_OK;
	mObservers->RemoveElement(n);
	return NS_OK;
}



NS_IMETHODIMP
SearchDataSource::Flush()
{
	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
SearchDataSource::GetAllCommands(nsIRDFResource* source,
                                     nsIEnumerator/*<nsIRDFResource>*/** commands)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
SearchDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                       nsIRDFResource*   aCommand,
                                       nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                       PRBool* aResult)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
SearchDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
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
	}
	NS_ADDREF(gSearchDataSource);
	*result = gSearchDataSource;
	return NS_OK;
}



nsresult
SearchDataSource::BeginSearchRequest(nsIRDFResource *source)
{
        nsresult		rv = NS_OK;
	char			*sourceURI = nsnull;

	if (NS_FAILED(rv = source->GetValue(&sourceURI)))
		return(rv);
	nsAutoString		uri(sourceURI);
	if (uri.Find("search://") != 0)
		return(NS_ERROR_FAILURE);
	uri.Cut(0, strlen("search://"));

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
				if (value.Find("engine://") == 0)
				{
					value.Cut(0, 9);

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
		char *basename = (char *)(engineArray->ElementAt(0));
		engineArray->RemoveElementAt(0);
		if (!basename)	continue;

		// process a search engine
		basename = nsUnescape(basename);
		if (!basename)	continue;
		
		nsAutoString	data;
		if (NS_SUCCEEDED(rv = ReadFileContents(basename, data)))
		{
#ifdef	DEBUG
			printf("Search engine to query: '%s'\n", basename);
#endif
			DoSearch(source, text, data);
		}

		delete [] basename;
		basename = nsnull;
	}
	
	delete engineArray;
	engineArray = nsnull;

	return(rv);
}



nsresult
SearchDataSource::DoSearch(nsIRDFResource *source, nsString text, nsString data)
{
	nsAutoString	action, method, input;
	nsresult	rv;

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
	if (cAction)	delete [] cAction;
	if (cMethod)	delete [] cMethod;
	if (cInput)	delete [] cInput;
#endif

	if (input.Length() < 1)
		return(NS_ERROR_UNEXPECTED);

	if (method.EqualsIgnoreCase("get"))
	{
		// HTTP Get method support
		action += "?";
		action += input;
		char	*searchURL = action.ToNewCString();
		if (searchURL)
		{
#ifdef	DEBUG
			printf("Search GET URL: '%s'\n\n", searchURL);
#endif
			nsIURL		*url;
			if (NS_SUCCEEDED(rv = NS_NewURL(&url, (const char*) searchURL)))
			{
				SearchDataSourceCallback *callback = new SearchDataSourceCallback(mInner, source);
				if (nsnull != callback)
				{
					rv = NS_OpenURL(url, NS_STATIC_CAST(nsIStreamListener *, callback));
				}
			}
			delete [] searchURL;
			searchURL = nsnull;
		}
	}
	else if (method.EqualsIgnoreCase("post"))
	{
		// HTTP Post method support

		// XXX implement
	}
	return(NS_OK);
}



nsresult
SearchDataSource::GetSearchEngineList(nsISimpleEnumerator** aResult)
{
        nsresult			rv;
        nsCOMPtr<nsISupportsArray>	engines;

        rv = NS_NewISupportsArray(getter_AddRefs(engines));
        if (NS_FAILED(rv))
        {
        	return(rv);
        }

#ifdef	XP_MAC
	// on Mac, use system's search files
	nsSpecialSystemDirectory	searchSitesDir(nsSpecialSystemDirectory::Mac_InternetSearchDirectory);
	nsNativeFileSpec 		nativeDir(searchSitesDir);
	for (nsDirectoryIterator i(nativeDir); i.Exists(); i++)
	{
		const nsNativeFileSpec	nativeSpec = (const nsNativeFileSpec &)i;
		if (!isVisible(nativeSpec))	continue;
		nsFileURL		fileURL(nativeSpec);
		const char		*childURL = fileURL.GetAsString();
		if (childURL != nsnull)
		{
			nsAutoString	uri(childURL);
			PRInt32		len = uri.Length();
			if (len > 4)
			{
				nsAutoString	extension;
				if ((uri.Right(extension, 4) == 4) && (extension.EqualsIgnoreCase(".src")))
				{
					PRInt32	slashOffset = uri.RFind("/");
					if (slashOffset > 0)
					{
						uri.Cut(0, slashOffset+1);
						
						nsAutoString	searchURL("engine://");
						searchURL += uri;

						nsCOMPtr<nsIRDFResource>	searchRes;
						char		*searchURI = searchURL.ToNewCString();
						if (searchURI)
						{
							gRDFService->GetResource(searchURI, getter_AddRefs(searchRes));
							engines->AppendElement(searchRes);
							delete [] searchURI;
							searchURI = nsnull;
						}
					}
				}
			}
		}
	}
#else
	// on other platforms, use our search files
#endif

        nsISimpleEnumerator* result = new nsArrayEnumerator(engines);
        if (! result)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(result);
        *aResult = result;

	return NS_OK;
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
	char		*basename = file.GetLeafName();
	if (nsnull != basename)
	{
		if ((!strcmp(basename, ".")) || (!strcmp(basename, "..")))
		{
			isVisible = PR_FALSE;
		}
		nsCRT::free(basename);
	}
#endif

	return(isVisible);
}



nsresult
SearchDataSource::ReadFileContents(char *basename, nsString& sourceContents)
{
	nsresult			rv = NS_OK;

#ifdef	XP_MAC
	nsSpecialSystemDirectory	searchEngine(nsSpecialSystemDirectory::Mac_InternetSearchDirectory);
#else
	nsSpecialSystemDirectory	searchEngine(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
	// XXX we should get this from prefs.
	searchEngine += "res";
	searchEngine += "rdf";
	searchEngine += "search";
#endif
	searchEngine += basename;

	nsInputFileStream		searchFile(searchEngine);
	if (! searchFile.is_open())
	{
		NS_ERROR("unable to open file");
		return(NS_ERROR_FAILURE);
	}
	nsRandomAccessInputStream	stream(searchFile);
	char				buffer[1024];
	while (!stream.eof())
	{
		stream.readline(buffer, sizeof(buffer)-1 );
		sourceContents += buffer;
		sourceContents += "\n";
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
		PRInt32 eol = buffer.FindCharInSet("\r\n");
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
			// XXX Use RFind for now as it can ignore case
			PRInt32	sectionOffset = line.RFind(section, PR_TRUE);
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
	nsAutoString	buffer(data);	
	nsresult	rv = NS_OK;
	PRBool		inSection = PR_FALSE;

	while(buffer.Length() > 0)
	{
		PRInt32 eol = buffer.FindCharInSet("\r\n");
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
			// XXX Use RFind for now as it can ignore case
			PRInt32	sectionOffset = line.Find(section);
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
		if (line.RFind("input", PR_TRUE) == 0)
		{
			line.Cut(0, 6);
			line = line.Trim(" \t");
			
			// first look for name attribute
			nsAutoString	nameAttrib("");

			PRInt32	nameOffset = line.RFind("name", PR_TRUE);
			if (nameOffset >= 0)
			{
				PRInt32 equal = line.Find(PRUnichar('='), nameOffset);
				if (equal >= 0)
				{
					PRInt32	startQuote = line.Find(PRUnichar('\"'), equal + 1);
					if (startQuote >= 0)
					{
						PRInt32	endQuote = line.Find(PRUnichar('\"'), startQuote + 1);
						if (endQuote >= 0)
						{
							line.Mid(nameAttrib, startQuote+1, endQuote-startQuote-1);
						}
					}
				}
			}
			if (nameAttrib.Length() <= 0)	continue;

			// first look for value attribute
			nsAutoString	valueAttrib("");

			PRInt32	valueOffset = line.RFind("value", PR_TRUE);
			if (valueOffset >= 0)
			{
				PRInt32 equal = line.Find(PRUnichar('='), valueOffset);
				if (equal >= 0)
				{
					PRInt32	startQuote = line.Find(PRUnichar('\"'), equal + 1);
					if (startQuote >= 0)
					{
						PRInt32	endQuote = line.Find(PRUnichar('\"'), startQuote + 1);
						if (endQuote >= 0)
						{
							line.Mid(valueAttrib, startQuote+1, endQuote-startQuote-1);
						}
					}
					else
					{
						// if value attribute's "value" isn't quoted, get the first word... ?
						PRInt32 theEnd = line.FindCharInSet(" >\t", equal);
						if (theEnd >= 0)
						{
							line.Mid(valueAttrib, equal+1, theEnd-equal-1);
							valueAttrib.Trim(" \t");
						}
					}
				}
			}
			else if (line.RFind("user", PR_TRUE) >= 0)
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
SearchDataSource::GetName(nsIRDFResource *source, nsIRDFLiteral **aResult)
{
	if (!source)	return(NS_RDF_NO_VALUE);
	nsresult	rv;
	nsAutoString	data;

        nsXPIDLCString	uri;
	source->GetValue( getter_Copies(uri) );
	nsAutoString	name(uri);
	name.Cut(0, strlen("engine://"));
	char *basename = name.ToNewCString();
	if (!basename)	return(NS_ERROR_NULL_POINTER);
	basename = nsUnescape(basename);

	if (NS_SUCCEEDED(rv = ReadFileContents(basename, data)))
	{
		nsAutoString	nameValue;
		if (NS_SUCCEEDED(rv = GetData(data, "search", "name", nameValue)))
		{
		        nsIRDFLiteral	*nameLiteral;
		        gRDFService->GetLiteral(nameValue.GetUnicode(), &nameLiteral);
		        *aResult = nameLiteral;
			rv = NS_OK;
		}
		else
		{
			rv = NS_RDF_NO_VALUE;
		}
	}
	return(rv);
}



nsresult
SearchDataSource::GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult)
{
        nsXPIDLCString	uri;
	source->GetValue( getter_Copies(uri) );
	nsAutoString	url(uri);
	nsIRDFLiteral	*literal;
	gRDFService->GetLiteral(url.GetUnicode(), &literal);
        *aResult = literal;
        return NS_OK;
}



// Search class for Netlib callback



SearchDataSourceCallback::SearchDataSourceCallback(nsIRDFDataSource *ds, nsIRDFResource *parent)
	: mDataSource(ds),
	  mParent(parent),
	  mLine(nsnull)
{
	NS_INIT_REFCNT();
	NS_ADDREF(mDataSource);
	NS_ADDREF(mParent);

	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   nsIRDFService::GetIID(),
                                                   (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(NC_NAMESPACE_URI "child", &kNC_Child);
		gRDFService->GetResource(NC_NAMESPACE_URI "loading", &kNC_loading);
	}
}



SearchDataSourceCallback::~SearchDataSourceCallback()
{
	NS_IF_RELEASE(mDataSource);
	NS_IF_RELEASE(mParent);

	if (mLine)
	{
		delete [] mLine;
		mLine = nsnull;
	}

	if (--gRefCnt == 0)
	{
		NS_RELEASE(kNC_Child);
		NS_RELEASE(kNC_loading);
	}
}



// stream observer methods



NS_IMETHODIMP
SearchDataSourceCallback::OnStartBinding(nsIURL *aURL, const char *aContentType)
{
	nsAutoString		trueStr("true");
	nsIRDFLiteral		*literal = nsnull;
	nsresult		rv;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(trueStr.GetUnicode(), &literal)))
	{
		mDataSource->Assert(mParent, kNC_loading, literal, PR_TRUE);
		NS_RELEASE(literal);
	}
	return(NS_OK);
}



NS_IMETHODIMP
SearchDataSourceCallback::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax) 
{
	return(NS_OK);
}



NS_IMETHODIMP
SearchDataSourceCallback::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
	return(NS_OK);
}



NS_IMETHODIMP
SearchDataSourceCallback::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg) 
{
	nsAutoString		trueStr("true");
	nsIRDFLiteral		*literal = nsnull;
	nsresult		rv;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(trueStr.GetUnicode(), &literal)))
	{
		mDataSource->Unassert(mParent, kNC_loading, literal);
		NS_RELEASE(literal);
	}
	if (mLine)
	{
		printf("\n\nSearch results:\n%s\n", mLine);
		delete [] mLine;
		mLine = nsnull;
	}
	return(NS_OK);
}



// stream listener methods



NS_IMPL_ISUPPORTS(SearchDataSourceCallback, nsIRDFSearchDataSourceCallback::GetIID());



NS_IMETHODIMP
SearchDataSourceCallback::GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo)
{
	return(NS_OK);
}



NS_IMETHODIMP
SearchDataSourceCallback::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
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
				printf("Search datasource read failure.\n");
				break;
			}
			if (numBytes != count)
			{
				printf("Search datasource read # of bytes failure.\n");
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
