/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpAuthCache_h__
#define nsHttpAuthCache_h__

#include "nsError.h"
#include "nsTArray.h"
#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsStringFwd.h"
#include "nsIObserver.h"

namespace mozilla {

class OriginAttributesPattern;

namespace net {

struct nsHttpAuthPath {
  struct nsHttpAuthPath* mNext;
  char mPath[1];
};

//-----------------------------------------------------------------------------
// nsHttpAuthIdentity
//-----------------------------------------------------------------------------

class nsHttpAuthIdentity {
 public:
  nsHttpAuthIdentity() = default;
  nsHttpAuthIdentity(const nsAString& domain, const nsAString& user,
                     const nsAString& password)
      : mUser(user), mPass(password), mDomain(domain) {}
  ~nsHttpAuthIdentity() { Clear(); }

  const nsString& Domain() const { return mDomain; }
  const nsString& User() const { return mUser; }
  const nsString& Password() const { return mPass; }

  void Clear();

  bool Equals(const nsHttpAuthIdentity& ident) const;
  bool IsEmpty() const {
    return mUser.IsEmpty() && mPass.IsEmpty() && mDomain.IsEmpty();
  }

 private:
  nsString mUser;
  nsString mPass;
  nsString mDomain;
};

//-----------------------------------------------------------------------------
// nsHttpAuthEntry
//-----------------------------------------------------------------------------

class nsHttpAuthEntry {
 public:
  const nsCString& Realm() const { return mRealm; }
  const nsCString& Creds() const { return mCreds; }
  const nsCString& Challenge() const { return mChallenge; }
  const nsString& Domain() const { return mIdent.Domain(); }
  const nsString& User() const { return mIdent.User(); }
  const nsString& Pass() const { return mIdent.Password(); }
  nsHttpAuthPath* RootPath() { return mRoot; }

  const nsHttpAuthIdentity& Identity() const { return mIdent; }

  [[nodiscard]] nsresult AddPath(const nsACString& aPath);

  nsCOMPtr<nsISupports> mMetaData;

 private:
  nsHttpAuthEntry(const nsACString& path, const nsACString& realm,
                  const nsACString& creds, const nsACString& challenge,
                  const nsHttpAuthIdentity* ident, nsISupports* metadata)
      : mRoot(nullptr), mTail(nullptr) {
    DebugOnly<nsresult> rv =
        Set(path, realm, creds, challenge, ident, metadata);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
  ~nsHttpAuthEntry();

  [[nodiscard]] nsresult Set(const nsACString& path, const nsACString& realm,
                             const nsACString& creds,
                             const nsACString& challenge,
                             const nsHttpAuthIdentity* ident,
                             nsISupports* metadata);

  nsHttpAuthIdentity mIdent;

  nsHttpAuthPath* mRoot;  // root pointer
  nsHttpAuthPath* mTail;  // tail pointer

  nsCString mRealm;
  nsCString mCreds;
  nsCString mChallenge;

  friend class nsHttpAuthNode;
  friend class nsHttpAuthCache;
  friend class mozilla::DefaultDelete<nsHttpAuthEntry>;  // needs to call the
                                                         // destructor
};

//-----------------------------------------------------------------------------
// nsHttpAuthNode
//-----------------------------------------------------------------------------

class nsHttpAuthNode {
 private:
  using EntryList = nsTArray<UniquePtr<nsHttpAuthEntry>>;

  nsHttpAuthNode();
  ~nsHttpAuthNode();

  // path can be null, in which case we'll search for an entry
  // with a null path.
  nsHttpAuthEntry* LookupEntryByPath(const nsACString& path);

  // realm must not be null
  nsHttpAuthEntry* LookupEntryByRealm(const nsACString& realm);
  EntryList::const_iterator LookupEntryItrByRealm(
      const nsACString& realm) const;

  // if a matching entry is found, then credentials will be changed.
  [[nodiscard]] nsresult SetAuthEntry(const nsACString& path,
                                      const nsACString& realm,
                                      const nsACString& creds,
                                      const nsACString& challenge,
                                      const nsHttpAuthIdentity* ident,
                                      nsISupports* metadata);

  void ClearAuthEntry(const nsACString& realm);

  uint32_t EntryCount() { return mList.Length(); }

 private:
  EntryList mList;

  friend class nsHttpAuthCache;
  friend class mozilla::DefaultDelete<nsHttpAuthNode>;  // needs to call the
                                                        // destructor
};

//-----------------------------------------------------------------------------
// nsHttpAuthCache
//  (holds a hash table from host:port to nsHttpAuthNode)
//-----------------------------------------------------------------------------

class nsHttpAuthCache {
 public:
  nsHttpAuthCache();
  ~nsHttpAuthCache();

  // |scheme|, |host|, and |port| are required
  // |path| can be null
  // |entry| is either null or a weak reference
  [[nodiscard]] nsresult GetAuthEntryForPath(const nsACString& scheme,
                                             const nsACString& host,
                                             int32_t port,
                                             const nsACString& path,
                                             nsACString const& originSuffix,
                                             nsHttpAuthEntry** entry);

  // |scheme|, |host|, and |port| are required
  // |realm| must not be null
  // |entry| is either null or a weak reference
  [[nodiscard]] nsresult GetAuthEntryForDomain(const nsACString& scheme,
                                               const nsACString& host,
                                               int32_t port,
                                               const nsACString& realm,
                                               nsACString const& originSuffix,
                                               nsHttpAuthEntry** entry);

  // |scheme|, |host|, and |port| are required
  // |path| can be null
  // |realm| must not be null
  // if |credentials|, |user|, |pass|, and |challenge| are each
  // null, then the entry is deleted.
  [[nodiscard]] nsresult SetAuthEntry(
      const nsACString& scheme, const nsACString& host, int32_t port,
      const nsACString& directory, const nsACString& realm,
      const nsACString& credentials, const nsACString& challenge,
      nsACString const& originSuffix, const nsHttpAuthIdentity* ident,
      nsISupports* metadata);

  void ClearAuthEntry(const nsACString& scheme, const nsACString& host,
                      int32_t port, const nsACString& realm,
                      nsACString const& originSuffix);

  // expire all existing auth list entries including proxy auths.
  void ClearAll();

  // For testing only.
  void CollectKeys(nsTArray<nsCString>& aValue);

 private:
  nsHttpAuthNode* LookupAuthNode(const nsACString& scheme,
                                 const nsACString& host, int32_t port,
                                 nsACString const& originSuffix,
                                 nsCString& key);

  class OriginClearObserver : public nsIObserver {
    virtual ~OriginClearObserver() = default;

   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    explicit OriginClearObserver(nsHttpAuthCache* aOwner) : mOwner(aOwner) {}
    nsHttpAuthCache* mOwner;
  };

  void ClearOriginData(OriginAttributesPattern const& pattern);

 private:
  using AuthNodeTable = nsClassHashtable<nsCStringHashKey, nsHttpAuthNode>;
  AuthNodeTable mDB;  // "host:port" --> nsHttpAuthNode
  RefPtr<OriginClearObserver> mObserver;
};

}  // namespace net
}  // namespace mozilla

#endif  // nsHttpAuthCache_h__
