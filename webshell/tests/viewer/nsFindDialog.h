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
#include "nsBaseDialog.h"
#include "nsIDOMElement.h"

/**
 * Implement Navigator Find Dialog
 */
class nsFindDialog : public nsBaseDialog
{
public:

  nsFindDialog(nsBrowserWindow * aBrowserWindow);
  virtual ~nsFindDialog();

  // nsIWindowListener Methods

  virtual void MouseClick(nsIDOMEvent* aMouseEvent, nsIXPBaseWindow * aWindow, PRBool &aStatus);
  virtual void Initialize(nsIXPBaseWindow * aWindow);
  virtual void Destroy(nsIXPBaseWindow * aWindow);
  virtual void DoClose();

  // new methods
  virtual void DoFind();

protected:

  nsIDOMElement   * mFindBtn;
  nsIDOMElement   * mUpRB;
  nsIDOMElement   * mDwnRB;
  nsIDOMElement   * mMatchCaseCB;

  PRBool            mSearchDown;

};

#endif /* nsFindDialog_h___ */
