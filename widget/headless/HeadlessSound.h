/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_HeadlessSound_h
#define mozilla_widget_HeadlessSound_h

#include "nsISound.h"
#include "nsIStreamLoader.h"


namespace mozilla {
namespace widget {

class HeadlessSound : public nsISound,
                      public nsIStreamLoaderObserver
{
public:
  HeadlessSound();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISOUND
  NS_DECL_NSISTREAMLOADEROBSERVER

private:
  virtual ~HeadlessSound();
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_HeadlessSound_h
