/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nsWebShellWindow.h"

#include "nsLayoutCID.h"
#include "nsIWeakReference.h"
#include "nsIDocumentLoader.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIStringBundle.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsIPref.h"

#include "nsINameSpaceManager.h"
#include "nsEscape.h"
#include "nsVoidArray.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMFocusListener.h"
#include "nsIWebNavigation.h"

#include "nsIXULPopupListener.h"
#include "nsIDOMXULElement.h"
#include "nsRDFCID.h"

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
#include "nsIContextMenu.h"
#include "nsITimer.h"

// For JS Execution
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIJSContextStack.h"

#include "nsIEventQueueService.h"
#include "plevent.h"
#include "prmem.h"
#include "prlock.h"

#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"

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
#include "nsIPresContext.h"

#include "nsIBaseWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"

#include "nsIMarkupDocumentViewer.h"


// HACK for M4, should be removed by M5
// ... its now M15
#if defined(XP_MAC) || defined(RHAPSODY)
#include <Menus.h>
#endif
#include "nsIMenuItem.h"
#include "nsIDOMXULDocument.h"
// End hack

#if (defined(XP_MAC) && !TARGET_CARBON) || defined(RHAPSODY)
#define USE_NATIVE_MENUS
#endif

#include "nsIPopupSetFrame.h"
#include "nsIWalletService.h"

/* Define Class IDs */
static NS_DEFINE_CID(kWindowCID,           NS_WINDOW_CID);
static NS_DEFINE_CID(kWebShellCID,         NS_WEB_SHELL_CID);
static NS_DEFINE_CID(kAppShellServiceCID,  NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kAppShellCID,         NS_APPSHELL_CID);

#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuBarCID,          NS_MENUBAR_CID);
static NS_DEFINE_CID(kMenuCID,             NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID,         NS_MENUITEM_CID);
static NS_DEFINE_CID(kContextMenuCID,      NS_CONTEXTMENU_CID);

static NS_DEFINE_CID(kPrefCID,             NS_PREF_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static NS_DEFINE_CID(kLayoutDocumentLoaderFactoryCID, NS_LAYOUT_DOCUMENT_LOADER_FACTORY_CID);
static NS_DEFINE_CID(kXULPopupListenerCID, NS_XULPOPUPLISTENER_CID);
static NS_DEFINE_CID(kSingleSignOnPromptCID, NS_SINGLESIGNONPROMPT_CID);


#ifdef DEBUG_rods
#define DEBUG_MENUSDEL 1
#endif
#include "nsICommonDialogs.h"

static NS_DEFINE_CID(	kCommonDialogsCID, NS_CommonDialog_CID );
#include "nsIWalletService.h"
static NS_DEFINE_CID(kWalletServiceCID, NS_WALLETSERVICE_CID);
#include "nsIWebShell.h"

static NS_DEFINE_CID(kStringBundleServiceCID,     NS_STRINGBUNDLESERVICE_CID);

#define SIZE_PERSISTENCE_TIMEOUT 500 // msec
#define kWebShellLocaleProperties "chrome://global/locale/commonDialogs.properties"


const char * kPrimaryContentTypeValue  = "content-primary";

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

// a little utility object to push an event queue and pop it when it
// goes out of scope. should probably be in a file of utility functions.
class stEventQueueStack {
public:
  stEventQueueStack();
  ~stEventQueueStack();
  nsresult Success() const { return mPushedStack; }
private:
  nsCOMPtr<nsIEventQueueService> mService;
  nsCOMPtr<nsIEventQueue>        mQueue;
  nsresult             mGotService,
                       mPushedStack;
};
stEventQueueStack::stEventQueueStack() {

  // ick! this makes bad assumptions about the structure of the service, but
  // the service manager seems to need to work this way...
  mService = do_GetService(kEventQueueServiceCID, &mGotService);

  mPushedStack = mGotService;
  if (NS_SUCCEEDED(mGotService))
    mService->PushThreadEventQueue(getter_AddRefs(mQueue));
}
stEventQueueStack::~stEventQueueStack() {
  if (NS_SUCCEEDED(mPushedStack))
    mService->PopThreadEventQueue(mQueue);
    // more ick!
}

nsWebShellWindow::nsWebShellWindow() : nsXULWindow()
{
  NS_INIT_REFCNT();

  mWebShell = nsnull;
  mWindow   = nsnull;
  mLockedUntilChromeLoad = PR_FALSE;
  mIntrinsicallySized = PR_FALSE;
  mDebuting = PR_FALSE;
  mLoadDefaultPage = PR_TRUE;
  mKillScrollbarsAfterLoad = PR_FALSE;
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

  PR_Lock(mSPTimerLock);
  if (mSPTimer)
    mSPTimer->Cancel();
  PR_Unlock(mSPTimerLock);
  PR_DestroyLock(mSPTimerLock);
}

NS_IMPL_THREADSAFE_ADDREF(nsWebShellWindow);
NS_IMPL_THREADSAFE_RELEASE(nsWebShellWindow);

NS_INTERFACE_MAP_BEGIN(nsWebShellWindow)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebShellContainer)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellWindow)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellContainer)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentLoaderObserver)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY(nsIXULWindow)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
NS_INTERFACE_MAP_END

nsresult nsWebShellWindow::Initialize(nsIXULWindow* aParent,
                                      nsIAppShell* aShell, nsIURI* aUrl, 
                                      PRBool aCreatedVisible,
                                      PRBool aLoadDefaultPage,
                                      PRBool aContentScrollbars,
                                      PRUint32 aZlevel,
                                      PRInt32 aInitialWidth, PRInt32 aInitialHeight,
                                      nsWidgetInitData& widgetInitData)
{
  nsresult rv;
  nsCOMPtr<nsIWidget> parentWidget;

  mShowAfterLoad = aCreatedVisible;
  mLoadDefaultPage = aLoadDefaultPage;
  mKillScrollbarsAfterLoad = !aContentScrollbars;
  mZlevel = aZlevel;
  
  // XXX: need to get the default window size from prefs...
  // Doesn't come from prefs... will come from CSS/XUL/RDF
  nsRect r(0, 0, aInitialWidth, aInitialHeight);
  
  // Create top level window
  rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, NS_GET_IID(nsIWidget),
                                    (void**)&mWindow);
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
    mParentWindow = getter_AddRefs(NS_GetWeakReference(aParent));
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
  CallQueryInterface(mDocShell, &mWebShell);

  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  r.x = r.y = 0;
  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  NS_ENSURE_SUCCESS(docShellAsWin->InitWindow(nsnull, mWindow, 
   r.x, r.y, r.width, r.height), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(docShellAsWin->Create(), NS_ERROR_FAILURE);
  mWebShell->SetContainer(this);
  mDocShell->SetDocLoaderObserver(this);

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(EnsureChromeTreeOwner(), NS_ERROR_FAILURE);

  docShellAsItem->SetTreeOwner(mChromeTreeOwner);
  docShellAsItem->SetItemType(nsIDocShellTreeItem::typeChrome);

  if (nsnull != aUrl)  {
    char *tmpStr = NULL;
    nsAutoString urlString;

    rv = aUrl->GetSpec(&tmpStr);
    if (NS_FAILED(rv)) return rv;
    urlString.AssignWithConversion(tmpStr);
    nsCRT::free(tmpStr);
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
    NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(webNav->LoadURI(urlString.GetUnicode()), NS_ERROR_FAILURE);
  }
                     
  return rv;
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

  // Get the WebShell instance...
  if (nsnull != aEvent->widget) {
    void* data;

    aEvent->widget->GetClientData(data);
    if (nsnull != data) {
      webShell = ((nsWebShellWindow*)data)->mWebShell;
    }
  }

  if (nsnull != webShell) {
    switch(aEvent->message) {
      /*
       * For size events, the WebShell must be resized to fill the entire
       * client area of the window...
       */
      case NS_MOVE: {
        void* data;
        nsWebShellWindow *win;
        aEvent->widget->GetClientData(data);
        win = NS_REINTERPRET_CAST(nsWebShellWindow *, data);
        // persist position, but not immediately, in case this OS is firing
        // repeated move events as the user drags the window
        win->SetPersistenceTimer(PR_FALSE, PR_TRUE);
        break;
      }
      case NS_SIZE: {
        void* data;
        nsWebShellWindow *win;
        nsSizeEvent* sizeEvent = (nsSizeEvent*)aEvent;
        nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(webShell));
        shellAsWin->SetPositionAndSize(0, 0, sizeEvent->windowSize->width, 
          sizeEvent->windowSize->height, PR_FALSE);  
        aEvent->widget->GetClientData(data);
        win = NS_REINTERPRET_CAST(nsWebShellWindow *, data);
        // persist size, but not immediately, in case this OS is firing
        // repeated size events as the user drags the sizing handle
        win->SetPersistenceTimer(PR_TRUE, PR_FALSE);
        result = nsEventStatus_eConsumeNoDefault;
        break;
      }
      case NS_SIZEMODE: {
        void* data;
        nsWebShellWindow *win;
        nsSizeModeEvent* modeEvent = (nsSizeModeEvent*)aEvent;
        aEvent->widget->SetSizeMode(modeEvent->mSizeMode);
        aEvent->widget->GetClientData(data);
        win = NS_REINTERPRET_CAST(nsWebShellWindow *, data);
        win->StoreBoundsToXUL(PR_FALSE, PR_FALSE, PR_TRUE);
        result = nsEventStatus_eConsumeDoDefault;
        // Note the current implementation of SetSizeMode just stores
        // the new state; it doesn't actually resize. So here we store
        // the state and pass the event on to the OS. The day is coming
        // when we'll handle the event here, and the return result will
        // then need to be different.
        break;
      }
      case NS_XUL_CLOSE: {
        void* data;
        nsWebShellWindow *win;
        aEvent->widget->GetClientData(data);
        win = NS_REINTERPRET_CAST(nsWebShellWindow *, data);
        if (!win->ExecuteCloseHandler())
          win->Close();
        break;
      }
      /*
       * Notify the ApplicationShellService that the window is being closed...
       */
      case NS_DESTROY: {
        void* data;
        aEvent->widget->GetClientData(data);
        if (data)
           ((nsWebShellWindow *)data)->Close();
        break;
      }

      case NS_SETZLEVEL: {
        void             *data;
        nsZLevelEvent    *zEvent = (nsZLevelEvent *) aEvent;

        zEvent->widget->GetClientData(data);
        if (data) {
          nsWebShellWindow *win;
          win = NS_REINTERPRET_CAST(nsWebShellWindow *, data);
          zEvent->mAdjusted = win->ConstrainToZLevel(zEvent->mImmediate,
                                &zEvent->mPlacement,
                                zEvent->mReqBelow, &zEvent->mActualBelow);
        }
        break;
      }

      case NS_MOUSE_ACTIVATE:
      case NS_ACTIVATE: {
#ifdef DEBUG_saari
        printf("nsWebShellWindow::NS_ACTIVATE\n");
#endif
        break;
      }

      case NS_DEACTIVATE: {
#ifdef DEBUG_saari
        printf("nsWebShellWindow::NS_DEACTIVATE\n");
#endif
        
        void* data;
        aEvent->widget->GetClientData(data);
        if (!data)
          break;
          
        nsCOMPtr<nsIDOMWindowInternal> domWindow;
        nsCOMPtr<nsIWebShell> contentShell;
        ((nsWebShellWindow *)data)->GetContentWebShell(getter_AddRefs(contentShell));
        if (contentShell) {
          
          if (NS_SUCCEEDED(((nsWebShellWindow *)data)->
              ConvertWebShellToDOMWindow(contentShell, getter_AddRefs(domWindow)))) {
            if(domWindow){
              nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(domWindow);
              if(privateDOMWindow)
                privateDOMWindow->Deactivate();
            }
          }
        }
        else if (domWindow) {
          nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(domWindow);
          if(privateDOMWindow)
            privateDOMWindow->Deactivate();
        }
          
        break;
      }
      
      case NS_GOTFOCUS: {
#ifdef DEBUG_saari
        printf("nsWebShellWindow::GOTFOCUS\n");
#endif
        void* data;
        aEvent->widget->GetClientData(data);
        if (!data)
          break;

        nsCOMPtr<nsIDOMDocument> domDocument;
        nsCOMPtr<nsIDOMWindowInternal> domWindow;
        ((nsWebShellWindow *)data)->ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow));
        domWindow->GetDocument(getter_AddRefs(domDocument));
        nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(domDocument);
        if (xulDoc) {
          nsCOMPtr<nsIDOMXULCommandDispatcher> commandDispatcher;
          xulDoc->GetCommandDispatcher(getter_AddRefs(commandDispatcher));
          if (commandDispatcher) {
            nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
            commandDispatcher->GetFocusedWindow(getter_AddRefs(focusedWindow));
            if (focusedWindow) {
              //commandDispatcher->SetSuppressFocus(PR_TRUE);
              domWindow->Focus(); // This sets focus, but we'll ignore it.  
                                  // A subsequent activate will cause us to stop suppressing.
              break;
            }
          }
        }

        nsCOMPtr<nsIWebShell> contentShell;
        ((nsWebShellWindow *)data)->GetContentWebShell(getter_AddRefs(contentShell));
        if (contentShell) {
          
          if (NS_SUCCEEDED(((nsWebShellWindow *)data)->
              ConvertWebShellToDOMWindow(contentShell, getter_AddRefs(domWindow)))) {
            domWindow->Focus();
          }
        }
        else if (domWindow)
          domWindow->Focus();
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
      menuElement->SetAttribute(NS_ConvertASCIItoUCS2("open"), NS_ConvertASCIItoUCS2("true"));

    // Begin menuitem inner loop
    
    // Now get the kids. Retrieve our menupopup child.
    nsCOMPtr<nsIDOMNode> menuPopupNode;
    aMenuNode->GetFirstChild(getter_AddRefs(menuPopupNode));
    while (menuPopupNode) {
      nsCOMPtr<nsIDOMElement> menuPopupElement(do_QueryInterface(menuPopupNode));
      if (menuPopupElement) {
        nsString menuPopupNodeType;
        menuPopupElement->GetNodeName(menuPopupNodeType);
        if (menuPopupNodeType.EqualsWithConversion("menupopup"))
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
        if (menuitemNodeType.EqualsWithConversion("menuitem")) {
          // LoadMenuItem
          LoadMenuItem(pnsMenu, menuitemElement, menuitemNode);
        } else if (menuitemNodeType.EqualsWithConversion("menuseparator")) {
          pnsMenu->AddSeparator();
        } else if (menuitemNodeType.EqualsWithConversion("menu")) {
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

  menuitemElement->GetAttribute(NS_ConvertASCIItoUCS2("value"), menuitemName);
  menuitemElement->GetAttribute(NS_ConvertASCIItoUCS2("cmd"), menuitemCmd);
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
    nsAutoString keyAtom; keyAtom.AssignWithConversion("key");
    nsString keyValue;
    domElement->GetAttribute(keyAtom, keyValue);
    
    // Try to find the key node.
    nsCOMPtr<nsIDocument> document;
    nsCOMPtr<nsIContent> content = do_QueryInterface(domElement);
    if (NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document)))) {
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
	    nsAutoString shiftAtom; shiftAtom.AssignWithConversion("shift");
	    nsAutoString altAtom; altAtom.AssignWithConversion("alt");
	    nsAutoString commandAtom; commandAtom.AssignWithConversion("command");
	    nsString shiftValue;
	    nsString altValue;
	    nsString commandValue;
	    nsString keyChar; keyChar.AssignWithConversion(" ");
	    
	    keyElement->GetAttribute(keyAtom, keyChar);
	    keyElement->GetAttribute(shiftAtom, shiftValue);
	    keyElement->GetAttribute(altAtom, altValue);
	    keyElement->GetAttribute(commandAtom, commandValue);
	    
	    if(!keyChar.EqualsWithConversion(" "))
	      pnsMenuItem->SetShortcutChar(keyChar);
	      
	    if(shiftValue.EqualsWithConversion("true"))
	      modifiers |= knsMenuItemShiftModifier;
	    
	    if(altValue.EqualsWithConversion("true"))
	      modifiers |= knsMenuItemAltModifier;
	    
	    if(commandValue.EqualsWithConversion("false"))
	     modifiers |= knsMenuItemCommandModifier;
	      
        pnsMenuItem->SetModifiers(modifiers);
    }
    
    // Make nsMenuItem a child of nsMenu
    nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(NS_GET_IID(nsISupports), (void**) &supports);
    pParentMenu->AddItem(supports);
    NS_RELEASE(supports);
          


    nsAutoString cmdAtom; cmdAtom.AssignWithConversion("onaction");
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
        printf("Adding menu listener to [%s]\n", menuitemName.ToNewCString());
#endif
      } 
#ifdef DEBUG_MENUSDEL
      else 
      {
        printf("*** NOT Adding menu listener to [%s]\n", menuitemName.ToNewCString());
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
  menuElement->GetAttribute(NS_ConvertASCIItoUCS2("value"), menuName);
  //printf("Creating Menu [%s] \n", menuName.ToNewCString()); // this leaks

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
    menuElement->SetAttribute(NS_ConvertASCIItoUCS2("open"), NS_ConvertASCIItoUCS2("true"));
      
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
        if (menuPopupNodeType.EqualsWithConversion("menupopup"))
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
        printf("Type [%s] %d\n", menuitemNodeType.ToNewCString(), menuitemNodeType.Equals("menuseparator"));
#endif

        if (menuitemNodeType.EqualsWithConversion("menuitem")) {
          // Load a menuitem
          LoadMenuItem(pnsMenu, menuitemElement, menuitemNode);
        } else if (menuitemNodeType.EqualsWithConversion("menuseparator")) {
          pnsMenu->AddSeparator();
        } else if (menuitemNodeType.EqualsWithConversion("menu")) {
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
  nsCOMPtr<nsIDOMNode> menubarNode(FindNamedDOMNode(NS_ConvertASCIItoUCS2("menubar"), window, endCount, 1));
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

      nsCOMPtr<nsIPresContext> presContext;
      if (NS_FAILED(rv = docViewer->GetPresContext(*getter_AddRefs(presContext)))) {
          NS_ERROR("Unable to retrieve the doc viewer's presentation context.");
          return;
      }

      nsCOMPtr<nsIPresShell> presShell;
      if (NS_FAILED(rv = presContext->GetShell(getter_AddRefs(presShell)))) {
          NS_ERROR("Unable to retrieve the shell from the presentation context.");
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
  nsCOMPtr<nsIDOMNode> menubarNode(FindNamedDOMNode(NS_ConvertASCIItoUCS2("menubar"), window, endCount, 1));
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
            if (menuNodeType.EqualsWithConversion("menu")) {
              menuElement->GetAttribute(NS_ConvertASCIItoUCS2("value"), menuName);

#ifdef DEBUG_rods
              printf("Creating Menu [%s] \n", menuName.ToNewCString()); // this leaks
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

//------------------------------------------------------------------------------
void nsWebShellWindow::DoContextMenu(
  nsMenuEvent * aMenuEvent,
  nsIDOMNode  * aMenuNode, 
  nsIWidget   * aParentWindow,
  PRInt32       aX,
  PRInt32       aY,
  const nsString& aPopupAlignment,
  const nsString& aAnchorAlignment) 
{
  if (aMenuNode) {
    nsIContextMenu * pnsContextMenu;
    nsresult rv = nsComponentManager::CreateInstance(kContextMenuCID, nsnull, NS_GET_IID(nsIContextMenu), (void**)&pnsContextMenu);
    if (NS_SUCCEEDED(rv) && pnsContextMenu) {
        nsISupports * supports;
        aParentWindow->QueryInterface(NS_GET_IID(nsISupports), (void**) &supports);
        pnsContextMenu->Create(supports, aPopupAlignment, aAnchorAlignment);
        NS_RELEASE(supports);
        pnsContextMenu->SetLocation(aX,aY);
        // Set webshell
        pnsContextMenu->SetWebShell( mWebShell );
        
        // Set DOM node
        pnsContextMenu->SetDOMNode( aMenuNode );
        
        // Construct and show menu
        nsIMenuListener * listener;
        pnsContextMenu->QueryInterface(NS_GET_IID(nsIMenuListener), (void**) &listener);
        
        // Dynamically construct and track the menu
        listener->MenuSelected(*aMenuEvent);
        
        // Destroy the menu
        listener->MenuDeselected(*aMenuEvent); 
        
        // The parent owns the context menu, so we can release it	
        NS_RELEASE(listener);	
		    NS_RELEASE(pnsContextMenu);
    }
  } // end if (aMenuNode)
}
#endif

//------------------------------------------------------------------------------
NS_IMETHODIMP
nsWebShellWindow::GetContentShellById(const nsString& aID, nsIWebShell** aChildShell)
{
	// Set to null just to be certain
   *aChildShell = nsnull;

   nsCOMPtr<nsIDocShellTreeItem> content;

   nsXULWindow::GetContentShellById(aID.GetUnicode(), getter_AddRefs(content));
   if(!content)
      return NS_ERROR_FAILURE;
   CallQueryInterface(content, aChildShell);

   return NS_OK;
}

//------------------------------------------------------------------------------
NS_IMETHODIMP
nsWebShellWindow::ConvertWebShellToDOMWindow(nsIWebShell* aShell, nsIDOMWindowInternal** aDOMWindow)
{
  nsCOMPtr<nsIScriptGlobalObjectOwner> globalObjectOwner(do_QueryInterface(aShell));
  NS_ENSURE_TRUE(globalObjectOwner, NS_ERROR_FAILURE);

  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  globalObjectOwner->GetScriptGlobalObject(getter_AddRefs(globalObject));
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


// yes, this one's name and ShowModal are a confusing pair. plan is to merge
// the two someday.
NS_IMETHODIMP
nsWebShellWindow::ShowModally(PRBool aPrepare)
{ 
   NS_ERROR("Can't use this anymore");
   return NS_ERROR_FAILURE;
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
nsWebShellWindow::SetPersistenceTimer(PRBool aSize, PRBool aPosition)
{
  PR_Lock(mSPTimerLock);
  if (mSPTimer) {
    mSPTimer->SetDelay(SIZE_PERSISTENCE_TIMEOUT);
    mSPTimerSize |= aSize;
    mSPTimerPosition |= aPosition;
  } else {
    nsresult rv;
    mSPTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      mSPTimer->Init(FirePersistenceTimer, this,
                     SIZE_PERSISTENCE_TIMEOUT, NS_TYPE_ONE_SHOT);
      mSPTimerSize = aSize;
      mSPTimerPosition = aPosition;
    }
  }
  PR_Unlock(mSPTimerLock);
}

void
nsWebShellWindow::FirePersistenceTimer(nsITimer *aTimer, void *aClosure)
{
  nsWebShellWindow *win = NS_STATIC_CAST(nsWebShellWindow *, aClosure);
  PR_Lock(win->mSPTimerLock);
  win->mSPTimer = nsnull;
  PR_Unlock(win->mSPTimerLock);
  win->StoreBoundsToXUL(win->mSPTimerPosition, win->mSPTimerSize, PR_FALSE);
}


//----------------------------------------
// nsIDocumentLoaderObserver implementation
//----------------------------------------
NS_IMETHODIMP
nsWebShellWindow::OnStartDocumentLoad(nsIDocumentLoader* loader, 
                                      nsIURI* aURL, const char* aCommand)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnEndDocumentLoad(nsIDocumentLoader* loader, 
                                    nsIChannel* channel, nsresult aStatus)
{
#ifdef DEBUG_MENUSDEL
  printf("OnEndDocumentLoad\n");
#endif

  /* We get notified every time a page/Frame is loaded. But we need to
   * Load the menus, run the startup script etc.. only once. So, Use
   * the mChrome Initialized  member to check whether chrome should be 
   * initialized or not - Radha
   */
  if (mChromeLoaded)
    return NS_OK;

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
    docv->GetDocument(*getter_AddRefs(doc));
    if (!doc)
      return NS_OK;

    doc->AddObserver(NS_STATIC_CAST(nsIDocumentObserver*, this));
  }
#endif

#ifdef USE_NATIVE_MENUS
  ///////////////////////////////
  // Find the Menubar DOM  and Load the menus, hooking them up to the loaded commands
  ///////////////////////////////
  nsCOMPtr<nsIDOMDocument> menubarDOMDoc(GetNamedDOMDoc(NS_ConvertASCIItoUCS2("this"))); // XXX "this" is a small kludge for code reused
  if (menubarDOMDoc)
  {
#ifdef SOME_PLATFORM // Anyone using native non-dynamic menus should add themselves here.
    LoadMenus(menubarDOMDoc, mWindow);
    // Context Menu test
    nsCOMPtr<nsIDOMElement> element;
    menubarDOMDoc->GetDocumentElement(getter_AddRefs(element));
    nsCOMPtr<nsIDOMNode> window(do_QueryInterface(element));

    int endCount = 0;
    contextMenuTest = FindNamedDOMNode(nsAutoString("contextmenu"), window, endCount, 1);
    // End Context Menu test
#else
    DynamicLoadMenus(menubarDOMDoc, mWindow);
#endif 
  }
#endif // USE_NATIVE_MENUS

  OnChromeLoaded();
  LoadContentAreas();
  if (mKillScrollbarsAfterLoad)
    KillContentScrollbars();

  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::OnStartURLLoad(nsIDocumentLoader* loader, 
                                 nsIChannel* channel)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnProgressURLLoad(nsIDocumentLoader* loader, 
                                    nsIChannel* channel, 
                                    PRUint32 aProgress, 
                                    PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnStatusURLLoad(nsIDocumentLoader* loader, 
                                  nsIChannel* channel, nsString& aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnEndURLLoad(nsIDocumentLoader* loader, 
                               nsIChannel* channel, nsresult aStatus)
{
  return NS_OK;
}

//----------------------------------------
nsCOMPtr<nsIDOMNode> nsWebShellWindow::FindNamedDOMNode(const nsString &aName, nsIDOMNode * aParent, PRInt32 & aCount, PRInt32 aEndCount)
{
  if(!aParent)
    return nsnull;
    
  nsCOMPtr<nsIDOMNode> node;
  aParent->GetFirstChild(getter_AddRefs(node));
  while (node) {
    nsString name;
    node->GetNodeName(name);
    //printf("FindNamedDOMNode[%s]==[%s] %d == %d\n", aName.ToNewCString(), name.ToNewCString(), aCount+1, aEndCount); //this leaks
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
nsCOMPtr<nsIDOMDocument> nsWebShellWindow::GetNamedDOMDoc(const nsString & aWebShellName)
{
  nsCOMPtr<nsIDOMDocument> domDoc; // result == nsnull;

  // first get the toolbar child docShell
  nsCOMPtr<nsIDocShell> childDocShell;
  if (aWebShellName.EqualsWithConversion("this")) { // XXX small kludge for code reused
    childDocShell = mDocShell;
  } else {
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem;
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(mDocShell));
    docShellAsNode->FindChildWithName(aWebShellName.GetUnicode(), 
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
  docv->GetDocument(*getter_AddRefs(doc));
  if (doc)
    return nsCOMPtr<nsIDOMDocument>(do_QueryInterface(doc));

  return domDoc;
} // nsWebShellWindow::GetNamedDOMDoc

//----------------------------------------

/* copy the window's size and position to the window tag */
void nsWebShellWindow::StoreBoundsToXUL(PRBool aPosition, PRBool aSize, PRBool aSizeMode)
{
   PersistPositionAndSize(aPosition, aSize, aSizeMode);
} // StoreBoundsToXUL


void nsWebShellWindow::KillPersistentSize()
{
   PRBool persistX, persistY;

   mContentTreeOwner->GetPersistence(&persistX, &persistY, nsnull, nsnull, nsnull);
   mContentTreeOwner->SetPersistence(persistX, persistY, PR_FALSE, PR_FALSE, PR_FALSE);
}


// if the main document URL specified URLs for any content areas, start them loading
void nsWebShellWindow::LoadContentAreas() {

  nsAutoString searchSpec;

  // fetch the chrome document URL
  nsCOMPtr<nsIContentViewer> contentViewer;
  mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer) {
    nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(contentViewer);
    if (docViewer) {
      nsCOMPtr<nsIDocument> doc;
      docViewer->GetDocument(*getter_AddRefs(doc));
      nsCOMPtr<nsIURI> mainURL = getter_AddRefs(doc->GetDocumentURL());
      if (mainURL) {
        char *search = nsnull;
        nsCOMPtr<nsIURL> url = do_QueryInterface(mainURL);
        if (url)
          url->GetQuery(&search);
        searchSpec.AssignWithConversion(search);
        nsCRT::free(search);
      }
    }
  }

  // content URLs are specified in the search part of the URL
  // as <contentareaID>=<escapedURL>[;(repeat)]
  if (searchSpec.Length() > 0) {
    PRInt32     begPos,
                eqPos,
                endPos;
    nsString    contentAreaID,
                contentURL;
    char        *urlChar;
    nsIWebShell *contentShell;
    nsresult rv;
    for (endPos = 0; endPos < (PRInt32)searchSpec.Length(); ) {
      // extract contentAreaID and URL substrings
      begPos = endPos;
      eqPos = searchSpec.FindChar('=', PR_FALSE,begPos);
      if (eqPos < 0)
        break;

      endPos = searchSpec.FindChar(';', PR_FALSE,eqPos);
      if (endPos < 0)
        endPos = searchSpec.Length();
      searchSpec.Mid(contentAreaID, begPos, eqPos-begPos);
      searchSpec.Mid(contentURL, eqPos+1, endPos-eqPos-1);
      endPos++;

      // see if we have a webshell with a matching contentAreaID
      rv = GetContentShellById(contentAreaID, &contentShell);
      if (NS_SUCCEEDED(rv)) {
        urlChar = contentURL.ToNewCString();
        if (urlChar) {
          nsUnescape(urlChar);
          contentURL.AssignWithConversion(urlChar);
          nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(contentShell));
          webNav->LoadURI(contentURL.GetUnicode());
          delete [] urlChar;
        }
        NS_RELEASE(contentShell);
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

  nsresult rv;
  nsCOMPtr<nsIScriptGlobalObjectOwner> globalObjectOwner(do_QueryInterface(mWebShell));
  nsCOMPtr<nsIScriptGlobalObject> globalObject;

  if (globalObjectOwner) {
    if (NS_SUCCEEDED(globalObjectOwner->GetScriptGlobalObject(getter_AddRefs(globalObject))) && globalObject) {
      nsCOMPtr<nsIContentViewer> contentViewer;
      if (NS_SUCCEEDED(mDocShell->GetContentViewer(getter_AddRefs(contentViewer)))) {
        nsCOMPtr<nsIDocumentViewer> docViewer;
        nsCOMPtr<nsIPresContext> presContext;
        docViewer = do_QueryInterface(contentViewer);
        if (docViewer && NS_SUCCEEDED(docViewer->GetPresContext(*getter_AddRefs(presContext)))) {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_EVENT;
          event.message = NS_XUL_CLOSE;
          rv = globalObject->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
          if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault)
            return PR_TRUE;
          // else fall through and return PR_FALSE
        }
      }
    }
  }

  return PR_FALSE;
} // ExecuteCloseHandler

//----------------------------------------------------------------
//-- nsIDocumentObserver
//----------------------------------------------------------------
NS_IMETHODIMP
nsWebShellWindow::BeginUpdate(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::EndUpdate(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::BeginLoad(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::EndLoad(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::EndReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK;
}

///////////////////////////////////////////////////////////////
// nsIDocumentObserver
// this is needed for menu changes
///////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsWebShellWindow::ContentChanged(nsIDocument *aDocument,
                                 nsIContent* aContent,
                                 nsISupports* aSubContent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::ContentStatesChanged(nsIDocument *aDocument,
                                       nsIContent* aContent1,
                                       nsIContent* aContent2)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::AttributeChanged(nsIDocument *aDocument,
                                   nsIContent*  aContent,
                                   PRInt32      aNameSpaceID,
                                   nsIAtom*     aAttribute,
                                   PRInt32      aHint)
{
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
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::ContentAppended(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            PRInt32     aNewIndexInContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::ContentInserted(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::ContentReplaced(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aOldChild,
                            nsIContent* aNewChild,
                            PRInt32 aIndexInContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::ContentRemoved(nsIDocument *aDocument,
                           nsIContent* aContainer,
                           nsIContent* aChild,
                           PRInt32 aIndexInContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::StyleSheetAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::StyleSheetRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                           nsIStyleSheet* aStyleSheet,
                                           PRBool aDisabled)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::StyleRuleChanged(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet,
                             nsIStyleRule* aStyleRule,
                             PRInt32 aHint)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::StyleRuleAdded(nsIDocument *aDocument,
                           nsIStyleSheet* aStyleSheet,
                           nsIStyleRule* aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::StyleRuleRemoved(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet,
                             nsIStyleRule* aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  return NS_OK;
}

// This should rightfully be somebody's CONTRACTID?
// Will switch when the "app shell browser component" arrives.
static const char *prefix = "@mozilla.org/appshell/component/browser/window;1";

nsresult
nsWebShellWindow::NotifyObservers( const nsString &aTopic, const nsString &someData ) {
    nsresult rv = NS_OK;
    // Get observer service.
    nsIObserverService *svc = 0;
    rv = nsServiceManager::GetService( NS_OBSERVERSERVICE_CONTRACTID,
                                       NS_GET_IID(nsIObserverService),
                                       (nsISupports**)&svc );
    if ( NS_SUCCEEDED( rv ) && svc ) {
        // Notify observers as instructed; the subject is "this" web shell window.
        nsAutoString topic; topic.AssignWithConversion(prefix);
        topic.AppendWithConversion(";");
        topic += aTopic;
        rv = svc->Notify( (nsIWebShellWindow*)this, topic.GetUnicode(), someData.GetUnicode() );
        // Release the service.
        nsServiceManager::ReleaseService( NS_OBSERVERSERVICE_CONTRACTID, svc );
    } else {
    }
    return rv;
}

// nsIBaseWindow
NS_IMETHODIMP nsWebShellWindow::Destroy()
{
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
      docv->GetDocument(*getter_AddRefs(doc));
      if(doc)
         doc->RemoveObserver(NS_STATIC_CAST(nsIDocumentObserver*, this));
      }
   }
#endif
   
   return nsXULWindow::Destroy();
}

////////////////////////////////////////////////////////////////////////////////
// XXX What a mess we have here w.r.t. our prompting interfaces. As far as I
// can tell, the situation looks something like this:
//
//      - clients get the nsIPrompt from the web shell window
//      - the web shell window passes control to nsCommonDialogs
//      - nsCommonDialogs calls into js with the current dom window
//      - the dom window gets the nsIPrompt of its tree owner
//      - somewhere along the way a real dialog comes up
// 
// This little transducer maps the nsIPrompt interface to the nsICommonDialogs
// interface. Ideally, nsIPrompt would be implemented by nsIDOMWindowInternal which 
// would eliminate the need for this.

class nsDOMWindowPrompter : public nsIPrompt
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROMPT

  nsDOMWindowPrompter(nsIDOMWindowInternal* window);
  virtual ~nsDOMWindowPrompter() {}

  nsresult Init();

protected:
  nsCOMPtr<nsIDOMWindowInternal>        mDOMWindow;
  nsCOMPtr<nsICommonDialogs>    mCommonDialogs;

  nsresult GetLocaleString(const PRUnichar*, PRUnichar**);
};

static nsresult
NS_NewDOMWindowPrompter(nsIPrompt* *result, nsIDOMWindowInternal* window)
{
  nsresult rv;
  nsDOMWindowPrompter* prompter = new nsDOMWindowPrompter(window);
  if (prompter == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(prompter);
  rv = prompter->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(prompter);
    return rv;
  }
  *result = prompter;
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDOMWindowPrompter, nsIPrompt)

nsDOMWindowPrompter::nsDOMWindowPrompter(nsIDOMWindowInternal* window)
  : mDOMWindow(window)
{
  NS_INIT_REFCNT();
}

nsresult
nsDOMWindowPrompter::Init()
{
  nsresult rv;
  mCommonDialogs = do_GetService(kCommonDialogsCID, &rv);
  return rv;
}

nsresult
nsDOMWindowPrompter::GetLocaleString(const PRUnichar* aString, PRUnichar** aResult)
{
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> stringService = do_GetService(kStringBundleServiceCID);
  nsCOMPtr<nsIStringBundle> stringBundle;
  nsILocale *locale = nsnull;
 
  rv = stringService->CreateBundle(kWebShellLocaleProperties, locale, getter_AddRefs(stringBundle));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  rv = stringBundle->GetStringFromName(aString, aResult);

  return rv;
}

NS_IMETHODIMP
nsDOMWindowPrompter::Alert(const PRUnichar* dialogTitle, 
                           const PRUnichar* text)
{
  nsresult rv;
 
  if (dialogTitle == nsnull) {
    PRUnichar *title;
    rv = GetLocaleString(NS_ConvertASCIItoUCS2("Alert").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    rv = mCommonDialogs->Alert(mDOMWindow, title, text);
    nsCRT::free(title);
    title = nsnull;
  }
  else {
    rv = mCommonDialogs->Alert(mDOMWindow, dialogTitle, text);
  }
  
  return rv; 
}

NS_IMETHODIMP
nsDOMWindowPrompter::AlertCheck(const PRUnichar* dialogTitle, 
                                  const PRUnichar* text,
                                  const PRUnichar* checkMsg,
                                  PRBool *checkValue)
{
  nsresult rv;

  if (dialogTitle == nsnull) {
    PRUnichar *title;
    rv = GetLocaleString(NS_ConvertASCIItoUCS2("Alert"), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    rv = mCommonDialogs->AlertCheck(mDOMWindow, title, text, checkMsg, checkValue);
    nsCRT::free(title);
    title = nsnull;
  }
  else {
    rv = mCommonDialogs->AlertCheck(mDOMWindow, dialogTitle, text, checkMsg, checkValue);
  }

  return rv;  
}

NS_IMETHODIMP
nsDOMWindowPrompter::Confirm(const PRUnichar* dialogTitle, 
                             const PRUnichar* text,
                             PRBool *_retval)
{
  nsresult rv;
  if (dialogTitle == nsnull) {
    PRUnichar *title;
    rv = GetLocaleString(NS_ConvertASCIItoUCS2("Confirm").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    rv = mCommonDialogs->Confirm(mDOMWindow, title, text, _retval);
    nsCRT::free(title);
    title = nsnull;
  }  
  else {
    rv = mCommonDialogs->Confirm(mDOMWindow, dialogTitle, text, _retval);
  }

  return rv; 
}

NS_IMETHODIMP
nsDOMWindowPrompter::ConfirmCheck(const PRUnichar* dialogTitle, 
                                  const PRUnichar* text,
                                  const PRUnichar* checkMsg,
                                  PRBool *checkValue,
                                  PRBool *_retval)
{
  nsresult rv;

  if (dialogTitle == nsnull) {
    PRUnichar *title;
    rv = GetLocaleString(NS_ConvertASCIItoUCS2("ConfirmCheck").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    rv = mCommonDialogs->ConfirmCheck(mDOMWindow, title, text, checkMsg, checkValue, _retval);
    nsCRT::free(title);
    title = nsnull;
  }
  else {
    rv = mCommonDialogs->ConfirmCheck(mDOMWindow, dialogTitle, text, checkMsg, checkValue, _retval);
  }

  return rv;  
}

NS_IMETHODIMP
nsDOMWindowPrompter::Prompt(const PRUnichar* dialogTitle,
                            const PRUnichar* text,
                            const PRUnichar* passwordRealm,
                            PRUint32 savePassword,
                            const PRUnichar* defaultText,
                            PRUnichar* *result,
                            PRBool *_retval)
{
  nsresult rv;

  if (dialogTitle == nsnull) {
    PRUnichar *title;
    rv = GetLocaleString(NS_ConvertASCIItoUCS2("Prompt").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    rv = mCommonDialogs->Prompt(mDOMWindow, title, text,
                                defaultText, result, _retval);
    nsCRT::free(title);
    title = nsnull;
  }
  else {
    rv = mCommonDialogs->Prompt(mDOMWindow, dialogTitle, text,
                                defaultText, result, _retval);
  }

  return rv; 
}

NS_IMETHODIMP
nsDOMWindowPrompter::PromptUsernameAndPassword(const PRUnichar* dialogTitle, 
                                               const PRUnichar* text,
                                               const PRUnichar* passwordRealm,
                                               PRUint32 savePassword,
                                               PRUnichar* *user,
                                               PRUnichar* *pwd,
                                               PRBool *_retval)
{	
  nsresult rv;
  
  if (dialogTitle == nsnull) {
    PRUnichar *title;
    rv = GetLocaleString(NS_ConvertASCIItoUCS2("PromptUsernameAndPassword").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    rv = mCommonDialogs->PromptUsernameAndPassword(mDOMWindow, title, text,
                                                   user, pwd, _retval);
    nsCRT::free(title);
    title = nsnull;
  }
  else {
    rv = mCommonDialogs->PromptUsernameAndPassword(mDOMWindow, dialogTitle, text,
                                                   user, pwd, _retval);
  }    
  
  return rv;
}

NS_IMETHODIMP
nsDOMWindowPrompter::PromptPassword(const PRUnichar* dialogTitle, 
                                    const PRUnichar* text,
                                    const PRUnichar* passwordRealm,
                                    PRUint32 savePassword,
                                    PRUnichar* *pwd,
                                    PRBool *_retval)
{
  // ignore passwordRealm and savePassword here?
  nsresult rv;

  if (dialogTitle == nsnull) {
    PRUnichar *title;
    rv = GetLocaleString(NS_ConvertASCIItoUCS2("PromptPassword").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    rv = mCommonDialogs->PromptPassword(mDOMWindow, title, text, 
                                        pwd, _retval);

    nsCRT::free(title);
    title = nsnull;
  }
  else {
    rv = mCommonDialogs->PromptPassword(mDOMWindow, dialogTitle, text,
                                        pwd, _retval);
  }

  return rv;
}

NS_IMETHODIMP
nsDOMWindowPrompter::Select(const PRUnichar *dialogTitle,
                            const PRUnichar* inMsg,
                            PRUint32 inCount, 
                            const PRUnichar **inList,
                            PRInt32 *outSelection,
                            PRBool *_retval)
{
  nsresult rv; 

  if (dialogTitle == nsnull) {
    PRUnichar *title;
    rv = GetLocaleString(NS_ConvertASCIItoUCS2("Select").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    rv = mCommonDialogs->Select(mDOMWindow, title, inMsg, 
                                inCount, inList, outSelection, _retval);
    nsCRT::free(title);
    title = nsnull;
  }
  else {
    rv = mCommonDialogs->Select(mDOMWindow, dialogTitle, inMsg,
                                inCount, inList, outSelection, _retval);
  }

  return rv;
}

NS_IMETHODIMP
nsDOMWindowPrompter::UniversalDialog(const PRUnichar *inTitleMessage,
                                     const PRUnichar *inDialogTitle, /* e.g., alert, confirm, prompt, prompt password */
                                     const PRUnichar *inMsg, /* main message for dialog */
                                     const PRUnichar *inCheckboxMsg, /* message for checkbox */
                                     const PRUnichar *inButton0Text, /* text for first button */
                                     const PRUnichar *inButton1Text, /* text for second button */
                                     const PRUnichar *inButton2Text, /* text for third button */
                                     const PRUnichar *inButton3Text, /* text for fourth button */
                                     const PRUnichar *inEditfield1Msg, /*message for first edit field */
                                     const PRUnichar *inEditfield2Msg, /* message for second edit field */
                                     PRUnichar **inoutEditfield1Value, /* initial and final value for first edit field */
                                     PRUnichar **inoutEditfield2Value, /* initial and final value for second edit field */
                                     const PRUnichar *inIConURL, /* url of icon to be displayed in dialog */
                                     /* examples are
                                        "chrome://global/skin/question-icon.gif" for question mark,
                                        "chrome://global/skin/alert-icon.gif" for exclamation mark
                                     */
                                     PRBool *inoutCheckboxState, /* initial and final state of check box */
                                     PRInt32 inNumberButtons, /* total number of buttons (0 to 4) */
                                     PRInt32 inNumberEditfields, /* total number of edit fields (0 to 2) */
                                     PRInt32 inEditField1Password, /* is first edit field a password field */
                                     PRInt32 *outButtonPressed) /* number of button that was pressed (0 to 3) */
{
  nsresult rv;
  NS_ASSERTION(inDialogTitle, "UniversalDialog must have a dialog title supplied");
  rv = mCommonDialogs->UniversalDialog(mDOMWindow, 
                                       inTitleMessage,
                                       inDialogTitle,
                                       inMsg,
                                       inCheckboxMsg,
                                       inButton0Text,
                                       inButton1Text,
                                       inButton2Text,
                                       inButton3Text,
                                       inEditfield1Msg,
                                       inEditfield2Msg,
                                       inoutEditfield1Value,
                                       inoutEditfield2Value,
                                       inIConURL,
                                       inoutCheckboxState,
                                       inNumberButtons,
                                       inNumberEditfields,
                                       inEditField1Password,
                                       outButtonPressed);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWebShellWindow::GetPrompter(nsIPrompt* *result)
{
  nsresult rv;
  if (mPrompter == nsnull) {
    nsIWebShell* tempWebShell;
    GetWebShell(tempWebShell);
    nsCOMPtr<nsIWebShell> webShell(dont_AddRef(tempWebShell));
    nsCOMPtr<nsIDOMWindowInternal> domWindow;
    rv = ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow));
    if (NS_FAILED(rv)) {
      NS_ERROR("Unable to retrieve the DOM window from the new web shell.");
      return rv;
    }
    nsCOMPtr<nsIPrompt> prompt;
    rv = NS_NewDOMWindowPrompter(getter_AddRefs(prompt), domWindow);
    if (NS_FAILED(rv)) return rv;

    // wrap the nsDOMWindowPrompter in a nsISingleSignOnPrompt:
    nsCOMPtr<nsISingleSignOnPrompt> siPrompt = do_CreateInstance(kSingleSignOnPromptCID, &rv);
    if (NS_SUCCEEDED(rv)) {
      // then single sign-on is installed
      rv = siPrompt->Init(prompt);
      if (NS_FAILED(rv)) return rv;
      mPrompter = siPrompt;
    }
    else {
      // if single sign-on isn't installed, just use the DOM window prompter directly
      mPrompter = prompt;
    }
  }
  *result = mPrompter; 
  NS_ADDREF(*result);
  return NS_OK;
}
