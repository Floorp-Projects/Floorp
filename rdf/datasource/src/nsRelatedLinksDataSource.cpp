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
  Implementation for a Related Links RDF data store.
 */

#include "nsCOMPtr.h"
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
#include "nsRelatedLinksDataSource.h"



static NS_DEFINE_CID(kRDFServiceCID,                           NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,                NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_IID(kIRDFServiceIID,                          NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,                       NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFRelatedLinksDataSourceIID,           NS_IRDFRELATEDLINKSDATAOURCE_IID);
static NS_DEFINE_IID(kIRDFAssertionCursorIID,                  NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,                           NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,                    NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kISupportsIID,                            NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFResourceIID,                         NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFNodeIID,                             NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,                          NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFRelatedLinksDataSourceCallbackIID,   NS_IRDFRELATEDLINKSDATASOURCECALLBACK_IID);

static const char kURINC_RelatedLinksRoot[] = "NC:RelatedLinks";

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, URL);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, pulse);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, FTPObject);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Seq);


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
                                                   kIRDFServiceIID,
                                                   (nsISupports**) &gRDFService);
		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(kURINC_RelatedLinksRoot, &kNC_RelatedLinksRoot);
		gRDFService->GetResource(kURINC_child, &kNC_Child);
		gRDFService->GetResource(kURINC_Name, &kNC_Name);
		gRDFService->GetResource(kURINC_URL, &kNC_URL);
		gRDFService->GetResource(kURINC_pulse, &kNC_pulse);
		gRDFService->GetResource(kURINC_FTPObject, &kNC_FTPObject);
		gRDFService->GetResource(kURIRDF_instanceOf, &kRDF_InstanceOf);
		gRDFService->GetResource(kURIRDF_type, &kRDF_type);
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
			nsnull, kIRDFDataSourceIID, (void **)&mInner)))
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
                           nsIRDFAssertionCursor **sources /* out */)
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
	if (peq(source, kNC_RelatedLinksRoot))
	{
		if (peq(property, kNC_pulse))
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
		else if (peq(property, kRDF_type))
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
                                                   kIRDFServiceIID,
                                                   (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(kURINC_child, &kNC_Child);
		gRDFService->GetResource(kURINC_Name,  &kNC_Name);
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



NS_IMPL_ISUPPORTS(RelatedLinksDataSourceCallback, kIRDFRelatedLinksDataSourceCallbackIID);



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
                           nsIRDFAssertionCursor **targets /* out */)
{
	nsVoidArray		*array = nsnull;
	nsresult		rv = NS_ERROR_FAILURE;

	// we only have positive assertions in the Related Links data source.

	*targets = nsnull;
	if ((tv) && peq(source, kNC_RelatedLinksRoot))
	{
		if (peq(property, kNC_Child))
		{
			rv = GetRelatedLinksListing(source);
		}
		else if (peq(property, kNC_pulse))
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
		else if (peq(property, kRDF_type))
		{
			nsXPIDLCString	uri;
			kNC_FTPObject->GetValue( getter_Copies(uri) );
			if (uri)
			{
				nsAutoString	url(uri);
				nsIRDFLiteral	*literal;
				gRDFService->GetLiteral(url.GetUnicode(), &literal);
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
			*targets = new RelatedLinksCursor(this, source, property, PR_FALSE, array);
			NS_ADDREF(*targets);
		}
	}
	if (nsnull == *targets)
	{
		rv = NS_RDF_NO_VALUE;
		if (mInner)	rv = mInner->GetTargets(source, property, tv, targets);
	}
	return(rv);
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
	nsresult		rv = NS_ERROR_FAILURE;

	// we only have positive assertions in the Related Links data source.

	*hasAssertion = PR_FALSE;
	if ((tv) && peq(source, kNC_RelatedLinksRoot))
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
		rv = NS_RDF_NO_VALUE;
		if (mInner)	rv = mInner->HasAssertion(source, property, target, tv, hasAssertion);
	}
	return (rv);
}



NS_IMETHODIMP
RelatedLinksDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsIRDFArcsInCursor ** labels /* out */)
{
	nsresult	result = NS_RDF_NO_VALUE;
	if (mInner)	result = mInner->ArcLabelsIn(node, labels);
	return(result);
}



NS_IMETHODIMP
RelatedLinksDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsIRDFArcsOutCursor **labels /* out */)
{
	nsresult		rv = NS_RDF_NO_VALUE;

	*labels = nsnull;

	if (peq(source, kNC_RelatedLinksRoot))
	{
		nsVoidArray *temp = new nsVoidArray();
		if (nsnull == temp)
			return NS_ERROR_OUT_OF_MEMORY;
		temp->AppendElement(kNC_Child);
		temp->AppendElement(kNC_pulse);
		*labels = new RelatedLinksCursor(this, source, kNC_Child, PR_TRUE, temp);
		if (nsnull != *labels)
		{
			NS_ADDREF(*labels);
			rv = NS_OK;
		}
	}
	else
	{
		if (mInner)	rv = mInner->ArcLabelsOut(source, labels);
	}
	return(rv);

}



NS_IMETHODIMP
RelatedLinksDataSource::GetAllResources(nsIRDFResourceCursor** aCursor)
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
			nsnull, kIRDFDataSourceIID, (void **)&mInner)))
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



RelatedLinksCursor::RelatedLinksCursor(nsIRDFDataSource *ds,
				nsIRDFResource *source,
				nsIRDFResource *property,
				PRBool isArcsOut,
				nsVoidArray *array)
	: mDataSource(ds),
	  mSource(source),
	  mProperty(property),
	  mArcsOut(isArcsOut),
	  mArray(array),
	  mCount(0),
	  mTarget(nsnull),
	  mValue(nsnull)
{
	NS_INIT_REFCNT();
	NS_ADDREF(mDataSource);
	NS_ADDREF(mSource);
	NS_ADDREF(mProperty);
}



RelatedLinksCursor::~RelatedLinksCursor(void)
{
	NS_IF_RELEASE(mDataSource);
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
RelatedLinksCursor::Advance(void)
{
	if (!mArray)
		return NS_ERROR_NULL_POINTER;
	if (mArray->Count() <= mCount)
		return NS_RDF_CURSOR_EMPTY;
	NS_IF_RELEASE(mValue);
	mTarget = mValue = (nsIRDFNode *)mArray->ElementAt(mCount++);
	NS_ADDREF(mValue);
	NS_ADDREF(mTarget);
	return NS_OK;
}



NS_IMETHODIMP
RelatedLinksCursor::GetValue(nsIRDFNode **aValue)
{
	if (nsnull == mValue)
		return NS_ERROR_NULL_POINTER;
	NS_ADDREF(mValue);
	*aValue = mValue;
	return NS_OK;
}



NS_IMETHODIMP
RelatedLinksCursor::GetDataSource(nsIRDFDataSource **aDataSource)
{
	NS_ADDREF(mDataSource);
	*aDataSource = mDataSource;
	return NS_OK;
}



NS_IMETHODIMP
RelatedLinksCursor::GetSource(nsIRDFResource **aResource)
{
	NS_ADDREF(mSource);
	*aResource = mSource;
	return NS_OK;
}



NS_IMETHODIMP
RelatedLinksCursor::GetLabel(nsIRDFResource **aPredicate)
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
RelatedLinksCursor::GetTarget(nsIRDFNode **aObject)
{
	if (nsnull != mTarget)
		NS_ADDREF(mTarget);
	*aObject = mTarget;
	return NS_OK;
}



NS_IMETHODIMP
RelatedLinksCursor::GetTruthValue(PRBool *aTruthValue)
{
	*aTruthValue = 1;
	return NS_OK;
}



NS_IMPL_ADDREF(RelatedLinksCursor);
NS_IMPL_RELEASE(RelatedLinksCursor);



NS_IMETHODIMP
RelatedLinksCursor::QueryInterface(REFNSIID iid, void **result)
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
