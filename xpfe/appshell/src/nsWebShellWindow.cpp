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

#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"

#include "nsGUIEvent.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIAppShell.h"

#include "nsIAppShellService.h"
#include "nsIWidgetController.h"
#include "nsAppShellCIDs.h"

#include "nsIXULCommand.h"
#include "nsXULCommand.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNodeList.h"

#include "nsIMenuBar.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"

// XXX: Only needed for the creation of the widget controller...
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDocumentLoader.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLImageElement.h"

// For calculating size
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"


/* Define Class IDs */
static NS_DEFINE_IID(kWindowCID,           NS_WINDOW_CID);
static NS_DEFINE_IID(kWebShellCID,         NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kAppShellServiceCID,  NS_APPSHELL_SERVICE_CID);

static NS_DEFINE_IID(kMenuBarCID,          NS_MENUBAR_CID);
static NS_DEFINE_IID(kMenuCID,             NS_MENU_CID);
static NS_DEFINE_IID(kMenuItemCID,         NS_MENUITEM_CID);
static NS_DEFINE_IID(kXULCommandCID,       NS_XULCOMMAND_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellWindowIID,     NS_IWEBSHELL_WINDOW_IID);
static NS_DEFINE_IID(kIWidgetIID,             NS_IWIDGET_IID);
static NS_DEFINE_IID(kIWebShellIID,           NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWebShellContainerIID,  NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIAppShellServiceIID,    NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kIWidgetControllerIID,   NS_IWIDGETCONTROLLER_IID);
static NS_DEFINE_IID(kIDocumentLoaderObserverIID, NS_IDOCUMENT_LOADER_OBSERVER_IID);
static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kIDOMDocumentIID,        NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIXULCommandIID,         NS_IXULCOMMAND_IID);
static NS_DEFINE_IID(kIDOMCharacterDataIID,   NS_IDOMCHARACTERDATA_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);

static NS_DEFINE_IID(kIMenuIID,     NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID,  NS_IMENUBAR_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);

#define DEBUGCMDS 0

#include "nsIWebShell.h"

const char * kThrobberOnStr  = "resource:/res/throbber/anims07.gif";
const char * kThrobberOffStr = "resource:/res/throbber/anims00.gif";

nsWebShellWindow::nsWebShellWindow()
{
  NS_INIT_REFCNT();

  mWebShell = nsnull;
  mWindow   = nsnull;
  mController = nsnull;
  mStatusText = nsnull;
  mURLBarText = nsnull;
  mThrobber   = nsnull;
}


nsWebShellWindow::~nsWebShellWindow()
{
  if (nsnull != mWebShell) {
    mWebShell->Destroy();
    NS_RELEASE(mWebShell);
  }

  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mController);
  NS_IF_RELEASE(mStatusText);
  NS_IF_RELEASE(mURLBarText);
  NS_IF_RELEASE(mThrobber);
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


nsresult nsWebShellWindow::Initialize(nsIAppShell* aShell, nsIURL* aUrl, 
                                      nsString& aControllerIID, nsIStreamObserver* anObserver)
{
  return Initialize(nsnull, aShell, aUrl, aControllerIID, anObserver);
}

nsresult nsWebShellWindow::Initialize(nsIWidget* aParent,
                                      nsIAppShell* aShell, nsIURL* aUrl, 
                                      nsString& aControllerIID, nsIStreamObserver* anObserver)
{
  nsresult rv;

  nsString urlString;
  const char *tmpStr = NULL;
  nsIID iid;
  char str[40];


  // XXX: need to get the default window size from prefs...
  nsRect r(0, 0, 650, 618);
  
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
  rv = nsRepository::CreateInstance(kWindowCID, nsnull, kIWidgetIID,
                                    (void**)&mWindow);
  if (NS_OK != rv) {
    goto done;
  }

  initData.mBorderStyle = eBorderStyle_dialog;

  mWindow->SetClientData(this);
  mWindow->Create((nsIWidget*)aParent,                // Parent nsIWidget
                  r,                                  // Widget dimensions
                  nsWebShellWindow::HandleEvent,      // Event handler function
                  nsnull,                             // Device context
                  aShell,                             // Application shell
                  nsnull,                             // nsIToolkit
                  &initData);                         // Widget initialization data
  mWindow->GetClientBounds(r);
  mWindow->SetBackgroundColor(NS_RGB(192,192,192));

  // Create web shell
  rv = nsRepository::CreateInstance(kWebShellCID, nsnull,
                                    kIWebShellIID,
                                    (void**)&mWebShell);
  if (NS_OK != rv) {
    goto done;
  }

  r.x = r.y = 0;
  rv = mWebShell->Init(mWindow->GetNativeData(NS_NATIVE_WIDGET), 
                       r.x, r.y, r.width, r.height,
                       nsScrollPreference_kNeverScroll, 
                       PR_TRUE,                     // Allow Plugins 
                       PR_TRUE);
  mWebShell->SetContainer(this);
  mWebShell->SetObserver((nsIStreamObserver*)anObserver);

  nsIDocumentLoader * docLoader;
  mWebShell->GetDocumentLoader(docLoader);
  if (nsnull != docLoader) {
    docLoader->AddObserver((nsIDocumentLoaderObserver *)this);
  }
///  webShell->SetPrefs(aPrefs);

  if (nsnull != aUrl)  {
    mWebShell->LoadURL(urlString);
  }

  // Create the IWidgetController for the document...
  mController = nsnull; 
  aControllerIID.ToCString(str, sizeof(str));
  iid.Parse(str);

  //rv = nsRepository::CreateInstance(iid, nsnull,
  //                                  kIWidgetControllerIID,
  //                                  (void**)&mController);
done:
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
  nsresult rv;
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
        nsIAppShellService* appShell;

        rv = nsServiceManager::GetService(kAppShellServiceCID,
                                          kIAppShellServiceIID,
                                          (nsISupports**)&appShell);
        if (NS_SUCCEEDED(rv)) {
          appShell->CloseTopLevelWindow(aEvent->widget);
          
          nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
        }
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
  if (nsnull != mThrobber) {
    mThrobber->SetSrc(kThrobberOnStr);
  }

  nsAutoString url(aURL);
  nsAutoString gecko("Gecko - ");
  gecko.Append(url);
 
  mWindow->SetTitle(gecko);

  if (nsnull != mURLBarText) {
    mURLBarText->SetValue(url);
  }

  if (nsnull != mStatusText) {
    nsAutoString msg(aURL);
    msg.Append(" :Start");
    mStatusText->SetData(msg);
  }
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
  if (nsnull != mStatusText) {
    nsAutoString url(aURL);
    url.Append(": progress ");
    url.Append(aProgress, 10);
    if (0 != aProgressMax) {
      url.Append(" (out of ");
      url.Append(aProgressMax, 10);
      url.Append(")");
    }
    mStatusText->SetData(url);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShellWindow::EndLoadURL(nsIWebShell* aWebShell, const PRUnichar* aURL,
                             PRInt32 aStatus)
{
  if (nsnull != mThrobber) {
    mThrobber->SetSrc(kThrobberOffStr);
  }
   
  if (nsnull != mStatusText) {
    nsAutoString msg(aURL);
    msg.Append(" :Stop");
    mStatusText->SetData(msg);
  }

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

  nsCOMPtr<nsIDOMNode> parent(element);
  if (!parent)
    return node;

  parent->GetFirstChild(getter_AddRefs(node));
  while (node) {
    nsString name;
    node->GetNodeName(name);
    printf("Looking for [%s] [%s]\n", aName.ToNewCString(), name.ToNewCString()); // this leaks
    if (name.Equals(aName))
      return node;
    nsCOMPtr<nsIDOMNode> oldNode(node);
    oldNode->GetNextSibling(getter_AddRefs(node));
  }
  node = nsnull;
  return node;
}

#if 0
//----------------------------------------
void nsWebShellWindow::LoadCommands(nsIWebShell * aWebShell, nsIDOMDocument * aDOMDoc) 
{
  nsresult rv = NS_NOINTERFACE;
  
  nsCOMPtr<nsIDOMElement> element;
  aDOMDoc->GetDocumentElement(getter_AddRefs(element));
  if (!element)
    return;
  nsCOMPtr<nsIDOMNode> window(element);
  
  // load up commands
  PRInt32 endCount = 0;
  nsCOMPtr<nsIDOMNode> parentCmd(FindNamedDOMNode(nsAutoString("commands"), window, endCount, 1));
  if ( parentCmd ) {
    nsCOMPtr<nsIDOMNode> node;
    parentCmd->GetFirstChild(getter_AddRefs(node));
    while ( node ) {
      nsString value;
      nsString nodeType;
      nsString name;
      nsCOMPtr<nsIDOMElement> element( node );
      if ( element ) {
        element->GetNodeName(nodeType);
        if (nodeType.Equals("command")) {
          element->GetAttribute(nsAutoString("name"), name);
          element->GetAttribute(nsAutoString("onCommand"), value);

          /*nsIXULCommand * xulCmdInterface = nsnull;
          rv = nsRepository::CreateInstance(kXULCommandCID, nsnull, kIXULCommandIID,
            (void**)&xulCmdInterface);
          if (NS_OK == rv) {
            xulCmdInterface->SetName(name);//nsIXULCommand
            xulCmdInterface->SetCommand(value);//nsIXULCommand
            xulCmdInterface->SetWebShell(aWebShell);// Added to nsIXULCommand
            xulCmdInterface->SetDOMElement(element);// Added to nsIXULCommand
		        mCommands.AppendElement(xulCmdInterface);
          }*/

          
          if (DEBUGCMDS) printf("Creating cmd [%s]\n",name.ToNewCString());

          nsXULCommand * xulCmd = new nsXULCommand();
          xulCmd->SetName(name);//nsIXULCommand
          xulCmd->SetCommand(value);//nsIXULCommand
          xulCmd->SetWebShell(aWebShell);// Added to nsIXULCommand
          xulCmd->SetDOMElement(element);// Added to nsIXULCommand
          nsIXULCommand * icmd;
          if (NS_OK == xulCmd->QueryInterface(kIXULCommandIID, (void**) &icmd)) {
            mCommands.AppendElement(icmd);
          }
          
          //printf("Commands[%s] value[%s]\n", nsAutoCString(name), nsAutoCString(value));
        }
      }

      nsCOMPtr<nsIDOMNode> oldNode( node );
      oldNode->GetNextSibling(getter_AddRefs(node));
    }
  }

  // Now install the commands onto the Toolbar GUI Nodes for each toolbar in the toolbox
  PRInt32 count = 1;
  endCount = 0;
  nsCOMPtr<nsIDOMNode> toolbox(FindNamedDOMNode(nsAutoString("toolbox"), window, endCount, count));
  endCount = 0; 
  count = 1;
  nsCOMPtr<nsIDOMNode> toolbar(FindNamedDOMNode(nsAutoString("toolbar"), toolbox, endCount, count));

  while ( toolbar ) {
    nsAutoString cmdAtom("cmd");
    nsCOMPtr<nsIDOMNode> node;
    toolbar->GetFirstChild(getter_AddRefs(node));
    while ( node ) {
      nsAutoString value;
      nsString nodeCmd;
      nsCOMPtr<nsIDOMElement> element ( node );
      if ( element ) {
        nsString name;
        element->GetNodeName(name);
        printf("Element name is [%s]==[button]\n", name.ToNewCString());// this leaks
        if (name.Equals(nsAutoString("button")))
           ConnectCommandToOneGUINode(node, element, name);
        else if (name.Equals(nsAutoString("input"))) {
          nsXULCommand * xulCmd = new nsXULCommand();
          xulCmd->SetName(name);
          xulCmd->SetCommand(value);
          xulCmd->SetWebShell(aWebShell);
          xulCmd->SetDOMElement(element);
          nsIXULCommand * icmd;
          if (NS_OK == xulCmd->QueryInterface(kIXULCommandIID, (void**) &icmd)) {
            mCommands.AppendElement(icmd);
          }
          xulCmd->AddUINode(node);
          
          printf("Linking newly created cmd to input [%s]\n", name.ToNewCString()); // this leaks
        }
      }

      nsCOMPtr<nsIDOMNode> oldNode ( node );
      oldNode->GetNextSibling(getter_AddRefs(node));
    }
	// find the next toolbar
    endCount = 0;
    toolbar = FindNamedDOMNode(nsAutoString("toolbar"), toolbox, endCount, ++count); 
  } // for each toolbar
  
  // XXX hack until Command DOM is available
  // Enable All Command
  PRInt32 i, n = mCommands.Count();
  for (i = 0; i < n; i++) {
    nsXULCommand* cmd = (nsXULCommand*) mCommands.ElementAt(i);
    cmd->SetEnabled(PR_TRUE);
  }
  //SetCommandEnabled(nsAutoString("nsCmd:BrowserStop"), PR_FALSE);
  nsCOMPtr<nsIXULCommand> cmd(FindCommandByName(nsAutoString("nsCmd:StartUp")));
  if (cmd) {
    cmd->DoCommand();
  }
  
}
#endif

//----------------------------------------
void nsWebShellWindow::ConnectCommandToOneGUINode(nsIDOMNode* aGUINode)
{
  nsCOMPtr<nsIDOMElement> domElement(aGUINode);
  if (!domElement)
    return;

  nsAutoString cmdAtom("cmd");
  nsString cmdName;

  domElement->GetAttribute(cmdAtom, cmdName);
  nsCOMPtr<nsIXULCommand> cmd(FindCommandByName(cmdName));
  if (cmd) {
    cmd->AddUINode(aGUINode);
#ifdef DEBUGCMDS
    nsString nodeName, nodeType;
    char *cmdNameString, *nodeNameString, *nodeTypeString;

    cmdNameString = cmdName.ToNewCString();
    domElement->GetNodeName(nodeType);
    nodeTypeString = nodeType.ToNewCString();
    domElement->GetAttribute(nsAutoString("name"), nodeName);
    nodeNameString = nodeName.ToNewCString();
    printf("Linkind cmd [%s] to %s [%s]\n",
           cmdNameString, nodeTypeString, nodeNameString);
    delete [] nodeNameString;
    delete [] nodeTypeString;
    delete [] cmdNameString;
#endif
  }
} // nsWebShellWindow::ConnectCommandToOneGUINode


//----------------------------------------
void nsWebShellWindow::LoadMenus(nsIDOMDocument * aDOMDoc, nsIWidget * aParentWindow) 
{
  // locate the window element which holds toolbars and menus and commands
  nsCOMPtr<nsIDOMElement> element;
  aDOMDoc->GetDocumentElement(getter_AddRefs(element));
  if (!element)
    return;
  nsCOMPtr<nsIDOMNode> window(element);

  nsresult rv;
  int endCount = 0;
  nsCOMPtr<nsIDOMNode> menubarNode(FindNamedDOMNode(nsAutoString("menubar"), window, endCount, 1));
  if (menubarNode) {
    nsIMenuBar * pnsMenuBar = nsnull;
    rv = nsRepository::CreateInstance(kMenuBarCID, nsnull, kIMenuBarIID, (void**)&pnsMenuBar);
    if (NS_OK != rv) {
      // Error
    }
    if (nsnull != pnsMenuBar) {
      pnsMenuBar->Create(aParentWindow);
      
      // set pnsMenuBar as a nsMenuListener on aParentWindow
      nsCOMPtr<nsIMenuListener> menuListener;
      pnsMenuBar->QueryInterface(kIMenuListenerIID, getter_AddRefs(menuListener));
      mWindow->AddMenuListener(menuListener);

      nsCOMPtr<nsIDOMNode> menuNode;
      menubarNode->GetFirstChild(getter_AddRefs(menuNode));
      while (menuNode) {
        nsCOMPtr<nsIDOMElement> menuElement(menuNode);
        if (menuElement) {
          nsString menuNodeType;
          nsString menuName;
          menuElement->GetNodeName(menuNodeType);
          if (menuNodeType.Equals("menu")) {
            menuElement->GetAttribute(nsAutoString("name"), menuName);
            printf("Creating Menu [%s]\n", menuName.ToNewCString()); // this leaks
    
            // Create nsMenu
            nsIMenu * pnsMenu = nsnull;
            rv = nsRepository::CreateInstance(kMenuCID, nsnull, kIMenuIID, (void**)&pnsMenu);
            if (NS_OK == rv) {
              // Call Create
              pnsMenu->Create(pnsMenuBar, menuName);
            
              // Set nsMenu Name
              pnsMenu->SetLabel(menuName);
              // Make nsMenu a child of nsMenuBar
              //pnsMenuBar->AddMenu(pnsMenu); // XXX adds an additional menu

              // Begin menuitem inner loop
              nsCOMPtr<nsIDOMNode> menuitemNode;
              menuNode->GetFirstChild(getter_AddRefs(menuitemNode));
              while (menuitemNode) {
                nsCOMPtr<nsIDOMElement> menuitemElement(menuitemNode);
                if (menuitemElement) {
                  nsString menuitemNodeType;
                  nsString menuitemName;
                  nsString menuitemCmd;
                  menuitemElement->GetNodeName(menuitemNodeType);
                  if (menuitemNodeType.Equals("menuitem")) {
                    menuitemElement->GetAttribute(nsAutoString("name"), menuitemName);
                    menuitemElement->GetAttribute(nsAutoString("cmd"), menuitemCmd);
                    //printf("Creating MenuItem [%s]\n", menuitemName.ToNewCString()); // this leaks
                    // Create nsMenuItem
                    nsIMenuItem * pnsMenuItem = nsnull;
                    rv = nsRepository::CreateInstance(kMenuItemCID, nsnull, kIMenuItemIID, (void**)&pnsMenuItem);
                    if (NS_OK == rv) {
                      pnsMenuItem->Create(pnsMenu, menuitemName, 0);
                  
                      // Set nsMenuItem Name
                      pnsMenuItem->SetLabel(menuitemName);
                      // Make nsMenuItem a child of nsMenu
                      //pnsMenu->AddMenuItem(pnsMenuItem); // XXX adds an additional item
                      ConnectCommandToOneGUINode(menuitemNode);
    
                    }
                  }
                }
                nsCOMPtr<nsIDOMNode> oldmenuitemNode(menuitemNode);
                oldmenuitemNode->GetNextSibling(getter_AddRefs(menuitemNode));
              } // end menu item innner loop
            }
          }
        }
        nsCOMPtr<nsIDOMNode> oldmenuNode(menuNode);
        oldmenuNode->GetNextSibling(getter_AddRefs(menuNode));
      } // end while (nsnull != menuNode)
          
      // Give the aParentWindow this nsMenuBar to hold onto.
      //mWindow->SetMenuBar(pnsMenuBar);
      
      // HACK: force a paint for now
      pnsMenuBar->Paint();
    } // end if ( nsnull != pnsMenuBar )
  } // end if (nsnull != node)

} // nsWebShellWindow::LoadMenus

nsCOMPtr<nsIXULCommand>  
nsWebShellWindow::FindCommandByName(const nsString & aCmdName)
{
  nsCOMPtr<nsIXULCommand> result;
  PRInt32 i, n = mCommands.Count();
  for (i = 0; i < n; i++) {
    nsAutoString name;
    nsIXULCommand* cmd = (nsIXULCommand*)mCommands.ElementAt(i);
    cmd->GetName(name);
    if (name.Equals(aCmdName)) {
      result = nsCOMPtr<nsIXULCommand>(cmd);
      break;
    }
  }
  return result;
}

void  
nsWebShellWindow::SetCommandEnabled(const nsString & aCmdName, PRBool aState)
{
  nsCOMPtr<nsIXULCommand> cmd(FindCommandByName(aCmdName));
  if (cmd)
    cmd->SetEnabled(aState);
}

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
  NS_ADDREF(mWindow); // XXX Why?
  return NS_OK;
}

//----------------------------------------
//----------------------------------------
//----------------------------------------
//----------------------------------------
// nsIDocumentLoaderObserver implementation
//----------------------------------------
NS_IMETHODIMP 
nsWebShellWindow::OnStartURLLoad(nsIURL* aURL, const char* aContentType, nsIContentViewer* aViewer)
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
  node = nsnull;
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
    return nsCOMPtr<nsIDOMNode>(element);
  return node;
} // nsWebShellWindow::GetParentNodeFromDOMDoc

//----------------------------------------
nsCOMPtr<nsIDOMDocument> nsWebShellWindow::GetNamedDOMDoc(const nsString & aWebShellName)
{
  nsCOMPtr<nsIDOMDocument> domDoc; // result == nsnull;

  // first get the toolbar child WebShell
  nsCOMPtr<nsIWebShell> childWebShell;
  mWebShell->FindChildWithName(aWebShellName, *getter_AddRefs(childWebShell));
  if (!childWebShell)
    return domDoc;
  
  nsCOMPtr<nsIContentViewer> cv;
  childWebShell->GetContentViewer(getter_AddRefs(cv));
  if (!cv)
    return domDoc;
   
  nsCOMPtr<nsIDocumentViewer> docv(cv);
  if (!docv)
    return domDoc;

  nsCOMPtr<nsIDocument> doc;
  docv->GetDocument(*getter_AddRefs(doc));
  if (doc)
    return nsCOMPtr<nsIDOMDocument>(doc);

  return domDoc;
} // nsWebShellWindow::GetNamedDOMDoc

//----------------------------------------
PRInt32 nsWebShellWindow::GetDocHeight(nsIDocument * aDoc)
{
  nsIPresShell * presShell = aDoc->GetShellAt(0);
  if (presShell) {
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
  }
  return 0;
}

//----------------------------------------
NS_IMETHODIMP nsWebShellWindow::OnConnectionsComplete()
{
  if (DEBUGCMDS) printf("OnConnectionsComplete\n");

  LoadCommandsInWebShell(mWebShell);
  ConnectCommandsToWidgetsByType(mWebShell, nsAutoString("button"));

  nsCOMPtr<nsIWebShell> contentWebShell;
  mWebShell->FindChildWithName(nsAutoString("browser.webwindow"), *getter_AddRefs(contentWebShell));

  nsCOMPtr<nsIWebShell> toolbarWebShell;
  mWebShell->FindChildWithName(nsAutoString("browser.toolbar"), *getter_AddRefs(toolbarWebShell));
  if (contentWebShell) {
    ///////////////////////////////
    // Find the Toolbar DOM  and Load all the commands
    ///////////////////////////////
    nsCOMPtr<nsIDOMDocument> toolbarDOMDoc(GetNamedDOMDoc(nsAutoString("browser.toolbar")));
    if (!toolbarDOMDoc)
      return NS_ERROR_FAILURE;
    nsCOMPtr<nsIDOMNode> parent(GetParentNodeFromDOMDoc(toolbarDOMDoc));
    if (!parent)
      return NS_ERROR_FAILURE;

    LoadCommandsInWebShell(toolbarWebShell);
    ConnectCommandsToWidgetsByType(toolbarWebShell, nsAutoString("button"));

    PRInt32 count = 0;
    nsCOMPtr<nsIDOMNode> imgNode(FindNamedDOMNode(nsAutoString("img"), parent, count, 7));
    if (!imgNode)
      return NS_ERROR_FAILURE;
    if (NS_OK == imgNode->QueryInterface(kIDOMHTMLImageElementIID, (void**) &mThrobber)) {
      nsAutoString srcStr;
      mThrobber->GetSrc(srcStr);
      //printf("src: %s\n", nsAutoCString(srcStr));
    }
    count = 0;
    nsCOMPtr<nsIDOMNode> node(FindNamedDOMNode(nsAutoString("input"), parent, count, 1));
    if (node) {
      if (NS_OK == node->QueryInterface(kIDOMHTMLInputElementIID, (void**) &mURLBarText)) {
        const PRUnichar * urlStr = nsnull;
        contentWebShell->GetURL(0, &urlStr);
        if (nsnull != urlStr) {
          nsString url(urlStr);
          mURLBarText->SetValue(url);
        }
      }
    }

    ///////////////////////////////
    // Find the Menubar DOM  and Load the menus, hooking them up to the loaded commands
    ///////////////////////////////
    nsCOMPtr<nsIDOMDocument> menubarDOMDoc(GetNamedDOMDoc(nsAutoString("browser.toolbar")));
    if (menubarDOMDoc)
      LoadMenus(menubarDOMDoc, mWindow);
  }

  // look for a special startup command and execute it -- hackery or permanent feature?
  nsCOMPtr<nsIXULCommand> startupCmd(FindCommandByName(nsAutoString("nsCmd:StartUp")));
  if (startupCmd)
    startupCmd->DoCommand();

  ///////////////////////////////
  // Find the Status Text DOM Node.  EVIL ASSUMPTION THAT ALL SUCH WINDOWS HAVE ONE.
  ///////////////////////////////
  nsCOMPtr<nsIDOMDocument> statusDOMDoc(GetNamedDOMDoc(nsAutoString("browser.status")));
  if (!statusDOMDoc)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIDOMNode> parent(GetParentNodeFromDOMDoc(statusDOMDoc));
  if (!parent)
    return NS_ERROR_FAILURE;
  PRInt32 count = 0;
  nsCOMPtr<nsIDOMNode> statusNode(FindNamedDOMNode(nsAutoString("#text"), parent, count, 7));
  if (!statusNode)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIDOMCharacterData> charData(statusNode);
  if (!charData)
    return NS_ERROR_FAILURE;
  mStatusText = charData;
  mStatusText->SetData(nsAutoString("Ready.....")); // <<====== EVIL HARD-CODED STRING.


  // Calculate size of windows
#if 0
  nsCOMPtr<nsIDOMDocument> toolbarDOMDoc(GetNamedDOMDoc(nsAutoString("browser.toolbar")));
  nsCOMPtr<nsIDOMDocument> contentDOMDoc(GetNamedDOMDoc(nsAutoString("browser.webwindow")));
  nsCOMPtr<nsIDocument> contentDoc(contentDOMDoc);
  nsCOMPtr<nsIDocument> statusDoc(statusDOMDoc);
  nsCOMPtr<nsIDocument> toolbarDoc(toolbarDOMDoc);

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


/**
 * Get nsIDOMElement corresponding to a given webshell
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
    nsCOMPtr<nsIDocumentViewer> docv(cv);
    if (docv) {
      nsCOMPtr<nsIDocument> doc;
      docv->GetDocument(*getter_AddRefs(doc));
      if (doc) {
        nsCOMPtr<nsIDOMDocument> domdoc(doc);
        if (domdoc) {
          nsCOMPtr<nsIDOMElement> element;
          domdoc->GetDocumentElement(getter_AddRefs(element));
          if (element)
            node = element;
        }
      }
    }
  }

  return node;
}


/**
 * Wire up any commands found in the given DOM element.  This is hackery
 * to be replaced by the new command structure when available, but it's
 * a more general hackery than LoadCommands().
 * @param aNode the DOM node inside which we suspect commands
 */
void nsWebShellWindow::LoadCommandsInWebShell(nsIWebShell *aShell)
{
//  NS_PRECONDITION(aNode, "null argument to LoadCommandsInElement");

  nsCOMPtr<nsIDOMNode> webshellNode = GetDOMNodeFromWebShell(aShell);
  if (!webshellNode)
    return;
  nsCOMPtr<nsIDOMElement> webshellElement(webshellNode);
  if (!webshellElement)
    return;

  // process all child <commands...> nodes
  nsCOMPtr<nsIDOMNodeList> commandRootList;
  webshellElement->GetElementsByTagName(nsAutoString("commands"), getter_AddRefs(commandRootList));
  if (commandRootList) {
    PRUint32 rootCtr, rootCount;
    commandRootList->GetLength(&rootCount);
    for (rootCtr = 0; rootCtr < rootCount; rootCtr++) {
      nsCOMPtr<nsIDOMNode> commandRoot;
      if (NS_FAILED(commandRootList->Item(rootCtr, getter_AddRefs(commandRoot))))
        break;
      nsCOMPtr<nsIDOMElement> commandRootElement(commandRoot);
      if (!commandRootElement)
        break;

      // process all child <command...> nodes of each <commands...> node
      nsCOMPtr<nsIDOMNodeList> commandList;
      commandRootElement->GetElementsByTagName(nsAutoString("command"), getter_AddRefs(commandList));
      if (commandList) {
        PRUint32 cmdCtr, cmdCount;
        commandList->GetLength(&cmdCount);
        for (cmdCtr = 0; cmdCtr < cmdCount; cmdCtr++) {

          nsCOMPtr<nsIDOMNode> command;
          if (NS_SUCCEEDED(commandList->Item(cmdCtr, getter_AddRefs(command))))
            MakeOneCommand(aShell, command);          
        }
      }
    }
  }

  // XXX hack until Command DOM is available
  // Enable All Command
  PRInt32 i, n = mCommands.Count();
  for (i = 0; i < n; i++) {
    nsXULCommand* cmd = (nsXULCommand*) mCommands.ElementAt(i);
    cmd->SetEnabled(PR_TRUE);
  }
}


/**
 * Create an nsXULCommand, add it to our list of commands.
 * @param aShell the webshell to which the command belongs, in which it is found in the XUL
 * @param aCommand the DOM command node describing the command
 */
void
nsWebShellWindow::MakeOneCommand(nsIWebShell *aShell, nsCOMPtr<nsIDOMNode> aCommand)
{
  nsCOMPtr<nsIDOMElement> cmdElement(aCommand);
  if (!cmdElement)
    return;

  nsString  name,
            value;

  cmdElement->GetAttribute(nsAutoString("name"), name);
  cmdElement->GetAttribute(nsAutoString("onCommand"), value);

  // create command object and add it to the webshell's list

  if (DEBUGCMDS) printf("Creating cmd [%s]\n",name.ToNewCString());

  nsXULCommand * xulCmd = new nsXULCommand();
  xulCmd->SetName(name);
  xulCmd->SetCommand(value);
  xulCmd->SetWebShell(aShell);
  xulCmd->SetDOMElement(cmdElement);

  nsIXULCommand *iCmd;
  if (NS_SUCCEEDED(xulCmd->QueryInterface(nsIXULCommand::IID(), (void**) &iCmd)))
    mCommands.AppendElement(iCmd);
}


/**
 * Search for widgets with the given type. Wire them to the command list in mCommands.
 * Assumes mCommands has already been built.
 * @param aShell the webshell in which to search for widgets
 * @param aType all widgets of this type (XML element tag) will be considered
 */
void
nsWebShellWindow::ConnectCommandsToWidgetsByType(nsIWebShell *aShell, nsString &aType)
{
  // hook it up to any toolbars you may find in the webshell
  nsCOMPtr<nsIDOMNode> webshellNode = GetDOMNodeFromWebShell(aShell);
  if (!webshellNode)
    return;
  nsCOMPtr<nsIDOMElement> webshellElement(webshellNode);
  if (!webshellElement)
    return;

  nsCOMPtr<nsIDOMNodeList> widgetList;
  webshellElement->GetElementsByTagName(aType, getter_AddRefs(widgetList));
  if (widgetList)
    ConnectWidgetCommands(widgetList);
}


/**
 * Look for the "cmd" attribute in a list of widgets, and hook up the corresponding
 * commands.  Assumes the list of commands in mCommands has already been built.
 * @param aNodes a list of DOM nodes, presumably widgets, which may contain commands
 */
void
nsWebShellWindow::ConnectWidgetCommands(nsIDOMNodeList *aNodes)
{
  PRUint32 listCount, listCtr;

  if (!aNodes)
    return;

  aNodes->GetLength(&listCount);
  for (listCtr = 0; listCtr < listCount; listCtr++) {

    nsCOMPtr<nsIDOMNode> domNode;
    if (NS_SUCCEEDED(aNodes->Item(listCtr, getter_AddRefs(domNode))))
      ConnectCommandToOneGUINode(domNode);
  }
}
