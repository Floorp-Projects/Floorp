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

  1) We have a serious problem if all the columns aren't created by
     the time that we start inserting rows into the table. Need to fix
     this so that columns can be dynamically added and removed (we
     need to do this anyway to handle column re-ordering or
     manipulation via DOM calls).

 */

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMElementObserver.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeObserver.h"
#include "nsIDOMXULTreeElement.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "rdf.h"
#include "rdfutil.h"
#include "nsITimer.h"
#include "nsVoidArray.h"
#include "nsQuickSort.h"
#include "nsRDFGenericBuilder.h"
#include "prtime.h"
#include "prlog.h"
#include "nsIXULSortService.h"

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

static NS_DEFINE_IID(kIDomXulElementIID,          NS_IDOMXULELEMENT_IID);

static NS_DEFINE_IID(kXULSortServiceCID,          NS_XULSORTSERVICE_CID);
static NS_DEFINE_IID(kIXULSortServiceIID,         NS_IXULSORTSERVICE_IID);

#define	BUILDER_NOTIFY_MINIMUM_TIMEOUT	5000L

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog;
#endif

////////////////////////////////////////////////////////////////////////

class RDFTreeBuilderImpl : public RDFGenericBuilderImpl
{
public:
    RDFTreeBuilderImpl();
    virtual ~RDFTreeBuilderImpl();

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDataBase(nsIRDFCompositeDataSource* aDataBase);

    // nsIDOMNodeObserver interface
    NS_IMETHOD OnAppendChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild);
    NS_IMETHOD OnRemoveChild(nsIDOMNode* aParent, nsIDOMNode* aOldChild);

    // Implementation methods
    nsresult
    AddWidgetItem(nsIContent* aTreeItemElement,
                  nsIRDFResource* aProperty,
                  nsIRDFResource* aValue, PRInt32 aNaturalOrderPos);

    nsresult
    RemoveWidgetItem(nsIContent* aElement,
                     nsIRDFResource* aProperty,
                     nsIRDFResource* aValue);

    nsresult
    SetWidgetAttribute(nsIContent* aTreeItemElement,
                       nsIRDFResource* aProperty,
                       nsIRDFNode* aValue);

    nsresult
    UnsetWidgetAttribute(nsIContent* aTreeItemElement,
                         nsIRDFResource* aProperty,
                         nsIRDFNode* aValue);

    nsresult
    FindTextElement(nsIContent* aElement,
                    nsITextContent** aResult);

    nsresult
    EnsureCell(nsIContent* aTreeItemElement, PRInt32 aIndex, nsIContent** aCellElement);

    nsresult
    CreateTreeItemCells(nsIContent* aTreeItemElement);

    nsresult
    FindTreeCellForProperty(nsIContent* aTreeRowElement,
                            nsIRDFResource* aProperty,
                            nsIContent** aTreeCell);

    nsresult
    GetColumnForProperty(nsIContent* aTreeElement,
                         nsIRDFResource* aProperty,
                         PRInt32* aIndex);

    nsresult
    SetCellValue(nsIContent* aTreeRowElement,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aValue);
 
    nsresult 
    GetRootWidgetAtom(nsIAtom** aResult) {
        NS_ADDREF(kTreeAtom);
        *aResult = kTreeAtom;
        return NS_OK;
    }

    nsresult
    GetWidgetItemAtom(nsIAtom** aResult) {
        NS_ADDREF(kTreeItemAtom);
        *aResult = kTreeItemAtom;
        return NS_OK;
    }

    nsresult
    GetWidgetFolderAtom(nsIAtom** aResult) {
        NS_ADDREF(kTreeItemAtom);
        *aResult = kTreeItemAtom;
        return NS_OK;
    }

    nsresult
    GetInsertionRootAtom(nsIAtom** aResult) {
        NS_ADDREF(kTreeBodyAtom);
        *aResult = kTreeBodyAtom;
        return NS_OK;
    }

    nsresult
    GetItemAtomThatContainsTheChildren(nsIAtom** aResult) {
        NS_ADDREF(kTreeChildrenAtom);
        *aResult = kTreeChildrenAtom;
        return NS_OK;
    }

    nsresult
    UpdateContainer(nsIContent *container);

    nsresult
    CheckRDFGraphForUpdates(nsIContent *container);

    void
    Notify(nsITimer *timer);

    // pseudo-constants
    static nsrefcnt gRefCnt;
 
    static nsIAtom* kPropertyAtom;
    static nsIAtom* kTreeAtom;
    static nsIAtom* kTreeBodyAtom;
    static nsIAtom* kTreeCellAtom;
    static nsIAtom* kTreeChildrenAtom;
    static nsIAtom* kTreeColAtom;
    static nsIAtom* kTreeHeadAtom;
    static nsIAtom* kTreeIconAtom;
    static nsIAtom* kTreeIndentationAtom;
    static nsIAtom* kTreeItemAtom;
    static nsIAtom* kTitledButtonAtom;
    static nsIAtom* kPulseAtom;
    static nsIAtom* kLastPulseAtom;
    static nsIAtom* kOpenAtom;

    static nsIXULSortService		*XULSortService;
};

////////////////////////////////////////////////////////////////////////

nsrefcnt		RDFTreeBuilderImpl::gRefCnt = 0;
nsIXULSortService*	RDFTreeBuilderImpl::XULSortService = nsnull;

nsIAtom* RDFTreeBuilderImpl::kPropertyAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeBodyAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeCellAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeChildrenAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeColAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeHeadAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeIconAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeIndentationAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeItemAtom;
nsIAtom* RDFTreeBuilderImpl::kTitledButtonAtom;
nsIAtom* RDFTreeBuilderImpl::kPulseAtom;
nsIAtom* RDFTreeBuilderImpl::kLastPulseAtom;
nsIAtom* RDFTreeBuilderImpl::kOpenAtom;

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFTreeBuilder(nsIRDFContentModelBuilder** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    RDFTreeBuilderImpl* builder = new RDFTreeBuilderImpl();
    if (! builder)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(builder);
    *result = builder;
    return NS_OK;
}



RDFTreeBuilderImpl::RDFTreeBuilderImpl(void)
    : RDFGenericBuilderImpl()
{
    if (gRefCnt == 0) {
        kPropertyAtom        = NS_NewAtom("property");
        kTreeAtom            = NS_NewAtom("tree");
        kTreeBodyAtom        = NS_NewAtom("treebody");
        kTreeCellAtom        = NS_NewAtom("treecell");
        kTreeChildrenAtom    = NS_NewAtom("treechildren");
        kTreeColAtom         = NS_NewAtom("treecol");
        kTreeHeadAtom        = NS_NewAtom("treehead");
        kTreeIconAtom        = NS_NewAtom("treeicon");
        kTreeIndentationAtom = NS_NewAtom("treeindentation");
        kTreeItemAtom        = NS_NewAtom("treeitem");
        kTitledButtonAtom    = NS_NewAtom("titledbutton");
        kPulseAtom           = NS_NewAtom("pulse");
        kLastPulseAtom       = NS_NewAtom("lastPulse");
        kOpenAtom            = NS_NewAtom("open");

	nsresult rv = nsServiceManager::GetService(kXULSortServiceCID,
		kIXULSortServiceIID, (nsISupports**) &XULSortService);
    }
    ++gRefCnt;

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsRDFTreeBuilder");
#endif
}

RDFTreeBuilderImpl::~RDFTreeBuilderImpl(void)
{
    --gRefCnt;
    if (gRefCnt == 0) {
        NS_RELEASE(kPropertyAtom);
        NS_RELEASE(kTreeAtom);
        NS_RELEASE(kTreeBodyAtom);
        NS_RELEASE(kTreeCellAtom);
        NS_RELEASE(kTreeChildrenAtom);
        NS_RELEASE(kTreeColAtom);
        NS_RELEASE(kTreeHeadAtom);
        NS_RELEASE(kTreeIconAtom);
        NS_RELEASE(kTreeIndentationAtom);
        NS_RELEASE(kTreeItemAtom);
        NS_RELEASE(kTitledButtonAtom);
        NS_RELEASE(kPulseAtom);
        NS_RELEASE(kLastPulseAtom);
        NS_RELEASE(kOpenAtom);

	nsServiceManager::ReleaseService(kXULSortServiceCID, XULSortService);
	XULSortService = nsnull;
    }
}

nsresult
RDFTreeBuilderImpl::UpdateContainer(nsIContent *container)
{
	PRInt32			childIndex = 0, numChildren = 0, numGrandChildren, grandChildrenIndex, nameSpaceID;
        nsCOMPtr<nsIContent>	child, grandChild;
	nsresult		rv;

	if (NS_FAILED(rv = container->ChildCount(numChildren)))	return(rv);

	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = container->ChildAt(childIndex, *getter_AddRefs(child))))	break;
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	break;
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;
			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))	return rv;
			if (tag.get() == kTreeItemAtom)
			{
				nsIDOMXULElement	*dom;
				if (NS_SUCCEEDED(rv = child->QueryInterface(kIDomXulElementIID, (void **)&dom)))
				{
					nsAutoString	open("");
					if (NS_SUCCEEDED(rv = child->GetAttribute(kNameSpaceID_None, kOpenAtom, open)))
					{
						if (open.EqualsIgnoreCase("true"))
						{
							nsAutoString	pulse("");
							if (NS_SUCCEEDED(rv = child->GetAttribute(kNameSpaceID_None, kPulseAtom, pulse)))
							{
								if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (pulse.Length() > 0))
								{
									PRInt32	errorCode;
									PRInt32 pulseInterval = pulse.ToInteger(&errorCode);
									if ((!errorCode) && (pulseInterval > 0))
									{
										nsAutoString	lastPulse("");
										PRInt32		lastPulseTime = 0;
										if (NS_SUCCEEDED(rv = child->GetAttribute(kNameSpaceID_None, kLastPulseAtom, lastPulse)))
										{
											if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (lastPulse.Length() > 0))
											{
												lastPulseTime = lastPulse.ToInteger(&errorCode);
												if (errorCode)	lastPulseTime = 0;
											}
										}
										PRInt32	now;
										PRTime	prNow = PR_Now(), oneMillion, temp;
										LL_I2L(oneMillion, PR_USEC_PER_SEC);
										LL_DIV(temp, prNow, oneMillion);
										LL_L2I(now, temp);
										if ((lastPulseTime == 0L) || ((now - lastPulseTime) > pulseInterval))
										{
											lastPulse.SetLength(0);
											lastPulse.Append(now, 10);
											// note: don't check for errors when unsetting the attribute in case this is the 1st time
											child->UnsetAttribute(kNameSpaceID_None, kLastPulseAtom, PR_FALSE);
											if (NS_SUCCEEDED(rv = child->SetAttribute(kNameSpaceID_None, kLastPulseAtom, lastPulse, PR_FALSE)))
											{
#ifdef PR_LOGGING
                                                if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
                                                    nsIRDFResource		*res;
                                                    if (NS_SUCCEEDED(rv = dom->GetResource(&res)))
                                                        {
                                                            nsXPIDLCString	uri;
                                                            res->GetValue( getter_Copies(uri) );
                                                            const char *url = uri;
                                                            PR_LOG(gLog, PR_LOG_DEBUG,
                                                                   ("    URL '%s' gets a pulse now (at %lu)\n", url, now));
                                                            NS_RELEASE(res);
                                                        }
                                                }
#endif
												CheckRDFGraphForUpdates(child);
											}
										}
									}
								}
							}

							// recurse on grandchildren
							if (NS_FAILED(rv = child->ChildCount(numGrandChildren)))	continue;
							for (grandChildrenIndex=0; grandChildrenIndex<numGrandChildren; grandChildrenIndex++)
							{
								if (NS_FAILED(rv = child->ChildAt(grandChildrenIndex, *getter_AddRefs(grandChild))))
									continue;
								if (NS_FAILED(rv = grandChild->GetNameSpaceID(nameSpaceID)))	continue;
								if (nameSpaceID == kNameSpaceID_XUL)
								{
									nsCOMPtr<nsIAtom> tag;
									if (NS_FAILED(rv = grandChild->GetTag(*getter_AddRefs(tag))))
										continue;
									if (tag.get() == kTreeChildrenAtom)
									{
										rv = UpdateContainer(grandChild);
									}
								}
							}
						}
					}
					NS_RELEASE(dom);
				}
			}
		}
	}
	return(NS_OK);
}

int		openSortCallback(const void *data1, const void *data2, void *sortData);

nsresult
RDFTreeBuilderImpl::CheckRDFGraphForUpdates(nsIContent *container)
{
	nsresult			rv = NS_OK;

	nsCOMPtr<nsIDOMXULElement>	domElement( do_QueryInterface(container) );
	NS_ASSERTION(domElement, "not a XULTreeElement");
	if (!domElement)	return(NS_ERROR_UNEXPECTED);

	// get composite db for tree
	if (!mRoot)	return(NS_ERROR_UNEXPECTED);
	nsCOMPtr<nsIDOMXULTreeElement>	domXulTree( do_QueryInterface(mRoot) );
	NS_ASSERTION(domXulTree, "not a nsIDOMXULTreeElement");
	if (!domXulTree)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIRDFCompositeDataSource> db;
	if (NS_FAILED(rv = domXulTree->GetDatabase(getter_AddRefs(db))))
	{
		return(rv);
	}

            nsCOMPtr<nsISupportsArray> childArray;
            rv = NS_NewISupportsArray(getter_AddRefs(childArray));
            if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIRDFResource>	res;
	if (NS_SUCCEEDED(rv = domElement->GetResource(getter_AddRefs(res))))
	{
        // XXX Per Bug 3367, this'll have to be fixed.
		nsCOMPtr<nsISimpleEnumerator> arcs;
		rv = mDB->ArcLabelsOut(res, getter_AddRefs(arcs));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get arcs out");
        if (NS_FAILED(rv)) return rv;

		while (1) {
            PRBool hasMore;
			rv = arcs->HasMoreElements(&hasMore);
			if (NS_FAILED(rv))
                return rv;

            if (! hasMore)
                break;

            nsCOMPtr<nsISupports> isupports;
			rv = arcs->GetNext(getter_AddRefs(isupports));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get cursor value");
            if(NS_FAILED(rv)) return rv;

			nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

			if (!IsContainmentProperty(container, property))	continue;

            // XXX this seems gratuitous? why is it here?
			PRInt32 nameSpaceID;
			nsCOMPtr<nsIAtom> tag;
			if (NS_FAILED(rv = mDocument->SplitProperty(property, &nameSpaceID, getter_AddRefs(tag))))
			{
				NS_ERROR("unable to split property");
				return(rv);
			}

			nsCOMPtr<nsISimpleEnumerator> targets;
			rv = db->GetTargets(res, property, PR_TRUE, getter_AddRefs(targets));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get targets for property");
			if (NS_FAILED(rv)) return rv;

			while (1) {
                PRBool hasMore;
				rv = targets->HasMoreElements(&hasMore);
				if (NS_FAILED(rv))		return rv;

                if (! hasMore)
                    break;

                nsCOMPtr<nsISupports> isupports;
				rv = targets->GetNext(getter_AddRefs(isupports));
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get cursor value");
                if (NS_FAILED(rv)) return rv;

				nsCOMPtr<nsIRDFResource> valueResource = do_QueryInterface(isupports);
				if (valueResource) {
					// Note: hack, storing value then property in array
					childArray->AppendElement(valueResource.get());
					childArray->AppendElement(property.get());
				}
			}
		}
	}
	
	PRInt32 numElements = childArray->Count();
	if (numElements > 0)
	{
		nsIRDFResource ** flatArray = new nsIRDFResource*[numElements];
		if (flatArray)
		{
			// flatten array of resources, sort them, then add/remove as necessary
			PRInt32 loop;
		        for (loop=0; loop<numElements; loop++)
		        {
				flatArray[loop] = (nsIRDFResource *)childArray->ElementAt(loop);
			}
//			nsQuickSort((void *)flatArray, numElements, sizeof(nsIRDFNode *),
//				openSortCallback, (void *)sortInfo);

			// first, remove any nodes that are stale
		        nsCOMPtr<nsIContent>	child;
			if (NS_SUCCEEDED(rv = nsRDFContentUtils::FindChildByTag(container, kNameSpaceID_XUL, kTreeChildrenAtom, getter_AddRefs(child))))
			{
			        // note: enumerate backwards so that indexing is easy
				PRInt32		numGrandChildren;
				if (NS_FAILED(rv = child->ChildCount(numGrandChildren)))	numGrandChildren = 0;
			        nsCOMPtr<nsIContent>	grandChild;
				for (PRInt32 grandchildIndex=numGrandChildren-1; grandchildIndex >= 0; grandchildIndex--)
				{
					if (NS_FAILED(rv = child->ChildAt(grandchildIndex, *getter_AddRefs(grandChild))))	break;
					PRInt32		nameSpaceID;
					if (NS_FAILED(rv = grandChild->GetNameSpaceID(nameSpaceID)))	break;
					if (nameSpaceID == kNameSpaceID_XUL)
					{
						nsCOMPtr<nsIRDFResource>	aRes;
						PRBool				removeNode = PR_TRUE;
						if (NS_SUCCEEDED(rv = nsRDFContentUtils::GetElementResource(grandChild, getter_AddRefs(aRes))))
						{
							PRInt32			innerLoop;
							for (innerLoop=0; innerLoop < numElements; innerLoop+=2)
							{
								PRBool	equals = PR_FALSE;
								if (NS_SUCCEEDED(rv = aRes.get()->EqualsNode(flatArray[innerLoop], &equals)))
								{
									if (equals == PR_TRUE)
									{
										removeNode = PR_FALSE;
										break;
									}
								}
							}
							if (removeNode == PR_TRUE)
							{
								child->RemoveChildAt(grandchildIndex, PR_TRUE);
							}
						}
					}
				}
			}

		        // second, add any nodes that are new
			for (loop=0; loop<numElements; loop+=2)
			{
				nsIRDFResource	*theRes = flatArray[loop];
				if (NS_SUCCEEDED(rv = nsRDFContentUtils::FindChildByTag(container, kNameSpaceID_XUL, kTreeChildrenAtom, getter_AddRefs(child))))
				{
					PRBool		nodeFound = PR_FALSE;
					PRInt32		numGrandChildren;
					if (NS_FAILED(rv = child->ChildCount(numGrandChildren)))	numGrandChildren = 0;
				        nsCOMPtr<nsIContent>	grandChild;
					for (PRInt32 grandchildIndex=0; grandchildIndex<numGrandChildren; grandchildIndex++)
					{
						if (NS_FAILED(rv = child->ChildAt(grandchildIndex, *getter_AddRefs(grandChild))))	break;
						PRInt32		nameSpaceID;
						if (NS_FAILED(rv = grandChild->GetNameSpaceID(nameSpaceID)))	break;
						if (nameSpaceID == kNameSpaceID_XUL)
						{
							nsCOMPtr<nsIRDFResource>	aRes;
							if (NS_SUCCEEDED(rv = nsRDFContentUtils::GetElementResource(grandChild, getter_AddRefs(aRes))))
							{
								PRBool	equals = PR_FALSE;
								if (NS_SUCCEEDED(rv = theRes->EqualsNode(aRes, &equals)))
								{
									if (equals == PR_TRUE)
									{
										nodeFound = PR_TRUE;
										break;
									}
								}
							}
						}
					}
					if (nodeFound == PR_FALSE)
					{
						AddWidgetItem(container, flatArray[loop+1], flatArray[loop], 0);
					}
				}
			}

			delete [] flatArray;
			flatArray = nsnull;
		}
	}
        for (int i = childArray->Count() - 1; i >= 0; i--)
        {
        	childArray->RemoveElementAt(i);
        }
	return(NS_OK);
}

void
RDFTreeBuilderImpl::Notify(nsITimer *timer)
{
	if (!mTimer)	return;

	if (mRoot)
	{
		nsresult		rv;
#ifdef	PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
            nsIDOMXULElement	*domElement;

            if (NS_SUCCEEDED(rv = mRoot->QueryInterface(kIDomXulElementIID, (void **)&domElement)))
                {
                    nsIRDFResource		*res;

                    if (NS_SUCCEEDED(rv = domElement->GetResource(&res)))
                        {
                            nsXPIDLCString	uri;
                            res->GetValue( getter_Copies(uri) );
                            const char	*url = uri;
                            PR_LOG(gLog, PR_LOG_DEBUG,
                                   ("Timer fired for '%s'\n", url));

                            NS_RELEASE(res);
                        }
                    NS_RELEASE(domElement);
                }
        }
#endif
		nsIContent	*treeBody;
		if (NS_SUCCEEDED(rv = nsRDFContentUtils::FindChildByTag(mRoot, kNameSpaceID_XUL, kTreeBodyAtom, &treeBody)))
		{
			UpdateContainer(treeBody);
			NS_RELEASE(treeBody);
		}
	}
	mTimer->Cancel();
	NS_RELEASE(mTimer);
	mTimer = nsnull;

	// reschedule timer (if document still around)
	if (mDocument)
	{
		NS_VERIFY(NS_SUCCEEDED(NS_NewTimer(&mTimer)), "couldn't get timer");
		if (mTimer)
		{
			mTimer->Init(this, /* PR_TRUE, */ BUILDER_NOTIFY_MINIMUM_TIMEOUT);
		}
	}
}

////////////////////////////////////////////////////////////////////////
// nsIRDFContentModelBuilder interface

NS_IMETHODIMP
RDFTreeBuilderImpl::SetDataBase(nsIRDFCompositeDataSource* aDataBase)
{
    NS_PRECONDITION(mRoot != nsnull, "not initialized");
    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;
    if (NS_FAILED(rv = RDFGenericBuilderImpl::SetDataBase(aDataBase)))
        return rv;

    // Now set the database on the tree root, so that script writers
    // can access it.
    nsCOMPtr<nsIDOMXULTreeElement> element( do_QueryInterface(mRoot) );
    NS_ASSERTION(element, "not a XULTreeElement");
    if (! element)
        return NS_ERROR_UNEXPECTED;

    rv = element->SetDatabase(aDataBase);
    NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't set database on tree element");
    return rv;
}


////////////////////////////////////////////////////////////////////////
// nsIDOMNodeObserver interface

NS_IMETHODIMP
RDFTreeBuilderImpl::OnAppendChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild)
{
    NS_PRECONDITION(aParent != nsnull, "null ptr");
    if (! aParent)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aNewChild != nsnull, "null ptr");
    if (! aNewChild)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    nsCOMPtr<nsIRDFResource> resource;
    rv = GetDOMNodeResource(aParent, getter_AddRefs(resource));
    if (NS_FAILED(rv)) {
        // XXX it's not a resource element, so there's no assertions
        // we need to make on the back-end. Should we just do the
        // update?
        return NS_OK;
    }

    // Get the nsIContent interface, it's a bit more utilitarian
    nsCOMPtr<nsIContent> parent( do_QueryInterface(aParent) );
    NS_ASSERTION(parent != nsnull, "parent doesn't support nsIContent");
    if (! parent) return NS_ERROR_UNEXPECTED;

    // Make sure that the element is in the widget. XXX Even this may be
    // a bit too promiscuous: an element may also be a XUL element...
    if (!IsElementInWidget(parent))
        return NS_OK;

    // Split the parent into its namespace and tag components
    PRInt32 parentNameSpaceID;
    rv = parent->GetNameSpaceID(parentNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIAtom> parentNameAtom;
    rv = parent->GetTag( *getter_AddRefs(parentNameAtom) );
    if (NS_FAILED(rv)) return rv;

    // Now do the same for the child
    nsCOMPtr<nsIContent> child( do_QueryInterface(aNewChild) );
    NS_ASSERTION(child != nsnull, "child doesn't support nsIContent");
    if (! child) return NS_ERROR_UNEXPECTED;

    PRInt32 childNameSpaceID;
    rv = child->GetNameSpaceID(childNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIAtom> childNameAtom;
    rv = child->GetTag( *getter_AddRefs(childNameAtom) );
    if (NS_FAILED(rv)) return rv;

    // Now see if there's anything we can do about it.

    if ((parentNameSpaceID == kNameSpaceID_XUL) &&
        ((parentNameAtom.get() == kTreeChildrenAtom) ||
         (parentNameAtom.get() == kTreeBodyAtom))) {
        // The parent is a xul:treechildren or xul:treebody...

        if ((childNameSpaceID == kNameSpaceID_XUL) &&
            (childNameAtom.get() == kTreeItemAtom)) {

            // ...and the child is a tree item. We can do this. First,
            // get the rdf:property out of the child to see what the
            // relationship was between the parent and the child.
            nsAutoString propertyStr;
            rv = child->GetAttribute(kNameSpaceID_RDF, kPropertyAtom, propertyStr);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                // It's a relationship that we'll need to set up in
                // RDF. So let's assert it into the graph. First we
                // need the property resource.
                nsCOMPtr<nsIRDFResource> property;
                rv = gRDFService->GetUnicodeResource(propertyStr.GetUnicode(), getter_AddRefs(property));
                if (NS_FAILED(rv)) return rv;

                // And now we need the child's resource.
                nsCOMPtr<nsIRDFResource> target;
                rv = nsRDFContentUtils::GetElementResource(child, getter_AddRefs(target));
                NS_ASSERTION(NS_SUCCEEDED(rv) && (target != nsnull), "expected child to have resource");
                if (NS_FAILED(rv)) return rv;

                if (! target)
                    return NS_ERROR_UNEXPECTED;

                // We'll handle things a bit differently if we're
                // tinkering with an RDF container...
                PRBool isContainer, isOrdinal;
                if (NS_SUCCEEDED(gRDFContainerUtils->IsContainer(mDB, resource, &isContainer)) &&
                    isContainer &&
                    NS_SUCCEEDED(gRDFContainerUtils->IsOrdinalProperty(property, &isOrdinal)) &&
                    isOrdinal)
                {
                    nsCOMPtr<nsIRDFContainer> container;
                    rv = NS_NewRDFContainer(getter_AddRefs(container));
                    if (NS_FAILED(rv)) return rv;
                    
                    rv = container->Init(mDB, resource);
                    if (NS_FAILED(rv)) return rv;

                    rv = container->AppendElement(target);
                }
                else {
                    rv = mDB->Assert(resource, property, target, PR_TRUE);
                }

                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to remove element from DB");
                if (NS_FAILED(rv)) return rv;

                return NS_OK;
            }

            // Otherwise, it's a random element that doesn't
            // correspond to anything in the graph. Fall through...
        }
    }
    else if ((parentNameSpaceID == kNameSpaceID_XUL) &&
             (parentNameAtom.get() == kTreeItemAtom)) {

        // The parent is a xul:treeitem.

        // XXX We really only care about treeitems in the body; not
        // treeitems in the header...
        if ((childNameSpaceID == kNameSpaceID_XUL) &&
            (childNameAtom.get() == kTreeCellAtom)) {

            // ...and the child is a tree cell. They're adding a value
            // for a property.
            NS_NOTYETIMPLEMENTED("write me");
        }
    }
    else if ((parentNameSpaceID == kNameSpaceID_XUL) &&
             (parentNameAtom.get() == kTreeCellAtom)) {

        // The parent is a xul:treecell. They're changing the value of
        // a cell.

        // XXX We really only care about cells in the body; not cells
        // in the header...
        NS_NOTYETIMPLEMENTED("write me");
    }


    // If we get here, then they're trying to manipulate the DOM in
    // some way that doesn't translate into a sensible update to the RDF
    // graph. So, just whack the change into the content model
    rv = parent->AppendChildTo(child, PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "error appending child to parent");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
RDFTreeBuilderImpl::OnRemoveChild(nsIDOMNode* aParent, nsIDOMNode* aOldChild)
{
    NS_PRECONDITION(aParent != nsnull, "null ptr");
    if (!aParent)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aOldChild != nsnull, "null ptr");
    if (!aOldChild)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsIRDFResource> resource;
    rv = GetDOMNodeResource(aParent, getter_AddRefs(resource));
    if (NS_FAILED(rv)) {
        // XXX it's not a resource element, so there's no assertions
        // we need to make on the back-end. Should we just do the
        // update?
        return NS_OK;
    }

    // Get the nsIContent interface, it's a bit more utilitarian
    nsCOMPtr<nsIContent> parent( do_QueryInterface(aParent) );
    NS_ASSERTION(parent != nsnull, "parent doesn't support nsIContent");
    if (! parent) return NS_ERROR_UNEXPECTED;

    // Make sure that the element is in the widget. XXX Even this may be
    // a bit too promiscuous: an element may also be a XUL element...
    if (!IsElementInWidget(parent))
        return NS_OK;

    // Split the parent into its namespace and tag components
    PRInt32 parentNameSpaceID;
    rv = parent->GetNameSpaceID(parentNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIAtom> parentNameAtom;
    rv = parent->GetTag( *getter_AddRefs(parentNameAtom) );
    if (NS_FAILED(rv)) return rv;

    // Now do the same for the child
    nsCOMPtr<nsIContent> child( do_QueryInterface(aOldChild) );
    NS_ASSERTION(child != nsnull, "child doesn't support nsIContent");
    if (! child) return NS_ERROR_UNEXPECTED;

    PRInt32 childNameSpaceID;
    rv = child->GetNameSpaceID(childNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIAtom> childNameAtom;
    rv = child->GetTag( *getter_AddRefs(childNameAtom) );
    if (NS_FAILED(rv)) return rv;

    // Now see if there's anything we can do about it.

    if ((parentNameSpaceID == kNameSpaceID_XUL) &&
        ((parentNameAtom.get() == kTreeChildrenAtom) ||
         (parentNameAtom.get() == kTreeBodyAtom))) {
        // The parent is a xul:treechildren or xul:treebody...

        if ((childNameSpaceID == kNameSpaceID_XUL) &&
            (childNameAtom.get() == kTreeItemAtom)) {

            // ...and the child is a tree item. We can do this. First,
            // get the rdf:property out of the child to see what the
            // relationship was between the parent and the child.
            nsAutoString propertyStr;
            rv = child->GetAttribute(kNameSpaceID_RDF, kPropertyAtom, propertyStr);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                // It's a relationship set up by RDF. So let's
                // unassert it from the graph. First we need the
                // property resource.
                nsCOMPtr<nsIRDFResource> property;
                rv = gRDFService->GetUnicodeResource(propertyStr.GetUnicode(), getter_AddRefs(property));
                if (NS_FAILED(rv)) return rv;

                // And now we need the child's resource.
                nsCOMPtr<nsIRDFResource> target;
                rv = nsRDFContentUtils::GetElementResource(child, getter_AddRefs(target));
                NS_ASSERTION(NS_SUCCEEDED(rv) && (target != nsnull), "expected child to have resource");
                if (NS_FAILED(rv)) return rv;

                if (! target)
                    return NS_ERROR_UNEXPECTED;

                // We'll handle things a bit differently if we're
                // tinkering with an RDF container...
                PRBool isContainer, isOrdinal;
                if (NS_SUCCEEDED(gRDFContainerUtils->IsContainer(mDB, resource, &isContainer)) &&
                    isContainer &&
                    NS_SUCCEEDED(gRDFContainerUtils->IsOrdinalProperty(property, &isOrdinal)) &&
                    isOrdinal)
                {
                    nsCOMPtr<nsIRDFContainer> container;
                    rv = NS_NewRDFContainer(getter_AddRefs(container));
                    if (NS_FAILED(rv)) return rv;
                    
                    rv = container->Init(mDB, resource);
                    if (NS_FAILED(rv)) return rv;

                    rv = container->RemoveElement(target, PR_TRUE);
                }
                else {
                    rv = mDB->Unassert(resource, property, target);
                }

                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to remove element from DB");
                if (NS_FAILED(rv)) return rv;

                return NS_OK;
            }

            // Otherwise, it's a random element that doesn't
            // correspond to anything in the graph. Fall through...
        }
    }
    else if ((parentNameSpaceID == kNameSpaceID_XUL) &&
             (parentNameAtom.get() == kTreeItemAtom)) {

        // The parent is a xul:treeitem.

        // XXX We really only care about treeitems in the body; not
        // treeitems in the header...
        if ((childNameSpaceID == kNameSpaceID_XUL) &&
            (childNameAtom.get() == kTreeCellAtom)) {

            // ...and the child is a tree cell. They're adding a value
            // for a property.
            NS_NOTYETIMPLEMENTED("write me");
        }
    }
    else if ((parentNameSpaceID == kNameSpaceID_XUL) &&
             (parentNameAtom.get() == kTreeCellAtom)) {

        // The parent is a xul:treecell. They're changing the value of
        // a cell.

        // XXX We really only care about cells in the body; not cells
        // in the header...
        NS_NOTYETIMPLEMENTED("write me");
    }

    // If we get here, then they're trying to manipulate the DOM in
    // some way that doesn't translate into a sensible update to the RDF
    // graph. So, just whack the change into the content model
    PRInt32 index;
    rv = parent->IndexOf(child, index);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(index >= 0, "child was already removed");
    if (index >= 0) {
        rv = parent->RemoveChildAt(index, PR_TRUE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error removing child from parent");
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
RDFTreeBuilderImpl::AddWidgetItem(nsIContent* aElement,
                                  nsIRDFResource* aProperty,
                                  nsIRDFResource* aValue,
                                  PRInt32 aNaturalOrderPos)
{
    // If it's a tree property, then we need to add the new child
    // element to a special "children" element in the parent.  The
    // child element's value will be the value of the
    // property. We'll also attach an "ID=" attribute to the new
    // child; e.g.,
    //
    // <xul:treeitem>
    //   ...
    //   <xul:treechildren>
    //     <xul:treeitem id="value" rdf:property="property">
    //        <xul:treecell>
    //           <!-- value not specified until SetCellValue() -->
    //        </xul:treecell>
    //
    //        <xul:treecell>
    //           <!-- value not specified until SetCellValue() -->
    //        </xul:treecell>
    //
    //        ...
    //
    //        <!-- Other content recursively generated -->
    //
    //     </xul:treeitem>
    //   </xul:treechildren>
    //   ...
    // </xul:treeitem>
    //
    // We can also handle the case where they've specified RDF
    // contents on the <xul:treebody> tag, in which case we'll just
    // dangle the new row directly off the treebody.

    nsresult rv;

    nsCOMPtr<nsIContent> treeChildren;
    if (IsItemOrFolder(aElement)) {
        // Ensure that the <xul:treechildren> element exists on the parent.
        if (NS_FAILED(rv = EnsureElementHasGenericChild(aElement,
                                                        kNameSpaceID_XUL,
                                                        kTreeChildrenAtom,
                                                        getter_AddRefs(treeChildren))))
            return rv;
    }
    else if (IsWidgetInsertionRootElement(aElement)) {
        // We'll just use the <xul:treebody> as the element onto which
        // we'll dangle a new row.
        treeChildren = do_QueryInterface(aElement);
        if (! treeChildren) {
            NS_ERROR("aElement is not nsIContent!?!");
            return NS_ERROR_UNEXPECTED;
        }
    }
    else {
        NS_ERROR("new tree row doesn't fit here!");
        return NS_ERROR_UNEXPECTED;
    }

    // Find out if we're a container or not.
    PRBool markAsContainer = PR_FALSE;

    // Create the <xul:treeitem> element
    nsCOMPtr<nsIContent> treeItem;
    if (NS_FAILED(rv = CreateResourceElement(kNameSpaceID_XUL,
                                             kTreeItemAtom,
                                             aValue,
                                             getter_AddRefs(treeItem))))
        return rv;

    // Set the rdf:property attribute to be the arc label from the
    // parent. This indicates how it got generated, so we can keep any
    // subsequent changes via the DOM in sink. This property should be
    // immutable.
    {
        nsXPIDLCString uri;
        aProperty->GetValue( getter_Copies(uri) );
        treeItem->SetAttribute(kNameSpaceID_RDF, kPropertyAtom, (const char*) uri, PR_FALSE);
    }

    // Create the cell substructure
    if (NS_FAILED(rv = CreateTreeItemCells(treeItem)))
        return rv;

    // Add the <xul:treeitem> to the <xul:treechildren> element.
	if (nsnull != XULSortService)
	{
		XULSortService->InsertContainerNode(treeChildren, treeItem);
	}
	else
	{
		treeChildren->AppendChildTo(treeItem, PR_TRUE);
	}

    // Add miscellaneous attributes by iterating _all_ of the
    // properties out of the resource.
    nsCOMPtr<nsISimpleEnumerator> arcs;
    rv = mDB->ArcLabelsOut(aValue, getter_AddRefs(arcs));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get arcs out");
    if (NS_FAILED(rv)) return rv;

    while (1) {
        PRBool hasMore;
        rv = arcs->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = arcs->GetNext(getter_AddRefs(isupports));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get cursor value");
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

        // Ignore properties that are used to indicate "tree-ness"
        if (IsContainmentProperty(aElement, property))
        {
            markAsContainer = PR_TRUE;
            continue;
        }

        PRInt32 nameSpaceID;
        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = mDocument->SplitProperty(property, &nameSpaceID, getter_AddRefs(tag)))) {
            NS_ERROR("unable to split property");
            return rv;
        }

        nsCOMPtr<nsIRDFNode> value;
        if (NS_FAILED(rv = mDB->GetTarget(aValue, property, PR_TRUE, getter_AddRefs(value)))) {
            NS_ERROR("unable to get target");
            return rv;
        }

        NS_ASSERTION(rv != NS_RDF_NO_VALUE, "arc-out with no target: fix your arcs-out cursor");
        if (rv == NS_RDF_NO_VALUE)
            continue;

        nsCOMPtr<nsIRDFResource> resource;
        nsCOMPtr<nsIRDFLiteral> literal;

        nsAutoString s;
        if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFResourceIID, getter_AddRefs(resource)))) {
            nsXPIDLCString uri;
            resource->GetValue( getter_Copies(uri) );
            s = uri;
        }
        else if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFLiteralIID, getter_AddRefs(literal)))) {
            nsXPIDLString p;
            literal->GetValue( getter_Copies(p) );
            s = p;
        }
        else {
            NS_ERROR("not a resource or a literal");
            return NS_ERROR_UNEXPECTED;
        }

        treeItem->SetAttribute(nameSpaceID, tag, s, PR_FALSE);

        if (aNaturalOrderPos > 0)
	    {
            // XXX Add this to menu builder as well, or better yet, abstract out.
		    nsAutoString	pos, zero("0000");
		    pos.Append(aNaturalOrderPos, 10);
		    if (pos.Length() < 4)
		    {
			    pos.Insert(zero, 0, 4-pos.Length()); 
		    }
		    treeItem->SetAttribute(kNameSpaceID_None, kNaturalOrderPosAtom, pos, PR_FALSE);
	    }
    }

    if (markAsContainer)
    {
        // Finally, mark this as a "container" so that we know to
        // recursively generate kids if they're asked for.
        if (NS_FAILED(rv = treeItem->SetAttribute(kNameSpaceID_RDF, kContainerAtom, "true", PR_FALSE)))
            return rv;
    }

    return NS_OK;
}




nsresult
RDFTreeBuilderImpl::RemoveWidgetItem(nsIContent* aElement,
                                     nsIRDFResource* aProperty,
                                     nsIRDFResource* aValue)
{
    nsresult rv;

    // We may put in a situation where we've been asked to either
    // remove a xul:treeitem directly from a xul:treechildren (or
    // xul:treebody) tag; or, we may be asked to remove a xul:treeitem
    // from a grandparent xul:treeitem tag.

    // Verify that the element is actually a xul:treeitem
    PRInt32 nameSpaceID;
    if (NS_FAILED(rv = aElement->GetNameSpaceID(nameSpaceID))) {
        NS_ERROR("unable to get element's namespace ID");
        return rv;
    }

    nsCOMPtr<nsIAtom> tag;
    if  (NS_FAILED(rv = aElement->GetTag(*getter_AddRefs(tag)))) {
        NS_ERROR("unable to get element's tag");
        return rv;
    }

    nsCOMPtr<nsIContent> treechildren; // put it here so it stays in scope

    if ((nameSpaceID == kNameSpaceID_XUL) && (tag.get() == kTreeItemAtom)) {
        rv = nsRDFContentUtils::FindChildByTag(aElement,
                                               kNameSpaceID_XUL,
                                               kTreeChildrenAtom,
                                               getter_AddRefs(treechildren));
        // XXX make this a warning
        NS_ASSERTION(NS_OK == rv, "attempt to remove child from an element with no treechildren");
        if (NS_OK != rv) return rv;

        aElement = treechildren.get();
    }

    // Now we assume that aElement is a xul:treebody or a
    // xul:treechildren; we'll just make sure for kicks.
    {
        aElement->GetNameSpaceID(nameSpaceID);
        NS_ASSERTION(kNameSpaceID_XUL == nameSpaceID, "not a xul:treebody or xul:treechildren");
        if (kNameSpaceID_XUL != nameSpaceID)
            return NS_ERROR_UNEXPECTED;

        aElement->GetTag(*getter_AddRefs(tag));
        NS_ASSERTION((kTreeBodyAtom == tag.get()) || (kTreeChildrenAtom == tag.get()),
                     "not a xul:treebody or xul:treechildren");
        if ((kTreeBodyAtom != tag.get()) && (kTreeChildrenAtom != tag.get()))
            return NS_ERROR_UNEXPECTED;
    }

    // Allright, now grovel to find the doomed kid and blow it away.
    PRInt32 count;
    if (NS_FAILED(rv = aElement->ChildCount(count)))
        return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aElement->ChildAt(i, *getter_AddRefs(kid))))
            return rv; // XXX fatal

        // Make sure it's a <xul:treeitem>
        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID != kNameSpaceID_XUL)
            continue; // wrong namespace

        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(tag))))
            return rv; // XXX fatal

        if (tag.get() != kTreeItemAtom)
            continue; // wrong tag

        // Now get the resource ID from the RDF:ID attribute. We do it
        // via the content model, because you're never sure who
        // might've added this stuff in...
        nsCOMPtr<nsIRDFResource> resource;
        if (NS_FAILED(rv = nsRDFContentUtils::GetElementResource(kid, getter_AddRefs(resource)))) {
            NS_ERROR("severe error retrieving resource");
            return rv;
        }

        if (resource.get() != aValue)
            continue; // not the resource we want

        // Fount it! Now kill it.
        if (NS_FAILED(rv = aElement->RemoveChildAt(i, PR_TRUE))) {
            NS_ERROR("unable to remove xul:treeitem from xul:treechildren");
            return rv;
        }

        return NS_OK;
    }

    // XXX make this a warning
    NS_WARNING("unable to find child to remove");
    return NS_OK;
}



nsresult
RDFTreeBuilderImpl::SetWidgetAttribute(nsIContent* aTreeItemElement,
                                       nsIRDFResource* aProperty,
                                       nsIRDFNode* aValue)
{
    nsresult rv;

    // figure out the textual value that we want to use
    nsAutoString value;
    rv = nsRDFContentUtils::GetTextForNode(aValue, value);

    // see it it's a cell in the tree
    PRInt32 index;
    if (NS_SUCCEEDED(rv = GetColumnForProperty(mRoot, aProperty, &index))) {
        // ...yep.
        nsCOMPtr<nsIContent> cellElement;
        rv = EnsureCell(aTreeItemElement, index, getter_AddRefs(cellElement));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to ensure cell exists");
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsITextContent> text;
        rv = FindTextElement(cellElement, getter_AddRefs(text));
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(text != nsnull, "unable to find text child");
        if (text)
            text->SetText(value.GetUnicode(), value.Length(), PR_TRUE);
    }


    // no matter what, it's also been set as an attribute.
    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> tagAtom;
    rv = mDocument->SplitProperty(aProperty, &nameSpaceID, getter_AddRefs(tagAtom));
    if (NS_FAILED(rv)) return rv;

    rv = aTreeItemElement->SetAttribute(nameSpaceID, tagAtom, value, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
RDFTreeBuilderImpl::UnsetWidgetAttribute(nsIContent* aTreeItemElement,
                                         nsIRDFResource* aProperty,
                                         nsIRDFNode* aValue)
{
    nsresult rv;

    PRInt32 index;
    if (NS_SUCCEEDED(rv = GetColumnForProperty(mRoot, aProperty, &index))) {
        // it's a cell. find it.
        nsCOMPtr<nsIContent> cellElement;
        rv = EnsureCell(aTreeItemElement, index, getter_AddRefs(cellElement));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to ensure cell exists");
        if (NS_FAILED(rv)) return rv;

        // find the text element
        nsCOMPtr<nsITextContent> text;
        rv = FindTextElement(cellElement, getter_AddRefs(text));
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(text != nsnull, "unable to find text child");
        if (text)
            text->SetText(nsAutoString().GetUnicode(), 0, PR_TRUE);
    }

    // no matter what, it's also been created as an attribute.
    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> tagAtom;
    rv = mDocument->SplitProperty(aProperty, &nameSpaceID, getter_AddRefs(tagAtom));
    if (NS_FAILED(rv)) return rv;

    rv = aTreeItemElement->UnsetAttribute(nameSpaceID, tagAtom, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
RDFTreeBuilderImpl::FindTextElement(nsIContent* aElement,
                                    nsITextContent** aResult)
{
    nsresult rv;

    PRInt32 count;
    rv = aElement->ChildCount(count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get element child count");
    if (NS_FAILED(rv)) return rv;

    while (--count >= 0) {
        nsCOMPtr<nsIContent> child;
        rv = aElement->ChildAt(count, *getter_AddRefs(child));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get child element");
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsITextContent> text( do_QueryInterface(child) );
        if (text) {
            *aResult = text;
            NS_ADDREF(*aResult);
            return NS_OK;
        }

        // depth-first search
        rv = FindTextElement(child, aResult);
        if (NS_FAILED(rv)) return rv;

        if (*aResult)
            return NS_OK;
    }

    *aResult = nsnull;
    return NS_OK;
}

nsresult
RDFTreeBuilderImpl::EnsureCell(nsIContent* aTreeItemElement,
                               PRInt32 aIndex,
                               nsIContent** aCellElement)
{
    // This method returns that the aIndex-th <xul:treecell> element
    // if it is already present, and if not, will create up to aIndex
    // nodes to create it.
    NS_PRECONDITION(aIndex >= 0, "invalid arg");
    if (aIndex < 0)
        return NS_ERROR_INVALID_ARG;

    nsresult rv;

    // XXX at this point, we should probably ensure that aElement is
    // actually a <xul:treeitem>...


    // Iterate through the children of the <xul:treeitem>, counting
    // <xul:treecell> tags until we get to the aIndex-th one.
    PRInt32 count;
    if (NS_FAILED(rv = aTreeItemElement->ChildCount(count))) {
        NS_ERROR("unable to get xul:treeitem's child count");
        return rv;
    }

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aTreeItemElement->ChildAt(i, *getter_AddRefs(kid)))) {
            NS_ERROR("unable to retrieve xul:treeitem's child");
            return rv;
        }

        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID))) {
            NS_ERROR("unable to get child namespace");
            return rv;
        }

        if (nameSpaceID != kNameSpaceID_XUL)
            continue; // not <xul:*>

        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(tag)))) {
            NS_ERROR("unable to get child tag");
            return rv;
        }

        if (tag.get() != kTreeCellAtom)
            continue; // not <xul:treecell>

        // Okay, it's a xul:treecell; see if it's the right one...
        if (aIndex == 0) {
            *aCellElement = kid;
            NS_ADDREF(*aCellElement);
            return NS_OK;
        }

        // Nope, decrement the counter and move on...
        --aIndex;
    }

    // Create all of the xul:treecell elements up to and including the
    // index of the cell that was asked for.
    NS_ASSERTION(aIndex >= 0, "uh oh, I thought aIndex was s'posed t' be >= 0...");

    nsCOMPtr<nsIContent> cellElement;
    while (aIndex-- >= 0) {
        if (NS_FAILED(rv = NS_NewRDFElement(kNameSpaceID_XUL,
                                            kTreeCellAtom,
                                            getter_AddRefs(cellElement)))) {
            NS_ERROR("unable to create new xul:treecell");
            return rv;
        }

        if (NS_FAILED(rv = aTreeItemElement->AppendChildTo(cellElement, PR_FALSE))) {
            NS_ERROR("unable to append xul:treecell to treeitem");
            return rv;
        }
    }

    *aCellElement = cellElement;
    NS_ADDREF(*aCellElement);
    return NS_OK;
}

nsresult
RDFTreeBuilderImpl::CreateTreeItemCells(nsIContent* aTreeItemElement)
{
    // <xul:treeitem>
    //   <xul:treecell RDF:ID="property">value</xul:treecell>
    //   ...
    // </xul:treeitem>
    nsresult rv;

    // XXX at this point, we should probably ensure that aElement is
    // actually a <xul:treeitem>...

    // Get the treeitem's resource so that we can generate cell
    // values. We could QI for the nsIRDFResource here, but doing this
    // via the nsIContent interface allows us to support generic nodes
    // that might get added in by DOM calls.
    nsCOMPtr<nsIRDFResource> treeItemResource;
    rv = nsRDFContentUtils::GetElementResource(aTreeItemElement, getter_AddRefs(treeItemResource));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get tree item resource");
    if (NS_FAILED(rv)) return rv;

    PRInt32 count;
    if (NS_FAILED(rv = mRoot->ChildCount(count))) {
        NS_ERROR("unable to count xul:tree element's kids");
        return rv;
    }

    // Iterate through all the columns that have been specified,
    // constructing a cell in the content model for each one.
    PRInt32 cellIndex = 0;
    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = mRoot->ChildAt(i, *getter_AddRefs(kid)))) {
            NS_ERROR("unable to get xul:tree's child");
            return rv;
        }

        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID))) {
            NS_ERROR("unable to get child's namespace");
            return rv;
        }

        if (nameSpaceID != kNameSpaceID_XUL)
            continue; // not <xul:*>

        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(tag)))) {
            NS_ERROR("unable to get child's tag");
            return rv;
        }

        if (tag.get() != kTreeColAtom)
            continue; // not <xul:treecol>

        // Okay, we've found a column. Ensure that we've got a real
        // tree cell that lives beneath _this_ tree item for its
        // value.
        nsCOMPtr<nsIContent> cellElement;
        if (NS_FAILED(rv = EnsureCell(aTreeItemElement, cellIndex, getter_AddRefs(cellElement)))) {
            NS_ERROR("unable to find/create cell element");
            return rv;
        }

        nsIContent* textParent = cellElement;

        // The first cell gets a <xul:treeindentation> element and a
        // <xul:treeicon> element...
        //
        // XXX This is bogus: dogfood ready crap. We need to figure
        // out a better way to specify this.
        if (cellIndex == 0) {
            nsCOMPtr<nsIContent> indentationElement;
            if (NS_FAILED(rv = NS_NewRDFElement(kNameSpaceID_XUL,
                                                kTreeIndentationAtom,
                                                getter_AddRefs(indentationElement)))) {
                NS_ERROR("unable to create indentation node");
                return rv;
            }

            if (NS_FAILED(rv = cellElement->AppendChildTo(indentationElement, PR_FALSE))) {
                NS_ERROR("unable to append indentation element");
                return rv;
            }

            nsCOMPtr<nsIContent> iconElement;
            if (NS_FAILED(rv = NS_NewRDFElement(kNameSpaceID_XUL,
                                                kTitledButtonAtom /* kTreeIconAtom */,
                                                getter_AddRefs(iconElement)))) {
                NS_ERROR("unable to create icon node");
                return rv;
            }

            if (NS_FAILED(rv = cellElement->AppendChildTo(iconElement, PR_FALSE))) {
                NS_ERROR("uanble to append icon element");
                return rv;
            }

            //textParent = iconElement;
        }


        // The column property is stored in the RDF:resource attribute
        // of the tag.
        nsAutoString uri;
        if (NS_FAILED(rv = kid->GetAttribute(kNameSpaceID_RDF, kResourceAtom, uri))) {
            NS_ERROR("severe error occured retrieving attribute");
            return rv;
        }

        // Set its value, if we know it.
        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {

            // First construct a property resource from the URI...
            nsCOMPtr<nsIRDFResource> property;
            if (NS_FAILED(gRDFService->GetUnicodeResource(uri.GetUnicode(), getter_AddRefs(property)))) {
                NS_ERROR("unable to construct resource for xul:treecell");
                return rv; // XXX fatal
            }

            // ...then query the RDF back-end
            nsCOMPtr<nsIRDFNode> value;
            rv = mDB->GetTarget(treeItemResource,
                                property,
                                PR_TRUE,
                                getter_AddRefs(value));

            if (rv == NS_RDF_NO_VALUE) {
                // There was no value for this. It'll have to get set
                // later, when an OnAssert() comes in (if it even has
                // a value at all...)
            }
            else if (NS_FAILED(rv)) {
                NS_ERROR("error getting cell's value");
                return rv; // XXX something serious happened
            }
            else {
                // Attach a plain old text node: nothing fancy. Here's
                // where we'd do wacky stuff like pull in an icon or
                // whatever.
                if (NS_FAILED(rv = nsRDFContentUtils::AttachTextNode(textParent, value))) {
                    NS_ERROR("unable to attach text node to xul:treecell");
                    return rv;
                }
            }
        }

        ++cellIndex;
    }

    return NS_OK;
}


nsresult
RDFTreeBuilderImpl::GetColumnForProperty(nsIContent* aTreeElement,
                                         nsIRDFResource* aProperty,
                                         PRInt32* aIndex)
{
    nsresult rv;

    nsXPIDLCString propertyURI;
    if (NS_FAILED(rv = aProperty->GetValue( getter_Copies(propertyURI) ))) {
        NS_ERROR("unable to get property's URI");
        return rv;
    }

    // XXX should ensure that aTreeElement really is a xul:tree
    
    PRInt32 count;
    if (NS_FAILED(rv = aTreeElement->ChildCount(count))) {
        NS_ERROR("unable to count xul:tree element's kids");
        return rv;
    }

    // Iterate through the columns to find the one that's appropriate
    // for this cell.
    PRInt32 index = 0;
    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aTreeElement->ChildAt(i, *getter_AddRefs(kid)))) {
            NS_ERROR("unable to get xul:tree's child");
            return rv;
        }

        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID))) {
            NS_ERROR("unable to get child's namespace");
            return rv;
        }

        if (nameSpaceID != kNameSpaceID_XUL)
            continue; // not <xul:*>

        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(tag)))) {
            NS_ERROR("unable to get child's tag");
            return rv;
        }

        if (tag.get() != kTreeColAtom)
            continue; // not <xul:treecol>

        // Okay, we've found a column. Is it the right one?  The
        // column property is stored in the RDF:resource attribute of
        // the tag....
        nsAutoString uri;
        if (NS_FAILED(rv = kid->GetAttribute(kNameSpaceID_RDF, kResourceAtom, uri))) {
            NS_ERROR("severe error occured retrieving attribute");
            return rv;
        }

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            if (0 == nsCRT::strcmp(uri.GetUnicode(), propertyURI)) {
                *aIndex = index;
                return NS_OK;
            }
        }

        ++index;
    }

    // Nope, couldn't find it.
    return NS_ERROR_FAILURE;
}

nsresult
RDFTreeBuilderImpl::SetCellValue(nsIContent* aTreeItemElement,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aValue)
{
    nsresult rv;

    // XXX We assume that aTreeItemElement is actually a
    // <xul:treeitem>, it'd be good to enforce this...

    PRInt32 index;
    if (NS_FAILED(rv = GetColumnForProperty(mRoot, aProperty, &index))) {
        // If we can't find a column for the specified property, that
        // just means there isn't a column in the tree for that
        // property. No big deal. Bye!
        return NS_OK;
    }

    nsCOMPtr<nsIContent> cellElement;
    if (NS_FAILED(rv = EnsureCell(aTreeItemElement, index, getter_AddRefs(cellElement)))) {
        NS_ERROR("unable to find/create cell element");
        return rv;
    }

    // XXX if the cell already has a value, we need to replace it, not
    // just append new text...

    if (NS_FAILED(rv = nsRDFContentUtils::AttachTextNode(cellElement, aValue)))
        return rv;

    return NS_OK;
}


