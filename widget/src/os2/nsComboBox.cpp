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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 */

#include "nscombobox.h"

// WC_COMBOBOX wrapper.  Hackery present in nsWindow to enable user to just
// talk about the height of the entryfield portion.  This may well not work
// because the combobox will size it on its own.  Oh well, can be worked on.

NS_IMPL_ADDREF(nsComboBox)
NS_IMPL_RELEASE(nsComboBox)

nsresult nsComboBox::QueryInterface( const nsIID &aIID, void **aInstancePtr)
{
   if( aIID.Equals( NS_GET_IID(nsIComboBox)))
   {
      *aInstancePtr = (void*) ((nsIComboBox*)this);
      NS_ADDREF_THIS();
      return NS_OK;
   }
   else if( aIID.Equals( NS_GET_IID(nsIListWidget)))
   {
      *aInstancePtr = (void*) ((nsIListWidget*)this);
      NS_ADDREF_THIS();
      return NS_OK;
   }

   return nsWindow::QueryInterface( aIID,aInstancePtr);
}

nsComboBox::nsComboBox() : mDropdown(60), mEntry(gModuleData.lHtEntryfield)
{}

nsresult nsComboBox::PreCreateWidget( nsWidgetInitData *aInitData)
{
   if( aInitData != nsnull)
   {
      // remember the dropdown height
      nsComboBoxInitData *comboData = (nsComboBoxInitData*) aInitData;
      mDropdown = comboData->mDropDownHeight;
   }
   return NS_OK;
}

PRBool nsComboBox::OnPresParamChanged( MPARAM mp1, MPARAM mp2)
{
   if( LONGFROMMP(mp1) == PP_FONTNAMESIZE)
   {
      // reget height of entryfield
      SWP swp;
      WinQueryWindowPos( WinWindowFromID( mWnd, CBID_EDIT), &swp);
      mEntry = swp.cy + 6; // another magic number...
   }

   return PR_FALSE;
}

PRInt32 nsComboBox::GetHeight( PRInt32 aProposedHeight)
{
// See layout/html/forms/src/nsSelectControlFrame.cpp
// return aProposedHeight + mDropdown;
   return mDropdown + mEntry;
}

nsresult nsComboBox::GetBounds( nsRect &aRect)
{
   aRect.x = mBounds.x;
   aRect.y = mBounds.y;
   aRect.width = mBounds.width;
   aRect.height = mEntry;

   return NS_OK;
}

nsresult nsComboBox::GetClientBounds( nsRect &aRect)
{
   aRect.x = 0;
   aRect.y = 0;
   aRect.width = mBounds.width;
   aRect.height = mEntry;

   return NS_OK;
}

PCSZ nsComboBox::WindowClass()
{
   return WC_COMBOBOX;
}

ULONG nsComboBox::WindowStyle()
{
   return CBS_DROPDOWNLIST | BASE_CONTROL_STYLE;
}
