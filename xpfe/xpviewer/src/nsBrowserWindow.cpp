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
#ifdef XP_MAC
#include "nsBrowserWindow.h"
#define NS_IMPL_IDS
#else
#define NS_IMPL_IDS
#include "nsBrowserWindow.h"
#endif
#include "prmem.h"

#include "nsIStreamListener.h"
#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsIButton.h"
#include "nsILabel.h"
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

#include "nsIXPBaseWindow.h"
#include "nsXPBaseWindow.h"
#include "nsFindDialog.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIDocumentViewer.h"
#include "nsIContentViewer.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsXIFDTD.h"
#include "nsIParser.h"
#include "nsHTMLContentSinkStream.h"
#include "nsGUIEvent.h"


// Needed for "Find" GUI
#include "nsIDialog.h"
#include "nsICheckButton.h"
#include "nsIRadioButton.h"
#include "nsILabel.h"
#include "nsIMenuBar.h"

// Menus
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsWidgetSupport.h"
#include "nsIPopUpMenu.h"
#include "nsIMenuButton.h"

// ImageButtons
#include "nsIImageButton.h"
#include "nsIToolbar.h"
#include "nsIToolbarManager.h"
#include "nsIToolbarItem.h"
#include "nsIToolbarItemHolder.h"

#include "resources.h"

#if defined(WIN32)
#include <strstrea.h>
#else
#include <strstream.h>
#include <ctype.h>
#endif

#if defined(WIN32)
#include <windows.h>
#endif

#include "nsIEditor.h"
#include "nsEditorCID.h"

// XXX For font setting below
#include "nsFont.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"

// XXX greasy constants
//#define THROBBER_WIDTH 32
//#define THROBBER_HEIGHT 32
#define THROBBER_WIDTH 42
#define THROBBER_HEIGHT 42

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
static NS_DEFINE_IID(kMenuBarCID, NS_MENUBAR_CID);
static NS_DEFINE_IID(kMenuCID, NS_MENU_CID);
static NS_DEFINE_IID(kMenuItemCID, NS_MENUITEM_CID);
static NS_DEFINE_IID(kImageButtonCID, NS_IMAGEBUTTON_CID);
static NS_DEFINE_IID(kToolbarCID, NS_TOOLBAR_CID);
static NS_DEFINE_IID(kToolbarManagerCID, NS_TOOLBARMANAGER_CID);
static NS_DEFINE_IID(kToolbarItemHolderCID, NS_TOOLBARITEMHOLDER_CID);
static NS_DEFINE_IID(kToolbarItemCID, NS_TOOLBARITEM_CID);
static NS_DEFINE_IID(kPopUpMenuCID, NS_POPUPMENU_CID);
static NS_DEFINE_IID(kMenuButtonCID, NS_MENUBUTTON_CID);
static NS_DEFINE_IID(kXPBaseWindowCID, NS_XPBASE_WINDOW_CID);

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
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIDialogIID, NS_IDIALOG_IID);
static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kIRadioButtonIID, NS_IRADIOBUTTON_IID);
static NS_DEFINE_IID(kILabelIID, NS_ILABEL_IID);
static NS_DEFINE_IID(kIMenuBarIID, NS_IMENUBAR_IID);
static NS_DEFINE_IID(kIMenuIID, NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);
static NS_DEFINE_IID(kIImageButtonIID, NS_IIMAGEBUTTON_IID);
static NS_DEFINE_IID(kIToolbarIID, NS_ITOOLBAR_IID);
static NS_DEFINE_IID(kIToolbarManagerIID, NS_ITOOLBARMANAGER_IID);
static NS_DEFINE_IID(kIToolbarItemHolderIID, NS_ITOOLBARITEMHOLDER_IID);
static NS_DEFINE_IID(kIToolbarItemIID, NS_ITOOLBARITEM_IID);
static NS_DEFINE_IID(kIPopUpMenuIID, NS_IPOPUPMENU_IID);
static NS_DEFINE_IID(kIMenuButtonIID, NS_IMENUBUTTON_IID);
static NS_DEFINE_IID(kIXPBaseWindowIID, NS_IXPBASE_WINDOW_IID);
static NS_DEFINE_IID(kINetSupportIID, NS_INETSUPPORT_IID);
static NS_DEFINE_IID(kIEditFactoryIID, NS_IEDITORFACTORY_IID);
static NS_DEFINE_IID(kIEditorIID, NS_IEDITOR_IID);
static NS_DEFINE_IID(kEditorCID, NS_EDITOR_CID);

static const char* gsAOLFormat = "AOLMAIL";
static const char* gsHTMLFormat = "text/html";

static PRInt32 gBtnWidth  = 54;
static PRInt32 gBtnHeight = 42;

//----------------------------------------------------------------------
nsVoidArray nsBrowserWindow::gBrowsers;

/////////////////////////////////////////////////////////////
// This part is temporary until we get the XML/HTML code working
/////////////////////////////////////////////////////////////

typedef struct {
  PRInt32 mGap;
  PRInt32 mImgWidth;
  PRInt32 mImgHeight;
  PRBool  mEnabled;
  PRInt32 mCommand;
  char *  mLabel;
  char *  mRollOverDesc;
  char *  mUpName;
  char *  mPressedName;
  char *  mDisabledName;
  char *  mRolloverName;
} ButtonCreateInfo;

//-----------------------------------------------------------------
// XXX These constants will go away once we are hooked up to JavaScript
const PRInt32 gBackBtnInx    = 0;
const PRInt32 gForwardBtnInx   = 1;
const PRInt32 gReloadBtnInx  = 2;
const PRInt32 gHomeBtnInx    = 3;
const PRInt32 gSearchBtnInx  = 4;
const PRInt32 gNetscapeBtnInx  = 5;
const PRInt32 gPrintBtnInx   = 6;
const PRInt32 gSecureBtnInx  = 7;
const PRInt32 gStopBtnInx    = 8;

const PRInt32 kBackCmd    = 3000;
const PRInt32 kForwardCmd   = 3001;
const PRInt32 kReloadCmd  = 3002;
const PRInt32 kHomeCmd    = 3003;
const PRInt32 kSearchCmd  = 3004;
const PRInt32 kNetscapeCmd  = 3005;
const PRInt32 kPrintCmd   = 3006;
const PRInt32 kSecureCmd  = 3007;
const PRInt32 kStopCmd    = 3008;

const PRInt32 kMiniTabCmd    = 3009;
const PRInt32 kMiniNavCmd    = 3010;
const PRInt32 kBookmarksCmd  = 3011;
const PRInt32 kWhatsRelatedCmd = 3012;

const PRInt32 kPersonalCmd  = 4000;

//-------------------------------------------
// These static toolbar definitions will go away once we have XML/HTML
ButtonCreateInfo gBtnToolbarInfo[] = {
{10, 23, 21, PR_FALSE, kBackCmd,   "Back",  "Return to previous document in history list", "TB_Back.gif",  "TB_Back.gif",  "TB_Back_dis.gif",   "TB_Back_mo.gif"},
{2,  23, 21, PR_FALSE, kForwardCmd,  "Forward", "Move forward to next document in history list", "TB_Forward.gif", "TB_Forward.gif", "TB_Forward_dis.gif",  "TB_Forward_mo.gif"},
{2,  23, 21, PR_TRUE,  kReloadCmd,   "Reload",  "Reload the current page",       "TB_Reload.gif",  "TB_Reload.gif",  "TB_Reload.gif",     "TB_Reload_mo.gif"},
{2,  23, 21, PR_TRUE,  kHomeCmd,   "Home",  "Go to the Home page",         "TB_Home.gif",  "TB_Home.gif",  "TB_Home.gif",     "TB_Home_mo.gif"},
{2,  23, 21, PR_FALSE, kSearchCmd,   "Search",  "Search the internet for information", "TB_Search.gif",  "TB_Search.gif",  "TB_Search.gif",     "TB_Search_mo.gif"},
{2,  23, 21, PR_FALSE, kNetscapeCmd, "Netscape","Go to your personal start page",    "TB_Netscape.gif","TB_Netscape.gif","TB_Netscape.gif",   "TB_Netscape_mo.gif"},
{2,  23, 21, PR_FALSE, kPrintCmd,  "Print",   "Print this page",           "TB_Print.gif",   "TB_Print.gif",   "TB_Print.gif",    "TB_Print_mo.gif"},
{2,  23, 21, PR_FALSE, kSecureCmd,   "Security","Show security information",       "TB_Secure.gif",   "TB_Secure.gif", "TB_Secure.gif",     "TB_Secure_mo.gif"},
{2,  23, 21, PR_FALSE, kStopCmd,   "Stop",  "Stop the current transfer",       "TB_Stop.gif",  "TB_Stop.gif",  "TB_Stop_dis.gif",   "TB_Stop_mo.gif"},
{0,   0,  0, PR_FALSE, 0, NULL, NULL, NULL, NULL, NULL}
};

ButtonCreateInfo gMiniAppsToolbarInfo[] = {
{0, 11, 14, PR_TRUE, kMiniTabCmd, "", "Expand toolbar", "TB_MiniTab.gif",  "TB_MiniTab.gif",  "TB_MiniTab.gif",  "TB_MiniTab.gif"},
{1, 30, 14, PR_TRUE, kMiniNavCmd, "", "Start a Navigator session", "TB_MiniNav.gif",  "TB_MiniNav.gif",  "TB_MiniNav.gif",  "TB_MiniNav.gif"},
{1, 30, 14, PR_TRUE, kMiniNavCmd, "", "Start a Mail session", "TB_MiniMail.gif", "TB_MiniMail.gif", "TB_MiniMail.gif", "TB_MiniMail.gif"},
{1, 30, 14, PR_TRUE, kMiniNavCmd, "", "Start a Address Book session", "TB_MiniAddr.gif", "TB_MiniAddr.gif", "TB_MiniAddr.gif", "TB_MiniAddr.gif"},
{1, 30, 14, PR_TRUE, kMiniNavCmd, "", "Start a Composer session", "TB_MiniComp.gif", "TB_MiniComp.gif", "TB_MiniComp.gif", "TB_MiniComp.gif"},
{0,  0,  0, PR_FALSE, 0, NULL, NULL, NULL, NULL, NULL}
};

ButtonCreateInfo gMiniAppsDialogInfo[] = {
{2, 36, 22, PR_TRUE, kMiniNavCmd, "", "Start a Navigator session", "DialogNavIcon.gif",  "DialogNavIcon.gif",  "DialogNavIcon.gif",  "DialogNavIcon_mo.gif"},
{2, 36, 22, PR_TRUE, kMiniNavCmd, "", "Start a Mail session", "DialogMailIcon.gif", "DialogMailIcon.gif", "DialogMailIcon.gif", "DialogMailIcon_mo.gif"},
{2, 36, 22, PR_TRUE, kMiniNavCmd, "", "Start a ddress Book session", "DialogAddrIcon.gif", "DialogAddrIcon.gif", "DialogAddrIcon.gif", "DialogAddrIcon_mo.gif"},
{2, 36, 22, PR_TRUE, kMiniNavCmd, "", "Start a Composer session", "DialogCompIcon.gif", "DialogCompIcon.gif", "DialogCompIcon.gif", "DialogCompIcon_mo.gif"},
{0,  0,  0, PR_FALSE, 0, NULL, NULL, NULL, NULL, NULL}
};

ButtonCreateInfo gPersonalToolbarInfo[] = {
{10, 18, 18, PR_TRUE, kPersonalCmd,  "People", "http://home.netscape.com/netcenter/whitepages.html?cp=xpviewerDR1",      "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif"},
{2,  18, 18, PR_TRUE, kPersonalCmd+1,  "Stocks",   "http://personalfinance.netscape.com/finance/", "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif"},
{2,  18, 18, PR_TRUE, kPersonalCmd+2,  "Weather",  "http://www.weather.com",   "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif"},
{2,  18, 18, PR_TRUE, kPersonalCmd+3,  "Sports",   "http://excite.netscape.com/sports/",      "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif"},
{2,  18, 18, PR_TRUE, kPersonalCmd+4,  "News",     "http://home.netscape.com/news/index.html?cp=xpviewerDR1",    "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif"},
{2,  18, 18, PR_TRUE, kPersonalCmd+5,  "IMDB",     "http://us.imdb.com/Title?You%27ve+Got+Mail+(1998)",         "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif",  "TB_PersonalIcon.gif"},
{0,   0,  0, PR_FALSE, 0, NULL, NULL, NULL, NULL, NULL}
};

char * gPersonalURLS[] = { "http://home.netscape.com/netcenter/whitepages.html?cp=xpviewerDR1",
                           "http://personalfinance.netscape.com/finance/",
                           "http://excite.netscape.com/weather/", 
                           "http://excite.netscape.com/sports/",
                           "http://home.netscape.com/news/index.html?cp=xpviewerDR1",
                           "http://us.imdb.com/Title?You%27ve+Got+Mail+(1998)"};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

// prototypes
static nsEventStatus PR_CALLBACK HandleGUIEvent(nsGUIEvent *aEvent);
static void* GetItemsNativeData(nsISupports* aObject);

//----------------------------------------------------------------------
// Note: operator new zeros our memory
nsBrowserWindow::nsBrowserWindow()
{
  AddBrowser(this);
}

//---------------------------------------------------------------
nsBrowserWindow::~nsBrowserWindow()
{
}

NS_IMPL_ADDREF(nsBrowserWindow)
NS_IMPL_RELEASE(nsBrowserWindow)

//----------------------------------------------------------------------
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

//---------------------------------------------------------------
static nsEventStatus PR_CALLBACK
HandleBrowserEvent(nsGUIEvent *aEvent)
{ 
  nsEventStatus result = nsEventStatus_eIgnore;
  nsBrowserWindow* bw = nsBrowserWindow::FindBrowserFor(aEvent->widget, FIND_WINDOW);

  if (nsnull != bw) {
    nsSizeEvent* sizeEvent;
    switch(aEvent->message) {
      case NS_SIZE:
        sizeEvent = (nsSizeEvent*)aEvent;  
        bw->Layout(sizeEvent->windowSize->width, sizeEvent->windowSize->height);
        result = nsEventStatus_eConsumeNoDefault;
        break;

      case NS_DESTROY:
        {
        nsViewerApp* app = bw->mApp;
        result = nsEventStatus_eConsumeDoDefault;
        bw->Close();
        NS_RELEASE(bw);

        // XXX Really shouldn't just exit, we should just notify somebody...
          if (0 == nsBrowserWindow::gBrowsers.Count()) {
            app->Exit();
          }
        }
        return result;

      case NS_MENU_SELECTED:
        result = bw->DispatchMenuItem(((nsMenuEvent*)aEvent)->mCommand);
        break;

      // XXX This is a hack, but a needed one
      // It draws one line between the layout window and the status bar
      case NS_PAINT: 
        nsIWidget * statusWidget;
        if (bw->mStatusBar && NS_OK == bw->mStatusBar->QueryInterface(kIWidgetIID,(void**)&statusWidget)) {
          nsRect rect;
          statusWidget->GetBounds(rect);

          nsRect r;
          aEvent->widget->GetBounds(r);
          r.x = 0;
          r.y = 0;
          nsIRenderingContext *drawCtx = ((nsPaintEvent*)aEvent)->renderingContext;
          drawCtx->SetColor(NS_RGB(192, 192, 192));//aEvent->widget->GetBackgroundColor());
          rect.y -= 1;
          drawCtx->DrawLine(0, rect.y, r.width, rect.y);
          NS_RELEASE(statusWidget);
        } break;

      default:
        break;
    }
    NS_RELEASE(bw);
  }
  return result;
}

// XXX
// Temporary way to execute JavaScript. Later the javascript
// will come through the content model.

void nsBrowserWindow::ExecuteJavaScriptString(nsIWebShell* aWebShell, nsString& aJavaScript)
{
  nsAutoString retval;
  PRBool isUndefined;

  NS_ASSERTION(nsnull != aWebShell, "null webshell passed to EvaluateJavaScriptString");
//  NS_ASSERTION(nsnull != aJavaScript, "null javascript string passed to EvaluateJavaScriptString");
  static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);

  // Get nsIScriptContextOwner
  nsIScriptContextOwner* scriptContextOwner;
	if (NS_OK == aWebShell->QueryInterface(kIScriptContextOwnerIID,(void**)&scriptContextOwner)) {
    const char* url = "";
    // Get nsIScriptContext
    nsIScriptContext* scriptContext;
    nsresult res = scriptContextOwner->GetScriptContext(&scriptContext);
    if (NS_OK == res) {
      // Ask the script context to evalute the javascript string
      scriptContext->EvaluateString(aJavaScript, 
      url, 0, retval, &isUndefined);

      NS_RELEASE(scriptContext);
    }
		NS_RELEASE(scriptContextOwner);
	}
}


//----------------------------------------------------------------------
nsresult
nsBrowserWindow::Init(nsIAppShell* aAppShell,
            nsIPref* aPrefs,
            const nsRect& aBounds,
            PRUint32 aChromeMask,
            PRBool aAllowPlugins)
{
  mChromeMask = aChromeMask;
  mAppShell = aAppShell;
  mPrefs = aPrefs;
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
  mWindow->Create((nsIWidget*)NULL, r, HandleBrowserEvent, nsnull, aAppShell, nsnull, &initData);
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
                       nsScrollPreference_kAuto, 
                       aAllowPlugins, PR_TRUE);
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

  // Now lay it all out
  Layout(r.width, r.height);


  return NS_OK;
}


//---------------------------------------------------------------
// XXX This is needed for now, but I will be putting a listener on the text widget
// eventually this will be replaced with and formal "URLBar" implementation
static nsEventStatus PR_CALLBACK
HandleLocationEvent(nsGUIEvent *aEvent)
{
  nsEventStatus    result = nsEventStatus_eIgnore;
  nsBrowserWindow* bw     = nsBrowserWindow::FindBrowserFor(aEvent->widget, FIND_LOCATION);
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
      default:
        break;
    }
    NS_RELEASE(bw);
  }
  return result;
}

//-----------------------------------------------------------------
nsBrowserWindow*
nsBrowserWindow::FindBrowserFor(nsIWidget* aWidget, PRIntn aWhich)
{
  nsIWidget*    widget;
  nsBrowserWindow* result = nsnull;

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

//---------------------------------------------------------
nsBrowserWindow*
nsBrowserWindow::FindBrowserFor(nsIWidget* aWidget)
{
  nsBrowserWindow* widgetBrowser;
  nsBrowserWindow* result = nsnull;

  if (NS_OK != aWidget->GetClientData((void *&)widgetBrowser)) {
    return NULL;
  }

  PRInt32 i, n = gBrowsers.Count();
  for (i = 0; i < n; i++) {
    nsBrowserWindow* bw = (nsBrowserWindow*) gBrowsers.ElementAt(i);
    if (nsnull != bw && widgetBrowser == bw) {
      result = bw;
    }
  }

  if (nsnull != result) {
    NS_ADDREF(result);
  }
  return result;
}

//---------------------------------------------------------
void
nsBrowserWindow::AddBrowser(nsBrowserWindow* aBrowser)
{
  gBrowsers.AppendElement(aBrowser);
  NS_ADDREF(aBrowser);
}

//---------------------------------------------------------
void
nsBrowserWindow::RemoveBrowser(nsBrowserWindow* aBrowser)
{
  nsViewerApp* app = aBrowser->mApp;
  gBrowsers.RemoveElement(aBrowser);
  NS_RELEASE(aBrowser);
}

//---------------------------------------------------------
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


//---------------------------------------------------------------
void
nsBrowserWindow::UpdateToolbarBtns()
{
  if (nsnull != mToolbarBtns) {
    mToolbarBtns[gBackBtnInx]->Enable(mWebShell->CanBack() == NS_OK);
    mToolbarBtns[gBackBtnInx]->Invalidate(PR_TRUE);
    mToolbarBtns[gForwardBtnInx]->Enable(mWebShell->CanForward() == NS_OK);
    mToolbarBtns[gForwardBtnInx]->Invalidate(PR_TRUE);
  }
}

//---------------------------------------------------------------
// 
nsEventStatus
nsBrowserWindow::DispatchMenuItem(PRInt32 aID)
{
#ifdef NS_DEBUG
  nsEventStatus result = DispatchDebugMenu(aID);
  if (nsEventStatus_eIgnore != result) {
  return result;
  }
#endif
  switch (aID) {
    case VIEWER_EXIT:
      mApp->Exit();
      return nsEventStatus_eConsumeNoDefault;

    case VIEWER_COMM_NAV:
    case VIEWER_WINDOW_OPEN:
      mApp->OpenWindow();
      break;
  
    case VIEWER_FILE_OPEN:
      DoFileOpen();
      break;

    case VIEWER_FILE_VIEW_SOURCE:
      DoViewSource();
      break;
  
    case VIEWER_TOP100:
      DoSiteWalker();
      break;

    case VIEWER_EDIT_COPY:
      DoCopy();
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
    case VIEWER_DEMO9: {
        PRIntn ix = aID - VIEWER_DEMO0;
        nsAutoString url(SAMPLES_BASE_URL);
        url.Append("/test");
        url.Append(ix, 10);
        url.Append(".html");
        mWebShell->LoadURL(url);

        UpdateToolbarBtns();
      } break;
    case JS_CONSOLE:
      DoJSConsole();
    break;

    case EDITOR_MODE:
      DoEditorMode(mWebShell);
      break;
  } //switch

  return nsEventStatus_eIgnore;
}

//---------------------------------------------------------------
void
nsBrowserWindow::Back()
{
   //XXX use javascript instead of mWebShell->Back();
  nsString str("window.back();");
  ExecuteJavaScriptString(mWebShell, str);
  printf("Back executed using JavaScript");
}

//---------------------------------------------------------------
void
nsBrowserWindow::Forward()
{
   //XXX use javascript instead  of mWebShell->Forward();
  nsString str("window.forward();");
  ExecuteJavaScriptString(mWebShell, str);
}

//---------------------------------------------------------------
void
nsBrowserWindow::GoTo(const PRUnichar* aURL)
{
  mWebShell->LoadURL(aURL);
  UpdateToolbarBtns();
}

#define FILE_PROTOCOL "file://"

//---------------------------------------------------------------
static PRBool GetFileNameFromFileSelector(nsIWidget* aParentWindow,
                      nsString* aFileName)
{
  PRBool selectedFileName = PR_FALSE;
  nsIFileWidget *fileWidget;
  nsString title("Open HTML");
  nsresult rv = nsComponentManager::CreateInstance(kFileWidgetCID,
                                             nsnull,
                                             kIFileWidgetIID,
                                             (void**)&fileWidget);
  if (NS_OK == rv) {
    nsString titles[] = {"all files","html" };
    nsString filters[] = {"*.*", "*.html"};
    fileWidget->SetFilterList(2, titles, filters);
    fileWidget->Create(aParentWindow, title, eMode_load, nsnull, nsnull);

    PRUint32 result = fileWidget->Show();
    if (result) {
      fileWidget->GetFile(*aFileName);
      selectedFileName = PR_TRUE;
    }
 
    NS_RELEASE(fileWidget);
  }

  return selectedFileName;
}

//-----------------------------------------------------
void
nsBrowserWindow::DoFileOpen()
{
  nsAutoString fileName;
  char szFile[1000];
  if (GetFileNameFromFileSelector(mWindow, &fileName)) {
    fileName.ToCString(szFile, sizeof(szFile));
    PRInt32 len = strlen(szFile);
    PRInt32 sum = len + sizeof(FILE_PROTOCOL);
    char* lpszFileURL = new char[sum];
  
    // Translate '\' to '/'
    for (PRInt32 i = 0; i < len; i++) {
      if (szFile[i] == '\\') {
        szFile[i] = '/';
      }
    }

    // Build the file URL
    PR_snprintf(lpszFileURL, sum, "%s%s", FILE_PROTOCOL, szFile);

    // Ask the Web widget to load the file URL
    mWebShell->LoadURL(nsString(lpszFileURL));
    delete[] lpszFileURL;
  }
}

//-----------------------------------------------------
void
nsBrowserWindow::DoViewSource()
{
    // Ask the app shell to open a "view source window" for this window's URL.
    PRInt32    theIndex;
    PRUnichar *theURL;
    if ( // Get the history index for this page.
         NS_OK == mWebShell->GetHistoryIndex(theIndex)
         &&
         // Get the URL for that index.
         NS_OK == mWebShell->GetURL(theIndex,&theURL) ) {
        // Tell the app to open the source for this URL.
        mApp->ViewSourceFor(theURL);
        //XXX Find out how theURL is allocated, and perhaps delete it...
    } else {
        // FE_Alert!
    }
}

#define DIALOG_FONT    "Helvetica"
#define DIALOG_FONT_SIZE 10

/**--------------------------------------------------------------------------------
 * Main Handler
 *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK HandleGUIEvent(nsGUIEvent *aEvent)
{
  //printf("HandleGUIEvent aEvent->message %d\n", aEvent->message);
  nsEventStatus result = nsEventStatus_eIgnore;
  if (aEvent == nsnull ||  aEvent->widget == nsnull) {
    return result;
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



//---------------------------------------------------------------
static void* GetItemsNativeData(nsISupports* aObject)
{
	void* 			result = nsnull;
	nsIWidget* 	widget;
	if (NS_OK == aObject->QueryInterface(kIWidgetIID,(void**)&widget)) {
		result = widget->GetNativeData(NS_NATIVE_WIDGET);
		NS_RELEASE(widget);
	}
	return result;
}

//--------------------------------------------------------------------------------
// Main Dialog Handler
// XXX This is experimental code, it will change a lot when we have XML/HTML dialogs
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
        PRBool findDwn   = PR_FALSE;
        mDwnRadioBtn->GetState(findDwn);
        nsString searchStr;
        PRUint32 actualSize;
        mTextField->GetText(searchStr, 255,actualSize);

        nsIPresShell* shell = GetPresShell();
        if (nsnull != shell) {
          nsIDocument* doc = shell->GetDocument();
          if (nsnull != doc) {
            PRBool foundIt = PR_FALSE;
            doc->FindNext(searchStr, matchCase, findDwn, foundIt);
            if (!foundIt) {
              // Display Dialog here
            }
            ForceRefresh();
            NS_RELEASE(doc);
          }
          NS_RELEASE(shell);
        }
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
        PRBool findDwn   = PR_FALSE;
        mDwnRadioBtn->GetState(findDwn);
        PRUint32 actualSize;
        nsString searchStr;
        mTextField->GetText(searchStr, 255,actualSize);

        nsIPresShell* shell = GetPresShell();
        if (nsnull != shell) {
          nsIDocument* doc = shell->GetDocument();
          if (nsnull != doc) {
            PRBool foundIt = PR_FALSE;
            doc->FindNext(searchStr, matchCase, findDwn, foundIt);
            if (!foundIt) {
              // Display Dialog here
            }
            ForceRefresh();
            NS_RELEASE(doc);
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

    case NS_DESTROY: {
      mStatusAppBarWidget->Show(PR_TRUE);
      mStatusBar->DoLayout();
      NS_RELEASE(mAppsDialog);

      PRInt32 i;
      for (i=0;i<mNumAppsDialogBtns;i++) {
        NS_RELEASE(mAppsDialogBtns[i]);
      }
      delete[] mAppsDialogBtns;
      mAppsDialogBtns = nsnull;
      
    }

    default:
      result = nsEventStatus_eIgnore;
  }
  //printf("result: %d = %d\n", result, PR_FALSE);

  return result;
}

//---------------------------------------------------------------
NS_IMETHODIMP nsBrowserWindow::FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
	nsIPresShell* shell = GetPresShell();
	if (nsnull != shell) {
	  nsIDocument* doc = shell->GetDocument();
	  if (nsnull != doc) {
		  PRBool foundIt = PR_FALSE;
		  doc->FindNext(aSearchStr, aMatchCase, aSearchDown, aIsFound);
		  if (!foundIt) {
		    // Display Dialog here
		  }
		  ForceRefresh();
		  NS_RELEASE(doc);
	  }
	  NS_RELEASE(shell);
	}
  return NS_OK;
}

//---------------------------------------------------------------
NS_IMETHODIMP nsBrowserWindow::ForceRefresh()
{
  mWindow->Invalidate(PR_TRUE);
  mWebShell->Repaint(PR_TRUE);
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsIViewManager* vm = shell->GetViewManager();
    if (nsnull != vm) {
      nsIView* root;
      vm->GetRootView(root);
      if (nsnull != root) {
        vm->UpdateView(root, (nsIRegion*)nsnull, NS_VMREFRESH_IMMEDIATE);
      }
      NS_RELEASE(vm);
    }
    NS_RELEASE(shell);
  }
  return NS_OK;
}



//---------------------------------------------------------------
void nsBrowserWindow::DoFind()
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

//---------------------------------------------------------------
void
nsBrowserWindow::DoSelectAll()
{

  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsIDocument* doc = shell->GetDocument();
    if (nsnull != doc) {
      doc->SelectAll();
      ForceRefresh();
      NS_RELEASE(doc);
    }
    NS_RELEASE(shell);
  }
}


// XXX This sort of thing should be in a resource
#define TOOL_BAR_FONT    "Helvetica"
#define TOOL_BAR_FONT_SIZE 12
#define STATUS_BAR_FONT    "Helvetica"
#define STATUS_BAR_FONT_SIZE 10


//------------------------------------------------------------------
// This method needs to be moved into nsWidgetSupport
nsresult 
NS_CreateImageButton(nsISupports    *aParent, 
								    nsIImageButton  *&aButton, 
  		              nsIWidget    *&aButtonWidget,
                    const nsString  &aLabel,
								    const nsRect  &aRect, 
								    EVENT_CALLBACK  aHandleEventFunction,
								    const nsFont   *aFont,
                    const nsString  &aBaseURL,
                    const nsString  &aUpURL,
                    const nsString  &aPressedURL,
                    const nsString  &aDisabledURL,
                    const nsString  &aRollOverURL,
                    PRInt32     anImageWidth,
                    PRInt32     anImageHeight
                     )
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull) {
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);
  } else {
    return NS_ERROR_FAILURE;
  }

  // Create MenuButton
  nsresult rv = nsComponentManager::CreateInstance(kImageButtonCID, nsnull, kIImageButtonIID,
                       (void**)&aButton);
  if (NS_OK != rv) {
    return rv;
  }

	if (NS_OK == aButton->QueryInterface(kIWidgetIID,(void**)&aButtonWidget)) {
	  aButtonWidget->Create(parent, aRect, aHandleEventFunction, NULL);
	  aButtonWidget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	aButtonWidget->SetFont(*aFont);
	}

  aButton->SetLabel(aLabel);
  aButton->SetShowText(aLabel.Length() > 0);
  aButton->SetShowButtonBorder(PR_TRUE);
  aButton->SetImageDimensions(anImageWidth, anImageHeight);

  // Load URLs
  if (aBaseURL.Length() > 0) {
    nsString upURL(aBaseURL);
    nsString pressedURL(aBaseURL);
    nsString disabledURL(aBaseURL);
    nsString rolloverURL(aBaseURL);

    upURL.Append(aUpURL);
    pressedURL.Append(aPressedURL);
    disabledURL.Append(aDisabledURL);
    rolloverURL.Append(aRollOverURL);

    aButton->SetImageURLs(upURL, pressedURL, disabledURL, rolloverURL);
  } else {
    aButton->SetShowImage(PR_FALSE);
  }
  
  NS_IF_RELEASE(parent);

  return NS_OK;
}

//------------------------------------------------------------------
// XXX This method needs to be moved into nsWidgetSupport
nsresult 
NS_CreateMenuButton(nsISupports    *aParent, 
								    nsIMenuButton  *&aButton, 
  		              nsIWidget    *&aButtonWidget,
                    const nsString  &aLabel,
								    const nsRect  &aRect, 
								    EVENT_CALLBACK  aHandleEventFunction,
								    const nsFont   *aFont,
                    const nsString  &aBaseURL,
                    const nsString  &aUpURL,
                    const nsString  &aPressedURL,
                    const nsString  &aDisabledURL,
                    const nsString  &aRollOverURL,
                    PRInt32     anImageWidth,
                    PRInt32     anImageHeight)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull) {
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);
  } else {
    return NS_ERROR_FAILURE;
  }

  // Create MenuButton
  nsresult rv = nsComponentManager::CreateInstance(kMenuButtonCID, nsnull, kIMenuButtonIID,
                       (void**)&aButton);
  if (NS_OK != rv) {
    return rv;
  }

	if (NS_OK == aButton->QueryInterface(kIWidgetIID,(void**)&aButtonWidget)) {
	  aButtonWidget->Create(parent, aRect, aHandleEventFunction, NULL);
	  aButtonWidget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	aButtonWidget->SetFont(*aFont);
	}

  aButton->SetLabel(aLabel);
  aButton->SetShowText(aLabel.Length() > 0);
  aButton->SetShowButtonBorder(PR_TRUE);
  aButton->SetImageDimensions(anImageWidth, anImageHeight);

  // Load URLs
  if (aBaseURL.Length() > 0) {
    nsString upURL(aBaseURL);
    nsString pressedURL(aBaseURL);
    nsString disabledURL(aBaseURL);
    nsString rolloverURL(aBaseURL);

    upURL.Append(aUpURL);
    pressedURL.Append(aPressedURL);
    disabledURL.Append(aDisabledURL);
    rolloverURL.Append(aRollOverURL);

    aButton->SetImageURLs(upURL, pressedURL, disabledURL, rolloverURL);
    aButton->SetShowImage(PR_TRUE);
  } else {
    aButton->SetShowImage(PR_FALSE);
  }
  
  NS_IF_RELEASE(parent);

  return NS_OK;
}

//-----------------------------------------------------------
// XXX This method needs to be moved into nsWidgetSupport
nsresult 
nsBrowserWindow::AddToolbarItem(nsIToolbar    *aToolbar,
                PRInt32      aGap,
                PRBool       aEnable,
								        nsIWidget    * aButtonWidget)
{

  // Create the generic toolbar holder for widget
  nsIToolbarItemHolder * toolbarItemHolder;
  nsresult rv = nsComponentManager::CreateInstance(kToolbarItemHolderCID, nsnull, kIToolbarItemHolderIID,
                      (void**)&toolbarItemHolder);
  if (NS_OK != rv) {
    return rv;
  }

  // Get the ToolbarItem interface for adding it to the toolbar
  nsIToolbarItem * toolbarItem;
	if (NS_OK != toolbarItemHolder->QueryInterface(kIToolbarItemIID,(void**)&toolbarItem)) {
    return NS_OK;
  }

  // Set client data for callback purposes
  aButtonWidget->SetClientData(this);

  aButtonWidget->Enable(aEnable);

  // Add to widget to item holder
  toolbarItemHolder->SetWidget(aButtonWidget);

  // Add item holder to toolbar
  aToolbar->AddItem(toolbarItem, aGap, PR_FALSE);

  NS_RELEASE(toolbarItem);
  NS_RELEASE(toolbarItemHolder);


  return NS_OK;
}


//-----------------------------------------------------------
// 
void
nsBrowserWindow::DoAppsDialog()
{
  if (mAppsDialog == nsnull) {
    nscoord txtHeight   = 24;
    nscolor textBGColor = NS_RGB(0, 0, 0);
    nscolor textFGColor = NS_RGB(255, 255, 255);

    nsILookAndFeel * lookAndFeel;
    if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
     lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TextFieldHeight, txtHeight);
     lookAndFeel->GetColor(nsILookAndFeel::eColor_TextBackground, textBGColor);
     lookAndFeel->GetColor(nsILookAndFeel::eColor_TextForeground, textFGColor);
     NS_RELEASE(lookAndFeel);
    }

    nsIDeviceContext* dc = mWindow->GetDeviceContext();
    float t2d;
    dc->GetTwipsToDevUnits(t2d);
    nsFont font(DIALOG_FONT, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
          NS_FONT_WEIGHT_NORMAL, 0,
          nscoord(t2d * NSIntPointsToTwips(DIALOG_FONT_SIZE)));
    NS_RELEASE(dc);

    // create a Dialog
    //
    nsRect rect(0, 0, (40*4)+8, 23+31);  

    nsComponentManager::CreateInstance(kDialogCID, nsnull, kIDialogIID, (void**)&mAppsDialog);
    nsIWidget* widget = nsnull;
    NS_CreateDialog(mWindow, mAppsDialog, rect, HandleGUIEvent, &font);
    if (NS_OK == mAppsDialog->QueryInterface(kIWidgetIID,(void**)&widget)) {
  	  widget->SetClientData(this);
  	  widget->Resize(rect.x, rect.y, rect.width, rect.height, PR_TRUE);
  	  NS_RELEASE(widget);
    }
    mAppsDialog->SetLabel("");

    nsString label("");
    nsString baseURL("resource:/res/toolbar/");

    // Count number of buttons and create array to hold them
    mNumAppsDialogBtns = 0;
    while (gMiniAppsDialogInfo[mNumAppsDialogBtns].mImgWidth > 0) {
      mNumAppsDialogBtns++;
    }
    mAppsDialogBtns = (nsIWidget **)new PRInt32[mNumAppsDialogBtns];

    // Create buttons  
    PRInt32 i = 0;
    PRInt32 x = 2;
    while (gMiniAppsDialogInfo[i].mImgWidth > 0) {
      nsIImageButton * btn;
      nsIWidget    * widget;
      rect.SetRect(x, 2, gMiniAppsDialogInfo[i].mImgWidth+4, gMiniAppsDialogInfo[i].mImgHeight+4);
      NS_CreateImageButton(mAppsDialog,  btn, widget, gMiniAppsDialogInfo[i].mLabel, 
                 rect, nsnull,
	  						 &font, baseURL,
                 gMiniAppsDialogInfo[i].mUpName,              
                 gMiniAppsDialogInfo[i].mPressedName,              
                 gMiniAppsDialogInfo[i].mDisabledName,              
                 gMiniAppsDialogInfo[i].mRolloverName,              
                 gMiniAppsDialogInfo[i].mImgWidth,              
                 gMiniAppsDialogInfo[i].mImgHeight);
      btn->SetShowText(PR_FALSE);
      btn->SetCommand(gMiniAppsDialogInfo[i].mCommand);
      btn->SetRollOverDesc(gMiniAppsDialogInfo[i].mRollOverDesc);
      btn->AddListener(this);
      mAppsDialogBtns[i] = widget;
      widget->Enable(gMiniAppsDialogInfo[i].mEnabled);

      NS_RELEASE(btn);
      x += 40;
      i++;
    }

  }
  mDialog = mAppsDialog;
}

//-------------------------------------------------------------------
nsresult
nsBrowserWindow::CreateToolBar(PRInt32 aWidth)
{
  nsresult rv;

  nscoord txtHeight     = 24;
  nscolor textFGColor   = NS_RGB(0, 0, 0);
  nscolor textBGColor   = NS_RGB(255, 255, 255);
  nscolor windowBGColor = NS_RGB(192, 192, 192);
  nscolor widgetBGColor = NS_RGB(192, 192, 192);

  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
    lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TextFieldHeight, txtHeight);
    lookAndFeel->GetColor(nsILookAndFeel::eColor_TextBackground,  textBGColor);
    lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetBackground,  widgetBGColor);
    lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetBackground,  windowBGColor);
    NS_RELEASE(lookAndFeel);
  }

  PRInt32 imageWidth  = 23;
  PRInt32 imageHeight = 21;

  nsIDeviceContext* dc = mWindow->GetDeviceContext();
  float t2d;
  dc->GetTwipsToDevUnits(t2d);
  nsFont font(TOOL_BAR_FONT, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
              NS_FONT_WEIGHT_NORMAL, 0,
              nscoord(t2d * NSIntPointsToTwips(TOOL_BAR_FONT_SIZE)));
  NS_RELEASE(dc);

  //----------------------------------------------------
  // Create Toolbar Manager
  //----------------------------------------------------
  rv = nsComponentManager::CreateInstance(kToolbarManagerCID, nsnull, kIToolbarManagerIID,
                  (void**)&mToolbarMgr);
  if (NS_OK != rv) {
    return rv;
  }
  nsIWidget * toolbarMgrWidget;
  nsRect rr(0, 0, 200, 32); 
  if (NS_OK != mToolbarMgr->QueryInterface(kIWidgetIID,(void**)&toolbarMgrWidget)) {
    return rv;
  }
	toolbarMgrWidget->Create(mWindow, rr, nsnull, NULL);
	toolbarMgrWidget->SetBackgroundColor(windowBGColor);
	toolbarMgrWidget->Show(PR_TRUE);

  mToolbarMgr->AddToolbarListener(this);
  mToolbarMgr->SetCollapseTabURLs("resource:/res/toolbar/TB_Tab.gif", "resource:/res/toolbar/TB_Tab.gif", "resource:/res/toolbar/TB_Tab.gif", "resource:/res/toolbar/TB_Tab_mo.gif");
  mToolbarMgr->SetExpandTabURLs("resource:/res/toolbar/TB_HTab.gif", "resource:/res/toolbar/TB_HTab.gif", "resource:/res/toolbar/TB_HTab.gif", "resource:/res/toolbar/TB_HTab_mo.gif");

  //----------------------------------------------------
  // Create Button Toolbar
  //----------------------------------------------------
  rv = nsComponentManager::CreateInstance(kToolbarCID, nsnull, kIToolbarIID,
                  (void**)&mBtnToolbar);
  if (NS_OK != rv) {
    return rv;
  }

  nsIWidget * toolbarWidget;
  nsRect rrr(0, 0, 200, 32); 
  if (NS_OK != mBtnToolbar->QueryInterface(kIWidgetIID,(void**)&toolbarWidget)) {
    return rv;
  }
  mBtnToolbar->SetMargin(1);
  mBtnToolbar->SetHGap(2);

	toolbarWidget->Create(toolbarMgrWidget, rrr, nsnull, NULL);
	toolbarWidget->SetBackgroundColor(windowBGColor);
	toolbarWidget->Show(PR_TRUE);
  mToolbarMgr->AddToolbar(mBtnToolbar);

  nsIWidget * tab = nsnull;
//  mBtnToolbar->CreateTab(tab);
  NS_IF_RELEASE(tab);


  gBtnWidth  = 54;
  gBtnHeight = 42;

  nsRect r(0, 0, gBtnWidth, gBtnHeight); 
	nsIWidget* widget = nsnull;

  nsString baseURL("resource:/res/toolbar/");

  // Count number of buttons and create array to hold them
  mNumToolbarBtns = 0;
  while (gBtnToolbarInfo[mNumToolbarBtns].mImgWidth > 0) {
    mNumToolbarBtns++;
  }
  mToolbarBtns = (nsIWidget **)new PRInt32[mNumToolbarBtns];

  nsRect rect(0,0, 54, 42);
  PRInt32 width, height;

  // Create buttons  
  PRInt32 i = 0;
  while (gBtnToolbarInfo[i].mImgWidth > 0) {
  nsIMenuButton  * btn;
  nsIWidget    * widget;
  NS_CreateMenuButton(mBtnToolbar,  btn, widget, gBtnToolbarInfo[i].mLabel, 
                        rect, nsnull,              
	  							      &font, baseURL,
                        gBtnToolbarInfo[i].mUpName,                            
                        gBtnToolbarInfo[i].mPressedName,                            
                        gBtnToolbarInfo[i].mDisabledName,                            
                        gBtnToolbarInfo[i].mRolloverName,                            
                        gBtnToolbarInfo[i].mImgWidth,                            
                        gBtnToolbarInfo[i].mImgHeight);
    AddToolbarItem(mBtnToolbar, gBtnToolbarInfo[i].mGap, gBtnToolbarInfo[i].mEnabled, widget);
    btn->SetBorderOffset(1);
    btn->SetCommand(gBtnToolbarInfo[i].mCommand);
    btn->SetRollOverDesc(gBtnToolbarInfo[i].mRollOverDesc);
    btn->AddListener(this);
	  widget->SetBackgroundColor(widgetBGColor);
    widget->GetPreferredSize(width, height);
    widget->SetPreferredSize(54, height);
    widget->Resize(0,0, 54, height,PR_FALSE);
    mToolbarBtns[i] = widget;
    rect.x += 40;
    i++;
    NS_RELEASE(btn);
  }
  mBtnToolbar->SetWrapping(PR_TRUE);
  toolbarWidget->GetPreferredSize(width, height);
  toolbarWidget->SetPreferredSize(width, height);
  toolbarWidget->Resize(0,0,width, height,PR_FALSE);

  // Create and place throbber
  r.SetRect(aWidth - THROBBER_WIDTH, 0,
            THROBBER_WIDTH, THROBBER_HEIGHT);
  rv = nsComponentManager::CreateInstance(kThrobberCID, nsnull, kIThrobberIID,
                                    (void**)&mThrobber);
  if (NS_OK != rv) {
    return rv;
  }
  //mBtnToolbar->SetThrobber(mThrobber);
  nsString throbberURL("resource:/res/throbber/LargeAnimation%02d.gif");
  mThrobber->Init(toolbarWidget, r, throbberURL, 38);

  nsIToolbarItem * toolbarItem;
  if (NS_OK != mThrobber->QueryInterface(kIToolbarItemIID,(void**)&toolbarItem)) {
    return NS_OK;
  }

  mBtnToolbar->AddItem(toolbarItem, 10, PR_FALSE);
  NS_RELEASE(toolbarItem);

  mThrobber->Show(); 

  mBtnToolbar->SetLastItemIsRightJustified(PR_TRUE);
  NS_IF_RELEASE(toolbarWidget);

  //----------------------------------------------------
  // Create URL Toolbar
  //----------------------------------------------------
  rv = nsComponentManager::CreateInstance(kToolbarCID, nsnull, kIToolbarIID,
                                    (void**)&mURLToolbar);
  if (NS_OK != rv) {
    return rv;
  }
  nsIWidget * urlToolbarWidget;
  if (NS_OK != mURLToolbar->QueryInterface(kIWidgetIID,(void**)&urlToolbarWidget)) {
    return rv;
  }
  mURLToolbar->SetMargin(1);
  mURLToolbar->SetHGap(2);

	urlToolbarWidget->Create(toolbarMgrWidget, rr, nsnull, NULL);
	urlToolbarWidget->SetBackgroundColor(windowBGColor);
	urlToolbarWidget->Show(PR_TRUE);
  mToolbarMgr->AddToolbar(mURLToolbar);

  tab = nsnull;
//  mURLToolbar->CreateTab(tab);
  NS_IF_RELEASE(tab);


  //------
  // Create and place Bookmark button
  //------
  rv = nsComponentManager::CreateInstance(kImageButtonCID, nsnull, kIImageButtonIID,
                                    (void**)&mBookmarks);
  if (NS_OK != rv) {
    return rv;
  }
  r.SetRect(0, 0, 105, 22);  

  // Create Bookmarks Menu Btn
  if (NS_OK == NS_CreateMenuButton(mURLToolbar, mBookmarks, mBookmarksWidget, "Bookmarks", 
                      r, nsnull, &font, baseURL,
                      "TB_Bookmarks.gif", "TB_Bookmarks.gif", "TB_Bookmarks.gif", "TB_Bookmarks_mo.gif",
                      30, 18)) {
    AddToolbarItem(mURLToolbar, 10, PR_FALSE, mBookmarksWidget);
	  mBookmarksWidget->SetBackgroundColor(widgetBGColor);
    mBookmarks->SetCommand(kBookmarksCmd);
    mBookmarks->SetRollOverDesc("Bookmarks functions");
    mBookmarks->AddListener(this);
    mBookmarks->SetImageVerticalAlignment(eButtonVerticalAligment_Center);
    mBookmarks->SetImageHorizontalAlignment(eButtonHorizontalAligment_Left);
    mBookmarks->SetTextVerticalAlignment(eButtonVerticalAligment_Center);
    mBookmarks->SetTextHorizontalAlignment(eButtonHorizontalAligment_Right);
#ifdef XP_WIN
    mBookmarks->AddMenuItem("Add Bookmark", 3000);
#endif
  }

  // Create location icon
  r.SetRect(0, 0, 18, 18);  
  if (NS_OK == NS_CreateImageButton(mURLToolbar, mLocationIcon, mLocationIconWidget, "", 
                      r, nsnull, &font, baseURL,
                      "TB_Location.gif", "TB_Location.gif", "TB_Location.gif", "TB_Location_mo.gif",
                      16, 16)) {
    AddToolbarItem(mURLToolbar, 10, PR_TRUE, mLocationIconWidget);
	  mLocationIconWidget->SetBackgroundColor(widgetBGColor);
    mLocationIcon->SetRollOverDesc("Drag this location to Bookmarks");
    mLocationIcon->SetImageVerticalAlignment(eButtonVerticalAligment_Center);
    mLocationIcon->SetImageHorizontalAlignment(eButtonHorizontalAligment_Middle);
    mLocationIcon->SetShowBorder(PR_FALSE);
  }

  // Create Location Label
  nsIImageButton * locationLabel;
  if (NS_OK == NS_CreateImageButton(mURLToolbar, locationLabel, widget, "Location:", 
                      r, nsnull, &font, "", "", "", "", "", 0, 0)) {

    AddToolbarItem(mURLToolbar, 1, PR_TRUE, widget);
	  widget->SetBackgroundColor(widgetBGColor);
    locationLabel->SetShowImage(PR_FALSE);
    locationLabel->SetShowBorder(PR_FALSE);
    locationLabel->SetTextVerticalAlignment(eButtonVerticalAligment_Center);
    locationLabel->SetTextHorizontalAlignment(eButtonHorizontalAligment_Right);

    PRInt32 width, height;
    widget->GetPreferredSize(width, height);
    widget->SetPreferredSize(width, height);
    widget->Resize(0,0, width, height, PR_FALSE);
		NS_RELEASE(locationLabel);
		NS_RELEASE(widget);
  }

  // Create and place URL Text Field
  r.SetRect(0, 0, 200, 24);

  rv = nsComponentManager::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID,
                                    (void**)&mLocation);
  if (NS_OK != rv) {
    return rv;
  }

  dc = mWindow->GetDeviceContext();
  dc->GetTwipsToDevUnits(t2d);
  nsFont font2(TOOL_BAR_FONT, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
              NS_FONT_WEIGHT_NORMAL, 0,
              nscoord(t2d * NSIntPointsToTwips(TOOL_BAR_FONT_SIZE-2)));
  NS_RELEASE(dc);

  NS_CreateTextWidget(urlToolbarWidget, mLocation, r, HandleLocationEvent, &font2);
	if (NS_OK == mLocation->QueryInterface(kIWidgetIID,(void**)&mLocationWidget)) { 

    AddToolbarItem(mURLToolbar, 2, PR_TRUE, mLocationWidget);

    mLocationWidget->SetPreferredSize(100, txtHeight);
    mLocationWidget->SetForegroundColor(textFGColor);
    mLocationWidget->SetBackgroundColor(textBGColor);
    mLocationWidget->Show(PR_TRUE);
    PRUint32 size;
    mLocation->SetText("",size);
  }

  // Create and place What's Related button
  r.SetRect(0, 0, 123, 22);  
  if (NS_OK == NS_CreateMenuButton(mURLToolbar, mWhatsRelated, mWhatsRelatedWidget, "What's Related", 
                      r, nsnull, &font, baseURL,
                      "TB_WhatsRelated.gif", "TB_WhatsRelated.gif", "TB_WhatsRelated.gif", "TB_WhatsRelated_mo.gif",
                      27, 16)) {
    AddToolbarItem(mURLToolbar, 2, PR_FALSE, mWhatsRelatedWidget);
	  mWhatsRelatedWidget->SetBackgroundColor(widgetBGColor);
    mWhatsRelated->SetCommand(kWhatsRelatedCmd);
    mWhatsRelated->SetRollOverDesc("View the \"What's Related\" list");
    mWhatsRelated->AddListener(this);
    mWhatsRelated->SetImageVerticalAlignment(eButtonVerticalAligment_Center);
    mWhatsRelated->SetImageHorizontalAlignment(eButtonHorizontalAligment_Left);
    mWhatsRelated->SetTextVerticalAlignment(eButtonVerticalAligment_Center);
    mWhatsRelated->SetTextHorizontalAlignment(eButtonHorizontalAligment_Right);
	}

  mURLToolbar->SetLastItemIsRightJustified(PR_TRUE);
  mURLToolbar->SetNextLastItemIsStretchy(PR_TRUE);


  //----------------------------------------------
  // Load Personal Toolbar
  //----------------------------------------------
  nsIToolbar * personalToolbar;
  rv = nsComponentManager::CreateInstance(kToolbarCID, nsnull, kIToolbarIID,
                                    (void**)&personalToolbar);
  if (NS_OK != rv) {
    return rv;
  }

  nsIWidget * personalToolbarWidget;
  rrr.SetRect(0, 0, 200, 23); 
  if (NS_OK != personalToolbar->QueryInterface(kIWidgetIID,(void**)&personalToolbarWidget)) {
    return rv;
  }
  personalToolbar->SetMargin(1);
  personalToolbar->SetHGap(2);
  personalToolbar->SetWrapping(PR_TRUE);

  personalToolbarWidget->Create(toolbarMgrWidget, rrr, nsnull, NULL);
	personalToolbarWidget->SetBackgroundColor(windowBGColor);
	personalToolbarWidget->Show(PR_TRUE);
  mToolbarMgr->AddToolbar(personalToolbar);

  tab = nsnull;
//  personalToolbar->CreateTab(tab);
  NS_IF_RELEASE(tab);


  // Count number of buttons and create array to hold them
  mNumPersonalToolbarBtns = 0;
  while (gPersonalToolbarInfo[mNumPersonalToolbarBtns].mImgWidth > 0) {
  mNumPersonalToolbarBtns++;
  }
  mPersonalToolbarBtns = (nsIWidget **)new PRInt32[mNumPersonalToolbarBtns];

  rect.SetRect(0,0, 100, 21);

  // Create buttons  
  i = 0;
  while (gPersonalToolbarInfo[i].mImgWidth > 0) {
  nsIImageButton * btn;
  nsIWidget    * widget;
  NS_CreateImageButton(personalToolbar,  btn, widget, gPersonalToolbarInfo[i].mLabel, 
            rect, nsnull,              
	  							    &font, baseURL,
            gPersonalToolbarInfo[i].mUpName,              
            gPersonalToolbarInfo[i].mPressedName,              
            gPersonalToolbarInfo[i].mDisabledName,              
            gPersonalToolbarInfo[i].mRolloverName,              
            gPersonalToolbarInfo[i].mImgWidth,              
            gPersonalToolbarInfo[i].mImgHeight);

  btn->SetImageVerticalAlignment(eButtonVerticalAligment_Center);
  btn->SetImageHorizontalAlignment(eButtonHorizontalAligment_Left);
  btn->SetTextVerticalAlignment(eButtonVerticalAligment_Center);
  btn->SetTextHorizontalAlignment(eButtonHorizontalAligment_Right);
  btn->SetCommand(gPersonalToolbarInfo[i].mCommand);
  btn->SetRollOverDesc(gPersonalToolbarInfo[i].mRollOverDesc);
  btn->AddListener(this);

  PRInt32 width, height;
	  widget->SetBackgroundColor(widgetBGColor);
  widget->GetPreferredSize(width, height);
  widget->SetPreferredSize(width, height);
  widget->Resize(0, 0, width, height, PR_FALSE);
  AddToolbarItem(personalToolbar, gPersonalToolbarInfo[i].mGap, gPersonalToolbarInfo[i].mEnabled, widget);
  mPersonalToolbarBtns[i] = widget;
  NS_RELEASE(btn);
  i++;
  }
  NS_RELEASE(personalToolbar);
  NS_RELEASE(personalToolbarWidget);

  //------
  NS_RELEASE(toolbarMgrWidget);

  return NS_OK;
}

//-----------------------------------------------------------
nsresult
nsBrowserWindow::CreateStatusBar(PRInt32 aWidth)
{
  nsresult rv;
  nscolor windowBGColor = NS_RGB(192, 192, 192);
  nscolor widgetBGColor = NS_RGB(192, 192, 192);

  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
    lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetBackground,  widgetBGColor);
    lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetBackground,  windowBGColor);
    NS_RELEASE(lookAndFeel);
  }

  nsIDeviceContext* dc = mWindow->GetDeviceContext();
  float t2d;
  dc->GetTwipsToDevUnits(t2d);
  nsFont font(STATUS_BAR_FONT, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
        NS_FONT_WEIGHT_NORMAL, 0,
        nscoord(t2d * NSIntPointsToTwips(STATUS_BAR_FONT_SIZE)));
  NS_RELEASE(dc);

  //----------------------------------------------------
  // Create StatusBar as a Toolbar
  //----------------------------------------------------
  rv = nsComponentManager::CreateInstance(kToolbarCID, nsnull, kIToolbarIID,
                  (void**)&mStatusBar);
  if (NS_OK != rv) {
    return rv;
  }

  nsIWidget * statusWidget;
  nsRect rrr(0, 0, 200, 32); 
  if (NS_OK != mStatusBar->QueryInterface(kIWidgetIID,(void**)&statusWidget)) {
    return rv;
  }
  mStatusBar->SetMargin(1);
  mStatusBar->SetHGap(2);
  mStatusBar->SetBorderType(eToolbarBorderType_none);

	statusWidget->Create(mWindow, rrr, nsnull, NULL);
	statusWidget->SetBackgroundColor(windowBGColor);
	statusWidget->Show(PR_TRUE);

  nsRect r(0, 0, 18, 16); 
	nsIWidget* widget = nsnull;

  nsString baseURL("resource:/res/toolbar/");

  nsString insecureImg("StatusBar-insecure.gif");
  nsString secureImg("StatusBar-secure.gif");

  if (NS_OK == NS_CreateImageButton(mStatusBar, mStatusSecurityLabel, widget, "", 
                                    r, nsnull, &font, baseURL,
                                    insecureImg, insecureImg, secureImg, insecureImg,
                                    16, 14)) {

    AddToolbarItem(mStatusBar, 0, PR_TRUE, widget);
    // Make button look depressed and always show border
    mStatusSecurityLabel->SwapHighlightShadowColors();
    mStatusSecurityLabel->SetAlwaysShowBorder(PR_TRUE);
    mStatusSecurityLabel->SetShowButtonBorder(PR_FALSE);
    mStatusSecurityLabel->SetShowText(PR_FALSE);
    mStatusSecurityLabel->SetBorderWidth(0);
    mStatusSecurityLabel->SetBorderOffset(0);
    mStatusSecurityLabel->SetImageVerticalAlignment(eButtonVerticalAligment_Center);
	  widget->SetBackgroundColor(widgetBGColor);
    NS_RELEASE(widget);
  }

  r.SetRect(0, 0, 96, 16); 
  if (NS_OK == NS_CreateImageButton(mStatusBar, mStatusProcess, widget, "", 
                                    r, nsnull, &font, "",
                                    "", "", "", "", 0, 0)) {

    AddToolbarItem(mStatusBar, 2, PR_TRUE, widget);
    // Make button look depressed and always show border
    mStatusProcess->SwapHighlightShadowColors();
    mStatusProcess->SetAlwaysShowBorder(PR_TRUE);
    mStatusProcess->SetShowButtonBorder(PR_FALSE);
    mStatusProcess->SetShowText(PR_TRUE);
    mStatusProcess->SetShowImage(PR_FALSE);
    mStatusProcess->SetBorderOffset(0);
	  widget->SetBackgroundColor(widgetBGColor);
    widget->SetPreferredSize(96, 16); 
    NS_RELEASE(widget);
  }

  if (NS_OK == NS_CreateImageButton(mStatusBar, mStatusText, widget, "", 
            r, nsnull, &font, "",
            "", "", "", "", 0,0)) {

    AddToolbarItem(mStatusBar, 2, PR_TRUE, widget);
    // Make button look depressed and always show border
    mStatusText->SwapHighlightShadowColors();
    mStatusText->SetAlwaysShowBorder(PR_TRUE);
    mStatusText->SetShowButtonBorder(PR_FALSE);
    mStatusText->SetBorderOffset(0);
    // Set up to just display text
    mStatusText->SetShowText(PR_TRUE);
    mStatusText->SetShowImage(PR_FALSE);
    mStatusText->SetTextHorizontalAlignment(eButtonHorizontalAligment_Left);
	  widget->SetBackgroundColor(widgetBGColor);
    NS_RELEASE(widget);
  }

  //----------------------------------------------------
  // Create Mini Tools Toolbar
  //----------------------------------------------------
  rrr.SetRect(0, 0, 150, 18); 
  rv = nsComponentManager::CreateInstance(kToolbarCID, nsnull, kIToolbarIID,
                  (void**)&mStatusAppBar);
  if (NS_OK != rv) {
    return rv;
  }

  if (NS_OK != mStatusAppBar->QueryInterface(kIWidgetIID,(void**)&mStatusAppBarWidget)) {
    return rv;
  }
  mStatusAppBar->SetMargin(1);
  mStatusAppBar->SetHGap(2);
  mStatusAppBar->SetBorderType(eToolbarBorderType_full);

	mStatusAppBarWidget->Create(statusWidget, rrr, nsnull, NULL);
	mStatusAppBarWidget->SetBackgroundColor(windowBGColor);
	mStatusAppBarWidget->Show(PR_TRUE);

  mStatusAppBarWidget->SetClientData(this);

  // Get the ToolbarItem interface for adding it to the toolbar
  nsIToolbarItem * toolbarItem;
	if (NS_OK != mStatusAppBar->QueryInterface(kIToolbarItemIID,(void**)&toolbarItem)) {
    return NS_OK;
  }
  mStatusBar->AddItem(toolbarItem, 2, PR_FALSE);

  // Count number of buttons and create array to hold them
  mNumMiniAppsBtns = 0;
  while (gMiniAppsToolbarInfo[mNumMiniAppsBtns].mImgWidth > 0) {
    mNumMiniAppsBtns++;
  }
  mMiniAppsBtns = (nsIWidget **)new PRInt32[mNumMiniAppsBtns];

  // Create buttons  
  PRInt32 i = 0;
  nsRect  rect;
  while (gMiniAppsToolbarInfo[i].mImgWidth > 0) {
    if (i == 0) {
      rect.SetRect(0, 0, 13, 18); 
    } else {
      rect.SetRect(0, 0, 32, 16); 
    }
    nsIImageButton * btn;
    nsIWidget    * widget;
    NS_CreateImageButton(mStatusAppBar,  btn, widget, gMiniAppsToolbarInfo[i].mLabel, 
                          rect, nsnull,
	  							        &font, baseURL,
                          gMiniAppsToolbarInfo[i].mUpName,              
                          gMiniAppsToolbarInfo[i].mPressedName,              
                          gMiniAppsToolbarInfo[i].mDisabledName,              
                          gMiniAppsToolbarInfo[i].mRolloverName,              
                          gMiniAppsToolbarInfo[i].mImgWidth,              
                          gMiniAppsToolbarInfo[i].mImgHeight);
    AddToolbarItem(mStatusAppBar, gMiniAppsToolbarInfo[i].mGap, gMiniAppsToolbarInfo[i].mEnabled, widget);
    btn->SetShowText(PR_FALSE);
    btn->SetImageVerticalAlignment(eButtonVerticalAligment_Center);
    btn->SetImageHorizontalAlignment(eButtonHorizontalAligment_Middle);
    btn->SetShowButtonBorder(PR_FALSE);
    btn->SetBorderOffset(0);
    btn->SetCommand(gMiniAppsToolbarInfo[i].mCommand);
    btn->SetRollOverDesc(gMiniAppsToolbarInfo[i].mRollOverDesc);
    btn->AddListener(this);
	  widget->SetBackgroundColor(widgetBGColor);
    if (i == 0) {
      btn->SetAlwaysShowBorder(PR_TRUE);
    }
    mMiniAppsBtns[i] = widget;
    rect.x += 40;
    i++;
    NS_RELEASE(btn);
  }
  PRInt32 width,height;
	mStatusAppBarWidget->GetPreferredSize(width, height);
	mStatusAppBarWidget->SetPreferredSize(width, height);
	mStatusAppBarWidget->Resize(0,0,width, height, PR_FALSE);

  mStatusBar->SetLastItemIsRightJustified(PR_TRUE);
  mStatusBar->SetNextLastItemIsStretchy(PR_TRUE);

  return NS_OK;
}

//----------------------------------------------------
NS_METHOD 
nsBrowserWindow::NotifyToolbarManagerChangedSize(nsIToolbarManager* aToolbarMgr)
{
  nsRect rect;
  GetBounds(rect);
  Layout(rect.width, rect.height);

  return NS_OK;
}

//----------------------------------------------------
NS_METHOD 
nsBrowserWindow::NotifyImageButtonEvent(nsIImageButton * aImgBtn, nsGUIEvent* anEvent)
{
  if (anEvent->message == NS_MOUSE_ENTER) {
    nsString msg;
    aImgBtn->GetRollOverDesc(msg);
    SetStatus(msg);
    return NS_OK;
  } else if (anEvent->message == NS_MOUSE_EXIT) {
    SetStatus("");
    return NS_OK;
  } else if (anEvent->message != NS_MOUSE_LEFT_BUTTON_UP) {
    return NS_OK;
  }

  PRInt32 command;
  if (NS_OK != aImgBtn->GetCommand(command)) {
    return NS_ERROR_FAILURE;
  }

  // Do Personal toolbar
  if (command >= kPersonalCmd) {
    nsString url(gPersonalURLS[command - kPersonalCmd]);
    mWebShell->LoadURL(url);
    return NS_OK;
  }

  // Do the rest of the commands
  switch (command) {
    case kStopCmd :
      mWebShell->Stop();
      UpdateToolbarBtns();
      break;

    case kBackCmd :
      Back();
      UpdateToolbarBtns();
      break;

    case kForwardCmd :
      Forward();
      UpdateToolbarBtns();
      break;

    case kReloadCmd :  {
      mWebShell->Reload(nsURLReloadBypassCache);
      } break;
  
    case kHomeCmd : {
      //XXX This test using javascript instead of calling directly
      nsString str("window.home();");
      ExecuteJavaScriptString(mWebShell, str);
   
      // nsString homeURL("http://www.netscape.com");
     // mWebShell->LoadURL(homeURL);
      } break;

    case kMiniNavCmd :
      mApp->OpenWindow();
      break;

    case kMiniTabCmd : {
      DoAppsDialog();
      nsIWidget * widget;
      if (NS_OK == mAppsDialog->QueryInterface(kIWidgetIID,(void**)&widget)) {
        widget->Show(PR_TRUE);
        NS_RELEASE(widget);
      }
      mStatusAppBarWidget->Show(PR_FALSE);
      mStatusBar->DoLayout();
      } break;

  } // switch

  return NS_OK;
}

//----------------------------------------------------
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

  nsIWidget * tbManagerWidget = 0;
  if (mToolbarMgr && NS_OK != mToolbarMgr->QueryInterface(kIWidgetIID,(void**)&tbManagerWidget)) {
    return;
  }

  nsRect rr(0, 0, aWidth, aHeight);

  PRInt32 preferredWidth  = aWidth;
  PRInt32 preferredHeight = aHeight;

  // If there is a toolbar widget, size/position it.
  if(tbManagerWidget) {
    tbManagerWidget->GetPreferredSize(preferredWidth, preferredHeight);
    tbManagerWidget->Resize(0,0, aWidth, preferredHeight, PR_TRUE);
    rr.y      += preferredHeight;
    rr.height -= preferredHeight;
  }

  nsIWidget* statusWidget = nsnull;
  PRInt32 statusBarHeight = 0;

  // If there is a status bar widget, size/position it.
  if (mStatusBar && NS_OK == mStatusBar->QueryInterface(kIWidgetIID,(void**)&statusWidget)) {
    if (statusWidget) {
      if (mChromeMask & NS_CHROME_STATUS_BAR_ON) {
        //PRInt32 width;
        //statusWidget->GetPreferredSize(width, statusBarHeight);
        statusBarHeight = 22;
        statusWidget->Resize(0, aHeight - statusBarHeight, aWidth, statusBarHeight, PR_TRUE);
        rr.height -= statusBarHeight;
        statusWidget->Show(PR_TRUE);
      } else {
        statusWidget->Show(PR_FALSE);
      }
    }
  }

  // inset the web widget
  rr.x += WEBSHELL_LEFT_INSET;
  rr.y += WEBSHELL_TOP_INSET;
  rr.width -= WEBSHELL_LEFT_INSET + WEBSHELL_RIGHT_INSET;
  rr.height -= WEBSHELL_TOP_INSET + WEBSHELL_BOTTOM_INSET;
  mWebShell->SetBounds(rr.x, rr.y, rr.width, rr.height);

  NS_IF_RELEASE(statusWidget);
  NS_IF_RELEASE(tbManagerWidget);
}

//----------------------------------------------------
NS_IMETHODIMP
nsBrowserWindow::MoveTo(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Move(aX, aY);
  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsBrowserWindow::SizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");

  // XXX We want to do this in one shot
  mWindow->Resize(aWidth, aHeight, PR_FALSE);
  Layout(aWidth, aHeight);

  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsBrowserWindow::GetBounds(nsRect& aBounds)
{
  mWindow->GetBounds(aBounds);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::GetWindowBounds(nsRect& aBounds)
{
  //XXX This needs to be non-client bounds when it exists.
  mWindow->GetBounds(aBounds);
  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsBrowserWindow::Show()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Show(PR_TRUE);
  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsBrowserWindow::Hide()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Show(PR_FALSE);
  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsBrowserWindow::Close()
{
  RemoveBrowser(this);

  if (nsnull != mWebShell) {
    mWebShell->Destroy();
    NS_RELEASE(mWebShell);
  }

  if (nsnull != mWindow) {
    nsIWidget* w = mWindow;
    NS_RELEASE(w);
  }

  return NS_OK;
}


//----------------------------------------------------
NS_IMETHODIMP
nsBrowserWindow::SetChrome(PRUint32 aChromeMask)
{
  mChromeMask = aChromeMask;
  nsRect r;
  mWindow->GetBounds(r);
  Layout(r.width, r.height);

  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsBrowserWindow::GetChrome(PRUint32& aChromeMaskResult)
{
  aChromeMaskResult = mChromeMask;
  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsBrowserWindow::GetWebShell(nsIWebShell*& aResult)
{
  aResult = mWebShell;
  NS_IF_ADDREF(mWebShell);
  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsBrowserWindow::HandleEvent(nsGUIEvent * anEvent)
{
  if (anEvent->widget == mToolbarBtns[gBackBtnInx]) {
    Back();
    UpdateToolbarBtns();

  } else if (anEvent->widget == mToolbarBtns[gForwardBtnInx]) {
    Forward();
    UpdateToolbarBtns();

  } else if (mAppsDialogBtns != nsnull && anEvent->widget == mAppsDialogBtns[0]) {
    mApp->OpenWindow();

  } else if (anEvent->widget == mMiniAppsBtns[1]) {
    mApp->OpenWindow();

  } else if (anEvent->widget == mToolbarBtns[gHomeBtnInx]) {
    nsString homeURL("http://home.netscape.com/eng/mozilla/5.0/DR1/hello.html");
    mWebShell->LoadURL(homeURL);

  } else if (anEvent->widget == mMiniAppsBtns[0]) {
    DoAppsDialog();
    nsIWidget * widget;
    if (NS_OK == mAppsDialog->QueryInterface(kIWidgetIID,(void**)&widget)) {
      widget->Show(PR_TRUE);
      NS_RELEASE(widget);
    }
    mStatusAppBarWidget->Show(PR_FALSE);
    mStatusBar->DoLayout();
  }

  PRInt32 i = 0;
  for (i=0;i<mNumPersonalToolbarBtns;i++) {
    if (mPersonalToolbarBtns[i] == anEvent->widget) {
      nsString url(gPersonalURLS[i]);
      mWebShell->LoadURL(url);
      break;
    }
  }
  
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::SetTitle(const PRUnichar* aTitle)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mTitle = aTitle;
  mWindow->SetTitle(aTitle);
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::GetTitle(PRUnichar** aResult)
{
  *aResult = mTitle;
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::SetStatus(const PRUnichar* aStatus)
{
  nsString msg(aStatus);
  SetStatus(msg);
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::SetStatus(const nsString & aMsg)
{
  if (nsnull != mStatusText) {
    mStatusText->SetLabel(aMsg);
    nsIWidget * widget;
    if (NS_OK == mStatusText->QueryInterface(kIWidgetIID,(void**)&widget)) {
      widget->Invalidate(PR_TRUE);
      NS_RELEASE(widget);
    }
  }
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::GetStatus(PRUnichar** aResult)
{
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::SetProgress(PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason)
{
  if (mStatusBar) {
    nsAutoString url("Connecting to ");
    url.Append(aURL);

    SetStatus(aURL);
  }
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
  if (mThrobber) {
    mThrobber->Start();
    PRUint32 size;
    mLocation->SetText(aURL,size);
  }
  nsIWidget * widget;
  if (mToolbarBtns && NS_OK == mToolbarBtns[gStopBtnInx]->QueryInterface(kIWidgetIID,(void**)&widget)) {
    widget->Enable(PR_TRUE);
    widget->Invalidate(PR_TRUE);
    NS_RELEASE(widget);
  }
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aStatus)
{
  if (mThrobber) {
    mThrobber->Stop();
  }
  nsIWidget * widget;
  if (mToolbarBtns && NS_OK == mToolbarBtns[gStopBtnInx]->QueryInterface(kIWidgetIID,(void**)&widget)) {
    widget->Enable(PR_FALSE);
    widget->Invalidate(PR_TRUE);
    NS_RELEASE(widget);
  }
  return NS_OK;
}


//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::NewWebShell(PRUint32 aChromeMask,
                             PRBool aVisible,
                             nsIWebShell*& aNewWebShell) 
{
  nsresult rv = NS_OK;

  // Create new window. By default, the refcnt will be 1 because of
  // the registration of the browser window in gBrowsers.
  nsBrowserWindow* browser;
  NS_NEWXPCOM(browser, nsBrowserWindow);

  if (nsnull != browser)   {
    nsRect  bounds;
    GetBounds(bounds);

    browser->SetApp(mApp);
    rv = browser->Init(mAppShell, mPrefs, bounds, aChromeMask, mAllowPlugins);
    if (NS_OK == rv) {
      if (aVisible) {
        browser->Show();
      }
      nsIWebShell *shell;
      rv = browser->GetWebShell(shell);
      aNewWebShell = shell;
    } else {
      browser->Close();
    }
  }
  else
  rv = NS_ERROR_OUT_OF_MEMORY;

  return rv;
}

//----------------------------------------
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
      PRUnichar *name;
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

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::OnProgress(nsIURL* aURL,
                            PRUint32 aProgress,
                            PRUint32 aProgressMax)
{
  if (mStatusBar) {
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
    SetStatus(url);
  }
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
  SetStatus(aMsg);
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  if (mStatusBar) {
    nsAutoString url;
    if (nsnull != aURL) {
      PRUnichar* str;
      aURL->ToString(&str);
      url = str;
      delete[] str;
    }
    url.Append(": start");
    SetStatus(url);
  }
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::OnStopBinding(nsIURL* aURL,
                               nsresult status,
                               const PRUnichar* aMsg)
{
  nsAutoString url;
  if (nsnull != aURL) {
    PRUnichar* str;
    aURL->ToString(&str);
    url = str;
    delete[] str;
  }
  url.Append(": stop");
  SetStatus(url);

  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP_(void)
nsBrowserWindow::Alert(const nsString &aText)
{
  char *str;

  str = aText.ToNewCString();
  printf("Browser Window Alert: %c%s\n", '\007', str);
  delete[] str;
}

//----------------------------------------
NS_IMETHODIMP_(PRBool)
nsBrowserWindow::Confirm(const nsString &aText)
{
  char *str;

  str = aText.ToNewCString();
  printf("%cBrowser Window Confirm: %s (y/n)? ", '\007', str);
  delete[] str;
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

//----------------------------------------
NS_IMETHODIMP_(PRBool)
nsBrowserWindow::Prompt(const nsString &aText,
            const nsString &aDefault,
            nsString &aResult)
{
  char *str;
  char buf[256];

  str = aText.ToNewCString();
  printf("Browser Window: %s\n", str);
  delete[] str;

  str = aDefault.ToNewCString();
  printf("%cPrompt (default=%s): ", '\007', str);
  gets(buf);
  if (strlen(buf)) {
    aResult = buf;
  } else {
    aResult = aDefault;
  }
  delete[] str;
  
  return (aResult.Length() > 0);
}

//----------------------------------------
NS_IMETHODIMP_(PRBool) 
nsBrowserWindow::PromptUserAndPassword(const nsString &aText,
                     nsString &aUser,
                     nsString &aPassword)
{
  char *str;
  char buf[256];

  str = aText.ToNewCString();
  printf("Browser Window: %s\n", str);
  delete[] str;

  str = aUser.ToNewCString();
  printf("%cUser (default=%s): ", '\007', str);
  gets(buf);
  if (strlen(buf)) {
    aUser = buf;
  }
  delete[] str;

  str = aPassword.ToNewCString();
  printf("%cPassword (default=%s): ", '\007', str);
  gets(buf);
  if (strlen(buf)) {
    aPassword = buf;
  }
  delete[] str;
  
  return (aUser.Length() > 0);
}

//----------------------------------------
NS_IMETHODIMP_(PRBool) 
nsBrowserWindow::PromptPassword(const nsString &aText,
                nsString &aPassword)
{
  char *str;
  char buf[256];
  
  str = aText.ToNewCString();
  printf("Browser Window: %s\n", str);
  delete[] str;

  printf("%cPassword: ", '\007');
  gets(buf);
  aPassword = buf;
 
  return PR_TRUE;
}

//----------------------------------------
// Toolbar support
void
nsBrowserWindow::StartThrobber()
{
}

//----------------------------------------
void
nsBrowserWindow::StopThrobber()
{
}

//----------------------------------------
void
nsBrowserWindow::LoadThrobberImages()
{
}

//----------------------------------------
void
nsBrowserWindow::DestroyThrobberImages()
{
}

static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);

//----------------------------------------
nsIPresShell*
nsBrowserWindow::GetPresShell()
{
  nsIPresShell* shell = nsnull;
  if (nsnull != mWebShell) {
    nsIContentViewer* cv = nsnull;
    mWebShell->GetContentViewer(cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIPresContext* cx;
        docv->GetPresContext(cx);
        if (nsnull != cx) {
          shell = cx->GetShell();
          NS_RELEASE(cx);
        }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
  return shell;
}

//----------------------------------------
#ifdef WIN32
void PlaceHTMLOnClipboard(PRUint32 aFormat, char* aData, int aLength)
{
  HGLOBAL   hGlobalMemory;
  PSTR    pGlobalMemory;

  PRUint32  cf_aol = RegisterClipboardFormat(gsAOLFormat);
  PRUint32  cf_html = RegisterClipboardFormat(gsHTMLFormat);

  char*     preamble = "";
  char*     postamble = "";

  if (aFormat == cf_aol || aFormat == CF_TEXT)
  {
  preamble = "<HTML>";
  postamble = "</HTML>";
  }

  PRInt32 size = aLength + 1 + strlen(preamble) + strlen(postamble);


  if (aLength) {
    // Copy text to Global Memory Area
    hGlobalMemory = (HGLOBAL)GlobalAlloc(GHND, size);
    if (hGlobalMemory != NULL) {
      pGlobalMemory = (PSTR) GlobalLock(hGlobalMemory);

      int i;

      // AOL requires HTML prefix/postamble
      char*   s  = preamble;
      PRInt32   len = strlen(s); 
      for (i=0; i < len; i++) {
        *pGlobalMemory++ = *s++;
      }

      s  = aData;
      len = aLength;
      for (i=0;i< len;i++) {
        *pGlobalMemory++ = *s++;
      }


      s = postamble;
      len = strlen(s); 
      for (i=0; i < len; i++) {
        *pGlobalMemory++ = *s++;
      }
    
      // Put data on Clipboard
      GlobalUnlock(hGlobalMemory);
      SetClipboardData(aFormat, hGlobalMemory);
    }
  }  
}
#endif



//----------------------------------------
void
nsBrowserWindow::DoCopy()
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsIDocument* doc = shell->GetDocument();
    if (nsnull != doc) {
      nsString buffer;

      doc->CreateXIF(buffer,PR_TRUE);

      nsIParser* parser;

      static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
      static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

      nsresult rv = nsComponentManager::CreateInstance(kCParserCID, 
                           nsnull, 
                           kCParserIID, 
                           (void **)&parser);

      if (NS_OK == rv) {
      nsIHTMLContentSink* sink = nsnull;
  
      rv = NS_New_HTML_ContentSinkStream(&sink,PR_FALSE,PR_FALSE);

      ostrstream  data;
      ((nsHTMLContentSinkStream*)sink)->SetOutputStream(data);

      if (NS_OK == rv) {
        parser->SetContentSink(sink);
    
        nsIDTD* dtd = nsnull;
        rv = NS_NewXIFDTD(&dtd);
        if (NS_OK == rv) {
          parser->RegisterDTD(dtd); 
          //dtd->SetContentSink(sink);
          // dtd->SetParser(parser);
          parser->Parse(buffer, PR_FALSE);       
        }
        NS_IF_RELEASE(dtd);
        NS_IF_RELEASE(sink);
        char* str = data.str();

  #if defined(WIN32)
        PRUint32 cf_aol = RegisterClipboardFormat(gsAOLFormat);
        PRUint32 cf_html = RegisterClipboardFormat(gsHTMLFormat);
   
        PRInt32   len = data.pcount();
        if (len) {   
          OpenClipboard(NULL);
          EmptyClipboard();
  
          PlaceHTMLOnClipboard(cf_aol,str,len);
          PlaceHTMLOnClipboard(cf_html,str,len);
          PlaceHTMLOnClipboard(CF_TEXT,str,len);      
          
          CloseClipboard();
        }
        // in ostrstreams if you cal the str() function
        // then you are responsible for deleting the string
  #endif
        if (str) delete str;

      }
      NS_RELEASE(parser);
      }
      NS_RELEASE(doc);
    }
    NS_RELEASE(shell);
  }
}

//----------------------------------------------------------------------
void
nsBrowserWindow::DoJSConsole()
{
  mApp->CreateJSConsole(this);
}

//----------------------------------------
void
nsBrowserWindow::DoEditorMode(nsIWebShell *aWebShell)
{
  PRInt32 i, n;
  if (nsnull != aWebShell) 
  {
    nsIContentViewer *cViewer;
    aWebShell->GetContentViewer(cViewer);//returns an addreffed viewer  dereference it because it accepts a reference to a * not a **
    if (cViewer) 
    {
      nsIDocumentViewer *dViewer;
      if (NS_SUCCEEDED(cViewer->QueryInterface(kIDocumentViewerIID, (void **)&dViewer))) 
      { //returns an addreffed document viewer
        nsIDocument *doc;
        dViewer->GetDocument(doc); //returns an addreffed document
        if (doc) 
        {
          nsIDOMDocument *domDoc;
          if (NS_SUCCEEDED(doc->QueryInterface(kIDOMDocumentIID, (void **)&domDoc)))
          { //returns an addreffed domdocument
            nsIEditor *editor;
            if (NS_SUCCEEDED(nsComponentManager::CreateInstance(kEditorCID, nsnull, kIEditorIID, (void **)&editor)))
            {
              editor->Init(domDoc);
              AddEditor(editor); //new call to set the editor interface this will addref
              NS_IF_RELEASE(editor);
            }
            NS_IF_RELEASE(domDoc);
          }
          NS_IF_RELEASE(doc);
        }
        NS_IF_RELEASE(dViewer);
      }
      NS_IF_RELEASE(cViewer);
    }
    aWebShell->GetChildCount(n);
    for (i = 0; i < n; i++) {
      nsIWebShell *child;
      aWebShell->ChildAt(i, child);
      DoEditorMode(child); //doesnt addref
      NS_IF_RELEASE(child);
    }
  }
}



void
nsBrowserWindow::AddEditor(nsIEditor *aEditor)
{
  if (!mEditor)//THIS FUNCTION REALLY NEEDS HELP
    mEditor = aEditor;
}


#ifdef NS_DEBUG
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIStyleContext.h"
#include "nsISizeOfHandler.h"
#include "nsIStyleSet.h"


//----------------------------------------
void
nsBrowserWindow::DumpContent(FILE* out)
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
  nsIDocument* doc = shell->GetDocument();
  if (nsnull != doc) {
    nsIContent* root = doc->GetRootContent();
    if (nsnull != root) {
    root->List(out);
    NS_RELEASE(root);
    }
    NS_RELEASE(doc);
  }
  NS_RELEASE(shell);
  }
  else {
  fputs("null pres shell\n", out);
  }
}

//----------------------------------------------------
void
nsBrowserWindow::DumpFrames(FILE* out, nsString *aFilterName)
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsIFrame* root;
    shell->GetRootFrame(root);
    if (nsnull != root) {
      nsIListFilter *filter = nsIFrame::GetFilter(aFilterName);
      root->List(out, 0, filter);
    }
    NS_RELEASE(shell);
    }
  else {
    fputs("null pres shell\n", out);
  }
}

void
DumpViewsRecurse(nsBrowserWindow* aBrowser, nsIWebShell* aWebShell, FILE* out)
{
  if (nsnull != aWebShell) {
    nsIPresShell* shell = aBrowser->GetPresShell();
    if (nsnull != shell) {
      nsIViewManager* vm = shell->GetViewManager();
      if (nsnull != vm) {
        nsIView* root;
        vm->GetRootView(root);
        if (nsnull != root) {
          root->List(out);
        }
        NS_RELEASE(vm);
      }
      NS_RELEASE(shell);
    } else {
      fputs("null pres shell\n", out);
    }
    // dump the views of the sub documents
    PRInt32 i, n;
    aWebShell->GetChildCount(n);
    for (i = 0; i < n; i++) {
      nsIWebShell* child;
      aWebShell->ChildAt(i, child);
      if (nsnull != child) {
        DumpViewsRecurse(aBrowser, child, out);
      }
    }
  }
}

void
nsBrowserWindow::DumpViews(FILE* out)
{
  DumpViewsRecurse(this, mWebShell, out);
}

static void DumpAWebShell(nsIWebShell* aShell, FILE* out, PRInt32 aIndent)
{
  PRUnichar *name;
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

void
nsBrowserWindow::DumpStyleSheets(FILE* out)
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
  nsIStyleSet* styleSet = shell->GetStyleSet();
  if (nsnull == styleSet) {
    fputs("null style set\n", out);
  } else {
    styleSet->List(out);
    NS_RELEASE(styleSet);
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
  nsIStyleSet* styleSet = shell->GetStyleSet();
  if (nsnull == styleSet) {
    fputs("null style set\n", out);
  } else {
    nsIFrame* root;
    shell->GetRootFrame(root);
    if (nsnull == root) {
    fputs("null root frame\n", out);
    } else {
    nsIStyleContext* rootContext;
    root->GetStyleContext(rootContext);
    if (nsnull != rootContext) {
      styleSet->ListContexts(rootContext, out);
      NS_RELEASE(rootContext);
    }
    else {
      fputs("null root context", out);
    }
    }
    NS_RELEASE(styleSet);
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
  nsISizeOfHandler* szh;
  if (NS_OK != NS_NewSizeOfHandler(&szh)) {
  return;
  }

  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
  nsIDocument* doc = shell->GetDocument();
  if (nsnull != doc) {
    nsIContent* content = doc->GetRootContent();
    if (nsnull != content) {
    content->SizeOf(szh);
    PRUint32 totalSize;
    szh->GetSize(totalSize);
    printf("Content model size is approximately %d bytes\n", totalSize);
    NS_RELEASE(content);
    }
    NS_RELEASE(doc);
  }
  NS_RELEASE(shell);
  }
  NS_RELEASE(szh);
}

void
nsBrowserWindow::ShowFrameSize()
{
  nsIPresShell* shell0 = GetPresShell();
  if (nsnull != shell0) {
  nsIDocument* doc = shell0->GetDocument();
  if (nsnull != doc) {
    PRInt32 i, shells = doc->GetNumberOfShells();
    for (i = 0; i < shells; i++) {
    nsIPresShell* shell = doc->GetShellAt(i);
    if (nsnull != shell) {
      nsISizeOfHandler* szh;
      if (NS_OK != NS_NewSizeOfHandler(&szh)) {
      return;
      }
      nsIFrame* root;
      shell->GetRootFrame(root);
      if (nsnull != root) {
      root->SizeOf(szh);
      PRUint32 totalSize;
      szh->GetSize(totalSize);
      printf("Frame model for shell=%p size is approximately %d bytes\n",
           shell, totalSize);
      }
      NS_RELEASE(szh);
      NS_RELEASE(shell);
    }
    }
    NS_RELEASE(doc);
  }
  NS_RELEASE(shell0);
  }
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
  PRBool  doSave = PR_FALSE;
  nsString  path;

  PRUnichar *urlString;
  mWebShell->GetURL(0,&urlString);
  nsIURL* url;
  nsresult rv = NS_NewURL(&url, urlString);
  
  if (rv == NS_OK)
  {
  const char* name;
  (void)url->GetFile(&name);
  path = name;

  doSave = GetSaveFileNameFromFileSelector(mWindow, path);
  NS_RELEASE(url);

  }
  if (!doSave)
  return;


  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
  nsIDocument* doc = shell->GetDocument();
  if (nsnull != doc) {
    nsString buffer;

    doc->CreateXIF(buffer,PR_FALSE);

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
    ofstream  out(filename);
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
      parser->Parse(buffer, PR_FALSE);       
      }
      out.close();

      NS_IF_RELEASE(dtd);
      NS_IF_RELEASE(sink);
    }
    NS_RELEASE(parser);
    }
    NS_RELEASE(doc);
  }
  NS_RELEASE(shell);
  }
}

void 
nsBrowserWindow::DoToggleSelection()
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
  nsIDocument* doc = shell->GetDocument();
  if (nsnull != doc) {
    PRBool  current = doc->GetDisplaySelection();
    doc->SetDisplaySelection(!current);
    ForceRefresh();
    NS_RELEASE(doc);
  }
  NS_RELEASE(shell);
  }
}


void
nsBrowserWindow::DoDebugRobot()
{
  mApp->CreateRobot(this);
}

#endif NS_DEBUG

void
nsBrowserWindow::DoSiteWalker()
{
  mApp->CreateSiteWalker(this);
}

//-----------------------------------------------------
//-- Menu Struct
//-----------------------------------------------------
typedef struct _menuBtns {
  char * title;
  char * mneu;
  long   command;
} MenuBtns;


//-----------------------------------------------------
// XXX This will be moved to nsWidgetSupport
nsIMenu * CreateMenu(nsIMenu * aMenu, const nsString &  aName, char aMneu)
{
  nsIMenu * menu;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID,
                       nsnull,
                       kIMenuIID,
                       (void**)&menu);
  if (NS_OK == rv) {
  menu->Create(aMenu, aName);
  }

  return menu;
}

//-----------------------------------------------------
// XXX This will be moved to nsWidgetSupport
nsIMenu * CreateMenu(nsIMenuBar * aMenuBar, const nsString &  aName, char aMneu)
{
  nsIMenu * menu;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID,
                       nsnull,
                       kIMenuIID,
                       (void**)&menu);
  if (NS_OK == rv) {
  menu->Create(aMenuBar, aName);
  }

  return menu;
}

//-----------------------------------------------------
// XXX This will be moved to nsWidgetSupport
nsIMenuItem * CreateMenuItem(nsIMenu * aMenu, const nsString & aName, PRUint32 aCommand)
{
  nsIMenuItem * menuItem = nsnull;

  if (!aName.Equals("-")) {
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID,
                         nsnull,
                         kIMenuItemIID,
                         (void**)&menuItem);
  if (NS_OK == rv) {
    menuItem->Create(aMenu, aName, aCommand);
  }
  } else {
  aMenu->AddSeparator();
  }

  return menuItem;
}


//-----------------------------------------------------
void CreateBrowserMenus(nsIMenuBar * aMenuBar) 
{
  MenuBtns editMenus[] = {
  {"Cut",      "T",  VIEWER_EDIT_CUT},
  {"Copy",     "C",  VIEWER_EDIT_COPY},
  {"Paste",    "P",  VIEWER_EDIT_PASTE},
  {"-",      NULL, 0},
  {"Select All",   "A",  VIEWER_EDIT_SELECTALL},
  {"-",      NULL, 0},
  {"Find in Page", "F",  VIEWER_EDIT_FINDINPAGE},
  {NULL, NULL, 0}
  };

  MenuBtns  debugMenus[] = {
  {"Visual Debugging", "V", VIEWER_VISUAL_DEBUGGING},
  {"Reflow Test", "R", VIEWER_REFLOW_TEST},
  {"-", NULL, 0},
  {"Dump Content", "C", VIEWER_DUMP_CONTENT},
  {"Dump Frames",  "F", VIEWER_DUMP_FRAMES},
  {"Dump Views",   "V", VIEWER_DUMP_VIEWS},
  {"-", NULL, 0},
  {"Dump Style Sheets",   "S", VIEWER_DUMP_STYLE_SHEETS},
  {"Dump Style Contexts",   "T", VIEWER_DUMP_STYLE_CONTEXTS},
  {"-", NULL, 0},
  {"Show Content Size",   "z", VIEWER_SHOW_CONTENT_SIZE},
  {"Show Frame Size",   "a", VIEWER_SHOW_FRAME_SIZE},
  {"Show Style Size",   "y", VIEWER_SHOW_STYLE_SIZE},
  {"-", NULL, 0},
  {"Debug Save",   "v", VIEWER_DEBUGSAVE},
  {"Debug Toggle Selection",   "q", VIEWER_TOGGLE_SELECTION},
  {"-", NULL, 0},
  {"Debug Robot",   "R", VIEWER_DEBUGROBOT},
  {"-", NULL, 0},
  {"Show Content Quality",   ".", VIEWER_SHOW_CONTENT_QUALITY},
  {"Editor",   "E", EDITOR_MODE},
  {NULL, NULL, 0}
  };


  nsIMenu * fileMenu = CreateMenu(aMenuBar,  "File", 'F');

  CreateMenuItem(fileMenu, "New Window", VIEWER_WINDOW_OPEN);
  CreateMenuItem(fileMenu, "Open...",  VIEWER_FILE_OPEN);
  CreateMenuItem(fileMenu, "View Source", VIEWER_FILE_VIEW_SOURCE);

  nsIMenu * samplesMenu = CreateMenu(fileMenu, "Samples", 'S');
  PRInt32 i = 0;
  for (i=0;i<10;i++) {
  char buf[64];
  sprintf(buf, "Demo #%d", i);
  CreateMenuItem(samplesMenu, buf, VIEWER_DEMO0+i);
  }

  CreateMenuItem(fileMenu, "Test Sites", VIEWER_TOP100);

#ifdef NOT_YET
  nsIMenu * printMenu = CreateMenu(fileMenu, "Print Preview", 'P');
  CreateMenuItem(printMenu, "One Column", VIEWER_ONE_COLUMN);
  CreateMenuItem(printMenu, "Two Column", VIEWER_TWO_COLUMN);
  CreateMenuItem(printMenu, "Three Column", VIEWER_THREE_COLUMN);
#endif   /* NOT_YET */

  CreateMenuItem(fileMenu, "-", 0);
  CreateMenuItem(fileMenu, "Exit", VIEWER_EXIT);

#if EDITOR_NOT_YET
  nsIMenu * editMenu = CreateMenu(aMenuBar,  "Edit", 'E');
  i = 0;
  while (editMenus[i].title != nsnull) {
  CreateMenuItem(editMenu, editMenus[i].title, editMenus[i].command);
  i++;
  }
#endif

#ifdef NOT_YET
  nsIMenu * viewMenu = CreateMenu(aMenuBar,  "View", 'V');
  CreateMenuItem(viewMenu, "Show", 0);
  CreateMenuItem(viewMenu, "-", 0);
  CreateMenuItem(viewMenu, "Reload", 0);
  CreateMenuItem(viewMenu, "Refresh", 0);
#endif   /* NOT_YET  */

/*   nsIMenu * goMenu   = CreateMenu(aMenuBar,  "Go", 'G'); */
  nsIMenu * commMenu = CreateMenu(aMenuBar,  "Communicator", 'C');
  CreateMenuItem(commMenu, "Navigator", VIEWER_COMM_NAV);

  nsIMenu * helpMenu = CreateMenu(aMenuBar,  "Help", 'H');

#ifdef NS_DEBUG
  nsIMenu * debugMenu = CreateMenu(aMenuBar,  "Debug", 'D');
  i = 0;
  while (debugMenus[i].title != nsnull) {
  CreateMenuItem(debugMenu, debugMenus[i].title, debugMenus[i].command);
  i++;
  }
#endif 

  /*nsIMenu * toolsMenu = CreateMenu(aMenuBar,  "Tools", 'T');
  CreateMenuItem(toolsMenu, "Java Script Console", JS_CONSOLE);
  CreateMenuItem(toolsMenu, "Editor Mode", EDITOR_MODE);*/

}

//#endif // NS_DEBUG

//-----------------------------------------------------
nsresult
nsBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
  nsIMenuBar * menuBar;
  nsresult rv = nsComponentManager::CreateInstance(kMenuBarCID,
                       nsnull,
                       kIMenuBarIID,
                       (void**)&menuBar);

  if (nsnull != menuBar) {
  menuBar->Create(mWindow);
  CreateBrowserMenus(menuBar);
  }

  return NS_OK;
}

#ifdef NS_DEBUG

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
  DumpWebShells();
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
#if XXX_fix_me
  if ((nsnull != wd) && (nsnull != wd->observer)) {
    nsIPresContext *px = wd->observer->mWebWidget->GetPresContext();
    nsIPresShell   *ps = px->GetShell();
    nsIViewManager *vm = ps->GetViewManager();

    vm->ShowQuality(!vm->GetShowQuality());

    NS_RELEASE(vm);
    NS_RELEASE(ps);
    NS_RELEASE(px);
  }
#endif
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

//----------------------------------------------------------------------

// Factory code for creating nsBrowserWindow's

class nsBrowserWindowFactory : public nsIFactory
{
public:
  nsBrowserWindowFactory();
  ~nsBrowserWindowFactory();

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

  NS_NEWXPCOM(inst, nsBrowserWindow);
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

nsViewSourceWindow::nsViewSourceWindow( nsIAppShell     *anAppShell,
                                        nsIPref         *aPrefs,
                                        nsViewerApp     *anApp,
                                        const PRUnichar *aURL )
  : nsBrowserWindow() {

  // Wire the window to the "parent" application.
  this->SetApp(anApp);

  // "View Source" windows have none of this chrome.
  PRUint32 chromeMask = (PRUint32)~( NS_CHROME_TOOL_BAR_ON            // No toolbar.
                                     |
                                     NS_CHROME_LOCATION_BAR_ON        // No location bar.
                                     |
                                     NS_CHROME_PERSONAL_TOOLBAR_ON ); // No personal toolbar.

  // Initialize the window.
  this->Init(anAppShell, mPrefs, nsRect(0, 0, 620, 400), chromeMask, PR_FALSE);

  // Get the new window's web shell.
  nsIWebShell *pWebShell = 0;
  if( NS_OK == this->GetWebShell(pWebShell) ) {
    // Have the web shell load the URL's source.
    pWebShell->LoadURL(aURL,"view-source");

    // Set title (bypassing our override so it gets set for real).
    nsString title = "Source for ";
    title += aURL;
    this->nsBrowserWindow::SetTitle( title );

    // Make the window visible.
    this->Show();

    // Release the shell object.
    NS_RELEASE(pWebShell);
  } else {
    // Do our best to handle the error.
    this->Alert( "Unable to get web shell in nsViewSourceWindow ctor" );
  }

}

nsresult
nsViewSourceWindow::SetTitle( const PRUnichar * ) {
  // Swallow this request (to block the nsIWebShell from giving us a 
  // bogus title.
  return NS_OK;
}
