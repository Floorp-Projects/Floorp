/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AboutWindowsMessages_h__
#define __AboutWindowsMessages_h__

#include "mozilla/MozPromise.h"
#include "nsIAboutWindowsMessages.h"

namespace mozilla {

class AboutWindowsMessages final : public nsIAboutWindowsMessages {
  ~AboutWindowsMessages() = default;

 public:
  static already_AddRefed<AboutWindowsMessages> GetSingleton();

  AboutWindowsMessages() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIABOUTWINDOWSMESSAGES
};

}  // namespace mozilla

#endif  // __AboutWindowsMessages_h__
