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
#include "nsBrowserWindow.h"
#include "nsIStreamListener.h"
#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsIButton.h"
#include "nsIImageGroup.h"
#include "nsITimer.h"
#include "nsIThrobber.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#include "nsIURL.h"
#include "nsRepository.h"
#include "nsIFactory.h"
#include "nsCRT.h"
#include "nsWidgetsCID.h"
#include "nsViewerApp.h"

#include "resources.h"
#define SAMPLES_BASE_URL "resource:/res/samples"

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

//----------------------------------------------------------------------

nsVoidArray nsBrowserWindow::gBrowsers;

nsBrowserWindow*
nsBrowserWindow::FindBrowserFor(nsIWidget* aWidget, PRIntn aWhich)
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

void
nsBrowserWindow::AddBrowser(nsBrowserWindow* aBrowser)
{
  gBrowsers.AppendElement(aBrowser);
}

void
nsBrowserWindow::RemoveBrowser(nsBrowserWindow* aBrowser)
{
  gBrowsers.RemoveElement(aBrowser);
  if (0 == gBrowsers.Count()) {
    aBrowser->mApp->Exit();
  }
}

static nsEventStatus PR_CALLBACK
HandleBrowserEvent(nsGUIEvent *aEvent)
{ 
  nsBrowserWindow* bw =
    nsBrowserWindow::FindBrowserFor(aEvent->widget, FIND_WINDOW);
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

  case NS_MENU_SELECTED:
    return bw->DispatchMenuItem(((nsMenuEvent*)aEvent)->menuItem);

  default:
    break;
  }
  return nsEventStatus_eIgnore;
}

static nsEventStatus PR_CALLBACK
HandleBackEvent(nsGUIEvent *aEvent)
{
  nsBrowserWindow* bw =
    nsBrowserWindow::FindBrowserFor(aEvent->widget, FIND_BACK);
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
  nsBrowserWindow* bw =
    nsBrowserWindow::FindBrowserFor(aEvent->widget, FIND_FORWARD);
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
  nsBrowserWindow* bw =
    nsBrowserWindow::FindBrowserFor(aEvent->widget, FIND_LOCATION);
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
nsBrowserWindow::DispatchMenuItem(PRInt32 aID)
{
  nsEventStatus result = DispatchDebugMenu(aID);
  if (nsEventStatus_eIgnore != result) {
    return result;
  }
  switch (aID) {
  case VIEWER_EXIT:
    mApp->Exit();
    return nsEventStatus_eConsumeNoDefault;

  case VIEWER_DEMO0:
  case VIEWER_DEMO1:
  case VIEWER_DEMO2:
  case VIEWER_DEMO3:
  case VIEWER_DEMO4:
  case VIEWER_DEMO5:
  case VIEWER_DEMO6:
  case VIEWER_DEMO7:
  case VIEWER_DEMO8: 
  case VIEWER_DEMO9: 
    {
      PRIntn ix = aID - VIEWER_DEMO0;
      nsAutoString url(SAMPLES_BASE_URL);
      url.Append("/test");
      url.Append(ix, 10);
      url.Append(".html");
      LoadURL(url);
    }
    break;
  }
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

nsresult
nsBrowserWindow::Init(nsIAppShell* aAppShell,
                      const nsRect& aBounds,
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

  if (NS_CHROME_MENU_BAR_ON & aChromeMask) {
    rv = CreateMenuBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
    mWindow->GetBounds(r);
    r.x = r.y = 0;
  }

  if (NS_CHROME_TOOL_BAR_ON & aChromeMask) {
    rv = CreateToolBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
  }

  if (NS_CHROME_STATUS_BAR_ON & aChromeMask) {
    rv = CreateStatusBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
  }

  // Now lay it all out
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

#ifdef NS_DEBUG
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIStyleContext.h"
#include "nsISizeOfHandler.h"
#include "nsIStyleSet.h"
#include "nsXIFDTD.h"
#include "nsIParser.h"
#include "nsHTMLContentSinkStream.h"

static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);

static nsIPresShell*
GetPresShell(nsIWebShell* aWebShell)
{
  nsIPresShell* shell = nsnull;
  if (nsnull != aWebShell) {
    nsIContentViewer* cv = nsnull;
    aWebShell->GetContentViewer(cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIPresContext* cx;
        docv->GetPresContext(cx);
        if (nsnull != cx) {
          shell = cx->GetShell();
          NS_RELEASE(cx);
        }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
  return shell;
}

void
nsBrowserWindow::DumpContent(FILE* out)
{
  nsIPresShell* shell = GetPresShell(mWebShell);
  if (nsnull != shell) {
    nsIDocument* doc = shell->GetDocument();
    if (nsnull != doc) {
      nsIContent* root = doc->GetRootContent();
      if (nsnull != root) {
        root->List(out);
        NS_RELEASE(root);
      }
      NS_RELEASE(doc);
    }
    NS_RELEASE(shell);
  }
  else {
    fputs("null pres shell\n", out);
  }
}

void
nsBrowserWindow::DumpFrames(FILE* out)
{
  nsIPresShell* shell = GetPresShell(mWebShell);
  if (nsnull != shell) {
    nsIFrame* root = shell->GetRootFrame();
    if (nsnull != root) {
      root->List(out);
    }
    NS_RELEASE(shell);
  }
  else {
    fputs("null pres shell\n", out);
  }
}

void
nsBrowserWindow::DumpViews(FILE* out)
{
  nsIPresShell* shell = GetPresShell(mWebShell);
  if (nsnull != shell) {
    nsIViewManager* vm = shell->GetViewManager();
    if (nsnull != vm) {
      nsIView* root = vm->GetRootView();
      if (nsnull != root) {
        root->List(out);
        NS_RELEASE(root);
      }
      NS_RELEASE(vm);
    }
    NS_RELEASE(shell);
  }
  else {
    fputs("null pres shell\n", out);
  }
}

static void DumpAWebShell(nsIWebShell* aShell, FILE* out, PRInt32 aIndent)
{
  nsAutoString name;
  nsIWebShell* parent;
  PRInt32 i, n;

  for (i = aIndent; --i >= 0; ) fprintf(out, "  ");

  fprintf(out, "%p '", aShell);
  aShell->GetName(name);
  aShell->GetParent(parent);
  fputs(name, out);
  fprintf(out, "' parent=%p <\n", parent);
  NS_IF_RELEASE(parent);

  aIndent++;
  aShell->GetChildCount(n);
  for (i = 0; i < n; i++) {
    nsIWebShell* child;
    aShell->ChildAt(i, child);
    if (nsnull != child) {
      DumpAWebShell(child, out, aIndent);
    }
  }
  aIndent--;
  for (i = aIndent; --i >= 0; ) fprintf(out, "  ");
  fputs(">\n", out);
}

void
nsBrowserWindow::DumpWebShells(FILE* out)
{
  DumpAWebShell(mWebShell, out, 0);
}

void
nsBrowserWindow::DumpStyleSheets(FILE* out)
{
  nsIPresShell* shell = GetPresShell(mWebShell);
  if (nsnull != shell) {
    nsIStyleSet* styleSet = shell->GetStyleSet();
    if (nsnull == styleSet) {
      fputs("null style set\n", out);
    } else {
      styleSet->List(out);
      NS_RELEASE(styleSet);
    }
    NS_RELEASE(shell);
  }
  else {
    fputs("null pres shell\n", out);
  }
}

void
nsBrowserWindow::DumpStyleContexts(FILE* out)
{
  nsIPresShell* shell = GetPresShell(mWebShell);
  if (nsnull != shell) {
    nsIPresContext* cx = shell->GetPresContext();
    nsIStyleSet* styleSet = shell->GetStyleSet();
    if (nsnull == styleSet) {
      fputs("null style set\n", out);
    } else {
      nsIFrame* root = shell->GetRootFrame();
      if (nsnull == root) {
        fputs("null root frame\n", out);
      } else {
        nsIStyleContext* rootContext;
        root->GetStyleContext(cx, rootContext);
        if (nsnull != rootContext) {
          styleSet->ListContexts(rootContext, out);
          NS_RELEASE(rootContext);
        }
        else {
          fputs("null root context", out);
        }
      }
      NS_RELEASE(styleSet);
    }
    NS_IF_RELEASE(cx);
    NS_RELEASE(shell);
  } else {
    fputs("null pres shell\n", out);
  }
}

void
nsBrowserWindow::ToggleFrameBorders()
{
  PRBool showing = nsIFrame::GetShowFrameBorders();
  nsIFrame::ShowFrameBorders(!showing);
  ForceRefresh();
}

void
nsBrowserWindow::ForceRefresh()
{
  nsIPresShell* shell = GetPresShell(mWebShell);
  if (nsnull != shell) {
    nsIViewManager* vm = shell->GetViewManager();
    if (nsnull != vm) {
      nsIView* root = vm->GetRootView();
      if (nsnull != root) {
        vm->UpdateView(root, (nsIRegion*)nsnull, NS_VMREFRESH_IMMEDIATE);
        NS_RELEASE(root);
      }
      NS_RELEASE(vm);
    }
    NS_RELEASE(shell);
  }
}

void
nsBrowserWindow::ShowContentSize()
{
  nsISizeOfHandler* szh;
  if (NS_OK != NS_NewSizeOfHandler(&szh)) {
    return;
  }

  nsIPresShell* shell = GetPresShell(mWebShell);
  if (nsnull != shell) {
    nsIDocument* doc = shell->GetDocument();
    if (nsnull != doc) {
      nsIContent* content = doc->GetRootContent();
      if (nsnull != content) {
        content->SizeOf(szh);
        PRUint32 totalSize;
        szh->GetSize(totalSize);
        printf("Content model size is approximately %d bytes\n", totalSize);
        NS_RELEASE(content);
      }
      NS_RELEASE(doc);
    }
    NS_RELEASE(shell);
  }
  NS_RELEASE(szh);
}

void
nsBrowserWindow::ShowFrameSize()
{
  nsIPresShell* shell0 = GetPresShell(mWebShell);
  if (nsnull != shell0) {
    nsIDocument* doc = shell0->GetDocument();
    if (nsnull != doc) {
      PRInt32 i, shells = doc->GetNumberOfShells();
      for (i = 0; i < shells; i++) {
        nsIPresShell* shell = doc->GetShellAt(i);
        if (nsnull != shell) {
          nsISizeOfHandler* szh;
          if (NS_OK != NS_NewSizeOfHandler(&szh)) {
            return;
          }
          nsIFrame* root;
          root = shell->GetRootFrame();
          if (nsnull != root) {
            root->SizeOf(szh);
            PRUint32 totalSize;
            szh->GetSize(totalSize);
            printf("Frame model for shell=%p size is approximately %d bytes\n",
                   shell, totalSize);
          }
          NS_RELEASE(szh);
          NS_RELEASE(shell);
        }
      }
      NS_RELEASE(doc);
    }
    NS_RELEASE(shell0);
  }
}

void
nsBrowserWindow::ShowStyleSize()
{
}

void
nsBrowserWindow::DoDebugSave()
{
  nsIPresShell* shell = GetPresShell(mWebShell);
  if (nsnull != shell) {
    nsIDocument* doc = shell->GetDocument();
    if (nsnull != doc) {
      nsString buffer;
      doc->CreateXIF(buffer,PR_FALSE);

      nsIParser* parser;
      nsresult rv = NS_NewParser(&parser);
      if (NS_OK == rv) {
        nsIHTMLContentSink* sink = nsnull;
        
        rv = NS_New_HTML_ContentSinkStream(&sink);

        if (NS_OK == rv) {
          parser->SetContentSink(sink);
          
          nsIDTD* dtd = nsnull;
          rv = NS_NewXIFDTD(&dtd);
          if (NS_OK == rv) 
          {
            parser->RegisterDTD(dtd);
            dtd->SetContentSink(sink);
            dtd->SetParser(parser);
            parser->Parse(buffer, PR_TRUE);           
          }
          NS_IF_RELEASE(dtd);
          NS_IF_RELEASE(sink);
        }
        NS_RELEASE(parser);
      }
    }
  }
}

nsEventStatus
nsBrowserWindow::DispatchDebugMenu(PRInt32 aID)
{
  nsEventStatus result = nsEventStatus_eIgnore;

  switch(aID) {
  case VIEWER_VISUAL_DEBUGGING:
    ToggleFrameBorders();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DUMP_CONTENT:
    DumpContent();
    DumpWebShells();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DUMP_FRAMES:
    DumpFrames();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DUMP_VIEWS:
    DumpViews();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DUMP_STYLE_SHEETS:
    DumpStyleSheets();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DUMP_STYLE_CONTEXTS:
    DumpStyleContexts();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_SHOW_CONTENT_SIZE:
    ShowContentSize();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_SHOW_FRAME_SIZE:
    ShowFrameSize();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_SHOW_STYLE_SIZE:
    ShowStyleSize();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_SHOW_CONTENT_QUALITY:
#if XXX_fix_me
    if ((nsnull != wd) && (nsnull != wd->observer)) {
      nsIPresContext *px = wd->observer->mWebWidget->GetPresContext();
      nsIPresShell   *ps = px->GetShell();
      nsIViewManager *vm = ps->GetViewManager();

      vm->ShowQuality(!vm->GetShowQuality());

      NS_RELEASE(vm);
      NS_RELEASE(ps);
      NS_RELEASE(px);
    }
#endif
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DEBUGSAVE:
    DoDebugSave();
    break;
  }
  return(result);
}

#endif // NS_DEBUG

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

  inst = new nsNativeBrowserWindow();
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

nsresult
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
