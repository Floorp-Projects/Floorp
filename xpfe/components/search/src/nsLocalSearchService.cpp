/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8; c-file-style: "stroustrup" -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert John Churchill      <rjc@netscape.com>
 *   Pierre Phaneuf             <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
  Implementation for a find RDF data store.
 */

#include "nsLocalSearchService.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIRDFContainerUtils.h"
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



NS_IMPL_ISUPPORTS1(LocalSearchDataSource, nsIRDFDataSource)



NS_IMETHODIMP
LocalSearchDataSource::GetURI(char **uri)
{
	NS_PRECONDITION(uri != nsnull, "null ptr");
	if (! uri)
		return NS_ERROR_NULL_POINTER;

	if ((*uri = nsCRT::strdup("rdf:localsearch")) == nsnull)
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
			gRDFService->GetLiteral(url.get(), &literal);
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
			gRDFService->GetLiteral(url.get(), &literal);

			*target = literal;
			return(NS_OK);
		}
		else if (property == kNC_pulse)
		{
			nsAutoString	pulse; pulse.AssignWithConversion("15");
			nsIRDFLiteral	*pulseLiteral;
			rv = gRDFService->GetLiteral(pulse.get(), &pulseLiteral);
			if (NS_FAILED(rv)) return rv;

			*target = pulseLiteral;
			return(NS_OK);
		}
		else if (property == kNC_Child)
		{
			// fake out the generic builder (i.e. return anything in this case)
			// so that search containers never appear to be empty
			*target = source;
			NS_ADDREF(*target);
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
            			nsCOMPtr<nsITextToSubURI> textToSubURI = 
            			         do_GetService(kTextToSubURICID, &rv);
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



PRBool
LocalSearchDataSource::doMatch(nsIRDFLiteral *literal,
                               const nsAReadableString &matchMethod,
                               const nsString &matchText)
{
	PRBool		found = PR_FALSE;

	if ((nsnull == literal) ||
            matchMethod.IsEmpty() ||
            matchText.IsEmpty())
		return(found);

	const	PRUnichar	*str = nsnull;
	literal->GetValueConst( &str );
	if (! str)	return(found);
	nsAutoString	value(str);
        
        if (matchMethod.Equals(NS_LITERAL_STRING("contains")))
	{
            if (value.Find(matchText, PR_TRUE) >= 0)
                found = PR_TRUE;
	}
        else if (matchMethod.Equals(NS_LITERAL_STRING("startswith")))
	{
            if (value.Find(matchText, PR_TRUE) == 0)
                found = PR_TRUE;
	}
        else if (matchMethod.Equals(NS_LITERAL_STRING("endswith")))
	{
            PRInt32 pos = value.RFind(matchText, PR_TRUE);
            if ((pos >= 0) &&
                (pos == (PRInt32(value.Length()) -
                         PRInt32(matchText.Length()))))
                found = PR_TRUE;
	}
        else if (matchMethod.Equals(NS_LITERAL_STRING("is")))
	{
            if (value.EqualsIgnoreCase(matchText))
                found = PR_TRUE;
	}
        else if (matchMethod.Equals(NS_LITERAL_STRING("isnot")))
	{
            if (!value.EqualsIgnoreCase(matchText))
                found = PR_TRUE;
	}
        else if (matchMethod.Equals(NS_LITERAL_STRING("doesntcontain")))
	{
            if (value.Find(matchText, PR_TRUE) < 0)
                found = PR_TRUE;
	}
        return(found);
}

PRBool
LocalSearchDataSource::doDateMatch(nsIRDFDate *aDate,
                                   const nsAReadableString& matchMethod,
                                   const nsAReadableString& matchText)
{
    PRBool found = PR_FALSE;
    
    if (matchMethod.Equals(NS_LITERAL_STRING("isbefore")) ||
        matchMethod.Equals(NS_LITERAL_STRING("isafter")))
    {
        PRInt64 matchDate;
        nsresult rv = parseDate(matchText, &matchDate);
        if (NS_SUCCEEDED(rv))
            found = dateMatches(aDate, matchMethod, matchDate);
    }

    return found;
}

PRBool
LocalSearchDataSource::doIntMatch(nsIRDFInt *aInt,
                                  const nsAReadableString& matchMethod,
                                  const nsString& matchText)
{
    nsresult rv;
    PRBool found = PR_FALSE;
    
    PRInt32 val;
    rv = aInt->GetValue(&val);
    if (NS_FAILED(rv)) return PR_FALSE;
    
    PRInt32 error=0;
    PRInt32 matchVal = matchText.ToInteger(&error);
    if (error != 0) return PR_FALSE;
    
    if (matchMethod.Equals(NS_LITERAL_STRING("is")))
        found = (val == matchVal);
    else if (matchMethod.Equals(NS_LITERAL_STRING("isgreater")))
        found = (val > matchVal);
    else if (matchMethod.Equals(NS_LITERAL_STRING("isless")))
        found = (val < matchVal);

    return found;
}

NS_METHOD
LocalSearchDataSource::parseDate(const nsAReadableString& aDate,
                                 PRInt64 *aResult)
{
    // date is in the form of msec since epoch, but use NSPR to
    // parse the time
    PRTime *outTime = NS_STATIC_CAST(PRTime*,aResult);
    PRStatus err;
    err = PR_ParseTimeString(NS_ConvertUCS2toUTF8(aDate).get(),
                             PR_FALSE, // PR_FALSE == use current timezone
                             outTime);
    NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);
    
    return NS_OK;
}


PRBool
LocalSearchDataSource::dateMatches(nsIRDFDate *aDate,
                                   const nsAReadableString& method,
                                   const PRInt64& matchDate)
{
    PRInt64 date;
    aDate->GetValue(&date);
    PRBool matches = PR_FALSE;
    
    if (method.Equals(NS_LITERAL_STRING("isbefore")))
        matches = LL_CMP(date, <, matchDate);
    
    else if (method.Equals(NS_LITERAL_STRING("isafter")))
        matches = LL_CMP(date, >, matchDate);

    else if (method.Equals(NS_LITERAL_STRING("is")))
        matches = LL_EQ(date, matchDate);

    return matches;
}


NS_METHOD
LocalSearchDataSource::parseFindURL(nsIRDFResource *u, nsISupportsArray *array)
{
  findTokenStruct		tokens[5];
  nsresult rv;

  // build up a token list
  tokens[0].token = "datasource";
  tokens[1].token = "match";
  tokens[2].token = "method";
  tokens[3].token = "text";
  tokens[4].token = NULL;

  // parse find URI, get parameters, search in appropriate
  // datasource(s), return results
  rv = parseResourceIntoFindTokens(u, tokens);
  if (NS_FAILED(rv)) 
    return rv;

  nsCAutoString dsName;
  dsName.AssignWithConversion(tokens[0].value);

  nsCOMPtr<nsIRDFDataSource> datasource;
  rv = gRDFService->GetDataSource(dsName, getter_AddRefs(datasource));
  if (NS_FAILED(rv)) 
    return rv;

  nsCOMPtr<nsISimpleEnumerator> cursor;
  rv = datasource->GetAllResources(getter_AddRefs(cursor));
  if (NS_FAILED(rv)) 
    return rv;
        
  while (PR_TRUE) {
    PRBool hasMore;
    rv = cursor->HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) 
      break;

    if (!hasMore) 
      break;

    nsCOMPtr<nsISupports> isupports;
    rv = cursor->GetNext(getter_AddRefs(isupports));
    if (NS_FAILED(rv)) 
      continue;

    nsCOMPtr<nsIRDFResource> source(do_QueryInterface(isupports));
    if (!source) 
      continue;

    const char	*uri = nsnull;
    source->GetValueConst(&uri);

    if (!uri) 
      continue;
            
    // never match against a "find:" URI
    if (PL_strncmp(uri, kFindProtocol, sizeof(kFindProtocol)-1) == 0)
      continue;

    // never match against a container. Searching for folders just isn't
    // much of a utility, and this fixes an infinite recursion crash. (65063)
    PRBool isContainer = PR_FALSE;

    // Check to see if this source is an RDF container
    nsCOMPtr<nsIRDFContainerUtils> cUtils(do_GetService("@mozilla.org/rdf/container-utils;1"));
    if (cUtils)
      cUtils->IsContainer(datasource, source, &isContainer);
    // Check to see if this source is a pseudo-container
    if (!isContainer)
      datasource->HasArcOut(source, kNC_Child, &isContainer);

    if (isContainer) 
      continue;

    nsCOMPtr<nsIRDFResource> property;
    rv = gRDFService->GetUnicodeResource(tokens[1].value.get(),
    getter_AddRefs(property));

    if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE) || !property)
      continue;

    nsCOMPtr<nsIRDFNode>    value;
    rv = datasource->GetTarget(source, property,
    PR_TRUE, getter_AddRefs(value));
    if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE) || !value)
      continue;

    PRBool found = PR_FALSE;
    found = matchNode(value, tokens[2].value, tokens[3].value);

    if (found)
      array->AppendElement(source);
   }

  if (rv == NS_RDF_CURSOR_EMPTY)
    rv = NS_OK;

  return rv;
}

// could speed up date/integer matching signifigantly by caching the
// last successful match data type (i.e. string, date, int) and trying
// to QI against that first
PRBool
LocalSearchDataSource::matchNode(nsIRDFNode *aValue,
                                 const nsAReadableString& matchMethod,
                                 const nsString& matchText)
{
    nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(aValue));
    if (literal)
        return doMatch(literal, matchMethod, matchText);

    nsCOMPtr<nsIRDFDate> dateLiteral(do_QueryInterface(aValue));
    if (dateLiteral)
        return doDateMatch(dateLiteral, matchMethod, matchText);
    
    nsCOMPtr<nsIRDFInt> intLiteral(do_QueryInterface(aValue));
    if (intLiteral)
        return doIntMatch(intLiteral, matchMethod, matchText);

    return PR_FALSE;
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
			rv = gRDFService->GetLiteral(url.get(), &literal);
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
			rv = gRDFService->GetLiteral(pulse.get(), &pulseLiteral);
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
