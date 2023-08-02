/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AboutWindowsMessages.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsIWindowMediator.h"
#include "nsIAppWindow.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsDocShellTreeOwner.h"
#include "mozilla/WidgetUtils.h"
#include "mozilla/widget/nsWindowLoggedMessages.h"

namespace mozilla {

static StaticRefPtr<AboutWindowsMessages> sSingleton;

NS_IMPL_ISUPPORTS(AboutWindowsMessages, nsIAboutWindowsMessages);

/*static*/
already_AddRefed<AboutWindowsMessages> AboutWindowsMessages::GetSingleton() {
  if (!sSingleton) {
    sSingleton = new AboutWindowsMessages;
    ClearOnShutdown(&sSingleton);
  }

  return do_AddRef(sSingleton);
}

static bool GetWindowTitleFromWindow(const nsCOMPtr<nsPIDOMWindowOuter>& window,
                                     nsAutoString& title) {
  nsIDocShell* docShell = window->GetDocShell();
  if (docShell) {
    nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
    docShell->GetTreeOwner(getter_AddRefs(parentTreeOwner));
    nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(parentTreeOwner));
    baseWindow->GetTitle(title);
    return true;
  }
  return false;
}

NS_IMETHODIMP
AboutWindowsMessages::GetMessages(mozIDOMWindowProxy* currentWindow,
                                  nsTArray<nsTArray<nsCString>>& messages,
                                  nsTArray<nsString>& windowTitles) {
  messages.Clear();
  // Display the current window's messages first
  auto currentWindowOuter = nsPIDOMWindowOuter::From(currentWindow);
  RefPtr<nsIWidget> currentWidget =
      widget::WidgetUtils::DOMWindowToWidget(currentWindowOuter);
  nsAutoString currentWindowTitle;
  if (GetWindowTitleFromWindow(currentWindowOuter, currentWindowTitle)) {
    nsTArray<nsCString> windowMessages;
    widget::GetLatestWindowMessages(currentWidget, windowMessages);
    windowTitles.AppendElement(currentWindowTitle);
    messages.EmplaceBack(std::move(windowMessages));
  }
  nsCOMPtr<nsIWindowMediator> mediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  mediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
  if (!windowEnumerator) return NS_ERROR_FAILURE;
  bool more;
  while (NS_SUCCEEDED(windowEnumerator->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsISupports> isupports;
    if (NS_FAILED(windowEnumerator->GetNext(getter_AddRefs(isupports)))) break;
    nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(isupports);
    NS_ASSERTION(window, "not an nsPIDOMWindowOuter");
    RefPtr<nsIWidget> windowWidget =
        widget::WidgetUtils::DOMWindowToWidget(window);
    if (&*windowWidget != &*currentWidget) {
      nsAutoString title;
      if (GetWindowTitleFromWindow(window, title)) {
        nsTArray<nsCString> windowMessages;
        widget::GetLatestWindowMessages(windowWidget, windowMessages);
        windowTitles.AppendElement(title);
        messages.EmplaceBack(std::move(windowMessages));
      }
    }
  }
  return NS_OK;
}
}  // namespace mozilla
