/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 */

#ifndef _NS_XPINSTALLMANAGER_H
#define _NS_XPINSTALLMANAGER_H

#include "nsInstall.h"

#include "nscore.h"
#include "nsISupports.h"
#include "nsString.h"

#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsIXPINotifier.h"
#include "nsXPITriggerInfo.h"
#include "nsIXPIProgressDlg.h"

#include "nsISoftwareUpdate.h"

#include "nsCOMPtr.h"

#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIXULWindowCallbacks.h"    
#include "nsIProgressEventSink.h"


class nsXPInstallManager : public nsIXPINotifier, 
                           public nsIStreamListener,
                           public nsIProgressEventSink,
                           public nsIXULWindowCallbacks
{
    public:
        nsXPInstallManager();
        virtual ~nsXPInstallManager();

        NS_DECL_ISUPPORTS

        NS_IMETHOD InitManager( nsXPITriggerInfo* aTrigger );

        // nsIStreamObserver
        NS_DECL_NSISTREAMOBSERVER

        // nsIStreamListener
        NS_DECL_NSISTREAMLISTENER
        
        // IXPINotifier methods
        NS_DECL_NSIXPINOTIFIER

        // nsIProgressEventSink
        NS_DECL_NSIPROGRESSEVENTSINK

        // IXULWindowCallbacks methods
        NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell);
        NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell);

    private:
        nsresult DownloadNext();
        void     Shutdown();
        
        nsXPITriggerInfo*   mTriggers;
        nsXPITriggerItem*   mItem;
        PRUint32            mNextItem;
        PRInt32             mNumJars;
        PRBool              mFinalizing;

        nsCOMPtr<nsIXPIProgressDlg>  mDlg;
        nsCOMPtr<nsIXPIProgressDlg>  mProxy;
        nsCOMPtr<nsIStringBundle>    mStringBundle;
};

#endif
