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

#ifndef nsWebShellWindow_h__
#define nsWebShellWindow_h__

#include "nsISupports.h"
#include "nsGUIEvent.h"


/* Forward declarations.... */
class nsIURL;
class nsIAppShell;
class nsIWidget;
class nsIWebShell;


class nsWebShellWindow : public nsISupports
{
public:
  nsWebShellWindow();

  NS_DECL_ISUPPORTS

  nsresult Initialize(nsIAppShell* aShell, nsIURL* aUrl);
  
  nsIWidget* GetWidget(void) { return mWindow; }

protected:
  virtual ~nsWebShellWindow();

  static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);

  nsIWidget*   mWindow;
  nsIWebShell* mWebShell;
};


#endif /* nsWebShellWindow_h__ */