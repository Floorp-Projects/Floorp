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
#include "nsICSSParser.h"
#include "nsIDOMDocument.h"
#include "nsIStyleContext.h"
#include "prprf.h"
#include "prtime.h"
#include "prlog.h"
#include "nsVoidArray.h"
#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"

#include "nsIDocumentLoader.h"

// XXX: a copy exists in nsPresShell.cpp!!!
#define UA_CSS_URL "resource:/res/ua.css"
#include "nsIDocumentWidget.h"

#define GET_OUTER() \
  ((nsWebWidget*) ((char*)this - nsWebWidget::GetOuterOffset()))

// Machine independent implementation portion of the web widget
class WebWidgetImpl : public nsIWebWidget, 
                      public nsIViewerContainer
{
public:
  WebWidgetImpl();
  ~WebWidgetImpl();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWebWidget
  NS_IMETHOD LoadURL(const nsString& aURLSpec, nsIStreamObserver* aObserver,
                     nsIPostData* aPostData);
  NS_IMETHOD GetContentViewer(nsIContentViewer*& aResult);

  //XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  // XXX from here on down is a mess

  NS_IMETHOD QueryCapability(const nsIID &aIID, void** aResult);

  NS_IMETHOD Embed(nsIContentViewer* aDocViewer, 
                   const char* aCommand, 
                   nsISupports* aExtraInfo);

  NS_IMETHOD SetContainer(nsIViewerContainer* aContainer);

  NS_IMETHOD Init(nsNativeWidget aNativeParent,
                  const nsRect& aBounds,
                  nsScrollPreference aScrolling);

  NS_IMETHOD Init(nsNativeWidget aParent,
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

  virtual nsIDocument* GetDocument();

  virtual nsIWidget* GetWWWindow();
  NS_IMETHOD GetDOMDocument(nsIDOMDocument** aDocument);

  NS_IMETHOD BindToDocument(nsISupports *aDoc, const char *aCommand);

  nsIStyleSheet* GetUAStyleSheet();
  NS_IMETHOD SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet);

private:
  nsresult ProvideDefaultHandlers();
  void ForceRefresh();
  nsresult InitUAStyleSheet(void);
  nsresult CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet);
  void ReleaseChildren();

  static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);


  nsIWebWidgetViewer* mContentViewer;

  nsIWidget* mWindow;
  nsIView *mView;
  nsIPresContext* mPresContext;
  nsIPresShell* mPresShell;
  nsIStyleSheet* mUAStyleSheet;
  nsILinkHandler* mLinkHandler;
  nsISupports* mContainer;
  nsIDocument* mDocument;

  nsIDocumentLoader* mDocLoader;

  nsVoidArray mChildren;
  //static nsIWebWidget* gRootWebWidget;
  nsString* mName;
  nsIDeviceContext *mDeviceContext;
};

//----------------------------------------------------------------------
#ifdef NS_DEBUG
/**
 * Note: the log module is created during initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule = PR_NewLogModule("webwidget");
#endif

#define WEB_TRACE_CALLS        0x1

#define WEB_LOG_TEST(_lm,_bit) (PRIntn((_lm)->level) & (_bit))

#ifdef NS_DEBUG
#define WEB_TRACE(_bit,_args)            \
  PR_BEGIN_MACRO                         \
    if (WEB_LOG_TEST(gLogModule,_bit)) { \
      PR_LogPrint _args;                 \
    }                                    \
  PR_END_MACRO
#else
#define WEB_TRACE(_bit,_args)
#endif

//----------------------------------------------------------------------
static NS_DEFINE_IID(kChildCID, NS_CHILD_CID);
static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_IID(kCDocumentLoaderCID, NS_DOCUMENTLOADER_CID);

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIWebWidgetIID, NS_IWEBWIDGET_IID);
static NS_DEFINE_IID(kIWebWidgetViewerIID, NS_IWEBWIDGETVIEWER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDocumentLoaderIID, NS_IDOCUMENTLOADER_IID);
static NS_DEFINE_IID(kILinkHandlerIID, NS_ILINKHANDLER_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);


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

// Note: operator new zeros our memory
WebWidgetImpl::WebWidgetImpl()
{
  NS_INIT_REFCNT();

  NSRepository::CreateInstance(kCDocumentLoaderCID, nsnull, kIDocumentLoaderIID, (void**)&mDocLoader);
  WEB_TRACE(WEB_TRACE_CALLS,
            ("WebWidgetImpl::WebWidgetImpl: this=%p"));
}

nsresult WebWidgetImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIWebWidgetIID)) {
    *aInstancePtr = (void*)(nsIWebWidget*)this;
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
  WEB_TRACE(WEB_TRACE_CALLS,
            ("WebWidgetImpl::~WebWidgetImpl: this=%p"));

  ReleaseChildren();
  mContainer = nsnull;

  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mView);

  NS_IF_RELEASE(mLinkHandler);

  NS_IF_RELEASE(mDocument);

  // Note: release context then shell
  NS_IF_RELEASE(mPresContext);

  if (nsnull != mPresShell) {
    // Break circular reference first
    mPresShell->EndObservingDocument();

    // Then release the shell
    NS_RELEASE(mPresShell);
  }

  NS_IF_RELEASE(mUAStyleSheet);

  NS_IF_RELEASE(mDeviceContext);

  NS_IF_RELEASE(mContentViewer);
}


NS_IMETHODIMP
WebWidgetImpl::QueryCapability(const nsIID &aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kILinkHandlerIID)) {
    if (nsnull != mLinkHandler) {
      *aInstancePtr = mLinkHandler;
      mLinkHandler->AddRef();
      return NS_OK;
    }
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
WebWidgetImpl::SetContainer(nsIViewerContainer* aContainer)
{
    NS_ASSERTION(0, "Not implemented...");

    return NS_ERROR_FAILURE;
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



void
WebWidgetImpl::ReleaseChildren()
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


NS_IMETHODIMP
WebWidgetImpl::Init(nsNativeWidget aNativeParent,
                    const nsRect& aBounds,
                    nsScrollPreference aScrolling)
{
  nsresult rv = NS_OK;

  NS_PRECONDITION(nsnull != aNativeParent, "null Parent Window");
  if (nsnull == aNativeParent) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }

  // Create device context
  rv = NSRepository::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&mDeviceContext);
  if (NS_OK != rv) {
    goto done;
  }
  mDeviceContext->Init(aNativeParent);
  mDeviceContext->SetDevUnitsToAppUnits(mDeviceContext->GetDevUnitsToTwips());
  mDeviceContext->SetAppUnitsToDevUnits(mDeviceContext->GetTwipsToDevUnits());
  mDeviceContext->SetGamma(1.7f);
  

  // Create a Native window for the WebWidget container...
  rv = NSRepository::CreateInstance(kChildCID, nsnull, kIWidgetIID, (void**)&mWindow);
  if (NS_OK != rv) {
    goto done;
  }
  mWindow->Create(aNativeParent, aBounds, WebWidgetImpl::HandleEvent, mDeviceContext, nsnull);

  // Create a Style Sheet...
  rv = InitUAStyleSheet();

done:
  return rv;
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

NS_IMETHODIMP
WebWidgetImpl::Init(nsNativeWidget aNativeParent,
                    const nsRect& aBounds,
                    nsIDocument* aDocument,
                    nsIPresContext* aPresContext,
                    nsScrollPreference aScrolling)
{
  nsresult rv;

  NS_PRECONDITION(nsnull != aNativeParent, "null Parent Window");
  if (nsnull == aNativeParent) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
  }

  rv = InitUAStyleSheet();
///  if (nsnull != mContentViewer) {
///    rv = mContentViewer->Init(aNativeParent, aBounds, aDocument, aPresContext, aScrolling);
///  } else {
///    rv = NS_ERROR_NULL_POINTER;
///  }

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
  nsIPresContext* presContext = nsnull;

  if (nsnull != mContentViewer) {
    presContext = mContentViewer->GetPresContext();
  }

  return presContext;
}

void WebWidgetImpl::SetBounds(const nsRect& aBounds)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    mWindow->Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height, PR_FALSE);
  }

  if (nsnull != mContentViewer) {
    nsRect rr(0, 0, aBounds.width, aBounds.height);
    mContentViewer->SetBounds(rr);
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

  if (nsnull != mContentViewer) {
    mContentViewer->Show();
  }
}

void WebWidgetImpl::Hide()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Show(PR_FALSE);
  }

  if (nsnull != mContentViewer) {
    mContentViewer->Hide();
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

NS_IMETHODIMP
WebWidgetImpl::BindToDocument(nsISupports* aDoc, const char* aCommand)
{
  nsresult rv = NS_ERROR_FAILURE;

  WEB_TRACE(WEB_TRACE_CALLS,
            ("WebWidgetImpl::BindToDocument: this=%p aDoc=%p aCommand=%s",
             this, aDoc, aCommand ? aCommand : ""));

  NS_PRECONDITION(nsnull != mContentViewer, "null Content Viewer");
  if (nsnull != mContentViewer) {
    rv = mContentViewer->BindToDocument(aDoc, aCommand);
  }

  return rv;
}

static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);

NS_IMETHODIMP
WebWidgetImpl::Embed(nsIContentViewer* aDocViewer, 
                     const char* aCommand, 
                     nsISupports* aExtraInfo)
{
  nsresult rv;
  nsRect bounds;

  WEB_TRACE(WEB_TRACE_CALLS,
            ("WebWidgetImpl::Embed: this=%p aDocViewer=%p aCommand=%s aExtraInfo=%p",
             this, aDocViewer, aCommand ? aCommand : "", aExtraInfo));

  NS_IF_RELEASE(mContentViewer);


  rv = aDocViewer->QueryInterface(kIWebWidgetViewerIID, (void**)&mContentViewer);

  if (NS_OK == rv) {
    mContentViewer->SetUAStyleSheet(mUAStyleSheet);

    mWindow->GetBounds(bounds);
    bounds.y = bounds.y = 0;
    rv = mContentViewer->Init(mWindow->GetNativeData(NS_NATIVE_WIDGET), 
                              mDeviceContext, 
                              bounds);
  }

  if (NS_OK == rv) {
    mContentViewer->Show();
  }

  return rv;
}


NS_IMETHODIMP
WebWidgetImpl::LoadURL(const nsString& aURLSpec,
                       nsIStreamObserver* anObserver,
                       nsIPostData* aPostData)
{
    nsresult rv;

    rv = mDocLoader->LoadURL(aURLSpec,       // URL string
                             nsnull,         // Command
                             this,           // Container
                             aPostData,      // Post Data
                             nsnull,         // Extra Info...
                             anObserver);    // Observer

    return rv;
}


NS_IMETHODIMP
WebWidgetImpl::GetContentViewer(nsIContentViewer*& aResult)
{
  NS_IF_ADDREF(mContentViewer);
  aResult = mContentViewer;
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


nsEventStatus PR_CALLBACK WebWidgetImpl::HandleEvent(nsGUIEvent *aEvent)
{ 
  return nsEventStatus_eIgnore;
}


//----------------------------------------------------------------------
// Debugging methods

void WebWidgetImpl::ForceRefresh()
{
  mWindow->Invalidate(PR_TRUE);
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

nsIStyleSheet* WebWidgetImpl::GetUAStyleSheet()
{
    return mUAStyleSheet;
}

NS_IMETHODIMP
WebWidgetImpl::SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet)
{
    NS_IF_RELEASE(mUAStyleSheet);
    mUAStyleSheet = aUAStyleSheet;
    NS_IF_ADDREF(mUAStyleSheet);

    return NS_OK;
}


/*******************************************
 *  nsWebWidgetFactory
 *******************************************/

//static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kCWebWidget, NS_WEBWIDGET_CID);
static NS_DEFINE_IID(kCDocumentLoader, NS_DOCUMENTLOADER_CID);

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
  nsresult rv = NS_OK;

  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aClass.Equals(kCWebWidget)) {
    nsIFactory* inst;

    inst = new nsWebWidgetFactory(aClass);
    NS_IF_ADDREF(inst);
    *aFactory = inst;

    if (nsnull == inst) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  else if (aClass.Equals(kCDocumentLoader)) {
    rv = NS_NewDocumentLoaderFactory(aFactory);
  }

  return rv;
}


