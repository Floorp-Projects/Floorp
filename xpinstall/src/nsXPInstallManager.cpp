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
 *   Dave Townsend <dtownsend@oxymoronical.com>
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
#include "prprf.h"
#include "plstr.h"

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
#include "nsICryptoHash.h"

#include "nsIExtensionManager.h"
#include "nsSoftwareUpdateIIDs.h"

#include "nsIStringEnumerator.h"
#include "nsXPITriggerInfo.h"
#include "nsXPInstallManager.h"
#include "nsInstallTrigger.h"
#include "nsIWindowWatcher.h"
#include "nsIAuthPrompt.h"
#include "nsIWindowMediator.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsReadableUtils.h"
#include "nsIPromptService.h"
#include "nsIScriptGlobalObject.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIObserverService.h"

#include "nsISSLStatusProvider.h"
#include "nsISSLStatus.h"
#include "nsIX509Cert.h"
#include "nsIX509Cert3.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "CertReader.h"

#include "nsEmbedCID.h"
#include "nsIAsyncVerifyRedirectCallback.h"

#define PREF_XPINSTALL_ENABLED                "xpinstall.enabled"
#define PREF_XPINSTALL_CONFIRM_DLG            "xpinstall.dialog.confirm"
#define PREF_XPINSTALL_STATUS_DLG_SKIN        "xpinstall.dialog.progress.skin"
#define PREF_XPINSTALL_STATUS_DLG_CHROME      "xpinstall.dialog.progress.chrome"
#define PREF_XPINSTALL_STATUS_DLG_TYPE_SKIN   "xpinstall.dialog.progress.type.skin"
#define PREF_XPINSTALL_STATUS_DLG_TYPE_CHROME "xpinstall.dialog.progress.type.chrome"

static NS_DEFINE_IID(kZipReaderCID,  NS_ZIPREADER_CID);


nsXPInstallManager::nsXPInstallManager()
  : mTriggers(0), mItem(0), mNextItem(0), mChromeType(NOT_CHROME),
    mContentLength(0), mDialogOpen(PR_FALSE), mCancelled(PR_FALSE),
    mNeedsShutdown(PR_FALSE), mFromChrome(PR_FALSE)
{
    // we need to own ourself because we have a longer
    // lifetime than the scriptlet that created us.
    NS_ADDREF_THIS();
}


nsXPInstallManager::~nsXPInstallManager()
{
    NS_ASSERT_OWNINGTHREAD(nsXPInstallManager);
    NS_ASSERTION(!mTriggers, "Shutdown not called, triggers still alive");
}


NS_INTERFACE_MAP_BEGIN(nsXPInstallManager)
  NS_INTERFACE_MAP_ENTRY(nsIXPIDialogService)
  NS_INTERFACE_MAP_ENTRY(nsIXPInstallManager)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsPICertNotification)
  NS_INTERFACE_MAP_ENTRY(nsIBadCertListener2)
  NS_INTERFACE_MAP_ENTRY(nsISSLErrorListener)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsXPInstallManager)
NS_IMPL_THREADSAFE_RELEASE(nsXPInstallManager)

NS_IMETHODIMP
nsXPInstallManager::InitManagerFromChrome(const PRUnichar **aURLs,
                                          PRUint32 aURLCount,
                                          nsIXPIProgressDialog* aListener)
{
    return InitManagerWithHashes(aURLs, nsnull, aURLCount, aListener);
}

NS_IMETHODIMP
nsXPInstallManager::InitManagerWithHashes(const PRUnichar **aURLs,
                                          const char **aHashes,
                                          PRUint32 aURLCount,
                                          nsIXPIProgressDialog* aListener)
{
    // If Software Installation is not enabled, we don't want to proceed with
    // update.
    bool xpinstallEnabled = true;
    nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (pref)
        pref->GetBoolPref(PREF_XPINSTALL_ENABLED, &xpinstallEnabled);

    if (!xpinstallEnabled)
        return NS_OK;

    mTriggers = new nsXPITriggerInfo();
    if (!mTriggers)
        return NS_ERROR_OUT_OF_MEMORY;

    mNeedsShutdown = PR_TRUE;

    for (PRUint32 i = 0; i < aURLCount; ++i)
    {
        nsXPITriggerItem* item = new nsXPITriggerItem(0, aURLs[i], nsnull,
                                                      aHashes ? aHashes[i] : nsnull);
        if (!item)
        {
            delete mTriggers; // nsXPITriggerInfo frees any alloc'ed nsXPITriggerItems
            mTriggers = nsnull;
            Shutdown();
            return NS_ERROR_OUT_OF_MEMORY;
        }
        mTriggers->Add(item);
    }

    mFromChrome = PR_TRUE;

    nsresult rv = Observe(aListener, XPI_PROGRESS_TOPIC, NS_LITERAL_STRING("open").get());
    if (NS_FAILED(rv))
        Shutdown();
    return rv;
}

NS_IMETHODIMP
nsXPInstallManager::InitManagerWithInstallInfo(nsIXPIInstallInfo* aInstallInfo)
{
    nsXPITriggerInfo* triggers;
    nsresult rv = aInstallInfo->GetTriggerInfo(&triggers);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMWindow> win;
    rv = aInstallInfo->GetOriginatingWindow(getter_AddRefs(win));
    if (NS_SUCCEEDED(rv))
    {
        PRUint32 type;
        rv = aInstallInfo->GetChromeType(&type);
        if (NS_SUCCEEDED(rv))
        {
            // Passing ownership onto InitManager which will free when necessary
            aInstallInfo->SetTriggerInfo(nsnull);
            return InitManager(win, triggers, type);
        }
    }

    NS_RELEASE_THIS();
    return rv;
}

NS_IMETHODIMP
nsXPInstallManager::InitManager(nsIDOMWindow* aParentWindow, nsXPITriggerInfo* aTriggers, PRUint32 aChromeType)
{
    if ( !aTriggers || aTriggers->Size() == 0 )
    {
        NS_WARNING("XPInstallManager called with no trigger info!");
        delete aTriggers;
        NS_RELEASE_THIS();
        return NS_ERROR_INVALID_POINTER;
    }

    nsresult rv = NS_OK;

    mNeedsShutdown = PR_TRUE;
    mTriggers = aTriggers;
    mChromeType = aChromeType;

    mParentWindow = aParentWindow;

    // Attempt to find a load group, continue if we can't find one though
    if (aParentWindow) {
        nsCOMPtr<nsIDOMDocument> domdoc;
        rv = aParentWindow->GetDocument(getter_AddRefs(domdoc));
        if (NS_SUCCEEDED(rv) && domdoc) {
            nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
            if (doc)
                mLoadGroup = doc->GetDocumentLoadGroup();
        }
    }

    // Start downloading initial chunks looking for signatures,
    mOutstandingCertLoads = mTriggers->Size();

    nsXPITriggerItem *item = mTriggers->Get(--mOutstandingCertLoads);

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), NS_ConvertUTF16toUTF8(item->mURL));
    nsCOMPtr<nsIStreamListener> listener = new CertReader(uri, nsnull, this);
    if (listener)
        rv = NS_OpenURI(listener, nsnull, uri, nsnull, mLoadGroup);
    else
        rv = NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv)) {
        Shutdown();
    }
    return rv;
}


nsresult
nsXPInstallManager::InitManagerInternal()
{
    nsresult rv;
    bool OKtoInstall = false; // initialize to secure state

    //-----------------------------------------------------
    // *** Do not return early after this point ***
    //
    // We have to clean up the triggers in case of error
    //-----------------------------------------------------

    // --- use embedding dialogs if any registered
    nsCOMPtr<nsIXPIDialogService> dlgSvc(do_CreateInstance(NS_XPIDIALOGSERVICE_CONTRACTID));
    if ( !dlgSvc )
        dlgSvc = this; // provide our own dialogs

    // --- prepare dialog params
    PRUint32 numTriggers = mTriggers->Size();
    PRUint32 numStrings = 4 * numTriggers;
    const PRUnichar** packageList =
        (const PRUnichar**)malloc( sizeof(PRUnichar*) * numStrings );

    if ( packageList )
    {
        // populate the list. The list doesn't own the strings
        for ( PRUint32 i=0, j=0; i < numTriggers; i++ )
        {
            nsXPITriggerItem *item = mTriggers->Get(i);
            packageList[j++] = item->mName.get();
            packageList[j++] = item->GetSafeURLString();
            packageList[j++] = item->mIconURL.get();
            packageList[j++] = item->mCertName.get();
        }

        //-----------------------------------------------------
        // Get permission to install
        //-----------------------------------------------------

#ifdef ENABLE_SKIN_SIMPLE_INSTALLATION_UI
        if ( mChromeType == CHROME_SKIN )
        {
            // We may want to enable the simple installation UI once
            // bug 343037 is fixed

            // skins get a simpler/friendlier dialog
            // XXX currently not embeddable
            OKtoInstall = ConfirmChromeInstall( mParentWindow, packageList );
        }
        else
        {
#endif
            rv = dlgSvc->ConfirmInstall( mParentWindow,
                                         packageList,
                                         numStrings,
                                         &OKtoInstall );
            if (NS_FAILED(rv))
                OKtoInstall = PR_FALSE;
#ifdef ENABLE_SKIN_SIMPLE_INSTALLATION_UI
        }
#endif

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
        // --- must shutdown if not continuing
        Shutdown( cbstatus );
    }

    return rv;
}


NS_IMETHODIMP
nsXPInstallManager::ConfirmInstall(nsIDOMWindow *aParent, const PRUnichar **aPackageList, PRUint32 aCount, bool *aRetval)
{
    *aRetval = PR_FALSE;

    nsCOMPtr<nsIDOMWindow> parentWindow = aParent;
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
        nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
        if (!pref)
          return rv;

        rv = pref->GetCharPref(PREF_XPINSTALL_CONFIRM_DLG, &confirmDialogURL);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't invoke XPInstall FE without a FE URL! Set xpinstall.dialog.confirm");
        if (NS_FAILED(rv))
          return rv;

        rv = parentWindow->OpenDialog(NS_ConvertASCIItoUTF16(confirmDialogURL),
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

#ifdef ENABLE_SKIN_SIMPLE_INSTALLATION_UI
bool nsXPInstallManager::ConfirmChromeInstall(nsIDOMWindow* aParentWindow, const PRUnichar **aPackage)
{
    // get the dialog strings
    nsXPIDLString applyNowText;
    nsXPIDLString confirmText;
    nsCOMPtr<nsIStringBundleService> bundleSvc =
             do_GetService(NS_STRINGBUNDLE_CONTRACTID);
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
    bool bInstall = false;
    nsCOMPtr<nsIPromptService> dlgService(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
    if (dlgService)
    {
        dlgService->Confirm(
            aParentWindow,
            nsnull,
            confirmText,
            &bInstall );
    }

    return bInstall;
}
#endif

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
    if (!wwatch)
        return rv;

    char *statusDialogURL, *statusDialogType;
    nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    if (!pref)
        return rv;
	const char* statusDlg = mChromeType == CHROME_SKIN ? PREF_XPINSTALL_STATUS_DLG_SKIN
                                                         : PREF_XPINSTALL_STATUS_DLG_CHROME;
	rv = pref->GetCharPref(statusDlg, &statusDialogURL);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Can't invoke XPInstall FE without a FE URL! Set xpinstall.dialog.status");
	if (NS_FAILED(rv))
		return rv;

    const char* statusType = mChromeType == CHROME_SKIN ? PREF_XPINSTALL_STATUS_DLG_TYPE_SKIN
                                                        : PREF_XPINSTALL_STATUS_DLG_TYPE_CHROME;
    rv = pref->GetCharPref(statusType, &statusDialogType);
    nsAutoString type;
    type.AssignWithConversion(statusDialogType);
    if (NS_SUCCEEDED(rv) && !type.IsEmpty()) {
        nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);

        nsCOMPtr<nsIDOMWindow> recentWindow;
        wm->GetMostRecentWindow(type.get(), getter_AddRefs(recentWindow));
        if (recentWindow) {
            nsCOMPtr<nsIObserverService> os =
              mozilla::services::GetObserverService();
            os->NotifyObservers(params, "xpinstall-download-started", nsnull);

            recentWindow->Focus();
            return NS_OK;
        }
    }

    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = wwatch->OpenWindow(0,
                            statusDialogURL,
                            "_blank",
                            "chrome,menubar,extra-chrome,toolbar,dialog=no,resizable",
                            params,
                            getter_AddRefs(newWindow));

    return rv;
}


NS_IMETHODIMP nsXPInstallManager::Observe( nsISupports *aSubject,
                                           const char *aTopic,
                                           const PRUnichar *aData )
{
    nsresult rv = NS_ERROR_ILLEGAL_VALUE;

    if ( !aTopic || !aData )
        return rv;

    nsDependentCString topic( aTopic );
    if ( topic.Equals( XPI_PROGRESS_TOPIC ) )
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

            nsCOMPtr<nsIObserverService> os =
              mozilla::services::GetObserverService();
            if (os)
            {
                os->AddObserver(this, NS_IOSERVICE_GOING_OFFLINE_TOPIC, PR_TRUE);
                os->AddObserver(this, "quit-application", PR_TRUE);
            }

            mDlg = do_QueryInterface(aSubject);

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
    else if ( topic.Equals( NS_IOSERVICE_GOING_OFFLINE_TOPIC ) ||
              topic.Equals( "quit-application" ) )
    {
        mCancelled = PR_TRUE;
        rv = NS_OK;
    }

    return rv;
}


///////////////////////////////////////////////////////////////////////////////////////////////
// Function name    : VerifySigning
// Description      : Verify that the entire zip file is signed by the certificate displayed to
//                    the user during download
// Return type      : PRInt32
// Argument         : nsIZipReader* hZip       - the zip reader
// Argument         : nsIPrincipal* aPrincipal - a principal, if any, displayed to the user
//                    during download. Would have been retrieved from the first file in the zip
///////////////////////////////////////////////////////////////////////////////////////////////

static nsresult
VerifySigning(nsIZipReader* hZip, nsIPrincipal* aPrincipal)
{
    // If we didn't detect a principal from the zip file during download then
    // we didn't suggest it was signed to the user, so just carry on.
    if (!aPrincipal)
        return NS_OK;

    bool hasCert;
    aPrincipal->GetHasCertificate(&hasCert);
    if (!hasCert)
        return NS_ERROR_FAILURE;

    // See if the archive is signed at all first
    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = hZip->GetCertificatePrincipal(EmptyCString(), getter_AddRefs(principal));
    if (NS_FAILED(rv) || !principal)
        return NS_ERROR_FAILURE;

    PRUint32 entryCount = 0;

    // first verify all files in the jar are also in the manifest.
    nsCOMPtr<nsIUTF8StringEnumerator> entries;
    rv = hZip->FindEntries(EmptyCString(), getter_AddRefs(entries));
    if (NS_FAILED(rv))
        return rv;

    bool more;
    nsCAutoString name;
    while (NS_SUCCEEDED(entries->HasMore(&more)) && more)
    {
        rv = entries->GetNext(name);
        if (NS_FAILED(rv)) return rv;

        // Do not verify the directory entries or
        // entries which are in the meta-inf directory
        if ((name.Last() == '/') ||
            (PL_strncasecmp("META-INF/", name.get(), 9) == 0))
            continue;

        // Count the entries to be verified
        entryCount++;

        // Each entry must be signed
        rv = hZip->GetCertificatePrincipal(name, getter_AddRefs(principal));
        if (NS_FAILED(rv) || !principal) return NS_ERROR_FAILURE;

        bool equal;
        rv = principal->Equals(aPrincipal, &equal);
        if (NS_FAILED(rv) || !equal) return NS_ERROR_FAILURE;
    }

    // next verify all files in the manifest are in the archive.
    PRUint32 manifestEntryCount;
    rv = hZip->GetManifestEntriesCount(&manifestEntryCount);
    if (NS_FAILED(rv))
        return rv;

    if (entryCount != manifestEntryCount)
        return NS_ERROR_FAILURE;  // some files were deleted from archive

    return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////////////////////
// Function name    : OpenAndValidateArchive
// Description      : Opens install archive and validates contents
// Return type      : PRInt32
// Argument         : nsIZipReader* hZip       - the zip reader
// Argument         : nsIFile* jarFile         - the .xpi file
// Argument         : nsIPrincipal* aPrincipal - a principal, if any, displayed to the user
//                    regarding the cert used to sign this install
///////////////////////////////////////////////////////////////////////////////////////////////

static PRInt32
OpenAndValidateArchive(nsIZipReader* hZip, nsIFile* jarFile, nsIPrincipal* aPrincipal)
{
    if (!jarFile)
        return nsInstall::DOWNLOAD_ERROR;

    nsCOMPtr<nsIFile> jFile;
    nsresult rv =jarFile->Clone(getter_AddRefs(jFile));
    if (NS_SUCCEEDED(rv))
        rv = hZip->Open(jFile);

    if (NS_FAILED(rv))
        return nsInstall::CANT_READ_ARCHIVE;

    // CRC check the integrity of all items in this archive
    rv = hZip->Test(EmptyCString());
    if (NS_FAILED(rv))
    {
        NS_WARNING("CRC check of archive failed!");
        return nsInstall::CANT_READ_ARCHIVE;
    }

    rv = VerifySigning(hZip, aPrincipal);
    if (NS_FAILED(rv))
    {
        NS_WARNING("Signing check of archive failed!");
        return nsInstall::INVALID_SIGNATURE;
    }

    if (NS_FAILED(hZip->Test(nsDependentCString("install.rdf"))))
    {
        NS_WARNING("Archive did not contain an install manifest!");
        return nsInstall::NO_INSTALL_SCRIPT;
    }

    return nsInstall::SUCCESS;
}


nsresult nsXPInstallManager::InstallItems()
{
    nsresult rv;
    nsCOMPtr<nsIZipReader> hZip = do_CreateInstance(kZipReaderCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIExtensionManager> em = do_GetService("@mozilla.org/extensions/manager;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // can't cancel from here on cause we can't undo installs in a multitrigger
    for (PRUint32 i = 0; i < mTriggers->Size(); ++i)
    {
        mItem = (nsXPITriggerItem*)mTriggers->Get(i);
        if ( !mItem || !mItem->mFile )
        {
            // notification for these errors already handled
            continue;
        }

        // If there was hash info in the trigger, but
        // there wasn't a hash object created, then the
        // algorithm used isn't known.

        if (mItem->mHashFound && !mItem->mHasher)
        {
            // report failure
            mTriggers->SendStatus( mItem->mURL.get(), nsInstall::INVALID_HASH_TYPE );
            if (mDlg)
                mDlg->OnStateChange( i, nsIXPIProgressDialog::INSTALL_DONE,
                                     nsInstall::INVALID_HASH_TYPE );
            continue;
        }

        // Don't install if we can't verify the hash (if specified)
        if (mItem->mHasher && !VerifyHash(mItem))
        {
            // report failure
            mTriggers->SendStatus( mItem->mURL.get(), nsInstall::INVALID_HASH );
            if (mDlg)
                mDlg->OnStateChange( i, nsIXPIProgressDialog::INSTALL_DONE,
                                     nsInstall::INVALID_HASH );
            continue;
        }

        if (mDlg)
            mDlg->OnStateChange( i, nsIXPIProgressDialog::INSTALL_START, 0 );

        PRInt32 finalStatus = OpenAndValidateArchive( hZip,
                                                      mItem->mFile,
                                                      mItem->mPrincipal);
        hZip->Close();

        if (finalStatus == nsInstall::SUCCESS)
        {
            rv = em->InstallItemFromFile( mItem->mFile,
                                          NS_INSTALL_LOCATION_APPPROFILE);
            if (NS_FAILED(rv))
                finalStatus = nsInstall::EXECUTION_ERROR;
        }

        mTriggers->SendStatus( mItem->mURL.get(), finalStatus );
        if (mDlg)
            mDlg->OnStateChange( i, nsIXPIProgressDialog::INSTALL_DONE,
                                 finalStatus );
    }
    return NS_OK;
}

NS_IMETHODIMP nsXPInstallManager::DownloadNext()
{
    nsresult rv = NS_OK;
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

                    rv = NS_NewChannel(getter_AddRefs(channel), pURL, nsnull, mLoadGroup, this);
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
        // all downloaded, install them
        //------------------------------------------------------
        InstallItems();
        Shutdown();
    }

    return rv;
}


//-------------------------------------------------------------------
// VerifyHash
//
// Returns true if the file hash matches the expected value (or if
// the item has no hash value). False if we can't verify the hash
// for any reason
//
bool nsXPInstallManager::VerifyHash(nsXPITriggerItem* aItem)
{
    NS_ASSERTION(aItem, "Null nsXPITriggerItem passed to VerifyHash");

    nsresult rv;
    if (!aItem->mHasher)
      return PR_FALSE;

    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), aItem->mFile);
    if (NS_FAILED(rv)) return PR_FALSE;

    rv = aItem->mHasher->UpdateFromStream(stream, PR_UINT32_MAX);
    if (NS_FAILED(rv)) return PR_FALSE;

    nsCAutoString binaryHash;
    rv = aItem->mHasher->Finish(PR_FALSE, binaryHash);
    if (NS_FAILED(rv)) return PR_FALSE;

    char* hash = nsnull;
    for (PRUint32 i=0; i < binaryHash.Length(); ++i)
    {
        hash = PR_sprintf_append(hash,"%.2x", (PRUint8)binaryHash[i]);
    }

    bool result = aItem->mHash.EqualsIgnoreCase(hash);

    PR_smprintf_free(hash);
    return result;
}


void nsXPInstallManager::Shutdown(PRInt32 status)
{
    if (mDlg)
    {
        // tell the dialog it can go away
        mDlg->OnStateChange(0, nsIXPIProgressDialog::DIALOG_CLOSE, 0 );
        mDlg = nsnull;
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
                mTriggers->SendStatus( item->mURL.get(), status );
            }
        }

        // Clean up downloaded files (regular install only, not chrome installs)
        for (PRUint32 i = 0; i < mTriggers->Size(); i++ )
        {
            item = static_cast<nsXPITriggerItem*>(mTriggers->Get(i));
            if ( item && item->mFile && !item->IsFileURL() )
                item->mFile->Remove(PR_FALSE);
        }

        nsCOMPtr<nsIObserverService> os =
          mozilla::services::GetObserverService();
        if (os)
        {
            os->RemoveObserver(this, NS_IOSERVICE_GOING_OFFLINE_TOPIC);
            os->RemoveObserver(this, "quit-application");
        }

        if (mTriggers)
        {
            delete mTriggers;
            mTriggers = nsnull;
        }

        NS_RELEASE_THIS();
    }
}

NS_IMETHODIMP
nsXPInstallManager::LoadParams(PRUint32 aCount, const PRUnichar** aPackageList, nsIDialogParamBlock** aParams)
{
    nsresult rv;
    nsCOMPtr<nsIDialogParamBlock> paramBlock = do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID, &rv);
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

    NS_IF_ADDREF(*aParams = paramBlock);
    return rv;
}


NS_IMETHODIMP
nsXPInstallManager::GetDestinationFile(nsString& url, nsILocalFile* *file)
{
    NS_ENSURE_ARG_POINTER(file);
    nsresult rv;

    nsCOMPtr<nsIProperties> directoryService =
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILocalFile> temp;
    rv = directoryService->Get(NS_OS_TEMP_DIR,
                               NS_GET_IID(nsIFile),
                               getter_AddRefs(temp));
    NS_ENSURE_SUCCESS(rv, rv);

    temp->AppendNative(NS_LITERAL_CSTRING("tmp.xpi"));
    temp->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    *file = temp;
    NS_IF_ADDREF(*file);

    return NS_OK;
}

nsresult
nsXPInstallManager::CheckCert(nsIChannel* aChannel)
{
    nsCOMPtr<nsIURI> uri;
    nsresult rv = aChannel->GetOriginalURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString scheme;
    rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!scheme.Equals(NS_LITERAL_CSTRING("https")))
        return NS_OK;

    nsCOMPtr<nsISupports> security;
    rv = aChannel->GetSecurityInfo(getter_AddRefs(security));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsISSLStatusProvider> statusProvider(do_QueryInterface(security));
    NS_ENSURE_TRUE(statusProvider, NS_ERROR_FAILURE);

    rv = statusProvider->GetSSLStatus(getter_AddRefs(security));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsISSLStatus> status(do_QueryInterface(security));
    NS_ENSURE_TRUE(status, NS_ERROR_FAILURE);
    nsCOMPtr<nsIX509Cert> cert;
    rv = status->GetServerCert(getter_AddRefs(cert));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIX509Cert> issuer;
    rv = cert->GetIssuer(getter_AddRefs(issuer));
    NS_ENSURE_SUCCESS(rv, rv);
    bool equal;
    while (issuer && NS_SUCCEEDED(cert->Equals(issuer, &equal)) && !equal) {
        cert = issuer;
        rv = cert->GetIssuer(getter_AddRefs(issuer));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (issuer) {
        PRUint32 length;
        PRUnichar** tokenNames;
        nsCOMPtr<nsIX509Cert3> issuer2(do_QueryInterface(issuer));
        NS_ENSURE_TRUE(status, NS_ERROR_FAILURE);
        rv = issuer2->GetAllTokenNames(&length, &tokenNames);
        NS_ENSURE_SUCCESS(rv ,rv);
        for (PRUint32 i = 0; i < length; i++) {
            if (nsDependentString(tokenNames[i]).Equals(NS_LITERAL_STRING("Builtin Object Token")))
                return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXPInstallManager::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
    nsresult rv = NS_ERROR_FAILURE;

    // If we are dealing with a HTTP request, then treat HTTP error pages as
    // download failures.
    nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(request);
    if (httpChan) {
        // If we were chrome lauched check the certificate on the request
        if (mFromChrome && NS_FAILED(CheckCert(httpChan))) {
            request->Cancel(NS_BINDING_ABORTED);
            return NS_OK;
        }
        bool succeeded;
        if (NS_SUCCEEDED(httpChan->GetRequestSucceeded(&succeeded)) && !succeeded) {
            // HTTP response is not a 2xx!
            request->Cancel(NS_BINDING_ABORTED);
            return NS_OK;
        }
    }

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(request, nsnull, NS_BINDING_RETARGETED);

    NS_ASSERTION( mItem && mItem->mFile, "XPIMgr::OnStartRequest bad state");
    if ( mItem && mItem->mFile )
    {
        NS_ASSERTION( !mItem->mOutStream, "Received double OnStartRequest from Necko");

        rv = NS_NewLocalFileOutputStream(getter_AddRefs(mItem->mOutStream),
                                         mItem->mFile,
                                         PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                         0600);
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
            NS_ASSERTION( mItem->mOutStream, "XPIManager: output stream doesn't exist");
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
    if ( mItem && mItem->mOutStream )
    {
        mItem->mOutStream->Close();
        mItem->mOutStream = nsnull;
    }

    if (NS_FAILED(rv) || mCancelled)
    {
        // Download error!
        // -- first clean up partially downloaded file
        if ( mItem && mItem->mFile )
        {
            bool flagExists;
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
        if (mItem)
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
#define XPI_ODA_BUFFER_SIZE 8*1024
    PRUint32 amt = NS_MIN(XPI_ODA_BUFFER_SIZE, length);
    nsresult err;
    char buffer[XPI_ODA_BUFFER_SIZE];
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
        err = pIStream->Read(buffer, amt, &amt);

        if (amt == 0) break;
        if (NS_FAILED(err)) return err;

        err = mItem->mOutStream->Write( buffer, amt, &writeCount);
        if (NS_FAILED(err) || writeCount != amt)
        {
            return NS_ERROR_FAILURE;
        }
        length -= amt;

        amt = NS_MIN(XPI_ODA_BUFFER_SIZE, length);

    } while (length > 0);

    return NS_OK;
}


NS_IMETHODIMP
nsXPInstallManager::OnProgress(nsIRequest* request, nsISupports *ctxt, PRUint64 aProgress, PRUint64 aProgressMax)
{
    nsresult rv = NS_OK;

    if (mDlg && !mCancelled)
    {
        if (mContentLength < 1) {
            nsCOMPtr<nsIChannel> channel = do_QueryInterface(request,&rv);
            NS_ASSERTION(channel, "should have a channel");
            if (NS_FAILED(rv)) return rv;
            rv = channel->GetContentLength(&mContentLength);
            if (NS_FAILED(rv)) return rv;
        }
        // XXX once channels support that, use 64-bit contentlength
        rv = mDlg->OnProgress( mNextItem-1, aProgress, PRUint64(mContentLength) );
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
    if (eventSinkIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
        *_retval = nsnull;

        nsresult rv;
        nsCOMPtr<nsIWindowWatcher> ww(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIAuthPrompt> prompt;
        rv = ww->GetNewAuthPrompter(nsnull, getter_AddRefs(prompt));
        NS_ENSURE_SUCCESS(rv, rv);

        nsIAuthPrompt *p = prompt.get();
        NS_ADDREF(p);
        *_retval = p;
        return NS_OK;
    }
    else if (eventSinkIID.Equals(NS_GET_IID(nsIBadCertListener2))) {
        // If we aren't chrome triggered fall back to the default dialogs
        if (!mFromChrome)
            return NS_ERROR_NO_INTERFACE;
    }
    return QueryInterface(eventSinkIID, (void**)_retval);
}

// nsIChannelEventSink method
NS_IMETHODIMP
nsXPInstallManager::AsyncOnChannelRedirect(nsIChannel *oldChannel,
                                           nsIChannel *newChannel,
                                           PRUint32 flags,
                                           nsIAsyncVerifyRedirectCallback *callback)
{
    // Chrome triggered installs need to have their certificates checked
    if (mFromChrome) {
        nsresult rv = CheckCert(oldChannel);
        if (NS_FAILED(rv()))
            return rv;
    }

    callback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
}

// nsIBadCertListener2 methods
NS_IMETHODIMP
nsXPInstallManager::NotifyCertProblem(nsIInterfaceRequestor *socketInfo,
                                      nsISSLStatus *status,
                                      const nsACString &targetSite,
                                      bool *_retval)
{
    *_retval = PR_TRUE;
    return NS_OK;
}

// nsISSLErrorListener methods
NS_IMETHODIMP
nsXPInstallManager::NotifySSLError(nsIInterfaceRequestor *socketInfo, 
                                    PRInt32 error, 
                                    const nsACString &targetSite, 
                                    bool *_retval)
{
    *_retval = PR_TRUE;
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
    NS_NewURI(getter_AddRefs(uri), NS_ConvertUTF16toUTF8(item->mURL.get()).get());

    if (!uri || mChromeType != NOT_CHROME)
        return OnCertAvailable(uri, context, NS_ERROR_FAILURE, nsnull);

    nsIStreamListener* listener = new CertReader(uri, nsnull, this);
    if (!listener)
        return OnCertAvailable(uri, context, NS_ERROR_FAILURE, nsnull);

    NS_ADDREF(listener);
    nsresult rv = NS_OpenURI(listener, nsnull, uri, nsnull, mLoadGroup);

    NS_ASSERTION(NS_SUCCEEDED(rv), "OpenURI failed");
    NS_RELEASE(listener);

    if (NS_FAILED(rv))
        return OnCertAvailable(uri, context, NS_ERROR_FAILURE, nsnull);

    return NS_OK;
}

