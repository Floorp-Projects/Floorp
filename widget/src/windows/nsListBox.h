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

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"
#include "nsIListBox.h"

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
  virtual PRBool    OnPaint();
  virtual PRBool    OnResize(nsRect &aWindowRect);
  virtual void      GetBounds(nsRect &aRect);


  // nsIListBox interface
  void      SetMultipleSelection(PRBool aMultipleSelections);
  void      AddItemAt(nsString &aItem, PRInt32 aPosition);
  PRInt32   FindItem(nsString &aItem, PRInt32 aStartPos);
  PRInt32   GetItemCount();
  PRBool    RemoveItemAt(PRInt32 aPosition);
  PRBool    GetItemAt(nsString& anItem, PRInt32 aPosition);
  void      GetSelectedItem(nsString& aItem);
  PRInt32   GetSelectedIndex();
  void      AllowMultipleSelections(PRBool aMultiple);
  PRInt32   GetSelectedCount();
  void      GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);
  void      SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);
  void      SelectItem(PRInt32 aPosition);
  void      Deselect() ;
  void      PreCreateWidget(nsWidgetInitData *aInitData);

   // nsWindow interface
  virtual   PRBool AutoErase();
protected:
  PRBool  mMultiSelect;
  virtual LPCTSTR WindowClass();
  virtual DWORD   WindowStyle();
  virtual DWORD   WindowExStyle();

};

#endif // nsListBox_h__
