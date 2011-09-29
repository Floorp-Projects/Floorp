/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsWebShellWindow.h"

#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsIWeakReference.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsIStringBundle.h"
#include "nsReadableUtils.h"

#include "nsEscape.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIWebNavigation.h"
#include "nsIWindowWatcher.h"

#include "nsIDOMXULElement.h"

#include "nsGUIEvent.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIAppShell.h"

#include "nsIAppShellService.h"

#include "nsIDOMCharacterData.h"
#include "nsIDOMNodeList.h"

#include "nsITimer.h"
#include "nsXULPopupManager.h"

#include "prmem.h"

#include "nsIDOMXULDocument.h"

#include "nsFocusManager.h"

#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"

#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIObserverService.h"
#include "prprf.h"

#include "nsIScreenManager.h"
#include "nsIScreen.h"

#include "nsIContent.h" // for menus

// For calculating size
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"

#include "nsIBaseWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"

#include "nsIMarkupDocumentViewer.h"

#ifdef XP_MACOSX
#include "nsINativeMenuService.h"
#define USE_NATIVE_MENUS
#endif

using namespace mozilla;

/* Define Class IDs */
static NS_DEFINE_CID(kWindowCID,           NS_WINDOW_CID);

#define SIZE_PERSISTENCE_TIMEOUT 500 // msec

nsWebShellWindow::nsWebShellWindow(PRUint32 aChromeFlags)
  : nsXULWindow(aChromeFlags)
  , mSPTimerLock("nsWebShellWindow.mSPTimerLock")
{
}


nsWebShellWindow::~nsWebShellWindow()
{
  if (mWindow) {
    mWindow->SetClientData(0);
    mWindow->Destroy();
    mWindow = nsnull; // Force release here.
  }

  MutexAutoLock lock(mSPTimerLock);
  if (mSPTimer)
    mSPTimer->Cancel();
}

NS_IMPL_ADDREF_INHERITED(nsWebShellWindow, nsXULWindow)
NS_IMPL_RELEASE_INHERITED(nsWebShellWindow, nsXULWindow)

NS_INTERFACE_MAP_BEGIN(nsWebShellWindow)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END_INHERITING(nsXULWindow)

nsresult nsWebShellWindow::Initialize(nsIXULWindow* aParent,
                                      nsIXULWindow* aOpener,
                                      nsIAppShell* aShell, nsIURI* aUrl,
                                      PRInt32 aInitialWidth,
                                      PRInt32 aInitialHeight,
                                      bool aIsHiddenWindow,
                                      nsWidgetInitData& widgetInitData)
{
  nsresult rv;
  nsCOMPtr<nsIWidget> parentWidget;

  mIsHiddenWindow = aIsHiddenWindow;

  PRInt32 initialX = 0, initialY = 0;
  nsCOMPtr<nsIBaseWindow> base(do_QueryInterface(aOpener));
  if (base) {
    rv = base->GetPositionAndSize(&mOpenerScreenRect.x,
                                  &mOpenerScreenRect.y,
                                  &mOpenerScreenRect.width,
                                  &mOpenerScreenRect.height);
    if (NS_FAILED(rv)) {
      mOpenerScreenRect.SetEmpty();
    } else {
      initialX = mOpenerScreenRect.x;
      initialY = mOpenerScreenRect.y;
      ConstrainToOpenerScreen(&initialX, &initialY);
    }
  }

  // XXX: need to get the default window size from prefs...
  // Doesn't come from prefs... will come from CSS/XUL/RDF
  nsIntRect r(initialX, initialY, aInitialWidth, aInitialHeight);
  
  // Create top level window
  mWindow = do_CreateInstance(kWindowCID, &rv);
  if (NS_OK != rv) {
    return rv;
  }

  /* This next bit is troublesome. We carry two different versions of a pointer
     to our parent window. One is the parent window's widget, which is passed
     to our own widget. The other is a weak reference we keep here to our
     parent WebShellWindow. The former is useful to the widget, and we can't
     trust its treatment of the parent reference because they're platform-
     specific. The latter is useful to this class.
       A better implementation would be one in which the parent keeps strong
     references to its children and closes them before it allows itself
     to be closed. This would mimic the behaviour of OSes that support
     top-level child windows in OSes that do not. Later.
  */
  nsCOMPtr<nsIBaseWindow> parentAsWin(do_QueryInterface(aParent));
  if (parentAsWin) {
    parentAsWin->GetMainWidget(getter_AddRefs(parentWidget));
    mParentWindow = do_GetWeakReference(aParent);
  }

  mWindow->SetClientData(this);
  mWindow->Create((nsIWidget *)parentWidget,          // Parent nsIWidget
                  nsnull,                             // Native parent widget
                  r,                                  // Widget dimensions
                  nsWebShellWindow::HandleEvent,      // Event handler function
                  nsnull,                             // Device context
                  aShell,                             // Application shell
                  nsnull,                             // nsIToolkit
                  &widgetInitData);                   // Widget initialization data
  mWindow->GetClientBounds(r);
  // Match the default background color of content. Important on windows
  // since we no longer use content child widgets.
  mWindow->SetBackgroundColor(NS_RGB(255,255,255));

  // Create web shell
  mDocShell = do_CreateInstance("@mozilla.org/docshell;1");
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  // Make sure to set the item type on the docshell _before_ calling
  // Create() so it knows what type it is.
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(EnsureChromeTreeOwner(), NS_ERROR_FAILURE);

  docShellAsItem->SetTreeOwner(mChromeTreeOwner);
  docShellAsItem->SetItemType(nsIDocShellTreeItem::typeChrome);

  r.x = r.y = 0;
  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  NS_ENSURE_SUCCESS(docShellAsWin->InitWindow(nsnull, mWindow, 
   r.x, r.y, r.width, r.height), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(docShellAsWin->Create(), NS_ERROR_FAILURE);

  // Attach a WebProgress listener.during initialization...
  nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(mDocShell, &rv));
  if (webProgress) {
    webProgress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_NETWORK);
  }

  if (nsnull != aUrl)  {
    nsCString tmpStr;

    rv = aUrl->GetSpec(tmpStr);
    if (NS_FAILED(rv)) return rv;

    NS_ConvertUTF8toUTF16 urlString(tmpStr);
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
    NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
    rv = webNav->LoadURI(urlString.get(),
                         nsIWebNavigation::LOAD_FLAGS_NONE,
                         nsnull,
                         nsnull,
                         nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
  }
                     
  return rv;
}


/*
 * Toolbar
 */
nsresult
nsWebShellWindow::Toolbar()
{
    nsCOMPtr<nsIXULWindow> kungFuDeathGrip(this);
    nsCOMPtr<nsIWebBrowserChrome> wbc(do_GetInterface(kungFuDeathGrip));
    if (!wbc)
      return NS_ERROR_UNEXPECTED;

    // rjc: don't use "nsIWebBrowserChrome::CHROME_EXTRA"
    //      due to components with multiple sidebar components
    //      (such as Mail/News, Addressbook, etc)... and frankly,
    //      Mac IE, OmniWeb, and other Mac OS X apps all work this way

    PRUint32    chromeMask = (nsIWebBrowserChrome::CHROME_TOOLBAR |
                              nsIWebBrowserChrome::CHROME_LOCATIONBAR |
                              nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);

    PRUint32    chromeFlags, newChromeFlags = 0;
    wbc->GetChromeFlags(&chromeFlags);
    newChromeFlags = chromeFlags & chromeMask;
    if (!newChromeFlags)    chromeFlags |= chromeMask;
    else                    chromeFlags &= (~newChromeFlags);
    wbc->SetChromeFlags(chromeFlags);
    return NS_OK;
}


/*
 * Event handler function...
 *
 * This function is called to process events for the nsIWidget of the 
 * nsWebShellWindow...
 */
nsEventStatus
nsWebShellWindow::HandleEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  nsIDocShell* docShell = nsnull;
  nsWebShellWindow *eventWindow = nsnull;

  // Get the WebShell instance...
  if (nsnull != aEvent->widget) {
    void* data;

    aEvent->widget->GetClientData(data);
    if (data != nsnull) {
      eventWindow = reinterpret_cast<nsWebShellWindow *>(data);
      docShell = eventWindow->mDocShell;
    }
  }

  if (docShell) {
    switch(aEvent->message) {
      /*
       * For size events, the DocShell must be resized to fill the entire
       * client area of the window...
       */
      case NS_MOVE: {
        // Adjust any child popups so that their widget offsets and coordinates
        // are correct with respect to the new position of the window
        nsCOMPtr<nsIMenuRollup> pm =
          do_GetService("@mozilla.org/xul/xul-popup-manager;1");
        if (pm) {
          nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(docShell);
          pm->AdjustPopupsOnWindowChange(window);
        }

        // persist position, but not immediately, in case this OS is firing
        // repeated move events as the user drags the window
        eventWindow->SetPersistenceTimer(PAD_POSITION);
        break;
      }
      case NS_SIZE: {
        nsCOMPtr<nsIMenuRollup> pm =
          do_GetService("@mozilla.org/xul/xul-popup-manager;1");
        if (pm) {
          nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(docShell);
          pm->AdjustPopupsOnWindowChange(window);
        }
 
        nsSizeEvent* sizeEvent = (nsSizeEvent*)aEvent;
        nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(docShell));
        shellAsWin->SetPositionAndSize(0, 0, sizeEvent->windowSize->width, 
          sizeEvent->windowSize->height, PR_FALSE);  
        // persist size, but not immediately, in case this OS is firing
        // repeated size events as the user drags the sizing handle
        if (!eventWindow->IsLocked())
          eventWindow->SetPersistenceTimer(PAD_POSITION | PAD_SIZE | PAD_MISC);
        result = nsEventStatus_eConsumeNoDefault;
        break;
      }
      case NS_SIZEMODE: {
        nsSizeModeEvent* modeEvent = (nsSizeModeEvent*)aEvent;

        // an alwaysRaised (or higher) window will hide any newly opened
        // normal browser windows. here we just drop a raised window
        // to the normal zlevel if it's maximized. we make no provision
        // for automatically re-raising it when restored.
        if (modeEvent->mSizeMode == nsSizeMode_Maximized ||
            modeEvent->mSizeMode == nsSizeMode_Fullscreen) {
          PRUint32 zLevel;
          eventWindow->GetZLevel(&zLevel);
          if (zLevel > nsIXULWindow::normalZ)
            eventWindow->SetZLevel(nsIXULWindow::normalZ);
        }

        aEvent->widget->SetSizeMode(modeEvent->mSizeMode);

        // persist mode, but not immediately, because in many (all?)
        // cases this will merge with the similar call in NS_SIZE and
        // write the attribute values only once.
        eventWindow->SetPersistenceTimer(PAD_MISC);
        result = nsEventStatus_eConsumeDoDefault;

        nsCOMPtr<nsPIDOMWindow> ourWindow = do_GetInterface(docShell);
        if (ourWindow) {
          // Let the application know if it's in fullscreen mode so it
          // can update its UI.
          if (modeEvent->mSizeMode == nsSizeMode_Fullscreen) {
            ourWindow->SetFullScreen(PR_TRUE);
          }

          // And always fire a user-defined sizemodechange event on the window
          ourWindow->DispatchCustomEvent("sizemodechange");
        }

        // Note the current implementation of SetSizeMode just stores
        // the new state; it doesn't actually resize. So here we store
        // the state and pass the event on to the OS. The day is coming
        // when we'll handle the event here, and the return result will
        // then need to be different.
        break;
      }
      case NS_OS_TOOLBAR: {
        nsCOMPtr<nsIXULWindow> kungFuDeathGrip(eventWindow);
        eventWindow->Toolbar();
        break;
      }
      case NS_XUL_CLOSE: {
        // Calling ExecuteCloseHandler may actually close the window
        // (it probably shouldn't, but you never know what the users JS 
        // code will do).  Therefore we add a death-grip to the window
        // for the duration of the close handler.
        nsCOMPtr<nsIXULWindow> kungFuDeathGrip(eventWindow);
        if (!eventWindow->ExecuteCloseHandler())
          eventWindow->Destroy();
        break;
      }
      /*
       * Notify the ApplicationShellService that the window is being closed...
       */
      case NS_DESTROY: {
        eventWindow->Destroy();
        break;
      }

      case NS_UISTATECHANGED: {
        nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(docShell);
        if (window) {
          nsUIStateChangeEvent* event = (nsUIStateChangeEvent*)aEvent;
          window->SetKeyboardIndicators(event->showAccelerators, event->showFocusRings);
        }
        break;
      }

      case NS_SETZLEVEL: {
        nsZLevelEvent *zEvent = (nsZLevelEvent *) aEvent;

        zEvent->mAdjusted = eventWindow->ConstrainToZLevel(zEvent->mImmediate,
                              &zEvent->mPlacement,
                              zEvent->mReqBelow, &zEvent->mActualBelow);
        break;
      }

      case NS_ACTIVATE: {
#if defined(DEBUG_saari) || defined(DEBUG_smaug)
        printf("nsWebShellWindow::NS_ACTIVATE\n");
#endif
        // focusing the window could cause it to close, so keep a reference to it
        nsCOMPtr<nsIXULWindow> kungFuDeathGrip(eventWindow);

        nsCOMPtr<nsIDOMWindow> window = do_GetInterface(docShell);
        nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
        if (fm && window)
          fm->WindowRaised(window);

        if (eventWindow->mChromeLoaded) {
          eventWindow->PersistentAttributesDirty(
                             PAD_POSITION | PAD_SIZE | PAD_MISC);
          eventWindow->SavePersistentAttributes();
        }

        break;
      }

      case NS_DEACTIVATE: {
#if defined(DEBUG_saari) || defined(DEBUG_smaug)
        printf("nsWebShellWindow::NS_DEACTIVATE\n");
#endif

        nsCOMPtr<nsIDOMWindow> window = do_GetInterface(docShell);
        nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
        if (fm && window)
          fm->WindowLowered(window);
        break;
      }
      
      case NS_GETACCESSIBLE: {
        nsCOMPtr<nsIPresShell> presShell;
        docShell->GetPresShell(getter_AddRefs(presShell));
        if (presShell) {
          presShell->HandleEventWithTarget(aEvent, nsnull, nsnull, &result);
        }
        break;
      }
      default:
        break;

    }
  }
  return result;
}

#ifdef USE_NATIVE_MENUS
static void LoadNativeMenus(nsIDOMDocument *aDOMDoc, nsIWidget *aParentWindow)
{
  // Find the menubar tag (if there is more than one, we ignore all but
  // the first).
  nsCOMPtr<nsIDOMNodeList> menubarElements;
  aDOMDoc->GetElementsByTagNameNS(NS_LITERAL_STRING("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"),
                                  NS_LITERAL_STRING("menubar"),
                                  getter_AddRefs(menubarElements));

  nsCOMPtr<nsIDOMNode> menubarNode;
  if (menubarElements)
    menubarElements->Item(0, getter_AddRefs(menubarNode));
  if (!menubarNode)
    return;

  nsCOMPtr<nsINativeMenuService> nms = do_GetService("@mozilla.org/widget/nativemenuservice;1");
  nsCOMPtr<nsIContent> menubarContent(do_QueryInterface(menubarNode));
  if (nms && menubarContent)
    nms->CreateNativeMenuBar(aParentWindow, menubarContent);
}
#endif

void
nsWebShellWindow::SetPersistenceTimer(PRUint32 aDirtyFlags)
{
  MutexAutoLock lock(mSPTimerLock);
  if (!mSPTimer) {
    nsresult rv;
    mSPTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      NS_ADDREF_THIS(); // for the timer, which holds a reference to this window
    }
  }
  mSPTimer->InitWithFuncCallback(FirePersistenceTimer, this,
                                 SIZE_PERSISTENCE_TIMEOUT, nsITimer::TYPE_ONE_SHOT);
  PersistentAttributesDirty(aDirtyFlags);
}

void
nsWebShellWindow::FirePersistenceTimer(nsITimer *aTimer, void *aClosure)
{
  nsWebShellWindow *win = static_cast<nsWebShellWindow *>(aClosure);
  MutexAutoLock lock(win->mSPTimerLock);
  win->SavePersistentAttributes();
}


//----------------------------------------
// nsIWebProgessListener implementation
//----------------------------------------
NS_IMETHODIMP
nsWebShellWindow::OnProgressChange(nsIWebProgress *aProgress,
                                   nsIRequest *aRequest,
                                   PRInt32 aCurSelfProgress,
                                   PRInt32 aMaxSelfProgress,
                                   PRInt32 aCurTotalProgress,
                                   PRInt32 aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnStateChange(nsIWebProgress *aProgress,
                                nsIRequest *aRequest,
                                PRUint32 aStateFlags,
                                nsresult aStatus)
{
  // If the notification is not about a document finishing, then just
  // ignore it...
  if (!(aStateFlags & nsIWebProgressListener::STATE_STOP) || 
      !(aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK)) {
    return NS_OK;
  }

  if (mChromeLoaded)
    return NS_OK;

  // If this document notification is for a frame then ignore it...
  nsCOMPtr<nsIDOMWindow> eventWin;
  aProgress->GetDOMWindow(getter_AddRefs(eventWin));
  nsCOMPtr<nsPIDOMWindow> eventPWin(do_QueryInterface(eventWin));
  if (eventPWin) {
    nsPIDOMWindow *rootPWin = eventPWin->GetPrivateRoot();
    if (eventPWin != rootPWin)
      return NS_OK;
  }

  mChromeLoaded = PR_TRUE;
  mLockedUntilChromeLoad = PR_FALSE;

#ifdef USE_NATIVE_MENUS
  ///////////////////////////////
  // Find the Menubar DOM  and Load the menus, hooking them up to the loaded commands
  ///////////////////////////////
  nsCOMPtr<nsIDOMDocument> menubarDOMDoc(GetNamedDOMDoc(NS_LITERAL_STRING("this"))); // XXX "this" is a small kludge for code reused
  if (menubarDOMDoc)
    LoadNativeMenus(menubarDOMDoc, mWindow);
#endif // USE_NATIVE_MENUS

  OnChromeLoaded();
  LoadContentAreas();

  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnLocationChange(nsIWebProgress *aProgress,
                                   nsIRequest *aRequest,
                                   nsIURI *aURI)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::OnStatusChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 nsresult aStatus,
                                 const PRUnichar* aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnSecurityChange(nsIWebProgress *aWebProgress,
                                   nsIRequest *aRequest,
                                   PRUint32 state)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}


//----------------------------------------
nsCOMPtr<nsIDOMDocument> nsWebShellWindow::GetNamedDOMDoc(const nsAString & aDocShellName)
{
  nsCOMPtr<nsIDOMDocument> domDoc; // result == nsnull;

  // first get the toolbar child docShell
  nsCOMPtr<nsIDocShell> childDocShell;
  if (aDocShellName.EqualsLiteral("this")) { // XXX small kludge for code reused
    childDocShell = mDocShell;
  } else {
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem;
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(mDocShell));
    docShellAsNode->FindChildWithName(PromiseFlatString(aDocShellName).get(), 
      PR_TRUE, PR_FALSE, nsnull, nsnull, getter_AddRefs(docShellAsItem));
    childDocShell = do_QueryInterface(docShellAsItem);
    if (!childDocShell)
      return domDoc;
  }
  
  nsCOMPtr<nsIContentViewer> cv;
  childDocShell->GetContentViewer(getter_AddRefs(cv));
  if (!cv)
    return domDoc;
 
  nsIDocument* doc = cv->GetDocument();
  if (doc)
    return nsCOMPtr<nsIDOMDocument>(do_QueryInterface(doc));

  return domDoc;
} // nsWebShellWindow::GetNamedDOMDoc

//----------------------------------------

// if the main document URL specified URLs for any content areas, start them loading
void nsWebShellWindow::LoadContentAreas() {

  nsAutoString searchSpec;

  // fetch the chrome document URL
  nsCOMPtr<nsIContentViewer> contentViewer;
  // yes, it's possible for the docshell to be null even this early
  // see bug 57514.
  if (mDocShell)
    mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer) {
    nsIDocument* doc = contentViewer->GetDocument();
    if (doc) {
      nsIURI* mainURL = doc->GetDocumentURI();

      nsCOMPtr<nsIURL> url = do_QueryInterface(mainURL);
      if (url) {
        nsCAutoString search;
        url->GetQuery(search);

        AppendUTF8toUTF16(search, searchSpec);
      }
    }
  }

  // content URLs are specified in the search part of the URL
  // as <contentareaID>=<escapedURL>[;(repeat)]
  if (!searchSpec.IsEmpty()) {
    PRInt32     begPos,
                eqPos,
                endPos;
    nsString    contentAreaID,
                contentURL;
    char        *urlChar;
    nsresult rv;
    for (endPos = 0; endPos < (PRInt32)searchSpec.Length(); ) {
      // extract contentAreaID and URL substrings
      begPos = endPos;
      eqPos = searchSpec.FindChar('=', begPos);
      if (eqPos < 0)
        break;

      endPos = searchSpec.FindChar(';', eqPos);
      if (endPos < 0)
        endPos = searchSpec.Length();
      searchSpec.Mid(contentAreaID, begPos, eqPos-begPos);
      searchSpec.Mid(contentURL, eqPos+1, endPos-eqPos-1);
      endPos++;

      // see if we have a docshell with a matching contentAreaID
      nsCOMPtr<nsIDocShellTreeItem> content;
      rv = GetContentShellById(contentAreaID.get(), getter_AddRefs(content));
      if (NS_SUCCEEDED(rv) && content) {
        nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(content));
        if (webNav) {
          urlChar = ToNewCString(contentURL);
          if (urlChar) {
            nsUnescape(urlChar);
            contentURL.AssignWithConversion(urlChar);
            webNav->LoadURI(contentURL.get(),
                          nsIWebNavigation::LOAD_FLAGS_NONE,
                          nsnull,
                          nsnull,
                          nsnull);
            nsMemory::Free(urlChar);
          }
        }
      }
    }
  }
}

/**
 * ExecuteCloseHandler - Run the close handler, if any.
 * @return PR_TRUE iff we found a close handler to run.
 */
bool nsWebShellWindow::ExecuteCloseHandler()
{
  /* If the event handler closes this window -- a likely scenario --
     things get deleted out of order without this death grip.
     (The problem may be the death grip in nsWindow::windowProc,
     which forces this window's widget to remain alive longer
     than it otherwise would.) */
  nsCOMPtr<nsIXULWindow> kungFuDeathGrip(this);

  nsCOMPtr<nsPIDOMWindow> window(do_GetInterface(mDocShell));
  nsCOMPtr<nsIDOMEventTarget> eventTarget = do_QueryInterface(window);

  if (eventTarget) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
    nsCOMPtr<nsIDocumentViewer> docViewer(do_QueryInterface(contentViewer));

    if (docViewer) {
      nsRefPtr<nsPresContext> presContext;
      docViewer->GetPresContext(getter_AddRefs(presContext));

      nsEventStatus status = nsEventStatus_eIgnore;
      nsMouseEvent event(PR_TRUE, NS_XUL_CLOSE, nsnull,
                         nsMouseEvent::eReal);

      nsresult rv =
        eventTarget->DispatchDOMEvent(&event, nsnull, presContext, &status);
      if (NS_SUCCEEDED(rv) && status == nsEventStatus_eConsumeNoDefault)
        return PR_TRUE;
      // else fall through and return PR_FALSE
    }
  }

  return PR_FALSE;
} // ExecuteCloseHandler

void nsWebShellWindow::ConstrainToOpenerScreen(PRInt32* aX, PRInt32* aY)
{
  if (mOpenerScreenRect.IsEmpty()) {
    *aX = *aY = 0;
    return;
  }

  PRInt32 left, top, width, height;
  // Constrain initial positions to the same screen as opener
  nsCOMPtr<nsIScreenManager> screenmgr = do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (screenmgr) {
    nsCOMPtr<nsIScreen> screen;
    screenmgr->ScreenForRect(mOpenerScreenRect.x, mOpenerScreenRect.y,
                             mOpenerScreenRect.width, mOpenerScreenRect.height,
                             getter_AddRefs(screen));
    if (screen) {
      screen->GetAvailRect(&left, &top, &width, &height);
      if (*aX < left || *aX > left + width) {
        *aX = left;
      }
      if (*aY < top || *aY > top + height) {
        *aY = top;
      }
    }
  }
}

// nsIBaseWindow
NS_IMETHODIMP nsWebShellWindow::Destroy()
{
  nsresult rv;
  nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(mDocShell, &rv));
  if (webProgress) {
    webProgress->RemoveProgressListener(this);
  }

  nsCOMPtr<nsIXULWindow> kungFuDeathGrip(this);
  {
    MutexAutoLock lock(mSPTimerLock);
    if (mSPTimer) {
      mSPTimer->Cancel();
      SavePersistentAttributes();
      mSPTimer = nsnull;
      NS_RELEASE_THIS(); // the timer held a reference to us
    }
  }
  return nsXULWindow::Destroy();
}
