/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AboutGlean_h
#define mozilla_AboutGlean_h

#include "nsIAboutGlean.h"

namespace mozilla {
class AboutGlean final : public nsIAboutGlean {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIABOUTGLEAN

 public:
  AboutGlean() = default;
  static already_AddRefed<AboutGlean> GetSingleton();

 private:
  ~AboutGlean() = default;
};

};  // namespace mozilla

#endif  // mozilla_AboutGlean_h
