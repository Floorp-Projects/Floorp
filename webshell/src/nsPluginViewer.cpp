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
#include "nscore.h"
#include "nsCRT.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerContainer.h"
#include "nsIPluginHost.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsRepository.h"
#include "nsWidgetsCID.h"

//#include "nsString.h"
//#include "nsISupports.h"
//
//#include "nsIDocument.h"
//#include "nsIPresContext.h"
//#include "nsIPresShell.h"
//#include "nsIStyleSet.h"
//#include "nsIStyleSheet.h"
//
//#include "nsIScriptContextOwner.h"
//#include "nsIScriptGlobalObject.h"
//#include "nsILinkHandler.h"
//#include "nsIDOMDocument.h"
//
//#include "nsViewsCID.h"
//#include "nsIDeviceContext.h"
//#include "nsIViewManager.h"
//#include "nsIView.h"
//

// Class IDs
static NS_DEFINE_IID(kChildWindowCID, NS_CHILD_CID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

// Interface IDs
static NS_DEFINE_IID(kIContentViewerIID, NS_ICONTENT_VIEWER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPluginHostIID, NS_IPLUGINHOST_IID);

class PluginViewerImpl;

class PluginListener : public nsIStreamListener {
public:
  PluginListener(PluginViewerImpl* aViewer);
  ~PluginListener();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIStreamListener
  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
  NS_IMETHOD OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
  NS_IMETHOD OnStatus(nsIURL* aURL, const nsString &aMsg);
  NS_IMETHOD OnStopBinding(nsIURL* aURL, PRInt32 aStatus,
                           const nsString& aMsg);
  NS_IMETHOD GetBindInfo(nsIURL* aURL);
  NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream* aStream,
                             PRInt32 aCount);

  PluginViewerImpl* mViewer;
  nsIStreamListener* mNextStream;
};

class PluginViewerImpl : public nsIContentViewer
{
public:
  PluginViewerImpl(const char* aCommand, nsIStreamListener** aDocListener);
    
  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentViewer
  NS_IMETHOD Init(nsNativeWidget aParent,
                  nsIDeviceContext* aDeviceContext,
                  nsIPref* aPrefs,
                  const nsRect& aBounds,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto);
    
  NS_IMETHOD BindToDocument(nsISupports* aDoc, const char* aCommand);
  NS_IMETHOD SetContainer(nsIContentViewerContainer* aContainer);
  NS_IMETHOD GetContainer(nsIContentViewerContainer*& aContainerResult);

  virtual nsRect GetBounds();
  virtual void SetBounds(const nsRect& aBounds);
  virtual void Move(PRInt32 aX, PRInt32 aY);
  virtual void Show();
  virtual void Hide();

  ~PluginViewerImpl();

  nsresult CreatePlugin(nsIPluginHost* aHost, const nsRect& aBounds);

  nsresult MakeWindow(nsNativeWidget aParent,
                      nsIDeviceContext* aDeviceContext,
                      const nsRect& aBounds);

  nsresult StartLoad(nsIURL* aURL, const char* aContentType,
                     nsIStreamListener*& aResult);

  void ForceRefresh(void);

  nsIWidget* mWindow;
  nsIContentViewerContainer* mContainer;
  nsPluginWindow mPluginWindow;
  nsIPluginInstance *mInstance;
  nsIURL* mURL;
  nsString mContentType;
};

//----------------------------------------------------------------------

nsresult
NS_NewPluginContentViewer(const char* aCommand,
                          nsIStreamListener** aDocListener,
                          nsIContentViewer** aDocViewer)
{
  PluginViewerImpl* it = new PluginViewerImpl(aCommand, aDocListener);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIContentViewerIID, (void**) aDocViewer);
}

// Note: operator new zeros our memory
PluginViewerImpl::PluginViewerImpl(const char* aCommand,
                                   nsIStreamListener** aDocListener)
{
  NS_INIT_REFCNT();
  nsIStreamListener* it = new PluginListener(this);
  *aDocListener = it;
}

// ISupports implementation...
NS_IMPL_ADDREF(PluginViewerImpl)
NS_IMPL_RELEASE(PluginViewerImpl)

nsresult
PluginViewerImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kIContentViewerIID)) {
    nsIContentViewer* tmp = this;
    *aInstancePtr = (void*)tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsISupports* tmp = this;
    *aInstancePtr = (void*)tmp;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

PluginViewerImpl::~PluginViewerImpl()
{
  NS_IF_RELEASE(mContainer);
}

/*
 * This method is called by the Document Loader once a document has
 * been created for a particular data stream...  The content viewer
 * must cache this document for later use when Init(...) is called.
 */
NS_IMETHODIMP
PluginViewerImpl::BindToDocument(nsISupports *aDoc, const char *aCommand)
{
#ifdef NS_DEBUG
  printf("PluginViewerImpl::BindToDocument\n");
#endif
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::SetContainer(nsIContentViewerContainer* aContainer)
{
  NS_IF_RELEASE(mContainer);
  mContainer = aContainer;
  NS_IF_ADDREF(mContainer);
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::GetContainer(nsIContentViewerContainer*& aResult)
{
  aResult = mContainer;
  NS_IF_ADDREF(mContainer);
  return NS_OK;
}


NS_IMETHODIMP
PluginViewerImpl::Init(nsNativeWidget aNativeParent,
                       nsIDeviceContext* aDeviceContext,
                       nsIPref* aPrefs,
                       const nsRect& aBounds,
                       nsScrollPreference aScrolling)
{
  nsresult rv = MakeWindow(aNativeParent, aDeviceContext, aBounds);
  return rv;
}

nsresult
PluginViewerImpl::StartLoad(nsIURL* aURL, const char* aContentType,
                            nsIStreamListener*& aResult)
{
  printf("PluginViewerImpl::StartLoad: content-type=%s\n", aContentType);

  NS_IF_RELEASE(mURL);
  mURL = aURL;
  NS_IF_ADDREF(aURL);
  mContentType = aContentType;

  aResult = nsnull;

  // Only instantiate the plugin if our container can host it
  nsIPluginHost* pm;
  nsresult rv = mContainer->QueryCapability(kIPluginHostIID, (void **)&pm);
  if (NS_OK == rv) {
    nsRect r;
    mWindow->GetBounds(r);
    rv = CreatePlugin(pm, nsRect(0, 0, r.width, r.height));
    NS_RELEASE(pm);
  }

  return rv;
}

nsresult
PluginViewerImpl::CreatePlugin(nsIPluginHost* aHost, const nsRect& aBounds)
{
  nsresult rv = NS_OK;

  mPluginWindow.window = (nsPluginPort *)
    mWindow->GetNativeData(NS_NATIVE_WINDOW);
  mPluginWindow.x = aBounds.x;
  mPluginWindow.y = aBounds.y;
  mPluginWindow.width = aBounds.width;
  mPluginWindow.height = aBounds.height;
  mPluginWindow.clipRect.top = aBounds.y;
  mPluginWindow.clipRect.left = aBounds.x;
  mPluginWindow.clipRect.bottom = aBounds.YMost();
  mPluginWindow.clipRect.right = aBounds.XMost();
#ifdef XP_UNIX
  mPluginWindow.ws_info = nsnull;   //XXX need to figure out what this is. MMP
#endif
  //this will change with support for windowless plugins... MMP
  mPluginWindow.type = nsPluginWindowType_Window;

  nsAutoString fullurl;
  mURL->ToString(fullurl);
  char* ct = mContentType.ToNewCString();
  rv = aHost->InstantiatePlugin(ct, &mInstance, &mPluginWindow, fullurl);
  delete ct;

  return rv;
}

static nsEventStatus PR_CALLBACK
HandlePluginEvent(nsGUIEvent *aEvent)
{
  return nsEventStatus_eIgnore;
}

nsresult
PluginViewerImpl::MakeWindow(nsNativeWidget aParent,
                             nsIDeviceContext* aDeviceContext,
                             const nsRect& aBounds)
{
  nsresult rv =
    nsRepository::CreateInstance(kChildWindowCID, nsnull, kIWidgetIID,
                                 (void**)&mWindow);
  if (NS_OK != rv) {
    return rv;
  }
  mWindow->Create(aParent, aBounds, HandlePluginEvent, aDeviceContext);
  return rv;
}

nsRect
PluginViewerImpl::GetBounds()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  nsRect zr(0, 0, 0, 0);
  if (nsnull != mWindow) {
    mWindow->GetBounds(zr);
  }
  return zr;
}

void
PluginViewerImpl::SetBounds(const nsRect& aBounds)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    mWindow->Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height, PR_FALSE);
  }
}

void
PluginViewerImpl::Move(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Move(aX, aY);
  }
}

void
PluginViewerImpl::Show()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Show(PR_TRUE);
  }
}

void
PluginViewerImpl::Hide()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Show(PR_FALSE);
  }
}

void
PluginViewerImpl::ForceRefresh()
{
  mWindow->Invalidate(PR_TRUE);
}

//----------------------------------------------------------------------

PluginListener::PluginListener(PluginViewerImpl* aViewer)
{
  mViewer = aViewer;
  NS_ADDREF(aViewer);
  mRefCnt = 1;
}

PluginListener::~PluginListener()
{
  NS_RELEASE(mViewer);
  NS_IF_RELEASE(mNextStream);
}

NS_IMPL_ISUPPORTS(PluginListener, kIStreamListenerIID)

NS_IMETHODIMP
PluginListener::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  mViewer->StartLoad(aURL, aContentType, mNextStream);
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnStartBinding(aURL, aContentType);
}

NS_IMETHODIMP
PluginListener::OnProgress(nsIURL* aURL, PRInt32 aProgress,
                           PRInt32 aProgressMax)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnProgress(aURL, aProgress, aProgressMax);
}

NS_IMETHODIMP
PluginListener::OnStatus(nsIURL* aURL, const nsString &aMsg)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnStatus(aURL, aMsg);
}

NS_IMETHODIMP
PluginListener::OnStopBinding(nsIURL* aURL, PRInt32 aStatus,
                              const nsString& aMsg)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnStopBinding(aURL, aStatus, aMsg);
}

NS_IMETHODIMP
PluginListener::GetBindInfo(nsIURL* aURL)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->GetBindInfo(aURL);
}

NS_IMETHODIMP
PluginListener::OnDataAvailable(nsIURL* aURL, nsIInputStream* aStream,
                                PRInt32 aCount)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnDataAvailable(aURL, aStream, aCount);
}
