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

#ifndef nsMenuItem_h__
#define nsMenuItem_h__


#include "nsIMenuItem.h"
#include "nsString.h"
#include "nsIMenuListener.h"

#include "nsIXULCommand.h"

class nsIMenu;
class nsIPopUpMenu;
class nsIWidget;

/**
 * Native Motif MenuItem wrapper
 */

class nsMenuItem : public nsIMenuItem, public nsIMenuListener
{

public:
  nsMenuItem();
  virtual ~nsMenuItem();

  // nsISupports
  NS_DECL_ISUPPORTS

  NS_IMETHOD Create(nsIMenu        *aParent, 
                    const nsString &aLabel,  
                    PRUint32        aCommand);

  NS_IMETHOD Create(nsIPopUpMenu   *aParent, 
                    const nsString &aLabel, 
                    PRUint32        aCommand) ;

  // nsIMenuBar Methods
  NS_IMETHOD GetLabel(nsString &aText);
  NS_IMETHOD SetLabel(nsString &aText);
  NS_IMETHOD GetCommand(PRUint32 & aCommand);
  NS_IMETHOD GetTarget(nsIWidget *& aTarget);
  NS_IMETHOD GetNativeData(void*& aData);

  // nsIMenuListener interface
  nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent);
  
  NS_IMETHOD SetXULCommand(nsIXULCommand * aXULCommand);
  
protected:
  //void Create(nsIWidget * aMBParent, Widget aParent,
              //const nsString &aLabel, PRUint32 aCommand);
  nsIWidget * GetMenuBarParent(nsISupports * aParentSupports);
  //Widget GetNativeParent();

  nsString   mLabel;
  PRUint32   mCommand;

  nsIMenu      * mMenuParent;
  nsIPopUpMenu * mPopUpParent;
  nsIWidget    * mTarget;

  nsIXULCommand * mXULCommand;
  //Widget mMenu; // native cascade widget

};

#endif // nsMenuItem_h__
