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
  Implementation for a find RDF data store.
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
#include "nsIRDFFind.h"
#include "nsFindDataSource.h"



static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kIRDFServiceIID,              NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,           NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFFindDataSourceIID,       NS_IRDFFINDDATAOURCE_IID);
static NS_DEFINE_IID(kIRDFAssertionCursorIID,      NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,               NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,        NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFResourceIID,             NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFNodeIID,                 NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,              NS_IRDFLITERAL_IID);



DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, URL);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, FindObject);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, pulse);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Seq);


static	nsIRDFService		*gRDFService = nsnull;
static	FindDataSource		*gFindDataSource = nsnull;

PRInt32 FindDataSource::gRefCnt;

nsIRDFResource		*FindDataSource::kNC_Child;
nsIRDFResource		*FindDataSource::kNC_Name;
nsIRDFResource		*FindDataSource::kNC_URL;
nsIRDFResource		*FindDataSource::kNC_FindObject;
nsIRDFResource		*FindDataSource::kNC_pulse;
nsIRDFResource		*FindDataSource::kRDF_InstanceOf;
nsIRDFResource		*FindDataSource::kRDF_type;



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
isFindURI(nsIRDFResource *r)
{
	PRBool		isFindURI = PR_FALSE;
        nsXPIDLCString uri;
	
	r->GetValue( getter_Copies(uri) );
	if (!strncmp(uri, "find:", 5))
	{
		isFindURI = PR_TRUE;
	}
	return(isFindURI);
}



FindDataSource::FindDataSource(void)
	: mURI(nsnull),
	  mObservers(nsnull)
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
	gRDFService->GetResource(kURINC_FindObject, &kNC_FindObject);
	gRDFService->GetResource(kURINC_FindObject, &kNC_pulse);
	gRDFService->GetResource(kURIRDF_instanceOf, &kRDF_InstanceOf);
	gRDFService->GetResource(kURIRDF_type, &kRDF_type);

        gFindDataSource = this;
    }
}



FindDataSource::~FindDataSource (void)
{
	gRDFService->UnregisterDataSource(this);

	PL_strfree(mURI);
	if (nsnull != mObservers)
	{
		for (PRInt32 i = mObservers->Count(); i >= 0; --i)
		{
			nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
			NS_RELEASE(obs);
		}
		delete mObservers;
		mObservers = nsnull;
	}

	if (--gRefCnt == 0)
	{
		NS_RELEASE(kNC_Child);
		NS_RELEASE(kNC_Name);
		NS_RELEASE(kNC_URL);
		NS_RELEASE(kNC_FindObject);
		NS_RELEASE(kNC_pulse);
		NS_RELEASE(kRDF_InstanceOf);
		NS_RELEASE(kRDF_type);

		gFindDataSource = nsnull;
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
		gRDFService = nsnull;
	}
}



// NS_IMPL_ISUPPORTS(FindDataSource, kIRDFFindDataSourceIID);
NS_IMPL_ISUPPORTS(FindDataSource, kIRDFDataSourceIID);



NS_IMETHODIMP
FindDataSource::Init(const char *uri)
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	if ((mURI = PL_strdup(uri)) == nsnull)
		return rv;

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this, PR_FALSE)))
		return rv;
	return NS_OK;
}



NS_IMETHODIMP
FindDataSource::GetURI(char **uri)
{
    if ((*uri = nsXPIDLCString::Copy(mURI)) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    else
        return NS_OK;
}



NS_IMETHODIMP
FindDataSource::GetSource(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFResource** source /* out */)
{
	return NS_RDF_NO_VALUE;
}



NS_IMETHODIMP
FindDataSource::GetSources(nsIRDFResource *property,
                           nsIRDFNode *target,
			   PRBool tv,
                           nsIRDFAssertionCursor **sources /* out */)
{
	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::GetTarget(nsIRDFResource *source,
                          nsIRDFResource *property,
                          PRBool tv,
                          nsIRDFNode **target /* out */)
{
	nsresult		rv = NS_RDF_NO_VALUE;

	// we only have positive assertions in the find data source.
	if (! tv)
		return rv;

	if (isFindURI(source))
	{
		nsVoidArray		*array = nsnull;

		if (peq(property, kNC_Name))
		{
//			rv = GetName(source, &array);
		}
		else if (peq(property, kNC_URL))
		{
			// note: lie and say there is no URL
//			rv = GetURL(source, &array);
			nsAutoString	url("");
			nsIRDFLiteral	*literal;
			gRDFService->GetLiteral(url.GetUnicode(), &literal);
			*target = literal;
			rv = NS_OK;
		}
		else if (peq(property, kRDF_type))
		{
                    nsXPIDLCString uri;
			kNC_FindObject->GetValue( getter_Copies(uri) );
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
		}
	}
	return(rv);
}



NS_METHOD
FindDataSource::parseResourceIntoFindTokens(nsIRDFResource *u, findTokenPtr tokens)
{
    nsXPIDLCString uri;
	char			*id, *token, *value;
	int			loop;
	nsresult		rv;

	if (NS_FAILED(rv = u->GetValue( getter_Copies(uri) )))	return(rv);

	printf("Find: %s\n", (const char*) uri);

	if (!(id = PL_strdup(uri + strlen("find:"))))	return(NS_ERROR_OUT_OF_MEMORY);

	/* parse ID, build up token list */
	if ((token = strtok(id, "&")) != NULL)
	{
		while (token != NULL)
		{
			if ((value = strstr(token, "=")) != NULL)
			{
				*value++ = '\0';
			}
			for (loop=0; tokens[loop].token != NULL; loop++)
			{
				if (!strcmp(token, tokens[loop].token))
				{
					tokens[loop].value = PL_strdup(value);
					break;
				}
			}
			token = strtok(NULL, "&");
		}
	}
	PL_strfree(id);
	return(NS_OK);
}



NS_METHOD
FindDataSource::doMatch(nsIRDFLiteral *literal, char *matchMethod, char *matchText)
{
	PRBool			found = PR_FALSE;

	if ((nsnull == literal) || (nsnull == matchMethod) || (nsnull == matchText))
		return(found);

        nsXPIDLString str;
	literal->GetValue( getter_Copies(str) );
	if (! str)	return(found);
	nsAutoString	value(str);

	// XXX Note: nsString.Find() is currently only case-significant.
	//           We really want a case insignificant Find() for all
	//           the comparisons below.

	if (!PL_strcmp(matchMethod, "contains"))
	{
		if (value.Find(matchText) >= 0)
			found = PR_TRUE;
	}
	else if (!PL_strcmp(matchMethod, "startswith"))
	{
		if (value.Find(matchText) == 0)
			found = PR_TRUE;
	}
	else if (!PL_strcmp(matchMethod, "endswith"))
	{
		PRInt32 pos = value.Find(matchText);
		if ((pos >= 0) && (pos == (value.Length() - strlen(matchText))))
			found = PR_TRUE;
	}
	else if (!PL_strcmp(matchMethod, "is"))
	{
		if (value.EqualsIgnoreCase(matchText))
			found = PR_TRUE;
	}
	else if (!PL_strcmp(matchMethod, "isnot"))
	{
		if (!value.EqualsIgnoreCase(matchText))
			found = PR_TRUE;
	}
	else if (!PL_strcmp(matchMethod, "doesntcontain"))
	{
		if (value.Find(matchText) < 0)
			found = PR_TRUE;
	}
	return(found);
}



NS_METHOD
FindDataSource::parseFindURL(nsIRDFResource *u, nsVoidArray *array)
{
	findTokenStruct		tokens[5];
	nsresult		rv;
	int			loop;

	/* build up a token list */
	tokens[0].token = "datasource";		tokens[0].value = NULL;
	tokens[1].token = "match";		tokens[1].value = NULL;
	tokens[2].token = "method";		tokens[2].value = NULL;
	tokens[3].token = "text";		tokens[3].value = NULL;
	tokens[4].token = NULL;			tokens[4].value = NULL;

	// parse find URI, get parameters, search in appropriate datasource(s), return results
	if (NS_SUCCEEDED(rv = parseResourceIntoFindTokens(u, tokens)))
	{
		nsIRDFDataSource	*datasource;
		if (NS_SUCCEEDED(rv = gRDFService->GetDataSource(tokens[0].value, &datasource)))
		{
			nsIRDFResourceCursor	*cursor = nsnull;
			if (NS_SUCCEEDED(rv = datasource->GetAllResources(&cursor)))
			{
				while (1) 
				{
                                    rv = cursor->Advance();
                                    if (NS_FAILED(rv))
                                        break;

                                    if (rv == NS_RDF_CURSOR_EMPTY)
                                        break;

					nsIRDFNode	*node = nsnull;
					if (NS_SUCCEEDED(rv = cursor->GetValue(&node)))
					{
						nsIRDFResource	*source = nsnull;
						if (NS_SUCCEEDED(rv = node->QueryInterface(kIRDFResourceIID, (void **)&source)))
						{
                                                    nsXPIDLCString uri;
							source->GetValue( getter_Copies(uri) );
							if (PL_strncmp(uri, "find:", PL_strlen("find:")))	// never match against a "find:" URI
							{
								nsIRDFResource	*property = nsnull;
								if (NS_SUCCEEDED(rv = gRDFService->GetResource(tokens[1].value, &property)) &&
									(rv != NS_RDF_NO_VALUE) && (nsnull != property))
								{
									nsIRDFNode	*value = nsnull;
									if (NS_SUCCEEDED(rv = datasource->GetTarget(source, property, PR_TRUE, &value)) &&
										(rv != NS_RDF_NO_VALUE) && (nsnull != value))
									{
										nsIRDFLiteral	*literal = nsnull;
										if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFLiteralIID, (void **)&literal)) &&
											(rv != NS_RDF_NO_VALUE) && (nsnull != literal))
										{
											if (PR_TRUE == doMatch(literal, tokens[2].value, tokens[3].value))
											{
												array->AppendElement(node);
											}
											NS_RELEASE(literal);
										}
										NS_RELEASE(node);
									}
									NS_RELEASE(property);
								}
							}
							NS_RELEASE(source);
						}
					}
				}
				if (rv == NS_RDF_CURSOR_EMPTY)
				{
					rv = NS_OK;
				}
				NS_RELEASE(cursor);
			}
			NS_RELEASE(datasource);
		}
	}
	/* free values in token list */
	for (loop=0; tokens[loop].token != NULL; loop++)
	{
		if (tokens[loop].value != NULL)
		{
			PL_strfree(tokens[loop].value);
			tokens[loop].value = NULL;
		}
	}
	return(rv);
}



NS_METHOD
FindDataSource::getFindResults(nsIRDFResource *source, nsVoidArray **array /* out */)
{
	nsresult	rv;
	nsVoidArray	*nameArray = new nsVoidArray();
	*array = nameArray;
	if (nsnull == nameArray)
	{
		return(NS_ERROR_OUT_OF_MEMORY);
	}
	rv = parseFindURL(source, *array);
	return(rv);
}



NS_METHOD
FindDataSource::getFindName(nsIRDFResource *source, nsVoidArray **array /* out */)
{
	// XXX construct find URI human-readable name
	*array = nsnull;
	return(NS_OK);
}



NS_IMETHODIMP
FindDataSource::GetTargets(nsIRDFResource *source,
                           nsIRDFResource *property,
                           PRBool tv,
                           nsIRDFAssertionCursor **targets /* out */)
{
	nsVoidArray		*array = nsnull;
	nsresult		rv = NS_ERROR_FAILURE;

	// we only have positive assertions in the find data source.
	if (! tv)
		return rv;

	if (isFindURI(source))
	{
		if (peq(property, kNC_Child))
		{
			rv = getFindResults(source, &array);
		}
		else if (peq(property, kNC_Name))
		{
			rv = getFindName(source, &array);
		}
		else if (peq(property, kRDF_type))
		{
			nsXPIDLCString uri;
			kNC_FindObject->GetValue( getter_Copies(uri) );
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
	}
	if ((rv == NS_OK) && (nsnull != array))
	{
		*targets = new FindCursor(source, property, PR_FALSE, array);
		NS_ADDREF(*targets);
	}
	return(rv);
}



NS_IMETHODIMP
FindDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
//	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
//	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::HasAssertion(nsIRDFResource *source,
                             nsIRDFResource *property,
                             nsIRDFNode *target,
                             PRBool tv,
                             PRBool *hasAssertion /* out */)
{
	PRBool			retVal = PR_FALSE;
	nsresult		rv = NS_OK;

	*hasAssertion = PR_FALSE;

	// we only have positive assertions in the find data source.
	if (! tv)
		return rv;

	if (isFindURI(source))
	{
		if (peq(property, kRDF_type))
		{
			if (peq((nsIRDFResource *)target, kRDF_type))
			{
				*hasAssertion = PR_TRUE;
			}
		}
	}
	return (rv);
}



NS_IMETHODIMP
FindDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsIRDFArcsInCursor ** labels /* out */)
{
	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsIRDFArcsOutCursor **labels /* out */)
{
	nsresult		rv = NS_RDF_NO_VALUE;

	*labels = nsnull;

	if (isFindURI(source))
	{
		nsVoidArray *temp = new nsVoidArray();
		if (nsnull == temp)
			return NS_ERROR_OUT_OF_MEMORY;
		temp->AppendElement(kNC_Child);
		temp->AppendElement(kNC_pulse);
		*labels = new FindCursor(source, kNC_Child, PR_TRUE, temp);
		if (nsnull != *labels)
		{
			NS_ADDREF(*labels);
			rv = NS_OK;
		}
	}
	return(rv);

}



NS_IMETHODIMP
FindDataSource::GetAllResources(nsIRDFResourceCursor** aCursor)
{
	NS_NOTYETIMPLEMENTED("sorry!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::AddObserver(nsIRDFObserver *n)
{
	if (nsnull == mObservers)
	{
		if ((mObservers = new nsVoidArray()) == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
	}
	mObservers->AppendElement(n);
	return NS_OK;
}



NS_IMETHODIMP
FindDataSource::RemoveObserver(nsIRDFObserver *n)
{
	if (nsnull == mObservers)
		return NS_OK;
	mObservers->RemoveElement(n);
	return NS_OK;
}



NS_IMETHODIMP
FindDataSource::Flush()
{
	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::GetAllCommands(nsIRDFResource* source,nsIEnumerator/*<nsIRDFResource>*/** commands)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



nsresult
NS_NewRDFFindDataSource(nsIRDFDataSource **result)
{
	if (!result)
		return NS_ERROR_NULL_POINTER;

	// only one find data source
	if (nsnull == gFindDataSource)
	{
		if ((gFindDataSource = new FindDataSource()) == nsnull)
		{
			return NS_ERROR_OUT_OF_MEMORY;
		}
	}
	NS_ADDREF(gFindDataSource);
	*result = gFindDataSource;
	return NS_OK;
}



FindCursor::FindCursor(nsIRDFResource *source,
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



FindCursor::~FindCursor(void)
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
FindCursor::Advance(void)
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
FindCursor::GetValue(nsIRDFNode **aValue)
{
	if (nsnull == mValue)
		return NS_ERROR_NULL_POINTER;
	NS_ADDREF(mValue);
	*aValue = mValue;
	return NS_OK;
}



NS_IMETHODIMP
FindCursor::GetDataSource(nsIRDFDataSource **aDataSource)
{
	NS_ADDREF(gFindDataSource);
	*aDataSource = gFindDataSource;
	return NS_OK;
}



NS_IMETHODIMP
FindCursor::GetSource(nsIRDFResource **aResource)
{
	NS_ADDREF(mSource);
	*aResource = mSource;
	return NS_OK;
}



NS_IMETHODIMP
FindCursor::GetLabel(nsIRDFResource **aPredicate)
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
FindCursor::GetTarget(nsIRDFNode **aObject)
{
	if (nsnull != mTarget)
		NS_ADDREF(mTarget);
	*aObject = mTarget;
	return NS_OK;
}



NS_IMETHODIMP
FindCursor::GetTruthValue(PRBool *aTruthValue)
{
	*aTruthValue = 1;
	return NS_OK;
}



NS_IMPL_ADDREF(FindCursor);
NS_IMPL_RELEASE(FindCursor);



NS_IMETHODIMP
FindCursor::QueryInterface(REFNSIID iid, void **result)
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
