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

#include "nsMenuBase.h"
#include "nsMenuItem.h"
#include "nsMenu.h"
#include "nsToolkit.h"
#include "nsISupportsArray.h"

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

// Common code used by menu classes

nsMenuBase::nsMenuBase() : mToolkit(0), mWnd(0), mElements(nsnull)
{}

void nsMenuBase::Create( HWND hwndParent, nsToolkit *aToolkit)
{
   NS_ASSERTION( aToolkit, "Missing toolkit in menucreation");

   mToolkit = aToolkit;
   NS_ADDREF(mToolkit);

   // thread-switch if necessary
   if( !mToolkit->IsPMThread())
   {
      NS_NOTYETIMPLEMENTED( "Threaded menus");
   }
   else
   {
      mWnd = WinCreateWindow( hwndParent,
                              WC_MENU,
                              0,              // text
                              WindowStyle(),
                              0, 0, 0, 0,     // pos/size
                              hwndParent,     // owner
                              HWND_TOP,
                              0,              // window ID
                              0, 0);          // ctldata, presparams
      NS_ASSERTION( mWnd, "No menu window");

      // store nsMenuBase * in QWL_USER
      WinSetWindowPtr( mWnd, QWL_USER, this);

      // nice font (hmm)
      WinSetPresParam( mWnd, PP_FONTNAMESIZE,
                       strlen( gModuleData.pszFontNameSize) + 1,
                       gModuleData.pszFontNameSize);

      NS_NewISupportsArray( &mElements);
   }
}

ULONG nsMenuBase::WindowStyle()
{
   return 0;
}

void nsMenuBase::Destroy()
{
   if( mToolkit)
      if( !mToolkit->IsPMThread())
      {
         NS_NOTYETIMPLEMENTED( "Threaded menus");
      }
      else
      {
         // XXX Not sure about this -- need to think more.
         //     Shouldn't it be dealt with by the normal parent-death process?
         WinDestroyWindow( mWnd);
         mWnd = 0;
      }
}

// dumb helper
MRESULT nsMenuBase::SendMsg( ULONG m, MPARAM mp1, MPARAM mp2)
{
   MRESULT mrc = 0;
   if( mToolkit)
      mrc = mToolkit->SendMsg( mWnd, m, mp1, mp2);
   return mrc;
}

nsMenuBase::~nsMenuBase()
{
   if( mWnd) Destroy();
   NS_IF_RELEASE(mToolkit);
   NS_IF_RELEASE(mElements);
}

nsresult nsMenuBase::GetItemCount( PRUint32 &aCount)
{
   MRESULT rc = SendMsg( MM_QUERYITEMCOUNT);
   aCount = SHORT1FROMMP( rc);
   return NS_OK;
}

nsresult nsMenuBase::GetNativeData( void **aNative)
{
   *aNative = (void*) mWnd;
   return NS_OK;
}

// New all-singing, all-dancing insert routine.
//
// The 'aThing' may be an nsIMenu or an nsIMenuItem, joy.
// If it is null, then we're talking about a separator, though this ought
// not to be used.
nsresult nsMenuBase::InsertItemAt( nsISupports *aThing, PRUint32 aPos)
{
   nsIMenu     *pMenu = nsnull;
   nsIMenuItem *pItem = nsnull;
   MENUITEM     mI    = { aPos, 0, 0, 0, 0, (ULONG) aThing };
   char        *pStr  = nsnull;

   // XXX needs to use unicode converter to get right text
   //     This is very much an issue now that menus are constructed
   //     from XUL and so can have an arbitrary encoding.
   //
   //     Whether PM will know what to do with non-western characters
   //     is another issue!  Probably okay if it's in the process'
   //     codepage (font set via presparams, which take that cp)

   // set up menitem depending on what we have...
   if( nsnull == aThing)
   {
      mI.afStyle |= MIS_SEPARATOR;
   }
   else if( NS_SUCCEEDED( aThing->QueryInterface( nsIMenu::GetIID(),
                                                  (void**) &pMenu)))
   {
      void *hwnd = nsnull;

      nsString aString;
      pMenu->GetLabel( aString);
      pStr = gModuleData.ConvertFromUcs( aString);

      mI.afStyle |= MIS_SUBMENU | MIS_TEXT;

      pMenu->GetNativeData( &hwnd);
      mI.hwndSubMenu = (HWND) hwnd;

      NS_RELEASE(pMenu);
   }
   else if( NS_SUCCEEDED( aThing->QueryInterface( nsIMenuItem::GetIID(),
                                                  (void**) &pItem)))
   {
      nsMenuItem *pPMItem = (nsMenuItem*) pItem; // XXX
      nsString    aString;
      PRBool      bIsSep = PR_FALSE;

      mI.id = pPMItem->GetPMID();

      pPMItem->IsSeparator( bIsSep);
      if( bIsSep)
      {
         mI.afStyle |= MIS_SEPARATOR;
      }
      else
      {
         mI.afStyle |= MIS_TEXT;
         pPMItem->GetLabel( aString);
         pStr = gModuleData.ConvertFromUcs( aString);
      }
      NS_RELEASE(pItem);
   }
   else
   {
#ifdef DEBUG
      printf( "Erk, strange menu thing being added\n");
#endif
      return NS_ERROR_FAILURE;
   }

   // add menu item to gui
   SendMsg( MM_INSERTITEM, MPFROMP(&mI), MPFROMP(pStr));
   // ..and take ownership of it (separators don't have it)
   if( aThing)
      mElements->InsertElementAt( aThing, 0);

   return NS_OK;
}

// Pull items off of a menu
nsresult nsMenuBase::GetItemID( USHORT usID, PMENUITEM pItem)
{
   SendMsg( MM_QUERYITEM, MPFROM2SHORT(usID, FALSE), MPFROMP(pItem));
   return NS_OK;
}

nsresult nsMenuBase::GetItemAt( const PRUint32 &pos, PMENUITEM pItem)
{
   nsresult rc = NS_ERROR_ILLEGAL_VALUE;

   if( VerifyIndex( pos))
   {
      // find the ID
      MRESULT mrc = SendMsg( MM_ITEMIDFROMPOSITION, MPFROMLONG(pos));
      rc = GetItemID( SHORT1FROMMR(mrc), pItem);
      rc = NS_OK;
   }
   return rc;
}

nsresult nsMenuBase::GetItemAt( const PRUint32 &aPos, nsISupports *&aThing)
{
   MENUITEM mI = { 0 };
   nsresult rc = GetItemAt( aPos, &mI);

   if( NS_SUCCEEDED(rc))
   {
      NS_IF_RELEASE(aThing); // XP-COM correct, but probably bad, sigh.

      // This is either an nsIMenu or an nsIMenuItem
      aThing = (nsISupports*) mI.hItem;
      NS_ADDREF(aThing);
   }

   return rc;
}

// Update an item (grey, tick)
nsresult nsMenuBase::UpdateItem( PMENUITEM aItem)
{
   SendMsg( MM_SETITEM, 0, MPFROMP(aItem));
   return NS_OK;
}

nsresult nsMenuBase::RemoveItemAt( const PRUint32 index)
{
   MENUITEM mI = { 0 };
   nsresult rc = GetItemAt( index, &mI);

   if( NS_SUCCEEDED(rc))
   {
      // remove item from gui
      SendMsg( MM_REMOVEITEM, MPFROM2SHORT( mI.id, FALSE));

      // remove item from our list if we have it (& hence delete the window)
      nsISupports *pThing = (nsISupports*) mI.hItem;
      PRInt32      lIndex = 0;
      if( pThing && NS_SUCCEEDED( mElements->GetIndexOf( pThing, &lIndex)))
         rc = mElements->DeleteElementAt( lIndex);
   }

   return rc;
}

nsresult nsMenuBase::RemoveAll()
{
   PRUint32 count;
   GetItemCount( count);

   for( PRUint32 i = 0; i < count; i++)
      RemoveItemAt( 0);

   return NS_OK;
}

BOOL nsMenuBase::VerifyIndex( PRUint32 index)
{
   PRUint32 count;
   GetItemCount( count);
   return index == NS_MIT_END || index < count;
}

// Dummy nsIMenuListener implementation

nsEventStatus nsMenuBase::MenuItemSelected( const nsMenuEvent &aMenuEvent)
{
   return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBase::MenuSelected(const nsMenuEvent & aMenuEvent)
{
   return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBase::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
   return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBase::MenuDestruct( const nsMenuEvent &aMenuEvent)
{
   return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBase::MenuConstruct( const nsMenuEvent &aMenuEvent,
                                         nsIWidget         *aParentWindow, 
                                         void              *menubarNode,
                                         void              *aWebShell)
{
   return nsEventStatus_eIgnore;
}

// nsDynamicMenu, common base class for context & 'normal' menus
//
nsDynamicMenu::nsDynamicMenu() : mListener(0), mWebShell(0),
                                mDOMNode(0), mDOMElement(0)
{}

nsDynamicMenu::~nsDynamicMenu()
{
   NS_IF_RELEASE(mListener);
}

nsresult nsDynamicMenu::AddMenuListener( nsIMenuListener *aMenuListener)
{
   if( !aMenuListener)
      return NS_ERROR_NULL_POINTER;

   NS_IF_RELEASE(mListener);
   mListener = aMenuListener;
   NS_ADDREF(mListener);

   return NS_OK;
}

nsresult nsDynamicMenu::RemoveMenuListener( nsIMenuListener *aMenuListener)
{
   if( aMenuListener == mListener)
      NS_IF_RELEASE(mListener);
   return NS_OK;
}

nsresult nsDynamicMenu::SetDOMNode( nsIDOMNode *aMenuNode)
{
   mDOMNode = aMenuNode;
   return NS_OK;
}

nsresult nsDynamicMenu::SetDOMElement( nsIDOMElement *aMenuElement)
{
   mDOMElement = aMenuElement;
   return NS_OK;
}

nsresult nsDynamicMenu::SetWebShell( nsIWebShell *aWebShell)
{
   mWebShell = aWebShell;
   return NS_OK;
}

// build the menu from the DOM content model.
//
// Note that the tear-down from the previous display is done in the init for
// the next menu so that the MENUITEMs are valid for WM_COMMAND dispatching
//
nsEventStatus nsDynamicMenu::MenuSelected( const nsMenuEvent &aMenuEvent)
{
   RemoveAll();

   // Go through children of menu node and populate menu
   nsCOMPtr<nsIDOMNode> pItemNode;
   mDOMNode->GetFirstChild( getter_AddRefs(pItemNode));

   while( pItemNode)
   {
      nsCOMPtr<nsIDOMElement> pItemElement( do_QueryInterface(pItemNode));
      if( pItemElement)
      {
         nsString nodeType;
         pItemElement->GetNodeName( nodeType);
         if( nodeType.Equals( "menuitem"))
         {
            // find attributes of menu item & create gui peer
            nsString itemName;
            nsIMenuItem *pItem = new nsMenuItem;
            NS_ADDREF(pItem);
            pItemElement->GetAttribute( nsAutoString("value"), itemName);
            pItem->Create( (nsIMenu*)this, itemName, PR_FALSE);
            InsertItemAt( pItem);

            nsString itemCmd, disabled, checked;
            pItemElement->GetAttribute( nsAutoString("oncommand"), itemCmd);
            pItemElement->GetAttribute( nsAutoString("disabled"), disabled);
            pItemElement->GetAttribute( nsAutoString("checked"), checked);
            pItem->SetCommand( itemCmd);
            pItem->SetWebShell( mWebShell);
            pItem->SetDOMElement( pItemElement);
            pItem->SetEnabled( !disabled.Equals( nsAutoString("true")));
            pItem->SetChecked( checked.Equals( nsAutoString("true")));
            NS_RELEASE(pItem); // ownership of the item has passed to nsMenuBase
         }
         else if( nodeType.Equals( "menuseparator"))
            InsertItemAt( 0);
         else if( nodeType.Equals( "menu"))
         {
            // new submenu
            nsString menuName;
            nsIMenu *pMenu = new nsMenu;
            NS_ADDREF(pMenu);
            pItemElement->GetAttribute( nsAutoString("value"), menuName);
            pMenu->Create( (nsIMenu*)this, menuName);
            pMenu->SetDOMNode( pItemNode);
            pMenu->SetDOMElement( pItemElement);
            pMenu->SetWebShell( mWebShell);

            // insert into menubar
            InsertItemAt( pMenu);
            NS_RELEASE(pMenu); // owned in nsMenuBase
         }
      }
      nsCOMPtr<nsIDOMNode> pOldNode( pItemNode);
      pOldNode->GetNextSibling( getter_AddRefs(pItemNode));
   }

   return nsEventStatus_eIgnore;
}
