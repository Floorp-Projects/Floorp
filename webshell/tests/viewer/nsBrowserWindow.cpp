/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * ***** END LICENSE BLOCK *****
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

#include "nsIPref.h" 
#include "prmem.h"

#include "nsBrowserWindow.h"
#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsIButton.h"
#include "nsITimer.h"
#include "nsIDOMDocument.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsIDOMWindowInternal.h"
#include "nsIFilePicker.h"
#include "nsILookAndFeel.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"
#include "nsCRT.h"
#include "nsWidgetsCID.h"
#include "nsViewerApp.h"
#include "prprf.h"
#include "nsIComponentManager.h"
#include "nsParserCIID.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsLayoutCID.h"
#include "nsIDocumentViewer.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerFile.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsILayoutDebugger.h"
#include "nsThrobber.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowserFocus.h"
#include "nsIBaseWindow.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIViewManager.h"
#include "nsGUIEvent.h"
#include "nsIWebProgress.h"
#include "nsIWebBrowserSetup.h"
#include "nsIWebBrowserPrint.h"

#include "nsCWebBrowser.h"
#include "nsUnicharUtils.h"

#include "nsIParser.h"
#include "nsEditorMode.h"

// Needed for "Find" GUI
#include "nsICheckButton.h"
#include "nsILabel.h"
#include "nsWidgetSupport.h"

#include "nsXPBaseWindow.h"

#include "resources.h"

#if defined(WIN32)
#include <windows.h>
#endif

#include <ctype.h> // tolower

// For Copy
#include "nsISelection.h"
#include "nsISelectionController.h"

// XXX For font setting below
#include "nsFont.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"

#if defined(CookieManagement) || defined(SingleSignon) || defined(ClientWallet)
#include "nsIServiceManager.h"
#endif

#ifdef CookieManagement

#include "nsIURL.h"

#endif
#include "nsIIOService.h"
#include "nsNetCID.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#if defined(ClientWallet) || defined(SingleSignon)
#include "nsIWalletService.h"
static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_CID(kWalletServiceCID, NS_WALLETSERVICE_CID);
#endif

#ifdef PURIFY
#include "pure.h"
#endif

#define THROBBING_N

#define CLEANUP_WIDGET(_widget, _txt) \
  if(_widget) {            \
  DestroyWidget((_widget)); \
  refCnt = (_widget)->Release(); \
  (_widget) = nsnull; \
  NS_ASSERTION(refCnt == 0, (_txt)); \
  }


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
#define MAX_TEXT_LENGTH 30000

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

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_CID(kButtonCID, NS_BUTTON_CID);
static NS_DEFINE_CID(kTextFieldCID, NS_TEXTFIELD_CID);
static NS_DEFINE_CID(kWindowCID, NS_WINDOW_CID);

static NS_DEFINE_IID(kIXPBaseWindowIID, NS_IXPBASE_WINDOW_IID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kIButtonIID, NS_IBUTTON_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_CID(kXPBaseWindowCID, NS_XPBASE_WINDOW_CID);
static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

static NS_DEFINE_IID(kILayoutDebuggerIID, NS_ILAYOUT_DEBUGGER_IID);
static NS_DEFINE_CID(kLayoutDebuggerCID, NS_LAYOUT_DEBUGGER_CID);

#define FILE_PROTOCOL "file://"

#ifdef USE_LOCAL_WIDGETS
  extern nsresult NS_NewButton(nsIButton** aControl);
  extern nsresult NS_NewLabel(nsILabel** aControl);
  extern nsresult NS_NewTextWidget(nsITextWidget** aControl);
  extern nsresult NS_NewCheckButton(nsICheckButton** aControl);
#endif

//******* Cleanup Above here***********/

//*****************************************************************************
// nsBrowserWindow::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsBrowserWindow::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   // Ignore wigdet parents for now.  Don't think those are a vaild thing to call.
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, PR_FALSE), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::Create()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsBrowserWindow::Destroy()
{
  RemoveBrowser(this);

  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
  docShellWin->Destroy();
  mDocShell = nsnull;

  nsrefcnt refCnt;

  CLEANUP_WIDGET(mBack,     "nsBrowserWindow::Destroy - mBack is being leaked.");
  CLEANUP_WIDGET(mForward,  "nsBrowserWindow::Destroy - mForward is being leaked.");
  CLEANUP_WIDGET(mLocation, "nsBrowserWindow::Destroy - mLocation is being leaked.");
  CLEANUP_WIDGET(mStatus,   "nsBrowserWindow::Destroy - mStatus is being leaked.");

  if (mThrobber) {
    mThrobber->Destroy();
    refCnt = mThrobber->Release();
    mThrobber = nsnull;
    NS_ASSERTION(refCnt == 0, "nsBrowserWindow::Destroy - mThrobber is being leaked.");
    mThrobber = nsnull;
  }

  // Others are holding refs to this, 
  // but it gets released OK.
  DestroyWidget(mWindow);
  mWindow = nsnull;
  // NS_RELEASE(mWindow);

  return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::SetPosition(PRInt32 aX, PRInt32 aY)
{
   PRInt32 cx=0;
   PRInt32 cy=0;

   NS_ENSURE_SUCCESS(GetSize(&cx, &cy), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(SetPositionAndSize(aX, aY, cx, cy, PR_FALSE), 
      NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::GetPosition(PRInt32* aX, PRInt32* aY)
{
   return GetPositionAndSize(aX, aY, nsnull, nsnull);
}

NS_IMETHODIMP nsBrowserWindow::SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   PRInt32 x=0;
   PRInt32 y=0;

   NS_ENSURE_SUCCESS(GetPosition(&x, &y), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, aCX, aCY, aRepaint), 
      NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::GetSize(PRInt32* aCX, PRInt32* aCY)
{
   return GetPositionAndSize(nsnull, nsnull, aCX, aCY);
}

NS_IMETHODIMP nsBrowserWindow::SetPositionAndSize(PRInt32 aX, PRInt32 aY, 
   PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   NS_ENSURE_SUCCESS(mWindow->Resize(aX, aY, aCX, aCY, aRepaint), 
      NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::GetPositionAndSize(PRInt32* aX, PRInt32* aY, 
   PRInt32* aCX, PRInt32* aCY)
{
   nsRect bounds;

   NS_ENSURE_SUCCESS(mWindow->GetBounds(bounds), NS_ERROR_FAILURE);

   if(aX)
      *aX = bounds.x;
   if(aY)
      *aY = bounds.y;
   if(aCX)
      *aCX = bounds.width;
   if(aCY)
      *aCY = bounds.height;

   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::Repaint(PRBool aForce)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_ARG_POINTER(aParentWidget);
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsBrowserWindow::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsBrowserWindow::GetVisibility(PRBool* aVisibility)
{
   NS_ENSURE_ARG_POINTER(aVisibility);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::SetVisibility(PRBool aVisibility)
{
   NS_ENSURE_STATE(mWindow);

   NS_ENSURE_SUCCESS(mWindow->Show(aVisibility), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::GetEnabled(PRBool *aEnabled)
{
  NS_ENSURE_ARG_POINTER(aEnabled);
  *aEnabled = PR_TRUE;
  if (mWindow)
    mWindow->IsEnabled(aEnabled);
  return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::SetEnabled(PRBool aEnabled)
{
  if (mWindow)
    mWindow->Enable(aEnabled);
  return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::GetBlurSuppression(PRBool *aBlurSuppression)
{
  NS_ENSURE_ARG_POINTER(aBlurSuppression);
  *aBlurSuppression = PR_FALSE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsBrowserWindow::SetBlurSuppression(PRBool aBlurSuppression)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsBrowserWindow::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::SetFocus()
{
   //XXX First Check In
   //NS_WARNING("Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);

   *aTitle = ToNewUnicode(mTitle);

   return NS_OK;
}

NS_IMETHODIMP nsBrowserWindow::SetTitle(const PRUnichar* aTitle)
{
   NS_ENSURE_STATE(mWindow);

   mTitle = aTitle;

   NS_ENSURE_SUCCESS(mWindow->SetTitle(nsAutoString(aTitle)), NS_ERROR_FAILURE);

   return NS_OK;
}

//*****************************************************************************
// nsBrowserWindow: Helper Function
//*****************************************************************************   

void nsBrowserWindow::DestroyWidget(nsISupports* aWidget)
{
  if(aWidget) {
    nsCOMPtr<nsIWidget> w(do_QueryInterface(aWidget));
    if (w) {
      w->Destroy();
    }
  }
}

//******* Cleanup below here *************/




//----------------------------------------------------------------------

static
nsIPresShell*
GetPresShellFor(nsIDocShell* aDocShell)
{
  nsIPresShell* shell = nsnull;
  if (nsnull != aDocShell) {
    nsIContentViewer* cv = nsnull;
    aDocShell->GetContentViewer(&cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsCOMPtr<nsPresContext> cx;
        docv->GetPresContext(getter_AddRefs(cx));
        if (nsnull != cx) {
          NS_IF_ADDREF(shell = cx->GetPresShell());
        }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
  return shell;
}

nsVoidArray* nsBrowserWindow::gBrowsers = nsnull;

nsBrowserWindow*
nsBrowserWindow::FindBrowserFor(nsIWidget* aWidget, PRIntn aWhich)
{
  nsIWidget*        widget;
  nsBrowserWindow*  result = nsnull;

  if (!gBrowsers)
    return nsnull;

  PRInt32 i, n = gBrowsers->Count();
  for (i = 0; i < n; i++) {
    nsBrowserWindow* bw = (nsBrowserWindow*) gBrowsers->ElementAt(i);
    if (nsnull != bw) {
      switch (aWhich) {
      case FIND_WINDOW:
        if (bw->mWindow) {
          bw->mWindow->QueryInterface(kIWidgetIID, (void**) &widget);
          if (widget == aWidget) {
            result = bw;
          }
          NS_IF_RELEASE(widget);
        }
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
  if (!gBrowsers) {
    gBrowsers = new nsVoidArray();
    if (!gBrowsers)
      return;
  }
  gBrowsers->AppendElement(aBrowser);
  NS_ADDREF(aBrowser);
}

void
nsBrowserWindow::RemoveBrowser(nsBrowserWindow* aBrowser)
{
  //nsViewerApp* app = aBrowser->mApp;
  if (!gBrowsers)
    return;
  gBrowsers->RemoveElement(aBrowser);
  if (!gBrowsers->Count()) {
    delete gBrowsers;
    gBrowsers = nsnull;
  }
  NS_RELEASE(aBrowser);
}

void
nsBrowserWindow::CloseAllWindows()
{
  while (gBrowsers && gBrowsers->Count()) {
    nsBrowserWindow* bw = (nsBrowserWindow*) gBrowsers->ElementAt(0);
    NS_ADDREF(bw);
    bw->Destroy();
    NS_RELEASE(bw);
  }
  if (gBrowsers) {
    delete gBrowsers;
    gBrowsers = nsnull;
  }
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

    case NS_XUL_CLOSE:
    case NS_DESTROY:
    {
      nsViewerApp* app = bw->mApp;
      app->CloseWindow(bw);
      result = nsEventStatus_eConsumeDoDefault;

#ifndef XP_MAC
      // XXX Really shouldn't just exit, we should just notify somebody...
      if (!nsBrowserWindow::gBrowsers || !nsBrowserWindow::gBrowsers->Count()) {
        app->Exit();
      }
#endif
    }
    return result;

    case NS_MENU_SELECTED:
      result = bw->DispatchMenuItem(((nsMenuEvent*)aEvent)->mCommand);
      break;

    case NS_ACTIVATE:
      {
        nsCOMPtr<nsIWebBrowserFocus> focus = do_GetInterface(bw->mWebBrowser);
        if (focus)
          focus->Activate();
      }
      break;

    case NS_DEACTIVATE:
      {
        nsCOMPtr<nsIWebBrowserFocus> focus = do_GetInterface(bw->mWebBrowser);
        if (focus)
          focus->Deactivate();
      }
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
        bw->GoTo(text.get());
      }
      break;

    default:
      break;
    }
    NS_RELEASE(bw);
  }
  return result;
}

#ifdef PURIFY
static void
DispatchPurifyEvent(PRInt32 aID)
{
  if (!PurifyIsRunning()) {
      printf("!!! Re-run viewer under Purify to use this menu item.\n");
      fflush(stdout);
      return;
  }

  switch (aID) {
  case VIEWER_PURIFY_SHOW_NEW_LEAKS:
    PurifyPrintf("viewer: new leaks");
    PurifyNewLeaks();
    break;
  case VIEWER_PURIFY_SHOW_ALL_LEAKS:
    PurifyPrintf("viewer: all leaks");
    PurifyAllLeaks();
    break;
  case VIEWER_PURIFY_CLEAR_ALL_LEAKS:
    PurifyPrintf("viewer: clear leaks");
    PurifyClearLeaks();
    break;
  case VIEWER_PURIFY_SHOW_ALL_HANDLES_IN_USE:
    PurifyPrintf("viewer: all handles");
    PurifyAllHandlesInuse();
    break;
  case VIEWER_PURIFY_SHOW_NEW_IN_USE:
    PurifyPrintf("viewer: new in-use");
    PurifyNewInuse();
    break;
  case VIEWER_PURIFY_SHOW_ALL_IN_USE:
    PurifyPrintf("viewer: all in-use");
    PurifyAllInuse();
    break;
  case VIEWER_PURIFY_CLEAR_ALL_IN_USE:
    PurifyPrintf("viewer: clear in-use");
    PurifyClearInuse();
    break;
  case VIEWER_PURIFY_HEAP_VALIDATE:
    PurifyPrintf("viewer: heap validate");
    PurifyHeapValidate(PURIFY_HEAP_ALL, PURIFY_HEAP_BLOCKS_ALL, NULL);
    break;
  }
}
#endif

nsEventStatus
nsBrowserWindow::DispatchMenuItem(PRInt32 aID)
{
#if defined(CookieManagement) || defined(SingleSignon) || defined(ClientWallet)
  nsresult res;
#if defined(ClientWallet) || defined(SingleSignon)
  nsIWalletService *walletservice;
#endif
#ifdef ClientWallet
#define WALLET_EDITOR_URL "file:///y|/walleted.html"
  nsAutoString urlString; urlString.AssignWithConversion(WALLET_EDITOR_URL);
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
//      PRInt32 theIndex;
//      nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
//      webShell->GetHistoryIndex(theIndex);
//      nsXPIDLString theURL;
//      webShell->GetURL(theIndex, getter_Copies(theURL));
//      nsAutoString theString(theURL);
//      mApp->ViewSource(theString);
      //XXX Find out how the string is allocated, and perhaps delete it...
    }
    break;
  
  case VIEWER_EDIT_COPY:
    DoCopy();
    break;

  case VIEWER_EDIT_PASTE:
    DoPaste();
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
  case VIEWER_DEMO16:
  case VIEWER_DEMO17:
    {
      PRIntn ix = aID - VIEWER_DEMO0;
      nsAutoString url; url.AssignWithConversion(SAMPLES_BASE_URL);
      url.AppendLiteral("/test");
      url.AppendInt(ix, 10);
      url.AppendLiteral(".html");
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
      webNav->LoadURI(url.get(), nsIWebNavigation::LOAD_FLAGS_NONE, nsnull, nsnull, nsnull);
    }
    break;

  case VIEWER_XPTOOLKITTOOLBAR1:
    {
      nsAutoString url; url.AssignWithConversion(SAMPLES_BASE_URL);
      url.AppendLiteral("/toolbarTest1.xul");
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
      webNav->LoadURI(url.get(), nsIWebNavigation::LOAD_FLAGS_NONE, nsnull, nsnull, nsnull);
      break;
    }
  case VIEWER_XPTOOLKITTREE1:
    {
      nsAutoString url; url.AssignWithConversion(SAMPLES_BASE_URL);
      url.AppendLiteral("/treeTest1.xul");
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
      webNav->LoadURI(url.get(), nsIWebNavigation::LOAD_FLAGS_NONE, nsnull, nsnull, nsnull);
      break;
    }
  
  case JS_CONSOLE:
    DoJSConsole();
    break;

  case VIEWER_PREFS:
    DoPrefs();
    break;

  case EDITOR_MODE:
    DoEditorMode(mDocShell);
    break;

  case VIEWER_ONE_COLUMN:
  case VIEWER_TWO_COLUMN:
  case VIEWER_THREE_COLUMN:
    ShowPrintPreview(aID);
    break;

  case VIEWER_PRINT:
    DoPrint();
    break;

  case VIEWER_TABLE_INSPECTOR:
    DoTableInspector();
    break;

  case VIEWER_IMAGE_INSPECTOR:
    DoImageInspector();
    break;

  case VIEWER_GFX_SCROLLBARS_ON: {
    SetBoolPref("nglayout.widget.gfxscrollbars", PR_TRUE);
    nsAutoString text;
    PRUint32 size;
    mLocation->GetText(text, 1000, size);
    GoTo(text.get());
    }
    break;

  case VIEWER_GFX_SCROLLBARS_OFF: {
    SetBoolPref("nglayout.widget.gfxscrollbars", PR_FALSE);
    nsAutoString text;
    PRUint32 size;
    mLocation->GetText(text, 1000, size);
    GoTo(text.get());
    }
    break;

  case VIEWER_GOTO_TEST_URL1: 
  case VIEWER_GOTO_TEST_URL2: {
    nsAutoString urlStr;
    const char * pref = (aID == VIEWER_GOTO_TEST_URL1) ? "nglayout.widget.testurl1" : "nglayout.widget.testurl2";
    GetStringPref(pref, urlStr);
    PRUint32 size;
    mLocation->SetText(urlStr, size);
    GoTo(urlStr.get());

    }
    break;

  case VIEWER_SAVE_TEST_URL1: 
  case VIEWER_SAVE_TEST_URL2: {
    nsAutoString text;
    PRUint32 size;
    mLocation->GetText(text, 1000, size);
    const char * pref = (aID == VIEWER_SAVE_TEST_URL1) ? "nglayout.widget.testurl1" : "nglayout.widget.testurl2";
    SetStringPref(pref, text);
    }
    break;

#ifdef PURIFY
  case VIEWER_PURIFY_SHOW_NEW_LEAKS:
  case VIEWER_PURIFY_SHOW_ALL_LEAKS:
  case VIEWER_PURIFY_CLEAR_ALL_LEAKS:
  case VIEWER_PURIFY_SHOW_ALL_HANDLES_IN_USE:
  case VIEWER_PURIFY_SHOW_NEW_IN_USE:
  case VIEWER_PURIFY_SHOW_ALL_IN_USE:
  case VIEWER_PURIFY_CLEAR_ALL_IN_USE:
  case VIEWER_PURIFY_HEAP_VALIDATE:
    DispatchPurifyEvent(aID);
    break;
#endif

  case VIEWER_ZOOM_500:
  case VIEWER_ZOOM_300:
  case VIEWER_ZOOM_200:
  case VIEWER_ZOOM_100:
  case VIEWER_ZOOM_070:
  case VIEWER_ZOOM_050:
  case VIEWER_ZOOM_030:
  case VIEWER_ZOOM_020:
    mDocShell->SetZoom((aID - VIEWER_ZOOM_BASE) / 10.0f);
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
    nsString urlString2;
//    res = walletservice->WALLET_Prefill(shell, (PRVCY_QPREFILL == aID));
    NS_RELEASE(walletservice);
  }

#ifndef HTMLDialogs 
  if (aID == PRVCY_PREFILL) {
    nsAutoString url(NS_LITERAL_STRING("file:///y|/htmldlgs.htm"));
    nsBrowserWindow* bw = nsnull;
    mApp->OpenWindow(PRUint32(~0), bw);
    bw->SetVisibility(PR_TRUE);
    bw->GoTo(url.get());
    NS_RELEASE(bw);
  }
#endif
  break;

  case PRVCY_DISPLAY_WALLET:
  {


  /* set a cookie for the javascript wallet editor */
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    nsIURI * url;
    nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID, &res));
    if (NS_FAILED(res)) return nsEventStatus_eIgnore;

    nsIURI *uri = nsnull;
    res = service->NewURI(NS_LITERAL_CSTRING(WALLET_EDITOR_URL), nsnull, nsnull, &uri);
    if (NS_FAILED(res)) return nsEventStatus_eIgnore;

    res = uri->QueryInterface(NS_GET_IID(nsIURI), (void**)&url);
    NS_RELEASE(uri);
    if (NS_SUCCEEDED(res)) {
//      res = walletservice->WALLET_PreEdit(url);
      NS_RELEASE(walletservice);
    }
  }

  /* invoke the javascript wallet editor */
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
  webNav->LoadURI(urlString.get(), nsIWebNavigation::LOAD_FLAGS_NONE, nsnull, nsnull, nsnull);
  }
  break;
#endif

#if defined(CookieManagement)
  case PRVCY_DISPLAY_COOKIES:
  {
      break;
  }
#endif

#if defined(SingleSignon)
  case PRVCY_DISPLAY_SIGNONS:
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
//    res = walletservice->SI_DisplaySignonInfoAsHTML();
    NS_RELEASE(walletservice);
  }
  break;
#endif

  }

  // Any menu IDs that the editor uses will be processed here
  DoEditorTest(mDocShell, aID);

  return nsEventStatus_eIgnore;
}

void
nsBrowserWindow::Back()
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
  webNav->GoBack();
}

void
nsBrowserWindow::Forward()
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
  webNav->GoForward();
}

void
nsBrowserWindow::GoTo(const PRUnichar* aURL)
{
   nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
   webNav->LoadURI(aURL, nsIWebNavigation::LOAD_FLAGS_NONE, nsnull, nsnull, nsnull);
}


static PRBool GetFileFromFileSelector(nsIDOMWindowInternal* aParentWindow,
                                      nsILocalFile **aFile,
                                      nsILocalFile **aDisplayDirectory)
{
  nsresult rv;
  nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1");

  if (filePicker) {
    rv = filePicker->Init(aParentWindow, NS_LITERAL_STRING("Open HTML"),
                          nsIFilePicker::modeOpen);
    if (NS_SUCCEEDED(rv)) {
      filePicker->AppendFilters(nsIFilePicker::filterAll | nsIFilePicker::filterHTML |
                                nsIFilePicker::filterXML | nsIFilePicker::filterImages);
      if (*aDisplayDirectory)
        filePicker->SetDisplayDirectory(*aDisplayDirectory);
      
      PRInt16 dialogResult;
      rv = filePicker->Show(&dialogResult);
      if (NS_FAILED(rv) || dialogResult == nsIFilePicker::returnCancel)
        return PR_FALSE;

      filePicker->GetFile(aFile);
      if (*aFile) {
        NS_IF_RELEASE(*aDisplayDirectory);
        filePicker->GetDisplayDirectory(aDisplayDirectory);
        return PR_TRUE;
      }
    }
  }

  return PR_FALSE;
}

void
nsBrowserWindow::DoFileOpen()
{
  nsCOMPtr<nsILocalFile> file;
  nsCOMPtr<nsIDOMWindow> domWindow;
  nsCOMPtr<nsIDOMWindowInternal> parentWindow;
  nsresult rv;

  // get nsIDOMWindowInternal interface for nsIFilePicker
  rv = mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
  if (NS_SUCCEEDED(rv))
    parentWindow = do_QueryInterface(domWindow);

  if (GetFileFromFileSelector(parentWindow, getter_AddRefs(file),
                              getter_AddRefs(mOpenFileDirectory))) {
    nsCOMPtr<nsIURI> uri;
    NS_NewFileURI(getter_AddRefs(uri), file);
    if (uri) {
      nsCAutoString spec;
      uri->GetSpec(spec);
      
      // Ask the Web widget to load the file URL
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
      webNav->LoadURI(NS_ConvertUTF8toUCS2(spec).get(),
                      nsIWebNavigation::LOAD_FLAGS_NONE,
                      nsnull,
                      nsnull,
                      nsnull);
      SetVisibility(PR_TRUE);
    }
  }
}

#define DIALOG_FONT      "Helvetica"
#define DIALOG_FONT_SIZE 10

//---------------------------------------------------------------
NS_IMETHODIMP nsBrowserWindow::FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
  // Insert find code here.

  return NS_OK;
}

//---------------------------------------------------------------
NS_IMETHODIMP nsBrowserWindow::ForceRefresh()
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsIViewManager *vm = shell->GetViewManager();
    if (vm) {
      nsIView* root;
      vm->GetRootView(root);
      if (nsnull != root) {
        vm->UpdateView(root, NS_VMREFRESH_IMMEDIATE);
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
    } break;

    case NS_MOUSE_LEFT_BUTTON_UP: {
    } break;

    case NS_PAINT: 
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
}


//----------------------------------------------------------------------

#define VIEWER_BUNDLE_URL NS_LITERAL_CSTRING("resource:/res/viewer.properties")

static nsString* gTitleSuffix = nsnull;

#if XXX
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
  nsIURI* url = nsnull;
    nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID, &ret));
    if (NS_FAILED(ret)) return ret;

    nsIURI *uri = nsnull;
    ret = service->NewURI(VIEWER_BUNDLE_URL, nsnull, nsnull, &uri);
    if (NS_FAILED(ret)) return ret;

    ret = uri->QueryInterface(NS_GET_IID(nsIURI), (void**)&url);
    NS_RELEASE(uri);
  if (NS_FAILED(ret)) {
    NS_RELEASE(service);
    return suffix;
  }
  nsIStringBundle* bundle = nsnull;
  ret = service->CreateBundle(url, &bundle);
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
#endif

// Note: operator new zeros our memory
nsBrowserWindow::nsBrowserWindow()
{
  if (!gTitleSuffix) {
#if XXX
    gTitleSuffix = GetTitleSuffix();
#endif
    if ( (gTitleSuffix = new nsString) != 0 )
      gTitleSuffix->AssignLiteral(" - Raptor");
  }
  AddBrowser(this);
}

nsBrowserWindow::~nsBrowserWindow()
{
  NS_IF_RELEASE(mAppShell);
  NS_IF_RELEASE(mTableInspectorDialog);
  NS_IF_RELEASE(mImageInspectorDialog);
  NS_IF_RELEASE(mWebCrawler);

  if (nsnull != mTableInspector) {
    delete mTableInspector;
  }
  if (nsnull != mImageInspector) {
    delete mImageInspector;
  }
  if (mWebBrowserChrome) {
    mWebBrowserChrome->BrowserWindow(nsnull);
    NS_RELEASE(mWebBrowserChrome);
  }
  if (nsnull != gTitleSuffix) {
    delete gTitleSuffix;
    gTitleSuffix = nsnull;
  }
}

NS_IMPL_ISUPPORTS4(nsBrowserWindow, nsIBaseWindow, nsIInterfaceRequestor, nsIProgressEventSink, nsIWebShellContainer)

nsresult
nsBrowserWindow::GetInterface(const nsIID& aIID,
                              void** aInstancePtrResult)
{
  nsresult rv;

  NS_PRECONDITION(aInstancePtrResult, "null pointer");
  if (!aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  *aInstancePtrResult = NULL;

  if (aIID.Equals(NS_GET_IID(nsIWebBrowserChrome))) {
    rv = EnsureWebBrowserChrome();
    if (NS_SUCCEEDED(rv))
      return mWebBrowserChrome->QueryInterface(aIID, aInstancePtrResult);
  }

  return QueryInterface(aIID, aInstancePtrResult);
}

nsresult
nsBrowserWindow::Init(nsIAppShell* aAppShell,
                      const nsRect& aBounds,
                      PRUint32 aChromeMask,
                      PRBool aAllowPlugins)
{
  mChromeMask = aChromeMask;
  mAppShell = aAppShell;
  NS_IF_ADDREF(mAppShell);
  mAllowPlugins = aAllowPlugins;

  // Create top level window
  nsresult rv = nsComponentManager::CreateInstance(kWindowCID, nsnull,
                                                   kIWidgetIID,
                                                   getter_AddRefs(mWindow));
  if (NS_OK != rv) {
    return rv;
  }
  nsWidgetInitData initData;
  initData.mWindowType = eWindowType_toplevel;
  initData.mBorderStyle = eBorderStyle_default;

  nsRect r(0, 0, aBounds.width, aBounds.height);
  mWindow->Create((nsIWidget*)NULL, r, HandleBrowserEvent,
                  nsnull, aAppShell, nsnull, &initData);
  mWindow->GetClientBounds(r);

  // Create web shell
  mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
  NS_ENSURE_TRUE(mWebBrowser, NS_ERROR_FAILURE);

  r.x = r.y = 0;
  nsCOMPtr<nsIBaseWindow> webBrowserWin(do_QueryInterface(mWebBrowser));
  rv = webBrowserWin->InitWindow(mWindow->GetNativeData(NS_NATIVE_WIDGET), nsnull, r.x, r.y, r.width, r.height);
  NS_ENSURE_SUCCESS(EnsureWebBrowserChrome(), NS_ERROR_FAILURE);
  mWebBrowser->SetContainerWindow(mWebBrowserChrome);
  nsWeakPtr weakling(
      do_GetWeakReference((nsIWebProgressListener*)mWebBrowserChrome));
  mWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsIWebProgressListener));

  webBrowserWin->Create();

  // Give the embedding app a chance to do platforms-specific window setup
  InitNativeWindow();

  // Disable global history because we don't have profile-relative file locations
  nsCOMPtr<nsIWebBrowserSetup> setup(do_QueryInterface(mWebBrowser));
  if (setup)
    setup->SetProperty(nsIWebBrowserSetup::SETUP_USE_GLOBAL_HISTORY, PR_FALSE);

  mDocShell = do_GetInterface(mWebBrowser);
  mDocShell->SetAllowPlugins(aAllowPlugins);
  nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
  webShell->SetContainer((nsIWebShellContainer*) this);
  webBrowserWin->SetVisibility(PR_TRUE);

  if (nsIWebBrowserChrome::CHROME_MENUBAR & aChromeMask) {
    rv = CreateMenuBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
    mHaveMenuBar = PR_TRUE;
    mWindow->GetClientBounds(r);
    r.x = r.y = 0;
  }

  if (nsIWebBrowserChrome::CHROME_TOOLBAR & aChromeMask) {
    rv = CreateToolBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
  }

  if (nsIWebBrowserChrome::CHROME_STATUSBAR & aChromeMask) {
    rv = CreateStatusBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
  }

  // Now lay it all out
  Layout(r.width, r.height);

  return NS_OK;
}

nsresult
nsBrowserWindow::Init(nsIAppShell* aAppShell,
                      const nsRect& aBounds,
                      PRUint32 aChromeMask,
                      PRBool aAllowPlugins,
                      nsIDocumentViewer* aDocumentViewer,
                      nsPresContext* aPresContext)
{
  mChromeMask = aChromeMask;
  mAppShell = aAppShell;
  NS_IF_ADDREF(mAppShell);
  mAllowPlugins = aAllowPlugins;
  // Create top level window
  nsresult rv = nsComponentManager::CreateInstance(kWindowCID,
nsnull,
                                                  
kIWidgetIID,
                                                  
getter_AddRefs(mWindow));
  if (NS_OK != rv) {
    return rv;
  }
  nsWidgetInitData initData;
  initData.mWindowType = eWindowType_toplevel;
  initData.mBorderStyle = eBorderStyle_default;
  nsRect r(0, 0, aBounds.width, aBounds.height);
  mWindow->Create((nsIWidget*)NULL, r, HandleBrowserEvent,
                 
nsnull, aAppShell, nsnull, &initData);
  mWindow->GetClientBounds(r);
  // Create web shell
  mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
  NS_ENSURE_TRUE(mWebBrowser, NS_ERROR_FAILURE);
  r.x = r.y = 0;
  nsCOMPtr<nsIBaseWindow> webBrowserWin(do_QueryInterface(mWebBrowser));
  rv = webBrowserWin->InitWindow(mWindow->GetNativeData(NS_NATIVE_WIDGET),
nsnull, r.x, r.y, r.width, r.height);
  NS_ENSURE_SUCCESS(EnsureWebBrowserChrome(), NS_ERROR_FAILURE);
  mWebBrowser->SetContainerWindow(mWebBrowserChrome);
  webBrowserWin->Create();
  mDocShell = do_GetInterface(mWebBrowser);
  mDocShell->SetAllowPlugins(aAllowPlugins);
  nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
  webShell->SetContainer((nsIWebShellContainer*) this);
  webBrowserWin->SetVisibility(PR_TRUE);
  if (nsIWebBrowserChrome::CHROME_MENUBAR & aChromeMask) {
    rv = CreateMenuBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
    mWindow->GetClientBounds(r);
    r.x = r.y = 0;
  }
  if (nsIWebBrowserChrome::CHROME_TOOLBAR & aChromeMask) {
    rv = CreateToolBar(r.width);
    if (NS_OK != rv) {
      return rv;
    }
  }
  if (nsIWebBrowserChrome::CHROME_STATUSBAR & aChromeMask)
{
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
  nsCOMPtr<nsIDocumentViewer> docv;
  aDocumentViewer->CreateDocumentViewerUsing(aPresContext,
                                             getter_AddRefs(docv));
  docv->SetContainer(mWebBrowser);
  nsCOMPtr<nsIContentViewerContainer> cvContainer = do_QueryInterface(mDocShell);
  cvContainer->Embed(docv, "duh", nsnull);

  webBrowserWin->SetVisibility(PR_TRUE);

  return NS_OK;
}

void
nsBrowserWindow::SetWebCrawler(nsWebCrawler* aCrawler)
{
  if (mWebCrawler) {
    if (mDocShell) {
      nsCOMPtr<nsIWebProgress> progress(do_GetInterface(mDocShell));
      NS_ASSERTION(progress, "no web progress avail");

      (void) progress->RemoveProgressListener(
                                (nsIWebProgressListener*)mWebCrawler);
    }
    NS_RELEASE(mWebCrawler);
  }
  if (aCrawler) {
    mWebCrawler = aCrawler;
    NS_ADDREF(aCrawler);
  }
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
  t2d = dc->TwipsToDevUnits();
  float d2a;
  d2a = dc->DevUnitsToAppUnits();
  nsFont font(TOOL_BAR_FONT, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
              NS_FONT_WEIGHT_NORMAL, 0,
              nscoord(NSIntPointsToTwips(TOOL_BAR_FONT_SIZE) * t2d * d2a));
  NS_RELEASE(dc);

#ifdef USE_LOCAL_WIDGETS
  rv = NS_NewButton(&mBack);
#else
  // Create and place back button
  rv = nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID,
                                          (void**)&mBack);
#endif

  if (NS_OK != rv) {
    return rv;
  }
  nsRect r(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT);

  nsIWidget* widget = nsnull;
  NS_CreateButton(mWindow,mBack,r,HandleBackEvent,&font);
  if (NS_OK == mBack->QueryInterface(kIWidgetIID,(void**)&widget))
  {
    nsAutoString back(NS_LITERAL_STRING("Back"));
    mBack->SetLabel(back);
    NS_RELEASE(widget);
  }


  // Create and place forward button
  r.SetRect(BUTTON_WIDTH, 0, BUTTON_WIDTH, BUTTON_HEIGHT);
  
#ifdef USE_LOCAL_WIDGETS
  rv = NS_NewButton(&mForward);
#else
  rv = nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID,
                                          (void**)&mForward);
#endif
  if (NS_OK != rv) {
    return rv;
  }
  if (NS_OK == mForward->QueryInterface(kIWidgetIID,(void**)&widget))
  {
    widget->Create(mWindow.get(), r, HandleForwardEvent, NULL);
    widget->SetFont(font);
    widget->Show(PR_TRUE);
    nsAutoString forward(NS_LITERAL_STRING("Forward"));
    mForward->SetLabel(forward);
    NS_RELEASE(widget);
  }


  // Create and place location bar
  r.SetRect(2*BUTTON_WIDTH, 0,
            aWidth - 2*BUTTON_WIDTH - THROBBER_WIDTH,
            BUTTON_HEIGHT);

#ifdef USE_LOCAL_WIDGETS
  rv = NS_NewTextWidget(&mLocation);
#else
  rv = nsComponentManager::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID,
                                          (void**)&mLocation);
#endif

  if (NS_OK != rv) {
    return rv;
  }

  NS_CreateTextWidget(mWindow,mLocation,r,HandleLocationEvent,&font);
  if (NS_OK == mLocation->QueryInterface(kIWidgetIID,(void**)&widget))
  { 
    widget->SetForegroundColor(NS_RGB(0, 0, 0));
    widget->SetBackgroundColor(NS_RGB(255, 255, 255));
    PRUint32 size;
    nsAutoString empty;
    mLocation->SetText(empty, size);
    mLocation->SetMaxTextLength(MAX_TEXT_LENGTH);
    NS_RELEASE(widget);
  }

  // Create and place throbber
  r.SetRect(aWidth - THROBBER_WIDTH, 0,
            THROBBER_WIDTH, THROBBER_HEIGHT);
  mThrobber = nsThrobber::NewThrobber();
  nsString throbberURL; throbberURL.AssignWithConversion(THROBBER_AT);
  mThrobber->Init(mWindow, r, throbberURL, THROB_NUM);
  mThrobber->Show();
  return NS_OK;
}

// Overload this method in your nsNativeBrowserWindow if you need to 
// have the logic in nsBrowserWindow::Layout() offset the menu within
// the parent window.
nsresult
nsBrowserWindow::GetMenuBarHeight(PRInt32 * aHeightOut)
{
  NS_ASSERTION(nsnull != aHeightOut,"null out param.");

  *aHeightOut = 0;

  return NS_OK;
}

nsresult
nsBrowserWindow::CreateStatusBar(PRInt32 aWidth)
{
  nsresult rv;

  nsIDeviceContext* dc = mWindow->GetDeviceContext();
  float t2d;
  t2d = dc->TwipsToDevUnits();
  nsFont font(STATUS_BAR_FONT, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
              NS_FONT_WEIGHT_NORMAL, 0,
              nscoord(t2d * NSIntPointsToTwips(STATUS_BAR_FONT_SIZE)));
  NS_RELEASE(dc);

  nsRect r(0, 0, aWidth, THROBBER_HEIGHT);

#ifdef USE_LOCAL_WIDGETS
  rv = NS_NewTextWidget(&mStatus);
#else
  rv = nsComponentManager::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID,
                                          (void**)&mStatus);
#endif

  if (NS_OK != rv) {
    return rv;
  }

  nsIWidget* widget = nsnull;
  NS_CreateTextWidget(mWindow,mStatus,r,HandleLocationEvent,&font);
  if (NS_OK == mStatus->QueryInterface(kIWidgetIID,(void**)&widget))
  {
    widget->SetForegroundColor(NS_RGB(0, 0, 0));
    PRUint32 size;
    mStatus->SetText(EmptyString(),size);
    mStatus->SetMaxTextLength(MAX_TEXT_LENGTH);

    nsITextWidget* textWidget = nsnull;
    if (NS_OK == mStatus->QueryInterface(kITextWidgetIID,(void**)&textWidget))
    {
      PRBool  wasReadOnly;
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
  nscoord menuBarHeight;
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
    lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TextFieldHeight, txtHeight);
    NS_RELEASE(lookAndFeel);
  } else {
    txtHeight = 24;
  }

  // Find out the menubar height
  GetMenuBarHeight(&menuBarHeight);

  nsRect rr(0, 0, aWidth, aHeight);

  // position location bar (it's stretchy)
  if (nsIWebBrowserChrome::CHROME_TOOLBAR & mChromeMask) {
    nsIWidget* locationWidget = nsnull;
    if (mLocation &&
        NS_SUCCEEDED(mLocation->QueryInterface(kIWidgetIID,
                                               (void**)&locationWidget))) {
      if (mThrobber) {
        PRInt32 width = PR_MAX(aWidth - (2*BUTTON_WIDTH + THROBBER_WIDTH), 0);
      
        locationWidget->Resize(2*BUTTON_WIDTH, menuBarHeight,
                               width,
                               BUTTON_HEIGHT,
                               PR_TRUE);
        mThrobber->MoveTo(aWidth - THROBBER_WIDTH, menuBarHeight);
      }
      else {
        PRInt32 width = PR_MAX(aWidth - 2*BUTTON_WIDTH, 0);
        locationWidget->Resize(2*BUTTON_WIDTH, menuBarHeight,
                               width,
                               BUTTON_HEIGHT,
                               PR_TRUE);
      }

      locationWidget->Show(PR_TRUE);
      NS_RELEASE(locationWidget);

      nsIWidget* w = nsnull;
      if (mBack &&
          NS_SUCCEEDED(mBack->QueryInterface(kIWidgetIID, (void**)&w))) {
        w->Move(0, menuBarHeight);
        w->Show(PR_TRUE);
        NS_RELEASE(w);
      }
      if (mForward &&
          NS_SUCCEEDED(mForward->QueryInterface(kIWidgetIID, (void**)&w))) {
        w->Move(BUTTON_WIDTH, menuBarHeight);
        w->Show(PR_TRUE);
        NS_RELEASE(w);
      }
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
      w->Move(0, menuBarHeight);
      w->Show(PR_FALSE);
      NS_RELEASE(w);
    }
    if (mForward &&
        NS_SUCCEEDED(mForward->QueryInterface(kIWidgetIID, (void**)&w))) {
      w->Move(BUTTON_WIDTH, menuBarHeight);
      w->Show(PR_FALSE);
      NS_RELEASE(w);
    }
    if (mThrobber) {
      mThrobber->Hide();
    }
  }
  
  nsIWidget* statusWidget = nsnull;

  if (mStatus && NS_OK == mStatus->QueryInterface(kIWidgetIID,(void**)&statusWidget)) {
    if (mChromeMask & nsIWebBrowserChrome::CHROME_STATUSBAR) {
      statusWidget->Resize(0, aHeight - txtHeight,
                           aWidth, txtHeight,
                           PR_TRUE);

      //Since allowing a negative height is a bad idea, let's condition this...
      rr.height = PR_MAX(0,rr.height-txtHeight);
      statusWidget->Show(PR_TRUE);
    }
    else {
      statusWidget->Show(PR_FALSE);
    }
    NS_RELEASE(statusWidget);
  }

  // inset the web widget

  if (nsIWebBrowserChrome::CHROME_TOOLBAR & mChromeMask) {
    rr.height -= BUTTON_HEIGHT;
    rr.y += BUTTON_HEIGHT;
  }

  rr.x += WEBSHELL_LEFT_INSET;
  rr.y += WEBSHELL_TOP_INSET + menuBarHeight;
  rr.width -= WEBSHELL_LEFT_INSET + WEBSHELL_RIGHT_INSET;
  rr.height -= WEBSHELL_TOP_INSET + WEBSHELL_BOTTOM_INSET + menuBarHeight;

  //Since allowing a negative height is a bad idea, let's condition this...
  if(rr.height<0)
    rr.height=0;

  nsCOMPtr<nsIBaseWindow> webBrowserWin(do_QueryInterface(mWebBrowser));
  if (webBrowserWin) {
    webBrowserWin->SetPositionAndSize(rr.x, rr.y, rr.width, rr.height, PR_FALSE);
  }
}

NS_IMETHODIMP
nsBrowserWindow::MoveTo(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");
  mWindow->Move(aX, aY);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::SizeContentTo(PRInt32 aWidth, PRInt32 aHeight)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");

  // XXX We want to do this in one shot
  mWindow->Resize(aWidth, aHeight, PR_FALSE);
  Layout(aWidth, aHeight);

  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::SizeWindowTo(PRInt32 aWidth, PRInt32 aHeight,
                              PRBool /*aWidthTransient*/,
                              PRBool /*aHeightTransient*/)
{
  return SizeContentTo(aWidth, aHeight);
}

NS_IMETHODIMP
nsBrowserWindow::GetContentBounds(nsRect& aBounds)
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
  nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
  aResult = webShell;
  NS_IF_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::GetContentWebShell(nsIWebShell **aResult)
{
  nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
  *aResult = webShell;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

//----------------------------------------
NS_IMETHODIMP
nsBrowserWindow::SetProgress(PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::ShowMenuBar(PRBool aShow)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL,
                             nsLoadType aReason)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
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
                            nsresult aStatus)
{  
   return NS_OK;
}


NS_IMETHODIMP
nsBrowserWindow::NewWebShell(PRUint32 aChromeMask,
                             PRBool aVisible,
                             nsIWebShell*& aNewWebShell)
{
  nsresult rv = NS_OK;

  if (mWebCrawler) {
    if (mWebCrawler->Crawling() || mWebCrawler->LoadingURLList()) {
      // Do not fly javascript popups when we are crawling
      aNewWebShell = nsnull;
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  }

  // Create new window. By default, the refcnt will be 1 because of
  // the registration of the browser window in gBrowsers.
  nsNativeBrowserWindow* browser;
  NS_NEWXPCOM(browser, nsNativeBrowserWindow);

  if (nsnull != browser)
  {
    nsRect  bounds;
    GetContentBounds(bounds);

    browser->SetApp(mApp);

    // Assume no controls for now
    rv = browser->Init(mAppShell, bounds, aChromeMask, mAllowPlugins);
    if (NS_OK == rv)
    {
      // Default is to startup hidden
      if (aVisible) {
        browser->SetVisibility(PR_TRUE);
      }
      nsIWebShell *shell;
      rv = browser->GetWebShell(shell);
      aNewWebShell = shell;
    }
    else
    {
      browser->Destroy();
    }
  }
  else
    rv = NS_ERROR_OUT_OF_MEMORY;

  return rv;
}


NS_IMETHODIMP
nsBrowserWindow::ContentShellAdded(nsIWebShell* aChildShell, nsIContent* frameNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult)
{
  PRInt32 i, n = gBrowsers ? gBrowsers->Count() : 0;

  aResult = nsnull;
  nsString aNameStr(aName);

  for (i = 0; i < n; i++) {
    nsBrowserWindow* bw = (nsBrowserWindow*) gBrowsers->ElementAt(i);
    nsCOMPtr<nsIWebShell> webShell;
    
    if (NS_OK == bw->GetWebShell(*getter_AddRefs(webShell))) {
      nsXPIDLString name;
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
      if (NS_OK == docShellAsItem->GetName(getter_Copies(name))) {
        if (aNameStr.Equals(name)) {
          aResult = webShell;
          NS_ADDREF(aResult);
          return NS_OK;
        }
      }      

      nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(mDocShell));

      nsCOMPtr<nsIDocShellTreeItem> result;
      if (NS_OK == docShellAsNode->FindChildWithName(aName, PR_TRUE, PR_FALSE, nsnull,
         getter_AddRefs(result))) {
        if (result) {
          CallQueryInterface(result, &aResult);
          return NS_OK;
        }
      }
    }
  }
  return NS_OK;
}


//----------------------------------------

NS_IMETHODIMP
nsBrowserWindow::OnProgress(nsIRequest* request, nsISupports *ctxt,
                            PRUint32 aProgress, PRUint32 aProgressMax)
{
  nsresult rv;

  nsCOMPtr<nsIURI> aURL;
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;
  
  if (mStatus) {
    nsAutoString url;
    if (nsnull != aURL) {
      nsCAutoString str;
      aURL->GetSpec(str);
      AppendUTF8toUTF16(str, url);
    }
    url.AppendLiteral(": progress ");
    url.AppendInt(aProgress, 10);
    if (0 != aProgressMax) {
      url.AppendLiteral(" (out of ");
      url.AppendInt(aProgressMax, 10);
      url.Append(NS_LITERAL_STRING(")"));
    }
    PRUint32 size;
    mStatus->SetText(url,size);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserWindow::OnStatus(nsIRequest* request, nsISupports *ctxt,
                          nsresult aStatus, const PRUnichar *aStatusArg)
{
  if (mStatus) {
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> sbs = do_GetService(kStringBundleServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLString msg;
    rv = sbs->FormatStatusMessage(aStatus, aStatusArg, getter_Copies(msg));
    if (NS_FAILED(rv)) return rv;
    PRUint32 size;
    nsAutoString msg2( NS_STATIC_CAST(const PRUnichar*, msg));
    mStatus->SetText(msg2, size);
  }
  return NS_OK;
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
  return GetPresShellFor(mDocShell);
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
  nsCOMPtr <nsIContentViewer> viewer;

  mDocShell->GetContentViewer(getter_AddRefs(viewer));

  if (viewer)
  {
    nsCOMPtr<nsIContentViewerFile> viewerFile = do_QueryInterface(viewer);
    if (viewerFile) {
      //viewerFile->PrintPreview(nsnull);
    }
  }
}

void nsBrowserWindow::DoPrint(void)
{
  nsCOMPtr <nsIContentViewer> viewer;

  mDocShell->GetContentViewer(getter_AddRefs(viewer));

  if (viewer) {
    nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint = do_QueryInterface(viewer);
    if (webBrowserPrint) {
      if (!mPrintSettings) {
        webBrowserPrint->GetGlobalPrintSettings(getter_AddRefs(mPrintSettings));
      }
      webBrowserPrint->Print(mPrintSettings, (nsIWebProgressListener*)nsnull);
    }
  }
}

//---------------------------------------------------------------
nsIDOMDocument* nsBrowserWindow::GetDOMDocument(nsIDocShell *aDocShell)
{
  nsIDOMDocument* domDoc = nsnull;
  if (nsnull != aDocShell) {
    nsIContentViewer* mCViewer;
    aDocShell->GetContentViewer(&mCViewer);
    if (nsnull != mCViewer) {
      nsIDocumentViewer* mDViewer;
      if (NS_OK == mCViewer->QueryInterface(kIDocumentViewerIID, (void**) &mDViewer)) {
        nsCOMPtr<nsIDocument> mDoc;
        mDViewer->GetDocument(getter_AddRefs(mDoc));
        if (nsnull != mDoc) {
          mDoc->QueryInterface(kIDOMDocumentIID, (void**) &domDoc);
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
  nsIDOMDocument* domDoc = GetDOMDocument(mDocShell);

  if (nsnull != domDoc) {
    nsString printHTML(NS_LITERAL_STRING("resource:/res/samples/printsetup.html"));
    nsRect rect(0, 0, 375, 510);
    nsString title(NS_LITERAL_STRING("Table Inspector"));

    nsXPBaseWindow * xpWin = nsnull;
    nsresult rv = nsComponentManager::CreateInstance(kXPBaseWindowCID, nsnull,
                                                     kIXPBaseWindowIID,
                                                     (void**) &xpWin);
    if (rv == NS_OK) {
      xpWin->Init(eXPBaseWindowType_dialog, mAppShell, printHTML, title, rect, PRUint32(~0), PR_FALSE);
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

  nsIDOMDocument* domDoc = GetDOMDocument(mDocShell);

  if (nsnull != domDoc) {
    nsString printHTML(NS_LITERAL_STRING("resource:/res/samples/image_props.html"));
    nsRect rect(0, 0, 485, 124);
    nsString title(NS_LITERAL_STRING("Image Inspector"));

    nsXPBaseWindow * xpWin = nsnull;
    nsresult rv = nsComponentManager::CreateInstance(kXPBaseWindowCID, nsnull, kIXPBaseWindowIID, (void**) &xpWin);
    if (rv == NS_OK) {
      xpWin->Init(eXPBaseWindowType_dialog, mAppShell, printHTML, title, rect, PRUint32(~0), PR_FALSE);
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
nsBrowserWindow::DoEditorMode(nsIDocShell *aDocShell)
{
  PRInt32 i, n;
  if (nsnull != aDocShell) {
    nsIContentViewer* mCViewer;
    aDocShell->GetContentViewer(&mCViewer);
    if (nsnull != mCViewer) {
      nsIDocumentViewer* mDViewer;
      if (NS_OK == mCViewer->QueryInterface(kIDocumentViewerIID, (void**) &mDViewer)) 
      {
        nsCOMPtr<nsIDocument> mDoc;
        mDViewer->GetDocument(getter_AddRefs(mDoc));
        if (nsnull != mDoc) {
          nsIDOMDocument* mDOMDoc;
          if (NS_OK == mDoc->QueryInterface(kIDOMDocumentIID, (void**) &mDOMDoc)) 
          {
            nsIPresShell* shell = GetPresShellFor(aDocShell);
            NS_InitEditorMode(mDOMDoc, shell);
            NS_RELEASE(mDOMDoc);
            NS_IF_RELEASE(shell);
          }
        }
        NS_RELEASE(mDViewer);
      }
      NS_RELEASE(mCViewer);
    }
    
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
    docShellAsNode->GetChildCount(&n);
    for (i = 0; i < n; i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShellAsNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      DoEditorMode(childAsShell);
    }
  }
}

// Same as above, but calls NS_DoEditorTest instead of starting an editor
void
nsBrowserWindow::DoEditorTest(nsIDocShell *aDocShell, PRInt32 aCommandID)
{
  if (nsnull != aDocShell) {
    nsIContentViewer* mCViewer;
    aDocShell->GetContentViewer(&mCViewer);
    if (nsnull != mCViewer) {
      nsIDocumentViewer* mDViewer;
      if (NS_OK == mCViewer->QueryInterface(kIDocumentViewerIID, (void**) &mDViewer)) 
      {
        nsCOMPtr<nsIDocument> mDoc;
        mDViewer->GetDocument(getter_AddRefs(mDoc));
        if (nsnull != mDoc) {
          nsIDOMDocument* mDOMDoc;
          if (NS_OK == mDoc->QueryInterface(kIDOMDocumentIID, (void**) &mDOMDoc)) 
          {
            NS_DoEditorTest(aCommandID);
            NS_RELEASE(mDOMDoc);
          }
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
#include "nsIFrameDebug.h"


static void DumpAWebShell(nsIDocShellTreeItem* aShellItem, FILE* out, PRInt32 aIndent)
{
  nsXPIDLString name;
  nsAutoString str;
  nsCOMPtr<nsIDocShellTreeItem> parent;
  PRInt32 i, n;

  for (i = aIndent; --i >= 0; ) fprintf(out, "  ");

  fprintf(out, "%p '", aShellItem);
  aShellItem->GetName(getter_Copies(name));
  aShellItem->GetSameTypeParent(getter_AddRefs(parent));
  str.Assign(name);
  fputs(NS_LossyConvertUCS2toASCII(str).get(), out);
  fprintf(out, "' parent=%p <\n", parent.get());

  aIndent++;
  nsCOMPtr<nsIDocShellTreeNode> shellAsNode(do_QueryInterface(aShellItem));
  shellAsNode->GetChildCount(&n);
  for (i = 0; i < n; i++) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    shellAsNode->GetChildAt(i, getter_AddRefs(child));
    if (child) {
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
  nsCOMPtr<nsIDocShellTreeItem> shellAsItem(do_QueryInterface(mDocShell));
  DumpAWebShell(shellAsItem, out, 0);
}

static
void 
DumpMultipleWebShells(nsBrowserWindow& aBrowserWindow, nsIDocShell* aDocShell, FILE* aOut)
{ 
  PRInt32 count;
  nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
  if (docShellAsNode) {
    docShellAsNode->GetChildCount(&count);
    if (count > 0) {
      fprintf(aOut, "webshells= \n");
      aBrowserWindow.DumpWebShells(aOut);
    }
  }
}

static
void
DumpContentRecurse(nsIDocShell* aDocShell, FILE* out)
{
  if (nsnull != aDocShell) {
    fprintf(out, "docshell=%p \n", aDocShell);
    nsIPresShell* shell = GetPresShellFor(aDocShell);
    if (nsnull != shell) {
      nsIDocument *doc = shell->GetDocument();
      if (doc) {
        nsIContent *root = doc->GetRootContent();
        if (nsnull != root) {
          root->List(out);
        }
      }
      NS_RELEASE(shell);
    }
    else {
      fputs("null pres shell\n", out);
    }
    // dump the frames of the sub documents
    PRInt32 i, n;
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
    docShellAsNode->GetChildCount(&n);
    for (i = 0; i < n; i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShellAsNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      if (nsnull != child) {
        DumpContentRecurse(childAsShell, out);
      }
    }
  }
}

void
nsBrowserWindow::DumpContent(FILE* out)
{
  DumpContentRecurse(mDocShell, out);
  DumpMultipleWebShells(*this, mDocShell, out);
}

static void
DumpFramesRecurse(nsIDocShell* aDocShell, FILE* out, nsString *aFilterName)
{
  if (nsnull != aDocShell) {
    fprintf(out, "webshell=%p \n", aDocShell);
    nsIPresShell* shell = GetPresShellFor(aDocShell);
    if (nsnull != shell) {
      nsIFrame* root;
      shell->GetRootFrame(&root);
      if (nsnull != root) {
        nsIFrameDebug* fdbg;
        if (NS_SUCCEEDED(CallQueryInterface(root, &fdbg))) {
          fdbg->List(shell->GetPresContext(), out, 0);
        }
      }
      NS_RELEASE(shell);
    }
    else {
      fputs("null pres shell\n", out);
    }

    // dump the frames of the sub documents
    PRInt32 i, n;
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
    docShellAsNode->GetChildCount(&n);
    for (i = 0; i < n; i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShellAsNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      if (childAsShell) {
        DumpFramesRecurse(childAsShell, out, aFilterName);
      }
    }
  }
}

void
nsBrowserWindow::DumpFrames(FILE* out, nsString *aFilterName)
{
  DumpFramesRecurse(mDocShell, out, aFilterName);
  DumpMultipleWebShells(*this, mDocShell, out);
}

//----------------------------------------------------------------------

static
void
DumpViewsRecurse(nsIDocShell* aDocShell, FILE* out)
{
  if (nsnull != aDocShell) {
    fprintf(out, "docshell=%p \n", aDocShell);
    nsIPresShell* shell = GetPresShellFor(aDocShell);
    if (nsnull != shell) {
      nsIViewManager *vm = shell->GetViewManager();
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
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
    docShellAsNode->GetChildCount(&n);
    for (i = 0; i < n; i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShellAsNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      if (childAsShell) {
        DumpViewsRecurse(childAsShell, out);
      }
    }
  }
}

void
nsBrowserWindow::DumpViews(FILE* out)
{
  DumpViewsRecurse(mDocShell, out);
  DumpMultipleWebShells(*this, mDocShell, out);
}

void
nsBrowserWindow::DumpStyleSheets(FILE* out)
{
  nsCOMPtr<nsIPresShell> shell = dont_AddRef(GetPresShell());
  if (shell) {
    shell->ListStyleSheets(out);
  } else {
    fputs("null pres shell\n", out);
  }
}

void
nsBrowserWindow::DumpStyleContexts(FILE* out)
{
  nsCOMPtr<nsIPresShell> shell = dont_AddRef(GetPresShell());
  if (shell) {
    nsIFrame* root;
    shell->GetRootFrame(&root);
    shell->ListStyleContexts(root, out);
  } else {
    fputs("null pres shell\n", out);
  }
}

void
nsBrowserWindow::DumpReflowStats(FILE* out)
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
#ifdef MOZ_REFLOW_PERF
    shell->DumpReflows();
#else
    fprintf(out,"***********************************\n");
    fprintf(out, "Sorry, you have built with MOZ_REFLOW_PERF=1\n");
    fprintf(out,"***********************************\n");
#endif
    NS_RELEASE(shell);
  } else {
    fputs("null pres shell\n", out);
  }
}

void
nsBrowserWindow::DisplayReflowStats(PRBool aIsOn)
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
#ifdef MOZ_REFLOW_PERF
    shell->SetPaintFrameCount(aIsOn);
    SetBoolPref("layout.reflow.showframecounts",aIsOn);
    ForceRefresh();
#else
    printf("***********************************\n");
    printf("Sorry, you have built with MOZ_REFLOW_PERF=1\n");
    printf("***********************************\n");
#endif
    NS_RELEASE(shell);
  }
}

void
nsBrowserWindow::ToggleFrameBorders()
{
  nsILayoutDebugger* ld;
  nsresult rv = nsComponentManager::CreateInstance(kLayoutDebuggerCID,
                                                   nsnull,
                                                   kILayoutDebuggerIID,
                                                   (void **)&ld);
  if (NS_SUCCEEDED(rv)) {
    PRBool showing;
    ld->GetShowFrameBorders(&showing);
    ld->SetShowFrameBorders(!showing);
    ForceRefresh();
    NS_RELEASE(ld);
  }
}

void
nsBrowserWindow::ToggleVisualEventDebugging()
{
  nsILayoutDebugger* ld;
  nsresult rv = nsComponentManager::CreateInstance(kLayoutDebuggerCID,
                                                   nsnull,
                                                   kILayoutDebuggerIID,
                                                   (void **)&ld);
  if (NS_SUCCEEDED(rv)) {
    PRBool showing;
    ld->GetShowEventTargetFrameBorder(&showing);
    ld->SetShowEventTargetFrameBorder(!showing);
    ForceRefresh();
    NS_RELEASE(ld);
  }
}

void
nsBrowserWindow::ToggleBoolPrefAndRefresh(const char * aPrefName)
{
  NS_ASSERTION(nsnull != aPrefName,"null pref name");

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (prefs && nsnull != aPrefName)
  {
    PRBool value;
    prefs->GetBoolPref(aPrefName,&value);
    prefs->SetBoolPref(aPrefName,!value);
    prefs->SavePrefFile(nsnull);
    
    ForceRefresh();
  }
}

#endif

void
nsBrowserWindow::SetBoolPref(const char * aPrefName, PRBool aValue)
{
  NS_ASSERTION(nsnull != aPrefName,"null pref name");

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (prefs && nsnull != aPrefName)
  {
    prefs->SetBoolPref(aPrefName, aValue);
    prefs->SavePrefFile(nsnull);
  }
}

void
nsBrowserWindow::SetStringPref(const char * aPrefName, const nsString& aValue)
{
  NS_ASSERTION(nsnull != aPrefName, "null pref name");

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (nsnull != prefs && nsnull != aPrefName)
  {
    char * prefStr = ToNewCString(aValue);
    prefs->SetCharPref(aPrefName, prefStr);
    prefs->SavePrefFile(nsnull);
    delete [] prefStr;
  }

}

void
nsBrowserWindow::GetStringPref(const char * aPrefName, nsString& aValue)
{
  NS_ASSERTION(nsnull != aPrefName, "null pref name");

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (nsnull != prefs && nsnull != aPrefName)
  {
    char* prefCharVal;
    nsresult result = prefs->CopyCharPref(aPrefName, &prefCharVal);
    if (NS_SUCCEEDED(result)) {
      aValue.AssignWithConversion(prefCharVal);
      PL_strfree(prefCharVal);
    }
  }
}

#ifdef NS_DEBUG
void
nsBrowserWindow::DoDebugSave()
{
  /* Removed 04/01/99 -- gpk */
}

void 
nsBrowserWindow::DoToggleSelection()
{
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsCOMPtr<nsISelectionController> selCon;
    selCon = do_QueryInterface(shell);
    if (selCon) {
      PRInt16  current;
      selCon->GetDisplaySelection(&current);
      selCon->SetDisplaySelection(!current);
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

    case VIEWER_VISUAL_EVENT_DEBUGGING:
      ToggleVisualEventDebugging();
      result = nsEventStatus_eConsumeNoDefault;
      break;

    case VIEWER_TOGGLE_PAINT_FLASHING:
      ToggleBoolPrefAndRefresh("nglayout.debug.paint_flashing");
      result = nsEventStatus_eConsumeNoDefault;
      break;

    case VIEWER_TOGGLE_PAINT_DUMPING:
      ToggleBoolPrefAndRefresh("nglayout.debug.paint_dumping");
      result = nsEventStatus_eConsumeNoDefault;
      break;

    case VIEWER_TOGGLE_INVALIDATE_DUMPING:
      ToggleBoolPrefAndRefresh("nglayout.debug.invalidate_dumping");
      result = nsEventStatus_eConsumeNoDefault;
      break;

    case VIEWER_TOGGLE_EVENT_DUMPING:
      ToggleBoolPrefAndRefresh("nglayout.debug.event_dumping");
      result = nsEventStatus_eConsumeNoDefault;
      break;

    case VIEWER_TOGGLE_MOTION_EVENT_DUMPING:
      ToggleBoolPrefAndRefresh("nglayout.debug.motion_event_dumping");
      result = nsEventStatus_eConsumeNoDefault;
      break;

    case VIEWER_TOGGLE_CROSSING_EVENT_DUMPING:
      ToggleBoolPrefAndRefresh("nglayout.debug.crossing_event_dumping");
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

    case VIEWER_DEBUG_DUMP_REFLOW_TOTS:
      DumpReflowStats();
      result = nsEventStatus_eConsumeNoDefault;
      break;

    case VIEWER_DSP_REFLOW_CNTS_ON:
      DisplayReflowStats(PR_TRUE);
      result = nsEventStatus_eConsumeNoDefault;
      break;

    case VIEWER_DSP_REFLOW_CNTS_OFF:
      DisplayReflowStats(PR_FALSE);
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

#define USE_DTD  0
#define STANDARD 1 
#define QUIRKS   2 
void 
nsBrowserWindow::SetCompatibilityMode(PRUint32 aMode)
{
  nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID));
  if (pref) { 
    int32 prefInt = USE_DTD;
    if (STANDARD == aMode) {
      prefInt = eCompatibility_FullStandards;
    }
    else if (QUIRKS == aMode) {
      prefInt = eCompatibility_NavQuirks;
    }
    pref->SetIntPref("nglayout.compatibility.mode", prefInt);
    pref->SavePrefFile(nsnull);
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
        nsIDocument *doc = shell->GetDocument();
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
          fputs(NS_LossyConvertUCS2toASCII(title).get(), stdout);
          fprintf(stdout, "\" %s%s\n", 
                  ((title.Equals(current, nsCaseInsensitiveStringComparator())) ? "<< current " : ""), 
                  ((title.Equals(defaultStyle, nsCaseInsensitiveStringComparator())) ? "** default" : ""));
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
        nsIDocument *doc = shell->GetDocument();
        if (doc) {
          nsAutoString  defaultStyle;
          nsIAtom* defStyleAtom = NS_NewAtom("default-style");
          doc->GetHeaderData(defStyleAtom, defaultStyle);
          NS_RELEASE(defStyleAtom);
          fputs("Selecting default style sheet \"", stdout);
          fputs(NS_LossyConvertUCS2toASCII(defaultStyle).get(), stdout);
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
        fputs(NS_LossyConvertUCS2toASCII(title).get(), stdout);
        fputs("\"\n", stdout);
        shell->SelectAlternateStyleSheet(title);
        NS_RELEASE(shell);
      }
    }
    result = nsEventStatus_eConsumeNoDefault;
    break;

  case VIEWER_USE_DTD_MODE:
  case VIEWER_NAV_QUIRKS_MODE:
  case VIEWER_STANDARD_MODE:
    SetCompatibilityMode(aID - VIEWER_USE_DTD_MODE);
    result = nsEventStatus_eConsumeNoDefault;
    break;
  }
  return result;
}

NS_IMETHODIMP nsBrowserWindow::EnsureWebBrowserChrome()
{
   if(mWebBrowserChrome)
      return NS_OK;

   mWebBrowserChrome = new nsWebBrowserChrome();
   NS_ENSURE_TRUE(mWebBrowserChrome, NS_ERROR_OUT_OF_MEMORY);

   NS_ADDREF(mWebBrowserChrome);
   mWebBrowserChrome->BrowserWindow(this);

   return NS_OK;
}

nsresult nsBrowserWindow::GetWindow(nsIWidget** aWindow)
{
  if (aWindow && mWindow) {
    *aWindow = mWindow.get();
    NS_IF_ADDREF(*aWindow);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

