/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This wraps nsSimpleURI so that all calls to it are done on the main thread.
 */

#ifndef __nsSiteSecurityService_h__
#define __nsSiteSecurityService_h__

#include "nsISiteSecurityService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "prtime.h"

// {16955eee-6c48-4152-9309-c42a465138a1}
#define NS_SITE_SECURITY_SERVICE_CID \
  {0x16955eee, 0x6c48, 0x4152, \
    {0x93, 0x09, 0xc4, 0x2a, 0x46, 0x51, 0x38, 0xa1} }

////////////////////////////////////////////////////////////////////////////////
// nsSSSHostEntry - similar to the nsHostEntry class in
// nsPermissionManager.cpp, but specific to private-mode caching of STS
// permissions.
//
// Each nsSSSHostEntry contains:
//  - Expiry time (PRTime, milliseconds)
//  - Expired flag (bool, default false)
//  - STS permission (uint32_t, default STS_UNSET)
//  - Include subdomains flag (bool, default false)
//
// Note: the subdomains flag has no meaning if the STS permission is STS_UNSET.
//
// The existence of the nsSSSHostEntry implies STS state is set for the given
// host -- unless the expired flag is set, in which case not only is the STS
// state not set for the host, but any permission actually present in the
// permission manager should be ignored.
//
// Note: Only one expiry time is stored since the subdomains and STS
// permissions are both encountered at the same time in the HTTP header; if the
// includeSubdomains directive isn't present in the header, it means to delete
// the permission, so the subdomains flag in the nsSSSHostEntry means both that
// the permission doesn't exist and any permission in the real permission
// manager should be ignored since newer information about it has been
// encountered in private browsing mode.
//
// Note: If there's a permission set by the user (EXPIRE_NEVER), STS is not set
// for the host (including the subdomains permission) when the header is
// encountered.  Furthermore, any user-set permissions are stored persistently
// and can't be shadowed.

class nsSSSHostEntry : public PLDHashEntryHdr
{
  public:
    explicit nsSSSHostEntry(const char* aHost);
    explicit nsSSSHostEntry(const nsSSSHostEntry& toCopy);

    nsCString    mHost;
    PRTime       mExpireTime;
    uint32_t     mStsPermission;
    bool         mExpired;
    bool         mIncludeSubdomains;

    // Hash methods
    typedef const char* KeyType;
    typedef const char* KeyTypePointer;

    KeyType GetKey() const
    {
      return mHost.get();
    }

    bool KeyEquals(KeyTypePointer aKey) const
    {
      return !strcmp(mHost.get(), aKey);
    }

    static KeyTypePointer KeyToPointer(KeyType aKey)
    {
      return aKey;
    }

    static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
      return PL_DHashStringKey(nullptr, aKey);
    }

    void SetExpireTime(PRTime aExpireTime)
    {
      mExpireTime = aExpireTime;
      mExpired = false;
    }

    bool IsExpired()
    {
      // If mExpireTime is 0, this entry never expires (this is the case for
      // knockout entries).
      // If we've already expired or we never expire, return early.
      if (mExpired || mExpireTime == 0) {
        return mExpired;
      }

      PRTime now = PR_Now() / PR_USEC_PER_MSEC;
      if (now > mExpireTime) {
        mExpired = true;
      }

      return mExpired;
    }

    // force the hashtable to use the copy constructor.
    enum { ALLOW_MEMMOVE = false };
};
////////////////////////////////////////////////////////////////////////////////

class nsSTSPreload;

class nsSiteSecurityService : public nsISiteSecurityService
                            , public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSISITESECURITYSERVICE

  nsSiteSecurityService();
  nsresult Init();
  virtual ~nsSiteSecurityService();

private:
  nsresult GetHost(nsIURI *aURI, nsACString &aResult);
  nsresult GetPrincipalForURI(nsIURI *aURI, nsIPrincipal **aPrincipal);
  nsresult SetState(uint32_t aType, nsIURI* aSourceURI, int64_t maxage,
                    bool includeSubdomains, uint32_t flags);
  nsresult ProcessHeaderMutating(uint32_t aType, nsIURI* aSourceURI,
                                 char* aHeader, uint32_t flags,
                                 uint64_t *aMaxAge, bool *aIncludeSubdomains);
  const nsSTSPreload *GetPreloadListEntry(const char *aHost);

  // private-mode-preserving permission manager overlay functions
  nsresult AddPermission(nsIURI     *aURI,
                         const char *aType,
                         uint32_t   aPermission,
                         uint32_t   aExpireType,
                         int64_t    aExpireTime,
                         bool       aIsPrivate);
  nsresult RemovePermission(const nsCString  &aHost,
                            const char       *aType,
                            bool              aIsPrivate);

  // cached services
  nsCOMPtr<nsIPermissionManager> mPermMgr;
  nsCOMPtr<nsIObserverService> mObserverService;

  nsTHashtable<nsSSSHostEntry> mPrivateModeHostTable;
  bool mUsePreloadList;
};

#endif // __nsSiteSecurityService_h__
