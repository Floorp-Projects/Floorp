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
#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateStream.h"

#include "nscore.h"
#include "nsISupports.h"

#include "nsIURL.h"
#include "nsINetlibURL.h"
#include "nsINetService.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"



nsresult
su_DownloadJar(const nsString& aUrlString, const nsString& aSaveLocationString)
{
    long            result=0;
    nsIURL          *pURL;
    char*            locationCString;

    nsSoftwareUpdateListener *aListener;

    ////////////////////////////
    // Create the URL object...
    ////////////////////////////

    pURL = NULL;

    result = NS_NewURL(&pURL, aUrlString);

    if (result != NS_OK) 
    {
        return result;
    }
    
    aListener = new nsSoftwareUpdateListener();
    
    
    locationCString = aSaveLocationString.ToNewCString();
    aListener->SetListenerInfo(locationCString);
    delete [] locationCString;

    result = NS_OpenURL(pURL, aListener);

    /* If the open failed... */
    if (NS_OK != result) 
    {
        return result;
    }

}


nsSoftwareUpdateListener::nsSoftwareUpdateListener()
{
    NS_INIT_REFCNT();
}

nsSoftwareUpdateListener::~nsSoftwareUpdateListener()
{    
    if (mOutFileDesc != NULL)
        PR_Close(mOutFileDesc);
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
    nsresult        result = NS_OK;

    switch( status ) 
    {

        case NS_BINDING_SUCCEEDED:
                /* Extract the Installer Script from the jar */

                /* Open the script up */
            
                PR_Close(mOutFileDesc);
            break;

        case NS_BINDING_FAILED:
        case NS_BINDING_ABORTED:
                PR_Close(mOutFileDesc);
            break;

        default:
            result = NS_ERROR_ILLEGAL_VALUE;
    }

    return result;
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
        err = pIStream->Read(buffer, 0, BUF_SIZE, &len);
        
        if (err == NS_OK) 
        {
            if( PR_Write(mOutFileDesc, buffer, len) == -1 )
            {
                /* Error */ 
                return -1;
            }
        }

    } while (len > 0 && err == NS_OK);

    return 0;
}

NS_IMETHODIMP
nsSoftwareUpdateListener::SetListenerInfo(char* fileName)
{
    mOutFileDesc = PR_Open(fileName,  PR_CREATE_FILE | PR_RDWR, 0644);
    
    if(mOutFileDesc == NULL)
    {
        return -1;
    };


    return NS_OK;
}
