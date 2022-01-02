/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "AltSvcTransactionParent.h"
#include "AlternateServices.h"
#include "mozilla/net/SocketProcessParent.h"
#include "nsHttpConnectionInfo.h"

namespace mozilla {
namespace net {

NS_IMPL_ADDREF_INHERITED(AltSvcTransactionParent, NullHttpTransaction)
NS_IMPL_RELEASE_INHERITED(AltSvcTransactionParent, NullHttpTransaction)

NS_INTERFACE_MAP_BEGIN(AltSvcTransactionParent)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(AltSvcTransactionParent)
NS_INTERFACE_MAP_END_INHERITING(NullHttpTransaction)

AltSvcTransactionParent::AltSvcTransactionParent(
    nsHttpConnectionInfo* aConnInfo, nsIInterfaceRequestor* aCallbacks,
    uint32_t aCaps, AltSvcMappingValidator* aValidator)
    : SpeculativeTransaction(aConnInfo, aCallbacks,
                             aCaps & ~NS_HTTP_ALLOW_KEEPALIVE),
      mValidator(aValidator) {
  LOG(("AltSvcTransactionParent %p ctor", this));
  MOZ_ASSERT(mValidator);
}

AltSvcTransactionParent::~AltSvcTransactionParent() {
  LOG(("AltSvcTransactionParent %p dtor", this));
}

bool AltSvcTransactionParent::Init() {
  SocketProcessParent* parent = SocketProcessParent::GetSingleton();
  if (!parent) {
    return false;
  }

  HttpConnectionInfoCloneArgs connInfo;
  nsHttpConnectionInfo::SerializeHttpConnectionInfo(mConnectionInfo, connInfo);
  return parent->SendPAltSvcTransactionConstructor(this, connInfo, mCaps);
}

mozilla::ipc::IPCResult AltSvcTransactionParent::Recv__delete__(
    const bool& aValidateResult) {
  LOG(("AltSvcTransactionParent::Recv__delete__ this=%p", this));
  mValidator->OnTransactionDestroy(aValidateResult);
  return IPC_OK();
}

mozilla::ipc::IPCResult AltSvcTransactionParent::RecvOnTransactionClose(
    const bool& aValidateResult) {
  LOG(("AltSvcTransactionParent::RecvOnTransactionClose this=%p", this));
  mValidator->OnTransactionClose(aValidateResult);
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
