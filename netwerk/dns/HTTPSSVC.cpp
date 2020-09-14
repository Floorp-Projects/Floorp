/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTTPSSVC.h"
#include "mozilla/net/DNS.h"
#include "nsHttp.h"
#include "nsNetAddr.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(SVCBRecord, nsISVCBRecord)

class SvcParam : public nsISVCParam,
                 public nsISVCParamAlpn,
                 public nsISVCParamNoDefaultAlpn,
                 public nsISVCParamPort,
                 public nsISVCParamIPv4Hint,
                 public nsISVCParamEchConfig,
                 public nsISVCParamIPv6Hint {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISVCPARAM
  NS_DECL_NSISVCPARAMALPN
  NS_DECL_NSISVCPARAMNODEFAULTALPN
  NS_DECL_NSISVCPARAMPORT
  NS_DECL_NSISVCPARAMIPV4HINT
  NS_DECL_NSISVCPARAMECHCONFIG
  NS_DECL_NSISVCPARAMIPV6HINT
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
      [](SvcParamIpv6Hint&) { return SvcParamKeyIpv6Hint; });
  return NS_OK;
}

NS_IMETHODIMP
SvcParam::GetAlpn(nsACString& aAlpn) {
  if (!mValue.is<SvcParamAlpn>()) {
    MOZ_ASSERT(false, "Unexpected type for variant");
    return NS_ERROR_NOT_AVAILABLE;
  }
  aAlpn = mValue.as<SvcParamAlpn>().mValue;
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

Maybe<nsCString> SVCB::GetAlpn(bool aNoHttp2, bool aNoHttp3) const {
  Maybe<nsCString> alpn;
  nsAutoCString alpnValue;
  for (const auto& value : mSvcFieldValue) {
    if (value.mValue.is<SvcParamAlpn>()) {
      alpn.emplace();
      alpnValue = value.mValue.as<SvcParamAlpn>().mValue;
      if (!alpnValue.IsEmpty()) {
        alpn->Assign(SelectAlpnFromAlpnList(alpnValue, aNoHttp2, aNoHttp3));
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

Maybe<nsCString> SVCBRecord::GetAlpn() { return mAlpn; }

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

already_AddRefed<nsISVCBRecord>
DNSHTTPSSVCRecordBase::GetServiceModeRecordInternal(
    bool aNoHttp2, bool aNoHttp3, const nsTArray<SVCB>& aRecords,
    bool& aRecordsAllExcluded) {
  nsCOMPtr<nsISVCBRecord> selectedRecord;
  uint32_t recordHasNoDefaultAlpnCount = 0;
  uint32_t recordExcludedCount = 0;
  aRecordsAllExcluded = false;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  for (const SVCB& record : aRecords) {
    if (record.mSvcFieldPriority == 0) {
      // In ServiceMode, the SvcPriority should never be 0.
      return nullptr;
    }

    if (record.NoDefaultAlpn()) {
      ++recordHasNoDefaultAlpnCount;
    }

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

    Maybe<nsCString> alpn = record.GetAlpn(aNoHttp2, aNoHttp3);
    if (alpn && alpn->IsEmpty()) {
      // Can't find any supported protocols, skip.
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
  }

  return selectedRecord.forget();
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
