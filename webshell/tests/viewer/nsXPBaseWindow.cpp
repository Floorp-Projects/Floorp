/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "prmem.h"

#ifdef XP_MAC
#include "nsXPBaseWindow.h"
#define NS_IMPL_IDS
#else
#define NS_IMPL_IDS
#include "nsXPBaseWindow.h"
#endif

#include "nsCOMPtr.h"
#include "nsIPrompt.h"
#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsIDOMDocument.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"
#include "nsCRT.h"
#include "nsWidgetsCID.h"
#include "nsViewerApp.h"

#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIDocumentViewer.h"
#include "nsIContentViewer.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIDocument.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIWindowListener.h"
#include "nsIBaseWindow.h"
#include "nsIWebNavigation.h"
#include "nsIViewManager.h"
#include "nsGUIEvent.h"

#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsXPIDLString.h"


#if defined(WIN32) || defined(XP_OS2_VACPP)
#include <strstrea.h>
#else
#include <strstream.h>
#endif

#include <ctype.h> // tolower

// XXX For font setting below
#include "nsFont.h"
//#include "nsUnitConversion.h"
//#include "nsIDeviceContext.h"

static NS_DEFINE_IID(kXPBaseWindowCID, NS_XPBASE_WINDOW_CID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID);


static NS_DEFINE_IID(kIXPBaseWindowIID, NS_IXPBASE_WINDOW_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

static NS_DEFINE_IID(kIDOMMouseListenerIID,   NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,   NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);

//----------------------------------------------------------------------
nsXPBaseWindow::nsXPBaseWindow() :
  mContentRoot(nsnull),
  mWindowListener(nsnull),
  mDocIsLoaded(PR_FALSE),
  mAppShell(nsnull)
{
}

//----------------------------------------------------------------------
nsXPBaseWindow::~nsXPBaseWindow()
{
  NS_IF_RELEASE(mContentRoot);
  NS_IF_RELEASE(mAppShell);
}

//----------------------------------------------------------------------
NS_IMPL_ADDREF(nsXPBaseWindow)
NS_IMPL_RELEASE(nsXPBaseWindow)

//----------------------------------------------------------------------
nsresult nsXPBaseWindow::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtrResult = NULL;

  if (aIID.Equals(kIXPBaseWindowIID)) {
    *aInstancePtrResult = (void*) ((nsIXPBaseWindow*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIWebShellContainerIID)) {
    *aInstancePtrResult = (void*) ((nsIWebShellContainer*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMMouseListenerIID)) {
    NS_ADDREF_THIS(); // Increase reference count for caller
    *aInstancePtrResult = (void *)((nsIDOMMouseListener*)this);
    return NS_OK;
  }
  
  if (aIID.Equals(kISupportsIID)) {
    NS_ADDREF_THIS();
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIXPBaseWindow*)this));
    return NS_OK;
  }

  return NS_NOINTERFACE;
}


//----------------------------------------------------------------------
static nsEventStatus PR_CALLBACK
HandleXPDialogEvent(nsGUIEvent *aEvent)
{ 
  nsEventStatus result = nsEventStatus_eIgnore;

  nsXPBaseWindow* baseWin;
  aEvent->widget->GetClientData(((void*&)baseWin));

  if (nsnull != baseWin) {
    nsSizeEvent* sizeEvent;
    switch(aEvent->message) {
      case NS_SIZE:
        sizeEvent = (nsSizeEvent*)aEvent;  
        baseWin->Layout(sizeEvent->windowSize->width,
                        sizeEvent->windowSize->height);
        result = nsEventStatus_eConsumeNoDefault;
        break;

    case NS_DESTROY: {
        //nsViewerApp* app = baseWin->mApp;
        result = nsEventStatus_eConsumeDoDefault;
        baseWin->Close();
        NS_RELEASE(baseWin);
      }
      return result;

    default:
      break;
    }
    //NS_RELEASE(baseWin);
  }
  return result;
}


//----------------------------------------------------------------------
nsresult nsXPBaseWindow::Init(nsXPBaseWindowType aType,
                              nsIAppShell*       aAppShell,
                              const nsString&    aDialogURL,
                              const nsString&    aTitle,
                              const nsRect&      aBounds,
                              PRUint32           aChromeMask,
                              PRBool             aAllowPlugins)
{
  mAllowPlugins = aAllowPlugins;
  mWindowType   = aType;
  mAppShell     = aAppShell;
  NS_IF_ADDREF(mAppShell);

  // Create top level window
  nsresult rv;
  rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID,
                                             (void**)&mWindow);

  if (NS_OK != rv) {
    return rv;
  }

  mWindow->SetClientData(this);

  nsWidgetInitData initData;
  initData.mWindowType = eWindowType_toplevel;
  initData.mBorderStyle = eBorderStyle_default;

  nsRect r(0, 0, aBounds.width, aBounds.height);
  mWindow->Create((nsIWidget*)NULL, r, HandleXPDialogEvent,
                  nsnull, aAppShell, nsnull, &initData);
  mWindow->GetBounds(r);

  // Create web shell
  rv = nsComponentManager::CreateInstance(kWebShellCID, nsnull,
                                    kIWebShellIID,
                                    (void**)&mWebShell);
  if (NS_OK != rv) {
    return rv;
  }
  r.x = r.y = 0;
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
  docShell->SetAllowPlugins(aAllowPlugins);
  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mWebShell));

  rv = docShellWin->InitWindow(nsnull, mWindow, r.x, r.y, r.width, r.height);
  docShellWin->Create();
  mWebShell->SetContainer((nsIWebShellContainer*) this);
  docShellWin->SetVisibility(PR_TRUE);

  // Now lay it all out
  Layout(r.width, r.height);

  // Load URL to Load GUI
  mDialogURL = aDialogURL;
  LoadURL(mDialogURL);

  SetTitle(aTitle.get());

  return NS_OK;
}

//----------------------------------------------------------------------
void nsXPBaseWindow::ForceRefresh()
{
  nsIPresShell* shell;
  GetPresShell(shell);
  if (nsnull != shell) {
    nsCOMPtr<nsIViewManager> vm;
    shell->GetViewManager(getter_AddRefs(vm));
    if (vm) {
      nsIView* root;
      vm->GetRootView(root);
      if (nsnull != root) {
        vm->UpdateView(root, NS_VMREFRESH_IMMEDIATE | NS_VMREFRESH_AUTO_DOUBLE_BUFFER);
      }
    }
    NS_RELEASE(shell);
  }
}

//----------------------------------------------------------------------
void nsXPBaseWindow::Layout(PRInt32 aWidth, PRInt32 aHeight)
{
  nsRect rr(0, 0, aWidth, aHeight);
  nsCOMPtr<nsIBaseWindow> webShellWin(do_QueryInterface(mWebShell));
  webShellWin->SetPositionAndSize(rr.x, rr.y, rr.width, rr.height, PR_FALSE);
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::SetLocation(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Move(aX, aY);
  return NS_OK;
}


//----------------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::SetDimensions(PRInt32 aWidth, PRInt32 aHeight)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");

  // XXX We want to do this in one shot
  mWindow->Resize(aWidth, aHeight, PR_FALSE);
  Layout(aWidth, aHeight);

  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::GetBounds(nsRect& aBounds)
{
  mWindow->GetBounds(aBounds);
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP
nsXPBaseWindow::GetWindowBounds(nsRect& aBounds)
{
  //XXX This needs to be non-client bounds when it exists.
  mWindow->GetBounds(aBounds);
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::SetVisible(PRBool aIsVisible)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Show(aIsVisible);
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::Close()
{
  if (nsnull != mWindowListener) {
    mWindowListener->Destroy(this);
  }

  if (nsnull != mWebShell) {
    nsCOMPtr<nsIBaseWindow> webShellWin(do_QueryInterface(mWebShell));
    webShellWin->Destroy();
    NS_RELEASE(mWebShell);
  }

  if (nsnull != mWindow) {
    nsIWidget* w = mWindow;
    NS_RELEASE(w);
  }

  return NS_OK;
}


//----------------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::GetWebShell(nsIWebShell*& aResult)
{
  aResult = mWebShell;
  NS_IF_ADDREF(mWebShell);
  return NS_OK;
}

//---------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::SetTitle(const PRUnichar* aTitle)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mTitle = aTitle;
  nsAutoString newTitle(aTitle);
  mWindow->SetTitle(newTitle);
  return NS_OK;
}

//---------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::GetTitle(const PRUnichar** aResult)
{
  *aResult = mTitle.ToNewUnicode();
  return NS_OK;
}

//---------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::LoadURL(const nsString& aURL)
{
   nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebShell));
   webNav->LoadURI(aURL.get(), nsIWebNavigation::LOAD_FLAGS_NONE);
   return NS_OK;
}

//---------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason)
{
  return NS_OK;
}

//-----------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
  return NS_OK;
}

//-----------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_OK;
}

//-----------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsresult aStatus)
{
  // Find the Root Conent Node for this Window
  nsIPresShell* shell;
  GetPresShell(shell);
  if (nsnull != shell) {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      doc->GetRootContent(&mContentRoot);
      mDocIsLoaded = PR_TRUE;
      if (nsnull != mWindowListener) {
        mWindowListener->Initialize(this);
      }
    }
    NS_RELEASE(shell);
  }

  return NS_OK;
}

//-----------------------------------------------------------------

NS_IMETHODIMP
nsXPBaseWindow::FindWebShellWithName(const PRUnichar* aName,
                                     nsIWebShell*& aResult)
{
  aResult = nsnull;

  nsString aNameStr(aName);

  nsCOMPtr<nsIWebShell> webShell;
  GetWebShell(*getter_AddRefs(webShell));
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(webShell));
  if (webShell) {
    nsXPIDLString name;
    if (NS_SUCCEEDED(docShellAsItem->GetName(getter_Copies(name)))) {
      if (aNameStr.Equals(name)) {
        aResult = webShell;
        NS_ADDREF(aResult);
        return NS_OK;
      }
    }      

    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(webShell));
    nsCOMPtr<nsIDocShellTreeItem> result;
    if (NS_OK == docShellAsNode->FindChildWithName(aName, PR_TRUE, PR_FALSE,
      nsnull, getter_AddRefs(result))) {
      if (result) {
        CallQueryInterface(result, &aResult);
        return NS_OK;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXPBaseWindow::FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken)
{
  return NS_OK;
}

//-----------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::AddEventListener(nsIDOMNode * aNode)
{
  nsIDOMEventReceiver * receiver;

  NS_PRECONDITION(nsnull != aNode, "adding event listener to null node");

  if (NS_OK == aNode->QueryInterface(kIDOMEventReceiverIID, (void**) &receiver)) {
    receiver->AddEventListenerByIID((nsIDOMMouseListener*)this, kIDOMMouseListenerIID);
    NS_RELEASE(receiver);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::RemoveEventListener(nsIDOMNode * aNode)
{
  nsIDOMEventReceiver * receiver;

  NS_PRECONDITION(nsnull != aNode, "removing event listener from null node");

  if (NS_OK == aNode->QueryInterface(kIDOMEventReceiverIID, (void**) &receiver)) {
    receiver->RemoveEventListenerByIID(this, kIDOMMouseListenerIID);
    NS_RELEASE(receiver);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::AddWindowListener(nsIWindowListener * aWindowListener)
{
  if (nsnull != mWindowListener) {
    return NS_ERROR_FAILURE;
  }

  mWindowListener = aWindowListener;
  if (mDocIsLoaded && nsnull != mWindowListener) {
    mWindowListener->Initialize(this);
  }
  return NS_OK;
}

//-----------------------------------------------------
// Get the HTML Document
NS_IMETHODIMP nsXPBaseWindow::GetDocument(nsIDOMHTMLDocument *& aDocument)
{
  nsIDOMHTMLDocument *htmlDoc = nsnull;
  nsIPresShell *shell = nsnull;
  GetPresShell(shell);
  if (nsnull != shell) {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      doc->QueryInterface(kIDOMHTMLDocumentIID,(void **)&htmlDoc);
    }
    NS_RELEASE(shell);
  }

  aDocument = htmlDoc;
  return NS_OK;
}

//-----------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::NewWebShell(PRUint32 aChromeMask,
                                          PRBool aVisible,
                                          nsIWebShell*& aNewWebShell)
{
  nsresult rv = NS_OK;

  // Create new window. By default, the refcnt will be 1 because of
  // the registration of the browser window in gBrowsers.
  nsXPBaseWindow* dialogWindow;
  NS_NEWXPCOM(dialogWindow, nsXPBaseWindow);

  if (nsnull != dialogWindow) {
    nsRect  bounds;
    GetBounds(bounds);

    rv = dialogWindow->Init(mWindowType, mAppShell, mDialogURL, mTitle, bounds, aChromeMask, mAllowPlugins);
    if (NS_OK == rv) {
      if (aVisible) {
        dialogWindow->SetVisible(PR_TRUE);
      }
      nsIWebShell *shell;
      rv = dialogWindow->GetWebShell(shell);
      aNewWebShell = shell;
    } else {
      dialogWindow->Close();
    }
  } else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

NS_IMETHODIMP
nsXPBaseWindow::ContentShellAdded(nsIWebShell* aChildShell, nsIContent* frameNode)
{
  return NS_OK;
}


//----------------------------------------------------------------------
NS_IMETHODIMP nsXPBaseWindow::GetPresShell(nsIPresShell*& aPresShell)
{
  aPresShell = nsnull;

  nsIPresShell* shell = nsnull;
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
  if (docShell) {
    nsIContentViewer* cv = nsnull;
    docShell->GetContentViewer(&cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIPresContext* cx;
        docv->GetPresContext(cx);
        if (nsnull != cx) {
          cx->GetShell(&shell); // does an add ref
          aPresShell = shell;
          NS_RELEASE(cx);
        }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
  return NS_OK;
}

//-----------------------------------------------------------------
//-- nsIDOMMouseListener
//-----------------------------------------------------------------

//-----------------------------------------------------------------
nsresult nsXPBaseWindow::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXPBaseWindow::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXPBaseWindow::MouseDown(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXPBaseWindow::MouseClick(nsIDOMEvent* aMouseEvent)
{
  if (nsnull != mWindowListener) {
    PRBool status;
    mWindowListener->MouseClick(aMouseEvent, this, status);
  }
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXPBaseWindow::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXPBaseWindow::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXPBaseWindow::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

//----------------------------------------------------------------------
// Factory code for creating nsXPBaseWindow's
//----------------------------------------------------------------------
class nsXPBaseWindowFactory : public nsIFactory
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY

  nsXPBaseWindowFactory();
  virtual ~nsXPBaseWindowFactory();
};

//----------------------------------------------------------------------
nsXPBaseWindowFactory::nsXPBaseWindowFactory()
{
  NS_INIT_REFCNT();
}

//----------------------------------------------------------------------
nsXPBaseWindowFactory::~nsXPBaseWindowFactory()
{
}

//----------------------------------------------------------------------
nsresult
nsXPBaseWindowFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aResult = NULL;

  if (aIID.Equals(kISupportsIID)) {
    *aResult = (void *)(nsISupports*)this;
  } else if (aIID.Equals(kIFactoryIID)) {
    *aResult = (void *)(nsIFactory*)this;
  }

  if (*aResult == NULL) {
    return NS_NOINTERFACE;
  }

  NS_ADDREF_THIS(); // Increase reference count for caller
  return NS_OK;
}

NS_IMPL_ADDREF(nsXPBaseWindowFactory);
NS_IMPL_RELEASE(nsXPBaseWindowFactory);

//----------------------------------------------------------------------
nsresult
nsXPBaseWindowFactory::CreateInstance(nsISupports *aOuter,
                                       const nsIID &aIID,
                                       void **aResult)
{
  nsresult rv;
  nsXPBaseWindow *inst;

  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  if (nsnull != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  NS_NEWXPCOM(inst, nsXPBaseWindow);
  if (inst == NULL) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

done:
  return rv;
}

//----------------------------------------------------------------------
nsresult
nsXPBaseWindowFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}

//----------------------------------------------------------------------
nsresult
NS_NewXPBaseWindowFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;
  nsXPBaseWindowFactory* inst;
  NS_NEWXPCOM(inst, nsXPBaseWindowFactory);
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}
