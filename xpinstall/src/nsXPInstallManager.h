/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIXPInstallManager.h"
#include "nsIXPIDialogService.h"
#include "nsXPITriggerInfo.h"
#include "nsIXPIProgressDialog.h"
#include "nsIChromeRegistry.h"
#include "nsIDOMWindowInternal.h"
#include "nsIObserver.h"

#include "nsISoftwareUpdate.h"

#include "nsCOMPtr.h"

#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#include "nsIDialogParamBlock.h"

#include "nsPICertNotification.h"

#include "nsWeakReference.h"

#define NS_XPIDIALOGSERVICE_CONTRACTID "@mozilla.org/embedui/xpinstall-dialog-service;1"
#define NS_XPINSTALLMANAGERCOMPONENT_CONTRACTID "@mozilla.org/xpinstall/install-manager;1"
#define XPI_PROGRESS_TOPIC "xpinstall-progress"

class nsXPInstallManager : public nsIXPIListener,
                           public nsIXPIDialogService,
                           public nsIXPInstallManager,
                           public nsIObserver,
                           public nsIStreamListener,
                           public nsIProgressEventSink,
                           public nsIInterfaceRequestor,
                           public nsPICertNotification,
                           public nsSupportsWeakReference
{
    public:
        nsXPInstallManager();
        virtual ~nsXPInstallManager();

        NS_DECL_ISUPPORTS
        NS_DECL_NSIXPILISTENER
        NS_DECL_NSIXPIDIALOGSERVICE
        NS_DECL_NSIXPINSTALLMANAGER
        NS_DECL_NSIOBSERVER
        NS_DECL_NSISTREAMLISTENER
        NS_DECL_NSIPROGRESSEVENTSINK
        NS_DECL_NSIREQUESTOBSERVER
        NS_DECL_NSIINTERFACEREQUESTOR
        NS_DECL_NSPICERTNOTIFICATION

        NS_IMETHOD InitManager(nsIScriptGlobalObject* aGlobalObject, nsXPITriggerInfo* aTrigger, PRUint32 aChromeType );

    private:
        nsresult    InitManagerInternal();
        NS_IMETHOD  DownloadNext();
        void        Shutdown();
        NS_IMETHOD  GetDestinationFile(nsString& url, nsILocalFile* *file);
        NS_IMETHOD  LoadParams(PRUint32 aCount, const PRUnichar** aPackageList, nsIDialogParamBlock** aParams);
        PRBool      ConfirmChromeInstall(nsIDOMWindowInternal* aParentWindow, const PRUnichar** aPackage);
        PRBool      TimeToUpdate(PRTime now);
        PRBool      VerifyHash(nsXPITriggerItem* aItem);
        PRInt32     GetIndexFromURL(const PRUnichar* aUrl);

        nsXPITriggerInfo*   mTriggers;
        nsXPITriggerItem*   mItem;
        PRTime              mLastUpdate;
        PRUint32            mNextItem;
        PRInt32             mNumJars;
        PRUint32            mChromeType;
        PRInt32             mContentLength;
        PRInt32             mOutstandingCertLoads;
        PRBool              mDialogOpen;
        PRBool              mCancelled;
        PRBool              mSelectChrome;
        PRBool              mNeedsShutdown;
  
        nsCOMPtr<nsIXPIProgressDialog>  mDlg;
        nsCOMPtr<nsISoftwareUpdate>     mInstallSvc;

        nsCOMPtr<nsIDOMWindowInternal>  mParentWindow;
};

#endif
