/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *     Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nscore.h"
#include "nsFileSpec.h"
#include "pratom.h"

#include "nsISupports.h"
#include "nsIServiceManager.h"

#include "nsIURL.h"
#include "nsIFileChannel.h"

#include "nsITransport.h"
#include "nsIFileTransportService.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "nsIInputStream.h"
#include "nsIFileStreams.h"
#include "nsIStreamListener.h"

#include "nsISoftwareUpdate.h"
#include "nsSoftwareUpdateIIDs.h"
#include "nsTextFormatter.h"

#include "nsXPITriggerInfo.h"
#include "nsXPInstallManager.h"
#include "nsInstallTrigger.h"
#include "nsInstallProgressDialog.h"
#include "nsInstallResources.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIProxyObjectManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsReadableUtils.h"
#include "nsProxiedService.h"
#include "nsIPromptService.h"
#include "nsIScriptGlobalObject.h"
#include "nsISupportsPrimitives.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#include "nsIEventQueueService.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

#define PROGRESS_BAR   'n'
#define BARBER_POLE    'u'
#define UPDATE_DLG(x)  (((x) - mLastUpdate) > 250000)

// Mac can't handle PRTime as integers, must go through this hell
inline PRBool nsXPInstallManager::TimeToUpdate(PRTime now)
{
	// XXX lets revisit this when dveditz gets back

	return PR_TRUE;
}

nsXPInstallManager::nsXPInstallManager()
  : mTriggers(0), mItem(0), mNextItem(0), mNumJars(0), 
    mFinalizing(PR_FALSE), mCancelled(PR_FALSE), mChromeType(0),
    mSelectChrome(PR_TRUE), mContentLength(0)
{
    NS_INIT_ISUPPORTS();

    // we need to own ourself because we have a longer
    // lifetime than the scriptlet that created us.
    NS_ADDREF_THIS();

    // initialize mLastUpdate to the current time
    mLastUpdate = PR_Now();

    // get the resourced xpinstall string bundle
    mStringBundle = nsnull;
    nsIStringBundleService *service;
    nsresult rv = nsServiceManager::GetService( kStringBundleServiceCID, 
                                       NS_GET_IID(nsIStringBundleService),
                                       (nsISupports**) &service );
    if (NS_SUCCEEDED(rv) && service)
    {
        rv = service->CreateBundle( XPINSTALL_BUNDLE_URL,
                                    getter_AddRefs(mStringBundle) );
        nsServiceManager::ReleaseService( kStringBundleServiceCID, service );
    }
}



nsXPInstallManager::~nsXPInstallManager()
{    
    if (mTriggers) 
        delete mTriggers;
}


NS_IMPL_THREADSAFE_ADDREF( nsXPInstallManager );
NS_IMPL_THREADSAFE_RELEASE( nsXPInstallManager );

NS_IMETHODIMP 
nsXPInstallManager::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (!aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(NS_GET_IID(nsIXPIListener)))
    *aInstancePtr = NS_STATIC_CAST(nsIXPIListener*,this);
  else if (aIID.Equals(NS_GET_IID(nsIStreamListener)))
    *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*,this);
  else if (aIID.Equals(NS_GET_IID(nsPIXPIManagerCallbacks)))
    *aInstancePtr = NS_STATIC_CAST(nsPIXPIManagerCallbacks*,this);
  else if (aIID.Equals(NS_GET_IID(nsIProgressEventSink)))
    *aInstancePtr = NS_STATIC_CAST(nsIProgressEventSink*,this);
  else if (aIID.Equals(NS_GET_IID(nsIInterfaceRequestor)))
    *aInstancePtr = NS_STATIC_CAST(nsIInterfaceRequestor*,this);
  else if (aIID.Equals(NS_GET_IID(nsISupports)))
    *aInstancePtr = NS_STATIC_CAST( nsISupports*, NS_STATIC_CAST(nsIXPIListener*,this));
  else
    *aInstancePtr = 0;

  nsresult status;
  if ( !*aInstancePtr )
    status = NS_NOINTERFACE;
  else 
  {
    NS_ADDREF_THIS();
    status = NS_OK;
  }

  return status;
}



NS_IMETHODIMP
nsXPInstallManager::InitManager(nsIScriptGlobalObject* aGlobalObject, nsXPITriggerInfo* aTriggers, PRUint32 aChromeType)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIDialogParamBlock> ioParamBlock;
    PRBool OKtoInstall = PR_FALSE;

    mTriggers = aTriggers;
    mChromeType = aChromeType;
    mNeedsShutdown = PR_TRUE;

    if ( !mTriggers || mTriggers->Size() == 0 )
    {
        rv = NS_ERROR_INVALID_POINTER;
        NS_RELEASE_THIS();
        return rv;
    }

    //-----------------------------------------------------
    // Create the nsIDialogParamBlock to pass the trigger
    // list to the dialog
    //-----------------------------------------------------
    rv = nsComponentManager::CreateInstance("@mozilla.org/embedcomp/dialogparam;1",
                                            nsnull,
                                            NS_GET_IID(nsIDialogParamBlock),
                                            getter_AddRefs(ioParamBlock));

    
    if ( NS_SUCCEEDED( rv ) ) 
    {
      
      LoadDialogWithNames(ioParamBlock);

      if (mChromeType == 0 || mChromeType > CHROME_SAFEMAX )
          OKtoInstall = ConfirmInstall(aGlobalObject, ioParamBlock);
      else
          OKtoInstall = ConfirmChromeInstall(aGlobalObject);
    }

    // --- create and open the progress dialog
    if (NS_SUCCEEDED(rv) && OKtoInstall)
    {
        nsInstallProgressDialog* dlg;
        nsCOMPtr<nsISupports>    Idlg;

        dlg = new nsInstallProgressDialog(this);

        if ( dlg )
        {
            rv = dlg->QueryInterface( NS_GET_IID(nsIXPIProgressDlg), 
                                      getter_AddRefs(Idlg) );
            if (NS_SUCCEEDED(rv))
            {
                nsCOMPtr<nsIProxyObjectManager> pmgr = 
                         do_GetService(kProxyObjectManagerCID, &rv);
                if (NS_SUCCEEDED(rv))
                {
                    rv = pmgr->GetProxyForObject( NS_UI_THREAD_EVENTQ, NS_GET_IID(nsIXPIProgressDlg),
                            Idlg, PROXY_SYNC | PROXY_ALWAYS, getter_AddRefs(mDlg) );

                }
                
                if (NS_SUCCEEDED(rv))
                {        
                    rv = mDlg->Open(ioParamBlock);
                }
            }
        }
        else
            rv = NS_ERROR_OUT_OF_MEMORY;
    }

    PRInt32 cbstatus = 0;  // callback status
    if (NS_SUCCEEDED(rv) && !OKtoInstall )
        cbstatus = nsInstall::USER_CANCELLED;
    else if (!NS_SUCCEEDED(rv))
        cbstatus = nsInstall::UNEXPECTED_ERROR;

    if ( cbstatus != 0 )
    {
        // --- inform callbacks of error
        for (PRUint32 i = 0; i < mTriggers->Size(); i++)
        {
            mTriggers->SendStatus( mTriggers->Get(i)->mURL.get(),
                                   cbstatus );
        }

        // -- must delete ourselves if not continuing
        NS_RELEASE_THIS();
    }

    return rv;
}

PRBool nsXPInstallManager::ConfirmInstall(nsIScriptGlobalObject* aGlobalObject, nsIDialogParamBlock* ioParamBlock)
{
    nsresult rv = NS_OK;
    PRBool result = PR_FALSE;

    nsCOMPtr<nsIDOMWindowInternal> parentWindow =
        do_QueryInterface(aGlobalObject);

    if (parentWindow)
    {
        nsCOMPtr<nsIDOMWindow> newWindow;
        nsCOMPtr<nsISupportsArray> array;

        nsCOMPtr<nsISupportsInterfacePointer> ifptr =
            do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        ifptr->SetData(ioParamBlock);
        ifptr->SetDataIID(&NS_GET_IID(nsIDialogParamBlock));

        rv = parentWindow->OpenDialog(NS_LITERAL_STRING("chrome://communicator/content/xpinstall/institems.xul"),
                                      NS_LITERAL_STRING("_blank"),
                                      NS_LITERAL_STRING("chrome,modal,titlebar,resizable"),
                                      ifptr, getter_AddRefs( newWindow));

        if (NS_SUCCEEDED(rv))
        {
            //Now get which button was pressed from the ParamBlock
            PRInt32 buttonPressed = 0;
            ioParamBlock->GetInt( 0, &buttonPressed );
            result = buttonPressed ? PR_FALSE : PR_TRUE;
        }
    }

    return result;
}

PRBool nsXPInstallManager::ConfirmChromeInstall(nsIScriptGlobalObject* aGlobalObject)
{
    nsXPITriggerItem* item = (nsXPITriggerItem*)mTriggers->Get(0);

    // get the dialog strings
    nsresult rv;
    nsXPIDLString applyNowText;
    nsXPIDLString confirmFormat;
    PRUnichar*    confirmText = nsnull;
    nsCOMPtr<nsIStringBundle> xpiBundle;
    nsCOMPtr<nsIStringBundleService> bundleSvc = 
             do_GetService( kStringBundleServiceCID, &rv );
    if (NS_SUCCEEDED(rv) && bundleSvc)
    {
        rv = bundleSvc->CreateBundle( XPINSTALL_BUNDLE_URL,
                                      getter_AddRefs(xpiBundle) );
        if (NS_SUCCEEDED(rv) && xpiBundle)
        {
            if ( mChromeType == CHROME_LOCALE )
            {
                xpiBundle->GetStringFromName(
                    NS_ConvertASCIItoUCS2("ApplyNowLocale").get(),
                    getter_Copies(applyNowText));
                xpiBundle->GetStringFromName(
                    NS_ConvertASCIItoUCS2("ConfirmLocale").get(),
                    getter_Copies(confirmFormat));
            }
            else
            {
                xpiBundle->GetStringFromName(
                    NS_ConvertASCIItoUCS2("ApplyNowSkin").get(),
                    getter_Copies(applyNowText));
                xpiBundle->GetStringFromName(
                    NS_ConvertASCIItoUCS2("ConfirmSkin").get(),
                    getter_Copies(confirmFormat));
            }

            confirmText = nsTextFormatter::smprintf(confirmFormat,
                                                    item->mName.get(),
                                                    item->mURL.get());
        }
    }


    // confirmation dialog
    PRBool bInstall = PR_FALSE;
    if (confirmText)
    {
        nsCOMPtr<nsIDOMWindowInternal> parentWindow(do_QueryInterface(aGlobalObject));
        if (parentWindow)
        {
            nsCOMPtr<nsIPromptService> dlgService(do_GetService("@mozilla.org/embedcomp/prompt-service;1"));
            if (dlgService)
            {
                rv = dlgService->ConfirmCheck( parentWindow,
                                               nsnull,
                                               confirmText, 
                                               applyNowText, 
                                               &mSelectChrome, 
                                               &bInstall );
            }
        }
    }

    return bInstall;
}


NS_IMETHODIMP nsXPInstallManager::DialogOpened(nsISupports* aWindow)
{
  nsresult rv;

  mParentWindow = do_QueryInterface(aWindow, &rv);
  DownloadNext();
  
  return rv;
}


NS_IMETHODIMP nsXPInstallManager::DownloadNext()
{
    nsresult rv;

    mContentLength = 0;
    if (mCancelled)
    {
        Shutdown();
        return NS_OK; // don't try to download anymore
    }

    if ( mNextItem < mTriggers->Size() )
    {
        // download the next item in list

        mItem = (nsXPITriggerItem*)mTriggers->Get(mNextItem++);

        NS_ASSERTION( mItem, "bogus Trigger slipped through" );
        NS_ASSERTION( mItem->mURL.Length() > 0, "bogus trigger");
        if ( !mItem || mItem->mURL.Length() == 0 )
        {
            // XXX serious problem with trigger! try to carry on
            rv = DownloadNext();
        }
        else if ( mItem->IsFileURL() && mChromeType == 0 )
        {
            
            nsCOMPtr<nsIURI> pURL;
            rv = NS_NewURI(getter_AddRefs(pURL), mItem->mURL);
            
            if (NS_SUCCEEDED(rv)) 
            {
                nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(pURL);
                if (!fileURL)
                    return NS_ERROR_NULL_POINTER;
                
                nsCOMPtr<nsIFile> localFile;
                rv = fileURL->GetFile(getter_AddRefs(localFile));

                if (NS_FAILED(rv))
                {
                    // serious problem with trigger! try to carry on
                    mTriggers->SendStatus( mItem->mURL.get(), 
                                           nsInstall::DOWNLOAD_ERROR );
                    mItem->mFile = 0;
                }
                else
                {
                    mItem->mFile = do_QueryInterface(localFile);
                    rv = DownloadNext();
                }
            }
        }
        else
        {
            // We have one to download -- figure out the destination file name
            rv = GetDestinationFile(mItem->mURL, getter_AddRefs(mItem->mFile));
            if (NS_SUCCEEDED(rv))
            {
                 // --- start the download
                nsCOMPtr<nsIURI> pURL;
                rv = NS_NewURI(getter_AddRefs(pURL), mItem->mURL);
                
                if (NS_SUCCEEDED(rv)) 
                {
                    nsCOMPtr<nsIChannel> channel;
                
                    rv = NS_OpenURI(getter_AddRefs(channel), pURL, nsnull, nsnull, this);
                
                    if (NS_SUCCEEDED(rv))
                    {
                        rv = channel->AsyncOpen(this, nsnull);
                    }
                }
            }

            if (NS_FAILED(rv))
            {
                // announce failure
                mTriggers->SendStatus( mItem->mURL.get(), 
                                       nsInstall::DOWNLOAD_ERROR );
                mItem->mFile = 0;
                // carry on with the next one
                rv = DownloadNext();
            }
        }
    }
    else
    {
        // all downloaded, queue them for installation

        // can't cancel from here on cause we can't undo installs in a multitrigger
        if (mDlg)
            mDlg->StartInstallPhase();

        nsCOMPtr<nsISoftwareUpdate> softupdate = 
                 do_GetService(nsSoftwareUpdate::GetCID(), &rv);
        if (NS_SUCCEEDED(rv))
        {
            for (PRUint32 i = 0; i < mTriggers->Size(); ++i)
            {
                mItem = (nsXPITriggerItem*)mTriggers->Get(i);
                if ( mItem && mItem->mFile )
                {
                    // We've got one to install; increment count first so we
                    // don't have to worry about thread timing.
                    PR_AtomicIncrement(&mNumJars);

                    if ( mChromeType == 0 ) {
                        rv = softupdate->InstallJar(mItem->mFile,
                                                mItem->mURL.get(),
                                                mItem->mArguments.get(),
                                                mParentWindow,
                                                mItem->mFlags,
                                                this );
                    }
                    else {
                        rv = softupdate->InstallChrome(mChromeType,
                                                mItem->mFile,
                                                mItem->mURL.get(),
                                                mItem->mName.get(),
                                                mSelectChrome,
                                                this );
                    }

                    if (NS_FAILED(rv))
                    {
                        // it failed so remove it from the count
                        PR_AtomicDecrement(&mNumJars);
                        // send the error status to any trigger callback
                        mTriggers->SendStatus( mItem->mURL.get(),
                                               nsInstall::UNEXPECTED_ERROR );
                    }
                }
            }
        }
        else
        {
            ; // XXX gotta clean up all those files...
        }

        if ( mNumJars == 0 )
        {
            // We must clean ourself up now -- we won't be called back
            Shutdown();
        }
    }

    return rv;
}

NS_IMETHODIMP
nsXPInstallManager::CancelInstall()
{
  mCancelled = PR_TRUE; // set by dialog thread
  return NS_OK;
}

void nsXPInstallManager::Shutdown()
{
    if (mDlg)
    {
        mDlg->Close();
        mDlg = nsnull;
    }
    if (mNeedsShutdown)
    {
        mNeedsShutdown = PR_FALSE;
        // Clean up downloaded files, regular XPInstall only not chrome installs
        nsXPITriggerItem* item;
        nsCOMPtr<nsIFile> tmpSpec;
        if ( mChromeType == 0 )
        {
            for (PRUint32 i = 0; i < mTriggers->Size(); i++ )
            {
                item = NS_STATIC_CAST(nsXPITriggerItem*, mTriggers->Get(i));
                if ( item && item->mFile && !item->IsFileURL() )
                    item->mFile->Remove(PR_FALSE);
            }
        }

        NS_RELEASE_THIS();
    }
}

/*-----------------------------------------------------
** LoadDialogWithNames loads the param block for the 
** javascript/xul dialog with the list of modules to 
** be installed and their source locations
**----------------------------------------------------*/
void nsXPInstallManager::LoadDialogWithNames(nsIDialogParamBlock* ioParamBlock)
{

    nsXPITriggerItem *triggerItem;
    nsString moduleName, URL;
    PRInt32 offset = 0, numberOfDialogTreeElements = 0;
    PRUint32 i=0, paramIndex=0;

    //Must multiply the number of triggers by 2 because
    //the dialog contains the name and url as 2 separate elements
    numberOfDialogTreeElements = (mTriggers->Size() * 2);     
    ioParamBlock->SetNumberStrings(numberOfDialogTreeElements);

    ioParamBlock->SetInt(0,2); //set the Ok and Cancel buttons
    ioParamBlock->SetInt(1,numberOfDialogTreeElements);

    for (i=0; i < mTriggers->Size(); i++)
    {
        triggerItem = mTriggers->Get(i);
    
        //Check to see if this trigger item has a pretty name
        if(!(moduleName = triggerItem->mName).IsEmpty())
        {
            ioParamBlock->SetString(paramIndex, moduleName.get());
            paramIndex++;
            URL = triggerItem->mURL;
            offset = URL.RFind("/");
            if (offset != -1)
            {
                URL.Cut(offset + 1, URL.Length() - 1);
                ioParamBlock->SetString(paramIndex, URL.get());
            }
            paramIndex++;
        }
        else
        {
            //triggerItem does not have a pretty name so parse the url for the file name
            //then use that as the pretty name
            moduleName = triggerItem->mURL;
            URL = triggerItem->mURL;
            offset = moduleName.RFind("/");
            if (offset != -1)
            {
                moduleName.Cut(0, offset + 1);
                ioParamBlock->SetString(paramIndex, moduleName.get());
                paramIndex++;
                URL.Cut(offset + 1, URL.Length() - 1); 
                ioParamBlock->SetString(paramIndex, URL.get());
            }
            paramIndex++;
        }
    }
}



NS_IMETHODIMP
nsXPInstallManager::GetDestinationFile(nsString& url, nsILocalFile* *file)
{
    NS_ENSURE_ARG_POINTER(file);

    nsresult rv;
    nsString leaf;

    PRInt32 pos = url.RFindChar('/');
    url.Mid( leaf, pos+1, url.Length() );

    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);

    if (mChromeType == 0 )
    {
        // a regular XPInstall, not chrome
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsILocalFile> temp;
            directoryService->Get(NS_OS_TEMP_DIR,
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(temp));
            temp->AppendUnicode(leaf.get());
            MakeUnique(temp);
            *file = temp;
            NS_IF_ADDREF(*file);
        }
    }
    else
    {
        // a chrome install, download straight to final destination
        if (NS_SUCCEEDED(rv)) // Getting directoryService
        {
            nsCOMPtr<nsILocalFile>  userChrome;

            // Get the user's Chrome directory, create if necessary
                        
            rv = directoryService->Get(NS_APP_USER_CHROME_DIR,
                                       NS_GET_IID(nsIFile), 
                            getter_AddRefs(userChrome));

            NS_ASSERTION(NS_SUCCEEDED(rv) && userChrome,
                         "App_UserChromeDirectory not defined!");
            if (NS_SUCCEEDED(rv))
            {
                PRBool exists;
                rv = userChrome->Exists(&exists);
                if (NS_SUCCEEDED(rv) && !exists)
                {
                    rv = userChrome->Create(nsIFile::DIRECTORY_TYPE, 0775);
                }

                if (NS_SUCCEEDED(rv))
                {
                    userChrome->AppendUnicode(leaf.get());
                    MakeUnique(userChrome);
                    *file = userChrome;
                    NS_IF_ADDREF(*file);
                }
            }
        }
    }
    return rv;
}



NS_IMETHODIMP
nsXPInstallManager::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
    nsresult rv = NS_ERROR_FAILURE;

    NS_ASSERTION( mItem && mItem->mFile, "XPIMgr::OnStartRequest bad state");
    if ( mItem && mItem->mFile )
    {
        NS_ASSERTION( !mItem->mOutStream, "Received double OnStartRequest from Necko");

        NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
        nsCOMPtr<nsIFileTransportService> fts = 
                 do_GetService( kFileTransportServiceCID, &rv );

        if (NS_SUCCEEDED(rv) && !mItem->mOutStream)
        {
            nsCOMPtr<nsITransport> outChannel;
            
            rv = fts->CreateTransport(mItem->mFile, 
                                      PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                      0664, 
                                      getter_AddRefs( outChannel));
                                      
            if (NS_SUCCEEDED(rv)) 
            {                          
                // Open output stream.
                rv = outChannel->OpenOutputStream(0, -1, 0,  getter_AddRefs( mItem->mOutStream ) );
            }
        }
    }
    return rv;
}

NS_IMETHODIMP
nsXPInstallManager::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                  nsresult status)
{
    nsresult rv;

    switch( status ) 
    {

        case NS_BINDING_SUCCEEDED:
            rv = NS_OK;
            break;

        case NS_BINDING_FAILED:
        case NS_BINDING_ABORTED:
            rv = status;
            // XXX need to note failure, both to send back status
            // to the callback, and also so we don't try to install
            // this probably corrupt file.
            break;

        default:
            rv = NS_ERROR_ILLEGAL_VALUE;
    }

    NS_ASSERTION( mItem, "Bad state in XPIManager");
    NS_ASSERTION( mItem->mOutStream, "XPIManager: output stream doesn't exist");
    if ( mItem && mItem->mOutStream )
    {
        mItem->mOutStream->Close();
        mItem->mOutStream = nsnull;
    }

    if (!NS_SUCCEEDED(rv))
    {
        if ( mItem->mFile )
        {
            PRBool flagExists;
            nsFileSpec fspec;
            nsresult rv2 ;
            rv2 = mItem->mFile->Exists(&flagExists);
            if (NS_SUCCEEDED(rv2) && flagExists)
                mItem->mFile->Remove(PR_FALSE);

            mItem->mFile = 0;
        }

        mTriggers->SendStatus( mItem->mURL.get(),
                               nsInstall::DOWNLOAD_ERROR );
    }

    DownloadNext();
    return rv;
}


NS_IMETHODIMP
nsXPInstallManager::OnDataAvailable(nsIRequest* request, nsISupports *ctxt, 
                                    nsIInputStream *pIStream,
                                    PRUint32 sourceOffset, 
                                    PRUint32 length)
{
    PRUint32 amt;
    nsresult err;
    char buffer[8*1024];
    PRUint32 writeCount;

    //printf("nsXPInstallManager::OnDataAvailable\n");
    
    if (mCancelled)
    {
        // returning an error will stop the download. We may get extra
        // OnData calls if they were already queued so beware
        if (mDlg)
        {
            mDlg->Close();
            mDlg = nsnull;
        }
        return NS_ERROR_FAILURE;
    }

    do 
    {
        err = pIStream->Read(buffer, sizeof(buffer), &amt);
        if (amt == 0) break;
        if (NS_FAILED(err)) 
        {
            //printf("pIStream->Read Failed!  %d", err);   
            return err;
        }
        err = mItem->mOutStream->Write( buffer, amt, &writeCount);
        if (NS_FAILED(err) || writeCount != amt) 
        {
            return NS_ERROR_FAILURE;
        }
        length -= amt;
    } while (length > 0);

    return NS_OK;
}

NS_IMETHODIMP 
nsXPInstallManager::OnProgress(nsIRequest* request, nsISupports *ctxt, PRUint32 aProgress, PRUint32 aProgressMax)
{
    nsresult rv = NS_OK;
    
    PRTime now = PR_Now();
    if (mDlg && !mCancelled && TimeToUpdate(now))
    {
        if (mContentLength < 1) {
            nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
            NS_ASSERTION(channel, "should have a channel");
            rv = channel->GetContentLength(&mContentLength);
            if (NS_FAILED(rv)) return rv;
        }
        mLastUpdate = now;
        rv = mDlg->SetProgress(aProgress, mContentLength, PROGRESS_BAR);
    }

    return rv;
}

NS_IMETHODIMP 
nsXPInstallManager::OnStatus(nsIRequest* request, nsISupports *ctxt, 
                             nsresult aStatus, const PRUnichar *aStatusArg)
{
    nsresult rv;
    PRTime now = PR_Now();
    if (mDlg && !mCancelled && TimeToUpdate(now))
    {
        mLastUpdate = now;
        nsCOMPtr<nsIStringBundleService> sbs = do_GetService(kStringBundleServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
        nsXPIDLString msg;
        rv = sbs->FormatStatusMessage(aStatus, aStatusArg, getter_Copies(msg));
        if (NS_FAILED(rv)) return rv;
        return mDlg->SetActionText(msg);
    }
    else
        return NS_OK;
}

// nsIInterfaceRequestor method
NS_IMETHODIMP 
nsXPInstallManager::GetInterface(const nsIID & eventSinkIID, void* *_retval)
{
    return QueryInterface(eventSinkIID, (void**)_retval);
}

// IXPIListener methods

NS_IMETHODIMP 
nsXPInstallManager::BeforeJavascriptEvaluation(const PRUnichar *URL)
{
    nsresult rv = NS_OK;

    mFinalizing = PR_FALSE;
    if (mDlg)
    {
        mDlg->SetProgress( 0, 0, BARBER_POLE ); // turn on the barber pole

        PRUnichar tmp[] = { '\0' };
        mDlg->SetActionText(tmp);
    }

    return rv;
}

NS_IMETHODIMP 
nsXPInstallManager::AfterJavascriptEvaluation(const PRUnichar *URL)
{
    PR_AtomicDecrement( &mNumJars );
    if ( mNumJars == 0 )
        Shutdown();

    return NS_OK;
}

NS_IMETHODIMP 
nsXPInstallManager::InstallStarted(const PRUnichar *URL, const PRUnichar *UIPackageName)
{
    if (!mDlg)
        return NS_OK;

    mDlg->SetActionText(nsnull);
    return mDlg->SetHeading( nsString(UIPackageName).get() );
}

NS_IMETHODIMP 
nsXPInstallManager::ItemScheduled(const PRUnichar *message)
{
    PRTime now = PR_Now();
    if (mDlg && TimeToUpdate(now))
    {
        mLastUpdate = now;
        return mDlg->SetActionText( nsString(message).get() );
    }
    else
        return NS_OK;
}

NS_IMETHODIMP 
nsXPInstallManager::FinalizeProgress(const PRUnichar *message, PRInt32 itemNum, PRInt32 totNum)
{
    if (!mDlg)
        return NS_OK;

    nsresult rv = NS_OK;

    if (!mFinalizing)
    {
        mFinalizing = PR_TRUE;
        if (mStringBundle)
        {
            nsString rsrcName; rsrcName.AssignWithConversion("FinishingInstallMsg");
            const PRUnichar* ucRsrcName = rsrcName.get();
            PRUnichar* ucRsrcVal = nsnull;
            rv = mStringBundle->GetStringFromName(ucRsrcName, &ucRsrcVal);
            if (NS_SUCCEEDED(rv) && ucRsrcVal)
            {
                rv = mDlg->SetActionText( ucRsrcVal );
                nsCRT::free(ucRsrcVal);
            }
        }
    }

    PRTime now = PR_Now();
    if (TimeToUpdate(now))
    {
        mLastUpdate = now;
        rv = mDlg->SetProgress( itemNum, totNum, PROGRESS_BAR );
    }

    return rv;
}

NS_IMETHODIMP 
nsXPInstallManager::FinalStatus(const PRUnichar *URL, PRInt32 status)
{
    mTriggers->SendStatus( URL, status );
    return NS_OK;
}

NS_IMETHODIMP 
nsXPInstallManager::LogComment(const PRUnichar* comment)
{
    return NS_OK;
}

