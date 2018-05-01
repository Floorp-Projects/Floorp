/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRedirectHistoryEntry_h__
#define nsRedirectHistoryEntry_h__

#include "nsIRedirectHistoryEntry.h"

class nsIURI;
class nsIPrincipal;

namespace mozilla {
namespace net {

class nsRedirectHistoryEntry final : public nsIRedirectHistoryEntry
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREDIRECTHISTORYENTRY

  nsRedirectHistoryEntry(nsIPrincipal* aPrincipal, nsIURI* aReferrer,
                         const nsACString& aRemoteAddress);

private:
  ~nsRedirectHistoryEntry() = default;

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIURI> mReferrer;
  nsCString mRemoteAddress;

};

} // namespace net
} // namespace mozilla

#endif // nsRedirectHistoryEntry_h__
