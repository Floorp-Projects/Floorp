/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

  An nsIRDFDocument implementation that builds a tree widget XUL
  content model that is to be used with a tree control.

  TO DO

  1) Get a real namespace for XUL. This should go in a 
     header file somewhere.

  2) Can we do sorting as an insertion sort? This is especially
     important in the case where content streams in; e.g., gets added
     in the OnAssert() method as opposed to the CreateContents()
     method.

 */

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMElementObserver.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeObserver.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCursor.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "rdf.h"
#include "rdfutil.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsIDOMXULElement.h"
#include "nsVoidArray.h"
#include "nsIXULSortService.h"

#include "nsRDFGenericBuilder.h"

////////////////////////////////////////////////////////////////////////

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Columns);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Column);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Folder);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Title);

DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, child);

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIContentIID,                NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,       NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIRDFResourceIID,            NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,             NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);
static NS_DEFINE_IID(kIRDFObserverIID,            NS_IRDFOBSERVER_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);

static NS_DEFINE_IID(kXULSortServiceCID,         NS_XULSORTSERVICE_CID);
static NS_DEFINE_IID(kIXULSortServiceIID,        NS_IXULSORTSERVICE_IID);

////////////////////////////////////////////////////////////////////////

nsrefcnt RDFGenericBuilderImpl::gRefCnt = 0;

nsIAtom* RDFGenericBuilderImpl::kContainerAtom;
nsIAtom* RDFGenericBuilderImpl::kItemContentsGeneratedAtom;
nsIAtom* RDFGenericBuilderImpl::kIdAtom;
nsIAtom* RDFGenericBuilderImpl::kOpenAtom;
nsIAtom* RDFGenericBuilderImpl::kResourceAtom;
nsIAtom* RDFGenericBuilderImpl::kContainmentAtom;
nsIAtom* RDFGenericBuilderImpl::kNaturalOrderPosAtom;

PRInt32  RDFGenericBuilderImpl::kNameSpaceID_RDF;
PRInt32  RDFGenericBuilderImpl::kNameSpaceID_XUL;

nsIRDFService*  RDFGenericBuilderImpl::gRDFService;
nsINameSpaceManager* RDFGenericBuilderImpl::gNameSpaceManager;

nsIRDFResource* RDFGenericBuilderImpl::kNC_Title;
nsIRDFResource* RDFGenericBuilderImpl::kNC_child;
nsIRDFResource* RDFGenericBuilderImpl::kNC_Column;
nsIRDFResource* RDFGenericBuilderImpl::kNC_Folder;
nsIRDFResource* RDFGenericBuilderImpl::kRDF_child;


////////////////////////////////////////////////////////////////////////


RDFGenericBuilderImpl::RDFGenericBuilderImpl(void)
    : mDocument(nsnull),
      mDB(nsnull),
      mRoot(nsnull),
      mTimer(nsnull)
{
    NS_INIT_REFCNT();

    if (gRefCnt == 0) {
        kContainerAtom             = NS_NewAtom("container");
        kItemContentsGeneratedAtom = NS_NewAtom("itemcontentsgenerated");

        kIdAtom              = NS_NewAtom("id");
        kOpenAtom            = NS_NewAtom("open");
        kResourceAtom        = NS_NewAtom("resource");
        kNaturalOrderPosAtom = NS_NewAtom("pos");
        kContainmentAtom     = NS_NewAtom("containment");

        nsresult rv;

        // Register the XUL and RDF namespaces: these'll just retrieve
        // the IDs if they've already been registered by someone else.
        if (NS_SUCCEEDED(rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                           nsnull,
                                                           kINameSpaceManagerIID,
                                                           (void**) &gNameSpaceManager))) {

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
static const char kXULNameSpaceURI[]
    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

static const char kRDFNameSpaceURI[]
    = RDF_NAMESPACE_URI;

            rv = gNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");

            rv = gNameSpaceManager->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");
        }
        else {
            NS_ERROR("couldn't create namepsace manager");
        }


        // Initialize the global shared reference to the service
        // manager and get some shared resource objects.
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          kIRDFServiceIID,
                                          (nsISupports**) &gRDFService);

        if (NS_FAILED(rv)) {
            NS_ERROR("couldnt' get RDF service");
        }

        NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURINC_Title,  &kNC_Title)),
                  "couldn't get resource");

        NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURINC_child,  &kNC_child)),
                  "couldn't get resource");

        NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURINC_Column, &kNC_Column)),
                  "couldn't get resource");

        NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURINC_Folder,  &kNC_Folder)),
                  "couldn't get resource");

        NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURIRDF_child,  &kRDF_child)),
                  "couldn't get resource");

    }
    ++gRefCnt;
}

RDFGenericBuilderImpl::~RDFGenericBuilderImpl(void)
{
    NS_IF_RELEASE(mRoot);
    if (mDB) {
        mDB->RemoveObserver(this);
        NS_RELEASE(mDB);
    }
    // NS_IF_RELEASE(mDocument) not refcounted

    --gRefCnt;
    if (gRefCnt == 0) {
        NS_RELEASE(kContainerAtom);
        NS_RELEASE(kItemContentsGeneratedAtom);

        NS_RELEASE(kIdAtom);
        NS_RELEASE(kOpenAtom);
        NS_RELEASE(kResourceAtom);

        NS_RELEASE(kContainmentAtom);

        NS_RELEASE(kNaturalOrderPosAtom);

        NS_RELEASE(kNC_Title);
        NS_RELEASE(kNC_Column);

        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
        NS_RELEASE(gNameSpaceManager);
    }

    if (mTimer)
    {
        mTimer->Cancel();
        NS_RELEASE(mTimer);
        mTimer = nsnull;
    }
}

////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(RDFGenericBuilderImpl);
NS_IMPL_RELEASE(RDFGenericBuilderImpl);

NS_IMETHODIMP
RDFGenericBuilderImpl::QueryInterface(REFNSIID iid, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFContentModelBuilderIID) ||
        iid.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIRDFContentModelBuilder*, this);
    }
    else if (iid.Equals(kIRDFObserverIID)) {
        *aResult = NS_STATIC_CAST(nsIRDFObserver*, this);
    }
    else if (iid.Equals(nsIDOMNodeObserver::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIDOMNodeObserver*, this);
    }
    else if (iid.Equals(nsIDOMElementObserver::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIDOMElementObserver*, this);
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }
    NS_ADDREF(this);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFContentModelBuilder methods

NS_IMETHODIMP
RDFGenericBuilderImpl::SetDocument(nsIRDFDocument* aDocument)
{
    // note: null now allowed, it indicates document going away

    mDocument = aDocument; // not refcounted
    if (aDocument)
    {
    	if (nsnull == mTimer)
    	{
		NS_VERIFY(NS_SUCCEEDED(NS_NewTimer(&mTimer)), "couldn't get timer");
		if (mTimer)
		{
			mTimer->Init(this, /* PR_TRUE, */ 1L);
		}
	}
    }
    else
    {
    	if (mTimer)
    	{
    		mTimer->Cancel();
    		NS_RELEASE(mTimer);
    		mTimer = nsnull;
    	}
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::SetDataBase(nsIRDFCompositeDataSource* aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(mDB == nsnull, "already initialized");
    if (mDB)
        return NS_ERROR_ALREADY_INITIALIZED;

    mDB = aDataBase;
    NS_ADDREF(mDB);

    mDB->AddObserver(this);
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::GetDataBase(nsIRDFCompositeDataSource** aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    *aDataBase = mDB;
    NS_ADDREF(mDB);
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::CreateRootContent(nsIRDFResource* aResource)
{
    // Create a structure that looks like this:
    //
    // <html:document>
    //    <html:body>
    //       <xul:rootwidget>
    //       </xul:rootwidget>
    //    </html:body>
    // </html:document>
    //
    // Eventually, we should be able to do away with the HTML tags and
    // just have it create XUL in some other context.

    nsresult rv;
    nsCOMPtr<nsIAtom>       tag;
    nsCOMPtr<nsIDocument>   doc;
    nsCOMPtr<nsIAtom>       rootAtom;
    if (NS_FAILED(rv = GetRootWidgetAtom(getter_AddRefs(rootAtom)))) {
        return rv;
    }

    if (NS_FAILED(rv = mDocument->QueryInterface(kIDocumentIID,
                                                 (void**) getter_AddRefs(doc))))
        return rv;

    // Create the DOCUMENT element
    if ((tag = dont_AddRef(NS_NewAtom("document"))) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIContent> root;
    if (NS_FAILED(rv = NS_NewRDFElement(kNameSpaceID_HTML, /* XXX */
                                        tag,
                                        getter_AddRefs(root))))
        return rv;

    doc->SetRootContent(NS_STATIC_CAST(nsIContent*, root));

    // Create the BODY element
    nsCOMPtr<nsIContent> body;
    if ((tag = dont_AddRef(NS_NewAtom("body"))) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = NS_NewRDFElement(kNameSpaceID_HTML, /* XXX */
                                        tag,
                                        getter_AddRefs(body))))
        return rv;


    // Attach the BODY to the DOCUMENT
    if (NS_FAILED(rv = root->AppendChildTo(body, PR_FALSE)))
        return rv;

    // Create the xul:rootwidget element, and indicate that children should
    // be recursively generated on demand
    nsCOMPtr<nsIContent> widget;
    if (NS_FAILED(rv = CreateResourceElement(kNameSpaceID_XUL,
                                             rootAtom,
                                             aResource,
                                             getter_AddRefs(widget))))
        return rv;

    if (NS_FAILED(rv = widget->SetAttribute(kNameSpaceID_RDF, kContainerAtom, "true", PR_FALSE)))
        return rv;

    // Attach the ROOTWIDGET to the BODY
    if (NS_FAILED(rv = body->AppendChildTo(widget, PR_FALSE)))
        return rv;

    return NS_OK;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::SetRootContent(nsIContent* aElement)
{
    NS_IF_RELEASE(mRoot);
    mRoot = aElement;
    NS_IF_ADDREF(mRoot);
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::CreateContents(nsIContent* aElement)
{
    nsresult rv;

    // First, make sure that the element is in the right widget -- ours.
    if (!IsElementInWidget(aElement))
        return NS_OK;

    // Next, see if the item is even "open". If not, then just
    // pretend it doesn't have _any_ contents. We check this _before_
    // checking the contents-generated attribute so that we don't
    // eagerly set contents-generated on a closed node.
    if (!IsOpen(aElement))
        return NS_OK;

    // Now see if someone has marked the element's contents as being
    // generated: this prevents a re-entrant call from triggering
    // another generation.
    nsAutoString attrValue;
    if (NS_FAILED(rv = aElement->GetAttribute(kNameSpaceID_None,
                                              kItemContentsGeneratedAtom,
                                              attrValue))) {
        NS_ERROR("unable to test contents-generated attribute");
        return rv;
    }

    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (attrValue.EqualsIgnoreCase("true")))
        return NS_OK;

    // Now mark the element's contents as being generated so that
    // any re-entrant calls don't trigger an infinite recursion.
    if (NS_FAILED(rv = aElement->SetAttribute(kNameSpaceID_None,
                                              kItemContentsGeneratedAtom,
                                              "true",
                                              PR_FALSE))) {
        NS_ERROR("unable to set contents-generated attribute");
        return rv;
    }

    // Get the element's resource so that we can generate cell
    // values. We could QI for the nsIRDFResource here, but doing this
    // via the nsIContent interface allows us to support generic nodes
    // that might get added in by DOM calls.
    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = GetElementResource(aElement, getter_AddRefs(resource)))) {
        NS_ERROR("unable to get element resource");
        return rv;
    }

    // XXX Eventually, we may want to factor this into one method that
    // handles RDF containers (RDF:Bag, et al.) and another that
    // handles multi-attributes. For performance...

    // Create a cursor that'll enumerate all of the outbound arcs
    nsCOMPtr<nsIRDFArcsOutCursor> properties;
    if (NS_FAILED(rv = mDB->ArcLabelsOut(resource, getter_AddRefs(properties))))
        return rv;

// rjc - sort
	nsVoidArray	*tempArray;
        if ((tempArray = new nsVoidArray()) == nsnull)	return (NS_ERROR_OUT_OF_MEMORY);

    while (1) {
        rv = properties->Advance();
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_RDF_CURSOR_EMPTY)
            break;

        nsCOMPtr<nsIRDFResource> property;

        if (NS_FAILED(rv = properties->GetLabel(getter_AddRefs(property))))
            break;

        // If it's not a widget item property, then it doesn't specify an
        // object that is member of the current container element;
        // rather, it specifies a "simple" property of the container
        // element. Skip it.
        if (!IsContainmentProperty(aElement, property))
            continue;

        // Create a second cursor that'll enumerate all of the values
        // for all of the arcs.
        nsCOMPtr<nsIRDFAssertionCursor> assertions;
        if (NS_FAILED(rv = mDB->GetTargets(resource, property, PR_TRUE, getter_AddRefs(assertions)))) {
            NS_ERROR("unable to get targets for property");
            return rv;
        }

        while (1) {
            rv = assertions->Advance();
            if (NS_FAILED(rv))
                return rv;

            if (rv == NS_RDF_CURSOR_EMPTY)
                break;

            nsCOMPtr<nsIRDFNode> value;
            if (NS_FAILED(rv = assertions->GetValue(getter_AddRefs(value)))) {
                NS_ERROR("unable to get cursor value");
                // return rv;
                break;
            }

            nsCOMPtr<nsIRDFResource> valueResource;
            if (NS_SUCCEEDED(value->QueryInterface(kIRDFResourceIID, (void**) getter_AddRefs(valueResource)))
                && IsContainmentProperty(aElement, property)) {
			/* XXX hack: always append value resource 1st!
			       due to sort callback implementation */
                	tempArray->AppendElement(valueResource);
                	tempArray->AppendElement(property);
            }
            else {
               /* if (NS_FAILED(rv = SetCellValue(aElement, property, value))) {
                    NS_ERROR("unable to set cell value");
                    // return rv;
                    break;
                XXX Dave - WHY IS THIS HERE? 
                }*/
            }
        }
    }

        unsigned long numElements = tempArray->Count();
        if (numElements > 0)
        {
        	nsIRDFResource ** flatArray = new nsIRDFResource *[numElements];
        	if (flatArray)
        	{
			// flatten array of resources, sort them, then add as item elements
			unsigned long loop;

        	        for (loop=0; loop<numElements; loop++)
				flatArray[loop] = (nsIRDFResource *)tempArray->ElementAt(loop);

			nsIXULSortService		*gXULSortService = nsnull;

			nsresult rv = nsServiceManager::GetService(kXULSortServiceCID,
				kIXULSortServiceIID, (nsISupports**) &gXULSortService);
			if (nsnull != gXULSortService)
			{
				gXULSortService->OpenContainer(mDB, aElement, flatArray, numElements/2, 2*sizeof(nsIRDFResource *));
				nsServiceManager::ReleaseService(kXULSortServiceCID, gXULSortService);
			}

        		for (loop=0; loop<numElements; loop+=2)
        		{
				if (NS_FAILED(rv = AddWidgetItem(aElement, flatArray[loop+1], flatArray[loop], loop+1)))
				{
					NS_ERROR("unable to create widget item");
				}
        		}
			delete [] flatArray;
        	}
        }
        delete tempArray;

    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OnAssert(nsIRDFResource* aSubject,
                                nsIRDFResource* aPredicate,
                                nsIRDFNode* aObject)
{
    NS_PRECONDITION(mDocument != nsnull, "not initialized");
    if (! mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    if (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(elements)))) {
        NS_ERROR("unable to create new ISupportsArray");
        return rv;
    }

    // Find all the elements in the content model that correspond to
    // aSubject: for each, we'll try to build XUL children if
    // appropriate.
    if (NS_FAILED(rv = mDocument->GetElementsForResource(aSubject, elements))) {
        NS_ERROR("unable to retrieve elements from resource");
        return rv;
    }

    for (PRInt32 i = elements->Count() - 1; i >= 0; --i) {
        nsCOMPtr<nsIContent> element( do_QueryInterface(elements->ElementAt(i)) );
        
        // XXX somehow figure out if building XUL kids on this
        // particular element makes any sense whatsoever.

        // We'll start by making sure that the element at least has
        // the same parent has the content model builder's root
        if (!IsElementInWidget(element))
            continue;
        
        nsCOMPtr<nsIRDFResource> resource;
        if (NS_SUCCEEDED(aObject->QueryInterface(kIRDFResourceIID,
                                                 (void**) getter_AddRefs(resource)))
            && IsContainmentProperty(element, aPredicate)) {
            // Okay, the object _is_ a resource, and the predicate is
            // a containment property. So this'll be a new item in the widget
            // control.

            // But if the contents of aElement _haven't_ yet been
            // generated, then just ignore the assertion. We do this
            // because we know that _eventually_ the contents will be
            // generated (via CreateContents()) when somebody asks for
            // them later.
            nsAutoString contentsGenerated;
            if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_None,
                                                     kItemContentsGeneratedAtom,
                                                     contentsGenerated))) {
                NS_ERROR("severe problem trying to get attribute");
                return rv;
            }

            if ((rv == NS_CONTENT_ATTR_HAS_VALUE) &&
                contentsGenerated.EqualsIgnoreCase("true")) {
                // Okay, it's a "live" element, so go ahead and append the new
                // child to this node.
                if (NS_FAILED(rv = AddWidgetItem(element, aPredicate, resource, 0))) {
                    NS_ERROR("unable to create new widget item");
                    return rv;
                }
            }
        }
        else {
            // Either the object of the assertion is not a resource,
            // or the object is a resource and the predicate is not a
            // tree property. So this won't be a new row in the
            // table. See if we can use it to set a cell value on the
            // current element.
            /* XXX: WHAT THE HECK IS THIS HERE FOR? - Dave
            if (NS_FAILED(rv = SetCellValue(element, aPredicate, aObject))) {
                NS_ERROR("unable to set cell value");
                return rv;
            }*/
        }
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OnUnassert(nsIRDFResource* aSubject,
                                  nsIRDFResource* aPredicate,
                                  nsIRDFNode* aObject)
{
    NS_PRECONDITION(mDocument != nsnull, "not initialized");
    if (! mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    if (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(elements)))) {
        NS_ERROR("unable to create new ISupportsArray");
        return rv;
    }

    // Find all the elements in the content model that correspond to
    // aSubject: for each, we'll try to build XUL children if
    // appropriate.
    if (NS_FAILED(rv = mDocument->GetElementsForResource(aSubject, elements))) {
        NS_ERROR("unable to retrieve elements from resource");
        return rv;
    }

    for (PRInt32 i = elements->Count() - 1; i >= 0; --i) {
        nsCOMPtr<nsIContent> element( do_QueryInterface(elements->ElementAt(i)) );
        
        // XXX somehow figure out if building XUL kids on this
        // particular element makes any sense whatsoever.

        // We'll start by making sure that the element at least has
        // the same parent has the content model builder's root
        if (!IsElementInWidget(element))
            continue;
        
        nsCOMPtr<nsIRDFResource> resource;
        if (NS_SUCCEEDED(aObject->QueryInterface(kIRDFResourceIID,
                                                 (void**) getter_AddRefs(resource)))
            && IsContainmentProperty(element, aPredicate)) {
            // Okay, the object _is_ a resource, and the predicate is
            // a containment property. So this'll be a new item in the widget
            // control.

            // But if the contents of aElement _haven't_ yet been
            // generated, then just ignore the unassertion: nothing is
            // in the content model to remove.
            nsAutoString contentsGenerated;
            if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_None,
                                                     kItemContentsGeneratedAtom,
                                                     contentsGenerated))) {
                NS_ERROR("severe problem trying to get attribute");
                return rv;
            }

            if ((rv == NS_CONTENT_ATTR_HAS_VALUE) &&
                contentsGenerated.EqualsIgnoreCase("true")) {
                // Okay, it's a "live" element, so go ahead and append the new
                // child to this node.
                if (NS_FAILED(rv = RemoveWidgetItem(element, aPredicate, resource))) {
                    NS_ERROR("unable to create new widget item");
                    return rv;
                }
            }
        }
        else {
            // Either the object of the assertion is not a resource,
            // or the object is a resource and the predicate is not a
            // tree property. So this won't be a new row in the
            // table. See if we can use it to set a cell value on the
            // current element.
            /* XXX: WHAT THE HECK IS THIS HERE FOR? - Dave
            if (NS_FAILED(rv = SetCellValue(element, aPredicate, aObject))) {
                NS_ERROR("unable to set cell value");
                return rv;
            }*/
        }
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIDOMNodeObserver interface
//
//   XXX Any of these methods that can't be implemented in a generic
//   way should become pure virtual on this class.
//

NS_IMETHODIMP
RDFGenericBuilderImpl::OnSetNodeValue(nsIDOMNode* aNode, const nsString& aValue)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::OnInsertBefore(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aRefChild)
{
    return NS_OK;
}



NS_IMETHODIMP
RDFGenericBuilderImpl::OnReplaceChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aOldChild)
{
    return NS_OK;
}



NS_IMETHODIMP
RDFGenericBuilderImpl::OnRemoveChild(nsIDOMNode* aParent, nsIDOMNode* aOldChild)
{
    return NS_OK;
}



NS_IMETHODIMP
RDFGenericBuilderImpl::OnAppendChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild)
{
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIDOMElementObserver interface


NS_IMETHODIMP
RDFGenericBuilderImpl::OnSetAttribute(nsIDOMElement* aElement, const nsString& aName, const nsString& aValue)
{
    nsresult rv;

    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = GetDOMNodeResource(aElement, getter_AddRefs(resource)))) {
        // XXX it's not a resource element, so there's no assertions
        // we need to make on the back-end. Should we just do the
        // update?
        return NS_OK;
    }

    // Get the nsIContent interface, it's a bit more utilitarian
    nsCOMPtr<nsIContent> element( do_QueryInterface(aElement) );
    if (! element) {
        NS_ERROR("element doesn't support nsIContent");
        return NS_ERROR_UNEXPECTED;
    }

    // Make sure that the element is in the widget. XXX Even this may be
    // a bit too promiscuous: an element may also be a XUL element...
    if (!IsElementInWidget(element))
        return NS_OK;

    // Split the element into its namespace and tag components
    PRInt32  elementNameSpaceID;
    if (NS_FAILED(rv = element->GetNameSpaceID(elementNameSpaceID))) {
        NS_ERROR("unable to get element namespace ID");
        return rv;
    }

    nsCOMPtr<nsIAtom> elementNameAtom;
    if (NS_FAILED(rv = element->GetTag( *getter_AddRefs(elementNameAtom) ))) {
        NS_ERROR("unable to get element tag");
        return rv;
    }

    // Split the property name into its namespace and tag components
    PRInt32  attrNameSpaceID;
    nsCOMPtr<nsIAtom> attrNameAtom;
    if (NS_FAILED(rv = element->ParseAttributeString(aName, *getter_AddRefs(attrNameAtom), attrNameSpaceID))) {
        NS_ERROR("unable to parse attribute string");
        return rv;
    }

    // Now do the work to change the attribute. There are a couple of
    // special cases that we need to check for here...
    if ((elementNameSpaceID == kNameSpaceID_XUL) &&
        IsItemOrFolder(element) && // XXX IsItemOrFolder(): is this what we really mean?
        (attrNameSpaceID    == kNameSpaceID_None) &&
        (attrNameAtom.get() == kOpenAtom)) {

        // We are (possibly) changing the value of the "open"
        // attribute. This may require us to generate or destroy
        // content in the widget. See what the old value was...
        nsAutoString attrValue;
        if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_None, kOpenAtom, attrValue))) {
            NS_ERROR("unable to get current open state");
            return rv;
        }

        if ((rv == NS_CONTENT_ATTR_NO_VALUE) || (rv == NS_CONTENT_ATTR_NOT_THERE) ||
            ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (! attrValue.EqualsIgnoreCase(aValue))) ||
            PR_TRUE // XXX just always allow this to fire.
            ) {
            // Okay, it's really changing.

            // This is a "transient" property, so we _don't_ go to the
            // RDF graph to set it.
            if (NS_FAILED(rv = element->SetAttribute(kNameSpaceID_None, kOpenAtom, aValue, PR_TRUE))) {
                NS_ERROR("unable to update attribute on content node");
                return rv;
            }

            if (aValue.EqualsIgnoreCase("true")) {
                rv = OpenWidgetItem(element);
            }
            else {
                rv = CloseWidgetItem(element);
            }

            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to open/close tree item");
            return rv;
        }
    }
    else if ((elementNameSpaceID == kNameSpaceID_XUL) &&
             (IsItemOrFolder(element) || // XXX IsItemOrFolder(): is this what we really mean?
              IsWidgetInsertionRootElement(element)) &&
             (attrNameSpaceID    == kNameSpaceID_None) &&
             (attrNameAtom.get() == kIdAtom)) {

        // We are (possibly) changing the actual identity of the
        // element; e.g., re-rooting an item in the tree.

        nsCOMPtr<nsIRDFResource> newResource;
        if (NS_FAILED(rv = gRDFService->GetUnicodeResource(aValue, getter_AddRefs(newResource)))) {
            NS_ERROR("unable to get new resource");
            return rv;
        }

#if 0 // XXX we're fighting with the XUL builder, so just _always_ let this through.
        // Didn't change. So bail!
        if (resource == newResource)
            return NS_OK;
#endif

        // Allright, it really is changing. So blow away the old
        // content node and insert a new one with the new ID in
        // its place.
        nsCOMPtr<nsIContent> parent;
        if (NS_FAILED(rv = element->GetParent(*getter_AddRefs(parent)))) {
            NS_ERROR("unable to get element's parent");
            return rv;
        }

        PRInt32 index;
        if (NS_FAILED(rv = parent->IndexOf(element, index))) {
            NS_ERROR("unable to get element's index within parent");
            return rv;
        }

        if (NS_FAILED(rv = parent->RemoveChildAt(index, PR_TRUE))) {
            NS_ERROR("unable to remove element");
            return rv;
        }

        nsCOMPtr<nsIContent> newElement;
        if (NS_FAILED(rv = CreateResourceElement(elementNameSpaceID,
                                                 elementNameAtom,
                                                 newResource,
                                                 getter_AddRefs(newElement)))) {
            NS_ERROR("unable to create new element");
            return rv;
        }

        // Attach transient properties to the new element.
        //
        // XXX all I really care about right this minute is the
        // "open" state. We could put this stuff in a table and
        // drive it that way.
        nsAutoString attrValue;
        if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_None, kOpenAtom, attrValue))) {
            NS_ERROR("unable to get open state of old element");
            return rv;
        }

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            if (NS_FAILED(rv = newElement->SetAttribute(kNameSpaceID_None, kOpenAtom, attrValue, PR_FALSE))) {
                NS_ERROR("unable to set open state of new element");
                return rv;
            }
        }

        // Mark as a container so the contents get regenerated
        if (NS_FAILED(rv = newElement->SetAttribute(kNameSpaceID_RDF,
                                                    kContainerAtom,
                                                    "true",
                                                    PR_FALSE))) {
            NS_ERROR("unable to mark as a container");
            return rv;
        }

        // Now insert the new element into the parent. This should
        // trigger a reflow and cause the contents to be regenerated.
        if (NS_FAILED(rv = parent->InsertChildAt(newElement, index, PR_TRUE))) {
            NS_ERROR("unable to add new element to the parent");
            return rv;
        }

        return NS_OK;
    }

    // If we get here, it's a "vanilla" property: push its value into the graph.
    nsCOMPtr<nsIRDFResource> property;
    if (NS_FAILED(rv = GetResource(attrNameSpaceID, attrNameAtom, getter_AddRefs(property)))) {
        NS_ERROR("unable to construct resource");
        return rv;
    }

    // Unassert the old value, if there was one.
    nsAutoString oldValue;
    if (NS_CONTENT_ATTR_HAS_VALUE == element->GetAttribute(attrNameSpaceID, attrNameAtom, oldValue)) {
        nsCOMPtr<nsIRDFLiteral> value;
        if (NS_FAILED(rv = gRDFService->GetLiteral(oldValue, getter_AddRefs(value)))) {
            NS_ERROR("unable to construct literal");
            return rv;
        }

        rv = mDB->Unassert(resource, property, value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to unassert old property value");
    }

    // Assert the new value
    {
        nsCOMPtr<nsIRDFLiteral> value;
        if (NS_FAILED(rv = gRDFService->GetLiteral(aValue, getter_AddRefs(value)))) {
            NS_ERROR("unable to construct literal");
            return rv;
        }

        rv = mDB->Assert(resource, property, value, PR_TRUE);
        NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to assert new property value");
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::OnRemoveAttribute(nsIDOMElement* aElement, const nsString& aName)
{
    nsresult rv;

    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = GetDOMNodeResource(aElement, getter_AddRefs(resource)))) {
        // XXX it's not a resource element, so there's no assertions
        // we need to make on the back-end. Should we just do the
        // update?
        return NS_OK;
    }

    // Get the nsIContent interface, it's a bit more utilitarian
    nsCOMPtr<nsIContent> element( do_QueryInterface(aElement) );
    if (! element) {
        NS_ERROR("element doesn't support nsIContent");
        return NS_ERROR_UNEXPECTED;
    }

    // Make sure that the element is in the widget. XXX Even this may be
    // a bit too promiscuous: an element may also be a XUL element...
    if (!IsElementInWidget(element))
        return NS_OK;

    // Split the element into its namespace and tag components
    PRInt32  elementNameSpaceID;
    if (NS_FAILED(rv = element->GetNameSpaceID(elementNameSpaceID))) {
        NS_ERROR("unable to get element namespace ID");
        return rv;
    }

    nsCOMPtr<nsIAtom> elementNameAtom;
    if (NS_FAILED(rv = element->GetTag( *getter_AddRefs(elementNameAtom) ))) {
        NS_ERROR("unable to get element tag");
        return rv;
    }

    // Split the property name into its namespace and tag components
    PRInt32  attrNameSpaceID;
    nsCOMPtr<nsIAtom> attrNameAtom;
    if (NS_FAILED(rv = element->ParseAttributeString(aName, *getter_AddRefs(attrNameAtom), attrNameSpaceID))) {
        NS_ERROR("unable to parse attribute string");
        return rv;
    }

    if ((elementNameSpaceID    == kNameSpaceID_XUL) &&
        IsItemOrFolder(element) && // XXX Is this what we really mean?
        (attrNameSpaceID       == kNameSpaceID_None) &&
        (attrNameAtom.get()    == kOpenAtom)) {
        // We are removing the value of the "open" attribute. This may
        // require us to destroy content from the tree.

        // XXX should we check for existence of the attribute first?
        if (NS_FAILED(rv = element->UnsetAttribute(kNameSpaceID_None, kOpenAtom, PR_TRUE))) {
            NS_ERROR("unable to attribute on update content node");
            return rv;
        }

        if (NS_FAILED(rv = CloseWidgetItem(element))) {
            NS_ERROR("unable to close widget item");
            return rv;
        }
    }
    else {
        // It's a "vanilla" property: push its value into the graph.

        nsCOMPtr<nsIRDFResource> property;
        if (NS_FAILED(rv = GetResource(attrNameSpaceID, attrNameAtom, getter_AddRefs(property)))) {
            NS_ERROR("unable to construct resource");
            return rv;
        }

        // Unassert the old value, if there was one.
        nsAutoString oldValue;
        if (NS_CONTENT_ATTR_HAS_VALUE == element->GetAttribute(attrNameSpaceID, attrNameAtom, oldValue)) {
            nsCOMPtr<nsIRDFLiteral> value;
            if (NS_FAILED(rv = gRDFService->GetLiteral(oldValue, getter_AddRefs(value)))) {
                NS_ERROR("unable to construct literal");
                return rv;
            }

            rv = mDB->Unassert(resource, property, value);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to unassert old property value");
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::OnSetAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aNewAttr)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::OnRemoveAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aOldAttr)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
RDFGenericBuilderImpl::FindChildByTag(nsIContent* aElement,
                                   PRInt32 aNameSpaceID,
                                   nsIAtom* aTag,
                                   nsIContent** aChild)
{
    nsresult rv;

    PRInt32 count;
    if (NS_FAILED(rv = aElement->ChildCount(count)))
        return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aElement->ChildAt(i, *getter_AddRefs(kid))))
            return rv; // XXX fatal

        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID != aNameSpaceID)
            continue; // wrong namespace

        nsCOMPtr<nsIAtom> kidTag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(kidTag))))
            return rv; // XXX fatal

        if (kidTag.get() != aTag)
            continue;

        *aChild = kid;
        NS_ADDREF(*aChild);
        return NS_OK;
    }

    return NS_RDF_NO_VALUE; // not found
}


nsresult
RDFGenericBuilderImpl::FindChildByTagAndResource(nsIContent* aElement,
                                              PRInt32 aNameSpaceID,
                                              nsIAtom* aTag,
                                              nsIRDFResource* aResource,
                                              nsIContent** aChild)
{
    nsresult rv;

    PRInt32 count;
    if (NS_FAILED(rv = aElement->ChildCount(count)))
        return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aElement->ChildAt(i, *getter_AddRefs(kid))))
            return rv; // XXX fatal

        // Make sure it's a <xul:treecell>
        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID != aNameSpaceID)
            continue; // wrong namespace

        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(tag))))
            return rv; // XXX fatal

        if (tag.get() != aTag)
            continue; // wrong tag

        // Now get the resource ID from the RDF:ID attribute. We do it
        // via the content model, because you're never sure who
        // might've added this stuff in...
        nsCOMPtr<nsIRDFResource> resource;
        if (NS_FAILED(rv = GetElementResource(kid, getter_AddRefs(resource)))) {
            NS_ERROR("severe error retrieving resource");
            return rv;
        }

        if (resource.get() != aResource)
            continue; // not the resource we want

        // Fount it!
        *aChild = kid;
        NS_ADDREF(*aChild);
        return NS_OK;
    }

    return NS_RDF_NO_VALUE; // not found
}


nsresult
RDFGenericBuilderImpl::EnsureElementHasGenericChild(nsIContent* parent,
                                                 PRInt32 nameSpaceID,
                                                 nsIAtom* tag,
                                                 nsIContent** result)
{
    nsresult rv;

    rv = FindChildByTag(parent, nameSpaceID, tag, result);
    if (NS_FAILED(rv))
        return rv;

    if (rv == NS_RDF_NO_VALUE) {
        // we need to construct a new child element.
        nsCOMPtr<nsIContent> element;

        if (NS_FAILED(rv = NS_NewRDFElement(nameSpaceID, tag, getter_AddRefs(element))))
            return rv;

        if (NS_FAILED(rv = parent->AppendChildTo(element, PR_FALSE)))
            return rv;

        *result = element;
        NS_ADDREF(*result);
    }
    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::FindWidgetRootElement(nsIContent* aElement,
                                             nsIContent** aWidgetElement)
{
    nsresult rv;

    nsCOMPtr<nsIAtom>       rootAtom;
    if (NS_FAILED(rv = GetRootWidgetAtom(getter_AddRefs(rootAtom)))) {
        return rv;
    }

    // walk up the tree until you find rootAtom
    nsCOMPtr<nsIContent> element(do_QueryInterface(aElement));

    while (element) {
        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = element->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID == kNameSpaceID_XUL) {
            nsCOMPtr<nsIAtom> tag;
            if (NS_FAILED(rv = element->GetTag(*getter_AddRefs(tag))))
                return rv;

            if (tag.get() == rootAtom) {
                *aWidgetElement = element;
                NS_ADDREF(*aWidgetElement);
                return NS_OK;
            }
        }

        // up to the parent...
        nsCOMPtr<nsIContent> parent;
        element->GetParent(*getter_AddRefs(parent));
        element = parent;
    }

    //NS_ERROR("must not've started from within the XUL widget");
    return NS_ERROR_FAILURE;
}


PRBool
RDFGenericBuilderImpl::IsItemOrFolder(nsIContent* aElement)
{
    // XXX It seems like this should be a pure virtual method that
    // subclasses implement?

    // XXX All of the callers of this method actually only seem to
    // care if the element is a "folder".

    // Returns PR_TRUE if the element is an "item" or a "folder" in
    // the widget.
    nsresult rv;

    nsCOMPtr<nsIAtom> itemAtom;
    nsCOMPtr<nsIAtom> folderAtom;
    if (NS_FAILED(rv = GetWidgetItemAtom(getter_AddRefs(itemAtom))) ||
        NS_FAILED(rv = GetWidgetFolderAtom(getter_AddRefs(folderAtom)))) {
        return rv;
    }

    // "element" must be itemAtom
    nsCOMPtr<nsIAtom> tag;
    if (NS_FAILED(rv = aElement->GetTag(*getter_AddRefs(tag))))
        return PR_FALSE;

    if (tag.get() != itemAtom && tag.get() != folderAtom)
        return PR_FALSE;

    return PR_TRUE;
}

PRBool
RDFGenericBuilderImpl::IsWidgetInsertionRootElement(nsIContent* element)
{
    // Returns PR_TRUE if the element is the root point to insert new items.
    nsresult rv;

    nsCOMPtr<nsIAtom> rootAtom;
    if (NS_FAILED(rv = GetInsertionRootAtom(getter_AddRefs(rootAtom)))) {
        return rv;
    }

    PRInt32 nameSpaceID;
    if (NS_FAILED(rv = element->GetNameSpaceID(nameSpaceID))) {
        NS_ERROR("unable to get namespace ID");
        return PR_FALSE;
    }

    if (nameSpaceID != kNameSpaceID_XUL)
        return PR_FALSE; // not a XUL element

    nsCOMPtr<nsIAtom> elementTag;
    if (NS_FAILED(rv = element->GetTag(*getter_AddRefs(elementTag)))) {
        NS_ERROR("unable to get tag");
        return PR_FALSE;
    }

    if (elementTag.get() != rootAtom)
        return PR_FALSE; // not the place to insert a child

    return PR_TRUE;
}

PRBool
RDFGenericBuilderImpl::IsContainmentProperty(nsIContent* aElement, nsIRDFResource* aProperty)
{
    // XXX is this okay to _always_ treat ordinal properties as tree
    // properties? Probably not...
    if (rdf_IsOrdinalProperty(aProperty))
        return PR_TRUE;

    nsresult rv;
    nsXPIDLCString propertyURI;
    if (NS_FAILED(rv = aProperty->GetValue( getter_Copies(propertyURI) ))) {
        NS_ERROR("unable to get property URI");
        return PR_FALSE;
    }

    nsAutoString uri;

    nsCOMPtr<nsIContent> element( do_QueryInterface(aElement) );
    while (element) {
        // Never ever ask an HTML element about non-HTML attributes.
        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = element->GetNameSpaceID(nameSpaceID))) {
            NS_ERROR("unable to get element namespace");
            PR_FALSE;
        }

        if (nameSpaceID != kNameSpaceID_HTML) {
            // Is this the "rdf:containment? property?
            if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_None, kContainmentAtom, uri))) {
                NS_ERROR("unable to get attribute value");
                return PR_FALSE;
            }

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                // Okay we've found the locally specified tree properties,
                // a whitespace-separated list of property URIs. So we
                // definitively know whether this is a tree property or
                // not.
                if (uri.Find(propertyURI) >= 0)
                    return PR_TRUE;
                else
                    return PR_FALSE;
            }
        }

        nsCOMPtr<nsIContent> parent;
        element->GetParent(*getter_AddRefs(parent));
        element = parent;
    }

    // If we get here, we didn't find any tree property: so now
    // defaults start to kick in.

#define TREE_PROPERTY_HACK
#if defined(TREE_PROPERTY_HACK)
    if ((aProperty == kNC_child) ||
        (aProperty == kNC_Folder) ||
        (aProperty == kRDF_child)) {
        return PR_TRUE;
    }
    else
#endif // defined(TREE_PROPERTY_HACK)

        return PR_FALSE;
}

PRBool
RDFGenericBuilderImpl::IsContainer(nsIContent* aElement, nsIRDFResource* aResource)
{
    PRBool result = PR_FALSE;
    nsCOMPtr<nsIRDFArcsOutCursor> arcs;
    if (NS_FAILED(mDB->ArcLabelsOut(aResource, getter_AddRefs(arcs)))) {
        NS_ERROR("unable to get arcs out");
        return result;
    }

    while (1) {
        nsresult rv = arcs->Advance();
        NS_ASSERTION(NS_SUCCEEDED(rv), "severe error advancing cursor");
        if (NS_FAILED(rv))
            return PR_FALSE;

        if (rv == NS_RDF_CURSOR_EMPTY)
            break;

        nsCOMPtr<nsIRDFResource> property;
        if (NS_FAILED(arcs->GetLabel(getter_AddRefs(property)))) {
            NS_ERROR("unable to get cursor value");
            return result;
        }

        // Ignore properties that are used to indicate "tree-ness"
        if (IsContainmentProperty(aElement, property))
            return PR_TRUE;
    }
    return result;
}

PRBool
RDFGenericBuilderImpl::IsOpen(nsIContent* aElement)
{
    nsresult rv;

    // needs to be a the valid insertion root or an item to begin with.
    PRInt32 nameSpaceID;
    if (NS_FAILED(rv = aElement->GetNameSpaceID(nameSpaceID))) {
        NS_ERROR("unable to get namespace ID");
        return PR_FALSE;
    }

    if (nameSpaceID != kNameSpaceID_XUL)
        return PR_FALSE;

    nsCOMPtr<nsIAtom> tag;
    if (NS_FAILED(rv = aElement->GetTag( *getter_AddRefs(tag) ))) {
        NS_ERROR("unable to get element's tag");
        return PR_FALSE;
    }

    nsCOMPtr<nsIAtom>       insertionAtom;
    if (NS_FAILED(rv = GetInsertionRootAtom(getter_AddRefs(insertionAtom)))) {
        return rv;
    }

    nsCOMPtr<nsIAtom>       folderAtom;
    if (NS_FAILED(rv = GetWidgetFolderAtom(getter_AddRefs(folderAtom)))) {
        return rv;
    }

    // The insertion root is _always_ open.
    if (tag.get() == insertionAtom)
        return PR_TRUE;

    // If it's not a widget folder item, then it's not open.
    if (tag.get() != folderAtom)
        return PR_FALSE;

    nsAutoString value;
    if (NS_FAILED(rv = aElement->GetAttribute(kNameSpaceID_None, kOpenAtom, value))) {
        NS_ERROR("unable to get open attribute");
        return rv;
    }

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        if (value.EqualsIgnoreCase("true"))
            return PR_TRUE;
    }

    return PR_FALSE;
}


PRBool
RDFGenericBuilderImpl::IsElementInWidget(nsIContent* aElement)
{
    // Make sure that we're actually creating content for the tree
    // content model that we've been assigned to deal with.
    nsCOMPtr<nsIContent> rootElement;
    if (NS_FAILED(FindWidgetRootElement(aElement, getter_AddRefs(rootElement))))
        return PR_FALSE;

    nsCOMPtr<nsIAtom> rootTag;
    nsCOMPtr<nsIAtom> elementTag;
    mRoot->GetTag(*getter_AddRefs(rootTag));
    rootElement->GetTag(*getter_AddRefs(elementTag));

    if (rootTag.get() != elementTag.get())
        return PR_FALSE;

    return PR_TRUE;
}

nsresult
RDFGenericBuilderImpl::GetDOMNodeResource(nsIDOMNode* aNode, nsIRDFResource** aResource)
{
    nsresult rv;

    // Given an nsIDOMNode that presumably has been created as a proxy
    // for an RDF resource, pull the RDF resource information out of
    // it.

    nsCOMPtr<nsIContent> element;
    if (NS_FAILED(rv = aNode->QueryInterface(kIContentIID, getter_AddRefs(element) ))) {
        NS_ERROR("DOM element doesn't support nsIContent");
        return rv;
    }

    return GetElementResource(element, aResource);
}

nsresult
RDFGenericBuilderImpl::CreateResourceElement(PRInt32 aNameSpaceID,
                                             nsIAtom* aTag,
                                             nsIRDFResource* aResource,
                                             nsIContent** aResult)
{
    nsresult rv;

    nsCOMPtr<nsIContent> result;
    if (NS_FAILED(rv = NS_NewRDFElement(aNameSpaceID, aTag, getter_AddRefs(result))))
        return rv;

    nsXPIDLCString uri;
    if (NS_FAILED(rv = aResource->GetValue( getter_Copies(uri) )))
        return rv;

    if (NS_FAILED(rv = result->SetAttribute(kNameSpaceID_None, kIdAtom, (const char*) uri, PR_FALSE)))
        return rv;

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}



nsresult
RDFGenericBuilderImpl::GetResource(PRInt32 aNameSpaceID,
                                   nsIAtom* aNameAtom,
                                   nsIRDFResource** aResource)
{
    NS_PRECONDITION(aNameAtom != nsnull, "null ptr");
    if (! aNameAtom)
        return NS_ERROR_NULL_POINTER;

    // XXX should we allow nodes with no namespace???
    NS_PRECONDITION(aNameSpaceID != kNameSpaceID_Unknown, "no namespace");
    if (aNameSpaceID == kNameSpaceID_Unknown)
        return NS_ERROR_UNEXPECTED;

    // construct a fully-qualified URI from the namespace/tag pair.
    nsAutoString uri;
    gNameSpaceManager->GetNameSpaceURI(aNameSpaceID, uri);

    // XXX check to see if we need to insert a '/' or a '#'
    nsAutoString tag(aNameAtom->GetUnicode());
    if (0 < uri.Length() && uri.Last() != '#' && uri.Last() != '/' && tag.First() != '#')
        uri.Append('#');

    uri.Append(tag);

    nsresult rv = gRDFService->GetUnicodeResource(uri, aResource);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
    return rv;
}


nsresult
RDFGenericBuilderImpl::OpenWidgetItem(nsIContent* aElement)
{
    return CreateContents(aElement);
}


nsresult
RDFGenericBuilderImpl::CloseWidgetItem(nsIContent* aElement)
{
    nsresult rv;

    // Find the tag that contains the children so that we can remove all of
    // the children.
    nsCOMPtr<nsIAtom> parentAtom;
    if (NS_FAILED(rv = GetItemAtomThatContainsTheChildren(getter_AddRefs(parentAtom)))) {
        return rv;
    }

    nsCOMPtr<nsIContent> parentNode;
    nsCOMPtr<nsIAtom> tag;
    if (NS_FAILED(rv = aElement->GetTag(*getter_AddRefs(tag))))
        return rv; // XXX fatal

    if (tag.get() == parentAtom) {
        parentNode = dont_QueryInterface(aElement);
        rv = NS_OK;
    }
    else rv = FindChildByTag(aElement, kNameSpaceID_XUL, parentAtom, getter_AddRefs(parentNode));

    if (rv == NS_RDF_NO_VALUE) {
        // No tag; must've already been closed
        return NS_OK;
    }

    if (NS_FAILED(rv)) {
        NS_ERROR("severe error retrieving parent node for removal");
        return rv;
    }

    PRInt32 count;
    if (NS_FAILED(rv = parentNode->ChildCount(count))) {
        NS_ERROR("unable to get count of the parent's children");
        return rv;
    }

    while (--count >= 0) {
        // XXX This is a bit gross. We need to recursively remove any
        // subtrees before we remove the current subtree.
        nsIContent* child;
        if (NS_SUCCEEDED(rv = parentNode->ChildAt(count, child))) {
            rv = CloseWidgetItem(child);
            NS_RELEASE(child);
        }

        rv = parentNode->RemoveChildAt(count, PR_TRUE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error removing child");
    }

    // Clear the contents-generated attribute so that the next time we
    // come back, we'll regenerate the kids we just killed.
    if (NS_FAILED(rv = aElement->UnsetAttribute(kNameSpaceID_None,
                                                kItemContentsGeneratedAtom,
                                                PR_FALSE))) {
        NS_ERROR("unable to clear contents-generated attribute");
        return rv;
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::GetElementResource(nsIContent* aElement, nsIRDFResource** aResult)
{
    // Perform a reverse mapping from an element in the content model
    // to an RDF resource.
    nsresult rv;
    nsAutoString uri;
    if (NS_FAILED(rv = aElement->GetAttribute(kNameSpaceID_None,
                                              kIdAtom,
                                              uri))) {
        NS_ERROR("severe error retrieving attribute");
        return rv;
    }

    if (rv != NS_CONTENT_ATTR_HAS_VALUE) {
        NS_ERROR("widget element has no ID");
        return NS_ERROR_UNEXPECTED;
    }

    // Since the element will store its ID attribute as a document-relative value,
    // we may need to qualify it first...
    NS_ASSERTION(mDocument != nsnull, "builder has no document -- can't fully qualify URI");
    if (nsnull != mDocument) {
        nsCOMPtr<nsIDocument> doc( do_QueryInterface(mDocument) );
        NS_ASSERTION(doc, "doesn't support nsIDocument");
        if (doc) {
          nsIURL* docURL = nsnull;
          doc->GetBaseURL(docURL);
          if (docURL) {
              const char* url;
              docURL->GetSpec(&url);
              rdf_PossiblyMakeAbsolute(url, uri);
              NS_RELEASE(docURL);
          }
        }
    }

    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(uri, aResult))) {
        NS_ERROR("unable to create resource");
        return rv;
    }

    return NS_OK;
}
