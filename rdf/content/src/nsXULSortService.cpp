/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-file-style: "stroustrup" -*-
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
#include "prlong.h"
#include "prlog.h"

#include "nsICollation.h"
#include "nsCollationCID.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"

#include "nsIContent.h"
#include "nsIDOMText.h"

#include "nsVoidArray.h"

#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "rdf.h"

#include "nsIDOMXULElement.h"

#include "nsILocale.h"
#include "nsLocaleCID.h"
#include "nsILocaleFactory.h"

#define	XUL_BINARY_INSERTION_SORT	1


////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kXULSortServiceCID,      NS_XULSORTSERVICE_CID);
static NS_DEFINE_IID(kIXULSortServiceIID,     NS_IXULSORTSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kNameSpaceManagerCID,    NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_IID(kINameSpaceManagerIID,   NS_INAMESPACEMANAGER_IID);

static NS_DEFINE_IID(kIContentIID,            NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMTextIID,            NS_IDOMTEXT_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);

static NS_DEFINE_CID(kRDFServiceCID,          NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFResourceIID,        NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,         NS_IRDFLITERAL_IID);

static NS_DEFINE_IID(kIDomXulElementIID,      NS_IDOMXULELEMENT_IID);

static NS_DEFINE_CID(kCollationFactoryCID,    NS_COLLATIONFACTORY_CID);
static NS_DEFINE_IID(kICollationFactoryIID,   NS_ICOLLATIONFACTORY_IID);

static NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
static NS_DEFINE_IID(kILocaleFactoryIID, NS_ILOCALEFACTORY_IID);
static NS_DEFINE_CID(kLocaleCID, NS_LOCALE_CID);
static NS_DEFINE_IID(kILocaleIID, NS_ILOCALE_IID);

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
static const char kXULNameSpaceURI[]
    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, BookmarkSeparator);


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



int		openSortCallback(const void *data1, const void *data2, void *privateData);
int		inplaceSortCallback(const void *data1, const void *data2, void *privateData);



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

    static nsIXULSortService	*gXULSortService;
    static nsICollation		*collationService;

private:
    static nsrefcnt	gRefCnt;
    static nsIAtom	*kTreeAtom;
    static nsIAtom	*kTreeBodyAtom;
    static nsIAtom	*kTreeCellAtom;
    static nsIAtom	*kTreeChildrenAtom;
    static nsIAtom	*kTreeColAtom;
    static nsIAtom	*kTreeItemAtom;
    static nsIAtom	*kResourceAtom;
    static nsIAtom	*kNameAtom;
    static nsIAtom	*kSortAtom;
    static nsIAtom	*kSortDirectionAtom;
    static nsIAtom	*kIdAtom;
    static nsIAtom	*kNaturalOrderPosAtom;
    static nsIAtom	*kRDF_type;
    static nsIAtom	*kURIAtom;

    static nsIRDFResource	*kNC_Name;
    static nsIRDFResource	*kRDF_instanceOf;
    static nsIRDFResource	*kRDF_Seq;

    static PRInt32	kNameSpaceID_XUL;
    static PRInt32	kNameSpaceID_RDF;

    static nsIRDFService	*gRDFService;

nsresult	FindTreeElement(nsIContent* aElement,nsIContent** aTreeElement);
nsresult	FindTreeChildrenElement(nsIContent *tree, nsIContent **treeBody);
nsresult	GetSortColumnIndex(nsIContent *tree, const nsString&sortResource, const nsString& sortDirection, PRInt32 *colIndex);
nsresult	GetSortColumnInfo(nsIContent *tree, nsString &sortResource, nsString &sortDirection);
nsresult	GetTreeCell(nsIContent *node, PRInt32 colIndex, nsIContent **cell);
nsresult	GetTreeCellValue(nsIContent *node, nsString & value);
nsresult	RemoveAllChildren(nsIContent *node);
nsresult	SortTreeChildren(nsIContent *container, PRInt32 colIndex, sortPtr sortInfo, PRInt32 indentLevel);

static nsresult	GetResourceValue(nsIRDFResource *res1, nsIRDFResource *sortProperty, sortPtr sortInfo, nsIRDFNode **, PRBool &isCollationKey);
static nsresult	GetNodeValue(nsIContent *node1, nsIRDFResource *sortProperty, sortPtr sortInfo, nsIRDFNode **, PRBool &isCollationKey);
static nsresult	GetTreeCell(sortPtr sortInfo, nsIContent *node, PRInt32 cellIndex, nsIContent **cell);
static nsresult	GetTreeCellValue(sortPtr sortInfo, nsIContent *node, nsString & val);

public:

    static nsresult GetSortService(nsIXULSortService** result);

    static nsresult	ImplaceSort(nsIContent *node1, nsIContent *node2, sortPtr sortInfo, PRInt32 & sortOrder);
    static nsresult	OpenSort(nsIRDFNode *node1, nsIRDFNode *node2, sortPtr sortInfo, PRInt32 & sortOrder);
    static nsresult	CompareNodes(nsIRDFNode *cellNode1, PRBool isCollationKey1,
				 nsIRDFNode *cellNode2, PRBool isCollationKey2,
				 PRInt32 & sortOrder);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsISortService
    NS_IMETHOD Sort(nsIDOMNode* node, const char *sortResource, const char *sortDirection);
    NS_IMETHOD DoSort(nsIDOMNode* node, const nsString& sortResource, const nsString& sortDirection);
    NS_IMETHOD OpenContainer(nsIRDFCompositeDataSource *db, nsIContent *container, nsIRDFResource **flatArray,
				PRInt32 numElements, PRInt32 elementSize);
    NS_IMETHOD InsertContainerNode(nsIContent *container, nsIContent *node, PRBool aNotify);
};

nsIXULSortService	*XULSortServiceImpl::gXULSortService = nsnull;
nsICollation		*XULSortServiceImpl::collationService = nsnull;
nsrefcnt XULSortServiceImpl::gRefCnt = 0;

nsIRDFService *XULSortServiceImpl::gRDFService = nsnull;

nsIAtom* XULSortServiceImpl::kTreeAtom;
nsIAtom* XULSortServiceImpl::kTreeBodyAtom;
nsIAtom* XULSortServiceImpl::kTreeCellAtom;
nsIAtom* XULSortServiceImpl::kTreeChildrenAtom;
nsIAtom* XULSortServiceImpl::kTreeColAtom;
nsIAtom* XULSortServiceImpl::kTreeItemAtom;
nsIAtom* XULSortServiceImpl::kResourceAtom;
nsIAtom* XULSortServiceImpl::kNameAtom;
nsIAtom* XULSortServiceImpl::kSortAtom;
nsIAtom* XULSortServiceImpl::kSortDirectionAtom;
nsIAtom* XULSortServiceImpl::kIdAtom;
nsIAtom* XULSortServiceImpl::kNaturalOrderPosAtom;
nsIAtom* XULSortServiceImpl::kRDF_type;
nsIAtom* XULSortServiceImpl::kURIAtom;

nsIRDFResource		*XULSortServiceImpl::kNC_Name;
nsIRDFResource		*XULSortServiceImpl::kRDF_instanceOf;
nsIRDFResource		*XULSortServiceImpl::kRDF_Seq;

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
		kNameAtom			= NS_NewAtom("Name");
		kSortAtom			= NS_NewAtom("sortActive");
		kSortDirectionAtom		= NS_NewAtom("sortDirection");
		kIdAtom				= NS_NewAtom("id");
		kNaturalOrderPosAtom		= NS_NewAtom("pos");
		kRDF_type			= NS_NewAtom("type");
		kURIAtom			= NS_NewAtom("uri");
 
		nsresult rv;

		if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
						  kIRDFServiceIID, (nsISupports**) &gRDFService)))
		{
			NS_ERROR("couldn't create rdf service");
		}

		nsILocaleFactory	*localeFactory = nsnull; 
		nsILocale		*locale = nsnull;

		// get a locale factory 
		if (NS_SUCCEEDED(rv = nsComponentManager::FindFactory(kLocaleFactoryCID, (nsIFactory**)&localeFactory))
			&& (localeFactory))
		{
			if (NS_SUCCEEDED(rv = localeFactory->GetApplicationLocale(&locale)) && (locale))
			{
				nsICollationFactory	*colFactory;
				if (NS_SUCCEEDED(rv = nsComponentManager::CreateInstance(kCollationFactoryCID, NULL,
						kICollationFactoryIID, (void**) &colFactory)))
				{
					if (NS_FAILED(rv = colFactory->CreateCollation(locale, &collationService)))
					{
						NS_ERROR("couldn't create collation instance");
					}
				}
				else
				{
					NS_ERROR("couldn't create instance of collation factory");
				}
				NS_RELEASE(locale);
				locale = nsnull;
			}
			else
			{
				NS_ERROR("unable to get application locale");
			}
			NS_RELEASE(localeFactory);
			localeFactory = nsnull;
		}
		else
		{
			NS_ERROR("couldn't get locale factory");
		}

		gRDFService->GetResource(kURINC_Name, 				&kNC_Name);
		gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf",	&kRDF_instanceOf);
		gRDFService->GetResource(RDF_NAMESPACE_URI "Seq",		&kRDF_Seq);

	        // Register the XUL and RDF namespaces: these'll just retrieve
	        // the IDs if they've already been registered by someone else.
		nsINameSpaceManager* mgr;
		if (NS_SUCCEEDED(rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
		                           nsnull,
		                           kINameSpaceManagerIID,
		                           (void**) &mgr)))
		{
			static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;

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
	        NS_RELEASE(kNameAtom);
	        NS_RELEASE(kSortAtom);
	        NS_RELEASE(kSortDirectionAtom);
	        NS_RELEASE(kIdAtom);
	        NS_RELEASE(kNaturalOrderPosAtom);
		NS_RELEASE(kRDF_type);
		NS_RELEASE(kURIAtom);

	        NS_RELEASE(kNC_Name);
	        NS_RELEASE(kRDF_instanceOf);
	        NS_RELEASE(kRDF_Seq);

		if (collationService)
		{
			NS_RELEASE(collationService);
			collationService = nsnull;
		}
		if (gRDFService)
		{
			nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
			gRDFService = nsnull;
		}
		if (gXULSortService)
		{
			nsServiceManager::ReleaseService(kXULSortServiceCID, gXULSortService);
			gXULSortService = nsnull;
		}
	}
}



nsresult
XULSortServiceImpl::GetSortService(nsIXULSortService** mgr)
{
	if (! gXULSortService)
	{
		gXULSortService = new XULSortServiceImpl();
		if (! gXULSortService)
			return NS_ERROR_OUT_OF_MEMORY;
	}
	NS_ADDREF(gXULSortService);
	*mgr = gXULSortService;
	return NS_OK;
}



NS_IMPL_ISUPPORTS(XULSortServiceImpl, nsIXULSortService::GetIID());

/*
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
*/


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
XULSortServiceImpl::FindTreeChildrenElement(nsIContent *tree, nsIContent **treeBody)
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
			if (tag.get() == kTreeChildrenAtom)
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
					PRBool		setFlag = PR_FALSE;
					if (colResource == sortResource)
					{
						*sortColIndex = colIndex;
						found = PR_TRUE;
						if (!sortDirection.EqualsIgnoreCase("natural"))
						{
							setFlag = PR_TRUE;
						}
					}
					if (setFlag == PR_TRUE)
					{
						nsString	trueStr("true");
						child->SetAttribute(kNameSpaceID_None, kSortAtom, trueStr, PR_TRUE);
						child->SetAttribute(kNameSpaceID_None, kSortDirectionAtom, sortDirection, PR_TRUE);

						// Note: don't break out of loop; want to set/unset attribs on ALL sort columns
						// break;
					}
					else
					{
						child->UnsetAttribute(kNameSpaceID_None, kSortAtom, PR_TRUE);
						child->UnsetAttribute(kNameSpaceID_None, kSortDirectionAtom, PR_TRUE);
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
XULSortServiceImpl::GetTreeCell(sortPtr sortInfo, nsIContent *node, PRInt32 cellIndex, nsIContent **cell)
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
XULSortServiceImpl::GetTreeCellValue(sortPtr sortInfo, nsIContent *node, nsString & val)
{
	PRBool			found = PR_FALSE;
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;

	if (NS_FAILED(rv = node->ChildCount(numChildren)))	return(rv);

	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = node->ChildAt(childIndex, *getter_AddRefs(child))))
			break;
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))
			break;
		if (nameSpaceID != sortInfo->kNameSpaceID_XUL)
		{
			// Get text using the DOM
			nsCOMPtr<nsIDOMText> domText;
			rv = child->QueryInterface(kIDOMTextIID, getter_AddRefs(domText));
			if (NS_FAILED(rv))
				break;
			val.Truncate();
			domText->GetData(val);
			found = PR_TRUE;
			break;
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



nsresult
XULSortServiceImpl::CompareNodes(nsIRDFNode *cellNode1, PRBool isCollationKey1,
				 nsIRDFNode *cellNode2, PRBool isCollationKey2,
				 PRInt32 & sortOrder)
{
	sortOrder = 0;

	nsAutoString			cellVal1, cellVal2;
	nsCOMPtr<nsIRDFLiteral>		literal1 = do_QueryInterface(cellNode1);
	nsCOMPtr<nsIRDFLiteral>		literal2 = do_QueryInterface(cellNode2);
	if (literal1)
	{
		const PRUnichar		*uni1 = nsnull;
		literal1->GetValueConst(&uni1);
		if (uni1)	cellVal1 = uni1;
	}
	if (literal2)
	{
		const PRUnichar		*uni2 = nsnull;
		literal2->GetValueConst(&uni2);
		if (uni2)	cellVal2 = uni2;
	}

	if (isCollationKey1 == PR_TRUE && isCollationKey2 == PR_TRUE)
	{
		// sort collation keys
		if (collationService)
		{
			collationService->CompareSortKey(cellVal1, cellVal2, &sortOrder);
		}
		else
		{
			// without a collation service, unable to collate
			sortOrder = 0;
		}
	}
	else if ((isCollationKey1 == PR_TRUE) && (isCollationKey2 == PR_FALSE))
	{
		sortOrder = -1;
	}
	else if ((isCollationKey1 == PR_FALSE) && (isCollationKey2 == PR_TRUE))
	{
		sortOrder = 1;
	}
	else if (literal1 && literal2)
	{
		// neither is a collation key, but both are strings, so fallback to a string comparison
		sortOrder = (PRInt32)cellVal1.Compare(cellVal2, PR_TRUE);
	}
	else
	{
		// not a collation key, and both aren't strings, so try other data types (ints)
		nsCOMPtr<nsIRDFInt>		intLiteral1 = do_QueryInterface(cellNode1);
		nsCOMPtr<nsIRDFInt>		intLiteral2 = do_QueryInterface(cellNode2);
		if (intLiteral1 && intLiteral2)
		{
			PRInt32			intVal1, intVal2;
			intLiteral1->GetValue(&intVal1);
			intLiteral2->GetValue(&intVal2);
			
			sortOrder = 0;
			if (intVal1 < intVal2)		sortOrder = -1;
			else if (intVal1 > intVal2)	sortOrder = 1;
		}
		else
		{
			// not a collation key, and both aren't strings, so try other data types (dates)
			nsCOMPtr<nsIRDFDate>		dateLiteral1 = do_QueryInterface(cellNode1);
			nsCOMPtr<nsIRDFDate>		dateLiteral2 = do_QueryInterface(cellNode2);
			if (dateLiteral1 && dateLiteral2)
			{
				PRInt64			dateVal1, dateVal2;
				dateLiteral1->GetValue(&dateVal1);
				dateLiteral2->GetValue(&dateVal2);
				
				sortOrder = 0;
				if (LL_CMP(dateVal1, <, dateVal2))	sortOrder = -1;
				else if (LL_CMP(dateVal1, >, dateVal2))	sortOrder = 1;
			}
		}
	}
	return(NS_OK);
}



nsresult
XULSortServiceImpl::OpenSort(nsIRDFNode *node1, nsIRDFNode *node2, sortPtr sortInfo, PRInt32 & sortOrder)
{
	nsCOMPtr<nsIRDFNode>	cellNode1, cellNode2;
	nsAutoString		cellVal1(""), cellVal2("");
	nsresult		rv = NS_OK;
	PRBool			isCollationKey1 = PR_FALSE, isCollationKey2 = PR_FALSE;

	sortOrder = 0;

	nsCOMPtr<nsIRDFResource>	res1 = do_QueryInterface(node1);
	if (res1)
	{
		rv = GetResourceValue(res1, sortInfo->sortProperty, sortInfo, getter_AddRefs(cellNode1), isCollationKey1);
	}
	nsCOMPtr<nsIRDFResource>	res2 = do_QueryInterface(node2);
	if (res2)
	{
		rv = GetResourceValue(res2, sortInfo->sortProperty, sortInfo, getter_AddRefs(cellNode2), isCollationKey2);
	}

	rv = CompareNodes(cellNode1, isCollationKey1, cellNode2, isCollationKey2, sortOrder);

	if (sortInfo->descendingSort == PR_TRUE)
	{
		// descending sort is being imposed, so reverse the sort order
		sortOrder = -sortOrder;
	}

	return(rv);
}



int
openSortCallback(const void *data1, const void *data2, void *privateData)
{
	/// Note: openSortCallback is a small C callback stub for NS_QuickSort

	_sortStruct		*sortInfo = (_sortStruct *)privateData;
	nsIRDFNode		*node1 = *(nsIRDFNode **)data1;
	nsIRDFNode		*node2 = *(nsIRDFNode **)data2;
	PRInt32			sortOrder = 0;
	nsresult		rv;

	if (nsnull != sortInfo)
	{
		rv = XULSortServiceImpl::OpenSort(node1, node2, sortInfo, sortOrder);
	}
	return(sortOrder);
}



nsresult
XULSortServiceImpl::GetResourceValue(nsIRDFResource *res1, nsIRDFResource *sortProperty, sortPtr sortInfo,
				nsIRDFNode **target, PRBool &isCollationKey)
{
	nsresult		rv = NS_OK;

	*target = nsnull;
	isCollationKey = PR_FALSE;

	if ((sortInfo->naturalOrderSort == PR_FALSE) && (sortInfo->sortProperty))
	{
		nsXPIDLCString		sortPropertyURI;
		sortInfo->sortProperty->GetValue( getter_Copies(sortPropertyURI) );
		if ((sortPropertyURI) && (res1))
		{
			// for any given property, first ask the graph for its value with "?collation=true" appended
			// to indicate that if there is a collation key available for this value, we want it

			nsAutoString	modSortProperty(sortPropertyURI);
			modSortProperty += "?collation=true";
			char	*collationSortProp = modSortProperty.ToNewCString();
			if (collationSortProp)
			{
				nsCOMPtr<nsIRDFResource>	modSortRes;
				if (NS_SUCCEEDED(sortInfo->rdfService->GetResource(collationSortProp, 
					getter_AddRefs(modSortRes))) && (modSortRes))
				{
					if (NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(res1, modSortRes,
						PR_TRUE, target)) && (rv != NS_RDF_NO_VALUE))
					{
						isCollationKey = PR_TRUE;
					}
				}
				delete []collationSortProp;
			}
			if (!(*target))
			{
				// if no collation key, ask the graph for its value with "?sort=true" appended
				// to indicate that if there is any distinction between its display value and sorting
				// value, we want the sorting value (so that, for example, a mail datasource could strip
				// off a "Re:" on a mail message subject)
				modSortProperty = sortPropertyURI;
				modSortProperty += "?sort=true";
				char	*sortProp = modSortProperty.ToNewCString();
				if (sortProp)
				{
					nsCOMPtr<nsIRDFResource>	modSortRes;
					if (NS_SUCCEEDED(sortInfo->rdfService->GetResource(sortProp, 
						getter_AddRefs(modSortRes))) && (modSortRes))
					{
						if (NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(res1, modSortRes,
							PR_TRUE, target)) && (rv != NS_RDF_NO_VALUE))
						{
						}
					}
					delete []sortProp;
				}
			}
		}
		if (!(*target))
		{
			// if no collation key and no special sorting value, just get the property value
			if ((res1) && (NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(res1, sortProperty,
				PR_TRUE, target)) && (rv != NS_RDF_NO_VALUE)))
			{
			}
		}
	}
	return(rv);
}



nsresult
XULSortServiceImpl::GetNodeValue(nsIContent *node1, nsIRDFResource *sortProperty, sortPtr sortInfo,
				nsIRDFNode **theNode, PRBool &isCollationKey)
{
	nsresult		rv;

	isCollationKey = PR_FALSE;

	nsCOMPtr<nsIDOMXULElement>	dom1 = do_QueryInterface(node1);
	if (!dom1)	return(NS_ERROR_FAILURE);

//	nsCOMPtr<nsIRDFResource>	res1 = do_QueryInterface(dom1);
	nsCOMPtr<nsIRDFResource>	res1;
	if (NS_FAILED(rv = dom1->GetResource(getter_AddRefs(res1))))
	{
		res1 = null_nsCOMPtr();
	}
	// Note: don't check for res1 QI failure here.  It only succeeds for RDF nodes,
	// but for XUL nodes it will failure; in the failure case, the code below gets
	// the cell's text value straight from the DOM
	
	if ((sortInfo->naturalOrderSort == PR_FALSE) && (sortInfo->sortProperty))
	{
		rv = GetResourceValue(res1, sortProperty, sortInfo, theNode, isCollationKey);

//		if (cellVal1.Length() == 0)
		if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE))
		{
		        nsCOMPtr<nsIContent>	cell1;
			if (NS_SUCCEEDED(rv = GetTreeCell(sortInfo, node1, sortInfo->colIndex,
				getter_AddRefs(cell1))) && (cell1))
			{
				nsAutoString		cellVal1;
				if (NS_SUCCEEDED(rv = GetTreeCellValue(sortInfo, cell1, cellVal1)) &&
					(rv != NS_RDF_NO_VALUE))
				{
					nsIRDFLiteral	*nodeLiteral;
					gRDFService->GetLiteral(cellVal1.GetUnicode(), &nodeLiteral);
					*theNode = nodeLiteral;
					isCollationKey = PR_FALSE;
				}
			}
		}
	}
	else if (sortInfo->naturalOrderSort == PR_TRUE)
	{
		nsAutoString		cellPosVal1;

		// check to see if this is a RDF_Seq
		// Note: this code doesn't handle the aggregated Seq case especially well
		if ((res1) && (sortInfo->db))
		{
			nsCOMPtr<nsISimpleEnumerator>	arcs;
			if (NS_SUCCEEDED(rv = sortInfo->db->ArcLabelsIn(res1, getter_AddRefs(arcs))))
			{
				PRBool		hasMore = PR_TRUE;
				while(hasMore)
				{
					if (NS_FAILED(rv = arcs->HasMoreElements(&hasMore)))	break;
					if (hasMore == PR_FALSE)	break;

					nsCOMPtr<nsISupports>	isupports;
					if (NS_FAILED(rv = arcs->GetNext(getter_AddRefs(isupports))))	break;
					nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);
					if (!property)			continue;

					// hack: it it looks like a RDF_Seq and smells like a RDF_Seq...
					static const char kRDFNameSpace_Seq_Prefix[] = "http://www.w3.org/1999/02/22-rdf-syntax-ns#_";
					const char	*uri = nsnull;
					if (NS_FAILED(rv = property->GetValueConst(&uri)))	continue;
					if (!uri)	continue;
					if (nsCRT::strncasecmp(uri, kRDFNameSpace_Seq_Prefix, sizeof(kRDFNameSpace_Seq_Prefix)-1))
						continue;

					cellPosVal1 = uri;
					cellPosVal1.Cut(0, sizeof(kRDFNameSpace_Seq_Prefix)-1);

					// hack: assume that its a number, so pad out a bit
			                nsAutoString	zero("000000");
			                if (cellPosVal1.Length() < zero.Length())
			                {
						cellPosVal1.Insert(zero, 0, zero.Length() - cellPosVal1.Length());
			                }

					hasMore = PR_FALSE;
					break;
				}
			}
		}
		if (cellPosVal1.Length() == 0)
		{
			rv = node1->GetAttribute(kNameSpaceID_None, sortInfo->kNaturalOrderPosAtom, cellPosVal1);
		}
		if (NS_SUCCEEDED(rv) && (rv != NS_RDF_NO_VALUE))
		{
			nsIRDFLiteral	*nodePosLiteral;
			gRDFService->GetLiteral(cellPosVal1.GetUnicode(), &nodePosLiteral);
			*theNode = nodePosLiteral;
			isCollationKey = PR_FALSE;
		}
	}
	return(rv);
}



nsresult
XULSortServiceImpl::ImplaceSort(nsIContent *node1, nsIContent *node2, sortPtr sortInfo, PRInt32 & sortOrder)
{
	nsAutoString		cellVal1(""), cellVal2("");
	PRBool			isCollationKey1 = PR_FALSE, isCollationKey2 = PR_FALSE;
	nsresult		rv;

	sortOrder = 0;

	nsCOMPtr<nsIRDFNode>	cellNode1, cellNode2;
	GetNodeValue(node1, sortInfo->sortProperty, sortInfo, getter_AddRefs(cellNode1), isCollationKey1);
	GetNodeValue(node2, sortInfo->sortProperty, sortInfo, getter_AddRefs(cellNode2), isCollationKey2);
	if ((!cellNode1) && (!cellNode2))
	{
		nsCOMPtr<nsIRDFResource>	name1;
		sortInfo->rdfService->GetResource(kURINC_Name, getter_AddRefs(name1));
		if (name1)
		{
			rv = GetNodeValue(node1, name1, sortInfo, getter_AddRefs(cellNode1), isCollationKey1);
		}
		nsCOMPtr<nsIRDFResource>	name2;
		sortInfo->rdfService->GetResource(kURINC_Name, getter_AddRefs(name2));
		if (name2)
		{
			rv = GetNodeValue(node2, name2, sortInfo, getter_AddRefs(cellNode2), isCollationKey2);
		}
	}

	rv = CompareNodes(cellNode1, isCollationKey1, cellNode2, isCollationKey2, sortOrder);

	if (sortInfo->descendingSort == PR_TRUE)
	{
		// descending sort is being imposed, so reverse the sort order
		sortOrder = -sortOrder;
	}

	return(NS_OK);
}



int
inplaceSortCallback(const void *data1, const void *data2, void *privateData)
{
	/// Note: inplaceSortCallback is a small C callback stub for NS_QuickSort

	_sortStruct		*sortInfo = (_sortStruct *)privateData;
	nsIContent		*node1 = *(nsIContent **)data1;
	nsIContent		*node2 = *(nsIContent **)data2;
	PRInt32			sortOrder = 0;
	nsresult		rv;

	if (nsnull != sortInfo)
	{
		rv = XULSortServiceImpl::ImplaceSort(node1, node2, sortInfo, sortOrder);
	}
	return(sortOrder);
}



nsresult
XULSortServiceImpl::SortTreeChildren(nsIContent *container, PRInt32 colIndex, sortPtr sortInfo, PRInt32 indentLevel)
{
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;

	if (NS_FAILED(rv = container->ChildCount(numChildren)))	return(rv);

	nsCOMPtr<nsISupportsArray> childArray;
	rv = NS_NewISupportsArray(getter_AddRefs(childArray));
	if (NS_FAILED(rv))	return(rv);

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
	PRUint32 cnt = 0;
	rv = childArray->Count(&cnt);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
	PRUint32 numElements = cnt;
	if (numElements > 0)
	{
		nsIContent ** flatArray = new nsIContent*[numElements];
		if (flatArray)
		{
			// flatten array of resources, sort them, then add as tree elements
			PRUint32	loop;
		        for (loop=0; loop<numElements; loop++)
		        {
				flatArray[loop] = (nsIContent *)childArray->ElementAt(loop);
			}

			/* smart sorting (sort within separators) on name column */
			if (sortInfo->sortProperty == kNC_Name)
			{
				PRUint32	startIndex=0;
				for (loop=0; loop<numElements; loop++)
				{
					nsAutoString	type;
					if (NS_OK == flatArray[loop]->GetAttribute(kNameSpaceID_None, kRDF_type, type))
					{
						if (type.EqualsIgnoreCase(kURINC_BookmarkSeparator))
						{
							if (loop > startIndex+1)
							{
								NS_QuickSort((void *)&flatArray[startIndex], loop-startIndex, sizeof(nsIContent *),
									inplaceSortCallback, (void *)sortInfo);
								startIndex = loop+1;
							}
						}
					}
				}
				if (loop > startIndex+1)
				{
					NS_QuickSort((void *)&flatArray[startIndex], loop-startIndex, sizeof(nsIContent *),
						inplaceSortCallback, (void *)sortInfo);
					startIndex = loop+1;
				}
			}
			else
			{
				NS_QuickSort((void *)flatArray, numElements, sizeof(nsIContent *),
					inplaceSortCallback, (void *)sortInfo);
			}

			// Bug 6665. This is a hack to "addref" the resources
			// before we remove them from the content model. This
			// keeps them from getting released, which causes
			// performance problems for some datasources.
			for (loop = 0; loop < numElements; loop++)
			{
				nsIRDFResource	*resource;
				nsRDFContentUtils::GetElementResource(flatArray[loop], &resource);
				// Note that we don't release; see part deux below...
			}

			RemoveAllChildren(container);
			
			// insert sorted children			
			numChildren = 0;
			for (loop=0; loop<numElements; loop++)
			{
				container->InsertChildAt((nsIContent *)flatArray[loop], numChildren++, PR_FALSE);
			}

			// Bug 6665, part deux. The Big Hack.
			for (loop = 0; loop < numElements; loop++)
			{
				nsIRDFResource	*resource;
				nsRDFContentUtils::GetElementResource(flatArray[loop], &resource);
				nsrefcnt	refcnt;
				NS_RELEASE2(resource, refcnt);
				NS_RELEASE(resource);
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
	rv = childArray->Count(&cnt);
	if (NS_FAILED(rv)) return rv;
	for (int i = cnt - 1; i >= 0; i--)
	{
		childArray->RemoveElementAt(i);
	}
	return(NS_OK);
}



NS_IMETHODIMP
XULSortServiceImpl::OpenContainer(nsIRDFCompositeDataSource *db, nsIContent *container,
			nsIRDFResource **flatArray, PRInt32 numElements, PRInt32 elementSize)
{
	nsresult	rv;
	nsString	sortResource, sortDirection;
	_sortStruct	sortInfo;

	// get sorting info (property to sort on, direction to sort, etc)

	nsCOMPtr<nsIContent>	treeNode;
	if (NS_FAILED(rv = FindTreeElement(container, getter_AddRefs(treeNode))))	return(rv);

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
		NS_QuickSort((void *)flatArray, numElements, elementSize, openSortCallback, (void *)&sortInfo);
	}
	return(NS_OK);
}



NS_IMETHODIMP
XULSortServiceImpl::InsertContainerNode(nsIContent *container, nsIContent *node, PRBool aNotify)
{
	nsresult	rv;
	nsString	sortResource, sortDirection;
	_sortStruct	sortInfo;

	// get sorting info (property to sort on, direction to sort, etc)

	nsCOMPtr<nsIContent>	treeNode;
	if (NS_FAILED(rv = FindTreeElement(container, getter_AddRefs(treeNode))))	return(rv);

	// get composite db for tree
	nsCOMPtr<nsIDOMXULElement> domXulTree;
	sortInfo.rdfService = gRDFService;
	sortInfo.db = nsnull;

	// Maintain an nsCOMPtr to _here_ to the composite datasource so
	// that we're sure that we'll hold a reference to it (and actually
	// release that reference when the stack frame goes away).
	nsCOMPtr<nsIRDFCompositeDataSource> cds;
	rv = treeNode->QueryInterface(kIDomXulElementIID, getter_AddRefs(domXulTree));
	if (NS_SUCCEEDED(rv))
	{
		if (NS_SUCCEEDED(rv = domXulTree->GetDatabase(getter_AddRefs(cds))))
		{
			sortInfo.db = cds;
		}
	}

	sortInfo.kNaturalOrderPosAtom = kNaturalOrderPosAtom;
	sortInfo.kTreeCellAtom = kTreeCellAtom;
	sortInfo.kNameSpaceID_XUL = kNameSpaceID_XUL;

	sortInfo.sortProperty = nsnull;
	if (NS_SUCCEEDED(rv = GetSortColumnInfo(treeNode, sortResource, sortDirection)))
	{
		char *uri = sortResource.ToNewCString();
		if (uri)
		{
			rv = gRDFService->GetResource(uri, &sortInfo.sortProperty);
			delete [] uri;
			if (NS_FAILED(rv))	return(rv);
		}
	}

	// set up sort order info
	sortInfo.naturalOrderSort = PR_FALSE;
	sortInfo.descendingSort = PR_FALSE;
	if (sortDirection.EqualsIgnoreCase("descending"))
	{
		sortInfo.descendingSort = PR_TRUE;
	}
	else if (!sortDirection.EqualsIgnoreCase("asscending"))
	{
		sortInfo.naturalOrderSort = PR_TRUE;
	}

	PRBool			isContainerRDFSeq = PR_FALSE;

	if ((sortInfo.db) && (sortInfo.naturalOrderSort == PR_TRUE))
	{
		// walk up the content model to find the REAL
		// parent container to determine if its a RDF_Seq
		nsCOMPtr<nsIContent>		parent = do_QueryInterface(container, &rv);
		nsCOMPtr<nsIContent>		aContent;
		while(NS_SUCCEEDED(rv) && parent)
		{
			nsAutoString	uriStr;
			if (NS_SUCCEEDED(rv = parent->GetAttribute(kNameSpaceID_None, kIdAtom, uriStr))
				&& (rv == NS_CONTENT_ATTR_HAS_VALUE))
			{
				char	*uri= uriStr.ToNewCString();
				if (uri)
				{
					nsCOMPtr<nsIRDFResource>	containerRes;
					if (NS_SUCCEEDED(rv = gRDFService->GetResource(uri, getter_AddRefs(containerRes)))
						&& (rv != NS_RDF_NO_VALUE) && (containerRes))
					{
						if (NS_FAILED(rv = sortInfo.db->HasAssertion( containerRes, kRDF_instanceOf,
							kRDF_Seq, PR_TRUE, &isContainerRDFSeq)) || (rv == NS_RDF_NO_VALUE))
						{
							isContainerRDFSeq = PR_FALSE;
						}
					}
					delete [] uri;
				}
				break;
			}
			aContent = do_QueryInterface(parent, &rv);
			if (NS_SUCCEEDED(rv))
				rv = aContent->GetParent(*getter_AddRefs(parent));
		}
	}

	PRBool			childAdded = PR_FALSE;

	if ((sortInfo.naturalOrderSort == PR_FALSE) ||
		((sortInfo.naturalOrderSort == PR_TRUE) &&
		(isContainerRDFSeq == PR_TRUE)))
	{
#ifdef	XUL_BINARY_INSERTION_SORT
		// figure out where to insert the node when a sort order is being imposed
		// using a smart binary comparison
		PRInt32			numChildren = 0;
		if (NS_FAILED(rv = container->ChildCount(numChildren)))	return(rv);
		if (numChildren > 0)
		{
		        nsCOMPtr<nsIContent>	child;
			PRInt32			last = -1, current, direction = 0, delta = numChildren;
			while(PR_TRUE)
			{
				delta = delta / 2;

				if (last == -1)
				{
					current = delta;
				}
				else if (direction > 0)
				{
					if (delta == 0)	delta = 1;
					current = last + delta;
				}
				else
				{
					if (delta == 0)	delta = 1;
					current = last - delta;
				}

				if (current != last)
				{
					container->ChildAt(current, *getter_AddRefs(child));
					nsIContent	*theChild = child.get();
					direction = inplaceSortCallback(&node, &theChild, &sortInfo);
				}
				if ( (direction == 0) ||
					((current == last + 1) && (direction < 0)) ||
					((current == last - 1) && (direction > 0)) ||
					((current == 0) && (direction < 0)) ||
					((current >= numChildren - 1) && (direction > 0)) )
				{
					if (current >= numChildren)
					{
						container->AppendChildTo(node, aNotify);
					}
					else
					{
						container->InsertChildAt(node,
							((direction > 0) ? current + 1: (current >= 0) ? current : 0),
							aNotify);
					}
					childAdded = PR_TRUE;
					break;
				}
				last = current;
			}
		}
#else
		// figure out where to insert the node when a sort order is being imposed
		// using a simple linear brute-force comparison
		PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
	        nsCOMPtr<nsIContent>	child;

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
					container->InsertChildAt(node, childIndex, aNotify);
					childAdded = PR_TRUE;
					break;
				}
			}
		}
#endif
	}

	if (childAdded == PR_FALSE)
	{
		container->AppendChildTo(node, aNotify);
	}
	return(NS_OK);
}



NS_IMETHODIMP
XULSortServiceImpl::Sort(nsIDOMNode* node, const char *sortResource, const char *sortDirection)
{
#ifdef	DEBUG
	printf("\n XULSortServiceImpl::Sort \n\n");
#endif

	nsAutoString	sortRes(sortResource), sortDir(sortDirection);
	nsresult	rv = DoSort(node, sortResource, sortDirection);
	return(rv);
}



NS_IMETHODIMP
XULSortServiceImpl::DoSort(nsIDOMNode* node, const nsString& sortResource,
                           const nsString& sortDirection)
{
	PRInt32		colIndex, treeBodyIndex;
	nsresult	rv;
	_sortStruct	sortInfo;

	// get tree node
	nsCOMPtr<nsIContent>	contentNode = do_QueryInterface(node);
	if (!contentNode)	return(NS_ERROR_FAILURE);
	nsCOMPtr<nsIContent>	treeNode;
	if (NS_FAILED(rv = FindTreeElement(contentNode, getter_AddRefs(treeNode))))	return(rv);

	// get composite db for tree
	sortInfo.rdfService = gRDFService;
	sortInfo.db = nsnull;
	nsCOMPtr<nsIDOMXULElement>	domXulTree = do_QueryInterface(treeNode);
	if (!domXulTree)	return(NS_ERROR_FAILURE);
	nsCOMPtr<nsIRDFCompositeDataSource> cds;
	if (NS_SUCCEEDED(rv = domXulTree->GetDatabase(getter_AddRefs(cds))))
	{
		sortInfo.db = cds;
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
	// _won't_ send any notifications, so we won't trigger any reflows...
	if (NS_FAILED(rv = GetSortColumnIndex(treeNode, sortResource, sortDirection, &colIndex)))	return(rv);
	sortInfo.colIndex = colIndex;
	nsCOMPtr<nsIContent>	treeBody;
	if (NS_FAILED(rv = FindTreeChildrenElement(treeNode, getter_AddRefs(treeBody))))	return(rv);
	if (NS_SUCCEEDED(rv = SortTreeChildren(treeBody, colIndex, &sortInfo, 0)))
	{
	}

	// Now remove the treebody and re-insert it to force the frames to be rebuilt.
    	nsCOMPtr<nsIContent>	treeParent;
	if (NS_FAILED(rv = treeBody->GetParent(*getter_AddRefs(treeParent))))	return(rv);
	if (NS_FAILED(rv = treeParent->IndexOf(treeBody, treeBodyIndex)))	return(rv);
	if (NS_FAILED(rv = treeParent->RemoveChildAt(treeBodyIndex, PR_TRUE)))	return(rv);

	if (NS_FAILED(rv = treeParent->AppendChildTo(treeBody, PR_TRUE)))	return(rv);
	return(NS_OK);
}



nsresult
NS_NewXULSortService(nsIXULSortService** mgr)
{
	return XULSortServiceImpl::GetSortService(mgr);
}
