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
#include "nsIPref.h" 
#include "prmem.h"

#ifdef XP_MAC
#include "nsBrowserWindow.h"
#define NS_IMPL_IDS
#else
#define NS_IMPL_IDS
#include "nsBrowserWindow.h"
#endif
#include "nsIStreamListener.h"
#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsIButton.h"
#include "nsIImageGroup.h"
#include "nsITimer.h"
#include "nsIThrobber.h"
#include "nsIDOMDocument.h"
#include "nsIURL.h"
#include "nsIFileWidget.h"
#include "nsILookAndFeel.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"
#include "nsCRT.h"
#include "nsWidgetsCID.h"
#include "nsViewerApp.h"
#include "prprf.h"
#include "nsIComponentManager.h"
#include "nsParserCIID.h"
#include "nsIEnumerator.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"

#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIDocumentViewer.h"
#include "nsIContentViewer.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsXIFDTD.h"
#include "nsIParser.h"
#include "nsHTMLContentSinkStream.h"
#include "nsEditorMode.h"

// Needed for "Find" GUI
#include "nsIDialog.h"
#include "nsICheckButton.h"
#include "nsIRadioButton.h"
#include "nsILabel.h"
#include "nsWidgetSupport.h"

#include "nsXPBaseWindow.h"
#include "nsFindDialog.h"

#include "nsIContentConnector.h"

#include "resources.h"

#if defined(WIN32)
#include <windows.h>
#endif

#include <ctype.h> // tolower

// For Copy
#include "nsIDOMSelection.h"

#include "nsISelectionMgr.h"


// XXX For font setting below
#include "nsFont.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"

#if defined(CookieManagement) || defined(SingleSignon) || defined(ClientWallet)
#include "nsIServiceManager.h"
#endif

#ifdef CookieManagement
#include "nsINetService.h"
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
#endif

#if defined(ClientWallet) || defined(SingleSignon)
#include "nsIWalletService.h"
static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);
#endif


#define THROBBING_N

// XXX greasy constants
#ifdef THROBBING_N
#define THROBBER_WIDTH 32
#define THROBBER_HEIGHT 32
#define THROBBER_AT "resource:/res/throbber/anims%02d.gif"
#define THROB_NUM 29
#else
#define THROBBER_WIDTH 42
#define THROBBER_HEIGHT 42
#define THROBBER_AT "resource:/res/throbber/LargeAnimation%02d.gif"
#define THROB_NUM 38
#endif
#define BUTTON_WIDTH 90
#define BUTTON_HEIGHT THROBBER_HEIGHT

#ifdef INSET_WEBSHELL
#define WEBSHELL_LEFT_INSET 5
#define WEBSHELL_RIGHT_INSET 5
#define WEBSHELL_TOP_INSET 5
#define WEBSHELL_BOTTOM_INSET 5
#else
#define WEBSHELL_LEFT_INSET 0
#define WEBSHELL_RIGHT_INSET 0
#define WEBSHELL_TOP_INSET 0
#define WEBSHELL_BOTTOM_INSET 0
#endif

static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kBrowserWindowCID, NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kButtonCID, NS_BUTTON_CID);
static NS_DEFINE_IID(kFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kTextFieldCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kThrobberCID, NS_THROBBER_CID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID);
static NS_DEFINE_IID(kDialogCID, NS_DIALOG_CID);
static NS_DEFINE_IID(kCheckButtonCID, NS_CHECKBUTTON_CID);
static NS_DEFINE_IID(kRadioButtonCID, NS_RADIOBUTTON_CID);
static NS_DEFINE_IID(kLabelCID, NS_LABEL_CID);
static NS_DEFINE_IID(kIXPBaseWindowIID, NS_IXPBASE_WINDOW_IID);

static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);
static NS_DEFINE_IID(kIButtonIID, NS_IBUTTON_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIThrobberIID, NS_ITHROBBER_IID);
static NS_DEFINE_IID(kIContentConnectorIID, NS_ICONTENTCONNECTOR_IID);
static NS_DEFINE_IID(kCTreeViewCID, NS_TREEVIEW_CID);
static NS_DEFINE_IID(kCToolboxCID, NS_TOOLBARMANAGER_CID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIDialogIID, NS_IDIALOG_IID);
static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kIRadioButtonIID, NS_IRADIOBUTTON_IID);
static NS_DEFINE_IID(kILabelIID, NS_ILABEL_IID);
static NS_DEFINE_IID(kINetSupportIID,         NS_INETSUPPORT_IID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kXPBaseWindowCID, NS_XPBASE_WINDOW_CID);
static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define FILE_PROTOCOL "file://"

// prototypes
#if 0
static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);
#endif
static void* GetItemsNativeData(nsISupports* aObject);

//----------------------------------------------------------------------

static
nsIPresShell*
GetPresShellFor(nsIWebShell* aWebShell)
{
  nsIPresShell* shell = nsnull;
  if (nsnull != aWebShell) {
    nsIContentViewer* cv = nsnull;
    aWebShell->GetContentViewer(&cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIPresContext* cx;
        docv->GetPresContext(cx);
	      if (nsnull != cx) {
	        cx->GetShell(&shell);
	        NS_RELEASE(cx);
	      }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
  return shell;
}

nsVoidArray nsBrowserWindow::gBrowsers;

nsBrowserWindow*
nsBrowserWindow::FindBrowserFor(nsIWidget* aWidget, PRIntn aWhich)
{
  nsIWidget*        widget;
  nsBrowserWindow*  result = nsnull;

  PRInt32 i, n = gBrowsers.Count();
  for (i = 0; i < n; i++) {
    nsBrowserWindow* bw = (nsBrowserWindow*) gBrowsers.ElementAt(i);
    if (nsnull != bw) {
      switch (aWhich) {
      case FIND_WINDOW:
        bw->mWindow->QueryInterface(kIWidgetIID, (void**) &widget);
        if (widget == aWidget) {
          result = bw;
        }
        NS_IF_RELEASE(widget);
        break;
      case FIND_BACK:
        if (bw->mBack) {
          bw->mBack->QueryInterface(kIWidgetIID, (void**) &widget);
          if (widget == aWidget) {
            result = bw;
          }
          NS_IF_RELEASE(widget);
        }
        break;
      case FIND_FORWARD:
        if (bw->mForward) {
          bw->mForward->QueryInterface(kIWidgetIID, (void**) &widget);
          if (widget == aWidget) {
            result = bw;
          }
          NS_IF_RELEASE(widget);
        }
        break;
      case FIND_LOCATION:
        if (bw->mLocation) {
          bw->mLocation->QueryInterface(kIWidgetIID, (void**) &widget);
          if (widget == aWidget) {
            result = bw;
          }
          NS_IF_RELEASE(widget);
        }
        break;
      }
    }
  }
  if (nsnull != result) {
    NS_ADDREF(result);
  }
  return result;
}

void
nsBrowserWindow::AddBrowser(nsBrowserWindow* aBrowser)
{
  gBrowsers.AppendElement(aBrowser);
  NS_ADDREF(aBrowser);
}

void
nsBrowserWindow::RemoveBrowser(nsBrowserWindow* aBrowser)
{
  //nsViewerApp* app = aBrowser->mApp;
  gBrowsers.RemoveElement(aBrowser);
  NS_RELEASE(aBrowser);
}

void
nsBrowserWindow::CloseAllWindows()
{
  while (0 != gBrowsers.Count()) {
    nsBrowserWindow* bw = (nsBrowserWindow*) gBrowsers.ElementAt(0);
    NS_ADDREF(bw);
    bw->Close();
    NS_RELEASE(bw);
  }
  gBrowsers.Clear();
}

static nsEventStatus PR_CALLBACK
HandleBrowserEvent(nsGUIEvent *aEvent)
{ 
  nsEventStatus result = nsEventStatus_eIgnore;
  nsBrowserWindow* bw =
    nsBrowserWindow::FindBrowserFor(aEvent->widget, FIND_WINDOW);
  if (nsnull != bw) {
    nsSizeEvent* sizeEvent;
    switch(aEvent->message) {
    case NS_SIZE:
      sizeEvent = (nsSizeEvent*)aEvent;  
      bw->Layout(sizeEvent->windowSize->width,
		 sizeEvent->windowSize->height);
      result = nsEventStatus_eConsumeNoDefault;
      break;

    case NS_DESTROY:
      {
	nsViewerApp* app = bw->mApp;
	app->CloseWindow(bw);
	result = nsEventStatus_eConsumeDoDefault;

#ifndef XP_MAC
	// XXX Really shouldn't just exit, we should just notify somebody...
	if (0 == nsBrowserWindow::gBrowsers.Count()) {
	  app->Exit();
	}
#endif
      }
      return result;

    case NS_MENU_SELECTED:
      result = bw->DispatchMenuItem(((nsMenuEvent*)aEvent)->mCommand);
      break;

    default:
      break;
    }
    NS_RELEASE(bw);
  }
  return result;
}

static nsEventStatus PR_CALLBACK
HandleBackEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  nsBrowserWindow* bw =
    nsBrowserWindow::FindBrowserFor(aEvent->widget, FIND_BACK);
  if (nsnull != bw) {
    switch(aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_UP:
      bw->Back();
      break;
    }
    NS_RELEASE(bw);
  }
  return result;
}

static nsEventStatus PR_CALLBACK
HandleForwardEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  nsBrowserWindow* bw =
    nsBrowserWindow::FindBrowserFor(aEvent->widget, FIND_FORWARD);
  if (nsnull != bw) {
    switch(aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_UP:
      bw->Forward();
      break;
    }
    NS_RELEASE(bw);
  }
  return result;
}

//----------------------------------------------------------
static void BuildFileURL(const char * aFileName, nsString & aFileURL) 
{
  nsAutoString fileName(aFileName);
  char * str = fileName.ToNewCString();

  PRInt32 len = strlen(str);
  PRInt32 sum = len + sizeof(FILE_PROTOCOL);
  char* lpszFileURL = new char[sum];

  // Translate '\' to '/'
  for (PRInt32 i = 0; i < len; i++) {
    if (str[i] == '\\') {
	    str[i] = '/';
    }
  }

  // Build the file URL
  PR_snprintf(lpszFileURL, sum, "%s%s", FILE_PROTOCOL, str);

  // create string
  aFileURL = lpszFileURL;
  delete[] lpszFileURL;
  delete[] str;
}

//----------------------------------------------------------
static void BuildFileURL(const PRUnichar * aFileName, nsString & aFileURL) 
{
  nsAutoString fileName(aFileName);
  char * str = fileName.ToNewCString();
  BuildFileURL(str, aFileURL);
  delete[] str;
}


static nsEventStatus PR_CALLBACK
HandleLocationEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  nsBrowserWindow* bw =
    nsBrowserWindow::FindBrowserFor(aEvent->widget, FIND_LOCATION);
  if (nsnull != bw) {
    switch (aEvent->message) {
    case NS_KEY_UP:
      if (NS_VK_RETURN == ((nsKeyEvent*)aEvent)->keyCode) {
        nsAutoString text;
        PRUint32 size;
        bw->mLocation->GetText(text, 1000, size);
        bw->GoTo(text);
      }
      break;

    case NS_DRAGDROP_EVENT: {
      /*printf("Drag & Drop Event\n");
      nsDragDropEvent * ev = (nsDragDropEvent *)aEvent;
      nsAutoString fileURL;
      BuildFileURL(ev->mURL, fileURL);
      nsAutoString fileName(ev->mURL);
      char * str = fileName.ToNewCString();

      PRInt32 len = strlen(str);
      PRInt32 sum = len + sizeof(FILE_PROTOCOL);
      char* lpszFileURL = new char[sum];
  
      // Translate '\' to '/'
      for (PRInt32 i = 0; i < len; i++) {
        if (str[i] == '\\') {
	        str[i] = '/';
        }
      }

      // Build the file URL
      PR_snprintf(lpszFileURL, sum, "%s%s", FILE_PROTOCOL, str);

      // Ask the Web widget to load the file URL
      nsString urlStr(lpszFileURL);
      const PRUnichar * uniStr = fileURL.GetUnicode();
      bw->GoTo(uniStr);
      //delete [] lpszFileURL;
      //delete [] str;*/
      } break;

    default:
      break;
    }
    NS_RELEASE(bw);
  }
  return result;
}

nsEventStatus
nsBrowserWindow::DispatchMenuItem(PRInt32 aID)
{
#if defined(CookieManagement) || defined(SingleSignon) || defined(ClientWallet)
  nsresult res;
#ifdef CookieManagement
  nsINetService *netservice;
#endif
#if defined(ClientWallet) || defined(SingleSignon)
  nsIWalletService *walletservice;
#endif
#ifdef ClientWallet
#define WALLET_EDITOR_URL "file:///y|/walleted.html"
  nsAutoString urlString(WALLET_EDITOR_URL);
#endif
#endif

  nsEventStatus result;
#ifdef NS_DEBUG
  result = DispatchDebugMenu(aID);
  if (nsEventStatus_eIgnore != result) {
    return result;
  }
#endif
  result = DispatchStyleMenu(aID);
  if (nsEventStatus_eIgnore != result) {
    return result;
  }

  switch (aID) {
  case VIEWER_EXIT:
    mApp->Exit();
    return nsEventStatus_eConsumeNoDefault;

  case VIEWER_WINDOW_OPEN:
    mApp->OpenWindow();
    break;
  
  case VIEWER_FILE_OPEN:
    DoFileOpen();
    break;

  case VIEW_SOURCE:
    {
      PRInt32 theIndex;
      mWebShell->GetHistoryIndex(theIndex);
      const PRUnichar* theURL;
      mWebShell->GetURL(theIndex,&theURL);
      nsAutoString theString(theURL);
      mApp->ViewSource(theString);
      //XXX Find out how the string is allocated, and perhaps delete it...
    }
    break;
  
  case VIEWER_EDIT_COPY:
    DoCopy();
    break;

  case VIEWER_EDIT_PASTE:
    DoPaste();
    break;

  case VIEWER_EDIT_SELECTALL:
    DoSelectAll();
    break;

  case VIEWER_EDIT_FINDINPAGE:
    DoFind();
    break;

  case VIEWER_DEMO0:
  case VIEWER_DEMO1:
  case VIEWER_DEMO2:
  case VIEWER_DEMO3:
  case VIEWER_DEMO4:
  case VIEWER_DEMO5:
  case VIEWER_DEMO6:
  case VIEWER_DEMO7:
  case VIEWER_DEMO8: 
  case VIEWER_DEMO9: 
  case VIEWER_DEMO10: 
  case VIEWER_DEMO11: 
  case VIEWER_DEMO12: 
  case VIEWER_DEMO13:
  case VIEWER_DEMO14:
  case VIEWER_DEMO15:
    {
      PRIntn ix = aID - VIEWER_DEMO0;
      nsAutoString url(SAMPLES_BASE_URL);
      url.Append("/test");
      url.Append(ix, 10);
      url.Append(".html");
      mWebShell->LoadURL(url);
    }
    break;

  case VIEWER_XPTOOLKITTOOLBAR1:
    {
      nsAutoString url(SAMPLES_BASE_URL);
      url.Append("/toolbarTest1.xul");
      mWebShell->LoadURL(url);
      break;
    }
  case VIEWER_XPTOOLKITTREE1:
    {
      nsAutoString url(SAMPLES_BASE_URL);
      url.Append("/treeTest1.xul");
      mWebShell->LoadURL(url);
      break;
    }
  
  case JS_CONSOLE:
    DoJSConsole();
    break;

  case VIEWER_PREFS:
    DoPrefs();
    break;

  case EDITOR_MODE:
    DoEditorMode(mWebShell);
    break;

  case VIEWER_ONE_COLUMN:
  case VIEWER_TWO_COLUMN:
  case VIEWER_THREE_COLUMN:
    ShowPrintPreview(aID);
    break;

  case VIEWER_PRINT:
    DoPrint();
    break;

  case VIEWER_PRINT_SETUP:
    DoPrintSetup();
    break;

  case VIEWER_TABLE_INSPECTOR:
    DoTableInspector();
    break;

  case VIEWER_IMAGE_INSPECTOR:
    DoImageInspector();
    break;

  case VIEWER_ZOOM_500:
  case VIEWER_ZOOM_300:
  case VIEWER_ZOOM_200:
  case VIEWER_ZOOM_100:
  case VIEWER_ZOOM_070:
  case VIEWER_ZOOM_050:
  case VIEWER_ZOOM_030:
  case VIEWER_ZOOM_020:
    mWebShell->SetZoom((aID - VIEWER_ZOOM_BASE) / 10.0f);
    break;

#ifdef ClientWallet
  case PRVCY_PREFILL:
  case PRVCY_QPREFILL:
  nsIPresShell* shell;
  shell = nsnull;
  shell = GetPresShell();
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_Prefill(shell, (PRVCY_QPREFILL == aID));
    NS_RELEASE(walletservice);
  }

#ifndef HTMLDialogs 
  if (aID == PRVCY_PREFILL) {
    nsAutoString url("file:///y|/htmldlgs.htm");
    nsIBrowserWindow* bw = nsnull;
    mApp->OpenWindow(PRUint32(~0), bw);
    bw->Show();
    ((nsBrowserWindow *)bw)->GoTo(url);
    NS_RELEASE(bw);
  }
#endif
  break;

  case PRVCY_DISPLAY_WALLET:

  /* set a cookie for the javascript wallet editor */
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    nsIURL * url;
    if (!NS_FAILED(NS_NewURL(&url, WALLET_EDITOR_URL))) {
      res = walletservice->WALLET_PreEdit(url);
      NS_RELEASE(walletservice);
    }
  }

  /* invoke the javascript wallet editor */
  mWebShell->LoadURL(urlString);

  break;
#endif

#if defined(CookieManagement)
  case PRVCY_DISPLAY_COOKIES:
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&netservice);
  if ((NS_OK == res) && (nsnull != netservice)) {
    res = netservice->NET_DisplayCookieInfoAsHTML();
    NS_RELEASE(netservice);
  }
  break;
#endif

#if defined(SingleSignon)
  case PRVCY_DISPLAY_SIGNONS:
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->SI_DisplaySignonInfoAsHTML();
    NS_RELEASE(walletservice);
  }
  break;
#endif

  }

  // Any menu IDs that the editor uses will be processed here
  DoEditorTest(mWebShell, aID);

  return nsEventStatus_eIgnore;
}

void
nsBrowserWindow::Back()
{
  mWebShell->Back();
}

void
nsBrowserWindow::Forward()
{
  mWebShell->Forward();
}

void
nsBrowserWindow::GoTo(const PRUnichar* aURL,const char* aCommand)
{
  mWebShell->LoadURL(aURL, aCommand, nsnull);
}


static PRBool GetFileNameFromFileSelector(nsIWidget* aParentWindow,
					  nsString* aFileName, nsString* aDisplayDirectory)
{
  PRBool selectedFileName = PR_FALSE;
  nsIFileWidget *fileWidget;
  nsString title("Open HTML");
  nsresult rv = nsComponentManager::CreateInstance(kFileWidgetCID,
					     nsnull,
					     kIFileWidgetIID,
					     (void**)&fileWidget);
  if (NS_OK == rv) {
    nsString titles[] = {"All Readable Files", "HTML Files",
                         "XML Files", "Image Files", "All Files"};
    nsString filters[] = {"*.htm; *.html; *.xml; *.gif; *.jpg; *.jpeg; *.png",
                          "*.htm; *.html",
                          "*.xml",
                          "*.gif; *.jpg; *.jpeg; *.png",
                          "*.*"};
    fileWidget->SetFilterList(5, titles, filters);
    fileWidget->SetDisplayDirectory(*aDisplayDirectory);
    fileWidget->Create(aParentWindow,
		       title,
		       eMode_load,
		       nsnull,
		       nsnull);

    PRUint32 result = fileWidget->Show();
    if (result) {
      fileWidget->GetFile(*aFileName);
      selectedFileName = PR_TRUE;
    }
 
    fileWidget->GetDisplayDirectory(*aDisplayDirectory);
    NS_RELEASE(fileWidget);
  }

  return selectedFileName;
}

void
nsBrowserWindow::DoFileOpen()
{
  nsAutoString fileName;
  if (GetFileNameFromFileSelector(mWindow, &fileName, &mOpenFileDirectory)) {
    nsAutoString fileURL;
    BuildFileURL(fileName, fileURL);
    // Ask the Web widget to load the file URL
    mWebShell->LoadURL(fileURL);
    Show();
  }
}

#define DIALOG_FONT      "Helvetica"
#define DIALOG_FONT_SIZE 10

/**--------------------------------------------------------------------------------
 * Main Handler
 *--------------------------------------------------------------------------------
 */
#if 0
nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent)
{
  //printf("HandleEvent aEvent->message %d\n", aEvent->message);
  nsEventStatus result = nsEventStatus_eIgnore;
  if (aEvent == nsnull ||  aEvent->widget == nsnull) {
    return result;
  }

  if (aEvent->message == 301 || aEvent->message == 302) {
    //int x = 0;
  }

  void * data;
  aEvent->widget->GetClientData(data);

  if (data == nsnull) {
    nsIWidget * parent = aEvent->widget->GetParent();
    if (parent != nsnull) {
      parent->GetClientData(data);
      NS_RELEASE(parent);
    }
  }
  
  if (data != nsnull) {
    nsBrowserWindow * browserWindow = (nsBrowserWindow *)data;
    result = browserWindow->ProcessDialogEvent(aEvent);
  }

  return result;
}
#endif

static void* GetItemsNativeData(nsISupports* aObject)
{
	void*                   result = nsnull;
	nsIWidget*      widget;
	if (NS_OK == aObject->QueryInterface(kIWidgetIID,(void**)&widget))
	{
		result = widget->GetNativeData(NS_NATIVE_WIDGET);
		NS_RELEASE(widget);
	}
	return result;
}

//---------------------------------------------------------------
NS_IMETHODIMP nsBrowserWindow::FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
	nsIPresShell* shell = GetPresShell();
	if (nsnull != shell) {
	  nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
	  if (doc) {
		  //PRBool foundIt = PR_FALSE;
		  doc->FindNext(aSearchStr, aMatchCase, aSearchDown, aIsFound);
		  if (!aIsFound) {
		    // Display Dialog here
		  }
		  ForceRefresh();
	  }
	  NS_RELEASE(shell);
	}
  return NS_OK;
}

//---------------------------------------------------------------
NS_IMETHODIMP nsBrowserWindow::ForceRefresh()
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsCOMPtr<nsIViewManager> vm;
    shell->GetViewManager(getter_AddRefs(vm));
    if (vm) {
      nsIView* root;
      vm->GetRootView(root);
      if (nsnull != root) {
        vm->UpdateView(root, (nsIRegion*)nsnull, NS_VMREFRESH_IMMEDIATE);
      }
    }
    NS_RELEASE(shell);
  }
  return NS_OK;
}


/**--------------------------------------------------------------------------------
 * Main Handler
 *--------------------------------------------------------------------------------
 */

 
 
nsEventStatus nsBrowserWindow::ProcessDialogEvent(nsGUIEvent *aEvent)
{ 
  nsEventStatus result = nsEventStatus_eIgnore;

	  //printf("aEvent->message %d\n", aEvent->message);
    switch(aEvent->message) {

	case NS_KEY_DOWN: {
	  nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
	  if (NS_VK_RETURN == keyEvent->keyCode) {
	    PRBool matchCase   = PR_FALSE;
	    mMatchCheckBtn->GetState(matchCase);
	    PRBool findDwn     = PR_FALSE;
	    mDwnRadioBtn->GetState(findDwn);
	    nsString searchStr;
	    PRUint32 actualSize;
	    mTextField->GetText(searchStr, 255,actualSize);
      PRBool foundIt;
	    FindNext(searchStr, matchCase, findDwn, foundIt);
	  }
	} break;

	case NS_MOUSE_LEFT_BUTTON_UP: {
		nsIWidget* dialogWidget = nsnull;               
		if (NS_OK !=  mDialog->QueryInterface(kIWidgetIID,(void**)&dialogWidget))
			break;
				
	  if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetItemsNativeData(mCancelBtn)) {
	    dialogWidget->Show(PR_FALSE);
	  } else if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetItemsNativeData(mFindBtn)) {

	    PRBool matchCase   = PR_FALSE;
	    mMatchCheckBtn->GetState(matchCase);
	    PRBool findDwn     = PR_FALSE;
	    mDwnRadioBtn->GetState(findDwn);
	    PRUint32 actualSize;
	    nsString searchStr;
	    mTextField->GetText(searchStr, 255,actualSize);

	    nsIPresShell* shell = GetPresShell();
	    if (nsnull != shell) {
	      nsCOMPtr<nsIDocument> doc;
        shell->GetDocument(getter_AddRefs(doc));
	      if (doc) {
          PRBool foundIt = PR_FALSE;
          doc->FindNext(searchStr, matchCase, findDwn, foundIt);
          if (!foundIt) {
            // Display Dialog here
          }
          ForceRefresh();
	      }
	      NS_RELEASE(shell);
	    }

	  } else if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetItemsNativeData(mUpRadioBtn)) {
	    mUpRadioBtn->SetState(PR_TRUE);
	    mDwnRadioBtn->SetState(PR_FALSE);
	  } else if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetItemsNativeData(mDwnRadioBtn)) {
	    mDwnRadioBtn->SetState(PR_TRUE);
	    mUpRadioBtn->SetState(PR_FALSE);
	  } else if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetItemsNativeData(mMatchCheckBtn)) {
	    PRBool state = PR_FALSE;
		mMatchCheckBtn->GetState(state);
	    mMatchCheckBtn->SetState(!state);
	  }
	  } break;
	
	case NS_PAINT: 
#ifndef XP_UNIX
	      // paint the background
	    if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetItemsNativeData(mDialog)) {
		nsIRenderingContext *drawCtx = ((nsPaintEvent*)aEvent)->renderingContext;
		drawCtx->SetColor(aEvent->widget->GetBackgroundColor());
		drawCtx->FillRect(*(((nsPaintEvent*)aEvent)->rect));

		return nsEventStatus_eIgnore;
	    }
#endif
	    break;
	default:
	    result = nsEventStatus_eIgnore;
    }
    //printf("result: %d = %d\n", result, PR_FALSE);

    return result;
}

void
nsBrowserWindow::DoFind()
{
  if (mXPDialog) {
    NS_RELEASE(mXPDialog);
    //mXPDialog->SetVisible(PR_TRUE);
    //return;
  }

  nsString findHTML("resource:/res/samples/find.html");
  //nsString findHTML("resource:/res/samples/find-table.html");
  nsRect rect(0, 0, 510, 170);
  //nsRect rect(0, 0, 470, 126);
  nsString title("Find");

  nsXPBaseWindow * dialog = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kXPBaseWindowCID, nsnull,
                                             kIXPBaseWindowIID,
                                             (void**) &dialog);
  if (rv == NS_OK) {
    dialog->Init(eXPBaseWindowType_dialog, mAppShell, nsnull, findHTML, title, rect, PRUint32(~0), PR_FALSE);
    dialog->SetVisible(PR_TRUE);
 	  if (NS_OK == dialog->QueryInterface(kIXPBaseWindowIID, (void**) &mXPDialog)) {
    }
  }

  nsFindDialog * findDialog = new nsFindDialog(this);
  if (nsnull != findDialog) {
    dialog->AddWindowListener(findDialog);
  }
  //NS_IF_RELEASE(dialog);

}

void
nsBrowserWindow::DoSelectAll()
{

  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      doc->SelectAll();
      ForceRefresh();
    }
    NS_RELEASE(shell);
  }
}


//----------------------------------------------------------------------

#define VIEWER_BUNDLE_URL "resource:/res/viewer.properties"

static nsString* gTitleSuffix = nsnull;

static nsString*
GetTitleSuffix(void)
{
  nsString* suffix = new nsString(" - Failed");
  nsIStringBundleService* service = nsnull;
  nsresult ret = nsServiceManager::GetService(kStringBundleServiceCID,
    kIStringBundleServiceIID, (nsISupports**) &service);
  if (NS_FAILED(ret)) {
    return suffix;
  }
  nsIURL* url = nsnull;
  ret = NS_NewURL(&url, nsString(VIEWER_BUNDLE_URL));
  if (NS_FAILED(ret)) {
    NS_RELEASE(service);
    return suffix;
  }
  nsILocale* locale = nsnull;
  nsIStringBundle* bundle = nsnull;
  ret = service->CreateBundle(url, locale, &bundle);
  NS_RELEASE(url);
  if (NS_FAILED(ret)) {
    NS_RELEASE(service);
    return suffix;
  }
  ret = bundle->GetStringFromID(1, *suffix);
  NS_RELEASE(bundle);
  NS_RELEASE(service);

  return suffix;
}

// Note: operator new zeros our memory
nsBrowserWindow::nsBrowserWindow()
{
  if (!gTitleSuffix) {
    //gTitleSuffix = GetTitleSuffix();
    gTitleSuffix = new nsString(" - Raptor");
  }
  AddBrowser(this);
}

nsBrowserWindow::~nsBrowserWindow()
{
  NS_IF_RELEASE(mPrefs);
  NS_IF_RELEASE(mAppShell);
  NS_IF_RELEASE(mTreeView);
  NS_IF_RELEASE(mTableInspectorDialog);
  NS_IF_RELEASE(mImageInspectorDialog);

  if (nsnull != mTableInspector) {
    delete mTableInspector;
  }
  if (nsnull != mImageInspector) {
    delete mImageInspector;
  }

}

NS_IMPL_ADDREF(nsBrowserWindow)
NS_IMPL_RELEASE(nsBrowserWindow)

nsresult
nsBrowserWindow::QueryInterface(const nsIID& aIID,
				void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtrResult = NULL;

  if (aIID.Equals(kIBrowserWindowIID)) {
    *aInstancePtrResult = (void*) ((nsIBrowserWindow*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStreamObserverIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIWebShellContainerIID)) {
    *aInstancePtrResult = (void*) ((nsIWebShellContainer*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kINetSupportIID)) {
    *aInstancePtrResult = (void*) ((nsINetSupport*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIBrowserWindow*)this));
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsBrowserWindow::Init(nsIAppShell* aAppShell,
		      nsIPref* aPrefs,
		      const nsRect& aBounds,
		      PRUint32 aChromeMask,
		      PRBool aAllowPlugins)
{
  mChromeMask = aChromeMask;
  mAppShell = aAppShell;
  NS_IF_ADDREF(mAppShell);
  mPrefs = aPrefs;
  NS_IF_ADDREF(mPrefs);
  mAllowPlugins = aAllowPlugins;

  // Create top level window
  nsresult rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID,
					     (void**)&mWindow);
  if (NS_OK != rv) {
    return rv;
  }
  nsWidgetInitData initData;
  initData.mBorderStyle = eBorderStyle_window;

  nsRect r(0, 0, aBounds.width, aBounds.height);
  mWindow->Create((nsIWidget*)NULL, r, HandleBrowserEvent,
		              nsnull, aAppShell, nsnull, &initData);
  mWindow->GetClientBounds(r);

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
		       nsScrollPreference_kAuto, aAllowPlugins, PR_TRUE);
  mWebShell->SetContainer((nsIWebShellContainer*) this);
  mWebShell->SetObserver((nsIStreamObserver*)this);
  mWebShell->SetPrefs(aPrefs);
  mWebShell->Show();

  if (NS_CHROME_MENU_BAR_ON & aChromeMask) {
    rv = CreateMenuBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
    mWindow->GetClientBounds(r);
    r.x = r.y = 0;
  }

  if (NS_CHROME_TOOL_BAR_ON & aChromeMask) {
    rv = CreateToolBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
  }

  if (NS_CHROME_STATUS_BAR_ON & aChromeMask) {
    rv = CreateStatusBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
  }

  // Give the embedding app a chance to do platforms-specific window setup
  InitNativeWindow();

  // Now lay it all out
  Layout(r.width, r.height);

  return NS_OK;
}

nsresult
nsBrowserWindow::Init(nsIAppShell* aAppShell,
                      nsIPref* aPrefs,
                      const nsRect& aBounds,
                      PRUint32 aChromeMask,
                      PRBool aAllowPlugins,
                      nsIDocumentViewer* aDocumentViewer,
                      nsIPresContext* aPresContext)
{
  mChromeMask = aChromeMask;
  mAppShell = aAppShell;
  NS_IF_ADDREF(mAppShell);
  mPrefs = aPrefs;
  NS_IF_ADDREF(mPrefs);
  mAllowPlugins = aAllowPlugins;

  // Create top level window
  nsresult rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID,
                                             (void**)&mWindow);
  if (NS_OK != rv) {
    return rv;
  }
  nsRect r(0, 0, aBounds.width, aBounds.height);
  mWindow->Create((nsIWidget*)NULL, r, HandleBrowserEvent,
                  nsnull, aAppShell);
  mWindow->GetClientBounds(r);

  // Create web shell
  rv = nsComponentManager::CreateInstance(kWebShellCID, nsnull,
                                    kIWebShellIID,
                                    (void**)&mWebShell);
  if (NS_OK != rv) {
    return rv;
  }
  r.x = r.y = 0;
  //nsRect ws = r;
  rv = mWebShell->Init(mWindow->GetNativeData(NS_NATIVE_WIDGET), 
                       r.x, r.y, r.width, r.height,
                       nsScrollPreference_kAuto, aAllowPlugins);
  mWebShell->SetContainer((nsIWebShellContainer*) this);
  mWebShell->SetObserver((nsIStreamObserver*)this);
  mWebShell->SetPrefs(aPrefs);

  if (NS_CHROME_MENU_BAR_ON & aChromeMask) {
    rv = CreateMenuBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
    mWindow->GetClientBounds(r);
    r.x = r.y = 0;
  }

  if (NS_CHROME_TOOL_BAR_ON & aChromeMask) {
    rv = CreateToolBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
  }

  if (NS_CHROME_STATUS_BAR_ON & aChromeMask) {
    rv = CreateStatusBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
  }

  // Give the embedding app a chance to do platforms-specific window setup
  InitNativeWindow();
  
  // Now lay it all out
  Layout(r.width, r.height);

  // Create a document viewer and bind it to the webshell
  nsIDocumentViewer* docv;
  aDocumentViewer->CreateDocumentViewerUsing(aPresContext, docv);
  mWebShell->Embed(docv, "duh", nsnull);
  mWebShell->Show();
  NS_RELEASE(docv);

  return NS_OK;
}

// XXX This sort of thing should be in a resource
#define TOOL_BAR_FONT      "Helvetica"
#define TOOL_BAR_FONT_SIZE 12
#define STATUS_BAR_FONT      "Helvetica"
#define STATUS_BAR_FONT_SIZE 10

nsresult
nsBrowserWindow::CreateToolBar(PRInt32 aWidth)
{
  nsresult rv;

  nsIDeviceContext* dc = mWindow->GetDeviceContext();
  float t2d;
  dc->GetTwipsToDevUnits(t2d);
  float d2a;
  dc->GetDevUnitsToAppUnits(d2a);
  nsFont font(TOOL_BAR_FONT, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
	      NS_FONT_WEIGHT_NORMAL, 0,
	      nscoord(NSIntPointsToTwips(TOOL_BAR_FONT_SIZE) * t2d * d2a));
  NS_RELEASE(dc);

  // Create and place back button
  rv = nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID,
                                    (void**)&mBack);
  if (NS_OK != rv) {
    return rv;
  }
  nsRect r(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT);
	
	nsIWidget* widget = nsnull;

  NS_CreateButton(mWindow,mBack,r,HandleBackEvent,&font);
  if (NS_OK == mBack->QueryInterface(kIWidgetIID,(void**)&widget))
	{
    mBack->SetLabel("Back");
		NS_RELEASE(widget);
	}


  // Create and place forward button
  r.SetRect(BUTTON_WIDTH, 0, BUTTON_WIDTH, BUTTON_HEIGHT);  
  rv = nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID,
				    (void**)&mForward);
  if (NS_OK != rv) {
    return rv;
  }
	if (NS_OK == mForward->QueryInterface(kIWidgetIID,(void**)&widget))
	{
    widget->Create(mWindow, r, HandleForwardEvent, NULL);
    widget->SetFont(font);
    widget->Show(PR_TRUE);
    mForward->SetLabel("Forward");
		NS_RELEASE(widget);
	}


  // Create and place location bar
  r.SetRect(2*BUTTON_WIDTH, 0,
	    aWidth - 2*BUTTON_WIDTH - THROBBER_WIDTH,
	    BUTTON_HEIGHT);
  rv = nsComponentManager::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID,
				    (void**)&mLocation);
  if (NS_OK != rv) {
    return rv;
  }

  NS_CreateTextWidget(mWindow,mLocation,r,HandleLocationEvent,&font);
	if (NS_OK == mLocation->QueryInterface(kIWidgetIID,(void**)&widget))
  { 
    widget->SetForegroundColor(NS_RGB(0, 0, 0));
    widget->SetBackgroundColor(NS_RGB(255, 255, 255));
    PRUint32 size;
    mLocation->SetText("",size);
    nsIWidget* widget = nsnull;
	  if (NS_OK == mLocation->QueryInterface(kIWidgetIID,(void**)&widget))
	  {
      widget->EnableFileDrop(PR_TRUE);
		  NS_RELEASE(widget);
    }
  }

  // Create and place throbber
  r.SetRect(aWidth - THROBBER_WIDTH, 0,
	    THROBBER_WIDTH, THROBBER_HEIGHT);
  rv = nsComponentManager::CreateInstance(kThrobberCID, nsnull, kIThrobberIID,
				    (void**)&mThrobber);
  if (NS_OK != rv) {
    return rv;
  }
  nsString throbberURL(THROBBER_AT);
  mThrobber->Init(mWindow, r, throbberURL, THROB_NUM);
  mThrobber->Show();
  return NS_OK;
}

nsresult
nsBrowserWindow::CreateStatusBar(PRInt32 aWidth)
{
  nsresult rv;

  nsIDeviceContext* dc = mWindow->GetDeviceContext();
  float t2d;
  dc->GetTwipsToDevUnits(t2d);
  nsFont font(STATUS_BAR_FONT, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
	      NS_FONT_WEIGHT_NORMAL, 0,
	      nscoord(t2d * NSIntPointsToTwips(STATUS_BAR_FONT_SIZE)));
  NS_RELEASE(dc);

  nsRect r(0, 0, aWidth, THROBBER_HEIGHT);
  rv = nsComponentManager::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID,
				    (void**)&mStatus);
  if (NS_OK != rv) {
    return rv;
  }

  nsIWidget* widget = nsnull;
  NS_CreateTextWidget(mWindow,mStatus,r,HandleLocationEvent,&font);
	if (NS_OK == mStatus->QueryInterface(kIWidgetIID,(void**)&widget))
	{
    widget->SetForegroundColor(NS_RGB(0, 0, 0));
    PRUint32 size;
    mStatus->SetText("",size);

		nsITextWidget*	textWidget = nsnull;
		if (NS_OK == mStatus->QueryInterface(kITextWidgetIID,(void**)&textWidget))
		{
	    PRBool		wasReadOnly;
	    textWidget->SetReadOnly(PR_TRUE, wasReadOnly);
			NS_RELEASE(textWidget);
		}

    NS_RELEASE(widget);
  }

  return NS_OK;
}

void
nsBrowserWindow::Layout(PRInt32 aWidth, PRInt32 aHeight)
{
  nscoord txtHeight;
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
    lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TextFieldHeight, txtHeight);
    NS_RELEASE(lookAndFeel);
  } else {
    txtHeight = 24;
  }

  nsRect rr(0, 0, aWidth, aHeight);

  // position location bar (it's stretchy)
  if (NS_CHROME_TOOL_BAR_ON & mChromeMask) {
    nsIWidget* locationWidget = nsnull;
    if (mLocation &&
        NS_SUCCEEDED(mLocation->QueryInterface(kIWidgetIID,
                                               (void**)&locationWidget))) {
      if (mThrobber) {
	      locationWidget->Resize(2*BUTTON_WIDTH, 0,
                               aWidth - (2*BUTTON_WIDTH + THROBBER_WIDTH),
                               BUTTON_HEIGHT,
                               PR_TRUE);
	      mThrobber->MoveTo(aWidth - THROBBER_WIDTH, 0);
      }
      else {
	      locationWidget->Resize(2*BUTTON_WIDTH, 0,
                               aWidth - 2*BUTTON_WIDTH,
                               BUTTON_HEIGHT,
                               PR_TRUE);
      }

      locationWidget->Show(PR_TRUE);
      NS_RELEASE(locationWidget);
    }
  }
  else {
    nsIWidget* w = nsnull;
    if (mLocation &&
        NS_SUCCEEDED(mLocation->QueryInterface(kIWidgetIID, (void**)&w))) {
      w->Show(PR_FALSE);
      NS_RELEASE(w);
    }
    if (mBack &&
        NS_SUCCEEDED(mBack->QueryInterface(kIWidgetIID, (void**)&w))) {
      w->Show(PR_FALSE);
      NS_RELEASE(w);
    }
    if (mForward &&
        NS_SUCCEEDED(mForward->QueryInterface(kIWidgetIID, (void**)&w))) {
      w->Show(PR_FALSE);
      NS_RELEASE(w);
    }
    if (mThrobber) {
      mThrobber->Hide();
    }
  }
  
  nsIWidget* statusWidget = nsnull;

  if (mStatus && NS_OK == mStatus->QueryInterface(kIWidgetIID,(void**)&statusWidget)) {
    if (mChromeMask & NS_CHROME_STATUS_BAR_ON) {
      statusWidget->Resize(0, aHeight - txtHeight,
		      aWidth, txtHeight,
		      PR_TRUE);
      rr.height -= txtHeight;
      statusWidget->Show(PR_TRUE);
    }
    else {
      statusWidget->Show(PR_FALSE);
    }
    NS_RELEASE(statusWidget);
  }

  // inset the web widget

  if (NS_CHROME_TOOL_BAR_ON & mChromeMask) {
    rr.height -= BUTTON_HEIGHT;
    rr.y += BUTTON_HEIGHT;
  }

  rr.x += WEBSHELL_LEFT_INSET;
  rr.y += WEBSHELL_TOP_INSET;
  rr.width -= WEBSHELL_LEFT_INSET + WEBSHELL_RIGHT_INSET;
  rr.height -= WEBSHELL_TOP_INSET + WEBSHELL_BOTTOM_INSET;
  mWebShell->SetBounds(rr.x, rr.y, rr.width, rr.height);
}

NS_IMETHODIMP
nsBrowserWindow::MoveTo(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Move(aX, aY);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::SizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");

  // XXX We want to do this in one shot
  mWindow->Resize(aWidth, aHeight, PR_FALSE);
  Layout(aWidth, aHeight);

  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::GetBounds(nsRect& aBounds)
{
  mWindow->GetClientBounds(aBounds);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::GetWindowBounds(nsRect& aBounds)
{
  mWindow->GetBounds(aBounds);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::Show()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Show(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::Hide()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Show(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::Close()
{
  RemoveBrowser(this);

  if (nsnull != mWebShell) {
    mWebShell->Destroy();
    NS_RELEASE(mWebShell);
  }

  NS_IF_RELEASE(mBack);
  NS_IF_RELEASE(mForward);
  NS_IF_RELEASE(mLocation);
  NS_IF_RELEASE(mThrobber);
  NS_IF_RELEASE(mStatus);

//  NS_IF_RELEASE(mWindow);
  if (nsnull != mWindow) {
    nsIWidget* w = mWindow;
    NS_RELEASE(w);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsBrowserWindow::SetChrome(PRUint32 aChromeMask)
{
  mChromeMask = aChromeMask;
  nsRect r;
  mWindow->GetClientBounds(r);
  Layout(r.width, r.height);

  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::GetChrome(PRUint32& aChromeMaskResult)
{
  aChromeMaskResult = mChromeMask;
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::GetWebShell(nsIWebShell*& aResult)
{
  aResult = mWebShell;
  NS_IF_ADDREF(mWebShell);
  return NS_OK;
}

//----------------------------------------

NS_IMETHODIMP
nsBrowserWindow::SetTitle(const PRUnichar* aTitle)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mTitle = aTitle;
  nsAutoString newTitle(aTitle);
  //newTitle.Append(" - Raptor");
  newTitle.Append(*gTitleSuffix);
  mWindow->SetTitle(newTitle.GetUnicode());
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::GetTitle(const PRUnichar** aResult)
{
  *aResult = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::SetStatus(const PRUnichar* aStatus)
{
  if (nsnull != mStatus) {
    PRUint32 size;
    mStatus->SetText(aStatus,size);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::GetStatus(const PRUnichar** aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::SetProgress(PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason)
{
  if (mStatus) {
    nsAutoString url("Connecting to ");
    url.Append(aURL);
    PRUint32 size;
    mStatus->SetText(url,size);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
  if (mThrobber) {
    mThrobber->Start();
    PRUint32 size;
    mLocation->SetText(aURL,size);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::ProgressLoadURL(nsIWebShell* aShell,
                                 const PRUnichar* aURL,
                                 PRInt32 aProgress,
                                 PRInt32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::EndLoadURL(nsIWebShell* aShell,
                            const PRUnichar* aURL,
                            PRInt32 aStatus)
{
  if (mThrobber) {
    mThrobber->Stop();
  }
  if (nsnull != mStatus) {
    nsAutoString msg(aURL);
    PRUint32 size;

    msg.Append(" done.");

    mStatus->SetText(msg, size);
  }
  if (nsnull != mApp) {
    mApp->EndLoadURL(aShell, aURL, aStatus);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsBrowserWindow::NewWebShell(PRUint32 aChromeMask,
                             PRBool aVisible,
                             nsIWebShell*& aNewWebShell)
{
  nsresult rv = NS_OK;

  // Create new window. By default, the refcnt will be 1 because of
  // the registration of the browser window in gBrowsers.
  nsNativeBrowserWindow* browser;
  NS_NEWXPCOM(browser, nsNativeBrowserWindow);

  if (nsnull != browser)
  {
    nsRect  bounds;
    GetBounds(bounds);

    browser->SetApp(mApp);

    // Assume no controls for now
    rv = browser->Init(mAppShell, mPrefs, bounds, aChromeMask, mAllowPlugins);
    if (NS_OK == rv)
    {
      // Default is to startup hidden
      if (aVisible) {
        browser->Show();
      }
      nsIWebShell *shell;
      rv = browser->GetWebShell(shell);
      aNewWebShell = shell;
    }
    else
    {
      browser->Close();
    }
  }
  else
    rv = NS_ERROR_OUT_OF_MEMORY;

  return rv;
}

NS_IMETHODIMP
nsBrowserWindow::CanCreateNewWebShell(PRBool& aResult)
{
  aResult = PR_TRUE; // We can always make a new web shell synchronously
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::ChildShellAdded(nsIWebShell** aChildShell, nsIContent* frameNode)
{
  return NS_OK;
}


NS_IMETHODIMP
nsBrowserWindow::SetNewWebShellInfo(const nsString& aName, const nsString& anURL, 
                                nsIWebShell* aOpenerShell, PRUint32 aChromeMask,
                                nsIWebShell** aNewShell, nsIWebShell** anInnerShell)
{
  return NS_OK; // We don't care about this method, since we can make new web shells immediately.
}

NS_IMETHODIMP
nsBrowserWindow::FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult)
{
  PRInt32 i, n = gBrowsers.Count();

  aResult = nsnull;
  nsString aNameStr(aName);

  for (i = 0; i < n; i++) {
    nsBrowserWindow* bw = (nsBrowserWindow*) gBrowsers.ElementAt(i);
    nsIWebShell *ws;
    
    if (NS_OK == bw->GetWebShell(ws)) {
      const PRUnichar *name;
      if (NS_OK == ws->GetName(&name)) {
	if (aNameStr.Equals(name)) {
	  aResult = ws;
	  NS_ADDREF(aResult);
	  return NS_OK;
	}
      }      
    }
    if (NS_OK == ws->FindChildWithName(aName, aResult)) {
      if (nsnull != aResult) {
	return NS_OK;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken)
{
  return NS_OK;
}

//----------------------------------------

// Stream observer implementation

NS_IMETHODIMP
nsBrowserWindow::OnProgress(nsIURL* aURL,
                            PRUint32 aProgress,
                            PRUint32 aProgressMax)
{
  if (mStatus) {
    nsAutoString url;
    if (nsnull != aURL) {
      PRUnichar* str;
      aURL->ToString(&str);
      url = str;
      delete[] str;
    }
    url.Append(": progress ");
    url.Append(aProgress, 10);
    if (0 != aProgressMax) {
      url.Append(" (out of ");
      url.Append(aProgressMax, 10);
      url.Append(")");
    }
    PRUint32 size;
    mStatus->SetText(url,size);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
  if (mStatus) {
    PRUint32 size;
    mStatus->SetText(aMsg,size);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  if (mStatus) {
    nsAutoString url;
    if (nsnull != aURL) {
      PRUnichar* str;
      aURL->ToString(&str);
      url = str;
      delete[] str;
    }
    url.Append(": start");
    PRUint32 size;
    mStatus->SetText(url,size);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::OnStopBinding(nsIURL* aURL,
                               nsresult status,
                               const PRUnichar* aMsg)
{
  if (mStatus) {
    nsAutoString url;
    if (nsnull != aURL) {
      PRUnichar* str;
      aURL->ToString(&str);
      url = str;
      delete[] str;
    }
    url.Append(": stop");
    PRUint32 size;
    mStatus->SetText(url,size);
  }
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsBrowserWindow::Alert(const nsString &aText)
{
  char* msg = nsnull;

  msg = aText.ToNewCString();
  if (nsnull != msg) {
    printf("%cBrowser Window Alert: %s\n", '\007', msg);
    delete[] msg;
  }
}

NS_IMETHODIMP_(PRBool)
nsBrowserWindow::Confirm(const nsString &aText)
{
  char* msg= nsnull;

  msg = aText.ToNewCString();
  if (nsnull != msg) {
    printf("Browser Window Confirm: %s (y/n)? ", msg);
    delete[] msg;
    char c;
    for (;;) {
      c = getchar();
      if (tolower(c) == 'y') {
        return PR_TRUE;
      }
      if (tolower(c) == 'n') {
        return PR_FALSE;
      }
    }
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsBrowserWindow::Prompt(const nsString &aText,
			const nsString &aDefault,
			nsString &aResult)
{
  char* msg = nsnull;
  char buf[256];

  msg = aText.ToNewCString();
  if (nsnull != msg) {
    printf("Browser Window: %s\n", msg);
    delete[] msg;

    printf("%cPrompt: ", '\007');
    scanf("%s", buf);
    aResult = buf;
  }
  
  return (aResult.Length() > 0);
}

NS_IMETHODIMP_(PRBool) 
nsBrowserWindow::PromptUserAndPassword(const nsString &aText,
				       nsString &aUser,
				       nsString &aPassword)
{
  char* msg = nsnull;
  char buf[256];

  msg = aText.ToNewCString();
  if (nsnull != msg) {
    printf("Browser Window: %s\n", msg);
    delete[] msg;

    printf("%cUser: ", '\007');
    scanf("%s", buf);
    aUser = buf;

    printf("%cPassword: ", '\007');
    scanf("%s", buf);
    aPassword = buf;
  }
  return (aUser.Length() > 0);
}

NS_IMETHODIMP_(PRBool) 
nsBrowserWindow::PromptPassword(const nsString &aText,
				nsString &aPassword)
{
  char* msg = nsnull;
  char buf[256];

  msg = aText.ToNewCString();
  if (nsnull != msg) {
    printf("Browser Window: %s\n", msg);
    printf("%cPassword: ", '\007');
    scanf("%s", buf);
    aPassword = buf;
    delete[] msg;
  }
 
  return PR_TRUE;
}

//----------------------------------------

// Toolbar support

void
nsBrowserWindow::StartThrobber()
{
}

void
nsBrowserWindow::StopThrobber()
{
}

void
nsBrowserWindow::LoadThrobberImages()
{
}

void
nsBrowserWindow::DestroyThrobberImages()
{
}

nsIPresShell*
nsBrowserWindow::GetPresShell()
{
  return GetPresShellFor(mWebShell);
}


void
nsBrowserWindow::DoCopy()
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    shell->DoCopy();
    NS_RELEASE(shell);
  }
}

void
nsBrowserWindow::DoPaste()
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {

    printf("nsBrowserWindow::DoPaste()\n");

    NS_RELEASE(shell);
  }
}

//----------------------------------------------------------------------

void
nsBrowserWindow::ShowPrintPreview(PRInt32 aID)
{
  nsIContentViewer* cv = nsnull;
  if (nsnull != mWebShell) {
    if ((NS_OK == mWebShell->GetContentViewer(&cv)) && (nsnull != cv)) {
      nsIDocumentViewer* docv = nsnull;
      if (NS_OK == cv->QueryInterface(kIDocumentViewerIID, (void**)&docv)) {
	      nsIPresContext* printContext;
	      if (NS_OK == NS_NewPrintPreviewContext(&printContext)) {
      	  // Prepare new printContext for print-preview
      	  nsCOMPtr<nsIDeviceContext> dc;
      	  nsIPresContext* presContext;
      	  docv->GetPresContext(presContext);
      	  presContext->GetDeviceContext(getter_AddRefs(dc));
      	  printContext->Init(dc, mPrefs);
      	  NS_RELEASE(presContext);

      	  // Make a window using that content viewer
          // XXX Some piece of code needs to properly hold the reference to this
          // browser window. For the time being the reference is released by the
          // browser event handling code during processing of the NS_DESTROY event...
      	  nsBrowserWindow* bw = new nsNativeBrowserWindow;
          bw->SetApp(mApp);
      	  bw->Init(mAppShell, mPrefs, nsRect(0, 0, 600, 400),
    		           NS_CHROME_MENU_BAR_ON, PR_TRUE, docv, printContext);
	        bw->Show();

      	  NS_RELEASE(printContext);
      	}
      	NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
}

void nsBrowserWindow::DoPrint(void)
{
  nsIContentViewer *viewer = nsnull;

  mWebShell->GetContentViewer(&viewer);

  if (nsnull != viewer)
  {
    viewer->Print();
    NS_RELEASE(viewer);
  }
}

//---------------------------------------------------------------
void nsBrowserWindow::DoPrintSetup()
{
  if (mXPDialog) {
    NS_RELEASE(mXPDialog);
    //mXPDialog->SetVisible(PR_TRUE);
    //return;
  }

  nsString printHTML("resource:/res/samples/printsetup.html");
  nsRect rect(0, 0, 375, 510);
  nsString title("Print Setup");

  nsXPBaseWindow * dialog = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kXPBaseWindowCID, nsnull,
                                             kIXPBaseWindowIID,
                                             (void**) &dialog);
  if (rv == NS_OK) {
    dialog->Init(eXPBaseWindowType_dialog, mAppShell, nsnull, printHTML, title, rect, PRUint32(~0), PR_FALSE);
    dialog->SetVisible(PR_TRUE);
 	  if (NS_OK == dialog->QueryInterface(kIXPBaseWindowIID, (void**) &mXPDialog)) {
    }
  }

  mPrintSetupInfo.mPortrait         = PR_TRUE;
  mPrintSetupInfo.mBevelLines       = PR_TRUE;
  mPrintSetupInfo.mBlackText        = PR_FALSE;
  mPrintSetupInfo.mBlackLines       = PR_FALSE;
  mPrintSetupInfo.mLastPageFirst    = PR_FALSE;
  mPrintSetupInfo.mPrintBackgrounds = PR_FALSE;
  mPrintSetupInfo.mTopMargin        = 0.50;
  mPrintSetupInfo.mBottomMargin     = 0.50;
  mPrintSetupInfo.mLeftMargin       = 0.50;
  mPrintSetupInfo.mRightMargin      = 0.50;

  mPrintSetupInfo.mDocTitle         = PR_TRUE;
  mPrintSetupInfo.mDocLocation      = PR_TRUE;

  mPrintSetupInfo.mHeaderText       = "Header Text";
  mPrintSetupInfo.mFooterText       = "Footer Text";

  mPrintSetupInfo.mPageNum          = PR_TRUE;
  mPrintSetupInfo.mPageTotal        = PR_TRUE;
  mPrintSetupInfo.mDatePrinted      = PR_TRUE;


  nsPrintSetupDialog * printSetupDialog = new nsPrintSetupDialog(this);
  if (nsnull != printSetupDialog) {
    dialog->AddWindowListener(printSetupDialog);
  }
  printSetupDialog->SetSetupInfo(mPrintSetupInfo);
  //NS_IF_RELEASE(dialog);

}

//---------------------------------------------------------------
nsIDOMDocument* nsBrowserWindow::GetDOMDocument(nsIWebShell *aWebShell)
{
  nsIDOMDocument* domDoc = nsnull;
  if (nsnull != aWebShell) {
    nsIContentViewer* mCViewer;
    aWebShell->GetContentViewer(&mCViewer);
    if (nsnull != mCViewer) {
      nsIDocumentViewer* mDViewer;
      if (NS_OK == mCViewer->QueryInterface(kIDocumentViewerIID, (void**) &mDViewer)) {
	      nsIDocument* mDoc;
	      mDViewer->GetDocument(mDoc);
	      if (nsnull != mDoc) {
	        if (NS_OK == mDoc->QueryInterface(kIDOMDocumentIID, (void**) &domDoc)) {
	        }
	        NS_RELEASE(mDoc);
	      }
	      NS_RELEASE(mDViewer);
      }
      NS_RELEASE(mCViewer);
    }
  }
  return domDoc;
}

//---------------------------------------------------------------
void nsBrowserWindow::DoTableInspector()
{
  if (mTableInspectorDialog) {
    mTableInspectorDialog->SetVisible(PR_TRUE);
    return;
  }
  nsIDOMDocument* domDoc = GetDOMDocument(mWebShell);

  if (nsnull != domDoc) {
    nsString printHTML("resource:/res/samples/printsetup.html");
    nsRect rect(0, 0, 375, 510);
    nsString title("Table Inspector");

    nsXPBaseWindow * xpWin = nsnull;
    nsresult rv = nsComponentManager::CreateInstance(kXPBaseWindowCID, nsnull,
                                               kIXPBaseWindowIID,
                                               (void**) &xpWin);
    if (rv == NS_OK) {
      xpWin->Init(eXPBaseWindowType_dialog, mAppShell, nsnull, printHTML, title, rect, PRUint32(~0), PR_FALSE);
      xpWin->SetVisible(PR_TRUE);
 	    if (NS_OK == xpWin->QueryInterface(kIXPBaseWindowIID, (void**) &mTableInspectorDialog)) {
        mTableInspector = new nsTableInspectorDialog(this, domDoc); // ref counts domDoc
        if (nsnull != mTableInspector) {
          xpWin->AddWindowListener(mTableInspector); 
        }
      }
      NS_RELEASE(xpWin);
    }
    NS_RELEASE(domDoc);
  }

}
void nsBrowserWindow::DoImageInspector()
{
  if (mImageInspectorDialog) {
    mImageInspectorDialog->SetVisible(PR_TRUE);
    return;
  }

  nsIDOMDocument* domDoc = GetDOMDocument(mWebShell);

  if (nsnull != domDoc) {
    nsString printHTML("resource:/res/samples/image_props.html");
    nsRect rect(0, 0, 485, 124);
    nsString title("Image Inspector");

    nsXPBaseWindow * xpWin = nsnull;
    nsresult rv = nsComponentManager::CreateInstance(kXPBaseWindowCID, nsnull, kIXPBaseWindowIID, (void**) &xpWin);
    if (rv == NS_OK) {
      xpWin->Init(eXPBaseWindowType_dialog, mAppShell, nsnull, printHTML, title, rect, PRUint32(~0), PR_FALSE);
      xpWin->SetVisible(PR_TRUE);
 	    if (NS_OK == xpWin->QueryInterface(kIXPBaseWindowIID, (void**) &mImageInspectorDialog)) {
        mImageInspector = new nsImageInspectorDialog(this, domDoc); // ref counts domDoc
        if (nsnull != mImageInspector) {
          xpWin->AddWindowListener(mImageInspector); 
        }
      }
      NS_RELEASE(xpWin);
    }
    NS_RELEASE(domDoc);
  }

}

//----------------------------------------------------------------------

void
nsBrowserWindow::DoJSConsole()
{
  mApp->CreateJSConsole(this);
}

void
nsBrowserWindow::DoPrefs()
{
  mApp->DoPrefs(this);
}

void
nsBrowserWindow::DoEditorMode(nsIWebShell *aWebShell)
{
  PRInt32 i, n;
  if (nsnull != aWebShell) {
    nsIContentViewer* mCViewer;
    aWebShell->GetContentViewer(&mCViewer);
    if (nsnull != mCViewer) {
      nsIDocumentViewer* mDViewer;
      if (NS_OK == mCViewer->QueryInterface(kIDocumentViewerIID, (void**) &mDViewer)) 
      {
	      nsIDocument* mDoc;
	      mDViewer->GetDocument(mDoc);
	      if (nsnull != mDoc) {
	        nsIDOMDocument* mDOMDoc;
	        if (NS_OK == mDoc->QueryInterface(kIDOMDocumentIID, (void**) &mDOMDoc)) 
          {
            nsIPresShell* shell = GetPresShellFor(aWebShell);
	          NS_InitEditorMode(mDOMDoc, shell);
	          NS_RELEASE(mDOMDoc);
            NS_IF_RELEASE(shell);
	        }
	        NS_RELEASE(mDoc);
	      }
	      NS_RELEASE(mDViewer);
      }
      NS_RELEASE(mCViewer);
    }
    
    aWebShell->GetChildCount(n);
    for (i = 0; i < n; i++) {
      nsIWebShell* mChild;
      aWebShell->ChildAt(i, mChild);
      DoEditorMode(mChild);
      NS_RELEASE(mChild);
    }
  }
}

// Same as above, but calls NS_DoEditorTest instead of starting an editor
void
nsBrowserWindow::DoEditorTest(nsIWebShell *aWebShell, PRInt32 aCommandID)
{
  if (nsnull != aWebShell) {
    nsIContentViewer* mCViewer;
    aWebShell->GetContentViewer(&mCViewer);
    if (nsnull != mCViewer) {
      nsIDocumentViewer* mDViewer;
      if (NS_OK == mCViewer->QueryInterface(kIDocumentViewerIID, (void**) &mDViewer)) 
      {
	      nsIDocument* mDoc;
	      mDViewer->GetDocument(mDoc);
	      if (nsnull != mDoc) {
	        nsIDOMDocument* mDOMDoc;
	        if (NS_OK == mDoc->QueryInterface(kIDOMDocumentIID, (void**) &mDOMDoc)) 
          {
            NS_DoEditorTest(aCommandID);
	          NS_RELEASE(mDOMDoc);
	        }
	        NS_RELEASE(mDoc);
	      }
	      NS_RELEASE(mDViewer);
      }
      NS_RELEASE(mCViewer);
    }
    // Do we need to do all the children as in DoEditorMode?
    // Its seems to work if we don't do that
  }
}




#ifdef NS_DEBUG
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIStyleContext.h"
#include "nsIStyleSet.h"


static void DumpAWebShell(nsIWebShell* aShell, FILE* out, PRInt32 aIndent)
{
  const PRUnichar *name;
  nsAutoString str;
  nsIWebShell* parent;
  PRInt32 i, n;

  for (i = aIndent; --i >= 0; ) fprintf(out, "  ");

  fprintf(out, "%p '", aShell);
  aShell->GetName(&name);
  aShell->GetParent(parent);
  str = name;
  fputs(str, out);
  fprintf(out, "' parent=%p <\n", parent);
  NS_IF_RELEASE(parent);

  aIndent++;
  aShell->GetChildCount(n);
  for (i = 0; i < n; i++) {
    nsIWebShell* child;
    aShell->ChildAt(i, child);
    if (nsnull != child) {
      DumpAWebShell(child, out, aIndent);
    }
  }
  aIndent--;
  for (i = aIndent; --i >= 0; ) fprintf(out, "  ");
  fputs(">\n", out);
}

void
nsBrowserWindow::DumpWebShells(FILE* out)
{
  DumpAWebShell(mWebShell, out, 0);
}

static
void 
DumpMultipleWebShells(nsBrowserWindow& aBrowserWindow, nsIWebShell* aWebShell, FILE* aOut)
{ 
  PRInt32 count;
  if (aWebShell) {
    aWebShell->GetChildCount(count);
    if (count > 0) {
      fprintf(aOut, "webshells= \n");
      aBrowserWindow.DumpWebShells(aOut);
    }
  }
}

static
void
DumpContentRecurse(nsIWebShell* aWebShell, FILE* out)
{
  if (nsnull != aWebShell) {
    fprintf(out, "webshell=%p \n", aWebShell);
    nsIPresShell* shell = GetPresShellFor(aWebShell);
    if (nsnull != shell) {
      nsCOMPtr<nsIDocument> doc;
      shell->GetDocument(getter_AddRefs(doc));
      if (doc) {
        nsIContent* root = doc->GetRootContent();
        if (nsnull != root) {
	        root->List(out);
	        NS_RELEASE(root);
        }
      }
      NS_RELEASE(shell);
    }
    else {
      fputs("null pres shell\n", out);
    }
    // dump the frames of the sub documents
    PRInt32 i, n;
    aWebShell->GetChildCount(n);
    for (i = 0; i < n; i++) {
      nsIWebShell* child;
      aWebShell->ChildAt(i, child);
      if (nsnull != child) {
	      DumpContentRecurse(child, out);
      }
    }
  }
}

void
nsBrowserWindow::DumpContent(FILE* out)
{
  DumpContentRecurse(mWebShell, out);
  DumpMultipleWebShells(*this, mWebShell, out);
}

static
void
DumpFramesRecurse(nsIWebShell* aWebShell, FILE* out, nsString *aFilterName)
{
  if (nsnull != aWebShell) {
    fprintf(out, "webshell=%p \n", aWebShell);
    nsIPresShell* shell = GetPresShellFor(aWebShell);
    if (nsnull != shell) {
      nsIFrame* root;
      shell->GetRootFrame(&root);
      if (nsnull != root) {
        root->List(out, 0);
      }
      NS_RELEASE(shell);
    }
    else {
      fputs("null pres shell\n", out);
    }
    // dump the frames of the sub documents
    PRInt32 i, n;
    aWebShell->GetChildCount(n);
    for (i = 0; i < n; i++) {
      nsIWebShell* child;
      aWebShell->ChildAt(i, child);
      if (nsnull != child) {
	      DumpFramesRecurse(child, out, aFilterName);
      }
    }
  }
}

void
nsBrowserWindow::DumpFrames(FILE* out, nsString *aFilterName)
{
  DumpFramesRecurse(mWebShell, out, aFilterName);
  DumpMultipleWebShells(*this, mWebShell, out);
}

static
void
DumpViewsRecurse(nsIWebShell* aWebShell, FILE* out)
{
  if (nsnull != aWebShell) {
    fprintf(out, "webshell=%p \n", aWebShell);
    nsIPresShell* shell = GetPresShellFor(aWebShell);
    if (nsnull != shell) {
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
	      nsIView* root;
	      vm->GetRootView(root);
	      if (nsnull != root) {
	        root->List(out);
	      }
      }
      NS_RELEASE(shell);
    }
    else {
      fputs("null pres shell\n", out);
    }
    // dump the views of the sub documents
    PRInt32 i, n;
    aWebShell->GetChildCount(n);
    for (i = 0; i < n; i++) {
      nsIWebShell* child;
      aWebShell->ChildAt(i, child);
      if (nsnull != child) {
	      DumpViewsRecurse(child, out);
      }
    }
  }
}

void
nsBrowserWindow::DumpViews(FILE* out)
{
  DumpViewsRecurse(mWebShell, out);
  DumpMultipleWebShells(*this, mWebShell, out);
}

void
nsBrowserWindow::DumpStyleSheets(FILE* out)
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsCOMPtr<nsIStyleSet> styleSet;
    shell->GetStyleSet(getter_AddRefs(styleSet));
    if (!styleSet) {
      fputs("null style set\n", out);
    } else {
      styleSet->List(out);
    }
    NS_RELEASE(shell);
  }
  else {
    fputs("null pres shell\n", out);
  }
}

void
nsBrowserWindow::DumpStyleContexts(FILE* out)
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsCOMPtr<nsIStyleSet> styleSet;
    shell->GetStyleSet(getter_AddRefs(styleSet));
    if (!styleSet) {
      fputs("null style set\n", out);
    } else {
      nsIFrame* root;
      shell->GetRootFrame(&root);
      if (nsnull == root) {
	      fputs("null root frame\n", out);
            } else {
	      nsIStyleContext* rootContext;
	      root->GetStyleContext(&rootContext);
	      if (nsnull != rootContext) {
	        styleSet->ListContexts(rootContext, out);
	        NS_RELEASE(rootContext);
	      }
	      else {
	        fputs("null root context", out);
	      }
      }
    }
    NS_RELEASE(shell);
  } else {
    fputs("null pres shell\n", out);
  }
}

void
nsBrowserWindow::ToggleFrameBorders()
{
  PRBool showing = nsIFrame::GetShowFrameBorders();
  nsIFrame::ShowFrameBorders(!showing);
  ForceRefresh();
}

void
nsBrowserWindow::ShowContentSize()
{
}

void
nsBrowserWindow::ShowFrameSize()
{
}

void
nsBrowserWindow::ShowStyleSize()
{
}




static PRBool GetSaveFileNameFromFileSelector(nsIWidget* aParentWindow,
					      nsString&  aFileName)
{
  PRInt32 offset = aFileName.RFind('/');
  if (offset != -1)
    aFileName.Cut(0,offset+1);

  PRBool selectedFileName = PR_FALSE;
  nsIFileWidget *fileWidget;
  nsString title("Save HTML");
  nsresult rv = nsComponentManager::CreateInstance(kFileWidgetCID,
					     nsnull,
					     kIFileWidgetIID,
					     (void**)&fileWidget);
  if (NS_OK == rv) {
    nsString titles[] = {"html","txt"};
    nsString filters[] = {"*.html", "*.txt"};
    fileWidget->SetFilterList(2, titles, filters);
    fileWidget->Create(aParentWindow,
		       title,
		       eMode_save,
		       nsnull,
		       nsnull);
    fileWidget->SetDefaultString(aFileName);

    PRUint32 result = fileWidget->Show();
    if (result) {
      fileWidget->GetFile(aFileName);
      selectedFileName = PR_TRUE;
    }
 
    NS_RELEASE(fileWidget);
  }

  return selectedFileName;
}


void
nsBrowserWindow::DoDebugSave()
{
  PRBool    doSave = PR_FALSE;
  nsString  path;

  const PRUnichar *urlString;
  mWebShell->GetURL(0,&urlString);
  nsIURL* url;
  nsresult rv = NS_NewURL(&url, urlString);
  
  if (rv == NS_OK)
  {
    const char* name;
    rv = url->GetFile(&name);
    path = name;

    doSave = GetSaveFileNameFromFileSelector(mWindow, path);
    NS_RELEASE(url);

  }
  if (!doSave)
    return;


  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      nsString buffer;

      doc->CreateXIF(buffer);

      nsIParser* parser;

      static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
      static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

      nsresult rv = nsComponentManager::CreateInstance(kCParserCID, 
                                                 nsnull, 
                                                 kCParserIID, 
                                                 (void **)&parser);

      if (NS_OK == rv) {
        nsIHTMLContentSink* sink = nsnull;

        rv = NS_New_HTML_ContentSinkStream(&sink);
	
#ifdef WIN32
#define   BUFFER_SIZE MAX_PATH
#else
#define   BUFFER_SIZE 1024
#endif
        char filename[BUFFER_SIZE];
        path.ToCString(filename,BUFFER_SIZE);
        ofstream    out(filename);
        ((nsHTMLContentSinkStream*)sink)->SetOutputStream(out);

        if (NS_OK == rv) {
          parser->SetContentSink(sink);
	  
          nsIDTD* dtd = nsnull;
          rv = NS_NewXIFDTD(&dtd);
          if (NS_OK == rv) 
          {
            parser->RegisterDTD(dtd);
            //dtd->SetContentSink(sink);
            //dtd->SetParser(parser);
            parser->Parse(buffer, 0, "text/xif",PR_FALSE,PR_TRUE);           
          }
          out.close();

          NS_IF_RELEASE(dtd);
          NS_IF_RELEASE(sink);
        }
        NS_RELEASE(parser);
      }
    }
    NS_RELEASE(shell);
  }
}

void 
nsBrowserWindow::DoToggleSelection()
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      PRBool  current = doc->GetDisplaySelection();
      doc->SetDisplaySelection(!current);
      ForceRefresh();
    }
    NS_RELEASE(shell);
  }
}

void
nsBrowserWindow::DoDebugRobot()
{
  mApp->CreateRobot(this);
}

void
nsBrowserWindow::DoSiteWalker()
{
  mApp->CreateSiteWalker(this);
}

nsEventStatus
nsBrowserWindow::DispatchDebugMenu(PRInt32 aID)
{
  nsEventStatus result = nsEventStatus_eIgnore;

  switch(aID) {
  case VIEWER_VISUAL_DEBUGGING:
    ToggleFrameBorders();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DUMP_CONTENT:
    DumpContent();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DUMP_FRAMES:
    DumpFrames();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DUMP_VIEWS:
    DumpViews();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DUMP_STYLE_SHEETS:
    DumpStyleSheets();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DUMP_STYLE_CONTEXTS:
    DumpStyleContexts();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_SHOW_CONTENT_SIZE:
    ShowContentSize();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_SHOW_FRAME_SIZE:
    ShowFrameSize();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_SHOW_STYLE_SIZE:
    ShowStyleSize();
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_SHOW_CONTENT_QUALITY:
    if (nsnull != mWebShell) {
      nsIPresShell   *ps = GetPresShellFor(mWebShell);
      nsIViewManager *vm = nsnull;
      PRBool         qual;

      if (ps) {
        ps->GetViewManager(&vm);

        if (vm) {
          vm->GetShowQuality(qual);
          vm->ShowQuality(!qual);

          NS_RELEASE(vm);
        }

        NS_RELEASE(ps);
      }
    }

    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_DEBUGSAVE:
    DoDebugSave();
    break;

  case VIEWER_TOGGLE_SELECTION:
    DoToggleSelection();
    break;


  case VIEWER_DEBUGROBOT:
    DoDebugRobot();
    break;

  case VIEWER_TOP100:
    DoSiteWalker();
    break;
  }
  return(result);
}

#endif // NS_DEBUG

void 
nsBrowserWindow::SetCompatibilityMode(PRBool aIsStandard)
{
  if (nsnull != mPrefs) { 
    int32 prefInt = (aIsStandard) ? eCompatibility_Standard : eCompatibility_NavQuirks;
    mPrefs->SetIntPref("nglayout.compatibility.mode", prefInt);
    mPrefs->SavePrefFile();
  }
}

void 
nsBrowserWindow::SetWidgetRenderingMode(PRBool aIsNative)
{
  if (nsnull != mPrefs) { 
    int32 prefInt = (aIsNative) ? eWidgetRendering_Native : eWidgetRendering_Gfx;
    mPrefs->SetIntPref("nglayout.widget.mode", prefInt);
    mPrefs->SavePrefFile();
  }
}

nsEventStatus
nsBrowserWindow::DispatchStyleMenu(PRInt32 aID)
{
  nsEventStatus result = nsEventStatus_eIgnore;

  switch(aID) {
  case VIEWER_SELECT_STYLE_LIST:
    {
      nsIPresShell* shell = GetPresShell();
      if (nsnull != shell) {
        nsAutoString  defaultStyle;
        nsCOMPtr<nsIDocument> doc;
        shell->GetDocument(getter_AddRefs(doc));
        if (doc) {
          nsIAtom* defStyleAtom = NS_NewAtom("default-style");
          doc->GetHeaderData(defStyleAtom, defaultStyle);
          NS_RELEASE(defStyleAtom);
        }

        nsStringArray titles;
        shell->ListAlternateStyleSheets(titles);
        nsAutoString  current;
        shell->GetActiveAlternateStyleSheet(current);

        PRInt32 count = titles.Count();
        fprintf(stdout, "There %s %d alternate style sheet%s\n",  
                ((1 == count) ? "is" : "are"),
                count,
                ((1 == count) ? "" : "s"));
        PRInt32 index;
        for (index = 0; index < count; index++) {
          fprintf(stdout, "%d: \"", index + 1);
          nsAutoString title;
          titles.StringAt(index, title);
          fputs(title, stdout);
          fprintf(stdout, "\" %s%s\n", 
                  ((title.EqualsIgnoreCase(current)) ? "<< current " : ""), 
                  ((title.EqualsIgnoreCase(defaultStyle)) ? "** default" : ""));
        }
        NS_RELEASE(shell);
      }
    }
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_SELECT_STYLE_DEFAULT:
    {
      nsIPresShell* shell = GetPresShell();
      if (nsnull != shell) {
        nsCOMPtr<nsIDocument> doc;
        shell->GetDocument(getter_AddRefs(doc));
        if (doc) {
          nsAutoString  defaultStyle;
          nsIAtom* defStyleAtom = NS_NewAtom("default-style");
          doc->GetHeaderData(defStyleAtom, defaultStyle);
          NS_RELEASE(defStyleAtom);
          fputs("Selecting default style sheet \"", stdout);
          fputs(defaultStyle, stdout);
          fputs("\"\n", stdout);
          shell->SelectAlternateStyleSheet(defaultStyle);
        }
        NS_RELEASE(shell);
      }
    }
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_SELECT_STYLE_ONE:
  case VIEWER_SELECT_STYLE_TWO:
  case VIEWER_SELECT_STYLE_THREE:
  case VIEWER_SELECT_STYLE_FOUR:
    {
      nsIPresShell* shell = GetPresShell();
      if (nsnull != shell) {
        nsStringArray titles;
        shell->ListAlternateStyleSheets(titles);
        nsAutoString  title;
        titles.StringAt(aID - VIEWER_SELECT_STYLE_ONE, title);
        fputs("Selecting alternate style sheet \"", stdout);
        fputs(title, stdout);
        fputs("\"\n", stdout);
        shell->SelectAlternateStyleSheet(title);
        NS_RELEASE(shell);
      }
    }
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_NAV_QUIRKS_MODE:
  case VIEWER_STANDARD_MODE:
    SetCompatibilityMode(VIEWER_STANDARD_MODE == aID);
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_NATIVE_WIDGET_MODE:
  case VIEWER_GFX_WIDGET_MODE:
    SetWidgetRenderingMode(VIEWER_NATIVE_WIDGET_MODE == aID);
    result = nsEventStatus_eConsumeNoDefault;
    break;

  }
  return result;
}


//----------------------------------------------------------------------

// Factory code for creating nsBrowserWindow's

class nsBrowserWindowFactory : public nsIFactory
{
public:
  nsBrowserWindowFactory();
  virtual ~nsBrowserWindowFactory();

  // nsISupports methods
  NS_IMETHOD QueryInterface(const nsIID &aIID, void **aResult);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
			    const nsIID &aIID,
			    void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

private:
  nsrefcnt  mRefCnt;
};

nsBrowserWindowFactory::nsBrowserWindowFactory()
{
  mRefCnt = 0;
}

nsBrowserWindowFactory::~nsBrowserWindowFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

nsresult
nsBrowserWindowFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aResult = NULL;

  if (aIID.Equals(kISupportsIID)) {
    *aResult = (void *)(nsISupports*)this;
  } else if (aIID.Equals(kIFactoryIID)) {
    *aResult = (void *)(nsIFactory*)this;
  }

  if (*aResult == NULL) {
    return NS_NOINTERFACE;
  }

  NS_ADDREF_THIS(); // Increase reference count for caller
  return NS_OK;
}

nsrefcnt
nsBrowserWindowFactory::AddRef()
{
  return ++mRefCnt;
}

nsrefcnt
nsBrowserWindowFactory::Release()
{
  if (--mRefCnt == 0) {
    delete this;
    return 0; // Don't access mRefCnt after deleting!
  }
  return mRefCnt;
}

nsresult
nsBrowserWindowFactory::CreateInstance(nsISupports *aOuter,
				       const nsIID &aIID,
				       void **aResult)
{
  nsresult rv;
  nsBrowserWindow *inst;

  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  if (nsnull != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  NS_NEWXPCOM(inst, nsNativeBrowserWindow);
  if (inst == NULL) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

done:
  return rv;
}

nsresult
nsBrowserWindowFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}

nsresult NS_NewBrowserWindowFactory(nsIFactory** aFactory);
nsresult
NS_NewBrowserWindowFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;
  nsBrowserWindowFactory* inst;
  NS_NEWXPCOM(inst, nsBrowserWindowFactory);
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}
