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

// Helper Classes
#include "nsCOMPtr.h"
#include "nsWeakReference.h"

// Interfaces Needed
#include "nsIBrowserInstance.h"
#include "nsIURIContentListener.h"
#include "nsIDocumentLoaderObserver.h"

 

#include "nsIAppShellComponentImpl.h"

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"

#include "nsIStreamObserver.h"
#include "nsIObserver.h"
#include "nsICmdLineHandler.h"
#include "nsIXULBrowserWindow.h"
#include "nsIWebProgressListener.h"
#include "nsIWebShell.h"
#include "nsIUrlbarHistory.h"
#include "nsISHistory.h"

class nsIDocShell;
class nsIScriptContext;
class nsIDOMWindowInternal;
class nsIDOMNode;
class nsIURI;
class nsIWebShellWindow;
class nsIFindComponent;



#define SHISTORY_POPUP_LIST 10
#define SH_IN_FRAMES

////////////////////////////////////////////////////////////////////////////////
// nsBrowserInstance:
////////////////////////////////////////////////////////////////////////////////

class nsBrowserInstance : public nsIBrowserInstance,
                          public nsIDocumentLoaderObserver,
                          public nsIURIContentListener,
                          public nsIWebProgressListener,
                          public nsSupportsWeakReference 
{
  public:

    nsBrowserInstance();
    virtual ~nsBrowserInstance();
                 
    NS_DECL_ISUPPORTS

    NS_DECL_NSIBROWSERINSTANCE

    NS_DEFINE_STATIC_CID_ACCESSOR( NS_BROWSERINSTANCE_CID )

    // nsIDocumentLoaderObserver
    NS_DECL_NSIDOCUMENTLOADEROBSERVER

    // URI Content listener
    NS_DECL_NSIURICONTENTLISTENER

    NS_DECL_NSIWEBPROGRESSLISTENER

    static PRUint32 gRefCnt;

  public:
    nsIDocShell* GetContentAreaDocShell();
    nsIDOMWindowInternal* GetContentWindow();
    nsIDocumentLoader* GetContentAreaDocLoader();
    void ReinitializeContentVariables();

  protected:
    nsresult InitializeSearch(nsIFindComponent*);
    NS_IMETHOD CreateMenuItem(nsIDOMNode * , PRInt32,const PRUnichar * );
    NS_IMETHOD EnsureXULBrowserWindow();
	NS_IMETHOD ClearHistoryMenus(nsIDOMNode * );

    PRBool              mIsClosed;

    nsCOMPtr<nsIXULBrowserWindow> mXULBrowserWindow;
#ifdef SH_IN_FRAMES
	nsCOMPtr<nsISHistory>   mSessionHistory;
#endif
    nsIScriptContext   *mContentScriptContext;			// weak reference

    nsWeakPtr          mContentWindowWeak;
    nsWeakPtr          mContentAreaDocShellWeak;
    nsWeakPtr          mContentAreaDocLoaderWeak;

    nsIWebShellWindow  *mWebShellWin;								// weak reference
    nsIDocShell *       mDocShell;									// weak reference
    nsIDOMWindowInternal*       mDOMWindow;                         // weak reference

    nsCOMPtr<nsIUrlbarHistory> mUrlbarHistory;                  //We own this
    nsCOMPtr<nsISupports>  mSearchContext;				// at last, something we really own
    nsInstanceCounter   mInstanceCounter;
#ifdef DEBUG_warren
    PRIntervalTime      mLoadStartTime;
#endif
};

#endif // nsBrowserInstance_h___
