/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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


#include "nscore.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"

#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFDataSource.h"
#include "rdf.h"

#include "nsIServiceManager.h"

#include "nsEnumeratorUtils.h"

#include "nsXPIDLString.h"

#ifdef NS_DEBUG
#include "stdio.h"
#endif

#define NC_RDF_Name  NC_NAMESPACE_URI "Name"
#define NC_RDF_Value NC_NAMESPACE_URI "Value"
#define NC_RDF_Child NC_NAMESPACE_URI "child"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

class nsRDFDataSourceDataSource : public nsIRDFDataSource {
public: 
  NS_DECL_ISUPPORTS

  nsRDFDataSourceDataSource();
  virtual ~nsRDFDataSourceDataSource();
  
  /* void Init (in string uri); */
  NS_IMETHOD Init(const char *uri);

  /* readonly attribute string URI; */
  NS_IMETHOD GetURI(char * *aURI);

  /* nsIRDFResource GetSource (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsIRDFResource **_retval);

  /* nsISimpleEnumerator GetSources (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval);

  /* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
  NS_IMETHOD GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **_retval);

  /* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
  NS_IMETHOD GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **_retval);

  /* void Assert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue);

  /* void Unassert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
  NS_IMETHOD Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget);

  /* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval);

  /* void AddObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD AddObserver(nsIRDFObserver *aObserver);

  /* void RemoveObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD RemoveObserver(nsIRDFObserver *aObserver);

  /* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
  NS_IMETHOD ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval);

  /* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval);

  /* nsISimpleEnumerator GetAllResources (); */
  NS_IMETHOD GetAllResources(nsISimpleEnumerator **_retval);

  /* void Flush (); */
  NS_IMETHOD Flush();

  /* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
  NS_IMETHOD GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval);

  /* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
  NS_IMETHOD IsCommandEnabled(nsISupportsArray * aSources, nsIRDFResource *aCommand, nsISupportsArray * aArguments, PRBool *_retval);

  /* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
  NS_IMETHOD DoCommand(nsISupportsArray * aSources, nsIRDFResource *aCommand, nsISupportsArray * aArguments);

private:
  char *mURI;
  nsIRDFDataSource* mDataSource;

  static nsIRDFResource* kNC_Name;
  static nsIRDFResource* kNC_Value;
  static nsIRDFResource* kNC_Child;
  
};

nsIRDFResource* nsRDFDataSourceDataSource::kNC_Name=nsnull;
nsIRDFResource* nsRDFDataSourceDataSource::kNC_Value=nsnull;
nsIRDFResource* nsRDFDataSourceDataSource::kNC_Child=nsnull;


nsRDFDataSourceDataSource::nsRDFDataSourceDataSource():
  mURI(nsnull),
  mDataSource(nsnull)
{
  NS_INIT_REFCNT();

}

nsRDFDataSourceDataSource::~nsRDFDataSourceDataSource()
{
  nsCRT::free(mURI);
}


NS_IMPL_ISUPPORTS1(nsRDFDataSourceDataSource, nsIRDFDataSource)

/* void Init (in string uri); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::Init(const char *uri)
{
  nsresult rv;

  if (mURI) nsCRT::free(mURI);
  mURI = nsCRT::strdup(uri);

  // get RDF resources
  
  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  if (!kNC_Name) {
    rdf->GetResource(NC_RDF_Name, &kNC_Name);
    rdf->GetResource(NC_RDF_Child, &kNC_Child);
    rdf->GetResource(NC_RDF_Value, &kNC_Value);
  }

#ifdef DEBUG_alecf
  printf("nsRDFDataSourceDataSource::Init(%s)\n", uri);
#endif
  
  return NS_OK;
}

/* readonly attribute string URI; */
NS_IMETHODIMP
nsRDFDataSourceDataSource::GetURI(char * *aURI)
{
#ifdef DEBUG_alecf
  printf("nsRDFDataSourceDataSource::GetURI()\n");
#endif
  *aURI = nsCRT::strdup(mURI);
  
  return NS_OK;
}

/* nsIRDFResource GetSource (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::GetSource(nsIRDFResource *aProperty,
                                     nsIRDFNode *aTarget,
                                     PRBool aTruthValue,
                                     nsIRDFResource **_retval)
{
  return NS_RDF_NO_VALUE;
}

/* nsISimpleEnumerator GetSources (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::GetSources(nsIRDFResource *aProperty,
                                      nsIRDFNode *aTarget,
                                      PRBool aTruthValue,
                                      nsISimpleEnumerator **_retval)
{
  return NS_RDF_NO_VALUE;
}

/* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::GetTarget(nsIRDFResource *aSource,
                                     nsIRDFResource *aProperty,
                                     PRBool aTruthValue,
                                     nsIRDFNode **_retval)
{
#ifdef DEBUG_alecf
  nsXPIDLCString sourceval;
  nsXPIDLCString propval;
  aSource->GetValue(getter_Copies(sourceval));
  aProperty->GetValue(getter_Copies(propval));
  printf("GetTarget(%s, %s,..)\n", (const char*)sourceval,
         (const char*)propval);
#endif

  return NS_OK;
}

/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::GetTargets(nsIRDFResource *aSource,
                                      nsIRDFResource *aProperty,
                                      PRBool aTruthValue,
                                      nsISimpleEnumerator **_retval)
{
#ifdef DEBUG_alecf
  nsXPIDLCString sourceval;
  nsXPIDLCString propval;
  aSource->GetValue(getter_Copies(sourceval));
  aProperty->GetValue(getter_Copies(propval));
  printf("GetTargets(%s, %s,..)\n", (const char*)sourceval,
         (const char*)propval);
#endif
  
  nsresult rv;
  PRBool isProp;
  nsCOMPtr<nsISupportsArray> arcs;
  nsISimpleEnumerator *enumerator;
  
  if (NS_SUCCEEDED(aProperty.EqualsResource(kNC_Child, &isProp)) &&
      isProp) {

    // here we need to determine if we need to extract out the source
    // or use aSource?
    if (!PL_strcmp(sourceval, "dsresource:", 11)) {
      // somehow get the source
      rv = mDataSource->ArcLabelsOut(realsource, &enumerator);      
    } else {
      rv = mDataSource->ArcLabelsOut(aSource, &enumerator);
    }
    // enumerate all the children and create the composite resources
    PRBool hasMoreArcs=PR_FALSE;

    rv = enumerator->hasMoreElements(&hasMoreArcs);
    while (NS_SUCCEEDED(rv) && hasMoreArcs) {
      
      // get the next arc
      nsCOMPtr<nsISupports> arcSupports;
      rv = enumerator->GetNext(getter_AddRefs(arcSupports));
      nsCOMPtr<nsIRDFResource> arc = do_QueryInterface(arcSupports, &rv);

      // get all the resources on the ends of the arc arcs
      nsCOMPtr<nsISimpleEnumerator> targetEnumerator;
      rv = mDataSource->GetTargets(aSource, element, PR_TRUE,
                                   getter_AddRefs(targetEnumerator));

      PRBool hasMoreTargets;
      rv = targetEnumerator->hasMoreElements(&hasMoreTargets);
      while (NS_SUCCEEDED(rv) && hasMoreTargets) {
        // get the next target
        nsCOMPtr<nsISupports> targetSupports;
        rv = enumerator->GetNext(getter_AddRefs(targetSupports));
        nsCOMPtr<nsIRDFResource> target=do_QueryInterface(targetSupports, &rv);

        // now we have an (arc, target) tuple that will be our node
        // arc will become #Name
        // target will become #Value
#ifdef DEBUG_alecf
        nsXPIDLString arcValue;
        nsXPIDLString targetValue;

        arc->GetValue(getter_Copies(arcValue));
        target->GetValue(getter_Copies(targetValue));
        printf("#child of %s:\n\t%s = %s\n",
               (const char*)sourceval
#endif

      }
      
      rv = enumerator->hasMoreElements(&hasMoreArcs);
    }
    
    nsIRDFResource *res = CreateCompositeResource(aSource, aProperty);

  } else if (NS_SUCCEEDED(aProperty.EqualsResource(kNC_Name, &isProp)) &&
             isProp) {
    if (!PL_strncmp(sourceval, "dsresource:", 11) {
      // extract out the name

    }
    
  } else if (NS_SUCCEEDED(aProperty.EqualsResource(kNC_Value, &isProp)) &&
             isProp) {


  } else {
    rv = NS_NewISupportsArray(getter_AddRefs(arcs));
    if (NS_FAILED(rv)) return rv;
    
    nsArrayEnumerator* cursor =
      new nsArrayEnumerator(arcs);

    if (!cursor) return NS_ERROR_OUT_OF_MEMORY;
    
    *_retval = cursor;
    NS_ADDREF(*_retval);
  }
  
  return NS_OK;
}

/* void Assert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue)
{
  return NS_RDF_NO_VALUE;
}

/* void Unassert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
  return NS_RDF_NO_VALUE;
}

/* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval)
{
  return NS_RDF_NO_VALUE;
}

/* void AddObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::AddObserver(nsIRDFObserver *aObserver)
{
  return NS_RDF_NO_VALUE;
}

/* void RemoveObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
  return NS_RDF_NO_VALUE;
}

/* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval)
{
  return NS_RDF_NO_VALUE;
}

/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::ArcLabelsOut(nsIRDFResource *aSource,
                                        nsISimpleEnumerator **_retval)
{
  nsresult rv=NS_OK;
  
  nsCOMPtr<nsISupportsArray> arcs;
  rv = NS_NewISupportsArray(getter_AddRefs(arcs));
  
  if (NS_FAILED(rv)) return rv;
  nsXPIDLCString sourceval;
  aSource->GetValue(getter_Copies(sourceval));
  
#ifdef DEBUG_alecf
  printf("ArcLabelsOut(%s)\n", (const char*)sourceval);
#endif
  
  // this is a terrible ugly hack, but it works.
  // set the datasource if the URI begins with rdf:
  if (!PL_strncmp((const char*)sourceval, "rdf:", 4)) {
#ifdef DEBUG_alecf
    printf("Ahah! This is a datasource node. Setting the datasource..");
#endif
    nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
    rv = rdf->GetDataSource((const char*)sourceval, &mDataSource);
    
#ifdef DEBUG_alecf
    printf("done.\n");
#endif
    
  } else {
    arcs->AppendElement(kNC_Name);
    arcs->AppendElement(kNC_Value);
    arcs->AppendElement(kNC_Child);
  }

  nsArrayEnumerator* cursor =
    new nsArrayEnumerator(arcs);

  if (!cursor) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(cursor);
  *_retval = cursor;

  return NS_OK;
}

/* nsISimpleEnumerator GetAllResources (); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::GetAllResources(nsISimpleEnumerator **_retval)
{
  return NS_RDF_NO_VALUE;
}

/* void Flush (); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::Flush()
{
  return NS_RDF_NO_VALUE;
}

/* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval)
{
  return NS_RDF_NO_VALUE;
}

/* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::IsCommandEnabled(nsISupportsArray * aSources, nsIRDFResource *aCommand, nsISupportsArray * aArguments, PRBool *_retval)
{
  return NS_RDF_NO_VALUE;
}

/* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP
nsRDFDataSourceDataSource::DoCommand(nsISupportsArray * aSources, nsIRDFResource *aCommand, nsISupportsArray * aArguments)
{
  return NS_RDF_NO_VALUE;
}

nsresult
NS_NewRDFDataSourceDataSource(nsISupports *, const nsIID& iid,
                              void ** result)

{
  nsRDFDataSourceDataSource * dsds = new nsRDFDataSourceDataSource();
  if (!dsds) return NS_ERROR_NOT_INITIALIZED;
  return dsds->QueryInterface(iid, result);
  
}
