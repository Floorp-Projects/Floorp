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

#ifndef _nslistbox_h
#define _nslistbox_h

#include "nsWindow.h"
#include "nsBaseList.h"
#include "nsIListBox.h"

// WC_LISTBOX wrapper for NS_LISTBOX_CID

class nsListBox : public nsWindow, public nsIListBox,
                  public nsBaseList, public nsIListWidget
{
 public:
   nsListBox() : mStyle(0) {}

   // nsISupports
   NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
   NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
   NS_IMETHOD_(nsrefcnt) Release(void);          

   // baselist
   DECL_BASE_LIST_METHODS

   // nsIListbox
   NS_IMETHOD      SetMultipleSelection( PRBool aMultipleSelections);
   virtual PRInt32 GetSelectedCount();
   NS_IMETHOD      GetSelectedIndices( PRInt32 aIndices[], PRInt32 aSize);
   NS_IMETHOD      SetSelectedIndices( PRInt32 aIndices[], PRInt32 aSize);

   // platform hooks
   virtual PCSZ  WindowClass();
   virtual ULONG WindowStyle();
   NS_IMETHOD    PreCreateWidget( nsWidgetInitData *aInitData);

   // Message stopping
   virtual PRBool   OnMove( PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
   virtual PRBool   OnResize( PRInt32 aX, PRInt32 aY)  { return PR_FALSE; }

 protected:
   PRInt32 GetSelections( PRInt32 *aIndices, PRInt32 aSize);
   ULONG mStyle;
};

#endif
