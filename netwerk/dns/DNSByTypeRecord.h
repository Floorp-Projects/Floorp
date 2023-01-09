/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DNSByTypeRecord_h__
#define DNSByTypeRecord_h__

#include "mozilla/net/HTTPSSVC.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/net/NeckoMessageUtils.h"

namespace mozilla {
namespace net {

// The types of nsIDNSByTypeRecord: Nothing, TXT, HTTPSSVC
using TypeRecordEmpty = Nothing;
using TypeRecordTxt = CopyableTArray<nsCString>;
using TypeRecordHTTPSSVC = CopyableTArray<SVCB>;

// This variant reflects the multiple types of data a nsIDNSByTypeRecord
// can hold.
using TypeRecordResultType =
    Variant<TypeRecordEmpty, TypeRecordTxt, TypeRecordHTTPSSVC>;

// TypeRecordResultType is a variant, but since it doesn't have a default
// constructor it's not a type we can pass directly over IPC.
struct IPCTypeRecord {
  bool operator==(const IPCTypeRecord& aOther) const {
    return mData == aOther.mData;
  }
  explicit IPCTypeRecord() : mData(Nothing{}) {}
  TypeRecordResultType mData;
  uint32_t mTTL = 0;
};

}  // namespace net
}  // namespace mozilla

namespace mozilla {
namespace ipc {

template <>
struct IPDLParamTraits<mozilla::net::IPCTypeRecord> {
  typedef mozilla::net::IPCTypeRecord paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mData);
    WriteIPDLParam(aWriter, aActor, aParam.mTTL);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    if (!ReadIPDLParam(aReader, aActor, &aResult->mData)) {
      return false;
    }
    if (!ReadIPDLParam(aReader, aActor, &aResult->mTTL)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::Nothing> {
  typedef mozilla::Nothing paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    bool isSome = false;
    WriteIPDLParam(aWriter, aActor, isSome);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    bool isSome;
    if (!ReadIPDLParam(aReader, aActor, &isSome)) {
      return false;
    }
    *aResult = Nothing();
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SVCB> {
  typedef mozilla::net::SVCB paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mSvcFieldPriority);
    WriteIPDLParam(aWriter, aActor, aParam.mSvcDomainName);
    WriteIPDLParam(aWriter, aActor, aParam.mEchConfig);
    WriteIPDLParam(aWriter, aActor, aParam.mODoHConfig);
    WriteIPDLParam(aWriter, aActor, aParam.mHasIPHints);
    WriteIPDLParam(aWriter, aActor, aParam.mHasEchConfig);
    WriteIPDLParam(aWriter, aActor, aParam.mSvcFieldValue);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    if (!ReadIPDLParam(aReader, aActor, &aResult->mSvcFieldPriority)) {
      return false;
    }
    if (!ReadIPDLParam(aReader, aActor, &aResult->mSvcDomainName)) {
      return false;
    }
    if (!ReadIPDLParam(aReader, aActor, &aResult->mEchConfig)) {
      return false;
    }
    if (!ReadIPDLParam(aReader, aActor, &aResult->mODoHConfig)) {
      return false;
    }
    if (!ReadIPDLParam(aReader, aActor, &aResult->mHasIPHints)) {
      return false;
    }
    if (!ReadIPDLParam(aReader, aActor, &aResult->mHasEchConfig)) {
      return false;
    }
    if (!ReadIPDLParam(aReader, aActor, &aResult->mSvcFieldValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamAlpn> {
  typedef mozilla::net::SvcParamAlpn paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mValue);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    if (!ReadIPDLParam(aReader, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamNoDefaultAlpn> {
  typedef mozilla::net::SvcParamNoDefaultAlpn paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {}

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamPort> {
  typedef mozilla::net::SvcParamPort paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mValue);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    if (!ReadIPDLParam(aReader, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamIpv4Hint> {
  typedef mozilla::net::SvcParamIpv4Hint paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mValue);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    if (!ReadIPDLParam(aReader, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamEchConfig> {
  typedef mozilla::net::SvcParamEchConfig paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mValue);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    if (!ReadIPDLParam(aReader, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamIpv6Hint> {
  typedef mozilla::net::SvcParamIpv6Hint paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mValue);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    if (!ReadIPDLParam(aReader, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamODoHConfig> {
  typedef mozilla::net::SvcParamODoHConfig paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mValue);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    if (!ReadIPDLParam(aReader, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcFieldValue> {
  typedef mozilla::net::SvcFieldValue paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mValue);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    if (!ReadIPDLParam(aReader, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // DNSByTypeRecord_h__
