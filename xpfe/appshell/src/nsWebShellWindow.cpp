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

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIPref.h"

#include "nsGUIEvent.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIAppShell.h"
#include "nsIXULWindowCallbacks.h"

#include "nsIAppShellService.h"
#include "nsIWidgetController.h"
#include "nsAppShellCIDs.h"

#include "nsXULCommand.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNodeList.h"

#include "nsIMenuBar.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"

// For JS Execution
#include "nsIScriptContextOwner.h"

#include "nsXPComCIID.h"
#include "nsIEventQueueService.h"
#include "plevent.h"
#include "prmem.h"

// XXX: Only needed for the creation of the widget controller...
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDocumentLoader.h"
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
		

/* Define Class IDs */
static NS_DEFINE_IID(kWindowCID,           NS_WINDOW_CID);
static NS_DEFINE_IID(kWebShellCID,         NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kAppShellServiceCID,  NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kAppShellCID,         NS_APPSHELL_CID);

static NS_DEFINE_IID(kMenuBarCID,          NS_MENUBAR_CID);
static NS_DEFINE_IID(kMenuCID,             NS_MENU_CID);
static NS_DEFINE_IID(kMenuItemCID,         NS_MENUITEM_CID);

static NS_DEFINE_CID(kPrefCID,             NS_PREF_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);


/* Define Interface IDs */
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellWindowIID,     NS_IWEBSHELL_WINDOW_IID);
static NS_DEFINE_IID(kIWidgetIID,             NS_IWIDGET_IID);
static NS_DEFINE_IID(kIWebShellIID,           NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWebShellContainerIID,  NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIAppShellServiceIID,    NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kIAppShellIID,           NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIWidgetControllerIID,   NS_IWIDGETCONTROLLER_IID);
static NS_DEFINE_IID(kIDocumentLoaderObserverIID, NS_IDOCUMENT_LOADER_OBSERVER_IID);
static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kIDOMDocumentIID,        NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMCharacterDataIID,   NS_IDOMCHARACTERDATA_IID);
//static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
//static NS_DEFINE_IID(kIDOMHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);

static NS_DEFINE_IID(kIMenuIID,       NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID,    NS_IMENUBAR_IID);
static NS_DEFINE_IID(kIMenuItemIID,   NS_IMENUITEM_IID);
static NS_DEFINE_IID(kIXULCommandIID, NS_IXULCOMMAND_IID);
static NS_DEFINE_IID(kIContentIID,    NS_ICONTENT_IID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

#ifdef DEBUG_rods
#define DEBUG_MENUSDEL 1
#endif

#include "nsIWebShell.h"

const char * kThrobberOnStr  = "resource:/res/throbber/anims07.gif";
const char * kThrobberOffStr = "resource:/res/throbber/anims00.gif";

struct ThreadedWindowEvent {
  PLEvent           event;
  nsWebShellWindow  *window;
};

nsWebShellWindow::nsWebShellWindow()
{
  NS_INIT_REFCNT();

  mWebShell = nsnull;
  mWindow   = nsnull;
  mController = nsnull;
  mCallbacks = nsnull;
  mContinueModalLoop = PR_FALSE;
  mChromeInitialized = PR_FALSE;
}


nsWebShellWindow::~nsWebShellWindow()
{
  if (nsnull != mWebShell) {
    mWebShell->Destroy();
    NS_RELEASE(mWebShell);
  }

  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mController);
  NS_IF_RELEASE(mCallbacks);
}


NS_IMPL_ADDREF(nsWebShellWindow);
NS_IMPL_RELEASE(nsWebShellWindow);

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
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIWebShellContainer*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return rv;
}


nsresult nsWebShellWindow::Initialize(nsIWebShellWindow* aParent,
                                      nsIAppShell* aShell, nsIURL* aUrl, 
                                      nsString& aControllerIID, nsIStreamObserver* anObserver,
                                      nsIXULWindowCallbacks *aCallbacks,
                                      PRInt32 aInitialWidth, PRInt32 aInitialHeight)
{
  nsresult rv;

  nsString urlString;
  nsIWidget *parentWidget;
  const char *tmpStr = NULL;
  nsIID iid;
  char str[40];


  // XXX: need to get the default window size from prefs...
  nsRect r(0, 0, aInitialWidth, aInitialHeight);
  
  nsWidgetInitData initData;


  //if (nsnull == aUrl) {
  //  rv = NS_ERROR_NULL_POINTER;
  //  goto done;
  //}
  if (nsnull != aUrl)  {
    aUrl->GetSpec(&tmpStr);
    urlString = tmpStr;
  }

  // Create top level window
  rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID,
                                    (void**)&mWindow);
  if (NS_OK != rv) {
    return rv;
  }

  initData.mBorderStyle = eBorderStyle_dialog;

  if (!aParent || NS_FAILED(aParent->GetWidget(parentWidget)))
    parentWidget = nsnull;

  mWindow->SetClientData(this);
  mWindow->Create(parentWidget,                       // Parent nsIWidget
                  r,                                  // Widget dimensions
                  nsWebShellWindow::HandleEvent,      // Event handler function
                  nsnull,                             // Device context
                  aShell,                             // Application shell
                  nsnull,                             // nsIToolkit
                  &initData);                         // Widget initialization data
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
    mWebShell->SetPrefs(prefs);
    nsServiceManager::ReleaseService(kPrefCID, prefs);
  }

  NS_IF_RELEASE(mCallbacks);
  mCallbacks = aCallbacks;
  NS_IF_ADDREF(mCallbacks);

  if (nsnull != aUrl)  {
    mWebShell->LoadURL(urlString);
  }

  // Create the IWidgetController for the document...
  mController = nsnull; 
  aControllerIID.ToCString(str, sizeof(str));
  iid.Parse(str);

  //rv = nsComponentManager::CreateInstance(iid, nsnull,
  //                                  kIWidgetControllerIID,
  //                                  (void**)&mController);
  return rv;
}


/*
 * Close the window
 */
NS_METHOD
nsWebShellWindow::Close()
{
  ExitModalLoop();
  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mWebShell);
  /* note: this next Release() seems like the right thing to do, but it doesn't
     appear exactly necessary, and we are afraid of possible repercussions
     unexplored at this time.  ("this time" being a stability release crunch.)
     Revisit this later!?
  */
//  Release();
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
                             PRInt32 aStatus)
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

    // Begin menuitem inner loop
    nsCOMPtr<nsIDOMNode> menuitemNode;
    aMenuNode->GetFirstChild(getter_AddRefs(menuitemNode));
    while (menuitemNode) {
      nsCOMPtr<nsIDOMElement> menuitemElement(do_QueryInterface(menuitemNode));
      if (menuitemElement) {
        nsString menuitemNodeType;
        nsString menuitemName;
        menuitemElement->GetNodeName(menuitemNodeType);
        if (menuitemNodeType.Equals("menuitem")) {
          // LoadMenuItem
          LoadMenuItem(pnsMenu, menuitemElement, menuitemNode);
        } else if (menuitemNodeType.Equals("separator")) {
          pnsMenu->AddSeparator();
        } else if (menuitemNodeType.Equals("menu")) {
          // Load a submenu
          LoadSubMenu(pnsMenu, menuitemElement, menuitemNode);
        }
      }
      nsCOMPtr<nsIDOMNode> oldmenuitemNode(menuitemNode);
      oldmenuitemNode->GetNextSibling(getter_AddRefs(menuitemNode));
    } // end menu item innner loop
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

  menuitemElement->GetAttribute(nsAutoString("name"), menuitemName);
  menuitemElement->GetAttribute(nsAutoString("cmd"), menuitemCmd);
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull, kIMenuItemIID, (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    pnsMenuItem->Create(pParentMenu, menuitemName, 0);                 
    // Set nsMenuItem Name
    //pnsMenuItem->SetLabel(menuitemName);
    // Make nsMenuItem a child of nsMenu
    //pParentMenu->AddMenuItem(pnsMenuItem);
    nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(kISupportsIID, (void**) &supports);
    pParentMenu->AddItem(supports);
    NS_RELEASE(supports);
          
    // Create MenuDelegate - this is the intermediator inbetween 
    // the DOM node and the nsIMenuItem
    // The nsWebShellWindow wacthes for Document changes and then notifies the 
    // the appropriate nsMenuDelegate object
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(menuitemNode));
    if (!domElement) {
      return NS_ERROR_FAILURE;
    }

    nsAutoString cmdAtom("onclick");
    nsString cmdName;

    domElement->GetAttribute(cmdAtom, cmdName);

    nsXULCommand * menuDelegate = new nsXULCommand();
    menuDelegate->SetCommand(cmdName);
    menuDelegate->SetWebShell(mWebShell);
    menuDelegate->SetDOMElement(domElement);
    menuDelegate->SetMenuItem(pnsMenuItem);
    nsIXULCommand * icmd;
    if (NS_OK == menuDelegate->QueryInterface(kIXULCommandIID, (void**) &icmd)) {
      mMenuDelegates.AppendElement(icmd);
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
    }
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
  menuElement->GetAttribute(nsAutoString("name"), menuName);
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

    // Set nsMenu Name
    pnsMenu->SetLabel(menuName); 
    // Make nsMenu a child of parent nsMenu
    //pParentMenu->AddMenu(pnsMenu);
    supports = nsnull;
    pnsMenu->QueryInterface(kISupportsIID, (void**) &supports);
	pParentMenu->AddItem(supports);
	NS_RELEASE(supports);

    // Begin menuitem inner loop
    nsCOMPtr<nsIDOMNode> menuitemNode;
    menuNode->GetFirstChild(getter_AddRefs(menuitemNode));
    while (menuitemNode) {
      nsCOMPtr<nsIDOMElement> menuitemElement(do_QueryInterface(menuitemNode));
      if (menuitemElement) {
        nsString menuitemNodeType;
        menuitemElement->GetNodeName(menuitemNodeType);

#ifdef DEBUG_saari
        printf("Type [%s] %d\n", menuitemNodeType.ToNewCString(), menuitemNodeType.Equals("separator"));
#endif

        if (menuitemNodeType.Equals("menuitem")) {
          // Load a menuitem
          LoadMenuItem(pnsMenu, menuitemElement, menuitemNode);
        } else if (menuitemNodeType.Equals("separator")) {
          pnsMenu->AddSeparator();
        } else if (menuitemNodeType.Equals("menu")) {
          // Add a submenu
          LoadSubMenu(pnsMenu, menuitemElement, menuitemNode);
        }
      }
      nsCOMPtr<nsIDOMNode> oldmenuitemNode(menuitemNode);
      oldmenuitemNode->GetNextSibling(getter_AddRefs(menuitemNode));
    } // end menu item innner loop
  }     
}

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
              menuElement->GetAttribute(nsAutoString("name"), menuName);

#ifdef DEBUG_rods
              printf("Creating Menu [%s] \n", menuName.ToNewCString()); // this leaks
#endif
              CreateMenu(pnsMenuBar, menuNode, menuName);
            } 

          }
          nsCOMPtr<nsIDOMNode> oldmenuNode(menuNode);  
          oldmenuNode->GetNextSibling(getter_AddRefs(menuNode));
        } // end while (nsnull != menuNode)
          
        // Give the aParentWindow this nsMenuBar to hold onto.
        aParentWindow->SetMenuBar(pnsMenuBar);
      
        // HACK: force a paint for now
        pnsMenuBar->Paint();
        
        // HACK for M4, should be removed by M5
        #ifdef XP_MAC
        Handle tempMenuBar = ::GetMenuBar(); // Get a copy of the menu list
		pnsMenuBar->SetNativeData((void*)tempMenuBar);
		#endif
    } // end if ( nsnull != pnsMenuBar )
    }
  } // end if (menuBar)

} // nsWebShellWindow::LoadMenus


NS_IMETHODIMP 
nsWebShellWindow::NewWebShell(PRUint32 aChromeMask, PRBool aVisible,
                              nsIWebShell *&aNewWebShell)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebShellWindow::FindWebShellWithName(const PRUnichar* aName,
                                                     nsIWebShell*& aResult)
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::FocusAvailable(nsIWebShell* aFocusedWebShell)
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
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::ShowModal()
{
  nsresult  rv;

#if 0
  // a nice though unnecessary shortcut should we happen to already be in the right thread
  if (PR_CurrentThread() == mozilla_thread)
    rv = ShowModalInternal();
  else {
    ThreadedWindowEvent *event = PR_NEW(ThreadedWindowEvent);
    if (!event)
      rv = NS_ERROR_OUT_OF_MEMORY;
    else {
      PL_InitEvent(&event->event, NULL, HandleModalDialogEvent, DestroyModalDialogEvent);
      event->window = this;

      nsIEventQueueService *eQueueService;
      PLEventQueue         *eQueue;
      rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                        kIEventQueueServiceIID,
                                        (nsISupports **)&eQueueService);
      if (NS_SUCCEEDED(rv)) {
        rv = eQueueService->GetThreadEventQueue(mozilla_thread, &eQueue);
        NS_RELEASE(eQueueService);
//        nsServiceManager::ReleaseService(kEventQueueServiceCID, eQueue); (?)
        PL_PostSynchronousEvent(eQueue, &event->event);
      }
      rv = NS_OK;
    }
  }
#else
  rv = ShowModalInternal();
#endif

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


NS_IMETHODIMP 
nsWebShellWindow::GetWebShell(nsIWebShell *& aWebShell)
{
  aWebShell = mWebShell;
  NS_ADDREF(mWebShell);
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
nsWebShellWindow::OnStartDocumentLoad(nsIURL* aURL, const char* aCommand)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnEndDocumentLoad(nsIURL* aURL, PRInt32 aStatus)
{
#ifdef DEBUG_MENUSDEL
  printf("OnEndDocumentLoad\n");
#endif

  /* We get notified every time a page/Frame is loaded. But we need to
   * Load the menus, run the startup script etc.. only once. So, Use
   * the mChrome Initialized  member to check whether chrome should be 
   * initialized or not - Radha
   *
   * This breaks file download (where I'm using a single window with
   * two separate xul files).  I'm turning this off for now... - Law
   */
  if (0 && mChromeInitialized)
    return NS_OK;

  mChromeInitialized = PR_TRUE;

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

  ExecuteStartupCode();

  ///////////////////////////////
  // Find the Menubar DOM  and Load the menus, hooking them up to the loaded commands
  ///////////////////////////////
  nsCOMPtr<nsIDOMDocument> menubarDOMDoc(GetNamedDOMDoc(nsAutoString("this"))); // XXX "this" is a small kludge for code reused
  if (menubarDOMDoc)
    LoadMenus(menubarDOMDoc, mWindow);

  SetSizeFromXUL();
  SetTitleFromXUL();

#if 0
  nsCOMPtr<nsIDOMDocument> toolbarDOMDoc(GetNamedDOMDoc(nsAutoString("browser.toolbar")));
  nsCOMPtr<nsIDOMDocument> contentDOMDoc(GetNamedDOMDoc(nsAutoString("browser.webwindow")));
  nsCOMPtr<nsIDocument> contentDoc(do_QueryInterface(contentDOMDoc));
  nsCOMPtr<nsIDocument> statusDoc(do_QueryInterface(statusDOMDoc));
  nsCOMPtr<nsIDocument> toolbarDoc(do_QueryInterface(toolbarDOMDoc));

  nsIWebShell* statusWebShell = nsnull;
  mWebShell->FindChildWithName(nsAutoString("browser.status"), statusWebShell);

  PRInt32 actualStatusHeight  = GetDocHeight(statusDoc);
  PRInt32 actualToolbarHeight = GetDocHeight(toolbarDoc);


  PRInt32 height = 0;
  PRInt32 x,y,w,h;
  PRInt32 contentHeight;
  PRInt32 toolbarHeight;
  PRInt32 statusHeight;

  mWebShell->GetBounds(x, y, w, h);
  toolbarWebShell->GetBounds(x, y, w, toolbarHeight);
  contentWebShell->GetBounds(x, y, w, contentHeight);
  statusWebShell->GetBounds(x, y, w, statusHeight); 

  //h = toolbarHeight + contentHeight + statusHeight;
  contentHeight = h - actualStatusHeight - actualToolbarHeight;

  toolbarWebShell->GetBounds(x, y, w, h);
  toolbarWebShell->SetBounds(x, y, w, actualToolbarHeight);

  contentWebShell->GetBounds(x, y, w, h);
  contentWebShell->SetBounds(x, y, w, contentHeight);

  statusWebShell->GetBounds(x, y, w, h);
  statusWebShell->SetBounds(x, y, w, actualStatusHeight);
#endif

  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::OnStartURLLoad(nsIURL* aURL, const char* aContentType, nsIContentViewer* aViewer)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnProgressURLLoad(nsIURL* aURL, PRUint32 aProgress, 
                                    PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnStatusURLLoad(nsIURL* aURL, nsString& aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnEndURLLoad(nsIURL* aURL, PRInt32 aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::HandleUnknownContentType(nsIURL* aURL,
                                           const char *aContentType,
                                           const char *aCommand )
{
  return NS_OK;
}


//----------------------------------------
nsCOMPtr<nsIDOMNode> nsWebShellWindow::FindNamedDOMNode(const nsString &aName, nsIDOMNode * aParent, PRInt32 & aCount, PRInt32 aEndCount)
{
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
    mWebShell->FindChildWithName(aWebShellName, *getter_AddRefs(childWebShell));
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
#if 0
NS_IMETHODIMP nsWebShellWindow::OnConnectionsComplete()
{
#ifdef DEBUG_MENUSDEL
  printf("OnConnectionsComplete\n");
#endif

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

  ExecuteStartupCode();

  ///////////////////////////////
  // Find the Menubar DOM  and Load the menus, hooking them up to the loaded commands
  ///////////////////////////////
  nsCOMPtr<nsIDOMDocument> menubarDOMDoc(GetNamedDOMDoc(nsAutoString("this"))); // XXX "this" is a small kludge for code reused
  if (menubarDOMDoc)
    LoadMenus(menubarDOMDoc, mWindow);

  SetSizeFromXUL();
  SetTitleFromXUL();

#if 0
  nsCOMPtr<nsIDOMDocument> toolbarDOMDoc(GetNamedDOMDoc(nsAutoString("browser.toolbar")));
  nsCOMPtr<nsIDOMDocument> contentDOMDoc(GetNamedDOMDoc(nsAutoString("browser.webwindow")));
  nsCOMPtr<nsIDocument> contentDoc(do_QueryInterface(contentDOMDoc));
  nsCOMPtr<nsIDocument> statusDoc(do_QueryInterface(statusDOMDoc));
  nsCOMPtr<nsIDocument> toolbarDoc(do_QueryInterface(toolbarDOMDoc));

  nsIWebShell* statusWebShell = nsnull;
  mWebShell->FindChildWithName(nsAutoString("browser.status"), statusWebShell);

  PRInt32 actualStatusHeight  = GetDocHeight(statusDoc);
  PRInt32 actualToolbarHeight = GetDocHeight(toolbarDoc);


  PRInt32 height = 0;
  PRInt32 x,y,w,h;
  PRInt32 contentHeight;
  PRInt32 toolbarHeight;
  PRInt32 statusHeight;

  mWebShell->GetBounds(x, y, w, h);
  toolbarWebShell->GetBounds(x, y, w, toolbarHeight);
  contentWebShell->GetBounds(x, y, w, contentHeight);
  statusWebShell->GetBounds(x, y, w, statusHeight); 

  //h = toolbarHeight + contentHeight + statusHeight;
  contentHeight = h - actualStatusHeight - actualToolbarHeight;

  toolbarWebShell->GetBounds(x, y, w, h);
  toolbarWebShell->SetBounds(x, y, w, actualToolbarHeight);

  contentWebShell->GetBounds(x, y, w, h);
  contentWebShell->SetBounds(x, y, w, contentHeight);

  statusWebShell->GetBounds(x, y, w, h);
  statusWebShell->SetBounds(x, y, w, actualStatusHeight);
#endif

  return NS_OK;
} // nsWebShellWindow::OnConnectionsComplete 
#endif  /* 0 */



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

void nsWebShellWindow::ExecuteJavaScriptString(nsString& aJavaScript)
{
  if (aJavaScript.Length() == 0) {
    return;
  }
  
  // Get nsIScriptContextOwner
  nsCOMPtr<nsIScriptContextOwner> scriptContextOwner ( do_QueryInterface(mWebShell) );
  if ( scriptContextOwner ) {
    const char* url = "";
      // Get nsIScriptContext
    nsCOMPtr<nsIScriptContext> scriptContext;
    nsresult status = scriptContextOwner->GetScriptContext(getter_AddRefs(scriptContext));
    if (NS_OK == status) {
      // Ask the script context to evalute the javascript string
      PRBool isUndefined = PR_FALSE;
      nsString rVal("xxx");
      scriptContext->EvaluateString(aJavaScript, url, 0, rVal, &isUndefined);

#ifdef DEBUG_MENUSDEL
      printf("EvaluateString - %d [%s]\n", isUndefined, rVal.ToNewCString());
#endif
    }

  }
}


/**
 * Execute window onLoad handler
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
  if (webshellElement && NS_SUCCEEDED(webshellElement->GetAttribute("onload", startupCode)))
    ExecuteJavaScriptString(startupCode);

  if (mCallbacks)
    mCallbacks->ConstructAfterJavaScript(mWebShell);

}


/* A somewhat early version of window sizing code.  This simply reads attributes
   from the window tag and blindly sets the size to whatever it finds within.
*/
void nsWebShellWindow::SetSizeFromXUL()
{
  nsCOMPtr<nsIDOMNode> webshellNode = GetDOMNodeFromWebShell(mWebShell);
  nsIWidget *windowWidget = GetWidget();
  nsCOMPtr<nsIDOMElement> webshellElement;
  nsString sizeString;
  PRInt32 errorCode,
          specWidth, specHeight,
          specSize;
  nsRect  currentSize;

  if (webshellNode)
    webshellElement = do_QueryInterface(webshellNode);
  if (!webshellElement || !windowWidget) // it's hopeless
    return;

  // first guess: use current size
  mWindow->GetBounds(currentSize);
  specWidth = currentSize.width;
  specHeight = currentSize.height;

  // read "height" attribute
  if (NS_SUCCEEDED(webshellElement->GetAttribute("height", sizeString))) {
    specSize = sizeString.ToInteger(&errorCode);
    if (NS_SUCCEEDED(errorCode) && specSize > 0)
      specHeight = specSize;
  }

  // read "width" attribute
  if (NS_SUCCEEDED(webshellElement->GetAttribute("width", sizeString))) {
    specSize = sizeString.ToInteger(&errorCode);
    if (NS_SUCCEEDED(errorCode) || specSize > 0)
      specWidth = specSize;
  }

  if (specWidth != currentSize.width || specHeight != currentSize.height)
    windowWidget->Resize(specWidth, specHeight, PR_TRUE);

#if 0
  // adjust height to fit contents?
  if (fitHeight == PR_TRUE) {
    nsCOMPtr<nsIContentViewer> cv;
    mWebShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
      if (docv) {
        nsCOMPtr<nsIDocument> doc;
        docv->GetDocument(*getter_AddRefs(doc));
        if (doc)
          specHeight = GetDocHeight(doc);
      }
    }
    mWindow->GetBounds(currentSize);
    windowWidget->Resize(currentSize.width, specHeight, PR_TRUE);
  }
#endif
} // SetSizeFromXUL


void nsWebShellWindow::SetTitleFromXUL()
{
  nsCOMPtr<nsIDOMNode> webshellNode = GetDOMNodeFromWebShell(mWebShell);
  nsIWidget *windowWidget = GetWidget();
  nsCOMPtr<nsIDOMElement> webshellElement;
  nsString windowTitle;

  if (webshellNode)
    webshellElement = do_QueryInterface(webshellNode);
  if (webshellElement && windowWidget &&
      NS_SUCCEEDED(webshellElement->GetAttribute("title", windowTitle)))
    windowWidget->SetTitle(windowTitle);
} // SetTitleFromXUL


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
nsWebShellWindow::ContentStateChanged(nsIDocument *aDocument,
                                      nsIContent* aContent)
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
