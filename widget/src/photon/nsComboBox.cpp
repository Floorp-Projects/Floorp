/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsComboBox.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include "nsIDeviceContext.h"

#include <Pt.h>
#include "nsPhWidgetLog.h"

#define DBG 0

#define INITIAL_MAX_ITEMS 128
#define ITEMS_GROWSIZE    128

NS_IMPL_ADDREF(nsComboBox)
NS_IMPL_RELEASE(nsComboBox)

//-------------------------------------------------------------------------
//
// nsComboBox constructor
//
//-------------------------------------------------------------------------
nsComboBox::nsComboBox() : nsWidget(), nsIListWidget(), nsIComboBox()
{
  NS_INIT_REFCNT();
  mMultiSelect = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsComboBox:: destructor
//
//-------------------------------------------------------------------------
nsComboBox::~nsComboBox()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::~nsComboBox - Destructor called\n"));
}

//-------------------------------------------------------------------------
//
//  initializer
//
//-------------------------------------------------------------------------

NS_METHOD nsComboBox::SetMultipleSelection(PRBool aMultipleSelections)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::SetMultipleSelection - aMultipleSelections=<%d>\n", aMultipleSelections));

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
	
  return res;
}


//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------

NS_METHOD nsComboBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  char *str = aItem.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::AddItemAt - aItem=<%s> aPos=<%d>\n",str,aPosition));

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
PRInt32  nsComboBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  char *str = aItem.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::FindItem - aItem=<%s> aStartPos=<%d>\n",str, aStartPos));

  PRInt32 index = -1;

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
PRInt32  nsComboBox::GetItemCount()
{
  PtArg_t  arg;
  short    *theCount = nsnull;
  PRInt32  total = 0;

  if (mWidget)
  {
    PtSetArg( &arg, Pt_ARG_LIST_ITEM_COUNT, &theCount, 0);
    PtGetResources(mWidget, 1, &arg );

	total = *theCount;
  }
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::GetItemCount - mNumItems=<%d>\n", total));
  return (PRInt32) total;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsComboBox::RemoveItemAt(PRInt32 aPosition)
{ 
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::RemoveItemAt - aPos=<%d>\n", aPosition));

  int err;
  PRBool res = PR_FALSE;
  
  if (mWidget)
  {
    if (PtListDeleteItemPos(mWidget, 1, aPosition) == 0)
      res = PR_TRUE;
  }
  
  return res;
}

//-------------------------------------------------------------------------
//
//  Get an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsComboBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
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

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::GetItemAt aPosition=<%d> aItem=<%s>\n", aPosition, items[aPosition+1]));

    anItem.Append(items[aPosition+1]);
    result = PR_TRUE;
  }    

  return result;
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::GetSelectedItem(nsString& aItem)
{
  nsresult res = NS_ERROR_FAILURE;
  int      theFirstSelectedItem = GetSelectedIndex();

  if (mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::GetSelectedItem theFirstSelectedItem is <%d>\n", theFirstSelectedItem));
    GetItemAt(aItem, theFirstSelectedItem);
    res = NS_OK;
  }
	
  return res;
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected items
//
//-------------------------------------------------------------------------
PRInt32 nsComboBox::GetSelectedIndex()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::GetSelectedIndex\n"));

  PRInt32 index=-1;
  int theFirstSelectedItem[1];
  nsresult res = NS_ERROR_FAILURE;
  
  if (mWidget)
  {
    GetSelectedIndices(theFirstSelectedItem, 1);

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::GetSelectedIndex theFirstSelectedItem is <%d>\n", theFirstSelectedItem[0]));

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
NS_METHOD nsComboBox::SelectItem(PRInt32 aPosition)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::SelectItem at %d\n", aPosition));

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
PRInt32 nsComboBox::GetSelectedCount()
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

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::GetSelectedCount theCount=<%d>\n", total));
  return total;
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::GetSelectedIndices - Not Implemented\n"));

  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::Deselect()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::DeSelect - Not Implemented\n"));

  if (mMultiSelect) {
    return NS_ERROR_FAILURE;
  }

  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsComboBox::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kInsComboBoxIID, NS_ICOMBOBOX_IID);
  static NS_DEFINE_IID(kInsListWidgetIID, NS_ILISTWIDGET_IID);

  if (aIID.Equals(kInsComboBoxIID)) {
    *aInstancePtr = (void*) ((nsIComboBox*)this);
    AddRef();
    return NS_OK;
  }
  else if (aIID.Equals(kInsListWidgetIID)) {
    *aInstancePtr = (void*) ((nsIListWidget*)this);
    AddRef();
    return NS_OK;
  }

  return nsWidget::QueryInterface(aIID,aInstancePtr);
}


//-------------------------------------------------------------------------
//
// Create the native GtkCombo widget
//
//-------------------------------------------------------------------------
NS_METHOD  nsComboBox::CreateNative(PtWidget_t *parentWindow)
{
  nsresult  res = NS_ERROR_FAILURE;
  PtArg_t   arg[5];
  PhPoint_t pos;
  PhDim_t   dim;

  pos.x = mBounds.x;
  pos.y = mBounds.y;
  dim.w = mBounds.width - 4; // Correct for border width
  dim.h = mBounds.height - 4;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::CreateNative pos=(%d,%d) dim=(%d,%d)\n",
    pos.x, pos.y, dim.w, dim.h));


  PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
  PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
  PtSetArg( &arg[2], Pt_ARG_BORDER_WIDTH, 2, 0 );

  mWidget = PtCreateWidget( PtComboBox, parentWindow, 3, arg );
  if( mWidget )
  {
    res = NS_OK;

    /* Mozilla calls this first before we have a widget so re-call this */
    SetMultipleSelection(mMultiSelect);
  }

  return res;  
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsComboBox::OnMove(PRInt32, PRInt32)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::OnMove - Not Implemented\n"));

  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsComboBox::OnPaint(nsPaintEvent &aEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::OnPaint - Not Implemented\n"));

  return PR_FALSE;
}

PRBool nsComboBox::OnResize(nsSizeEvent &aEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsComboBox::OnResize - Not Implemented\n"));

    return PR_FALSE;
}
