/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsIContentViewerContainer.h"
#include "nsIDocumentViewer.h"

#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsIFrame.h"

#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsILinkHandler.h"
#include "nsIDOMDocument.h"

#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "nsIDeviceContextSpecFactory.h"
#include "nsIViewManager.h"
#include "nsIView.h"

#include "nsIPref.h"
#include "nsIPageSequenceFrame.h"
#include "nsIURL.h"

#ifdef NS_DEBUG
#undef NOISY_VIEWER
#else
#undef NOISY_VIEWER
#endif

class DocumentViewerImpl : public nsIDocumentViewer
{
public:
    DocumentViewerImpl();
    DocumentViewerImpl(nsIPresContext* aPresContext);
  
    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

    // nsISupports interface...
    NS_DECL_ISUPPORTS

    // nsIContentViewer interface...
    NS_IMETHOD Init(nsNativeWidget aParent,
                    nsIDeviceContext* aDeviceContext,
                    nsIPref* aPrefs,
                    const nsRect& aBounds,
                    nsScrollPreference aScrolling = nsScrollPreference_kAuto);
    NS_IMETHOD BindToDocument(nsISupports* aDoc, const char* aCommand);
    NS_IMETHOD SetContainer(nsIContentViewerContainer* aContainer);
    NS_IMETHOD GetContainer(nsIContentViewerContainer*& aContainerResult);
    NS_IMETHOD Stop(void);
    NS_IMETHOD GetBounds(nsRect& aResult);
    NS_IMETHOD SetBounds(const nsRect& aBounds);
    NS_IMETHOD Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD Show();
    NS_IMETHOD Hide();
    NS_IMETHOD Print(void);

    // nsIDocumentViewer interface...
    NS_IMETHOD SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet);
    NS_IMETHOD GetDocument(nsIDocument*& aResult);
    NS_IMETHOD GetPresShell(nsIPresShell*& aResult);
    NS_IMETHOD GetPresContext(nsIPresContext*& aResult);
    NS_IMETHOD CreateDocumentViewerUsing(nsIPresContext* aPresContext,
                                         nsIDocumentViewer*& aResult);

protected:
    virtual ~DocumentViewerImpl();

private:
    void ForceRefresh(void);
    nsresult CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet);
    nsresult MakeWindow(nsNativeWidget aNativeParent,
                        const nsRect& aBounds,
                        nsScrollPreference aScrolling);

protected:

    // IMPORTANT: The ownership implicit in the following member variables has been 
    // explicitly checked and set using nsCOMPtr for owning pointers and raw COM interface 
    // pointers for weak (ie, non owning) references. If you add any members to this
    // class, please make the ownership explicit (pinkerton, scc).
  
    nsIContentViewerContainer* mContainer; // [WEAK] it owns me!
    nsCOMPtr<nsIDeviceContext> mDeviceContext;   // ??? can't hurt, but...
    nsIView*                 mView;        // [WEAK] cleaned up by view mgr

    // the following six items are explicitly in this order
    // so they will be destroyed in the reverse order (pinkerton, scc)
    nsCOMPtr<nsIDocument>    mDocument;
    nsCOMPtr<nsIWidget>      mWindow;      // ??? should we really own it?
    nsCOMPtr<nsIViewManager> mViewManager;
    nsCOMPtr<nsIPresContext> mPresContext;
    nsCOMPtr<nsIPresShell>   mPresShell;

    nsCOMPtr<nsIStyleSheet>  mUAStyleSheet;
};

//Class IDs
static NS_DEFINE_IID(kViewManagerCID,       NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kScrollingViewCID,     NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kWidgetCID,            NS_CHILD_CID);
static NS_DEFINE_IID(kViewCID,              NS_VIEW_CID);


// Interface IDs
static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID,         NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDocumentIID,         NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMDocumentIID,      NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIViewManagerIID,      NS_IVIEWMANAGER_IID);
static NS_DEFINE_IID(kIViewIID,             NS_IVIEW_IID);
static NS_DEFINE_IID(kScrollViewIID,        NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIContentViewerIID,    NS_ICONTENT_VIEWER_IID);
static NS_DEFINE_IID(kIDocumentViewerIID,   NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kILinkHandlerIID,      NS_ILINKHANDLER_IID);


// Note: operator new zeros our memory
DocumentViewerImpl::DocumentViewerImpl()
{
    NS_INIT_REFCNT();
}

DocumentViewerImpl::DocumentViewerImpl(nsIPresContext* aPresContext)
  : mPresContext(dont_QueryInterface(aPresContext))
{
    NS_INIT_REFCNT();
}

// ISupports implementation...
NS_IMPL_ADDREF(DocumentViewerImpl)
NS_IMPL_RELEASE(DocumentViewerImpl)

nsresult DocumentViewerImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    if (aIID.Equals(kIContentViewerIID)) {
        *aInstancePtr = (void*)(nsIContentViewer*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(kIDocumentViewerIID)) {
        *aInstancePtr = (void*)(nsIDocumentViewer*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*)(nsISupports*)(nsIContentViewer*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


DocumentViewerImpl::~DocumentViewerImpl()
{
    if (mDocument) {
      //Break global object circular reference on the document
      //created in the DocViewer Init
      nsIScriptContextOwner *mOwner = mDocument->GetScriptContextOwner();
      if (nsnull != mOwner) {
        nsIScriptGlobalObject *mGlobal;
        mOwner->GetScriptGlobalObject(&mGlobal);
        if (nsnull != mGlobal) {
          mGlobal->SetNewDocument(nsnull);
          NS_RELEASE(mGlobal);
        }
        NS_RELEASE(mOwner);

        mDocument->SetScriptContextOwner(nsnull);  // out of band cleanup of webshell
      }
    }

    if (mDeviceContext)
      mDeviceContext->FlushFontCache();

    if (mPresShell) {
        // Break circular reference (or something)
        mPresShell->EndObservingDocument();
    }
}



/*
 * This method is called by the Document Loader once a document has
 * been created for a particular data stream...  The content viewer
 * must cache this document for later use when Init(...) is called.
 */
NS_IMETHODIMP
DocumentViewerImpl::BindToDocument(nsISupports *aDoc, const char *aCommand)
{
    NS_PRECONDITION(!mDocument, "Viewer is already bound to a document!");

#ifdef NOISY_VIEWER
    printf("DocumentViewerImpl::BindToDocument\n");
#endif

    nsresult rv;
    mDocument = do_QueryInterface(aDoc,&rv);
    return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::SetContainer(nsIContentViewerContainer* aContainer)
{
    mContainer = aContainer;
 
    // this seems wrong if someone passes in nsnull for |aContainer|. it will leave
    // |mPresContext| pointing to the old container??? (pinkerton, scc)
    if (aContainer && mPresContext) 
      mPresContext->SetContainer(aContainer);

    return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetContainer(nsIContentViewerContainer*& aResult)
{
    aResult = mContainer;
    NS_IF_ADDREF(aResult);
    return NS_OK;
}


NS_IMETHODIMP
DocumentViewerImpl::Init(nsNativeWidget aNativeParent,
                         nsIDeviceContext* aDeviceContext,
                         nsIPref* aPrefs,
                         const nsRect& aBounds,
                         nsScrollPreference aScrolling)
{
    nsresult rv;

    if (!mDocument) {
        return NS_ERROR_NULL_POINTER;
    }

    mDeviceContext = dont_QueryInterface(aDeviceContext);

    PRBool makeCX = PR_FALSE;
    if (!mPresContext) {
        // Create presentation context
        rv = NS_NewGalleyContext(getter_AddRefs(mPresContext));
        if (NS_OK != rv) {
            return rv;
        }

        mPresContext->Init(aDeviceContext, aPrefs); 
        makeCX = PR_TRUE;
    }

    if (nsnull != mContainer) {
        nsILinkHandler* linkHandler = nsnull;
        mContainer->QueryCapability(kILinkHandlerIID, (void**)&linkHandler);
        mPresContext->SetContainer(mContainer);
        mPresContext->SetLinkHandler(linkHandler);
        NS_IF_RELEASE(linkHandler);

        // Set script-context-owner in the document
        nsIScriptContextOwner* owner = nsnull;
        mContainer->QueryCapability(kIScriptContextOwnerIID, (void**)&owner);
        if (nsnull != owner) {
            mDocument->SetScriptContextOwner(owner);
            nsIScriptGlobalObject* global;
            owner->GetScriptGlobalObject(&global);
            if (nsnull != global) {
                nsIDOMDocument *domdoc = nsnull;
                mDocument->QueryInterface(kIDOMDocumentIID,
                                          (void**) &domdoc);
                if (nsnull != domdoc) {
                    global->SetNewDocument(domdoc);
                    NS_RELEASE(domdoc);
                }
                NS_RELEASE(global);
            }
            NS_RELEASE(owner);
        }
    }

    // Create the ViewManager and Root View...
    MakeWindow(aNativeParent, aBounds, aScrolling);

    // Create the style set...
    nsIStyleSet* styleSet;
    rv = CreateStyleSet(mDocument, &styleSet);
    if (NS_OK == rv) {
        // Now make the shell for the document
        rv = mDocument->CreateShell(mPresContext, mViewManager, styleSet,
                                    getter_AddRefs(mPresShell));
        NS_RELEASE(styleSet);
        if (NS_OK == rv) {
            // Initialize our view manager
            nsRect bounds;
            mWindow->GetBounds(bounds);
            nscoord width = bounds.width;
            nscoord height = bounds.height;
            float p2t;
            mPresContext->GetPixelsToTwips(&p2t);
            width = NSIntPixelsToTwips(width, p2t);
            height = NSIntPixelsToTwips(height, p2t);
            mViewManager->DisableRefresh();
            mViewManager->SetWindowDimensions(width, height);

            if (!makeCX) {
                // Make shell an observer for next time
                mPresShell->BeginObservingDocument();

                // Resize-reflow this time
                mPresShell->InitialReflow(width, height);

                // Now trigger a refresh
                mViewManager->EnableRefresh();
            }
        }
    }

    return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::Stop(void)
{
    if (mPresContext) {
        mPresContext->Stop();
    }
    return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet)
{
    mUAStyleSheet = dont_QueryInterface(aUAStyleSheet);
    return NS_OK;
}
  
NS_IMETHODIMP
DocumentViewerImpl::GetDocument(nsIDocument*& aResult)
{
    aResult = mDocument;
    NS_IF_ADDREF(aResult);
    return NS_OK;
}
  
NS_IMETHODIMP
DocumentViewerImpl::GetPresShell(nsIPresShell*& aResult)
{
    aResult = mPresShell;
    NS_IF_ADDREF(aResult);
    return NS_OK;
}
  
NS_IMETHODIMP
DocumentViewerImpl::GetPresContext(nsIPresContext*& aResult)
{
    aResult = mPresContext;
    NS_IF_ADDREF(aResult);
    return NS_OK;
}


NS_IMETHODIMP
DocumentViewerImpl::GetBounds(nsRect& aResult)
{
    NS_PRECONDITION(mWindow, "null window");
    if (mWindow) {
        mWindow->GetBounds(aResult);
    }
    else {
        aResult.SetRect(0, 0, 0, 0);
    }
    return NS_OK;
}


NS_IMETHODIMP
DocumentViewerImpl::SetBounds(const nsRect& aBounds)
{
    NS_PRECONDITION(mWindow, "null window");
    if (mWindow) {
        // Don't have the widget repaint. Layout will generate repaint requests
        // during reflow
        mWindow->Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height, PR_FALSE);
    }
    return NS_OK;
}


NS_IMETHODIMP
DocumentViewerImpl::Move(PRInt32 aX, PRInt32 aY)
{
    NS_PRECONDITION(mWindow, "null window");
    if (mWindow) {
        mWindow->Move(aX, aY);
    }
    return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Show(void)
{
    NS_PRECONDITION(mWindow, "null window");
    if (mWindow) {
        mWindow->Show(PR_TRUE);
    }
    return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Hide(void)
{
    NS_PRECONDITION(mWindow, "null window");
    if (mWindow) {
        mWindow->Show(PR_FALSE);
    }
    return NS_OK;
}


static NS_DEFINE_IID(kIDeviceContextSpecFactoryIID, NS_IDEVICE_CONTEXT_SPEC_FACTORY_IID);
static NS_DEFINE_IID(kDeviceContextSpecFactoryCID, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);

NS_IMETHODIMP DocumentViewerImpl::Print(void)
{
  nsIDeviceContextSpecFactory *factory = nsnull;

  nsComponentManager::CreateInstance(kDeviceContextSpecFactoryCID, nsnull,
                               kIDeviceContextSpecFactoryIID, (void **)&factory);

  if (nsnull != factory)
  {
    nsIDeviceContextSpec *devspec = nsnull;
    nsCOMPtr<nsIDeviceContext> dx;
    nsIDeviceContext *newdx = nsnull;

    factory->CreateDeviceContextSpec(nsnull, devspec, PR_FALSE);

    if (nsnull != devspec) {
      mPresContext->GetDeviceContext(getter_AddRefs(dx));

      if (NS_OK == dx->GetDeviceContextFor(devspec, newdx))
      {
        nsIPresShell *ps;
        nsIPresContext *cx;
        nsIStyleSet *ss;
        nsIPref *prefs;
        nsIViewManager *vm;
        PRInt32 width, height;
        nsIView *view;

        NS_RELEASE(devspec);

        newdx->BeginDocument();
        newdx->GetDeviceSurfaceDimensions(width, height);

        NS_NewPrintContext(&cx);
        mPresContext->GetPrefs(&prefs);
        cx->Init(newdx, prefs);

        CreateStyleSet(mDocument, &ss);

        NS_NewPresShell(&ps);

        {
          nsresult rv;

          rv = nsComponentManager::CreateInstance(kViewManagerCID, 
                                            nsnull, 
                                            kIViewManagerIID, 
                                            (void **)&vm);

          if ((NS_OK != rv) || (NS_OK != vm->Init(newdx))) {
            NS_ASSERTION(PR_FALSE, "can't get good VM");
          }

          nsRect tbounds = nsRect(0, 0, width, height);

          // Create a child window of the parent that is our "root view/window"
          // Create a view
          rv = nsComponentManager::CreateInstance(kViewCID, 
                                            nsnull, 
                                            kIViewIID, 
                                            (void **)&view);
          if ((NS_OK != rv) || (NS_OK != view->Init(vm, 
                                                    tbounds,
                                                    nsnull))) {
            NS_ASSERTION(PR_FALSE, "can't get good view");
          }

          // Setup hierarchical relationship in view manager
          vm->SetRootView(view);
        }

        ps->Init(mDocument, cx, vm, ss);

        //lay it out...
        //newdx->BeginDocument();
        ps->InitialReflow(width, height);

        // Ask the page sequence frame to print all the pages
        nsIPageSequenceFrame* pageSequence;
        nsPrintOptions        options;

        ps->GetPageSequenceFrame(&pageSequence);
        NS_ASSERTION(nsnull != pageSequence, "no page sequence frame");
        pageSequence->Print(*cx, options, nsnull);
        newdx->EndDocument();

        NS_RELEASE(ps);
        NS_RELEASE(vm);
        NS_RELEASE(ss);
        NS_RELEASE(newdx);
        NS_IF_RELEASE(prefs); // XXX why is the prefs null??
      }

    }
    NS_RELEASE(factory);
  }

  return NS_OK;
}

void DocumentViewerImpl::ForceRefresh()
{
    mWindow->Invalidate(PR_TRUE);
}

nsresult
DocumentViewerImpl::CreateStyleSet(nsIDocument* aDocument,
                                   nsIStyleSet** aStyleSet)
{
    // this should eventually get expanded to allow for creating
    // different sets for different media
    nsresult rv;

    if (!mUAStyleSheet) {
        NS_WARNING("unable to load UA style sheet");
    }

    rv = NS_NewStyleSet(aStyleSet);
    if (NS_OK == rv) {
        PRInt32 index = aDocument->GetNumberOfStyleSheets();

        while (0 < index--) {
            nsIStyleSheet* sheet = aDocument->GetStyleSheetAt(index);
            (*aStyleSet)->AddDocStyleSheet(sheet, aDocument);
            NS_RELEASE(sheet);
        }
        if (mUAStyleSheet) {
            (*aStyleSet)->AppendBackstopStyleSheet(mUAStyleSheet);
        }
    }
    return rv;
}


nsresult DocumentViewerImpl::MakeWindow(nsNativeWidget aNativeParent,
                                       const nsRect& aBounds,
                                       nsScrollPreference aScrolling)
{
    nsresult rv;

    rv = nsComponentManager::CreateInstance(kViewManagerCID, 
                                      nsnull, 
                                      kIViewManagerIID, 
                                      getter_AddRefs(mViewManager));

    nsCOMPtr<nsIDeviceContext> dx;
    mPresContext->GetDeviceContext(getter_AddRefs(dx));

    if ((NS_OK != rv) || (NS_OK != mViewManager->Init(dx))) {
      return rv;
    }

    nsRect tbounds = aBounds;
    float p2t;
    mPresContext->GetPixelsToTwips(&p2t);
    tbounds *= p2t;

    // Create a child window of the parent that is our "root view/window"
    // Create a view
    rv = nsComponentManager::CreateInstance(kViewCID, 
                                      nsnull, 
                                      kIViewIID, 
                                      (void**)&mView);
    static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
    if ((NS_OK != rv) || (NS_OK != mView->Init(mViewManager, 
                                               tbounds,
                                               nsnull))) {
        return rv;
    }

    rv = mView->CreateWidget(kWidgetCID, nsnull, aNativeParent);

    if (rv != NS_OK)
      return rv;

    // Setup hierarchical relationship in view manager
    mViewManager->SetRootView(mView);

    mView->GetWidget(*getter_AddRefs(mWindow));

    //set frame rate to 25 fps
    mViewManager->SetFrameRate(25);

    // This SetFocus is necessary so the Arrow Key and Page Key events
    // go to the scrolled view as soon as the Window is created instead of going to
    // the browser window (this enables keyboard scrolling of the document)
    mWindow->SetFocus();

    return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::CreateDocumentViewerUsing(nsIPresContext* aPresContext,
                                              nsIDocumentViewer*& aResult)
{
    if (!mDocument) {
        // XXX better error
        return NS_ERROR_NULL_POINTER;
    }
    if (nsnull == aPresContext) {
        return NS_ERROR_NULL_POINTER;
    }

    // Create new viewer
    DocumentViewerImpl* viewer = new DocumentViewerImpl(aPresContext);
    if (nsnull == viewer) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(viewer);

    // XXX make sure the ua style sheet is used (for now; need to be
    // able to specify an alternate)
    viewer->SetUAStyleSheet(mUAStyleSheet);

    // Bind the new viewer to the old document
    nsresult rv = viewer->BindToDocument(mDocument, "create");/* XXX verb? */

    aResult = viewer;

    return rv;
}

NS_WEB nsresult NS_NewDocumentViewer(nsIDocumentViewer*& aViewer)
{
    aViewer = new DocumentViewerImpl();
    if (nsnull == aViewer) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(aViewer);
    return NS_OK;
}
