/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _nscombobox_h
#define _nscombobox_h

#include "nsWindow.h"
#include "nsBaseList.h"
#include "nsIComboBox.h"

// WC_COMBOBOX wrapper for NS_COMBOBOX_CID

class nsComboBox : public nsWindow, public nsIComboBox,
                   public nsBaseList, public nsIListWidget
{
 public:
   nsComboBox();

   // nsISupports
   NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
   NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
   NS_IMETHOD_(nsrefcnt) Release(void);          

   // baselist
   DECL_BASE_LIST_METHODS

   // platform hooks
   virtual PCSZ     WindowClass();
   virtual ULONG    WindowStyle();
   virtual PRInt32  GetHeight( PRInt32 aHeight);
   NS_IMETHOD       PreCreateWidget( nsWidgetInitData *aInitData);

   // Message stopping
   virtual PRBool   OnMove( PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
   virtual PRBool   OnResize( PRInt32 aX, PRInt32 aY)  { return PR_FALSE; }

   // Hacks to pretend we're the size of our entryfield part
   NS_IMETHOD GetBounds( nsRect &aRect);
   NS_IMETHOD GetClientBounds( nsRect &aRect);
   virtual PRBool OnPresParamChanged( MPARAM mp1, MPARAM mp2);

 protected:
   PRInt32 mDropdown;
   PRInt32 mEntry;
};

#endif
