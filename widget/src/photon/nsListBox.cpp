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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsListBox.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include <Pt.h>
#include "nsPhWidgetLog.h"


NS_IMPL_ADDREF(nsListBox)
NS_IMPL_RELEASE(nsListBox)

//-------------------------------------------------------------------------
//
// nsListBox constructor
//
//-------------------------------------------------------------------------
nsListBox::nsListBox() : nsWidget(), nsIListWidget(), nsIListBox()
{
  NS_INIT_REFCNT();
  mMultiSelect = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsListBox:: destructor
//
//-------------------------------------------------------------------------
nsListBox::~nsListBox()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::~nsListBox - Destructor Called\n"));
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsListBox::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsListBoxIID, NS_ILISTBOX_IID);
    static NS_DEFINE_IID(kInsListWidgetIID, NS_ILISTWIDGET_IID);
    if (result == NS_NOINTERFACE) {
      if (aIID.Equals(kInsListBoxIID)) {
        *aInstancePtr = (void*) ((nsIListBox*)this);
        AddRef();
        result = NS_OK;
      }
      else if (aIID.Equals(kInsListWidgetIID)) {
        *aInstancePtr = (void*) ((nsIListWidget*)this);
        AddRef();
        result = NS_OK;
      }
    }

    return result;
}


//-------------------------------------------------------------------------
//
//  initializer
//
//-------------------------------------------------------------------------

NS_METHOD nsListBox::SetMultipleSelection(PRBool aMultipleSelections)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::SetMultipleSelection aMultipleSelections=<%d>\n", aMultipleSelections));

  PtArg_t arg;
  nsresult res = NS_ERROR_FAILURE;
    
  mMultiSelect = aMultipleSelections;

  if (mWidget)
  {
    if (mMultiSelect)
      PtSetArg( &arg, Pt_ARG_SELECTION_MODE, (Pt_EXTENDED_MODE), 0);
    else
      PtSetArg( &arg, Pt_ARG_SELECTION_MODE, (Pt_SINGLE_MODE), 0);

    if ( PtSetResources(mWidget, 1, &arg ) == 0)
      res = NS_OK;
  }
	
  return NS_OK;
}


//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------

NS_METHOD nsListBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  char *str = aItem.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::AddItemAt aItem=<%s> aPosition=<%d>\n", str, aPosition));

  PtArg_t arg;
  nsresult res = NS_ERROR_FAILURE;
  int err;
    
  if (mWidget)
  {
    err = PtListAddItems(mWidget, (const char **) &str, 1, (unsigned int) (aPosition+1));
    if (err == 0)
      res = NS_OK;
  }

  delete [] str;
  return res;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  char *str = aItem.ToNewCString();
  int index = -1;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::AddItemAt aItem=<%s> aStartPos=<%d>\n", str, aStartPos));
  
  if (mWidget)
  {
    index = PtListItemPos(mWidget, str);
	if (index == 0)
	  index = -1;	/* item wasn't found */
  }
  

  delete [] str;
  return index;
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::GetItemCount()
{
  PtArg_t  arg;
  short    *theCount;
  PRInt32  total = 0;

  if (mWidget)
  {
    PtSetArg( &arg, Pt_ARG_LIST_ITEM_COUNT, &theCount, 0);
    PtGetResources(mWidget, 1, &arg );

	total = *theCount;
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::GetItemCount is <%d>\n", total));
  return total;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsListBox::RemoveItemAt(PRInt32 aPosition)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::RemoveItemAt %d\n", aPosition));

  int err;
  PRBool res = PR_FALSE;
  
  if (mWidget)
  {
    if (PtListDeleteItemPos(mWidget, 1, (aPosition+1)) == 0)
      res = PR_TRUE;
  }
  
  return res;	  
}

//-------------------------------------------------------------------------
//
//
//
//-------------------------------------------------------------------------
PRBool nsListBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
  PRBool result = PR_FALSE;
  short *num;
  char **items;
  PtArg_t  arg;
    
  /* Clear the nsString */
  anItem.SetLength(0);

  if (mWidget)
  {
    PtSetArg( &arg, Pt_ARG_ITEMS, &items, &num);
    PtGetResources(mWidget, 1, &arg );

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::GetItemAt aPosition=<%d> aItem=<%s>\n", aPosition, items[aPosition]));

    anItem.Append(items[aPosition]);
    result = PR_TRUE;
  }    

  return result;
}

//-------------------------------------------------------------------------
//
//  Gets the text from the first selected Item
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::GetSelectedItem(nsString& aItem)
{
  int theFirstSelectedItem = GetSelectedIndex();
  nsresult res = NS_ERROR_FAILURE;
  

  if (mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::GetSelectedItem theFirstSelectedItem is <%d>\n", theFirstSelectedItem));

    GetItemAt(aItem, theFirstSelectedItem);

    res = NS_OK;
  }
	
  return res;
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected items
//  Gets the Position of the first selected Item or -1 if no items.
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedIndex()
{
//  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::GetSelectedIndex - Not Implemented\n"));

  PRInt32 index=-1;
  int theFirstSelectedItem[1];
  nsresult res = NS_ERROR_FAILURE;
  
  if (mWidget)
  {
    GetSelectedIndices(theFirstSelectedItem, 1);

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::GetSelectedIndex theFirstSelectedItem is <%d>\n", theFirstSelectedItem[0]));

    index = theFirstSelectedItem[0];
    res = NS_OK;
  }

  return index;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SelectItem(PRInt32 aPosition)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::SelectItem %d\n", aPosition));
  nsresult res = NS_ERROR_FAILURE;
  
  if (mWidget)
  {
    PtListSelectPos(mWidget, (aPosition+1));  /* Photon is 1 based */
    res = NS_OK;
  }

  return res;
}

//-------------------------------------------------------------------------
//
//  GetSelectedCount
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedCount()
{
  PtArg_t  arg;
  short    *theCount;
  PRInt32  total = 0;
        
  if (mWidget)
  {
    PtSetArg( &arg, Pt_ARG_LIST_SEL_COUNT, &theCount, 0);
    PtGetResources(mWidget, 1, &arg );

	total = *theCount;
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::GetSelectedCount theCount=<%d>\n", total));
  return total;
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::GetSelectedIndices aSize=<%d>\n", aSize));

  PRInt32 i=0, num = 0;
  short *theSelectedIndices, *theSelectedCount;
  nsresult res = NS_ERROR_FAILURE;
  PtArg_t  arg;

  if (mWidget)
  {
    PtSetArg( &arg, Pt_ARG_SELECTION_INDEXES, &theSelectedIndices, &theSelectedCount);
    PtGetResources(mWidget, 1, &arg );

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::GetSelectedIndices Found %d Selected Items\n", *theSelectedCount));

    for (i=0; i < *theSelectedCount && i < aSize; i++)
	{
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::GetSelectedIndices %d is Selected.\n", theSelectedIndices[i]));

	  aIndices[i] = (theSelectedIndices[i]-1) ; /* photon is 1 based, mozilla appears to be 0 based */
    }	

    res = NS_OK;
  }
  
  return res;
}

//-------------------------------------------------------------------------
//
//  SetSelectedIndices
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::SetSelectedIndices\n"));

  /* De-Select Everything ... */
  Deselect();

  int i;
  for (i=0;i<aSize;i++)
  {
    SelectItem(aIndices[i]);
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::Deselect()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::Deselect\n"));

  nsresult res = NS_ERROR_FAILURE;
  
  if (mWidget)
  {
    const int theSelectedCount = GetSelectedCount();
    int theList[theSelectedCount];
    int i;
  
    GetSelectedIndices(theList, theSelectedCount);
    for(i=0;i<theSelectedCount;i++)
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::Deselect Unselecting <%d>\n", theList[i]));
      PtListUnselectPos(mWidget, theList[i]); 
    }

    res = NS_OK;
  }

  return res;
}

//-------------------------------------------------------------------------
//
// Set initial parameters
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::PreCreateWidget(nsWidgetInitData *aInitData)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::PreCreateWidget - Not Implemented\n"));

  if (nsnull != aInitData) {
    nsListBoxInitData* data = (nsListBoxInitData *) aInitData;
    mMultiSelect = data->mMultiSelect;
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create the native widget
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::CreateNative(PtWidget_t *parentWindow)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::CreateNative\n"));

  nsresult  res = NS_ERROR_FAILURE;
  PtArg_t   arg[5];
  PhPoint_t pos;
  PhDim_t   dim;

  pos.x = mBounds.x;
  pos.y = mBounds.y;
  dim.w = mBounds.width - 4; // Correct for border width
  dim.h = mBounds.height - 4;

  PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
  PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
  PtSetArg( &arg[2], Pt_ARG_BORDER_WIDTH, 2, 0 );

  mWidget = PtCreateWidget( PtList, parentWindow, 3, arg );
  if( mWidget )
  {
    res = NS_OK;

    /* Mozilla calls this first before we have a widget so re-call this */
    SetMultipleSelection(mMultiSelect);

    /* Add a Callback so I can keep track of whats selected or not */
//    PtAddCallback(mWidget, Pt_CB_SELECTION, handle_selection_event, this);
  }

  return res;  
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsListBox::OnMove(PRInt32, PRInt32)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::OnMove - Not Implemented\n"));

  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsListBox::OnPaint(nsPaintEvent &aEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::OnPaint - Not Implemented\n"));

  return PR_FALSE;
}

PRBool nsListBox::OnResize(nsSizeEvent &aEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsListBox::OnResize - Not Implemented\n"));

  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Callback when user selects or deselects an item
//
//-------------------------------------------------------------------------
int nsListBox::handle_selection_event (PtWidget_t *aWidget, void *aData, PtCallbackInfo_t *aCbinfo )
{
  nsListBox *me = (nsListBox *) aData;

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsListBox::handle_select_event widget=<%p>\n",me));

  if ((me) && (aCbinfo->reason == Pt_CB_SELECTION) &&
     (aCbinfo->reason_subtype == Pt_LIST_SELECTION_FINAL))
  {
    PtListCallback_t  *ListCallback = (PtListCallback_t *) aCbinfo->cbdata;

//    PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsListBox::handle_select_event mode=<%d>\n",ListCal));

  }
  
  return (Pt_CONTINUE);
}
