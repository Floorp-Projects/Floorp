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

#ifndef nsCheckButton_h__
#define nsCheckButton_h__

#include "nsWidget.h"
#include "nsICheckButton.h"

/**
 * Native GTK+ Checkbox wrapper
 */


class nsCheckButton : public nsWidget,
                      public nsICheckButton
{

public:
  nsCheckButton();
  virtual                 ~nsCheckButton();

  NS_DECL_ISUPPORTS_INHERITED

  // nsICheckButton part
  NS_IMETHOD SetLabel(const nsString &aText);
  NS_IMETHOD GetLabel(nsString &aBuffer);
  NS_IMETHOD SetState(const PRBool aState);
  NS_IMETHOD GetState(PRBool& aState);

  virtual void OnToggledSignal(const gboolean aState);

protected:
  NS_IMETHOD CreateNative(GtkWidget *parentWindow);
  virtual void InitCallbacks(char * aName = nsnull);
  virtual void OnDestroySignal(GtkWidget* aGtkWidget);

  // Sets background for checkbutton
  virtual void SetBackgroundColorNative(GdkColor *aColorNor,
                                        GdkColor *aColorBri,
                                        GdkColor *aColorDark);

  GtkWidget *mLabel;
  GtkWidget *mCheckButton;

  // We need to maintain our own state to be in sync with the
  // gecko check controlling frame.
  PRBool     mState;

private:

  static gint ToggledSignal(GtkWidget *      aWidget, 
                            gpointer         aData);
};

#endif // nsCheckButton_h__
