/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsRDFDOMDataSource.h"
#include "nsRDFCID.h"
#include "rdf.h"
#include "plstr.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsEnumeratorUtils.h"

#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMText.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMAttr.h"

#include "nsIDOMViewerElement.h"

#include "nsIDOMHTMLDocument.h"

#include "nsIDOMXULDocument.h"

#include "prprf.h"

#define NC_RDF_Name  NC_NAMESPACE_URI "Name"
#define NC_RDF_Value NC_NAMESPACE_URI "Value"
#define NC_RDF_Type  NC_NAMESPACE_URI "Type"
#define NC_RDF_Child NC_NAMESPACE_URI "child"
#define NC_RDF_DOMRoot "NC:DOMRoot"

static PRInt32 gCurrentId=0;

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kISupportsIID, NS_ISUPPORTS_IID);

nsRDFDOMDataSource::nsRDFDOMDataSource():
    mURI(nsnull),
    mRDFService(nsnull),
    mObservers(nsnull)
{
  NS_INIT_REFCNT();
  getRDFService()->GetResource(NC_RDF_Child, &kNC_Child);
  getRDFService()->GetResource(NC_RDF_Name, &kNC_Name);
  getRDFService()->GetResource(NC_RDF_Value, &kNC_Value);
  getRDFService()->GetResource(NC_RDF_Type, &kNC_Type);
  getRDFService()->GetResource(NC_RDF_DOMRoot, &kNC_DOMRoot);

}

nsRDFDOMDataSource::~nsRDFDOMDataSource()
{
  if (mURI) PL_strfree(mURI);
  NS_RELEASE(kNC_Child);
  NS_RELEASE(kNC_Name);
  NS_RELEASE(kNC_Type);
  NS_RELEASE(kNC_Value);
  if (mRDFService) nsServiceManager::ReleaseService(kRDFServiceCID,
                                                      mRDFService);
}

NS_IMPL_ADDREF(nsRDFDOMDataSource)
NS_IMPL_RELEASE(nsRDFDOMDataSource)

NS_IMETHODIMP
nsRDFDOMDataSource::QueryInterface(const nsIID& iid, void **result)
{
  nsresult rv = NS_NOINTERFACE;
  if (!result) return NS_ERROR_NULL_POINTER;

  void *res=nsnull;

  if (iid.Equals(nsIRDFDataSource::GetIID()) ||
      iid.Equals(kISupportsIID)) {
    res = NS_STATIC_CAST(nsIRDFDataSource*, this);
  }
  else if(iid.Equals(nsIDOMDataSource::GetIID())) {
      res = NS_STATIC_CAST(nsIDOMDataSource*, this);
  }
  if (res) {
      NS_ADDREF(this);
      *result = res;
      rv = NS_OK;
  }

  return rv;

}


/* void Init (in string uri); */
NS_IMETHODIMP
nsRDFDOMDataSource::Init(const char *uri)
{
    nsresult rv=NS_OK;

    if (!mURI || PL_strcmp(uri, mURI) != 0)
        mURI = PL_strdup(uri);
    
    
    rv = getRDFService()->RegisterDataSource(this, PR_FALSE);
    if (NS_FAILED(rv)) return rv;
    
    return rv;
}


/* readonly attribute string URI; */
NS_IMETHODIMP
nsRDFDOMDataSource::GetURI(char * *aURI)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;
    
    if ((*aURI = nsXPIDLCString::Copy(mURI)) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    else
        return NS_OK;
}


/* nsIRDFResource GetSource (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDOMDataSource::GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsIRDFResource **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* nsISimpleEnumerator GetSources (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDOMDataSource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDOMDataSource::GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **_retval)
{
  
#ifdef DEBUG_alecf
  nsXPIDLCString sourceval;
  aSource->GetValue(getter_Copies(sourceval));
  nsXPIDLCString propval;
  aProperty->GetValue(getter_Copies(propval));
  printf("GetTarget(%s, %s,..)\n", (const char*)sourceval,
         (const char*)propval);
#endif

  nsresult rv;
  nsString str;
  if (aSource == kNC_DOMRoot) {
    if (aProperty == kNC_Name)
      str="DOMRoot";
    else if (aProperty == kNC_Value)
      str="DOMRootValue";
    else if (aProperty == kNC_Type)
      str = "DOMRootType";

  } else {
    nsCOMPtr<nsIDOMViewerElement> nodeContainer =
      do_QueryInterface(aSource);

    nsCOMPtr<nsIDOMNode> node;
    nodeContainer->GetNode(getter_AddRefs(node));

    if (node) {
      if (aProperty == kNC_Name)
        node->GetNodeName(str);
      else if (aProperty == kNC_Value)
        node->GetNodeValue(str);
      else if (aProperty == kNC_Type) {
        PRUint16 type;
        node->GetNodeType(&type);
        str = type;
      } else
        str = "Valid node, unknown arc";
        
    }
  }

  printf("GetTarget() returning %s\n", str.ToNewCString());
  nsCOMPtr<nsIRDFLiteral> literal;

  PRUnichar* uniStr = str.ToNewUnicode();
  rv = getRDFService()->GetLiteral(uniStr,
                                   getter_AddRefs(literal));
  nsAllocator::Free(uniStr);
  
  *_retval = literal;
  NS_IF_ADDREF(*_retval);
  
  return rv;
}


/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDOMDataSource::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{

  nsXPIDLCString sourceval;
  aSource->GetValue(getter_Copies(sourceval));
#ifdef DEBUG_alecf
  nsXPIDLCString propval;
  aProperty->GetValue(getter_Copies(propval));
  printf("GetTarget(%s, %s,..)\n", (const char*)sourceval,
         (const char*)propval);
#endif

  // prepare the root
  nsresult rv;
  nsCOMPtr<nsISupportsArray> arcs;
  rv = NS_NewISupportsArray(getter_AddRefs(arcs));
  nsArrayEnumerator* cursor =
    new nsArrayEnumerator(arcs);
  
  if (!cursor) return NS_ERROR_OUT_OF_MEMORY;
    
  *_retval = cursor;
  NS_ADDREF(*_retval);

  if (!mDocument) return NS_OK;

  
  // what node is this?
  nsCOMPtr<nsIDOMNode> node;
  if (aSource == kNC_DOMRoot) {
    node = mDocument;
  } else {
    nsCOMPtr<nsIDOMViewerElement> nodeContainer;
    nodeContainer = do_QueryInterface(aSource, &rv);

    if (NS_SUCCEEDED(rv) && nodeContainer)
      nodeContainer->GetNode(getter_AddRefs(node));
  }

  // node is now the node we're interested in.

  if (aProperty == kNC_Child) {
    
    PRUint32 i;
    PRUint32 length;

    // attributes
    nsCOMPtr<nsIDOMNamedNodeMap> attrmap;
    node->GetAttributes(getter_AddRefs(attrmap));
    rv = attrmap->GetLength(&length);
    if (NS_FAILED(rv)) return rv;
    
    for (i=0; i<length; i++) {
      nsCOMPtr<nsIDOMNode> attrNode;
      attrmap->Item(i,getter_AddRefs(attrNode));
      
      nsString attrName;
      attrNode->GetNodeName(attrName);
      char *attr_name = attrName.ToNewCString();
      char *uri = PR_smprintf("%s#%s",
                              (const char*)sourceval,
                              attr_name);
      nsCRT::free(attr_name);
      if (uri) {
        nsCOMPtr<nsIRDFResource> resource;
        getRDFService()->GetResource(uri,
                                     getter_AddRefs(resource));
        {
          // now fill in the resource stuff
          nsCOMPtr<nsIDOMViewerElement> nodeContainer =
            do_QueryInterface(resource);
          if (nodeContainer)
            nodeContainer->SetNode(attrNode);
        }
        arcs->AppendElement(resource);
      }
    }
    
    // child nodes
    nsCOMPtr<nsIDOMNodeList> childNodes;
    rv = node->GetChildNodes(getter_AddRefs(childNodes));
    if (NS_FAILED(rv)) return rv;
    
    rv = childNodes->GetLength(&length);
    if (NS_FAILED(rv)) return rv;

    printf("Adding %d child nodes..\n", length);
    
    nsCOMPtr<nsIDOMNode> childNode;
    for (i=0; i<length; i++) {
      
      rv = childNodes->Item(i, getter_AddRefs(childNode));
      if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_alecf
      PRUint16 nodeType;
      childNode->GetNodeType(&nodeType);
      printf("child node %d has type %d\n", i, (PRUint32)nodeType);
#endif

      char *uri =
        PR_smprintf("dom://%8.8X", gCurrentId++);

      if (uri) {
        nsCOMPtr<nsIRDFResource> resource;
        getRDFService()->GetResource(uri,
                                     getter_AddRefs(resource));
        {
          // now fill in the resource stuff
          nsCOMPtr<nsIDOMViewerElement> nodeContainer =
            do_QueryInterface(resource);
          if (nodeContainer)
            nodeContainer->SetNode(childNode);
        }
        arcs->AppendElement(resource);
      }
    }

  }

  return NS_OK;
}


/* void Assert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDOMDataSource::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue)
{
    return NS_RDF_NO_VALUE;
}


/* void Unassert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
NS_IMETHODIMP
nsRDFDOMDataSource::Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_RDF_NO_VALUE;
}


/* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDOMDataSource::HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}


/* void AddObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP
nsRDFDOMDataSource::AddObserver(nsIRDFObserver *aObserver)
{
  if (! mObservers) {
    if ((mObservers = new nsVoidArray()) == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  mObservers->AppendElement(aObserver);
  return NS_OK;
}


/* void RemoveObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP
nsRDFDOMDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
  if (! mObservers)
    return NS_OK;
  mObservers->RemoveElement(aObserver);
  return NS_OK;
}


/* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
NS_IMETHODIMP
nsRDFDOMDataSource::ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsRDFDOMDataSource::ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
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
  
  arcs->AppendElement(kNC_Name);
  arcs->AppendElement(kNC_Value);
  arcs->AppendElement(kNC_Type);
  arcs->AppendElement(kNC_Child);
  
  nsArrayEnumerator* cursor =
    new nsArrayEnumerator(arcs);

  if (!cursor) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(cursor);
  *_retval = cursor;

  return NS_OK;
}


/* nsISimpleEnumerator GetAllResources (); */
NS_IMETHODIMP
nsRDFDOMDataSource::GetAllResources(nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* void Flush (); */
NS_IMETHODIMP
nsRDFDOMDataSource::Flush()
{
    return NS_RDF_NO_VALUE;
}


/* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsRDFDOMDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* nsISimpleEnumerator GetAllCmds (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsRDFDOMDataSource::GetAllCmds(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP
nsRDFDOMDataSource::IsCommandEnabled(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments, PRBool *_retval)
{
    return NS_RDF_NO_VALUE;
}


/* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP
nsRDFDOMDataSource::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments)
{
    return NS_RDF_NO_VALUE;
}


nsIRDFService *
nsRDFDOMDataSource::getRDFService()
{
    
    if (!mRDFService) {
        nsresult rv;
        
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          nsIRDFService::GetIID(),
                                          (nsISupports**) &mRDFService);
        if (NS_FAILED(rv)) return nsnull;
    }
    
    return mRDFService;
}

nsresult
nsRDFDOMDataSource::NotifyObservers(nsIRDFResource *subject,
                                    nsIRDFResource *property,
                                    nsIRDFNode *object,
                                    PRBool assert)
{
#if 0
	if(mObservers)
	{
		nsRDFDOMNotification note = { subject, property, object };
		if (assert)
			mObservers->EnumerateForwards(assertEnumFunc, &note);
		else
			mObservers->EnumerateForwards(unassertEnumFunc, &note);
    }
#endif
	return NS_OK;
}

PRBool
nsRDFDOMDataSource::assertEnumFunc(void *aElement, void *aData)
{
#if 0
  nsRDFDOMNotification *note = (nsRDFDOMNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;
  
  observer->OnAssert(note->subject,
                     note->property,
                     note->object);
#endif
  return PR_TRUE;
}

PRBool
nsRDFDOMDataSource::unassertEnumFunc(void *aElement, void *aData)
{
#if 0
  nsRDFDOMNotification* note = (nsRDFDOMNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnUnassert(note->subject,
                     note->property,
                     note->object);
#endif
  return PR_TRUE;
}

nsresult
nsRDFDOMDataSource::SetWindow(nsIDOMWindow *window) {
  nsresult rv;

  nsCOMPtr<nsIDOMDocument> document;
  rv = window->GetDocument(getter_AddRefs(mDocument));
  printf("Got the document\n");
  if (NS_FAILED(rv)) return rv;

  return rv;
}



nsresult
NS_NewRDFDOMDataSource(nsISupports* aOuter,
                       const nsIID& iid,
                       void **result)
{
  nsRDFDOMDataSource* ds = new nsRDFDOMDataSource();
  if (!ds) return NS_ERROR_NULL_POINTER;
  return ds->QueryInterface(iid, result);

}
