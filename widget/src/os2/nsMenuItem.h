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

// nsMenuItem - interface to things which live in menus.
//
// This is rather complicated, not least because it contains about 2 1/2
// versions of the API rolled up into 1.

#ifndef _nsmenuitem_h
#define _nsmenuitem_h

#include "nsString.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"

class nsMenuBase;
class nsIDOMElement;
class nsIWebShell;

class nsMenuItem : public nsIMenuItem, public nsIMenuListener
{
 public:
   nsMenuItem();
   virtual ~nsMenuItem();

   // nsISupports
   NS_DECL_ISUPPORTS

   // nsIMenuItem
   NS_IMETHOD Create( nsISupports *aParent, const nsString &aLabel, PRBool isSep);
   NS_IMETHOD GetLabel( nsString &aText);
   NS_IMETHOD SetLabel( nsString &aText);
   NS_IMETHOD SetEnabled( PRBool aIsEnabled);
   NS_IMETHOD GetEnabled( PRBool *aIsEnabled);
   NS_IMETHOD SetChecked( PRBool aIsEnabled);
   NS_IMETHOD GetChecked( PRBool *aIsEnabled);
   NS_IMETHOD SetCheckboxType(PRBool aIsCheckbox);
   NS_IMETHOD GetCheckboxType(PRBool *aIsCheckbox);
   NS_IMETHOD GetCommand( PRUint32 &aCommand);
   NS_IMETHOD GetTarget( nsIWidget *&aTarget);
   NS_IMETHOD GetNativeData( void *&aData);
   NS_IMETHOD AddMenuListener( nsIMenuListener *aMenuListener);
   NS_IMETHOD RemoveMenuListener( nsIMenuListener *aMenuListener);
   NS_IMETHOD IsSeparator( PRBool &aIsSep);
   NS_IMETHOD SetCommand( const nsString &aStrCmd);
   NS_IMETHOD DoCommand();
   NS_IMETHOD SetDOMNode(nsIDOMNode * aDOMNode);
   NS_IMETHOD GetDOMNode(nsIDOMNode ** aDOMNode);
   NS_IMETHOD SetDOMElement( nsIDOMElement *aDOMElement);
   NS_IMETHOD GetDOMElement( nsIDOMElement **aDOMElement);
   NS_IMETHOD SetWebShell( nsIWebShell *aWebShell);
   NS_IMETHOD SetShortcutChar(const nsString &aText);
   NS_IMETHOD GetShortcutChar(nsString &aText);
   NS_IMETHOD SetModifiers(PRUint8 aModifiers);
   NS_IMETHOD GetModifiers(PRUint8 * aModifiers);
   NS_IMETHOD SetMenuItemType(EMenuItemType aIsCheckbox);
   NS_IMETHOD GetMenuItemType(EMenuItemType *aIsCheckbox);

   // nsIMenuListener interface
   nsEventStatus MenuSelected( const nsMenuEvent &aMenuEvent);
   nsEventStatus MenuItemSelected( const nsMenuEvent &aMenuEvent);
   nsEventStatus MenuDeselected( const nsMenuEvent &aMenuEvent);
   nsEventStatus MenuConstruct( const nsMenuEvent &aMenuEvent,
                                nsIWidget         *aParentWindow, 
                                void              *menubarNode,
                                void              *aWebShell);
   nsEventStatus MenuDestruct( const nsMenuEvent &aMenuEvent);

   // nsMenuItem
   USHORT GetPMID() { return mPMID; }

 protected:
   nsMenuBase      *mMenuBase;     // Menu we are attached to
   nsString         mLabel;
   PRBool           mIsSeparator;
   nsString         mKeyEquivalent;
   PRUint8          mModifiers;
   nsIWidget       *mTarget;       // window we dispatch to
   USHORT           mPMID;         // PM command ID
   nsIMenuListener *mMenuListener;
   nsString         mCmdString;    // JS command
   nsIDOMElement   *mDOMElement;   // dom element for item
   nsIWebShell     *mWebShell;
   EMenuItemType    mMenuType;
};

#endif
