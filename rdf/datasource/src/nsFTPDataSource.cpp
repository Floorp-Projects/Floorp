/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
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

#include "nsCOMPtr.h"

#include "nsEscape.h"

#include "nsIURL.h"
#ifdef NECKO
#include "nsNeckoUtil.h"
#include "nsIBufferInputStream.h"
#endif // NECKO
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
	static nsIRDFResource	*kNC_loading;
	static nsIRDFResource	*kNC_Child;

	char			*mLine;

public:

	NS_DECL_ISUPPORTS

			FTPDataSourceCallback(nsIRDFDataSource *ds, nsIRDFResource *parent);
	virtual		~FTPDataSourceCallback(void);

#ifdef NECKO
	// nsIStreamObserver methods:
    NS_DECL_NSISTREAMOBSERVER

	// nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER
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



class FTPDataSource : public nsIRDFFTPDataSource
{
private:
	static PRInt32		gRefCnt;

    // pseudo-constants
	static nsIRDFResource	*kNC_Child;
	static nsIRDFResource	*kNC_Name;
	static nsIRDFResource	*kNC_URL;
	static nsIRDFResource	*kNC_FTPObject;
	static nsIRDFResource	*kRDF_InstanceOf;
	static nsIRDFResource	*kRDF_type;

	static nsIRDFResource	*kNC_FTPCommand_Refresh;
	static nsIRDFResource	*kNC_FTPCommand_DeleteFolder;
	static nsIRDFResource	*kNC_FTPCommand_DeleteFile;

	NS_METHOD	GetFTPListing(nsIRDFResource *source, nsISimpleEnumerator** aResult);
	NS_METHOD	GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult);
	NS_METHOD	GetName(nsIRDFResource *source, nsIRDFLiteral** aResult);

protected:
	nsIRDFDataSource	*mInner;

public:

	NS_DECL_ISUPPORTS

                FTPDataSource(void);
	virtual		~FTPDataSource(void);

    nsresult Init();

//friend	class		FTPDataSourceCallback;

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
	NS_IMETHOD	GetAllResources(nsISimpleEnumerator** aCursor);
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
};



static	nsIRDFService	*gRDFService = nsnull;
static	FTPDataSource	*gFTPDataSource = nsnull;

nsIRDFResource		*FTPDataSource::kNC_Child;
nsIRDFResource		*FTPDataSource::kNC_Name;
nsIRDFResource		*FTPDataSource::kNC_URL;
nsIRDFResource		*FTPDataSource::kNC_FTPObject;
nsIRDFResource		*FTPDataSource::kRDF_InstanceOf;
nsIRDFResource		*FTPDataSource::kRDF_type;

nsIRDFResource		*FTPDataSource::kNC_FTPCommand_Refresh;
nsIRDFResource		*FTPDataSource::kNC_FTPCommand_DeleteFolder;
nsIRDFResource		*FTPDataSource::kNC_FTPCommand_DeleteFile;

PRInt32			FTPDataSource::gRefCnt;
PRInt32			FTPDataSourceCallback::gRefCnt;

nsIRDFResource		*FTPDataSourceCallback::kNC_Child;
nsIRDFResource		*FTPDataSourceCallback::kNC_loading;

static const char	kFTPprotocol[] = "ftp:";
static const char	kFTPcommand[] = "http://home.netscape.com/NC-rdf#ftpcommand?";



static PRBool
isFTPURI(nsIRDFResource *r)
{
	PRBool		isFTPURIFlag = PR_FALSE;
        const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kFTPprotocol, sizeof(kFTPprotocol) - 1)))
	{
		isFTPURIFlag = PR_TRUE;
	}
	return(isFTPURIFlag);
}



static PRBool
isFTPCommand(nsIRDFResource *r)
{
	PRBool		isFTPCommandFlag = PR_FALSE;
        const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kFTPcommand, sizeof(kFTPcommand) - 1)))
	{
		isFTPCommandFlag = PR_TRUE;
	}
	return(isFTPCommandFlag);
}



static PRBool
isFTPDirectory(nsIRDFResource *r)
{
	PRBool		isFTPDirectoryFlag = PR_FALSE;
	const char	*uri = nsnull;
	int		len;
	
	r->GetValueConst(&uri);
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

		gRDFService->GetResource(NC_NAMESPACE_URI "ftpcommand?refresh",      &kNC_FTPCommand_Refresh);
		gRDFService->GetResource(NC_NAMESPACE_URI "ftpcommand?deletefolder", &kNC_FTPCommand_DeleteFolder);
		gRDFService->GetResource(NC_NAMESPACE_URI "ftpcommand?deletefile",   &kNC_FTPCommand_DeleteFile);

		gFTPDataSource = this;
	}
}



FTPDataSource::~FTPDataSource (void)
{
	if (--gRefCnt == 0)
	{
		NS_RELEASE(kNC_Child);
		NS_RELEASE(kNC_Name);
		NS_RELEASE(kNC_URL);
		NS_RELEASE(kNC_FTPObject);
		NS_RELEASE(kRDF_InstanceOf);
		NS_RELEASE(kRDF_type);

		NS_RELEASE(kNC_FTPCommand_Refresh);
		NS_RELEASE(kNC_FTPCommand_DeleteFolder);
		NS_RELEASE(kNC_FTPCommand_DeleteFile);

		NS_RELEASE(mInner);

		gFTPDataSource = nsnull;
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
		gRDFService = nsnull;
	}
}



NS_IMPL_ISUPPORTS(FTPDataSource, nsIRDFDataSource::GetIID());



nsresult
FTPDataSource::Init()
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
				nsnull, nsIRDFDataSource::GetIID(), (void **)&mInner)))
		return rv;

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this, PR_FALSE)))
		return rv;

	return NS_OK;
}



NS_IMETHODIMP
FTPDataSource::GetURI(char **uri)
{
	if ((*uri = nsXPIDLCString::Copy("rdf:ftp")) == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

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
	nsresult	rv = NS_OK;

	nsXPIDLCString uri;
	rv = source->GetValue( getter_Copies(uri) );
	if (NS_FAILED(rv))
		return(rv);

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
	PRInt32		slash = url.RFindChar('/');
	if (slash > 0)
	{
		url.Cut(0, slash+1);
	}

	// unescape basename
	rv = NS_ERROR_NULL_POINTER;
	char	*baseFilename = url.ToNewCString();
	if (baseFilename)
	{
		baseFilename = nsUnescape(baseFilename);
		if (baseFilename)
		{
			url = baseFilename;
			nsCRT::free(baseFilename);
			baseFilename = nsnull;

			nsIRDFLiteral	*literal;
			if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(url.GetUnicode(), &literal)))
			{
				*aResult = literal;
			}
		}
	}
	return(rv);
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
	else if (isFTPCommand(source))
	{
		nsAutoString	name;
		if (property == kNC_Name)
		{
			if (source == kNC_FTPCommand_Refresh)
			{
				name = "Refresh FTP file listing";		// XXX localization
			}
			else if (source == kNC_FTPCommand_DeleteFolder)
			{
				name = "Delete remote FTP folder";		// XXX localization
			}
			else if (source == kNC_FTPCommand_DeleteFile)
			{
				name = "Delete remote FTP file";		// XXX localization
			}
			if (name.Length() > 0)
			{
				nsIRDFLiteral	*literal;
				rv = gRDFService->GetLiteral(name.GetUnicode(), &literal);

				rv = literal->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
				NS_RELEASE(literal);

				return rv;
			}
		}
	}
	return mInner->GetTarget(source, property, tv, target);
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
		gRDFService->GetResource(NC_NAMESPACE_URI "loading", &kNC_loading);
	}
}



FTPDataSourceCallback::~FTPDataSourceCallback()
{
	NS_IF_RELEASE(mDataSource);
	NS_IF_RELEASE(mParent);

	if (mLine)
	{
		nsCRT::free(mLine);
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
#ifdef NECKO
FTPDataSourceCallback::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
#else
FTPDataSourceCallback::OnStartRequest(nsIURI *aURL, const char *aContentType)
#endif
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



#ifndef NECKO
NS_IMETHODIMP
FTPDataSourceCallback::OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax) 
{
	return(NS_OK);
}



NS_IMETHODIMP
FTPDataSourceCallback::OnStatus(nsIURI* aURL, const PRUnichar* aMsg)
{
	return(NS_OK);
}
#endif



NS_IMETHODIMP
#ifdef NECKO
FTPDataSourceCallback::OnStopRequest(nsIChannel* channel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg) 
#else
FTPDataSourceCallback::OnStopRequest(nsIURI* aURL, nsresult aStatus, const PRUnichar* aMsg) 
#endif
{
	nsAutoString		trueStr("true");
	nsIRDFLiteral		*literal = nsnull;
	nsresult		rv;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(trueStr.GetUnicode(), &literal)))
	{
		mDataSource->Unassert(mParent, kNC_loading, literal);
		NS_RELEASE(literal);
	}
	return(NS_OK);
}



// stream listener methods



#ifndef NECKO
NS_IMETHODIMP
FTPDataSourceCallback::GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo)
{
	return(NS_OK);
}
#endif



NS_IMETHODIMP
#ifdef NECKO
FTPDataSourceCallback::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt,
                                       nsIInputStream *aIStream, PRUint32 sourceOffset, PRUint32 aLength)
#else
FTPDataSourceCallback::OnDataAvailable(nsIURI* aURL, nsIInputStream *aIStream, PRUint32 aLength)
#endif
{
	nsresult	rv = NS_OK;

	if (aLength > 0)
	{
		nsString	line;
		if (mLine)
		{
			line += mLine;
			nsCRT::free(mLine);
			mLine = nsnull;
		}

		char	buffer[257];
		while (aLength > 0)
		{
			PRUint32	count=0, numBytes = (aLength > sizeof(buffer)-1 ? sizeof(buffer)-1 : aLength);
			if (NS_FAILED(rv = aIStream->Read(buffer, numBytes, &count)))
			{
				printf("FTP datasource read failure.\n");
				break;
			}
			if (numBytes != count)
			{
				printf("FTP datasource read # of bytes failure.\n");
				break;
			}
			buffer[count] = '\0';
			line += buffer;
			aLength -= count;
		}
		PRInt32 eol = line.FindCharInSet("\r\n");

		nsAutoString	oneLiner("");
		if (eol >= 0)
		{
			line.Left(oneLiner, eol);
			line.Cut(0, eol+1);
		}
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

				// ignore certain paths (if they start with a slash, or ".", or ".."
				if ((file.Find("/") == 0) || file.Equals(".") || file.Equals(".."))
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
					nsCRT::free(name);
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

#ifndef NECKO
        nsIURI		*url;
        rv = NS_NewURL(&url, (const char*) ftpURL);
		if (NS_SUCCEEDED(rv))
		{
			FTPDataSourceCallback	*callback = new FTPDataSourceCallback(mInner, source);
			if (nsnull != callback)
			{
				rv = NS_OpenURL(url, NS_STATIC_CAST(nsIStreamListener *, callback));
			}
		}
#else
        nsIURI		*url;
        rv = NS_NewURI(&url, (const char*) ftpURL);
		if (NS_SUCCEEDED(rv))
		{
			FTPDataSourceCallback	*callback = new FTPDataSourceCallback(mInner, source);
			if (nsnull != callback)
			{
				rv = NS_OpenURI(NS_STATIC_CAST(nsIStreamListener *, callback), nsnull, url, nsnull);
			}
            NS_RELEASE(url);
		}
#endif // NECKO
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
	return mInner->GetTargets(source, property, tv, targets);
//	return NS_NewEmptyEnumerator(targets);
}



NS_IMETHODIMP
FTPDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FTPDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FTPDataSource::Change(nsIRDFResource* aSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aOldTarget,
                      nsIRDFNode* aNewTarget)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FTPDataSource::Move(nsIRDFResource* aOldSource,
                    nsIRDFResource* aNewSource,
                    nsIRDFResource* aProperty,
                    nsIRDFNode* aTarget)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FTPDataSource::HasAssertion(nsIRDFResource *source,
                             nsIRDFResource *property,
                             nsIRDFNode *target,
                             PRBool tv,
                             PRBool *hasAssertion /* out */)
{
	nsresult		rv = NS_OK;

	*hasAssertion = PR_FALSE;

	// we only have positive assertions in the FTP data source.

	rv = mInner->HasAssertion(source, property, target,
		tv, hasAssertion);
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
	return mInner->ArcLabelsOut(source, labels);
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
FTPDataSource::GetAllCommands(nsIRDFResource* source,nsIEnumerator/*<nsIRDFResource>*/** commands)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FTPDataSource::GetAllCmds(nsIRDFResource* source, nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	if (isFTPURI(source))
	{
		nsresult	rv;
		nsXPIDLCString	uri;
		rv = source->GetValue( getter_Copies(uri) );
		if (NS_FAILED(rv))	return(rv);

		nsCOMPtr<nsISupportsArray>	cmdArray;
		rv = NS_NewISupportsArray(getter_AddRefs(cmdArray));
		if (NS_FAILED(rv))	return(rv);

		if (isFTPDirectory(source))
		{
			cmdArray->AppendElement(kNC_FTPCommand_Refresh);
			cmdArray->AppendElement(kNC_FTPCommand_DeleteFolder);
		}
		else
		{
			cmdArray->AppendElement(kNC_FTPCommand_DeleteFile);
		}

		nsISimpleEnumerator		*result = new nsArrayEnumerator(cmdArray);
		if (!result)
			return(NS_ERROR_OUT_OF_MEMORY);
		NS_ADDREF(result);
		*commands = result;
		return(NS_OK);
	}
	return(NS_NewEmptyEnumerator(commands));
}



NS_IMETHODIMP
FTPDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
FTPDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
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

        nsresult rv;
        rv = gFTPDataSource->Init();
        if (NS_FAILED(rv)) {
            delete gFTPDataSource;
            gFTPDataSource = nsnull;
            return rv;
        }
	}
	NS_ADDREF(gFTPDataSource);
	*result = gFTPDataSource;
	return NS_OK;
}
