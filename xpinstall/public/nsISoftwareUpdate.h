/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *     Daniel Veditz <dveditz@netscape.com>
 *     Douglas Turner <dougt@netscape.com>
 */

#ifndef nsISoftwareUpdate_h__
#define nsISoftwareUpdate_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsString.h"
#include "nsIFileSpec.h"

#include "nsIXPINotifier.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"

#define NS_IXPINSTALLCOMPONENT_CONTRACTID  "@mozilla.org/xpinstall;1"
#define NS_IXPINSTALLCOMPONENT_CLASSNAME "Mozilla XPInstall Component"

#define XPINSTALL_ENABLE_PREF            "xpinstall.enabled"
#define XPINSTALL_MANUAL_CONFIRM         "xpinstall.manual_confirm"
#define XPINSTALL_NOTIFICATIONS_ENABLE   "xpinstall.notifications.enabled"  //TODO: this needs to be fixed at some point.
#define XPINSTALL_NOTIFICATIONS_LASTDATE "xpinstall.notifications.lastDate"  //TODO: this needs to be fixed at some point.
#define XPINSTALL_NOTIFICATIONS_INTERVAL "xpinstall.notifications.interval"  //TODO: this needs to be fixed at some point.


#define XPI_NO_NEW_THREAD   0x1000

#define NS_ISOFTWAREUPDATE_IID                   \
{ 0x18c2f992, 									 \
  0xb09f, 										 \
  0x11d2,						                 \
 {0xbc, 0xde, 0x00, 0x80, 0x5f, 0x0e, 0x13, 0x53}\
}


class nsISoftwareUpdate : public nsISupports
{
    public:
            NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISOFTWAREUPDATE_IID)

            NS_IMETHOD InstallJar(nsIFile* localFile,
                                  const PRUnichar* URL,
                                  const PRUnichar* arguments,
                                  PRUint32 flags,
                                  nsIXPIListener* aListener = 0) = 0;

            NS_IMETHOD InstallChrome(PRUint32 aType,
                                     nsIFile* aFile,
                                     const PRUnichar* URL,
                                     const PRUnichar* aName,
                                     PRBool aSelect,
                                     nsIXPIListener* aListener = 0) = 0;

            NS_IMETHOD RegisterListener(nsIXPIListener *aListener) = 0;

            /* FIX: these should be in a private interface */
            NS_IMETHOD InstallJarCallBack()                   = 0;
            NS_IMETHOD GetMasterListener(nsIXPIListener **aListener) = 0;
            NS_IMETHOD SetActiveListener(nsIXPIListener *aListener) = 0;
            NS_IMETHOD StartupTasks( PRBool* outAutoreg ) = 0;
};


/**
 * Request that an autoreg be performed at next startup. (Used
 * internally by XPI.)  This basically drops a files next to the
 * application.  Next time XPCOM sees this file, it will cause
 * an autoreg, then delete this file.
 */
static void
NS_SoftwareUpdateRequestAutoReg()
{
  nsresult rv;
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                         getter_AddRefs(file));
  
  if (!file) {
    NS_WARNING("Getting NS_XPCOM_CURRENT_PROCESS_DIR failed");
    return;
  }
  
  file->AppendNative(nsDependentCString(".autoreg"));
  
  rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0666);
  
  if (NS_FAILED(rv)) {
    NS_WARNING("creating file failed");
    return;
  }
}

#endif // nsISoftwareUpdate_h__

