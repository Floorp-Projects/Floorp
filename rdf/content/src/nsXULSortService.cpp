/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
  This file provides the implementation for the sort service manager.
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

#include "nsVoidArray.h"
#include "nsQuickSort.h"
#include "nsIAtom.h"
#include "nsIXULSortService.h"
#include "nsString.h"
#include "plhash.h"
#include "plstr.h"
#include "prlog.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"

#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"

#include "nsVoidArray.h"

#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "rdf.h"

#include "nsIDOMXULTreeElement.h"
#include "nsIDOMXULElement.h"


////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kXULSortServiceCID,      NS_XULSORTSERVICE_CID);
static NS_DEFINE_IID(kIXULSortServiceIID,     NS_IXULSORTSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kNameSpaceManagerCID,    NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_IID(kINameSpaceManagerIID,   NS_INAMESPACEMANAGER_IID);

static NS_DEFINE_IID(kIContentIID,            NS_ICONTENT_IID);
static NS_DEFINE_IID(kITextContentIID,        NS_ITEXT_CONTENT_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);

static NS_DEFINE_CID(kRDFServiceCID,          NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFResourceIID,        NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,         NS_IRDFLITERAL_IID);

static NS_DEFINE_IID(kIDomXulTreeElementIID,  NS_IDOMXULTREEELEMENT_IID);
static NS_DEFINE_IID(kIDomXulElementIID,      NS_IDOMXULELEMENT_IID);

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
static const char kXULNameSpaceURI[]
    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);



typedef	struct	_sortStruct	{
    nsIRDFService		*rdfService;
    nsIRDFCompositeDataSource	*db;
    nsIRDFResource		*sortProperty;
    PRInt32			colIndex;
    nsIAtom			*kNaturalOrderPosAtom;
    nsIAtom			*kTreeCellAtom;
    PRInt32			kNameSpaceID_XUL;
    PRBool			descendingSort;
    PRBool			naturalOrderSort;
} sortStruct, *sortPtr;



int		openSortCallback(const void *data1, const void *data2, void *sortData);
int		inplaceSortCallback(const void *data1, const void *data2, void *sortData);
nsresult	getNodeValue(nsIContent *node1, nsIRDFResource *sortProperty, sortPtr sortInfo, nsString &cellVal1);
nsresult	GetTreeCell(sortPtr sortInfo, nsIContent *node, PRInt32 cellIndex, nsIContent **cell);
nsresult	GetTreeCellValue(sortPtr sortInfo, nsIContent *node, nsString & val);



////////////////////////////////////////////////////////////////////////
// ServiceImpl
//
//   This is the sort service.
//
class XULSortServiceImpl : public nsIXULSortService
{
protected:
    XULSortServiceImpl(void);
    virtual ~XULSortServiceImpl(void);

    static nsIXULSortService* gXULSortService; // The one-and-only sort service

private:
    static nsrefcnt	gRefCnt;
    static nsIAtom	*kTreeAtom;
    static nsIAtom	*kTreeBodyAtom;
    static nsIAtom	*kTreeCellAtom;
    static nsIAtom	*kTreeChildrenAtom;
    static nsIAtom	*kTreeColAtom;
    static nsIAtom	*kTreeItemAtom;
    static nsIAtom	*kResourceAtom;
    static nsIAtom	*kTreeContentsGeneratedAtom;
    static nsIAtom	*kNameAtom;
    static nsIAtom	*kSortAtom;
    static nsIAtom	*kSortDirectionAtom;
    static nsIAtom	*kIdAtom;
    static nsIAtom	*kNaturalOrderPosAtom;

    static PRInt32	kNameSpaceID_XUL;
    static PRInt32	kNameSpaceID_RDF;

    static nsIRDFService	*gRDFService;

nsresult	FindTreeElement(nsIContent* aElement,nsIContent** aTreeElement);
nsresult	FindTreeBodyElement(nsIContent *tree, nsIContent **treeBody);
nsresult	GetSortColumnIndex(nsIContent *tree, const nsString&sortResource, const nsString& sortDirection, PRInt32 *colIndex);
nsresult	GetSortColumnInfo(nsIContent *tree, nsString &sortResource, nsString &sortDirection);
nsresult	GetTreeCell(nsIContent *node, PRInt32 colIndex, nsIContent **cell);
nsresult	GetTreeCellValue(nsIContent *node, nsString & value);
nsresult	RemoveAllChildren(nsIContent *node);
nsresult	SortTreeChildren(nsIContent *container, PRInt32 colIndex, sortPtr sortInfo, PRInt32 indentLevel);
nsresult	PrintTreeChildren(nsIContent *container, PRInt32 colIndex, PRInt32 indentLevel);

public:

    static nsresult GetSortService(nsIXULSortService** result);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsISortService
    NS_IMETHOD DoSort(nsIDOMNode* node, const nsString& sortResource, const nsString& sortDirection);
    NS_IMETHOD OpenContainer(nsIRDFCompositeDataSource *db, nsIContent *container, nsIRDFResource **flatArray,
				PRInt32 numElements, PRInt32 elementSize);
    NS_IMETHOD InsertContainerNode(nsIContent *container, nsIContent *node);
};

nsIXULSortService* XULSortServiceImpl::gXULSortService = nsnull;
nsrefcnt XULSortServiceImpl::gRefCnt = 0;

nsIRDFService *XULSortServiceImpl::gRDFService = nsnull;

nsIAtom* XULSortServiceImpl::kTreeAtom;
nsIAtom* XULSortServiceImpl::kTreeBodyAtom;
nsIAtom* XULSortServiceImpl::kTreeCellAtom;
nsIAtom* XULSortServiceImpl::kTreeChildrenAtom;
nsIAtom* XULSortServiceImpl::kTreeColAtom;
nsIAtom* XULSortServiceImpl::kTreeItemAtom;
nsIAtom* XULSortServiceImpl::kResourceAtom;
nsIAtom* XULSortServiceImpl::kTreeContentsGeneratedAtom;
nsIAtom* XULSortServiceImpl::kNameAtom;
nsIAtom* XULSortServiceImpl::kSortAtom;
nsIAtom* XULSortServiceImpl::kSortDirectionAtom;
nsIAtom* XULSortServiceImpl::kIdAtom;
nsIAtom* XULSortServiceImpl::kNaturalOrderPosAtom;

PRInt32  XULSortServiceImpl::kNameSpaceID_XUL;
PRInt32  XULSortServiceImpl::kNameSpaceID_RDF;

////////////////////////////////////////////////////////////////////////



XULSortServiceImpl::XULSortServiceImpl(void)
{
	NS_INIT_REFCNT();
	if (gRefCnt == 0)
	{
	        kTreeAtom            		= NS_NewAtom("tree");
	        kTreeBodyAtom        		= NS_NewAtom("treebody");
	        kTreeCellAtom        		= NS_NewAtom("treecell");
		kTreeChildrenAtom    		= NS_NewAtom("treechildren");
		kTreeColAtom         		= NS_NewAtom("treecol");
		kTreeItemAtom        		= NS_NewAtom("treeitem");
		kResourceAtom        		= NS_NewAtom("resource");
		kTreeContentsGeneratedAtom	= NS_NewAtom("treecontentsgenerated");
		kNameAtom			= NS_NewAtom("Name");
		kSortAtom			= NS_NewAtom("sortActive");
		kSortDirectionAtom		= NS_NewAtom("sortDirection");
		kIdAtom				= NS_NewAtom("id");
		kNaturalOrderPosAtom		= NS_NewAtom("pos");
 
		nsresult rv;

		if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
						  kIRDFServiceIID, (nsISupports**) &gRDFService)))
		{
			NS_ERROR("couldn't create rdf service");
		}
	        // Register the XUL and RDF namespaces: these'll just retrieve
	        // the IDs if they've already been registered by someone else.
		nsINameSpaceManager* mgr;
		if (NS_SUCCEEDED(rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
		                           nsnull,
		                           kINameSpaceManagerIID,
		                           (void**) &mgr)))
		{

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
static const char kXULNameSpaceURI[]
    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

static const char kRDFNameSpaceURI[]
    = RDF_NAMESPACE_URI;

			rv = mgr->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
			NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");

			rv = mgr->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
			NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");

			NS_RELEASE(mgr);
		}
		else
		{
			NS_ERROR("couldn't create namepsace manager");
		}
	}
	++gRefCnt;
}



XULSortServiceImpl::~XULSortServiceImpl(void)
{
	--gRefCnt;
	if (gRefCnt == 0)
	{
		NS_RELEASE(kTreeAtom);
		NS_RELEASE(kTreeBodyAtom);
		NS_RELEASE(kTreeCellAtom);
	        NS_RELEASE(kTreeChildrenAtom);
	        NS_RELEASE(kTreeColAtom);
	        NS_RELEASE(kTreeItemAtom);
	        NS_RELEASE(kResourceAtom);
	        NS_RELEASE(kTreeContentsGeneratedAtom);
	        NS_RELEASE(kNameAtom);
	        NS_RELEASE(kSortAtom);
	        NS_RELEASE(kSortDirectionAtom);
	        NS_RELEASE(kIdAtom);
	        NS_RELEASE(kNaturalOrderPosAtom);

		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
		gRDFService = nsnull;
		nsServiceManager::ReleaseService(kXULSortServiceCID, gXULSortService);
		gXULSortService = nsnull;
	}
}



nsresult
XULSortServiceImpl::GetSortService(nsIXULSortService** mgr)
{
    if (! gXULSortService) {
        gXULSortService = new XULSortServiceImpl();
        if (! gXULSortService)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(gXULSortService);
    *mgr = gXULSortService;
    return NS_OK;
}



NS_IMETHODIMP_(nsrefcnt)
XULSortServiceImpl::AddRef(void)
{
    return 2;
}



NS_IMETHODIMP_(nsrefcnt)
XULSortServiceImpl::Release(void)
{
    return 1;
}



NS_IMPL_QUERY_INTERFACE(XULSortServiceImpl, kIXULSortServiceIID);



////////////////////////////////////////////////////////////////////////



nsresult
XULSortServiceImpl::FindTreeElement(nsIContent *aElement, nsIContent **aTreeElement)
{
	nsresult rv;
	nsCOMPtr<nsIContent> element(do_QueryInterface(aElement));

	while (element)
	{
		PRInt32 nameSpaceID;
		if (NS_FAILED(rv = element->GetNameSpaceID(nameSpaceID)))	return rv;
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;
			if (NS_FAILED(rv = element->GetTag(*getter_AddRefs(tag))))	return rv;
			if (tag.get() == kTreeAtom)
			{
				*aTreeElement = element;
				NS_ADDREF(*aTreeElement);
				return NS_OK;
			}
		}
		nsCOMPtr<nsIContent> parent;
		element->GetParent(*getter_AddRefs(parent));
		element = parent;
	}
	return(NS_ERROR_FAILURE);
}



nsresult
XULSortServiceImpl::FindTreeBodyElement(nsIContent *tree, nsIContent **treeBody)
{
        nsCOMPtr<nsIContent>	child;
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
	nsresult		rv;

	if (NS_FAILED(rv = tree->ChildCount(numChildren)))	return(rv);
	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = tree->ChildAt(childIndex, *getter_AddRefs(child))))	return(rv);
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	return rv;
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;
			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))	return rv;
			if (tag.get() == kTreeBodyAtom)
			{
				*treeBody = child;
				NS_ADDREF(*treeBody);
				return NS_OK;
			}
		}
	}
	return(NS_ERROR_FAILURE);
}



nsresult
XULSortServiceImpl::GetSortColumnIndex(nsIContent *tree, const nsString& sortResource, const nsString& sortDirection, PRInt32 *sortColIndex)
{
	PRBool			found = PR_FALSE;
	PRInt32			childIndex, colIndex = 0, numChildren, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;

	*sortColIndex = 0;
	if (NS_FAILED(rv = tree->ChildCount(numChildren)))	return(rv);
	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = tree->ChildAt(childIndex, *getter_AddRefs(child))))	return(rv);
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	return(rv);
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;

			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))
				return rv;
			if (tag.get() == kTreeColAtom)
			{
				nsString	colResource;

				if (NS_OK == child->GetAttribute(kNameSpaceID_RDF, kResourceAtom, colResource))
				{
					if (colResource == sortResource)
					{
						nsString	trueStr("true");
						child->SetAttribute(kNameSpaceID_None, kSortAtom, trueStr, PR_TRUE);
						child->SetAttribute(kNameSpaceID_None, kSortDirectionAtom, sortDirection, PR_TRUE);
						*sortColIndex = colIndex;
						found = PR_TRUE;
						// Note: don't break, want to set/unset attribs on ALL sort columns
						// break;
					}
					else
					{
						nsString	falseStr("false");
						child->SetAttribute(kNameSpaceID_None, kSortAtom, falseStr, PR_TRUE);
						child->SetAttribute(kNameSpaceID_None, kSortDirectionAtom, sortDirection, PR_TRUE);
					}
				}
				++colIndex;
			}
		}
	}
	return((found == PR_TRUE) ? NS_OK : NS_ERROR_FAILURE);
}



nsresult
XULSortServiceImpl::GetSortColumnInfo(nsIContent *tree, nsString &sortResource, nsString &sortDirection)
{
        nsCOMPtr<nsIContent>	child;
	PRBool			found = PR_FALSE;
	PRInt32			childIndex, numChildren, nameSpaceID;
	nsresult		rv;

	if (NS_FAILED(rv = tree->ChildCount(numChildren)))	return(rv);
	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = tree->ChildAt(childIndex, *getter_AddRefs(child))))	return(rv);
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	return(rv);
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;

			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))
				return rv;
			if (tag.get() == kTreeColAtom)
			{
				nsString	value;
				if (NS_OK == child->GetAttribute(kNameSpaceID_None, kSortAtom, value))
				{
					if (value.EqualsIgnoreCase("true"))
					{
						if (NS_OK == child->GetAttribute(kNameSpaceID_RDF, kResourceAtom, sortResource))
						{
							if (NS_OK == child->GetAttribute(kNameSpaceID_None, kSortDirectionAtom, sortDirection))
							{
								found = PR_TRUE;
							}
						}
						break;
					}
				}
			}
		}
	}
	return((found == PR_TRUE) ? NS_OK : NS_ERROR_FAILURE);
}



nsresult
GetTreeCell(sortPtr sortInfo, nsIContent *node, PRInt32 cellIndex, nsIContent **cell)
{
	PRBool			found = PR_FALSE;
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;

	if (NS_FAILED(rv = node->ChildCount(numChildren)))	return(rv);

	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = node->ChildAt(childIndex, *getter_AddRefs(child))))	break;
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	break;
		if (nameSpaceID == sortInfo->kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;
			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))	return rv;
			if (tag.get() == sortInfo->kTreeCellAtom)
			{
				if (cellIndex == 0)
				{
					found = PR_TRUE;
					*cell = child;
					NS_ADDREF(*cell);
					break;
				}
				--cellIndex;
			}
		}
	}
	return((found == PR_TRUE) ? NS_OK : NS_ERROR_FAILURE);
}



nsresult
GetTreeCellValue(sortPtr sortInfo, nsIContent *node, nsString & val)
{
	PRBool			found = PR_FALSE;
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;

	if (NS_FAILED(rv = node->ChildCount(numChildren)))	return(rv);

	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = node->ChildAt(childIndex, *getter_AddRefs(child))))	break;;
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	break;
		// XXX is this the correct way to get text?  Probably not...
		if (nameSpaceID != sortInfo->kNameSpaceID_XUL)
		{
			nsITextContent	*text = nsnull;
			if (NS_SUCCEEDED(rv = child->QueryInterface(kITextContentIID, (void **)&text)))
			{
				const nsTextFragment	*textFrags;
				PRInt32		numTextFrags;
				if (NS_SUCCEEDED(rv = text->GetText(textFrags, numTextFrags)))
				{
					const char *value = textFrags->Get1b();
					PRInt32 len = textFrags->GetLength();
					val.SetString(value, len);
					found = PR_TRUE;
					break;
				}
			}
		}
	}
	return((found == PR_TRUE) ? NS_OK : NS_ERROR_FAILURE);
}



nsresult
XULSortServiceImpl::RemoveAllChildren(nsIContent *container)
{
        nsCOMPtr<nsIContent>	child;
	PRInt32			childIndex, numChildren;
	nsresult		rv;

	if (NS_FAILED(rv = container->ChildCount(numChildren)))	return(rv);
	if (numChildren == 0)	return(NS_OK);

	for (childIndex=numChildren-1; childIndex >= 0; childIndex--)
	{
		if (NS_FAILED(rv = container->ChildAt(childIndex, *getter_AddRefs(child))))	break;
		container->RemoveChildAt(childIndex, PR_FALSE);
	}
	return(rv);
}



int
openSortCallback(const void *data1, const void *data2, void *sortData)
{
	int		sortOrder = 0;
	nsresult	rv;

	nsIRDFNode	*node1, *node2;
	node1 = *(nsIRDFNode **)data1;
	node2 = *(nsIRDFNode **)data2;
	_sortStruct	*sortPtr = (_sortStruct *)sortData;

	nsIRDFResource	*res1;
	nsIRDFResource	*res2;
	nsXPIDLString	uniStr1;
	nsXPIDLString	uniStr2;

	if (NS_SUCCEEDED(node1->QueryInterface(kIRDFResourceIID, (void **) &res1)))
	{
		nsIRDFNode	*nodeVal1;
		if (NS_SUCCEEDED(rv = sortPtr->db->GetTarget(res1, sortPtr->sortProperty, PR_TRUE, &nodeVal1)))
		{
			nsIRDFLiteral *literal1;
			if (NS_SUCCEEDED(nodeVal1->QueryInterface(kIRDFLiteralIID, (void **) &literal1)))
			{
				literal1->GetValue( getter_Copies(uniStr1) );
				NS_RELEASE(literal1);
			}
		}
		NS_RELEASE(res1);
	}
	if (NS_SUCCEEDED(node2->QueryInterface(kIRDFResourceIID, (void **) &res2)))
	{
		nsIRDFNode	*nodeVal2;
		if (NS_SUCCEEDED(rv = sortPtr->db->GetTarget(res2, sortPtr->sortProperty, PR_TRUE, &nodeVal2)))
		{
			nsIRDFLiteral	*literal2;
			if (NS_SUCCEEDED(nodeVal2->QueryInterface(kIRDFLiteralIID, (void **) &literal2)))
			{
				literal2->GetValue( getter_Copies(uniStr2) );
				NS_RELEASE(literal2);
			}
		}
		NS_RELEASE(res2);
	}
	if ((uniStr1 != nsnull) && (uniStr2 != nsnull))
	{
		nsAutoString	str1(uniStr1), str2(uniStr2);
		sortOrder = (int)str1.Compare(str2, PR_TRUE);
		if (sortPtr->descendingSort == PR_TRUE)
		{
			sortOrder = -sortOrder;
		}
	}
	else if ((uniStr1 != nsnull) && (uniStr2 == nsnull))
	{
		sortOrder = -1;
	}
	else if ((uniStr1 == nsnull) && (uniStr2 != nsnull))
	{
		sortOrder = 1;
	}
	return(sortOrder);
}



nsresult
getNodeValue(nsIContent *node1, nsIRDFResource *sortProperty, sortPtr sortInfo, nsString &cellVal1)
{
	nsIDOMXULElement	*dom1;
	nsIRDFResource		*res1;
	nsresult		rv;

	cellVal1 = "";
	if (NS_SUCCEEDED(rv = node1->QueryInterface(kIDomXulElementIID, (void **)&dom1)))
	{
		if (NS_SUCCEEDED(rv = dom1->GetResource(&res1)))
		{
			if ((sortInfo->naturalOrderSort == PR_FALSE) && (sortInfo->sortProperty))
			{
				nsIRDFNode	*target1 = nsnull;

				// for any given property, first ask the graph for its value with "?sort=true" appended
				// to indicate that if there is any distinction between its display value and sorting
				// value, we want the sorting value (so that, for example, a mail datasource could strip
				// off a "Re:" on a mail message subject)

				nsXPIDLCString	sortPropertyURI;
				sortInfo->sortProperty->GetValue( getter_Copies(sortPropertyURI) );
				if (sortPropertyURI)
				{
					nsAutoString	modSortProperty(sortPropertyURI);
					modSortProperty += "?sort=true";
					char	*sortProp = modSortProperty.ToNewCString();
					if (sortProp)
					{
						nsIRDFResource	*modSortRes = nsnull;
						if (NS_SUCCEEDED(sortInfo->rdfService->GetResource(sortProp, &modSortRes)))
						{
							if (modSortRes)
							{
								if (NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(res1, modSortRes, PR_TRUE, &target1)) &&
									(rv != NS_RDF_NO_VALUE))
								{
									nsIRDFLiteral *literal1;
									if (NS_SUCCEEDED(target1->QueryInterface(kIRDFLiteralIID, (void **) &literal1)))
									{
										nsXPIDLString uniStr1;
										literal1->GetValue( getter_Copies(uniStr1) );
										cellVal1 = uniStr1;
										NS_RELEASE(literal1);
									}
									NS_RELEASE(target1);
								}
								NS_RELEASE(modSortRes);
							}
						}
						delete []sortProp;
					}
				}

				if (cellVal1.Length() == 0)
				{
					if (NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(res1, sortProperty, PR_TRUE, &target1)) &&
						(rv != NS_RDF_NO_VALUE))
					{
						nsIRDFLiteral *literal1;
						if (NS_SUCCEEDED(target1->QueryInterface(kIRDFLiteralIID, (void **) &literal1)))
						{
							nsXPIDLString uniStr1;
							literal1->GetValue( getter_Copies(uniStr1) );
							cellVal1 = uniStr1;
							NS_RELEASE(literal1);
						}
						NS_RELEASE(target1);
					}
					else
					{
					        nsIContent	*cell1 = nsnull;
						if (NS_SUCCEEDED(rv = GetTreeCell(sortInfo, node1, sortInfo->colIndex, &cell1)))
						{
							if (cell1)
							{
								if (NS_SUCCEEDED(rv = GetTreeCellValue(sortInfo, cell1, cellVal1)))
								{
								}
							}
						}
					}
				}
			}
			else if (sortInfo->naturalOrderSort == PR_TRUE)
			{
				if (NS_OK == node1->GetAttribute(kNameSpaceID_None, sortInfo->kNaturalOrderPosAtom, cellVal1))
				{
				}
			}
			NS_RELEASE(res1);
		}
		NS_RELEASE(dom1);
	}
	return(rv);
}



int
inplaceSortCallback(const void *data1, const void *data2, void *sortData)
{
	_sortStruct		*sortInfo = (_sortStruct *)sortData;
	PRInt32			sortOrder = 0;
	nsIContent		*node1 = *(nsIContent **)data1;
	nsIContent		*node2 = *(nsIContent **)data2;
	nsIDOMXULElement	*dom1 = nsnull, *dom2 = nsnull;
	nsIRDFResource		*res1 = nsnull, *res2 = nsnull;
	nsAutoString		cellVal1(""), cellVal2("");
	nsresult		rv;
	PRBool			sortOnName = PR_FALSE;

	if (NS_FAILED(rv = getNodeValue(node1, sortInfo->sortProperty, sortInfo, cellVal1)))
	{
		nsIRDFResource	*name;
		sortInfo->rdfService->GetResource(kURINC_Name, &name);
		if (name)
		{
			rv = getNodeValue(node1, name, sortInfo, cellVal1);
			NS_RELEASE(name);
		}
	}
	if (NS_FAILED(rv = getNodeValue(node2, sortInfo->sortProperty, sortInfo, cellVal2)))
	{
		nsIRDFResource	*name;
		sortInfo->rdfService->GetResource(kURINC_Name, &name);
		if (name)
		{
			rv = getNodeValue(node2, sortInfo->sortProperty, sortInfo, cellVal2);
			NS_RELEASE(name);
		}
	}
	sortOrder = (int)cellVal1.Compare(cellVal2, PR_TRUE);
	if (sortInfo->descendingSort == PR_TRUE)
	{
		sortOrder = -sortOrder;
	}
	return(sortOrder);
}



nsresult
XULSortServiceImpl::SortTreeChildren(nsIContent *container, PRInt32 colIndex, sortPtr sortInfo, PRInt32 indentLevel)
{
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;
	nsVoidArray		*childArray;

	if (NS_FAILED(rv = container->ChildCount(numChildren)))	return(rv);

        if ((childArray = new nsVoidArray()) == nsnull)	return (NS_ERROR_OUT_OF_MEMORY);

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
				childArray->AppendElement(child);
				
				// if no pos is specified, set one
				nsAutoString	pos;
				if (NS_OK != child->GetAttribute(kNameSpaceID_None, kNaturalOrderPosAtom, pos))
				{
					nsAutoString	zero("0000");
					pos = "";
					pos.Append(childIndex+1, 10);
					if (pos.Length() < 4)
					{
						pos.Insert(zero, 0, 4-pos.Length()); 
					}
					child->SetAttribute(kNameSpaceID_None, kNaturalOrderPosAtom, pos, PR_FALSE);
				}
			}
		}
	}
	unsigned long numElements = childArray->Count();
	if (numElements > 0)
	{
		nsIContent ** flatArray = new nsIContent*[numElements];
		if (flatArray)
		{
			// flatten array of resources, sort them, then add as tree elements
			unsigned long loop;
		        for (loop=0; loop<numElements; loop++)
		        {
				flatArray[loop] = (nsIContent *)childArray->ElementAt(loop);
			}

			nsQuickSort((void *)flatArray, numElements, sizeof(nsIContent *),
				inplaceSortCallback, (void *)sortInfo);

			RemoveAllChildren(container);
			if (NS_FAILED(rv = container->UnsetAttribute(kNameSpaceID_None,
			                        kTreeContentsGeneratedAtom,
			                        PR_FALSE)))
			{
				printf("unable to clear contents-generated attribute\n");
			}
			
			// insert sorted children			
			numChildren = 0;
			for (loop=0; loop<numElements; loop++)
			{
				container->InsertChildAt((nsIContent *)flatArray[loop], numChildren++, PR_FALSE);
			}

			// recurse on grandchildren
			for (loop=0; loop<numElements; loop++)
			{
				container =  (nsIContent *)flatArray[loop];
				if (NS_FAILED(rv = container->ChildCount(numChildren)))	continue;
				for (childIndex=0; childIndex<numChildren; childIndex++)
				{
					if (NS_FAILED(rv = container->ChildAt(childIndex, *getter_AddRefs(child))))
						continue;
					if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	continue;
					if (nameSpaceID == kNameSpaceID_XUL)
					{
						nsCOMPtr<nsIAtom> tag;
						if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))
							continue;
						if (tag.get() == kTreeChildrenAtom)
						{
							SortTreeChildren(child, colIndex, sortInfo, indentLevel+1);
						}
					}
				}
			}

			delete [] flatArray;
			flatArray = nsnull;
		}
	}
	return(NS_OK);
}



NS_IMETHODIMP
XULSortServiceImpl::OpenContainer(nsIRDFCompositeDataSource *db, nsIContent *container,
			nsIRDFResource **flatArray, PRInt32 numElements, PRInt32 elementSize)
{
	nsresult	rv;
	nsIContent	*treeNode;
	nsString	sortResource, sortDirection;
	_sortStruct	sortInfo;

	// get sorting info (property to sort on, direction to sort, etc)

	if (NS_FAILED(rv = FindTreeElement(container, &treeNode)))	return(rv);

	sortInfo.rdfService = gRDFService;
	sortInfo.db = db;

	sortInfo.kNaturalOrderPosAtom = kNaturalOrderPosAtom;
	sortInfo.kTreeCellAtom = kTreeCellAtom;
	sortInfo.kNameSpaceID_XUL = kNameSpaceID_XUL;

	if (NS_FAILED(rv = GetSortColumnInfo(treeNode, sortResource, sortDirection)))	return(rv);
	char *uri = sortResource.ToNewCString();
	if (uri)
	{
		rv = gRDFService->GetResource(uri, &sortInfo.sortProperty);
		delete [] uri;
		if (NS_FAILED(rv))	return(rv);
	}
	if (sortDirection.EqualsIgnoreCase("natural"))
	{
		sortInfo.naturalOrderSort = PR_TRUE;
		sortInfo.descendingSort = PR_FALSE;
		// no need to sort for natural order
	}
	else
	{
		sortInfo.naturalOrderSort = PR_FALSE;
		if (sortDirection.EqualsIgnoreCase("descending"))
			sortInfo.descendingSort = PR_TRUE;
		else
			sortInfo.descendingSort = PR_FALSE;
		nsQuickSort((void *)flatArray, numElements, elementSize, openSortCallback, (void *)&sortInfo);
	}
	return(NS_OK);
}



NS_IMETHODIMP
XULSortServiceImpl::InsertContainerNode(nsIContent *container, nsIContent *node)
{
	nsresult	rv;
	nsIContent	*treeNode;
	nsString	sortResource, sortDirection;
	_sortStruct	sortInfo;

	// get sorting info (property to sort on, direction to sort, etc)

	if (NS_FAILED(rv = FindTreeElement(container, &treeNode)))	return(rv);

	// get composite db for tree
	nsIDOMXULTreeElement	*domXulTree;
	sortInfo.rdfService = gRDFService;
	sortInfo.db = nsnull;
	if (NS_SUCCEEDED(rv = treeNode->QueryInterface(kIDomXulTreeElementIID, (void**)&domXulTree)))
	{
		if (NS_SUCCEEDED(rv = domXulTree->GetDatabase(&sortInfo.db)))
		{
		}
	}

	sortInfo.kNaturalOrderPosAtom = kNaturalOrderPosAtom;
	sortInfo.kTreeCellAtom = kTreeCellAtom;
	sortInfo.kNameSpaceID_XUL = kNameSpaceID_XUL;

	PRBool			childAdded = PR_FALSE;
	if (NS_SUCCEEDED(rv = GetSortColumnInfo(treeNode, sortResource, sortDirection)))
	{
		char *uri = sortResource.ToNewCString();
		if (uri)
		{
			rv = gRDFService->GetResource(uri, &sortInfo.sortProperty);
			delete [] uri;
			if (NS_FAILED(rv))	return(rv);
		}
		if (sortDirection.EqualsIgnoreCase("natural"))
		{
			sortInfo.naturalOrderSort = PR_TRUE;
			sortInfo.descendingSort = PR_FALSE;
			// no need to sort for natural order
			container->AppendChildTo(node, PR_TRUE);
		}
		else
		{
			sortInfo.naturalOrderSort = PR_FALSE;
			if (sortDirection.EqualsIgnoreCase("descending"))
				sortInfo.descendingSort = PR_TRUE;
			else
				sortInfo.descendingSort = PR_FALSE;

			// figure out where to insert the node when a sort order is being imposed
			PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
		        nsCOMPtr<nsIContent>	child;
			nsresult		rv;

			if (NS_FAILED(rv = container->ChildCount(numChildren)))	return(rv);
			for (childIndex=0; childIndex<numChildren; childIndex++)
			{
				if (NS_FAILED(rv = container->ChildAt(childIndex, *getter_AddRefs(child))))	return(rv);
				if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	return(rv);
				if (nameSpaceID == kNameSpaceID_XUL)
				{
					nsIContent	*theChild = child.get();
					PRInt32 sortVal = inplaceSortCallback(&node, &theChild, &sortInfo);
					if (sortVal <= 0)
					{
						container->InsertChildAt(node, childIndex, PR_TRUE);
						childAdded = PR_TRUE;
						break;
					}
				}
			}
		}
	}
	if (childAdded == PR_FALSE)
	{
		container->AppendChildTo(node, PR_TRUE);
	}
	return(NS_OK);
}



nsresult
XULSortServiceImpl::PrintTreeChildren(nsIContent *container, PRInt32 colIndex, PRInt32 indentLevel)
{
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;

	if (NS_FAILED(rv = container->ChildCount(numChildren)))	return(rv);
	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = container->ChildAt(childIndex, *getter_AddRefs(child))))	return(rv);
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	return(rv);
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;

			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))
				return rv;
			nsString	tagName;
			tag->ToString(tagName);
			for (PRInt32 indentLoop=0; indentLoop<indentLevel; indentLoop++) printf("    ");
			printf("Child #%d: tagName='%s'\n", childIndex, tagName.ToNewCString());

			PRInt32		attribIndex, numAttribs;
			child->GetAttributeCount(numAttribs);
			for (attribIndex = 0; attribIndex < numAttribs; attribIndex++)
			{
				PRInt32			attribNameSpaceID;
				nsCOMPtr<nsIAtom> 	attribAtom;

				if (NS_SUCCEEDED(rv = child->GetAttributeNameAt(attribIndex, attribNameSpaceID,
					*getter_AddRefs(attribAtom))))
				{
					nsString	attribName, attribValue;
					attribAtom->ToString(attribName);
					rv = child->GetAttribute(attribNameSpaceID, attribAtom, attribValue);
					if (rv == NS_CONTENT_ATTR_HAS_VALUE)
					{
						for (PRInt32 indentLoop=0; indentLoop<indentLevel; indentLoop++) printf("    ");
						printf("Attrib #%d: name='%s' value='%s'\n", attribIndex,
							attribName.ToNewCString(),
							attribValue.ToNewCString());
					}
				}
			}
			if ((tag.get() == kTreeItemAtom) || (tag.get() == kTreeChildrenAtom) || (tag.get() == kTreeCellAtom))
			{
				PrintTreeChildren(child, colIndex, indentLevel+1);
			}
		}
		else
		{
			for (PRInt32 loop=0; loop<indentLevel; loop++) printf("    ");
			printf("(Non-XUL node)  ");
			nsITextContent	*text = nsnull;
			if (NS_SUCCEEDED(rv = child->QueryInterface(kITextContentIID, (void **)&text)))
			{
				for (PRInt32 indentLoop=0; indentLoop<indentLevel; indentLoop++) printf("    ");
				printf("(kITextContentIID)  ");

				const nsTextFragment	*textFrags;
				PRInt32		numTextFrags;
				if (NS_SUCCEEDED(rv = text->GetText(textFrags, numTextFrags)))
				{
					const char *val = textFrags->Get1b();
					PRInt32 len = textFrags->GetLength();
					if (val)	printf("value='%.*s'", len, val);
				}
			}
			printf("\n");
		}
	}
	return(NS_OK);
}



NS_IMETHODIMP
XULSortServiceImpl::DoSort(nsIDOMNode* node, const nsString& sortResource,
                           const nsString& sortDirection)
{
	PRInt32		colIndex, treeBodyIndex;
	nsIContent	*contentNode, *treeNode, *treeBody, *treeParent;
	nsresult	rv;
	_sortStruct	sortInfo;

	// get tree node
	if (NS_FAILED(rv = node->QueryInterface(kIContentIID, (void**)&contentNode)))	return(rv);
	if (NS_FAILED(rv = FindTreeElement(contentNode, &treeNode)))	return(rv);

	// get composite db for tree
	nsIDOMXULTreeElement	*domXulTree;
	sortInfo.rdfService = gRDFService;
	sortInfo.db = nsnull;
	if (NS_SUCCEEDED(rv = treeNode->QueryInterface(kIDomXulTreeElementIID, (void**)&domXulTree)))
	{
		if (NS_SUCCEEDED(rv = domXulTree->GetDatabase(&sortInfo.db)))
		{
		}
	}

	sortInfo.kNaturalOrderPosAtom = kNaturalOrderPosAtom;
	sortInfo.kTreeCellAtom = kTreeCellAtom;
	sortInfo.kNameSpaceID_XUL = kNameSpaceID_XUL;

	char *uri = sortResource.ToNewCString();
	if (uri)
	{
		rv = gRDFService->GetResource(uri, &sortInfo.sortProperty);
		delete [] uri;
		if (NS_FAILED(rv))	return(rv);
	}
	
	// determine new sort resource and direction to use
	if (sortDirection.EqualsIgnoreCase("natural"))
	{
		sortInfo.naturalOrderSort = PR_TRUE;
		sortInfo.descendingSort = PR_FALSE;
	}
	else
	{
		sortInfo.naturalOrderSort = PR_FALSE;
		if (sortDirection.EqualsIgnoreCase("ascending"))	sortInfo.descendingSort = PR_FALSE;
		else if (sortDirection.EqualsIgnoreCase("descending"))	sortInfo.descendingSort = PR_TRUE;
	}

	// get index of sort column, find tree body, and sort. The sort
	// _won't_ send any notifications, so we won't trigger any
	// reflows...
	if (NS_FAILED(rv = GetSortColumnIndex(treeNode, sortResource, sortDirection, &colIndex)))	return(rv);
	sortInfo.colIndex = colIndex;
	if (NS_FAILED(rv = FindTreeBodyElement(treeNode, &treeBody)))	return(rv);
	if (NS_SUCCEEDED(rv = SortTreeChildren(treeBody, colIndex, &sortInfo, 0)))
	{
	}

    // Now remove the treebody and re-insert it to force the frames to be rebuilt.
	if (NS_FAILED(rv = treeBody->GetParent(treeParent)))	return(rv);
	if (NS_FAILED(rv = treeParent->IndexOf(treeBody, treeBodyIndex)))	return(rv);
	if (NS_FAILED(rv = treeParent->RemoveChildAt(treeBodyIndex, PR_TRUE)))	return(rv);

    if (NS_SUCCEEDED(rv = treeBody->UnsetAttribute(kNameSpaceID_None,
		kTreeContentsGeneratedAtom,PR_FALSE)))
	{
	}
	if (NS_SUCCEEDED(rv = treeParent->UnsetAttribute(kNameSpaceID_None,
		kTreeContentsGeneratedAtom,PR_FALSE)))
	{
	}

	if (NS_FAILED(rv = treeParent->AppendChildTo(treeBody, PR_TRUE)))	return(rv);

#if 0
	if (NS_FAILED(rv = PrintTreeChildren(treeBody, colIndex, 0)))	return(rv);
#endif
	return(NS_OK);
}



nsresult
NS_NewXULSortService(nsIXULSortService** mgr)
{
    return XULSortServiceImpl::GetSortService(mgr);
}
