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
#include "nsIWebBrowserPrint.h"
#include "nsIWidget.h"
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
#include "nsContentCID.h"
#include "nsIScriptGlobalObject.h"
#include "nsITimer.h"
#include "nsPIPluginHost.h"
#include "nsPluginNativeWindow.h"

class nsIPrintSettings;
class nsIDOMWindow;

// Class IDs
static NS_DEFINE_IID(kChildWindowCID, NS_CHILD_CID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

// Interface IDs
static NS_DEFINE_IID(kIContentViewerIID, NS_ICONTENTVIEWER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kHTMLDocumentCID, NS_HTMLDOCUMENT_CID);

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
                    void *aHeadersData, PRUint32 aHeadersDataLen, 
                    PRBool isFile = PR_FALSE);

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
  NS_DECL_NSITIMERCALLBACK

  void CancelTimer();
  void StartTimer();

  nsPluginPort* GetPluginPort();

#if defined(XP_MAC) || defined(XP_MACOSX)
  void GUItoMacEvent(const nsGUIEvent& anEvent, EventRecord& aMacEvent);
  nsPluginPort* FixUpPluginWindow();
#endif
                   
private:
  nsPluginNativeWindow *mPluginWindow;
  nsIPluginInstance *mInstance;
  nsIWidget         *mWindow;       //we do not addref this...
  PluginViewerImpl  *mViewer;       //we do not addref this...
  nsCOMPtr<nsITimer> mPluginTimer;
  PRPackedBool       mWidgetVisible;    // used on Mac to store our widget's visible state
};

#if defined(XP_MAC) || defined(XP_MACOSX)
  static void GetWidgetPosClipAndVis(nsIWidget* aWidget,nscoord& aAbsX, nscoord& aAbsY, 
                                     nsRect& aClipRect, PRBool& aIsVisible);
#endif


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
  NS_INIT_ISUPPORTS();
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
                         nsIWebBrowserPrint)


PluginViewerImpl::~PluginViewerImpl()
{
#if defined(XP_MAC) || defined(XP_MACOSX)
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
  NS_ENSURE_TRUE(aParentWidget, NS_ERROR_NULL_POINTER);
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
  if (!channel || !mWindow) 
    return NS_ERROR_FAILURE;
  
  NS_IF_RELEASE(mChannel);
  mChannel = channel;
  NS_ADDREF(mChannel);

#ifdef DEBUG
  nsCAutoString contentType;
  channel->GetContentType(contentType);
  printf("PluginViewerImpl::StartLoad: content-type=%s\n", contentType.get());
#endif

  aResult = nsnull;

  // Only instantiate the plugin if our container can host it
  nsCOMPtr<nsIPluginHost> host;

  host = do_GetService(kCPluginManagerCID);
  nsresult rv = NS_ERROR_FAILURE;
  if(host) 
  {
    // create a document so that we can pass something back to plugin
    // instance if it wants one
    nsCOMPtr<nsIDocument> doc(do_CreateInstance(kHTMLDocumentCID));
    if (doc) {    
      mDocument = doc;
      NS_ADDREF(mDocument);  // released in ~nsPluginViewer
      
      // set the document's URL 
      // so it can be fetched later for resolving relative URLs
      nsCOMPtr<nsIURI> uri;
      GetURI(getter_AddRefs(uri));
      if (uri)
        mDocument->SetDocumentURL(uri);
       
      // we're going to need the docshell later to reload this full-page plugin in the event of a plugins refresh
      // so stuff it in the document we created
      nsCOMPtr<nsIScriptGlobalObject> global (do_GetInterface(mContainer));
      if (global) {
        mDocument->SetScriptGlobalObject(global);
        nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(mDocument));
        if (domdoc)
          global->SetNewDocument(domdoc, PR_TRUE, PR_TRUE);
      }

    }

    nsRect r;
    mWindow->GetClientBounds(r);
    rv = CreatePlugin(request, host, nsRect(0, 0, r.width, r.height), aResult);

#if defined(XP_MAC) || defined(XP_MACOSX)
    // On Mac, we need to initiate the intial invalidate for full-page plugins to ensure
    // the entire window gets cleared. Otherwise, Acrobat won't initially repaint on top 
    // of our previous presentation and we may have garbage leftover
    mWindow->Invalidate(PR_FALSE);
#endif
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
#if defined(XP_UNIX) && !defined(XP_MACOSX)
    win->ws_info = nsnull;   //XXX need to figure out what this is. MMP
#endif

    nsIURI* uri;
    rv = mChannel->GetURI(&uri);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString spec;
    rv = uri->GetSpec(spec);
    NS_RELEASE(uri);
    if (NS_FAILED(rv)) return rv;
    NS_ConvertUTF8toUCS2 str(spec);

    nsCAutoString ct;
  
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    channel->GetContentType(ct);
    if (NS_FAILED(rv)) return rv;
    rv = aHost->InstantiateFullPagePlugin(ct.get(), str, aResult, mOwner);
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
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_GetInterface(mContainer));

  if (sgo) {
    sgo->SetNewDocument(nsnull, PR_TRUE, PR_TRUE);
  }

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
    // stop the timer explicitly to reduce reference count.
    mOwner->CancelTimer();

    nsCOMPtr<nsIPluginInstance> inst;
    if(NS_SUCCEEDED(mOwner->GetInstance(*getter_AddRefs(inst))))
    {
      nsPluginWindow *win;
      mOwner->GetWindow(win);
      nsPluginNativeWindow *window = (nsPluginNativeWindow *)win;
      nsCOMPtr<nsIPluginInstance> nullinst;

      PRBool doCache = PR_TRUE;
      PRBool doCallSetWindowAfterDestroy = PR_FALSE;

      // first, determine if the plugin wants to be cached
      inst->GetValue(nsPluginInstanceVariable_DoCacheBool, 
                     (void *) &doCache);
      if (!doCache) {
        // then determine if the plugin wants Destroy to be called after
        // Set Window. This is for bug 50547.
        inst->GetValue(nsPluginInstanceVariable_CallSetWindowAfterDestroyBool, 
                       (void *) &doCallSetWindowAfterDestroy);
        if (!doCallSetWindowAfterDestroy) {
          if (window) 
            window->CallSetWindow(nullinst);
          else 
            inst->SetWindow(nsnull);
        }
        inst->Stop();
        inst->Destroy();
        if (doCallSetWindowAfterDestroy) {
          if (window) 
            window->CallSetWindow(nullinst);
          else 
            inst->SetWindow(nsnull);
        }
      } else {
        window ? window->CallSetWindow(nullinst) : inst->SetWindow(nsnull);
        inst->Stop();
      }
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

#ifndef XP_WIN
  // the Mac, and presumably others, send NS_MOUSE_ACTIVATE
  if (aEvent->message == NS_MOUSE_ACTIVATE) {
    (nsIWidget*)(aEvent->widget)->SetFocus();  // send focus to child window
#if defined(XP_MAC) || defined(XP_MACOSX)
  // furthermore on the Mac nsMacEventHandler sends the NS_PLUGIN_ACTIVATE
  // followed by the mouse down event, so we need to handle this
  } else {
    // on Mac, we store a pointer to this class as native data in the widget
    void *clientData;
    (nsIWidget*)(aEvent->widget)->GetClientData(clientData);
    PluginViewerImpl * pluginViewer = (PluginViewerImpl *)clientData;
    if (pluginViewer && pluginViewer->mOwner && pluginViewer->mWindow == aEvent->widget)
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

#ifdef XP_WIN
NS_IMETHODIMP
PluginViewerImpl::GetPluginPort(HWND *aPort)
{
  *aPort = (HWND) mOwner->GetPluginPort();
  return NS_OK;
}
#endif

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
    nsCOMPtr<nsIPluginInstance> inst;
    mWindow->Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height, PR_FALSE);
    if (mOwner && NS_SUCCEEDED(mOwner->GetInstance(*getter_AddRefs(inst))) && inst) {
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
        
#if defined(XP_MAC) || defined(XP_MACOSX)
        // On Mac we also need to add in the widget offset to the plugin window
        mOwner->FixUpPluginWindow();
#endif        
        ((nsPluginNativeWindow *)win)->CallSetWindow(inst);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PluginViewerImpl::Move(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  if (nsnull != mWindow) {
    nsCOMPtr<nsIPluginInstance> inst;
    mWindow->Move(aX, aY);
    if (mOwner && NS_SUCCEEDED(mOwner->GetInstance(*getter_AddRefs(inst))) && inst) {
      nsPluginWindow  *win;
      if (NS_OK == mOwner->GetWindow(win)) {
        win->x = aX;
        win->y = aY;
        win->clipRect.bottom = (win->clipRect.bottom - win->clipRect.top) + aY;
        win->clipRect.right = (win->clipRect.right - win->clipRect.left) + aX;
        win->clipRect.top = aY;
        win->clipRect.left = aX;

#if defined(XP_MAC) || defined(XP_MACOSX)
        // On Mac we also need to add in the widget offset to the plugin window
        mOwner->FixUpPluginWindow();
#endif
        ((nsPluginNativeWindow *)win)->CallSetWindow(inst);
      }
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

NS_IMETHODIMP
PluginViewerImpl::SetSticky(PRBool aSticky)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PluginViewerImpl::GetSticky(PRBool *aSticky)
{
  *aSticky = PR_FALSE;

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
 * nsIContentViewerEdit
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
 * nsIWebBrowserPrint
 * ======================================================================================== */
NS_IMETHODIMP
PluginViewerImpl::Print(nsIPrintSettings* aPrintSettings,
                        nsIWebProgressListener* aWebProgressListener)
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

/* readonly attribute nsIPrintSettings globalPrintSettings; */
NS_IMETHODIMP
PluginViewerImpl::GetGlobalPrintSettings(nsIPrintSettings * *aGlobalPrintSettings)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void printPreview (in nsIPrintSettings aThePrintSettings); */
NS_IMETHODIMP 
PluginViewerImpl::PrintPreview(nsIPrintSettings *aThePrintSettings, 
                               nsIDOMWindow *aChildDOMWin, 
                               nsIWebProgressListener* aWebProgressListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void printPreviewNavigate (in short aNavType, in long aPageNum); */
NS_IMETHODIMP 
PluginViewerImpl::PrintPreviewNavigate(PRInt16 aNavType, PRInt32 aPageNum)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean doingPrintPreview; */
NS_IMETHODIMP
PluginViewerImpl::GetDoingPrintPreview(PRBool *aDoingPrintPreview)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean doingPrint; */
NS_IMETHODIMP
PluginViewerImpl::GetDoingPrint(PRBool *aDoingPrint)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIPrintSettings currentPrintSettings; */
NS_IMETHODIMP 
PluginViewerImpl::GetCurrentPrintSettings(nsIPrintSettings * *aCurrentPrintSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMWindow currentChildDOMWindow; */
NS_IMETHODIMP 
PluginViewerImpl::GetCurrentChildDOMWindow(nsIDOMWindow * *aCurrentChildDOMWindow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cancel (); */
NS_IMETHODIMP 
PluginViewerImpl::Cancel()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void enumerateDocumentNames (out PRUint32 aCount, [array, size_is (aCount), retval] out wstring aResult); */
NS_IMETHODIMP 
PluginViewerImpl::EnumerateDocumentNames(PRUint32 *aCount, PRUnichar ***aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean isFramesetDocument; */
NS_IMETHODIMP 
PluginViewerImpl::GetIsFramesetDocument(PRBool *aIsFramesetDocument)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean isIFrameSelected; */
NS_IMETHODIMP 
PluginViewerImpl::GetIsIFrameSelected(PRBool *aIsIFrameSelected)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean isRangeSelection; */
NS_IMETHODIMP 
PluginViewerImpl::GetIsRangeSelection(PRBool *aIsRangeSelection)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean isFramesetFrameSelected; */
NS_IMETHODIMP 
PluginViewerImpl::GetIsFramesetFrameSelected(PRBool *aIsFramesetFrameSelected)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long printPreviewNumPages; */
NS_IMETHODIMP 
PluginViewerImpl::GetPrintPreviewNumPages(PRInt32 *aPrintPreviewNumPages)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void exitPrintPreview (); */
NS_IMETHODIMP 
PluginViewerImpl::ExitPrintPreview()
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
  NS_INIT_ISUPPORTS();
  mViewer = aViewer;
  NS_ADDREF(aViewer);
  mNextStream = nsnull;                                                         
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
  NS_INIT_ISUPPORTS();

  // create nsPluginNativeWindow object, it is derived from nsPluginWindow
  // struct and allows to manipulate native window procedure
  nsCOMPtr<nsIPluginHost> ph = do_GetService(kCPluginManagerCID);
  nsCOMPtr<nsPIPluginHost> pph(do_QueryInterface(ph));
  if (pph)
    pph->NewPluginNativeWindow(&mPluginWindow);
  else
    mPluginWindow = nsnull;

  mInstance = nsnull;
  mWindow = nsnull;
  mViewer = nsnull;
  mWidgetVisible = PR_TRUE;
}

pluginInstanceOwner :: ~pluginInstanceOwner()
{
  // shut off the timer.
  CancelTimer();

  NS_IF_RELEASE(mInstance);

  mWindow = nsnull;
  mViewer = nsnull;
  mWidgetVisible = PR_TRUE;

  // clean up plugin native window object
  nsCOMPtr<nsIPluginHost> ph = do_GetService(kCPluginManagerCID);
  nsCOMPtr<nsPIPluginHost> pph(do_QueryInterface(ph));
  if (pph) {
    pph->DeletePluginNativeWindow(mPluginWindow);
    mPluginWindow = nsnull;
  }
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
  NS_ASSERTION(mPluginWindow, "the plugin window object being returned is null");
  aWindow = mPluginWindow;
  return NS_OK;
}

NS_IMETHODIMP pluginInstanceOwner :: GetMode(nsPluginMode *aMode)
{
  *aMode = nsPluginMode_Full;
  return NS_OK;
}

NS_IMETHODIMP pluginInstanceOwner :: CreateWidget(void)
{
  NS_ENSURE_TRUE(mPluginWindow, NS_ERROR_NULL_POINTER);

  PRBool    windowless;
  
  if (nsnull != mInstance) {
    mInstance->GetValue(nsPluginInstanceVariable_WindowlessBool, (void *)&windowless);

    if (PR_TRUE == windowless) {
      mPluginWindow->window = nsnull;    //XXX this needs to be a HDC
      mPluginWindow->type = nsPluginWindowType_Drawable;
    } else if (nsnull != mWindow) {
      mPluginWindow->type = nsPluginWindowType_Window;
      mPluginWindow->window = (nsPluginPort *)mWindow->GetNativeData(NS_NATIVE_PLUGIN_PORT);
    } else
      return NS_ERROR_FAILURE;

#if defined(XP_MAC) || defined(XP_MACOSX)
    FixUpPluginWindow();
    // start the idle timer.
    StartTimer();
#endif
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP pluginInstanceOwner::GetURL(const char *aURL, const char *aTarget, void *aPostData, PRUint32 aPostDataLen, void *aHeadersData, 
                                            PRUint32 aHeadersDataLen, PRBool isFile)
{
  NS_ENSURE_TRUE(mViewer,NS_ERROR_NULL_POINTER);

  // the container of the pres context will give us the link handler
  nsCOMPtr<nsISupports> container;
  nsresult rv = mViewer->GetContainer(getter_AddRefs(container));
  NS_ENSURE_TRUE(container,NS_ERROR_FAILURE);
  nsCOMPtr<nsILinkHandler> lh = do_QueryInterface(container);
  NS_ENSURE_TRUE(lh, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> baseURI;
  rv = mViewer->GetURI(getter_AddRefs(baseURI));
  NS_ENSURE_SUCCESS(rv,NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> targetURI;
  rv = NS_NewURI(getter_AddRefs(targetURI), aURL, baseURI);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsCOMPtr<nsIInputStream> postDataStream;
  nsCOMPtr<nsIInputStream> headersDataStream;

  // deal with post data, either in a file or raw data, and any headers
  if (aPostData) {
    rv = NS_NewPluginPostDataStream(getter_AddRefs(postDataStream), (const char *)aPostData, aPostDataLen, isFile);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed in creating plugin post data stream");
    if (NS_FAILED(rv)) return rv;

    if (aHeadersData) {
      rv = NS_NewPluginPostDataStream(getter_AddRefs(headersDataStream), 
                                      (const char *) aHeadersData, 
                                      aHeadersDataLen,
                                      PR_FALSE,
                                      PR_TRUE);  // last arg says we are headers
      NS_ASSERTION(NS_SUCCEEDED(rv),"failed in creating plugin header data stream");
      if (NS_FAILED(rv)) return rv;
    }
  }


  nsAutoString  unitarget; unitarget.AssignWithConversion(aTarget);
  rv = lh->OnLinkClick(nsnull, eLinkVerb_Replace, 
                       targetURI, unitarget.get(), 
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

#if defined(XP_MAC) || defined(XP_MACOSX)

#if TARGET_CARBON
static void InitializeEventRecord(EventRecord* event)
{
    memset(event, 0, sizeof(EventRecord));
    ::GetGlobalMouse(&event->where);
    event->when = ::TickCount();
    event->modifiers = ::GetCurrentKeyModifiers();
}
#else
inline void InitializeEventRecord(EventRecord* event) { ::OSEventAvail(0, event); }
#endif

void pluginInstanceOwner::GUItoMacEvent(const nsGUIEvent& anEvent, EventRecord& aMacEvent)
{
    InitializeEventRecord(&aMacEvent);
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
  if (!mInstance || !mWindow || anEvent.message == NS_MENU_SELECTED)
    return rv;

#if defined(XP_MAC) || defined(XP_MACOSX)
    //if (mWidget != NULL) {  // check for null mWidget
        EventRecord* event = (EventRecord*)anEvent.nativeMsg;
        if (event == NULL || event->what == nullEvent ||
            anEvent.message == NS_CONTEXTMENU_MESSAGE_START) {
            EventRecord macEvent;
            GUItoMacEvent(anEvent, macEvent);
            event = &macEvent;
            if (event->what == updateEvt) {
                // Add in child windows absolute position to get make the dirty rect
                // relative to the top-level window.
                nsPluginPort* pluginPort = FixUpPluginWindow();
                if (pluginPort) {
                    EventRecord updateEvent;
                    InitializeEventRecord(&updateEvent);
                    updateEvent.what = updateEvt;
                    updateEvent.message = UInt32(pluginPort->port);
    
                    nsPluginEvent pluginEvent = { &updateEvent, nsPluginPlatformWindowRef(pluginPort->port) };
                    PRBool eventHandled = PR_FALSE;
                    mInstance->HandleEvent(&pluginEvent, &eventHandled);
                }
            }
            return nsEventStatus_eConsumeNoDefault;
            
        }
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
        if (pPluginEvent) {
            PRBool eventHandled = PR_FALSE;
            mInstance->HandleEvent(pPluginEvent, &eventHandled);
            if (eventHandled)
                rv = nsEventStatus_eConsumeNoDefault;
        }
#endif

  return rv;
}


// Here's how we give idle time to plugins.

NS_IMETHODIMP pluginInstanceOwner::Notify(nsITimer* /* timer */)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
    // validate the plugin clipping information by syncing the plugin window info to
    // reflect the current widget location. This makes sure that everything is updated
    // correctly in the event of scrolling in the window.
    if (mInstance != NULL) {
        nsPluginPort* pluginPort = FixUpPluginWindow();
        if (pluginPort) {
            EventRecord idleEvent;
            InitializeEventRecord(&idleEvent);
            idleEvent.what = nullEvent;

            // give a bogus 'where' field of our null event when hidden, so Flash
            // won't respond to mouse moves in other tabs, see bug 120875
            if (!mWidgetVisible)
              idleEvent.where.h = idleEvent.where.v = 20000;

            nsPluginEvent pluginEvent = { &idleEvent, nsPluginPlatformWindowRef(pluginPort->port) };

            PRBool eventHandled = PR_FALSE;
            mInstance->HandleEvent(&pluginEvent, &eventHandled);
        }
    }
#endif // XP_MAC || XP_MACOSX
  return NS_OK;
}


void pluginInstanceOwner::CancelTimer()
{
    if (mPluginTimer) {
        mPluginTimer->Cancel();
        mPluginTimer = nsnull;
    }
}


void pluginInstanceOwner::StartTimer()
{
#if defined(XP_MAC) || defined(XP_MACOSX)
  nsresult rv;

  // start a periodic timer to provide null events to the plugin instance.
  if (!mPluginTimer) {
    mPluginTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (rv == NS_OK)
      rv = mPluginTimer->InitWithCallback(this, 1020 / 60, nsITimer::TYPE_REPEATING_SLACK);
  }
#endif
}

nsPluginPort* pluginInstanceOwner::GetPluginPort()
{
//!!! Port must be released for windowless plugins on Windows, because it is HDC !!!

  nsPluginPort* result = NULL;
  if (mWindow != NULL)
  {
    result = (nsPluginPort*) mWindow->GetNativeData(NS_NATIVE_PLUGIN_PORT);
  }
  return result;
}

#if defined(XP_MAC) || defined(XP_MACOSX)

  // calculate the absolute position and clip for a widget 
  // and use other windows in calculating the clip
  // also find out if we are visible or not
static void GetWidgetPosClipAndVis(nsIWidget* aWidget,nscoord& aAbsX, nscoord& aAbsY,
                                   nsRect& aClipRect, PRBool& aIsVisible) 
{
  if (aIsVisible)
    aWidget->IsVisible(aIsVisible);

  aWidget->GetBounds(aClipRect); 
  aAbsX = aClipRect.x; 
  aAbsY = aClipRect.y; 
  
  nscoord ancestorX = -aClipRect.x, ancestorY = -aClipRect.y; 
  // Calculate clipping relative to the widget passed in 
  aClipRect.x = 0; 
  aClipRect.y = 0; 

  // Gather up the absolute position of the widget, clip window, and visibilty 
  nsCOMPtr<nsIWidget> widget = getter_AddRefs(aWidget->GetParent());
  while (widget != nsnull) { 
    if (aIsVisible)
      widget->IsVisible(aIsVisible);

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

  // if we are not visible, clear out the plugin's clip so it won't paint
  if (!aIsVisible)
    aClipRect.Empty();

  //printf("--------------\n"); 
  //printf("Widget clip X %d Y %d rect %d %d %d %d\n", aAbsX, aAbsY,  aClipRect.x,  aClipRect.y, aClipRect.width,  aClipRect.height ); 
  //printf("--------------\n"); 
} 


nsPluginPort* pluginInstanceOwner::FixUpPluginWindow()
{
  if (!mPluginWindow)
    return nsnull;

  if (mWindow) {
    nsPluginPort* pluginPort = GetPluginPort();
    nscoord absWidgetX = 0;
    nscoord absWidgetY = 0;
    nsRect widgetClip(0,0,0,0);
    PRBool isVisible = PR_TRUE;
    GetWidgetPosClipAndVis(mWindow,absWidgetX,absWidgetY,widgetClip,isVisible);

#if defined(MOZ_WIDGET_COCOA)
    // set the port coordinates
    mPluginWindow->x = -pluginPort->portx;
    mPluginWindow->y = -pluginPort->porty;
    widgetClip.x += mPluginWindow->x - absWidgetX;
    widgetClip.y += mPluginWindow->y - absWidgetY;
#else
    // set the port coordinates
    mPluginWindow->x = absWidgetX;
    mPluginWindow->y = absWidgetY;
#endif

    // fix up the clipping region
    mPluginWindow->clipRect.top = widgetClip.y;
    mPluginWindow->clipRect.left = widgetClip.x;
    mPluginWindow->clipRect.bottom =  mPluginWindow->clipRect.top + widgetClip.height;
    mPluginWindow->clipRect.right =  mPluginWindow->clipRect.left + widgetClip.width;  

    if (mWidgetVisible != isVisible) {
      mWidgetVisible = isVisible;
      // must do this to disable async Java Applet drawing
      if (isVisible) {
        mInstance->SetWindow(mPluginWindow);
      } else {
        mInstance->SetWindow(nsnull);
        // switching states, do not draw
        pluginPort = nsnull;
      }
    }

#if defined(MOZ_WIDGET_COCOA)
    if (!mWidgetVisible) {
      mPluginWindow->clipRect.right = mPluginWindow->clipRect.left;
      mPluginWindow->clipRect.bottom = mPluginWindow->clipRect.top;
    }
#endif
    return pluginPort;
  }
  return nsnull;
}

#endif
