/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_AndroidView_h
#define mozilla_widget_AndroidView_h

#include "mozilla/widget/EventDispatcher.h"

namespace mozilla {
namespace widget {

class AndroidView final : public nsIAndroidView {
  virtual ~AndroidView() {}

 public:
  const RefPtr<mozilla::widget::EventDispatcher> mEventDispatcher{
      new mozilla::widget::EventDispatcher()};

  AndroidView() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIANDROIDVIEW

  NS_FORWARD_NSIANDROIDEVENTDISPATCHER(mEventDispatcher->)

  mozilla::java::GeckoBundle::GlobalRef mInitData;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_AndroidView_h
