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
#ifndef NECKO
#include "nsINetlibURL.h"
#include "nsINetService.h"
#endif
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



class nsXPInstallManager : public nsIXPINotifier, public nsIStreamListener
{
    public:
        nsXPInstallManager();
        virtual ~nsXPInstallManager();

        NS_DECL_ISUPPORTS

        NS_IMETHOD InitManager( nsXPITriggerInfo* aTrigger );

#ifdef NECKO
        // nsIStreamObserver
        NS_IMETHOD OnStartBinding(nsISupports *ctxt);
        NS_IMETHOD OnStopBinding(nsISupports *ctxt, nsresult status, 
                                 const PRUnichar *errorMsg);
        NS_IMETHOD OnStartRequest(nsISupports *ctxt) { return NS_OK; }
        NS_IMETHOD OnStopRequest(nsISupports *ctxt, nsresult status, 
                                 const PRUnichar *errorMsg) { return NS_OK; }
        // nsIStreamListener
        NS_IMETHOD OnDataAvailable(nsISupports *ctxt, 
                                   nsIBufferInputStream *inStr,
                                   PRUint32 sourceOffset, 
                                   PRUint32 count);
#else
        // IStreamListener methods
        NS_IMETHOD GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* info);
        NS_IMETHOD OnProgress(nsIURI* aURL, PRUint32 Progress, PRUint32 ProgressMax);
        NS_IMETHOD OnStatus(nsIURI* aURL, const PRUnichar* aMsg);
        NS_IMETHOD OnStartBinding(nsIURI* aURL, const char *aContentType);
        NS_IMETHOD OnDataAvailable(nsIURI* aURL, nsIInputStream *pIStream, PRUint32 length);
        NS_IMETHOD OnStopBinding(nsIURI* aURL, nsresult status, const PRUnichar* aMsg);
#endif
        
        // IXPINotifier methods
        NS_IMETHOD BeforeJavascriptEvaluation();
        NS_IMETHOD AfterJavascriptEvaluation();
        NS_IMETHOD InstallStarted(const char *UIPackageName);
        NS_IMETHOD ItemScheduled(const char *message);
        NS_IMETHOD InstallFinalization(const char *message, PRInt32 itemNum, PRInt32 totNum);
        NS_IMETHOD InstallAborted();
        NS_IMETHOD LogComment(const char *comment);

        // IXULWindowCallbacks methods
//        NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell);
//        NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell) { return NS_OK; }

    private:
        nsresult DownloadNext();
        
        nsXPITriggerInfo*   mTriggers;
        nsXPITriggerItem*   mItem;
        PRUint32            mNextItem;

        nsCOMPtr<nsIXPIProgressDlg>  mDlg;
        nsCOMPtr<nsIXPIProgressDlg>  mProxy;
};

#endif
