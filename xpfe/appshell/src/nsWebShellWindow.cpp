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

#include "nsXULCommand.h"
#include "nsIXULCommand.h"

// XXX: Only needed for the creation of the widget controller...
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDocumentLoader.h"

/* Define Class IDs */
static NS_DEFINE_IID(kWindowCID,           NS_WINDOW_CID);
static NS_DEFINE_IID(kWebShellCID,         NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kAppShellServiceCID,  NS_APPSHELL_SERVICE_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
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



#include "nsIWebShell.h"


nsWebShellWindow::nsWebShellWindow()
{
  NS_INIT_REFCNT();

  mWebShell = nsnull;
  mWindow   = nsnull;
  mController = nsnull;
}


nsWebShellWindow::~nsWebShellWindow()
{
  if (nsnull != mWebShell) {
    mWebShell->Destroy();
    NS_RELEASE(mWebShell);
  }

  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mController);
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
                                      nsString& aControllerIID)
{
  nsresult rv;

  nsString urlString;
  const char *tmpStr = NULL;
  nsIID iid;
  char str[40];

  aUrl->GetSpec(&tmpStr);
  urlString = tmpStr;

  // XXX: need to get the default window size from prefs...
  nsRect r(0, 0, 600, 600);
  
  nsWidgetInitData initData;


  if (nsnull == aUrl) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }

  // Create top level window
  rv = nsRepository::CreateInstance(kWindowCID, nsnull, kIWidgetIID,
                                    (void**)&mWindow);
  if (NS_OK != rv) {
    goto done;
  }

  initData.mBorderStyle = eBorderStyle_dialog;

  mWindow->SetClientData(this);
  mWindow->Create((nsIWidget*)nsnull,                 // Parent nsIWidget
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
  //mWebShell->SetObserver((nsIStreamObserver*)this);

  nsIDocumentLoader * docLoader;
  mWebShell->GetDocumentLoader(docLoader);
  if (nsnull != docLoader) {
    docLoader->AddObserver((nsIDocumentLoaderObserver *)this);
  }
///  webShell->SetPrefs(aPrefs);

  mWebShell->LoadURL(urlString);
  mWebShell->Show();

  mWindow->Show(PR_TRUE);

  // Create the IWidgetController for the document...
  mController = nsnull; 
  aControllerIID.ToCString(str, sizeof(str));
  iid.Parse(str);

  rv = nsRepository::CreateInstance(iid, nsnull,
                                    kIWidgetControllerIID,
                                    (void**)&mController);
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

nsIDOMNode * nsWebShellWindow::FindNamedParentFromDoc(nsIDOMDocument * aDomDoc, const nsString &aName) 
{
  nsIDOMElement * element;
  aDomDoc->GetDocumentElement(&element);
  if (nsnull != element) {
    nsIDOMNode * parent;
    if (NS_OK == element->QueryInterface(kIDOMNodeIID, (void**) &parent)) {
      nsIDOMNode * node;
      parent->GetFirstChild(&node);
      while (nsnull != node) {
        nsString name;
        node->GetNodeName(name);
        printf("[%s]\n", name.ToNewCString());
        if (name.Equals(aName)) {
          NS_ADDREF(node);
          NS_RELEASE(parent);
          NS_RELEASE(element);
          return node;
        }
        nsIDOMNode * oldNode = node;
        oldNode->GetNextSibling(&node);
        NS_RELEASE(oldNode);
      }
      NS_IF_RELEASE(node);
      NS_RELEASE(parent);
    }
    NS_RELEASE(element);
  }
  return nsnull;
}

void nsWebShellWindow::LoadCommands(nsIWebShell * aWebShell, nsIDOMDocument * aDOMDoc) 
{
  nsIDOMNode * parentCmd = FindNamedParentFromDoc(aDOMDoc, nsAutoString("commands"));
  if (nsnull != parentCmd) {
    nsIDOMNode * node;
    parentCmd->GetFirstChild(&node);
    while (nsnull != node) {
      nsIDOMElement * element;
      nsString value;
      nsString nodeType;
      nsString name;
      if (NS_OK == node->QueryInterface(kIDOMNodeIID, (void**) &element)) {
        element->GetNodeName(nodeType);
        if (nodeType.Equals("command")) {
          element->GetAttribute(nsAutoString("name"), name);
          element->GetAttribute(nsAutoString("onCommand"), value);

          nsXULCommand * xulCmd = new nsXULCommand();
          xulCmd->SetName(name);
          xulCmd->SetCommand(value);
          xulCmd->SetWebShell(aWebShell);
          xulCmd->SetDOMElement(element);
          nsIXULCommand * icmd;
          if (NS_OK == xulCmd->QueryInterface(kIXULCommandIID, (void**) &icmd)) {
            mCommands.AppendElement(icmd);
          }
          printf("Commands[%s] value[%s]\n", name.ToNewCString(), value.ToNewCString());
        }
        NS_RELEASE(element);
      }

      //node->GetAttribute();
      nsIDOMNode * oldNode = node;
      oldNode->GetNextSibling(&node);
      NS_RELEASE(oldNode);
    }
    NS_IF_RELEASE(node);

    NS_RELEASE(parentCmd);
  }

  // Now install the commands onto the Toolbar GUI Nodes
  parentCmd = FindNamedParentFromDoc(aDOMDoc, nsAutoString("toolbar"));
  if (nsnull != parentCmd) {
    nsAutoString cmdAtom("cmd");
    nsIDOMNode * node;
    parentCmd->GetFirstChild(&node);
    while (nsnull != node) {
      nsIDOMElement * element;
      nsAutoString value;
      nsString nodeCmd;
      if (NS_OK == node->QueryInterface(kIDOMElementIID, (void**) &element)) {
        nsString name;
        element->GetNodeName(name);
        if (name.Equals(nsAutoString("BUTTON"))) {
          element->GetAttribute(cmdAtom, nodeCmd);
          PRInt32 i, n = mCommands.Count();
          for (i = 0; i < n; i++) {
            nsIXULCommand* cmd = (nsIXULCommand*) mCommands.ElementAt(i);
            nsAutoString cmdName;
            cmd->GetName(cmdName);
            printf("Cmd [%s] Node[%s]\n", cmdName.ToNewCString(), nodeCmd.ToNewCString());
            if (nodeCmd.Equals(cmdName)) {
              printf("Linking up cmd to button [%s]\n", cmdName.ToNewCString());
              cmd->AddUINode(node);
            }
          }
        } else if (name.Equals(nsAutoString("INPUT"))) {
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
          //printf("Linking up cmd to button [%s]\n", cmdName.ToNewCString());
        }
        NS_RELEASE(element);
      }

      //node->GetAttribute();
      nsIDOMNode * oldNode = node;
      oldNode->GetNextSibling(&node);
      NS_RELEASE(oldNode);
    }
    NS_IF_RELEASE(node);

    NS_RELEASE(parentCmd);
  }

  PRInt32 i, n = mCommands.Count();
  for (i = 0; i < n; i++) {
    nsIXULCommand* cmd = (nsIXULCommand*) mCommands.ElementAt(i);
    cmd->SetEnabled(PR_FALSE);
  }


}

NS_IMETHODIMP 
nsWebShellWindow::EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL,
                             PRInt32 aStatus)
{
 /* nsIWebShell* child = nsnull;
  if (aShell == mWebShell) {
    mWebShell->ChildAt(0, child);
    printf("Child 0 is 0x%x\n", child);
    NS_IF_RELEASE(child);
    mWebShell->ChildAt(1, child);
    printf("Child 1 is 0x%x\n", child);
    if (nsnull != child) {
      child->SetObserver((nsIStreamObserver*)this);
      NS_IF_RELEASE(child);
    }
  }
  // browserwindow
  mWebShell->ChildAt(0, child);
  if (aShell == child) {
    int x = 0;
    //child->SetObserver((nsIStreamObserver*)this);
  }
  NS_RELEASE(child);
*/
  // Toolbar
/*  nsIWebShell* child = nsnull;
  mWebShell->ChildAt(0, child);
  if (aShell == child) {
    nsIContentViewer* cv = nsnull;
    child->GetContentViewer(&cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIDocument * doc;
        docv->GetDocument(doc);
        if (nsnull != doc) {
          nsIDOMDocument * domDoc;
          if (NS_OK == doc->QueryInterface(kIDOMDocumentIID, (void**) &domDoc)) {
            nsIWebShell* targetWS = nsnull;
            mWebShell->ChildAt(1, targetWS);
            LoadCommands(targetWS, domDoc);
            NS_RELEASE(domDoc);
          }

          NS_RELEASE(doc);
        }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
  NS_IF_RELEASE(child);
*/
  return NS_OK;
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
//----------------------------------------
//----------------------------------------
//----------------------------------------
// nsIDocumentLoaderObserver implementation
//----------------------------------------
NS_IMETHODIMP nsWebShellWindow::OnStartURLLoad(nsIURL* aURL, const char* aContentType, nsIContentViewer* aViewer)
{
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP nsWebShellWindow::OnConnectionsComplete()
{
  printf("*********** OnConnectionsComplete\n");
  {
    nsIWebShell* child = nsnull;
    printf("Main WebShell 0 is 0x%x\n", mWebShell);
    mWebShell->ChildAt(0, child);
    printf("Child 0 is 0x%x\n", child);
    NS_IF_RELEASE(child);
    mWebShell->ChildAt(1, child);
    printf("Child 1 is 0x%x\n", child);
    NS_IF_RELEASE(child);
  }

  nsIWebShell* child = nsnull;
  mWebShell->ChildAt(0, child);
  nsIContentViewer* cv = nsnull;
  child->GetContentViewer(&cv);
  if (nsnull != cv) {
    nsIDocumentViewer* docv = nsnull;
    cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
    if (nsnull != docv) {
      nsIDocument * doc;
      docv->GetDocument(doc);
      if (nsnull != doc) {
        nsIDOMDocument * domDoc;
        if (NS_OK == doc->QueryInterface(kIDOMDocumentIID, (void**) &domDoc)) {
          nsIWebShell* targetWS = nsnull;
          mWebShell->ChildAt(1, targetWS);
          LoadCommands(targetWS, domDoc);
          NS_RELEASE(domDoc);
        }

        NS_RELEASE(doc);
      }
      NS_RELEASE(docv);
    }
    NS_RELEASE(cv);
  }
  NS_IF_RELEASE(child);

  return NS_OK;
}

//----------------------------------------

// Stream observer implementation

/*NS_IMETHODIMP
nsWebShellWindow::OnProgress(nsIURL* aURL,
                             PRUint32 aProgress,
                             PRUint32 aProgressMax)
{
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP nsWebShellWindow::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP nsWebShellWindow::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP nsWebShellWindow::OnStopBinding(nsIURL* aURL, nsresult status, const PRUnichar* aMsg)
{
  {
    PRUnichar* urlStr;
    aURL->ToString(&urlStr);
    nsString s(urlStr);
    printf("OnStopBinding URL[%s]\n", s.ToNewCString());
  }
  if (nsnull != mWebShell) {
    nsIWebShell* child = nsnull;
    PRInt32 i, cnt;
    mWebShell->GetChildCount(cnt);
    for (i=0;i<cnt;i++) {
      mWebShell->ChildAt(i, child);
      if (nsnull != child) {

        nsIContentViewer* cv = nsnull;
        child->GetContentViewer(&cv);
        if (nsnull != cv) {
          nsIDocumentViewer* docv = nsnull;
          cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
          if (nsnull != docv) {
            nsIDocument * doc;
            docv->GetDocument(doc);
            if (nsnull != doc) {
              PRUnichar* docStr;
              PRUnichar* urlStr;
              doc->GetDocumentURL()->ToString(&docStr);
              aURL->ToString(&urlStr);
              nsAutoString docAutoStr(docStr);
              nsAutoString urlAutoStr(urlStr);
              if (docAutoStr.Equals(urlAutoStr)) {
                // This child is the child that has just completed loading
                printf("OnStopBinding Child 0x%x has stopped.\n", child);
              }
              delete[] urlStr;
              delete[] docStr;
              NS_RELEASE(doc);
            }
            NS_RELEASE(docv);
          }
          NS_RELEASE(cv);
        }
      }
    }
  }

  return NS_OK;
}

*/