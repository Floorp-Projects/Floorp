/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef __NS_XPFCMENU_CONTAINER
#define __NS_XPFCMENU_CONTAINER

#include "nsIXPFCMenuContainer.h"
#include "nsIXPFCMenuBar.h"
#include "nsXPFCMenuItem.h"
#include "nsIArray.h"
#include "nsIIterator.h"
#include "nsIXPFCCommandReceiver.h"

class nsXPFCMenuContainer : public nsIXPFCMenuContainer,
                        public nsIXPFCMenuBar,
                        public nsIXPFCCommandReceiver,
                        public nsXPFCMenuItem
{

public:

  /**
   * Constructor and Destructor
   */

  nsXPFCMenuContainer();
  ~nsXPFCMenuContainer();

  /**
   * ISupports Interface
   */
  NS_DECL_ISUPPORTS

  /**
   * Initialize Method
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD Init();

  NS_IMETHOD AddMenuItem(nsIXPFCMenuItem * aMenuItem);
  NS_IMETHOD_(void*) GetNativeHandle();

  // nsIXMLParserObject methods
  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;

  NS_IMETHOD AddChild(nsIXPFCMenuItem * aItem);
  NS_IMETHOD Update() ;
  NS_IMETHOD SetShellContainer(nsIShellInstance * aShellInstance,nsIWebViewerContainer * aWebViewerContainer)  ;
  NS_IMETHOD_(nsIXPFCMenuItem *) MenuItemFromID(PRUint32 aID) ;
  NS_IMETHOD_(nsEventStatus) Action(nsIXPFCCommand * aCommand) ;

private:
  NS_IMETHOD ProcessActionCommand(nsString& aAction);

protected:
  nsIArray *     mChildMenus ;
  nsIShellInstance * mShellInstance;
  nsIWebViewerContainer * mWebViewerContainer;
};



#endif
