/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsXULCommand_h__
#define nsXULCommand_h__

//#include "nsIMenuListener.h"
#include "nsIXULCommand.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIDOMElement.h"    // for some older c++ compilers.
#include "nsIWebShell.h"      // for some older c++ compilers.
#include "nsCOMPtr.h"

// Forward Declarations
class nsIDOMNode;
class nsIDOMEvent;


//----------------------------------------------------------------------

class nsXULCommand : public nsIXULCommand

{
public:
  nsXULCommand();
  virtual ~nsXULCommand();

  // nsISupports
  NS_DECL_ISUPPORTS


  NS_IMETHOD SetMenuItem(nsIMenuItem * aMenuItem);
  NS_IMETHOD AttributeHasBeenSet(const nsString & aAttr);
  NS_IMETHOD SetDOMElement(nsIDOMElement * aDOMElement);
  NS_IMETHOD GetDOMElement(nsIDOMElement ** aDOMElement);
  NS_IMETHOD SetWebShell(nsIWebShell * aWebShell);

  NS_IMETHOD SetCommand(const nsString & aStrCmd);
  NS_IMETHOD DoCommand();


  // nsIMenuListener 
  virtual nsEventStatus MenuItemSelected(const nsMenuEvent & aMenuEvent);
  virtual nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent);
  virtual nsEventStatus MenuDeselected(const nsMenuEvent & aMenuEvent) ;
  virtual nsEventStatus MenuConstruct(

    const nsMenuEvent & aMenuEvent,

    nsIWidget         * aParentWindow, 

    void              * menubarNode,

	void              * aWebShell);

  virtual nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent);
  
protected:
  NS_IMETHOD ExecuteJavaScriptString(nsIWebShell* aWebShell, nsString& aJavaScript);

  nsString                 mCommandStr;
  nsCOMPtr<nsIWebShell>    mWebShell;
  nsCOMPtr<nsIDOMElement>  mDOMElement;

  nsIMenuItem *            mMenuItem;

};


#endif /* nsXULCommand_h__ */
