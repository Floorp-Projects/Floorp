/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTTPSSVC_h__
#define HTTPSSVC_h__

#include "nsIDNSByTypeRecord.h"
#include "mozilla/net/DNS.h"
#include "mozilla/Variant.h"
#include "mozilla/Maybe.h"
#include "nsHttp.h"

namespace mozilla {
namespace net {

class DNSHTTPSSVCRecordBase;

enum SvcParamKey : uint16_t {
  SvcParamKeyMandatory = 0,
  SvcParamKeyAlpn = 1,
  SvcParamKeyNoDefaultAlpn = 2,
  SvcParamKeyPort = 3,
  SvcParamKeyIpv4Hint = 4,
  SvcParamKeyEchConfig = 5,
  SvcParamKeyIpv6Hint = 6,
  SvcParamKeyODoHConfig = 32769,
};

inline bool IsValidSvcParamKey(uint16_t aKey) {
  return aKey <= SvcParamKeyIpv6Hint || aKey == SvcParamKeyODoHConfig;
}

struct SvcParamAlpn {
  bool operator==(const SvcParamAlpn& aOther) const {
    return mValue == aOther.mValue;
  }
  CopyableTArray<nsCString> mValue;
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

struct SvcParamODoHConfig {
  bool operator==(const SvcParamODoHConfig& aOther) const {
    return mValue == aOther.mValue;
  }
  nsCString mValue;
};

using SvcParamType =
    mozilla::Variant<Nothing, SvcParamAlpn, SvcParamNoDefaultAlpn, SvcParamPort,
                     SvcParamIpv4Hint, SvcParamEchConfig, SvcParamIpv6Hint,
                     SvcParamODoHConfig>;

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
  bool operator<(const SVCB& aOther) const;
  Maybe<uint16_t> GetPort() const;
  bool NoDefaultAlpn() const;
  void GetIPHints(CopyableTArray<mozilla::net::NetAddr>& aAddresses) const;
  nsTArray<Tuple<nsCString, SupportedAlpnRank>> GetAllAlpn() const;
  uint16_t mSvcFieldPriority = 0;
  nsCString mSvcDomainName;
  nsCString mEchConfig;
  nsCString mODoHConfig;
  bool mHasIPHints = false;
  bool mHasEchConfig = false;
  CopyableTArray<SvcFieldValue> mSvcFieldValue;
};

struct SVCBWrapper {
  explicit SVCBWrapper(const SVCB& aRecord) : mRecord(aRecord) {}
  Maybe<Tuple<nsCString, SupportedAlpnRank>> mAlpn;
  const SVCB& mRecord;
};

class SVCBRecord : public nsISVCBRecord {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISVCBRECORD
 public:
  explicit SVCBRecord(const SVCB& data)
      : mData(data), mPort(Nothing()), mAlpn(Nothing()) {}
  explicit SVCBRecord(const SVCB& data,
                      Maybe<Tuple<nsCString, SupportedAlpnRank>> aAlpn);

 private:
  friend class DNSHTTPSSVCRecordBase;

  virtual ~SVCBRecord() = default;

  SVCB mData;
  Maybe<uint16_t> mPort;
  Maybe<Tuple<nsCString, SupportedAlpnRank>> mAlpn;
};

class DNSHTTPSSVCRecordBase {
 public:
  explicit DNSHTTPSSVCRecordBase(const nsACString& aHost) : mHost(aHost) {}

 protected:
  virtual ~DNSHTTPSSVCRecordBase() = default;

  already_AddRefed<nsISVCBRecord> GetServiceModeRecordInternal(
      bool aNoHttp2, bool aNoHttp3, const nsTArray<SVCB>& aRecords,
      bool& aRecordsAllExcluded, bool aCheckHttp3ExcludedList = true);

  bool HasIPAddressesInternal(const nsTArray<SVCB>& aRecords);

  void GetAllRecordsWithEchConfigInternal(
      bool aNoHttp2, bool aNoHttp3, const nsTArray<SVCB>& aRecords,
      bool* aAllRecordsHaveEchConfig, bool* aAllRecordsInH3ExcludedList,
      nsTArray<RefPtr<nsISVCBRecord>>& aResult,
      bool aCheckHttp3ExcludedList = true);

  // The owner name of this HTTPS RR.
  nsCString mHost;
};

}  // namespace net
}  // namespace mozilla

#endif  // HTTPSSVC_h__
