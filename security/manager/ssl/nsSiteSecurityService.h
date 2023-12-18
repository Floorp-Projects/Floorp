/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsSiteSecurityService_h__
#define __nsSiteSecurityService_h__

#include "mozilla/BasePrincipal.h"
#include "mozilla/Dafsa.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIDataStorage.h"
#include "nsIObserver.h"
#include "nsISiteSecurityService.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozpkix/pkixtypes.h"
#include "prtime.h"

class nsIURI;

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
  SecurityPropertyUnset = 0,
  SecurityPropertySet = 1,
  SecurityPropertyKnockout = 2,
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
class SiteHSTSState {
 public:
  SiteHSTSState(const nsCString& aHost,
                const OriginAttributes& aOriginAttributes,
                const nsCString& aStateString);
  SiteHSTSState(const nsCString& aHost,
                const OriginAttributes& aOriginAttributes,
                PRTime aHSTSExpireTime, SecurityPropertyState aHSTSState,
                bool aHSTSIncludeSubdomains);

  nsCString mHostname;
  OriginAttributes mOriginAttributes;
  PRTime mHSTSExpireTime;
  SecurityPropertyState mHSTSState;
  bool mHSTSIncludeSubdomains;

  bool IsExpired() {
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

  static nsresult GetHost(nsIURI* aURI, nsACString& aResult);
  static bool HostIsIPAddress(const nsCString& hostname);

 protected:
  virtual ~nsSiteSecurityService();

 private:
  nsresult SetHSTSState(const char* aHost, int64_t maxage,
                        bool includeSubdomains,
                        SecurityPropertyState aHSTSState,
                        const OriginAttributes& aOriginAttributes);
  nsresult ProcessHeaderInternal(nsIURI* aSourceURI, const nsCString& aHeader,
                                 const OriginAttributes& aOriginAttributes,
                                 uint64_t* aMaxAge, bool* aIncludeSubdomains,
                                 uint32_t* aFailureResult);
  nsresult ProcessSTSHeader(nsIURI* aSourceURI, const nsCString& aHeader,
                            const OriginAttributes& aOriginAttributes,
                            uint64_t* aMaxAge, bool* aIncludeSubdomains,
                            uint32_t* aFailureResult);
  nsresult MarkHostAsNotHSTS(const nsAutoCString& aHost,
                             const OriginAttributes& aOriginAttributes);
  nsresult ResetStateInternal(nsIURI* aURI,
                              const OriginAttributes& aOriginAttributes,
                              nsISiteSecurityService::ResetStateBy aScope);
  void ResetStateForExactDomain(const nsCString& aHostname,
                                const OriginAttributes& aOriginAttributes);
  nsresult HostMatchesHSTSEntry(const nsAutoCString& aHost,
                                bool aRequireIncludeSubdomains,
                                const OriginAttributes& aOriginAttributes,
                                bool& aHostMatchesHSTSEntry);
  bool GetPreloadStatus(
      const nsACString& aHost,
      /*optional out*/ bool* aIncludeSubdomains = nullptr) const;
  nsresult IsSecureHost(const nsACString& aHost,
                        const OriginAttributes& aOriginAttributes,
                        bool* aResult);

  nsresult GetWithMigration(const nsACString& aHostname,
                            const OriginAttributes& aOriginAttributes,
                            nsIDataStorage::DataType aDataStorageType,
                            nsACString& aValue);
  nsresult PutWithMigration(const nsACString& aHostname,
                            const OriginAttributes& aOriginAttributes,
                            nsIDataStorage::DataType aDataStorageType,
                            const nsACString& aStateString);
  nsresult RemoveWithMigration(const nsACString& aHostname,
                               const OriginAttributes& aOriginAttributes,
                               nsIDataStorage::DataType aDataStorageType);

  bool mUsePreloadList;
  int64_t mPreloadListTimeOffset;
  nsCOMPtr<nsIDataStorage> mSiteStateStorage;
  const mozilla::Dafsa mDafsa;
};

#endif  // __nsSiteSecurityService_h__
