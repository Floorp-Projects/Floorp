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

#ifndef nsContextMenu_h__
#define nsContextMenu_h__

#include "nsIContextMenu.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"

#include "nsIDOMElement.h"
#include "nsIWebShell.h"

class nsIMenuListener;

/**
 * Native Win32 button wrapper
 */

class nsContextMenu : public nsIContextMenu, public nsIMenuListener
{

public:
  nsContextMenu();
  virtual ~nsContextMenu();

  NS_DECL_ISUPPORTS
  
  // nsIMenu Methods
  NS_IMETHOD Create(nsISupports * aParent);
  NS_IMETHOD GetParent(nsISupports *&aParent);

  nsEventStatus MenuConstruct(const nsMenuEvent &aMenuEvent,
                              nsIWidget         *aParentWindow, 
                              void              *menubarNode,
                              void              *aWebShell);
  nsEventStatus MenuDestruct(const nsMenuEvent &aMenuEvent);

  NS_IMETHOD GetNativeData(void** aData);

  NS_IMETHOD SetDOMNode(nsIDOMNode * menuNode);
  NS_IMETHOD SetDOMElement(nsIDOMElement * menuElement);
  NS_IMETHOD SetWebShell(nsIWebShell * aWebShell);
  NS_IMETHOD Show(PRBool aShow);
  NS_IMETHOD SetLocation(PRInt32 aX, PRInt32 aY,
                         const nsString& anAlignment,
                         const nsString& aAnchorAlign);

  gint GetX(void);
  gint GetY(void);


protected:
  void LoadMenuItem(nsIMenu             *pParentMenu,
                    nsIDOMElement       *menuitemElement,
                    nsIDOMNode          *menuitemNode,
                    unsigned short       menuitemIndex,
                    nsIWebShell         *aWebShell);
  
  void LoadSubMenu(nsIMenu              *pParentMenu,
                   nsIDOMElement        *menuElement,
                   nsIDOMNode           *menuNode);

  void LoadMenuItem(nsIContextMenu      *pParentMenu,
                    nsIDOMElement       *menuitemElement,
                    nsIDOMNode          *menuitemNode,
                    unsigned short       menuitemIndex,
                    nsIWebShell         *aWebShell);
  
  void LoadSubMenu(nsIContextMenu       *pParentMenu,
                   nsIDOMElement        *menuElement,
                   nsIDOMNode           *menuNode);

  nsIMenu               *mMenu;
  
  nsIWidget             *mParent;
  
  nsIDOMNode            *mDOMNode;
  nsIWebShell           *mWebShell;
  nsIDOMElement         *mDOMElement;
  nsIMenuListener       *mListener;
  PRBool                mConstructed;

  nsString              mAlignment;
  nsString              mAnchorAlignment;

  PRInt32               mX;
  PRInt32               mY;
};

#endif // nsContextMenu_h__
