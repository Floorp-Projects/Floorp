/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nscore.h"
#include "pratom.h"
#include "prmem.h"

#include "nsISupports.h"
#include "nsIServiceManager.h"

#include "nsIURL.h"
#include "nsIFileURL.h"

#include "nsITransport.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "nsIInputStream.h"
#include "nsIFileStreams.h"
#include "nsIStreamListener.h"

#include "nsISoftwareUpdate.h"
#include "nsSoftwareUpdateIIDs.h"

#include "nsXPITriggerInfo.h"
#include "nsXPInstallManager.h"
#include "nsInstallTrigger.h"
#include "nsInstallResources.h"
#include "nsIProxyObjectManager.h"
#include "nsIWindowWatcher.h"
#include "nsIWindowMediator.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsReadableUtils.h"
#include "nsProxiedService.h"
#include "nsIPromptService.h"
#include "nsIScriptGlobalObject.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIObserverService.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "CertReader.h"

static NS_DEFINE_IID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#include "nsIEventQueueService.h"

#define PREF_XPINSTALL_CONFIRM_DLG "xpinstall.dialog.confirm"
#define PREF_XPINSTALL_STATUS_DLG "xpinstall.dialog.progress"
#define PREF_XPINSTALL_STATUS_DLG_TYPE "xpinstall.dialog.progress.type"

#define UPDATE_DLG(x)  (((x) - mLastUpdate) > 250000)

// Mac can't handle PRTime as integers, must go through this hell
inline PRBool nsXPInstallManager::TimeToUpdate(PRTime now)
{
	// XXX lets revisit this when dveditz gets back

	return PR_TRUE;
}


nsXPInstallManager::nsXPInstallManager()
  : mTriggers(0), mItem(0), mNextItem(0), mNumJars(0), mChromeType(NOT_CHROME),
    mContentLength(0), mDialogOpen(PR_FALSE), mCancelled(PR_FALSE),
    mSelectChrome(PR_TRUE), mNeedsShutdown(PR_FALSE)
{
    // we need to own ourself because we have a longer
    // lifetime than the scriptlet that created us.
    NS_ADDREF_THIS();

    // initialize mLastUpdate to the current time
    mLastUpdate = PR_Now();

    nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1"));
    if (os)
        os->AddObserver(this, XPI_PROGRESS_TOPIC, PR_TRUE);
}


nsXPInstallManager::~nsXPInstallManager()
{
    nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1"));
    if (os)
        os->RemoveObserver(this, XPI_PROGRESS_TOPIC);
 
    if (mTriggers)
        delete mTriggers;
}


NS_IMPL_THREADSAFE_ISUPPORTS8( nsXPInstallManager,
                               nsIXPIListener,
                               nsIXPIDialogService,
                               nsIObserver,
                               nsIStreamListener,
                               nsIProgressEventSink,
                               nsIInterfaceRequestor,
                               nsPICertNotification,
                               nsISupportsWeakReference)


NS_IMETHODIMP
nsXPInstallManager::InitManager(nsIScriptGlobalObject* aGlobalObject, nsXPITriggerInfo* aTriggers, PRUint32 aChromeType)
{
    if ( !aTriggers || aTriggers->Size() == 0 )
    {
        NS_WARNING("XPInstallManager called with no trigger info!");
        NS_RELEASE_THIS();
        return NS_ERROR_INVALID_POINTER;
    }

    nsresult rv = NS_OK;

    mTriggers = aTriggers;
    mChromeType = aChromeType;
    mNeedsShutdown = PR_TRUE;

    mParentWindow = do_QueryInterface(aGlobalObject);

    // Don't launch installs while page is still loading
    nsCOMPtr<nsPIDOMWindow> piWindow = do_QueryInterface(mParentWindow);
    if (piWindow && piWindow->IsLoadingOrRunningTimeout())
        rv = NS_ERROR_FAILURE;
    else
    {
        // Start downloading initial chunks looking for signatures,
        mOutstandingCertLoads = mTriggers->Size();

        nsXPITriggerItem *item = mTriggers->Get(--mOutstandingCertLoads);

        nsCOMPtr<nsIURI> uri;
        NS_NewURI(getter_AddRefs(uri), NS_ConvertUCS2toUTF8(item->mURL));
        nsCOMPtr<nsIStreamListener> listener = new CertReader(uri, nsnull, this);
        if (listener)
            rv = NS_OpenURI(listener, nsnull, uri);
        else
            rv = NS_ERROR_OUT_OF_MEMORY;
    }

    if (NS_FAILED(rv)) {
        Shutdown();
    }
    return rv;
}


nsresult 
nsXPInstallManager::InitManagerInternal()
{
    nsresult rv;
    PRBool OKtoInstall = PR_FALSE; // initialize to secure state

    //-----------------------------------------------------
    // *** Do not return early after this point ***
    //
    // We have to clean up the triggers in case of error
    //-----------------------------------------------------

    // --- use embedding dialogs if any registered
    nsCOMPtr<nsIXPIDialogService> dlgSvc(do_CreateInstance(NS_XPIDIALOGSERVICE_CONTRACTID));
    if ( !dlgSvc )
        dlgSvc = this; // provide our own dialogs

    // --- make sure we can get the install service
    mInstallSvc = do_GetService(nsSoftwareUpdate::GetCID(), &rv);

    // --- prepare dialog params
    PRUint32 numTriggers = mTriggers->Size();
    PRUint32 numStrings = 4 * numTriggers;
    const PRUnichar** packageList =
        (const PRUnichar**)malloc( sizeof(PRUnichar*) * numStrings );

    if ( packageList && NS_SUCCEEDED(rv) )
    {
        // populate the list. The list doesn't own the strings
        for ( PRUint32 i=0, j=0; i < numTriggers; i++ )
        {
            nsXPITriggerItem *item = mTriggers->Get(i);
            packageList[j++] = item->mName.get();
            packageList[j++] = item->mURL.get();
            packageList[j++] = item->mIconURL.get();
            packageList[j++] = item->mCertName.get();
        }

        //-----------------------------------------------------
        // Get permission to install
        //-----------------------------------------------------

        if ( mChromeType == CHROME_SKIN )
        {
            // skins get a simpler/friendlier dialog
            // XXX currently not embeddable
            OKtoInstall = ConfirmChromeInstall( mParentWindow, packageList );
        }
        else
        {
            rv = dlgSvc->ConfirmInstall( mParentWindow,
                                         packageList,
                                         numStrings,
                                         &OKtoInstall );
            if (NS_FAILED(rv))
                OKtoInstall = PR_FALSE;
        }

        if (OKtoInstall)
        {
            //-----------------------------------------------------
            // Open the progress dialog
            //-----------------------------------------------------

            rv = dlgSvc->OpenProgressDialog( packageList, numStrings, this );
        }
    }
    else
        rv = NS_ERROR_OUT_OF_MEMORY;

    //-----------------------------------------------------
    // cleanup and signal callbacks if there were errors
    //-----------------------------------------------------

    if (packageList)
        free(packageList);

    PRInt32 cbstatus = 0;  // callback status
    if (NS_FAILED(rv))
        cbstatus = nsInstall::UNEXPECTED_ERROR;
    else if (!OKtoInstall)
        cbstatus = nsInstall::USER_CANCELLED;

    if ( cbstatus != 0 )
    {
        // --- inform callbacks of error
        for (PRUint32 i = 0; i < mTriggers->Size(); i++)
        {
            mTriggers->SendStatus( mTriggers->Get(i)->mURL.get(), cbstatus );
        }

        // --- must delete ourselves if not continuing
        NS_RELEASE_THIS();
    }

    return rv;
}


NS_IMETHODIMP
nsXPInstallManager::ConfirmInstall(nsIDOMWindow *aParent, const PRUnichar **aPackageList, PRUint32 aCount, PRBool *aRetval)
{
    *aRetval = PR_FALSE;

    nsCOMPtr<nsIDOMWindowInternal> parentWindow( do_QueryInterface(aParent) );
    nsCOMPtr<nsIDialogParamBlock> params;
    nsresult rv = LoadParams( aCount, aPackageList, getter_AddRefs(params) );

    if ( NS_SUCCEEDED(rv) && parentWindow && params)
    {
        nsCOMPtr<nsIDOMWindow> newWindow;

        nsCOMPtr<nsISupportsInterfacePointer> ifptr =
            do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        ifptr->SetData(params);
        ifptr->SetDataIID(&NS_GET_IID(nsIDialogParamBlock));

        char* confirmDialogURL;
        nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID));
        if (pref) {
          rv = pref->GetCharPref(PREF_XPINSTALL_CONFIRM_DLG, &confirmDialogURL);
          NS_ASSERTION(NS_SUCCEEDED(rv), "Can't invoke XPInstall FE without a FE URL! Set xpinstall.dialog.confirm");
          if (NS_FAILED(rv))
            return rv;
        }

        rv = parentWindow->OpenDialog(NS_ConvertASCIItoUCS2(confirmDialogURL),
                                      NS_LITERAL_STRING("_blank"),
                                      NS_LITERAL_STRING("chrome,centerscreen,modal,titlebar"),
                                      ifptr,
                                      getter_AddRefs(newWindow));

        if (NS_SUCCEEDED(rv))
        {
            //Now get which button was pressed from the ParamBlock
            PRInt32 buttonPressed = 0;
            params->GetInt( 0, &buttonPressed );
            *aRetval = buttonPressed ? PR_FALSE : PR_TRUE;
        }
    }

    return rv;
}

PRBool nsXPInstallManager::ConfirmChromeInstall(nsIDOMWindowInternal* aParentWindow, const PRUnichar **aPackage)
{
    // get the dialog strings
    nsXPIDLString applyNowText;
    nsXPIDLString confirmText;
    nsCOMPtr<nsIStringBundleService> bundleSvc =
             do_GetService( kStringBundleServiceCID );
    if (!bundleSvc)
        return PR_FALSE;

    nsCOMPtr<nsIStringBundle> xpiBundle;
    bundleSvc->CreateBundle( XPINSTALL_BUNDLE_URL,
                             getter_AddRefs(xpiBundle) );
    if (!xpiBundle)
        return PR_FALSE;

    const PRUnichar *formatStrings[2] = { aPackage[0], aPackage[1] }; 
    if ( mChromeType == CHROME_LOCALE )
    {
        xpiBundle->GetStringFromName(
            NS_LITERAL_STRING("ApplyNowLocale").get(),
            getter_Copies(applyNowText));
        xpiBundle->FormatStringFromName(
            NS_LITERAL_STRING("ConfirmLocale").get(),
            formatStrings, 
            2, 
            getter_Copies(confirmText));
    }
    else
    {
        xpiBundle->GetStringFromName(
            NS_LITERAL_STRING("ApplyNowSkin").get(),
            getter_Copies(applyNowText));
        xpiBundle->FormatStringFromName(
            NS_LITERAL_STRING("ConfirmSkin").get(),
            formatStrings, 
            2, 
            getter_Copies(confirmText));
    }
    
    if (confirmText.IsEmpty())
        return PR_FALSE;

    // confirmation dialog
    PRBool bInstall = PR_FALSE;
    nsCOMPtr<nsIPromptService> dlgService(do_GetService("@mozilla.org/embedcomp/prompt-service;1"));
    if (dlgService)
    {
        dlgService->ConfirmCheck(
            aParentWindow,
            nsnull,
            confirmText,
            applyNowText,
            &mSelectChrome,
            &bInstall );
    }

    return bInstall;
}


NS_IMETHODIMP
nsXPInstallManager::OpenProgressDialog(const PRUnichar **aPackageList, PRUint32 aCount, nsIObserver *aObserver)
{
    // --- convert parameters into nsISupportArray members
    nsCOMPtr<nsIDialogParamBlock> list;
    nsresult rv = LoadParams( aCount, aPackageList, getter_AddRefs(list) );
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsISupportsInterfacePointer> listwrap(do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID));
    if (listwrap) {
        listwrap->SetData(list);
        listwrap->SetDataIID(&NS_GET_IID(nsIDialogParamBlock));
    }

    nsCOMPtr<nsISupportsInterfacePointer> callbackwrap(do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID));
    if (callbackwrap) {
        callbackwrap->SetData(aObserver);
        callbackwrap->SetDataIID(&NS_GET_IID(nsIObserver));
    }

    nsCOMPtr<nsISupportsArray> params(do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID));

    if ( !params || !listwrap || !callbackwrap )
        return NS_ERROR_FAILURE;

    params->AppendElement(listwrap);
    params->AppendElement(callbackwrap);

    // --- open the window
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
    if (wwatch) {
        char *statusDialogURL, *statusDialogType;
        nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID));
        if (pref) {
          rv = pref->GetCharPref(PREF_XPINSTALL_STATUS_DLG, &statusDialogURL);
          NS_ASSERTION(NS_SUCCEEDED(rv), "Can't invoke XPInstall FE without a FE URL! Set xpinstall.dialog.status");
          if (NS_FAILED(rv))
            return rv;

          rv = pref->GetCharPref(PREF_XPINSTALL_STATUS_DLG_TYPE, &statusDialogType);
          nsAutoString type; 
          type.AssignWithConversion(statusDialogType);
          if (NS_SUCCEEDED(rv) && !type.IsEmpty()) {
            nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
          
            nsCOMPtr<nsIDOMWindowInternal> recentWindow;
            wm->GetMostRecentWindow(type.get(), getter_AddRefs(recentWindow));
            if (recentWindow) {
              nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1"));
              os->NotifyObservers(params, "xpinstall-download-started", nsnull);

              recentWindow->Focus();
              return NS_OK;
            }
          }
        }

        nsCOMPtr<nsIDOMWindow> newWindow;
        rv = wwatch->OpenWindow(0, 
                                statusDialogURL,
                                "_blank", 
                                "chrome,centerscreen,titlebar,resizable",
                                params, 
                                getter_AddRefs(newWindow));
    }

    return rv;
}


NS_IMETHODIMP nsXPInstallManager::Observe( nsISupports *aSubject,
                                           const char *aTopic,
                                           const PRUnichar *aData )
{
    nsresult rv = NS_ERROR_ILLEGAL_VALUE;

    if ( !aTopic || !aData )
        return rv;

    if ( nsDependentCString( aTopic ).Equals( XPI_PROGRESS_TOPIC ) )
    {
        //------------------------------------------------------
        // Communication from the XPInstall Progress Dialog
        //------------------------------------------------------

        nsDependentString data( aData );

        if ( data.Equals( NS_LITERAL_STRING("open") ) )
        {
            // -- The dialog has been opened
            if (mDialogOpen)
                return NS_OK; // We've already been opened, nothing more to do

            mDialogOpen = PR_TRUE;
            rv = NS_OK;

            nsCOMPtr<nsIXPIProgressDialog> dlg( do_QueryInterface(aSubject) );
            if (dlg)
            {
                // --- create and save a proxy for the dialog
                nsCOMPtr<nsIProxyObjectManager> pmgr =
                            do_GetService(kProxyObjectManagerCID, &rv);
                if (pmgr)
                {
                    rv = pmgr->GetProxyForObject( NS_UI_THREAD_EVENTQ,
                                                  NS_GET_IID(nsIXPIProgressDialog),
                                                  dlg,
                                                  PROXY_SYNC | PROXY_ALWAYS,
                                                  getter_AddRefs(mDlg) );
                }
            }

            // -- get the ball rolling
            DownloadNext();
        }

        else if ( data.Equals( NS_LITERAL_STRING("cancel") ) )
        {
            // -- The dialog/user wants us to cancel the download
            mCancelled = PR_TRUE;
            if ( !mDialogOpen )
            {
                // if we've never been opened then we can shutdown right here,
                // otherwise we need to let mCancelled get discovered elsewhere
                Shutdown();
            }
            rv = NS_OK;
        }
    }

    return rv;
}


NS_IMETHODIMP nsXPInstallManager::DownloadNext()
{
    nsresult rv;
    mContentLength = 0;

    if (mCancelled)
    {
        // Don't download any more if we were cancelled
        Shutdown();
        return NS_OK;
    }

    if ( mNextItem < mTriggers->Size() )
    {
        //-------------------------------------------------
        // There are items to download, get the next one
        //-------------------------------------------------
        mItem = (nsXPITriggerItem*)mTriggers->Get(mNextItem++);

        NS_ASSERTION( mItem, "bogus Trigger slipped through" );
        NS_ASSERTION( !mItem->mURL.IsEmpty(), "bogus trigger");
        if ( !mItem || mItem->mURL.IsEmpty() )
        {
            // serious problem with trigger! Can't notify anyone of the
            // error without the URL, just try to carry on.
            return DownloadNext();
        }

        // --- Tell the dialog we're starting a download
        if (mDlg)
            mDlg->OnStateChange( mNextItem-1, nsIXPIProgressDialog::DOWNLOAD_START, 0 );

        if ( mItem->IsFileURL() && mChromeType == NOT_CHROME )
        {
            //--------------------------------------------------
            // Already local, we can open it where it is
            //--------------------------------------------------
            nsCOMPtr<nsIURI> pURL;
            rv = NS_NewURI(getter_AddRefs(pURL), mItem->mURL);

            if (NS_SUCCEEDED(rv))
            {
                nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(pURL,&rv);
                if (fileURL)
                {
                    nsCOMPtr<nsIFile> localFile;
                    rv = fileURL->GetFile(getter_AddRefs(localFile));
                    if (NS_SUCCEEDED(rv))
                    {
                        mItem->mFile = do_QueryInterface(localFile,&rv);
                    }
                }
            }

            if ( NS_FAILED(rv) || !mItem->mFile )
            {
                // send error status back
                if (mDlg)
                    mDlg->OnStateChange( mNextItem-1,
                                         nsIXPIProgressDialog::INSTALL_DONE,
                                         nsInstall::UNEXPECTED_ERROR );
                mTriggers->SendStatus( mItem->mURL.get(),
                                       nsInstall::UNEXPECTED_ERROR );
                mItem->mFile = 0;
            }
            else if (mDlg)
            {
                mDlg->OnStateChange( mNextItem-1,
                                     nsIXPIProgressDialog::DOWNLOAD_DONE, 0);
            }

            // --- on to the next one
            return DownloadNext();
        }
        else
        {
            //--------------------------------------------------
            // We have one to download
            //--------------------------------------------------
            rv = GetDestinationFile(mItem->mURL, getter_AddRefs(mItem->mFile));
            if (NS_SUCCEEDED(rv))
            {
                nsCOMPtr<nsIURI> pURL;
                rv = NS_NewURI(getter_AddRefs(pURL), mItem->mURL);
                if (NS_SUCCEEDED(rv))
                {
                    nsCOMPtr<nsIChannel> channel;

                    rv = NS_NewChannel(getter_AddRefs(channel), pURL, nsnull, nsnull, this);
                    if (NS_SUCCEEDED(rv))
                    {
                        rv = channel->AsyncOpen(this, nsnull);
                    }
                }
            }

            if (NS_FAILED(rv))
            {
                // announce failure
                if (mDlg)
                    mDlg->OnStateChange( mNextItem-1,
                                         nsIXPIProgressDialog::INSTALL_DONE,
                                         nsInstall::DOWNLOAD_ERROR );
                mTriggers->SendStatus( mItem->mURL.get(),
                                       nsInstall::DOWNLOAD_ERROR );
                mItem->mFile = 0;

                // We won't get Necko callbacks so start the next one now
                return DownloadNext();
            }
        }
    }
    else
    {
        //------------------------------------------------------
        // all downloaded, queue them for installation
        //------------------------------------------------------

        // can't cancel from here on cause we can't undo installs in a multitrigger
        for (PRUint32 i = 0; i < mTriggers->Size(); ++i)
        {
            mItem = (nsXPITriggerItem*)mTriggers->Get(i);
            if ( !mItem || !mItem->mFile )
            {
                // notification for these errors already handled
                continue;
            }

            // We've got one to install; increment count first so we
            // don't have to worry about thread timing.
            PR_AtomicIncrement(&mNumJars);

            if ( mChromeType == NOT_CHROME ) {
                rv = mInstallSvc->InstallJar( mItem->mFile,
                                              mItem->mURL.get(),
                                              mItem->mArguments.get(),
                                              mItem->mPrincipal,
                                              mItem->mFlags,
                                              this );
            }
            else {
                rv = mInstallSvc->InstallChrome(mChromeType,
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
                if (mDlg)
                    mDlg->OnStateChange( i, nsIXPIProgressDialog::INSTALL_DONE,
                                         nsInstall::UNEXPECTED_ERROR );
            }
        }

        if ( mNumJars == 0 )
        {
            // We must clean ourself up now -- we won't be called back
            Shutdown();
        }
    }

    return rv;
}


void nsXPInstallManager::Shutdown()
{
    if (mDlg)
    {
        // tell the dialog it can go away
        mDlg->OnStateChange(0, nsIXPIProgressDialog::DIALOG_CLOSE, 0 );
        mDlg = nsnull;
        mDialogOpen = PR_FALSE;
    }

    if (mNeedsShutdown)
    {
        mNeedsShutdown = PR_FALSE;

        // Send remaining status notifications if we were cancelled early
        nsXPITriggerItem* item;
        while ( mNextItem < mTriggers->Size() )
        {
            item = (nsXPITriggerItem*)mTriggers->Get(mNextItem++);
            if ( item && !item->mURL.IsEmpty() )
            {
                mTriggers->SendStatus( item->mURL.get(),
                                       nsInstall::USER_CANCELLED );
            }
        }

        // Clean up downloaded files (regular install only, not chrome installs)
        nsCOMPtr<nsIFile> tmpSpec;
        if ( mChromeType == NOT_CHROME )
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

NS_IMETHODIMP
nsXPInstallManager::LoadParams(PRUint32 aCount, const PRUnichar** aPackageList, nsIDialogParamBlock** aParams)
{
    nsIDialogParamBlock* paramBlock = 0;
    nsresult rv = nsComponentManager::CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID,
                                            nsnull,
                                            NS_GET_IID(nsIDialogParamBlock),
                                            (void**)&paramBlock);

    if (NS_SUCCEEDED(rv))
    {
        // set OK and Cancel buttons
        paramBlock->SetInt( 0, 2 );
        // pass in number of strings
        paramBlock->SetInt( 1, aCount );
        // add strings
        paramBlock->SetNumberStrings( aCount );
        for (PRUint32 i = 0; i < aCount; i++)
            paramBlock->SetString( i, aPackageList[i] );
    }

    *aParams = paramBlock;
    return rv;
}


NS_IMETHODIMP
nsXPInstallManager::GetDestinationFile(nsString& url, nsILocalFile* *file)
{
    NS_ENSURE_ARG_POINTER(file);

    nsresult rv;
    nsAutoString leaf;

    PRInt32 pos = url.RFindChar('/');
    url.Mid( leaf, pos+1, url.Length() );

    nsCOMPtr<nsIProperties> directoryService =
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);

    if (mChromeType == NOT_CHROME )
    {
        // a regular XPInstall, not chrome
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsILocalFile> temp;
            rv = directoryService->Get(NS_OS_TEMP_DIR,
                                       NS_GET_IID(nsIFile),
                                       getter_AddRefs(temp));
            if (NS_SUCCEEDED(rv))
            { 
                temp->AppendNative(NS_LITERAL_CSTRING("tmp.xpi"));
                temp->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0644);
                *file = temp;
                NS_IF_ADDREF(*file);
            }
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
                    userChrome->Append(leaf);
                    userChrome->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0644);
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

        rv = NS_NewLocalFileOutputStream(getter_AddRefs(mItem->mOutStream),
                                         mItem->mFile,
                                         PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                         0664);
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

    if (NS_FAILED(rv) || mCancelled)
    {
        // Download error!
        // -- first clean up partially downloaded file
        if ( mItem->mFile )
        {
            PRBool flagExists;
            nsresult rv2 ;
            rv2 = mItem->mFile->Exists(&flagExists);
            if (NS_SUCCEEDED(rv2) && flagExists)
                mItem->mFile->Remove(PR_FALSE);

            mItem->mFile = 0;
        }

        // -- then notify interested parties
        PRInt32 errorcode = mCancelled ? nsInstall::USER_CANCELLED 
                                       : nsInstall::DOWNLOAD_ERROR;
        if (mDlg)
            mDlg->OnStateChange( mNextItem-1,
                                 nsIXPIProgressDialog::INSTALL_DONE,
                                 errorcode );
        mTriggers->SendStatus( mItem->mURL.get(), errorcode );
    }
    else if (mDlg)
    {
        mDlg->OnStateChange( mNextItem-1, nsIXPIProgressDialog::DOWNLOAD_DONE, 0);
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

    if (mCancelled)
    {
        // We must cancel this download in progress. We may get extra
        // OnData calls if they were already queued so beware
        request->Cancel(NS_BINDING_ABORTED);
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
            nsCOMPtr<nsIChannel> channel = do_QueryInterface(request,&rv);
            NS_ASSERTION(channel, "should have a channel");
            if (NS_FAILED(rv)) return rv;
            rv = channel->GetContentLength(&mContentLength);
            if (NS_FAILED(rv)) return rv;
        }
        mLastUpdate = now;
        rv = mDlg->OnProgress( mNextItem-1, aProgress, mContentLength );
    }

    return rv;
}

NS_IMETHODIMP
nsXPInstallManager::OnStatus(nsIRequest* request, nsISupports *ctxt,
                             nsresult aStatus, const PRUnichar *aStatusArg)
{
    // don't need to do anything
    return NS_OK;
}

// nsIInterfaceRequestor method
NS_IMETHODIMP
nsXPInstallManager::GetInterface(const nsIID & eventSinkIID, void* *_retval)
{
    return QueryInterface(eventSinkIID, (void**)_retval);
}

// IXPIListener methods

PRInt32 
nsXPInstallManager::GetIndexFromURL(const PRUnichar* aUrl)
{
    // --- figure out which index corresponds to this URL
    PRUint32 i;
    for (i=0; i < mTriggers->Size(); i++)
    {
        if ( (nsXPITriggerInfo*)mTriggers->Get(i)->mURL.Equals(aUrl) )
            break;
    }
    NS_ASSERTION( i < mTriggers->Size(), "invalid result URL!" );

    return i;
}

NS_IMETHODIMP
nsXPInstallManager::OnInstallStart(const PRUnichar *URL)
{
    if (mDlg)
        mDlg->OnStateChange( GetIndexFromURL( URL ),
                             nsIXPIProgressDialog::INSTALL_START,
                             0 );

    return NS_OK;
}

NS_IMETHODIMP
nsXPInstallManager::OnPackageNameSet(const PRUnichar *URL, const PRUnichar *UIPackageName, const PRUnichar *aVersion)
{
    // Don't need to do anything
    return NS_OK;
}

NS_IMETHODIMP
nsXPInstallManager::OnItemScheduled(const PRUnichar *message)
{
    // Don't need to do anything
    return NS_OK;
}

NS_IMETHODIMP
nsXPInstallManager::OnFinalizeProgress(const PRUnichar *message, PRInt32 itemNum, PRInt32 totNum)
{
    // Don't need to do anything
    return NS_OK;
}

NS_IMETHODIMP
nsXPInstallManager::OnInstallDone(const PRUnichar *URL, PRInt32 status)
{
    // --- send the final success/error status on to listeners
    mTriggers->SendStatus( URL, status );

    if (mDlg)
        mDlg->OnStateChange( GetIndexFromURL( URL ),
                             nsIXPIProgressDialog::INSTALL_DONE,
                             status );

    PR_AtomicDecrement( &mNumJars );
    if ( mNumJars == 0 )
        Shutdown();

    return NS_OK;
}

NS_IMETHODIMP
nsXPInstallManager::OnLogComment(const PRUnichar* comment)
{
    // Don't need to do anything
    return NS_OK;
}


NS_IMETHODIMP 
nsXPInstallManager::OnCertAvailable(nsIURI *aURI, 
                                    nsISupports* context, 
                                    nsresult aStatus, 
                                    nsIPrincipal *aPrincipal)
{
    if (NS_FAILED(aStatus) && aStatus != NS_BINDING_ABORTED) {
        // Check for a bad status.  The only acceptable failure status code we accept 
        // is NS_BINDING_ABORTED.  For all others we want to ensure that the 
        // nsIPrincipal is nsnull.

        NS_ASSERTION(aPrincipal == nsnull, "There has been an error, but we have a principal!");
        aPrincipal = nsnull;
    }

    // get the current one and assign the cert name
    nsXPITriggerItem *item = mTriggers->Get(mOutstandingCertLoads);
    item->SetPrincipal(aPrincipal);

    if (mOutstandingCertLoads == 0) {
        InitManagerInternal();
        return NS_OK;
    }

    // get the next one to load.  If there is any failure, we just go on to the 
    // next trigger.  When all triggers items are handled, we call into InitManagerInternal

    item = mTriggers->Get(--mOutstandingCertLoads);

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), NS_ConvertUCS2toUTF8(item->mURL.get()).get());

    if (!uri || mChromeType != NOT_CHROME)
        return OnCertAvailable(uri, context, NS_ERROR_FAILURE, nsnull);

    nsIStreamListener* listener = new CertReader(uri, nsnull, this);
    if (!listener)
        return OnCertAvailable(uri, context, NS_ERROR_FAILURE, nsnull);

    NS_ADDREF(listener);
    nsresult rv = NS_OpenURI(listener, nsnull, uri);

    NS_ASSERTION(NS_SUCCEEDED(rv), "OpenURI failed");
    NS_RELEASE(listener);

    if (NS_FAILED(rv))
        return OnCertAvailable(uri, context, NS_ERROR_FAILURE, nsnull);

    return NS_OK;
}

