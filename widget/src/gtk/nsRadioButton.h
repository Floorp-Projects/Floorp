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

#ifndef nsRadioButton_h__
#define nsRadioButton_h__

#include "nsWidget.h"
#include "nsIRadioButton.h"

/**
 * Native GTK+ Radiobutton wrapper
 */
class nsRadioButton : public nsWidget,
                      public nsIRadioButton
{

public:
  nsRadioButton();
  virtual                 ~nsRadioButton();

  // nsISupports
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIRadioButton part
  NS_IMETHOD              SetLabel(const nsString& aText);
  NS_IMETHOD              GetLabel(nsString& aBuffer);
  NS_IMETHOD              SetState(const PRBool aState);
  NS_IMETHOD              GetState(PRBool& aState);


  virtual PRBool OnMove(PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
  virtual PRBool OnPaint(nsPaintEvent & aEvent) { return PR_FALSE; }
  virtual PRBool OnResize(nsRect &aRect) { return PR_FALSE; }

  // These are needed to Override the auto check behavior
  void Armed();
  void DisArmed();

protected:
  NS_IMETHOD  CreateNative(GtkWidget *parentWindow);
  virtual void InitCallbacks(char * aName = nsnull);
  virtual void OnDestroySignal(GtkWidget* aGtkWidget);

  // Sets background for checkbutton
  virtual void SetBackgroundColorNative(GdkColor *aColorNor,
                                        GdkColor *aColorBri,
                                        GdkColor *aColorDark);

  GtkWidget *mLabel;
  GtkWidget *mRadioButton;

};

#endif // nsRadioButton_h__
