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

#ifndef nsIAppShellService_h__
#define nsIAppShellService_h__

#include "nscore.h"
#include "nsCom.h"
#include "nsISupports.h"

/* Forward declarations... */
class nsIFactory;
class nsIURL;
class nsIWidget;
class nsString;
class nsIStreamObserver;

// e5e5af70-8a38-11d2-9938-0080c7cb1080
#define NS_IAPPSHELL_SERVICE_IID \
{ 0xe5e5af70, 0x8a38, 0x11d2, \
  {0x99, 0x38, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80} }


class nsIAppShellService : public nsISupports
{
public:
  static const nsIID& IID() { static nsIID iid = NS_IAPPSHELL_SERVICE_IID; return iid; }

  NS_IMETHOD Initialize(void) = 0;
  NS_IMETHOD Run(void) = 0;
  NS_IMETHOD GetNativeEvent(void *& aEvent, nsIWidget* aWidget, PRBool &aIsInWindow, PRBool &aIsMouseEvent) = 0;
  NS_IMETHOD DispatchNativeEvent(void * aEvent) = 0;
  NS_IMETHOD Shutdown(void) = 0;

  NS_IMETHOD CreateTopLevelWindow(nsIURL* aUrl, 
                                  nsString& aControllerIID,
                                  nsIWidget*& aResult, nsIStreamObserver* anObserver) = 0;
  NS_IMETHOD CreateDialogWindow(nsIWidget * aParent,
                                nsIURL* aUrl, 
                                nsString& aControllerIID,
                                nsIWidget*& aResult, nsIStreamObserver* anObserver) = 0;
  NS_IMETHOD CloseTopLevelWindow(nsIWidget* aWindow) = 0;
};

#endif /* nsIAppShellService_h__ */
