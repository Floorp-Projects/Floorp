/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#ifndef nsBrowserInstance_h___
#define nsBrowserInstance_h___

#include "nsIBrowserInstance.h"

#include "nsIAppShellComponentImpl.h"

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsString.h"
#include "nsISupports.h"

#include "nsIStreamObserver.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIObserver.h"
#include "nsISessionHistory.h"
#include "nsIURIContentListener.h"
#include "nsICmdLineHandler.h"

#ifdef DEBUG_radha
#include "nsISHistory.h"
#endif  

class nsIDocShell;
class nsIScriptContext;
class nsIDOMWindow;
class nsIDOMNode;
class nsIURI;
class nsIWebShellWindow;
class nsIFindComponent;


#define SHISTORY_POPUP_LIST 10

////////////////////////////////////////////////////////////////////////////////
// nsBrowserInstance:
////////////////////////////////////////////////////////////////////////////////

class nsBrowserInstance : public nsIBrowserInstance,
                          public nsIDocumentLoaderObserver,
					                public nsISessionHistory,
                          public nsIURIContentListener,
                          public nsSupportsWeakReference {
  public:

    nsBrowserInstance();
    virtual ~nsBrowserInstance();
                 
    NS_DECL_ISUPPORTS

    NS_DECL_NSIBROWSERINSTANCE

    NS_DEFINE_STATIC_CID_ACCESSOR( NS_BROWSERINSTANCE_CID )

    // nsIDocumentLoaderObserver
    NS_DECL_NSIDOCUMENTLOADEROBSERVER

    // nsISessionHistory
    NS_DECL_NSISESSIONHISTORY
    // URI Content listener
    NS_DECL_NSIURICONTENTLISTENER

  protected:
    NS_IMETHOD ExecuteScript(nsIScriptContext * aContext, const nsString& aScript);
    nsresult InitializeSearch(nsIFindComponent*);
    NS_IMETHOD CreateMenuItem(nsIDOMNode * , PRInt32,const PRUnichar * );
	  NS_IMETHOD UpdateGoMenu();
	  NS_IMETHOD ClearHistoryPopup(nsIDOMNode * );

    PRBool              mIsClosed;

    nsIScriptContext   *mToolbarScriptContext;			// weak reference
    nsIScriptContext   *mContentScriptContext;			// weak reference

    nsIDOMWindow       *mToolbarWindow;							// weak reference
    nsIDOMWindow       *mContentWindow;							// weak reference

    nsIWebShellWindow  *mWebShellWin;								// weak reference
    nsIDocShell *       mDocShell;									// weak reference
    nsIDOMWindow*       mDOMWindow;                         // weak reference
    nsIWebShell *       mContentAreaWebShell;				// weak reference
    nsIDocumentLoader * mContentAreaDocLoader;          // weak reference

    nsISessionHistory*  mSHistory;			           // this is a service

    nsCOMPtr<nsISupports>  mSearchContext;				// at last, something we really own
    nsInstanceCounter   mInstanceCounter;
#ifdef DEBUG_radha
	nsISHistory *       mNewSHistory;
#endif  /* DEBUG_radha */
	PRBool              mIsLoadingHistory;
#ifdef DEBUG_warren
    PRIntervalTime      mLoadStartTime;
#endif
};

#endif // nsBrowserInstance_h___
