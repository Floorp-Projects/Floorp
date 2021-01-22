/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTTPSSVC.h"
#include "mozilla/net/DNS.h"
#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsNetAddr.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(SVCBRecord, nsISVCBRecord)

class SvcParam : public nsISVCParam,
                 public nsISVCParamAlpn,
                 public nsISVCParamNoDefaultAlpn,
                 public nsISVCParamPort,
                 public nsISVCParamIPv4Hint,
                 public nsISVCParamEchConfig,
                 public nsISVCParamIPv6Hint,
                 public nsISVCParamODoHConfig {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISVCPARAM
  NS_DECL_NSISVCPARAMALPN
  NS_DECL_NSISVCPARAMNODEFAULTALPN
  NS_DECL_NSISVCPARAMPORT
  NS_DECL_NSISVCPARAMIPV4HINT
  NS_DECL_NSISVCPARAMECHCONFIG
  NS_DECL_NSISVCPARAMIPV6HINT
  NS_DECL_NSISVCPARAMODOHCONFIG
 public:
  explicit SvcParam(const SvcParamType& value) : mValue(value){};

 private:
  virtual ~SvcParam() = default;
  SvcParamType mValue;
};

NS_IMPL_ADDREF(SvcParam)
NS_IMPL_RELEASE(SvcParam)
NS_INTERFACE_MAP_BEGIN(SvcParam)
  NS_INTERFACE_MAP_ENTRY(nsISVCParam)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVCParam)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISVCParamAlpn, mValue.is<SvcParamAlpn>())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISVCParamNoDefaultAlpn,
                                     mValue.is<SvcParamNoDefaultAlpn>())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISVCParamPort, mValue.is<SvcParamPort>())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISVCParamIPv4Hint,
                                     mValue.is<SvcParamIpv4Hint>())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISVCParamEchConfig,
                                     mValue.is<SvcParamEchConfig>())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISVCParamIPv6Hint,
                                     mValue.is<SvcParamIpv6Hint>())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISVCParamODoHConfig,
                                     mValue.is<SvcParamODoHConfig>())
NS_INTERFACE_MAP_END

NS_IMETHODIMP
SvcParam::GetType(uint16_t* aType) {
  *aType = mValue.match(
      [](Nothing&) { return SvcParamKeyMandatory; },
      [](SvcParamAlpn&) { return SvcParamKeyAlpn; },
      [](SvcParamNoDefaultAlpn&) { return SvcParamKeyNoDefaultAlpn; },
      [](SvcParamPort&) { return SvcParamKeyPort; },
      [](SvcParamIpv4Hint&) { return SvcParamKeyIpv4Hint; },
      [](SvcParamEchConfig&) { return SvcParamKeyEchConfig; },
      [](SvcParamIpv6Hint&) { return SvcParamKeyIpv6Hint; },
      [](SvcParamODoHConfig&) { return SvcParamKeyODoHConfig; });
  return NS_OK;
}

NS_IMETHODIMP
SvcParam::GetAlpn(nsTArray<nsCString>& aAlpn) {
  if (!mValue.is<SvcParamAlpn>()) {
    MOZ_ASSERT(false, "Unexpected type for variant");
    return NS_ERROR_NOT_AVAILABLE;
  }
  aAlpn.AppendElements(mValue.as<SvcParamAlpn>().mValue);
  return NS_OK;
}

NS_IMETHODIMP
SvcParam::GetPort(uint16_t* aPort) {
  if (!mValue.is<SvcParamPort>()) {
    MOZ_ASSERT(false, "Unexpected type for variant");
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aPort = mValue.as<SvcParamPort>().mValue;
  return NS_OK;
}

NS_IMETHODIMP
SvcParam::GetEchconfig(nsACString& aEchConfig) {
  if (!mValue.is<SvcParamEchConfig>()) {
    MOZ_ASSERT(false, "Unexpected type for variant");
    return NS_ERROR_NOT_AVAILABLE;
  }
  aEchConfig = mValue.as<SvcParamEchConfig>().mValue;
  return NS_OK;
}

NS_IMETHODIMP
SvcParam::GetIpv4Hint(nsTArray<RefPtr<nsINetAddr>>& aIpv4Hint) {
  if (!mValue.is<SvcParamIpv4Hint>()) {
    MOZ_ASSERT(false, "Unexpected type for variant");
    return NS_ERROR_NOT_AVAILABLE;
  }
  const auto& results = mValue.as<SvcParamIpv4Hint>().mValue;
  for (const auto& ip : results) {
    if (ip.raw.family != AF_INET) {
      return NS_ERROR_UNEXPECTED;
    }
    RefPtr<nsINetAddr> hint = new nsNetAddr(&ip);
    aIpv4Hint.AppendElement(hint);
  }

  return NS_OK;
}

NS_IMETHODIMP
SvcParam::GetIpv6Hint(nsTArray<RefPtr<nsINetAddr>>& aIpv6Hint) {
  if (!mValue.is<SvcParamIpv6Hint>()) {
    MOZ_ASSERT(false, "Unexpected type for variant");
    return NS_ERROR_NOT_AVAILABLE;
  }
  const auto& results = mValue.as<SvcParamIpv6Hint>().mValue;
  for (const auto& ip : results) {
    if (ip.raw.family != AF_INET6) {
      return NS_ERROR_UNEXPECTED;
    }
    RefPtr<nsINetAddr> hint = new nsNetAddr(&ip);
    aIpv6Hint.AppendElement(hint);
  }
  return NS_OK;
}

NS_IMETHODIMP
SvcParam::GetODoHConfig(nsACString& aODoHConfig) {
  if (!mValue.is<SvcParamODoHConfig>()) {
    MOZ_ASSERT(false, "Unexpected type for variant");
    return NS_ERROR_NOT_AVAILABLE;
  }
  aODoHConfig = mValue.as<SvcParamODoHConfig>().mValue;
  return NS_OK;
}

bool SVCB::operator<(const SVCB& aOther) const {
  if (gHttpHandler->EchConfigEnabled()) {
    if (mHasEchConfig && !aOther.mHasEchConfig) {
      return true;
    }
    if (!mHasEchConfig && aOther.mHasEchConfig) {
      return false;
    }
  }

  return mSvcFieldPriority < aOther.mSvcFieldPriority;
}

Maybe<uint16_t> SVCB::GetPort() const {
  Maybe<uint16_t> port;
  for (const auto& value : mSvcFieldValue) {
    if (value.mValue.is<SvcParamPort>()) {
      port.emplace(value.mValue.as<SvcParamPort>().mValue);
      if (NS_FAILED(NS_CheckPortSafety(*port, "https"))) {
        *port = 0;
      }
      return port;
    }
  }

  return Nothing();
}

bool SVCB::NoDefaultAlpn() const {
  for (const auto& value : mSvcFieldValue) {
    if (value.mValue.is<SvcParamKeyNoDefaultAlpn>()) {
      return true;
    }
  }

  return false;
}

Maybe<Tuple<nsCString, bool>> SVCB::GetAlpn(bool aNoHttp2,
                                            bool aNoHttp3) const {
  Maybe<Tuple<nsCString, bool>> alpn;
  for (const auto& value : mSvcFieldValue) {
    if (value.mValue.is<SvcParamAlpn>()) {
      nsTArray<nsCString> alpnList;
      alpnList.AppendElements(value.mValue.as<SvcParamAlpn>().mValue);
      if (!alpnList.IsEmpty()) {
        alpn.emplace();
        alpn = Some(SelectAlpnFromAlpnList(alpnList, aNoHttp2, aNoHttp3));
      }
      return alpn;
    }
  }

  return Nothing();
}

void SVCB::GetIPHints(CopyableTArray<mozilla::net::NetAddr>& aAddresses) const {
  if (mSvcFieldPriority == 0) {
    return;
  }

  for (const auto& value : mSvcFieldValue) {
    if (value.mValue.is<SvcParamIpv4Hint>()) {
      aAddresses.AppendElements(value.mValue.as<SvcParamIpv4Hint>().mValue);
    } else if (value.mValue.is<SvcParamIpv6Hint>()) {
      aAddresses.AppendElements(value.mValue.as<SvcParamIpv6Hint>().mValue);
    }
  }
}

NS_IMETHODIMP SVCBRecord::GetPriority(uint16_t* aPriority) {
  *aPriority = mData.mSvcFieldPriority;
  return NS_OK;
}

NS_IMETHODIMP SVCBRecord::GetName(nsACString& aName) {
  aName = mData.mSvcDomainName;
  return NS_OK;
}

Maybe<uint16_t> SVCBRecord::GetPort() { return mPort; }

Maybe<Tuple<nsCString, bool>> SVCBRecord::GetAlpn() { return mAlpn; }

NS_IMETHODIMP SVCBRecord::GetEchConfig(nsACString& aEchConfig) {
  aEchConfig = mData.mEchConfig;
  return NS_OK;
}

NS_IMETHODIMP SVCBRecord::GetODoHConfig(nsACString& aODoHConfig) {
  aODoHConfig = mData.mODoHConfig;
  return NS_OK;
}

NS_IMETHODIMP SVCBRecord::GetValues(nsTArray<RefPtr<nsISVCParam>>& aValues) {
  for (const auto& v : mData.mSvcFieldValue) {
    RefPtr<nsISVCParam> param = new SvcParam(v.mValue);
    aValues.AppendElement(param);
  }
  return NS_OK;
}

NS_IMETHODIMP SVCBRecord::GetHasIPHintAddress(bool* aHasIPHintAddress) {
  *aHasIPHintAddress = mData.mHasIPHints;
  return NS_OK;
}

static bool CheckAlpnIsUsable(const nsACString& aTargetName,
                              const nsACString& aAlpn, bool aIsHttp3,
                              uint32_t& aExcludedCount) {
  if (aAlpn.IsEmpty()) {
    return false;
  }

  if (aIsHttp3 && gHttpHandler->IsHttp3Excluded(aTargetName)) {
    aExcludedCount++;
    return false;
  }

  return true;
}

already_AddRefed<nsISVCBRecord>
DNSHTTPSSVCRecordBase::GetServiceModeRecordInternal(
    bool aNoHttp2, bool aNoHttp3, const nsTArray<SVCB>& aRecords,
    bool& aRecordsAllExcluded, bool aCheckHttp3ExcludedList) {
  nsCOMPtr<nsISVCBRecord> selectedRecord;
  uint32_t recordHasNoDefaultAlpnCount = 0;
  uint32_t recordExcludedCount = 0;
  aRecordsAllExcluded = false;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  bool RRSetHasEchConfig = false;
  uint32_t h3ExcludedCount = 0;

  for (const SVCB& record : aRecords) {
    if (record.mSvcFieldPriority == 0) {
      // In ServiceMode, the SvcPriority should never be 0.
      return nullptr;
    }

    if (record.NoDefaultAlpn()) {
      ++recordHasNoDefaultAlpnCount;
    }

    RRSetHasEchConfig |= record.mHasEchConfig;

    bool excluded = false;
    if (NS_SUCCEEDED(dns->IsSVCDomainNameFailed(mHost, record.mSvcDomainName,
                                                &excluded)) &&
        excluded) {
      // Skip if the domain name of this record was failed to connect before.
      ++recordExcludedCount;
      continue;
    }

    Maybe<uint16_t> port = record.GetPort();
    if (port && *port == 0) {
      // Found an unsafe port, skip this record.
      continue;
    }

    Maybe<Tuple<nsCString, bool>> alpn = record.GetAlpn(aNoHttp2, aNoHttp3);
    if (alpn) {
      if (!CheckAlpnIsUsable(record.mSvcDomainName, Get<0>(*alpn),
                             aCheckHttp3ExcludedList && Get<1>(*alpn),
                             h3ExcludedCount)) {
        continue;
      }
    }

    if (gHttpHandler->EchConfigEnabled() && RRSetHasEchConfig &&
        !record.mHasEchConfig) {
      // Don't use this record if this record has no echConfig, but others have.
      continue;
    }

    if (!selectedRecord) {
      selectedRecord = new SVCBRecord(record, std::move(port), std::move(alpn));
    }
  }

  // If all records indicate "no-default-alpn", we should not use this RRSet.
  if (recordHasNoDefaultAlpnCount == aRecords.Length()) {
    return nullptr;
  }

  if (recordExcludedCount == aRecords.Length()) {
    aRecordsAllExcluded = true;
    return nullptr;
  }

  // If all records are in http3 excluded list, try again without checking the
  // excluded list. This is better than returning nothing.
  if (h3ExcludedCount == aRecords.Length() && aCheckHttp3ExcludedList) {
    return GetServiceModeRecordInternal(aNoHttp2, aNoHttp3, aRecords,
                                        aRecordsAllExcluded, false);
  }

  return selectedRecord.forget();
}

void DNSHTTPSSVCRecordBase::GetAllRecordsWithEchConfigInternal(
    bool aNoHttp2, bool aNoHttp3, const nsTArray<SVCB>& aRecords,
    bool* aAllRecordsHaveEchConfig, bool* aAllRecordsInH3ExcludedList,
    nsTArray<RefPtr<nsISVCBRecord>>& aResult, bool aCheckHttp3ExcludedList) {
  if (aRecords.IsEmpty()) {
    return;
  }

  *aAllRecordsHaveEchConfig = aRecords[0].mHasEchConfig;
  *aAllRecordsInH3ExcludedList = false;
  // The first record should have echConfig.
  if (!(*aAllRecordsHaveEchConfig)) {
    return;
  }

  uint32_t h3ExcludedCount = 0;
  for (const SVCB& record : aRecords) {
    if (record.mSvcFieldPriority == 0) {
      // This should not happen, since GetAllRecordsWithEchConfigInternal()
      // should be called only if GetServiceModeRecordInternal() returns a
      // non-null record.
      MOZ_ASSERT(false);
      return;
    }

    // Records with echConfig are in front of records without echConfig, so we
    // don't have to continue.
    *aAllRecordsHaveEchConfig &= record.mHasEchConfig;
    if (!(*aAllRecordsHaveEchConfig)) {
      aResult.Clear();
      return;
    }

    Maybe<uint16_t> port = record.GetPort();
    if (port && *port == 0) {
      // Found an unsafe port, skip this record.
      continue;
    }

    Maybe<Tuple<nsCString, bool>> alpn = record.GetAlpn(aNoHttp2, aNoHttp3);
    if (alpn) {
      if (!CheckAlpnIsUsable(record.mSvcDomainName, Get<0>(*alpn),
                             aCheckHttp3ExcludedList && Get<1>(*alpn),
                             h3ExcludedCount)) {
        continue;
      }
    }

    RefPtr<nsISVCBRecord> svcbRecord =
        new SVCBRecord(record, std::move(port), std::move(alpn));
    aResult.AppendElement(svcbRecord);
  }

  // If all records are in http3 excluded list, try again without checking the
  // excluded list. This is better than returning nothing.
  if (h3ExcludedCount == aRecords.Length() && aCheckHttp3ExcludedList) {
    GetAllRecordsWithEchConfigInternal(
        aNoHttp2, aNoHttp3, aRecords, aAllRecordsHaveEchConfig,
        aAllRecordsInH3ExcludedList, aResult, false);
    *aAllRecordsInH3ExcludedList = true;
  }
}

bool DNSHTTPSSVCRecordBase::HasIPAddressesInternal(
    const nsTArray<SVCB>& aRecords) {
  for (const SVCB& record : aRecords) {
    if (record.mSvcFieldPriority != 0) {
      for (const auto& value : record.mSvcFieldValue) {
        if (value.mValue.is<SvcParamIpv4Hint>()) {
          return true;
        }
        if (value.mValue.is<SvcParamIpv6Hint>()) {
          return true;
        }
      }
    }
  }

  return false;
}

}  // namespace net
}  // namespace mozilla
