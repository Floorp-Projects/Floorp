/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AltSvcTransactionChild_h__
#define AltSvcTransactionChild_h__

#include "mozilla/net/PAltSvcTransactionChild.h"

namespace mozilla {
namespace net {

class nsHttpConnectionInfo;
class SpeculativeTransaction;

class AltSvcTransactionChild final : public PAltSvcTransactionChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AltSvcTransactionChild, override)

  explicit AltSvcTransactionChild(nsHttpConnectionInfo* aConnInfo,
                                  uint32_t aCaps);

  void OnTransactionDestroy(bool aValidateResult);
  void OnTransactionClose(bool aValidateResult);

  already_AddRefed<SpeculativeTransaction> CreateTransaction();

 private:
  virtual ~AltSvcTransactionChild();

  RefPtr<nsHttpConnectionInfo> mConnInfo;
  uint32_t mCaps;
};

}  // namespace net
}  // namespace mozilla

#endif  // AltSvcTransactionChild_h__
