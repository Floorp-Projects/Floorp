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

// menu bar

#include "nsWindow.h"
#include "nsMenuBar.h"
#include "nsMenu.h"

#include "nsIWebShell.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

// XPCom
NS_IMPL_ADDREF(nsMenuBar)
NS_IMPL_RELEASE(nsMenuBar)

nsresult nsMenuBar::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
   if( !aInstancePtr)
     return NS_ERROR_NULL_POINTER;
 
   *aInstancePtr = 0;
 
   if( aIID.Equals(nsIMenuBar::GetIID()))
   {
      *aInstancePtr = (void*) ((nsIMenuBar*) this);
      NS_ADDREF_THIS();
      return NS_OK;
   }
   if( aIID.Equals(nsIMenuListener::GetIID()))
   {
      *aInstancePtr = (void*) ((nsIMenuListener*)this);
      NS_ADDREF_THIS();
      return NS_OK;
   }
   if( aIID.Equals(((nsISupports*)(nsIMenuBar*)this)->GetIID()))
   {
      *aInstancePtr = (void*) ((nsISupports*)((nsIMenuBar*)this));
      NS_ADDREF_THIS();
      return NS_OK;
   }
 
   return NS_NOINTERFACE;
}

nsMenuBar::nsMenuBar() : mWndFrame(nsnull), mParent(nsnull)
{
   NS_INIT_REFCNT();
}

nsMenuBar::~nsMenuBar()
{
}

// Creation
nsresult nsMenuBar::Create( nsIWidget *aParent)
{
   NS_ASSERTION(aParent,"Menu must have a parent");

   nsToolkit *aToolkit = (nsToolkit*) aParent->GetToolkit();

   mWndFrame = (HWND) aParent->GetNativeData( NS_NATIVE_WINDOW);
   //
   // Who knows what kind of window the client's given to us here.
   // Search up the window hierarchy until we find a frame.
   //
   while( !((ULONG)aToolkit->SendMsg( mWndFrame, WM_QUERYFRAMEINFO) & FI_FRAME))
      mWndFrame = WinQueryWindow( mWndFrame, QW_PARENT);

   nsMenuBase::Create( mWndFrame, aToolkit);
   NS_RELEASE(aToolkit);                // add-ref'd by above call

   // must have id FID_MENU
   WinSetWindowUShort( mWnd, QWS_ID, FID_MENU);
   // tell the frame about us
   UpdateFrame();

   // remember parent
   mParent = aParent;

   return NS_OK;
}

ULONG nsMenuBar::WindowStyle()
{
   return MS_ACTIONBAR;
}

nsresult nsMenuBar::GetParent( nsIWidget *&aParent)
{
   NS_IF_RELEASE(aParent);
   aParent = mParent;
   NS_ADDREF(aParent);
   return NS_OK;
}

nsresult nsMenuBar::SetParent( nsIWidget *aParent)
{
   // XXX oh dear.  I don't really want to implement this.
   //     I guess we could work out what new frame window this
   //     is & then reparent ourselves, but ewwww.
   printf( "nsMenuBar::SetParent() - tell someone\n");

   return NS_OK;
}

void nsMenuBar::UpdateFrame()
{
   if( mToolkit)
      mToolkit->SendMsg( mWndFrame, WM_UPDATEFRAME, MPFROMLONG(FCF_MENU));
}

// Need to override these so we can tell the frame about the new entries
nsresult nsMenuBar::AddMenu( nsIMenu *aMenu)
{
   nsresult rc = nsMenuBase::InsertItemAt( aMenu);
   UpdateFrame();
   return rc;
}

nsresult nsMenuBar::InsertMenuAt( const PRUint32 aCount, nsIMenu *&aMenu)
{
   nsresult rc = nsMenuBase::InsertItemAt( aMenu, aCount);
   UpdateFrame();
   return rc;
}

nsresult nsMenuBar::RemoveMenu( const PRUint32 aCount)
{ 
   nsresult rc = nsMenuBase::RemoveItemAt( aCount);
   UpdateFrame();
   return rc;
}

nsresult nsMenuBar::RemoveAll()
{
   nsresult rc = nsMenuBase::RemoveAll();
   UpdateFrame();
   return rc;
}

// accessor
nsresult nsMenuBar::GetMenuAt( const PRUint32 aCount, nsIMenu *&aMenu)
{
   nsresult rc = NS_ERROR_FAILURE;

   NS_IF_RELEASE(aMenu);

   if( VerifyIndex( aCount))
   {
      MENUITEM mI;
      nsMenuBase::GetItemAt( aCount, &mI);

      nsISupports *aThing = (nsISupports*) mI.hItem;
      rc = aThing->QueryInterface( nsIMenu::GetIID(), (void**) &aMenu);
   }

   return rc;
}

// hmm
nsresult nsMenuBar::Paint()
{
   UpdateFrame();
   mParent->Invalidate( PR_TRUE);
   return NS_OK;
}

// nsIMenuListener interface - used to update menu dynamically and build it
// from a dom content model

// nsWebShellWindow currently fakes a call into here to kick us off
nsEventStatus nsMenuBar::MenuConstruct( const nsMenuEvent &aMenuEvent,
                                        nsIWidget         *aParentWindow, 
                                        void              *aMenubarNode,
                                        void              *aWebShell)
{
   nsIWebShell *pWebShell = (nsIWebShell*) aWebShell;
   nsIDOMNode  *pMenubarNode = (nsIDOMNode*) aMenubarNode;

   // Create the menubar, register for notifications with the window
   Create( aParentWindow);
   aParentWindow->AddMenuListener( this);
   aParentWindow->SetMenuBar( this);

   // Now iterate through the DOM children creating submenus.
   nsCOMPtr<nsIDOMNode> pMenuNode;
   pMenubarNode->GetFirstChild( getter_AddRefs(pMenuNode));
   while( pMenuNode)
   {
      nsCOMPtr<nsIDOMElement> pMenuElement( do_QueryInterface(pMenuNode));
      if( pMenuElement)
      {
         nsString nodeType, menuName;
         pMenuElement->GetNodeName( nodeType);
         if( nodeType.Equals( "menu"))
         {
            // new submenu
            pMenuElement->GetAttribute( nsAutoString( "value"), menuName);
            nsIMenu *pMenu = new nsMenu;
            NS_ADDREF(pMenu);
            pMenu->Create( (nsIMenuBar*)this, menuName);
            pMenu->SetDOMNode( pMenuNode);
            pMenu->SetDOMElement( pMenuElement);
            pMenu->SetWebShell( pWebShell);

            // insert into menubar; nsMenuBase takes ownership 
            AddMenu( pMenu);
            NS_RELEASE(pMenu);
         }
      }
      nsCOMPtr<nsIDOMNode> pOldNode( pMenuNode);  
      pOldNode->GetNextSibling( getter_AddRefs(pMenuNode));
   }

   // Hackish -- nsWebShellWindow doesn't cut its ref, so leave the frame
   // window with ownership.
   Release(); // can't call NS_RELEASE 'cos |this| is not an lvalue...

   return nsEventStatus_eIgnore;
}
