/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
  Implementation for a FTP RDF data store.
 */

#include <ctype.h> // for toupper()
#include <stdio.h>
#include "nscore.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFResourceFactory.h"
#include "nsIServiceManager.h"
#include "nsEnumeratorUtils.h"
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

#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsIRDFFTP.h"



static NS_DEFINE_CID(kRDFServiceCID,                  NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,       NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_IID(kISupportsIID,                   NS_ISUPPORTS_IID);





class FTPDataSourceCallback : public nsIStreamListener
{
private:
	nsIRDFDataSource	*mDataSource;
	nsIRDFResource		*mParent;
	static PRInt32		gRefCnt;

    // pseudo-constants
	static nsIRDFResource	*kNC_Child;

	char			*mLine;

public:

	NS_DECL_ISUPPORTS

                FTPDataSourceCallback(nsIRDFDataSource *ds, nsIRDFResource *parent);
	virtual		~FTPDataSourceCallback(void);

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



class FTPDataSource : public nsIRDFFTPDataSource
{
private:
	char			*mURI;

	static PRInt32		gRefCnt;

    // pseudo-constants
	static nsIRDFResource	*kNC_Child;
	static nsIRDFResource	*kNC_Name;
	static nsIRDFResource	*kNC_URL;
	static nsIRDFResource	*kNC_FTPObject;
	static nsIRDFResource	*kRDF_InstanceOf;
	static nsIRDFResource	*kRDF_type;

	NS_METHOD	GetFTPListing(nsIRDFResource *source, nsISimpleEnumerator** aResult);
	NS_METHOD	GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult);
	NS_METHOD	GetName(nsIRDFResource *source, nsIRDFLiteral** aResult);

protected:
	nsIRDFDataSource	*mInner;

public:

	NS_DECL_ISUPPORTS

			FTPDataSource(void);
	virtual		~FTPDataSource(void);

//friend	class		FTPDataSourceCallback;

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




static	nsIRDFService		*gRDFService = nsnull;
static	FTPDataSource		*gFTPDataSource = nsnull;

nsIRDFResource		*FTPDataSource::kNC_Child;
nsIRDFResource		*FTPDataSource::kNC_Name;
nsIRDFResource		*FTPDataSource::kNC_URL;
nsIRDFResource		*FTPDataSource::kNC_FTPObject;
nsIRDFResource		*FTPDataSource::kRDF_InstanceOf;
nsIRDFResource		*FTPDataSource::kRDF_type;

PRInt32			FTPDataSource::gRefCnt;
PRInt32			FTPDataSourceCallback::gRefCnt;

nsIRDFResource		*FTPDataSourceCallback::kNC_Child;




static PRBool
isFTPURI(nsIRDFResource *r)
{
	PRBool		isFTPURI = PR_FALSE;
        nsXPIDLCString uri;
	
	r->GetValue( getter_Copies(uri) );
	if (!strncmp(uri, "ftp:", PL_strlen("ftp:")))
	{
		isFTPURI = PR_TRUE;
	}
	return(isFTPURI);
}



static PRBool
isFTPDirectory(nsIRDFResource *r)
{
	PRBool		isFTPDirectoryFlag = PR_FALSE;
        nsXPIDLCString uri;
	int		len;
	
	r->GetValue( getter_Copies(uri) );
	if (uri)
	{
		if ((len = PL_strlen(uri)) > 0)
		{
			if (uri[len-1] == '/')
			{
				isFTPDirectoryFlag = PR_TRUE;
			}
		}
	}
	return(isFTPDirectoryFlag);
}



FTPDataSource::FTPDataSource(void)
	: mURI(nsnull)
{
	NS_INIT_REFCNT();

	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   nsIRDFService::GetIID(),
                                                   (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(NC_NAMESPACE_URI "child",       &kNC_Child);
		gRDFService->GetResource(NC_NAMESPACE_URI "Name",        &kNC_Name);
		gRDFService->GetResource(NC_NAMESPACE_URI "URL",         &kNC_URL);
		gRDFService->GetResource(NC_NAMESPACE_URI "FTPObject",   &kNC_FTPObject);
		gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_InstanceOf);
		gRDFService->GetResource(RDF_NAMESPACE_URI "type",       &kRDF_type);

		gFTPDataSource = this;
	}
}



FTPDataSource::~FTPDataSource (void)
{
	gRDFService->UnregisterDataSource(this);

	PL_strfree(mURI);

	if (--gRefCnt == 0)
	{
		NS_RELEASE(kNC_Child);
		NS_RELEASE(kNC_Name);
		NS_RELEASE(kNC_URL);
		NS_RELEASE(kNC_FTPObject);
		NS_RELEASE(kRDF_InstanceOf);
		NS_RELEASE(kRDF_type);

		NS_RELEASE(mInner);

		gFTPDataSource = nsnull;
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
		gRDFService = nsnull;
	}
}



NS_IMPL_ISUPPORTS(FTPDataSource, nsIRDFDataSource::GetIID());



NS_IMETHODIMP
FTPDataSource::Init(const char *uri)
{
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
FTPDataSource::GetURI(char **uri)
{
    if ((*uri = nsXPIDLCString::Copy(mURI)) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    else
        return NS_OK;
}



NS_IMETHODIMP
FTPDataSource::GetSource(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFResource** source /* out */)
{
	return mInner->GetSource(property, target, tv, source);
}



NS_IMETHODIMP
FTPDataSource::GetSources(nsIRDFResource *property,
                           nsIRDFNode *target,
			   PRBool tv,
                           nsISimpleEnumerator **sources /* out */)
{
	return mInner->GetSources(property, target, tv, sources);
}



NS_METHOD
FTPDataSource::GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult)
{
    nsresult rv;

    nsXPIDLCString uri;
	rv = source->GetValue( getter_Copies(uri) );
    if (NS_FAILED(rv)) return rv;

	nsAutoString	url(uri);

	nsIRDFLiteral	*literal;
	rv = gRDFService->GetLiteral(url.GetUnicode(), &literal);
    if (NS_FAILED(rv)) return rv;

    *aResult = literal;
    return NS_OK;
}



NS_METHOD
FTPDataSource::GetName(nsIRDFResource *source, nsIRDFLiteral** aResult)
{
    nsresult rv;

    nsXPIDLCString uri;
	rv = source->GetValue( getter_Copies(uri) );
    if (NS_FAILED(rv)) return rv;

	nsAutoString	url(uri);

	// strip off trailing slash, if its a directory
	PRInt32		len = url.Length();
	if (len > 0)
	{
		if (url.CharAt(len-1) == '/')
		{
			url.Cut(len-1, 1);
		}
	}
	// get basename
	PRInt32		slash = url.RFind('/');
	if (slash > 0)
	{
		url.Cut(0, slash+1);
	}

	// XXX To Do: unescape basename
	
	nsIRDFLiteral	*literal;
	rv = gRDFService->GetLiteral(url.GetUnicode(), &literal);
    if (NS_FAILED(rv)) return rv;

    *aResult = literal;
    return NS_OK;
}



NS_IMETHODIMP
FTPDataSource::GetTarget(nsIRDFResource *source,
                          nsIRDFResource *property,
                          PRBool tv,
                          nsIRDFNode **target /* out */)
{
	nsresult		rv = NS_RDF_NO_VALUE;

	// we only have positive assertions in the FTP data source.
	if (! tv)
		return rv;

	if (isFTPURI(source))
	{
		if (property == kNC_Name)
		{
            nsIRDFLiteral* name;
			rv = GetName(source, &name);
            if (NS_FAILED(rv)) return rv;

            rv = name->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
            NS_RELEASE(name);

            return rv;
		}
		else if (property == kNC_URL)
		{
            nsIRDFLiteral* url;
            rv = GetURL(source, &url);
            if (NS_FAILED(rv)) return rv;

            rv = url->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
            NS_RELEASE(url);

            return rv;
		}
		else if (property == kRDF_type)
		{
            nsXPIDLCString uri;
			rv = kNC_FTPObject->GetValue( getter_Copies(uri) );
            if (NS_FAILED(rv)) return rv;

            nsAutoString	url(uri);
            nsIRDFLiteral	*literal;
            rv = gRDFService->GetLiteral(url.GetUnicode(), &literal);

            rv = literal->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
            NS_RELEASE(literal);

			return rv;
		}
	}
	return NS_RDF_NO_VALUE;
}



// FTP class for Netlib callback



FTPDataSourceCallback::FTPDataSourceCallback(nsIRDFDataSource *ds, nsIRDFResource *parent)
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
	}
}



FTPDataSourceCallback::~FTPDataSourceCallback()
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
	}
}



// stream observer methods



NS_IMETHODIMP
FTPDataSourceCallback::OnStartBinding(nsIURL *aURL, const char *aContentType)
{
	return(NS_OK);
}



NS_IMETHODIMP
FTPDataSourceCallback::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax) 
{
	return(NS_OK);
}



NS_IMETHODIMP
FTPDataSourceCallback::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
	return(NS_OK);
}



NS_IMETHODIMP
FTPDataSourceCallback::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg) 
{
	return(NS_OK);
}



// stream listener methods



NS_IMETHODIMP
FTPDataSourceCallback::GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo)
{
	return(NS_OK);
}



NS_IMETHODIMP
FTPDataSourceCallback::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
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
				printf("FTP datasource read failure.\n");
				break;
			}

			if (count != 1)	break;
			line += c;
		}

		PRInt32 eol = line.FindCharInSet("\r\n");
		if (eol < 0)
		{
			if (mLine)	delete []mLine;
			mLine = line.ToNewCString();
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


		printf("FTP: '%s'\n", oneLiner.ToNewCString());


		// yes, very primitive HTML parsing follows

		PRInt32 hrefStart = oneLiner.Find("<A HREF=\"");
		if (hrefStart >= 0)
		{
			hrefStart += PL_strlen("<A HREF=\"");
			oneLiner.Cut(0, hrefStart);
			PRInt32 hrefEnd = oneLiner.Find("\"");
			PRInt32 isDirectory = oneLiner.Find("Directory");
			if (hrefEnd > 0)
			{
				nsAutoString	file("");
				oneLiner.Mid(file, 0, hrefEnd);
				if (file.Equals("/") || file.Equals(".") || file.Equals(".."))
					return(rv);

                                nsXPIDLCString parentURL;
				mParent->GetValue( getter_Copies(parentURL) );

				nsAutoString	path(parentURL);
				path += file;
				if (isDirectory > 0)
				{
					path += "/";
				}

				char		*name = path.ToNewCString();
				if (nsnull != name)
				{
					nsIRDFResource		*ftpChild;
					if (NS_SUCCEEDED(rv = gRDFService->GetResource(name,
						&ftpChild)))
					{
						printf("File: %s\n", name);

						mDataSource->Assert(mParent, kNC_Child,
							ftpChild, PR_TRUE);
					}
					delete []name;
				}
			}
		}
	}
	return(rv);
}



NS_IMPL_ISUPPORTS(FTPDataSourceCallback, nsIRDFFTPDataSourceCallback::GetIID());
#if 0
NS_IMPL_ADDREF(FTPDataSourceCallback);
NS_IMPL_RELEASE(FTPDataSourceCallback);



NS_IMETHODIMP
FTPDataSourceCallback::QueryInterface(REFNSIID iid, void **result)
{
	if (!result)
		return (NS_ERROR_NULL_POINTER);

	*result = nsnull;
	if (iid.Equals(kIRDFFTPDataSourceCallbackIID))
	{
		*result = NS_STATIC_CAST(nsIRDFFTPDataSourceCallback *, this);
		AddRef();
		return(NS_OK);
	}
	return(NS_NOINTERFACE);
}
#endif


NS_METHOD
FTPDataSource::GetFTPListing(nsIRDFResource *source, nsISimpleEnumerator** aResult)
{
	nsresult	rv = NS_ERROR_FAILURE;

	if (isFTPDirectory(source))
	{
        nsXPIDLCString ftpURL;
		source->GetValue( getter_Copies(ftpURL) );

		nsIURL		*url;
		if (NS_SUCCEEDED(rv = NS_NewURL(&url, (const char*) ftpURL)))
		{
			FTPDataSourceCallback	*callback = new FTPDataSourceCallback(mInner, source);
			if (nsnull != callback)
			{
				rv = NS_OpenURL(url, NS_STATIC_CAST(nsIStreamListener *, callback));
			}
		}
	}

    return NS_NewEmptyEnumerator(aResult);
}



NS_IMETHODIMP
FTPDataSource::GetTargets(nsIRDFResource *source,
                           nsIRDFResource *property,
                           PRBool tv,
                           nsISimpleEnumerator **targets /* out */)
{
	nsresult		rv = NS_ERROR_FAILURE;

	// we only have positive assertions in the FTP data source.

	if ((tv) && isFTPURI(source))
	{
		if (property == kNC_Child)
		{
            return GetFTPListing(source, targets);
		}
		else if (property == kNC_Name)
		{
//			rv = getFindName(source, &array);
		}
		else if (property == kRDF_type)
		{
            nsXPIDLCString uri;
			rv = kNC_FTPObject->GetValue( getter_Copies(uri) );
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
	}
	else
	{
		return mInner->GetTargets(source, property, tv, targets);
	}

    return NS_NewEmptyEnumerator(targets);
}



NS_IMETHODIMP
FTPDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
//	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FTPDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
//	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FTPDataSource::HasAssertion(nsIRDFResource *source,
                             nsIRDFResource *property,
                             nsIRDFNode *target,
                             PRBool tv,
                             PRBool *hasAssertion /* out */)
{
	PRBool			retVal = PR_FALSE;
	nsresult		rv = NS_OK;

	*hasAssertion = PR_FALSE;

	// we only have positive assertions in the FTP data source.

	if ((tv) && isFTPURI(source))
	{
		if (property == kRDF_type)
		{
			if ((nsIRDFResource *)target == kRDF_type)
			{
				*hasAssertion = PR_TRUE;
			}
		}
	}
	else
	{
		rv = mInner->HasAssertion(source, property, target,
			tv, hasAssertion);
	}
	return (rv);
}



NS_IMETHODIMP
FTPDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
	return mInner->ArcLabelsIn(node, labels);
}



NS_IMETHODIMP
FTPDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsISimpleEnumerator **labels /* out */)
{
	if (isFTPURI(source) && isFTPDirectory(source))
	{
        nsISimpleEnumerator* result = new nsSingletonEnumerator(kNC_Child);
        if (! result)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(result);
        *labels = result;
        return NS_OK;
	}
	else
	{
		return mInner->ArcLabelsOut(source, labels);
	}
}



NS_IMETHODIMP
FTPDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	return mInner->GetAllResources(aCursor);
}



NS_IMETHODIMP
FTPDataSource::AddObserver(nsIRDFObserver *n)
{
	return mInner->AddObserver(n);
}



NS_IMETHODIMP
FTPDataSource::RemoveObserver(nsIRDFObserver *n)
{
	return mInner->RemoveObserver(n);
}



NS_IMETHODIMP
FTPDataSource::Flush()
{
	return mInner->Flush();
}



NS_IMETHODIMP
FTPDataSource::GetAllCommands(nsIRDFResource* source,nsIEnumerator/*<nsIRDFResource>*/** commands)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FTPDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FTPDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



nsresult
NS_NewRDFFTPDataSource(nsIRDFDataSource **result)
{
	if (!result)
		return NS_ERROR_NULL_POINTER;

	// only one FTP data source
	if (nsnull == gFTPDataSource)
	{
		if ((gFTPDataSource = new FTPDataSource()) == nsnull)
		{
			return NS_ERROR_OUT_OF_MEMORY;
		}
	}
	NS_ADDREF(gFTPDataSource);
	*result = gFTPDataSource;
	return NS_OK;
}



