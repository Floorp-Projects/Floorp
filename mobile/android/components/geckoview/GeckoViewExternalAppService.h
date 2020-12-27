/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoViewExternalAppService_h__
#define GeckoViewExternalAppService_h__

#include "nsIExternalHelperAppService.h"
#include "mozilla/StaticPtr.h"

class GeckoViewExternalAppService : public nsIExternalHelperAppService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXTERNALHELPERAPPSERVICE

  GeckoViewExternalAppService();

  static already_AddRefed<GeckoViewExternalAppService> GetSingleton();

 private:
  virtual ~GeckoViewExternalAppService() {}
  static mozilla::StaticRefPtr<GeckoViewExternalAppService> sService;
};

#endif