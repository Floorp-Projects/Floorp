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


#include "nsAppCoresManager.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsIDOMBaseAppCore.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "nsIURL.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIWidget.h"
#include "nsToolkitCore.h"

#include "nsIXULWindowCallbacks.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"

class nsIScriptContext;

static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kIAppShellServiceIID, NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kIDOMBaseAppCoreIID, NS_IDOMBASEAPPCORE_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIToolkitCoreIID, NS_IDOMTOOLKITCORE_IID);



/////////////////////////////////////////////////////////////////////////
// nsToolkitCore
/////////////////////////////////////////////////////////////////////////

nsToolkitCore::nsToolkitCore() {

  printf("Created nsToolkitCore\n");

  IncInstanceCount();
  NS_INIT_REFCNT();
}

nsToolkitCore::~nsToolkitCore() {

  DecInstanceCount();  
}


NS_IMPL_ADDREF(nsToolkitCore)
NS_IMPL_RELEASE(nsToolkitCore)


NS_IMETHODIMP 
nsToolkitCore::QueryInterface(REFNSIID aIID, void** aInstancePtr) {

  if (aInstancePtr == NULL)
    return NS_ERROR_NULL_POINTER;

  *aInstancePtr = NULL;

  if (aIID.Equals(kIToolkitCoreIID)) {
    *aInstancePtr = (void*) ((nsIDOMToolkitCore*) this);
    AddRef();
    return NS_OK;
  }
 
  return nsBaseAppCore::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP 
nsToolkitCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject) {
  nsresult rv = NS_OK;

  NS_PRECONDITION(aScriptObject != nsnull, "null arg");
  if (mScriptObject == nsnull) {
      nsISupports *core;
      rv = QueryInterface(kISupportsIID, (void **)&core);
      if (NS_SUCCEEDED(rv)) {
        rv = NS_NewScriptToolkitCore(aContext, 
                                     (nsISupports *) core,
                                     nsnull, 
                                     &mScriptObject);
        NS_RELEASE(core);
      }
  }

  *aScriptObject = mScriptObject;
  return rv;
}


NS_IMETHODIMP    
nsToolkitCore::Init(const nsString& aId) {

  nsresult rv;

  nsBaseAppCore::Init(aId);

  nsIDOMBaseAppCore *core;
  rv = QueryInterface(kIDOMBaseAppCoreIID, (void **)&core);
  if (NS_SUCCEEDED(rv)) {
    nsAppCoresManager* sdm = new nsAppCoresManager();
    if (sdm) {
      sdm->Add(core);
      delete sdm;
      return NS_OK;
    } else
      rv = NS_ERROR_OUT_OF_MEMORY;
    NS_RELEASE(core);
  }
  return rv;
}


NS_IMETHODIMP
nsToolkitCore::ShowDialog(const nsString& aUrl, nsIDOMWindow* aParent) {

  nsresult           rv;
  nsString           controllerCID;
  nsIAppShellService *appShell;
  nsIWebShellWindow  *window;

  window = nsnull;

  nsCOMPtr<nsIURL> urlObj;
  rv = NS_NewURL(getter_AddRefs(urlObj), aUrl);
  if (NS_FAILED(rv))
    return rv;

  rv = nsServiceManager::GetService(kAppShellServiceCID, kIAppShellServiceIID,
                                    (nsISupports**) &appShell);
  if (NS_FAILED(rv))
    return rv;

  // hardwired temporary hack.  See nsAppRunner.cpp at main()
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";

  nsCOMPtr<nsIWebShellWindow> parent = DOMWindowToWebShellWindow(aParent);
  appShell->CreateDialogWindow(parent, urlObj, controllerCID, window,
                               nsnull, nsnull, 615, 480);
  nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);

  if (window != nsnull)
    window->Show(PR_TRUE);

  return rv;
}

NS_IMETHODIMP
nsToolkitCore::ShowWindow(const nsString& aUrl, nsIDOMWindow* aParent) {

  nsresult           rv;
  nsString           controllerCID;
  nsIAppShellService *appShell;
  nsIWebShellWindow  *window;

  window = nsnull;

  nsCOMPtr<nsIURL> urlObj;
  rv = NS_NewURL(getter_AddRefs(urlObj), aUrl);
  if (NS_FAILED(rv))
    return rv;

  rv = nsServiceManager::GetService(kAppShellServiceCID, kIAppShellServiceIID,
                                    (nsISupports**) &appShell);
  if (NS_FAILED(rv))
    return rv;

  // hardwired temporary hack.  See nsAppRunner.cpp at main()
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";

  nsCOMPtr<nsIWebShellWindow> parent = DOMWindowToWebShellWindow(aParent);
  appShell->CreateTopLevelWindow(parent, urlObj, controllerCID, window,
                               nsnull, nsnull, 615, 480);
  nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);

  if (window != nsnull)
    window->Show(PR_TRUE);

  return rv;
}

struct nsArgCallbacks : public nsIXULWindowCallbacks {
    // Declare implementation of ISupports stuff.
    NS_DECL_ISUPPORTS

    // Declare implementations of nsIXULWindowCallbacks interface functions.
    NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell);
    NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell) { return NS_OK; }

    // Specifics...
    nsArgCallbacks( const nsString& aArgs ) : mArgs( aArgs ) { NS_INIT_REFCNT(); }
    nsArgCallbacks() { NS_INIT_REFCNT(); }
private:
    nsString mArgs;
}; // nsArgCallbacks

// Implement ISupports stuff.
NS_IMPL_ISUPPORTS( nsArgCallbacks, nsIXULWindowCallbacks::GetIID() );

static const int APP_DEBUG = 0;
static nsresult setAttribute( nsIWebShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value ) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsIContentViewer> cv;
    rv = shell ? shell->GetContentViewer(getter_AddRefs(cv))
               : NS_ERROR_NULL_POINTER;
    if ( cv ) {
        // Up-cast.
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
        if ( docv ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(*getter_AddRefs(doc));
            if ( doc ) {
                // Up-cast.
                nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) );
                if ( xulDoc ) {
                    // Find specified element.
                    nsCOMPtr<nsIDOMElement> elem;
                    rv = xulDoc->GetElementById( id, getter_AddRefs(elem) );
                    if ( elem ) {
                        // Set the text attribute.
                        rv = elem->SetAttribute( name, value );
                        if ( APP_DEBUG ) {
                            char *p = value.ToNewCString();
                            delete [] p;
                        }
                        if ( rv != NS_OK ) {
                             if (APP_DEBUG) printf("SetAttribute failed, rv=0x%X\n",(int)rv);
                        }
                    } else {
                        if (APP_DEBUG) printf("GetElementByID failed, rv=0x%X\n",(int)rv);
                    }
                } else {
                  if (APP_DEBUG)   printf("Upcast to nsIDOMXULDocument failed\n");
                }
            } else {
                if (APP_DEBUG) printf("GetDocument failed, rv=0x%X\n",(int)rv);
            }
        } else {
             if (APP_DEBUG) printf("Upcast to nsIDocumentViewer failed\n");
        }
    } else {
        if (APP_DEBUG) printf("GetContentViewer failed, rv=0x%X\n",(int)rv);
    }
    return rv;
}

// Stick the arg in the document.
NS_IMETHODIMP
nsArgCallbacks::ConstructBeforeJavaScript( nsIWebShell *aWebShell ) {
    nsresult rv = NS_OK;
    setAttribute( aWebShell, "args", "value", mArgs );
    return rv;
}

NS_IMETHODIMP
nsToolkitCore::ShowWindowWithArgs(const nsString& aUrl,
                                  nsIDOMWindow* aParent,
                                  const nsString& aArgs) {

  nsresult           rv;
  nsString           controllerCID;
  nsIAppShellService *appShell;
  nsIWebShellWindow  *window;

  window = nsnull;

  nsCOMPtr<nsIURL> urlObj;
  rv = NS_NewURL(getter_AddRefs(urlObj), aUrl);
  if (NS_FAILED(rv))
    return rv;

  rv = nsServiceManager::GetService(kAppShellServiceCID, kIAppShellServiceIID,
                                    (nsISupports**) &appShell);
  if (NS_FAILED(rv))
    return rv;

  // hardwired temporary hack.  See nsAppRunner.cpp at main()
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";

  nsCOMPtr<nsIWebShellWindow> parent = DOMWindowToWebShellWindow(aParent);
  nsCOMPtr<nsArgCallbacks> cb;
  cb = nsDontQueryInterface<nsArgCallbacks>( new nsArgCallbacks( aArgs ) );
  appShell->CreateTopLevelWindow(parent, urlObj, controllerCID, window,
                               nsnull, cb, 615, 650);
  nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);

  if (window != nsnull)
    window->Show(PR_TRUE);

  return rv;
}

NS_IMETHODIMP
nsToolkitCore::ShowModalDialog(const nsString& aUrl, nsIDOMWindow* aParent) {

  nsresult           rv;
  nsString           controllerCID;
  nsIAppShellService *appShell;
  nsIWebShellWindow  *window;

  window = nsnull;

  nsCOMPtr<nsIURL> urlObj;
  rv = NS_NewURL(getter_AddRefs(urlObj), aUrl);
  if (NS_FAILED(rv))
    return rv;

  rv = nsServiceManager::GetService(kAppShellServiceCID, kIAppShellServiceIID,
                                    (nsISupports**) &appShell);
  if (NS_FAILED(rv))
    return rv;

  // hardwired temporary hack.  See nsAppRunner.cpp at main()
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";

  nsCOMPtr<nsIWebShellWindow> parent = DOMWindowToWebShellWindow(aParent);
  appShell->CreateDialogWindow(parent, urlObj, controllerCID, window,
                               nsnull, nsnull, 615, 480);
  nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);

  if (window != nsnull) {
    window->ShowModal();
  }

  return rv;
}

NS_IMETHODIMP
nsToolkitCore::CloseWindow(nsIDOMWindow* aWindow) {

  nsCOMPtr<nsIWebShellWindow> window = DOMWindowToWebShellWindow(aWindow);
  if (window)
    window->Close();

  return NS_OK;
}

// horribly complicated routine to simply convert from one to the other
nsCOMPtr<nsIWebShellWindow>
nsToolkitCore::DOMWindowToWebShellWindow(nsIDOMWindow *DOMWindow) const {

  nsCOMPtr<nsIWebShellWindow> webWindow;

  nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(DOMWindow));
  nsCOMPtr<nsIWebShell> webshell;
  if (globalScript)
    globalScript->GetWebShell(getter_AddRefs(webshell));
  if (webshell) {
    nsCOMPtr<nsIWebShellContainer> webshellContainer;
    webshell->GetContainer(*getter_AddRefs(webshellContainer));
    webWindow = do_QueryInterface(webshellContainer);
  }
  return webWindow;
}
