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
 *   Dan Rosen <dr@netscape.com>
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
#include "nscore.h"
#include "nsCRT.h"
#include "nsIContentViewer.h"
#include "nsIPluginHost.h"
#include "nsIPluginInstance.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsILinkHandler.h"
#include "nsIWebShell.h"
#include "nsIContentViewerEdit.h"
#include "nsIContentViewerFile.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDOMDocument.h"
#include "nsPluginViewer.h"
#include "nsGUIEvent.h"
#include "nsIPluginViewer.h"


#include "nsITimer.h"
#include "nsITimerCallback.h"

// Class IDs
static NS_DEFINE_IID(kChildWindowCID, NS_CHILD_CID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

// Interface IDs
static NS_DEFINE_IID(kIContentViewerIID, NS_ICONTENTVIEWER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);


class PluginViewerImpl;

class PluginListener : public nsIStreamListener {
public:
  PluginListener(PluginViewerImpl* aViewer);
  virtual ~PluginListener();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIRequestObserver methods:
  NS_DECL_NSIREQUESTOBSERVER

  // nsIStreamListener methods:
  NS_DECL_NSISTREAMLISTENER

  PluginViewerImpl* mViewer;
  nsIStreamListener* mNextStream;
};
 
  
class pluginInstanceOwner : public nsIPluginInstanceOwner,
                            public nsITimerCallback
{
public:
  pluginInstanceOwner();
  virtual ~pluginInstanceOwner();

  NS_DECL_ISUPPORTS

  //nsIPluginInstanceOwner interface

  NS_IMETHOD SetInstance(nsIPluginInstance *aInstance);

  NS_IMETHOD GetInstance(nsIPluginInstance *&aInstance);

  NS_IMETHOD GetWindow(nsPluginWindow *&aWindow);

  NS_IMETHOD GetMode(nsPluginMode *aMode);

  NS_IMETHOD CreateWidget(void);

  NS_IMETHOD GetURL(const char *aURL, const char *aTarget, 
                    void *aPostData, PRUint32 aPostDataLen, 
                    void *aHeadersData, PRUint32 aHeadersDataLen);

  NS_IMETHOD ShowStatus(const char *aStatusMsg);

  NS_IMETHOD ShowStatus(const PRUnichar *aStatusMsg);

  NS_IMETHOD GetDocument(nsIDocument* *aDocument);

  NS_IMETHOD InvalidateRect(nsPluginRect *invalidRect);

  NS_IMETHOD InvalidateRegion(nsPluginRegion invalidRegion);

  NS_IMETHOD ForceRedraw();

  NS_IMETHOD GetValue(nsPluginInstancePeerVariable variable, void *value);

  //nsIEventListener interface
  nsEventStatus ProcessEvent(const nsGUIEvent & anEvent);

  //locals

  NS_IMETHOD Init(PluginViewerImpl *aViewer, nsIWidget *aWindow);

  // nsITimerCallback interface
  NS_IMETHOD_(void) Notify(nsITimer *timer);
  void CancelTimer();

#ifdef XP_MAC
  void GUItoMacEvent(const nsGUIEvent& anEvent, EventRecord& aMacEvent);
  nsPluginPort* GetPluginPort();
  void FixUpPluginWindow();
#endif
                   
private:
  nsPluginWindow    mPluginWindow;
  nsIPluginInstance *mInstance;
  nsIWidget         *mWindow;       //we do not addref this...
  PluginViewerImpl  *mViewer;       //we do not addref this...
  nsCOMPtr<nsITimer> mPluginTimer;
};

  static void GetWidgetPosAndClip(nsIWidget* aWidget,nscoord& aAbsX, nscoord& aAbsY,
                                nsRect& aClipRect);


class PluginViewerImpl : public nsIPluginViewer,
                         public nsIContentViewer,
                         public nsIContentViewerEdit,
                         public nsIContentViewerFile
{
public:
  PluginViewerImpl(const char* aCommand);
  nsresult Init(nsIStreamListener** aDocListener);
    
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIPluginViewer
  NS_IMETHOD StartLoad(nsIRequest* request, nsIStreamListener*& aResult);

  // nsIContentViewer
  NS_DECL_NSICONTENTVIEWER

  // nsIContentViewerEdit
  NS_DECL_NSICONTENTVIEWEREDIT

  // nsIContentViewerFile
  NS_DECL_NSICONTENTVIEWERFILE

  virtual ~PluginViewerImpl();

  nsresult CreatePlugin(nsIRequest* request, nsIPluginHost* aHost, const nsRect& aBounds,
                        nsIStreamListener*& aResult);

  nsresult MakeWindow(nsNativeWidget aParent,
                      nsIDeviceContext* aDeviceContext,
                      const nsRect& aBounds);

  void ForceRefresh(void);

  nsresult GetURI(nsIURI* *aURI);

  nsresult GetDocument(nsIDocument* *aDocument);

  nsIWidget* mWindow;
  nsIDocument* mDocument;
  nsCOMPtr<nsISupports> mContainer;
  nsIChannel* mChannel;
  pluginInstanceOwner *mOwner;
  PRBool mEnableRendering;

};

//----------------------------------------------------------------------

nsresult
NS_NewPluginContentViewer(const char* aCommand,
                          nsIStreamListener** aDocListener,
                          nsIContentViewer** aDocViewer)
{
  PluginViewerImpl* it = new PluginViewerImpl(aCommand);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = it->Init(aDocListener);
  if (NS_FAILED(rv)) {
    delete it;
    return rv;
  }
  return it->QueryInterface(kIContentViewerIID, (void**) aDocViewer);
}

// Note: operator new zeros our memory
PluginViewerImpl::PluginViewerImpl(const char* aCommand)
{
  NS_INIT_REFCNT();
  mEnableRendering = PR_TRUE;
}

nsresult
PluginViewerImpl::Init(nsIStreamListener** aDocListener)
{
  nsIStreamListener* it = new PluginListener(this);
  if (it == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(it);
  *aDocListener = it;
  return NS_OK;
}

// ISupports implementation...
NS_IMPL_ADDREF(PluginViewerImpl)
NS_IMPL_RELEASE(PluginViewerImpl)
NS_IMPL_QUERY_INTERFACE4(PluginViewerImpl,
                         nsIPluginViewer,
                         nsIContentViewer,
                         nsIContentViewerEdit,
                         nsIContentViewerFile)


PluginViewerImpl::~PluginViewerImpl()
{
#ifdef XP_MAC
  if (mOwner) mOwner->CancelTimer();
#endif

  if(mOwner) {
    nsIPluginInstance * inst;

    if(NS_SUCCEEDED(mOwner->GetInstance(inst)) && inst) {
      nsCOMPtr<nsIPluginHost> host = do_GetService(kCPluginManagerCID);
      if(host)
        host->StopPluginInstance(inst);
      
      NS_RELEASE(inst);
    }
  }

  NS_IF_RELEASE(mOwner);
  if (nsnull != mWindow) {
    mWindow->Destroy();
    NS_RELEASE(mWindow);
  }
  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mChannel);
}

NS_IMETHODIMP
PluginViewerImpl::SetContainer(nsISupports* aContainer)
{
  mContainer = aContainer;
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::GetContainer(nsISupports** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);

   *aResult = mContainer;
   NS_IF_ADDREF(*aResult);

   return NS_OK;
}


NS_IMETHODIMP
PluginViewerImpl::Init(nsIWidget* aParentWidget,
                       nsIDeviceContext* aDeviceContext,
                       const nsRect& aBounds)
{
  nsresult rv = MakeWindow(aParentWidget->GetNativeData(NS_NATIVE_WIDGET),
   aDeviceContext, aBounds);
  if (NS_OK == rv) {
    mOwner = new pluginInstanceOwner();
    if (nsnull != mOwner) {
      NS_ADDREF(mOwner);
      rv = mOwner->Init(this, mWindow);
    }
  }
  return rv;
}

NS_IMETHODIMP
PluginViewerImpl::StartLoad(nsIRequest* request, nsIStreamListener*& aResult)
{
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (!channel) return NS_ERROR_FAILURE;
  
  NS_IF_RELEASE(mChannel);
  mChannel = channel;
  NS_ADDREF(mChannel);

#ifdef DEBUG
  char* contentType;
  channel->GetContentType(&contentType);
  printf("PluginViewerImpl::StartLoad: content-type=%s\n", contentType);
  nsCRT::free(contentType);
#endif

  aResult = nsnull;

  // Only instantiate the plugin if our container can host it
  nsCOMPtr<nsIPluginHost> host;

  host = do_GetService(kCPluginManagerCID);
  nsresult rv = NS_ERROR_FAILURE;
  if(host) 
  {
    nsRect r;
    mWindow->GetClientBounds(r);
    rv = CreatePlugin(request, host, nsRect(0, 0, r.width, r.height), aResult);
  }

  return rv;
}

nsresult
PluginViewerImpl::CreatePlugin(nsIRequest* request, nsIPluginHost* aHost, const nsRect& aBounds,
                               nsIStreamListener*& aResult)
{
  nsresult rv = NS_OK;

  if (nsnull != mOwner) {
    nsPluginWindow  *win;

    mOwner->GetWindow(win);

    win->x = aBounds.x;
    win->y = aBounds.y;
    win->width = aBounds.width;
    win->height = aBounds.height;
    win->clipRect.top = aBounds.y;
    win->clipRect.left = aBounds.x;
    win->clipRect.bottom = aBounds.YMost();
    win->clipRect.right = aBounds.XMost();
  #ifdef XP_UNIX
    win->ws_info = nsnull;   //XXX need to figure out what this is. MMP
  #endif

    nsIURI* uri;
    rv = mChannel->GetURI(&uri);
    if (NS_FAILED(rv)) return rv;

    char* spec;
    rv = uri->GetSpec(&spec);
    NS_RELEASE(uri);
    if (NS_FAILED(rv)) return rv;
    nsAutoString str; str.AssignWithConversion(spec);
    nsCRT::free(spec);

    char* ct;
  
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    channel->GetContentType(&ct);
    if (NS_FAILED(rv)) return rv;
    rv = aHost->InstantiateFullPagePlugin(ct, str, aResult, mOwner);
   
    delete[] ct;
  }

  return rv;
}

NS_IMETHODIMP
PluginViewerImpl::Stop(void)
{
  // XXX write this
  return NS_OK;
}

/*
 * This method is called by the Document Loader once a document has
 * been created for a particular data stream...  The content viewer
 * must cache this document for later use when Init(...) is called.
 */
NS_IMETHODIMP
PluginViewerImpl::LoadStart(nsISupports *aDoc)
{
#ifdef NS_DEBUG
  printf("PluginViewerImpl::LoadStart\n");
#endif
  return aDoc->QueryInterface(kIDocumentIID, (void**)&mDocument);
}


NS_IMETHODIMP
PluginViewerImpl::LoadComplete(nsresult aStatus)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PluginViewerImpl::Unload(void)
{
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::Close(void)
{
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::Destroy(void)
{
  // XXX ripped off from nsObjectFrame::Destroy()
  
  // we need to finish with the plugin before native window is destroyed
  // doing this in the destructor is too late.
  if(mOwner != nsnull)
  {
    nsIPluginInstance *inst;
    if(NS_OK == mOwner->GetInstance(inst))
    {
      PRBool doCache = PR_TRUE;
      PRBool doCallSetWindowAfterDestroy = PR_FALSE;

      // first, determine if the plugin wants to be cached
      inst->GetValue(nsPluginInstanceVariable_DoCacheBool, 
                     (void *) &doCache);
      if (!doCache) {
        // then determine if the plugin wants Destroy to be called after
        // Set Window.  This is for bug 50547.
        inst->GetValue(nsPluginInstanceVariable_CallSetWindowAfterDestroyBool, 
                       (void *) &doCallSetWindowAfterDestroy);
        !doCallSetWindowAfterDestroy ? inst->SetWindow(nsnull) : 0;
        inst->Stop();
        inst->Destroy();
        doCallSetWindowAfterDestroy ? inst->SetWindow(nsnull) : 0;      }
      else {
        inst->SetWindow(nsnull);
        inst->Stop();
      }
      NS_RELEASE(inst);
    }
  }

	
	return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::GetDOMDocument(nsIDOMDocument **aResult)
{
  return (mDocument) ? CallQueryInterface(mDocument, aResult) : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PluginViewerImpl::SetDOMDocument(nsIDOMDocument *aDocument)
{
  return NS_ERROR_FAILURE;
}

nsEventStatus PR_CALLBACK
HandlePluginEvent(nsGUIEvent *aEvent)
{
  if (aEvent == nsnull || aEvent->widget == nsnull)   //null pointer check
    return nsEventStatus_eIgnore;

#ifdef XP_WIN
  // on Windows, the mouse click is converted to an NS_PLUGIN_ACTIVATE
  if( aEvent->message == NS_PLUGIN_ACTIVATE)  
    (nsIWidget*)(aEvent->widget)->SetFocus();  // send focus to child window
#else
  // the Mac, and presumably others, send NS_MOUSE_ACTIVATE
  if (aEvent->message == NS_MOUSE_ACTIVATE) {
    (nsIWidget*)(aEvent->widget)->SetFocus();  // send focus to child window
#ifdef XP_MAC
  // furthermore on the Mac nsMacEventHandler sends the NS_PLUGIN_ACTIVATE
  // followed by the mouse down event, so we need to handle this
  } else {
    // on Mac, we store a pointer to this class as native data in the widget
    PluginViewerImpl * pluginViewer;
    (nsIWidget*)(aEvent->widget)->GetClientData((PluginViewerImpl *)pluginViewer);
    if (pluginViewer != nsnull && pluginViewer->mOwner != nsnull)
      return pluginViewer->mOwner->ProcessEvent(*aEvent);
#endif // XP_MAC
  }
#endif // else XP_WIN
  return nsEventStatus_eIgnore;
}


nsresult
PluginViewerImpl::MakeWindow(nsNativeWidget aParent,
                             nsIDeviceContext* aDeviceContext,
                             const nsRect& aBounds)
{
  nsresult rv =
    nsComponentManager::CreateInstance(kChildWindowCID, nsnull, kIWidgetIID,
                                 (void**)&mWindow);
  if (NS_OK != rv) {
    return rv;
  }
  
 
  mWindow->Create(aParent, aBounds,HandlePluginEvent, aDeviceContext);
  mWindow->SetClientData(this);
  Show();
  return rv;
}

NS_IMETHODIMP
PluginViewerImpl::GetBounds(nsRect& aResult)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->GetBounds(aResult);
  }
  else {
    aResult.SetRect(0, 0, 0, 0);
  }
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::GetPreviousViewer(nsIContentViewer** aViewer)
{
  *aViewer = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::SetPreviousViewer(nsIContentViewer* aViewer)
{
  if (aViewer)
    aViewer->Destroy();
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::SetBounds(const nsRect& aBounds)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    nsIPluginInstance *inst;
    mWindow->Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height, PR_FALSE);
    if ((nsnull != mOwner) && (NS_OK == mOwner->GetInstance(inst)) && (nsnull != inst)) {
      nsPluginWindow  *win;
      if (NS_OK == mOwner->GetWindow(win)) {
        win->x = aBounds.x;
        win->y = aBounds.y;
        win->width = aBounds.width;
        win->height = aBounds.height;
        win->clipRect.top = aBounds.y;
        win->clipRect.left = aBounds.x;
        win->clipRect.bottom = aBounds.YMost();
        win->clipRect.right = aBounds.XMost();

        inst->SetWindow(win);
      }
      NS_RELEASE(inst);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::Move(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    nsIPluginInstance *inst;
    mWindow->Move(aX, aY);
    if ((nsnull != mOwner) && (NS_OK == mOwner->GetInstance(inst)) && (nsnull != inst)) {
      nsPluginWindow  *win;
      if (NS_OK == mOwner->GetWindow(win)) {
        win->x = aX;
        win->y = aY;
        win->clipRect.bottom = (win->clipRect.bottom - win->clipRect.top) + aY;
        win->clipRect.right = (win->clipRect.right - win->clipRect.left) + aX;
        win->clipRect.top = aY;
        win->clipRect.left = aX;

        inst->SetWindow(win);
      }
      NS_RELEASE(inst);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::Show()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Show(PR_TRUE);
  }

  // XXX should we call SetWindow here?

  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::Hide()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    mWindow->Show(PR_FALSE);
  }

  // should we call SetWindow(nsnull) here?

  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::SetEnableRendering(PRBool aOn)
{
  mEnableRendering = aOn;
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::GetEnableRendering(PRBool* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null OUT ptr");
  if (aResult) {
    *aResult = mEnableRendering;
  }
  return NS_OK;
}

void
PluginViewerImpl::ForceRefresh()
{
  mWindow->Invalidate(PR_TRUE);
}

nsresult PluginViewerImpl::GetURI(nsIURI* *aURI)
{
  return mChannel->GetURI(aURI);
}

nsresult PluginViewerImpl::GetDocument(nsIDocument* *aDocument)
{
  NS_IF_ADDREF(mDocument);
  *aDocument = mDocument;
  return NS_OK;
}

NS_IMETHODIMP pluginInstanceOwner::InvalidateRect(nsPluginRect *invalidRect)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP pluginInstanceOwner::InvalidateRegion(nsPluginRegion invalidRegion)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP pluginInstanceOwner::ForceRedraw()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP pluginInstanceOwner::GetValue(nsPluginInstancePeerVariable variable, void *value)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
/* ========================================================================================
 * nsIContentViewerFile
 * ======================================================================================== */

NS_IMETHODIMP PluginViewerImpl::Search()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PluginViewerImpl::GetSearchable(PRBool *aSearchable)
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PluginViewerImpl::ClearSelection()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PluginViewerImpl::SelectAll()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PluginViewerImpl::CopySelection()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PluginViewerImpl::GetCopyable(PRBool *aCopyable)
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PluginViewerImpl::CutSelection()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PluginViewerImpl::GetCutable(PRBool *aCutable)
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PluginViewerImpl::Paste()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PluginViewerImpl::GetPasteable(PRBool *aPasteable)
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* ========================================================================================
 * nsIContentViewerEdit
 * ======================================================================================== */
NS_IMETHODIMP
PluginViewerImpl::Save()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PluginViewerImpl::GetSaveable(PRBool *aSaveable)
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PluginViewerImpl::Print(PRBool aSilent,FILE *aFile, nsIPrintListener *aPrintListener)
{
  nsPluginPrint npprint;
  npprint.mode = nsPluginMode_Full;
  npprint.print.fullPrint.pluginPrinted = PR_FALSE;
  npprint.print.fullPrint.printOne = PR_FALSE;
  npprint.print.fullPrint.platformPrint = nsnull;

  NS_ENSURE_TRUE(mOwner,NS_ERROR_FAILURE);
  nsCOMPtr<nsIPluginInstance> pi;
  mOwner->GetInstance(*getter_AddRefs(pi));
  NS_ENSURE_TRUE(pi,NS_ERROR_FAILURE);

  return pi->Print(&npprint);

}


NS_IMETHODIMP
PluginViewerImpl::GetPrintable(PRBool *aPrintable) 
{
  NS_ENSURE_ARG_POINTER(aPrintable);

  *aPrintable = PR_FALSE;   // XXX: hey, plug in guys!  implement me!
  return NS_OK;
}


NS_IMETHODIMP
PluginViewerImpl::PrintContent(nsIWebShell *      aParent,
                               nsIDeviceContext * aDContext,
                               nsIDOMWindow     * aDOMWin,
                               PRBool             aIsSubDoc)
{
  NS_ENSURE_ARG_POINTER(aParent);
  NS_ENSURE_ARG_POINTER(aDContext);

  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::GetInLink(PRBool* aInLink)
{
  NS_ENSURE_ARG_POINTER(aInLink);
  *aInLink = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::GetInImage(PRBool* aInImage)
{
  NS_ENSURE_ARG_POINTER(aInImage);
  *aInImage = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::CopyLinkLocation()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PluginViewerImpl::CopyImageLocation()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PluginViewerImpl::CopyImageContents()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


//----------------------------------------------------------------------

PluginListener::PluginListener(PluginViewerImpl* aViewer)
{
  NS_INIT_REFCNT();
  mViewer = aViewer;
  NS_ADDREF(aViewer);
}

PluginListener::~PluginListener()
{
  NS_RELEASE(mViewer);
  NS_IF_RELEASE(mNextStream);
}

NS_IMPL_ISUPPORTS1(PluginListener, nsIStreamListener)

NS_IMETHODIMP
PluginListener::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  nsresult rv;
  char* contentType = nsnull;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  rv = channel->GetContentType(&contentType);

  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = mViewer->StartLoad(request, mNextStream);

  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nsnull == mNextStream) 
    return NS_ERROR_FAILURE;
  return mNextStream->OnStartRequest(request, ctxt);
}

NS_IMETHODIMP
PluginListener::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                              nsresult status)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnStopRequest(request, ctxt, status);
}

NS_IMETHODIMP
PluginListener::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
                                nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnDataAvailable(request, ctxt, inStr, sourceOffset, count);
}

//----------------------------------------------------------------------

pluginInstanceOwner :: pluginInstanceOwner()
{
  NS_INIT_REFCNT();

  memset(&mPluginWindow, 0, sizeof(mPluginWindow));
  mInstance = nsnull;
  mWindow = nsnull;
  mViewer = nsnull;
}

pluginInstanceOwner :: ~pluginInstanceOwner()
{
  // shut off the timer.
  if (mPluginTimer != nsnull) {
    CancelTimer();
  }

  NS_IF_RELEASE(mInstance);

  mWindow = nsnull;
  mViewer = nsnull;
}

NS_IMPL_ISUPPORTS2(pluginInstanceOwner, nsIPluginInstanceOwner,nsITimerCallback);

NS_IMETHODIMP pluginInstanceOwner :: SetInstance(nsIPluginInstance *aInstance)
{
  NS_IF_RELEASE(mInstance);
  mInstance = aInstance;
  NS_IF_ADDREF(mInstance);

  return NS_OK;
}

NS_IMETHODIMP pluginInstanceOwner :: GetInstance(nsIPluginInstance *&aInstance)
{
  if(!mInstance)
    return NS_ERROR_FAILURE;

  NS_ADDREF(mInstance);
  aInstance = mInstance;

  return NS_OK;
}

NS_IMETHODIMP pluginInstanceOwner :: GetWindow(nsPluginWindow *&aWindow)
{
  aWindow = &mPluginWindow;
  return NS_OK;
}

NS_IMETHODIMP pluginInstanceOwner :: GetMode(nsPluginMode *aMode)
{
  *aMode = nsPluginMode_Full;
  return NS_OK;
}

NS_IMETHODIMP pluginInstanceOwner :: CreateWidget(void)
{
  PRBool    windowless;
  nsresult  rv = NS_OK;
  
  if (nsnull != mInstance)
  {
#if defined(XP_MAC)
          // start a periodic timer to provide null events to the plugin instance.
          mPluginTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
          if (rv == NS_OK)
            rv = mPluginTimer->Init(this, 1020 / 60, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
#endif


    mInstance->GetValue(nsPluginInstanceVariable_WindowlessBool, (void *)&windowless);

    if (PR_TRUE == windowless)
    {
      mPluginWindow.window = nsnull;    //XXX this needs to be a HDC
      mPluginWindow.type = nsPluginWindowType_Drawable;
    }
    else if (nsnull != mWindow)
    {
      mPluginWindow.window = (nsPluginPort *)mWindow->GetNativeData(NS_NATIVE_PLUGIN_PORT);
      mPluginWindow.type = nsPluginWindowType_Window;
    }
    else
      return NS_ERROR_FAILURE;
  }
  else
    return NS_ERROR_FAILURE;

#if defined(XP_MAC)
  FixUpPluginWindow();
#endif

  return rv;
}

NS_IMETHODIMP pluginInstanceOwner :: GetURL(const char *aURL, const char *aTarget, void *aPostData, PRUint32 aPostDataLen, void *aHeadersData, 
                                            PRUint32 aHeadersDataLen)
{
  NS_ENSURE_TRUE(mViewer,NS_ERROR_NULL_POINTER);

  // the container of the pres context will give us the link handler
  nsCOMPtr<nsISupports> container;
  nsresult rv = mViewer->GetContainer(getter_AddRefs(container));
  NS_ENSURE_TRUE(container,NS_ERROR_FAILURE);
  nsCOMPtr<nsILinkHandler> lh = do_QueryInterface(container);
  NS_ENSURE_TRUE(lh, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> uri;
  rv = mViewer->GetURI(getter_AddRefs(uri));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv),NS_ERROR_FAILURE);

  char* absURIStr;
  NS_MakeAbsoluteURI(&absURIStr, aURL, uri);
  nsAutoString fullurl; fullurl.AssignWithConversion(absURIStr);
  nsCRT::free(absURIStr);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv),NS_ERROR_FAILURE);

  nsCOMPtr<nsISupports> result;
  nsCOMPtr<nsIInputStream> postDataStream;
  nsCOMPtr<nsIInputStream> headersDataStream;
  if (aPostData) {
    if (PL_strncasecmp((const char*)aPostData, "file:///", 8) == 0) {
      const char *fileURL = (const char*)aPostData + 8 ;
      rv = NS_NewPostDataStream(getter_AddRefs(postDataStream), PR_TRUE, fileURL, 0);
    } else
    rv = NS_NewPostDataStream(getter_AddRefs(postDataStream), PR_FALSE, (const char *) aPostData, 0);
  }
  if (aHeadersData) {
    rv = NS_NewByteInputStream(getter_AddRefs(result), (const char *) aHeadersData, aHeadersDataLen);
    if (result) {
      headersDataStream = do_QueryInterface(result, &rv);
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv),"failed in creating plugin post data stream");

  nsAutoString  unitarget; unitarget.AssignWithConversion(aTarget);
  rv = lh->OnLinkClick(nsnull, eLinkVerb_Replace, 
                       fullurl.get(), unitarget.get(), 
                       postDataStream, headersDataStream);

  return rv;
}

NS_IMETHODIMP pluginInstanceOwner :: ShowStatus(const char *aStatusMsg)
{
  nsresult  rv = NS_ERROR_FAILURE;
  
  rv = this->ShowStatus(NS_ConvertUTF8toUCS2(aStatusMsg).get());
  
  return rv;
}

NS_IMETHODIMP pluginInstanceOwner::ShowStatus(const PRUnichar *aStatusMsg)
{
  nsresult  rv = NS_ERROR_FAILURE;
  
  if (!mViewer) {
    return rv;
  }
  nsCOMPtr<nsISupports> cont;
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  
  rv = mViewer->GetContainer(getter_AddRefs(cont));
  if (NS_FAILED(rv) || !cont) {
    return rv;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(cont, &rv));
  if (NS_FAILED(rv) || !docShellItem) {
    return rv;
  }

  rv = docShellItem->GetTreeOwner(getter_AddRefs(treeOwner));
  if (NS_FAILED(rv) || !treeOwner) {
    return rv;
  }

  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner, &rv));
  if (NS_FAILED(rv) || !browserChrome) {
    return rv;
  }
  rv = browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT, 
                                aStatusMsg);
  return rv;
}

NS_IMETHODIMP pluginInstanceOwner :: GetDocument(nsIDocument* *aDocument)
{
	return mViewer->GetDocument(aDocument);
}

NS_IMETHODIMP pluginInstanceOwner :: Init(PluginViewerImpl *aViewer, nsIWidget *aWindow)
{
  //do not addref
  mWindow = aWindow;
  mViewer = aViewer;

  return NS_OK;
}


// Here's where we forward events to plugins.

#ifdef XP_MAC

#if TARGET_CARBON
inline Boolean OSEventAvail(EventMask mask, EventRecord* event) { return EventAvail(mask, event); }
#endif

void pluginInstanceOwner::GUItoMacEvent(const nsGUIEvent& anEvent, EventRecord& aMacEvent)
{
       ::OSEventAvail(0, &aMacEvent);
       switch (anEvent.message) {
       case NS_GOTFOCUS:
       case NS_FOCUS_EVENT_START:
               aMacEvent.what = nsPluginEventType_GetFocusEvent;
               break;
       case NS_LOSTFOCUS:
       case NS_MOUSE_EXIT:
               aMacEvent.what = nsPluginEventType_LoseFocusEvent;
               break;
       case NS_MOUSE_MOVE:
       case NS_MOUSE_ENTER:
         mWindow->SetFocus();
               aMacEvent.what = nsPluginEventType_AdjustCursorEvent;
               break;
       case NS_PAINT:
          aMacEvent.what = updateEvt;
         break;
       case NS_KEY_DOWN:
       case NS_KEY_PRESS:
         break;
         
       default:
               aMacEvent.what = nullEvent;
               break;
       }
}
#endif

nsEventStatus pluginInstanceOwner::ProcessEvent(const nsGUIEvent& anEvent)
{
  nsEventStatus rv = nsEventStatus_eIgnore;
  if (!mInstance || !mWindow)   // if the instance or the window is null, we shouldn't be here
    return rv;

#ifdef XP_MAC
    //if (mWidget != NULL) {  // check for null mWidget
        EventRecord* event = (EventRecord*)anEvent.nativeMsg;
        if (event == NULL || event->what == nullEvent ||
            anEvent.message == NS_KEY_PRESS           ||
            anEvent.message == NS_CONTEXTMENU_MESSAGE_START) {
            EventRecord macEvent;
            GUItoMacEvent(anEvent, macEvent);
            event = &macEvent;
            if (event->what == updateEvt) {
              nsPluginPort* pluginPort = GetPluginPort();
                // Add in child windows absolute position to get make the dirty rect
                // relative to the top-level window.
                nscoord absWidgetX = 0;
                nscoord absWidgetY = 0;
               nsRect widgetClip(0,0,0,0);
               GetWidgetPosAndClip(mWindow,absWidgetX,absWidgetY,widgetClip);
               //mViewer->GetBounds(widgetClip);
               //absWidgetX = widgetClip.x;
               //absWidgetY = widgetClip.y;
               
                // set the port
    mPluginWindow.x = absWidgetX;
    mPluginWindow.y = absWidgetY;


    // fix up the clipping region
    mPluginWindow.clipRect.top = widgetClip.y;
    mPluginWindow.clipRect.left = widgetClip.x;
    mPluginWindow.clipRect.bottom =  mPluginWindow.clipRect.top + widgetClip.height;
    mPluginWindow.clipRect.right =  mPluginWindow.clipRect.left + widgetClip.width;  

    EventRecord updateEvent;
    ::OSEventAvail(0, &updateEvent);
    updateEvent.what = updateEvt;
    updateEvent.message = UInt32(pluginPort->port);

    nsPluginEvent pluginEvent = { &updateEvent, nsPluginPlatformWindowRef(pluginPort->port) };
    PRBool eventHandled = PR_FALSE;
    mInstance->HandleEvent(&pluginEvent, &eventHandled);
     }
            
            return  nsEventStatus_eConsumeNoDefault;
            
        }
        //nsPluginPort* port = (nsPluginPort*)mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
        nsPluginPort* port = (nsPluginPort*)mWindow->GetNativeData(NS_NATIVE_PLUGIN_PORT);
        nsPluginEvent pluginEvent = { event, nsPluginPlatformWindowRef(port->port) };
        PRBool eventHandled = PR_FALSE;
        mInstance->HandleEvent(&pluginEvent, &eventHandled);
        if (eventHandled && anEvent.message != NS_MOUSE_LEFT_BUTTON_DOWN)
            rv = nsEventStatus_eConsumeNoDefault;
   // }
#endif

//~~~
#ifdef XP_WIN
        nsPluginEvent * pPluginEvent = (nsPluginEvent *)anEvent.nativeMsg;
        PRBool eventHandled = PR_FALSE;
        mInstance->HandleEvent(pPluginEvent, &eventHandled);
        if (eventHandled)
            rv = nsEventStatus_eConsumeNoDefault;
#endif

  return rv;
}


// Here's how we give idle time to plugins.

NS_IMETHODIMP_(void) pluginInstanceOwner::Notify(nsITimer* /* timer */)
{
#ifdef XP_MAC
    // validate the plugin clipping information by syncing the plugin window info to
    // reflect the current widget location. This makes sure that everything is updated
    // correctly in the event of scrolling in the window.
    FixUpPluginWindow();
    if (mInstance != NULL) {
        EventRecord idleEvent;
        ::OSEventAvail(0, &idleEvent);
        idleEvent.what = nullEvent;
        
        nsPluginPort* pluginPort = GetPluginPort();
        nsPluginEvent pluginEvent = { &idleEvent, nsPluginPlatformWindowRef(pluginPort->port) };
        
        PRBool eventHandled = PR_FALSE;
        mInstance->HandleEvent(&pluginEvent, &eventHandled);
    }

#ifndef REPEATING_TIMERS
  // reprime the timer? currently have to create a new timer for each call, which is
  // kind of wasteful. need to get periodic timers working on all platforms.
  nsresult rv;
  mPluginTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_SUCCEEDED(rv))
    mPluginTimer->Init(this, 1020 / 60);
#endif  // REPEATING_TIMERS
#endif // XP_MAC
}


void pluginInstanceOwner::CancelTimer()
{
    if (mPluginTimer) {
        mPluginTimer->Cancel();
        mPluginTimer = nsnull;
    }
}


#ifdef XP_MAC
nsPluginPort* pluginInstanceOwner::GetPluginPort()
{

  nsPluginPort* result = NULL;
    if (mWindow != NULL)
      result = (nsPluginPort*) mWindow->GetNativeData(NS_NATIVE_PLUGIN_PORT);
    return result;
}

  // calculate the absolute position and clip for a widget 
  // and use other windows in calculating the clip
static void GetWidgetPosAndClip(nsIWidget* aWidget,nscoord& aAbsX, nscoord& aAbsY,
                                nsRect& aClipRect) 
{
  aWidget->GetBounds(aClipRect); 
  aAbsX = aClipRect.x; 
  aAbsY = aClipRect.y; 
  
  nscoord ancestorX = -aClipRect.x, ancestorY = -aClipRect.y; 
  // Calculate clipping relative to the widget passed in 
  aClipRect.x = 0; 
  aClipRect.y = 0; 

   // Gather up the absolute position of the widget 
   // + clip window 
  nsCOMPtr<nsIWidget> widget = getter_AddRefs(aWidget->GetParent());
  while (widget != nsnull) { 
    nsRect wrect; 
    widget->GetClientBounds(wrect); 
    nscoord wx, wy; 
    wx = wrect.x; 
    wy = wrect.y; 
    wrect.x = ancestorX; 
    wrect.y = ancestorY; 
    aClipRect.IntersectRect(aClipRect, wrect); 
    aAbsX += wx; 
    aAbsY += wy; 
    widget = getter_AddRefs(widget->GetParent());
    if (widget == nsnull) { 
      // Don't include the top-level windows offset 
      // printf("Top level window offset %d %d\n", wx, wy); 
      aAbsX -= wx; 
      aAbsY -= wy; 
    } 
    ancestorX -=wx; 
    ancestorY -=wy; 
  } 

  aClipRect.x += aAbsX; 
  aClipRect.y += aAbsY; 

  //printf("--------------\n"); 
  //printf("Widget clip X %d Y %d rect %d %d %d %d\n", aAbsX, aAbsY,  aClipRect.x,  aClipRect.y, aClipRect.width,  aClipRect.height ); 
  //printf("--------------\n"); 
} 


void pluginInstanceOwner::FixUpPluginWindow()
{
  if (mWindow) {
    nscoord absWidgetX = 0;
    nscoord absWidgetY = 0;
    nsRect widgetClip(0,0,0,0);
    GetWidgetPosAndClip(mWindow,absWidgetX,absWidgetY,widgetClip);

    // set the port coordinates
    mPluginWindow.x = absWidgetX;
    mPluginWindow.y = absWidgetY;

    // fix up the clipping region
    mPluginWindow.clipRect.top = widgetClip.y;
    mPluginWindow.clipRect.left = widgetClip.x;
    mPluginWindow.clipRect.bottom =  mPluginWindow.clipRect.top + widgetClip.height;
    mPluginWindow.clipRect.right =  mPluginWindow.clipRect.left + widgetClip.width; 
  }
}

#endif
