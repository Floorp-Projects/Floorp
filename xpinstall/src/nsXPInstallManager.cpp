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
#include "nsIDOMWindow.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsProxiedService.h"
#include "nsIAppShellComponentImpl.h"
#include "nsINetSupportDialogService.h"
#include "nsIPrompt.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID );
static NS_DEFINE_IID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kDialogParamBlockCID, NS_DialogParamBlock_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);

#include "nsIEventQueueService.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

#define PROGRESS_BAR   'n'
#define BARBER_POLE    'u'
#define UPDATE_DLG(x)  (((x) - mLastUpdate) > 250000)

// Mac can't handle PRTime as integers, must go through this hell
inline PRBool nsXPInstallManager::TimeToUpdate(PRTime now)
{
    PRTime  diff;
    LL_SUB(diff, now, mLastUpdate);

    PRInt32 interval;
    LL_L2I(interval, diff);
    return (interval > 250000);
}

nsXPInstallManager::nsXPInstallManager()
  : mTriggers(0), mItem(0), mNextItem(0), mNumJars(0), 
    mFinalizing(PR_FALSE), mCancelled(PR_FALSE), mChromeType(0),
    mSelectChrome(PR_TRUE), mContentLength(0), mLastUpdate(LL_ZERO)
{
    NS_INIT_ISUPPORTS();

    // we need to own ourself because we have a longer
    // lifetime than the scriptlet that created us.
    NS_ADDREF_THIS();

    // get the resourced xpinstall string bundle
    mStringBundle = nsnull;
    nsIStringBundleService *service;
    nsresult rv = nsServiceManager::GetService( kStringBundleServiceCID, 
                                       NS_GET_IID(nsIStringBundleService),
                                       (nsISupports**) &service );
    if (NS_SUCCEEDED(rv) && service)
    {
        nsILocale* locale = nsnull;
        rv = service->CreateBundle( XPINSTALL_BUNDLE_URL, locale,
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
nsXPInstallManager::InitManager(nsXPITriggerInfo* aTriggers, PRUint32 aChromeType)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIDialogParamBlock> ioParamBlock;
    PRBool OKtoInstall = PR_FALSE;

    mTriggers = aTriggers;
    mChromeType = aChromeType;

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
    rv = nsComponentManager::CreateInstance(kDialogParamBlockCID,
                                            nsnull,
                                            NS_GET_IID(nsIDialogParamBlock),
                                            getter_AddRefs(ioParamBlock));

    
    if ( NS_SUCCEEDED( rv ) ) 
    {
      
      LoadDialogWithNames(ioParamBlock);

      if (mChromeType == 0 || mChromeType > CHROME_SAFEMAX )
          OKtoInstall = ConfirmInstall(ioParamBlock);
      else
          OKtoInstall = ConfirmChromeInstall();
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
                NS_WITH_SERVICE( nsIProxyObjectManager, pmgr, kProxyObjectManagerCID, &rv);
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
            mTriggers->SendStatus( mTriggers->Get(i)->mURL.GetUnicode(),
                                   cbstatus );
        }

        // -- must delete ourselves if not continuing
        NS_RELEASE_THIS();
    }

    return rv;
}

PRBool nsXPInstallManager::ConfirmInstall(nsIDialogParamBlock* ioParamBlock)
{
    nsresult rv = NS_OK;
    PRBool result = PR_FALSE;

    // create a window and pass the JS args to it.
    NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv );
    if ( NS_SUCCEEDED( rv ) ) 
    {
      nsCOMPtr<nsIDOMWindow> hiddenWindow;
      JSContext* jsContext;
      rv = appShell->GetHiddenWindowAndJSContext( getter_AddRefs(hiddenWindow), &jsContext);
      if (NS_SUCCEEDED(rv))
      {
        void* stackPtr;
        jsval *argv = JS_PushArguments( jsContext,
                                        &stackPtr,
                                        "sss%ip",
                                        "chrome://communicator/content/xpinstall/institems.xul",
                                        "_blank",
                                        "chrome,modal",
                                        (const nsIID*)(&NS_GET_IID(nsIDialogParamBlock)),
                                        (nsISupports*)ioParamBlock);

        if (argv)
        {
          nsCOMPtr<nsIDOMWindow> newWindow;
          rv = hiddenWindow->OpenDialog( jsContext,
                                         argv,
                                         4,
                                         getter_AddRefs( newWindow));
          if (NS_SUCCEEDED(rv))
          {
            JS_PopArguments( jsContext, stackPtr);

            //Now get which button was pressed from the ParamBlock
            PRInt32 buttonPressed = 0;
            ioParamBlock->GetInt( 0, &buttonPressed );
            result = buttonPressed ? PR_FALSE : PR_TRUE;
          }
        }
      }
    }

    return result;
}

PRBool nsXPInstallManager::ConfirmChromeInstall()
{
    nsXPITriggerItem* item = (nsXPITriggerItem*)mTriggers->Get(0);

    // get the dialog strings
    nsresult rv;
    nsXPIDLString applyNowText;
    nsXPIDLString confirmFormat;
    PRUnichar*    confirmText = nsnull;
    nsCOMPtr<nsIStringBundle> xpiBundle;
    NS_WITH_SERVICE( nsIStringBundleService, bundleSvc,
                     kStringBundleServiceCID, &rv );
    if (NS_SUCCEEDED(rv) && bundleSvc)
    {
        rv = bundleSvc->CreateBundle( XPINSTALL_BUNDLE_URL, nsnull,
                                      getter_AddRefs(xpiBundle) );
        if (NS_SUCCEEDED(rv) && xpiBundle)
        {
            if ( mChromeType == CHROME_LOCALE )
            {
                xpiBundle->GetStringFromName(
                    NS_ConvertASCIItoUCS2("ApplyNowLocale").GetUnicode(),
                    getter_Copies(applyNowText));
                xpiBundle->GetStringFromName(
                    NS_ConvertASCIItoUCS2("ConfirmLocale").GetUnicode(),
                    getter_Copies(confirmFormat));
            }
            else
            {
                xpiBundle->GetStringFromName(
                    NS_ConvertASCIItoUCS2("ApplyNowSkin").GetUnicode(),
                    getter_Copies(applyNowText));
                xpiBundle->GetStringFromName(
                    NS_ConvertASCIItoUCS2("ConfirmSkin").GetUnicode(),
                    getter_Copies(confirmFormat));
            }

            confirmText = nsTextFormatter::smprintf(confirmFormat,
                                                    item->mName.GetUnicode(),
                                                    item->mURL.GetUnicode());
        }
    }


    // confirmation dialog
    PRBool bInstall = PR_FALSE;
    if (confirmText)
    {
        NS_WITH_SERVICE(nsIPrompt, dlgService, kNetSupportDialogCID, &rv);
        if (NS_SUCCEEDED(rv))
        {
            rv = dlgService->ConfirmCheck( nsnull,
                                           confirmText, 
                                           applyNowText, 
                                           &mSelectChrome, 
                                           &bInstall );
        }
    }

    return bInstall;
}


NS_IMETHODIMP nsXPInstallManager::DialogOpened(nsISupports* aWindow)
{
  nsresult rv;
  nsCOMPtr<nsIDOMWindow> win = do_QueryInterface(aWindow, &rv);
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
                    mTriggers->SendStatus( mItem->mURL.GetUnicode(), 
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
                        rv = channel->AsyncRead(this, nsnull);
                    }
                }
            }

            if (NS_FAILED(rv))
            {
                // announce failure
                mTriggers->SendStatus( mItem->mURL.GetUnicode(), 
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

        NS_WITH_SERVICE(nsISoftwareUpdate, softupdate, nsSoftwareUpdate::GetCID(), &rv);
        if (NS_SUCCEEDED(rv))
        {
            for (PRUint32 i = 0; i < mTriggers->Size(); ++i)
            {
                mItem = (nsXPITriggerItem*)mTriggers->Get(i);
                if ( mItem && mItem->mFile )
                {
                    if ( mChromeType == 0 ) {
                        rv = softupdate->InstallJar(mItem->mFile,
                                                mItem->mURL.GetUnicode(),
                                                mItem->mArguments.GetUnicode(),
                                                mItem->mFlags,
                                                this );
                    }
                    else {
                        rv = softupdate->InstallChrome(mChromeType,
                                                mItem->mFile,
                                                mItem->mURL.GetUnicode(),
                                                mItem->mName.GetUnicode(),
                                                mSelectChrome,
                                                this );
                    }

                    if (NS_SUCCEEDED(rv))
                        PR_AtomicIncrement(&mNumJars);
                    else
                        mTriggers->SendStatus( mItem->mURL.GetUnicode(),
                                               nsInstall::UNEXPECTED_ERROR );
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
        mDlg->Close();

    // Clean up downloaded files, regular XPInstall only not chrome installs
    nsXPITriggerItem* item;
    nsCOMPtr<nsIFile> tmpSpec;
    if ( mChromeType == 0 )
    {
        for (PRUint32 i = 0; i < mTriggers->Size(); i++ )
        {
            item = NS_STATIC_CAST(nsXPITriggerItem*, mTriggers->Get(i));
            if ( item && item->mFile && !item->IsFileURL() )
                item->mFile->Delete(PR_FALSE);
        }
    }

    NS_RELEASE_THIS();
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
            ioParamBlock->SetString(paramIndex, moduleName.ToNewUnicode());
            paramIndex++;
            URL = triggerItem->mURL;
            offset = URL.RFind("/");
            if (offset != -1)
            {
                URL.Cut(offset + 1, URL.Length() - 1);
                ioParamBlock->SetString(paramIndex, URL.ToNewUnicode());
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
                ioParamBlock->SetString(paramIndex, moduleName.ToNewUnicode());
                paramIndex++;
                URL.Cut(offset + 1, URL.Length() - 1); 
                ioParamBlock->SetString(paramIndex, URL.ToNewUnicode());
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

    NS_WITH_SERVICE(nsIProperties, directoryService,
                    NS_DIRECTORY_SERVICE_PROGID, &rv);

    if (mChromeType == 0 )
    {
        // a regular XPInstall, not chrome
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsILocalFile> temp;
            directoryService->Get(NS_OS_TEMP_DIR,
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(temp));
            temp->AppendUnicode(leaf.GetUnicode());
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
                    userChrome->AppendUnicode(leaf.GetUnicode());
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
nsXPInstallManager::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
{
    nsresult rv = NS_ERROR_FAILURE;

    NS_ASSERTION( mItem && mItem->mFile, "XPIMgr::OnStartRequest bad state");
    if ( mItem && mItem->mFile )
    {
        NS_ASSERTION( !mItem->mOutStream, "Received double OnStartRequest from Necko");

        NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
        NS_WITH_SERVICE( nsIFileTransportService, fts, kFileTransportServiceCID, &rv );

        if (NS_SUCCEEDED(rv) && !mItem->mOutStream)
        {
            nsCOMPtr<nsIChannel> outChannel;
            
            rv = fts->CreateTransport(mItem->mFile, 
                                      PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                      0664, 
                                      getter_AddRefs( outChannel));
                                      
            if (NS_SUCCEEDED(rv)) 
            {                          
                // Open output stream.
                rv = outChannel->OpenOutputStream( getter_AddRefs( mItem->mOutStream ) );
            }
        }
    }
    return rv;
}

NS_IMETHODIMP
nsXPInstallManager::OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
                                  nsresult status, const PRUnichar *errorMsg)
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
                mItem->mFile->Delete(PR_FALSE);

            mItem->mFile = 0;
        }

        mTriggers->SendStatus( mItem->mURL.GetUnicode(),
                               nsInstall::DOWNLOAD_ERROR );
    }

    DownloadNext();
    return rv;
}


NS_IMETHODIMP
nsXPInstallManager::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt, 
                                    nsIInputStream *pIStream,
                                    PRUint32 sourceOffset, 
                                    PRUint32 length)
{
    PRUint32 amt;
    //PRInt32  result;
    nsresult err;
    char buffer[8*1024];
    PRUint32 writeCount;
    
    if (mCancelled)
    {
        // returning an error will stop the download. We may get extra
        // OnData calls if they were already queued so beware
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
        //err = mItem->mFile->Write( buffer, amt, &result);
        //printf("mItem->mFile->Write err:%d   amt:%d    result:%d\n", err, amt, result);
        if (NS_FAILED(err) || writeCount != amt) 
        {
            //printf("mItem->mFile->Write Failed!  err:%d   amt:%d    result:%d\n", err, amt, result);
            return NS_ERROR_FAILURE;
        }
        length -= amt;
    } while (length > 0);

    return NS_OK;
}

NS_IMETHODIMP 
nsXPInstallManager::OnProgress(nsIChannel *channel, nsISupports *ctxt, PRUint32 aProgress, PRUint32 aProgressMax)
{
    nsresult rv = NS_OK;
    
    PRTime now = PR_Now();
    if (!mCancelled && TimeToUpdate(now))
    {
        if (mContentLength < 1) {
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
nsXPInstallManager::OnStatus(nsIChannel *channel, nsISupports *ctxt, 
                             nsresult aStatus, const PRUnichar *aStatusArg)
{
    nsresult rv;
    PRTime now = PR_Now();
    if (!mCancelled && TimeToUpdate(now))
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
    mDlg->SetProgress( 0, 0, BARBER_POLE ); // turn on the barber pole

    PRUnichar tmp[] = { '\0' };
    mDlg->SetActionText(tmp);

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
    mDlg->SetActionText(nsnull);
    return mDlg->SetHeading( nsString(UIPackageName).GetUnicode() );
}

NS_IMETHODIMP 
nsXPInstallManager::ItemScheduled(const PRUnichar *message)
{
    PRTime now = PR_Now();
    if (TimeToUpdate(now))
    {
        mLastUpdate = now;
        return mDlg->SetActionText( nsString(message).GetUnicode() );
    }
    else
        return NS_OK;
}

NS_IMETHODIMP 
nsXPInstallManager::FinalizeProgress(const PRUnichar *message, PRInt32 itemNum, PRInt32 totNum)
{
    nsresult rv = NS_OK;

    if (!mFinalizing)
    {
        mFinalizing = PR_TRUE;
        if (mStringBundle)
        {
            nsString rsrcName; rsrcName.AssignWithConversion("FinishingInstallMsg");
            const PRUnichar* ucRsrcName = rsrcName.GetUnicode();
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

