
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

#include "nsPrefsCore.h"

#include "nsIURL.h"
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
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIWebShellWindow.h"

// Globals
static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPrefsCoreIID,            NS_IDOMPREFSCORE_IID);

static NS_DEFINE_IID(kIDOMDocumentIID,          nsIDOMDocument::GetIID());
static NS_DEFINE_IID(kIDocumentIID,             nsIDocument::GetIID());
static NS_DEFINE_IID(kIAppShellServiceIID,      NS_IAPPSHELL_SERVICE_IID);

static NS_DEFINE_IID(kPrefsCoreCID,             NS_PREFSCORE_CID);
static NS_DEFINE_IID(kBrowserWindowCID,         NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);

//----------------------------------------------------------------------------------------
nsPrefsCore::nsPrefsCore()
//----------------------------------------------------------------------------------------
:    mTreeScriptContext(nsnull)
,    mPanelScriptContext(nsnull)
,    mTreeWindow(nsnull)
,    mPanelWindow(nsnull)
{
    
    printf("Created nsPrefsCore\n");
    
}

//----------------------------------------------------------------------------------------
nsPrefsCore::~nsPrefsCore()
//----------------------------------------------------------------------------------------
{
    NS_IF_RELEASE(mTreeScriptContext);
    NS_IF_RELEASE(mPanelScriptContext);

    NS_IF_RELEASE(mTreeWindow);
    NS_IF_RELEASE(mPanelWindow);
}


NS_IMPL_ISUPPORTS_INHERITED(nsPrefsCore, nsBaseAppCore, nsIDOMPrefsCore)

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(nsnull != aScriptObject, "null arg");
    nsresult res = NS_OK;
    if (nsnull == mScriptObject) 
    {
            res = NS_NewScriptPrefsCore(aContext, 
                                (nsISupports *)(nsIDOMPrefsCore*)this, 
                                nsnull, 
                                &mScriptObject);
    }

    *aScriptObject = mScriptObject;
    return res;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::Init(const nsString& aId)
//----------------------------------------------------------------------------------------
{ 
    return nsBaseAppCore::Init(aId);
}

#if 0
//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::SetPrefsTree(nsIDOMWindow* aWin)
//----------------------------------------------------------------------------------------
{
    if (!aWin)
       return NS_OK;
    mTreeWindow = aWin;
    NS_ADDREF(aWin);
    mTreeScriptContext = GetScriptContext(aWin);
	return NS_OK;
}
#endif // 0

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::ShowWindow(nsIDOMWindow* /*aCurrentFrontWin*/)
//----------------------------------------------------------------------------------------
{
    // Get app shell service.
    nsIAppShellService *appShell;
    nsresult rv = nsServiceManager::GetService(kAppShellServiceCID,
                                      kIAppShellServiceIID,
                                      (nsISupports**)&appShell);

    if (NS_FAILED(rv))
        return rv;

    nsString controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
    nsIWebShellWindow *newWindow;

    nsIURL *url;
    rv = NS_NewURL(&url, "resource:/res/samples/PrefsWindow.html");

    if (NS_FAILED(rv))
        return rv;

    // Create "save to disk" nsIXULCallbacks...
    //nsIXULWindowCallbacks *cb = new nsFindDialogCallbacks( aURL, aContentType );
    nsIXULWindowCallbacks *cb = nsnull;

    rv = appShell->CreateTopLevelWindow( nsnull,
                                         url,
                                         controllerCID,
                                         newWindow,
                                         nsnull,
                                         cb,
                                         504,
                                         346 );
    NS_RELEASE(url);
    return rv;
} // nsPrefsCore::ShowPrefsWindow

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::ChangePanel(const nsString& aURL)
// Start loading of a new prefs panel.
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
    webshell->LoadURL(aURL);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::PanelLoaded(nsIDOMWindow* aWin)
// Callback after loading of a new prefs panel.
//----------------------------------------------------------------------------------------
{
    if (mPanelWindow == aWin)
        return NS_OK;
    if (mPanelWindow)
        NS_RELEASE(mPanelWindow);
    mPanelWindow = aWin;
    if (!aWin)
        return NS_OK;
    NS_ADDREF(aWin);
    mPanelScriptContext = GetScriptContext(aWin);
	return NS_OK;
}

//----------------------------------------------------------------------------------------
static nsCOMPtr<nsIWebShellWindow>
	DOMWindowToWebShellWindow(nsIDOMWindow *DOMWindow)
// horribly complicated routine simply to convert from one to the other
//----------------------------------------------------------------------------------------	
{
    nsCOMPtr<nsIWebShellWindow> webWindow;
    nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(DOMWindow));
    nsCOMPtr<nsIWebShell> webshell;
    if (globalScript)
        globalScript->GetWebShell(getter_AddRefs(webshell));
    if (webshell)
    {
        nsCOMPtr<nsIWebShellContainer> webshellContainer;
        webshell->GetContainer(*getter_AddRefs(webshellContainer));
        webWindow = do_QueryInterface(webshellContainer);
    }
    return webWindow;
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
    nsCOMPtr<nsIWebShellWindow> parent = DOMWindowToWebShellWindow(top);
    if (parent)
        parent->Close();
    NS_IF_RELEASE(dw);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::SavePrefs()
//----------------------------------------------------------------------------------------
{
	// Do the prefs stuff...
	
	// Then close    
	return Close(mPanelWindow);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::CancelPrefs()
//----------------------------------------------------------------------------------------
{
	// Do the prefs stuff...
	
	// Then close    
	return Close(mPanelWindow);
}
