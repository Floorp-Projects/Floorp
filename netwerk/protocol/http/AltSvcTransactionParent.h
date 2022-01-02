/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AltSvcTransactionParent_h__
#define AltSvcTransactionParent_h__

#include "mozilla/net/PAltSvcTransactionParent.h"
#include "mozilla/net/SpeculativeTransaction.h"

namespace mozilla {
namespace net {

class AltSvcMappingValidator;

// 03d22e57-c364-4871-989a-6593eb909d24
#define ALTSVCTRANSACTIONPARENT_IID                  \
  {                                                  \
    0x03d22e57, 0xc364, 0x4871, {                    \
      0x98, 0x9a, 0x65, 0x93, 0xeb, 0x90, 0x9d, 0x24 \
    }                                                \
  }

class AltSvcTransactionParent final : public PAltSvcTransactionParent,
                                      public SpeculativeTransaction {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECLARE_STATIC_IID_ACCESSOR(ALTSVCTRANSACTIONPARENT_IID)

  explicit AltSvcTransactionParent(nsHttpConnectionInfo* aConnInfo,
                                   nsIInterfaceRequestor* aCallbacks,
                                   uint32_t aCaps,
                                   AltSvcMappingValidator* aValidator);

  bool Init();
  mozilla::ipc::IPCResult Recv__delete__(const bool& aValidateResult);
  mozilla::ipc::IPCResult RecvOnTransactionClose(const bool& aValidateResult);

 private:
  virtual ~AltSvcTransactionParent();

  RefPtr<AltSvcMappingValidator> mValidator;
};

NS_DEFINE_STATIC_IID_ACCESSOR(AltSvcTransactionParent,
                              ALTSVCTRANSACTIONPARENT_IID)

}  // namespace net
}  // namespace mozilla

#endif  // AltSvcTransactionParent_h__
