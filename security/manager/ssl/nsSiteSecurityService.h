/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsSiteSecurityService_h__
#define __nsSiteSecurityService_h__

#include "mozilla/BasePrincipal.h"
#include "mozilla/Dafsa.h"
#include "mozilla/DataStorage.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsISiteSecurityService.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozpkix/pkixtypes.h"
#include "prtime.h"

class nsIURI;
class nsITransportSecurityInfo;

using mozilla::OriginAttributes;

// {16955eee-6c48-4152-9309-c42a465138a1}
#define NS_SITE_SECURITY_SERVICE_CID                 \
  {                                                  \
    0x16955eee, 0x6c48, 0x4152, {                    \
      0x93, 0x09, 0xc4, 0x2a, 0x46, 0x51, 0x38, 0xa1 \
    }                                                \
  }

/**
 * SecurityPropertyState: A utility enum for representing the different states
 * a security property can be in.
 * SecurityPropertySet and SecurityPropertyUnset correspond to indicating
 * a site has or does not have the security property in question, respectively.
 * SecurityPropertyKnockout indicates a value on a preloaded list is being
 * overridden, and the associated site does not have the security property
 * in question.
 */
enum SecurityPropertyState {
  SecurityPropertyUnset = nsISiteSecurityState::SECURITY_PROPERTY_UNSET,
  SecurityPropertySet = nsISiteSecurityState::SECURITY_PROPERTY_SET,
  SecurityPropertyKnockout = nsISiteSecurityState::SECURITY_PROPERTY_KNOCKOUT,
  SecurityPropertyNegative = nsISiteSecurityState::SECURITY_PROPERTY_NEGATIVE,
};

enum SecurityPropertySource {
  SourceUnknown = nsISiteSecurityService::SOURCE_UNKNOWN,
  SourcePreload = nsISiteSecurityService::SOURCE_PRELOAD_LIST,
  SourceOrganic = nsISiteSecurityService::SOURCE_ORGANIC_REQUEST,
};

/**
 * SiteHSTSState: A utility class that encodes/decodes a string describing
 * the security state of a site. Currently only handles HSTS.
 * HSTS state consists of:
 *  - Hostname (nsCString)
 *  - Origin attributes (OriginAttributes)
 *  - Expiry time (PRTime (aka int64_t) in milliseconds)
 *  - A state flag (SecurityPropertyState, default SecurityPropertyUnset)
 *  - An include subdomains flag (bool, default false)
 */
class SiteHSTSState : public nsISiteHSTSState {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISITEHSTSSTATE
  NS_DECL_NSISITESECURITYSTATE

  SiteHSTSState(const nsCString& aHost,
                const OriginAttributes& aOriginAttributes,
                const nsCString& aStateString);
  SiteHSTSState(const nsCString& aHost,
                const OriginAttributes& aOriginAttributes,
                PRTime aHSTSExpireTime, SecurityPropertyState aHSTSState,
                bool aHSTSIncludeSubdomains, SecurityPropertySource aSource);

  nsCString mHostname;
  OriginAttributes mOriginAttributes;
  PRTime mHSTSExpireTime;
  SecurityPropertyState mHSTSState;
  bool mHSTSIncludeSubdomains;
  SecurityPropertySource mHSTSSource;

  bool IsExpired(uint32_t aType) {
    // If mHSTSExpireTime is 0, this entry never expires (this is the case for
    // knockout entries).
    if (mHSTSExpireTime == 0) {
      return false;
    }

    PRTime now = PR_Now() / PR_USEC_PER_MSEC;
    if (now > mHSTSExpireTime) {
      return true;
    }

    return false;
  }

  void ToString(nsCString& aString);

 protected:
  virtual ~SiteHSTSState() = default;
};

struct nsSTSPreload;

class nsSiteSecurityService : public nsISiteSecurityService,
                              public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSISITESECURITYSERVICE

  nsSiteSecurityService();
  nsresult Init();

 protected:
  virtual ~nsSiteSecurityService();

 private:
  nsresult GetHost(nsIURI* aURI, nsACString& aResult);
  nsresult SetHSTSState(uint32_t aType, const char* aHost, int64_t maxage,
                        bool includeSubdomains, uint32_t flags,
                        SecurityPropertyState aHSTSState,
                        SecurityPropertySource aSource,
                        const OriginAttributes& aOriginAttributes);
  nsresult ProcessHeaderInternal(
      uint32_t aType, nsIURI* aSourceURI, const nsCString& aHeader,
      nsITransportSecurityInfo* aSecInfo, uint32_t aFlags,
      SecurityPropertySource aSource, const OriginAttributes& aOriginAttributes,
      uint64_t* aMaxAge, bool* aIncludeSubdomains, uint32_t* aFailureResult);
  nsresult ProcessSTSHeader(nsIURI* aSourceURI, const nsCString& aHeader,
                            uint32_t flags, SecurityPropertySource aSource,
                            const OriginAttributes& aOriginAttributes,
                            uint64_t* aMaxAge, bool* aIncludeSubdomains,
                            uint32_t* aFailureResult);
  nsresult MarkHostAsNotHSTS(uint32_t aType, const nsAutoCString& aHost,
                             uint32_t aFlags, bool aIsPreload,
                             const OriginAttributes& aOriginAttributes);
  nsresult ResetStateInternal(uint32_t aType, nsIURI* aURI, uint32_t aFlags,
                              const OriginAttributes& aOriginAttributes);
  bool HostHasHSTSEntry(const nsAutoCString& aHost,
                        bool aRequireIncludeSubdomains, uint32_t aFlags,
                        const OriginAttributes& aOriginAttributes,
                        bool* aResult, bool* aCached,
                        SecurityPropertySource* aSource);
  bool GetPreloadStatus(
      const nsACString& aHost,
      /*optional out*/ bool* aIncludeSubdomains = nullptr) const;
  nsresult IsSecureHost(uint32_t aType, const nsACString& aHost,
                        uint32_t aFlags,
                        const OriginAttributes& aOriginAttributes,
                        bool* aCached, SecurityPropertySource* aSource,
                        bool* aResult);

  bool mUsePreloadList;
  int64_t mPreloadListTimeOffset;
  RefPtr<mozilla::DataStorage> mSiteStateStorage;
  RefPtr<mozilla::DataStorage> mPreloadStateStorage;
  const mozilla::Dafsa mDafsa;
};

#endif  // __nsSiteSecurityService_h__
