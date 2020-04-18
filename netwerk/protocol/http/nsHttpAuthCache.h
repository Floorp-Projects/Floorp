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
  nsHttpAuthIdentity() : mUser(nullptr), mPass(nullptr), mDomain(nullptr) {}
  nsHttpAuthIdentity(const char16_t* domain, const char16_t* user,
                     const char16_t* password)
      : mUser(nullptr), mPass{nullptr}, mDomain{nullptr} {
    DebugOnly<nsresult> rv = Set(domain, user, password);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
  ~nsHttpAuthIdentity() { Clear(); }

  const char16_t* Domain() const { return mDomain; }
  const char16_t* User() const { return mUser; }
  const char16_t* Password() const { return mPass; }

  [[nodiscard]] nsresult Set(const char16_t* domain, const char16_t* user,
                             const char16_t* password);
  [[nodiscard]] nsresult Set(const nsHttpAuthIdentity& other) {
    return Set(other.mDomain, other.mUser, other.mPass);
  }
  void Clear();

  bool Equals(const nsHttpAuthIdentity& other) const;
  bool IsEmpty() const { return !mUser; }

 private:
  // allocated as one contiguous blob, starting at mUser.
  char16_t* mUser;
  char16_t* mPass;
  char16_t* mDomain;
};

//-----------------------------------------------------------------------------
// nsHttpAuthEntry
//-----------------------------------------------------------------------------

class nsHttpAuthEntry {
 public:
  const char* Realm() const { return mRealm; }
  const char* Creds() const { return mCreds; }
  const char* Challenge() const { return mChallenge; }
  const char16_t* Domain() const { return mIdent.Domain(); }
  const char16_t* User() const { return mIdent.User(); }
  const char16_t* Pass() const { return mIdent.Password(); }
  nsHttpAuthPath* RootPath() { return mRoot; }

  const nsHttpAuthIdentity& Identity() const { return mIdent; }

  [[nodiscard]] nsresult AddPath(const char* aPath);

  nsCOMPtr<nsISupports> mMetaData;

 private:
  nsHttpAuthEntry(const char* path, const char* realm, const char* creds,
                  const char* challenge, const nsHttpAuthIdentity* ident,
                  nsISupports* metadata)
      : mRoot(nullptr),
        mTail(nullptr),
        mRealm(nullptr),
        mCreds{nullptr},
        mChallenge{nullptr} {
    DebugOnly<nsresult> rv =
        Set(path, realm, creds, challenge, ident, metadata);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
  ~nsHttpAuthEntry();

  [[nodiscard]] nsresult Set(const char* path, const char* realm,
                             const char* creds, const char* challenge,
                             const nsHttpAuthIdentity* ident,
                             nsISupports* metadata);

  nsHttpAuthIdentity mIdent;

  nsHttpAuthPath* mRoot;  // root pointer
  nsHttpAuthPath* mTail;  // tail pointer

  // allocated together in one blob, starting with mRealm.
  char* mRealm;
  char* mCreds;
  char* mChallenge;

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
  nsHttpAuthEntry* LookupEntryByPath(const char* path);

  // realm must not be null
  nsHttpAuthEntry* LookupEntryByRealm(const char* realm);
  EntryList::const_iterator LookupEntryItrByRealm(const char* realm) const;

  // if a matching entry is found, then credentials will be changed.
  [[nodiscard]] nsresult SetAuthEntry(const char* path, const char* realm,
                                      const char* credentials,
                                      const char* challenge,
                                      const nsHttpAuthIdentity* ident,
                                      nsISupports* metadata);

  void ClearAuthEntry(const char* realm);

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
  [[nodiscard]] nsresult GetAuthEntryForPath(const char* scheme,
                                             const char* host, int32_t port,
                                             const char* path,
                                             nsACString const& originSuffix,
                                             nsHttpAuthEntry** entry);

  // |scheme|, |host|, and |port| are required
  // |realm| must not be null
  // |entry| is either null or a weak reference
  [[nodiscard]] nsresult GetAuthEntryForDomain(const char* scheme,
                                               const char* host, int32_t port,
                                               const char* realm,
                                               nsACString const& originSuffix,
                                               nsHttpAuthEntry** entry);

  // |scheme|, |host|, and |port| are required
  // |path| can be null
  // |realm| must not be null
  // if |credentials|, |user|, |pass|, and |challenge| are each
  // null, then the entry is deleted.
  [[nodiscard]] nsresult SetAuthEntry(
      const char* scheme, const char* host, int32_t port, const char* directory,
      const char* realm, const char* credentials, const char* challenge,
      nsACString const& originSuffix, const nsHttpAuthIdentity* ident,
      nsISupports* metadata);

  void ClearAuthEntry(const char* scheme, const char* host, int32_t port,
                      const char* realm, nsACString const& originSuffix);

  // expire all existing auth list entries including proxy auths.
  void ClearAll();

 private:
  nsHttpAuthNode* LookupAuthNode(const char* scheme, const char* host,
                                 int32_t port, nsACString const& originSuffix,
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
