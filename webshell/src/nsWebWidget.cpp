/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include <stdio.h>
#include "nsIWebWidget.h"
#include "nsILinkHandler.h"
#include "nsIURL.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsIScrollbar.h"
#include "nsUnitConversion.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsCRT.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsWidgetsCID.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsICSSParser.h"
#include "nsIDOMDocument.h"
#include "prprf.h"
#include "prtime.h"
#include "nsVoidArray.h"
#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"

#define UA_CSS_URL "resource:/res/ua.css"

#define GET_OUTER() \
  ((nsWebWidget*) ((char*)this - nsWebWidget::GetOuterOffset()))

// Machine independent implementation portion of the web widget
class WebWidgetImpl : public nsIWebWidget, public nsIScriptContextOwner {
public:
  WebWidgetImpl();
  ~WebWidgetImpl();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  virtual nsresult Init(nsNativeWidget aParent,
                        const nsRect& aBounds,
                        nsScrollPreference aScrolling = nsScrollPreference_kAuto);
  virtual nsresult Init(nsNativeWidget aParent,
                        const nsRect& aBounds,
                        nsIDocument* aDocument,
                        nsIPresContext* aPresContext,
                        nsScrollPreference aScrolling = nsScrollPreference_kAuto);

  virtual nsRect GetBounds();
  virtual void SetBounds(const nsRect& aBounds);
  virtual void Move(PRInt32 aX, PRInt32 aY);
  virtual void Show();
  virtual void Hide();

  NS_IMETHOD SetContainer(nsISupports* aContainer, PRBool aRelationship = PR_TRUE);

  NS_IMETHOD GetContainer(nsISupports** aResult);

  virtual PRInt32 GetNumChildren();
  NS_IMETHOD AddChild(nsIWebWidget* aChild, PRBool aRelationship = PR_TRUE);
  NS_IMETHOD GetChildAt(PRInt32 aIndex, nsIWebWidget** aChild);

  virtual nsIWebWidget* GetRootWebWidget();
  virtual nsIPresContext* GetPresContext();
  virtual PRBool GetName(nsString& aName);
  virtual void SetName(const nsString& aName);
  virtual nsIWebWidget* GetWebWidgetWithName(const nsString& aName);
  virtual nsIWebWidget* GetTarget(const nsString& aName);

  NS_IMETHOD SetLinkHandler(nsILinkHandler* aHandler);

  NS_IMETHOD GetLinkHandler(nsILinkHandler** aResult);

  NS_IMETHOD LoadURL(const nsString& aURL, nsIStreamListener* aListener,
                     nsIPostData* aPostData);
  virtual nsIDocument* GetDocument();

  virtual void DumpContent(FILE* out);
  virtual void DumpFrames(FILE* out);
  virtual void DumpStyleSheets(FILE* out = nsnull);
  virtual void DumpStyleContexts(FILE* out = nsnull);
  virtual void DumpViews(FILE* out);
  virtual void ShowFrameBorders(PRBool aEnable);
  virtual PRBool GetShowFrameBorders();
  virtual nsIWidget* GetWWWindow();
  NS_IMETHOD GetScriptContext(nsIScriptContext **aContext);
  NS_IMETHOD ReleaseScriptContext(nsIScriptContext *aContext);
  NS_IMETHOD GetDOMDocument(nsIDOMDocument** aDocument);

private:
  nsresult ProvideDefaultHandlers();
  void ForceRefresh();
  nsresult MakeWindow(nsNativeWidget aParent, const nsRect& aBounds,
                      nsScrollPreference aScrolling);
  nsresult InitUAStyleSheet(void);
  nsresult CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet);
  void ReleaseChildren();

  nsIWidget* mWindow;
  nsIView *mView;
  nsIViewManager *mViewManager;
  nsIPresContext* mPresContext;
  nsIPresShell* mPresShell;
  nsIStyleSheet* mUAStyleSheet;
  nsILinkHandler* mLinkHandler;
  nsISupports* mContainer;
  nsIScriptGlobalObject *mScriptGlobal;
  nsIScriptContext* mScriptContext;

  nsVoidArray mChildren;
  //static nsIWebWidget* gRootWebWidget;
  nsString* mName;
  nsIDeviceContext *mDeviceContext;

  friend class WebWidgetImpl;
};

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIWebWidgetIID, NS_IWEBWIDGET_IID);
static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//nsIWebWidget* WebWidgetImpl::gRootWebWidget = nsnull;

// Note: operator new zeros our memory
WebWidgetImpl::WebWidgetImpl()
{
}

nsresult WebWidgetImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
  if (aIID.Equals(kIWebWidgetIID)) {
    *aInstancePtr = (void*)(nsIWebWidget*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptContextOwnerIID)) {
    *aInstancePtr = (void*)(nsIScriptContextOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIWebWidget*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(WebWidgetImpl)
NS_IMPL_RELEASE(WebWidgetImpl)

WebWidgetImpl::~WebWidgetImpl()
{
printf("del %d ", this);
  ReleaseChildren();
  mContainer = nsnull;

  // Release windows and views
  if (nsnull != mViewManager)
  {
    mViewManager->SetRootView(nsnull);
    mViewManager->SetRootWindow(nsnull);
    NS_RELEASE(mViewManager);
  }

  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mView);

  NS_IF_RELEASE(mLinkHandler);

  // Note: release context then shell
  NS_IF_RELEASE(mPresContext);
  NS_IF_RELEASE(mPresShell);
  NS_IF_RELEASE(mUAStyleSheet);

  NS_IF_RELEASE(mScriptContext);
  NS_IF_RELEASE(mScriptGlobal);
  NS_IF_RELEASE(mDeviceContext);
}

void Check(WebWidgetImpl* ww) 
{
  PRInt32 foo = ww->GetNumChildren();
  for (int i = 0; i < foo; i++) {
    nsIWebWidget* child;
    ww->GetChildAt(i, &child);
    if (child == 0) {
      printf("hello");
    }
  }
}



void WebWidgetImpl::ReleaseChildren()
{
  PRInt32 numChildren = GetNumChildren();
  for (PRInt32 i = 0; i < numChildren; i++) {
    nsIWebWidget* child;
    GetChildAt(i, &child);
    child->SetContainer(nsnull, PR_FALSE);  
    PRInt32 refCnt = child->Release();  // XXX can't use macro, need ref count
    // XXX add this back and find out why the ref count is 1
    // NS_ASSERTION(0 == refCnt, "reference to a web widget that will have no parent");
  }
  mChildren.Clear();
}

PRBool WebWidgetImpl::GetName(nsString& aName)
{
  if (mName) {
    aName = *mName;
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}

void WebWidgetImpl::SetName(const nsString& aName)
{
  if (!mName) {
    mName = new nsString(aName);
  }
}

nsIWebWidget* WebWidgetImpl::GetWebWidgetWithName(const nsString& aName)
{
  nsIWebWidget* ww = this;
  if (mName && (mName->EqualsIgnoreCase(aName))) {
    NS_ADDREF(this);
    return this;
  }

  PRInt32 numChildren = GetNumChildren();
  for (PRInt32 i = 0; i < numChildren; i++) {
    nsIWebWidget* child;
    GetChildAt(i, &child);
if ((numChildren == 3) && (i == 0)) {
  printf("hello ");
}
    nsIWebWidget* result = child->GetWebWidgetWithName(aName);
    if (result) {
      return result;
    }
    NS_RELEASE(child);
  }

  return nsnull;
}

nsIWebWidget* WebWidgetImpl::GetTarget(const nsString& aName)
{
  nsIWebWidget* target = nsnull;

  if (aName.EqualsIgnoreCase("_blank")) {
    NS_ASSERTION(0, "not implemented yet");
    target = this;
  } 
  else if (aName.EqualsIgnoreCase("_self")) {
    target = this;
  } 
  else if (aName.EqualsIgnoreCase("_parent")) {
    nsIWebWidget* top = GetRootWebWidget();
    if (top == this) {
      target = this;
      NS_RELEASE(top);
    }
    else {
      nsISupports* container;
      nsresult result = GetContainer(&container);
      if (NS_OK == result) {
        result = container->QueryInterface(kIWebWidgetIID, (void**)&target);
        if (NS_OK != result) {
          NS_ASSERTION(0, "invalid container");
          target = this;
        }
      }
      else {
        NS_ASSERTION(0, "invalid container for sub document - not a web widget");
      }
    }
  }
  else if (aName.EqualsIgnoreCase("_top")) {
    target = GetRootWebWidget();
  }
  else {
    nsIWebWidget* top = GetRootWebWidget();
    target = top->GetWebWidgetWithName(aName);
    NS_RELEASE(top);
    if (!target) {
      target = this;
    }
  }

  if (target == this) {
    NS_ADDREF(this);
  }

  return target;
}


nsresult WebWidgetImpl::MakeWindow(nsNativeWidget aNativeParent,
                                   const nsRect& aBounds,
                                   nsScrollPreference aScrolling)
{
  nsresult rv;
  static NS_DEFINE_IID(kViewManagerCID, NS_VIEW_MANAGER_CID);
  static NS_DEFINE_IID(kIViewManagerIID, NS_IVIEWMANAGER_IID);

  rv = NSRepository::CreateInstance(kViewManagerCID, 
                                     nsnull, 
                                     kIViewManagerIID, 
                                     (void **)&mViewManager);

  if ((NS_OK != rv) || (NS_OK != mViewManager->Init(mPresContext))) {
    return rv;
  }

  nsRect tbounds = aBounds;
  tbounds *= mPresContext->GetPixelsToTwips();

  // Create a child window of the parent that is our "root view/window"
  // Create a view
  static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
  static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

  rv = NSRepository::CreateInstance(kScrollingViewCID, 
                                     nsnull, 
                                     kIViewIID, 
                                     (void **)&mView);
  static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
  if ((NS_OK != rv) || (NS_OK != mView->Init(mViewManager, 
                                                tbounds, 
                                                nsnull,
                                                &kWidgetCID,
                                                nsnull,
                                                aNativeParent))) {
    return rv;
  }

  static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
  nsIScrollableView* scrollView;
  rv = mView->QueryInterface(kScrollViewIID, (void**)&scrollView);
  if (NS_OK == rv) {
    scrollView->SetScrollPreference(aScrolling);
  }
  else {
    NS_ASSERTION(0, "invalid scrolling view");
    return rv;
  }

  // Setup hierarchical relationship in view manager
  mViewManager->SetRootView(mView);
  mWindow = mView->GetWidget();
  if (mWindow) {
    mViewManager->SetRootWindow(mWindow);
  }

  //set frame rate to 25 fps
  mViewManager->SetFrameRate(25);

  return rv;
}

nsresult WebWidgetImpl::Init(nsNativeWidget aNativeParent,
                             const nsRect& aBounds,
                             nsScrollPreference aScrolling)
{
  // Create presentation context

  static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
  static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

  nsresult rv = NSRepository::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&mDeviceContext);

  if (NS_OK == rv) {
    mDeviceContext->Init(aNativeParent);
    mDeviceContext->SetDevUnitsToAppUnits(mDeviceContext->GetDevUnitsToTwips());
    mDeviceContext->SetAppUnitsToDevUnits(mDeviceContext->GetTwipsToDevUnits());
    mDeviceContext->SetGamma(1.7f);

    NS_ADDREF(mDeviceContext);
  }

  rv = NS_NewGalleyContext(&mPresContext);

  if (NS_OK != rv) {
    return rv;
  }

  mPresContext->Init(mDeviceContext);

  return MakeWindow(aNativeParent, aBounds, aScrolling);
}

nsresult WebWidgetImpl::InitUAStyleSheet(void)
{
  nsresult rv = NS_OK;

  if (nsnull == mUAStyleSheet) {  // snarf one
    nsIURL* uaURL;
    rv = NS_NewURL(&uaURL, nsnull, UA_CSS_URL); // XXX this bites, fix it
    if (NS_OK == rv) {
      // Get an input stream from the url
      PRInt32 ec;
      nsIInputStream* in = uaURL->Open(&ec);
      if (nsnull != in) {
        // Translate the input using the argument character set id into unicode
        nsIUnicharInputStream* uin;
        rv = NS_NewConverterStream(&uin, nsnull, in);
        if (NS_OK == rv) {
          // Create parser and set it up to process the input file
          nsICSSParser* css;
          rv = NS_NewCSSParser(&css);
          if (NS_OK == rv) {
            // Parse the input and produce a style set
            // XXX note: we are ignoring rv until the error code stuff in the
            // input routines is converted to use nsresult's
            css->Parse(uin, uaURL, mUAStyleSheet);
            NS_RELEASE(css);
          }
          NS_RELEASE(uin);
        }
        NS_RELEASE(in);
      }
      else {
//        printf("open of %s failed: error=%x\n", UA_CSS_URL, ec);
        rv = NS_ERROR_ILLEGAL_VALUE;  // XXX need a better error code here
      }

      NS_RELEASE(uaURL);
    }
  }
  return rv;
}

nsresult WebWidgetImpl::Init(nsNativeWidget aNativeParent,
                             const nsRect& aBounds,
                             nsIDocument* aDocument,
                             nsIPresContext* aPresContext,
                             nsScrollPreference aScrolling)
{
  NS_PRECONDITION(nsnull != aPresContext, "null ptr");
  NS_PRECONDITION(nsnull != aDocument, "null ptr");
  if ((nsnull == aPresContext) || (nsnull == aDocument)) {
    return NS_ERROR_NULL_POINTER;
  }

  mPresContext = aPresContext;
  NS_ADDREF(aPresContext);

  nsresult rv = MakeWindow(aNativeParent, aBounds, aScrolling);
  if (NS_OK != rv) {
    return rv;
  }

  nsIStyleSet* styleSet;
  rv = CreateStyleSet(aDocument, &styleSet);
  if (NS_OK != rv) {
    return rv;
  }

  // Now make the shell for the document
  rv = aDocument->CreateShell(mPresContext, mViewManager, styleSet,
                              &mPresShell);
  NS_RELEASE(styleSet);
  if (NS_OK != rv) {
    return rv;
  }

  // Now that we have a presentation shell trigger a reflow so we
  // create a frame model
  nsRect bounds;
  mWindow->GetBounds(bounds);
  if (nsnull != mPresShell) {
    nscoord width = bounds.width;
    nscoord height = bounds.height;
    width = NS_TO_INT_ROUND(width * mPresContext->GetPixelsToTwips());
    height = NS_TO_INT_ROUND(height * mPresContext->GetPixelsToTwips());
    mViewManager->SetWindowDimensions(width, height);
  }
  ForceRefresh();
  return rv;
}

nsRect WebWidgetImpl::GetBounds()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  nsRect zr(0, 0, 0, 0);
  if (nsnull != mWindow) {
    mWindow->GetBounds(zr);
  }
  return zr;
}

nsIPresContext* WebWidgetImpl::GetPresContext()
{
  NS_IF_ADDREF(mPresContext);
  return mPresContext;
}

void WebWidgetImpl::SetBounds(const nsRect& aBounds)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    mWindow->Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height, PR_FALSE);
  }
}

void WebWidgetImpl::Move(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Move(aX, aY);
  }
}

void WebWidgetImpl::Show()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Show(PR_TRUE);
  }
}

void WebWidgetImpl::Hide()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Show(PR_FALSE);
  }
}

nsresult WebWidgetImpl::ProvideDefaultHandlers()
{
  // Provide a default link handler if needed
  if (nsnull == mLinkHandler) {
    nsresult rv = NS_NewLinkHandler(&mLinkHandler);
    if (NS_OK != rv) {
      return rv;
    }
    if (NS_OK != mLinkHandler->Init(this)) {
      NS_RELEASE(mLinkHandler);
      return rv;
    }
    if (nsnull != mPresContext) {
      mPresContext->SetLinkHandler(mLinkHandler);
    }
  }
  return NS_OK;
}

// XXX need to save old document in case of failure? Does caller do that?

NS_IMETHODIMP
WebWidgetImpl::LoadURL(const nsString& aURLSpec,
                       nsIStreamListener* aListener,
                       nsIPostData* aPostData)
{
#ifdef NS_DEBUG
  printf("WebWidgetImpl::LoadURL: loadURL(");
  fputs(aURLSpec, stdout);
  printf(")\n");
#endif

  nsresult rv = ProvideDefaultHandlers();
  if (NS_OK != rv) {
    return rv;
  }

  nsIURL* url;
  rv = NS_NewURL(&url, aURLSpec);
  if (NS_OK != rv) {
    return rv;
  }
      

  if (nsnull != mPresShell) {
    // Break circular reference first
    mPresShell->EndObservingDocument();

    if (nsnull != mScriptGlobal) {
      mScriptGlobal->SetNewDocument(nsnull);
    }

    // Then release the shell
    NS_RELEASE(mPresShell);
    mPresShell = nsnull;
  }

  // Create document
  nsIDocument* doc;
  rv = NS_NewHTMLDocument(&doc);

  // set the root web widget to this if its container is not
  // an embedded web webwidget
  //nsISupports* parent;
  //rv = GetContainer(&parent);
  //if ((rv == NS_OK) && (nsnull != parent)) {
  //  nsISupports* webFrame;
  //  rv = parent->QueryInterface(kIWebWidgetIID, (void**)&webFrame);
  //  if (rv == NS_OK) {
  //    NS_RELEASE(webFrame);
  //  } else {
  //    SetRootWebWidget(this);
  //  }
  //}

  ReleaseChildren();

  // Create style set
  nsIStyleSet* styleSet = nsnull;
  rv = CreateStyleSet(doc, &styleSet);
  if (NS_OK != rv) {
    NS_RELEASE(doc);
    return rv;
  }

  // Create presentation shell
  rv = doc->CreateShell(mPresContext, mViewManager, styleSet, &mPresShell);
  NS_RELEASE(styleSet);
  if (NS_OK != rv) {
    NS_RELEASE(doc);
    return rv;
  }

  // Setup view manager's window
  nsRect bounds;
  mWindow->GetBounds(bounds);
  if ((nsnull != mPresContext) && (nsnull != mViewManager)) {
    float p2t = mPresContext->GetPixelsToTwips();
    //reset scrolling offset to upper left
    mViewManager->ResetScrolling();
    nscoord width = NS_TO_INT_ROUND(bounds.width * p2t);
    nscoord height = NS_TO_INT_ROUND(bounds.height * p2t);
    mViewManager->SetWindowDimensions(width, height);
  }

  PRTime start = PR_Now();

  if (nsnull != mScriptGlobal) {
    nsIDOMDocument *domdoc;
    GetDOMDocument(&domdoc);
    if (nsnull != domdoc) {
      mScriptGlobal->SetNewDocument(domdoc);
      NS_RELEASE(domdoc);
    }
  }

  // Set the script object owner to ourselves
  doc->SetScriptContextOwner(this);
  
  // Now load the document
  mPresShell->EnterReflowLock();
  doc->LoadURL(url, aListener, this, aPostData);
  mPresShell->ExitReflowLock();

  PRTime end = PR_Now();
  PRTime conversion, ustoms;
  LL_I2L(ustoms, 1000);
  LL_SUB(conversion, end, start);
  LL_DIV(conversion, conversion, ustoms);
  char buf[500];
  PR_snprintf(buf, sizeof(buf),
              "loading the document took %lldms\n",
              conversion);
  puts(buf);

  NS_RELEASE(doc);

  ForceRefresh();/* XXX temporary */

  return NS_OK;
}

nsIDocument* WebWidgetImpl::GetDocument()
{
  if (nsnull != mPresShell) {
    return mPresShell->GetDocument();
  }
  return nsnull;
}

NS_IMETHODIMP WebWidgetImpl::SetLinkHandler(nsILinkHandler* aHandler)
{ // XXX this should probably be a WEAK reference
  NS_IF_RELEASE(mLinkHandler);
  mLinkHandler = aHandler;
  NS_IF_ADDREF(aHandler);
  if (nsnull != mPresContext) {
    mPresContext->SetLinkHandler(aHandler);
  }
  return NS_OK;
}

NS_IMETHODIMP WebWidgetImpl::GetLinkHandler(nsILinkHandler** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mLinkHandler;
  NS_IF_ADDREF(mLinkHandler);
  return NS_OK;
}

NS_IMETHODIMP WebWidgetImpl::SetContainer(nsISupports* aContainer, PRBool aRelationship)
{ 
  mContainer = aContainer;

  if (nsnull == aContainer) {
    return NS_OK;
  }

  if (aRelationship) {
    // if aContainer is a web widget add this as a child
    nsIWebWidget* ww;
    nsresult result = aContainer->QueryInterface(kIWebWidgetIID, (void**)&ww);
    if (NS_OK == result) {
      nsIWebWidget* thisWW;
      result = QueryInterface(kIWebWidgetIID, (void**)&thisWW);
      if (NS_OK == result) {
        ww->AddChild(thisWW, PR_FALSE);
        NS_RELEASE(thisWW);
      } else {
        NS_ASSERTION(0, "invalid nsISupports");
        return result;
      }
      NS_RELEASE(ww);
    } 
  }
    
  if (nsnull != mPresContext) {
    mPresContext->SetContainer(aContainer);
  }
  return NS_OK;
}

NS_IMETHODIMP WebWidgetImpl::GetContainer(nsISupports** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mContainer;
  return NS_OK;
}

PRInt32 WebWidgetImpl::GetNumChildren() 
{
  return mChildren.Count();
}

NS_IMETHODIMP WebWidgetImpl::AddChild(nsIWebWidget* aChild, PRBool aRelationship)
{
  NS_ASSERTION(nsnull != aChild, "null child");

  mChildren.AppendElement(aChild);

  if (aRelationship) {
    nsISupports* thisSupports;
    nsresult result = QueryInterface(kISupportsIID, (void**)&thisSupports);
    if (NS_OK == result) {
      aChild->SetContainer(thisSupports, PR_FALSE);
      NS_RELEASE(thisSupports);
    } else {
      NS_ASSERTION(0, "invalid nsISupports");
      return result;
    }
  }

  NS_ADDREF(aChild);
  return NS_OK;
}

NS_IMETHODIMP WebWidgetImpl::GetChildAt(PRInt32 aIndex, nsIWebWidget** aChild)
{
  NS_ASSERTION(nsnull != aChild, "null child address");
  *aChild = (nsIWebWidget*)mChildren.ElementAt(aIndex);
  NS_ADDREF(*aChild);
  return NS_OK;
}

//void WebWidgetImpl::SetRootWebWidget(nsIWebWidget* aWebWidget)
//{
//  if (aWebWidget != gRootWebWidget) {
//    NS_IF_RELEASE(gRootWebWidget);
//    gRootWebWidget = aWebWidget;
//    NS_IF_ADDREF(gRootWebWidget);
//  }
//}

nsIWebWidget* WebWidgetImpl::GetRootWebWidget()
{
  nsIWebWidget* childWW = this;
  nsISupports* parSup;
  childWW->GetContainer(&parSup);
  if (parSup) {
    nsIWebWidget* parWW;
    nsresult result = parSup->QueryInterface(kIWebWidgetIID, (void**)&parWW);
    if (NS_OK == result) {
      nsIWebWidget* root = parWW->GetRootWebWidget();
      NS_RELEASE(parWW);
      return root;
    }
  }

  NS_ADDREF(this);
  return this;
}

//----------------------------------------------------------------------
// Debugging methods

void WebWidgetImpl::DumpContent(FILE* out)
{
  if (nsnull == out) {
    out = stdout;
  }
  if (nsnull != mPresShell) {
    nsIDocument* doc = mPresShell->GetDocument();
    if (nsnull != doc) {
      nsIContent* root = doc->GetRootContent();
      if (nsnull == root) {
        fputs("null root content\n", out);
      } else {
        root->List(out);
        NS_RELEASE(root);
      }
      NS_RELEASE(doc);
    } else {
      fputs("null document\n", out);
    }
  } else {
    fputs("null pres shell\n", out);
  }
}

void WebWidgetImpl::DumpFrames(FILE* out)
{
  if (nsnull == out) {
    out = stdout;
  }
  if (nsnull != mPresShell) {
    nsIFrame* root = mPresShell->GetRootFrame();
    if (nsnull == root) {
      fputs("null root frame\n", out);
    } else {
      root->List(out);
    }
  } else {
    fputs("null pres shell\n", out);
  }
}

void WebWidgetImpl::DumpStyleSheets(FILE* out)
{
  if (nsnull == out) {
    out = stdout;
  }
  if (nsnull != mPresShell) {
    nsIStyleSet* styleSet = mPresShell->GetStyleSet();
    if (nsnull == styleSet) {
      fputs("null style set\n", out);
    } else {
      styleSet->List(out);
      NS_RELEASE(styleSet);
    }
  } else {
    fputs("null pres shell\n", out);
  }
}

void WebWidgetImpl::DumpStyleContexts(FILE* out)
{
  if (nsnull == out) {
    out = stdout;
  }
  if (nsnull != mPresShell) {
    nsIStyleSet* styleSet = mPresShell->GetStyleSet();
    if (nsnull == styleSet) {
      fputs("null style set\n", out);
    } else {
      styleSet->ListContexts(out);
      NS_RELEASE(styleSet);
    }
  } else {
    fputs("null pres shell\n", out);
  }
}

void WebWidgetImpl::DumpViews(FILE* out)
{
  if (nsnull == out) {
    out = stdout;
  }
  if (nsnull != mView) {
    mView->List(out);
  } else {
    fputs("null view\n", out);
  }
}

void WebWidgetImpl::ShowFrameBorders(PRBool aEnable)
{
  nsIFrame::ShowFrameBorders(aEnable);
  ForceRefresh();
}

PRBool WebWidgetImpl::GetShowFrameBorders()
{
  return nsIFrame::GetShowFrameBorders();
}

void WebWidgetImpl::ForceRefresh()
{
  mWindow->Invalidate(PR_TRUE);
}

NS_WEB nsresult
NS_NewWebWidget(nsIWebWidget** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  WebWidgetImpl* it = new WebWidgetImpl();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIWebWidgetIID, (void **) aInstancePtrResult);
}

//----------------------------------------------------------------------
// XXX temporary code

nsresult WebWidgetImpl::CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet)
{ // this should eventually get expanded to allow for creating different sets for different media
  nsresult rv = InitUAStyleSheet();

  if (NS_OK != rv) {
    NS_WARNING("unable to load UA style sheet");
//    return rv;
  }

  rv = NS_NewStyleSet(aStyleSet);
  if (NS_OK == rv) {
    PRInt32 count = aDocument->GetNumberOfStyleSheets();
    for (PRInt32 index = 0; index < count; index++) {
      nsIStyleSheet* sheet = aDocument->GetStyleSheetAt(index);
      (*aStyleSet)->AppendDocStyleSheet(sheet);
      NS_RELEASE(sheet);
    }
    if (nsnull != mUAStyleSheet) {
      (*aStyleSet)->AppendBackstopStyleSheet(mUAStyleSheet);
    }
  }
  return rv;
}

nsIWidget* WebWidgetImpl::GetWWWindow()
{
  if (nsnull != mWindow) {
    NS_ADDREF(mWindow);
  }
  return mWindow;
}

nsresult WebWidgetImpl::GetScriptContext(nsIScriptContext **aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null arg");
  nsresult res = NS_OK;

  if (nsnull == mScriptGlobal) {
    res = NS_NewScriptGlobalObject(&mScriptGlobal);
    if (NS_OK != res) {
      return res;
    }

    nsIDOMDocument *document;
    res = GetDOMDocument(&document);
    if (NS_OK != res) {
      return res;
    }
    mScriptGlobal->SetNewDocument(document);
    NS_RELEASE(document);
  }

  if (nsnull == mScriptContext) {
    res = NS_CreateContext(mScriptGlobal, &mScriptContext);
  }

  if (NS_OK == res) {
    *aContext = mScriptContext;
    NS_ADDREF(mScriptContext);
  }

  return res;
}

nsresult WebWidgetImpl::ReleaseScriptContext(nsIScriptContext *aContext)
{
  NS_IF_RELEASE(aContext);

  return NS_OK;
}

nsresult WebWidgetImpl::GetDOMDocument(nsIDOMDocument** aDocument)
{
  nsresult res = NS_OK;
  nsIDocument *doc = GetDocument();
  *aDocument = nsnull;

  static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
   
  if (doc != nsnull) {
    res = doc->QueryInterface(kIDOMDocumentIID, (void**)aDocument);
    NS_RELEASE(doc);
  }

  return res;
}

/*******************************************
/*  nsWebWidgetFactory
/*******************************************/

//static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kCWebWidget, NS_WEBWIDGET_CID);

class nsWebWidgetFactory : public nsIFactory
{   
  public:   
    // nsISupports methods   
    NS_IMETHOD QueryInterface(const nsIID &aIID,    
                                       void **aResult);   
    NS_IMETHOD_(nsrefcnt) AddRef(void);   
    NS_IMETHOD_(nsrefcnt) Release(void);   

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                              const nsIID &aIID,   
                              void **aResult);   

    NS_IMETHOD LockFactory(PRBool aLock);   

    nsWebWidgetFactory(const nsCID &aClass);   
    ~nsWebWidgetFactory();   

  private:   
    nsrefcnt  mRefCnt;   
    nsCID     mClassID;
};   

nsWebWidgetFactory::nsWebWidgetFactory(const nsCID &aClass)   
{   
  mRefCnt = 0;
  mClassID = aClass;
}   

nsWebWidgetFactory::~nsWebWidgetFactory()   
{   
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult nsWebWidgetFactory::QueryInterface(const nsIID &aIID,   
                                            void **aResult)   
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

nsrefcnt nsWebWidgetFactory::AddRef()   
{   
  return ++mRefCnt;   
}   

nsrefcnt nsWebWidgetFactory::Release()   
{   
  if (--mRefCnt == 0) {   
    delete this;   
    return 0; // Don't access mRefCnt after deleting!   
  }   
  return mRefCnt;   
}  

nsresult nsWebWidgetFactory::CreateInstance(nsISupports *aOuter,  
                                            const nsIID &aIID,  
                                            void **aResult)  
{  
  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  WebWidgetImpl *inst = nsnull;

  if (mClassID.Equals(kCWebWidget)) {
    inst = new WebWidgetImpl();
  }

  if (inst == NULL) {  
    return NS_ERROR_OUT_OF_MEMORY;  
  }  

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  

  return res;  
}  

nsresult nsWebWidgetFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

// return the proper factory to the caller
extern "C" NS_WEB nsresult NSGetFactory(const nsCID &aClass, nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsWebWidgetFactory(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}


