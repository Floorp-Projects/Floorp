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
  Implementation for a file system RDF data store.
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
#include "nsIRDFFileSystem.h"
#include "nsSpecialSystemDirectory.h"
#include "nsEnumeratorUtils.h"


#ifdef	XP_WIN
#include "windef.h"
#include "winbase.h"
#endif

#ifdef	XP_BEOS
#include <File.h>
#include <NodeInfo.h>
#endif


static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);

static const char kURINC_FileSystemRoot[] = "NC:FilesRoot";



class FileSystemDataSource : public nsIRDFFileSystemDataSource
{
private:
	nsCOMPtr<nsISupportsArray> mObservers;

    static PRInt32 gRefCnt;

    // pseudo-constants
	static nsIRDFResource		*kNC_FileSystemRoot;
	static nsIRDFResource		*kNC_Child;
	static nsIRDFResource		*kNC_Name;
	static nsIRDFResource		*kNC_URL;
	static nsIRDFResource		*kNC_FileSystemObject;
	static nsIRDFResource		*kNC_pulse;
	static nsIRDFResource		*kRDF_InstanceOf;
	static nsIRDFResource		*kRDF_type;

#ifdef	XP_WIN
	static nsIRDFResource		*kNC_IEFavoriteObject;
	static char			*ieFavoritesDir;
#endif

#ifdef	XP_BEOS
	static nsIRDFResource		*kNC_NetPositiveObject;
	static char			*netPositiveDir;
#endif

public:

	NS_DECL_ISUPPORTS

			FileSystemDataSource(void);
	virtual		~FileSystemDataSource(void);

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
    static PRBool isFileURI(nsIRDFResource* aResource);

    static nsresult GetVolumeList(nsISimpleEnumerator **aResult);
    static nsresult GetFolderList(nsIRDFResource *source, nsISimpleEnumerator **aResult);
    static nsresult GetName(nsIRDFResource *source, nsIRDFLiteral** aResult);
    static nsresult GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult);
    static PRBool   isVisible(const nsNativeFileSpec& file);

#ifdef	XP_WIN
    static PRBool   isValidFolder(nsIRDFResource *source);
    static nsresult getIEFavoriteURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral);
#endif

#ifdef	XP_BEOS
    static nsresult getNetPositiveURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral);
#endif

};



static	nsIRDFService		*gRDFService = nsnull;
static	FileSystemDataSource	*gFileSystemDataSource = nsnull;

PRInt32 FileSystemDataSource::gRefCnt;

nsIRDFResource		*FileSystemDataSource::kNC_FileSystemRoot;
nsIRDFResource		*FileSystemDataSource::kNC_Child;
nsIRDFResource		*FileSystemDataSource::kNC_Name;
nsIRDFResource		*FileSystemDataSource::kNC_URL;
nsIRDFResource		*FileSystemDataSource::kNC_FileSystemObject;
nsIRDFResource		*FileSystemDataSource::kNC_pulse;
nsIRDFResource		*FileSystemDataSource::kRDF_InstanceOf;
nsIRDFResource		*FileSystemDataSource::kRDF_type;

#ifdef	XP_WIN
nsIRDFResource		*FileSystemDataSource::kNC_IEFavoriteObject;
char			*FileSystemDataSource::ieFavoritesDir;
#endif

#ifdef	XP_BEOS
nsIRDFResource		*FileSystemDataSource::kNC_NetPositiveObject;
char			*FileSystemDataSource::netPositiveDir;
#endif



static const char	kFileProtocol[] = "file://";



PRBool
FileSystemDataSource::isFileURI(nsIRDFResource *r)
{
	PRBool		isFileURIFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kFileProtocol, sizeof(kFileProtocol) - 1)))
	{
		// XXX HACK HACK HACK
		if (!strchr(uri, '#'))
		{
			isFileURIFlag = PR_TRUE;
		}
	}
	return(isFileURIFlag);
}



FileSystemDataSource::FileSystemDataSource(void)
{
	NS_INIT_REFCNT();

	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
					   nsIRDFService::GetIID(),
					   (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

#ifdef	XP_WIN
		nsSpecialSystemDirectory	ieFavoritesFolder(nsSpecialSystemDirectory::Win_Favorites);
		nsFileURL			ieFavoritesURLSpec(ieFavoritesFolder);
		const char			*ieFavoritesURI = ieFavoritesURLSpec.GetAsString();
		if (ieFavoritesURI)
		{
			ieFavoritesDir = nsCRT::strdup(ieFavoritesURI);
		}
		gRDFService->GetResource(NC_NAMESPACE_URI "IEFavorite",       &kNC_IEFavoriteObject);
#endif

		gRDFService->GetResource(kURINC_FileSystemRoot,               &kNC_FileSystemRoot);
		gRDFService->GetResource(NC_NAMESPACE_URI "child",            &kNC_Child);
		gRDFService->GetResource(NC_NAMESPACE_URI "Name",             &kNC_Name);
		gRDFService->GetResource(NC_NAMESPACE_URI "URL",              &kNC_URL);
		gRDFService->GetResource(NC_NAMESPACE_URI "FileSystemObject", &kNC_FileSystemObject);
		gRDFService->GetResource(NC_NAMESPACE_URI "pulse",            &kNC_pulse);

		gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf",      &kRDF_InstanceOf);
		gRDFService->GetResource(RDF_NAMESPACE_URI "type",            &kRDF_type);

		gFileSystemDataSource = this;
	}
}



FileSystemDataSource::~FileSystemDataSource (void)
{
    if (--gRefCnt == 0) {
        NS_RELEASE(kNC_FileSystemRoot);
        NS_RELEASE(kNC_Child);
        NS_RELEASE(kNC_Name);
        NS_RELEASE(kNC_URL);
	NS_RELEASE(kNC_FileSystemObject);
	NS_RELEASE(kNC_pulse);
        NS_RELEASE(kRDF_InstanceOf);
        NS_RELEASE(kRDF_type);

#ifdef	XP_WIN
	NS_RELEASE(kNC_IEFavoriteObject);

        if (ieFavoritesDir)
        {
        	nsCRT::free(ieFavoritesDir);
        	ieFavoritesDir = nsnull;
        }
#endif

        gFileSystemDataSource = nsnull;
        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
        gRDFService = nsnull;
    }
}



NS_IMPL_ISUPPORTS(FileSystemDataSource, nsIRDFDataSource::GetIID());



NS_IMETHODIMP
FileSystemDataSource::GetURI(char **uri)
{
    NS_PRECONDITION(uri != nsnull, "null ptr");
    if (! uri)
        return NS_ERROR_NULL_POINTER;

    if ((*uri = nsXPIDLCString::Copy("rdf:files")) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::GetSource(nsIRDFResource* property,
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
FileSystemDataSource::GetSources(nsIRDFResource *property,
                                 nsIRDFNode *target,
                                 PRBool tv,
                                 nsISimpleEnumerator **sources /* out */)
{
	NS_NOTYETIMPLEMENTED("write me");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::GetTarget(nsIRDFResource *source,
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
		return NS_RDF_NO_VALUE;

	if (source == kNC_FileSystemRoot)
	{
		if (property == kNC_pulse)
		{
			nsAutoString	pulse("12");
			nsIRDFLiteral	*pulseLiteral;
			gRDFService->GetLiteral(pulse.GetUnicode(), &pulseLiteral);
			*target = pulseLiteral;
			return NS_OK;
		}
	}
	else if (isFileURI(source))
	{
		if (property == kNC_Name)
		{
			nsCOMPtr<nsIRDFLiteral> name;
			rv = GetName(source, getter_AddRefs(name));
			if (NS_FAILED(rv)) return(rv);
			if (!name)	rv = NS_RDF_NO_VALUE;
			if (rv == NS_RDF_NO_VALUE)	return(rv);
			return name->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
		}
		else if (property == kNC_URL)
		{
			nsCOMPtr<nsIRDFLiteral> url;
			rv = GetURL(source, getter_AddRefs(url));
			if (NS_FAILED(rv)) return(rv);
			if (!url)	rv = NS_RDF_NO_VALUE;
			if (rv == NS_RDF_NO_VALUE)	return(rv);

			return url->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
		}
		else if (property == kRDF_type)
		{
			const char	*type;
			rv = kNC_FileSystemObject->GetValueConst(&type);
			if (NS_FAILED(rv)) return(rv);

#ifdef	XP_WIN
			// under Windows, if its an IE favorite, return that type
			if (ieFavoritesDir)
			{
				const char		*uri;
				rv = source->GetValueConst(&uri);
				if (NS_FAILED(rv)) return(rv);

				nsAutoString		theURI(uri);
				if (theURI.Find(ieFavoritesDir) == 0)
				{
					rv = kNC_IEFavoriteObject->GetValueConst(&type);
					if (NS_FAILED(rv)) return rv;
				}
			}
#endif

			nsAutoString	url(type);
			nsIRDFLiteral	*literal;
			gRDFService->GetLiteral(url.GetUnicode(), &literal);
			rv = literal->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
			NS_RELEASE(literal);
			return rv;
		}
		else if (property == kNC_pulse)
		{
			nsAutoString	pulse("12");
			nsIRDFLiteral	*pulseLiteral;
			gRDFService->GetLiteral(pulse.GetUnicode(), &pulseLiteral);
			rv = pulseLiteral->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
			NS_RELEASE(pulseLiteral);
			return rv;
		}
		else if (property == kNC_Child)
		{
			// Oh this is evil. Somebody kill me now.
			nsCOMPtr<nsISimpleEnumerator> children;
			rv = GetFolderList(source, getter_AddRefs(children));
			if (NS_FAILED(rv)) return rv;

			PRBool hasMore;
			rv = children->HasMoreElements(&hasMore);
			if (NS_FAILED(rv)) return rv;

			if (hasMore) {
				nsCOMPtr<nsISupports> isupports;
				rv = children->GetNext(getter_AddRefs(isupports));
				if (NS_FAILED(rv)) return rv;

				return isupports->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
			}
		}
	}

	*target = nsnull;
	return NS_RDF_NO_VALUE;
}



NS_IMETHODIMP
FileSystemDataSource::GetTargets(nsIRDFResource *source,
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

	if (source == kNC_FileSystemRoot)
	{
		if (property == kNC_Child)
		{
			return GetVolumeList(targets);
		}
		else if (property == kNC_pulse)
		{
			nsAutoString	pulse("12");
			nsIRDFLiteral	*pulseLiteral;
			gRDFService->GetLiteral(pulse.GetUnicode(), &pulseLiteral);
			nsISimpleEnumerator* result = new nsSingletonEnumerator(pulseLiteral);
			NS_RELEASE(pulseLiteral);

			if (! result)
				return NS_ERROR_OUT_OF_MEMORY;

			NS_ADDREF(result);
			*targets = result;
			return NS_OK;
		}
	}
	else if (isFileURI(source))
	{
		if (property == kNC_Child)
		{
			return GetFolderList(source, targets);
		}
		else if (property == kNC_Name)
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
		else if (property == kRDF_type)
		{
			nsXPIDLCString uri;
			rv = kNC_FileSystemObject->GetValue( getter_Copies(uri) );
			if (NS_FAILED(rv)) return rv;

			nsAutoString	url(uri);
			nsIRDFLiteral	*literal;

			rv = gRDFService->GetLiteral(url.GetUnicode(), &literal);
			if (NS_FAILED(rv)) return rv;

			nsISimpleEnumerator* result = new nsSingletonEnumerator(literal);
			NS_RELEASE(literal);

			if (! result)
				return NS_ERROR_OUT_OF_MEMORY;

			NS_ADDREF(result);
			*targets = result;
			return NS_OK;
		}
		else if (property == kNC_pulse)
		{
			nsAutoString	pulse("12");
			nsIRDFLiteral	*pulseLiteral;
			rv = gRDFService->GetLiteral(pulse.GetUnicode(), &pulseLiteral);
			if (NS_FAILED(rv)) return rv;

			nsISimpleEnumerator* result = new nsSingletonEnumerator(pulseLiteral);
			NS_RELEASE(pulseLiteral);

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
FileSystemDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FileSystemDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FileSystemDataSource::Change(nsIRDFResource* aSource,
							 nsIRDFResource* aProperty,
							 nsIRDFNode* aOldTarget,
							 nsIRDFNode* aNewTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FileSystemDataSource::Move(nsIRDFResource* aOldSource,
						   nsIRDFResource* aNewSource,
						   nsIRDFResource* aProperty,
						   nsIRDFNode* aTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FileSystemDataSource::HasAssertion(nsIRDFResource *source,
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
	if (! tv) {
		*hasAssertion = PR_FALSE;
		return NS_OK;
	}

	if ((source == kNC_FileSystemRoot) || isFileURI(source))
	{
		if (property == kRDF_type)
		{
			nsCOMPtr<nsIRDFResource> resource( do_QueryInterface(target) );
			if (resource.get() == kRDF_type) {
				*hasAssertion = PR_TRUE;
				return NS_OK;
			}
		}
	}

	*hasAssertion = PR_FALSE;
	return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
	NS_NOTYETIMPLEMENTED("write me");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsISimpleEnumerator **labels /* out */)
{
	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(labels != nsnull, "null ptr");
	if (! labels)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	if (source == kNC_FileSystemRoot)
	{
		nsCOMPtr<nsISupportsArray> array;
		rv = NS_NewISupportsArray(getter_AddRefs(array));
		if (NS_FAILED(rv)) return rv;

		array->AppendElement(kNC_Child);
		array->AppendElement(kNC_pulse);

		nsISimpleEnumerator* result = new nsArrayEnumerator(array);
		if (! result)
			return NS_ERROR_OUT_OF_MEMORY;

		NS_ADDREF(result);
		*labels = result;
		return NS_OK;
	}
	else if (isFileURI(source))
	{
		nsCOMPtr<nsISupportsArray> array;
		rv = NS_NewISupportsArray(getter_AddRefs(array));
		if (NS_FAILED(rv)) return rv;

		nsXPIDLCString uri;
		rv = source->GetValue( getter_Copies(uri) );
		if (NS_FAILED(rv)) return rv;

		nsFileURL	fileURL(uri);
		nsFileSpec	fileSpec(fileURL);
		if (fileSpec.IsDirectory()) {
#ifdef	XP_WIN
			if (isValidFolder(source) == PR_TRUE)
			{
				array->AppendElement(kNC_Child);
			}
#else
			array->AppendElement(kNC_Child);
#endif
			array->AppendElement(kNC_pulse);
		}

		array->AppendElement(kRDF_type);

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
FileSystemDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	NS_NOTYETIMPLEMENTED("sorry!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::AddObserver(nsIRDFObserver *n)
{
    NS_PRECONDITION(n != nsnull, "null ptr");
    if (! n)
        return NS_ERROR_NULL_POINTER;

	if (! mObservers)
	{
		nsresult rv;
		rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
		if (NS_FAILED(rv)) return rv;
	}
	mObservers->AppendElement(n);
	return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::RemoveObserver(nsIRDFObserver *n)
{
    NS_PRECONDITION(n != nsnull, "null ptr");
    if (! n)
        return NS_ERROR_NULL_POINTER;

	if (! mObservers)
		return NS_OK;

	mObservers->RemoveElement(n);
	return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::GetAllCommands(nsIRDFResource* source,
                                     nsIEnumerator/*<nsIRDFResource>*/** commands)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::GetAllCmds(nsIRDFResource* source,
                                     nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	return(NS_NewEmptyEnumerator(commands));
}



NS_IMETHODIMP
FileSystemDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                       nsIRDFResource*   aCommand,
                                       nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                       PRBool* aResult)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
FileSystemDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



nsresult
NS_NewRDFFileSystemDataSource(nsIRDFDataSource **result)
{
	if (!result)
		return NS_ERROR_NULL_POINTER;

	// only one file system data source
	if (nsnull == gFileSystemDataSource)
	{
		if ((gFileSystemDataSource = new FileSystemDataSource()) == nsnull)
		{
			return NS_ERROR_OUT_OF_MEMORY;
		}
	}
	NS_ADDREF(gFileSystemDataSource);
	*result = gFileSystemDataSource;
	return NS_OK;
}



nsresult
FileSystemDataSource::GetVolumeList(nsISimpleEnumerator** aResult)
{
	nsresult rv;
	nsCOMPtr<nsISupportsArray> volumes;

	rv = NS_NewISupportsArray(getter_AddRefs(volumes));
	if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIRDFResource> vol;

#ifdef	XP_MAC
	StrFileName     fname;
	HParamBlockRec	pb;
	for (int16 volNum = 1; ; volNum++)
	{
		pb.volumeParam.ioCompletion = NULL;
		pb.volumeParam.ioVolIndex = volNum;
		pb.volumeParam.ioNamePtr = (StringPtr)fname;
		if (PBHGetVInfo(&pb,FALSE) != noErr)
			break;
		nsFileSpec fss(pb.volumeParam.ioVRefNum, fsRtParID, fname);
		rv = gRDFService->GetResource(nsFileURL(fss).GetAsString(), getter_AddRefs(vol));
		if (NS_FAILED(rv)) return rv;

		volumes->AppendElement(vol);
	}
#endif

#ifdef	XP_WIN
	PRInt32			driveType;
	char			drive[32];
	PRInt32			volNum;
	char			*url;

	for (volNum = 0; volNum < 26; volNum++)
	{
		sprintf(drive, "%c:\\", volNum + 'A');
		driveType = GetDriveType(drive);
		if (driveType != DRIVE_UNKNOWN && driveType != DRIVE_NO_ROOT_DIR)
		{
			if (nsnull != (url = PR_smprintf("file:///%c|/", volNum + 'A')))
			{
				rv = gRDFService->GetResource(url, getter_AddRefs(vol));
				PR_Free(url);

				if (NS_FAILED(rv)) return rv;
				volumes->AppendElement(vol);
			}
		}
	}
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
	gRDFService->GetResource("file:///", getter_AddRefs(vol));
	volumes->AppendElement(vol);
#endif

	nsISimpleEnumerator* result = new nsArrayEnumerator(volumes);
	if (! result)
		return NS_ERROR_OUT_OF_MEMORY;

	NS_ADDREF(result);
	*aResult = result;

	return NS_OK;
}



PRBool
FileSystemDataSource::isVisible(const nsNativeFileSpec& file)
{
	PRBool isVisible = (!file.IsHidden());
	return(isVisible);
}



#ifdef	XP_WIN
PRBool
FileSystemDataSource::isValidFolder(nsIRDFResource *source)
{
	PRBool	isValid = PR_TRUE;
	if (!ieFavoritesDir)	return(isValid);

	nsresult		rv;
	const char		*uri;
	rv = source->GetValueConst(&uri);
	if (NS_FAILED(rv)) return(isValid);

	PRBool			isIEFavorite = PR_FALSE;
	nsAutoString		theURI(uri);
	if (theURI.Find(ieFavoritesDir) == 0)
	{
		isValid = PR_FALSE;

		nsCOMPtr<nsISimpleEnumerator>	folderEnum;
		if (NS_SUCCEEDED(rv = GetFolderList(source, getter_AddRefs(folderEnum))))
		{
			PRBool		hasMore;
			while (NS_SUCCEEDED(folderEnum->HasMoreElements(&hasMore)) &&
					(hasMore == PR_TRUE))
			{
				nsCOMPtr<nsISupports>		isupports;
				if (NS_FAILED(rv = folderEnum->GetNext(getter_AddRefs(isupports))))
					break;
				nsCOMPtr<nsIRDFResource>	res = do_QueryInterface(isupports);
				if (!res)	break;

				nsCOMPtr<nsIRDFLiteral>		nameLiteral;
				if (NS_FAILED(rv = GetName(res, getter_AddRefs(nameLiteral))))
					break;
				
				const PRUnichar			*uniName;
				if (NS_FAILED(rv = nameLiteral->GetValueConst(&uniName)))
					break;
				nsAutoString			name(uniName);
				if (!name.EqualsIgnoreCase("desktop.ini"))
				{
					isValid = PR_TRUE;
					break;
				}
			}
		}
	}
	return(isValid);
}
#endif



nsresult
FileSystemDataSource::GetFolderList(nsIRDFResource *source, nsISimpleEnumerator** aResult)
{
	nsresult	rv;
	nsCOMPtr<nsISupportsArray> nameArray;
	rv = NS_NewISupportsArray(getter_AddRefs(nameArray));
	if (NS_FAILED(rv)) return rv;

	const char		*uri;
	rv = source->GetValueConst(&uri);
	if (NS_FAILED(rv)) return(rv);

	nsFileURL 		parentDir(uri);
	nsNativeFileSpec 	nativeDir(parentDir);
	for (nsDirectoryIterator i(nativeDir, PR_FALSE); i.Exists(); i++)
	{
		const nsNativeFileSpec	nativeSpec = (const nsNativeFileSpec &)i;
		if (!isVisible(nativeSpec))	continue;
		nsFileURL		fileURL(nativeSpec);
		const char		*childURL = fileURL.GetAsString();
		if (childURL != nsnull)
		{
			nsCOMPtr<nsIRDFResource> file;
			gRDFService->GetResource(childURL, getter_AddRefs(file));
			nameArray->AppendElement(file);
		}
	}

	nsISimpleEnumerator* result = new nsArrayEnumerator(nameArray);
	if (! result)
		return NS_ERROR_OUT_OF_MEMORY;

	NS_ADDREF(result);
	*aResult = result;

	return NS_OK;
}



nsresult
FileSystemDataSource::GetName(nsIRDFResource *source, nsIRDFLiteral **aResult)
{
	nsresult		rv;
	const char		*uri;
	rv = source->GetValueConst(&uri);
	if (NS_FAILED(rv)) return(rv);

	nsFileURL		url(uri);
	nsNativeFileSpec	native(url);
	char			*baseFilename = native.GetLeafName();

	if (! baseFilename)
		return NS_ERROR_OUT_OF_MEMORY;

	nsAutoString	name(baseFilename);

#ifdef	XP_WIN

	// special hack for IE favorites under Windows; strip off the
	// trailing ".url" or ".lnk" at the end of IE favorites names
	PRInt32			nameLen = name.Length();
	nsAutoString		theURI(uri);
	if ((theURI.Find(ieFavoritesDir) == 0) && (nameLen > 4))
	{
		nsAutoString	extension;
		name.Right(extension, 4);
		if (extension.EqualsIgnoreCase(".url") ||
		    extension.EqualsIgnoreCase(".lnk"))
		{
			name.Truncate(nameLen - 4);
		}
	}
#endif

#ifdef	XP_BEOS
	// under BEOS, try and get the "META:title" attribute (if its a file)
	nsAutoString		theURI(uri);
	if (theURI.Find(netPositiveDir) == 0)
	{
		nsFileSpec		spec(url);
		if (spec.IsFile() && (!spec.IsHidden()))
		{
			const char	*nativeURI = spec.GetNativePathCString();
			if (nativeURI)
			{
				BFile	bf(nativeURI, B_READ_ONLY);
				if (bf.InitCheck() == B_OK)
				{
					char		beNameAttr[4096];
					ssize_t		len;

					if ((len = bf.ReadAttr("META:title", B_STRING_TYPE,
						0, beNameAttr, sizeof(beNameAttr-1))) > 0)
					{
						beNameAttr[len] = '\0';
						name = beNameAttr;
					}
				}
			}
		}
	}
#endif

	nsIRDFLiteral *literal;
	gRDFService->GetLiteral(name.GetUnicode(), &literal);
	*aResult = literal;
	nsCRT::free(baseFilename);

	return NS_OK;
}



#ifdef	XP_WIN
nsresult
FileSystemDataSource::getIEFavoriteURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral)
{
	nsresult		rv = NS_OK;

	*urlLiteral = nsnull;

	nsFileURL		url(aFileURL);
	nsFileSpec		uri(url);
	if (uri.IsDirectory())
	{
		if (isValidFolder(source))
			return(NS_RDF_NO_VALUE);
		uri += "desktop.ini";
	}
	else if (aFileURL.Length() > 4)
	{
		nsAutoString	extension;
		aFileURL.Right(extension, 4);
		if (!extension.EqualsIgnoreCase(".url"))
		{
			return(NS_RDF_NO_VALUE);
		}
	}

	nsInputFileStream	stream(uri);

	if (!stream.is_open())
		return(NS_RDF_NO_VALUE);

	char		buffer[256];
	nsAutoString	line;
	while(NS_SUCCEEDED(rv) && (!stream.eof()) && (!stream.failed()))
	{
		PRBool	untruncated = stream.readline(buffer, sizeof(buffer));

		if (stream.failed())
		{
			rv = NS_ERROR_FAILURE;
			break;
		}

		line.Append(buffer);

		if (untruncated)
		{
			if (line.Find("URL=", PR_TRUE) == 0)
			{
				line.Cut(0, 4);
				rv = gRDFService->GetLiteral(line.GetUnicode(), urlLiteral);
				break;
			}
			else if (line.Find("CDFURL=", PR_TRUE) == 0)
			{
				line.Cut(0, 7);
				rv = gRDFService->GetLiteral(line.GetUnicode(), urlLiteral);
				break;
			}
			line.Truncate();
		}

	}

	return(rv);
}
#endif



nsresult
FileSystemDataSource::GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult)
{
	nsresult		rv;
	const char		*uri;
	rv = source->GetValueConst(&uri);
	if (NS_FAILED(rv)) return(rv);
	nsAutoString		url(uri);

	nsIRDFLiteral		*literal = nsnull;

#ifdef	XP_WIN
	// under Windows, if its an IE favorite, munge the URL
	if (ieFavoritesDir)
	{
		if (url.Find(ieFavoritesDir) == 0)
		{
			rv = getIEFavoriteURL(source, url, &literal);
			*aResult = literal;
			return(rv);
		}
	}
#endif

#ifdef	XP_BEOS
	// under BEOS, try and get the "META:url" attribute
	if (netPositiveDir)
	{
		if (url.Find(netPositiveDir) == 0)
		{
			rv = getNetPositiveURL(source, url, &literal);
			*aResult = literal;
			return(rv);
		}
	}
#endif

	// if we fall through to here, its not any type of bookmark
	// stored in the platform native file system, so just set the URL

	gRDFService->GetLiteral(url.GetUnicode(), &literal);
	*aResult = literal;
	return NS_OK;
}



#ifdef	XP_BEOS

nsresult
FileSystemDataSource::getNetPositiveURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral)
{
	nsresult		rv = NS_RDF_NO_VALUE;

	*urlLiteral = nsnull;

	nsFileURL		url(aFileURL);
	nsFileSpec		uri(url);
	if (uri.IsFile() && (!uri.IsHidden()))
	{
		const char	*nativeURI = uri.GetNativePathCString();
		if (nativeURI)
		{
			BFile	bf(nativeURI, B_READ_ONLY);
			if (bf.InitCheck() == B_OK)
			{
				char		beURLattr[4096];
				ssize_t		len;

				if ((len = bf.ReadAttr("META:url", B_STRING_TYPE,
					0, beURLattr, sizeof(beURLattr-1))) > 0)
				{
					beURLattr[len] = '\0';
					nsAutoString	bookmarkURL(beURLattr);
					rv = gRDFService->GetLiteral(bookmarkURL.GetUnicode(),
						urlLiteral);
				}
			}
		}
	}
	return(rv);
}

#endif

