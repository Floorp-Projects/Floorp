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

#include "nsIXULCommand.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMKeyListener.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIDOMElement.h"    // for some older c++ compilers.
#include "nsIWebShell.h"      // for some older c++ compilers.
#include "nsCOMPtr.h"

// Forward Declarations
class nsIDOMNode;
class nsIDOMEvent;


//----------------------------------------------------------------------

class nsXULCommand : public nsIXULCommand,
                     public nsIDOMMouseListener,
                     public nsIDOMKeyListener

{
public:
  nsXULCommand();
  virtual ~nsXULCommand();

  // nsISupports
  NS_DECL_ISUPPORTS

  // XUL UI Objects
  NS_IMETHOD SetName(const nsString &aName);
  NS_IMETHOD GetName(nsString &aName) const;

  NS_IMETHOD AddUINode(nsIDOMNode * aNode);
  NS_IMETHOD RemoveUINode(nsIDOMNode * aCmd);

  NS_IMETHOD SetEnabled(PRBool aIsEnabled);
  NS_IMETHOD GetEnabled(PRBool & aIsEnabled);

  NS_IMETHOD SetTooltip(const nsString &aTip);
  NS_IMETHOD GetTooltip(nsString &aTip) const;
  NS_IMETHOD SetDescription(const nsString &aDescription);
  NS_IMETHOD GetDescription(nsString &aDescription) const;

  NS_IMETHOD DoCommand();

  // Non-Interface Methods
  NS_IMETHOD SetDOMElement(nsIDOMElement * aDOMNode);
  NS_IMETHOD SetWebShell(nsIWebShell * aWebShell);
  NS_IMETHOD SetCommand(const nsString & aStrCmd);


  // nsIDOMEventListener
  virtual nsresult ProcessEvent(nsIDOMEvent* aEvent);

  // nsIDOMMouseListener (is derived from nsIDOMEventListener)
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent);

  // nsIDOMKeyListener 
  virtual nsresult KeyDown(nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyUp(nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent);


protected:
  NS_IMETHOD ExecuteJavaScriptString(nsIWebShell* aWebShell, nsString& aJavaScript);

  nsString     mName;
  nsString     mCommandStr;
  nsString     mTooltip;
  nsString     mDescription;
  PRBool       mIsEnabled;

  nsVoidArray  mSrcWidgets;

  nsCOMPtr<nsIWebShell>    mWebShell;
  nsCOMPtr<nsIDOMElement>  mDOMElement;

};


#endif /* nsXULCommand_h__ */