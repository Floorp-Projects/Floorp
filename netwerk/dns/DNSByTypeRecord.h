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
};

}  // namespace net
}  // namespace mozilla

namespace mozilla {
namespace ipc {

template <>
struct IPDLParamTraits<mozilla::net::IPCTypeRecord> {
  typedef mozilla::net::IPCTypeRecord paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mData);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mData)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::Nothing> {
  typedef mozilla::Nothing paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    bool isSome = false;
    WriteIPDLParam(aMsg, aActor, isSome);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    bool isSome;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &isSome)) {
      return false;
    }
    *aResult = Nothing();
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SVCB> {
  typedef mozilla::net::SVCB paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mSvcFieldPriority);
    WriteIPDLParam(aMsg, aActor, aParam.mSvcDomainName);
    WriteIPDLParam(aMsg, aActor, aParam.mSvcFieldValue);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSvcFieldPriority)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSvcDomainName)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSvcFieldValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamAlpn> {
  typedef mozilla::net::SvcParamAlpn paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mValue);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamNoDefaultAlpn> {
  typedef mozilla::net::SvcParamNoDefaultAlpn paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {}

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamPort> {
  typedef mozilla::net::SvcParamPort paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mValue);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamIpv4Hint> {
  typedef mozilla::net::SvcParamIpv4Hint paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mValue);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamEsniConfig> {
  typedef mozilla::net::SvcParamEsniConfig paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mValue);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcParamIpv6Hint> {
  typedef mozilla::net::SvcParamIpv6Hint paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mValue);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::net::SvcFieldValue> {
  typedef mozilla::net::SvcFieldValue paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mValue);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // DNSByTypeRecord_h__