/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Retrieves and displays icons in native menu items on Windows.
 */

#include "gfxPlatform.h"
#include "imgIContainer.h"
#include "imgLoader.h"
#include "imgRequestProxy.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsNameSpaceManager.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsToolkit.h"
#include "nsWindowGfx.h"
#include "IconLoaderHelperWin.h"

using namespace mozilla;

using mozilla::gfx::SourceSurface;
using mozilla::widget::IconLoader;
using mozilla::widget::IconLoaderListenerWin;

namespace mozilla::widget {

NS_IMPL_CYCLE_COLLECTION(IconLoaderHelperWin, mLoadListener)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IconLoaderHelperWin)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(IconLoaderHelperWin)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IconLoaderHelperWin)

IconLoaderHelperWin::IconLoaderHelperWin(IconLoaderListenerWin* aListener)
    : mLoadListener(aListener) {
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(NS_IsMainThread());
}

IconLoaderHelperWin::~IconLoaderHelperWin() { Destroy(); }

nsresult IconLoaderHelperWin::OnComplete(imgIContainer* aImage,
                                         const nsIntRect& aRect) {
  NS_ENSURE_ARG_POINTER(aImage);

  nsresult rv = nsWindowGfx::CreateIcon(
      aImage, false, LayoutDeviceIntPoint(),
      nsWindowGfx::GetIconMetrics(nsWindowGfx::kRegularIcon),
      &mNativeIconImage);
  NS_ENSURE_SUCCESS(rv, rv);

  mLoadListener->OnComplete();
  return NS_OK;
}

HICON IconLoaderHelperWin::GetNativeIconImage() {
  if (mNativeIconImage) {
    return mNativeIconImage;
  }
  return ::LoadIcon(::GetModuleHandle(NULL), IDI_APPLICATION);
}

void IconLoaderHelperWin::Destroy() {
  if (mNativeIconImage) {
    ::DestroyIcon(mNativeIconImage);
    mNativeIconImage = nullptr;
  }
  if (mLoadListener) {
    mLoadListener = nullptr;
  }
}

}  // namespace mozilla::widget
