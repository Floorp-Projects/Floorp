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
#include "rdf.h"
#include "rdfutil.h"

#include "nsVoidArray.h"
#include "rdf_qsort.h"

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

////////////////////////////////////////////////////////////////////////

class RDFTreeBuilderImpl : public RDFGenericBuilderImpl
{
public:
    RDFTreeBuilderImpl();
    virtual ~RDFTreeBuilderImpl();

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDataBase(nsIRDFCompositeDataSource* aDataBase);

    // nsIDOMNodeObserver interface
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
};

////////////////////////////////////////////////////////////////////////

nsrefcnt RDFTreeBuilderImpl::gRefCnt = 0;

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
    }

    ++gRefCnt;
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
    if (NS_FAILED(rv = GetDOMNodeResource(aParent, getter_AddRefs(resource)))) {
        // XXX it's not a resource element, so there's no assertions
        // we need to make on the back-end. Should we just do the
        // update?
        return NS_OK;
    }

    // Get the nsIContent interface, it's a bit more utilitarian
    nsCOMPtr<nsIContent> parent( do_QueryInterface(aParent) );
    if (! parent) {
        NS_ERROR("parent doesn't support nsIContent");
        return NS_ERROR_UNEXPECTED;
    }

    // Make sure that the element is in the widget. XXX Even this may be
    // a bit too promiscuous: an element may also be a XUL element...
    if (!IsElementInWidget(parent))
        return NS_OK;

    // Split the parent into its namespace and tag components
    PRInt32 parentNameSpaceID;
    if (NS_FAILED(rv = parent->GetNameSpaceID(parentNameSpaceID))) {
        NS_ERROR("unable to get parent namespace ID");
        return rv;
    }

    nsCOMPtr<nsIAtom> parentNameAtom;
    if (NS_FAILED(rv = parent->GetTag( *getter_AddRefs(parentNameAtom) ))) {
        NS_ERROR("unable to get parent tag");
        return rv;
    }

    // Now do the same for the child
    nsCOMPtr<nsIContent> child( do_QueryInterface(aOldChild) );
    if (! child) {
        NS_ERROR("child doesn't support nsIContent");
        return NS_ERROR_UNEXPECTED;
    }

    PRInt32 childNameSpaceID;
    if (NS_FAILED(rv = child->GetNameSpaceID(childNameSpaceID))) {
        NS_ERROR("unable to get child's namespace ID");
        return rv;
    }

    nsCOMPtr<nsIAtom> childNameAtom;
    if (NS_FAILED(rv = child->GetTag( *getter_AddRefs(childNameAtom) ))) {
        NS_ERROR("unable to get child's tag");
        return rv;
    }

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
            if  (NS_FAILED(rv = child->GetAttribute(kNameSpaceID_RDF, kPropertyAtom, propertyStr))) {
                NS_ERROR("severe error trying to retrieve attribute");
                return rv;
            }

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                // It's a relationship set up by RDF. So let's
                // unassert it from the graph. First we need the
                // property resource.
                nsCOMPtr<nsIRDFResource> property;
                if (NS_FAILED(gRDFService->GetUnicodeResource(propertyStr, getter_AddRefs(property)))) {
                    NS_ERROR("unable to construct resource for property");
                    return rv;
                }


                // And now we need the child's resource.
                nsCOMPtr<nsIRDFResource> target;
                if (NS_FAILED(rv = GetElementResource(child, getter_AddRefs(target)))) {
                    NS_ERROR("expected child to have resource");
                    return rv;
                }

                // We'll handle things a bit differently if we're
                // tinkering with an RDF container...
                if (rdf_IsContainer(mDB, resource) &&
                    rdf_IsOrdinalProperty(property)) {
                    rv = rdf_ContainerRemoveElement(mDB, resource, target);
                }
                else {
                    rv = mDB->Unassert(resource, property, target);
                }

                if (NS_FAILED(rv)) {
                    NS_ERROR("unable to remove element from DB");
                    return rv;
                }
            }
            else {

                // It's a random (non-RDF inserted) element. Just use
                // the content interface to remove it.
                PRInt32 index;
                if (NS_FAILED(rv = parent->IndexOf(child, index))) {
                    NS_ERROR("unable to get index of child in parent node");
                    return rv;
                }

                NS_ASSERTION(index >= 0, "child was already removed");

                if (index >= 0) {
                    rv = parent->RemoveChildAt(index, PR_TRUE);
                    NS_ASSERTION(NS_SUCCEEDED(rv), "error removing child from parent");
                }
            }
        }
    }
    else if ((parentNameSpaceID == kNameSpaceID_XUL) &&
             (parentNameAtom.get() == kTreeItemAtom)) {

        // The parent is a xul:treeitem. We really only care about
        // treeitems in the body; not treeitems in the header...
        NS_NOTYETIMPLEMENTED("write me");
    }
    else if ((parentNameSpaceID == kNameSpaceID_XUL) &&
             (parentNameAtom.get() == kTreeCellAtom)) {

        // The parent is a xul:treecell. We really only care about
        // cells in the body; not cells in the header...
        NS_NOTYETIMPLEMENTED("write me");
    }

    // they're trying to manipulate the DOM in some way that we don't
    // care about: we should probably just call through and do the
    // operation on nsIContent, but...let the XUL builder do that I
    // guess.
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
        const char* uri;
        aProperty->GetValue(&uri);
        treeItem->SetAttribute(kNameSpaceID_RDF, kPropertyAtom, uri, PR_FALSE);
    }

    // Create the cell substructure
    if (NS_FAILED(rv = CreateTreeItemCells(treeItem)))
        return rv;

    // Add the <xul:treeitem> to the <xul:treechildren> element.
    treeChildren->AppendChildTo(treeItem, PR_TRUE);

    // Add miscellaneous attributes by iterating _all_ of the
    // properties out of the resource.
    nsCOMPtr<nsIRDFArcsOutCursor> arcs;
    if (NS_FAILED(rv = mDB->ArcLabelsOut(aValue, getter_AddRefs(arcs)))) {
        NS_ERROR("unable to get arcs out");
        return rv;
    }

    while (NS_SUCCEEDED(rv = arcs->Advance())) {
        nsCOMPtr<nsIRDFResource> property;
        if (NS_FAILED(rv = arcs->GetPredicate(getter_AddRefs(property)))) {
            NS_ERROR("unable to get cursor value");
            return rv;
        }

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

        nsCOMPtr<nsIRDFResource> resource;
        nsCOMPtr<nsIRDFLiteral> literal;

        nsAutoString s;
        if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFResourceIID, getter_AddRefs(resource)))) {
            const char* uri;
            resource->GetValue(&uri);
            s = uri;
        }
        else if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFLiteralIID, getter_AddRefs(literal)))) {
            const PRUnichar* p;
            literal->GetValue(&p);
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

    if (NS_FAILED(rv) && (rv != NS_ERROR_RDF_CURSOR_EMPTY)) {
        NS_ERROR("error advancing cursor");
        return rv;
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
        if (NS_FAILED(rv = FindChildByTag(aElement,
                                          kNameSpaceID_XUL,
                                          kTreeChildrenAtom,
                                          getter_AddRefs(treechildren)))) {
            // XXX make this a warning
            NS_ERROR("attempt to remove child from an element with no treechildren");
            return NS_OK;
        }

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
        if (NS_FAILED(rv = GetElementResource(kid, getter_AddRefs(resource)))) {
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
    NS_ERROR("unable to find child to remove");
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
    if (NS_FAILED(rv = GetElementResource(aTreeItemElement, getter_AddRefs(treeItemResource)))) {
        NS_ERROR("unable to get tree item resource");
        return rv;
    }

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
            if (NS_FAILED(gRDFService->GetUnicodeResource(uri, getter_AddRefs(property)))) {
                NS_ERROR("unable to construct resource for xul:treecell");
                return rv; // XXX fatal
            }

            // ...then query the RDF back-end
            nsCOMPtr<nsIRDFNode> value;
            if (NS_SUCCEEDED(rv = mDB->GetTarget(treeItemResource,
                                                 property,
                                                 PR_TRUE,
                                                 getter_AddRefs(value)))) {

                // Attach a plain old text node: nothing fancy. Here's
                // where we'd do wacky stuff like pull in an icon or
                // whatever.
                if (NS_FAILED(rv = nsRDFContentUtils::AttachTextNode(textParent, value))) {
                    NS_ERROR("unable to attach text node to xul:treecell");
                    return rv;
                }
            }
            else if (rv == NS_ERROR_RDF_NO_VALUE) {
                // otherwise, there was no value for this. It'll have to
                // get set later, when an OnAssert() comes in (if it even
                // has a value at all...)
            }
            else {
                NS_ERROR("error getting cell's value");
                return rv; // XXX something serious happened
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

    const char* propertyURI;
    if (NS_FAILED(rv = aProperty->GetValue(&propertyURI))) {
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
            if (0 == nsCRT::strcmp(uri, propertyURI)) {
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


