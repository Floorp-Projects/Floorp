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
#ifndef nsBaseDialog_h___
#define nsBaseDialog_h___

//#include "nsBrowserWindow.h"
#include "nsIWindowListener.h"
#include "nsIDOMElement.h"

class nsIXPBaseWindow;
class nsBrowserWindow;

/**
 * Implement Navigator Find Dialog
 */
class nsBaseDialog : public nsIWindowListener
{
public:

  nsBaseDialog(nsBrowserWindow * aBrowserWindow);
  virtual ~nsBaseDialog();

  virtual void DoClose();

  // nsIWindowListener Methods

  virtual void MouseClick(nsIDOMEvent* aMouseEvent, nsIXPBaseWindow * aWindow, PRBool &aStatus);
  virtual void Initialize(nsIXPBaseWindow * aWindow);
  virtual void Destroy(nsIXPBaseWindow * aWindow);


protected:
  PRBool IsChecked(const nsString & aName);
  PRBool IsChecked(nsIDOMElement * aNode);
  void   SetChecked(nsIDOMElement * aNode, PRBool aValue);
  void   SetChecked(const nsString & aName, PRBool aValue);
  void   GetText(nsIDOMElement * aNode, nsString & aStr);
  void   GetText(const nsString & aName, nsString & aStr);
  float  GetFloat(nsString & aStr);
  void   SetText(nsIDOMElement * aNode, const nsString &aValue);
  void   SetText(const nsString & aName, const nsString & aStr);

  nsBrowserWindow * mBrowserWindow;
  nsIXPBaseWindow * mWindow;
  nsIDOMElement   * mCancelBtn;

};

#endif /* nsBaseDialog_h___ */
