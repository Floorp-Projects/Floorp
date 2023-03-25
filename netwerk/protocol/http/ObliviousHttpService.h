/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ObliviousHttpService_h
#define mozilla_net_ObliviousHttpService_h

#include "mozilla/DataMutex.h"
#include "nsCOMPtr.h"
#include "nsIObliviousHttp.h"
#include "nsIObserver.h"
#include "nsIStreamLoader.h"

namespace mozilla::net {

class ObliviousHttpConfig {
 public:
  nsCOMPtr<nsIURI> mRelayURI;
  nsTArray<uint8_t> mEncodedConfig;
};

class ObliviousHttpService final : public nsIObliviousHttpService,
                                   public nsIObserver,
                                   public nsIStreamLoaderObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBLIVIOUSHTTPSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSISTREAMLOADEROBSERVER

  ObliviousHttpService();

 private:
  ~ObliviousHttpService() = default;
  void ReadPrefs(const nsACString& whichPref);
  void FetchConfig(bool aConfigURIChanged);

  DataMutex<ObliviousHttpConfig> mTRRConfig;
};

}  // namespace mozilla::net

#endif  // mozilla_net_ObliviousHttpService_h
