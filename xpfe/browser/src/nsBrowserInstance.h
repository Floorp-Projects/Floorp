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

#define SH_IN_FRAMES

////////////////////////////////////////////////////////////////////////////////
// nsBrowserInstance:
////////////////////////////////////////////////////////////////////////////////

class nsBrowserInstance : public nsIBrowserInstance,
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

    // URI Content listener
    NS_DECL_NSIURICONTENTLISTENER

    // WebProgress listener
    NS_DECL_NSIWEBPROGRESSLISTENER

  protected:

    nsresult GetContentAreaDocShell(nsIDocShell** outDocShell);
    nsresult GetContentWindow(nsIDOMWindowInternal** outContentWindow);
    
    nsresult GetFocussedContentWindow(nsIDOMWindowInternal** outFocussedWindow);
    
    void ReinitializeContentWindow();
    void ReinitializeContentVariables();

    nsresult InitializeSearch(nsIDOMWindowInternal* windowToSearch, nsIFindComponent *finder );
    
    NS_IMETHOD EnsureXULBrowserWindow();

    // helper methods for dealing with document loading...
    nsresult StartDocumentLoad(nsIDOMWindow *aDOMWindow,
                               nsIRequest *request);

    nsresult EndDocumentLoad(nsIDOMWindow *aDOMWindow,
                             nsIRequest *request,
                             nsresult aResult);

    PRBool              mIsClosed;
    static PRBool       sCmdLineURLUsed;

    nsCOMPtr<nsIXULBrowserWindow> mXULBrowserWindow;
#ifdef SH_IN_FRAMES
	nsCOMPtr<nsISHistory>   mSessionHistory;
#endif

    nsWeakPtr          mContentAreaDocShellWeak;

    nsIWebShellWindow  *mWebShellWin;								// weak reference
    nsIDocShell *       mDocShell;									// weak reference
    nsIDOMWindowInternal*       mDOMWindow;                         // weak reference

    nsCOMPtr<nsIUrlbarHistory> mUrlbarHistory;                  //We own this
    nsCOMPtr<nsISupports>  mSearchContext;				// at last, something we really own
    nsInstanceCounter   mInstanceCounter;
    nsCOMPtr<nsIInputStream> mPostData; // Post data for current page.
#ifdef DEBUG_warren
    PRIntervalTime      mLoadStartTime;
#endif
};

#endif // nsBrowserInstance_h___
