/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTTPSSVC_h__
#define HTTPSSVC_h__

#include "nsIDNSByTypeRecord.h"
#include "mozilla/net/DNS.h"
#include "mozilla/Variant.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace net {

enum SvcParamKey : uint16_t {
  SvcParamKeyMandatory = 0,
  SvcParamKeyAlpn = 1,
  SvcParamKeyNoDefaultAlpn = 2,
  SvcParamKeyPort = 3,
  SvcParamKeyIpv4Hint = 4,
  SvcParamKeyEchConfig = 5,
  SvcParamKeyIpv6Hint = 6,

  SvcParamKeyLast = SvcParamKeyIpv6Hint
};

struct SvcParamAlpn {
  bool operator==(const SvcParamAlpn& aOther) const {
    return mValue == aOther.mValue;
  }
  nsCString mValue;
};

struct SvcParamNoDefaultAlpn {
  bool operator==(const SvcParamNoDefaultAlpn& aOther) const { return true; }
};

struct SvcParamPort {
  bool operator==(const SvcParamPort& aOther) const {
    return mValue == aOther.mValue;
  }
  uint16_t mValue;
};

struct SvcParamIpv4Hint {
  bool operator==(const SvcParamIpv4Hint& aOther) const {
    return mValue == aOther.mValue;
  }
  CopyableTArray<mozilla::net::NetAddr> mValue;
};

struct SvcParamEchConfig {
  bool operator==(const SvcParamEchConfig& aOther) const {
    return mValue == aOther.mValue;
  }
  nsCString mValue;
};

struct SvcParamIpv6Hint {
  bool operator==(const SvcParamIpv6Hint& aOther) const {
    return mValue == aOther.mValue;
  }
  CopyableTArray<mozilla::net::NetAddr> mValue;
};

using SvcParamType =
    mozilla::Variant<Nothing, SvcParamAlpn, SvcParamNoDefaultAlpn, SvcParamPort,
                     SvcParamIpv4Hint, SvcParamEchConfig, SvcParamIpv6Hint>;

struct SvcFieldValue {
  bool operator==(const SvcFieldValue& aOther) const {
    return mValue == aOther.mValue;
  }
  SvcFieldValue() : mValue(AsVariant(Nothing{})) {}
  SvcParamType mValue;
};

struct SVCB {
  bool operator==(const SVCB& aOther) const {
    return mSvcFieldPriority == aOther.mSvcFieldPriority &&
           mSvcDomainName == aOther.mSvcDomainName &&
           mSvcFieldValue == aOther.mSvcFieldValue;
  }
  bool operator<(const SVCB& aOther) const {
    return mSvcFieldPriority < aOther.mSvcFieldPriority;
  }
  Maybe<uint16_t> GetPort() const;
  bool NoDefaultAlpn() const;
  Maybe<nsCString> GetAlpn(bool aNoHttp2, bool aNoHttp3) const;
  uint16_t mSvcFieldPriority = 0;
  nsCString mSvcDomainName;
  CopyableTArray<SvcFieldValue> mSvcFieldValue;
};

class SVCBRecord : public nsISVCBRecord {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISVCBRECORD
 public:
  explicit SVCBRecord(const SVCB& data)
      : mData(data), mPort(Nothing()), mAlpn(Nothing()) {}
  explicit SVCBRecord(const SVCB& data, Maybe<uint16_t>&& aPort,
                      Maybe<nsCString>&& aAlpn)
      : mData(data), mPort(std::move(aPort)), mAlpn(std::move(aAlpn)) {}

 private:
  virtual ~SVCBRecord() = default;
  SVCB mData;
  Maybe<uint16_t> mPort;
  Maybe<nsCString> mAlpn;
};

class DNSHTTPSSVCRecordBase {
 public:
  DNSHTTPSSVCRecordBase() = default;

 protected:
  virtual ~DNSHTTPSSVCRecordBase() = default;

  already_AddRefed<nsISVCBRecord> GetServiceModeRecordInternal(
      bool aNoHttp2, bool aNoHttp3, const nsTArray<SVCB>& aRecords);
};

}  // namespace net
}  // namespace mozilla

#endif  // HTTPSSVC_h__
