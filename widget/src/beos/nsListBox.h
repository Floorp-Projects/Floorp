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

#ifndef nsListBox_h__
#define nsListBox_h__

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"
#include "nsIListBox.h"

#include <ListView.h>
#include <ListItem.h>

/**
 * Native Win32 Listbox wrapper
 */

class nsListBox :   public nsWindow,
                    public nsIListWidget,
                    public nsIListBox
{

public:
  nsListBox();
  virtual ~nsListBox();

    // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
  NS_IMETHOD_(nsrefcnt) Release(void);          

  virtual PRBool    OnMove(PRInt32 aX, PRInt32 aY);
  virtual PRBool    OnPaint(nsRect &r);
  virtual PRBool    OnResize(nsRect &aWindowRect);

  NS_IMETHOD        GetBounds(nsRect &aRect);


  // nsIListBox interface
  NS_IMETHOD SetMultipleSelection(PRBool aMultipleSelections);
  NS_IMETHOD AddItemAt(nsString &aItem, PRInt32 aPosition);
  PRInt32    FindItem(nsString &aItem, PRInt32 aStartPos);
  PRInt32    GetItemCount();
  PRBool     RemoveItemAt(PRInt32 aPosition);
  PRBool     GetItemAt(nsString& anItem, PRInt32 aPosition);
  NS_IMETHOD GetSelectedItem(nsString& aItem);
  PRInt32    GetSelectedIndex();
  PRInt32    GetSelectedCount();
  NS_IMETHOD GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);
  NS_IMETHOD SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);
  NS_IMETHOD SelectItem(PRInt32 aPosition);
  NS_IMETHOD Deselect() ;
  NS_IMETHOD PreCreateWidget(nsWidgetInitData *aInitData);

   // nsWindow interface
  virtual   PRBool AutoErase();
protected:
	PRBool  mMultiSelect;
	virtual BView *CreateBeOSView();
	BListView	*mListView;

	friend class nsListViewBeOS;
};

//
// A BListView subclass
//
class nsListViewBeOS : public BListView, public nsIWidgetStore {
  public:
	nsListViewBeOS( nsIWidget *aWidgetWindow, BRect aFrame, 
	    const char *name, uint32 aResizingMode = B_FOLLOW_LEFT | B_FOLLOW_TOP, 
        uint32 aFlags = B_WILL_DRAW | B_NAVIGABLE );
};

#endif // nsListBox_h__
