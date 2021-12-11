/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttpAuthCache.h"

#include <algorithm>
#include <stdlib.h>

#include "mozilla/Attributes.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/DebugOnly.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

static inline void GetAuthKey(const nsACString& scheme, const nsACString& host,
                              int32_t port, nsACString const& originSuffix,
                              nsCString& key) {
  key.Truncate();
  key.Append(originSuffix);
  key.Append(':');
  key.Append(scheme);
  key.AppendLiteral("://");
  key.Append(host);
  key.Append(':');
  key.AppendInt(port);
}

//-----------------------------------------------------------------------------
// nsHttpAuthCache <public>
//-----------------------------------------------------------------------------

nsHttpAuthCache::nsHttpAuthCache()
    : mDB(128), mObserver(new OriginClearObserver(this)) {
  LOG(("nsHttpAuthCache::nsHttpAuthCache %p", this));

  nsCOMPtr<nsIObserverService> obsSvc = services::GetObserverService();
  if (obsSvc) {
    obsSvc->AddObserver(mObserver, "clear-origin-attributes-data", false);
  }
}

nsHttpAuthCache::~nsHttpAuthCache() {
  LOG(("nsHttpAuthCache::~nsHttpAuthCache %p", this));

  ClearAll();
  nsCOMPtr<nsIObserverService> obsSvc = services::GetObserverService();
  if (obsSvc) {
    obsSvc->RemoveObserver(mObserver, "clear-origin-attributes-data");
    mObserver->mOwner = nullptr;
  }
}

nsresult nsHttpAuthCache::GetAuthEntryForPath(const nsACString& scheme,
                                              const nsACString& host,
                                              int32_t port,
                                              const nsACString& path,
                                              nsACString const& originSuffix,
                                              nsHttpAuthEntry** entry) {
  LOG(("nsHttpAuthCache::GetAuthEntryForPath %p [path=%s]\n", this,
       path.BeginReading()));

  nsAutoCString key;
  nsHttpAuthNode* node = LookupAuthNode(scheme, host, port, originSuffix, key);
  if (!node) return NS_ERROR_NOT_AVAILABLE;

  *entry = node->LookupEntryByPath(path);
  LOG(("  returning %p", *entry));
  return *entry ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

nsresult nsHttpAuthCache::GetAuthEntryForDomain(const nsACString& scheme,
                                                const nsACString& host,
                                                int32_t port,
                                                const nsACString& realm,
                                                nsACString const& originSuffix,
                                                nsHttpAuthEntry** entry)

{
  LOG(("nsHttpAuthCache::GetAuthEntryForDomain %p [realm=%s]\n", this,
       realm.BeginReading()));

  nsAutoCString key;
  nsHttpAuthNode* node = LookupAuthNode(scheme, host, port, originSuffix, key);
  if (!node) return NS_ERROR_NOT_AVAILABLE;

  *entry = node->LookupEntryByRealm(realm);
  LOG(("  returning %p", *entry));
  return *entry ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

nsresult nsHttpAuthCache::SetAuthEntry(
    const nsACString& scheme, const nsACString& host, int32_t port,
    const nsACString& path, const nsACString& realm, const nsACString& creds,
    const nsACString& challenge, nsACString const& originSuffix,
    const nsHttpAuthIdentity* ident, nsISupports* metadata) {
  nsresult rv;

  LOG(("nsHttpAuthCache::SetAuthEntry %p [realm=%s path=%s metadata=%p]\n",
       this, realm.BeginReading(), path.BeginReading(), metadata));

  nsAutoCString key;
  nsHttpAuthNode* node = LookupAuthNode(scheme, host, port, originSuffix, key);

  if (!node) {
    // create a new entry node and set the given entry
    auto node = UniquePtr<nsHttpAuthNode>(new nsHttpAuthNode);
    LOG(("  new nsHttpAuthNode %p for key='%s'", node.get(), key.get()));
    rv = node->SetAuthEntry(path, realm, creds, challenge, ident, metadata);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mDB.InsertOrUpdate(key, std::move(node));
    return NS_OK;
  }

  return node->SetAuthEntry(path, realm, creds, challenge, ident, metadata);
}

void nsHttpAuthCache::ClearAuthEntry(const nsACString& scheme,
                                     const nsACString& host, int32_t port,
                                     const nsACString& realm,
                                     nsACString const& originSuffix) {
  nsAutoCString key;
  GetAuthKey(scheme, host, port, originSuffix, key);
  LOG(("nsHttpAuthCache::ClearAuthEntry %p key='%s'\n", this, key.get()));
  mDB.Remove(key);
}

void nsHttpAuthCache::ClearAll() {
  LOG(("nsHttpAuthCache::ClearAll %p\n", this));
  mDB.Clear();
}

//-----------------------------------------------------------------------------
// nsHttpAuthCache <private>
//-----------------------------------------------------------------------------

nsHttpAuthNode* nsHttpAuthCache::LookupAuthNode(const nsACString& scheme,
                                                const nsACString& host,
                                                int32_t port,
                                                nsACString const& originSuffix,
                                                nsCString& key) {
  GetAuthKey(scheme, host, port, originSuffix, key);
  nsHttpAuthNode* result = mDB.Get(key);

  LOG(("nsHttpAuthCache::LookupAuthNode %p key='%s' found node=%p", this,
       key.get(), result));
  return result;
}

NS_IMPL_ISUPPORTS(nsHttpAuthCache::OriginClearObserver, nsIObserver)

NS_IMETHODIMP
nsHttpAuthCache::OriginClearObserver::Observe(nsISupports* subject,
                                              const char* topic,
                                              const char16_t* data_unicode) {
  NS_ENSURE_TRUE(mOwner, NS_ERROR_NOT_AVAILABLE);

  OriginAttributesPattern pattern;
  if (!pattern.Init(nsDependentString(data_unicode))) {
    NS_ERROR("Cannot parse origin attributes pattern");
    return NS_ERROR_FAILURE;
  }

  mOwner->ClearOriginData(pattern);
  return NS_OK;
}

void nsHttpAuthCache::ClearOriginData(OriginAttributesPattern const& pattern) {
  LOG(("nsHttpAuthCache::ClearOriginData %p", this));

  for (auto iter = mDB.Iter(); !iter.Done(); iter.Next()) {
    const nsACString& key = iter.Key();

    // Extract the origin attributes suffix from the key.
    int32_t colon = key.FindChar(':');
    MOZ_ASSERT(colon != kNotFound);
    nsDependentCSubstring oaSuffix = StringHead(key, colon);

    // Build the OriginAttributes object of it...
    OriginAttributes oa;
    DebugOnly<bool> rv = oa.PopulateFromSuffix(oaSuffix);
    MOZ_ASSERT(rv);

    // ...and match it against the given pattern.
    if (pattern.Matches(oa)) {
      iter.Remove();
    }
  }
}

void nsHttpAuthCache::CollectKeys(nsTArray<nsCString>& aValue) {
  AppendToArray(aValue, mDB.Keys());
}

//-----------------------------------------------------------------------------
// nsHttpAuthIdentity
//-----------------------------------------------------------------------------

void nsHttpAuthIdentity::Clear() {
  mUser.Truncate();
  mPass.Truncate();
  mDomain.Truncate();
}

bool nsHttpAuthIdentity::Equals(const nsHttpAuthIdentity& ident) const {
  // we could probably optimize this with a single loop, but why bother?
  return mUser == ident.mUser && mPass == ident.mPass &&
         mDomain == ident.mDomain;
}

//-----------------------------------------------------------------------------
// nsHttpAuthEntry
//-----------------------------------------------------------------------------

nsresult nsHttpAuthEntry::AddPath(const nsACString& aPath) {
  for (const auto& p : mPaths) {
    if (StringBeginsWith(aPath, p)) {
      return NS_OK;  // subpath already exists in the list
    }
  }

  mPaths.AppendElement(aPath);
  return NS_OK;
}

nsresult nsHttpAuthEntry::Set(const nsACString& path, const nsACString& realm,
                              const nsACString& creds, const nsACString& chall,
                              const nsHttpAuthIdentity* ident,
                              nsISupports* metadata) {
  if (ident) {
    mIdent = *ident;
  } else if (mIdent.IsEmpty()) {
    // If we are not given an identity and our cached identity has not been
    // initialized yet (so is currently empty), initialize it now by
    // filling it with nulls.  We need to do that because consumers expect
    // that mIdent is initialized after this function returns.
    mIdent.Clear();
  }

  nsresult rv = AddPath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mRealm = realm;
  mCreds = creds;
  mChallenge = chall;
  mMetaData = metadata;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpAuthNode
//-----------------------------------------------------------------------------

nsHttpAuthNode::nsHttpAuthNode() {
  LOG(("Creating nsHttpAuthNode @%p\n", this));
}

nsHttpAuthNode::~nsHttpAuthNode() {
  LOG(("Destroying nsHttpAuthNode @%p\n", this));

  mList.Clear();
}

nsHttpAuthEntry* nsHttpAuthNode::LookupEntryByPath(const nsACString& aPath) {
  // look for an entry that either matches or contains this directory.
  // ie. we'll give out credentials if the given directory is a sub-
  // directory of an existing entry.
  for (uint32_t i = 0; i < mList.Length(); ++i) {
    const auto& entry = mList[i];

    for (const auto& entryPath : entry->mPaths) {
      // proxy auth entries have no path, so require exact match on
      // empty path string.
      if (entryPath.IsEmpty()) {
        if (aPath.IsEmpty()) {
          return entry.get();
        }
      } else if (StringBeginsWith(aPath, entryPath)) {
        return entry.get();
      }
    }
  }
  return nullptr;
}

nsHttpAuthNode::EntryList::const_iterator nsHttpAuthNode::LookupEntryItrByRealm(
    const nsACString& realm) const {
  return std::find_if(mList.cbegin(), mList.cend(), [&realm](const auto& val) {
    return realm.Equals(val->Realm());
  });
}

nsHttpAuthEntry* nsHttpAuthNode::LookupEntryByRealm(const nsACString& realm) {
  auto itr = LookupEntryItrByRealm(realm);
  if (itr != mList.cend()) {
    return itr->get();
  }

  return nullptr;
}

nsresult nsHttpAuthNode::SetAuthEntry(const nsACString& path,
                                      const nsACString& realm,
                                      const nsACString& creds,
                                      const nsACString& challenge,
                                      const nsHttpAuthIdentity* ident,
                                      nsISupports* metadata) {
  // look for an entry with a matching realm
  nsHttpAuthEntry* entry = LookupEntryByRealm(realm);
  if (!entry) {
    // We want the latest identity be at the begining of the list so that
    // the newest working credentials are sent first on new requests.
    // Changing a realm is sometimes used to "timeout" authrozization.
    mList.InsertElementAt(
        0, WrapUnique(new nsHttpAuthEntry(path, realm, creds, challenge, ident,
                                          metadata)));
  } else {
    // update the entry...
    nsresult rv = entry->Set(path, realm, creds, challenge, ident, metadata);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void nsHttpAuthNode::ClearAuthEntry(const nsACString& realm) {
  auto idx = LookupEntryItrByRealm(realm);
  if (idx != mList.cend()) {
    mList.RemoveElementAt(idx);
  }
}

}  // namespace net
}  // namespace mozilla
