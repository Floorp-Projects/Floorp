/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsIDocumentWidget.h"
#include "nsIDocumentLoader.h"

#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"

#include "nsILinkHandler.h"

#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIViewManager.h"
#include "nsIView.h"

#include "nsIURL.h"

class ContentViewerImpl : public nsIWebWidgetViewer
{
public:
    ContentViewerImpl();
    
    void* operator new(size_t sz) {
        void* rv = new char[sz];
        nsCRT::zero(rv, sz);
        return rv;
    }

    // nsISupports interface...
    NS_DECL_ISUPPORTS

    // nsIContentViewer interface...
    NS_IMETHOD Init(nsNativeWidget aParent,
                    nsIDeviceContext* aDeviceContext,
                    const nsRect& aBounds,
                    nsScrollPreference aScrolling = nsScrollPreference_kAuto);
    
    NS_IMETHOD BindToDocument(nsISupports* aDoc, const char* aCommand);
    NS_IMETHOD SetContainer(nsIViewerContainer* aContainer);

    virtual nsRect GetBounds();
    virtual void SetBounds(const nsRect& aBounds);
    virtual void Move(PRInt32 aX, PRInt32 aY);
    virtual void Show();
    virtual void Hide();


    // nsIWebWidgetViewer interface...
    NS_IMETHOD Init(nsNativeWidget aParent,
                    const nsRect& aBounds,
                    nsIDocument* aDocument,
                    nsIPresContext* aPresContext,
                    nsScrollPreference aScrolling = nsScrollPreference_kAuto);

    NS_IMETHOD SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet);
  
    virtual nsIPresContext* GetPresContext();


protected:
    virtual ~ContentViewerImpl();

private:
    void ForceRefresh(void);
    nsresult CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet);
    nsresult MakeWindow(nsNativeWidget aNativeParent,
                        const nsRect& aBounds,
                        nsScrollPreference aScrolling);

protected:
    nsIViewManager* mViewManager;
    nsIView*        mView;
    nsIWidget*      mWindow;
    nsIViewerContainer* mContainer;

    nsIDocument*    mDocument;
    nsIPresContext* mPresContext;
    nsIPresShell*   mPresShell;
    nsIStyleSheet*  mUAStyleSheet;

};

//Class IDs
static NS_DEFINE_IID(kViewManagerCID,       NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kScrollingViewCID,     NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kWidgetCID,            NS_CHILD_CID);


// Interface IDs
static NS_DEFINE_IID(kISupportsIID,         NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDocumentIID,         NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIViewManagerIID,      NS_IVIEWMANAGER_IID);
static NS_DEFINE_IID(kIViewIID,             NS_IVIEW_IID);
static NS_DEFINE_IID(kScrollViewIID,        NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIDocumentWidgetIID,   NS_IDOCUMENTWIDGET_IID);
static NS_DEFINE_IID(kIWebWidgetViewerIID,  NS_IWEBWIDGETVIEWER_IID);
static NS_DEFINE_IID(kILinkHandlerIID,      NS_ILINKHANDLER_IID);


// Note: operator new zeros our memory
ContentViewerImpl::ContentViewerImpl()
{
    NS_INIT_REFCNT();
}

// ISupports implementation...
NS_IMPL_ADDREF(ContentViewerImpl)
NS_IMPL_RELEASE(ContentViewerImpl)

nsresult ContentViewerImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    if (aIID.Equals(kIDocumentWidgetIID)) {
        *aInstancePtr = (void*)(nsIContentViewer*)this;
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIWebWidgetViewerIID)) {
        *aInstancePtr = (void*)(nsIWebWidgetViewer*)this;
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*)(nsISupports*)(nsIContentViewer*)this;
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


ContentViewerImpl::~ContentViewerImpl()
{
    // Release windows and views
    if (nsnull != mViewManager) {
        mViewManager->SetRootView(nsnull);
        mViewManager->SetRootWindow(nsnull);
        NS_RELEASE(mViewManager);
    }
    NS_IF_RELEASE(mWindow);
    NS_IF_RELEASE(mView);

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
    NS_IF_RELEASE(mContainer);
}



/*
 * This method is called by the Document Loader once a document has been created
 * for a particular data stream...  The content viewer must cache this document
 * for later use when Init(...) is called.
 */
NS_IMETHODIMP
ContentViewerImpl::BindToDocument(nsISupports *aDoc, const char *aCommand)
{
  nsresult rv;

  NS_PRECONDITION(nsnull == mDocument, "Viewer is already bound to a document!");

#ifdef NS_DEBUG
  printf("ContentViewerImpl::BindToDocument\n");
#endif

  rv = aDoc->QueryInterface(kIDocumentIID, (void**)&mDocument);
  return rv;

}

NS_IMETHODIMP
ContentViewerImpl::SetContainer(nsIViewerContainer* aContainer)
{
    NS_IF_RELEASE(mContainer);
    mContainer = aContainer;
    NS_IF_ADDREF(mContainer);

    return NS_OK;
}


NS_IMETHODIMP
ContentViewerImpl::Init(nsNativeWidget aNativeParent,
                        nsIDeviceContext* aDeviceContext,
                        const nsRect& aBounds,
                        nsScrollPreference aScrolling)
{
    nsresult rv;

    if (nsnull == mDocument) {
        return NS_ERROR_NULL_POINTER;
    }

    // Create presentation context
    rv = NS_NewGalleyContext(&mPresContext);
    if (NS_OK != rv) {
        return rv;
    }

    mPresContext->Init(aDeviceContext);
    rv = Init(aNativeParent, aBounds, mDocument, mPresContext, aScrolling);

    // Init(...) will addref the Presentation Context...
    if (NS_OK == rv) {
        mPresContext->Release();
    }
    return rv;
}


NS_IMETHODIMP
ContentViewerImpl::Init(nsNativeWidget aNativeParent,
                        const nsRect& aBounds,
                        nsIDocument* aDocument,
                        nsIPresContext* aPresContext,
                        nsScrollPreference aScrolling)
{
    nsresult rv;
    nsRect bounds;

    NS_PRECONDITION(nsnull != aPresContext, "null ptr");
    NS_PRECONDITION(nsnull != aDocument,    "null ptr");
    if ((nsnull == aPresContext) || (nsnull == aDocument)) {
        rv = NS_ERROR_NULL_POINTER;
        goto done;
    }

    mPresContext = aPresContext;
    NS_ADDREF(mPresContext);

    if (nsnull != mContainer) {
        nsILinkHandler* linkHandler = nsnull;

        mContainer->QueryCapability(kILinkHandlerIID, (void**)&linkHandler);
        mPresContext->SetContainer(mContainer);
        mPresContext->SetLinkHandler(linkHandler);
        NS_IF_RELEASE(linkHandler);
    }

    // Create the ViewManager and Root View...
    MakeWindow(aNativeParent, aBounds, aScrolling);

    // Create the style set...
    nsIStyleSet* styleSet;
    rv = CreateStyleSet(aDocument, &styleSet);
    if (NS_OK != rv) {
        goto done;
    }

    // Now make the shell for the document
    rv = aDocument->CreateShell(mPresContext, mViewManager, styleSet,
                                &mPresShell);
    NS_RELEASE(styleSet);
    if (NS_OK != rv) {
        goto done;
    }

    // Now that we have a presentation shell trigger a reflow so we
    // create a frame model
    mWindow->GetBounds(bounds);
    if (nsnull != mPresShell) {
        nscoord width = bounds.width;
        nscoord height = bounds.height;
        width = NS_TO_INT_ROUND(width * mPresContext->GetPixelsToTwips());
        height = NS_TO_INT_ROUND(height * mPresContext->GetPixelsToTwips());
        mViewManager->SetWindowDimensions(width, height);
    }
    ForceRefresh();

done:
    return rv;
}

NS_IMETHODIMP
ContentViewerImpl::SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet)
{
    NS_IF_RELEASE(mUAStyleSheet);
    mUAStyleSheet = aUAStyleSheet;
    NS_IF_ADDREF(mUAStyleSheet);

    return NS_OK;
}


nsIPresContext* ContentViewerImpl::GetPresContext()
{
  NS_IF_ADDREF(mPresContext);
  return mPresContext;
}


nsRect ContentViewerImpl::GetBounds()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  nsRect zr(0, 0, 0, 0);
  if (nsnull != mWindow) {
    mWindow->GetBounds(zr);
  }
  return zr;
}


void ContentViewerImpl::SetBounds(const nsRect& aBounds)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    mWindow->Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height, PR_FALSE);
  }
}


void ContentViewerImpl::Move(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Move(aX, aY);
  }
}

void ContentViewerImpl::Show()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Show(PR_TRUE);
  }
}

void ContentViewerImpl::Hide()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Show(PR_FALSE);
  }
}



void ContentViewerImpl::ForceRefresh()
{
    mWindow->Invalidate(PR_TRUE);
}

nsresult ContentViewerImpl::CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet)
{ // this should eventually get expanded to allow for creating different sets for different media
    nsresult rv;

    if (nsnull == mUAStyleSheet) {
        NS_WARNING("unable to load UA style sheet");
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


nsresult ContentViewerImpl::MakeWindow(nsNativeWidget aNativeParent,
                                       const nsRect& aBounds,
                                       nsScrollPreference aScrolling)
{
  nsresult rv;

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

  nsIScrollableView* scrollView;
  rv = mView->QueryInterface(kScrollViewIID, (void**)&scrollView);
  if (NS_OK == rv) {
    scrollView->SetScrollPreference(aScrolling);
    NS_RELEASE(scrollView);
  }
  else {
    NS_ASSERTION(0, "invalid scrolling view");
    return rv;
  }

  // Setup hierarchical relationship in view manager
  mViewManager->SetRootView(mView);
  mWindow = mView->GetWidget();
  if (nsnull != mWindow) {
    mViewManager->SetRootWindow(mWindow);
  }

  //set frame rate to 25 fps
  mViewManager->SetFrameRate(25);

  return rv;
}



extern "C" NS_WEB nsresult NS_NewContentViewer(nsIWebWidgetViewer** aViewer)
{
    if (nsnull == aViewer) {
        return NS_ERROR_NULL_POINTER;
    }

    *aViewer = new ContentViewerImpl();
    if (nsnull == *aViewer) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(*aViewer);

    return NS_OK;
}


