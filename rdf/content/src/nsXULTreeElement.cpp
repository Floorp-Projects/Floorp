/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

  Implementation methods for the XUL tree element APIs.

*/

#include "nsCOMPtr.h"
#include "nsRDFCID.h"
#include "nsXULTreeElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsINameSpaceManager.h"
#include "nsIFrame.h"
#include "nsIDOMElement.h"
#include "nsIComponentManager.h"
#include "nsITreeFrame.h"
#include "nsIDOMRange.h"
#include "nsIContentIterator.h"
#include "nsLayoutCID.h"
#include "nsString.h"

static NS_DEFINE_CID(kCRangeCID, NS_RANGE_CID);
static NS_DEFINE_IID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);

nsIAtom*             nsXULTreeElement::kSelectedAtom;
nsIAtom*             nsXULTreeElement::kOpenAtom;
nsIAtom*             nsXULTreeElement::kTreeRowAtom;
nsIAtom*             nsXULTreeElement::kTreeItemAtom;
nsIAtom*             nsXULTreeElement::kTreeChildrenAtom;
nsIAtom*             nsXULTreeElement::kCurrentAtom;
int                  nsXULTreeElement::gRefCnt = 0;

NS_IMPL_ADDREF_INHERITED(nsXULTreeElement, nsXULAggregateElement);
NS_IMPL_RELEASE_INHERITED(nsXULTreeElement, nsXULAggregateElement);

nsresult
nsXULTreeElement::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIDOMXULTreeElement))) {
        *aResult = NS_STATIC_CAST(nsIDOMXULTreeElement*, this);
    }
    else if (aIID.Equals(NS_GET_IID(nsIXULTreeContent))) {
        *aResult = NS_STATIC_CAST(nsIXULTreeContent*, this);
    }
    else {
        return nsXULAggregateElement::QueryInterface(aIID, aResult);
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult));
    return NS_OK;
}


nsXULTreeElement::nsXULTreeElement(nsIDOMXULElement* aOuter)
  : nsXULAggregateElement(aOuter)
{
  if (gRefCnt++ == 0) {
    kSelectedAtom    = NS_NewAtom("selected");
    kOpenAtom        = NS_NewAtom("open");
    kTreeRowAtom     = NS_NewAtom("treerow");
    kTreeItemAtom    = NS_NewAtom("treeitem");
    kTreeChildrenAtom= NS_NewAtom("treechildren");
    kCurrentAtom     = NS_NewAtom("current");
  }

  nsresult rv;

  nsRDFDOMNodeList* children;
  rv = nsRDFDOMNodeList::Create(&children);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create DOM node list");
  if (NS_FAILED(rv)) return;

  mSelectedItems = children;

  children = nsnull;
  rv = nsRDFDOMNodeList::Create(&children);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create DOM node list");
  if (NS_FAILED(rv)) return;

  mSelectedCells = children;

  mCurrentItem = nsnull;
  mCurrentCell = nsnull;
}

nsXULTreeElement::~nsXULTreeElement()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: nsXULTreeElement\n", gInstanceCount);
#endif

  NS_IF_RELEASE(mSelectedItems);
  NS_IF_RELEASE(mSelectedCells);

  if (--gRefCnt == 0) {
    NS_IF_RELEASE(kSelectedAtom);
    NS_IF_RELEASE(kTreeItemAtom);
    NS_IF_RELEASE(kTreeRowAtom);
    NS_IF_RELEASE(kTreeChildrenAtom);
    NS_IF_RELEASE(kOpenAtom);
    NS_IF_RELEASE(kCurrentAtom);
  }
}

NS_IMETHODIMP
nsXULTreeElement::GetSelectedItems(nsIDOMNodeList** aSelectedItems)
{
  NS_IF_ADDREF(mSelectedItems);
  *aSelectedItems = mSelectedItems;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::GetSelectedCells(nsIDOMNodeList** aSelectedCells)
{
  NS_IF_ADDREF(mSelectedCells);
  *aSelectedCells = mSelectedCells;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SelectItem(nsIDOMXULElement* aTreeItem)
{
  NS_ASSERTION(aTreeItem, "trying to select a null tree item");
  if (!aTreeItem) return NS_OK;

  // Sanity check. If we're the only item, just bail.
  PRUint32 length;
  mSelectedItems->GetLength(&length);
  if (length == 1) {
    // See if the single item already selected is us.
    nsCOMPtr<nsIDOMNode> domNode;
    mSelectedItems->Item(0, getter_AddRefs(domNode));
    nsCOMPtr<nsIDOMXULElement> treeItem = do_QueryInterface(domNode);
    if (treeItem.get() == aTreeItem)
      return NS_OK;
  }

  // First clear our selection.
  ClearItemSelectionInternal();

  // Now add ourselves to the selection by setting our selected attribute.
  AddItemToSelectionInternal(aTreeItem);

  SetCurrentItem(aTreeItem);

  FireOnSelectHandler();

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SelectCell(nsIDOMXULElement* aTreeCell)
{
  // Sanity check. If we're the only item, just bail.
  PRUint32 length;
  mSelectedCells->GetLength(&length);
  if (length == 1) {
    // See if the single item already selected is us.
    nsCOMPtr<nsIDOMNode> domNode;
    mSelectedCells->Item(0, getter_AddRefs(domNode));
    nsCOMPtr<nsIDOMXULElement> treeCell = do_QueryInterface(domNode);
    if (treeCell.get() == aTreeCell)
      return NS_OK;
  }

  // First clear our selection.
  ClearCellSelectionInternal();

  // Now add ourselves to the selection by setting our selected attribute.
  AddCellToSelectionInternal(aTreeCell);

  SetCurrentCell(aTreeCell);

  FireOnSelectHandler();

  return NS_OK;
}

NS_IMETHODIMP    
nsXULTreeElement::ClearItemSelection()
{
  ClearItemSelectionInternal();
  FireOnSelectHandler();
  return NS_OK;
}

void
nsXULTreeElement::ClearItemSelectionInternal()
{
  // Enumerate the elements and remove them from the selection.
  PRUint32 length;
  mSelectedItems->GetLength(&length);
  for (PRUint32 i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMNode> node;
    mSelectedItems->Item(0, getter_AddRefs(node));
    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    content->UnsetAttribute(kNameSpaceID_None, kSelectedAtom, PR_TRUE);
  }
}

void
nsXULTreeElement::ClearCellSelectionInternal()
{
  // Enumerate the elements and remove them from the selection.
  PRUint32 length;
  mSelectedCells->GetLength(&length);
  for (PRUint32 i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMNode> node;
    mSelectedCells->Item(0, getter_AddRefs(node));
    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    content->UnsetAttribute(kNameSpaceID_None, kSelectedAtom, PR_TRUE);
  }
}

NS_IMETHODIMP    
nsXULTreeElement::ClearCellSelection()
{
  ClearCellSelectionInternal();
  FireOnSelectHandler();
  return NS_OK;
}

void
nsXULTreeElement::AddItemToSelectionInternal(nsIDOMXULElement* aTreeItem)
{
  NS_ASSERTION(aTreeItem,"attepting to add a null tree item to the selection");
  if (!aTreeItem) return;

  // Without clearing the selection, perform the add.
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTreeItem);
  content->SetAttribute(kNameSpaceID_None, kSelectedAtom, NS_ConvertASCIItoUCS2("true"), PR_TRUE);
}

NS_IMETHODIMP
nsXULTreeElement::AddItemToSelection(nsIDOMXULElement* aTreeItem)
{
  // Without clearing the selection, perform the add.
  AddItemToSelectionInternal(aTreeItem);
  FireOnSelectHandler();
  return NS_OK;
}


void
nsXULTreeElement::RemoveItemFromSelectionInternal(nsIDOMXULElement* aTreeItem)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTreeItem);
  content->UnsetAttribute(kNameSpaceID_None, kSelectedAtom, PR_TRUE);
}

NS_IMETHODIMP
nsXULTreeElement::RemoveItemFromSelection(nsIDOMXULElement* aTreeItem)
{
  RemoveItemFromSelectionInternal(aTreeItem);
  FireOnSelectHandler();
  return NS_OK;
}

void
nsXULTreeElement::AddCellToSelectionInternal(nsIDOMXULElement* aTreeCell)
{
  // Without clearing the selection, perform the add.
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTreeCell);
  content->SetAttribute(kNameSpaceID_None, kSelectedAtom, NS_ConvertASCIItoUCS2("true"), PR_TRUE);
}

NS_IMETHODIMP
nsXULTreeElement::AddCellToSelection(nsIDOMXULElement* aTreeCell)
{
  AddCellToSelectionInternal(aTreeCell);
  FireOnSelectHandler();
  return NS_OK;
}

void
nsXULTreeElement::RemoveCellFromSelectionInternal(nsIDOMXULElement* aTreeCell)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTreeCell);
  content->UnsetAttribute(kNameSpaceID_None, kSelectedAtom, PR_TRUE);
}

NS_IMETHODIMP
nsXULTreeElement::RemoveCellFromSelection(nsIDOMXULElement* aTreeCell)
{
  RemoveCellFromSelectionInternal(aTreeCell);
  FireOnSelectHandler();
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::ToggleItemSelection(nsIDOMXULElement* aTreeItem)
{
  PRUint32 length;
  mSelectedItems->GetLength(&length);
  
  nsAutoString multiple;
  mOuter->GetAttribute(NS_ConvertASCIItoUCS2("multiple"), multiple);

  nsAutoString isSelected;
  aTreeItem->GetAttribute(NS_ConvertASCIItoUCS2("selected"), isSelected);
  if (isSelected.EqualsWithConversion("true"))
    RemoveItemFromSelectionInternal(aTreeItem);
  else if (multiple.EqualsWithConversion("true") || length == 0)
    AddItemToSelectionInternal(aTreeItem);
  else 
    return NS_OK;

  SetCurrentItem(aTreeItem);

  FireOnSelectHandler();
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::ToggleCellSelection(nsIDOMXULElement* aTreeCell)
{
  PRUint32 length;
  mSelectedCells->GetLength(&length);
  
  nsAutoString multiple;
  mOuter->GetAttribute(NS_ConvertASCIItoUCS2("multiple"), multiple);

  nsAutoString isSelected;
  aTreeCell->GetAttribute(NS_ConvertASCIItoUCS2("selected"), isSelected);
  if (isSelected.EqualsWithConversion("true"))
    RemoveCellFromSelectionInternal(aTreeCell);
  else if (multiple.EqualsWithConversion("true") || length == 0)
    AddCellToSelectionInternal(aTreeCell);
  else 
    return NS_OK;

  SetCurrentCell(aTreeCell);

  FireOnSelectHandler();

  return NS_OK;
}


NS_IMETHODIMP
nsXULTreeElement::SelectItemRange(nsIDOMXULElement* aStartItem, nsIDOMXULElement* aEndItem)
{
  nsAutoString multiple;
  mOuter->GetAttribute(NS_ConvertASCIItoUCS2("multiple"), multiple);

  if (!multiple.EqualsWithConversion("true")) {
    // We're a single selection tree only. This
    // is not allowed.
    return NS_OK;
  }

  nsCOMPtr<nsIDOMXULElement> startItem;
  if (aStartItem == nsnull) {
    // Continue the ranged selection based off the current item.
    startItem = mCurrentItem;
  }
  else startItem = aStartItem;

  if (!startItem)
    startItem = aEndItem;

  // First clear our selection out completely.
  ClearItemSelectionInternal();

  // Get a range so we can create an iterator
	nsCOMPtr<nsIDOMRange> range;
	nsresult result;
	result = nsComponentManager::CreateInstance(kCRangeCID, nsnull, 
						NS_GET_IID(nsIDOMRange), getter_AddRefs(range));

	PRInt32 startIndex = 0;
	PRInt32 endIndex = 0;

  nsCOMPtr<nsIDOMNode> startParentNode;
  nsCOMPtr<nsIDOMNode> endParentNode;

  startItem->GetParentNode(getter_AddRefs(startParentNode));
  aEndItem->GetParentNode(getter_AddRefs(endParentNode));

  nsCOMPtr<nsIContent> startParent = do_QueryInterface(startParentNode);
  nsCOMPtr<nsIContent> endParent = do_QueryInterface(endParentNode);

  nsCOMPtr<nsIContent> startItemContent = do_QueryInterface(startItem);
  nsCOMPtr<nsIContent> endItemContent = do_QueryInterface(aEndItem);
  startParent->IndexOf(startItemContent, startIndex);
	endParent->IndexOf(endItemContent, endIndex);

	result = range->SetStart(startParentNode, startIndex);
	result = range->SetEnd(endParentNode, endIndex+1);
	if (NS_FAILED(result) || 
      ((startParentNode.get() == endParentNode.get()) && (startIndex == endIndex+1)))
	{
		// Ranges need to be increasing, try reversing directions
		result = range->SetStart(endParentNode, endIndex);
		result = range->SetEnd(startParentNode, startIndex+1);
		if (NS_FAILED(result))
			return NS_ERROR_FAILURE;
	}

	// Create the iterator
	nsCOMPtr<nsIContentIterator> iter;
	result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
																							NS_GET_IID(nsIContentIterator), 
																							getter_AddRefs(iter));
	if (NS_FAILED(result))
    return result;

	// Iterate and select
	nsAutoString trueString; trueString.AssignWithConversion("true", 4);
	nsCOMPtr<nsIContent> content;
	nsCOMPtr<nsIAtom> tag;

	iter->Init(range);
	result = iter->First();
	while (NS_SUCCEEDED(result) && NS_ENUMERATOR_FALSE == iter->IsDone())
	{
		result = iter->CurrentNode(getter_AddRefs(content));
		if (NS_FAILED(result) || !content)
			return result; // result;

		// If tag==item, Do selection stuff
    content->GetTag(*getter_AddRefs(tag));
    if (tag && tag.get() == kTreeItemAtom)
    {
      // Only select if we aren't already selected.
			content->SetAttribute(kNameSpaceID_None, kSelectedAtom, 
														trueString, /*aNotify*/ PR_TRUE);
		}

		result = iter->Next();
		// XXX Deal with closed nodes here
		// XXX Also had strangeness where parent of selected subrange was selected even 
		// though it wasn't in the range.
	}

  SetCurrentItem(aEndItem);
  FireOnSelectHandler();

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SelectCellRange(nsIDOMXULElement* aStartItem, nsIDOMXULElement* aEndItem)
{
  // XXX Fill in.
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SelectAll()
{
  nsIDOMXULElement* oldItem = mCurrentItem;

  PRInt32 childCount;
  nsCOMPtr<nsIContent> content = do_QueryInterface(mOuter);
  content->ChildCount(childCount);
  if (childCount == 0)
    return NS_OK;

  nsCOMPtr<nsIContent> startContent;
  content->ChildAt(0, *getter_AddRefs(startContent));
  nsCOMPtr<nsIContent> endContent;
  content->ChildAt(childCount-1, *getter_AddRefs(endContent));

  nsCOMPtr<nsIDOMXULElement> startElement = do_QueryInterface(startContent);
  nsCOMPtr<nsIDOMXULElement> endElement = do_QueryInterface(endContent);

  // Select the whole range.
  SelectItemRange(startElement, endElement);

  // We shouldn't move the active item.
  mCurrentItem = oldItem;

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::InvertSelection()
{
  // XXX Woo hoo. Write this later.
  // Yikes. Involves an enumeration of the whole tree.
  return NS_OK;
}

nsresult
nsXULTreeElement::FireOnSelectHandler()
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mOuter);
  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));

  // If there's no document (e.g., a selection is occuring in a
  // 'orphaned' node), then there ain't a whole lot to do here!
  if (! document) {
    NS_WARNING("FireOnSelectHandler occurred in orphaned node");
    return NS_OK;
  }

  // The frame code can suppress the firing of this handler by setting an attribute
  // for us.  Look for that and bail if it's present.
  nsCOMPtr<nsIAtom> kSuppressSelectChange = dont_AddRef(NS_NewAtom("suppressonselect"));
  nsAutoString value;
  content->GetAttribute(kNameSpaceID_None, kSuppressSelectChange, value);
  if (value.EqualsWithConversion("true"))
    return NS_OK;

  PRInt32 count = document->GetNumberOfShells();
	for (PRInt32 i = 0; i < count; i++) {
		nsCOMPtr<nsIPresShell> shell = getter_AddRefs(document->GetShellAt(i));
		if (nsnull == shell)
				continue;

		// Retrieve the context in which our DOM event will fire.
		nsCOMPtr<nsIPresContext> aPresContext;
		shell->GetPresContext(getter_AddRefs(aPresContext));
				
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_FORM_SELECTED;

    content->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  }

  return NS_OK;
}


// get the rowindex of the given element
// this has two special optimizations:
// - if you're looking for the row of a <treeitem>, it will actually
//   find the index of the NEXT row
// - if you're looking for a <treeitem>, <treerow>, or a <treechildren>
//   it won't descend into <treerow>s
nsresult
nsXULTreeElement::GetRowIndexOf(nsIDOMXULElement *aElement, PRInt32 *aReturn)
{
  NS_ENSURE_ARG_POINTER(aElement);
  NS_ENSURE_ARG_POINTER(aReturn);

  nsresult rv;

  nsCOMPtr<nsIContent> elementContent = do_QueryInterface(aElement, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIContent> treeContent =
    do_QueryInterface((nsIXULTreeContent*)this,&rv);
  if (NS_FAILED(rv)) return rv;
  
  *aReturn = 0;

  nsCOMPtr<nsIAtom> elementTag;
  elementContent->GetTag(*getter_AddRefs(elementTag));

  // if looking for a treeitem, get the first row instead
  if (elementTag.get() == kTreeItemAtom) {
    // search through immediate children for the first row
    PRInt32 childCount;
    elementContent->ChildCount(childCount);

    PRInt32 i;
    for (i=0; i< childCount; i++) {
      nsCOMPtr<nsIContent> childContent;
      elementContent->ChildAt(i, *getter_AddRefs(childContent));

      nsCOMPtr<nsIAtom> childTag;
      childContent->GetTag(*getter_AddRefs(childTag));

      // found it! fix elementContent and update tag
      if (childTag.get() == kTreeRowAtom) {
        elementContent = childContent;
        elementTag = childTag;
        break;
      }

    }
  }

  // if we're looking for a treeitem, treerow, or treechildren
  // there's no need to descend into cells
  PRBool descendIntoRows = PR_TRUE;
  if (elementTag.get() == kTreeRowAtom ||
      elementTag.get() == kTreeChildrenAtom ||
      elementTag.get() == kTreeItemAtom)
    descendIntoRows = PR_FALSE;

  // now begin with the first <treechildren> child of this node
  PRInt32 i;
  PRInt32 treeChildCount;
  nsCOMPtr<nsIContent> treeChildren;
  treeContent->ChildCount(treeChildCount);
  for (i=0; i<treeChildCount; i++) {
    treeChildren = null_nsCOMPtr();
    treeContent->ChildAt(i, *getter_AddRefs(treeChildren));
    
    nsCOMPtr<nsIAtom> tag;
    treeChildren->GetTag(*getter_AddRefs(tag));
    if (tag.get() == kTreeChildrenAtom)
      break;
  }

  if (treeChildren)
    return IndexOfContent(treeChildren, elementContent,
                          descendIntoRows, PR_TRUE /* aParentIsOpen */,
                          aReturn);
  
  NS_WARNING("EnsureContentVisible: tree has no <treechildren>");
  return NS_ERROR_FAILURE;
}


nsresult
nsXULTreeElement::EnsureElementIsVisible(nsIDOMXULElement *aElement)
{
  if (!aElement) return NS_OK;

  nsresult rv;
  
  PRInt32 indexOfContent;
  rv = GetRowIndexOf(aElement, &indexOfContent);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIContent> content = do_QueryInterface(mOuter);
  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));

  // If there's no document (e.g., a selection is occuring in a
  // 'orphaned' node), then there ain't a whole lot to do here!
  if (! document) {
    NS_WARNING("Trying to EnsureElementIsVisible on orphaned element!");
    return NS_OK;
  }

  // now call EnsureElementIsVisible on all the frames
  PRInt32 count = document->GetNumberOfShells();
	for (PRInt32 i = 0; i < count; i++) {
		nsCOMPtr<nsIPresShell> shell = getter_AddRefs(document->GetShellAt(i));
		if (!shell)
				continue;

    nsIFrame *outerFrame;
    shell->GetPrimaryFrameFor(content, &outerFrame);

    if (outerFrame) {
      nsCOMPtr<nsIPresContext> presContext;
      shell->GetPresContext(getter_AddRefs(presContext));

      // need to look at the outer frame's children to find the nsTreeFrame
      nsIFrame *childFrame=nsnull;
      outerFrame->FirstChild(presContext, nsnull, &childFrame);

      // now iterate through the children
      while (childFrame) {
        nsITreeFrame *treeFrame = nsnull;      
        rv = childFrame->QueryInterface(NS_GET_IID(nsITreeFrame),
                                        (void **)&treeFrame);
        if (NS_SUCCEEDED(rv) && treeFrame) {
          treeFrame->EnsureRowIsVisible(indexOfContent);
        }

        nsIFrame *nextFrame;
        childFrame->GetNextSibling(&nextFrame);
        childFrame=nextFrame;
      }
    }

  }

  return NS_OK;
  
  
}


// helper routine for GetRowIndexOf()
// walks the DOM to get the zero-based row index of the current content
// note that aContent can be any element, this will get the index of the
// element's parent
nsresult
nsXULTreeElement::IndexOfContent(nsIContent* aRoot,
                                 nsIContent* aContent, // invariant
                                 PRBool aDescendIntoRows, // invariant
                                 PRBool aParentIsOpen,
                                 PRInt32 *aResult)
{
  PRInt32 childCount=0;
  aRoot->ChildCount(childCount);

  nsresult rv;
  
  PRInt32 childIndex;
  for (childIndex=0; childIndex<childCount; childIndex++) {
    nsCOMPtr<nsIContent> child;
    aRoot->ChildAt(childIndex, *getter_AddRefs(child));
    
    nsCOMPtr<nsIAtom> childTag;
    child->GetTag(*getter_AddRefs(childTag));

    // is this it?
    if (child.get() == aContent)
      return NS_OK;

    // we hit a treerow, count it
    if (childTag.get() == kTreeRowAtom)
      (*aResult)++;

    // now recurse. This gets tricky, depending on our tag:
    // first set up some default state for the recursion
    PRBool parentIsOpen = aParentIsOpen;
    PRBool descend = PR_TRUE;
    
    // don't descend into closed children
    if (childTag.get() == kTreeChildrenAtom && !parentIsOpen)
      descend = PR_FALSE;

    // speed optimization - descend into rows only when told
    else if (childTag.get() == kTreeRowAtom && !aDescendIntoRows)
      descend = PR_FALSE;

    // descend as normally, but remember that the parent is closed!
    else if (childTag.get() == kTreeItemAtom) {
      nsAutoString isOpen;
      rv = child->GetAttribute(kNameSpaceID_None, kOpenAtom, isOpen);

      if (!isOpen.EqualsWithConversion("true"))
        parentIsOpen=PR_FALSE;
    }

    // now that we've analyzed the tags, recurse
    if (descend) {
      rv = IndexOfContent(child, aContent,
                          aDescendIntoRows, parentIsOpen, aResult);
      if (NS_SUCCEEDED(rv))
        return NS_OK;
    }
  }

  // not found
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULTreeElement::GetCurrentItem(nsIDOMXULElement** aResult)
{
  *aResult = mCurrentItem;
  NS_IF_ADDREF(mCurrentItem);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::GetCurrentCell(nsIDOMXULElement** aResult)
{
  *aResult = mCurrentCell;
  NS_IF_ADDREF(mCurrentCell);
  return NS_OK;
}

void
nsXULTreeElement::SetCurrentItem(nsIDOMXULElement* aCurrentItem)
{
  mCurrentItem = aCurrentItem;
  nsCOMPtr<nsIContent> current = do_QueryInterface(mCurrentItem);
  current->SetAttribute(kNameSpaceID_None, kCurrentAtom, NS_ConvertASCIItoUCS2("true"), PR_TRUE);
}

void
nsXULTreeElement::SetCurrentCell(nsIDOMXULElement* aCurrentCell)
{
  mCurrentCell = aCurrentCell;
  nsCOMPtr<nsIContent> current = do_QueryInterface(mCurrentCell);
  current->SetAttribute(kNameSpaceID_None, kCurrentAtom, NS_ConvertASCIItoUCS2("true"), PR_TRUE);
}


