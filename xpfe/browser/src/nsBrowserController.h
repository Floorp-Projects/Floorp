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

#ifndef nsBrowserController_h__
#define nsBrowserController_h__

#include "nsWidgetListener.h"
#include "nsIFactory.h"

// Forward class declarations...
class nsIWebShell;

class nsBrowserController : public nsWidgetListener
{
public:
  nsBrowserController();

  // nsIWidgetController interface...
  NS_IMETHOD Initialize(nsIDOMDocument* aDocument, nsISupports* aContainer);

  // Event handlers...
  virtual nsresult KeyUp     (nsIDOMEvent* aKeyEvent);
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult Load      (nsIDOMEvent* aLoadEvent);
  virtual nsresult Unload    (nsIDOMEvent* aLoadEvent);


protected:
  virtual ~nsBrowserController();

  void  doUpdateToolbarState(void);

  nsIDOMElement* mLocationForm;
  nsIDOMElement* mForwardBtn;
  nsIDOMElement* mBackBtn;
  nsIDOMElement* mURLTypeIn;
  nsIWebShell*   mWebWindow;
  nsIWebShell*   mWebShell;
};

/* Function which creates an instance of the BrowserController factory... */
nsresult NS_NewBrowserControllerFactory(nsIFactory** aResult);

#endif /* nsBrowserController_h__ */