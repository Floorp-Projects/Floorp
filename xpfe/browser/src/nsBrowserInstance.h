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
#include "nsString.h"
#include "nsISupports.h"

#include "nsIStreamObserver.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIObserver.h"
#include "nsISessionHistory.h"

class nsIBrowserWindow;
class nsIWebShell;
class nsIScriptContext;
class nsIDOMWindow;
class nsIDOMNode;
class nsIURI;
class nsIWebShellWindow;
class nsIFindComponent;

#define SHISTORY_POPUP_LIST 10

// {5551A1E0-5A66-11d3-806A-00600811A9C3}
#define NS_BROWSERINSTANCE_CID { 0x5551a1e0, 0x5a66, 0x11d3, { 0x80, 0x6a, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }

////////////////////////////////////////////////////////////////////////////////
// nsBrowserInstance:
////////////////////////////////////////////////////////////////////////////////

class nsBrowserInstance : public nsIBrowserInstance,
                          public nsIDocumentLoaderObserver,
                          public nsIObserver,
					      public nsISessionHistory {
  public:

    nsBrowserInstance();
    virtual ~nsBrowserInstance();
                 
    NS_DECL_ISUPPORTS

    NS_DECL_NSIBROWSERINSTANCE

    NS_DEFINE_STATIC_CID_ACCESSOR( NS_BROWSERINSTANCE_CID )

    // nsIDocumentLoaderObserver
    NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand);
    NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus, nsIDocumentLoaderObserver* aObserver);
    NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsIContentViewer* aViewer);
    NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, PRUint32 aProgress, PRUint32 aProgressMax);
    NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg);
    NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus);
    NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader, nsIChannel* channel, const char *aContentType,const char *aCommand );		

    // nsIObserver
    NS_DECL_NSIOBSERVER
    // nsISessionHistory
    NS_DECL_NSISESSIONHISTORY

  protected:
    NS_IMETHOD ExecuteScript(nsIScriptContext * aContext, const nsString& aScript);
    void InitializeSearch(nsIFindComponent*);
    void BeginObserving();
    void EndObserving();
    NS_IMETHOD CreateMenuItem(nsIDOMNode * , PRInt32,const PRUnichar * );
	NS_IMETHOD UpdateGoMenu();
	NS_IMETHOD ClearHistoryPopup(nsIDOMNode * );

    PRBool              mIsViewSource;

    nsIScriptContext   *mToolbarScriptContext;			// weak reference
    nsIScriptContext   *mContentScriptContext;			// weak reference

    nsIDOMWindow       *mToolbarWindow;							// weak reference
    nsIDOMWindow       *mContentWindow;							// weak reference

    nsIWebShellWindow  *mWebShellWin;								// weak reference
    nsIWebShell *       mWebShell;									// weak reference
    nsIWebShell *       mContentAreaWebShell;				// weak reference
    nsIDocumentLoader * mContentAreaDocLoader;          // weak reference

    nsISessionHistory*  mSHistory;			           // this is a service

    nsCOMPtr<nsISupports>  mSearchContext;				// at last, something we really own
    nsInstanceCounter   mInstanceCounter;
	PRBool              mIsLoadingHistory;
#ifdef DEBUG_warren
    PRIntervalTime      mLoadStartTime;
#endif
};

#endif // nsBrowserInstance_h___
