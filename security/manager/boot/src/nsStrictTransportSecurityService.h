/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Strict-Transport-Security.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Sid Stamm <sid@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * This wraps nsSimpleURI so that all calls to it are done on the main thread.
 */

#ifndef __nsStrictTransportSecurityService_h__
#define __nsStrictTransportSecurityService_h__

#include "nsIStrictTransportSecurityService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsTHashtable.h"

// {16955eee-6c48-4152-9309-c42a465138a1}
#define NS_STRICT_TRANSPORT_SECURITY_CID \
  {0x16955eee, 0x6c48, 0x4152, \
    {0x93, 0x09, 0xc4, 0x2a, 0x46, 0x51, 0x38, 0xa1} }

////////////////////////////////////////////////////////////////////////////////
// nsSTSHostEntry - similar to the nsHostEntry class in
// nsPermissionManager.cpp, but specific to private-mode caching of STS
// permissions.
//
// Each nsSTSHostEntry contains:
//  - Expiry time
//  - Deleted flag (boolean, default PR_FALSE)
//  - Subdomains flag (boolean, default PR_FALSE)
//
// The existence of the nsSTSHostEntry implies STS state is set for the given
// host -- unless the deleted flag is set, in which case not only is the STS
// state not set for the host, but any permission actually present in the
// permission manager should be ignored.
//
// Note: Only one expiry time is stored since the subdomains and STS
// permissions are both encountered at the same time in the HTTP header; if the
// includeSubdomains directive isn't present in the header, it means to delete
// the permission, so the subdomains flag in the nsSTSHostEntry means both that
// the permission doesn't exist and any permission in the real permission
// manager should be ignored since newer information about it has been
// encountered in private browsing mode.
//
// Note: If there's a permission set by the user (EXPIRE_NEVER), STS is not set
// for the host (including the subdomains permission) when the header is
// encountered.  Furthermore, any user-set permissions are stored persistently
// and can't be shadowed.

class nsSTSHostEntry : public PLDHashEntryHdr
{
  public:
    explicit nsSTSHostEntry(const char* aHost);
    explicit nsSTSHostEntry(const nsSTSHostEntry& toCopy);

    nsCString    mHost;
    PRInt64      mExpireTime;
    bool mDeleted;
    bool mIncludeSubdomains;

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
      return PL_DHashStringKey(nsnull, aKey);
    }

    // force the hashtable to use the copy constructor.
    enum { ALLOW_MEMMOVE = PR_FALSE };
};
////////////////////////////////////////////////////////////////////////////////

class nsStrictTransportSecurityService : public nsIStrictTransportSecurityService
                                       , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSISTRICTTRANSPORTSECURITYSERVICE

  nsStrictTransportSecurityService();
  nsresult Init();
  virtual ~nsStrictTransportSecurityService();

private:
  nsresult GetHost(nsIURI *aURI, nsACString &aResult);
  nsresult SetStsState(nsIURI* aSourceURI, PRInt64 maxage, bool includeSubdomains);
  nsresult ProcessStsHeaderMutating(nsIURI* aSourceURI, char* aHeader);

  // private-mode-preserving permission manager overlay functions
  nsresult AddPermission(nsIURI     *aURI,
                         const char *aType,
                         PRUint32   aPermission,
                         PRUint32   aExpireType,
                         PRInt64    aExpireTime);
  nsresult RemovePermission(const nsCString  &aHost,
                            const char       *aType);
  nsresult TestPermission(nsIURI     *aURI,
                          const char *aType,
                          PRUint32   *aPermission,
                          bool       testExact);

  // cached services
  nsCOMPtr<nsIPermissionManager> mPermMgr;
  nsCOMPtr<nsIObserverService> mObserverService;

  bool mInPrivateMode;
  nsTHashtable<nsSTSHostEntry> mPrivateModeHostTable;
};

#endif // __nsStrictTransportSecurityService_h__
