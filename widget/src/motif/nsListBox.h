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

#ifndef nsListBox_h__
#define nsListBox_h__

#include "nsWindow.h"
#include "nsIListBox.h"

/**
 * Native Motif Listbox wrapper
 */

class nsListBox :   public nsWindow
{

public:
    nsListBox(nsISupports *aOuter);
    virtual ~nsListBox();

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


    // nsIListBox interface
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
    PRBool  mMultiSelect;

private:

  // this should not be public
  static PRInt32 GetOuterOffset() {
    return offsetof(nsListBox,mAggWidget);
  }


  // Aggregator class and instance variable used to aggregate in the
  // nsIListBox interface to nsListBox w/o using multiple
  // inheritance.
  class AggListBox : public nsIListWidget {
  public:
    AggListBox();
    virtual ~AggListBox();

    AGGREGATE_METHOD_DEF

    // nsIListBox
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
  AggListBox mAggWidget;
  friend class AggListBox;

};

#endif // nsListBox_h__
