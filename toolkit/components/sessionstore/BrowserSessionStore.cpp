/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowserSessionStore.h"
#include <algorithm>
#include <cstdint>
#include <functional>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPtr.h"

#include "mozilla/dom/BrowserSessionStoreBinding.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/SessionStoreFormData.h"
#include "mozilla/dom/SessionStoreScrollData.h"
#include "mozilla/dom/WindowGlobalParent.h"

#include "nsTHashMap.h"
#include "nsHashtablesFwd.h"

#include "js/RootingAPI.h"

using namespace mozilla;
using namespace mozilla::dom;

static StaticAutoPtr<nsTHashMap<nsUint64HashKey, BrowserSessionStore*>>
    sSessionStore;

NS_IMPL_CYCLE_COLLECTION(BrowserSessionStore, mBrowsingContext, mFormData,
                         mScrollData)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(BrowserSessionStore, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(BrowserSessionStore, Release)

/* static */
already_AddRefed<BrowserSessionStore> BrowserSessionStore::GetOrCreate(
    CanonicalBrowsingContext* aBrowsingContext) {
  if (!aBrowsingContext->IsTop()) {
    return nullptr;
  }

  if (!sSessionStore) {
    sSessionStore = new nsTHashMap<nsUint64HashKey, BrowserSessionStore*>();
    ClearOnShutdown(&sSessionStore);
  }

  return do_AddRef(sSessionStore->LookupOrInsertWith(
      aBrowsingContext->Id(),
      [&] { return new BrowserSessionStore(aBrowsingContext); }));
}

BrowserSessionStore::BrowserSessionStore(
    CanonicalBrowsingContext* aBrowsingContext)
    : mBrowsingContext(aBrowsingContext) {}

SessionStoreFormData* BrowserSessionStore::GetFormdata() { return mFormData; }

SessionStoreScrollData* BrowserSessionStore::GetScroll() { return mScrollData; }

static bool ShouldUpdateSessionStore(CanonicalBrowsingContext* aBrowsingContext,
                                     uint32_t aEpoch) {
  if (!aBrowsingContext) {
    return false;
  }

  if (aBrowsingContext->Top()->GetSessionStoreEpoch() != aEpoch) {
    return false;
  }

  if (aBrowsingContext->IsReplaced()) {
    return false;
  }

  if (aBrowsingContext->IsDynamic()) {
    return false;
  }

  return true;
}

// With GetOrCreate we can create either of the weak fields:
//   WeakPtr<SessionStoreFormData> mFormdata;
//   WeakPtr<SessionStoreScrollData> mScroll;
// in CanonicalBrowsingContext. If one already exists, then we return that.
template <typename T, WeakPtr<T>& (CanonicalBrowsingContext::*GetWeakRef)()>
static already_AddRefed<T> GetOrCreateEntry(
    CanonicalBrowsingContext* aBrowsingContext) {
  typename T::LocationType& location = (aBrowsingContext->*GetWeakRef)();
  RefPtr<T> entry = location.get();
  if (!entry) {
    entry = MakeRefPtr<T>();
    location = entry;
  }

  return entry.forget();
}

// With InsertEntry we can insert an entry in the session store data tree in
// either of the weak fields:
//   WeakPtr<SessionStoreFormData> mFormdata;
//   WeakPtr<SessionStoreScrollData> mScroll;
// in CanonicalBrowsingContext. If an entry is inserted where there is no parent
// entry, a spine of entries will be created until one is found, or we reach the
// top browsing context.
template <typename T>
void InsertEntry(BrowsingContext* aBrowsingContext, T* aParent, T* aUpdate) {
  int32_t offset = aBrowsingContext->ChildOffset();
  if (offset < 0) {
    return;
  }

  aParent->ClearCachedChildren();

  auto& children = aParent->Children();

  children.EnsureLengthAtLeast(offset + 1);
  if (children[offset] && !aBrowsingContext->Children().IsEmpty()) {
    children[offset]->ClearCachedChildren();
    aUpdate->ClearCachedChildren();
  }

  children[offset] = aUpdate;
}

// With RemoveEntry we can remove an entry in the session store data tree in
// either of the weak fields:
//   WeakPtr<SessionStoreFormData> mFormdata;
//   WeakPtr<SessionStoreScrollData> mScroll;
// in CanonicalBrowsingContext. If an entry is removed, where its parent doesn't
// contain data, we'll remove the parent and repeat until we either find an
// entry with data or reach the top browsing context.
template <typename T>
void RemoveEntry(BrowsingContext* aBrowsingContext, T* aParent) {
  int32_t offset = aBrowsingContext->ChildOffset();
  if (offset < 0) {
    return;
  }

  if (!aParent) {
    return;
  }

  aParent->ClearCachedChildren();

  auto& children = aParent->Children();
  size_t length = children.Length();
  if (children.Length() <= static_cast<size_t>(offset)) {
    // The children array doesn't extend to offset.
    return;
  }

  if (static_cast<size_t>(offset) < length - 1) {
    // offset is before the last item in the children array.
    children[offset] = nullptr;
    return;
  }

  // offset is the last item, find the first non-null item before it
  // and remove anything after that item.
  while (offset > 0 && !children[offset - 1]) {
    --offset;
  }

  children.TruncateLength(offset);
}

// With UpdateSessionStoreField we can update an entry in the session store
// data tree in either of the weak fields:
//   WeakPtr<SessionStoreFormData> mFormdata;
//   WeakPtr<SessionStoreScrollData> mScroll;
// in CanonicalBrowsingContext. UpdateSessionStoreField uses the above
// functions, `GetOrCreateEntry`, `InsertEntry` and `RemoveEntry` to operate on
// the weak fields. We return the top-level entry attached to the top browsing
// context through `aEntry`. If the entire browsing context tree contains no
// session store data this will be set to nullptr.
template <typename T, WeakPtr<T>& (CanonicalBrowsingContext::*GetWeakRef)()>
void UpdateSessionStoreField(CanonicalBrowsingContext* aBrowsingContext,
                             const typename T::CollectedType& aUpdate,
                             T** aEntry) {
  RefPtr<T> currentEntry;

  if (T::HasData(aUpdate)) {
    currentEntry = GetOrCreateEntry<T, GetWeakRef>(aBrowsingContext);
    currentEntry->Update(aUpdate);

    CanonicalBrowsingContext* currentBrowsingContext = aBrowsingContext;
    while (CanonicalBrowsingContext* parent =
               currentBrowsingContext->GetParent()) {
      WeakPtr<T>& parentEntry = (parent->*GetWeakRef)();
      if (parentEntry) {
        InsertEntry(aBrowsingContext, parentEntry.get(), currentEntry.get());
        break;
      }

      RefPtr<T> entry = GetOrCreateEntry<T, GetWeakRef>(parent);
      InsertEntry(currentBrowsingContext, entry.get(), currentEntry.get());

      currentEntry = entry;
      currentBrowsingContext = parent;
    }

    currentEntry = (aBrowsingContext->Top()->*GetWeakRef)().get();
  } else if ((currentEntry = (aBrowsingContext->*GetWeakRef)())) {
    currentEntry->Update(aUpdate);

    CanonicalBrowsingContext* currentBrowsingContext = aBrowsingContext;
    while (CanonicalBrowsingContext* parent =
               currentBrowsingContext->GetParent()) {
      if (!currentEntry || !currentEntry->IsEmpty()) {
        break;
      }

      T* parentEntry = (parent->*GetWeakRef)().get();
      RemoveEntry(currentBrowsingContext, parentEntry);

      currentEntry = parentEntry;
      currentBrowsingContext = parent;
    }

    if (currentEntry && currentEntry->IsEmpty()) {
      currentEntry = nullptr;
    } else {
      currentEntry = (aBrowsingContext->Top()->*GetWeakRef)().get();
    }
  } else {
    currentEntry = (aBrowsingContext->Top()->*GetWeakRef)().get();
  }

  *aEntry = currentEntry.forget().take();
}

void BrowserSessionStore::UpdateSessionStore(
    CanonicalBrowsingContext* aBrowsingContext,
    const Maybe<sessionstore::FormData>& aFormData,
    const Maybe<nsPoint>& aScrollPosition, uint32_t aEpoch) {
  if (!aFormData && !aScrollPosition) {
    return;
  }

  if (!ShouldUpdateSessionStore(aBrowsingContext, aEpoch)) {
    return;
  }

  if (aFormData) {
    UpdateSessionStoreField<
        SessionStoreFormData,
        &CanonicalBrowsingContext::GetSessionStoreFormDataRef>(
        aBrowsingContext, *aFormData, getter_AddRefs(mFormData));
  }

  if (aScrollPosition) {
    UpdateSessionStoreField<
        SessionStoreScrollData,
        &CanonicalBrowsingContext::GetSessionStoreScrollDataRef>(
        aBrowsingContext, *aScrollPosition, getter_AddRefs(mScrollData));
  }
}

void BrowserSessionStore::RemoveSessionStore(
    CanonicalBrowsingContext* aBrowsingContext) {
  if (!aBrowsingContext) {
    return;
  }

  CanonicalBrowsingContext* parentContext = aBrowsingContext->GetParent();

  if (parentContext) {
    RemoveEntry(aBrowsingContext,
                parentContext->GetSessionStoreFormDataRef().get());

    RemoveEntry(aBrowsingContext,
                parentContext->GetSessionStoreScrollDataRef().get());

    return;
  }

  if (aBrowsingContext->IsTop()) {
    mFormData = nullptr;
    mScrollData = nullptr;
  }
}

BrowserSessionStore::~BrowserSessionStore() {
  if (sSessionStore) {
    sSessionStore->Remove(mBrowsingContext->Id());
  }
}
