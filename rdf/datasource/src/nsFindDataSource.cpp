/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8; c-file-style: "stroustrup" -*-
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
#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
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
#include "nsIRDFFind.h"

static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);


typedef	struct	_findTokenStruct
{
	char			*token;
	char			*value;
} findTokenStruct, *findTokenPtr;



class FindDataSource : public nsIRDFFindDataSource
{
private:
	nsCOMPtr<nsISupportsArray> mObservers;

	static PRInt32		gRefCnt;

    // pseudo-constants
	static nsIRDFResource	*kNC_Child;
	static nsIRDFResource	*kNC_Name;
	static nsIRDFResource	*kNC_URL;
	static nsIRDFResource	*kNC_FindObject;
	static nsIRDFResource	*kNC_pulse;
	static nsIRDFResource	*kRDF_InstanceOf;
	static nsIRDFResource	*kRDF_type;

	NS_METHOD	getFindResults(nsIRDFResource *source, nsISimpleEnumerator** aResult);

	NS_METHOD	getFindName(nsIRDFResource *source, nsIRDFLiteral** aResult);

	NS_METHOD	parseResourceIntoFindTokens(nsIRDFResource *u,
				findTokenPtr tokens);
	NS_METHOD	doMatch(nsIRDFLiteral *literal, char *matchMethod,
				char *matchText);
	NS_METHOD	parseFindURL(nsIRDFResource *u,
				nsISupportsArray *array);

public:

	NS_DECL_ISUPPORTS

			FindDataSource(void);
	virtual		~FindDataSource(void);

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

static const char	kFindProtocol[] = "find:";



static PRBool
isFindURI(nsIRDFResource *r)
{
	PRBool		isFindURIFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kFindProtocol, sizeof(kFindProtocol) - 1)))
	{
		isFindURIFlag = PR_TRUE;
	}
	return(isFindURIFlag);
}



FindDataSource::FindDataSource(void)
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
		gRDFService->GetResource(NC_NAMESPACE_URI "FindObject",  &kNC_FindObject);
		gRDFService->GetResource(NC_NAMESPACE_URI "pulse",       &kNC_pulse);

		gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_InstanceOf);
		gRDFService->GetResource(RDF_NAMESPACE_URI "type",       &kRDF_type);

		gFindDataSource = this;
	}
}



FindDataSource::~FindDataSource (void)
{
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



NS_IMPL_ISUPPORTS(FindDataSource, nsIRDFDataSource::GetIID());



NS_IMETHODIMP
FindDataSource::GetURI(char **uri)
{
	NS_PRECONDITION(uri != nsnull, "null ptr");
	if (! uri)
		return NS_ERROR_NULL_POINTER;

	if ((*uri = nsXPIDLCString::Copy("rdf:find")) == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}



NS_IMETHODIMP
FindDataSource::GetSource(nsIRDFResource* property,
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
FindDataSource::GetSources(nsIRDFResource *property,
                           nsIRDFNode *target,
			   PRBool tv,
                           nsISimpleEnumerator **sources /* out */)
{
	NS_NOTYETIMPLEMENTED("write me");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::GetTarget(nsIRDFResource *source,
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

	// we only have positive assertions in the find data source.
	if (! tv)
		return rv;

	if (isFindURI(source))
	{
		if (property == kNC_Name)
		{
//			rv = GetName(source, &array);
		}
		else if (property == kNC_URL)
		{
			// note: lie and say there is no URL
//			rv = GetURL(source, &array);
			nsAutoString	url("");
			nsIRDFLiteral	*literal;
			gRDFService->GetLiteral(url.GetUnicode(), &literal);
			*target = literal;
			rv = NS_OK;
		}
		else if (property == kRDF_type)
		{
			nsXPIDLCString uri;
			rv = kNC_FindObject->GetValue( getter_Copies(uri) );
			if (NS_FAILED(rv)) return rv;

			nsAutoString	url(uri);
			nsIRDFLiteral	*literal;
			gRDFService->GetLiteral(url.GetUnicode(), &literal);

			*target = literal;
			return NS_OK;
		}
		else if (property == kNC_pulse)
		{
			nsAutoString	pulse("15");
			nsIRDFLiteral	*pulseLiteral;
			rv = gRDFService->GetLiteral(pulse.GetUnicode(), &pulseLiteral);
			if (NS_FAILED(rv)) return rv;

			*target = pulseLiteral;
			return NS_OK;
		}
	}
	return NS_RDF_NO_VALUE;
}



NS_METHOD
FindDataSource::parseResourceIntoFindTokens(nsIRDFResource *u, findTokenPtr tokens)
{
	nsXPIDLCString		uri;
	char			*id, *token, *value;
	int			loop;
	nsresult		rv;

	if (NS_FAILED(rv = u->GetValue( getter_Copies(uri) )))	return(rv);

#ifdef	DEBUG
	printf("Find: %s\n", (const char*) uri);
#endif

	if (!(id = PL_strdup(uri + sizeof(kFindProtocol) - 1)))
		return(NS_ERROR_OUT_OF_MEMORY);

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
	PRBool		found = PR_FALSE;

	if ((nsnull == literal) || (nsnull == matchMethod) || (nsnull == matchText))
		return(found);

        nsXPIDLString	str;
	literal->GetValue( getter_Copies(str) );
	if (! str)	return(found);
	nsAutoString	value(str);

	if (!PL_strcmp(matchMethod, "contains"))
	{
		if (value.Find(matchText, PR_TRUE) >= 0)
			found = PR_TRUE;
	}
	else if (!PL_strcmp(matchMethod, "startswith"))
	{
		if (value.Find(matchText, PR_TRUE) == 0)
			found = PR_TRUE;
	}
	else if (!PL_strcmp(matchMethod, "endswith"))
	{
		PRInt32 pos = value.RFind(matchText, PR_TRUE);
		if ((pos >= 0) && (pos == (value.Length() - PRInt32(strlen(matchText)))))
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
		if (value.Find(matchText, PR_TRUE) < 0)
			found = PR_TRUE;
	}
	return(found);
}



NS_METHOD
FindDataSource::parseFindURL(nsIRDFResource *u, nsISupportsArray *array)
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
			nsISimpleEnumerator	*cursor = nsnull;
			if (NS_SUCCEEDED(rv = datasource->GetAllResources(&cursor)))
			{
				while (1) 
				{
					PRBool hasMore;
					rv = cursor->HasMoreElements(&hasMore);
					if (NS_FAILED(rv))
						break;

					if (! hasMore)
						break;

					nsCOMPtr<nsISupports> isupports;
					rv = cursor->GetNext(getter_AddRefs(isupports));
					if (NS_SUCCEEDED(rv))
					{
						nsIRDFResource	*source = nsnull;
						if (NS_SUCCEEDED(rv = isupports->QueryInterface(nsIRDFResource::GetIID(), (void **)&source)))
						{
							nsXPIDLCString	uri;
							source->GetValue( getter_Copies(uri) );

							// never match against a "find:" URI
							if (PL_strncmp(uri, kFindProtocol, sizeof(kFindProtocol) - 1))
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
										if (NS_SUCCEEDED(rv = value->QueryInterface(nsIRDFLiteral::GetIID(), (void **)&literal)) &&
											(rv != NS_RDF_NO_VALUE) && (nsnull != literal))
										{
											if (PR_TRUE == doMatch(literal, tokens[2].value, tokens[3].value))
											{
												array->AppendElement(source);
											}
											NS_RELEASE(literal);
										}
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
FindDataSource::getFindResults(nsIRDFResource *source, nsISimpleEnumerator** aResult)
{
	nsresult			rv;
	nsCOMPtr<nsISupportsArray>	nameArray;
	rv = NS_NewISupportsArray( getter_AddRefs(nameArray) );
	if (NS_FAILED(rv)) return rv;

	rv = parseFindURL(source, nameArray);
	if (NS_FAILED(rv)) return rv;

	nsISimpleEnumerator* result = new nsArrayEnumerator(nameArray);
	if (! result)
		return(NS_ERROR_OUT_OF_MEMORY);

	NS_ADDREF(result);
	*aResult = result;

	return NS_OK;
}



NS_METHOD
FindDataSource::getFindName(nsIRDFResource *source, nsIRDFLiteral** aResult)
{
	// XXX construct find URI human-readable name
	*aResult = nsnull;
	return(NS_OK);
}



NS_IMETHODIMP
FindDataSource::GetTargets(nsIRDFResource *source,
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

	nsresult		rv = NS_ERROR_FAILURE;

	// we only have positive assertions in the find data source.
	if (! tv)
		return rv;

	if (isFindURI(source))
	{
		if (property == kNC_Child)
		{
			return getFindResults(source, targets);
		}
		else if (property == kNC_Name)
		{
			nsCOMPtr<nsIRDFLiteral>	name;
			rv = getFindName(source, getter_AddRefs(name));
			if (NS_FAILED(rv)) return rv;

			nsISimpleEnumerator* result =
			new nsSingletonEnumerator(name);

			if (! result)
				return NS_ERROR_OUT_OF_MEMORY;

			NS_ADDREF(result);
			*targets = result;
			return NS_OK;
		}
		else if (property == kRDF_type)
		{
			nsXPIDLCString	uri;
			rv = kNC_FindObject->GetValue( getter_Copies(uri) );
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
	}

	return NS_NewEmptyEnumerator(targets);
}



NS_IMETHODIMP
FindDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FindDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FindDataSource::Change(nsIRDFResource* aSource,
                       nsIRDFResource* aProperty,
                       nsIRDFNode* aOldTarget,
                       nsIRDFNode* aNewTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FindDataSource::Move(nsIRDFResource* aOldSource,
                     nsIRDFResource* aNewSource,
                     nsIRDFResource* aProperty,
                     nsIRDFNode* aTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FindDataSource::HasAssertion(nsIRDFResource *source,
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

	nsresult		rv = NS_OK;

	*hasAssertion = PR_FALSE;

	// we only have positive assertions in the find data source.
	if (! tv)
		return rv;

	if (isFindURI(source))
	{
		if (property == kRDF_type)
		{
			if ((nsIRDFResource *)target == kRDF_type)
			{
				*hasAssertion = PR_TRUE;
			}
		}
	}
	return (rv);
}



NS_IMETHODIMP
FindDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
	NS_NOTYETIMPLEMENTED("write me");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsISimpleEnumerator **labels /* out */)
{
	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(labels != nsnull, "null ptr");
	if (! labels)
		return NS_ERROR_NULL_POINTER;

	nsresult		rv;

	if (isFindURI(source))
	{
		nsCOMPtr<nsISupportsArray> array;
		rv = NS_NewISupportsArray( getter_AddRefs(array) );
		if (NS_FAILED(rv)) return rv;

		array->AppendElement(kNC_Child);
		array->AppendElement(kNC_pulse);

		nsISimpleEnumerator* result = new nsArrayEnumerator(array);
		if (! result)
			return NS_ERROR_OUT_OF_MEMORY;

		NS_ADDREF(result);
		*labels = result;
		return(NS_OK);
	}
	return(NS_NewEmptyEnumerator(labels));
}



NS_IMETHODIMP
FindDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	NS_NOTYETIMPLEMENTED("sorry!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::AddObserver(nsIRDFObserver *n)
{
	NS_PRECONDITION(n != nsnull, "null ptr");
	if (! n)
		return NS_ERROR_NULL_POINTER;

	if (! mObservers)
	{
		nsresult	rv;
		rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
		if (NS_FAILED(rv)) return rv;
	}
	return mObservers->AppendElement(n) ? NS_OK : NS_ERROR_FAILURE;
}



NS_IMETHODIMP
FindDataSource::RemoveObserver(nsIRDFObserver *n)
{
	NS_PRECONDITION(n != nsnull, "null ptr");
	if (! n)
		return NS_ERROR_NULL_POINTER;

	if (! mObservers)
		return(NS_OK);

	NS_VERIFY(mObservers->RemoveElement(n), "observer not present");
	return(NS_OK);
}



NS_IMETHODIMP
FindDataSource::GetAllCommands(nsIRDFResource* source,nsIEnumerator/*<nsIRDFResource>*/** commands)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FindDataSource::GetAllCmds(nsIRDFResource* source, nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	return(NS_NewEmptyEnumerator(commands));
}



NS_IMETHODIMP
FindDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
FindDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
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
