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
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"


NS_IMPL_ADDREF(nsListBox)
NS_IMPL_RELEASE(nsListBox)

//-------------------------------------------------------------------------
//
//  initializer
//
//-------------------------------------------------------------------------

NS_METHOD nsListBox::SetMultipleSelection(PRBool aMultipleSelections) 
{
	mMultiSelect = aMultipleSelections;
	if(mListView && mListView->LockLooper())
	{
		mListView->SetListType(aMultipleSelections ? B_MULTIPLE_SELECTION_LIST : B_SINGLE_SELECTION_LIST);
		mListView->UnlockLooper();
	}
	return NS_OK;
}

NS_METHOD nsListBox::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    nsListBoxInitData* data = (nsListBoxInitData *) aInitData;
    mMultiSelect = data->mMultiSelect;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  destructor
//
//-------------------------------------------------------------------------

NS_METHOD nsListBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
	if(mListView && mListView->LockLooper())
	{
		NS_ALLOC_STR_BUF(val, aItem, 256);
		mListView->AddItem(new BStringItem(val), aPosition);
		NS_FREE_STR_BUF(val);
		mListView->UnlockLooper();
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
#if 0
  NS_ALLOC_STR_BUF(val, aItem, 256);
  int index = ::SendMessage(mWnd, LB_FINDSTRINGEXACT, (int)aStartPos, (LPARAM)(LPCTSTR)val); 
  NS_FREE_STR_BUF(val);

  return index;
#endif
	printf("nsListBox::FindItem not implemented\n");
	return -1;
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::GetItemCount()
{
	PRInt32	result = 0;
	if(mListView && mListView->LockLooper())
	{
		result = mListView->CountItems();
		mListView->UnlockLooper();
	}
	return result;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsListBox::RemoveItemAt(PRInt32 aPosition)
{
	if(mListView && mListView->LockLooper())
	{
		BListItem *it = mListView->RemoveItem(aPosition);
		delete it;
		mListView->UnlockLooper();
		return it ? PR_TRUE : PR_FALSE;
	}
	return PR_FALSE;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsListBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
	PRBool result = PR_FALSE;
	anItem.SetLength(0);
	if(mListView && mListView->LockLooper())
	{
		BListItem	*it = mListView->ItemAt(aPosition);
		BStringItem	*str;
		if((str = dynamic_cast<BStringItem *>(it)) != 0)
		{
			anItem.AppendWithConversion(str->Text());
			result = PR_TRUE;
		}
		mListView->UnlockLooper();
	}
	return result;
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::GetSelectedItem(nsString& aItem)
{
  if (!mMultiSelect) { 
    GetItemAt(aItem, GetSelectedIndex()); 
  } else {
    NS_ASSERTION(0, "Multi selection list box does not support GetSelectedItem()");
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedIndex()
{  
	if (!mMultiSelect)
	{
		PRInt32	index = -1;
		if(mListView && mListView->LockLooper())
		{
			index = mListView->CurrentSelection();
			mListView->UnlockLooper();
		}
		return index;
	} else {
		NS_ASSERTION(0, "Multi selection list box does not support GetSelectedIndex()");
	}
	return 0;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SelectItem(PRInt32 aPosition)
{
	if(mListView && mListView->LockLooper())
	{
		mListView->Select(aPosition);
		mListView->UnlockLooper();
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  GetSelectedCount
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedCount()
{
  if (!mMultiSelect) { 
    PRInt32 inx = GetSelectedIndex();
    return (inx == -1? 0 : 1);
  } else {
	PRInt32 count = 0;
	if(mListView && mListView->LockLooper())
	{
		int index = 0;
		while((index = mListView->CurrentSelection(index)) != -1)
			count++;
		mListView->UnlockLooper();
	}
	return count;
  }
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
	if(mListView && mListView->LockLooper())
	{
		int index = 0;
		for(int i = 0; i < aSize; i++)
		{
			if(index != -1)
				index = mListView->CurrentSelection(index);
			aIndices[i] = index;
		}
		mListView->UnlockLooper();
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  SetSelectedIndices 
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
	if(mListView && mListView->LockLooper())
	{
		mListView->Select(aIndices[0], false);
		for(int i = 1; i < aSize; i++)
			mListView->Select(aIndices[i], true);
		mListView->UnlockLooper();
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
	if(mListView && mListView->LockLooper())
	{
		mListView->DeselectAll();
		mListView->UnlockLooper();
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// nsListBox constructor
//
//-------------------------------------------------------------------------
nsListBox::nsListBox() : nsWindow(), nsIListWidget(), nsIListBox()
{
  NS_INIT_REFCNT();
  mMultiSelect = PR_FALSE;
  mBackground  = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsListBox:: destructor
//
//-------------------------------------------------------------------------
nsListBox::~nsListBox()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsListBox::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWindow::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsListBoxIID, NS_ILISTBOX_IID);
    static NS_DEFINE_IID(kInsListWidgetIID, NS_ILISTWIDGET_IID);
    if (result == NS_NOINTERFACE) {
      if (aIID.Equals(kInsListBoxIID)) {
        *aInstancePtr = (void*) ((nsIListBox*)this);
        NS_ADDREF_THIS();
        result = NS_OK;
      }
      else if (aIID.Equals(kInsListWidgetIID)) {
        *aInstancePtr = (void*) ((nsIListWidget*)this);
        NS_ADDREF_THIS();
        result = NS_OK;
      }
    }

    return result;
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsListBox::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsListBox::OnPaint(nsRect &r)
{
    return PR_FALSE;
}

PRBool nsListBox::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Clear window before paint
//
//-------------------------------------------------------------------------

PRBool nsListBox::AutoErase()
{
  return(PR_TRUE);
}


//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

NS_METHOD nsListBox::GetBounds(nsRect &aRect)
{
#if 0
  nsWindow::GetNonClientBounds(aRect);
#endif
printf("nsListBox::GetBounds not wrong\n");	// the following is just a placeholder
  nsWindow::GetClientBounds(aRect);
  return NS_OK;
}

BView *nsListBox::CreateBeOSView()
{
	return mListView = new nsListViewBeOS((nsIWidget *)this, BRect(0, 0, 0, 0), "");
}

//-------------------------------------------------------------------------
// Sub-class of BeOS ListView
//-------------------------------------------------------------------------
nsListViewBeOS::nsListViewBeOS( nsIWidget *aWidgetWindow, BRect aFrame, 
    const char *aName, uint32 aResizingMode, uint32 aFlags )
  : BListView( aFrame, aName,
		((nsListBox *)aWidgetWindow)->mMultiSelect ? B_MULTIPLE_SELECTION_LIST : B_SINGLE_SELECTION_LIST,
		aResizingMode, aFlags ),
    nsIWidgetStore( aWidgetWindow )
{
}
