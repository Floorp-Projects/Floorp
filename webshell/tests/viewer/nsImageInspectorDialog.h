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
#ifndef nsImageInspectorDialog_h___
#define nsImageInspectorDialog_h___

#include "nsBaseDialog.h"
#include "nsIDOMElement.h"


class nsBrowserWindow;
class nsIDOMDocument;

/**
 * Implement Navigator Find Dialog
 */
class nsImageInspectorDialog : public nsBaseDialog
{
public:

  nsImageInspectorDialog(nsBrowserWindow * aBrowserWindow, nsIDOMDocument * aDoc);
  virtual ~nsImageInspectorDialog();

  virtual void DoClose();

  // nsIWindowListener Methods

  virtual void MouseClick(nsIDOMEvent* aMouseEvent, nsIXPBaseWindow * aWindow, PRBool &aStatus);
  virtual void Initialize(nsIXPBaseWindow * aWindow);
  virtual void Destroy(nsIXPBaseWindow * aWindow);

protected:

  nsIDOMElement  * mOKBtn;
  nsIDOMDocument * mDOMDoc;

};

#endif /* nsImageInspectorDialog_h___ */
