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
#include "nsIBrowserWindow.h"
#include "nsIStreamListener.h"
#include "nsIAppShell.h"
#include "nsIWebShell.h"
#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsIButton.h"
#include "nsIImageGroup.h"
#include "nsITimer.h"
#include "nsIThrobber.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#include "nsIURL.h"
#include "nsRepository.h"
#include "nsVoidArray.h"
#include "nsIFactory.h"
#include "nsCRT.h"

#include "nsWidgetsCID.h"

// XXX greasy constants
#define THROBBER_WIDTH 32
#define THROBBER_HEIGHT 32
#define BUTTON_WIDTH 90
#define BUTTON_HEIGHT THROBBER_HEIGHT

#ifdef INSET_WEBSHELL
#define WEBSHELL_LEFT_INSET 5
#define WEBSHELL_RIGHT_INSET 5
#define WEBSHELL_TOP_INSET 5
#define WEBSHELL_BOTTOM_INSET 5
#else
#define WEBSHELL_LEFT_INSET 0
#define WEBSHELL_RIGHT_INSET 0
#define WEBSHELL_TOP_INSET 0
#define WEBSHELL_BOTTOM_INSET 0
#endif

static NS_DEFINE_IID(kBrowserWindowCID, NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kButtonCID, NS_BUTTON_CID);
static NS_DEFINE_IID(kTextFieldCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kThrobberCID, NS_THROBBER_CID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID);

static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);
static NS_DEFINE_IID(kIButtonIID, NS_IBUTTON_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIThrobberIID, NS_ITHROBBER_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

static nsVoidArray gBrowsers;

class nsBrowserWindow : public nsIBrowserWindow,
                        public nsIStreamObserver,
                        public nsIWebShellContainer,
                        public nsIScriptContextOwner
{
public:
  nsBrowserWindow();
  virtual ~nsBrowserWindow();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIBrowserWindow
  NS_IMETHOD Init(nsIAppShell* aAppShell,
                  const nsRect& aBounds,
                  PRUint32 aChromeMask);
  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD SizeTo(PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD Show();
  NS_IMETHOD Hide();
  NS_IMETHOD ChangeChrome(PRUint32 aNewChromeMask);
  NS_IMETHOD GetChrome(PRUint32& aChromeMaskResult);
  NS_IMETHOD LoadURL(const nsString& aURL);
  NS_IMETHOD SetTitle(const nsString& aTitle);
  NS_IMETHOD GetTitle(nsString& aResult);

  // nsIStreamObserver
  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
  NS_IMETHOD OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax,
                        const nsString& aMsg);
  NS_IMETHOD OnStopBinding(nsIURL* aURL, PRInt32 status, const nsString& aMsg);

  // nsIScriptContextOwner
  NS_IMETHOD GetScriptContext(nsIScriptContext **aContext);
  NS_IMETHOD ReleaseScriptContext(nsIScriptContext *aContext);

  // nsIWebShellContainer
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const nsString& aURL);
  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const nsString& aURL);
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const nsString& aURL);

  // nsBrowserWindow
  nsresult CreateToolBar(PRInt32 aWidth);
  nsresult CreateStatusBar(PRInt32 aWidth);
  void Layout(PRInt32 aWidth, PRInt32 aHeight);
  void Destroy();
  void Back();
  void Forward();
  void GoTo(const nsString& aURL);
  void StartThrobber();
  void StopThrobber();
  void LoadThrobberImages();
  void DestroyThrobberImages();
  nsEventStatus DispatchMenuItem(nsGUIEvent *aEvent);

  PRUint32 mChromeMask;
  nsString mTitle;

  nsIWidget* mWindow;
  nsIWebShell* mWebShell;

  // "Toolbar"
  nsITextWidget* mLocation;
  nsIButton* mBack;
  nsIButton* mForward;
  nsIThrobber* mThrobber;

  // "Status bar"
  nsITextWidget* mStatus;

  nsIScriptGlobalObject *mScriptGlobal;
  nsIScriptContext* mScriptContext;
};

//----------------------------------------------------------------------

// XXX This is bad; because we can't hang a closure off of the event
// callback we have no way to store our This pointer; therefore we
// have to hunt to find the browswer that the event belongs too!!!

#define FIND_WINDOW   0
#define FIND_BACK     1
#define FIND_FORWARD  2
#define FIND_LOCATION 3

static nsBrowserWindow*
FindBrowserFor(nsIWidget* aWidget, PRIntn aWhich)
{
  nsIWidget* widget;

  PRInt32 i, n = gBrowsers.Count();
  for (i = 0; i < n; i++) {
    nsBrowserWindow* bw = (nsBrowserWindow*) gBrowsers.ElementAt(i);
    if (nsnull != bw) {
      switch (aWhich) {
      case FIND_WINDOW:
        bw->mWindow->QueryInterface(kIWidgetIID, (void**) &widget);
        if (widget == aWidget) return bw;
        break;
      case FIND_BACK:
        bw->mBack->QueryInterface(kIWidgetIID, (void**) &widget);
        if (widget == aWidget) return bw;
        break;
      case FIND_FORWARD:
        bw->mForward->QueryInterface(kIWidgetIID, (void**) &widget);
        if (widget == aWidget) return bw;
        break;
      case FIND_LOCATION:
        bw->mLocation->QueryInterface(kIWidgetIID, (void**) &widget);
        if (widget == aWidget) return bw;
        break;
      }
    }
  }
  return nsnull;
}

static void
AddBrowser(nsBrowserWindow* aBrowser)
{
  gBrowsers.AppendElement(aBrowser);
}

static void
RemoveBrowser(nsBrowserWindow* aBrowser)
{
  gBrowsers.RemoveElement(aBrowser);
  if (0 == gBrowsers.Count()) {
    printf("I want to exit, how about you?\n");
    exit(0);
  }
}

static nsEventStatus PR_CALLBACK
HandleBrowserEvent(nsGUIEvent *aEvent)
{ 
  nsBrowserWindow* bw = FindBrowserFor(aEvent->widget, FIND_WINDOW);
  if (nsnull == bw) {
    return nsEventStatus_eIgnore;
  }

  nsSizeEvent* sizeEvent;
  nsEventStatus result = nsEventStatus_eIgnore;
  switch(aEvent->message) {
  case NS_SIZE:
    sizeEvent = (nsSizeEvent*)aEvent;  
    bw->Layout(sizeEvent->windowSize->width,
               sizeEvent->windowSize->height);
    return nsEventStatus_eConsumeNoDefault;

  case NS_DESTROY:
    bw->Destroy();
    return nsEventStatus_eConsumeDoDefault;

  default:
    break;
  }
  return bw->DispatchMenuItem(aEvent);
}

static nsEventStatus PR_CALLBACK
HandleBackEvent(nsGUIEvent *aEvent)
{
  nsBrowserWindow* bw = FindBrowserFor(aEvent->widget, FIND_BACK);
  if (nsnull == bw) {
    return nsEventStatus_eIgnore;
  }

  switch(aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
    bw->Back();
    break;
  }
  return nsEventStatus_eIgnore;
}

static nsEventStatus PR_CALLBACK
HandleForwardEvent(nsGUIEvent *aEvent)
{
  nsBrowserWindow* bw = FindBrowserFor(aEvent->widget, FIND_FORWARD);
  if (nsnull == bw) {
    return nsEventStatus_eIgnore;
  }

  switch(aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
    bw->Forward();
    break;
  }
  return nsEventStatus_eIgnore;
}

static nsEventStatus PR_CALLBACK
HandleLocationEvent(nsGUIEvent *aEvent)
{
  nsBrowserWindow* bw = FindBrowserFor(aEvent->widget, FIND_LOCATION);
  if (nsnull == bw) {
    return nsEventStatus_eIgnore;
  }

  switch (aEvent->message) {
  case NS_KEY_UP:
    if (NS_VK_RETURN == ((nsKeyEvent*)aEvent)->keyCode) {
      nsAutoString text;
      bw->mLocation->GetText(text, 1000);
      bw->GoTo(text);
    }
    break;
  }

  return nsEventStatus_eIgnore;
}

nsEventStatus
nsBrowserWindow::DispatchMenuItem(nsGUIEvent* aEvent)
{
  return nsEventStatus_eIgnore;
}

void
nsBrowserWindow::Destroy()
{
  mWebShell->SetContainer(nsnull);
  printf("refcnt=%d\n", mRefCnt);
  Release();
}

void
nsBrowserWindow::Back()
{
  mWebShell->Back((nsIStreamObserver*) this);
}

void
nsBrowserWindow::Forward()
{
  mWebShell->Forward((nsIStreamObserver*) this);
}

void
nsBrowserWindow::GoTo(const nsString& aURL)
{
  mWebShell->LoadURL(aURL, (nsIStreamObserver*) this, nsnull);
}

//----------------------------------------------------------------------

// Note: operator new zeros our memory
nsBrowserWindow::nsBrowserWindow()
{
  AddBrowser(this);
}

nsBrowserWindow::~nsBrowserWindow()
{
  RemoveBrowser(this);
}

NS_IMPL_ADDREF(nsBrowserWindow)
NS_IMPL_RELEASE(nsBrowserWindow)

nsresult
nsBrowserWindow::QueryInterface(const nsIID& aIID,
                                void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIBrowserWindowIID)) {
    *aInstancePtrResult = (void*) ((nsIBrowserWindow*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIStreamObserverIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptContextOwnerIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptContextOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIWebShellContainerIID)) {
    *aInstancePtrResult = (void*) ((nsIWebShellContainer*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIBrowserWindow*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsBrowserWindow::Init(nsIAppShell* aAppShell, const nsRect& aBounds,
                      PRUint32 aChromeMask)
{
  mChromeMask = aChromeMask;

  // Create top level window
  nsresult rv = NSRepository::CreateInstance(kWindowCID, nsnull, kIWidgetIID,
                                             (void**)&mWindow);
  if (NS_OK != rv) {
    return rv;
  }
  nsRect r(0, 0, aBounds.width, aBounds.height);
  mWindow->Create((nsIWidget*)NULL, r, HandleBrowserEvent,
                  nsnull, nsnull,
                  (nsWidgetInitData *)
                    aAppShell->GetNativeData(NS_NATIVE_SHELL));
  mWindow->GetBounds(r);

  // Create web shell
  rv = NSRepository::CreateInstance(kWebShellCID, nsnull,
                                    kIWebShellIID,
                                    (void**)&mWebShell);
  if (NS_OK != rv) {
    return rv;
  }
  r.x = r.y = 0;
  rv = mWebShell->Init(mWindow->GetNativeData(NS_NATIVE_WIDGET), r);
  mWebShell->SetContainer((nsIWebShellContainer*) this);
  mWebShell->Show();

  rv = CreateToolBar(r.width);
  if (NS_OK != rv) {
    return rv;
  }

  // XXX status bar
  rv = CreateStatusBar(r.width);
  if (NS_OK != rv) {
    return rv;
  }

  // XXX now lay it all out

  Layout(r.width, r.height);

  return NS_OK;
}

nsresult
nsBrowserWindow::CreateToolBar(PRInt32 aWidth)
{
  nsresult rv;

  // Create and place back button
  rv = NSRepository::CreateInstance(kButtonCID, nsnull, kIButtonIID,
                                    (void**)&mBack);
  if (NS_OK != rv) {
    return rv;
  }
  nsRect r(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT);
  mBack->Create(mWindow, r, HandleBackEvent, NULL);
  mBack->SetLabel("Back");
  mBack->Show(PR_TRUE);

  // Create and place forward button
  r.SetRect(BUTTON_WIDTH, 0, BUTTON_WIDTH, BUTTON_HEIGHT);  
  rv = NSRepository::CreateInstance(kButtonCID, nsnull, kIButtonIID,
                               (void**)&mForward);
  if (NS_OK != rv) {
    return rv;
  }
  mForward->Create(mWindow, r, HandleForwardEvent, NULL);
  mForward->SetLabel("Forward");
  mForward->Show(PR_TRUE);

  // Create and place location bar
  r.SetRect(2*BUTTON_WIDTH, 0,
            aWidth - 2*BUTTON_WIDTH - THROBBER_WIDTH,
            BUTTON_HEIGHT);
  rv = NSRepository::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID,
                                    (void**)&mLocation);
  if (NS_OK != rv) {
    return rv;
  }
  mLocation->Create(mWindow, r, HandleLocationEvent, NULL);
  mLocation->SetText("");
  mLocation->SetForegroundColor(NS_RGB(255, 0, 0));
  mLocation->SetBackgroundColor(NS_RGB(255, 255, 255));
  mLocation->Show(PR_TRUE);

  // Create and place throbber
  r.SetRect(aWidth - THROBBER_WIDTH, 0,
            THROBBER_WIDTH, THROBBER_HEIGHT);
  rv = NSRepository::CreateInstance(kThrobberCID, nsnull, kIThrobberIID,
                                    (void**)&mThrobber);
  if (NS_OK != rv) {
    return rv;
  }
  mThrobber->Init(mWindow, r);
  mThrobber->Show();

  return NS_OK;
}

nsresult
nsBrowserWindow::CreateStatusBar(PRInt32 aWidth)
{
  nsresult rv;

  nsRect r(0, 0, aWidth, THROBBER_HEIGHT);
  rv = NSRepository::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID,
                                    (void**)&mStatus);
  if (NS_OK != rv) {
    return rv;
  }
  mStatus->Create(mWindow, r, HandleLocationEvent, NULL);
  mStatus->SetText("");
  mStatus->SetForegroundColor(NS_RGB(255, 0, 0));
  mStatus->SetBackgroundColor(NS_RGB(255, 255, 255));
  mStatus->Show(PR_TRUE);

  return NS_OK;
}

void
nsBrowserWindow::Layout(PRInt32 aWidth, PRInt32 aHeight)
{
  // position location bar (it's stretchy)
  if (mLocation) {
    if (mThrobber) {
      mLocation->Resize(2*BUTTON_WIDTH, 0,
                        aWidth - (2*BUTTON_WIDTH + THROBBER_WIDTH),
                        BUTTON_HEIGHT,
                        PR_TRUE);
    }
    else {
      mLocation->Resize(2*BUTTON_WIDTH, 0,
                        aWidth - 2*BUTTON_WIDTH,
                        BUTTON_HEIGHT,
                        PR_TRUE);
    }
  }

  if (mThrobber) {
    mThrobber->MoveTo(aWidth - THROBBER_WIDTH, 0);
  }

  nsRect rr(0, BUTTON_HEIGHT,
            aWidth,
            aHeight - BUTTON_HEIGHT);
  if (mStatus) {
    mStatus->Resize(0, aHeight - THROBBER_HEIGHT,
                    aWidth, THROBBER_HEIGHT,
                    PR_TRUE);
    rr.height -= THROBBER_HEIGHT;
  }

  // inset the web widget
  rr.x += WEBSHELL_LEFT_INSET;
  rr.y += WEBSHELL_TOP_INSET;
  rr.width -= WEBSHELL_LEFT_INSET + WEBSHELL_RIGHT_INSET;
  rr.height -= WEBSHELL_TOP_INSET + WEBSHELL_BOTTOM_INSET;
  mWebShell->SetBounds(rr);
}

NS_IMETHODIMP
nsBrowserWindow::MoveTo(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Move(aX, aY);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::SizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");

  // XXX We want to do this in one shot
  mWindow->Resize(aWidth, aHeight, PR_FALSE);
  Layout(aWidth, aHeight);

  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::Show()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Show(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::Hide()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Show(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::ChangeChrome(PRUint32 aChromeMask)
{
  // XXX write me
  mChromeMask = aChromeMask;
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::GetChrome(PRUint32& aChromeMaskResult)
{
  aChromeMaskResult = mChromeMask;
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::LoadURL(const nsString& aURL)
{
  return mWebShell->LoadURL(aURL, this, nsnull);
}

//----------------------------------------

NS_IMETHODIMP
nsBrowserWindow::SetTitle(const nsString& aTitle)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mTitle = aTitle;
  mWindow->SetTitle(aTitle);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::GetTitle(nsString& aResult)
{
  aResult = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::WillLoadURL(nsIWebShell* aShell, const nsString& aURL)
{
  if (mStatus) {
    nsAutoString url("Connecting to ");
    url.Append(aURL);
    mStatus->SetText(url);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::BeginLoadURL(nsIWebShell* aShell, const nsString& aURL)
{
  if (mThrobber) {
    mThrobber->Start();
    mLocation->SetText(aURL);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::EndLoadURL(nsIWebShell* aShell, const nsString& aURL)
{
  if (mThrobber) {
    mThrobber->Stop();
  }
  return NS_OK;
}

//----------------------------------------

// Stream observer implementation

NS_IMETHODIMP
nsBrowserWindow::OnProgress(nsIURL* aURL,
                            PRInt32 aProgress,
                            PRInt32 aProgressMax,
                            const nsString& aMsg)
{
  if (mStatus) {
    nsAutoString url;
    if (nsnull != aURL) {
      aURL->ToString(url);
    }
    url.Append(": progress ");
    url.Append(aProgress, 10);
    if (0 != aProgressMax) {
      url.Append(" (out of ");
      url.Append(aProgressMax, 10);
      url.Append(")");
    }
    mStatus->SetText(url);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  if (mStatus) {
    nsAutoString url;
    if (nsnull != aURL) {
      aURL->ToString(url);
    }
    url.Append(": start");
    mStatus->SetText(url);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::OnStopBinding(nsIURL* aURL,
                               PRInt32 status,
                               const nsString& aMsg)
{
  if (mThrobber) {
    mThrobber->Stop();
  }
  if (mStatus) {
    nsAutoString url;
    if (nsnull != aURL) {
      aURL->ToString(url);
    }
    url.Append(": stop");
    mStatus->SetText(url);
  }
  return NS_OK;
}

//----------------------------------------

// nsIScriptContextOwner

nsresult 
nsBrowserWindow::GetScriptContext(nsIScriptContext** aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null arg");
  nsresult res = NS_OK;

  if (nsnull == mScriptGlobal) {
    res = NS_NewScriptGlobalObject(&mScriptGlobal);
    if (NS_OK != res) {
      return res;
    }

    res = NS_CreateContext(mScriptGlobal, &mScriptContext);
    if (NS_OK != res) {
      return res;
    }

#if XXX_this_is_wrong
    nsIPresShell* shell = GetPresShell(mWebShell);
    if (nsnull != shell) {
      nsIDocument* doc = shell->GetDocument();
      if (nsnull != doc) {
        nsIDOMDocument *domdoc = nsnull;
        doc->QueryInterface(kIDOMDocumentIID, (void**) &domdoc);
        if (nsnull != domdoc) {
          mScriptGlobal->SetNewDocument(domdoc);
          NS_RELEASE(domdoc);
        }
        NS_RELEASE(doc);
      }
      NS_RELEASE(shell);
    }
#endif
  }

  *aContext = mScriptContext;
  NS_ADDREF(mScriptContext);

  return res;
}

nsresult 
nsBrowserWindow::ReleaseScriptContext(nsIScriptContext *aContext)
{
  NS_IF_RELEASE(aContext);

  return NS_OK;
}

//----------------------------------------

// Toolbar support

void
nsBrowserWindow::StartThrobber()
{
}

void
nsBrowserWindow::StopThrobber()
{
}

void
nsBrowserWindow::LoadThrobberImages()
{
}

void
nsBrowserWindow::DestroyThrobberImages()
{
}

//----------------------------------------------------------------------

// Factory code for creating nsBrowserWindow's

class nsBrowserWindowFactory : public nsIFactory
{
public:
  nsBrowserWindowFactory();
  ~nsBrowserWindowFactory();

  // nsISupports methods
  NS_IMETHOD QueryInterface(const nsIID &aIID, void **aResult);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

private:
  nsrefcnt  mRefCnt;
};

nsBrowserWindowFactory::nsBrowserWindowFactory()
{
  mRefCnt = 0;
}

nsBrowserWindowFactory::~nsBrowserWindowFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

nsresult
nsBrowserWindowFactory::QueryInterface(const nsIID &aIID, void **aResult)
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

  AddRef(); // Increase reference count for caller
  return NS_OK;
}

nsrefcnt
nsBrowserWindowFactory::AddRef()
{
  return ++mRefCnt;
}

nsrefcnt
nsBrowserWindowFactory::Release()
{
  if (--mRefCnt == 0) {
    delete this;
    return 0; // Don't access mRefCnt after deleting!
  }
  return mRefCnt;
}

nsresult
nsBrowserWindowFactory::CreateInstance(nsISupports *aOuter,
                                       const nsIID &aIID,
                                       void **aResult)
{
  nsresult rv;
  nsBrowserWindow *inst;

  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  if (nsnull != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  inst = new nsBrowserWindow();
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

nsresult
nsBrowserWindowFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}

extern "C" NS_WEB nsresult
NS_NewBrowserWindowFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsBrowserWindowFactory();
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}
