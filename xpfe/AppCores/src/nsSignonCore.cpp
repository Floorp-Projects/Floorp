
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

#include "nsSignonCore.h"

#include "nsIWalletService.h"

#include "nsIURL.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsAppCores.h"
#include "nsAppCoresCIDs.h"
#include "nsAppShellCIDs.h"
#include "nsAppCoresManager.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"

#include "nsIScriptGlobalObject.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMWindow.h"
#include "nsIWebShellWindow.h"

#include "plstr.h"
#include "prmem.h"

#include <ctype.h>

static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIAppShellServiceIID,      NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);

static void DOMWindowToWebShellWindow(nsIDOMWindow *DOMWindow, nsCOMPtr<nsIWebShellWindow> *webWindow);

//----------------------------------------------------------------------------------------
nsSignonCore::nsSignonCore()
//----------------------------------------------------------------------------------------
:    mPanelWindow(nsnull)
{
    printf("Created nsSignonCore\n");
}

//----------------------------------------------------------------------------------------
nsSignonCore::~nsSignonCore()
//----------------------------------------------------------------------------------------
{
    NS_IF_RELEASE(mPanelWindow);
}


NS_IMPL_ISUPPORTS_INHERITED(nsSignonCore, nsBaseAppCore, nsIDOMSignonCore)

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(nsnull != aScriptObject, "null arg");
    nsresult res = NS_OK;
    if (nsnull == mScriptObject) 
    {
            res = NS_NewScriptSignonCore(aContext, 
                                (nsISupports *)(nsIDOMSignonCore*)this, 
                                nsnull, 
                                &mScriptObject);
    }

    *aScriptObject = mScriptObject;
    return res;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCore::Init(const nsString& aId)
//----------------------------------------------------------------------------------------
{ 
    nsresult rv = nsBaseAppCore::Init(aId);
    return rv;
} // nsSignonCore::Init

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCore::ShowWindow(nsIDOMWindow* aCurrentFrontWin)
//----------------------------------------------------------------------------------------
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->SI_DisplaySignonInfoAsHTML();
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }

    // (code adapted from nsToolkitCore::ShowModal. yeesh.)
    nsresult           rv;
    nsIAppShellService *appShell;
    nsIWebShellWindow  *window;

    window = nsnull;

    nsCOMPtr<nsIURL> urlObj;
    rv = NS_NewURL(getter_AddRefs(urlObj), "resource://res/samples/htmldlgs.htm");
    if (NS_FAILED(rv))
        return rv;

    rv = nsServiceManager::GetService(kAppShellServiceCID, kIAppShellServiceIID,
                                    (nsISupports**) &appShell);
    if (NS_FAILED(rv))
        return rv;

    // Create "save to disk" nsIXULCallbacks...
    //nsIXULWindowCallbacks *cb = new nsFindDialogCallbacks( aURL, aContentType );
    nsIXULWindowCallbacks *cb = nsnull;

    nsCOMPtr<nsIWebShellWindow> parent;
    DOMWindowToWebShellWindow(aCurrentFrontWin, &parent);
    appShell->CreateDialogWindow(parent, urlObj, PR_TRUE, window,
                                 nsnull, cb, 504, 436);
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);

    if (window != nsnull) {
        nsCOMPtr<nsIWidget> parentWindowWidgetThing;
        nsresult gotParent;
        gotParent = parent ? parent->GetWidget(*getter_AddRefs(parentWindowWidgetThing)) :
                             NS_ERROR_FAILURE;
        // Windows OS is the only one that needs the parent disabled, or cares
        // arguably this should be done by the new window, within ShowModal...
        if (NS_SUCCEEDED(gotParent))
            parentWindowWidgetThing->Enable(PR_FALSE);
        window->ShowModal();
        if (NS_SUCCEEDED(gotParent))
            parentWindowWidgetThing->Enable(PR_TRUE);
    }

    return rv;
} // nsSignonCore::ShowWindow

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCore::ChangePanel(const nsString& aURL)
// Start loading of a new signon panel.
//----------------------------------------------------------------------------------------
{
    NS_ASSERTION(mPanelWindow, "panel window is null");
    if (!mPanelWindow)
        return NS_OK;

    nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(mPanelWindow));
    if (!globalScript)
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIWebShell> webshell;
    globalScript->GetWebShell(getter_AddRefs(webshell));
    if (!webshell)
        return NS_ERROR_FAILURE;
    webshell->LoadURL(aURL.GetUnicode());
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCore::PanelLoaded(nsIDOMWindow* aWin)
// Callback after loading of a new signon panel.
//----------------------------------------------------------------------------------------
{
    // Out with the old!
    
    if (mPanelWindow != aWin)
    {
        NS_IF_RELEASE(mPanelWindow);
        mPanelWindow = aWin;
        NS_IF_ADDREF(mPanelWindow);
    }
    return NS_OK;
}


//----------------------------------------------------------------------------------------    
static void DOMWindowToWebShellWindow(
              nsIDOMWindow *DOMWindow,
              nsCOMPtr<nsIWebShellWindow> *webWindow)
//----------------------------------------------------------------------------------------    
{
  if (!DOMWindow)
    return; // with webWindow unchanged -- its constructor gives it a null ptr

  nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(DOMWindow));
  nsCOMPtr<nsIWebShell> webshell, rootWebshell;
  if (globalScript)
    globalScript->GetWebShell(getter_AddRefs(webshell));
  if (webshell)
    webshell->GetRootWebShellEvenIfChrome(*getter_AddRefs(rootWebshell));
  if (rootWebshell) {
    nsCOMPtr<nsIWebShellContainer> webshellContainer;
    rootWebshell->GetContainer(*getter_AddRefs(webshellContainer));
    *webWindow = do_QueryInterface(webshellContainer);
  }
}


//----------------------------------------------------------------------------------------
static nsresult Close(nsIDOMWindow*& dw)
//----------------------------------------------------------------------------------------    
{
    if (!dw)
        return NS_ERROR_FAILURE;
    nsIDOMWindow* top;
    dw->GetTop(&top);
    if (!top)
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIWebShellWindow> parent;
    DOMWindowToWebShellWindow(top, &parent);
    if (parent)
        parent->Close();
    NS_IF_RELEASE(dw);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCore::SaveSignon(const nsString& results)
//----------------------------------------------------------------------------------------
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->SI_SignonViewerReturn (results);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }

    return Close(mPanelWindow);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCore::CancelSignon()
//----------------------------------------------------------------------------------------
{
    return Close(mPanelWindow);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCore::GetSignonList(nsString& aSignonList)
//----------------------------------------------------------------------------------------
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->SI_GetSignonListForViewer(aSignonList);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }

    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCore::GetRejectList(nsString& aRejectList)
//----------------------------------------------------------------------------------------
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->SI_GetRejectListForViewer(aRejectList);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }

    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCore::GetNopreviewList(nsString& aNopreviewList)
//----------------------------------------------------------------------------------------
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_GetNopreviewListForViewer(aNopreviewList);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }

    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCore::GetNocaptureList(nsString& aNocaptureList)
//----------------------------------------------------------------------------------------
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_GetNocaptureListForViewer(aNocaptureList);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }

    return NS_OK;
}


