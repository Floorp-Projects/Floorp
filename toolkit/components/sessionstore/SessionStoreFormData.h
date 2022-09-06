/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreFormData_h
#define mozilla_dom_SessionStoreFormData_h

#include "mozilla/WeakPtr.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/WindowGlobalParent.h"

#include "nsTArrayForwardDeclare.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla::dom {

namespace sessionstore {
class FormData;
}

class BrowsingContext;
class OwningByteStringOrObjectOrNull;
class OwningStringOrObjectOrNull;
class WindowGlobalParent;

class SessionStoreFormData final : public nsISupports,
                                   public nsWrapperCache,
                                   public SupportsWeakPtr {
 public:
  using CollectedType = sessionstore::FormData;
  using LocationType = WeakPtr<SessionStoreFormData>;
  using ChildrenArray = nsTArray<RefPtr<SessionStoreFormData>>;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(SessionStoreFormData)
  nsISupports* GetParentObject() const;
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetUrl(nsACString& aUrl) const;

  void GetId(JSContext* aCx,
             Nullable<Record<nsString, OwningStringOrBooleanOrObject>>& aId);

  void GetXpath(
      JSContext* aCx,
      Nullable<Record<nsString, OwningStringOrBooleanOrObject>>& aXpath);

  void GetInnerHTML(nsAString& aInnerHTML);

  ChildrenArray& Children();

  void GetChildren(Nullable<ChildrenArray>& aChildren) const;

  void ToJSON(JSContext* aCx, JS::MutableHandleObject aRetval);

  void Update(const CollectedType& aFormData);

  void ClearCachedChildren();

  static bool HasData(const CollectedType& aFormData);

  bool IsEmpty() const;

 private:
  ~SessionStoreFormData() = default;

  bool mParseSessionData = false;
  bool mHasData = false;
  nsCString mUrl;
  nsTArray<sessionstore::FormEntry> mId;
  nsTArray<sessionstore::FormEntry> mXpath;
  nsString mInnerHTML;
  ChildrenArray mChildren;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_SessionStoreFormData_h
