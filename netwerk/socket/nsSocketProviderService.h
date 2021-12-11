/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSocketProviderService_h__
#define nsSocketProviderService_h__

#include "nsISocketProviderService.h"
#include "mozilla/StaticPtr.h"

class nsSocketProviderService : public nsISocketProviderService {
  nsSocketProviderService() = default;
  virtual ~nsSocketProviderService() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISOCKETPROVIDERSERVICE

  static already_AddRefed<nsISocketProviderService> GetOrCreate();

 private:
  static mozilla::StaticRefPtr<nsSocketProviderService> gSingleton;
};

#endif /* nsSocketProviderService_h__ */
