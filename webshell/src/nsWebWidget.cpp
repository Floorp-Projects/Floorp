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
#include "nsIScriptObjectOwner.h"

#include "nsICSSParser.h"

#define UA_CSS_URL "resource:/res/ua.css"

#define GET_OUTER() \
  ((nsWebWidget*) ((char*)this - nsWebWidget::GetOuterOffset()))

// Machine independent implementation portion of the web widget
class WebWidgetImpl : public nsIWebWidget, public nsIScriptObjectOwner {
public:
  WebWidgetImpl();
  ~WebWidgetImpl();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  virtual nsresult Init(nsNativeWindow aParent,
                        const nsRect& aBounds);
  virtual nsresult Init(nsNativeWindow aParent,
                        const nsRect& aBounds,
                        nsIDocument* aDocument,
                        nsIPresContext* aPresContext);

  virtual nsRect GetBounds();
  virtual void SetBounds(const nsRect& aBounds);
  virtual void Show();
  virtual void Hide();

  NS_IMETHOD SetContainer(nsISupports* aContainer);

  NS_IMETHOD GetContainer(nsISupports** aResult);

  NS_IMETHOD SetLinkHandler(nsILinkHandler* aHandler);

  NS_IMETHOD GetLinkHandler(nsILinkHandler** aResult);

  NS_IMETHOD LoadURL(const nsString& aURL);

  virtual nsIDocument* GetDocument();

  virtual void DumpContent(FILE* out);
  virtual void DumpFrames(FILE* out);
  virtual void DumpStyle(FILE* out);
  virtual void DumpViews(FILE* out);
  virtual void ShowFrameBorders(PRBool aEnable);
  virtual PRBool GetShowFrameBorders();
  virtual nsIWidget* GetWWWindow();
  virtual nsresult GetScriptContext(nsIScriptContext **aContext);
  virtual nsresult ReleaseScriptContext();

  virtual nsresult GetScriptObject(JSContext *aContext, void** aScriptObject);
  virtual nsresult ResetScriptObject();

private:
  nsresult ProvideDefaultHandlers();
  void ForceRefresh();
  nsresult MakeWindow(nsNativeWindow aParent, const nsRect& aBounds);
  nsresult InitUAStyleSheet(void);
  nsresult CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet);

  nsIWidget* mWindow;
  nsIView *mView;
  nsIViewManager *mViewManager;
  nsIPresContext* mPresContext;
  nsIPresShell* mPresShell;
  nsIStyleSheet* mUAStyleSheet;
  nsILinkHandler* mLinkHandler;
  nsISupports* mContainer;
  nsIScriptContext* mScriptContext;
  void* mScriptObject;
};

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIWebWidgetIID, NS_IWEBWIDGET_IID);

// Note: operator new zeros our memory
WebWidgetImpl::WebWidgetImpl()
{
}

nsresult WebWidgetImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
  if (aIID.Equals(kIWebWidgetIID)) {
    *aInstancePtr = (void*)(nsIWebWidget*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtr = (void*)(nsIScriptObjectOwner*)this;
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
  // Release windows and views
  if (nsnull != mViewManager)
  {
    mViewManager->SetRootView(nsnull);
    mViewManager->SetRootWindow(nsnull);
    NS_RELEASE(mViewManager);
  }

  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mView);

  NS_IF_RELEASE(mContainer);
  NS_IF_RELEASE(mLinkHandler);

  // Note: release context then shell
  NS_IF_RELEASE(mPresContext);
  NS_IF_RELEASE(mPresShell);
  NS_IF_RELEASE(mUAStyleSheet);

  NS_IF_RELEASE(mScriptContext);
}

nsresult WebWidgetImpl::MakeWindow(nsNativeWindow aNativeParent,
                                   const nsRect& aBounds)
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

  // Setup hierarchical relationship in view manager
  mViewManager->SetRootView(mView);
  mWindow = mView->GetWidget();
  if (mWindow) {
    mViewManager->SetRootWindow(mWindow);
  }

  return rv;
}

nsresult WebWidgetImpl::Init(nsNativeWindow aNativeParent,
                             const nsRect& aBounds)
{
  // Create presentation context
  nsresult rv = NS_NewGalleyContext(&mPresContext);
  if (NS_OK != rv) {
    return rv;
  }
  return MakeWindow(aNativeParent, aBounds);
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
            mUAStyleSheet = css->Parse(&ec, uin, uaURL);

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

nsresult WebWidgetImpl::Init(nsNativeWindow aNativeParent,
                             const nsRect& aBounds,
                             nsIDocument* aDocument,
                             nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull != aPresContext, "null ptr");
  NS_PRECONDITION(nsnull != aDocument, "null ptr");
  if ((nsnull == aPresContext) || (nsnull == aDocument)) {
    return NS_ERROR_NULL_POINTER;
  }

  mPresContext = aPresContext;
  NS_ADDREF(aPresContext);

  nsresult rv = MakeWindow(aNativeParent, aBounds);
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

void WebWidgetImpl::SetBounds(const nsRect& aBounds)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height);
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

NS_IMETHODIMP WebWidgetImpl::LoadURL(const nsString& aURLSpec)
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

    // Then release the shell
    NS_RELEASE(mPresShell);
    mPresShell = nsnull;
  }

  // Create document
  nsIDocument* doc;
  rv = NS_NewHTMLDocument(&doc);

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

  // Now load the document
  doc->LoadURL(url);

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

NS_IMETHODIMP WebWidgetImpl::SetContainer(nsISupports* aContainer)
{ // XXX this should most likely be a WEAK reference
  NS_IF_RELEASE(mContainer);
  mContainer = aContainer;
  NS_IF_ADDREF(aContainer);
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
  NS_IF_ADDREF(mContainer);
  return NS_OK;
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

void WebWidgetImpl::DumpStyle(FILE* out)
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

  if (nsnull == mScriptContext) {
    res = NS_CreateContext(this, &mScriptContext);
  }

  if (NS_OK == res) {
    NS_ADDREF(mScriptContext);
    *aContext = mScriptContext;
  }

  return res;
}

nsresult WebWidgetImpl::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

nsresult WebWidgetImpl::ReleaseScriptContext()
{
  NS_IF_RELEASE(mScriptContext);
  mScriptContext = nsnull;

  return NS_OK;
}

nsresult WebWidgetImpl::GetScriptObject(JSContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    mScriptObject = mScriptContext->GetGlobalObject();
    if (nsnull == mScriptObject) {
      res = NS_ERROR_FAILURE;
    }
  }
  
  *aScriptObject = (void*)mScriptObject;
  return res;
}



