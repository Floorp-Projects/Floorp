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
class nsIWindowListener;
class nsIDOMHTMLDocument;

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
                eXPBaseWindowType_modalDialog
              }; 


/**
 * BaseWindow for HTML Dialog Boxes and Windows. The desciption of the dialog box
 * or window is encoded in a HTML File. The Contents of the HTML file and the current
 * settings for form elements are accessed through the W3C DOM interfaces. 
 * Access to the nsIDOM classes is done through C++ rather than JavaScript. However,
 * JavaScript event handlers can be used with the HTML File as well.
 * The BaseWindow contains methods for:
 *
 * 1) loading a HTML file 
 * 2) Initializing the default values for form elements.
 * 3) attaching an event listener to process click events.
 * 4) Getting a handle to the HTMLDocumentElement to access nsIDOMElements.
 */

class nsIXPBaseWindow : public nsISupports {
public:

 /**
  * Initialize the window or dialog box.
  * @param aType see nsXPBaseWindowType's above
  * @param aAppShell application shell
  * @param aPref     Preferences
  * @param aDialogURL URL of HTML file describing the dialog or window
  * @param aTitle    Title of the dialog box or window
  * @param aBounds   x, y, width, and height of the window or dialog box
  * XXX: aChrome is probably not needed for dialog boxes and windows, this is a holdover
  * from the nsBrowserWindow.
  * @param aChrome   Chrome mask for toolbars and statusbars. 
  * @param aAllowPlugins  if TRUE then plugins can be referenced in the HTML file.          
  */

  NS_IMETHOD Init(nsXPBaseWindowType aType,
                  nsIAppShell*       aAppShell,
                  nsIPref*           aPrefs,
                  const nsString&    aDialogURL,
                  const nsString&    aTitle,
                  const nsRect&      aBounds,
                  PRUint32           aChromeMask,
                  PRBool             aAllowPlugins = PR_TRUE) = 0;
  
 /**
  * Set the location the window or dialog box on the screen
  * @param aX   horizontal location of the upper left 
  *             corner of the window in pixels from the screen.
  * @param aY   vertical location of the upper left 
  *             corner of the window in pixels from the screen.        
  */

  NS_IMETHOD SetLocation(PRInt32 aX, PRInt32 aY) = 0;

 /**
  * Set the width and height of the window or dialog box in pixels
  * @param aWidth width of the window or dialog box in pixels.
  * @param aHeight height of the window or dialog box in pixels. 
  */

  NS_IMETHOD SetDimensions(PRInt32 aWidth, PRInt32 aHeight) = 0;

  NS_IMETHOD GetBounds(nsRect& aResult) = 0;

  NS_IMETHOD GetWindowBounds(nsRect& aResult) = 0;

  NS_IMETHOD SetVisible(PRBool aIsVisible) = 0;

  NS_IMETHOD Close() = 0;

  NS_IMETHOD SetTitle(const PRUnichar* aTitle) = 0;

  NS_IMETHOD GetTitle(const PRUnichar** aResult) = 0;

  NS_IMETHOD GetWebShell(nsIWebShell*& aResult) = 0;

  NS_IMETHOD LoadURL(const nsString &aURL) = 0;

  NS_IMETHOD GetPresShell(nsIPresShell*& aPresShell) = 0;

  NS_IMETHOD GetDocument(nsIDOMHTMLDocument *& aDocument) = 0;

  NS_IMETHOD AddEventListener(nsIDOMNode * aNode) = 0;

  NS_IMETHOD RemoveEventListener(nsIDOMNode * aNode) = 0;

  NS_IMETHOD AddWindowListener(nsIWindowListener * aWindowListener) = 0;

  // XXX minimize, maximize
  // XXX event control: enable/disable window close box, stick to glass, modal
};

#endif /* nsIXPBaseWindow_h___ */
