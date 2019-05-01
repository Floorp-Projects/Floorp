/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 ;*; */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__net__RequestContextService_h
#define mozilla__net__RequestContextService_h

#include "nsCOMPtr.h"
#include "nsInterfaceHashtable.h"
#include "nsIObserver.h"
#include "nsIRequestContext.h"

namespace mozilla {
namespace net {

class RequestContextService final : public nsIRequestContextService,
                                    public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTCONTEXTSERVICE
  NS_DECL_NSIOBSERVER

  static already_AddRefed<nsIRequestContextService> GetOrCreate();

 private:
  RequestContextService();
  virtual ~RequestContextService();

  nsresult Init();
  void Shutdown();

  static RequestContextService* sSelf;

  nsInterfaceHashtable<nsUint64HashKey, nsIRequestContext> mTable;
  uint32_t mRCIDNamespace;
  uint32_t mNextRCID;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla__net__RequestContextService_h
