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
#ifndef nsIXPBaseWindow_h___
#define nsIXPBaseWindow_h___

#include "nsweb.h"
#include "nsISupports.h"

class nsIAppShell;
class nsIPref;
class nsIFactory;
class nsIWebShell;
class nsString;
class nsIPresShell;
class nsIDocumentLoaderObserver;
class nsIDOMElement;
class nsIDOMNode;
class nsWindowListener;

struct nsRect;

#define NS_IXPBASE_WINDOW_IID \
{ 0x36c1fe51, 0x6f3e, 0x11d2, { 0x8d, 0xca, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

#define NS_XPBASE_WINDOW_CID \
{ 0x36c1fe51, 0x6f3e, 0x11d2, { 0x8d, 0xca, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

enum nsXPBaseWindowType {   
                  ///creates a window
                eXPBaseWindowType_window,
                  ///creates a dialog
                eXPBaseWindowType_dialog,
                  ///creates a modal dialog
                eXPBaseWindowType_modalDialog,
              }; 



/**
 * API to a "XP window"
 */
class nsIXPBaseWindow : public nsISupports {
public:
  NS_IMETHOD Init(nsXPBaseWindowType aType,
                  nsIAppShell*       aAppShell,
                  nsIPref*           aPrefs,
                  const nsString&    aDialogURL,
                  const nsString&    aTitle,
                  const nsRect&      aBounds,
                  PRUint32           aChromeMask,
                  PRBool             aAllowPlugins = PR_TRUE) = 0;

  NS_IMETHOD SetLocation(PRInt32 aX, PRInt32 aY) = 0;

  NS_IMETHOD SetDimensions(PRInt32 aWidth, PRInt32 aHeight) = 0;

  NS_IMETHOD GetBounds(nsRect& aResult) = 0;

  NS_IMETHOD GetWindowBounds(nsRect& aResult) = 0;

  NS_IMETHOD SetVisible(PRBool aIsVisible) = 0;

  NS_IMETHOD Close() = 0;

  NS_IMETHOD SetTitle(const PRUnichar* aTitle) = 0;

  NS_IMETHOD GetTitle(PRUnichar** aResult) = 0;

  NS_IMETHOD GetWebShell(nsIWebShell*& aResult) = 0;

  NS_IMETHOD LoadURL(const nsString &aURL) = 0;

  NS_IMETHOD GetPresShell(nsIPresShell*& aPresShell) = 0;

  NS_IMETHOD FindDOMElement(const nsString &aId, nsIDOMElement *& aElement) = 0;
  NS_IMETHOD AddEventListener(nsIDOMNode * aNode) = 0;
  NS_IMETHOD RemoveEventListener(nsIDOMNode * aNode) = 0;

  NS_IMETHOD AddWindowListener(nsWindowListener * aWindowListener) = 0;

  // XXX minimize, maximize
  // XXX event control: enable/disable window close box, stick to glass, modal
};

#endif /* nsIXPBaseWindow_h___ */
