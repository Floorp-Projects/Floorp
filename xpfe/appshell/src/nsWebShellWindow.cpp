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


#include "nsWebShellWindow.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"

#include "nsGUIEvent.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIAppShell.h"

#include "nsIAppShellService.h"
#include "nsIWidgetController.h"
#include "nsAppShellCIDs.h"

// XXX: Only needed for the creation of the widget controller...
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"

/* Define Class IDs */
static NS_DEFINE_IID(kWindowCID,           NS_WINDOW_CID);
static NS_DEFINE_IID(kWebShellCID,         NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kAppShellServiceCID,  NS_APPSHELL_SERVICE_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWidgetIID,             NS_IWIDGET_IID);
static NS_DEFINE_IID(kIWebShellIID,           NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWebShellContainerIID,  NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIAppShellServiceIID,    NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kIWidgetControllerIID,   NS_IWIDGETCONTROLLER_IID);



#include "nsIWebShell.h"


nsWebShellWindow::nsWebShellWindow()
{
  NS_INIT_REFCNT();

  mWebShell = nsnull;
  mWindow   = nsnull;
  mController = nsnull;
}


nsWebShellWindow::~nsWebShellWindow()
{
  if (nsnull != mWebShell) {
    mWebShell->Destroy();
    NS_RELEASE(mWebShell);
  }

  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mController);
}


NS_IMPL_ADDREF(nsWebShellWindow);
NS_IMPL_RELEASE(nsWebShellWindow);

nsresult
nsWebShellWindow::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIWebShellContainerIID)) {
    *aInstancePtr = (void*)(nsIWebShellContainer*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return rv;
}


nsresult nsWebShellWindow::Initialize(nsIAppShell* aShell, nsIURL* aUrl, 
                                      nsString& aControllerIID)
{
  nsresult rv;

  nsString urlString;
  const char *tmpStr = NULL;
  nsIID iid;
  char str[40];

  aUrl->GetSpec(&tmpStr);
  urlString = tmpStr;

  // XXX: need to get the default window size from prefs...
  nsRect r(0, 0, 600, 600);
  
  nsWidgetInitData initData;


  if (nsnull == aUrl) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }

  // Create top level window
  rv = nsRepository::CreateInstance(kWindowCID, nsnull, kIWidgetIID,
                                    (void**)&mWindow);
  if (NS_OK != rv) {
    goto done;
  }

  initData.mBorderStyle = eBorderStyle_dialog;

  mWindow->SetClientData(this);
  mWindow->Create((nsIWidget*)nsnull,                 // Parent nsIWidget
                  r,                                  // Widget dimensions
                  nsWebShellWindow::HandleEvent,      // Event handler function
                  nsnull,                             // Device context
                  aShell,                             // Application shell
                  nsnull,                             // nsIToolkit
                  &initData);                         // Widget initialization data
  mWindow->GetClientBounds(r);
  mWindow->SetBackgroundColor(NS_RGB(192,192,192));

  // Create web shell
  rv = nsRepository::CreateInstance(kWebShellCID, nsnull,
                                    kIWebShellIID,
                                    (void**)&mWebShell);
  if (NS_OK != rv) {
    goto done;
  }

  r.x = r.y = 0;
  rv = mWebShell->Init(mWindow->GetNativeData(NS_NATIVE_WIDGET), 
                       r.x, r.y, r.width, r.height,
                       nsScrollPreference_kNeverScroll, 
                       PR_TRUE,                     // Allow Plugins 
                       PR_TRUE);
  mWebShell->SetContainer(this);
///  webShell->SetObserver((nsIStreamObserver*)this);
///  webShell->SetPrefs(aPrefs);

  mWebShell->LoadURL(urlString);
  mWebShell->Show();

  mWindow->Show(PR_TRUE);

  // Create the IWidgetController for the document...
  mController = nsnull; 
  aControllerIID.ToCString(str, sizeof(str));
  iid.Parse(str);

  rv = nsRepository::CreateInstance(iid, nsnull,
                                    kIWidgetControllerIID,
                                    (void**)&mController);
done:
  return rv;
}


/*
 * Event handler function...
 *
 * This function is called to process events for the nsIWidget of the 
 * nsWebShellWindow...
 */
nsEventStatus PR_CALLBACK
nsWebShellWindow::HandleEvent(nsGUIEvent *aEvent)
{
  nsresult rv;
  nsEventStatus result = nsEventStatus_eIgnore;
  nsIWebShell* webShell = nsnull;

  // Get the WebShell instance...
  if (nsnull != aEvent->widget) {
    void* data;

    aEvent->widget->GetClientData(data);
    if (nsnull != data) {
      webShell = ((nsWebShellWindow*)data)->mWebShell;
    }
  }

  if (nsnull != webShell) {
    switch(aEvent->message) {
      /*
       * For size events, the WebShell must be resized to fill the entire
       * client area of the window...
       */
      case NS_SIZE: {
        nsSizeEvent* sizeEvent = (nsSizeEvent*)aEvent;  
        webShell->SetBounds(0, 0, sizeEvent->windowSize->width, sizeEvent->windowSize->height);
        result = nsEventStatus_eConsumeNoDefault;
        break;
      }
      /*
       * Notify the ApplicationShellService that the window is being closed...
       */
      case NS_DESTROY: {
        nsIAppShellService* appShell;

        rv = nsServiceManager::GetService(kAppShellServiceCID,
                                          kIAppShellServiceIID,
                                          (nsISupports**)&appShell);
        if (NS_SUCCEEDED(rv)) {
          appShell->CloseTopLevelWindow(aEvent->widget);
          
          nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
        }
        break;
      }
    }
  }
  return nsEventStatus_eIgnore;
}


NS_IMETHODIMP 
nsWebShellWindow::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL,
                              nsLoadType aReason)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL,
                                  PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL,
                             PRInt32 aStatus)
{
  nsresult rv;

  if (nsnull != mController) {
    static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);
    static NS_DEFINE_IID(kIDOMDocumentIID,    NS_IDOMDOCUMENT_IID);

    nsIContentViewer* viewer;
    nsIDocumentViewer* docViewer;
    nsIDocument* document;
    nsIDOMDocument* domDocument;

    mWebShell->GetContentViewer(&viewer);
    rv = viewer->QueryInterface(kIDocumentViewerIID, (void**)&docViewer);
    if (NS_SUCCEEDED(rv)) {
      rv = docViewer->GetDocument(document);
      if (NS_SUCCEEDED(rv)) {
        rv = document->QueryInterface(kIDOMDocumentIID, (void**)&domDocument);
        if (NS_SUCCEEDED(rv)) {
          rv = mController->Initialize(domDocument, mWebShell);
          NS_RELEASE(domDocument);
        }
        NS_RELEASE(document);
      }
      NS_RELEASE(docViewer);
    }
    NS_RELEASE(viewer);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::NewWebShell(PRUint32 aChromeMask, PRBool aVisible,
                              nsIWebShell *&aNewWebShell)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebShellWindow::FindWebShellWithName(const PRUnichar* aName,
                                                     nsIWebShell*& aResult)
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::FocusAvailable(nsIWebShell* aFocusedWebShell)
{
  return NS_OK;
}

