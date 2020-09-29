/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_IconLoaderHelperWin_h
#define mozilla_widget_IconLoaderHelperWin_h

#include "mozilla/widget/IconLoader.h"

namespace mozilla::widget {

/**
 * Classes that want to hear about when icons load should subclass
 * IconLoaderListenerWin, and implement the OnComplete() method,
 * which will be called once the load of the icon has completed.
 */
class IconLoaderListenerWin {
 public:
  IconLoaderListenerWin() = default;

  NS_INLINE_DECL_REFCOUNTING(mozilla::widget::IconLoaderListenerWin)

  virtual nsresult OnComplete() = 0;

 protected:
  virtual ~IconLoaderListenerWin() = default;
};

/**
 * This is a Helper used with mozilla::widget::IconLoader that implements the
 * Windows-specific functionality for converting a loaded icon into an HICON.
 */
class IconLoaderHelperWin final : public mozilla::widget::IconLoader::Helper {
 public:
  explicit IconLoaderHelperWin(
      mozilla::widget::IconLoaderListenerWin* aLoadListener);

  nsresult OnComplete(imgIContainer* aImage, const nsIntRect& aRect) override;

  /**
   * IconLoaderHelperWin will default the HICON returned by GetNativeIconImage
   * to the application icon. Once the load of the icon by IconLoader has
   * completed, GetNativeIconImage will return the loaded icon.
   *
   * Note that IconLoaderHelperWin owns this HICON. If you don't need it to hold
   * onto the HICON anymore, call Destroy on it to deallocate. The
   * IconLoaderHelperWin destructor will also deallocate the HICON if necessary.
   */
  HICON GetNativeIconImage();
  void Destroy();

 protected:
  ~IconLoaderHelperWin();

 private:
  RefPtr<mozilla::widget::IconLoaderListenerWin> mLoadListener;
  HICON mNativeIconImage;
};

}  // namespace mozilla::widget

#endif  // mozilla_widget_IconLoaderHelperWin_h
