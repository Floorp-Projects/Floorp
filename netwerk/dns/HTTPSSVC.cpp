/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTTPSSVC.h"
#include "mozilla/net/DNS.h"
#include "nsNetAddr.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(SVCBRecord, nsISVCBRecord)

class SvcParam : public nsISVCParam,
                 public nsISVCParamAlpn,
                 public nsISVCParamNoDefaultAlpn,
                 public nsISVCParamPort,
                 public nsISVCParamIPv4Hint,
                 public nsISVCParamEsniConfig,
                 public nsISVCParamIPv6Hint {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISVCPARAM
  NS_DECL_NSISVCPARAMALPN
  NS_DECL_NSISVCPARAMNODEFAULTALPN
  NS_DECL_NSISVCPARAMPORT
  NS_DECL_NSISVCPARAMIPV4HINT
  NS_DECL_NSISVCPARAMESNICONFIG
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
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISVCParamEsniConfig,
                                     mValue.is<SvcParamEsniConfig>())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISVCParamIPv6Hint,
                                     mValue.is<SvcParamIpv6Hint>())
NS_INTERFACE_MAP_END

NS_IMETHODIMP
SvcParam::GetType(uint16_t* aType) {
  *aType = mValue.match(
      [](Nothing&) { return SvcParamKeyNone; },
      [](SvcParamAlpn&) { return SvcParamKeyAlpn; },
      [](SvcParamNoDefaultAlpn&) { return SvcParamKeyNoDefaultAlpn; },
      [](SvcParamPort&) { return SvcParamKeyPort; },
      [](SvcParamIpv4Hint&) { return SvcParamKeyIpv4Hint; },
      [](SvcParamEsniConfig&) { return SvcParamKeyEsniConfig; },
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
SvcParam::GetEsniConfig(nsACString& aEsniConfig) {
  if (!mValue.is<SvcParamEsniConfig>()) {
    MOZ_ASSERT(false, "Unexpected type for variant");
    return NS_ERROR_NOT_AVAILABLE;
  }
  aEsniConfig = mValue.as<SvcParamEsniConfig>().mValue;
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

NS_IMETHODIMP SVCBRecord::GetPriority(uint16_t* aPriority) {
  *aPriority = mData.mSvcFieldPriority;
  return NS_OK;
}

NS_IMETHODIMP SVCBRecord::GetName(nsACString& aName) {
  aName = mData.mSvcDomainName;
  return NS_OK;
}

NS_IMETHODIMP SVCBRecord::GetValues(nsTArray<RefPtr<nsISVCParam>>& aValues) {
  for (const auto& v : mData.mSvcFieldValue) {
    RefPtr<nsISVCParam> param = new SvcParam(v.mValue);
    aValues.AppendElement(param);
  }
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
