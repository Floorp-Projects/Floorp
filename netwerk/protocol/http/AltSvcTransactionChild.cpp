/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "AltSvcTransactionChild.h"
#include "AlternateServices.h"
#include "nsHttpConnectionInfo.h"

namespace mozilla {
namespace net {

AltSvcTransactionChild::AltSvcTransactionChild(nsHttpConnectionInfo* aConnInfo,
                                               uint32_t aCaps)
    : mConnInfo(aConnInfo), mCaps(aCaps) {
  LOG(("AltSvcTransactionChild %p ctor", this));
}

AltSvcTransactionChild::~AltSvcTransactionChild() {
  LOG(("AltSvcTransactionChild %p dtor", this));
}

void AltSvcTransactionChild::OnTransactionDestroy(bool aValidateResult) {
  LOG(("AltSvcTransactionChild::OnTransactionDestroy %p aValidateResult=%d",
       this, aValidateResult));
  RefPtr<AltSvcTransactionChild> self = this;
  auto task = [self, aValidateResult]() {
    if (self->CanSend()) {
      self->Send__delete__(self, aValidateResult);
    }
  };
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("AltSvcTransactionChild::OnTransactionClose",
                               std::move(task)),
        NS_DISPATCH_NORMAL);
    return;
  }

  task();
}

void AltSvcTransactionChild::OnTransactionClose(bool aValidateResult) {
  LOG(("AltSvcTransactionChild::OnTransactionClose %p aValidateResult=%d", this,
       aValidateResult));
  RefPtr<AltSvcTransactionChild> self = this;
  auto task = [self, aValidateResult]() {
    if (self->CanSend()) {
      self->SendOnTransactionClose(aValidateResult);
    }
  };
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("AltSvcTransactionChild::OnTransactionClose",
                               std::move(task)),
        NS_DISPATCH_NORMAL);
    return;
  }

  task();
}

already_AddRefed<SpeculativeTransaction>
AltSvcTransactionChild::CreateTransaction() {
  RefPtr<SpeculativeTransaction> transaction =
      new AltSvcTransaction<AltSvcTransactionChild>(mConnInfo, nullptr, mCaps,
                                                    this, mConnInfo->IsHttp3());
  return transaction.forget();
}

}  // namespace net
}  // namespace mozilla
