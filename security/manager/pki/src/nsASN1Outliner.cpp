/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *  Javier Delgadillo <javi@netscape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include "nsASN1Outliner.h"
#include "nsIComponentManager.h"
#include "nsString.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsNSSASN1Outliner, nsIASN1Outliner, 
                                                 nsIOutlinerView);

nsNSSASN1Outliner::nsNSSASN1Outliner() 
{
  NS_INIT_ISUPPORTS();
}

nsNSSASN1Outliner::~nsNSSASN1Outliner()
{
}

/* void loadASN1Structure (in nsIASN1Object asn1Object); */
NS_IMETHODIMP 
nsNSSASN1Outliner::LoadASN1Structure(nsIASN1Object *asn1Object)
{
  //
  // The outliner won't automatically re-draw if the contents
  // have been changed.  So I do a quick test here to let
  // me know if I should forced the outliner to redraw itself
  // by calling RowCountChanged on it.
  //
  PRBool redraw = (mASN1Object && mOutliner);
  PRInt32 rowsToDelete;

  if (redraw) {
    // This is the number of rows we will be deleting after
    // the contents have changed.
    rowsToDelete = 0-CountNumberOfVisibleRows(mASN1Object);
  }
  mASN1Object = asn1Object;
  if (redraw) {
    // The number of rows in the new content.
    PRInt32 newRows = CountNumberOfVisibleRows(mASN1Object);
    // Erase all of the old rows.
    mOutliner->RowCountChanged(0, rowsToDelete);
    // Replace them with the new contents
    mOutliner->RowCountChanged(0, newRows);
  }
  return NS_OK;
}

/* wstring getDisplayData (in unsigned long index); */
NS_IMETHODIMP 
nsNSSASN1Outliner::GetDisplayData(PRUint32 index, PRUnichar **_retval)
{
  nsCOMPtr<nsIASN1Object> object;
  GetASN1ObjectAtIndex(index, mASN1Object, getter_AddRefs(object));
  if (object) {
    object->GetDisplayValue(_retval);
  } else {
    *_retval = nsnull;
  }
  return NS_OK;
}

nsresult
nsNSSASN1Outliner::GetASN1ObjectAtIndex(PRUint32 index, 
                                        nsIASN1Object *sourceObject,
                                        nsIASN1Object **retval)
{
  if (mASN1Object == nsnull) {
    *retval = nsnull;
  } else {
    if (index == 0) {
      *retval =  sourceObject;
      NS_IF_ADDREF(*retval);
      return NS_OK;
    }
    // the source object better be an nsIASN1Sequence, otherwise,
    // the index better be 1.  If neither of these is ture, then
    // someting bad has happened.
    nsCOMPtr<nsIASN1Sequence> sequence = do_QueryInterface(sourceObject);
    if (sequence == nsnull) {
        //Something really bad has happened. bail out.
        *retval = nsnull;
        return NS_ERROR_FAILURE;
    } else {
      PRBool showObjects;
      sequence->GetShowObjects(&showObjects);
      if (!showObjects) {
        *retval = nsnull;
        return NS_OK;
      }
      nsCOMPtr<nsISupportsArray>asn1Objects;
      sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
      PRUint32 numObjects;
      asn1Objects->Count(&numObjects);
      PRUint32 i;
      nsCOMPtr<nsISupports>isupports;
      nsCOMPtr<nsIASN1Object>currObject;
      PRUint32 numObjectsCounted = 0;
      PRUint32 numObjToDisplay;
      for (i=0; i<numObjects; i++) {
        isupports = dont_AddRef(asn1Objects->ElementAt(i));
        currObject = do_QueryInterface(isupports);
        numObjToDisplay = CountNumberOfVisibleRows(currObject);
        if ((numObjectsCounted+numObjToDisplay) >= index) {
         return GetASN1ObjectAtIndex(index-numObjectsCounted-1,
                                     currObject, retval);
        }
        numObjectsCounted += numObjToDisplay;
      } 
    }
  }
  // We should never get here.
  return NS_ERROR_FAILURE;
}

PRUint32
nsNSSASN1Outliner::CountNumberOfVisibleRows(nsIASN1Object *asn1Object)
{
  nsCOMPtr<nsIASN1Sequence> sequence;
  PRUint32 count = 1;
  
  sequence = do_QueryInterface(asn1Object);
  if (sequence) {
    PRBool showObjects;
    sequence->GetShowObjects(&showObjects);
    if (showObjects) {
      nsCOMPtr<nsISupportsArray> asn1Objects;
      sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
      PRUint32 numObjects;
      asn1Objects->Count(&numObjects);
      PRUint32 i;
      nsCOMPtr<nsISupports> isupports;
      nsCOMPtr<nsIASN1Object> currObject;
      for (i=0; i<numObjects; i++) {
        isupports = dont_AddRef(asn1Objects->ElementAt(i));
        currObject = do_QueryInterface(isupports);
        count += CountNumberOfVisibleRows(currObject);
      }
    }
  }
  return count;
}

/* readonly attribute long rowCount; */
NS_IMETHODIMP 
nsNSSASN1Outliner::GetRowCount(PRInt32 *aRowCount)
{
  if (mASN1Object) {
    *aRowCount = CountNumberOfVisibleRows(mASN1Object);
  } else {
    *aRowCount = 0;
  }
  return NS_OK;
}

/* attribute nsIOutlinerSelection selection; */
NS_IMETHODIMP 
nsNSSASN1Outliner::GetSelection(nsIOutlinerSelection * *aSelection)
{
  *aSelection = mSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Outliner::SetSelection(nsIOutlinerSelection * aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

/* void getRowProperties (in long index, in nsISupportsArray properties); */
NS_IMETHODIMP 
nsNSSASN1Outliner::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
  return NS_OK;
}

/* void getCellProperties (in long row, in wstring colID, 
                           in nsISupportsArray properties); */
NS_IMETHODIMP 
nsNSSASN1Outliner::GetCellProperties(PRInt32 row, const PRUnichar *colID, 
                                     nsISupportsArray *properties)
{
  return NS_OK;
}

/* void getColumnProperties (in wstring colID, in nsIDOMElement colElt, 
                             in nsISupportsArray properties); */
NS_IMETHODIMP 
nsNSSASN1Outliner::GetColumnProperties(const PRUnichar *colID, 
                                       nsIDOMElement *colElt, 
                                       nsISupportsArray *properties)
{
  return NS_OK;
}

/* boolean isContainer (in long index); */
NS_IMETHODIMP 
nsNSSASN1Outliner::IsContainer(PRInt32 index, PRBool *_retval)
{
  nsCOMPtr<nsIASN1Object> object;
  nsCOMPtr<nsIASN1Sequence> sequence;

  nsresult rv = GetASN1ObjectAtIndex(index, mASN1Object, 
                                     getter_AddRefs(object));
  if (NS_FAILED(rv))
    return rv;

  sequence = do_QueryInterface(object);
  if (sequence != nsnull) {
    sequence->GetProcessObjects(_retval);
  } else {
    *_retval =  PR_FALSE;
  }
  return NS_OK; 
}

/* boolean isContainerOpen (in long index); */
NS_IMETHODIMP 
nsNSSASN1Outliner::IsContainerOpen(PRInt32 index, PRBool *_retval)
{
  nsCOMPtr<nsIASN1Object> object;
  nsCOMPtr<nsIASN1Sequence> sequence;

  nsresult rv = GetASN1ObjectAtIndex(index, mASN1Object,
                                     getter_AddRefs(object));
  if (NS_FAILED(rv))
    return rv;

  sequence = do_QueryInterface(object);
  if (sequence == nsnull) {
    *_retval = PR_FALSE;
  } else {
    sequence->GetShowObjects(_retval);
  }
  return NS_OK;
}

/* boolean isContainerEmpty (in long index); */
NS_IMETHODIMP 
nsNSSASN1Outliner::IsContainerEmpty(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* boolean isSeparator (in long index); */
NS_IMETHODIMP 
nsNSSASN1Outliner::IsSeparator(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK; 
}

PRInt32
nsNSSASN1Outliner::GetParentOfObjectAtIndex(PRUint32 index,
                                            nsIASN1Object *sourceObject)
{
  if (index == 0) {
    return -1;
  } else {
    PRUint32 numVisibleRows = CountNumberOfVisibleRows(sourceObject);
    if (numVisibleRows > index) {
      nsCOMPtr<nsIASN1Sequence>sequence(do_QueryInterface(sourceObject));
      if (sequence == nsnull)
        return -2;
      nsCOMPtr<nsISupportsArray>asn1Objects;
      nsCOMPtr<nsISupports>isupports;
      nsCOMPtr<nsIASN1Object>currObject;
      sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
      PRUint32 indexCnt = 0;
      PRUint32 i,numObjects;
      asn1Objects->Count(&numObjects);
      for (i=0; i<numObjects; i++) {
        isupports = dont_AddRef(asn1Objects->ElementAt(i));
        currObject = do_QueryInterface(isupports);
        numVisibleRows = CountNumberOfVisibleRows(currObject);
        if (numVisibleRows+indexCnt > index) {
          //We're dealing with a sequence with visible elements
          //that has the desired element.
          PRInt32 subIndex = GetParentOfObjectAtIndex(index-indexCnt+1,
                                                      currObject);
          if (subIndex == -1) {
            return indexCnt+1;  
          } else if (subIndex == -2) {
            return -2;
          } else {
            // This is a case where a subIndex was returned.
            return indexCnt+1+subIndex;
          }
        } 
        indexCnt+=numVisibleRows;
        if (indexCnt == index) {
          // The passed in source object is the parent.
          return -1; 
        }
      }
    }// the else case is an error, just let it fall through.
  }
  return -2;
}

/* long getParentIndex (in long rowIndex); */
NS_IMETHODIMP 
nsNSSASN1Outliner::GetParentIndex(PRInt32 rowIndex, PRInt32 *_retval)
{
  *_retval = GetParentOfObjectAtIndex(rowIndex, mASN1Object);
  return NS_OK;
}

/* boolean hasNextSibling (in long rowIndex, in long afterIndex); */
NS_IMETHODIMP 
nsNSSASN1Outliner::HasNextSibling(PRInt32 rowIndex, PRInt32 afterIndex, 
                                  PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

PRInt32
nsNSSASN1Outliner::GetLevelsTilIndex(PRUint32 index, 
                                     nsIASN1Object *sourceObject)
{
  if (index == 0) {
    return 0;
  } else {
    nsCOMPtr<nsIASN1Sequence> sequence(do_QueryInterface(sourceObject));
    nsCOMPtr<nsISupportsArray>asn1Objects;
    if (sequence == nsnull)
      return -1;
    sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
    PRUint32 numObjects,i,indexCnt=0,numVisibleRows;
    asn1Objects->Count(&numObjects);
    nsCOMPtr<nsISupports> isupports;
    nsCOMPtr<nsIASN1Object> currObject;
    for (i=0; i<numObjects; i++) {
      isupports = dont_AddRef(asn1Objects->ElementAt(i));
      currObject = do_QueryInterface(isupports);
      numVisibleRows = CountNumberOfVisibleRows(currObject);
      if ((numVisibleRows+indexCnt)>=index) {
        PRInt32 numSubLayers;
        numSubLayers = GetLevelsTilIndex(index-indexCnt-1,
                                         currObject);
        if (numSubLayers == -1) {
          // This return value means the parent is not a child
          // object, we're not adding any more layers to the nested
          // levels.
          return -1;
        } else {
          return 1+numSubLayers;
        }
      }
      indexCnt += numVisibleRows;
    }
  }
  return -2;
}

/* long getLevel (in long index); */
NS_IMETHODIMP 
nsNSSASN1Outliner::GetLevel(PRInt32 index, PRInt32 *_retval)
{
  *_retval = GetLevelsTilIndex(index, mASN1Object);
  return NS_OK;
}

/* wstring getCellText (in long row, in wstring colID); */
NS_IMETHODIMP 
nsNSSASN1Outliner::GetCellText(PRInt32 row, const PRUnichar *colID, 
                               PRUnichar **_retval)
{
  nsCOMPtr<nsIASN1Object> object;
  *_retval = nsnull;
  char *col = NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(colID).get());
  nsresult rv = NS_OK;
  if (strcmp(col, "certDataCol") == 0) {
    rv = GetASN1ObjectAtIndex(row, mASN1Object,
                                     getter_AddRefs(object));
    if (NS_FAILED(rv))
      return rv;

    //There's only one column for ASN1 dump.
    rv = object->GetDisplayName(_retval);
  }
  return rv;
}

/* void setOutliner (in nsIOutlinerBoxObject outliner); */
NS_IMETHODIMP 
nsNSSASN1Outliner::SetOutliner(nsIOutlinerBoxObject *outliner)
{
  mOutliner = outliner;
  return NS_OK;
}

/* void toggleOpenState (in long index); */
NS_IMETHODIMP 
nsNSSASN1Outliner::ToggleOpenState(PRInt32 index)
{
  nsCOMPtr<nsIASN1Object> object;

  nsresult rv = GetASN1ObjectAtIndex(index, mASN1Object,
                                     getter_AddRefs(object));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIASN1Sequence> sequence(do_QueryInterface(object));
  if (sequence == nsnull)
    return NS_ERROR_FAILURE;

  PRBool showObjects;
  sequence->GetShowObjects(&showObjects);
  PRInt32 rowCountChange;
  if (showObjects) {
    rowCountChange = 1-CountNumberOfVisibleRows(object);
    sequence->SetShowObjects(PR_FALSE);
  } else {
    sequence->SetShowObjects(PR_TRUE);
    rowCountChange = CountNumberOfVisibleRows(object)-1;
  }
  if (mOutliner)
    mOutliner->RowCountChanged(index, rowCountChange);
  return NS_OK;
}

/* void cycleHeader (in wstring colID, in nsIDOMElement elt); */
NS_IMETHODIMP 
nsNSSASN1Outliner::CycleHeader(const PRUnichar *colID, nsIDOMElement *elt)
{
  return NS_OK;
}

/* void selectionChanged (); */
NS_IMETHODIMP 
nsNSSASN1Outliner::SelectionChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cycleCell (in long row, in wstring colID); */
NS_IMETHODIMP 
nsNSSASN1Outliner::CycleCell(PRInt32 row, const PRUnichar *colID)
{
  return NS_OK;
}

/* boolean isEditable (in long row, in wstring colID); */
NS_IMETHODIMP 
nsNSSASN1Outliner::IsEditable(PRInt32 row, const PRUnichar *colID, 
                              PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* void setCellText (in long row, in wstring colID, in wstring value); */
NS_IMETHODIMP 
nsNSSASN1Outliner::SetCellText(PRInt32 row, const PRUnichar *colID, 
                               const PRUnichar *value)
{
  return NS_OK;
}

/* void performAction (in wstring action); */
NS_IMETHODIMP 
nsNSSASN1Outliner::PerformAction(const PRUnichar *action)
{
  return NS_OK;
}

/* void performActionOnRow (in wstring action, in long row); */
NS_IMETHODIMP 
nsNSSASN1Outliner::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
  return NS_OK;
}

/* void performActionOnCell (in wstring action, in long row, in wstring colID); */
NS_IMETHODIMP 
nsNSSASN1Outliner::PerformActionOnCell(const PRUnichar *action, PRInt32 row, 
                                       const PRUnichar *colID)
{
  return NS_OK;
}

//
// CanDropOn
//
// Can't drop on the thread pane.
//
NS_IMETHODIMP nsNSSASN1Outliner::CanDropOn(PRInt32 index, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  return NS_OK;
}

//
// CanDropBeforeAfter
//
// Can't drop on the thread pane.
//
NS_IMETHODIMP nsNSSASN1Outliner::CanDropBeforeAfter(PRInt32 index, PRBool before, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  return NS_OK;
}


//
// Drop
//
// Can't drop on the thread pane.
//
NS_IMETHODIMP nsNSSASN1Outliner::Drop(PRInt32 row, PRInt32 orient)
{
  return NS_OK;
}


//
// IsSorted
//
// ...
//
NS_IMETHODIMP nsNSSASN1Outliner::IsSorted(PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

