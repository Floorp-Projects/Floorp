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
 */

// popup context menu

#include "nsContextMenu.h"
#include "nsWindow.h"
#include "nsCOMPtr.h"

// Can't WinQueryWindowRect() on menu before popping up...
static void CalcMenuSize( HWND hwnd, PSIZEL szl);

NS_IMPL_ADDREF(nsContextMenu)
NS_IMPL_RELEASE(nsContextMenu)

nsresult nsContextMenu::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
   if( !aInstancePtr)
      return NS_ERROR_NULL_POINTER;
 
   *aInstancePtr = 0;
 
   if( aIID.Equals(NS_GET_IID(nsIContextMenu)))
   {
      *aInstancePtr = (void*) ((nsIContextMenu*) this);
      NS_ADDREF_THIS();
      return NS_OK;
   }
   if( aIID.Equals(NS_GET_IID(nsIMenuListener)))
   {
      *aInstancePtr = (void*) ((nsIMenuListener*)this);
      NS_ADDREF_THIS();
      return NS_OK;
   }
   if( aIID.Equals(((nsIContextMenu*)this)->GetIID()))
   {
      *aInstancePtr = (void*) ((nsIContextMenu*)this);
      NS_ADDREF_THIS();
      return NS_OK;
   }
 
   return NS_NOINTERFACE;
}

nsContextMenu::nsContextMenu() : mOwner(nsnull), mX(0), mY(0), mShowing(PR_FALSE)
{
   NS_INIT_REFCNT();
}

nsContextMenu::~nsContextMenu()
{
}

NS_METHOD nsContextMenu::Create(nsISupports *aParent, const nsString& anAlignment,
                                const nsString& anAnchorAlignment)
{
  if(aParent)
  {
      nsIWidget * parent = nsnull;
      aParent->QueryInterface(nsCOMTypeInfo<nsIWidget>::GetIID(), (void**) &parent); // This does the addref
      if(parent)
	  {
        mParentWindow = parent;
	  }
  }

  mAlignment = anAlignment;
  mAnchorAlignment = anAnchorAlignment;
  //  mMenu = CreatePopupMenu();
  return NS_OK;
}

nsresult nsContextMenu::GetParent( nsISupports *&aParent)
{
  aParent = nsnull;
  if (nsnull != mParentWindow) {
    return mParentWindow->QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(),(void**)&aParent);
  } 

  return NS_ERROR_FAILURE;
}

nsresult nsContextMenu::SetLocation( PRInt32 aX, PRInt32 aY)
{
   mX = aX;
   mY = aY;
   return NS_OK;
}

// nsIMenuListener specialization for 

// Called to display the menu: update the DOM tree & then pop up the menu
nsEventStatus nsContextMenu::MenuSelected( const nsMenuEvent &aMenuEvent)
{
   // Bail if we're already up.  This happens for complicated reasons.
   if( mShowing) return nsEventStatus_eIgnore;

   // Call superclass method to build the menu
   nsDynamicMenu::MenuSelected( aMenuEvent);

   // The coords we have are relative to the desktop.  Convert to PM.
   POINTL ptl = { mX, mY };
   ptl.y = gModuleData.szScreen.cy - ptl.y - 1;

   // Now look at the "popupalign" attribute to see what corner of the popup
   // should go at this location.
   // (see http://www.mozilla.org/xpfe/xptoolkit/popups.html)
   SIZEL szMenu;
   CalcMenuSize( mWnd, &szMenu);

   if( mAlignment.EqualsWithConversion("topleft")) // most common case
      ptl.y -= szMenu.cy;
   else if( mAlignment.EqualsWithConversion("bottomright"))
      ptl.x -= szMenu.cx;
   else if( mAlignment.EqualsWithConversion("topright"))
   {
      ptl.x -= szMenu.cx;
      ptl.y -= szMenu.cy;
   }

   mShowing = TRUE;

   // Tell owner we're up so it can dispatch our commands.
   mOwner->SetContextMenu( this);

   WinPopupMenu( HWND_DESKTOP,
                (HWND) mOwner->GetNativeData( NS_NATIVE_WIDGET),
                 mWnd,
                 ptl.x, ptl.y,
                 0,
                 PU_HCONSTRAIN | PU_VCONSTRAIN | PU_NONE |
                 PU_KEYBOARD | PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2);

   // Because XPFE is Really Great, this is meant to be synchronous.  Oh yes.
   //
   // XXX this loop is WRONG if there is a ``modal dialog'' showing.

#if 0
   nsIAppShell *pAppShell;
   NS_CreateAppShell( &pAppShell);
#endif

   //     Guess we ought to get appshell in on the act -- we can get
   //     the current appshell using NS_GetAppShell() or something, and it
   //     knows if there's a modal loop 'cos it's smart :-).
   //     Add a method to the appshell to 'process contextmenu' or something.
   //
   QMSG qmsg;
   BOOL rc;

   while( mShowing)
   {
      rc = WinGetMsg( 0, &qmsg, 0, 0, 0);
      if( qmsg.msg == WM_USER) break; // menu is done, leave loop.
      WinDispatchMsg( 0, &qmsg);
   }

   mOwner->SetContextMenu(0);

   mShowing = PR_FALSE;

   return nsEventStatus_eIgnore;
}

nsEventStatus nsContextMenu::MenuDeselected( const nsMenuEvent &aMenuEvent)
{
   // terminate modal loop from popup method
   if( mShowing)
   {
      WinPostQueueMsg( HMQ_CURRENT, WM_USER, 0, 0);
   }
   return nsEventStatus_eIgnore;
}

// Can't WinQueryWindowRect() on menu before popping up...
void CalcMenuSize( HWND hwnd, PSIZEL szl)
{

#ifndef XP_OS2_VACPP     // XXXXX "SHORT too small for MRESULT"

   SHORT sItems = (SHORT) WinSendMsg( hwnd, MM_QUERYITEMCOUNT, 0, 0);

   memset( szl, 0, sizeof(SIZEL));

   for( SHORT i = 0; i < sItems; i++)
   {
      SHORT sID = (SHORT) WinSendMsg( hwnd, MM_ITEMIDFROMPOSITION,
                                      MPFROMSHORT(i), 0);
      RECTL rclItem;

      WinSendMsg( hwnd, MM_QUERYITEMRECT, MPFROM2SHORT(sID,0), MPFROMP(&rclItem));
      LONG lWidth = rclItem.xRight - rclItem.xLeft;
      if( lWidth > szl->cx) szl->cx = lWidth;
      szl->cy += (rclItem.yTop - rclItem.yBottom);
   }

   // Fudge-factor
   szl->cy += WinQuerySysValue( HWND_DESKTOP, SV_CYSIZEBORDER);
#endif
}
