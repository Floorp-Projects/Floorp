/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsCOMPtr.h"
#include "nsRDFDOMDataSource.h"
#include "nsRDFCID.h"
#include "rdf.h"
#include "plstr.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsEnumeratorUtils.h"

#include "nsIRDFLiteral.h"
#include "nsIRDFResource.h"
#include "nsISupportsArray.h"

#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNode.h"
#include "nsISupportsArray.h"
#include "nsIDOMViewerElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsRDFDOMViewerUtils.h"


#include "nsICSSDeclaration.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDocument.h"

#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"

#ifdef DEBUG
#include "nsIFrameDebug.h"
#endif

#include "prprf.h"

#define NC_RDF_Name  NC_NAMESPACE_URI "Name"
#define NC_RDF_Value NC_NAMESPACE_URI "Value"
#define NC_RDF_Type  NC_NAMESPACE_URI "Type"
#define NC_RDF_Child NC_NAMESPACE_URI "child"
#define NC_RDF_DOMRoot "NC:DOMRoot"

static PRInt32 gCurrentId=0;

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kFrameIID, NS_IFRAME_IID);

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

NS_IMPL_ISUPPORTS2(nsRDFDOMDataSource,
                   nsIRDFDataSource,
                   nsIDOMDataSource);


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
  
#ifdef DEBUG_alecf_
  nsXPIDLCString sourceval;
  aSource->GetValue(getter_Copies(sourceval));
  nsXPIDLCString propval;
  aProperty->GetValue(getter_Copies(propval));
  printf("GetTarget(%s, %s,..)\n", (const char*)sourceval,
         (const char*)propval);
#endif

  *_retval = nsnull;
  
  nsresult rv;
  nsAutoString str; str.AssignWithConversion("unknown");
  if (aSource == kNC_DOMRoot) {
    if (aProperty == kNC_Name)
      str.AssignWithConversion("DOMRoot");
    else if (aProperty == kNC_Value)
      str.AssignWithConversion("DOMRootValue");
    else if (aProperty == kNC_Type)
      str.AssignWithConversion("DOMRootType");
    rv = NS_OK;
  } else {
    // try the different objects we know about:

    nsCOMPtr<nsIDOMViewerElement> nodeContainer =
      do_QueryInterface(aSource);

    if (nodeContainer) {
	    nsCOMPtr<nsISupports> supports;
	    nodeContainer->GetObject(getter_AddRefs(supports));

	    rv = getTargetForKnownObject(supports, aProperty, _retval);
    }
  }

  // if nobody set _retval, then create a literal from *str
  if (!(*_retval))
    rv = createLiteral(str, _retval);
  
  return rv;
}

nsresult
nsRDFDOMDataSource::createFrameTarget(nsIFrame *frame,
                                        nsIRDFResource *aProperty,
                                        nsIRDFNode **aResult)
{
  nsAutoString str;
  if (aProperty == kNC_Name) {
#ifdef DEBUG
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->GetFrameName(str);
    }
#endif
  }
  else if (aProperty == kNC_Type)
    str.AssignWithConversion("frame");

  return createLiteral(str, aResult);
}


nsresult
nsRDFDOMDataSource::createDOMNodeTarget(nsIDOMNode *node,
                                        nsIRDFResource *aProperty,
                                        nsIRDFNode **aResult)
{
  nsAutoString str;
  if (aProperty == kNC_Name)
    node->GetNodeName(str);
  else if (aProperty == kNC_Value)
    node->GetNodeValue(str);
  else if (aProperty == kNC_Type) {
    PRUint16 type;
    node->GetNodeType(&type);
    str.AppendInt(PRInt32(type));
  }
  return createLiteral(str, aResult);
}

nsresult
nsRDFDOMDataSource::createContentTarget(nsIContent *content,
                                        nsIRDFResource *aProperty,
                                        nsIRDFNode **aResult)
{
  nsAutoString str;
  if (aProperty == kNC_Name) {
    nsCOMPtr<nsIAtom> atom;
    content->GetTag(*getter_AddRefs(atom));
    atom->ToString(str);
  } else if (aProperty == kNC_Value) {


  } else if (aProperty == kNC_Type) {

    str.AssignWithConversion("content");
  }

  return createLiteral(str, aResult);
}

nsresult
nsRDFDOMDataSource::createStyledContentTarget(nsIStyledContent *styledContent,
                                              nsIRDFResource *aProperty,
                                              nsIRDFNode **aResult)
{

  return createContentTarget(styledContent, aProperty, aResult);
}
                                        
/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP
nsRDFDOMDataSource::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{

  nsXPIDLCString sourceval;
  aSource->GetValue(getter_Copies(sourceval));
#ifdef DEBUG_alecf_
  nsXPIDLCString propval;
  aProperty->GetValue(getter_Copies(propval));
  printf("GetTargets(%s, %s,..)\n", (const char*)sourceval,
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
  if (aSource == kNC_DOMRoot) {
    nsCStringKey frameMode("frame");
    if (mModeTable.Get(&frameMode))
      rv = getTargetsForKnownObject(mRootFrame, aProperty, PR_TRUE, arcs);
    else
      rv = getTargetsForKnownObject(mDocument, aProperty, PR_TRUE, arcs);
  } else {
    nsCOMPtr<nsIDOMViewerElement> nodeContainer;
    nodeContainer = do_QueryInterface(aSource, &rv);

    if (NS_SUCCEEDED(rv) && nodeContainer) {
      nsCOMPtr<nsISupports> supports;
      nodeContainer->GetObject(getter_AddRefs(supports));

      rv = getTargetsForKnownObject(supports, aProperty, PR_FALSE, arcs);
      
    }
  }

  return NS_OK;
}


nsresult
nsRDFDOMDataSource::createContentArcs(nsIContent *content,
                                      nsISupportsArray* arcs)
{
  nsresult rv;
  rv = createContentAttributeArcs(content, arcs);
  rv = createContentMiscArcs(content, arcs);
  rv = createContentChildArcs(content, arcs);
  return rv;
}

nsresult
nsRDFDOMDataSource::createContentChildArcs(nsIContent *content,
                                           nsISupportsArray* arcs)
{
  nsresult rv = NS_OK;
  PRInt32 length;
  content->ChildCount(length);
  PRInt32 i;
  for (i=0; i<length; i++) {
    nsCOMPtr<nsIContent> child;
    content->ChildAt(i, *getter_AddRefs(child));

    nsCOMPtr<nsIRDFResource> resource;
    rv = getResourceForObject(child, getter_AddRefs(resource));
    rv = arcs->AppendElement(resource);
  }

  return rv;
}

nsresult
nsRDFDOMDataSource::createContentAttributeArcs(nsIContent* content,
                                               nsISupportsArray* arcs)
{
  nsresult rv;
  PRInt32 attribs;

  content->GetAttributeCount(attribs);

  // content attributes don't have objects associated with them, so
  // we have touse the viewerObjects
  PRInt32 i;
  for (i=0; i< attribs; i++) {
    nsCOMPtr<nsIAtom> nameAtom, prefix;
    PRInt32 nameSpace;
    content->GetAttributeNameAt(i, nameSpace, *getter_AddRefs(nameAtom),
                                *getter_AddRefs(prefix));

    nsString attribValue;
    rv = content->GetAttribute(nameSpace, nameAtom, attribValue);
    if (NS_FAILED(rv)) continue;

    nsString name; nameAtom->ToString(name);

    appendLeafObject(name, attribValue, arcs);
    
  }
  return NS_OK;
}

nsresult
nsRDFDOMDataSource::createContentMiscArcs(nsIContent *content,
                                          nsISupportsArray *arcs)
{
  nsAutoString name;
  nsAutoString value;

  // namespace
  name.AssignWithConversion("namespace");

  PRInt32 namespaceID;
  content->GetNameSpaceID(namespaceID);
  value.AppendInt(namespaceID);

  appendLeafObject(name, value, arcs);

  // cancontainchildren
  name.AssignWithConversion("Can contain children");
  
  PRBool containerCapability;
  content->CanContainChildren(containerCapability);
  value.AssignWithConversion(containerCapability ? "yes" : "no");
 
  appendLeafObject(name, value, arcs);

  return NS_OK;
}

nsresult
nsRDFDOMDataSource::createStyledContentArcs(nsIStyledContent *content,
                                            nsISupportsArray *arcs)
{
  nsresult rv;

  // classes
  createStyledContentClassArcs(content, arcs);
  
  nsCOMPtr<nsISupportsArray> rules;

  // content style rules
  NS_NewISupportsArray(getter_AddRefs(rules));
  rv = content->GetContentStyleRules(rules);
  if (NS_SUCCEEDED(rv))
    rv = createArcsFromSupportsArray(rules, arcs);

  // inline style rules
  NS_NewISupportsArray(getter_AddRefs(rules));
  rv = content->GetContentStyleRules(rules);
  if (NS_SUCCEEDED(rv))
    rv = createArcsFromSupportsArray(rules, arcs);

  // continue up hierarchy
  return createContentArcs(content, arcs);
}

nsresult
nsRDFDOMDataSource::createStyledContentClassArcs(nsIStyledContent *content,
                                                 nsISupportsArray *arcs)
{
  nsVoidArray classes;
  content->GetClasses(classes);
  PRInt32 count = classes.Count();
  for (PRInt32 i=0; i< count ; i++) {
    nsIAtom *atom = (nsIAtom*)classes[i];
    
    nsAutoString value;
    atom->ToString(value);

    nsAutoString name; name.AssignWithConversion("class");
    appendLeafObject(name, value, arcs);
  }
  return NS_OK;
}

nsresult
nsRDFDOMDataSource::getDOMCSSStyleDeclTarget(nsIDOMCSSStyleDeclaration *decl,
                                             nsIRDFResource *property,
                                             nsIRDFNode **aResult)
{
  nsAutoString str;
  if (property == kNC_Name)
    str.AssignWithConversion("DOM CSS Style Declaration");
  
  else if (property == kNC_Value)
    decl->GetCssText(str);
  
  else if (property == kNC_Type)
    str.AssignWithConversion("domcssstyledecl");

  return createLiteral(str, aResult);
}
                                       

nsresult
nsRDFDOMDataSource::getCSSStyleRuleTarget(nsICSSStyleRule *rule,
                                          nsIRDFResource *property,
                                          nsIRDFNode **aResult)
{
  nsICSSDeclaration* decl;

  nsAutoString str;
  if (property == kNC_Name)
    str.AssignWithConversion("DOM CSS Style Rule");
  
  else if (property == kNC_Value) {
    decl = rule->GetDeclaration();
    if (decl) {
      decl->ToString(str);
    } else
      str.AssignWithConversion("<unavailable>");
    
  } else if (property == kNC_Type)
    str.AssignWithConversion("domcssstylerule");
    
  return createLiteral(str, aResult);

}

nsresult
nsRDFDOMDataSource::getDOMCSSRuleTarget(nsIDOMCSSRule *rule,
                                        nsIRDFResource *property,
                                        nsIRDFNode **aResult)
{
  nsAutoString str;
  if (property == kNC_Name)
    str.AssignWithConversion("DOM CSS Rule");
  else if (property == kNC_Value)
    rule->GetCssText(str);
  else
    str.AssignWithConversion("domcssrule");

  return createLiteral(str, aResult);
}


nsresult
nsRDFDOMDataSource::getDOMCSSStyleRuleTarget(nsIDOMCSSStyleRule *rule,
                                       nsIRDFResource *property,
                                       nsIRDFNode **aResult)
{
  return getDOMCSSRuleTarget(rule, property, aResult);
}


nsresult
nsRDFDOMDataSource::getCSSRuleTarget(nsICSSRule *rule,
                                       nsIRDFResource *property,
                                       nsIRDFNode **aResult)
{
  nsAutoString str;
  if (property == kNC_Name)
    str.AssignWithConversion("CSSRule");
  else if (property == kNC_Value)
    str.AssignWithConversion("<unavailable>");
  else if (property == kNC_Type)
    str.AssignWithConversion("cssrule");

  return createLiteral(str, aResult);
}

nsresult
nsRDFDOMDataSource::getStyleRuleTarget(nsIStyleRule *rule,
                                       nsIRDFResource *property,
                                       nsIRDFNode **aResult)
{
  nsAutoString str;
  if (property == kNC_Name)
    str.AssignWithConversion("styleRule");
  else if (property == kNC_Value)
    str.AssignWithConversion("<unavailable>");
  else if (property == kNC_Type) {
    str.AssignWithConversion("stylerule");
  }

  return createLiteral(str, aResult);
}

nsresult
nsRDFDOMDataSource::createFrameArcs(nsIFrame* frame,
                                    nsISupportsArray* arcs)
{
  nsresult rv;
  nsIFrame *child;
  nsCOMPtr<nsIRDFResource> resource;
  nsCOMPtr<nsIPresContext> presContext;

  // primary child list
  frame->FirstChild(nsnull, nsnull, &child);
  while (child) {
    rv = getResourceForObject(child, getter_AddRefs(resource));
    arcs->AppendElement(resource);

    child->GetNextSibling(&child);
  }
  
  // secondary child lists
  PRInt32 listIndex=0;
  nsCOMPtr<nsIAtom> childList;
  frame->GetAdditionalChildListName(listIndex++, getter_AddRefs(childList));
  while (childList) {
    rv = frame->FirstChild(nsnull, childList, &child);
    while (NS_SUCCEEDED(rv) && (child)) {
      rv = getResourceForObject(child, getter_AddRefs(resource));
      arcs->AppendElement(resource);
      
      child->GetNextSibling(&child);
    }

    frame->GetAdditionalChildListName(listIndex++, getter_AddRefs(childList));
  }

  return NS_OK;
}

nsresult
nsRDFDOMDataSource::createHTMLElementArcs(nsIDOMHTMLElement* element,
                                          nsISupportsArray* arcs)
{
  nsresult rv;
  nsCOMPtr<nsIDOMCSSStyleDeclaration> decl;
  
  rv = element->GetStyle(getter_AddRefs(decl));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIRDFResource> resource;
    rv = getResourceForObject(decl, getter_AddRefs(resource));
    arcs->AppendElement(resource);
  }

  return createDOMNodeArcs(element, arcs);
    
}


nsresult
nsRDFDOMDataSource::createDOMNodeArcs(nsIDOMNode *node,
                                      nsISupportsArray* arcs)
{
  nsresult rv;

  rv = createDOMAttributeArcs(node, arcs);
  rv = createDOMChildArcs(node, arcs);
    
  return rv;

}

nsresult
nsRDFDOMDataSource::createDOMAttributeArcs(nsIDOMNode *node,
                                           nsISupportsArray *arcs)
{
  if (!node) return NS_OK;
  nsresult rv;
  // attributes
  nsCOMPtr<nsIDOMNamedNodeMap> attrmap;
  rv = node->GetAttributes(getter_AddRefs(attrmap));
  if (NS_FAILED(rv)) return rv;

  rv = createDOMNamedNodeMapArcs(attrmap, arcs);
  
  return rv;
}

nsresult
nsRDFDOMDataSource::createDOMChildArcs(nsIDOMNode *node,
                                       nsISupportsArray *arcs)
{
  if (!node) return NS_OK;
  nsresult rv;
  // child nodes
  nsCOMPtr<nsIDOMNodeList> childNodes;
  rv = node->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(rv)) return rv;

  rv = createDOMNodeListArcs(childNodes, arcs);
  
  return rv;
}

nsresult
nsRDFDOMDataSource::createDOMNodeListArcs(nsIDOMNodeList *nodelist,
                                          nsISupportsArray *arcs)
{

  if (!nodelist) return NS_OK;
  PRUint32 length=0;
  nsresult rv = nodelist->GetLength(&length);
  if (NS_FAILED(rv)) return rv;

    
  PRUint32 i;
  for (i=0; i<length; i++) {
      
    nsCOMPtr<nsIDOMNode> node;
    rv = nodelist->Item(i, getter_AddRefs(node));

    nsCOMPtr<nsIRDFResource> resource;
    rv = getResourceForObject(node, getter_AddRefs(resource));
    rv = arcs->AppendElement(resource);
  }
  return rv;


}

nsresult
nsRDFDOMDataSource::createDOMNamedNodeMapArcs(nsIDOMNamedNodeMap *nodelist,
                                          nsISupportsArray *arcs)
{

  if (!nodelist) return NS_OK;
  PRUint32 length=0;
  nsresult  rv = nodelist->GetLength(&length);
  if (NS_FAILED(rv)) return rv;

    
  PRUint32 i;
  for (i=0; i<length; i++) {
      
    nsCOMPtr<nsIDOMNode> node;
    rv = nodelist->Item(i, getter_AddRefs(node));

    nsCOMPtr<nsIRDFResource> resource;
    rv = getResourceForObject(node, getter_AddRefs(resource));
    rv = arcs->AppendElement(resource);
  }
  return rv;


}


nsresult
nsRDFDOMDataSource::getResourceForObject(nsISupports* object,
                                         nsIRDFResource** resource)
{
  nsresult rv;
  nsISupportsKey objKey(object);
  nsCOMPtr<nsISupports> supports = dont_AddRef((nsISupports*)objectTable.Get(&objKey));

  // it's not in the hash table, so add it.
  if (supports) {
    nsCOMPtr<nsIRDFResource> res = do_QueryInterface(supports, &rv);
    *resource = res;
  } else {
    char *uri =
      PR_smprintf("object://%8.8X", gCurrentId++);
    
    rv = getRDFService()->GetResource(uri, resource);
    if (NS_FAILED(rv)) return rv;

    // add it to the hash table
    objectTable.Put(&objKey, *resource);
    
    // now fill in the resource stuff
    nsCOMPtr<nsIDOMViewerElement> nodeContainer =
      do_QueryInterface(*resource, &rv);

    if (NS_FAILED(rv)) return rv;
    nodeContainer->SetObject(object);
  }

  return NS_OK;
}

nsresult
nsRDFDOMDataSource::getTargetForKnownObject(nsISupports* object,
                                            nsIRDFResource *aProperty,
                                            nsIRDFNode ** aResult)
{
  nsresult rv;

  // should we display nsIFrames?
  nsCStringKey frameMode("frame");
  if (mModeTable.Get(&frameMode)) {
    nsIFrame *frame;
    rv = object->QueryInterface(kFrameIID, (void **)&frame);
    if (NS_SUCCEEDED(rv)) {
      return createFrameTarget(frame, aProperty, aResult);
    }
  }

  // should we display nsIDOMNodes?
  nsCStringKey domMode("dom");
  if (mModeTable.Get(&domMode)) {
    
    // nsIDOMNode
    nsCOMPtr<nsIDOMNode> node =
      do_QueryInterface(object, &rv);
    if (NS_SUCCEEDED(rv))
      return createDOMNodeTarget(node, aProperty, aResult);
  }

  // if we get this far, default to nsIContent mode
  
  // nsIStyledContent
  // nsIContent
  nsCOMPtr<nsIStyledContent> styledContent =
    do_QueryInterface(object, &rv);
  if (NS_SUCCEEDED(rv))
    return createStyledContentTarget(styledContent, aProperty, aResult);
    
    // nsIContent
  nsCOMPtr<nsIContent> content =
    do_QueryInterface(object, &rv);
  if (NS_SUCCEEDED(rv))
    return createContentTarget(content, aProperty, aResult);


  //
  // nsIRDFDOMViewerObject:
  //
  nsCOMPtr<nsIRDFDOMViewerObject> viewerObject =
    do_QueryInterface(object, &rv);
  
  if (NS_SUCCEEDED(rv))
    return viewerObject->GetTarget(aProperty, aResult);

  // nsIDOMCSSStyleDeclaration
  nsCOMPtr<nsIDOMCSSStyleDeclaration> decl =
    do_QueryInterface(object, &rv);
  if (NS_SUCCEEDED(rv))
    return getDOMCSSStyleDeclTarget(decl, aProperty, aResult);
    
  
  // nsIDOMCSSStyleRule
  // nsIDOMCSSRule
  nsCOMPtr<nsIDOMCSSStyleRule> domCSSStyleRule =
    do_QueryInterface(object, &rv);
  if (NS_SUCCEEDED(rv))
    return getDOMCSSStyleRuleTarget(domCSSStyleRule, aProperty, aResult);

  nsCOMPtr<nsIDOMCSSRule> domCSSRule =
    do_QueryInterface(object, &rv);
  if (NS_SUCCEEDED(rv))
    return getDOMCSSRuleTarget(domCSSRule, aProperty, aResult);

  // nsICSSStyleRule
  // nsICSSRule
  // nsIStyleRule
  nsCOMPtr<nsICSSStyleRule> cssStyleRule =
    do_QueryInterface(object, &rv);
  if (NS_SUCCEEDED(rv))
    return getCSSRuleTarget(cssStyleRule, aProperty, aResult);

  nsCOMPtr<nsICSSRule> cssRule =
    do_QueryInterface(object, &rv);
  if (NS_SUCCEEDED(rv))
    return getCSSRuleTarget(cssRule, aProperty, aResult);
  
  nsCOMPtr<nsIStyleRule> styleRule =
    do_QueryInterface(object,&rv);
  if (NS_SUCCEEDED(rv))
    return getStyleRuleTarget(styleRule, aProperty, aResult);

  //
  // doh! I don't know what the heck this is.
  printf("getTargetForKnownObject: unknown Object!\n");
  return NS_OK;
}

// there's some special logic here, because there is no dupe-checking amongst
// the interfaces.
// the problem is that "object" may implement more than one support interface,
// and some of those interfaces may inherit from each other
// solutions are as follows:
// - For objects which implement multiple interfaces, we present a UI to the
//   user which determines which interfaces we should follow
//   (The problem yet to be solved: which interface to we answer to with
//   GetTarget?
// - For interfaces which inherit from each other, we need to start at a
//   leaf interface, and keep QI'ing up in the inheritance hierarchy.
//   It is the responsibility of each of the interface creators to call
//   up the hierarchy. For an example, follow the target creation for
//   nsIStyledContent
// - special case the nsIDocument interface

nsresult
nsRDFDOMDataSource::getTargetsForKnownObject(nsISupports *object,
                                             nsIRDFResource *aProperty,
                                             PRBool useDOM,
                                             nsISupportsArray *arcs)
{
  nsresult rv;

  if (!object) return NS_ERROR_NULL_POINTER;
  
  // nsIDocument (special case)
  nsCOMPtr<nsIDOMDocument> document =
    do_QueryInterface(object, &rv);
  if (NS_SUCCEEDED(rv)) {
    //    nsIContent *content;
    //    document->GetRootContent(&content);
    //    if (content) {
    //      rv = createContentArcs(content, arcs);
    //      NS_RELEASE(content);
    //    }
    printf("This is a document.\n");
    createDOMNodeArcs(document, arcs);
    return NS_OK;
  }

  // nsIFrame (testing right now)
  nsCStringKey frameKey("frame");
  if (mModeTable.Get(&frameKey)) {
    nsIFrame* frame;
    rv = object->QueryInterface(kFrameIID, (void **)&frame);
    if (NS_SUCCEEDED(rv)) {
      printf("creating arcs for frame\n");
      createFrameArcs(frame, arcs);
    }
  }
  
  // nsIDOMNode hierarchy
  nsCStringKey domKey("dom");
  if (mModeTable.Get(&domKey)) {

    // try HTML first
    //    nsCOMPtr<nsIDOMHTMLElement> htmlElement =
    //      do_QueryInterface(object, &rv);
    //    if (NS_SUCCEEDED(rv))
    //      rv = createHTMLElementArcs(htmlElement, arcs);
    //    else {
      
      nsCOMPtr<nsIDOMNode> node =
        do_QueryInterface(object, &rv);
      if (NS_SUCCEEDED(rv))
        rv = createDOMNodeArcs(node, arcs);
      //    }
  }

  // nsIContent hierarchy
  nsCStringKey contentKey("content");
  if (mModeTable.Get(&contentKey) && !useDOM) {
    // start at nsIStyleContent and work upwards
    nsCOMPtr<nsIStyledContent> styledContent =
      do_QueryInterface(object, &rv);
    if (NS_SUCCEEDED(rv))
      rv = createStyledContentArcs(styledContent, arcs);
    else {
      nsCOMPtr<nsIContent> content =
        do_QueryInterface(object, &rv);
      if (NS_SUCCEEDED(rv))
        rv = createContentArcs(content, arcs);
    }
    
  }
  return NS_OK;
}



nsresult
nsRDFDOMDataSource::appendLeafObject(nsString& name,
                               nsString& value,
                               nsISupportsArray* arcs)
{
  nsresult rv;
  nsCStringKey leafKey("leaf");
  if (!mModeTable.Get(&leafKey)) return NS_OK;
  
  nsIRDFDOMViewerObject* viewerObject;
  rv = NS_NewDOMViewerObject(NS_GET_IID(nsIRDFDOMViewerObject), (void **)&viewerObject);

  nsAutoString type; type.AssignWithConversion("leaf");
  viewerObject->SetTargetLiteral(kNC_Name, name);
  viewerObject->SetTargetLiteral(kNC_Value, value);
  viewerObject->SetTargetLiteral(kNC_Type, type);
  
  nsCOMPtr<nsIRDFResource> resource;
  rv = getResourceForObject(viewerObject, getter_AddRefs(resource));
  rv = arcs->AppendElement(resource);
  
  NS_RELEASE(viewerObject);
  return NS_OK;
}

nsresult
nsRDFDOMDataSource::createArcsFromSupportsArray(nsISupportsArray *array,
                                                nsISupportsArray *arcs)
{
  nsresult rv;
  PRUint32 count;
  array->Count(&count);

  for (PRUint32 i=0; i<count; i++) {
    nsCOMPtr<nsISupports> item;

    rv = array->GetElementAt(i, getter_AddRefs(item));
    if (NS_FAILED(rv)) continue;

    nsCOMPtr<nsIRDFResource> resource;
    getResourceForObject(item, getter_AddRefs(resource));
    arcs->AppendElement(resource);
    
  }
  return NS_OK;
}

nsresult
nsRDFDOMDataSource::createLiteral(nsString& str, nsIRDFNode **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIRDFLiteral> literal;
    
  PRUnichar* uniStr = str.ToNewUnicode();
  rv = getRDFService()->GetLiteral(uniStr,
                                   getter_AddRefs(literal));
  nsMemory::Free(uniStr);
    
  *aResult = literal;
  NS_IF_ADDREF(*aResult);
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


NS_IMETHODIMP 
nsRDFDOMDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsRDFDOMDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  *result = (aArc == kNC_Name ||
             aArc == kNC_Value ||
             aArc == kNC_Type ||
             aArc == kNC_Child);
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

#ifdef DEBUG_alecf_
  nsXPIDLCString sourceval;
  aSource->GetValue(getter_Copies(sourceval));
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

NS_IMETHODIMP
nsRDFDOMDataSource::Change(nsIRDFResource *, nsIRDFResource *,
                           nsIRDFNode *, nsIRDFNode*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFDOMDataSource::Move(nsIRDFResource *, nsIRDFResource *,
                         nsIRDFResource *, nsIRDFNode*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


/* nsISimpleEnumerator GetAllResources (); */
NS_IMETHODIMP
nsRDFDOMDataSource::GetAllResources(nsISimpleEnumerator **_retval)
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
                                          NS_GET_IID(nsIRDFService),
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

// nsIDOMDataSource methods
nsresult
nsRDFDOMDataSource::SetWindow(nsIDOMWindowInternal *window) {
  nsresult rv;

  objectTable.Reset();
  nsCOMPtr<nsIDOMDocument> document;
  rv = window->GetDocument(getter_AddRefs(mDocument));
  
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject =
    do_QueryInterface(window, &rv);
  if (NS_FAILED(rv)) {
    printf("Couldn't get scriptglobalobject\n");
    return NS_OK;
  }
  
  nsCOMPtr<nsIDocShell> docShell;
  scriptGlobalObject->GetDocShell(getter_AddRefs(docShell));
  if (!docShell) {
    printf("Couldn't get webshell\n");
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> pshell;
  rv = docShell->GetPresShell(getter_AddRefs(pshell));
  if (NS_FAILED(rv)) {
    printf("Couldn't get presshell\n");
    return NS_OK;
  }
    
  pshell->GetRootFrame(&mRootFrame);
  if (NS_FAILED(rv)) {
    printf("Couldn't get frame\n");
    return NS_OK;
  }
  
  nsAutoString framename;
#ifdef DEBUG
  nsIFrameDebug*  frameDebug;

  if (NS_SUCCEEDED(mRootFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
    frameDebug->GetFrameName(framename);
  }
#endif

  printf("Got root frame: %s\n", framename.ToNewCString());
  return rv;
}

nsresult
nsRDFDOMDataSource::SetMode(const char *mode, PRBool active)
{
  printf("Turning %s the %s mode\n", active ? "ON" : "OFF",
         mode);
  nsCStringKey modeKey(mode);
  mModeTable.Put(&modeKey, (void *)active);
  return NS_OK;
}

nsresult
nsRDFDOMDataSource::GetMode(const char *mode, PRBool *active)
{
  nsCStringKey modeKey(mode);
  *active = (PRBool)mModeTable.Get(&modeKey);
  return NS_OK;
}


NS_METHOD
nsRDFDOMDataSource::Create(nsISupports* aOuter,
                       const nsIID& iid,
                       void **result)
{
  nsRDFDOMDataSource* ds = new nsRDFDOMDataSource();
  if (!ds) return NS_ERROR_NULL_POINTER;
  NS_ADDREF(ds);
  nsresult rv =
    ds->QueryInterface(iid, result);
  NS_RELEASE(ds);
  return rv;
}
