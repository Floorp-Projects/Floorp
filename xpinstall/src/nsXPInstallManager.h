/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#include "nsIChromeRegistry.h"
#include "nsIDOMWindowInternal.h"

#include "nsISoftwareUpdate.h"

#include "nsCOMPtr.h"

#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsPIXPIManagerCallbacks.h"

#include "nsIDialogParamBlock.h"


class nsXPInstallManager : public nsIXPIListener, 
                           public nsIStreamListener,
                           public nsIProgressEventSink,
                           public nsIInterfaceRequestor,
                           public nsPIXPIManagerCallbacks
{
    public:
        nsXPInstallManager();
        virtual ~nsXPInstallManager();

        NS_DECL_ISUPPORTS

        NS_IMETHOD InitManager(nsIScriptGlobalObject* aGlobalObject, nsXPITriggerInfo* aTrigger, PRUint32 aChromeType );

        // nsIStreamObserver
        NS_DECL_NSISTREAMOBSERVER

        // nsIStreamListener
        NS_DECL_NSISTREAMLISTENER
        
        // IXPIListener methods
        NS_DECL_NSIXPILISTENER

        // nsIProgressEventSink
        NS_DECL_NSIPROGRESSEVENTSINK

        // nsIInterfaceRequestor
        NS_DECL_NSIINTERFACEREQUESTOR
    
        //nsPIXPIMANAGERCALLBACKS
        NS_DECL_NSPIXPIMANAGERCALLBACKS


    private:
        NS_IMETHOD  DownloadNext();
        void        Shutdown();
        NS_IMETHOD  GetDestinationFile(nsString& url, nsILocalFile* *file);
        void        LoadDialogWithNames(nsIDialogParamBlock* ioParamBlock);
        PRBool      ConfirmInstall(nsIScriptGlobalObject* aGlobalObject, nsIDialogParamBlock* ioParamBlock);
        PRBool      ConfirmChromeInstall(nsIScriptGlobalObject* aGlobalObject);
        PRBool      TimeToUpdate(PRTime now);
        
        nsXPITriggerInfo*   mTriggers;
        nsXPITriggerItem*   mItem;
        PRUint32            mNextItem;
        PRInt32             mNumJars;
        PRBool              mFinalizing;
        PRBool              mCancelled;
        PRUint32            mChromeType;
        PRBool              mSelectChrome;
        PRInt32             mContentLength;
        PRTime              mLastUpdate;

        nsCOMPtr<nsIXPIProgressDlg>  mDlg;
        nsCOMPtr<nsIStringBundle>    mStringBundle;
};

#endif
