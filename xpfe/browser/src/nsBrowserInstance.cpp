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
 *    Pierre Phaneuf <pp@ludusdesign.com>
 *    Travis Bogard <travis@netscape.com>
 */

// Local Includes
#include "nsBrowserInstance.h"

// Helper Includes

// Interfaces Needed
#include "nsIXULWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"

// Use this trick temporarily, to minimize delta to nsBrowserAppCore.cpp.
#define nsBrowserAppCore nsBrowserInstance

#include "nsIWebShell.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIClipboardCommands.h"
#include "pratom.h"
#include "prprf.h"
#include "nsIComponentManager.h"

#include "nsIFileSpecWithUI.h"

#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"

#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIWebShellWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsIWidget.h"
#include "plevent.h"

#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

#include "nsIDocumentViewer.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsICmdLineService.h"
#include "nsIGlobalHistory.h"

#include "nsIDOMXULDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"


#include "nsIPresContext.h"
#include "nsIPresShell.h"

#include "nsIDocumentLoader.h"
#include "nsIObserverService.h"
#include "nsFileLocations.h"

#include "nsIFileLocator.h"
#include "nsIFileSpec.h"
#include "nsIWalletService.h"

#include "nsCURILoader.h"
#include "nsIContentHandler.h"
#include "nsNetUtil.h"
#include "nsICmdLineHandler.h"

static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);

// Interface for "unknown content type handler" component/service.
#include "nsIUnkContentTypeHandler.h"

// Stuff to implement file download dialog.
#include "nsIDocumentObserver.h"
#include "nsIContent.h"
#include "nsIContentViewerFile.h"
#include "nsINameSpaceManager.h"
#include "nsFileStream.h"
 

#ifdef DEBUG
#ifndef DEBUG_pavlov
#define FORCE_CHECKIN_GUIDELINES
#endif /* DEBUG_pavlov */
#endif /* DEBUG */


// Stuff to implement find/findnext
#include "nsIFindComponent.h"
#ifdef DEBUG_warren
#include "prlog.h"
#if defined(DEBUG) || defined(FORCE_PR_LOG)
static PRLogModuleInfo* gTimerLog = nsnull;
#endif /* DEBUG || FORCE_PR_LOG */
#endif

// if DEBUG or MOZ_PERF_METRICS are defined, enable the PageCycler
#ifdef DEBUG
#define ENABLE_PAGE_CYCLER
#endif
#ifdef MOZ_PERF_METRICS
#define ENABLE_PAGE_CYCLER
#endif

#include "nsTimeBomb.h"
static NS_DEFINE_CID(kTimeBombCID,     NS_TIMEBOMB_CID);

static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);


/* Define Class IDs */
static NS_DEFINE_IID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kCmdLineServiceCID,        NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_IID(kCGlobalHistoryCID,        NS_GLOBALHISTORY_CID);
static NS_DEFINE_IID(kCSessionHistoryCID,       NS_SESSIONHISTORY_CID);
static NS_DEFINE_CID(kCPrefServiceCID,          NS_PREF_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIStreamObserverIID,       NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kIGlobalHistoryIID,        NS_IGLOBALHISTORY_IID);
static NS_DEFINE_IID(kIWebShellIID,             NS_IWEB_SHELL_IID);

#ifdef DEBUG                                                           
static int APP_DEBUG = 0; // Set to 1 in debugger to turn on debugging.
#else                                                                  
#define APP_DEBUG 0                                                    
#endif                                                                 

#define FILE_PROTOCOL "file://"

#define PREF_HOMEPAGE_OVERRIDE_URL "startup.homepage_override_url"
#define PREF_HOMEPAGE_OVERRIDE "browser.startup.homepage_override.1"
#define PREF_BROWSER_STARTUP_PAGE "browser.startup.page"
#define PREF_BROWSER_STARTUP_HOMEPAGE "browser.startup.homepage"

static nsresult
FindNamedXULElement(nsIDocShell * aShell, const char *aId, nsCOMPtr<nsIDOMElement> * aResult );


static nsresult setAttribute( nsIDocShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value );
/////////////////////////////////////////////////////////////////////////
// nsBrowserAppCore
/////////////////////////////////////////////////////////////////////////

nsBrowserAppCore::nsBrowserAppCore() : mIsClosed(PR_FALSE)
{
  mContentWindow        = nsnull;
  mContentScriptContext = nsnull;
  mWebShellWin          = nsnull;
  mDocShell             = nsnull;
  mContentAreaWebShell  = nsnull;
  mContentAreaDocLoader = nsnull;
  mSHistory             = nsnull;
  mIsLoadingHistory     = PR_FALSE;
  NS_INIT_REFCNT();
}

nsBrowserAppCore::~nsBrowserAppCore()
{
  Close();
  NS_IF_RELEASE(mSHistory);
}

NS_IMPL_ADDREF(nsBrowserInstance)
NS_IMPL_RELEASE(nsBrowserInstance)

NS_INTERFACE_MAP_BEGIN(nsBrowserInstance)
   NS_INTERFACE_MAP_ENTRY(nsIBrowserInstance)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentLoaderObserver)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
NS_INTERFACE_MAP_END

NS_IMETHODIMP    
nsBrowserAppCore::Init()
{
  nsresult rv = NS_OK;

  // Create session history.
  
  rv = nsComponentManager::CreateInstance(kCSessionHistoryCID,
                                          nsnull,
                                          NS_GET_IID(nsISessionHistory),
                                          (void **)&mSHistory );

  if ( !NS_SUCCEEDED( rv ) ) {
    printf("Error initialising session history\n");

  }

  // register ourselves as a content listener with the uri dispatcher service
  rv = NS_OK;
  NS_WITH_SERVICE(nsIURILoader, dispatcher, NS_URI_LOADER_PROGID, &rv);
  if (NS_SUCCEEDED(rv)) 
    rv = dispatcher->RegisterContentListener(this);


  return rv;
}

NS_IMETHODIMP    
nsBrowserAppCore::SetDocumentCharset(const PRUnichar *aCharset)
{
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(mContentWindow) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (docShell) 
  {
    nsCOMPtr<nsIContentViewer> childCV;
    NS_ENSURE_SUCCESS(docShell->GetContentViewer(getter_AddRefs(childCV)), NS_ERROR_FAILURE);
    if (childCV) 
    {
      nsCOMPtr<nsIMarkupDocumentViewer> markupCV = do_QueryInterface(childCV);
      if (markupCV) {
        NS_ENSURE_SUCCESS(markupCV->SetDefaultCharacterSet(aCharset), NS_ERROR_FAILURE);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Back()
{
  GoBack(mContentAreaWebShell);
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserAppCore::GetSessionHistory(nsISessionHistory ** aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  if (mSHistory) {
     NS_ADDREF(mSHistory);
     *aResult = mSHistory;
  }
  else
    return NS_ERROR_NO_INTERFACE;
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::Reload(nsLoadFlags flags)
{
  if (mContentAreaWebShell)
     Reload(mContentAreaWebShell, flags);
  return NS_OK;
}   

NS_IMETHODIMP
nsBrowserAppCore::Forward()
{
  GoForward(mContentAreaWebShell);
  return NS_OK;
}



NS_IMETHODIMP    
nsBrowserAppCore::Stop()
{
   nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mContentAreaWebShell));
   if(webNav)
      webNav->Stop();

  if (mIsLoadingHistory) {
    SetLoadingFlag(PR_FALSE);
  }
  nsAutoString v( "false" );
  // XXX: The throbber should be turned off when the OnStopDocumentLoad 
  //      notification is received 
  setAttribute( mDocShell, "Browser:Throbber", "busy", v );
  return NS_OK;
}



nsresult ProfileDirectory(nsFileSpec& dirSpec) {
  nsIFileSpec* spec = NS_LocateFileOrDirectory(
              nsSpecialFileSpec::App_UserProfileDirectory50);
  if (!spec)
    return NS_ERROR_FAILURE;
  return spec->GetFileSpec(&dirSpec);
}



NS_IMETHODIMP
nsBrowserAppCore::GotoHistoryIndex(PRInt32 aIndex)
{
    Goto(aIndex, mContentAreaWebShell, PR_FALSE);
  return NS_OK;

}

NS_IMETHODIMP
nsBrowserAppCore::BackButtonPopup()
{
  if (!mSHistory)  {
    printf("nsBrowserAppCore::BackButtonPopup Couldn't get a handle to SessionHistory\n");
    return NS_ERROR_FAILURE;
  }

 // Get handle to the "backbuttonpopup" element
  nsCOMPtr<nsIDOMElement>   backPopupElement;
  nsresult rv = FindNamedXULElement(mDocShell, "backbuttonpopup", &backPopupElement);

  if (!NS_SUCCEEDED(rv) ||  !backPopupElement)
  {
     return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNode> backPopupNode(do_QueryInterface(backPopupElement)); 
  if (!backPopupNode) {
    return NS_ERROR_FAILURE;
  }

  //get handle to the Menu item under popup
  nsCOMPtr<nsIDOMNode>   menu;
  backPopupNode->GetFirstChild(getter_AddRefs(menu));
  if (!menu) {
   printf("nsBrowserAppCore::BackButtonPopup Call to GetFirstChild failed\n");
     return NS_ERROR_FAILURE;
  }
  
  PRBool hasChildren=PR_FALSE;

  // Check if menu has children. If so, remove them.
  rv = menu->HasChildNodes(&hasChildren);
  if (NS_SUCCEEDED(rv) && hasChildren) {
    rv = ClearHistoryPopup(menu);
    if (!NS_SUCCEEDED(rv))
      printf("nsBrowserAppCore::BackButtonPopup ERROR While removing old history menu items\n");
  }    // hasChildren
  else {
   if (APP_DEBUG) printf("nsBrowserAppCore::BackButtonPopup Menu has no children\n");
  }             

  PRInt32 indix=0, i=0;
  //Get current index in Session History. We have already verified 
  // if mSHistory is null
  mSHistory->GetCurrentIndex(&indix);

  //Decide on the # of items in the popup list 
  if (indix > SHISTORY_POPUP_LIST)
     i  = indix-SHISTORY_POPUP_LIST;

  for (PRInt32 j=indix-1;j>=i;j--) {
      PRUnichar *title=nsnull;
    char * url=nsnull;
    
        mSHistory->GetURLForIndex(j, &url);
        nsAutoString  histURL(url);
        mSHistory->GetTitleForIndex(j, &title);
        nsAutoString  histTitle(title);
        rv = CreateMenuItem(menu, j, title);
      if (!NS_SUCCEEDED(rv)) 
      printf("nsBrowserAppCore:;BackButtonpopup ERROR while creating menu item\n");
    Recycle(title);
    Recycle(url);
     } 

  return NS_OK;

}



NS_IMETHODIMP nsBrowserAppCore::CreateMenuItem(
  nsIDOMNode *    aParentMenu,
  PRInt32      aIndex,
  const PRUnichar *  aName)
{
  if (APP_DEBUG) printf("In CreateMenuItem\n");
  nsresult rv=NS_OK;  
  nsCOMPtr<nsIDOMDocument>  doc;

  rv = aParentMenu->GetOwnerDocument(getter_AddRefs(doc));
  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserAppCore::CreateMenuItem ERROR Getting handle to the document\n");
    return NS_ERROR_FAILURE;
  }
  nsString menuitemName(aName);
  
  // Create nsMenuItem
  nsCOMPtr<nsIDOMElement>  menuItemElement;
  nsString  tagName("menuitem");
  rv = doc->CreateElement(tagName, getter_AddRefs(menuItemElement));
  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserAppCore::CreateMenuItem ERROR creating the menu item element\n");
    return NS_ERROR_FAILURE;
  }

  //Set the label for the menu item
  nsString menuitemlabel(aName);
  if (APP_DEBUG) printf("nsBrowserAppCore::CreateMenuItem Setting menu name to %s\n", menuitemlabel.ToNewCString());
  rv = menuItemElement->SetAttribute(nsString("value"), menuitemlabel);
  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserAppCore::CreateMenuItem ERROR Setting node value for menu item ****\n");
    return NS_ERROR_FAILURE;
  }

  // Set the hist attribute to true
  rv = menuItemElement->SetAttribute(nsString("ishist"), nsString("true"));
  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserAppCore::CreateMenuItem ERROR setting ishist handler\n");
    return NS_ERROR_FAILURE;
  }

    // Make a DOMNode out of it
  nsCOMPtr<nsIDOMNode>  menuItemNode = do_QueryInterface(menuItemElement);
  if (!menuItemNode) {
    printf("nsBrowserAppCore::CreateMenuItem ERROR converting DOMElement to DOMNode *****\n");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNode> resultNode;
  // Make nsMenuItem a child of nsMenu
  rv = aParentMenu->AppendChild(menuItemNode, getter_AddRefs(resultNode));
  
  if (!NS_SUCCEEDED(rv)) 
  {
       printf("nsBrowserAppCore::CreateMenuItem ERROR appending menuitem to menu *****\n");
     return NS_ERROR_FAILURE;
  }

  //Set the onaction attribute
  nsString menuitemCmd("gotoHistoryIndex(");
  menuitemCmd.Append(aIndex);
  menuitemCmd += ")";  
  if (APP_DEBUG) printf("nsBrowserAppCore::CreateMenuItem Setting action handler to %s\n", menuitemCmd.ToNewCString());
  nsString attrName("oncommand");
  rv = menuItemElement->SetAttribute(attrName, menuitemCmd);
  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserAppCore::CreateMenuItem ERROR setting onaction handler\n");
    return NS_ERROR_FAILURE;
  }
  else
       if (APP_DEBUG) printf("nsBrowserAppCore::CreateMenuItem Successfully appended menu item to parent\n");



  return NS_OK;
}



NS_IMETHODIMP
nsBrowserAppCore::ForwardButtonPopup()
{

  if (!mSHistory)  {
    printf("nsBrowserAppCore::ForwardButtonPopup Couldn't get a handle to SessionHistory\n");
    return NS_ERROR_FAILURE;
  }

  if (APP_DEBUG) printf("In BrowserAppCore::Forwardbuttonpopup\n");

 // Get handle to the "forwardbuttonpopup" element
  nsCOMPtr<nsIDOMElement>   forwardPopupElement;
  nsresult rv = FindNamedXULElement(mDocShell, "forwardbuttonpopup", &forwardPopupElement);

  if (!NS_SUCCEEDED(rv) ||  !forwardPopupElement)
  {
   printf("nsBrowserAppCore::ForwardButtonPopup Couldn't get handle to forwardPopupElement\n");
     return NS_ERROR_FAILURE;
  }

  // Make a nsIDOMNode out of it
  nsCOMPtr<nsIDOMNode> forwardPopupNode(do_QueryInterface(forwardPopupElement)); 
  if (!forwardPopupNode) {
    printf("nsBrowserAppCore::ForwardButtonPopup Couldn't make a node out of forwardpopupelement\n");
    return NS_ERROR_FAILURE;
  }
  
  //get handle to the Menu item under popup
  nsCOMPtr<nsIDOMNode>   menu;
  rv = forwardPopupNode->GetFirstChild(getter_AddRefs(menu));
  if (!NS_SUCCEEDED(rv) || !menu) {
    printf("nsBrowserAppCore::ForwardButtonPopup Call to GetFirstChild failed\n");
     return NS_ERROR_FAILURE;
  }
  
  PRBool hasChildren=PR_FALSE;

  // Check if menu has children. If so, remove them.
  menu->HasChildNodes(&hasChildren);
  if (hasChildren) {
    // Remove all old entries 
    rv = ClearHistoryPopup(menu);
    if (!NS_SUCCEEDED(rv)) {
      printf("nsBrowserAppCore::ForwardMenuPopup Error while clearing old history entries\n");
    }
  }    // hasChildren
  else {
    if (APP_DEBUG) printf("nsBrowserAppCore::ForwardButtonPopup Menu has no children\n");
  }  

  PRInt32 indix=0, i=0, length=0;
  //Get current index in Session History
  mSHistory->GetCurrentIndex(&indix);
 //Get total length of Session History
  mSHistory->GetHistoryLength(&length);

  //Decide on the # of items in the popup list 
  if ((length-indix) > SHISTORY_POPUP_LIST)
     i  = indix+SHISTORY_POPUP_LIST;
  else
   i = length;

  for (PRInt32 j=indix+1;j<i;j++) {
      PRUnichar *title=nsnull;
    char * url=nsnull;

      mSHistory->GetURLForIndex(j, &url);
      mSHistory->GetTitleForIndex(j, &title);
      nsAutoString  histTitle(title);      
      rv = CreateMenuItem(menu, j, title);
    if (!NS_SUCCEEDED(rv)) 
      printf("nsBrowserAppCore::ForwardbuttonPopup, Error while creating history menu items\n");
    Recycle(title);
    Recycle(url);
  } 
   return NS_OK;

}


NS_IMETHODIMP
nsBrowserAppCore::UpdateGoMenu()
{

    if (!mSHistory)  {
    printf("nsBrowserAppCore::UpdateGoMenu Couldn't get a handle to SessionHistory\n");
    return NS_ERROR_FAILURE;
  }

  // Get handle to the "main-menubar" element
  nsCOMPtr<nsIDOMElement>   mainMenubarElement;
  nsresult rv = FindNamedXULElement(mDocShell, "main-menubar", &mainMenubarElement);

  if (!NS_SUCCEEDED(rv) ||  !mainMenubarElement)
  {
   printf("Couldn't get handle to the Go menu\n");
     return NS_ERROR_FAILURE;
  }
  else {
    if (APP_DEBUG) printf("nsBrowserAppCore::UpdateGoMenu Got handle to the main-toolbox element\n"); 
  }

  nsCOMPtr<nsIDOMNode> mainMenubarNode(do_QueryInterface(mainMenubarElement)); 
  if (!mainMenubarNode) {
    if (APP_DEBUG) printf("nsBrowserAppCore::UpdateGoMenu Couldn't get Node out of Element\n");
    return NS_ERROR_FAILURE;
  }

   nsCOMPtr<nsIDOMNode> goMenuNode;
   PRBool hasChildren=PR_FALSE;
  // Check if toolbar has children.
  rv = mainMenubarNode->HasChildNodes(&hasChildren);
  if (NS_SUCCEEDED(rv) && hasChildren) {  
     nsCOMPtr<nsIDOMNodeList>   childList;

     //Get handle to the children list
     rv = mainMenubarNode->GetChildNodes(getter_AddRefs(childList));
     if (NS_SUCCEEDED(rv) && childList) {
        PRInt32 ccount=0;
        childList->GetLength((unsigned int *)&ccount);
        
    // Get the 'Go' menu
        for (PRInt32 i=0; i<ccount; i++) {
            nsCOMPtr<nsIDOMNode> child;
            rv = childList->Item(i, getter_AddRefs(child));
      if (!NS_SUCCEEDED(rv) || !child) {
               if (APP_DEBUG) printf("nsBrowserAppCore::UpdateGoMenu Couldn't get child %d from menu bar\n", i);
         return NS_ERROR_FAILURE;
      }
      // Get element out of the node
      nsCOMPtr<nsIDOMElement> childElement(do_QueryInterface(child));
      if (!childElement) {
        printf("nsBrowserAppCore::UpdateGoMenu Could n't get DOMElement out of DOMNode for child\n");
        return NS_ERROR_FAILURE;
      }
      nsString nodelabel;
            rv = childElement->GetAttribute(nsAutoString("value"), nodelabel);
      if (APP_DEBUG) printf("nsBrowserAppCore::UpdateGoMenu Node Name for menu = %s\n", nodelabel.ToNewCString());
      if (!NS_SUCCEEDED(rv)) {
        printf("nsBrowserAppCore::UpdateGoMenu Couldn't get node name\n");
        return NS_ERROR_FAILURE;
      }
      nsString nodeid;
            rv = childElement->GetAttribute(nsAutoString("id"), nodeid);
      if (nodeid == "gomenu") {
        goMenuNode = child;
        break;
      }
        } //(for) 
     }   // if (childList)
  }    // hasChildren
  else {
    if (APP_DEBUG) printf("nsBrowserAppCore::UpdateGoMenu Menubar has no children\n");
    return NS_ERROR_FAILURE;
  }

  if (!goMenuNode) {
     printf("nsBrowserAppCore::UpdateGoMenu Couldn't find Go Menu. returning\n");
   return NS_ERROR_FAILURE;
 
  }

    //get handle to the menupopup under gomenu
  nsCOMPtr<nsIDOMNode>   menuPopup;
  rv = goMenuNode->GetFirstChild(getter_AddRefs(menuPopup));
  if (!NS_SUCCEEDED(rv) || !menuPopup) {
    printf("nsBrowserAppCore::UpdateGoMenu Call to get menupopup under go menu failed\n");
     return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDOMElement> menuPopupElement(do_QueryInterface(menuPopup));
  if (!menuPopupElement) {
  printf("nsBrowserAppCore::UpdateGoMenu Could n't get DOMElement out of DOMNode for menuPopup\n");
  return NS_ERROR_FAILURE;
  }

  // Clear all history children under Go menu
  rv = ClearHistoryPopup(menuPopup);
  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserAppCore::UpdateGoMenu Error while clearing old history list\n");
  }
  
  PRInt32 length=0,i=0;
  //Get total length of the  Session History
  mSHistory->GetHistoryLength(&length);

  //Decide on the # of items in the popup list 
  if (length > SHISTORY_POPUP_LIST)
     i  = length-SHISTORY_POPUP_LIST;

  for (PRInt32 j=length-1;j>=i;j--) {
      PRUnichar  *title=nsnull;
    char * url=nsnull;

      mSHistory->GetURLForIndex(j, &url);
      nsAutoString  histURL(url);
      mSHistory->GetTitleForIndex(j, &title);
      nsAutoString  histTitle(title);
      if (APP_DEBUG) printf("nsBrowserAppCore::UpdateGoMenu URL = %s, TITLE = %s\n", histURL.ToNewCString(), histTitle.ToNewCString());
      rv = CreateMenuItem(menuPopup, j, title);
    if (!NS_SUCCEEDED(rv)) {
        printf("nsBrowserAppCore::UpdateGoMenu Error while creating history mene item\n");
    }
    Recycle(title);
    Recycle(url);
     }
  return NS_OK;

}



NS_IMETHODIMP
nsBrowserAppCore::ClearHistoryPopup(nsIDOMNode * aParent)
{

   nsresult rv;
   nsCOMPtr<nsIDOMNode> menu = dont_QueryInterface(aParent);

     nsCOMPtr<nsIDOMNodeList>   childList;

     //Get handle to the children list
     rv = menu->GetChildNodes(getter_AddRefs(childList));
     if (NS_SUCCEEDED(rv) && childList) {
        PRInt32 ccount=0;
        childList->GetLength((unsigned int *)&ccount);

        // Remove the children that has the 'hist' attribute set to true.
        for (PRInt32 i=0; i<ccount; i++) {
            nsCOMPtr<nsIDOMNode> child;
            rv = childList->Item(i, getter_AddRefs(child));
      if (!NS_SUCCEEDED(rv) ||  !child) {
        printf("nsBrowserAppCore::ClearHistoryPopup, Could not get child\n");
        return NS_ERROR_FAILURE;
      }
      // Get element out of the node
      nsCOMPtr<nsIDOMElement> childElement(do_QueryInterface(child));
      if (!childElement) {
        printf("nsBrowserAppCore::ClearHistorypopup Could n't get DOMElement out of DOMNode for child\n");
        return NS_ERROR_FAILURE;
      }
      nsString  attrname("ishist");
      nsString  attrvalue;
      rv = childElement->GetAttribute(attrname, attrvalue);
      if (NS_SUCCEEDED(rv) && attrvalue == "true") {
        // It is a history menu item. Remove it
                nsCOMPtr<nsIDOMNode> ret;         
                rv = menu->RemoveChild(child, getter_AddRefs(ret));
          if (NS_SUCCEEDED(rv)) {
           if (ret) {
              if (APP_DEBUG) printf("nsBrowserAppCore::ClearHistoryPopup Child %x removed from the popuplist \n", (unsigned int) child.get());                
           }
           else {
              printf("nsBrowserAppCore::ClearHistoryPopup Child %x was not removed from popuplist\n", (unsigned int) child.get());
           }
        }  // NS_SUCCEEDED(rv)
          else
        {
           printf("nsBrowserAppCore::ClearHistoryPopup Child %x was not removed from popuplist\n", (unsigned int) child.get());
                   return NS_ERROR_FAILURE;
        }         
      }  // atrrvalue == true      
    } //(for) 
   }   // if (childList)
   return NS_OK;
}


NS_IMETHODIMP    
nsBrowserAppCore::WalletPreview(nsIDOMWindow* aWin, nsIDOMWindow* aForm)
{
  NS_PRECONDITION(aForm != nsnull, "null ptr");
  if (! aForm)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
  scriptGlobalObject = do_QueryInterface(aForm); 
  nsCOMPtr<nsIDocShell> docShell; 
  scriptGlobalObject->GetDocShell(getter_AddRefs(docShell)); 

  nsCOMPtr<nsIPresShell> presShell;
  if(docShell)
   docShell->GetPresShell(getter_AddRefs(presShell));
  nsIWalletService *walletservice;
  nsresult res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if (NS_SUCCEEDED(res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_Prefill(presShell, PR_FALSE);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
    if (NS_FAILED(res)) { /* this just means that there was nothing to prefill */
      return NS_OK;
    }
  } else {
    return res;
  }

   nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aWin));
   NS_ENSURE_TRUE(sgo, NS_ERROR_FAILURE);

   nsCOMPtr<nsIScriptContext> scriptContext;
   sgo->GetContext(getter_AddRefs(scriptContext));
   NS_ENSURE_TRUE(scriptContext, NS_ERROR_FAILURE);

   JSContext* jsContext = (JSContext*)scriptContext->GetNativeContext();
   NS_ENSURE_TRUE(jsContext, NS_ERROR_FAILURE);

   void* mark;
   jsval* argv;

   argv = JS_PushArguments(jsContext, &mark, "sss", "chrome://wallet/content/WalletPreview.xul", "_blank", 
                  //"chrome,dialog=yes,modal=yes,all");
                  "chrome,modal=yes,dialog=yes,all,width=504,height=436");
   NS_ENSURE_TRUE(argv, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDOMWindow> newWindow;
   aWin->OpenDialog(jsContext, argv, 3, getter_AddRefs(newWindow));
   JS_PopArguments(jsContext, mark);

   return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::WalletChangePassword()
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_ChangePassword();
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }
  return NS_OK;
}

#include "nsIDOMHTMLDocument.h"
static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
NS_IMETHODIMP    
nsBrowserAppCore::WalletQuickFillin(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject; 
  scriptGlobalObject = do_QueryInterface(aWin);
  nsCOMPtr<nsIDocShell> docShell; 
  scriptGlobalObject->GetDocShell(getter_AddRefs(docShell)); 

  nsCOMPtr<nsIPresShell> presShell;
  if(docShell)
   docShell->GetPresShell(getter_AddRefs(presShell));
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_Prefill(presShell, PR_TRUE);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
    return NS_OK;
  } else {
    return res;
  }
}

NS_IMETHODIMP    
nsBrowserAppCore::WalletRequestToCapture(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject; 
  scriptGlobalObject = do_QueryInterface(aWin);
  nsCOMPtr<nsIDocShell> docShell; 
  scriptGlobalObject->GetDocShell(getter_AddRefs(docShell)); 

  nsCOMPtr<nsIPresShell> presShell;
  if(docShell) 
   docShell->GetPresShell(getter_AddRefs(presShell));
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_RequestToCapture(presShell);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
    return NS_OK;
  } else {
    return res;
  }
}

NS_IMETHODIMP    
nsBrowserAppCore::LoadUrl(const PRUnichar * urlToLoad)
{
  nsresult rv = NS_OK;

  if (mIsLoadingHistory) {
     SetLoadingFlag(PR_FALSE);
  }
  /* Ask nsWebShell to load the URl */
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mContentAreaWebShell));
    
  // Normal browser.
  rv = webNav->LoadURI( urlToLoad );

  return rv;
}

#ifdef ENABLE_PAGE_CYCLER
#include "nsProxyObjectManager.h"
#include "nsITimer.h"

static void TimesUp(nsITimer *aTimer, void *aClosure);
  // Timer callback: called when the timer fires

class PageCycler : public nsIObserver {
public:
  NS_DECL_ISUPPORTS

  PageCycler(nsBrowserAppCore* appCore, char *aTimeoutValue = nsnull)
    : mAppCore(appCore), mBuffer(nsnull), mCursor(nsnull), mTimeoutValue(0) { 
    NS_INIT_REFCNT();
    NS_ADDREF(mAppCore);
    if (aTimeoutValue){
      mTimeoutValue = atol(aTimeoutValue);
    }
  }

  virtual ~PageCycler() { 
    if (mBuffer) delete[] mBuffer;
    NS_RELEASE(mAppCore);
  }

  nsresult Init(const char* nativePath) {
    nsresult rv;
    mFile = nativePath;
    if (!mFile.IsFile())
      return mFile.Error();

    nsCOMPtr<nsISupports> in;
    rv = NS_NewTypicalInputFileStream(getter_AddRefs(in), mFile);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIInputStream> inStr = do_QueryInterface(in, &rv);
    if (NS_FAILED(rv)) return rv;
    PRUint32 avail;
    rv = inStr->Available(&avail);
    if (NS_FAILED(rv)) return rv;

    mBuffer = new char[avail + 1];
    if (mBuffer == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
    PRUint32 amt;
    rv = inStr->Read(mBuffer, avail, &amt);
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(amt == avail, "Didn't get the whole file.");
    mBuffer[avail] = '\0';
    mCursor = mBuffer;

    NS_WITH_SERVICE(nsIObserverService, obsServ, NS_OBSERVERSERVICE_PROGID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsString topic("EndDocumentLoad");
    rv = obsServ->AddObserver(this, topic.GetUnicode());
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add self to observer service");
    return rv; 
  }

  nsresult GetNextURL(nsString& result) {
    result = mCursor;
    PRInt32 pos = result.Find(NS_LINEBREAK);
    if (pos > 0) {
      result.Truncate(pos);
      mLastRequest = result;
      mCursor += pos + NS_LINEBREAK_LEN;
      return NS_OK;
    }
    else if ( !result.IsEmpty() ) {
      // no more URLs after this one
      mCursor += result.Length(); // Advance cursor to terminating '\0'
      mLastRequest = result;
      return NS_OK;
    }
    else {
      // no more URLs, so quit the browser
      nsresult rv;
      // make sure our timer is stopped first
      StopTimer();
      NS_WITH_SERVICE(nsIAppShellService, appShellServ, kAppShellServiceCID, &rv);
      if(NS_FAILED(rv)) return rv;
      NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, nsIProxyObjectManager::GetCID(), &rv);
      if(NS_FAILED(rv)) return rv;
      nsCOMPtr<nsIAppShellService> appShellProxy;
      rv = pIProxyObjectManager->GetProxyObject(NS_UI_THREAD_EVENTQ, NS_GET_IID(nsIAppShellService), 
                                                appShellServ, PROXY_ASYNC | PROXY_ALWAYS,
                                                getter_AddRefs(appShellProxy));

      (void)appShellProxy->Quit();
      return NS_ERROR_FAILURE;
    }
  }

  NS_IMETHOD Observe(nsISupports* aSubject, 
                     const PRUnichar* aTopic,
                     const PRUnichar* someData) {
    nsresult rv = NS_OK;
    nsString data(someData);
    if (data.Find(mLastRequest) == 0) {
      char* dataStr = data.ToNewCString();
      printf("########## PageCycler loaded: %s\n", dataStr);
      nsCRT::free(dataStr);

      nsAutoString url;
      rv = GetNextURL(url);
      if (NS_SUCCEEDED(rv)) {
        // stop previous timer
        StopTimer();
        if (aSubject == this){
          // if the aSubject arg is 'this' then we were called by the Timer
          // Stop the current load and move on to the next
          mAppCore->Stop();
        }
        // load the URL
        rv = mAppCore->LoadUrl(url.GetUnicode());
        // start new timer
        StartTimer();
      }
    }
    else {
      char* dataStr = data.ToNewCString();
      printf("########## PageCycler possible failure for: %s\n", dataStr);
      nsCRT::free(dataStr);
    }
    return rv;
  }

  const nsAutoString &GetLastRequest( void )
  { 
    return mLastRequest; 
  }

  // StartTimer: if mTimeoutValue is positive, then create a new timer
  //             that will call the callback fcn 'TimesUp' to keep us cycling
  //             through the URLs
  nsresult StartTimer(void)
  {
    nsresult rv=NS_OK;
    if (mTimeoutValue > 0){
      rv=NS_NewTimer(getter_AddRefs(mShutdownTimer));
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create timer for PageCycler...");
      if (NS_SUCCEEDED(rv) && mShutdownTimer){
        mShutdownTimer->Init(TimesUp, (void *)this, mTimeoutValue*1000);
      }
    }
    return rv;
  }

  // StopTimer: if there is a timer, cancel it
  nsresult StopTimer(void)
  {
    if (mShutdownTimer){
      mShutdownTimer->Cancel();
    }
    return NS_OK;
  }

protected:
  nsBrowserAppCore*     mAppCore;
  nsFileSpec            mFile;
  char*                 mBuffer;
  char*                 mCursor;
  nsAutoString          mLastRequest;
  nsCOMPtr<nsITimer>    mShutdownTimer;
  PRUint32              mTimeoutValue;
};

NS_IMPL_ADDREF(PageCycler)
NS_IMPL_RELEASE(PageCycler)

NS_INTERFACE_MAP_BEGIN(PageCycler)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END

// TimesUp: callback for the PageCycler timer: called when we have waited too long
// for the page to finish loading.
// 
// The aClosure argument is actually the PageCycler, 
// so use it to stop the timer and call the Observe fcn to move on to the next URL
// Note that we pass the PageCycler instance as the aSubject argument to the Observe
// function. This is our indication that the Observe method is being called after a
// timeout, allowing the PageCycler to do any necessary processing before moving on.
void TimesUp(nsITimer *aTimer, void *aClosure)
{
  if(aClosure){
    char urlBuf[64];
    PageCycler *pCycler = (PageCycler *)aClosure;
    pCycler->StopTimer();
    pCycler->GetLastRequest().ToCString( urlBuf, sizeof(urlBuf), 0 );
    fprintf(stderr,"########## PageCycler Timeout on URL: %s\n", urlBuf);
    pCycler->Observe( pCycler, nsnull, (pCycler->GetLastRequest()).GetUnicode() );
  }
}

#endif //ENABLE_PAGE_CYCLER

NS_IMETHODIMP    
nsBrowserAppCore::LoadInitialPage(void)
{
  static PRBool cmdLineURLUsed = PR_FALSE;
  char * urlstr = nsnull;
  nsresult rv;

  if (!cmdLineURLUsed) {
    NS_WITH_SERVICE(nsICmdLineService, cmdLineArgs, kCmdLineServiceCID, &rv);
    if (NS_FAILED(rv)) {
      if (APP_DEBUG) fprintf(stderr, "Could not obtain CmdLine processing service\n");
      return NS_ERROR_FAILURE;
    }

#ifdef ENABLE_PAGE_CYCLER
    // First, check if there's a URL file to load (for testing), and if there 
    // is, process it instead of anything else.
    char* file = 0;
    rv = cmdLineArgs->GetCmdLineValue("-f", &file);
    if (NS_SUCCEEDED(rv) && file) {
      // see if we have a timeout value corresponding to the url-file
      char *timeoutVal=nsnull;
      rv = cmdLineArgs->GetCmdLineValue("-ftimeout", &timeoutVal);
      // cereate the cool PageCycler instance
      PageCycler* bb = new PageCycler(this, timeoutVal);
      if (bb == nsnull) {
        nsCRT::free(file);
        nsCRT::free(timeoutVal);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      NS_ADDREF(bb);
      rv = bb->Init(file);
      nsCRT::free(file);
      nsCRT::free(timeoutVal);
      if (NS_FAILED(rv)) return rv;

      nsAutoString str;
      rv = bb->GetNextURL(str);
      if (NS_SUCCEEDED(rv)) {
        urlstr = str.ToNewCString();
      }
      NS_RELEASE(bb);
    }
    else 
#endif //ENABLE_PAGE_CYCLER
    {
      // Examine content URL.
      if ( mContentAreaWebShell ) {
        nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(mContentAreaWebShell);
        nsCOMPtr<nsIURI> uri;
        rv = docShell->GetCurrentURI(getter_AddRefs(uri));
        nsXPIDLCString spec;
        if (NS_SUCCEEDED(rv))
          rv = uri->GetSpec(getter_Copies(spec));
        /* Check whether url is valid. Otherwise we compare with 
           * "about:blank" and there by return from here with out 
           * loading the command line url or default home page.
           */
        if (NS_SUCCEEDED(rv) && nsCRT::strcasecmp(spec, "about:blank") != 0) {
            // Something has already been loaded (probably via window.open),
            // leave it be.
            return NS_OK;
        }
      }

      // Get the URL to load
      rv = cmdLineArgs->GetURLToLoad(&urlstr);
    }

    if (urlstr != nsnull) {
      // A url was provided. Load it
      if (APP_DEBUG) printf("Got Command line URL to load %s\n", urlstr);
      nsString url ( urlstr );
      rv = LoadUrl( url.GetUnicode() );
      cmdLineURLUsed = PR_TRUE;
      return rv;
    }
  }

  // No URL was provided in the command line. Load the default provided
  // in the navigator.xul;

  // but first, abort if the window doesn't want a default page loaded
  if (mWebShellWin) {
    PRBool loadDefault;
    mWebShellWin->ShouldLoadDefaultPage(&loadDefault);
    if (!loadDefault)
      return NS_OK;
  }

  nsCOMPtr<nsIDOMElement>    argsElement;

  rv = FindNamedXULElement(mDocShell, "args", &argsElement);
  if (!argsElement) {
    // Couldn't get the "args" element from the xul file. Load a blank page
    if (APP_DEBUG) printf("Couldn't find args element\n");
    nsAutoString url ("about:blank");
    rv = LoadUrl( url.GetUnicode() );
    return rv;
  }

  // Load the default page mentioned in the xul file.
  nsString value;
  argsElement->GetAttribute(nsString("value"), value);
  if (value.Length()) {
    rv = LoadUrl(value.GetUnicode());
    return rv;
  }

  if (APP_DEBUG) printf("Quitting LoadInitialPage\n");
  return NS_OK;
}

static
nsIScriptContext *    
GetScriptContext(nsIDOMWindow * aWin) {
  nsIScriptContext * scriptContext = nsnull;
  if (nsnull != aWin) {
    nsCOMPtr<nsIScriptGlobalObject> global(do_QueryInterface(aWin));
    if (!NS_WARN_IF_FALSE(global, "This should succeed")) {
      global->GetContext(&scriptContext);
    }
  }

  return scriptContext;
}

NS_IMETHODIMP    
nsBrowserAppCore::SetContentWindow(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

  mContentWindow = aWin;
  // NS_ADDREF(aWin); WE DO NOT OWN THIS


  // we do not own the script context, so don't addref it
  nsCOMPtr<nsIScriptContext>  scriptContext = getter_AddRefs(GetScriptContext(aWin));
  mContentScriptContext = scriptContext;

  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(mContentWindow) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));
  if (webShell) {
    mContentAreaWebShell = webShell;
    // NS_ADDREF(mContentAreaWebShell); WE DO NOT OWN THIS
    docShell->SetDocLoaderObserver((nsIDocumentLoaderObserver *)this);
  if (mSHistory)
       webShell->SetSessionHistory(mSHistory);

    // Cache the Document Loader for the content area webshell.  This is a 
    // weak reference that is *not* reference counted...
    nsCOMPtr<nsIDocumentLoader> docLoader;

    webShell->GetDocumentLoader(*getter_AddRefs(docLoader));
    mContentAreaDocLoader = docLoader.get();

    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
    nsXPIDLString name;
    docShellAsItem->GetName(getter_Copies(name));
    nsAutoString str(name);

    if (APP_DEBUG) {
      printf("Attaching to Content WebShell [%s]\n", (const char *)nsCAutoString(str));
    }
  }

  return NS_OK;

}



NS_IMETHODIMP    
nsBrowserAppCore::SetWebShellWindow(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (docShell) {
    mDocShell = docShell.get();
    //NS_ADDREF(mDocShell); WE DO NOT OWN THIS
    // inform our top level webshell that we are its parent URI content listener...
    docShell->SetParentURIContentListener(this);

    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
    nsXPIDLString name;
    docShellAsItem->GetName(getter_Copies(name));
    nsAutoString str(name);

    if (APP_DEBUG) {
      printf("Attaching to WebShellWindow[%s]\n", (const char *)nsCAutoString(str));
    }

    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));
    nsCOMPtr<nsIWebShellContainer> webShellContainer;
    webShell->GetContainer(*getter_AddRefs(webShellContainer));
    if (nsnull != webShellContainer)
    {
      nsCOMPtr<nsIWebShellWindow> webShellWin;
      if (NS_OK == webShellContainer->QueryInterface(NS_GET_IID(nsIWebShellWindow), getter_AddRefs(webShellWin)))
      {
        mWebShellWin = webShellWin;   // WE DO NOT OWN THIS
      }
    }
  }
  return NS_OK;
}



// Utility to extract document from a webshell object.
static nsCOMPtr<nsIDocument> getDocument( nsIDocShell *aDocShell ) {
    nsCOMPtr<nsIDocument> result;

    // Get content viewer from the web shell.
    nsCOMPtr<nsIContentViewer> contentViewer;
    nsresult rv = aDocShell ? aDocShell->GetContentViewer(getter_AddRefs(contentViewer))
                            : NS_ERROR_NULL_POINTER;

    if ( contentViewer ) {
        // Up-cast to a document viewer.
        nsCOMPtr<nsIDocumentViewer> docViewer(do_QueryInterface(contentViewer));
        if ( docViewer ) {
            // Get the document from the doc viewer.
            docViewer->GetDocument(*getter_AddRefs(result));
        } else {
            if (APP_DEBUG) printf( "%s %d: Upcast to nsIDocumentViewer failed\n",
                                   __FILE__, (int)__LINE__ );
        }
    } else {
        if (APP_DEBUG) printf( "%s %d: GetContentViewer failed, rv=0x%X\n",
                               __FILE__, (int)__LINE__, (int)rv );
    }
    return result;
}

// Utility to set element attribute.
static nsresult setAttribute( nsIDocShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value ) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsIDocument> doc = getDocument( shell );

    if ( doc ) {
        // Up-cast.
        nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) );
        if ( xulDoc ) {
            // Find specified element.
            nsCOMPtr<nsIDOMElement> elem;
            rv = xulDoc->GetElementById( id, getter_AddRefs(elem) );
            if ( elem ) {
                // Set the text attribute.
                rv = elem->SetAttribute( name, value );
                if ( rv != NS_OK ) {
                     if (APP_DEBUG) printf("SetAttribute failed, rv=0x%X\n",(int)rv);
                }
            } else {
                if (APP_DEBUG) printf("GetElementByID failed, rv=0x%X\n",(int)rv);
            }
        } else {
          if (APP_DEBUG)   printf("Upcast to nsIDOMXULDocument failed\n");
        }
    } else {
        if (APP_DEBUG) printf("getDocument failed, rv=0x%X\n",(int)rv);
    }
    return rv;
}

// nsIDocumentLoaderObserver methods

NS_IMETHODIMP
nsBrowserAppCore::OnStartDocumentLoad(nsIDocumentLoader* aLoader, nsIURI* aURL, const char* aCommand)
{
  NS_PRECONDITION(aLoader != nsnull, "null ptr");
  if (! aLoader)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aURL != nsnull, "null ptr");
  if (! aURL)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;

  // Notify observers that a document load has started in the
  // content window.
  NS_WITH_SERVICE(nsIObserverService, observer, NS_OBSERVERSERVICE_PROGID, &rv);
  if (NS_FAILED(rv)) return rv;

  char* url;
  rv = aURL->GetSpec(&url);
  if (NS_FAILED(rv)) return rv;

  nsAutoString urlStr(url);

#ifdef DEBUG_warren
  char* urls;
  aURL->GetSpec(&urls);
  if (gTimerLog == nsnull)
    gTimerLog = PR_NewLogModule("Timer");
  mLoadStartTime = PR_IntervalNow();
  PR_LOG(gTimerLog, PR_LOG_DEBUG, 
         (">>>>> Starting timer for %s\n", urls));
  printf(">>>>> Starting timer for %s\n", urls);
  nsCRT::free(urls);
#endif

  // Check if this notification is for a frame
  PRBool isFrame=PR_FALSE;
  nsCOMPtr<nsISupports> container;
  aLoader->GetContainer(getter_AddRefs(container));
  if (container) {
     nsCOMPtr<nsIDocShellTreeItem>   docShellAsItem(do_QueryInterface(container));
   if (docShellAsItem) {
       nsCOMPtr<nsIDocShellTreeItem> parent;
     docShellAsItem->GetSameTypeParent(getter_AddRefs(parent));
     if (parent) 
       isFrame = PR_TRUE;
     }
  }

  if (!isFrame) {
    nsAutoString kStartDocumentLoad("StartDocumentLoad");
    rv = observer->Notify(mContentWindow,
                        kStartDocumentLoad.GetUnicode(),
                        urlStr.GetUnicode());

    // XXX Ignore rv for now. They are using nsIEnumerator instead of
    // nsISimpleEnumerator.
    // set the url string in the urlbar only for toplevel pages, not for frames
  setAttribute( mDocShell, "urlbar", "value", url);
  }


  // Kick start the throbber
  nsAutoString trueStr("true");
  nsAutoString emptyStr;
  setAttribute( mDocShell, "Browser:Throbber", "busy", trueStr );

  // Enable the Stop buton
  setAttribute( mDocShell, "canStop", "disabled", emptyStr );

  //Disable the reload button
  setAttribute(mDocShell, "canReload", "disabled", trueStr);

  PRBool result=PR_TRUE;
  // Check with sessionHistory if you can go forward
  CanGoForward(&result);
  setAttribute(mDocShell, "canGoForward", "disabled", (result == PR_TRUE) ? "" : "true");


    // Check with sessionHistory if you can go back
  CanGoBack(&result);
  setAttribute(mDocShell, "canGoBack", "disabled", (result == PR_TRUE) ? "" : "true");


  nsCRT::free(url);

  return NS_OK;
}


NS_IMETHODIMP
nsBrowserAppCore::OnEndDocumentLoad(nsIDocumentLoader* aLoader, nsIChannel* channel, nsresult aStatus)
{
  NS_PRECONDITION(aLoader != nsnull, "null ptr");
  if (! aLoader)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(channel != nsnull, "null ptr");
  if (! channel)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;

  nsCOMPtr<nsIURI> aUrl;
  rv = channel->GetOriginalURI(getter_AddRefs(aUrl));
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString url;
  rv = aUrl->GetSpec(getter_Copies(url));
  if (NS_FAILED(rv)) return rv;

  PRBool  isFrame=PR_FALSE;
  nsCOMPtr<nsISupports> container;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem, parent;

  aLoader->GetContainer(getter_AddRefs(container));
  // Is this a frame ?
  docShellAsItem = do_QueryInterface(container);
  if (docShellAsItem) {
    docShellAsItem->GetSameTypeParent(getter_AddRefs(parent));
  }
  if (parent)
  isFrame = PR_TRUE;


  if (mContentAreaDocLoader) {
    PRBool isBusy = PR_FALSE;

    mContentAreaDocLoader->IsBusy(&isBusy);
    if (isBusy) {
      return NS_OK;
    }
  }

  /* Inform Session History about the status of the page load */
  nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShellAsItem));
  if (mSHistory) {
    mSHistory->UpdateStatus(webShell, (PRInt32) aStatus); 
  }
  if (mIsLoadingHistory) {
      SetLoadingFlag(PR_FALSE);
  }

  /* If this is a frame, don't do any of the Global History
   * & observer thingy 
   */
  if (!isFrame) {
      nsAutoString urlStr(url);
      nsAutoString kEndDocumentLoad("EndDocumentLoad");
      nsAutoString kFailDocumentLoad("FailDocumentLoad");

      // Notify observers that a document load has started in the
      // content window.
      NS_WITH_SERVICE(nsIObserverService, observer, NS_OBSERVERSERVICE_PROGID, &rv);
      if (NS_FAILED(rv)) return rv;

      rv = observer->Notify(mContentWindow,
                            NS_SUCCEEDED(aStatus) ? kEndDocumentLoad.GetUnicode() : kFailDocumentLoad.GetUnicode(),
                            urlStr.GetUnicode());

      // XXX Ignore rv for now. They are using nsIEnumerator instead of
      // nsISimpleEnumerator.
    /*
     * Update the 'Go' menu. I know this adds discrepancy between the 'Go'
     * menu and the Back button when it comes to Session History in frame
     * pages. But most of these sub-frames don't have title which leads to
     * blank menu items in the 'go' menu. So, I'm taking sub-frames
     * totally off the go menu. This is how 4.x behaves.
     */ 
      /* Partially loaded and unresolved urls now get in to SH. So,
     * add them in to go menu too for consistency sake. Revisit when
     * browser implements nsStreamListener or something similar when it can
     * distinguish between unresolved urls and partially loaded urls
     */
      UpdateGoMenu();

      /* To satisfy a request from the QA group */
      if (aStatus == NS_OK) {
        fprintf(stdout, "Document %s loaded successfully\n", (const char*)url);
        fflush(stdout);
    }
      else {
        fprintf(stdout, "Error loading URL %s \n", (const char*)url);
        fflush(stdout);
    }
  } //if (!isFrame)

#ifdef DEBUG_warren
  char* urls;
  aUrl->GetSpec(&urls);
  if (gTimerLog == nsnull)
    gTimerLog = PR_NewLogModule("Timer");
  PRIntervalTime end = PR_IntervalNow();
  PRIntervalTime diff = end - mLoadStartTime;
  PR_LOG(gTimerLog, PR_LOG_DEBUG, 
         (">>>>> Stopping timer for %s. Elapsed: %.3f\n", 
          urls, PR_IntervalToMilliseconds(diff) / 1000.0));
  printf(">>>>> Stopping timer for %s. Elapsed: %.3f\n", 
         urls, PR_IntervalToMilliseconds(diff) / 1000.0);
  nsCRT::free(urls);
#endif

  setAttribute( mDocShell, "Browser:Throbber", "busy", "false" );

    //Disable the Stop button
  setAttribute( mDocShell, "canStop", "disabled", "true" );

  //Enable the reload button
  setAttribute(mDocShell, "canReload", "disabled", "");

  return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::OnStartURLLoad(nsIDocumentLoader* loader, 
                                 nsIChannel* channel)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::OnProgressURLLoad(nsIDocumentLoader* loader, 
                                    nsIChannel* channel, PRUint32 aProgress, 
                                    PRUint32 aProgressMax)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;
  char *urlString = 0;
  aURL->GetSpec( &urlString );
  nsCRT::free(urlString);
  return rv;
}


NS_IMETHODIMP
nsBrowserAppCore::OnStatusURLLoad(nsIDocumentLoader* loader, 
                                  nsIChannel* channel, nsString& aMsg)
{
  nsresult rv = setAttribute( mDocShell, "WebBrowserChrome", "status", aMsg );
   return rv;
}


NS_IMETHODIMP
nsBrowserAppCore::OnEndURLLoad(nsIDocumentLoader* loader, 
                               nsIChannel* channel, nsresult aStatus)
{
  return NS_OK;
}

/////////////////////////////////////////////////////////
//             nsISessionHistory methods              //
////////////////////////////////////////////////////////


NS_IMETHODIMP    
nsBrowserAppCore::GoBack(nsIWebShell * aPrev)
{
  if (mIsLoadingHistory) {
    SetLoadingFlag(PR_FALSE);
  }
  mIsLoadingHistory = PR_TRUE;
  if (mSHistory) {
    //mSHistory checks for null pointers
    return mSHistory->GoBack(aPrev);
  }


  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::GoForward(nsIWebShell * aPrev)
{
  if (mIsLoadingHistory) {
     SetLoadingFlag(PR_FALSE);
  }
  mIsLoadingHistory = PR_TRUE;

  if (mSHistory) {
    //mSHistory checks for null pointers
    return mSHistory->GoForward(aPrev);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::Reload(nsIWebShell * aPrev, nsLoadFlags aType)
{
  if (mIsLoadingHistory) {
     SetLoadingFlag(PR_FALSE);
  }
  mIsLoadingHistory = PR_TRUE;
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(aPrev));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  webNav->Reload(nsIWebNavigation::reloadNormal);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::Add(const char * aURL, const char * aReferrer, nsIWebShell * aWebShell)
{
 return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::Goto(PRInt32 aGotoIndex, nsIWebShell * aPrev, PRBool aIsReloading)
{
   nsresult rv=NS_OK;
   if (mSHistory) {
     //mSHistory checks for null pointers
     rv = mSHistory->Goto(aGotoIndex, aPrev, PR_FALSE);
   }
   return rv;
}


NS_IMETHODIMP
nsBrowserAppCore::SetLoadingFlag(PRBool aFlag)
{
  mIsLoadingHistory = aFlag;
  if (mSHistory)
  mSHistory->SetLoadingFlag(aFlag);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::UpdateStatus(nsIWebShell * aWebShell, PRInt32 aStatus) {
  if (mSHistory) {
    //mSHistory checks for null pointers
    mSHistory->UpdateStatus(aWebShell, aStatus);
  }
  return NS_OK;
}

/* Error checks on the arguments for all the following
 * methods done in nsSessionHistory.cpp
 */

NS_IMETHODIMP
nsBrowserAppCore::GetLoadingFlag(PRBool *aFlag)
{

  if (mSHistory)
  mSHistory->GetLoadingFlag(aFlag);
  return NS_OK;
}


NS_IMETHODIMP
nsBrowserAppCore::CanGoForward(PRBool * aResult)
{

   if (mSHistory) {
     mSHistory->CanGoForward(aResult);
   }


   return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::CanGoBack(PRBool * aResult)
{
   if (mSHistory)
     mSHistory->CanGoBack(aResult);
   return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::GetHistoryLength(PRInt32 * aResult)
{

   if (mSHistory)
     mSHistory->GetHistoryLength(aResult);
   return NS_OK;
}


NS_IMETHODIMP
nsBrowserAppCore::GetCurrentIndex(PRInt32 * aResult)
{

   if (mSHistory)
     mSHistory->GetCurrentIndex(aResult);
   return NS_OK;

}

NS_IMETHODIMP
nsBrowserAppCore::GetURLForIndex(PRInt32 aIndex,  char** aURL)
{
   if (mSHistory)
     return  mSHistory->GetURLForIndex(aIndex, aURL);
   return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::SetURLForIndex(PRInt32 aIndex, const char* aURL)
{
   if (mSHistory)
      mSHistory->SetURLForIndex(aIndex, aURL);
   return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::GetTitleForIndex(PRInt32 aIndex,  PRUnichar** aTitle)
{

   if (mSHistory)
      mSHistory->GetTitleForIndex(aIndex, aTitle);
   return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::SetTitleForIndex(PRInt32 aIndex, const PRUnichar* aTitle)
{
   if (mSHistory)
      mSHistory->SetTitleForIndex(aIndex, aTitle);
   return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::GetHistoryObjectForIndex(PRInt32 aIndex, nsISupports ** aState)
{

   if (mSHistory)
      mSHistory->GetHistoryObjectForIndex(aIndex, aState);
   return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::SetHistoryObjectForIndex(PRInt32 aIndex, nsISupports * aState)
{
   if (mSHistory)
      mSHistory->SetHistoryObjectForIndex(aIndex, aState);
   return NS_OK;
}


NS_IMETHODIMP    
nsBrowserAppCore::PrintPreview()
{ 

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Copy()
{ 
   nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mContentAreaWebShell));
   nsCOMPtr<nsIPresShell> presShell;
   docShell->GetPresShell(getter_AddRefs(presShell));
  if (presShell) {
    presShell->DoCopy();
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Print()
{  
   nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mContentAreaWebShell));
  if (docShell) {
    nsCOMPtr<nsIContentViewer> viewer;    
    docShell->GetContentViewer(getter_AddRefs(viewer));    
    if (nsnull != viewer) {
      nsCOMPtr<nsIContentViewerFile> viewerFile = do_QueryInterface(viewer);
      if (viewerFile) {
        NS_ENSURE_SUCCESS(viewerFile->Print(PR_FALSE,nsnull), NS_ERROR_FAILURE);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Close()
{ 
  // if we have already been closed....then just return
  if (mIsClosed) 
    return NS_OK;
  else
    mIsClosed = PR_TRUE;

  // Undo other stuff we did in SetContentWindow.
  if ( mContentAreaWebShell ) {
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mContentAreaWebShell));
      docShell->SetDocLoaderObserver( 0 );
      mContentAreaWebShell->SetSessionHistory( 0 );
  }

  // session history is an instance, not a service
  NS_IF_RELEASE(mSHistory);

  // Release search context.
  mSearchContext = 0;

  // unregister ourselves with the uri loader because
  // we can no longer accept new content!
  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIURILoader, dispatcher, NS_URI_LOADER_PROGID, &rv);
  if (NS_SUCCEEDED(rv)) 
    rv = dispatcher->UnRegisterContentListener(this);

  return NS_OK;
}


void
nsBrowserAppCore::InitializeSearch( nsIFindComponent *finder )
{
    nsresult rv = NS_OK;

    if ( finder && !mSearchContext ) {
        // Create the search context for this browser window.
        rv = finder->CreateContext( mContentAreaWebShell, nsnull, getter_AddRefs(mSearchContext));
        if ( NS_FAILED( rv ) ) {
            #ifdef NS_DEBUG
            printf( "%s %d CreateContext failed, rv=0x%X\n",
                    __FILE__, (int)__LINE__, (int)rv );
            #endif
        }
    }
}

NS_IMETHODIMP    
nsBrowserAppCore::Find()
{
    nsresult rv = NS_OK;
    PRBool   found = PR_FALSE;
    
    // Get find component.
    nsIFindComponent *finder;
    rv = nsServiceManager::GetService( NS_IFINDCOMPONENT_PROGID,
                                       NS_GET_IID(nsIFindComponent),
                                       (nsISupports**)&finder );
    if ( NS_SUCCEEDED( rv ) ) {
        // Make sure we've initialized searching for this document.
        InitializeSearch( finder );

        // Perform find via find component.
        if ( finder && mSearchContext ) {
            rv = finder->Find( mSearchContext, &found );
        }

        // Release the service.
        nsServiceManager::ReleaseService( NS_IFINDCOMPONENT_PROGID, finder );
    } else {
        #ifdef NS_DEBUG
            printf( "%s %d: GetService failed for find component, rv=0x08%X\n",
                    __FILE__, (int)__LINE__, (int)rv );
        #endif
    }

    return rv;
}

NS_IMETHODIMP    
nsBrowserAppCore::FindNext()
{
    nsresult rv = NS_OK;
    PRBool   found = PR_FALSE;

    // Get find component.
    nsIFindComponent *finder;
    rv = nsServiceManager::GetService( NS_IFINDCOMPONENT_PROGID,
                                       NS_GET_IID(nsIFindComponent),
                                       (nsISupports**)&finder );
    if ( NS_SUCCEEDED( rv ) ) {
        // Make sure we've initialized searching for this document.
        InitializeSearch( finder );

        // Perform find via find component.
        if ( finder && mSearchContext ) {
            rv = finder->FindNext( mSearchContext, &found );
        }

        // Release the service.
        nsServiceManager::ReleaseService( NS_IFINDCOMPONENT_PROGID, finder );
    } else {
        #ifdef NS_DEBUG
            printf( "%s %d: GetService failed for find component, rv=0x08%X\n",
                    __FILE__, (int)__LINE__, (int)rv );
        #endif
    }

    return rv;
}

// XXXbe why is this needed?  eliminate
NS_IMETHODIMP    
nsBrowserAppCore::ExecuteScript(nsIScriptContext * aContext, const nsString& aScript)
{
  if (nsnull != aContext) {
    const char* url = "";
    PRBool isUndefined = PR_FALSE;
    nsString rVal;
    if (APP_DEBUG) {
      printf("Executing [%s]\n", (const char *)nsCAutoString(aScript));
    }
    aContext->EvaluateString(aScript, url, 0, nsnull, rVal, &isUndefined);
  } 
  return NS_OK;
}




//----------------------------------------------------------------------



static nsresult
FindNamedXULElement(nsIDocShell * aShell,
                              const char *aId,
                              nsCOMPtr<nsIDOMElement> * aResult ) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsIContentViewer> cv;
    rv = aShell ? aShell->GetContentViewer(getter_AddRefs(cv))
               : NS_ERROR_NULL_POINTER;
    if ( cv ) {
        // Up-cast.
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
        if ( docv ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(*getter_AddRefs(doc));
            if ( doc ) {
                // Up-cast.

                nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) );
                  if ( xulDoc ) {
                    // Find specified element.
                    nsCOMPtr<nsIDOMElement> elem;

                    rv = xulDoc->GetElementById( aId, getter_AddRefs(elem) );
                    if ( elem ) {
      *aResult =  elem;
                    } else {
                       if (APP_DEBUG) printf("GetElementByID failed, rv=0x%X\n",(int)rv);
                    }
                } else {
                  if (APP_DEBUG)   printf("Upcast to nsIDOMXULDocument failed\n");
                }

            } else {
               if (APP_DEBUG)  printf("GetDocument failed, rv=0x%X\n",(int)rv);
            }
        } else {
             if (APP_DEBUG)  printf("Upcast to nsIDocumentViewer failed\n");
        }
    } else {
       if (APP_DEBUG) printf("GetContentViewer failed, rv=0x%X\n",(int)rv);
    }
    return rv;
}

NS_IMETHODIMP
nsBrowserAppCore::SelectAll()
{
  nsresult rv;
  nsCOMPtr<nsIClipboardCommands> clip(do_QueryInterface(mContentAreaWebShell,&rv));
  if ( NS_SUCCEEDED(rv) ) {
      rv = clip->SelectAll();
  }

  return rv;
}

// nsIURIContentListener support

NS_IMETHODIMP nsBrowserInstance::OnStartURIOpen(nsIURI* aURI, 
   const char* aWindowTarget, PRBool* aAbortOpen)
{
   return NS_OK;
}


NS_IMETHODIMP 
nsBrowserInstance::GetProtocolHandler(nsIURI * /* aURI */, nsIProtocolHandler **aProtocolHandler)
{
   // we don't have any app specific protocol handlers we want to use so 
  // just use the system default by returning null.
  *aProtocolHandler = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::DoContent(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, 
                             nsIChannel *aChannel, nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
  // forward the DoContent call to our content area webshell
  nsCOMPtr<nsIURIContentListener> ctnListener = do_QueryInterface(mContentAreaWebShell);
  if (ctnListener)
    return ctnListener->DoContent(aContentType, aCommand, aWindowTarget, aChannel, aContentHandler, aAbortProcess);
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::IsPreferred(const char * aContentType,
                               nsURILoadCommand aCommand,
                               const char * aWindowTarget,
                               char ** aDesiredContentType,
                               PRBool * aCanHandleContent)
{
  // the browser window is the primary content handler for the following types:
  // If we are asked to handle any of these types, we will always say Yes!
  // regardlesss of the uri load command.
  //    incoming Type                     Preferred type
  //      text/html
  //      text/xul
  //      text/rdf
  //      text/xml
  //      text/css
  //      image/gif
  //      image/jpeg
  //      image/png
  //      image/tiff
  //      application/http-index-format

  if (aContentType)
  {
     // (1) list all content types we want to  be the primary handler for....
     // and suggest a desired content type if appropriate...
     if (nsCRT::strcasecmp(aContentType,  "text/html") == 0
       || nsCRT::strcasecmp(aContentType, "text/xul") == 0
       || nsCRT::strcasecmp(aContentType, "text/rdf") == 0 
       || nsCRT::strcasecmp(aContentType, "text/xml") == 0
       || nsCRT::strcasecmp(aContentType, "text/css") == 0
       || nsCRT::strcasecmp(aContentType, "image/gif") == 0
       || nsCRT::strcasecmp(aContentType, "image/jpeg") == 0
       || nsCRT::strcasecmp(aContentType, "image/png") == 0
       || nsCRT::strcasecmp(aContentType, "image/tiff") == 0
       || nsCRT::strcasecmp(aContentType, "application/http-index-format") == 0)
       *aCanHandleContent = PR_TRUE;
  }
  else
    *aCanHandleContent = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::CanHandleContent(const char * aContentType,
                                    nsURILoadCommand aCommand,
                                    const char * aWindowTarget,
                                    char ** aDesiredContentType,
                                    PRBool * aCanHandleContent)

{
  // can handle is really determined by what our docshell can 
  // load...so ask it....
  nsCOMPtr<nsIURIContentListener> ctnListener (do_GetInterface(mContentAreaWebShell)); 
  if (ctnListener)
    return ctnListener->CanHandleContent(aContentType, aCommand, aWindowTarget, aDesiredContentType, aCanHandleContent);
  else
    *aCanHandleContent = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::GetParentContentListener(nsIURIContentListener** aParent)
{
  *aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::SetParentContentListener(nsIURIContentListener* aParent)
{
  NS_ASSERTION(!aParent, "SetParentContentListener on the application level should never be called");
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::GetLoadCookie(nsISupports ** aLoadCookie)
{
  *aLoadCookie = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::SetLoadCookie(nsISupports * aLoadCookie)
{
  NS_ASSERTION(!aLoadCookie, "SetLoadCookie on the application level should never be called");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// browserCntHandler is a content handler component that registers
// the browse as the preferred content handler for various content
// types like text/html, image/jpeg, etc. When the uri loader
// has a content type that no currently open window wants to handle, 
// it will ask the registry for a registered content handler for this
// type. If the browser is registered to handle that type, we'll end
// up in here where we create a new browser window for the url.
//
// I had intially written this as a JS component and would like to do
// so again. However, JS components can't access xpconnect objects that
// return DOM objects. And we need a dom window to bootstrap the browser
/////////////////////////////////////////////////////////////////////////

class nsBrowserContentHandler : public nsIContentHandler, nsICmdLineHandler
{
public:
  NS_DECL_NSICONTENTHANDLER
  NS_DECL_NSICMDLINEHANDLER
  NS_DECL_ISUPPORTS
  CMDLINEHANDLER_REGISTERPROC_DECLS

  nsBrowserContentHandler();
  virtual ~nsBrowserContentHandler();

protected:

};

NS_IMPL_ADDREF(nsBrowserContentHandler);
NS_IMPL_RELEASE(nsBrowserContentHandler);

NS_INTERFACE_MAP_BEGIN(nsBrowserContentHandler)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentHandler)
   NS_INTERFACE_MAP_ENTRY(nsIContentHandler)
   NS_INTERFACE_MAP_ENTRY(nsICmdLineHandler)
NS_INTERFACE_MAP_END

nsBrowserContentHandler::nsBrowserContentHandler()
{
  NS_INIT_ISUPPORTS();
}

nsBrowserContentHandler::~nsBrowserContentHandler()
{
}

CMDLINEHANDLER2_IMPL(nsBrowserContentHandler,"-chrome","general.startup.browser","chrome://navigator/content/","Start with browser.",NS_BROWSERSTARTUPHANDLER_PROGID,"Browser Startup Handler", PR_TRUE, PR_FALSE)

NS_IMETHODIMP nsBrowserContentHandler::GetDefaultArgs(PRUnichar **aDefaultArgs) 
{ 
    static PRBool timebombChecked = PR_FALSE;

    if (!aDefaultArgs) return NS_ERROR_FAILURE; 

    nsString args;

#ifdef FORCE_CHECKIN_GUIDELINES
    printf("FOR DEBUG BUILDS ONLY:  we are forcing you to see the checkin guidelines when you open a browser window\n");
    args = "http://www.mozilla.org/quality/checkin-guidelines.html";
#else
    nsresult rv;
    nsXPIDLCString url;

    if (timebombChecked == PR_FALSE)
    {
        // timebomb check
        timebombChecked = PR_TRUE;
        
        PRBool expired;
        NS_WITH_SERVICE(nsITimeBomb, timeBomb, kTimeBombCID, &rv);
        if ( NS_FAILED(rv) ) return rv; 

        rv = timeBomb->Init();
        if ( NS_FAILED(rv) ) return rv; 

        rv = timeBomb->CheckWithUI(&expired);
        if ( NS_FAILED(rv) ) return rv; 

        if ( expired ) 
        {
            char* urlString;
            rv = timeBomb->GetTimebombURL(&urlString);
            if ( NS_FAILED(rv) ) return rv;

            nsString url ( urlString );
            *aDefaultArgs =  url.ToNewUnicode();
            nsAllocator::Free(urlString);
            return rv;
        }
    }

    /* the default, in case we fail somewhere */
    args = "about:blank";

    nsCOMPtr<nsIPref> prefs(do_GetService(kCPrefServiceCID));
    if (!prefs) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIGlobalHistory> history(do_GetService(kCGlobalHistoryCID));

    PRBool override = PR_FALSE;
    rv = prefs->GetBoolPref(PREF_HOMEPAGE_OVERRIDE, &override);
    if (NS_SUCCEEDED(rv) && override) {
        rv = prefs->CopyCharPref(PREF_HOMEPAGE_OVERRIDE_URL, getter_Copies(url));
        if (NS_SUCCEEDED(rv) && (const char *)url) {
            rv = prefs->SetBoolPref(PREF_HOMEPAGE_OVERRIDE, PR_FALSE);
        }
    }
        
    if (!((const char *)url) || (PL_strlen((const char *)url) == 0)) {
        PRInt32 choice = 0;
        rv = prefs->GetIntPref(PREF_BROWSER_STARTUP_PAGE, &choice);
        if (NS_SUCCEEDED(rv)) {
            switch (choice) {
                case 1:
                    rv = prefs->CopyCharPref(PREF_BROWSER_STARTUP_HOMEPAGE, getter_Copies(url));
                    break;
                case 2:
                    if (history) {
                        rv = history->GetLastPageVisted(getter_Copies(url));
                    }
                    break;
                case 0:
                default:
                    args = "about:blank";
                    break;
            }
        }
    }

    if (NS_SUCCEEDED(rv) && (const char *)url && (PL_strlen((const char *)url))) {              
        args = (const char *) url;
    }
#endif /* FORCE_CHECKIN_GUIDELINES */

    *aDefaultArgs = args.ToNewUnicode(); 
    return NS_OK;
}

NS_IMETHODIMP nsBrowserContentHandler::HandleContent(const char * aContentType,
                                                     const char * aCommand,
                                                     const char * aWindowTarget,
                                                     nsISupports * aWindowContext,
                                                     nsIChannel * aChannel)
{
  // we need a dom window to create the new browser window...in order
  // to do this, we need to get the window mediator service and ask it for a dom window
  NS_ENSURE_ARG(aChannel);
  nsCOMPtr<nsIDOMWindow> parentWindow;
  JSContext* jsContext = nsnull;

  if (aWindowContext)
  {
    parentWindow = do_GetInterface(aWindowContext);
    if (parentWindow)
    {
      nsCOMPtr<nsIScriptGlobalObject> sgo;     
      sgo = do_QueryInterface( parentWindow );
      if (sgo)
      {
        nsCOMPtr<nsIScriptContext> scriptContext;
        sgo->GetContext( getter_AddRefs( scriptContext ) );
        if (scriptContext)
          jsContext = (JSContext*)scriptContext->GetNativeContext();
      }

    }
  }

  // if we still don't have a parent window, try to use the hidden window...
  if (!parentWindow || !jsContext)
  {
    nsCOMPtr<nsIAppShellService> windowService (do_GetService(kAppShellServiceCID));
    NS_ENSURE_SUCCESS(windowService->GetHiddenWindowAndJSContext(getter_AddRefs(parentWindow), &jsContext),
                      NS_ERROR_FAILURE);
  }

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  nsXPIDLCString spec;
  uri->GetSpec(getter_Copies(spec));

  void* mark;
  jsval* argv;

  nsAutoString value = "";
  value += spec;

  // we only want to pass in the window target name if it isn't something like _new or _blank....
  // i.e. only real names like "my window", etc...
  const char * windowTarget = aWindowTarget;
  if (!aWindowTarget || !nsCRT::strcasecmp(aWindowTarget, "_new") || !nsCRT::strcasecmp(aWindowTarget, "_blank"))
    windowTarget = "";

  argv = JS_PushArguments(jsContext, &mark, "Ws", value.GetUnicode(), windowTarget);
  NS_ENSURE_TRUE(argv, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindow> newWindow;
  parentWindow->Open(jsContext, argv, 2, getter_AddRefs(newWindow));
  JS_PopArguments(jsContext, mark);

  // now abort the current channel load...
  aChannel->Cancel();

  return NS_OK;
}

NS_DEFINE_MODULE_INSTANCE_COUNTER()

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsBrowserInstance, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBrowserContentHandler)

static nsModuleComponentInfo components[] = {
  { "nsBrowserInstance",
    NS_BROWSERINSTANCE_CID,
    NS_BROWSERINSTANCE_PROGID, 
    nsBrowserInstanceConstructor
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"text/html", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"text/xul", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"text/rdf", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"text/css", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"image/gif", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"image/jpeg", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"image/png", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"image/tiff", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"application/http-index-format", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Startup Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_BROWSERSTARTUPHANDLER_PROGID, 
    nsBrowserContentHandlerConstructor,
    nsBrowserContentHandler::RegisterProc,
    nsBrowserContentHandler::UnregisterProc,
  },
  { "Chrome Startup Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    "component://netscape/commandlinehandler/general-startup-chrome",
    nsBrowserContentHandlerConstructor,
  } 
  
};

NS_IMPL_NSGETMODULE("nsBrowserModule", components)

