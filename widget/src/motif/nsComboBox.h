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

#ifndef nsComboBox_h__
#define nsComboBox_h__

#include "nsWindow.h"
#include "nsIComboBox.h"

/**
 * Native Motif Listbox wrapper
 */

class nsComboBox :   public nsWindow
{

public:
    nsComboBox(nsISupports *aOuter);
    virtual ~nsComboBox();

    // nsISupports. Forward to the nsObject base class
    NS_IMETHOD QueryObject(const nsIID& aIID, void** aInstancePtr);

    void Create(nsIWidget *aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);

    void Create(nsNativeWidget aParent,
              const nsRect &aRect, 
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);


    virtual PRBool    OnMove(PRInt32 aX, PRInt32 aY);
    virtual PRBool OnPaint(nsPaintEvent & aEvent);
    virtual PRBool OnResize(nsSizeEvent &aEvent);

    // nsIComboBox interface
    void      SetMultipleSelection(PRBool aMultipleSelections);
    void      AddItemAt(nsString &aItem, PRInt32 aPosition);
    PRInt32   FindItem(nsString &aItem, PRInt32 aStartPos);
    PRInt32   GetItemCount();
    PRBool    RemoveItemAt(PRInt32 aPosition);
    PRBool    GetItemAt(nsString& anItem, PRInt32 aPosition);
    void      GetSelectedItem(nsString& aItem);
    PRInt32   GetSelectedIndex();
    PRInt32   GetSelectedCount();
    void      GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);
    void      SelectItem(PRInt32 aPosition);
    void      Deselect() ;

protected:
    Widget  mPullDownMenu;
    Widget  mOptionMenu;
    PRBool  mMultiSelect;

    long    * mItems; // an array of Widgets
    int       mMaxNumItems;
    int       mNumItems;

private:

  // this should not be public
  static PRInt32 GetOuterOffset() {
    return offsetof(nsComboBox,mAggWidget);
  }


  // Aggregator class and instance variable used to aggregate in the
  // nsIComboBox interface to nsComboBox w/o using multiple
  // inheritance.
  class AggComboBox : public nsIComboBox {
  public:
    AggComboBox();
    virtual ~AggComboBox();

    AGGREGATE_METHOD_DEF

    // nsIComboBox
    void      SetMultipleSelection(PRBool aMultipleSelections);
    void      AddItemAt(nsString &aItem, PRInt32 aPosition);
    PRInt32   FindItem(nsString &aItem, PRInt32 aStartPos);
    PRInt32   GetItemCount();
    PRBool    RemoveItemAt(PRInt32 aPosition);
    PRBool    GetItemAt(nsString& anItem, PRInt32 aPosition);
    void      GetSelectedItem(nsString& aItem);
    PRInt32   GetSelectedIndex();
    PRInt32   GetSelectedCount();
    void      GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);
    void      SelectItem(PRInt32 aPosition);
    void      Deselect() ;

  };
  AggComboBox mAggWidget;
  friend class AggComboBox;

};

#endif // nsComboBox_h__
