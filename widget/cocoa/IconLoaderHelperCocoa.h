/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_IconLoaderHelperCocoa_h
#define mozilla_widget_IconLoaderHelperCocoa_h

#import <Cocoa/Cocoa.h>

#include "mozilla/widget/IconLoader.h"

namespace mozilla::widget {

/**
 * Classes that want to hear about when icons load should subclass
 * IconLoaderListenerCocoa, and implement the OnComplete() method,
 * which will be called once the load of the icon has completed.
 */
class IconLoaderListenerCocoa {
 public:
  IconLoaderListenerCocoa() = default;

  NS_INLINE_DECL_REFCOUNTING(mozilla::widget::IconLoaderListenerCocoa)

  virtual nsresult OnComplete() = 0;

 protected:
  virtual ~IconLoaderListenerCocoa() = default;
};

/**
 * This is a Helper used with mozilla::widget::IconLoader that implements the
 * macOS-specific functionality for converting a loaded icon into an NSImage*.
 */
class IconLoaderHelperCocoa final : public mozilla::widget::IconLoader::Helper {
 public:
  IconLoaderHelperCocoa(mozilla::widget::IconLoaderListenerCocoa* aLoadListener,
                        uint32_t aIconHeight, uint32_t aIconWidth, CGFloat aScaleFactor = 0.0f);

  NS_DECL_ISUPPORTS

  nsresult OnComplete(imgIContainer* aImage, const nsIntRect& aRect) override;

  /**
   * IconLoaderHelperCocoa will default the NSImage* returned by
   * GetNativeIconImage to an empty icon. Once the load of the icon
   * by IconLoader has completed, GetNativeIconImage will return the
   * loaded icon.
   *
   * Note that IconLoaderHelperCocoa owns this NSImage. If you don't
   * need it to hold onto the NSImage anymore, call Destroy on it to
   * deallocate. The IconLoaderHelperCocoa destructor will also deallocate
   * the NSImage if necessary.
   */
  NSImage* GetNativeIconImage();  // Owned by IconLoaderHelperCocoa
  void Destroy();

 protected:
  ~IconLoaderHelperCocoa();

 private:
  RefPtr<mozilla::widget::IconLoaderListenerCocoa> mLoadListener;
  uint32_t mIconHeight;
  uint32_t mIconWidth;
  CGFloat mScaleFactor;
  NSImage* mNativeIconImage;
};

}  // namespace mozilla::widget

#endif  // mozilla_widget_IconLoaderHelperCocoa_h
