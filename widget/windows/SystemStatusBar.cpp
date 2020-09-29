/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <strsafe.h>
#include "SystemStatusBar.h"

#include "mozilla/dom/Element.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/LinkedList.h"
#include "mozilla/StaticPtr.h"
#include "nsComputedDOMStyle.h"
#include "nsIContentPolicy.h"
#include "nsMenuFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsXULPopupManager.h"
#include "IconLoaderHelperWin.h"

namespace mozilla::widget {

using mozilla::LinkedListElement;
using mozilla::dom::Element;
using mozilla::widget::IconLoaderListenerWin;

class StatusBarEntry final : public LinkedListElement<RefPtr<StatusBarEntry>>,
                             public IconLoaderListenerWin {
 public:
  explicit StatusBarEntry(Element* aMenu);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(StatusBarEntry)
  nsresult Init();
  LRESULT OnMessage(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
  const Element* GetMenu() { return mMenu; };

  nsresult OnComplete();

 private:
  ~StatusBarEntry();
  RefPtr<mozilla::widget::IconLoader> mIconLoader;
  RefPtr<mozilla::widget::IconLoaderHelperWin> mIconLoaderHelper;
  RefPtr<Element> mMenu;
  NOTIFYICONDATAW mIconData;
  boolean mInitted;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(StatusBarEntry)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(StatusBarEntry)
  tmp->OnComplete();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(StatusBarEntry)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIconLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIconLoaderHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMenu)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StatusBarEntry)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(StatusBarEntry)
NS_IMPL_CYCLE_COLLECTING_RELEASE(StatusBarEntry)

StatusBarEntry::StatusBarEntry(Element* aMenu) : mMenu(aMenu), mInitted(false) {
  mIconData = {/* cbSize */ sizeof(NOTIFYICONDATA),
               /* hWnd */ 0,
               /* uID */ 2,
               /* uFlags */ NIF_ICON | NIF_MESSAGE | NIF_TIP,
               /* uCallbackMessage */ WM_USER,
               /* hIcon */ 0,
               /* szTip */ L"",  // This is updated in Init()
               /* dwState */ 0,
               /* dwStateMask */ 0,
               /* szInfo */ L"",
               /* uVersion */ {NOTIFYICON_VERSION},
               /* szInfoTitle */ L"",
               /* dwInfoFlags */ 0};
  MOZ_ASSERT(mMenu);
}

StatusBarEntry::~StatusBarEntry() {
  if (!mInitted) {
    return;
  }
  ::Shell_NotifyIconW(NIM_DELETE, &mIconData);
  VERIFY(::DestroyWindow(mIconData.hWnd));
}

nsresult StatusBarEntry::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  // First, look at the content node's "image" attribute.
  nsAutoString imageURIString;
  bool hasImageAttr =
      mMenu->GetAttr(kNameSpaceID_None, nsGkAtoms::image, imageURIString);

  nsresult rv;
  RefPtr<ComputedStyle> sc;
  nsCOMPtr<nsIURI> iconURI;
  if (!hasImageAttr) {
    // If the content node has no "image" attribute, get the
    // "list-style-image" property from CSS.
    RefPtr<mozilla::dom::Document> document = mMenu->GetComposedDoc();
    if (!document) {
      return NS_ERROR_FAILURE;
    }

    sc = nsComputedDOMStyle::GetComputedStyle(mMenu, nullptr);
    if (!sc) {
      return NS_ERROR_FAILURE;
    }

    iconURI = sc->StyleList()->GetListStyleImageURI();
  } else {
    uint64_t dummy = 0;
    nsContentPolicyType policyType;
    nsCOMPtr<nsIPrincipal> triggeringPrincipal = mMenu->NodePrincipal();
    nsContentUtils::GetContentPolicyTypeForUIImageLoading(
        mMenu, getter_AddRefs(triggeringPrincipal), policyType, &dummy);
    if (policyType != nsIContentPolicy::TYPE_INTERNAL_IMAGE) {
      return NS_ERROR_ILLEGAL_VALUE;
    }

    // If this menu item shouldn't have an icon, the string will be empty,
    // and NS_NewURI will fail.
    rv = NS_NewURI(getter_AddRefs(iconURI), imageURIString);
    if (NS_FAILED(rv)) return rv;
  }

  mIconLoaderHelper = new IconLoaderHelperWin(this);
  nsIntRect rect;
  mIconLoader = new IconLoader(mIconLoaderHelper, mMenu, rect);

  if (iconURI) {
    rv = mIconLoader->LoadIcon(iconURI);
  }

  HWND iconWindow;
  NS_ENSURE_TRUE(iconWindow = ::CreateWindowExW(
                     /* extended style */ 0,
                     /* className */ L"IconWindowClass",
                     /* title */ 0,
                     /* style */ WS_CAPTION,
                     /* x, y, cx, cy */ 0, 0, 0, 0,
                     /* parent */ 0,
                     /* menu */ 0,
                     /* instance */ 0,
                     /* create struct */ 0),
                 NS_ERROR_FAILURE);
  ::SetWindowLongPtr(iconWindow, GWLP_USERDATA, (LONG_PTR)this);

  mIconData.hWnd = iconWindow;
  mIconData.hIcon = mIconLoaderHelper->GetNativeIconImage();

  nsAutoString labelAttr;
  mMenu->GetAttr(kNameSpaceID_None, nsGkAtoms::label, labelAttr);
  const nsString& label = PromiseFlatString(labelAttr);

  size_t destLength = sizeof mIconData.szTip / (sizeof mIconData.szTip[0]);
  wchar_t* tooltip = &(mIconData.szTip[0]);
  ::StringCchCopyNW(tooltip, destLength, label.get(), label.Length());

  ::Shell_NotifyIconW(NIM_ADD, &mIconData);
  ::Shell_NotifyIconW(NIM_SETVERSION, &mIconData);

  mInitted = true;
  return NS_OK;
}

nsresult StatusBarEntry::OnComplete() {
  RefPtr<StatusBarEntry> kungFuDeathGrip = this;
  mIconData.hIcon = mIconLoaderHelper->GetNativeIconImage();

  ::Shell_NotifyIconW(NIM_MODIFY, &mIconData);

  // To simplify things, we won't react to CSS changes to update the icon
  // with this implementation. We can get rid of the IconLoader and Helper
  // at this point, which will also free the allocated HICON.
  mIconLoaderHelper->Destroy();
  mIconLoader->ReleaseJSObjects();
  mIconLoader->Destroy();
  mIconLoader = nullptr;
  mIconLoaderHelper = nullptr;
  return NS_OK;
}

LRESULT StatusBarEntry::OnMessage(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_USER && (lp == WM_LBUTTONUP || lp == WM_RBUTTONUP)) {
    nsMenuFrame* menu = do_QueryFrame(mMenu->GetPrimaryFrame());
    if (!menu) {
      return TRUE;
    }

    nsMenuPopupFrame* popupFrame = menu->GetPopup();
    if (!popupFrame) {
      return TRUE;
    }
    nsIWidget* widget = popupFrame->GetNearestWidget();
    if (!widget) {
      return TRUE;
    }

    HWND win = static_cast<HWND>(widget->GetNativeData(NS_NATIVE_WINDOW));
    if (!win) {
      return TRUE;
    }

    // The menu that is being opened is a Gecko <xul:menu>, and the popup code
    // that manages it expects that the window that the <xul:menu> belongs to
    // will be in the foreground when it opens. If we don't do this, then if the
    // icon is clicked when the window is _not_ in the foreground, then the
    // opened menu will not be keyboard focusable, nor will it close on its own
    // if the user clicks away from the menu (at least, not until the user
    // focuses any window in the parent process).
    ::SetForegroundWindow(win);
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    POINT pt;
    ::GetCursorPos(&pt);
    pm->ShowPopup(popupFrame->GetContent(), nullptr, EmptyString(), pt.x, pt.y,
                  false, false, true, nullptr);
  }

  return DefWindowProc(hWnd, msg, wp, lp);
}

NS_IMPL_ISUPPORTS(SystemStatusBar, nsISystemStatusBar)

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
  StatusBarEntry* entry =
      (StatusBarEntry*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  if (entry) {
    return entry->OnMessage(hWnd, msg, wp, lp);
  }
  return TRUE;
}

static StaticRefPtr<SystemStatusBar> sSingleton;

SystemStatusBar& SystemStatusBar::GetSingleton() {
  if (!sSingleton) {
    sSingleton = new SystemStatusBar();
    ClearOnShutdown(&sSingleton);
  }
  return *sSingleton;
}

already_AddRefed<SystemStatusBar> SystemStatusBar::GetAddRefedSingleton() {
  RefPtr<SystemStatusBar> sm = &GetSingleton();
  return sm.forget();
}

nsresult SystemStatusBar::Init() {
  WNDCLASS classStruct = {/* style */ 0,
                          /* lpfnWndProc */ &WindowProc,
                          /* cbClsExtra */ 0,
                          /* cbWndExtra */ 0,
                          /* hInstance */ 0,
                          /* hIcon */ 0,
                          /* hCursor */ 0,
                          /* hbrBackground */ 0,
                          /* lpszMenuName */ 0,
                          /* lpszClassName */ L"IconWindowClass"};
  NS_ENSURE_TRUE(::RegisterClass(&classStruct), NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
SystemStatusBar::AddItem(Element* aElement) {
  RefPtr<StatusBarEntry> entry = new StatusBarEntry(aElement);
  nsresult rv = entry->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mStatusBarEntries.insertBack(entry);
  return NS_OK;
}

NS_IMETHODIMP
SystemStatusBar::RemoveItem(Element* aElement) {
  for (StatusBarEntry* entry : mStatusBarEntries) {
    if (entry->GetMenu() == aElement) {
      entry->removeFrom(mStatusBarEntries);
      return NS_OK;
    }
  }
  return NS_ERROR_NOT_AVAILABLE;
}

}  // namespace mozilla::widget
