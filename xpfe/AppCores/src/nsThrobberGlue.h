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

//
// nsThrobberGlue
// Mike Pinkerton, Netscape Communications
//
// The goal of XPToolkit is to write reusable chunks of UI which know very little
// about the actual implementations, and application services (AppCores) which
// know very little about the UI. Gecko's nsIWebShell unfortunately not only knows
// about the existance of a "throbber" which illustrates that the web page is
// currently loading, it want's to speak to it directly.
//
// In order to make the web shell fit into the "XPToolkit Way," we've created a
// little bit of "glue" code which lives in the AppCore for the browser.
// nsThrobberGlue intercepts the Start() and Stop() methods of nsIThrobber and
// reinterprets them into changes in a DOM node which either the style system or
// any listener may notice.
//


#ifndef nsThrobberGlue_h__
#define nsThrobberGlue_h__


#include "nsIThrobber.h"


class nsThrobberGlue : public nsIThrobber
{
public:
  nsThrobberGlue();
  virtual ~nsThrobberGlue();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIThrobber
  NS_IMETHOD Init(nsIWidget* aParent, const nsRect& aBounds, const nsString& aFileNameMask, PRInt32 aNumImages);
  NS_IMETHOD Init(nsIWidget* aParent, const nsRect& aBounds);
  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD Show();
  NS_IMETHOD Hide();
  NS_IMETHOD Start();
  NS_IMETHOD Stop();

protected:

}; // class nsThrobberGlue


#endif