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
#include "nsIDocumentLoader.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsIStringBundle.h"
#include "nsIPref.h"
#include "nsReadableUtils.h"

#include "nsEscape.h"
#include "nsVoidArray.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMFocusListener.h"
#include "nsIWebNavigation.h"
#include "nsIWindowWatcher.h"

#include "nsIDOMXULElement.h"

#include "nsGUIEvent.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIAppShell.h"

#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

#include "nsIDOMCharacterData.h"
#include "nsIDOMNodeList.h"

#include "nsIMenuBar.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsITimer.h"

// For JS Execution
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIJSContextStack.h"

#include "nsIEventQueueService.h"
#include "plevent.h"
#include "prmem.h"
#include "prlock.h"

#include "nsIDOMXULDocument.h"

#include "nsIFocusController.h"

#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"

#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIObserverService.h"
#include "prprf.h"
//#include "nsIDOMHTMLInputElement.h"
//#include "nsIDOMHTMLImageElement.h"

#include "nsIContent.h" // for menus

// For calculating size
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"

#include "nsIBaseWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"

#include "nsIMarkupDocumentViewer.h"


// HACK for M4, should be removed by M5
// ... its now M15
#if defined(XP_MAC) || defined(XP_MACOSX)
#include <Menus.h>
#endif
#include "nsIMenuItem.h"
#include "nsIDOMXULDocument.h"
// End hack

#if defined(XP_MAC) || defined(XP_MACOSX)
#define USE_NATIVE_MENUS
#endif

#include "nsIPopupSetFrame.h"

/* Define Class IDs */
static NS_DEFINE_CID(kWindowCID,           NS_WINDOW_CID);
static NS_DEFINE_CID(kWebShellCID,         NS_WEB_SHELL_CID);

#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuBarCID,          NS_MENUBAR_CID);
static NS_DEFINE_CID(kMenuCID,             NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID,         NS_MENUITEM_CID);


#ifdef DEBUG_rods
#define DEBUG_MENUSDEL 1
#endif

#include "nsIWebShell.h"

#define SIZE_PERSISTENCE_TIMEOUT 500 // msec

struct ThreadedWindowEvent {
  PLEvent           event;
  nsWebShellWindow  *window;
};

// The web shell info object is used to hold information about content areas that will
// subsequently be filled in when we receive a webshell added notification.
struct nsWebShellInfo {
  nsString id; // The identifier of the iframe or frame node in the XUL tree.
  PRBool   primary;   // whether it's considered a/the primary content
  nsIWebShell* child; // The child web shell that will end up being used for the content area.

  nsWebShellInfo(const nsString& anID, PRBool aPrimary, nsIWebShell* aChildShell)
  {
    id = anID; 
    primary = aPrimary;
    child = aChildShell;
    NS_IF_ADDREF(aChildShell);
  }

  ~nsWebShellInfo()
  {
    NS_IF_RELEASE(child);
  }
};

nsWebShellWindow::nsWebShellWindow() : nsXULWindow()
{
  mWebShell = nsnull;
  mWindow   = nsnull;
  mLockedUntilChromeLoad = PR_FALSE;
  mIntrinsicallySized = PR_FALSE;
  mDebuting = PR_FALSE;
  mLoadDefaultPage = PR_TRUE;
  mSPTimerLock = PR_NewLock();
}


nsWebShellWindow::~nsWebShellWindow()
{
  if (nsnull != mWebShell) {
    nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(mWebShell));
    shellAsWin->Destroy();
    NS_RELEASE(mWebShell);
  }

  if (mWindow)
    mWindow->SetClientData(0);
  mWindow = nsnull; // Force release here.

  if (mSPTimerLock) {
    PR_Lock(mSPTimerLock);
    if (mSPTimer)
      mSPTimer->Cancel();
    PR_Unlock(mSPTimerLock);
    PR_DestroyLock(mSPTimerLock);
  }
}

NS_IMPL_THREADSAFE_ADDREF(nsWebShellWindow)
NS_IMPL_THREADSAFE_RELEASE(nsWebShellWindow)

NS_INTERFACE_MAP_BEGIN(nsWebShellWindow)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebShellContainer)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellWindow)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellContainer)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIXULWindow)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

nsresult nsWebShellWindow::Initialize(nsIXULWindow* aParent,
                                      nsIAppShell* aShell, nsIURI* aUrl, 
                                      PRBool aCreatedVisible,
                                      PRBool aLoadDefaultPage,
                                      PRInt32 aInitialWidth, PRInt32 aInitialHeight,
                                      PRBool aIsHiddenWindow, nsWidgetInitData& widgetInitData)
{
  nsresult rv;
  nsCOMPtr<nsIWidget> parentWidget;

  mIsHiddenWindow = aIsHiddenWindow;
  
  mShowAfterLoad = aCreatedVisible;
  mLoadDefaultPage = aLoadDefaultPage;
  
  // XXX: need to get the default window size from prefs...
  // Doesn't come from prefs... will come from CSS/XUL/RDF
  nsRect r(0, 0, aInitialWidth, aInitialHeight);
  
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
                  r,                                  // Widget dimensions
                  nsWebShellWindow::HandleEvent,      // Event handler function
                  nsnull,                             // Device context
                  aShell,                             // Application shell
                  nsnull,                             // nsIToolkit
                  &widgetInitData);                   // Widget initialization data
  mWindow->GetClientBounds(r);
  mWindow->SetBackgroundColor(NS_RGB(192,192,192));

  // Create web shell
  mDocShell = do_CreateInstance(kWebShellCID);
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
  CallQueryInterface(mDocShell, &mWebShell);
  NS_ENSURE_TRUE(mWebShell, NS_ERROR_FAILURE);

  r.x = r.y = 0;
  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  NS_ENSURE_SUCCESS(docShellAsWin->InitWindow(nsnull, mWindow, 
   r.x, r.y, r.width, r.height), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(docShellAsWin->Create(), NS_ERROR_FAILURE);
  mWebShell->SetContainer(this);

  // Attach a WebProgress listener.during initialization...
  nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(mDocShell, &rv));
  if (webProgress) {
    webProgress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_NETWORK);
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(EnsureChromeTreeOwner(), NS_ERROR_FAILURE);

  docShellAsItem->SetTreeOwner(mChromeTreeOwner);
  docShellAsItem->SetItemType(nsIDocShellTreeItem::typeChrome);

  if (nsnull != aUrl)  {
    nsCAutoString tmpStr;

    rv = aUrl->GetSpec(tmpStr);
    if (NS_FAILED(rv)) return rv;

    NS_ConvertUTF8toUCS2 urlString(tmpStr);
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
NS_METHOD
nsWebShellWindow::Toolbar()
{
    nsCOMPtr<nsIWebShellWindow> kungFuDeathGrip(this);
    nsCOMPtr<nsIWebBrowserChrome> wbc(do_GetInterface(kungFuDeathGrip));
    if (!wbc) return(PR_FALSE);

    // rjc: don't use "nsIWebBrowserChrome::CHROME_EXTRA"
    //      due to components with multiple sidebar components
    //      (such as Mail/News, Addressbook, etc)... and frankly,
    //      Mac IE, OmniWeb, and other Mac OS X apps all work this way

    PRUint32    chromeMask = (nsIWebBrowserChrome::CHROME_TOOLBAR |
                              nsIWebBrowserChrome::CHROME_LOCATIONBAR |
                              nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR |
                              nsIWebBrowserChrome::CHROME_STATUSBAR);

    PRUint32    chromeFlags, newChromeFlags = 0;
    wbc->GetChromeFlags(&chromeFlags);
    newChromeFlags = chromeFlags & chromeMask;
    if (!newChromeFlags)    chromeFlags |= chromeMask;
    else                    chromeFlags &= (~newChromeFlags);
    wbc->SetChromeFlags(chromeFlags);
    return NS_OK;
}


/*
 * Close the window
 */
NS_METHOD
nsWebShellWindow::Close()
{
   return Destroy();
}


/*
 * Event handler function...
 *
 * This function is called to process events for the nsIWidget of the 
 * nsWebShellWindow...
 */
nsEventStatus PR_CALLBACK
nsWebShellWindow::HandleEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  nsIWebShell* webShell = nsnull;
  nsWebShellWindow *eventWindow = nsnull;

  // Get the WebShell instance...
  if (nsnull != aEvent->widget) {
    void* data;

    aEvent->widget->GetClientData(data);
    if (data != nsnull) {
      eventWindow = NS_REINTERPRET_CAST(nsWebShellWindow *, data);
      webShell = eventWindow->mWebShell;
    }
  }

  if (nsnull != webShell) {
    switch(aEvent->message) {
      /*
       * For size events, the WebShell must be resized to fill the entire
       * client area of the window...
       */
      case NS_MOVE: {
        // persist position, but not immediately, in case this OS is firing
        // repeated move events as the user drags the window
        eventWindow->SetPersistenceTimer(PAD_POSITION);
        break;
      }
      case NS_SIZE: {
        PRBool chromeLock = PR_FALSE;
        nsSizeEvent* sizeEvent = (nsSizeEvent*)aEvent;
        nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(webShell));
        shellAsWin->SetPositionAndSize(0, 0, sizeEvent->windowSize->width, 
          sizeEvent->windowSize->height, PR_FALSE);  
        // persist size, but not immediately, in case this OS is firing
        // repeated size events as the user drags the sizing handle
        if (NS_FAILED(eventWindow->GetLockedState(chromeLock)) || !chromeLock)
          eventWindow->SetPersistenceTimer(PAD_SIZE | PAD_MISC);
        result = nsEventStatus_eConsumeNoDefault;
        break;
      }
      case NS_SIZEMODE: {
        nsSizeModeEvent* modeEvent = (nsSizeModeEvent*)aEvent;

        // an alwaysRaised (or higher) window will hide any newly opened
        // normal browser windows. here we just drop a raised window
        // to the normal zlevel if it's maximized. we make no provision
        // for automatically re-raising it when restored.
        if (modeEvent->mSizeMode == nsSizeMode_Maximized) {
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

        // Note the current implementation of SetSizeMode just stores
        // the new state; it doesn't actually resize. So here we store
        // the state and pass the event on to the OS. The day is coming
        // when we'll handle the event here, and the return result will
        // then need to be different.
#ifdef XP_WIN
        // This is a nasty hack to get around the fact that win32 sends the kill focus
        // event in a different sequence than the deactivate depending on if you're
        // minimizing the window vs. just clicking in a different window to cause
        // the deactivation. Bug #82534
        if(modeEvent->mSizeMode == nsSizeMode_Minimized) {
          nsCOMPtr<nsIDOMWindowInternal> domWindow;
          eventWindow->ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow));
          if (domWindow) {
            nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(domWindow);
            if(privateDOMWindow) {
              nsIFocusController *focusController =
                privateDOMWindow->GetRootFocusController();
              if (focusController)
                focusController->RewindFocusState();
            }
          }
        }
#endif
        break;
      }
      case NS_OS_TOOLBAR: {
        nsCOMPtr<nsIWebShellWindow> kungFuDeathGrip(eventWindow);
        eventWindow->Toolbar();
        break;
      }
      case NS_XUL_CLOSE: {
        // Calling ExecuteCloseHandler may actually close the window
        // (it probably shouldn't, but you never know what the users JS 
        // code will do).  Therefore we add a death-grip to the window
        // for the duration of the close handler.
        nsCOMPtr<nsIWebShellWindow> kungFuDeathGrip(eventWindow);
        if (!eventWindow->ExecuteCloseHandler())
          eventWindow->Close();
        break;
      }
      /*
       * Notify the ApplicationShellService that the window is being closed...
       */
      case NS_DESTROY: {
        eventWindow->Close();
        break;
      }

      case NS_SETZLEVEL: {
        nsZLevelEvent *zEvent = (nsZLevelEvent *) aEvent;

        zEvent->mAdjusted = eventWindow->ConstrainToZLevel(zEvent->mImmediate,
                              &zEvent->mPlacement,
                              zEvent->mReqBelow, &zEvent->mActualBelow);
        break;
      }

      case NS_MOUSE_ACTIVATE:{
        break;
      }
      
      case NS_ACTIVATE: {
#ifdef DEBUG_saari
        printf("nsWebShellWindow::NS_ACTIVATE\n");
#endif
        nsCOMPtr<nsIDOMWindowInternal> domWindow;
        eventWindow->ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow));
        /*
        nsCOMPtr<nsIWebShell> contentShell;
        eventWindow->GetContentWebShell(getter_AddRefs(contentShell));
        if (contentShell) {
          
          if (NS_SUCCEEDED(eventWindow->
              ConvertWebShellToDOMWindow(contentShell, getter_AddRefs(domWindow)))) {
            if(domWindow){
              nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(domWindow);
              if(privateDOMWindow)
                privateDOMWindow->Activate();
            }
          }
        }
        else */
        if (domWindow) {
          nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(domWindow);
          if(privateDOMWindow)
            privateDOMWindow->Activate();
        }
        break;

      }

      case NS_DEACTIVATE: {
#ifdef DEBUG_saari
        printf("nsWebShellWindow::NS_DEACTIVATE\n");
#endif

        nsCOMPtr<nsIDOMWindowInternal> domWindow;
        eventWindow->ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow));
        /*
        nsCOMPtr<nsIWebShell> contentShell;
        eventWindow->GetContentWebShell(getter_AddRefs(contentShell));
        if (contentShell) {
          
          if (NS_SUCCEEDED(eventWindow->
              ConvertWebShellToDOMWindow(contentShell, getter_AddRefs(domWindow)))) {
            if(domWindow){
              nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(domWindow);
              if(privateDOMWindow)
                privateDOMWindow->Deactivate();
            }
          }
        }
        else */
        if (domWindow) {
          nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(domWindow);
          if(privateDOMWindow) {
            nsIFocusController *focusController = 
              privateDOMWindow->GetRootFocusController();
            if (focusController)
              focusController->SetActive(PR_FALSE);
            privateDOMWindow->Deactivate();
          }
        }
        break;
      }
      
      case NS_GOTFOCUS: {
#ifdef DEBUG_saari
        printf("nsWebShellWindow::GOTFOCUS\n");
#endif
        nsCOMPtr<nsIDOMDocument> domDocument;
        nsCOMPtr<nsIDOMWindowInternal> domWindow;
        eventWindow->ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow));
        nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(domWindow));
        if (!domWindow) {
          break;
        }
        nsIFocusController *focusController = piWin->GetRootFocusController();
        if (focusController) {
          // This is essentially the first stage of activation (NS_GOTFOCUS is
          // followed by the DOM window getting activated (which is direct on Win32
          // and done through web shell window via an NS_ACTIVATE message on the
          // other platforms).
          //
          // Go ahead and mark the focus controller as being active.  We have
          // to do this even before the activate message comes in, since focus
          // memory kicks in prior to the activate being processed.
          focusController->SetActive(PR_TRUE);

          nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
          focusController->GetFocusedWindow(getter_AddRefs(focusedWindow));
          if (focusedWindow) {
            // It's possible for focusing the window to cause it to close.
            // To avoid holding a pointer to deleted memory, keep a reference
            // on eventWindow. -bryner
            nsCOMPtr<nsIWebShellWindow> kungFuDeathGrip(eventWindow);

            focusController->SetSuppressFocus(PR_TRUE, "Activation Suppression");
            domWindow->Focus(); // This sets focus, but we'll ignore it.  
                                // A subsequent activate will cause us to stop suppressing.

            // since the window has been activated, replace persistent size data
            // with the newly activated window's
            if (eventWindow->mChromeLoaded) {
              eventWindow->PersistentAttributesDirty(
                             PAD_POSITION | PAD_SIZE | PAD_MISC);
              eventWindow->SavePersistentAttributes();
            }

            break;
          }
        }
        break;
      }
      default:
        break;

    }
  }
  return result;
}

#if 0
//----------------------------------------
NS_IMETHODIMP nsWebShellWindow::CreateMenu(nsIMenuBar * aMenuBar, 
                                           nsIDOMNode * aMenuNode, 
                                           nsString   & aMenuName) 
{
  // Create nsMenu
  nsIMenu * pnsMenu = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID, nsnull, NS_GET_IID(nsIMenu), (void**)&pnsMenu);
  if (NS_OK == rv) {
    // Call Create
    nsISupports * supports = nsnull;
    aMenuBar->QueryInterface(NS_GET_IID(nsISupports), (void**) &supports);
    pnsMenu->Create(supports, aMenuName);
    NS_RELEASE(supports);

    // Set nsMenu Name
    pnsMenu->SetLabel(aMenuName); 
    // Make nsMenu a child of nsMenuBar
    aMenuBar->AddMenu(pnsMenu); 

    // Open the node so that the contents are visible.
    nsCOMPtr<nsIDOMElement> menuElement = do_QueryInterface(aMenuNode);
    if (menuElement)
      menuElement->SetAttribute(NS_LITERAL_STRING("open"), NS_LITERAL_STRING("true"));

    // Begin menuitem inner loop
    
    // Now get the kids. Retrieve our menupopup child.
    nsCOMPtr<nsIDOMNode> menuPopupNode;
    aMenuNode->GetFirstChild(getter_AddRefs(menuPopupNode));
    while (menuPopupNode) {
      nsCOMPtr<nsIDOMElement> menuPopupElement(do_QueryInterface(menuPopupNode));
      if (menuPopupElement) {
        nsString menuPopupNodeType;
        menuPopupElement->GetNodeName(menuPopupNodeType);
        if (menuPopupNodeType.EqualsLiteral("menupopup"))
          break;
      }
      nsCOMPtr<nsIDOMNode> oldMenuPopupNode(menuPopupNode);
      oldMenuPopupNode->GetNextSibling(getter_AddRefs(menuPopupNode));
    }

    if (!menuPopupNode)
      return NS_OK;

    nsCOMPtr<nsIDOMNode> menuitemNode;
    menuPopupNode->GetFirstChild(getter_AddRefs(menuitemNode));

    while (menuitemNode) {
      nsCOMPtr<nsIDOMElement> menuitemElement(do_QueryInterface(menuitemNode));
      if (menuitemElement) {
        nsString menuitemNodeType;
        nsString menuitemName;
        menuitemElement->GetNodeName(menuitemNodeType);
        if (menuitemNodeType.EqualsLiteral("menuitem")) {
          // LoadMenuItem
          LoadMenuItem(pnsMenu, menuitemElement, menuitemNode);
        } else if (menuitemNodeType.EqualsLiteral("menuseparator")) {
          pnsMenu->AddSeparator();
        } else if (menuitemNodeType.EqualsLiteral("menu")) {
          // Load a submenu
          LoadSubMenu(pnsMenu, menuitemElement, menuitemNode);
        }
      }
      nsCOMPtr<nsIDOMNode> oldmenuitemNode(menuitemNode);
      oldmenuitemNode->GetNextSibling(getter_AddRefs(menuitemNode));
    } // end menu item innner loop
    // The parent owns us, so we can release
    NS_RELEASE(pnsMenu);
  }

  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP nsWebShellWindow::LoadMenuItem(
  nsIMenu *    pParentMenu,
  nsIDOMElement * menuitemElement,
  nsIDOMNode *    menuitemNode)
{
  nsString menuitemName;
  nsString menuitemCmd;

  menuitemElement->GetAttribute(NS_LITERAL_STRING("label"), menuitemName);
  menuitemElement->GetAttribute(NS_LITERAL_STRING("cmd"), menuitemCmd);
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull, NS_GET_IID(nsIMenuItem), (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    // Create MenuDelegate - this is the intermediator inbetween 
    // the DOM node and the nsIMenuItem
    // The nsWebShellWindow wacthes for Document changes and then notifies the 
    // the appropriate nsMenuDelegate object
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(menuitemNode));
    if (!domElement) {
      return NS_ERROR_FAILURE;
    }
    
    pnsMenuItem->Create(pParentMenu, menuitemName, 0);                 
    // Set nsMenuItem Name
    //pnsMenuItem->SetLabel(menuitemName);
    
    // Set key shortcut and modifiers
    nsAutoString keyAtom(NS_LITERAL_STRING("key"));
    nsString keyValue;
    domElement->GetAttribute(keyAtom, keyValue);
    
    // Try to find the key node.
    nsCOMPtr<nsIDocument> document;
    nsCOMPtr<nsIContent> content = do_QueryInterface(domElement);
    if (NS_FAILED(rv = content->GetDocument(getter_AddRefs(document)))) {
      NS_ERROR("Unable to retrieve the document.");
      return rv;
    }

    // Turn the document into a XUL document so we can use getElementById
    nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
    if (xulDocument == nsnull) {
      NS_ERROR("not XUL!");
      return NS_ERROR_FAILURE;
    }
  
    nsCOMPtr<nsIDOMElement> keyElement;
    xulDocument->GetElementById(keyValue, getter_AddRefs(keyElement));
    
    if(keyElement){
        PRUint8 modifiers = knsMenuItemNoModifier;
	    nsAutoString shiftAtom(NS_LITERAL_STRING("shift"));
	    nsAutoString altAtom(NS_LITERAL_STRING("alt"));
	    nsAutoString commandAtom(NS_LITERAL_STRING("command"));
	    nsString shiftValue;
	    nsString altValue;
	    nsString commandValue;
	    nsString keyChar(NS_LITERAL_STRING(" "));
	    
	    keyElement->GetAttribute(keyAtom, keyChar);
	    keyElement->GetAttribute(shiftAtom, shiftValue);
	    keyElement->GetAttribute(altAtom, altValue);
	    keyElement->GetAttribute(commandAtom, commandValue);
	    
	    if(!keyChar.EqualsLiteral(" "))
	      pnsMenuItem->SetShortcutChar(keyChar);
	      
	    if(shiftValue.EqualsLiteral("true"))
	      modifiers |= knsMenuItemShiftModifier;
	    
	    if(altValue.EqualsLiteral("true"))
	      modifiers |= knsMenuItemAltModifier;
	    
	    if(commandValue.EqualsLiteral("false"))
	     modifiers |= knsMenuItemCommandModifier;
	      
        pnsMenuItem->SetModifiers(modifiers);
    }
    
    // Make nsMenuItem a child of nsMenu
    nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(NS_GET_IID(nsISupports), (void**) &supports);
    pParentMenu->AddItem(supports);
    NS_RELEASE(supports);
          


    nsAutoString cmdAtom(NS_LITERAL_STRING("onaction"));
    nsString cmdName;

    domElement->GetAttribute(cmdAtom, cmdName);

    nsXULCommand * menuDelegate = new nsXULCommand();
    if ( menuDelegate ) {
        menuDelegate->SetCommand(cmdName);
        menuDelegate->SetDocShell(mDocShell);
        menuDelegate->SetDOMElement(domElement);
        menuDelegate->SetMenuItem(pnsMenuItem);
    } else {
        NS_RELEASE( pnsMenuItem );
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    nsIXULCommand * icmd;
    if (NS_OK == menuDelegate->QueryInterface(NS_GET_IID(nsIXULCommand), (void**) &icmd)) {
      nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(menuDelegate));

      if (listener) 
      {
        pnsMenuItem->AddMenuListener(listener);
        
#ifdef DEBUG_MENUSDEL
        printf("Adding menu listener to [%s]\n", NS_LossyConvertUCS2toASCII(menuitemName).get());
#endif
      } 
#ifdef DEBUG_MENUSDEL
      else 
      {
        printf("*** NOT Adding menu listener to [%s]\n", NS_LossyConvertUCS2toASCII(menuitemName).get());
      }
#endif
      NS_RELEASE(icmd);
    }
    
    // The parent owns us, so we can release
    NS_RELEASE(pnsMenuItem);
  }
  return NS_OK;
}

//----------------------------------------
void nsWebShellWindow::LoadSubMenu(
  nsIMenu *       pParentMenu,
  nsIDOMElement * menuElement,
  nsIDOMNode *    menuNode)
{
  nsString menuName;
  menuElement->GetAttribute(NS_LITERAL_STRING("label"), menuName);
  //printf("Creating Menu [%s] \n", NS_LossyConvertUCS2toASCII(menuName).get());

  // Create nsMenu
  nsIMenu * pnsMenu = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID, nsnull, NS_GET_IID(nsIMenu), (void**)&pnsMenu);
  if (NS_OK == rv) {
    // Call Create
    nsISupports * supports = nsnull;
    pParentMenu->QueryInterface(NS_GET_IID(nsISupports), (void**) &supports);
    pnsMenu->Create(supports, menuName);
    NS_RELEASE(supports); // Balance QI

    // Open the node so that the contents are visible.
    menuElement->SetAttribute(NS_LITERAL_STRING("open"), NS_LITERAL_STRING("true"));
      
    // Set nsMenu Name
    pnsMenu->SetLabel(menuName); 
    // Make nsMenu a child of parent nsMenu
    //pParentMenu->AddMenu(pnsMenu);
    supports = nsnull;
    pnsMenu->QueryInterface(NS_GET_IID(nsISupports), (void**) &supports);
	pParentMenu->AddItem(supports);
	NS_RELEASE(supports);

    // Begin menuitem inner loop
    
    // Now get the kids. Retrieve our menupopup child.
    nsCOMPtr<nsIDOMNode> menuPopupNode;
    menuNode->GetFirstChild(getter_AddRefs(menuPopupNode));
    while (menuPopupNode) {
      nsCOMPtr<nsIDOMElement> menuPopupElement(do_QueryInterface(menuPopupNode));
      if (menuPopupElement) {
        nsString menuPopupNodeType;
        menuPopupElement->GetNodeName(menuPopupNodeType);
        if (menuPopupNodeType.EqualsLiteral("menupopup"))
          break;
      }
      nsCOMPtr<nsIDOMNode> oldMenuPopupNode(menuPopupNode);
      oldMenuPopupNode->GetNextSibling(getter_AddRefs(menuPopupNode));
    }

    if (!menuPopupNode)
      return;

  nsCOMPtr<nsIDOMNode> menuitemNode;
  menuPopupNode->GetFirstChild(getter_AddRefs(menuitemNode));

    while (menuitemNode) {
      nsCOMPtr<nsIDOMElement> menuitemElement(do_QueryInterface(menuitemNode));
      if (menuitemElement) {
        nsString menuitemNodeType;
        menuitemElement->GetNodeName(menuitemNodeType);

#ifdef DEBUG_saari
        printf("Type [%s] %d\n", NS_LossyConvertUCS2toASCII(menuitemNodeType).get(), menuitemNodeType.Equals("menuseparator"));
#endif

        if (menuitemNodeType.EqualsLiteral("menuitem")) {
          // Load a menuitem
          LoadMenuItem(pnsMenu, menuitemElement, menuitemNode);
        } else if (menuitemNodeType.EqualsLiteral("menuseparator")) {
          pnsMenu->AddSeparator();
        } else if (menuitemNodeType.EqualsLiteral("menu")) {
          // Add a submenu
          LoadSubMenu(pnsMenu, menuitemElement, menuitemNode);
        }
      }
      nsCOMPtr<nsIDOMNode> oldmenuitemNode(menuitemNode);
      oldmenuitemNode->GetNextSibling(getter_AddRefs(menuitemNode));
    } // end menu item innner loop
    
    // The parent owns us, so we can release
    NS_RELEASE(pnsMenu);
  }     
}
#endif

//----------------------------------------
void nsWebShellWindow::DynamicLoadMenus(nsIDOMDocument * aDOMDoc, nsIWidget * aParentWindow) 
{
  nsRect oldRect;
  mWindow->GetClientBounds(oldRect);

  // locate the window element which holds toolbars and menus and commands
  nsCOMPtr<nsIDOMElement> element;
  aDOMDoc->GetDocumentElement(getter_AddRefs(element));
  if (!element) {
    return;
  }
  nsCOMPtr<nsIDOMNode> window(do_QueryInterface(element));

  nsresult rv;
  int endCount = 0;
  nsCOMPtr<nsIDOMNode> menubarNode(FindNamedDOMNode(NS_LITERAL_STRING("menubar"), window, endCount, 1));
  if (menubarNode) {
    nsIMenuBar * pnsMenuBar = nsnull;
    rv = nsComponentManager::CreateInstance(kMenuBarCID, nsnull, NS_GET_IID(nsIMenuBar), (void**)&pnsMenuBar);
    if (NS_OK == rv) {
      if (nsnull != pnsMenuBar) {      
        // set pnsMenuBar as a nsMenuListener on aParentWindow
        nsCOMPtr<nsIMenuListener> menuListener;
        pnsMenuBar->QueryInterface(NS_GET_IID(nsIMenuListener), getter_AddRefs(menuListener));

        //fake event
        nsMenuEvent fake;
        menuListener->MenuConstruct(fake, aParentWindow, menubarNode, mWebShell);

        // Parent should own menubar now
        NS_RELEASE(pnsMenuBar);
        
      #ifdef USE_NATIVE_MENUS
      #else
      // Resize around the menu.
      rv = NS_ERROR_FAILURE;

      // do a resize
      nsCOMPtr<nsIContentViewer> contentViewer;
      if( NS_FAILED(mDocShell->GetContentViewer(getter_AddRefs(contentViewer))))
         {
         NS_WARN_IF_FALSE(PR_FALSE, "Error Getting contentViewer");
         return;
         }

      nsCOMPtr<nsIDocumentViewer> docViewer;
      docViewer = do_QueryInterface(contentViewer);
      if (!docViewer) {
          NS_ERROR("Document viewer interface not supported by the content viewer.");
          return;
      }

      nsCOMPtr<nsPresContext> presContext;
      if (NS_FAILED(rv = docViewer->GetPresContext(getter_AddRefs(presContext)))) {
          NS_ERROR("Unable to retrieve the doc viewer's presentation context.");
          return;
      }

      nsRect rect;

      if (NS_FAILED(rv = mWindow->GetClientBounds(rect))) {
          NS_ERROR("Failed to get web shells bounds");
          return;
      }

      // Resize the browser window by the difference.
      PRInt32 heightDelta = oldRect.height - rect.height;
      PRInt32 cx, cy;
      GetSize(&cx, &cy);
      SetSize(cx, cy + heightDelta, PR_FALSE);
      // END REFLOW CODE
      #endif
                  
      } // end if ( nsnull != pnsMenuBar )
    }
  } // end if (menuBar)
} // nsWebShellWindow::DynamicLoadMenus

#if 0
//----------------------------------------
void nsWebShellWindow::LoadMenus(nsIDOMDocument * aDOMDoc, nsIWidget * aParentWindow) 
{
  // locate the window element which holds toolbars and menus and commands
  nsCOMPtr<nsIDOMElement> element;
  aDOMDoc->GetDocumentElement(getter_AddRefs(element));
  if (!element) {
    return;
  }
  nsCOMPtr<nsIDOMNode> window(do_QueryInterface(element));

  nsresult rv;
  int endCount = 0;
  nsCOMPtr<nsIDOMNode> menubarNode(FindNamedDOMNode(NS_LITERAL_STRING("menubar"), window, endCount, 1));
  if (menubarNode) {
    nsIMenuBar * pnsMenuBar = nsnull;
    rv = nsComponentManager::CreateInstance(kMenuBarCID, nsnull, NS_GET_IID(nsIMenuBar), (void**)&pnsMenuBar);
    if (NS_OK == rv) {
      if (nsnull != pnsMenuBar) {
        pnsMenuBar->Create(aParentWindow);
      
        // set pnsMenuBar as a nsMenuListener on aParentWindow
        nsCOMPtr<nsIMenuListener> menuListener;
        pnsMenuBar->QueryInterface(NS_GET_IID(nsIMenuListener), getter_AddRefs(menuListener));
        aParentWindow->AddMenuListener(menuListener);

        nsCOMPtr<nsIDOMNode> menuNode;
        menubarNode->GetFirstChild(getter_AddRefs(menuNode));
        while (menuNode) {
          nsCOMPtr<nsIDOMElement> menuElement(do_QueryInterface(menuNode));
          if (menuElement) {
            nsString menuNodeType;
            nsString menuName;
            menuElement->GetNodeName(menuNodeType);
            if (menuNodeType.EqualsLiteral("menu")) {
              menuElement->GetAttribute(NS_LITERAL_STRING("label"), menuName);

#ifdef DEBUG_rods
              printf("Creating Menu [%s] \n", NS_LossyConvertUCS2toASCII(menuName).get());
#endif
              CreateMenu(pnsMenuBar, menuNode, menuName);
            } 

          }
          nsCOMPtr<nsIDOMNode> oldmenuNode(menuNode);  
          oldmenuNode->GetNextSibling(getter_AddRefs(menuNode));
        } // end while (nsnull != menuNode)
          
        // Give the aParentWindow this nsMenuBar to own.
        aParentWindow->SetMenuBar(pnsMenuBar);
      
        // HACK: force a paint for now
        pnsMenuBar->Paint();
        
        // HACK for M4, should be removed by M5
        // ... it is now M15
#ifdef USE_NATIVE_MENUS
        Handle tempMenuBar = ::GetMenuBar(); // Get a copy of the menu list
		pnsMenuBar->SetNativeData((void*)tempMenuBar);
#endif

        // The parent owns the menubar, so we can release it		
		NS_RELEASE(pnsMenuBar);
    } // end if ( nsnull != pnsMenuBar )
    }
  } // end if (menuBar)

} // nsWebShellWindow::LoadMenus
#endif


//------------------------------------------------------------------------------
NS_IMETHODIMP
nsWebShellWindow::ConvertWebShellToDOMWindow(nsIWebShell* aShell, nsIDOMWindowInternal** aDOMWindow)
{
  nsCOMPtr<nsIScriptGlobalObjectOwner> globalObjectOwner(do_QueryInterface(aShell));
  NS_ENSURE_TRUE(globalObjectOwner, NS_ERROR_FAILURE);

  nsIScriptGlobalObject* globalObject = globalObjectOwner->GetScriptGlobalObject();
  NS_ENSURE_TRUE(globalObject, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindowInternal> newDOMWindow(do_QueryInterface(globalObject));
  NS_ENSURE_TRUE(newDOMWindow, NS_ERROR_FAILURE);

  *aDOMWindow = newDOMWindow.get();
  NS_ADDREF(*aDOMWindow);
  return NS_OK;
}

//----------------------------------------
// nsIWebShellWindow methods...
//----------------------------------------
NS_IMETHODIMP
nsWebShellWindow::Show(PRBool aShow)
{
  return nsXULWindow::SetVisibility(aShow);
}

NS_IMETHODIMP
nsWebShellWindow::ShowModal()
{
  return nsXULWindow::ShowModal();
}

/* return the main, outermost webshell in this window */
NS_IMETHODIMP 
nsWebShellWindow::GetWebShell(nsIWebShell *& aWebShell)
{
  aWebShell = mWebShell;
  NS_ADDREF(mWebShell);
  return NS_OK;
}

/* return the webshell intended to hold (html) content.  In a simple
   browser window, that would be the main content area.  If no such
   webshell was found for any reason, the outermost webshell will be
   returned.  (Note that is the main chrome webshell, and probably
   not what you wanted, but at least it's a webshell.)
     Also note that if no content webshell was marked "primary,"
   we return the chrome webshell, even if (non-primary) content webshells
   do exist.  Thas was done intentionally.  The selection would be
   nondeterministic, and it seems dangerous to set a precedent like that.
*/
NS_IMETHODIMP
nsWebShellWindow::GetContentWebShell(nsIWebShell **aResult)
{
   *aResult = nsnull;
   nsCOMPtr<nsIDocShellTreeItem> content;

   GetPrimaryContentShell(getter_AddRefs(content));
   if(!content)
      return NS_OK;
   CallQueryInterface(content, aResult);

   return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::GetWidget(nsIWidget *& aWidget)
{
  aWidget = mWindow;
  NS_IF_ADDREF(aWidget);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::GetDOMWindow(nsIDOMWindowInternal** aDOMWindow)
{
   return ConvertWebShellToDOMWindow(mWebShell, aDOMWindow);
}

void *
nsWebShellWindow::HandleModalDialogEvent(PLEvent *aEvent)
{
  ThreadedWindowEvent *event = (ThreadedWindowEvent *) aEvent;

  event->window->ShowModal();
  return 0;
}

void
nsWebShellWindow::DestroyModalDialogEvent(PLEvent *aEvent)
{
  PR_Free(aEvent);
}

void
nsWebShellWindow::SetPersistenceTimer(PRUint32 aDirtyFlags)
{
  if (!mSPTimerLock)
    return;

  PR_Lock(mSPTimerLock);
  if (mSPTimer) {
    mSPTimer->SetDelay(SIZE_PERSISTENCE_TIMEOUT);
    PersistentAttributesDirty(aDirtyFlags);
  } else {
    nsresult rv;
    mSPTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      NS_ADDREF_THIS(); // for the timer, which holds a reference to this window
      mSPTimer->InitWithFuncCallback(FirePersistenceTimer, this,
                                     SIZE_PERSISTENCE_TIMEOUT, nsITimer::TYPE_ONE_SHOT);
      PersistentAttributesDirty(aDirtyFlags);
    }
  }
  PR_Unlock(mSPTimerLock);
}

void
nsWebShellWindow::FirePersistenceTimer(nsITimer *aTimer, void *aClosure)
{
  nsWebShellWindow *win = NS_STATIC_CAST(nsWebShellWindow *, aClosure);
  if (!win->mSPTimerLock)
    return;
  PR_Lock(win->mSPTimerLock);
  win->SavePersistentAttributes();
  PR_Unlock(win->mSPTimerLock);
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


#ifdef DEBUG_MENUSDEL
  printf("OnEndDocumentLoad\n");
#endif

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
  // register as document listener
  // this is needed for menus
  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
   
    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
    if (!docv)
      return NS_OK;

    nsCOMPtr<nsIDocument> doc;
    docv->GetDocument(getter_AddRefs(doc));
    if (!doc)
      return NS_OK;

    doc->AddObserver(NS_STATIC_CAST(nsIDocumentObserver*, this));
  }
#endif

#ifdef USE_NATIVE_MENUS
  ///////////////////////////////
  // Find the Menubar DOM  and Load the menus, hooking them up to the loaded commands
  ///////////////////////////////
  nsCOMPtr<nsIDOMDocument> menubarDOMDoc(GetNamedDOMDoc(NS_LITERAL_STRING("this"))); // XXX "this" is a small kludge for code reused
  if (menubarDOMDoc)
  {
#ifdef SOME_PLATFORM // Anyone using native non-dynamic menus should add themselves here.
    LoadMenus(menubarDOMDoc, mWindow);
    // Context Menu test
    nsCOMPtr<nsIDOMElement> element;
    menubarDOMDoc->GetDocumentElement(getter_AddRefs(element));
    nsCOMPtr<nsIDOMNode> window(do_QueryInterface(element));

    int endCount = 0;
    contextMenuTest = FindNamedDOMNode(NS_LITERAL_STRING("contextmenu"), window, endCount, 1);
    // End Context Menu test
#else
    DynamicLoadMenus(menubarDOMDoc, mWindow);
#endif 
  }
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
nsCOMPtr<nsIDOMNode> nsWebShellWindow::FindNamedDOMNode(const nsAString &aName, nsIDOMNode * aParent, PRInt32 & aCount, PRInt32 aEndCount)
{
  if(!aParent)
    return nsnull;
    
  nsCOMPtr<nsIDOMNode> node;
  aParent->GetFirstChild(getter_AddRefs(node));
  while (node) {
    nsString name;
    node->GetNodeName(name);
    //printf("FindNamedDOMNode[%s]==[%s] %d == %d\n", NS_LossyConvertUCS2toASCII(aName).get(), NS_LossyConvertUCS2toASCII(name).get(), aCount+1, aEndCount);
    if (name.Equals(aName)) {
      aCount++;
      if (aCount == aEndCount)
        return node;
    }
    PRBool hasChildren;
    node->HasChildNodes(&hasChildren);
    if (hasChildren) {
      nsCOMPtr<nsIDOMNode> found(FindNamedDOMNode(aName, node, aCount, aEndCount));
      if (found)
        return found;
    }
    nsCOMPtr<nsIDOMNode> oldNode = node;
    oldNode->GetNextSibling(getter_AddRefs(node));
  }
  node = do_QueryInterface(nsnull);
  return node;

} // nsWebShellWindow::FindNamedDOMNode

//----------------------------------------
nsCOMPtr<nsIDOMDocument> nsWebShellWindow::GetNamedDOMDoc(const nsAString & aWebShellName)
{
  nsCOMPtr<nsIDOMDocument> domDoc; // result == nsnull;

  // first get the toolbar child docShell
  nsCOMPtr<nsIDocShell> childDocShell;
  if (aWebShellName.EqualsLiteral("this")) { // XXX small kludge for code reused
    childDocShell = mDocShell;
  } else {
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem;
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(mDocShell));
    docShellAsNode->FindChildWithName(PromiseFlatString(aWebShellName).get(), 
      PR_TRUE, PR_FALSE, nsnull, getter_AddRefs(docShellAsItem));
    childDocShell = do_QueryInterface(docShellAsItem);
    if (!childDocShell)
      return domDoc;
  }
  
  nsCOMPtr<nsIContentViewer> cv;
  childDocShell->GetContentViewer(getter_AddRefs(cv));
  if (!cv)
    return domDoc;
   
  nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
  if (!docv)
    return domDoc;

  nsCOMPtr<nsIDocument> doc;
  docv->GetDocument(getter_AddRefs(doc));
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
    nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(contentViewer);
    if (docViewer) {
      nsCOMPtr<nsIDocument> doc;
      docViewer->GetDocument(getter_AddRefs(doc));
      nsIURI *mainURL = doc->GetDocumentURI();

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

      // see if we have a webshell with a matching contentAreaID
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
PRBool nsWebShellWindow::ExecuteCloseHandler()
{
  /* If the event handler closes this window -- a likely scenario --
     things get deleted out of order without this death grip.
     (The problem may be the death grip in nsWindow::windowProc,
     which forces this window's widget to remain alive longer
     than it otherwise would.) */
  nsCOMPtr<nsIWebShellWindow> kungFuDeathGrip(this);

  nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(mWebShell));

  if (globalObject) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
    nsCOMPtr<nsIDocumentViewer> docViewer(do_QueryInterface(contentViewer));

    if (docViewer) {
      nsCOMPtr<nsPresContext> presContext;
      docViewer->GetPresContext(getter_AddRefs(presContext));

      nsEventStatus status = nsEventStatus_eIgnore;
      nsMouseEvent event(NS_XUL_CLOSE);

      nsresult rv = globalObject->HandleDOMEvent(presContext, &event, nsnull,
                                                 NS_EVENT_FLAG_INIT, &status);
      if (NS_SUCCEEDED(rv) && status == nsEventStatus_eConsumeNoDefault)
        return PR_TRUE;
      // else fall through and return PR_FALSE
    }
  }

  return PR_FALSE;
} // ExecuteCloseHandler

//----------------------------------------------------------------
//-- nsIDocumentObserver
//----------------------------------------------------------------
NS_IMPL_NSIDOCUMENTOBSERVER_CORE_STUB(nsWebShellWindow)
NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(nsWebShellWindow)
NS_IMPL_NSIDOCUMENTOBSERVER_REFLOW_STUB(nsWebShellWindow)
NS_IMPL_NSIDOCUMENTOBSERVER_STATE_STUB(nsWebShellWindow)
NS_IMPL_NSIDOCUMENTOBSERVER_STYLE_STUB(nsWebShellWindow)

///////////////////////////////////////////////////////////////
// nsIDocumentObserver
// this is needed for menu changes
///////////////////////////////////////////////////////////////
void
nsWebShellWindow::CharacterDataChanged(nsIDocument *aDocument,
                                       nsIContent* aContent,
                                       PRBool aAppend)
{
}

void
nsWebShellWindow::AttributeChanged(nsIDocument *aDocument,
                                   nsIContent*  aContent,
                                   PRInt32      aNameSpaceID,
                                   nsIAtom*     aAttribute,
                                   PRInt32      aModType)
{
  // XXX: Uh, none of this nsIDocumentObserver stuff is needed if the
  // blow code isn't needed.
#if 0
  //printf("AttributeChanged\n");
  PRInt32 i;
  for (i=0;i<mMenuDelegates.Count();i++) {
    nsIXULCommand * cmd  = (nsIXULCommand *)mMenuDelegates[i];
    nsIDOMElement * node;
    cmd->GetDOMElement(&node);
    //nsCOMPtr<nsIContent> content(do_QueryInterface(node));
    // Doing this for the must speed
    nsIContent * content;
    if (NS_OK == node->QueryInterface(NS_GET_IID(nsIContent), (void**) &content)) {
      if (content == aContent) {
        nsAutoString attr;
        aAttribute->ToString(attr);
        cmd->AttributeHasBeenSet(attr);
      }
      NS_RELEASE(content);
    }
  }
#endif  
}

void
nsWebShellWindow::ContentAppended(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            PRInt32     aNewIndexInContainer)
{
}

void
nsWebShellWindow::ContentInserted(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer)
{
}

void
nsWebShellWindow::ContentRemoved(nsIDocument *aDocument,
                           nsIContent* aContainer,
                           nsIContent* aChild,
                           PRInt32 aIndexInContainer)
{
}

// nsIBaseWindow
NS_IMETHODIMP nsWebShellWindow::Destroy()
{
  nsresult rv;
  nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(mDocShell, &rv));
  if (webProgress) {
    webProgress->RemoveProgressListener(this);
  }

#ifdef USE_NATIVE_MENUS
  {
  // unregister as document listener
  // this is needed for menus
   nsCOMPtr<nsIContentViewer> cv;
   if(mDocShell)
 	   mDocShell->GetContentViewer(getter_AddRefs(cv));
   nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
   if(docv)
      {
      nsCOMPtr<nsIDocument> doc;
      docv->GetDocument(getter_AddRefs(doc));
      if(doc)
         doc->RemoveObserver(NS_STATIC_CAST(nsIDocumentObserver*, this));
      }
   }
#endif

  nsCOMPtr<nsIWebShellWindow> kungFuDeathGrip(this);
  if (mSPTimerLock) {
  PR_Lock(mSPTimerLock);
  if (mSPTimer) {
    mSPTimer->Cancel();
    SavePersistentAttributes();
    mSPTimer = nsnull;
    NS_RELEASE_THIS(); // the timer held a reference to us
  }
  PR_Unlock(mSPTimerLock);
  PR_DestroyLock(mSPTimerLock);
  mSPTimerLock = nsnull;
  }
  return nsXULWindow::Destroy();
}

