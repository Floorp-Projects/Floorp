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
  Implementation for a FTP RDF data store.
 */

#include <ctype.h> // for toupper()
#include <stdio.h>
#include "nscore.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFResourceFactory.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsRDFCID.h"
#include "rdfutil.h"
#include "nsIRDFService.h"
#include "xp_core.h"
#include "plhash.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "prio.h"

//#include "net.h"
//#include "nsINetlibURL.h"
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsIRDFFTP.h"
#include "nsFTPDataSource.h"



static NS_DEFINE_CID(kRDFServiceCID,                  NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,       NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_IID(kIRDFServiceIID,                 NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,              NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFFTPDataSourceIID,           NS_IRDFFTPDATAOURCE_IID);
static NS_DEFINE_IID(kIRDFAssertionCursorIID,         NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,                  NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,           NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kISupportsIID,                   NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFResourceIID,                NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFNodeIID,                    NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,                 NS_IRDFLITERAL_IID);
//static NS_DEFINE_IID(kINetlibURLIID,                  NS_INETLIBURL_IID);
static NS_DEFINE_IID(kIRDFFTPDataSourceCallbackIID,   NS_IRDFFTPDATASOURCECALLBACK_IID);


DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, URL);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, FTPObject);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Seq);


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
peq(nsIRDFResource* r1, nsIRDFResource* r2)
{
	PRBool		retVal=PR_FALSE, result;

	if (NS_SUCCEEDED(r1->EqualsResource(r2, &result)))
	{
		if (result)
		{
			retVal = PR_TRUE;
		}
	}
	return(retVal);
}



static PRBool
isFTPURI(nsIRDFResource *r)
{
	PRBool		isFTPURI = PR_FALSE;
	const char	*uri;
	
	r->GetValue(&uri);
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
	const char	*uri;
	int		len;
	
	r->GetValue(&uri);
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

    if (gRefCnt++ == 0) {
        nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   kIRDFServiceIID,
                                                   (nsISupports**) &gRDFService);

        PR_ASSERT(NS_SUCCEEDED(rv));

	gRDFService->GetResource(kURINC_child, &kNC_Child);
	gRDFService->GetResource(kURINC_Name, &kNC_Name);
	gRDFService->GetResource(kURINC_URL, &kNC_URL);
	gRDFService->GetResource(kURINC_FTPObject, &kNC_FTPObject);
	gRDFService->GetResource(kURIRDF_instanceOf, &kRDF_InstanceOf);
	gRDFService->GetResource(kURIRDF_type, &kRDF_type);

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



// NS_IMPL_ISUPPORTS(FTPDataSource, kIRDFFTPDataSourceIID);
NS_IMPL_ISUPPORTS(FTPDataSource, kIRDFDataSourceIID);

#if 0
NS_IMPL_ADDREF(FTPDataSource);
NS_IMPL_RELEASE(FTPDataSource);



nsresult
FTPDataSource::QueryInterface(REFNSIID aIID, void ** aInstancePtr)
{
	nsresult	rv = NS_NOINTERFACE;

	if (nsnull == aInstancePtr)	return(NS_ERROR_NULL_POINTER);
	if (aIID.Equals(kIRDFDataSourceIID))
	{
		*aInstancePtr = (void *)((nsIRDFFTPDataSource *)this);
		NS_ADDREF_THIS();
		rv = NS_OK;
	}
	return(rv);
}
#endif



NS_IMETHODIMP
FTPDataSource::Init(const char *uri)
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
				nsnull, kIRDFDataSourceIID, (void **)&mInner)))
		return rv;
	if (NS_FAILED(rv = mInner->Init(uri)))
		return rv;

	if ((mURI = PL_strdup(uri)) == nsnull)
		return rv;

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this)))
		return rv;
	return NS_OK;
}



NS_IMETHODIMP
FTPDataSource::GetURI(const char **uri) const
{
	*uri = mURI;
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
                           nsIRDFAssertionCursor **sources /* out */)
{
	return mInner->GetSources(property, target, tv, sources);
}



NS_METHOD
FTPDataSource::GetURL(nsIRDFResource *source, nsVoidArray **array)
{
	nsVoidArray *urlArray = new nsVoidArray();
	*array = urlArray;
	if (nsnull == urlArray)
	{
		return(NS_ERROR_OUT_OF_MEMORY);
	}

	const char	*uri;
	source->GetValue(&uri);
	nsAutoString	url(uri);

	nsIRDFLiteral	*literal;
	gRDFService->GetLiteral(url, &literal);
	urlArray->AppendElement(literal);
	return(NS_OK);
}



NS_IMETHODIMP
FTPDataSource::GetTarget(nsIRDFResource *source,
                          nsIRDFResource *property,
                          PRBool tv,
                          nsIRDFNode **target /* out */)
{
	nsresult		rv = NS_ERROR_RDF_NO_VALUE;

	// we only have positive assertions in the FTP data source.
	if (! tv)
		return rv;

	if (isFTPURI(source))
	{
		nsVoidArray		*array = nsnull;

		if (peq(property, kNC_Name))
		{
//			rv = GetName(source, &array);
		}
		else if (peq(property, kNC_URL))
		{
			rv = GetURL(source, &array);
			rv = NS_OK;
		}
		else if (peq(property, kRDF_type))
		{
			const char	*uri;
			kNC_FTPObject->GetValue(&uri);
			if (uri)
			{
				nsAutoString	url(uri);
				nsIRDFLiteral	*literal;
				gRDFService->GetLiteral(url, &literal);
				*target = literal;
				rv = NS_OK;
			}
			return(rv);
		}
		if (array != nsnull)
		{
			nsIRDFLiteral *literal = (nsIRDFLiteral *)(array->ElementAt(0));
			*target = (nsIRDFNode *)literal;
			delete array;
			rv = NS_OK;
		}
		else
		{
			rv = NS_ERROR_RDF_NO_VALUE;
		}
	}
	return(rv);
}



// FTP class for Netlib callback



FTPDataSourceCallback::FTPDataSourceCallback(nsIRDFDataSource *ds, nsIRDFResource *parent)
	: mDataSource(ds),
	  mParent(parent)
{
	NS_INIT_REFCNT();
	NS_ADDREF(mDataSource);
	NS_ADDREF(mParent);

	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   kIRDFServiceIID,
                                                   (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(kURINC_child, &kNC_Child);
	}
}



FTPDataSourceCallback::~FTPDataSourceCallback()
{
	NS_IF_RELEASE(mDataSource);
	NS_IF_RELEASE(mParent);
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
		nsAutoString	line("");
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



	printf("Line: '%s'\n", line.ToNewCString());


		// yes, very primitive HTML parsing follows

		PRInt32 hrefStart = line.Find("<A HREF=\"");
		if (hrefStart >= 0)
		{
			hrefStart += PL_strlen("<A HREF=\"");
			line.Cut(0, hrefStart);
			PRInt32 hrefEnd = line.Find("\"");
			PRInt32 isDirectory = line.Find("Directory");
			if (hrefEnd > 0)
			{
				nsAutoString	file("");
				line.Mid(file, 0, hrefEnd);

				const char	*parentURL;
				mParent->GetValue(&parentURL);

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



NS_IMPL_ISUPPORTS(FTPDataSourceCallback, kIRDFFTPDataSourceCallbackIID);
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
FTPDataSource::GetFTPListing(nsIRDFResource *source, nsVoidArray **array)
{
	nsresult	rv = NS_ERROR_FAILURE;
	nsVoidArray *urlArray = new nsVoidArray();
	*array = urlArray;
	if (nsnull == urlArray)
	{
		return(NS_ERROR_OUT_OF_MEMORY);
	}

	if (isFTPDirectory(source))
	{
		const char	*ftpURL;
		source->GetValue(&ftpURL);

		nsIURL		*url;
		if (NS_SUCCEEDED(rv = NS_NewURL(&url, ftpURL)))
		{
			FTPDataSourceCallback	*callback = new FTPDataSourceCallback(mInner, source);
			if (nsnull != callback)
			{
				rv = NS_OpenURL(url, NS_STATIC_CAST(nsIStreamListener *, callback));
			}
		}
	}
	return(rv);
}



NS_IMETHODIMP
FTPDataSource::GetTargets(nsIRDFResource *source,
                           nsIRDFResource *property,
                           PRBool tv,
                           nsIRDFAssertionCursor **targets /* out */)
{
	nsVoidArray		*array = nsnull;
	nsresult		rv = NS_ERROR_FAILURE;

	// we only have positive assertions in the FTP data source.

	if ((tv) && isFTPURI(source))
	{
		if (peq(property, kNC_Child))
		{
			rv = GetFTPListing(source, &array);
		}
		else if (peq(property, kNC_Name))
		{
//			rv = getFindName(source, &array);
		}
		else if (peq(property, kRDF_type))
		{
			const char	*uri;
			kNC_FTPObject->GetValue(&uri);
			if (uri)
			{
				nsAutoString	url(uri);
				nsIRDFLiteral	*literal;
				gRDFService->GetLiteral(url, &literal);
				array = new nsVoidArray();
				if (array)
				{
					array->AppendElement(literal);
					rv = NS_OK;
				}
			}
		}
		if ((rv == NS_OK) && (nsnull != array))
		{
			*targets = new FTPCursor(source, property, PR_FALSE, array);
			NS_ADDREF(*targets);
		}
	}
	else
	{
		rv = mInner->GetTargets(source, property, tv, targets);
	}
	return(rv);
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
	nsresult		rv = NS_ERROR_FAILURE;

	// we only have positive assertions in the FTP data source.

	*hasAssertion = PR_FALSE;
	if ((tv) && isFTPURI(source))
	{
		if (peq(property, kRDF_type))
		{
			if (peq((nsIRDFResource *)target, kRDF_type))
			{
				*hasAssertion = PR_TRUE;
				rv = NS_OK;
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
                            nsIRDFArcsInCursor ** labels /* out */)
{
	return mInner->ArcLabelsIn(node, labels);
}



NS_IMETHODIMP
FTPDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsIRDFArcsOutCursor **labels /* out */)
{
	nsresult		rv = NS_ERROR_RDF_NO_VALUE;

	*labels = nsnull;

	if (isFTPURI(source) && isFTPDirectory(source))
	{
		nsVoidArray *temp = new nsVoidArray();
		if (nsnull == temp)
			return NS_ERROR_OUT_OF_MEMORY;
		temp->AppendElement(kNC_Child);
		*labels = new FTPCursor(source, kNC_Child, PR_TRUE, temp);
		if (nsnull != *labels)
		{
			NS_ADDREF(*labels);
			rv = NS_OK;
		}
	}
	else
	{
		rv = mInner->ArcLabelsOut(source, labels);
	}
	return(rv);

}



NS_IMETHODIMP
FTPDataSource::GetAllResources(nsIRDFResourceCursor** aCursor)
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
				nsISupportsArray/*<nsIRDFResource>*/* aArguments)
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



FTPCursor::FTPCursor(nsIRDFResource *source,
				nsIRDFResource *property,
				PRBool isArcsOut,
				nsVoidArray *array)
	: mSource(source),
	  mProperty(property),
	  mArcsOut(isArcsOut),
	  mArray(array),
	  mCount(0),
	  mTarget(nsnull),
	  mValue(nsnull)
{
	NS_INIT_REFCNT();
	NS_ADDREF(mSource);
	NS_ADDREF(mProperty);
}



FTPCursor::~FTPCursor(void)
{
	NS_IF_RELEASE(mSource);
	NS_IF_RELEASE(mValue);
	NS_IF_RELEASE(mProperty);
	NS_IF_RELEASE(mTarget);
	if (nsnull != mArray)
	{
		delete mArray;
	}
}



NS_IMETHODIMP
FTPCursor::Advance(void)
{
	if (!mArray)
		return NS_ERROR_NULL_POINTER;
	if (mArray->Count() <= mCount)
		return NS_ERROR_RDF_CURSOR_EMPTY;
	NS_IF_RELEASE(mValue);
	mTarget = mValue = (nsIRDFNode *)mArray->ElementAt(mCount++);
	NS_ADDREF(mValue);
	NS_ADDREF(mTarget);
	return NS_OK;
}



NS_IMETHODIMP
FTPCursor::GetValue(nsIRDFNode **aValue)
{
	if (nsnull == mValue)
		return NS_ERROR_NULL_POINTER;
	NS_ADDREF(mValue);
	*aValue = mValue;
	return NS_OK;
}



NS_IMETHODIMP
FTPCursor::GetDataSource(nsIRDFDataSource **aDataSource)
{
	NS_ADDREF(gFTPDataSource);
	*aDataSource = gFTPDataSource;
	return NS_OK;
}



NS_IMETHODIMP
FTPCursor::GetSubject(nsIRDFResource **aResource)
{
	NS_ADDREF(mSource);
	*aResource = mSource;
	return NS_OK;
}



NS_IMETHODIMP
FTPCursor::GetPredicate(nsIRDFResource **aPredicate)
{
	if (mArcsOut == PR_FALSE)
	{
		NS_ADDREF(mProperty);
		*aPredicate = mProperty;
	}
	else
	{
		if (nsnull == mValue)
			return NS_ERROR_NULL_POINTER;
		NS_ADDREF(mValue);
		*(nsIRDFNode **)aPredicate = mValue;
	}
	return NS_OK;
}



NS_IMETHODIMP
FTPCursor::GetObject(nsIRDFNode **aObject)
{
	if (nsnull != mTarget)
		NS_ADDREF(mTarget);
	*aObject = mTarget;
	return NS_OK;
}



NS_IMETHODIMP
FTPCursor::GetTruthValue(PRBool *aTruthValue)
{
	*aTruthValue = 1;
	return NS_OK;
}



NS_IMPL_ADDREF(FTPCursor);
NS_IMPL_RELEASE(FTPCursor);



NS_IMETHODIMP
FTPCursor::QueryInterface(REFNSIID iid, void **result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if (iid.Equals(kIRDFAssertionCursorIID) ||
		iid.Equals(kIRDFCursorIID) ||
		iid.Equals(kIRDFArcsOutCursorIID) ||
		iid.Equals(kISupportsIID))
	{
		*result = NS_STATIC_CAST(nsIRDFAssertionCursor *, this);
		AddRef();
		return NS_OK;
	}
	return(NS_NOINTERFACE);
}

