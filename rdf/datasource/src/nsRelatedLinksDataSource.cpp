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
  Implementation for a Related Links RDF data store.
 */

#include "nsCOMPtr.h"
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
#include "nsIRelatedLinksDataSource.h"

static NS_DEFINE_CID(kRDFServiceCID,                           NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,                NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_IID(kISupportsIID,                            NS_ISUPPORTS_IID);

static const char kURINC_RelatedLinksRoot[] = "NC:RelatedLinks";

class RelatedLinksDataSourceCallback : public nsIStreamListener
{
private:
	nsIRDFDataSource	*mDataSource;
	nsIRDFResource		*mParent;
	static PRInt32		gRefCnt;
	nsVoidArray		*mParentArray;

    // pseudo-constants
	static nsIRDFResource	*kNC_Child;
	static nsIRDFResource	*kNC_Name;
	static nsIRDFResource	*kNC_RelatedLinksRoot;

	char			*mLine;

public:

	NS_DECL_ISUPPORTS

			RelatedLinksDataSourceCallback(nsIRDFDataSource *ds, nsIRDFResource *parent);
	virtual		~RelatedLinksDataSourceCallback(void);

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



class RelatedLinksDataSource : public nsIRDFRelatedLinksDataSource
{
private:
	char			*mURI;
	char			*mRelatedLinksURL;
	PRBool			mPerformQuery;
	static PRInt32		gRefCnt;

    // pseudo-constants
	static nsIRDFResource	*kNC_RelatedLinksRoot;
	static nsIRDFResource	*kNC_Child;
	static nsIRDFResource	*kNC_Name;
	static nsIRDFResource	*kNC_URL;
	static nsIRDFResource	*kNC_pulse;
	static nsIRDFResource	*kNC_FTPObject;
	static nsIRDFResource	*kRDF_InstanceOf;
	static nsIRDFResource	*kRDF_type;

	NS_METHOD	SetRelatedLinksURL(const char *url);
	NS_METHOD	GetRelatedLinksListing(nsIRDFResource *source);
//	NS_METHOD	GetURL(nsIRDFResource *source, nsVoidArray **array);
//	NS_METHOD	GetName(nsIRDFResource *source, nsVoidArray **array);

protected:
	nsIRDFDataSource	*mInner;
	nsVoidArray		*mObservers;

public:

	NS_DECL_ISUPPORTS

			RelatedLinksDataSource(void);
	virtual		~RelatedLinksDataSource(void);

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

nsIRDFResource		*RelatedLinksDataSource::kNC_RelatedLinksRoot;
nsIRDFResource		*RelatedLinksDataSource::kNC_Child;
nsIRDFResource		*RelatedLinksDataSource::kNC_Name;
nsIRDFResource		*RelatedLinksDataSource::kNC_URL;
nsIRDFResource		*RelatedLinksDataSource::kNC_pulse;
nsIRDFResource		*RelatedLinksDataSource::kNC_FTPObject;
nsIRDFResource		*RelatedLinksDataSource::kRDF_InstanceOf;
nsIRDFResource		*RelatedLinksDataSource::kRDF_type;

PRInt32			RelatedLinksDataSource::gRefCnt;
PRInt32			RelatedLinksDataSourceCallback::gRefCnt;

nsIRDFResource		*RelatedLinksDataSourceCallback::kNC_Child;
nsIRDFResource		*RelatedLinksDataSourceCallback::kNC_Name;
nsIRDFResource		*RelatedLinksDataSourceCallback::kNC_RelatedLinksRoot;



RelatedLinksDataSource::RelatedLinksDataSource(void)
	: mURI(nsnull),
	  mRelatedLinksURL(nsnull),
	  mPerformQuery(PR_FALSE),
	  mInner(nsnull),
	  mObservers(nsnull)
{
	NS_INIT_REFCNT();

	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   nsIRDFService::GetIID(),
                                                   (nsISupports**) &gRDFService);
		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(kURINC_RelatedLinksRoot, &kNC_RelatedLinksRoot);
		gRDFService->GetResource(NC_NAMESPACE_URI "child", &kNC_Child);
		gRDFService->GetResource(NC_NAMESPACE_URI "Name", &kNC_Name);
		gRDFService->GetResource(NC_NAMESPACE_URI "URL", &kNC_URL);
		gRDFService->GetResource(NC_NAMESPACE_URI "pulse", &kNC_pulse);
		gRDFService->GetResource(NC_NAMESPACE_URI "FTPObject", &kNC_FTPObject);
		gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_InstanceOf);
		gRDFService->GetResource(RDF_NAMESPACE_URI "type", &kRDF_type);
	}
}



RelatedLinksDataSource::~RelatedLinksDataSource (void)
{
	gRDFService->UnregisterDataSource(this);

	if (nsnull != mURI)
	{
		PL_strfree(mURI);
		mURI = nsnull;
	}
	if (nsnull != mRelatedLinksURL)
	{
		PL_strfree(mRelatedLinksURL);
		mRelatedLinksURL = nsnull;
	}

	if (--gRefCnt == 0)
	{
		NS_RELEASE(kNC_RelatedLinksRoot);
		NS_RELEASE(kNC_Child);
		NS_RELEASE(kNC_Name);
		NS_RELEASE(kNC_URL);
		NS_RELEASE(kNC_pulse);
		NS_RELEASE(kNC_FTPObject);
		NS_RELEASE(kRDF_InstanceOf);
		NS_RELEASE(kRDF_type);

		if (nsnull != mObservers)
		{
			for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i)
			{
				nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
				NS_RELEASE(obs);
			}
			delete mObservers;
			mObservers = nsnull;
		}

		if (nsnull != mInner)
		{
			NS_RELEASE(mInner);
			mInner = nsnull;
		}

		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
		gRDFService = nsnull;
	}
}



//NS_IMPL_ISUPPORTS(RelatedLinksDataSource, kIRDFDataSourceIID);
NS_IMPL_ADDREF(RelatedLinksDataSource);
NS_IMPL_RELEASE(RelatedLinksDataSource);



NS_IMETHODIMP
RelatedLinksDataSource::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(nsIRDFRelatedLinksDataSource::GetIID()) ||
        aIID.Equals(nsIRDFDataSource::GetIID()) ||
        aIID.Equals(nsISupports::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIRDFRelatedLinksDataSource*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }
}



NS_IMETHODIMP
RelatedLinksDataSource::Init(const char *uri)
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	if ((mURI = PL_strdup(uri)) == nsnull)
		return(rv);

	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
			nsnull, nsIRDFDataSource::GetIID(), (void **)&mInner)))
		return(rv);
	if (NS_FAILED(rv = mInner->Init(mURI)))
		return(rv);

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this, PR_FALSE)))
		return(rv);

	return(NS_OK);
}



NS_IMETHODIMP
RelatedLinksDataSource::GetURI(char **uri)
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	if ((*uri = nsXPIDLCString::Copy(mURI)) != nsnull)
		rv = NS_OK;
	return(rv);
}



NS_IMETHODIMP
RelatedLinksDataSource::GetSource(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFResource** source /* out */)
{
	nsresult	result = NS_RDF_NO_VALUE;
	if (mInner)	result = mInner->GetSource(property, target, tv, source);
	return(result);
}



NS_IMETHODIMP
RelatedLinksDataSource::GetSources(nsIRDFResource *property,
                           nsIRDFNode *target,
			   PRBool tv,
                           nsISimpleEnumerator **sources /* out */)
{
	nsresult	result = NS_RDF_NO_VALUE;
	if (mInner)	result = mInner->GetSources(property, target, tv, sources);
	return(result);
}



NS_IMETHODIMP
RelatedLinksDataSource::GetTarget(nsIRDFResource *source,
                          nsIRDFResource *property,
                          PRBool tv,
                          nsIRDFNode **target /* out */)
{
	nsresult		rv = NS_RDF_NO_VALUE;

	// we only have positive assertions in the Related Links data source.
	if (! tv)
		return rv;

	nsVoidArray		*array = nsnull;
	if (source == kNC_RelatedLinksRoot)
	{
		if (property == kNC_pulse)
		{
			nsAutoString	pulse("15");
			nsIRDFLiteral	*pulseLiteral;
			gRDFService->GetLiteral(pulse.GetUnicode(), &pulseLiteral);
			array = new nsVoidArray();
			if (array)
			{
				array->AppendElement(pulseLiteral);
				rv = NS_OK;
			}
		}
		else if (property == kRDF_type)
		{
			nsXPIDLCString	uri;
			kNC_FTPObject->GetValue( getter_Copies(uri) );
			if (uri)
			{
				nsAutoString	url(uri);
				nsIRDFLiteral	*literal;
				gRDFService->GetLiteral(url.GetUnicode(), &literal);
				*target = literal;
				rv = NS_OK;
			}
			return(rv);
		}
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
		rv = NS_RDF_NO_VALUE;
		if (mInner)	rv = mInner->GetTarget(source, property, tv, target);
	}
	return(rv);
}



// Related Links class for Netlib callback



RelatedLinksDataSourceCallback::RelatedLinksDataSourceCallback(nsIRDFDataSource *ds, nsIRDFResource *parent)
	: mDataSource(ds),
	  mParent(parent),
	  mParentArray(nsnull),
	  mLine(nsnull)
{
	NS_ADDREF(mDataSource);
	NS_ADDREF(mParent);

	NS_INIT_REFCNT();
	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   nsIRDFService::GetIID(),
                                                   (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(NC_NAMESPACE_URI "child", &kNC_Child);
		gRDFService->GetResource(NC_NAMESPACE_URI "Name",  &kNC_Name);
		gRDFService->GetResource(kURINC_RelatedLinksRoot, &kNC_RelatedLinksRoot);
		
		if (nsnull != (mParentArray = new nsVoidArray()))
		{
			mParentArray->AppendElement(parent);
		}
	}
}



RelatedLinksDataSourceCallback::~RelatedLinksDataSourceCallback()
{
	NS_IF_RELEASE(mDataSource);
	NS_IF_RELEASE(mParent);

	if (mParentArray)
	{
		delete mParentArray;
		mParentArray = nsnull;
	}

	if (mLine)
	{
		delete [] mLine;
		mLine = nsnull;
	}

	if (--gRefCnt == 0)
	{
		NS_RELEASE(kNC_Child);
		NS_RELEASE(kNC_Name);
		NS_RELEASE(kNC_RelatedLinksRoot);
	}
}



// stream observer methods



NS_IMETHODIMP
RelatedLinksDataSourceCallback::OnStartBinding(nsIURL *aURL, const char *aContentType)
{
	return(NS_OK);
}



NS_IMETHODIMP
RelatedLinksDataSourceCallback::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax) 
{
	return(NS_OK);
}



NS_IMETHODIMP
RelatedLinksDataSourceCallback::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
	return(NS_OK);
}



NS_IMETHODIMP
RelatedLinksDataSourceCallback::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg) 
{
	return(NS_OK);
}



// stream listener methods



NS_IMETHODIMP
RelatedLinksDataSourceCallback::GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo)
{
	return(NS_OK);
}



NS_IMETHODIMP
RelatedLinksDataSourceCallback::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
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
					if (NS_SUCCEEDED(rv = rdf_CreateAnonymousResource(rlRoot, getter_AddRefs(newTopic))))
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
								for (PRInt32 i=0; i<mParentArray->Count(); i++)	printf("    ");
								printf("Topic: '%s'\n", name);
#endif
								delete [] name;
							}
						}
						PRInt32		numParents = mParentArray->Count();
						if (numParents > 0)
						{
							nsIRDFResource	*parent = (nsIRDFResource *)(mParentArray->ElementAt(numParents - 1));
							mDataSource->Assert(parent, kNC_Child, newTopic, PR_TRUE);
						}
						mParentArray->AppendElement(newTopic);
					}

				}
				else
				{
					theStart = oneLiner.Find("</Topic>");
					if (theStart == 0)
					{
						PRInt32		numParents = mParentArray->Count();
						if (numParents > 0)
						{
							mParentArray->RemoveElementAt(numParents - 1);
						}
#ifdef	DEBUG
						for (PRInt32 i=0; i<mParentArray->Count(); i++)	printf("    ");
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
					PRInt32		numParents = mParentArray->Count();
					if (numParents > 1)
					{
						nsAutoString	rlRoot(kURINC_RelatedLinksRoot);
						rv = rdf_CreateAnonymousResource(rlRoot, getter_AddRefs(relatedLinksChild));
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
								for (PRInt32 i=0; i<mParentArray->Count(); i++)	printf("    ");
								printf("Name: '%s'  URL: '%s'\n", name, url);
#endif
								delete [] name;
							}
						}
						if (numParents > 0)
						{
							nsIRDFResource	*parent = (nsIRDFResource *)(mParentArray->ElementAt(numParents - 1));
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



NS_IMPL_ISUPPORTS(RelatedLinksDataSourceCallback, nsIRDFRelatedLinksDataSourceCallback::GetIID());



NS_METHOD
RelatedLinksDataSource::GetRelatedLinksListing(nsIRDFResource *source)
{
	nsresult	rv = NS_ERROR_FAILURE;

	if (mPerformQuery == PR_TRUE)
	{
		mPerformQuery = PR_FALSE;
		if (nsnull != mRelatedLinksURL)
		{
			nsAutoString	relatedLinksQueryURL("http://www-rl.netscape.com/wtgn?");
			relatedLinksQueryURL += mRelatedLinksURL;

			char *queryURL = relatedLinksQueryURL.ToNewCString();
			if (nsnull != queryURL)
			{
				nsIURL		*url;
				if (NS_SUCCEEDED(rv = NS_NewURL(&url, (const char*) queryURL)))
				{
					RelatedLinksDataSourceCallback	*callback = new RelatedLinksDataSourceCallback(mInner, source);
					if (nsnull != callback)
					{
						rv = NS_OpenURL(url, NS_STATIC_CAST(nsIStreamListener *, callback));
					}
				}
				delete [] queryURL;
			}
		}
	}
	return(rv);
}



NS_IMETHODIMP
RelatedLinksDataSource::GetTargets(nsIRDFResource *source,
                           nsIRDFResource *property,
                           PRBool tv,
                           nsISimpleEnumerator **targets /* out */)
{
	nsresult		rv = NS_ERROR_FAILURE;

	// we only have positive assertions in the Related Links data source.

	*targets = nsnull;
	if ((tv) && (source == kNC_RelatedLinksRoot))
	{
		if ((property == kNC_Child) && (mPerformQuery == PR_TRUE))
		{
			rv = GetRelatedLinksListing(source);
            if (NS_FAILED(rv)) return rv;

            return NS_NewEmptyEnumerator(targets);
		}
		else if (property == kNC_pulse)
		{
			nsAutoString	pulse("15");
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
		else if (property == kRDF_type)
		{
			nsXPIDLCString	uri;
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

    if (mInner)	{
        return mInner->GetTargets(source, property, tv, targets);
	}
    else {
        return NS_NewEmptyEnumerator(targets);
    }
}



NS_IMETHODIMP
RelatedLinksDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
//	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
RelatedLinksDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
//	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
RelatedLinksDataSource::HasAssertion(nsIRDFResource *source,
                             nsIRDFResource *property,
                             nsIRDFNode *target,
                             PRBool tv,
                             PRBool *hasAssertion /* out */)
{
	PRBool			retVal = PR_FALSE;
	nsresult		rv = NS_OK;

	*hasAssertion = PR_FALSE;

	// we only have positive assertions in the Related Links data source.

	if ((tv) && (source == kNC_RelatedLinksRoot))
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
		if (mInner)	rv = mInner->HasAssertion(source, property, target, tv, hasAssertion);
	}
	return (rv);
}



NS_IMETHODIMP
RelatedLinksDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
	nsresult	result = NS_RDF_NO_VALUE;
	if (mInner)	result = mInner->ArcLabelsIn(node, labels);
	return(result);
}



NS_IMETHODIMP
RelatedLinksDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsISimpleEnumerator **labels /* out */)
{
	nsresult		rv = NS_RDF_NO_VALUE;

	*labels = nsnull;

	if (source == kNC_RelatedLinksRoot)
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
	else if (mInner) {
        return mInner->ArcLabelsOut(source, labels);
	}
    else {
        return NS_NewEmptyEnumerator(labels);
    }
}



NS_IMETHODIMP
RelatedLinksDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	nsresult	result;
	if (mInner)	result = mInner->GetAllResources(aCursor);
	return(result);
}



NS_IMETHODIMP
RelatedLinksDataSource::AddObserver(nsIRDFObserver *n)
{
	nsresult	result = NS_OK;
	if (mInner)	result = mInner->AddObserver(n);

	if (nsnull == mObservers)
	{
		if ((mObservers = new nsVoidArray()) == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
	}
	mObservers->AppendElement(n);

	return(result);
}



NS_IMETHODIMP
RelatedLinksDataSource::RemoveObserver(nsIRDFObserver *n)
{
	NS_ASSERTION(n != nsnull, "null ptr");
	if (! n)
		return NS_ERROR_NULL_POINTER;

	nsresult	result = NS_OK;
	if (mInner)	result = mInner->RemoveObserver(n);

//	NS_AUTOLOCK(mLock);
	if (! mObservers)
		return NS_OK;
	mObservers->RemoveElement(n);

	return(result);
}



NS_IMETHODIMP
RelatedLinksDataSource::Flush()
{
	nsresult	result = NS_OK;
	if (mInner)	result = mInner->Flush();
	return(result);
}



NS_IMETHODIMP
RelatedLinksDataSource::GetAllCommands(nsIRDFResource* source,nsIEnumerator/*<nsIRDFResource>*/** commands)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
RelatedLinksDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
RelatedLinksDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
RelatedLinksDataSource::SetRelatedLinksURL(const char *url)
{
	if (nsnull == url)
		return(NS_ERROR_NULL_POINTER);
	if (nsnull != mRelatedLinksURL)
	{
		PL_strfree(mRelatedLinksURL);
		mRelatedLinksURL = nsnull;
	}
	if (nsnull != url)
	{
		mRelatedLinksURL = PL_strdup(url);
		mPerformQuery = PR_TRUE;
	}
	if (nsnull != mInner)
	{
		// first, forget any related links current in memory
		// (quickest way is to delete in-memory data source)
		NS_RELEASE(mInner);
		mInner = nsnull;

		// now, create a new in-memory data source
		nsresult	rv;
		if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
			nsnull, nsIRDFDataSource::GetIID(), (void **)&mInner)))
		{
			return(rv);
		}
		if (NS_FAILED(rv = mInner->Init(mURI)))
		{
			return(rv);
		}
		// add any observers into new in-memory data source
		if (nsnull != mObservers)
		{
			for (PRInt32 i = 0; i < mObservers->Count(); i++)
			{
				nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
				mInner->AddObserver(obs);
			}
		}
		// finally, try and fetch any new related links
		// GetRelatedLinksListing(kNC_RelatedLinksRoot);
	}
	return(NS_OK);
}



nsresult
NS_NewRDFRelatedLinksDataSource(nsIRDFDataSource **result)
{
	if (!result)
		return NS_ERROR_NULL_POINTER;

	// Note: can have many Related Links data sources!
	nsIRDFRelatedLinksDataSource	*relatedLinksDataSource;
	if ((relatedLinksDataSource = new RelatedLinksDataSource()) == nsnull)
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}
	NS_ADDREF(relatedLinksDataSource);
	*result = (nsIRDFDataSource *)relatedLinksDataSource;
	return NS_OK;
}



