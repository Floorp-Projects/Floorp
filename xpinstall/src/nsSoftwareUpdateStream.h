/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _NS_SOFTWAREUPDATESTREAM_H__
#define _NS_SOFTWAREUPDATESTREAM_H__

#include "nsInstall.h"

#include "nscore.h"
#include "nsISupports.h"
#include "nsString.h"

#include "nsIURL.h"
#include "nsINetlibURL.h"
#include "nsINetService.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"

#include "nsISoftwareUpdate.h"

static NS_DEFINE_IID(kInetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kInetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kInetLibURLIID, NS_INETLIBURL_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);


extern "C" nsresult DownloadJar(nsInstallInfo *nextInstall);


class nsSoftwareUpdateListener : public nsIStreamListener
{

    public:
        NS_DECL_ISUPPORTS
    
        nsSoftwareUpdateListener(nsInstallInfo *nextInstall);

        NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* info);
        NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 Progress, PRUint32 ProgressMax);
        NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
        NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
        NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRUint32 length);
        NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult status, const PRUnichar* aMsg);
        
        


    protected:
        ~nsSoftwareUpdateListener();


    private:

          nsISoftwareUpdate *mSoftwareUpdate;
          PRFileDesc        *mOutFileDesc;
          nsInstallInfo     *mInstallInfo;
          PRInt32            mResult;
};

#endif