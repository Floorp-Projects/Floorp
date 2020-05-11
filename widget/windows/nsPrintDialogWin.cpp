/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintDialogWin.h"

#include "nsArray.h"
#include "nsCOMPtr.h"
#include "nsIBaseWindow.h"
#include "nsIBrowserChild.h"
#include "nsIDialogParamBlock.h"
#include "nsIDocShell.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPrintSettings.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWidget.h"
#include "nsPrintDialogUtil.h"
#include "nsIPrintSettings.h"
#include "nsIWebBrowserChrome.h"
#include "nsPIDOMWindow.h"
#include "nsQueryObject.h"

static const char* kPageSetupDialogURL =
    "chrome://global/content/printPageSetup.xhtml";

using namespace mozilla;
using namespace mozilla::widget;

/**
 * ParamBlock
 */

class ParamBlock {
 public:
  ParamBlock() { mBlock = 0; }
  ~ParamBlock() { NS_IF_RELEASE(mBlock); }
  nsresult Init() {
    return CallCreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID, &mBlock);
  }
  nsIDialogParamBlock* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN {
    return mBlock;
  }
  operator nsIDialogParamBlock* const() { return mBlock; }

 private:
  nsIDialogParamBlock* mBlock;
};

NS_IMPL_ISUPPORTS(nsPrintDialogServiceWin, nsIPrintDialogService)

nsPrintDialogServiceWin::nsPrintDialogServiceWin() {}

nsPrintDialogServiceWin::~nsPrintDialogServiceWin() {}

NS_IMETHODIMP
nsPrintDialogServiceWin::Init() {
  nsresult rv;
  mWatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  return rv;
}

NS_IMETHODIMP
nsPrintDialogServiceWin::Show(nsPIDOMWindowOuter* aParent,
                              nsIPrintSettings* aSettings) {
  NS_ENSURE_ARG(aParent);
  HWND hWnd = GetHWNDForDOMWindow(aParent);
  NS_ASSERTION(hWnd, "Couldn't get native window for PRint Dialog!");

  return NativeShowPrintDialog(hWnd, aSettings);
}

NS_IMETHODIMP
nsPrintDialogServiceWin::ShowPageSetup(nsPIDOMWindowOuter* aParent,
                                       nsIPrintSettings* aNSSettings) {
  NS_ENSURE_ARG(aParent);
  NS_ENSURE_ARG(aNSSettings);

  ParamBlock block;
  nsresult rv = block.Init();
  if (NS_FAILED(rv)) return rv;

  block->SetInt(0, 0);
  rv = DoDialog(aParent, block, aNSSettings, kPageSetupDialogURL);

  // if aWebBrowserPrint is not null then we are printing
  // so we want to pass back NS_ERROR_ABORT on cancel
  if (NS_SUCCEEDED(rv)) {
    int32_t status;
    block->GetInt(0, &status);
    return status == 0 ? NS_ERROR_ABORT : NS_OK;
  }

  // We don't call nsPrintSettingsService::SavePrintSettingsToPrefs here since
  // it's called for us in printPageSetup.js.  Maybe we should move that call
  // here for consistency with the other platforms though?

  return rv;
}

nsresult nsPrintDialogServiceWin::DoDialog(mozIDOMWindowProxy* aParent,
                                           nsIDialogParamBlock* aParamBlock,
                                           nsIPrintSettings* aPS,
                                           const char* aChromeURL) {
  NS_ENSURE_ARG(aParamBlock);
  NS_ENSURE_ARG(aPS);
  NS_ENSURE_ARG(aChromeURL);

  if (!mWatcher) return NS_ERROR_FAILURE;

  // get a parent, if at all possible
  // (though we'd rather this didn't fail, it's OK if it does. so there's
  // no failure or null check.)
  // retain ownership for method lifetime
  nsCOMPtr<mozIDOMWindowProxy> activeParent;
  if (!aParent) {
    mWatcher->GetActiveWindow(getter_AddRefs(activeParent));
    aParent = activeParent;
  }

  // create a nsIMutableArray of the parameters
  // being passed to the window
  nsCOMPtr<nsIMutableArray> array = nsArray::Create();

  nsCOMPtr<nsISupports> psSupports(do_QueryInterface(aPS));
  NS_ASSERTION(psSupports, "PrintSettings must be a supports");
  array->AppendElement(psSupports);

  nsCOMPtr<nsISupports> blkSupps(do_QueryInterface(aParamBlock));
  NS_ASSERTION(blkSupps, "IOBlk must be a supports");
  array->AppendElement(blkSupps);

  nsCOMPtr<mozIDOMWindowProxy> dialog;
  nsresult rv = mWatcher->OpenWindow(aParent, aChromeURL, "_blank",
                                     "centerscreen,chrome,modal,titlebar",
                                     array, getter_AddRefs(dialog));

  return rv;
}

HWND nsPrintDialogServiceWin::GetHWNDForDOMWindow(mozIDOMWindowProxy* aWindow) {
  nsCOMPtr<nsIWebBrowserChrome> chrome;

  // We might be embedded so check this path first
  if (mWatcher) {
    nsCOMPtr<mozIDOMWindowProxy> fosterParent;
    // it will be a dependent window. try to find a foster parent.
    if (!aWindow) {
      mWatcher->GetActiveWindow(getter_AddRefs(fosterParent));
      aWindow = fosterParent;
    }
    mWatcher->GetChromeForWindow(aWindow, getter_AddRefs(chrome));
  }

  if (chrome) {
    nsCOMPtr<nsIEmbeddingSiteWindow> site(do_QueryInterface(chrome));
    if (site) {
      HWND w;
      site->GetSiteWindow(reinterpret_cast<void**>(&w));
      return w;
    }
  }

  // Now we might be the Browser so check this path
  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  nsCOMPtr<nsIWebBrowserChrome> webBrowserChrome =
      window->GetWebBrowserChrome();
  if (!webBrowserChrome) return nullptr;

  nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(webBrowserChrome));
  if (!baseWin) return nullptr;

  nsCOMPtr<nsIWidget> widget;
  baseWin->GetMainWidget(getter_AddRefs(widget));
  if (!widget) return nullptr;

  return (HWND)widget->GetNativeData(NS_NATIVE_TMP_WINDOW);
}
