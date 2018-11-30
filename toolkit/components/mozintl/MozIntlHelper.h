/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozIMozIntlHelper.h"

namespace mozilla {

class MozIntlHelper final : public mozIMozIntlHelper {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZIMOZINTLHELPER

  MozIntlHelper();

 private:
  ~MozIntlHelper();
};

}  // namespace mozilla
