/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStore_h
#define mozilla_dom_SessionStore_h

#include "nsWrapperCache.h"

#include "mozilla/Maybe.h"

struct nsPoint;

namespace mozilla::dom {

class CanonicalBrowsingContext;
class GlobalObject;
class SessionStoreFormData;
class SessionStoreScrollData;
class WindowGlobalParent;

namespace sessionstore {
class FormData;
}

class BrowserSessionStore final {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(BrowserSessionStore)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(BrowserSessionStore)

  static already_AddRefed<BrowserSessionStore> GetOrCreate(
      CanonicalBrowsingContext* aBrowsingContext);

  SessionStoreFormData* GetFormdata();
  SessionStoreScrollData* GetScroll();

  void UpdateSessionStore(CanonicalBrowsingContext* aBrowsingContext,
                          const Maybe<sessionstore::FormData>& aFormData,
                          const Maybe<nsPoint>& aScrollPosition,
                          uint32_t aEpoch);

  void RemoveSessionStore(CanonicalBrowsingContext* aBrowsingContext);

 private:
  explicit BrowserSessionStore(CanonicalBrowsingContext* aBrowsingContext);
  virtual ~BrowserSessionStore();

  RefPtr<CanonicalBrowsingContext> mBrowsingContext;
  RefPtr<SessionStoreFormData> mFormData;
  RefPtr<SessionStoreScrollData> mScrollData;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_SessionStore_h
