/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#ifdef NECKO
    NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand);
    NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus, nsIDocumentLoaderObserver* aObserver);
    NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsIContentViewer* aViewer);
    NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, PRUint32 aProgress, PRUint32 aProgressMax);
    NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg);
    NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus);
    NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader, nsIChannel* channel, const char *aContentType,const char *aCommand );		
#else
    NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand);
    NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, nsIURI *aUrl, PRInt32 aStatus,
								 nsIDocumentLoaderObserver * aObserver);

    NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aContentType, 
                            nsIContentViewer* aViewer);
    NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRUint32 aProgress, 
                               PRUint32 aProgressMax);
    NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, nsString& aMsg);
    NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRInt32 aStatus);
    NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,
                                        nsIURI *aURL,
                                        const char *aContentType,
                                        const char *aCommand );
#endif

    // nsIObserver
    NS_DECL_NSIOBSERVER

	// nsISessionHistory methods 
    NS_IMETHOD GoForward(nsIWebShell * aPrev);

    NS_IMETHOD GoBack(nsIWebShell * aPrev);
#ifdef NECKO
	NS_IMETHOD Reload(nsIWebShell * aPrev, nsLoadFlags aType);
#else
	NS_IMETHOD Reload(nsIWebShell * aPrev, nsURLReloadType aType);
#endif  /* NECKO */

    NS_IMETHOD canForward(PRBool &aResult);

    NS_IMETHOD canBack(PRBool &aResult);

    NS_IMETHOD add(nsIWebShell * aWebShell);

    NS_IMETHOD Goto(PRInt32 aHistoryIndex, nsIWebShell * aPrev, PRBool aIsReloading);

    NS_IMETHOD getHistoryLength(PRInt32 & aResult);

    NS_IMETHOD getCurrentIndex(PRInt32 & aResult);

  //  NS_IMETHOD cloneHistory(nsISessionHistory * aSessionHistory);

    NS_IMETHOD SetLoadingFlag(PRBool aFlag);

    NS_IMETHOD GetLoadingFlag(PRBool &aFlag);

    NS_IMETHOD SetLoadingHistoryEntry(nsHistoryEntry * aHistoryEntry);

    NS_IMETHOD GetURLForIndex(PRInt32 aIndex, const PRUnichar ** aURL);

    NS_IMETHOD SetURLForIndex(PRInt32 aIndex, const PRUnichar * aURL);

	NS_IMETHOD GetTitleForIndex(PRInt32 aIndex, const PRUnichar ** aTitle);

    NS_IMETHOD SetTitleForIndex(PRInt32 aIndex, const PRUnichar * aTitle);

	NS_IMETHOD GetHistoryObjectForIndex(PRInt32 aIndex,  nsISupports ** aState);

    NS_IMETHOD SetHistoryObjectForIndex(PRInt32 aIndex, nsISupports * aState);

  protected:
    NS_IMETHOD DoDialog();
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

    nsISessionHistory*  mSHistory;			           // this is a service

    nsCOMPtr<nsISupports>  mSearchContext;				// at last, something we really own

    nsInstanceCounter   mInstanceCounter;
#ifdef DEBUG_warren
    PRIntervalTime      mLoadStartTime;
#endif
};

#endif // nsBrowserInstance_h___
