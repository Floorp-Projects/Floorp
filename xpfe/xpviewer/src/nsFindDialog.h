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
#ifndef nsFindDialog_h___
#define nsFindDialog_h___

#include "nsBrowserWindow.h"
#include "nsWindowListener.h"
#include "nsIDOMElement.h"

/**
 * Implement Navigator Find Dialog
 */
class nsFindDialog : public nsWindowListener
{
public:

  nsFindDialog(nsBrowserWindow * aBrowserWindow);
  virtual ~nsFindDialog();

  // nsWindowListener Methods

  void MouseClick(nsIDOMEvent* aMouseEvent, nsIXPBaseWindow * aWindow);
  void Initialize(nsIXPBaseWindow * aWindow);
  void Destroy(nsIXPBaseWindow * aWindow);

  // new methods
  virtual void DoFind(nsIXPBaseWindow * aWindow);

protected:
  PRBool IsChecked(nsIDOMElement * aNode);
  void   SetChecked(nsIDOMElement * aNode, PRBool aValue);

  nsBrowserWindow * mBrowserWindow;
  nsIDOMElement   * mFindBtn;
  nsIDOMElement   * mCancelBtn;
  nsIDOMElement   * mUpRB;
  nsIDOMElement   * mDwnRB;
  nsIDOMElement   * mMatchCaseCB;

  PRBool            mSearchDown;

};

#endif /* nsFindDialog_h___ */
