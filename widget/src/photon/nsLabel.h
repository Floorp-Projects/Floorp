/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsLabel_h__
#define nsLabel_h__

#include "nsWidget.h"
#include "nsIAppShell.h"
#include "nsILabel.h"

/**
 * Native Photon Label wrapper
 */
class nsLabel :  public nsWidget,
                 public nsILabel
{

public:

  nsLabel();
  virtual ~nsLabel();

  // nsISupports
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsILabel part
  NS_IMETHOD SetLabel(const nsString &aText);
  NS_IMETHOD GetLabel(nsString &aBuffer);
  NS_IMETHOD SetAlignment(nsLabelAlignment aAlignment);

  NS_IMETHOD PreCreateWidget(nsWidgetInitData *aInitData);

  virtual PRBool OnMove(PRInt32 aX, PRInt32 aY);
  virtual PRBool OnPaint(nsPaintEvent &aEvent);
  virtual PRBool OnResize(nsSizeEvent &aEvent);

protected:
  NS_METHOD CreateNative( PtWidget_t *aParent );

  nsLabelAlignment mAlignment;

};

#endif // nsLabel_h__
