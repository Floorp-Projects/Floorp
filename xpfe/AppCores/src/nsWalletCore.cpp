
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

#include "nsWalletCore.h"

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
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIAppShellServiceIID,      NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);

static void DOMWindowToWebShellWindow(nsIDOMWindow *DOMWindow, nsCOMPtr<nsIWebShellWindow> *webWindow);

//----------------------------------------------------------------------------------------
nsWalletCore::nsWalletCore()
//----------------------------------------------------------------------------------------
:    mPanelWindow(nsnull)
{
    printf("Created nsWalletCore\n");
}

//----------------------------------------------------------------------------------------
nsWalletCore::~nsWalletCore()
//----------------------------------------------------------------------------------------
{
    NS_IF_RELEASE(mPanelWindow);
}


NS_IMPL_ISUPPORTS_INHERITED(nsWalletCore, nsBaseAppCore, nsIDOMWalletCore)

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsWalletCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(nsnull != aScriptObject, "null arg");
    nsresult res = NS_OK;
    if (nsnull == mScriptObject) 
    {
            res = NS_NewScriptWalletCore(aContext, 
                                (nsISupports *)(nsIDOMWalletCore*)this, 
                                nsnull, 
                                &mScriptObject);
    }

    *aScriptObject = mScriptObject;
    return res;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsWalletCore::Init(const nsString& aId)
//----------------------------------------------------------------------------------------
{ 
    nsresult rv = nsBaseAppCore::Init(aId);
    return rv;
} // nsWalletCore::Init


static
nsIPresShell*
GetPresShellFor(nsIWebShell* aWebShell)
{
  nsIPresShell* shell = nsnull;
  if (nsnull != aWebShell) {
    nsIContentViewer* cv = nsnull;
    aWebShell->GetContentViewer(&cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIPresContext* cx;
        docv->GetPresContext(cx);
	      if (nsnull != cx) {
	        cx->GetShell(&shell);
	        NS_RELEASE(cx);
	      }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
  return shell;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsWalletCore::ShowWindow(nsIDOMWindow* aCurrentFrontWin, nsIDOMWindow* aForm)
//----------------------------------------------------------------------------------------
{
  NS_PRECONDITION(aForm != nsnull, "null ptr");
  if (! aForm)
    return NS_ERROR_NULL_POINTER;

  nsIPresShell* shell;
  shell = nsnull;
  nsCOMPtr<nsIWebShell> webcontent; 

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject; 
  scriptGlobalObject = do_QueryInterface(aForm); 
  scriptGlobalObject->GetWebShell(getter_AddRefs(webcontent)); 

  /* obtain the url */
  nsresult res;
  nsString urlString = nsString("");
  if (webcontent) {
    const PRUnichar *url = 0;
    PRInt32 history;
    res = webcontent->GetHistoryIndex(history);
    if (NS_SUCCEEDED(res)) {
      res = webcontent->GetURL( history, &url );
      if (NS_SUCCEEDED(res)) {
        urlString = nsString(url);
      }
    }
  }

  shell = GetPresShellFor(webcontent);
  nsIWalletService *walletservice;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if (NS_SUCCEEDED(res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_Prefill(shell, urlString, PR_FALSE);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
    if (NS_FAILED(res)) { /* this just means that there was nothing to prefill */
      return NS_OK;
    }
  } else {
    return res;
  }

    // (code adapted from nsToolkitCore::ShowModal. yeesh.)
    nsresult           rv;
    nsIAppShellService *appShell;
    nsIWebShellWindow  *window;

    window = nsnull;

    nsCOMPtr<nsIURL> urlObj;
    rv = NS_NewURL(getter_AddRefs(urlObj), "resource://res/samples/WalletPreview.html");
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
} // nsWalletCore::ShowWindow

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsWalletCore::ChangePanel(const nsString& aURL)
// Start loading of a new wallet panel.
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
NS_IMETHODIMP nsWalletCore::PanelLoaded(nsIDOMWindow* aWin)
// Callback after loading of a new wallet panel.
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
NS_IMETHODIMP nsWalletCore::SaveWallet(const nsString& results)
//----------------------------------------------------------------------------------------
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_PrefillReturn (results);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }

    return Close(mPanelWindow);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsWalletCore::CancelWallet()
//----------------------------------------------------------------------------------------
{
    return Close(mPanelWindow);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsWalletCore::GetPrefillList(nsString& aPrefillList)
//----------------------------------------------------------------------------------------
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_GetPrefillListForViewer(aPrefillList);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }

    return NS_OK;
}


