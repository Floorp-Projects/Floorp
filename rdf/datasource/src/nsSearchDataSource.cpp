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
#include "nsIRDFSearch.h"
#include "nsEnumeratorUtils.h"

#include "nsEscape.h"

#ifdef	XP_WIN
#include "windef.h"
#include "winbase.h"
#endif



static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);

static const char kURINC_SearchRoot[] = "NC:SearchRoot";

class SearchDataSource : public nsIRDFSearchDataSource
{
private:
	char			*mURI;
	nsVoidArray		*mObservers;

    static PRInt32 gRefCnt;

    // pseudo-constants
	static nsIRDFResource		*kNC_SearchRoot;
	static nsIRDFResource		*kNC_Child;
	static nsIRDFResource		*kNC_Name;
	static nsIRDFResource		*kNC_URL;
	static nsIRDFResource		*kRDF_InstanceOf;
	static nsIRDFResource		*kRDF_type;

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
static nsresult		GetSearchEngineList(nsISimpleEnumerator **aResult);
static nsresult		ReadFileContents(nsIRDFResource *source, nsString & sourceContents);
static nsresult		GetData(nsString data, char *attribToFind, nsString &value);
static nsresult		GetName(nsIRDFResource *source, nsIRDFLiteral** aResult);
static nsresult		GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult);
static PRBool		isVisible(const nsNativeFileSpec& file);

};



static	nsIRDFService		*gRDFService = nsnull;
static	SearchDataSource	*gSearchDataSource = nsnull;

PRInt32				SearchDataSource::gRefCnt;

nsIRDFResource			*SearchDataSource::kNC_SearchRoot;
nsIRDFResource			*SearchDataSource::kNC_Child;
nsIRDFResource			*SearchDataSource::kNC_Name;
nsIRDFResource			*SearchDataSource::kNC_URL;
nsIRDFResource			*SearchDataSource::kRDF_InstanceOf;
nsIRDFResource			*SearchDataSource::kRDF_type;




PRBool
SearchDataSource::isEngineURI(nsIRDFResource *r)
{
	PRBool		isEngineURIFlag = PR_FALSE;
        nsXPIDLCString uri;
	
	r->GetValue( getter_Copies(uri) );
	if (!strncmp(uri, "engine://", 9))
	{
		isEngineURIFlag = PR_TRUE;
	}
	return(isEngineURIFlag);
}



SearchDataSource::SearchDataSource(void)
	: mURI(nsnull),
	  mObservers(nsnull)
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   nsIRDFService::GetIID(),
                                                   (nsISupports**) &gRDFService);

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

    if (--gRefCnt == 0) {
        NS_RELEASE(kNC_SearchRoot);
        NS_RELEASE(kNC_Child);
        NS_RELEASE(kNC_Name);
        NS_RELEASE(kNC_URL);
        NS_RELEASE(kRDF_InstanceOf);
        NS_RELEASE(kRDF_type);

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

	if (source == kNC_SearchRoot)
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

	// only one file system data source
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
SearchDataSource::ReadFileContents(nsIRDFResource *source, nsString& sourceContents)
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
        nsXPIDLCString	uri;
	source->GetValue( getter_Copies(uri) );
	nsAutoString	name(uri);
	name.Cut(0, strlen("engine://"));
	char *basename = name.ToNewCString();
	if (!basename)	return(NS_ERROR_NULL_POINTER);
	basename = nsUnescape(basename);
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

#ifdef	DEBUG
	char *crap = sourceContents.ToNewCString();
#endif

	return(rv);
}



nsresult
SearchDataSource::GetData(nsString data, char *attribToFind, nsString &value)
{
	nsAutoString	buffer(data);	
	nsresult	rv = NS_RDF_NO_VALUE;
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
			if (value[0] == PRUnichar('\"'))
			{
				value.Cut(0,1);
			}
			PRInt32	len = value.Length();
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
SearchDataSource::GetName(nsIRDFResource *source, nsIRDFLiteral **aResult)
{
	if (!source)	return(NS_RDF_NO_VALUE);
	nsresult	rv;
	nsAutoString	data;
	if (NS_SUCCEEDED(rv = ReadFileContents(source, data)))
	{
		nsAutoString	nameValue;
		if (NS_SUCCEEDED(rv = GetData(data, "name", nameValue)))
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
