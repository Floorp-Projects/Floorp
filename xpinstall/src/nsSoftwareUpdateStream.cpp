/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsSoftwareUpdateStream.h"
#include "nscore.h"
#include "nsFileSpec.h"
#include "nsVector.h"

#include "nsISupports.h"
#include "nsIComponentManager.h"

#include "nsIURL.h"
#include "nsINetlibURL.h"
#include "nsINetService.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"

#include "nsISoftwareUpdate.h"
#include "nsSoftwareUpdateIIDs.h"


static NS_DEFINE_IID(kISoftwareUpdateIID, NS_ISOFTWAREUPDATE_IID);
static NS_DEFINE_IID(kSoftwareUpdateCID,  NS_SoftwareUpdate_CID);


nsSoftwareUpdateListener::nsSoftwareUpdateListener(nsInstallInfo *nextInstall)
{
    NS_INIT_REFCNT();
    
    mInstallInfo = nextInstall;
    mOutFileDesc = PR_Open(nsAutoCString(nextInstall->GetLocalFile()),  PR_CREATE_FILE | PR_RDWR, 0644);
    
    if(mOutFileDesc == NULL)
    {
        mResult = -1;
    };

    mResult = nsComponentManager::CreateInstance(  kSoftwareUpdateCID, 
                                                   nsnull,
                                                   kISoftwareUpdateIID,
                                                   (void**) &mSoftwareUpdate);
    
    if (mResult != NS_OK)
        return;

    nsIURL  *pURL  = nsnull;
    mResult = NS_NewURL(&pURL, nextInstall->GetFromURL());

    if (mResult != NS_OK)
        return;
 
    mResult = NS_OpenURL(pURL, this);
}

nsSoftwareUpdateListener::~nsSoftwareUpdateListener()
{    
    mSoftwareUpdate->Release();
}


NS_IMPL_ISUPPORTS( nsSoftwareUpdateListener, kIStreamListenerIID )

NS_IMETHODIMP
nsSoftwareUpdateListener::GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* info)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSoftwareUpdateListener::OnProgress( nsIURL* aURL,
                          PRUint32 Progress,
                          PRUint32 ProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSoftwareUpdateListener::OnStatus(nsIURL* aURL, 
                       const PRUnichar* aMsg)
{ 
  return NS_OK;
}

NS_IMETHODIMP
nsSoftwareUpdateListener::OnStartBinding(nsIURL* aURL, 
                             const char *aContentType)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSoftwareUpdateListener::OnStopBinding(nsIURL* aURL,
                                        nsresult status,
                                        const PRUnichar* aMsg)
{
    switch( status ) 
    {

        case NS_BINDING_SUCCEEDED:
                PR_Close(mOutFileDesc);
                // Add to the XPInstall Queue
                mSoftwareUpdate->InstallJar(mInstallInfo);
            break;

        case NS_BINDING_FAILED:
        case NS_BINDING_ABORTED:
            mResult = status;
            PR_Close(mOutFileDesc);
            break;

        default:
            mResult = NS_ERROR_ILLEGAL_VALUE;
    }

    return mResult;
}

#define BUF_SIZE 1024

NS_IMETHODIMP
nsSoftwareUpdateListener::OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRUint32 length)
{
    PRUint32 len;
    nsresult err;
    char buffer[BUF_SIZE];
    
    do 
    {
        err = pIStream->Read(buffer, BUF_SIZE, &len);
        
        if (err == NS_OK) 
        {
            if ( PR_Write(mOutFileDesc, buffer, len) == -1 )
            {
                /* Error */ 
                return -1;
            }
        }

    } while (len > 0 && err == NS_OK);

    return 0;
}