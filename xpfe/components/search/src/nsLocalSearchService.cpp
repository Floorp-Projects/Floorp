/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8; c-file-style: "stroustrup" -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author(s):
 *   Robert John Churchill      <rjc@netscape.com>
 *
 * Contributor(s): 
 *   Pierre Phaneuf             <pp@ludusdesign.com>
 */

/*
  Implementation for a find RDF data store.
 */

#include "nsLocalSearchService.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsEnumeratorUtils.h"
#include "nsXPIDLString.h"
#include "xp_core.h"
#include "plhash.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "prio.h"
#include "nsITextToSubURI.h"
#include "nsIRDFObserver.h"
#include "nsRDFCID.h"
#include "rdf.h"

static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kTextToSubURICID,             NS_TEXTTOSUBURI_CID);

static	nsIRDFService		*gRDFService = nsnull;
static	LocalSearchDataSource		*gLocalSearchDataSource = nsnull;
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


PRInt32              LocalSearchDataSource::gRefCnt;
nsIRDFResource		*LocalSearchDataSource::kNC_Child;
nsIRDFResource		*LocalSearchDataSource::kNC_Name;
nsIRDFResource		*LocalSearchDataSource::kNC_URL;
nsIRDFResource		*LocalSearchDataSource::kNC_FindObject;
nsIRDFResource		*LocalSearchDataSource::kNC_pulse;
nsIRDFResource		*LocalSearchDataSource::kRDF_InstanceOf;
nsIRDFResource		*LocalSearchDataSource::kRDF_type;

LocalSearchDataSource::LocalSearchDataSource(void)
{
	NS_INIT_REFCNT();

	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
		                           NS_GET_IID(nsIRDFService),
		                           (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(NC_NAMESPACE_URI "child",       &kNC_Child);
		gRDFService->GetResource(NC_NAMESPACE_URI "Name",        &kNC_Name);
		gRDFService->GetResource(NC_NAMESPACE_URI "URL",         &kNC_URL);
		gRDFService->GetResource(NC_NAMESPACE_URI "FindObject",  &kNC_FindObject);
		gRDFService->GetResource(NC_NAMESPACE_URI "pulse",       &kNC_pulse);

		gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_InstanceOf);
		gRDFService->GetResource(RDF_NAMESPACE_URI "type",       &kRDF_type);

		gLocalSearchDataSource = this;
	}
}



LocalSearchDataSource::~LocalSearchDataSource (void)
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

		gLocalSearchDataSource = nsnull;
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
		gRDFService = nsnull;
	}
}



nsresult
LocalSearchDataSource::Init()
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this, PR_FALSE)))
		return(rv);

	return(rv);
}



NS_IMPL_ISUPPORTS(LocalSearchDataSource, NS_GET_IID(nsIRDFDataSource));



NS_IMETHODIMP
LocalSearchDataSource::GetURI(char **uri)
{
	NS_PRECONDITION(uri != nsnull, "null ptr");
	if (! uri)
		return NS_ERROR_NULL_POINTER;

	if ((*uri = nsXPIDLCString::Copy("rdf:localsearch")) == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}



NS_IMETHODIMP
LocalSearchDataSource::GetSource(nsIRDFResource* property,
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
LocalSearchDataSource::GetSources(nsIRDFResource *property,
                           nsIRDFNode *target,
			   PRBool tv,
                           nsISimpleEnumerator **sources /* out */)
{
	NS_NOTYETIMPLEMENTED("write me");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
LocalSearchDataSource::GetTarget(nsIRDFResource *source,
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
			nsAutoString	url;
			nsIRDFLiteral	*literal;
			gRDFService->GetLiteral(url.GetUnicode(), &literal);
			*target = literal;
			return(NS_OK);
		}
		else if (property == kRDF_type)
		{
			const char	*uri = nsnull;
			rv = kNC_FindObject->GetValueConst(&uri);
			if (NS_FAILED(rv)) return rv;

			nsAutoString	url; url.AssignWithConversion(uri);
			nsIRDFLiteral	*literal;
			gRDFService->GetLiteral(url.GetUnicode(), &literal);

			*target = literal;
			return(NS_OK);
		}
		else if (property == kNC_pulse)
		{
			nsAutoString	pulse; pulse.AssignWithConversion("15");
			nsIRDFLiteral	*pulseLiteral;
			rv = gRDFService->GetLiteral(pulse.GetUnicode(), &pulseLiteral);
			if (NS_FAILED(rv)) return rv;

			*target = pulseLiteral;
			return(NS_OK);
		}
		else if (property == kNC_Child)
		{
			// fake out the generic builder (i.e. return anything in this case)
			// so that search containers never appear to be empty
			*target = source;
			NS_ADDREF(source);
			return(NS_OK);
		}
	}
	return NS_RDF_NO_VALUE;
}



NS_METHOD
LocalSearchDataSource::parseResourceIntoFindTokens(nsIRDFResource *u, findTokenPtr tokens)
{
	const char		*uri = nsnull;
	char			*id, *token, *value;
	int			loop;
	nsresult		rv;

	if (NS_FAILED(rv = u->GetValueConst(&uri)))	return(rv);

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
				    if (!strcmp(token, "text"))
				    {
            			NS_WITH_SERVICE(nsITextToSubURI, textToSubURI,
            				kTextToSubURICID, &rv);
            			if (NS_SUCCEEDED(rv) && (textToSubURI))
            			{
            				PRUnichar	*uni = nsnull;
            				if (NS_SUCCEEDED(rv = textToSubURI->UnEscapeAndConvert("UTF-8", value, &uni)) && (uni))
            				{
    					        tokens[loop].value = uni;
    					        Recycle(uni);
    					    }
    					}
				    }
				    else
				    {
				        nsAutoString    valueStr;
				        valueStr.AssignWithConversion(value);
				        tokens[loop].value = valueStr;
    			    }
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
LocalSearchDataSource::doMatch(nsIRDFLiteral *literal, const nsString &matchMethod, const nsString &matchText)
{
	PRBool		found = PR_FALSE;

	if ((nsnull == literal) || (nsnull == matchMethod) || (nsnull == matchText))
		return(found);

	const	PRUnichar	*str = nsnull;
	literal->GetValueConst( &str );
	if (! str)	return(found);
	nsAutoString	value(str);

    if (matchMethod.EqualsIgnoreCase("contains"))
	{
		if (value.Find(matchText, PR_TRUE) >= 0)
			found = PR_TRUE;
	}
    else if (matchMethod.EqualsIgnoreCase("startswith"))
	{
		if (value.Find(matchText, PR_TRUE) == 0)
			found = PR_TRUE;
	}
    else if (matchMethod.EqualsIgnoreCase("endswith"))
	{
		PRInt32 pos = value.RFind(matchText, PR_TRUE);
		if ((pos >= 0) && (pos == (PRInt32(value.Length()) - PRInt32(matchText.Length()))))
			found = PR_TRUE;
	}
    else if (matchMethod.EqualsIgnoreCase("is"))
	{
		if (value.EqualsIgnoreCase(matchText))
			found = PR_TRUE;
	}
    else if (matchMethod.EqualsIgnoreCase("isnot"))
	{
		if (!value.EqualsIgnoreCase(matchText))
			found = PR_TRUE;
	}
    else if (matchMethod.EqualsIgnoreCase("doesntcontain"))
	{
		if (value.Find(matchText, PR_TRUE) < 0)
			found = PR_TRUE;
	}
	return(found);
}



NS_METHOD
LocalSearchDataSource::parseFindURL(nsIRDFResource *u, nsISupportsArray *array)
{
	findTokenStruct		tokens[5];
	nsresult		rv;

	/* build up a token list */
	tokens[0].token = "datasource";
	tokens[1].token = "match";
	tokens[2].token = "method";
	tokens[3].token = "text";
	tokens[4].token = NULL;

	// parse find URI, get parameters, search in appropriate datasource(s), return results
	if (NS_SUCCEEDED(rv = parseResourceIntoFindTokens(u, tokens)))
	{
		nsCAutoString       dsName;
		dsName.AssignWithConversion(tokens[0].value);

		nsCOMPtr<nsIRDFDataSource>  datasource;
		if (NS_SUCCEEDED(rv = gRDFService->GetDataSource(dsName, getter_AddRefs(datasource))))
		{
			nsCOMPtr<nsISimpleEnumerator>   cursor;
			if (NS_SUCCEEDED(rv = datasource->GetAllResources(getter_AddRefs(cursor))))
			{
				while (PR_TRUE) 
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
						nsCOMPtr<nsIRDFResource>    source = do_QueryInterface(isupports);
						if (source)
						{
							const char	*uri = nsnull;
							source->GetValueConst(&uri);

							// never match against a "find:" URI
							if ((uri) && (PL_strncmp(uri, kFindProtocol, sizeof(kFindProtocol) - 1)))
							{
								nsCOMPtr<nsIRDFResource>    property;
								if (NS_SUCCEEDED(rv = gRDFService->GetUnicodeResource(tokens[1].value.GetUnicode(),
								    getter_AddRefs(property))) && (rv != NS_RDF_NO_VALUE) && (nsnull != property))
								{
									nsCOMPtr<nsIRDFNode>    value;
									if (NS_SUCCEEDED(rv = datasource->GetTarget(source, property, PR_TRUE, getter_AddRefs(value))) &&
										(rv != NS_RDF_NO_VALUE) && (nsnull != value))
									{
										nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(value);
										if (literal)
										{
											if (PR_TRUE == doMatch(literal, tokens[2].value, tokens[3].value))
											{
												array->AppendElement(source);
											}
										}
									}
								}
							}
						}
					}
				}
				if (rv == NS_RDF_CURSOR_EMPTY)
				{
					rv = NS_OK;
				}
			}
		}
	}
	return(rv);
}



NS_METHOD
LocalSearchDataSource::getFindResults(nsIRDFResource *source, nsISimpleEnumerator** aResult)
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
LocalSearchDataSource::getFindName(nsIRDFResource *source, nsIRDFLiteral** aResult)
{
	// XXX construct find URI human-readable name
	*aResult = nsnull;
	return(NS_OK);
}



NS_IMETHODIMP
LocalSearchDataSource::GetTargets(nsIRDFResource *source,
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
			const	char	*uri = nsnull;
			rv = kNC_FindObject->GetValueConst( &uri );
			if (NS_FAILED(rv)) return rv;

			nsAutoString	url; url.AssignWithConversion(uri);
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
			nsAutoString	pulse; pulse.AssignWithConversion("15");
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
LocalSearchDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
LocalSearchDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
LocalSearchDataSource::Change(nsIRDFResource* aSource,
                       nsIRDFResource* aProperty,
                       nsIRDFNode* aOldTarget,
                       nsIRDFNode* aNewTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
LocalSearchDataSource::Move(nsIRDFResource* aOldSource,
                     nsIRDFResource* aNewSource,
                     nsIRDFResource* aProperty,
                     nsIRDFNode* aTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
LocalSearchDataSource::HasAssertion(nsIRDFResource *source,
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
LocalSearchDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP 
LocalSearchDataSource::HasArcOut(nsIRDFResource *source, nsIRDFResource *aArc, PRBool *result)
{
    NS_PRECONDITION(source != nsnull, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    if ((aArc == kNC_Child ||
         aArc == kNC_pulse)) {
        *result = isFindURI(source);
    }
    else {
        *result = PR_FALSE;
    }
    return NS_OK;
}

NS_IMETHODIMP
LocalSearchDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
	NS_NOTYETIMPLEMENTED("write me");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
LocalSearchDataSource::ArcLabelsOut(nsIRDFResource *source,
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
LocalSearchDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	NS_NOTYETIMPLEMENTED("sorry!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
LocalSearchDataSource::AddObserver(nsIRDFObserver *n)
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
LocalSearchDataSource::RemoveObserver(nsIRDFObserver *n)
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
LocalSearchDataSource::GetAllCommands(nsIRDFResource* source,nsIEnumerator/*<nsIRDFResource>*/** commands)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
LocalSearchDataSource::GetAllCmds(nsIRDFResource* source, nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	return(NS_NewEmptyEnumerator(commands));
}



NS_IMETHODIMP
LocalSearchDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
LocalSearchDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}
