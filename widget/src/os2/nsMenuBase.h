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

#ifndef _nsmenubase_h
#define _nsmenubase_h

#include "nsWidgetDefs.h"
#include "nsIMenuListener.h"

class nsString;
class nsIWebShell;
class nsIDOMElement;
class nsIDOMNode;
class nsISupportsArray;

// Base class for the various menu widgets to share code.

class nsToolkit;

class nsMenuBase : public nsIMenuListener
{
 public:
   nsMenuBase();
   virtual ~nsMenuBase();

   // common menu methods
   NS_IMETHOD GetItemCount( PRUint32 &aCount);
   NS_IMETHOD InsertItemAt( nsISupports *aThing, PRUint32 aPos = NS_MIT_END);
   NS_IMETHOD GetItemID( USHORT usID, PMENUITEM pItem);
   NS_IMETHOD GetItemAt( const PRUint32 &pos, PMENUITEM pItem);
   NS_IMETHOD GetItemAt( const PRUint32 &pos, nsISupports *&aThing);
   NS_IMETHOD UpdateItem( PMENUITEM pItem);
   NS_IMETHOD RemoveItemAt( const PRUint32 index);
   NS_IMETHOD RemoveAll();
   NS_IMETHOD GetNativeData( void **aNative);

   // dummy nsIMenuListener implementation
   virtual nsEventStatus MenuSelected( const nsMenuEvent &aMenuEvent);
   virtual nsEventStatus MenuItemSelected( const nsMenuEvent &aMenuEvent);
   virtual nsEventStatus MenuDeselected( const nsMenuEvent &aMenuEvent);
   virtual nsEventStatus MenuConstruct( const nsMenuEvent &aMenuEvent,
                                        nsIWidget         *aParentWindow, 
                                        void              *menubarNode,
                                        void              *aWebShell);
   virtual nsEventStatus MenuDestruct( const nsMenuEvent &aMenuEvent);

   // helpers
   MRESULT SendMsg( ULONG m, MPARAM mp1=0, MPARAM mp2=0);
   nsToolkit *GetTK() { return mToolkit; }

 protected:
   virtual BOOL  VerifyIndex( PRUint32 index);

   void          Create( HWND hwndParent, nsToolkit *aToolkit);
   void          Destroy();

   // Subclasses should supply extra style bits here if required.
   virtual ULONG WindowStyle();

   nsToolkit        *mToolkit;
   HWND              mWnd;
   nsISupportsArray *mElements; // we now own the menu elements
};

#define DECL_MENU_BASE_METHODS                                               \
   NS_IMETHOD AddItem( nsISupports *aThing)                                  \
     { return nsMenuBase::InsertItemAt( aThing); }                           \
   NS_IMETHOD AddSeparator()                                                 \
     { return nsMenuBase::InsertItemAt( nsnull); }                           \
   NS_IMETHOD GetItemCount( PRUint32 &aCount)                                \
     { return nsMenuBase::GetItemCount( aCount); }                           \
   NS_IMETHOD GetItemAt( const PRUint32 aCount, nsISupports *&aThing)        \
     { return nsMenuBase::GetItemAt( aCount, aThing); }                      \
   NS_IMETHOD InsertItemAt( const PRUint32 aCount, nsISupports *aThing)      \
     { return nsMenuBase::InsertItemAt( aThing, aCount); }                   \
   NS_IMETHOD InsertSeparator( const PRUint32 aCount)                        \
     { return nsMenuBase::InsertItemAt( nsnull, aCount); }                   \
   NS_IMETHOD RemoveItem( const PRUint32 aCount)                             \
     { return nsMenuBase::RemoveItemAt( aCount); }                           \
   NS_IMETHOD RemoveAll()                                                    \
     { return nsMenuBase::RemoveAll(); }                                     \
   NS_IMETHOD GetNativeData( void **aNative)                                 \
     { return nsMenuBase::GetNativeData( aNative); }

// Base class for nsMenu & nsContextMenu, menus whose contents are build afresh
// from the DOM content model on each activation.
class nsDynamicMenu : public nsMenuBase
{
 public:
   nsDynamicMenu();
   virtual ~nsDynamicMenu();

   // Common methods
   NS_IMETHOD AddMenuListener( nsIMenuListener *aMenuListener);
   NS_IMETHOD RemoveMenuListener( nsIMenuListener *aMenuListener);
   NS_IMETHOD GetDOMNode( nsIDOMNode ** aMenuNode);
   NS_IMETHOD SetDOMNode( nsIDOMNode *aMenuNode);
   NS_IMETHOD SetDOMElement( nsIDOMElement *aMenuElement);
   NS_IMETHOD SetWebShell( nsIWebShell *aWebShell);

   // nsIMenuListener override to build/tear down the menu
   virtual nsEventStatus MenuSelected( const nsMenuEvent &aMenuEvent);

 protected:
   nsIMenuListener *mListener;
   nsIWebShell     *mWebShell;
   nsIDOMNode      *mDOMNode;
   nsIDOMElement   *mDOMElement;
};

#define DECL_DYNAMIC_MENU_METHODS                                            \
   DECL_MENU_BASE_METHODS                                                    \
   NS_IMETHOD AddMenuListener( nsIMenuListener *aMenuListener)               \
     { return nsDynamicMenu::AddMenuListener( aMenuListener); }              \
   NS_IMETHOD RemoveMenuListener( nsIMenuListener *aMenuListener)            \
     { return nsDynamicMenu::RemoveMenuListener( aMenuListener); }           \
   NS_IMETHOD GetDOMNode( nsIDOMNode **aMenuNode)                            \
     { return nsDynamicMenu::GetDOMNode( aMenuNode); }                       \
   NS_IMETHOD SetDOMNode( nsIDOMNode *aMenuNode)                             \
     { return nsDynamicMenu::SetDOMNode( aMenuNode); }                       \
   NS_IMETHOD SetDOMElement( nsIDOMElement *aMenuElement)                    \
     { return nsDynamicMenu::SetDOMElement( aMenuElement); }                 \
   NS_IMETHOD SetWebShell( nsIWebShell *aWebShell)                           \
     { return nsDynamicMenu::SetWebShell( aWebShell); }

#endif
