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

#include "nsListBox.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

//=============================================================================
//
// nsQListBox class
//
//=============================================================================
nsQListBox::nsQListBox(nsWidget * widget,
                       QWidget * parent, 
                       const char * name, 
                       WFlags f)
	: QListBox(parent, name, f), nsQBaseWidget(widget)
{
}


nsQListBox::~nsQListBox()
{
}

NS_IMPL_ADDREF(nsListBox)
NS_IMPL_RELEASE(nsListBox)

//-------------------------------------------------------------------------
//
// nsListBox constructor
//
//-------------------------------------------------------------------------
nsListBox::nsListBox() : nsWidget(), nsIListWidget(), nsIListBox()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::nsListBox()\n"));
    mMultiSelect = PR_FALSE;
//  mBackground  = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsListBox:: destructor
//
//-------------------------------------------------------------------------
nsListBox::~nsListBox()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::~nsListBox()\n"));
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsListBox::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::QueryInterface()\n"));
    nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsListBoxIID, NS_ILISTBOX_IID);
    static NS_DEFINE_IID(kInsListWidgetIID, NS_ILISTWIDGET_IID);
    if (result == NS_NOINTERFACE) 
    {
        if (aIID.Equals(kInsListBoxIID)) 
        {
            *aInstancePtr = (void*) ((nsIListBox*)this);
            AddRef();
            result = NS_OK;
        }
        else if (aIID.Equals(kInsListWidgetIID)) 
        {
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
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::SetMultipleSelection()\n"));
    mMultiSelect = aMultipleSelections;

    if (mWidget)
    {
        ((QListBox *)mWidget)->setMultiSelection(mMultiSelect);
        QListBox::SelectionMode mode = (mMultiSelect ? QListBox::Multi : QListBox::Single);
        ((QListBox *)mWidget)->setSelectionMode(mode);
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
    char *buf = aItem.ToNewCString();

    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::AddItemAt: %s at %d\n",
                                       buf,
                                       aPosition));

    ((QListBox *)mWidget)->insertItem(buf, aPosition);

    delete[] buf;

    return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
    NS_ALLOC_STR_BUF(val, aItem, 256);
    int i;
    PRInt32 index = -1;

    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::FindItem: find %s at %d\n",
                                       val,
                                       aStartPos));

    int count = ((QListBox *)mWidget)->count();

    for (i = aStartPos; i < count; i++)
    {
        QString string = ((QListBox*)mWidget)->text(i);

        if (string == val)
        {
            index = i;
            break;
        }
    }

    NS_FREE_STR_BUF(val);

    if (index < aStartPos) 
    {
        index = -1;
    }

    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::FindItem: returning %d \n",
                                       index));

    return index;
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::GetItemCount()
{
    PRInt32 count = ((QListBox *)mWidget)->count();
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::GetItemCount: count=%d\n",
                                       count));
    return count;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsListBox::RemoveItemAt(PRInt32 aPosition)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::RemoveItemAt: remove at %d\n",
                                       aPosition));
    ((QListBox *)mWidget)->removeItem(aPosition);

    return PR_TRUE;
}

//-------------------------------------------------------------------------
//
//
//
//-------------------------------------------------------------------------
PRBool nsListBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::GetItemAt: get at %d\n",
                                       aPosition));
    PRBool result = PR_FALSE;

    QString text = ((QListBox *)mWidget)->text(aPosition);

    anItem.SetLength(0);
    anItem.Append((const char *) text);
    result = PR_TRUE;

    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::GetItemAt: returning %s\n",
                                       (const char *) text));

    return result;
}

//-------------------------------------------------------------------------
//
//  Gets the text of selected item
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::GetSelectedItem(nsString& aItem)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::GetSelectedItem()\n"));
    int item = ((QListBox *) mWidget)->currentItem();

    QString text = ((QListBox *) mWidget)->text(item);

    aItem.SetLength(0);
    aItem.Append((const char *) text);

    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsListBox::GetSelectedItem: returning %s\n",
            (const char *)text));

    return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedIndex()
{
    PRInt32 index=-1;
    if (!mMultiSelect) 
    {
        index = ((QListBox *)mWidget)->currentItem();
    } 
    else 
    {
        NS_ASSERTION(PR_FALSE, "Multi selection list box doesn't support GetSelectedIndex()");
    }

    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsListBox::GetSelectedIndex: returning %d\n",
            index));

    return index;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SelectItem(PRInt32 aPosition)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::SelectItem at %d\n",
                                       aPosition));
    //((QListBox *)mWidget)->setCurrentItem(aPosition);
    if (!mMultiSelect)
    {
        ((QListBox *)mWidget)->setCurrentItem(aPosition);
    }
    else
    {
        ((QListBox *)mWidget)->setSelected(aPosition, true);
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
    PRInt32 i = 0;
    PRInt32 selectedCount = 0;
    PRInt32 count = GetItemCount();

    for (i = 0; i < count; i++)
    {
        if (((QListBox *)mWidget)->isSelected(i))
        {
            selectedCount++;
        }
    }

    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsListBox::GetSelectedCount returning %d\n", selectedCount));

    return selectedCount;
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::GetSelectedIndices()\n"));
    PRInt32 i = 0;
    PRInt32 num = 0;
    PRInt32 count = GetItemCount();

    for (i = 0; i < count && num < aSize; i++)
    {
        if (((QListBox *)mWidget)->isSelected(i))
        {
            PR_LOG(QtWidgetsLM, 
                   PR_LOG_DEBUG, 
                   ("nsListBox::GetSelectedIndices: %d is selected\n",
                    i));
            aIndices[num] = i;
            num++;
        }
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
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::SetSelectedIndices()\n"));
    PRInt32 i = 0;

    Deselect();

    GetSelectedCount();

    for (i = 0; i < aSize; i++)
    {
        PR_LOG(QtWidgetsLM, 
               PR_LOG_DEBUG, 
               ("nsListBox::SetSelectedIndices: setting %d\n",
                aIndices[i]));
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
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::Deselect()\n"));
    ((QListBox *)mWidget)->clearSelection();

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set initial parameters
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::PreCreateWidget(nsWidgetInitData *aInitData)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::PreCreateWidget()\n"));
    if (nsnull != aInitData) 
    {
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
NS_METHOD nsListBox::CreateNative(QWidget *parentWindow)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsListBox::CreateNative()\n"));
    // to handle scrolling

    mWidget = new nsQListBox(this, parentWindow, QListBox::tr("nsListBox"));

    if (mWidget)
    {
        ((QListBox *)mWidget)->setAutoScrollBar(TRUE);
        SetMultipleSelection(PR_FALSE);
    }

    return NS_OK;
}
