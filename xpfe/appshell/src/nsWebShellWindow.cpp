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


#include "nsWebShellWindow.h"

#include "nsLayoutCID.h"
#include "nsIWeakReference.h"
#include "nsIDocumentLoader.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsIIOService.h"
#include "nsIURL.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO
#include "nsIPref.h"

#include "nsINameSpaceManager.h"
#include "nsEscape.h"
#include "nsVoidArray.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMFocusListener.h"

#include "nsIXULParentDocument.h"
#include "nsIXULPopupListener.h"
#include "nsIXULChildDocument.h"
#include "nsIDOMXULElement.h"
#include "nsRDFCID.h"

#include "nsGUIEvent.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIAppShell.h"
#include "nsIXULWindowCallbacks.h"

#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

#include "nsXULCommand.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNodeList.h"

#include "nsIMenuBar.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsIContextMenu.h"

// For JS Execution
#include "nsIScriptContextOwner.h"

#include "nsIEventQueueService.h"
#include "plevent.h"
#include "prmem.h"

#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDocumentLoader.h"
#include "nsIObserverService.h"
#include "prprf.h"
//#include "nsIDOMHTMLInputElement.h"
//#include "nsIDOMHTMLImageElement.h"

#include "nsIContent.h" // for menus

// For calculating size
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"

// HACK for M4, should be removed by M5
#ifdef XP_MAC
#include <Menus.h>
#endif
#include "nsIMenuItem.h"
#include "nsIDOMXULDocument.h"
// End hack

#include "nsIWindowMediator.h"

// This is to bring up a MsgCompose window when a mailto link is clicked.
// This is a temporary hack for M8 until NECKO lands when I can use their
// Protocol registry
#include "nsIDOMToolkitCore.h"
#include "nsAppCoresCIDs.h"

#include "nsIPopupSetFrame.h"

/* Define Class IDs */
static NS_DEFINE_IID(kWindowCID,           NS_WINDOW_CID);
static NS_DEFINE_IID(kWebShellCID,         NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kAppShellServiceCID,  NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kAppShellCID,         NS_APPSHELL_CID);

#include "nsWidgetsCID.h"
static NS_DEFINE_IID(kMenuBarCID,          NS_MENUBAR_CID);
static NS_DEFINE_IID(kMenuCID,             NS_MENU_CID);
static NS_DEFINE_IID(kMenuItemCID,         NS_MENUITEM_CID);
static NS_DEFINE_IID(kContextMenuCID,      NS_CONTEXTMENU_CID);

static NS_DEFINE_CID(kPrefCID,             NS_PREF_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kToolkitCoreCID, NS_TOOLKITCORE_CID);



static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

static NS_DEFINE_IID(kIDocumentLoaderFactoryIID, NS_IDOCUMENTLOADERFACTORY_IID);
static NS_DEFINE_CID(kLayoutDocumentLoaderFactoryCID, NS_LAYOUT_DOCUMENT_LOADER_FACTORY_CID);

#ifndef NECKO
#ifdef MOZ_MAIL_NEWS
static NS_DEFINE_CID(kMsgComposeServiceCID, NS_MSGCOMPOSESERVICE_CID);
#endif /* MOZ_MAIL_NEWS */
#endif   /* NECKO  */


/* Define Interface IDs */
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellWindowIID,     NS_IWEBSHELL_WINDOW_IID);
static NS_DEFINE_IID(kIWidgetIID,             NS_IWIDGET_IID);
static NS_DEFINE_IID(kIWebShellIID,           NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWebShellContainerIID,  NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIBrowserWindowIID,      NS_IBROWSER_WINDOW_IID);

static NS_DEFINE_IID(kIAppShellServiceIID,    NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kIAppShellIID,           NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIDocumentLoaderObserverIID, NS_IDOCUMENT_LOADER_OBSERVER_IID);
static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kIDOMDocumentIID,        NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMCharacterDataIID,   NS_IDOMCHARACTERDATA_IID);
//static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
//static NS_DEFINE_IID(kIDOMHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);

static NS_DEFINE_IID(kIMenuIID,        NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID,     NS_IMENUBAR_IID);
static NS_DEFINE_IID(kIMenuItemIID,    NS_IMENUITEM_IID);
static NS_DEFINE_IID(kIContextMenuIID, NS_ICONTEXTMENU_IID);
static NS_DEFINE_IID(kIXULCommandIID,  NS_IXULCOMMAND_IID);
static NS_DEFINE_IID(kIContentIID,     NS_ICONTENT_IID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

static NS_DEFINE_IID(kIWindowMediatorIID,NS_IWINDOWMEDIATOR_IID);

static NS_DEFINE_IID(kIXULPopupListenerIID, NS_IXULPOPUPLISTENER_IID);
static NS_DEFINE_CID(kXULPopupListenerCID, NS_XULPOPUPLISTENER_CID);
static NS_DEFINE_IID(kIUrlDispatcherIID,     NS_IURLDISPATCHER_IID);

#ifdef DEBUG_rods
#define DEBUG_MENUSDEL 1
#endif
#include "nsICommonDialogs.h"
static NS_DEFINE_CID(	kCommonDialogsCID, NS_CommonDialog_CID );
static NS_DEFINE_IID( kIPromptIID, NS_IPROMPT_IID );
#include "nsIWebShell.h"

const char * kThrobberOnStr  = "resource:/res/throbber/anims07.gif";
const char * kThrobberOffStr = "resource:/res/throbber/anims00.gif";
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

nsWebShellWindow::nsWebShellWindow()
{
  NS_INIT_REFCNT();

  mWebShell = nsnull;
  mWindow   = nsnull;
  mCallbacks = nsnull;
  mContinueModalLoop = PR_FALSE;
  mChromeInitialized = PR_FALSE;
  mLockedUntilChromeLoad = PR_FALSE;
  mContentShells = nsnull;
  mChromeMask = NS_CHROME_ALL_CHROME;
  mIntrinsicallySized = PR_FALSE;
  mCreatedVisible = PR_TRUE;
  mLoadDefaultPage = PR_TRUE;
}


nsWebShellWindow::~nsWebShellWindow()
{
  if (nsnull != mWebShell) {
    mWebShell->Destroy();
    NS_RELEASE(mWebShell);
  }

  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mCallbacks);

  // Delete any remaining content shells.
  PRInt32 count;
  if (mContentShells && ((count = mContentShells->Count()) > 0)) {
    for (PRInt32 i = 0; i < count; i++)
    {
      nsWebShellInfo* webInfo = (nsWebShellInfo*)(mContentShells->ElementAt(i));
      delete webInfo;
    }

    delete mContentShells;
  }

}

NS_IMETHODIMP_(nsrefcnt) nsWebShellWindow::AddRef(void)
{
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");
  ++mRefCnt;
  return mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt) nsWebShellWindow::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  --mRefCnt;
  if (mRefCnt == 0) {
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

nsresult
nsWebShellWindow::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if ( aIID.Equals(kIWebShellWindowIID) ) {
    *aInstancePtr = (void*) ((nsIWebShellWindow*)this);
    NS_ADDREF_THIS();  
    return NS_OK;
  }
  if (aIID.Equals(kIWebShellContainerIID)) {
    *aInstancePtr = (void*)(nsIWebShellContainer*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDocumentLoaderObserverIID)) {
    *aInstancePtr = (void*) ((nsIDocumentLoaderObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIBrowserWindowIID)) {
    *aInstancePtr = (void*) (nsIBrowserWindow*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIUrlDispatcherIID)) {
     *aInstancePtr = (void*) (nsIUrlDispatcher*) this;
     NS_ADDREF_THIS();
     return NS_OK;
  }	
  if (aIID.Equals(kIPromptIID )) {
    *aInstancePtr = (void*)(nsIPrompt*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIModalWindowSupport::GetIID())) {
     *aInstancePtr = (void*)NS_STATIC_CAST(nsIModalWindowSupport*, this);
     NS_ADDREF_THIS();
     return NS_OK;
  }
  if (aIID.Equals(nsISupportsWeakReference::GetIID())) {
    *aInstancePtr = (void*)NS_STATIC_CAST(nsISupportsWeakReference *, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIWebShellContainer*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return rv;
}


nsresult nsWebShellWindow::Initialize(nsIWebShellWindow* aParent,
                                      nsIAppShell* aShell, nsIURI* aUrl, 
                                      PRBool aCreatedVisible,
                                      PRBool aLoadDefaultPage,
                                      nsIStreamObserver* anObserver,
                                      nsIXULWindowCallbacks *aCallbacks,
                                      PRInt32 aInitialWidth, PRInt32 aInitialHeight,
                                      nsWidgetInitData& widgetInitData)
{
  nsresult rv;
  nsCOMPtr<nsIWidget> parentWidget;

  mCreatedVisible = aCreatedVisible;
  mLoadDefaultPage = aLoadDefaultPage;
  
  // XXX: need to get the default window size from prefs...
  // Doesn't come from prefs... will come from CSS/XUL/RDF
  nsRect r(0, 0, aInitialWidth, aInitialHeight);
  
  // Create top level window
  rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID,
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
  if (aParent) {
    aParent->GetWidget(*getter_AddRefs(parentWidget));
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
  rv = nsComponentManager::CreateInstance(kWebShellCID, nsnull,
                                    kIWebShellIID,
                                    (void**)&mWebShell);
  if (NS_OK != rv) {
    return rv;
  }

  r.x = r.y = 0;
  rv = mWebShell->Init(mWindow->GetNativeData(NS_NATIVE_WIDGET), 
                       r.x, r.y, r.width, r.height,
                       nsScrollPreference_kNeverScroll, 
                       PR_TRUE,                     // Allow Plugins 
                       PR_TRUE);
  mWebShell->SetContainer(this);
  mWebShell->SetObserver((nsIStreamObserver*)anObserver);
  mWebShell->SetDocLoaderObserver(this);

  // The outermost web shell is always considered to be chrome.
  mWebShell->SetWebShellType(nsWebShellChrome);

  /*
   * XXX:  How should preferences be supplied to the nsWebShellWindow?
   *       Should there be the notion of a global preferences service?
   *       Or should there be many preferences components based on 
   *       the user profile...
   */
  // Initialize the webshell with the preferences service
  nsIPref *prefs;

  rv = nsServiceManager::GetService(kPrefCID, 
                                    nsIPref::GetIID(), 
                                    (nsISupports **)&prefs);
  if (NS_SUCCEEDED(rv)) {
    // Set the prefs in the outermost webshell.
    mWebShell->SetPrefs(prefs);
    nsServiceManager::ReleaseService(kPrefCID, prefs);
  }

  NS_IF_RELEASE(mCallbacks);
  mCallbacks = aCallbacks;
  NS_IF_ADDREF(mCallbacks);

  if (nsnull != aUrl)  {
#ifdef NECKO
    char *tmpStr = NULL;
#else
    const char *tmpStr = NULL;
#endif
    nsString urlString;

    aUrl->GetSpec(&tmpStr);
    urlString = tmpStr;
#ifdef NECKO
    nsCRT::free(tmpStr);
#endif
    mWebShell->LoadURL(urlString.GetUnicode());
  }
                     
  return rv;
}


/*
 * Close the window
 */
NS_METHOD
nsWebShellWindow::Close()
{
  ExitModalLoop();
  if (mWebShell) {
    mWebShell->Destroy();
    NS_RELEASE(mWebShell);
  }

  NS_IF_RELEASE(mWindow);
  nsIAppShellService* appShell;
  nsresult rv = nsServiceManager::GetService(kAppShellServiceCID,
                                  kIAppShellServiceIID,
                                  (nsISupports**)&appShell);
  if (NS_FAILED(rv))
    return rv;
  
  rv = appShell->UnregisterTopLevelWindow(this);
  nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);

  return rv;
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
      case NS_SIZE: {
        nsSizeEvent* sizeEvent = (nsSizeEvent*)aEvent;  
        webShell->SetBounds(0, 0, sizeEvent->windowSize->width, sizeEvent->windowSize->height);
        result = nsEventStatus_eConsumeNoDefault;
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

      case NS_GOTFOCUS: {
        void* data;
        aEvent->widget->GetClientData(data);
        if (data) {
          nsCOMPtr<nsIDOMWindow> domWindow;
          nsCOMPtr<nsIWebShell> contentShell;
          ((nsWebShellWindow *)data)->GetContentWebShell(getter_AddRefs(contentShell));
          if (contentShell) {
            
            if (NS_SUCCEEDED(((nsWebShellWindow *)data)->
                ConvertWebShellToDOMWindow(contentShell, getter_AddRefs(domWindow)))) {
              domWindow->Focus();
            }
          }
          else if (webShell && NS_SUCCEEDED(((nsWebShellWindow *)data)->
                ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow)))) {
              domWindow->Focus();
          }
        }
      }
      default:
        break;

    }
  }
  return nsEventStatus_eIgnore;
}


NS_IMETHODIMP 
nsWebShellWindow::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL,
                              nsLoadType aReason)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
  // If loading a new root .xul document, then redo chrome.
  if (aShell == mWebShell) {
      mChromeInitialized = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL,
                                  PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::EndLoadURL(nsIWebShell* aWebShell, const PRUnichar* aURL,
                             nsresult aStatus)
{
  return NS_OK;
}



//----------------------------------------
nsCOMPtr<nsIDOMNode> nsWebShellWindow::FindNamedParentFromDoc(nsIDOMDocument * aDomDoc, const nsString &aName) 
{
  nsCOMPtr<nsIDOMNode> node; // result.
  nsCOMPtr<nsIDOMElement> element;
  aDomDoc->GetDocumentElement(getter_AddRefs(element));
  if (!element)
    return node;

  nsCOMPtr<nsIDOMNode> parent(do_QueryInterface(element));
  if (!parent)
    return node;

  parent->GetFirstChild(getter_AddRefs(node));
  while (node) {
    nsString name;
    node->GetNodeName(name);

#ifdef DEBUG_rods
    printf("Looking for [%s] [%s]\n", aName.ToNewCString(), name.ToNewCString()); // this leaks
#endif

    if (name.Equals(aName))
      return node;
    nsCOMPtr<nsIDOMNode> oldNode(node);
    oldNode->GetNextSibling(getter_AddRefs(node));
  }
  node = do_QueryInterface(nsnull);
  return node;
}


//----------------------------------------
NS_IMETHODIMP nsWebShellWindow::CreateMenu(nsIMenuBar * aMenuBar, 
                                           nsIDOMNode * aMenuNode, 
                                           nsString   & aMenuName) 
{
  // Create nsMenu
  nsIMenu * pnsMenu = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID, nsnull, kIMenuIID, (void**)&pnsMenu);
  if (NS_OK == rv) {
    // Call Create
    nsISupports * supports = nsnull;
    aMenuBar->QueryInterface(kISupportsIID, (void**) &supports);
    pnsMenu->Create(supports, aMenuName);
    NS_RELEASE(supports);

    // Set nsMenu Name
    pnsMenu->SetLabel(aMenuName); 
    // Make nsMenu a child of nsMenuBar
    aMenuBar->AddMenu(pnsMenu); 

    // Open the node so that the contents are visible.
    nsCOMPtr<nsIDOMElement> menuElement = do_QueryInterface(aMenuNode);
    if (menuElement)
      menuElement->SetAttribute("open", "true");

    // Begin menuitem inner loop
    
    // Now get the kids. Retrieve our menupopup child.
    nsCOMPtr<nsIDOMNode> menuPopupNode;
    aMenuNode->GetFirstChild(getter_AddRefs(menuPopupNode));
    while (menuPopupNode) {
      nsCOMPtr<nsIDOMElement> menuPopupElement(do_QueryInterface(menuPopupNode));
      if (menuPopupElement) {
        nsString menuPopupNodeType;
        menuPopupElement->GetNodeName(menuPopupNodeType);
        if (menuPopupNodeType.Equals("menupopup"))
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
        if (menuitemNodeType.Equals("menuitem")) {
          // LoadMenuItem
          LoadMenuItem(pnsMenu, menuitemElement, menuitemNode);
        } else if (menuitemNodeType.Equals("menuseparator")) {
          pnsMenu->AddSeparator();
        } else if (menuitemNodeType.Equals("menu")) {
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

  menuitemElement->GetAttribute(nsAutoString("value"), menuitemName);
  menuitemElement->GetAttribute(nsAutoString("cmd"), menuitemCmd);
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull, kIMenuItemIID, (void**)&pnsMenuItem);
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
    nsAutoString keyAtom("key");
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
  
    nsIDOMElement * keyElement = nsnull;
    xulDocument->GetElementById(keyValue, &keyElement);
    
    if(keyElement){
        PRUint8 modifiers = knsMenuItemNoModifier;
	    nsAutoString shiftAtom("shift");
	    nsAutoString altAtom("alt");
	    nsAutoString commandAtom("command");
	    nsString shiftValue;
	    nsString altValue;
	    nsString commandValue;
	    nsString keyChar = " ";
	    
	    keyElement->GetAttribute(keyAtom, keyChar);
	    keyElement->GetAttribute(shiftAtom, shiftValue);
	    keyElement->GetAttribute(altAtom, altValue);
	    keyElement->GetAttribute(commandAtom, commandValue);
	    
	    if(keyChar != " ")
	      pnsMenuItem->SetShortcutChar(keyChar);
	      
	    if(shiftValue == "true")
	      modifiers |= knsMenuItemShiftModifier;
	    
	    if(altValue == "true")
	      modifiers |= knsMenuItemAltModifier;
	    
	    if(commandValue == "false")
	     modifiers |= knsMenuItemCommandModifier;
	      
        pnsMenuItem->SetModifiers(modifiers);
    }
    
    // Make nsMenuItem a child of nsMenu
    nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(kISupportsIID, (void**) &supports);
    pParentMenu->AddItem(supports);
    NS_RELEASE(supports);
          


    nsAutoString cmdAtom("onaction");
    nsString cmdName;

    domElement->GetAttribute(cmdAtom, cmdName);

    nsXULCommand * menuDelegate = new nsXULCommand();
    if ( menuDelegate ) {
        menuDelegate->SetCommand(cmdName);
        menuDelegate->SetWebShell(mWebShell);
        menuDelegate->SetDOMElement(domElement);
        menuDelegate->SetMenuItem(pnsMenuItem);
    } else {
        NS_RELEASE( pnsMenuItem );
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    nsIXULCommand * icmd;
    if (NS_OK == menuDelegate->QueryInterface(kIXULCommandIID, (void**) &icmd)) {
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
  menuElement->GetAttribute(nsAutoString("value"), menuName);
  //printf("Creating Menu [%s] \n", menuName.ToNewCString()); // this leaks

  // Create nsMenu
  nsIMenu * pnsMenu = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID, nsnull, kIMenuIID, (void**)&pnsMenu);
  if (NS_OK == rv) {
    // Call Create
    nsISupports * supports = nsnull;
    pParentMenu->QueryInterface(kISupportsIID, (void**) &supports);
    pnsMenu->Create(supports, menuName);
    NS_RELEASE(supports); // Balance QI

    // Open the node so that the contents are visible.
    menuElement->SetAttribute("open", "true");
      
    // Set nsMenu Name
    pnsMenu->SetLabel(menuName); 
    // Make nsMenu a child of parent nsMenu
    //pParentMenu->AddMenu(pnsMenu);
    supports = nsnull;
    pnsMenu->QueryInterface(kISupportsIID, (void**) &supports);
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
        if (menuPopupNodeType.Equals("menupopup"))
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

        if (menuitemNodeType.Equals("menuitem")) {
          // Load a menuitem
          LoadMenuItem(pnsMenu, menuitemElement, menuitemNode);
        } else if (menuitemNodeType.Equals("menuseparator")) {
          pnsMenu->AddSeparator();
        } else if (menuitemNodeType.Equals("menu")) {
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
  nsCOMPtr<nsIDOMNode> menubarNode(FindNamedDOMNode(nsAutoString("menubar"), window, endCount, 1));
  if (menubarNode) {
    nsIMenuBar * pnsMenuBar = nsnull;
    rv = nsComponentManager::CreateInstance(kMenuBarCID, nsnull, kIMenuBarIID, (void**)&pnsMenuBar);
    if (NS_OK == rv) {
      if (nsnull != pnsMenuBar) {      
        // set pnsMenuBar as a nsMenuListener on aParentWindow
        nsCOMPtr<nsIMenuListener> menuListener;
        pnsMenuBar->QueryInterface(kIMenuListenerIID, getter_AddRefs(menuListener));

        //fake event
        nsMenuEvent fake;
        menuListener->MenuConstruct(fake, aParentWindow, menubarNode, mWebShell);

      #ifdef XP_MAC
      #else
      // Resize around the menu.
      rv = NS_ERROR_FAILURE;

      // do a resize
      nsCOMPtr<nsIContentViewerContainer> contentViewerContainer;
      contentViewerContainer = do_QueryInterface(mWebShell);
      if (!contentViewerContainer) {
          NS_ERROR("Webshell doesn't support the content viewer container interface");
          return;
      }

      nsCOMPtr<nsIContentViewer> contentViewer;
      if (NS_FAILED(rv = contentViewerContainer->GetContentViewer(getter_AddRefs(contentViewer)))) {
          NS_ERROR("Unable to retrieve content viewer.");
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
      nsRect currentBounds;
      GetWindowBounds(currentBounds);
      SizeWindowTo(currentBounds.width, currentBounds.height + heightDelta);
      // END REFLOW CODE
      #endif
                  
      } // end if ( nsnull != pnsMenuBar )
    }
  } // end if (menuBar)
} // nsWebShellWindow::DynamicLoadMenus

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
  nsCOMPtr<nsIDOMNode> menubarNode(FindNamedDOMNode(nsAutoString("menubar"), window, endCount, 1));
  if (menubarNode) {
    nsIMenuBar * pnsMenuBar = nsnull;
    rv = nsComponentManager::CreateInstance(kMenuBarCID, nsnull, kIMenuBarIID, (void**)&pnsMenuBar);
    if (NS_OK == rv) {
      if (nsnull != pnsMenuBar) {
        pnsMenuBar->Create(aParentWindow);
      
        // set pnsMenuBar as a nsMenuListener on aParentWindow
        nsCOMPtr<nsIMenuListener> menuListener;
        pnsMenuBar->QueryInterface(kIMenuListenerIID, getter_AddRefs(menuListener));
        aParentWindow->AddMenuListener(menuListener);

        nsCOMPtr<nsIDOMNode> menuNode;
        menubarNode->GetFirstChild(getter_AddRefs(menuNode));
        while (menuNode) {
          nsCOMPtr<nsIDOMElement> menuElement(do_QueryInterface(menuNode));
          if (menuElement) {
            nsString menuNodeType;
            nsString menuName;
            menuElement->GetNodeName(menuNodeType);
            if (menuNodeType.Equals("menu")) {
              menuElement->GetAttribute(nsAutoString("value"), menuName);

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
#ifdef XP_MAC
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
    nsresult rv = nsComponentManager::CreateInstance(kContextMenuCID, nsnull, kIContextMenuIID, (void**)&pnsContextMenu);
    if (NS_SUCCEEDED(rv) && pnsContextMenu) {
        nsISupports * supports;
        aParentWindow->QueryInterface(kISupportsIID, (void**) &supports);
        pnsContextMenu->Create(supports, aPopupAlignment, aAnchorAlignment);
        NS_RELEASE(supports);
        pnsContextMenu->SetLocation(aX,aY);
        // Set webshell
        pnsContextMenu->SetWebShell( mWebShell );
        
        // Set DOM node
        pnsContextMenu->SetDOMNode( aMenuNode );
        
        // Construct and show menu
        nsIMenuListener * listener;
        pnsContextMenu->QueryInterface(kIMenuListenerIID, (void**) &listener);
        
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

//------------------------------------------------------------------------------
NS_IMETHODIMP
nsWebShellWindow::GetContentShellById(const nsString& aID, nsIWebShell** aChildShell)
{
	// Set to null just to be certain
  *aChildShell = nsnull;

  // If we don't have a content array, we just don't care.
  if (mContentShells == nsnull)
    return NS_ERROR_FAILURE;

  // Find out if the id in question is one that we have web shell info for.
  PRInt32 count = mContentShells->Count();
  for (PRInt32 i = 0; i < count; i++)
  {
    nsWebShellInfo* webInfo = (nsWebShellInfo*)(mContentShells->ElementAt(i));
    if (webInfo->id == aID)
    {
      // We have a match!
      *aChildShell = webInfo->child;
      NS_ADDREF(*aChildShell);

      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

//------------------------------------------------------------------------------
NS_IMETHODIMP
nsWebShellWindow::AddWebShellInfo(const nsString& aID,
                                  PRBool aPrimary,
                                  nsIWebShell* aChildShell)
{

  nsWebShellInfo* webShellInfo = new nsWebShellInfo(aID, aPrimary,
                                                    aChildShell);
  
  if (mContentShells == nsnull)
    mContentShells = new nsVoidArray();

  mContentShells->AppendElement((void*)webShellInfo);

  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::ConvertWebShellToDOMWindow(nsIWebShell* aShell, nsIDOMWindow** aDOMWindow)
{
  nsresult rv;
  nsCOMPtr<nsIScriptContextOwner> newContextOwner;
  nsCOMPtr<nsIScriptGlobalObject> newGlobalObject;
  nsCOMPtr<nsIDOMWindow> newDOMWindow;

  newContextOwner = do_QueryInterface(aShell);
  if (newContextOwner)
  {
    if (NS_FAILED(rv = newContextOwner->GetScriptGlobalObject(getter_AddRefs(newGlobalObject)))) {
      NS_ERROR("Unable to retrieve global object.");
      return rv;
    }
    
    if (newGlobalObject) {
      newDOMWindow = do_QueryInterface(newGlobalObject);
      *aDOMWindow = newDOMWindow.get();
      NS_ADDREF(*aDOMWindow);
    }
    else return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::GetPresShell(nsIPresShell** aPresShell) 
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIContentViewerContainer> contentViewerContainer;
  contentViewerContainer = do_QueryInterface(mWebShell);
  if (!contentViewerContainer) {
      NS_ERROR("Webshell doesn't support the content viewer container interface");
      return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContentViewer> contentViewer;
  if (NS_FAILED(rv = contentViewerContainer->GetContentViewer(getter_AddRefs(contentViewer)))) {
      NS_ERROR("Unable to retrieve content viewer.");
      return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocumentViewer> docViewer;
  docViewer = do_QueryInterface(contentViewer);
  if (!docViewer) {
      NS_ERROR("Document viewer interface not supported by the content viewer.");
      return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresContext> presContext;
  if (NS_FAILED(rv = docViewer->GetPresContext(*getter_AddRefs(presContext)))) {
      NS_ERROR("Unable to retrieve the doc viewer's presentation context.");
      return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresShell> presShell;
  if (NS_FAILED(rv = presContext->GetShell(getter_AddRefs(presShell)))) {
      NS_ERROR("Unable to retrieve the pres shell.");
      return NS_ERROR_FAILURE;
  }

  *aPresShell = presShell.get();
  NS_IF_ADDREF(*aPresShell);

  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::CreatePopup(nsIDOMElement* aElement, nsIDOMElement* aPopupContent, 
                              PRInt32 aXPos, PRInt32 aYPos, 
                              const nsString& aPopupType, const nsString& anAnchorAlignment,
                              const nsString& aPopupAlignment,
                              nsIDOMWindow* aWindow, nsIDOMWindow** outPopup)
{
  nsresult rv = NS_OK;

  // First we need to obtain the popup set frame that encapsulates the target popup.
  // Without a popup set, we're dead in the water.
  nsCOMPtr<nsIPresShell> presShell;
  if (NS_FAILED(GetPresShell(getter_AddRefs(presShell)))) {
    return NS_OK;
  }

  // Get the parent of the popup content.
  nsCOMPtr<nsIDOMNode> popupSet;
  aPopupContent->GetParentNode(getter_AddRefs(popupSet));
  if (!popupSet)
    return NS_OK;

  // Do a sanity check to ensure we have a popup set element.
  nsString tagName;
  nsCOMPtr<nsIDOMElement> popupSetElement = do_QueryInterface(popupSet);
  popupSetElement->GetTagName(tagName);
  if (tagName != "menupopupset")
    return NS_OK;

  // Now obtain the popup set frame.
  nsCOMPtr<nsIContent> popupSetContent = do_QueryInterface(popupSet);
  nsIFrame* frame;
  presShell->GetPrimaryFrameFor(popupSetContent, &frame);  
  if (!frame)
    return NS_OK;

  // Retrieve the frame that corresponds to the element that the popup
  // is attached to.
  nsIFrame* elementFrame;
  nsCOMPtr<nsIContent> elementContent = do_QueryInterface(aElement);
  presShell->GetPrimaryFrameFor(elementContent, &elementFrame);  
  if (!elementFrame)
    return NS_OK;

  // Pass this all off to the popup set frame.
  nsCOMPtr<nsIPopupSetFrame> popupSetFrame = do_QueryInterface(frame);
  nsCOMPtr<nsIContent> popupContent = do_QueryInterface(aPopupContent);
  popupSetFrame->CreatePopup(elementFrame, popupContent, aXPos, aYPos,
                             aPopupType, anAnchorAlignment,
                             aPopupAlignment);


  /*
  // clear out result param up front. It's an error if a legal place to
  // stick the result isn't provided.
  if ( !outPopup ) {
    NS_ERROR ( "Invalid param -- need to provide a place for result" );
    return NS_ERROR_INVALID_ARG;
  }
  *outPopup = nsnull;

 
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  if (anAnchorAlignment != "none") {
    // We need to compute our screen coordinates.
    nsIFrame* primaryFrame;
    presShell->GetPrimaryFrameFor(content, &primaryFrame);
    if (primaryFrame) {
      // Get the coordinates within the view.
      nsRect    rect;
      nsIView * view;
      nsPoint   pnt;
      primaryFrame->GetOffsetFromView(pnt, &view);
      primaryFrame->GetRect(rect);
      rect.x = pnt.x;
      rect.y = pnt.y;

      float t2p;
      presContext->GetTwipsToPixels(&t2p);
      rect.x = NSToCoordRound((float)(rect.x)*t2p);
      rect.y = NSToCoordRound((float)(rect.y)*t2p);
      rect.width  = NSToCoordRound((float)(rect.width)*t2p);
      rect.height = NSToCoordRound((float)(rect.height)*t2p);

      if (anAnchorAlignment == "topleft") {
        aXPos = rect.x + aXPos;
        aYPos = rect.y + aYPos;
      }
      else if (anAnchorAlignment == "topright") {
        aXPos = rect.x + rect.width + aXPos;
        aYPos = rect.y + aYPos;
      }
      else if (anAnchorAlignment == "bottomleft") {
        aXPos = rect.x + aXPos;
        aYPos = rect.y + rect.height + aYPos;
      }
      else {
        aXPos = rect.x + rect.width + aXPos;
        aYPos = rect.y + rect.height + aYPos;
      }
    }
  }

  nsCOMPtr<nsIContent> popupContent = do_QueryInterface(aPopupContent);
  if ( !popupContent ) 
    return NS_OK; // It's ok. Really.
  
  // Fire the create DOM event to give JS/C++ a chance to build the popup
  // dynamically. After we fire off the event, make sure we check the result
  // for |nsEventStatus_eConsumeNoDefault| which translates into an event
  // handler returning false. If that happens, abort creating the popup.
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_MENU_CREATE;
  rv = popupContent->HandleDOMEvent(*presContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
  if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
    return rv;

  // Find out if we're a menu.
  PRInt32 childCount;
  popupContent->ChildCount(childCount);
  if (childCount == 0)
    return NS_OK;

  nsCOMPtr<nsIContent> rootContent;
  if (NS_FAILED(popupContent->ChildAt(0, *getter_AddRefs(rootContent))))
    return NS_OK; // Doesn't matter. Don't report it.

  nsString tagName;
  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(rootContent);
  if ( !rootElement || NS_FAILED(rootElement->GetTagName(tagName))) 
    return NS_OK; // It's ok. Really.

  if (tagName == "menu") {
      
    // XXX Need to distinguish between popup menus and context menus?
    DoContextMenu(nsnull, rootElement, mWindow, aXPos, aYPos, aPopupAlignment, anAnchorAlignment);

    // Fire the destroy DOM event to give JS/C++ a chance to destroy the popup contents
    status = nsEventStatus_eIgnore;
    event.eventStructType = NS_EVENT;
    event.message = NS_MENU_DESTROY;
    rv = popupContent->HandleDOMEvent(*presContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
    return rv;
  }
  
  if (tagName != "window")
    return NS_OK; // I promise. It really is ok. Stop whining.

  // Handle the arbitrary popup XUL case.
  
  // (1) Create a top-level chromeless window. The size of the window can be specified
  // on the window tag contained inside.  Retrieve the webshell from the new nsWebShellWindow.

  nsCOMPtr<nsIWebShellWindow> newWindow;
  
  nsWebShellWindow* window = new nsWebShellWindow();
  if (nsnull == window) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsWidgetInitData widgetInitData;
  widgetInitData.mWindowType = eWindowType_popup;
  widgetInitData.mBorderStyle = eBorderStyle_default;

  window->SetIntrinsicallySized(PR_TRUE);
  rv = window->Initialize((nsIWebShellWindow *) nsnull, nsnull, nsnull,
                          PR_TRUE, PR_TRUE, nsnull, nsnull,
                          1, 1, widgetInitData);

  if (NS_FAILED(rv)) 
    return rv;

  newWindow = window;

  // Move the window to aXPos and aYPos
  nsCOMPtr<nsIBrowserWindow> browserWindow = do_QueryInterface(newWindow);
  browserWindow->MoveTo(aXPos, aYPos);

  // (2) Get the webshell
  nsCOMPtr<nsIWebShell> newShell;
  newWindow->GetWebShell(*getter_AddRefs(newShell));
  
  nsCOMPtr<nsIDOMWindow> domWindow;
  if (NS_FAILED(rv = ConvertWebShellToDOMWindow(newShell, getter_AddRefs(domWindow)))) {
    NS_ERROR("Unable to retrieve the DOM window from the new web shell.");
    return rv;
  }
  
  // (3) We need to create a new document that clones the original document's popup
  // content.  This new document must use the different root and a different global script 
  // context (window object) but everything else about it is the same (namespaces, URLs,
  // stylesheets).
  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));
  if (document == nsnull)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIXULParentDocument> parentDocument = do_QueryInterface(document);
  if (parentDocument == nsnull)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> popupDocument;
  if (NS_FAILED(rv = parentDocument->CreatePopupDocument(rootContent, getter_AddRefs(popupDocument)))) {
    NS_ERROR("Unable to create the child popup document.");
    return rv;
  }

  // The tricky part now is bypassing the normal load process and just putting a document into
  // the webshell.  This is particularly nasty, since webshells don't normally even know
  // about their documents
  // (4) Create a document viewer 
  nsCOMPtr<nsIContentViewer> documentViewer;
  nsCOMPtr<nsIDocumentLoaderFactory> docFactory;
  if (NS_FAILED(rv = nsComponentManager::CreateInstance(kLayoutDocumentLoaderFactoryCID, nsnull, 
                                          kIDocumentLoaderFactoryIID,
                                          (void**)getter_AddRefs(docFactory)))) {
    NS_ERROR("Unable to instantiate layout's doc loader factory.");
    return rv;
  }
  nsCOMPtr<nsIContentViewerContainer> cvContainer = do_QueryInterface(newShell);
  if (NS_FAILED(rv = docFactory->CreateInstanceForDocument(cvContainer,
                                        popupDocument,
                                        "view",
                                        getter_AddRefs(documentViewer)))) {
    NS_ERROR("I couldn't get a document viewer for the popup document.");
    return rv;
  }

  // (5) Feed the webshell to the content viewer
  documentViewer->SetContainer(cvContainer);

  // (6) Tell the content viewer container to embed the content viewer.
  //     (This step causes everything to be set up for an initial flow.)
  if (NS_FAILED(rv = cvContainer->Embed(documentViewer, "view", nsnull))) {
    NS_ERROR("Unable to embed the viewer in the container.");
    return rv;
  }
  
  // (7) Hook up a blur handler to the window that will cause it to close.
  nsCOMPtr<nsIXULPopupListener> popupListener;
  if (NS_FAILED(rv = nsComponentManager::CreateInstance(kXULPopupListenerCID,
                                          nsnull,
                                          kIXULPopupListenerIID,
                                          (void**) getter_AddRefs(popupListener)))) {
    NS_ERROR("Unable to create an instance of the popup listener object.");
    return rv;
  }
  nsIContent* popupRoot = popupDocument->GetRootContent(); 
  nsCOMPtr<nsIDOMElement> popupRootElement = do_QueryInterface(popupRoot);
  if (NS_FAILED(rv = popupListener->Init(popupRootElement, eXULPopupType_blur))) {
    NS_ERROR("Unable to initialize our blur listener.");
    return rv;
  }
  NS_IF_RELEASE(popupRoot);
  nsCOMPtr<nsIDOMFocusListener> blurListener = do_QueryInterface(popupListener);
  nsCOMPtr<nsIDOMEventTarget> targetWindow = do_QueryInterface(domWindow);
  targetWindow->AddEventListener("blur", blurListener, PR_FALSE);  
       
  // (8) Set up the opener property
  domWindow->SetOpener(aWindow);

  // (9) Show the window. Don't give the focus yet because we may not want to.
  // For example, popup windows want focus, but tooltips do not.
  newWindow->Show(PR_TRUE);

  // (10) Do some layout.
  nsCOMPtr<nsIXULChildDocument> popupChild = do_QueryInterface(popupDocument);
  popupChild->LayoutPopupDocument();

  // return the popup.
  *outPopup = domWindow;
  NS_ADDREF(*outPopup);
  */

  return rv;
}

NS_IMETHODIMP
nsWebShellWindow::ContentShellAdded(nsIWebShell* aChildShell, nsIContent* frameNode)
{
  // Find out the id of the frameNode in question 
  nsIAtom      *idAtom = NS_NewAtom("id");
  nsIAtom      *typeAtom = NS_NewAtom("type");
  PRBool       isPrimary;
  nsAutoString value;

  // right now, any webshell with type "content" or "content-XXX" is
  // considered a content webshell.  but only "content-primary" is
  // considered primary.
  frameNode->GetAttribute(kNameSpaceID_None, typeAtom, value);
  isPrimary = value.EqualsIgnoreCase(kPrimaryContentTypeValue) ? PR_TRUE : PR_FALSE;
  frameNode->GetAttribute(kNameSpaceID_None, idAtom, value);

  AddWebShellInfo(value, isPrimary, aChildShell);

  // For some reason nsCOMPtr and do_queryInterface fail here
  if (aChildShell) {
    nsIUrlDispatcher *  ud = nsnull;
    QueryInterface(kIUrlDispatcherIID, (void **)&ud);
    aChildShell->SetUrlDispatcher(ud);
    NS_RELEASE(ud);
  }

  NS_RELEASE(typeAtom);
  NS_RELEASE(idAtom);
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::NewWebShell(PRUint32 aChromeMask, PRBool aVisible,
                              nsIWebShell *&aNewWebShell)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIWebShellWindow> newWindow;

  // XXX Check modal chrome flag to run a modal dialog!

  if ((aChromeMask & NS_CHROME_OPEN_AS_CHROME) != 0) {
    // Just do a nice normal create of a web shell and
    // return it immediately. 

    nsIWebShellWindow *parent = aChromeMask & NS_CHROME_DEPENDENT ? this : nsnull;
    rv = appShell->CreateTopLevelWindow(parent, nsnull, PR_FALSE, PR_FALSE,
                                 aChromeMask, nsnull,
                                 NS_SIZETOCONTENT, NS_SIZETOCONTENT,
                                 getter_AddRefs(newWindow));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIBrowserWindow> browser(do_QueryInterface(newWindow));
      if (browser)
        browser->SetChrome(aChromeMask);
      newWindow->GetWebShell(aNewWebShell); // GetWebShell does the addref.
    }
    return rv;
  }

  // We need to create a new top level window and then enter a nested
  // loop. Eventually the new window will be told that it has loaded,
  // at which time we know it is safe to spin out of the nested loop
  // and allow the opening code to proceed.

  // First push a nested event queue for event processing from netlib
  // onto our UI thread queue stack.
  appShell->PushThreadEventQueue();

  nsCOMPtr<nsIURI> urlObj;
  char * urlStr = "chrome://navigator/content/";
#ifndef NECKO
  rv = NS_NewURL(getter_AddRefs(urlObj), urlStr);
#else
  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsIURI *uri = nsnull;
  rv = service->NewURI(urlStr, nsnull, &uri);
  if (NS_FAILED(rv)) return rv;

  rv = uri->QueryInterface(nsIURI::GetIID(), (void**)&urlObj);
  NS_RELEASE(uri);
#endif // NECKO

  if (NS_SUCCEEDED(rv)) {
    rv = appShell->CreateTopLevelWindow(nsnull, urlObj, PR_FALSE, PR_FALSE,
                                 aChromeMask, nsnull, 615, 480,
                                 getter_AddRefs(newWindow));
  }

  nsIAppShell *subshell;
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIBrowserWindow> browser(do_QueryInterface(newWindow));
    if (browser)
      browser->SetChrome(aChromeMask);

    // Spin into the modal loop.
    rv = nsComponentManager::CreateInstance(kAppShellCID, nsnull, kIAppShellIID, (void**)&subshell);
  }

  if (NS_SUCCEEDED(rv)) {

    subshell->Create(0, nsnull);
    subshell->Spinup(); // Spin up 

    // Specify that we want the window to remain locked until the chrome has loaded.
    newWindow->LockUntilChromeLoad();

    PRBool locked = PR_FALSE;
    nsresult looprv = NS_OK;
    newWindow->GetLockedState(locked);
    while (NS_SUCCEEDED(looprv) && locked) {
      void      *data;
      PRBool    isRealEvent;
    
      looprv = subshell->GetNativeEvent(isRealEvent, data);
      subshell->DispatchNativeEvent(isRealEvent, data);

      newWindow->GetLockedState(locked);
    }

    // Get rid of the nested UI thread queue used for netlib
    appShell->PopThreadEventQueue();

    subshell->Spindown();
    NS_RELEASE(subshell);
  }

  // We're out of the nested loop.
  // During the layout of the new window, all content shells were located and placed
  // into the new window's content shell array.  Locate the "content area" content
  // shell.
  if (NS_SUCCEEDED(rv)) {
    rv = newWindow->GetContentShellById("content", &aNewWebShell);
    if (NS_FAILED(rv)) {
      NS_ERROR("Unable to obtain a browser content shell.");
      return rv;
    }
  }

  return rv;
}


/**
 * FindWebShellWithName - recursively search for any open window
 * containing a webshell with the given name.
 * @param aName - the name of the webshell to find. I believe this cannot
 *                be null. Hard to tell. If zero-length, the find will
 *                always fail (returning NS_OK).
 * @param aResult - the webshell, returned, addrefed. null on failure to
 *                  locate the desired webshell.
 * @return an error indication. Can be NS_OK even if no match was found.
 */
NS_IMETHODIMP nsWebShellWindow::FindWebShellWithName(const PRUnichar* aName,
                                                     nsIWebShell*& aResult)
{
  nsresult rv = NS_OK;
  nsString nameStr(aName);

  // Zero result (in case we fail).
  aResult = nsnull;

  // first, special cases
  if (nameStr.Length() == 0)
    return NS_OK;
  if (nameStr.EqualsIgnoreCase("_content"))
    return GetContentWebShell(&aResult);

  // look for open windows with the given name
  /*   Note: this function arguably works as expected, but the end effect
     is wrong.  The webshell that catches the name given from a JavaScript
     window.open call is the content, not the chrome, so a window whose
     location is redirected will have its content replaced, not its chrome.
     That may be the right choice; maybe not.  Also, there's a visual problem
     where the window position is reset.
       So when two or three bad things get cleared up, this next bit will be
     helpful.  As it is, it's not too. */

  NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

    if (NS_SUCCEEDED(windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator)))) {
      PRBool more;

      // get the (main) webshell for each window in the enumerator
      windowEnumerator->HasMoreElements(&more);
      while (more) {
        nsCOMPtr<nsISupports> protoWindow;
        rv = windowEnumerator->GetNext(getter_AddRefs(protoWindow));
        if (NS_SUCCEEDED(rv) && protoWindow) {
          // it's supposed to be an nsIDOMWindow, so it's one of these, too
          nsCOMPtr<nsIScriptGlobalObject> whatever(do_QueryInterface(protoWindow));
          if (whatever) {
            nsCOMPtr<nsIWebShell> webshell;
            whatever->GetWebShell(getter_AddRefs(webshell));
            if (webshell) {
              // check the webshell, and then its children, for a name match
              const PRUnichar *name;
              if (NS_SUCCEEDED(webshell->GetName(&name)) && nameStr.Equals(name)) {
                aResult = webshell;
                NS_ADDREF(aResult);
                break;
              }
              // Search for named frame within our root webshell.  This will
              // bypass the .xul document (and rightfully so).  We need to be
              // careful not to give that documents child frames names!
              if (NS_SUCCEEDED(webshell->FindChildWithName(aName, aResult)) && aResult)
                break;
            }
          }
        }
        windowEnumerator->HasMoreElements(&more);
      }
    }
  } else
    // should be redundant, but left in for now
    if (mWebShell)
      rv = mWebShell->FindChildWithName(aName, aResult);

  return rv;
}

NS_IMETHODIMP 
nsWebShellWindow::FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken)
{
  return NS_OK;
}

//----------------------------------------
// nsIWebShellWindow methods...
//----------------------------------------
NS_IMETHODIMP
nsWebShellWindow::Show(PRBool aShow)
{
  mWebShell->Show(); // crimminy -- it doesn't take a parameter!
  mWindow->Show(aShow);
  
  nsresult rv;
  NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
  if ( NS_SUCCEEDED(rv) )
  {
   	windowMediator->UpdateWindowTimeStamp( this ); 
  } 
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::ShowModal()
{
  nsresult  rv;
  rv = ShowModalInternal();
  return rv;
}


NS_IMETHODIMP
nsWebShellWindow::ShowModalInternal()
{
  nsresult    rv;
  nsIAppShell *subshell;

  // spin up a new application shell: event loops live there
  rv = nsComponentManager::CreateInstance(kAppShellCID, nsnull, kIAppShellIID, (void**)&subshell);
  if (NS_FAILED(rv))
    return rv;

  subshell->Create(0, nsnull);
  subshell->Spinup();

  nsIWidget *window = GetWidget();
  window->SetModal();
  NS_ADDREF(window);
  mContinueModalLoop = PR_TRUE;
  while (NS_SUCCEEDED(rv) && mContinueModalLoop == PR_TRUE) {
    void      *data;
    PRBool    isRealEvent,
              processEvent;

    rv = subshell->GetNativeEvent(isRealEvent, data);
    if (NS_SUCCEEDED(rv)) {
      subshell->EventIsForModalWindow(isRealEvent, data, window, &processEvent);
      if (processEvent == PR_TRUE)
        subshell->DispatchNativeEvent(isRealEvent, data);
    }
  }

  subshell->Spindown();
  NS_RELEASE(window);
  NS_RELEASE(subshell);

  return rv;
}


// yes, this one's name and ShowModal are a confusing pair. plan is to merge
// the two someday.
NS_IMETHODIMP
nsWebShellWindow::ShowModally(PRBool aPrepare)
{
  nsresult            rv;
  nsCOMPtr<nsIWidget> parentWidget;

  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  if (aPrepare && NS_FAILED(appShell->PushThreadEventQueue()))
    aPrepare = PR_FALSE;

  parentWidget = do_QueryReferent(mParentWindow);
  if (parentWidget)
    parentWidget->Enable(PR_FALSE);
  rv = ShowModal();
  if (parentWidget)
    parentWidget->Enable(PR_TRUE);

  if (aPrepare)
    appShell->PopThreadEventQueue();

  return rv;
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
  nsIWebShell  *content;

  content = nsnull;
  
  // first, try looking in the webshell list
  // (note this list isn't dynamic: it's set up when the webshell is added,
  // but not updated when its attributes are poked. could be a problem...)
  if (mContentShells) {
    PRInt32 count = mContentShells->Count();
    for (PRInt32 ctr = 0; ctr < count; ctr++) {
      nsWebShellInfo* webInfo = (nsWebShellInfo*)(mContentShells->ElementAt(ctr));
      if (webInfo->primary) {
        content = webInfo->child;
        break;
      }
    }
  }

  NS_IF_ADDREF(content);
  *aResult = content;
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::GetWidget(nsIWidget *& aWidget)
{
  aWidget = mWindow;
  NS_ADDREF(mWindow);
  return NS_OK;
}

void *
nsWebShellWindow::HandleModalDialogEvent(PLEvent *aEvent)
{
  ThreadedWindowEvent *event = (ThreadedWindowEvent *) aEvent;

  event->window->ShowModalInternal();
  return 0;
}

void
nsWebShellWindow::DestroyModalDialogEvent(PLEvent *aEvent)
{
  PR_Free(aEvent);
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
#ifdef NECKO
nsWebShellWindow::OnEndDocumentLoad(nsIDocumentLoader* loader, 
                                    nsIChannel* channel, nsresult aStatus,
                                    nsIDocumentLoaderObserver * aDocObserver)
#else
nsWebShellWindow::OnEndDocumentLoad(nsIDocumentLoader* loader, 
                                    nsIURI* aURL, PRInt32 aStatus,
                                    nsIDocumentLoaderObserver * aDocObserver)
#endif
{
#ifdef DEBUG_MENUSDEL
  printf("OnEndDocumentLoad\n");
#endif

  /* We get notified every time a page/Frame is loaded. But we need to
   * Load the menus, run the startup script etc.. only once. So, Use
   * the mChrome Initialized  member to check whether chrome should be 
   * initialized or not - Radha
   */
  if (mChromeInitialized)
    return NS_OK;

  mChromeInitialized = PR_TRUE;

  mLockedUntilChromeLoad = PR_FALSE;

#ifdef XP_MAC // Anyone still using native menus should add themselves here.
  // register as document listener
  // this is needed for menus
  nsCOMPtr<nsIContentViewer> cv;
  mWebShell->GetContentViewer(getter_AddRefs(cv));
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

  ExecuteStartupCode();

#ifdef XP_MAC // Anyone still using native menus should add themselves here.
  ///////////////////////////////
  // Find the Menubar DOM  and Load the menus, hooking them up to the loaded commands
  ///////////////////////////////
  nsCOMPtr<nsIDOMDocument> menubarDOMDoc(GetNamedDOMDoc(nsAutoString("this"))); // XXX "this" is a small kludge for code reused
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
#endif // XP_MAC

  SetTitleFromXUL();
  ShowAppropriateChrome();
  LoadContentAreas();
  
  // Here's where we service the "show" request initially given in Initialize()
  if (mCreatedVisible)
    Show(PR_TRUE);

  nsCOMPtr<nsIWebShell> contentShell;
  GetContentWebShell(getter_AddRefs(contentShell));
  if (contentShell) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    if (NS_SUCCEEDED(ConvertWebShellToDOMWindow(contentShell, 
                                                getter_AddRefs(domWindow)))) {
      domWindow->Focus();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
#ifdef NECKO
nsWebShellWindow::OnStartURLLoad(nsIDocumentLoader* loader, 
                                 nsIChannel* channel, 
                                 nsIContentViewer* aViewer)
#else
nsWebShellWindow::OnStartURLLoad(nsIDocumentLoader* loader, 
                                 nsIURI* aURL, 
                                 const char* aContentType, 
                                 nsIContentViewer* aViewer)
#endif
{
  return NS_OK;
}

NS_IMETHODIMP
#ifdef NECKO
nsWebShellWindow::OnProgressURLLoad(nsIDocumentLoader* loader, 
                                    nsIChannel* channel, 
                                    PRUint32 aProgress, 
                                    PRUint32 aProgressMax)
#else
nsWebShellWindow::OnProgressURLLoad(nsIDocumentLoader* loader, 
                                    nsIURI* aURL, 
                                    PRUint32 aProgress, 
                                    PRUint32 aProgressMax)
#endif
{
  return NS_OK;
}

NS_IMETHODIMP
#ifdef NECKO
nsWebShellWindow::OnStatusURLLoad(nsIDocumentLoader* loader, 
                                  nsIChannel* channel, nsString& aMsg)
#else
nsWebShellWindow::OnStatusURLLoad(nsIDocumentLoader* loader, 
                                  nsIURI* aURL, nsString& aMsg)
#endif
{
  return NS_OK;
}

NS_IMETHODIMP
#ifdef NECKO
nsWebShellWindow::OnEndURLLoad(nsIDocumentLoader* loader, 
                               nsIChannel* channel, nsresult aStatus)
#else
nsWebShellWindow::OnEndURLLoad(nsIDocumentLoader* loader, 
                               nsIURI* aURL, PRInt32 aStatus)
#endif
{
  return NS_OK;
}

NS_IMETHODIMP
#ifdef NECKO
nsWebShellWindow::HandleUnknownContentType(nsIDocumentLoader* loader, 
                                           nsIChannel* channel, 
                                           const char *aContentType,
                                           const char *aCommand )
#else
nsWebShellWindow::HandleUnknownContentType(nsIDocumentLoader* loader, 
                                           nsIURI* aURL,
                                           const char *aContentType,
                                           const char *aCommand )
#endif
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
nsCOMPtr<nsIDOMNode> nsWebShellWindow::GetParentNodeFromDOMDoc(nsIDOMDocument * aDOMDoc)
{
  nsCOMPtr<nsIDOMNode> node; // null

  if (nsnull == aDOMDoc) {
    return node;
  }

  nsCOMPtr<nsIDOMElement> element;
  aDOMDoc->GetDocumentElement(getter_AddRefs(element));
  if (element)
    return nsCOMPtr<nsIDOMNode>(do_QueryInterface(element));
  return node;
} // nsWebShellWindow::GetParentNodeFromDOMDoc

//----------------------------------------
nsCOMPtr<nsIDOMDocument> nsWebShellWindow::GetNamedDOMDoc(const nsString & aWebShellName)
{
  nsCOMPtr<nsIDOMDocument> domDoc; // result == nsnull;

  // first get the toolbar child WebShell
  nsCOMPtr<nsIWebShell> childWebShell;
  if (aWebShellName.Equals("this")) { // XXX small kludge for code reused
    childWebShell = do_QueryInterface(mWebShell);
  } else {
    mWebShell->FindChildWithName(aWebShellName.GetUnicode(), *getter_AddRefs(childWebShell));
    if (!childWebShell)
      return domDoc;
  }
  
  nsCOMPtr<nsIContentViewer> cv;
  childWebShell->GetContentViewer(getter_AddRefs(cv));
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
PRInt32 nsWebShellWindow::GetDocHeight(nsIDocument * aDoc)
{
  nsIPresShell * presShell = aDoc->GetShellAt(0);
  if (!presShell)
    return 0;

  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if (presContext) {
    nsRect rect;
    presContext->GetVisibleArea(rect);
    nsIFrame * rootFrame;
    nsSize size;
    presShell->GetRootFrame(&rootFrame);
    if (rootFrame) {
      rootFrame->GetSize(size);
      float t2p;
      presContext->GetTwipsToPixels(&t2p);
      printf("Doc size %d,%d\n", PRInt32((float)size.width*t2p), 
                                 PRInt32((float)size.height*t2p));
      //return rect.height;
      return PRInt32((float)rect.height*t2p);
      //return PRInt32((float)size.height*presContext->GetTwipsToPixels());
    }
  }
  NS_RELEASE(presShell);
  return 0;
}

//----------------------------------------

/**
 * Get nsIDOMNode corresponding to a given webshell
 * @param aShell the given webshell
 * @return the corresponding DOM element, null if for some reason there is none
 */
nsCOMPtr<nsIDOMNode>
nsWebShellWindow::GetDOMNodeFromWebShell(nsIWebShell *aShell)
{
  nsCOMPtr<nsIDOMNode> node;

  nsCOMPtr<nsIContentViewer> cv;
  aShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
    if (docv) {
      nsCOMPtr<nsIDocument> doc;
      docv->GetDocument(*getter_AddRefs(doc));
      if (doc) {
        nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(doc));
        if (domdoc) {
          nsCOMPtr<nsIDOMElement> element;
          domdoc->GetDocumentElement(getter_AddRefs(element));
          if (element)
            node = do_QueryInterface(element);
        }
      }
    }
  }

  return node;
}

/**
 * XXX Hack for XUL Window callbacks. MUST GO AWAY!!!!
 */
void nsWebShellWindow::ExecuteStartupCode()
{
  nsCOMPtr<nsIDOMNode> webshellNode = GetDOMNodeFromWebShell(mWebShell);
  nsCOMPtr<nsIDOMElement> webshellElement;

  if (webshellNode)
    webshellElement = do_QueryInterface(webshellNode);

  if (mCallbacks)
    mCallbacks->ConstructBeforeJavaScript(mWebShell);

  // Execute the string in the onLoad attribute of the webshellElement.
  nsString startupCode;
  
  if (mCallbacks)
    mCallbacks->ConstructAfterJavaScript(mWebShell);
}



void nsWebShellWindow::SetTitleFromXUL()
{
  nsCOMPtr<nsIDOMNode> webshellNode = GetDOMNodeFromWebShell(mWebShell);
  nsIWidget *windowWidget = GetWidget();
  nsCOMPtr<nsIDOMElement> webshellElement;
  nsString windowTitle;

  if (webshellNode)
    webshellElement = do_QueryInterface(webshellNode);
  if (webshellElement && windowWidget &&
      NS_SUCCEEDED(webshellElement->GetAttribute("title", windowTitle)) &&
      windowTitle != "")
    SetTitle(windowTitle.GetUnicode());
} // SetTitleFromXUL


// show/hide contents according to the current chrome mask
void nsWebShellWindow::ShowAppropriateChrome()
{
  nsCOMPtr<nsIDOMElement>    rootElement;
  nsCOMPtr<nsIDOMXULElement> xulRoot;
  nsCOMPtr<nsIDOMDocument>   chromeDoc;
  nsCOMPtr<nsIDOMWindow>     domWindow;
  PRUint32                   chromeMask;

  // get this window's document
  if (NS_FAILED(ConvertWebShellToDOMWindow(mWebShell, getter_AddRefs(domWindow))))
    return;
  if (NS_FAILED(domWindow->GetDocument(getter_AddRefs(chromeDoc))) || !chromeDoc)
    return;
  if (NS_FAILED(chromeDoc->GetDocumentElement(getter_AddRefs(rootElement))) || !rootElement)
    return;

  // calculate a special version of the chrome mask. we store the actual
  // value sent, but we make local changes depending on whether defaults
  // were asked for.  Note that only internal (not OS-) chrome matters
  // at this point, so the OS chrome is not calculated.
  chromeMask = mChromeMask;
  if (chromeMask & NS_CHROME_DEFAULT_CHROME)
    if (chromeMask & NS_CHROME_OPEN_AS_DIALOG)
      chromeMask &= ~(NS_CHROME_MENU_BAR_ON | NS_CHROME_TOOL_BAR_ON |
                      NS_CHROME_LOCATION_BAR_ON | NS_CHROME_STATUS_BAR_ON |
                      NS_CHROME_PERSONAL_TOOLBAR_ON | NS_CHROME_SCROLLBARS_ON);
    else
      // theoretically, this won't happen (only dialogs can have defaults)
      // but, we cover this case anyway
      chromeMask |= NS_CHROME_ALL_CHROME;

  // special treatment for the menubar
  ShowMenuBar(mChromeMask & NS_CHROME_MENU_BAR_ON ? PR_TRUE : PR_FALSE);

  // get a list of this document's elements with the chromeclass attribute specified
  xulRoot = do_QueryInterface(rootElement);
  if (xulRoot) { // todo (maybe) the longer, straight DOM (not RDF) version?
    nsCOMPtr<nsIDOMNodeList> chromeNodes;
    if (NS_SUCCEEDED(xulRoot->GetElementsByAttribute("chromeclass", "*",
                                getter_AddRefs(chromeNodes)))) {
      PRUint32 nodeCtr, nodeCount;
      chromeNodes->GetLength(&nodeCount);
      for (nodeCtr = 0; nodeCtr < nodeCount; nodeCtr++) {
        nsCOMPtr<nsIDOMNode> domNode;
        nsCOMPtr<nsIDOMElement> domElement;
        chromeNodes->Item(nodeCtr, getter_AddRefs(domNode));
        domElement = do_QueryInterface(domNode);
        if (domElement) {
          nsAutoString chromeClass;
          PRBool       makeChange;
          PRUint32     flag;
          // show or hide the element according to its chromeclass and the chromemask
          domElement->GetAttribute("chromeclass", chromeClass);
          makeChange = PR_FALSE;
          if (chromeClass == "menubar") {
            makeChange = PR_TRUE;
            flag = mChromeMask & NS_CHROME_MENU_BAR_ON;
          } else if (chromeClass == "toolbar") {
            makeChange = PR_TRUE;
            flag = mChromeMask & NS_CHROME_TOOL_BAR_ON;
          } else if (chromeClass == "location") {
            makeChange = PR_TRUE;
            flag = mChromeMask & NS_CHROME_LOCATION_BAR_ON;
          } else if (chromeClass == "directories") {
            makeChange = PR_TRUE;
            flag = mChromeMask & NS_CHROME_PERSONAL_TOOLBAR_ON;
          } else if (chromeClass == "status") {
            makeChange = PR_TRUE;
            flag = mChromeMask & NS_CHROME_STATUS_BAR_ON;
          }
          if (makeChange)
            if (flag)
              domElement->RemoveAttribute("chromehidden");
            else
              domElement->SetAttribute("chromehidden", "T");
        }
      }
    }
  }
}

// if the main document URL specified URLs for any content areas, start them loading
void nsWebShellWindow::LoadContentAreas() {

  nsAutoString searchSpec;

  // fetch the chrome document URL
  nsCOMPtr<nsIContentViewer> contentViewer;
  mWebShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer) {
    nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(contentViewer);
    if (docViewer) {
      nsCOMPtr<nsIDocument> doc;
      docViewer->GetDocument(*getter_AddRefs(doc));
      nsCOMPtr<nsIURI> mainURL = getter_AddRefs(doc->GetDocumentURL());
      if (mainURL) {
#ifdef NECKO
        char *search = nsnull;
        nsCOMPtr<nsIURL> url = do_QueryInterface(mainURL);
        if (url)
          url->GetQuery(&search);
#else
        const char *search;
        mainURL->GetSearch(&search);
#endif
        searchSpec = search;
#ifdef NECKO
        nsCRT::free(search);
#endif
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
    for (endPos = 0; endPos < searchSpec.Length(); ) {
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
          contentURL = urlChar;
          contentShell->LoadURL(contentURL.GetUnicode());
          delete [] urlChar;
        }
        NS_RELEASE(contentShell);
      }
    }
  }
}

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
                                   nsIAtom*     aAttribute,
                                   PRInt32      aHint)
{
  //printf("AttributeChanged\n");
  PRInt32 i;
  for (i=0;i<mMenuDelegates.Count();i++) {
    nsIXULCommand * cmd  = (nsIXULCommand *)mMenuDelegates[i];
    nsIDOMElement * node;
    cmd->GetDOMElement(&node);
    //nsCOMPtr<nsIContent> content(do_QueryInterface(node));
    // Doing this for the must speed
    nsIContent * content;
    if (NS_OK == node->QueryInterface(kIContentIID, (void**) &content)) {
      if (content == aContent) {
        nsAutoString attr;
        aAttribute->ToString(attr);
        cmd->AttributeHasBeenSet(attr);
      }
      NS_RELEASE(content);
    }
  }
  
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

/**************** nsIBrowserWindow interface ********************/
NS_IMETHODIMP nsWebShellWindow::Init(nsIAppShell* aAppShell,
                   nsIPref* aPrefs,
                   const nsRect& aBounds,
                   PRUint32 aChromeMask,
                   PRBool aAllowPlugins)
{
   nsresult rv;
   nsCOMPtr<nsIURI> urlObj;
   char * urlStr = "chrome://navigator/content/";
 
#ifndef NECKO
   rv = NS_NewURL(getter_AddRefs(urlObj), urlStr);
#else
   NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &rv);
   if (NS_FAILED(rv)) return rv;

   nsIURI *uri = nsnull;
   rv = service->NewURI(urlStr, nsnull, &uri);
   if (NS_FAILED(rv)) return rv;

   rv = uri->QueryInterface(nsIURI::GetIID(), (void**)&urlObj);
   NS_RELEASE(uri);
#endif // NECKO
   if (NS_FAILED(rv))
     return rv;
 
   // Note: null nsIStreamObserver means this window won't be able to answer FE_callback-type
   // questions from netlib.  Observers are generally appcores.  We'll have to supply
   // a generic browser appcore here someday.
   nsWidgetInitData widgetInitData;
   widgetInitData.mWindowType = eWindowType_child;
   widgetInitData.mBorderStyle = eBorderStyle_default;

   rv = Initialize(nsnull, aAppShell, urlObj, PR_TRUE, PR_TRUE,
       nsnull, nsnull, aBounds.width, aBounds.height, widgetInitData);
   mChromeMask = aChromeMask;
   if (NS_SUCCEEDED(rv))
     MoveTo(aBounds.x, aBounds.y);
   return rv;
}
 
NS_IMETHODIMP nsWebShellWindow::MoveTo(PRInt32 aX, PRInt32 aY)
{
   mWindow->Move(aX, aY);
   return NS_OK;
}
 
NS_IMETHODIMP nsWebShellWindow::SizeWindowTo(PRInt32 aWidth, PRInt32 aHeight)
{
  // XXX We have to look at the delta between our content shell's 
  // size and the size passed in and then resize ourselves based on that
  // delta.
   mIntrinsicallySized = PR_FALSE; // We got changed. No more intrinsic sizing here.
   mWindow->Resize(aWidth, aHeight, PR_TRUE);
   return NS_OK;
}
 
NS_IMETHODIMP nsWebShellWindow::SizeContentTo(PRInt32 aWidth, PRInt32 aHeight)
{
   nsCOMPtr<nsIWebShell> content;
   GetContentWebShell(getter_AddRefs(content));
   if (content) {
     PRInt32 x, y, width, height,
             widthDelta, heightDelta;
     content->GetBounds(x,y,width,height);
     widthDelta = aWidth - width;
     heightDelta = aHeight - height;
   
     nsRect windowBounds;
     mWindow->GetBounds(windowBounds);
     mWindow->Resize(windowBounds.width + widthDelta, 
                     windowBounds.height + heightDelta,
                     PR_TRUE);
   }
   return NS_OK;
}

NS_IMETHODIMP nsWebShellWindow::GetContentBounds(nsRect& aResult)
{
  // Should return the size of the content webshell.
  nsCOMPtr<nsIWebShell> contentShell;
  GetContentWebShell(getter_AddRefs(contentShell));
  if (!contentShell) {
    NS_ERROR("Attempt to retrieve the content bounds for a window with no content.");
    return NS_ERROR_FAILURE;
  }

  PRInt32 x,y,width,height;
  contentShell->GetBounds(x,y,width,height);
  aResult.x = x;
  aResult.y = y;
  aResult.width = width;
  aResult.height = height;

  return NS_OK;
}
 
NS_IMETHODIMP nsWebShellWindow::GetWindowBounds(nsRect& aResult)
{
   mWindow->GetBounds(aResult);
   return NS_OK;
}
 
NS_IMETHODIMP nsWebShellWindow::SetChrome(PRUint32 aNewChromeMask)
{
   mChromeMask = aNewChromeMask;
   ShowAppropriateChrome();
   return NS_OK;
}
 
NS_IMETHODIMP nsWebShellWindow::GetChrome(PRUint32& aChromeMaskResult)
{
   aChromeMaskResult = mChromeMask;
   return NS_OK;
}
 
NS_IMETHODIMP nsWebShellWindow::SetTitle(const PRUnichar* aTitle)
{
   nsIWidget *windowWidget = GetWidget();
 	

// Get window modifier
  nsCOMPtr<nsIDOMNode> webshellNode = GetDOMNodeFromWebShell(mWebShell);
  nsCOMPtr<nsIDOMElement> webshellElement;
  nsString windowTitleModifier;
  nsString windowSeparator;
  if (webshellNode)
    webshellElement = do_QueryInterface(webshellNode);
  if (webshellElement )
  {
  	webshellElement->GetAttribute("titlemodifier", windowTitleModifier );
  	webshellElement->GetAttribute("titlemenuseparator", windowSeparator );
  }
   nsString title( aTitle );
   
   if( title.Length() > 0 )
  	 title += windowSeparator+windowTitleModifier;
   else
   	title = windowTitleModifier;  
   if (windowWidget)
     windowWidget->SetTitle(title);

     

     // Tell the window mediator that a title has changed
   #if 1 
   {
   	  nsIWindowMediator* service;
  		if (NS_FAILED(nsServiceManager::GetService(kWindowMediatorCID, kIWindowMediatorIID, (nsISupports**) &service ) ) )
    		return NS_OK;
  		service->UpdateWindowTitle( this, title.GetUnicode() );
	 		nsServiceManager::ReleaseService(kWindowMediatorCID, service);
   }
   #endif // Window Mediation
   return NS_OK;

}
 
NS_IMETHODIMP nsWebShellWindow::GetTitle(const PRUnichar** aResult)
{
   // no, we didn't store the title for you. why so nosy?
   return NS_ERROR_FAILURE;
}

// This should rightfully be somebody's PROGID?
// Will switch when the "app shell browser component" arrives.
static const char *prefix = "component://netscape/appshell/component/browser/window";

nsresult
nsWebShellWindow::NotifyObservers( const nsString &aTopic, const nsString &someData ) {
    nsresult rv = NS_OK;
    // Get observer service.
    nsIObserverService *svc = 0;
    rv = nsServiceManager::GetService( NS_OBSERVERSERVICE_PROGID,
                                       nsIObserverService::GetIID(),
                                       (nsISupports**)&svc );
    if ( NS_SUCCEEDED( rv ) && svc ) {
        // Notify observers as instructed; the subject is "this" web shell window.
        nsString topic = prefix;
        topic += ";";
        topic += aTopic;
        rv = svc->Notify( (nsIWebShellWindow*)this, topic.GetUnicode(), someData.GetUnicode() );
        // Release the service.
        nsServiceManager::ReleaseService( NS_OBSERVERSERVICE_PROGID, svc );
    } else {
    }
    return rv;
}
 
NS_IMETHODIMP nsWebShellWindow::SetStatus(const PRUnichar* aStatus)
{
    nsresult rv = NS_OK;
    // Store status text unless empty string was set, then use defaultStatus
    mStatus = aStatus;
    if (mStatus.Length() == 0) {
      mStatus = mDefaultStatus;
    }
    // Broadcast status text change to interested parties.
    rv = NotifyObservers( "status", aStatus );
    return rv;
}
 
NS_IMETHODIMP nsWebShellWindow::GetStatus(const PRUnichar** aResult)
{
    nsresult rv = NS_OK;
    if ( aResult ) {
        // Semantics are ill-defined: How to allocate?  Who frees it?
        *aResult = mStatus.ToNewUnicode();
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}
 
NS_IMETHODIMP nsWebShellWindow::SetDefaultStatus(const PRUnichar* aStatus)
{
    nsresult rv = NS_OK;
    // Store status text.
    mDefaultStatus = aStatus;
    // Broadcast status text change to interested parties.
    rv = NotifyObservers( "defaultStatus", aStatus );
    return rv;
}
 
NS_IMETHODIMP nsWebShellWindow::GetDefaultStatus(const PRUnichar** aResult)
{
    nsresult rv = NS_OK;
    if ( aResult ) {
        // Semantics are ill-defined: How to allocate?  Who frees it?
        *aResult = mDefaultStatus.ToNewUnicode();
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}
 
NS_IMETHODIMP nsWebShellWindow::SetProgress(PRInt32 aProgress, PRInt32 aProgressMax)
{
    nsresult rv = NS_OK;        

    // Encode progress report in topic (there is no GetProgress for observers
    // to query it).
    char topic[32];
    PR_snprintf( topic,
                 sizeof topic,
                 "%ld %ld",
                 (long)aProgress,
                 (long)aProgressMax );

    // Broadcast progress info to interested parties.
    rv = NotifyObservers( "progress", topic );

    return rv;
}

NS_IMETHODIMP
nsWebShellWindow::ShowMenuBar(PRBool aShow)
{
  if (aShow)
    mChromeMask |= NS_CHROME_MENU_BAR_ON;
  else
    mChromeMask &= ~NS_CHROME_MENU_BAR_ON;
  return mWindow->ShowMenuBar(aShow);
}

//nsIUrlDispatcher methods

NS_IMETHODIMP
nsWebShellWindow::HandleUrl(const PRUnichar * aCommand, 
                            const PRUnichar * aURLSpec, 
                            nsIInputStream * aPostDataStream)
{  
  /* Make the topic to observe. The topic will be of the format 
   * linkclick:<prototocol>. Note thet this is a totally made up thing.
   * Things are going to change later
   */
  nsString topic(aCommand);
  topic += ":";
  nsAutoString url(aURLSpec);
  nsresult rv;

  PRInt32 offset = url.FindChar(':');
  if (offset <= 0)
     return NS_ERROR_FAILURE;

  PRInt32 offset2= url.Find("mailto:", PR_TRUE);

  if (offset2 == 0) {
    topic += "mailto";

	/* I know about all that is going on regarding using window.open
     * instead of showWindowWithArgs(). But, I really don't have another
     * option in this case to invoke the messenger compose window.
     * This piece of code will eventually go away when I start using the 
     * protocol registries in NECKO
     */

	NS_WITH_SERVICE(nsIDOMToolkitCore, toolkitCore, kToolkitCoreCID, &rv)
	if (NS_FAILED(rv))
		return rv;
  /* Messenger doesn't understand to:xyz@domain.com,subject="xyz" yet.
   * So, just pass the type and mode info
   */
	rv = toolkitCore->ShowWindowWithArgs("chrome://messengercompose/content",
		                                    nsnull,
		                                   "type=0,mode=0");
	if (NS_FAILED(rv))
		return rv;
  }
  else {
    topic += "browser";
  }

  return NS_OK;
  
}

NS_IMETHODIMP
nsWebShellWindow::IsIntrinsicallySized(PRBool& aResult)
{
  aResult = mIntrinsicallySized;
  return NS_OK;
}


// nsIPrompt
NS_IMETHODIMP nsWebShellWindow::Alert(const PRUnichar *text)
{
  nsresult rv; 
  nsCOMPtr<nsIDOMWindow> domWindow;
  nsIWebShell* tempWebShell;
  GetWebShell(tempWebShell  );
  nsCOMPtr<nsIWebShell> webShell( dont_AddRef(tempWebShell) );
  if (NS_FAILED(rv = ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow))))
  {
    NS_ERROR("Unable to retrieve the DOM window from the new web shell.");
    return rv;
  }
 
 NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
 if ( NS_SUCCEEDED( rv ) )
 	rv = dialog->Alert( domWindow, NULL,text );
  return rv; 
}

NS_IMETHODIMP nsWebShellWindow::Confirm(const PRUnichar *text, PRBool *_retval)
{
  nsresult rv; 
  nsCOMPtr<nsIDOMWindow> domWindow;
  nsIWebShell* tempWebShell;
  GetWebShell(tempWebShell  );
  nsCOMPtr<nsIWebShell> webShell( dont_AddRef(tempWebShell) );
  if (NS_FAILED(rv = ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow))))
  {
    NS_ERROR("Unable to retrieve the DOM window from the new web shell.");
    return rv;
  }
 
 NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
 if ( NS_SUCCEEDED( rv ) )
 	rv = dialog->Confirm( domWindow, NULL, text, _retval );
  return rv; 
}

NS_IMETHODIMP nsWebShellWindow::ConfirmCheck(const PRUnichar *text, const PRUnichar *checkMsg, PRBool *checkValue, PRBool *_retval)
{
	 nsresult rv; 
  nsCOMPtr<nsIDOMWindow> domWindow;
  nsIWebShell* tempWebShell;
  GetWebShell(tempWebShell  );
  nsCOMPtr<nsIWebShell> webShell( dont_AddRef(tempWebShell) );
  if (NS_FAILED(rv = ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow))))
  {
    NS_ERROR("Unable to retrieve the DOM window from the new web shell.");
    return rv;
  }
 
 NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
 if ( NS_SUCCEEDED( rv ) )
 	rv =dialog->ConfirmCheck( domWindow,NULL, text, checkMsg, checkValue, _retval );
  return rv; 
}

NS_IMETHODIMP nsWebShellWindow::Prompt(const PRUnichar *text, const PRUnichar *defaultText, PRUnichar **result, PRBool *_retval)
{
  nsresult rv; 
  nsCOMPtr<nsIDOMWindow> domWindow;
  nsIWebShell* tempWebShell;
  GetWebShell(tempWebShell  );
  nsCOMPtr<nsIWebShell> webShell( dont_AddRef(tempWebShell) );
  if (NS_FAILED(rv = ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow))))
  {
    NS_ERROR("Unable to retrieve the DOM window from the new web shell.");
    return rv;
  }
 
 NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
 if ( NS_SUCCEEDED( rv ) )
 	rv = dialog->Prompt( domWindow, NULL, text, defaultText, result, _retval );
  return rv; 
}

NS_IMETHODIMP nsWebShellWindow::PromptUsernameAndPassword(const PRUnichar *text, PRUnichar **user, PRUnichar **pwd, PRBool *_retval)
{	
  nsresult rv; 
  nsCOMPtr<nsIDOMWindow> domWindow;
  nsIWebShell* tempWebShell;
  GetWebShell(tempWebShell  );
  nsCOMPtr<nsIWebShell> webShell( dont_AddRef(tempWebShell) );
  if (NS_FAILED(rv = ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow))))
  {
    NS_ERROR("Unable to retrieve the DOM window from the new web shell.");
    return rv;
  }
 
 NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
 if ( NS_SUCCEEDED( rv ) )
 	rv = dialog->PromptUsernameAndPassword( domWindow, NULL, text, user, pwd, _retval );
  return rv;
}

NS_IMETHODIMP nsWebShellWindow::PromptPassword(const PRUnichar *text, PRUnichar **pwd, PRBool *_retval)
{
  nsresult rv; 
  nsCOMPtr<nsIDOMWindow> domWindow;
  nsIWebShell* tempWebShell;
  GetWebShell(tempWebShell  );
  nsCOMPtr<nsIWebShell> webShell( dont_AddRef(tempWebShell) );
  if (NS_FAILED(rv = ConvertWebShellToDOMWindow(webShell, getter_AddRefs(domWindow))))
  {
    NS_ERROR("Unable to retrieve the DOM window from the new web shell.");
    return rv;
  }
 
 NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
 if ( NS_SUCCEEDED( rv ) )
 	rv = dialog->PromptPassword( domWindow, NULL, text, pwd, _retval );
  return rv;
}

NS_IMETHODIMP nsWebShellWindow::ConfirmYN(const PRUnichar *text, PRBool *_retval)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP nsWebShellWindow::ConfirmCheckYN(const PRUnichar *text, const PRUnichar *checkMsg, PRBool *checkValue, PRBool *_retval)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIModalWindowSupport interface
nsresult nsWebShellWindow::PrepareModality()
{
  nsresult rv;
  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  if (NS_FAILED(rv))
    return rv;
  return appShell->PushThreadEventQueue();
}

nsresult nsWebShellWindow::FinishModality()
{
  nsresult rv;
  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  if (NS_FAILED(rv))
    return rv;
  return appShell->PopThreadEventQueue();
}

