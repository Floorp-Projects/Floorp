/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SessionStoreScrollData.h"
#include <utility>

#include "js/PropertyAndElement.h"
#include "js/TypeDecls.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BrowserSessionStoreBinding.h"
#include "mozilla/dom/BrowsingContext.h"

#include "nsPresContext.h"

#include "mozilla/dom/WebIDLGlobalNameHash.h"
#include "js/PropertyDescriptor.h"

#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/GeneratedAtomList.h"
#include "js/StructuredClone.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "nsContentUtils.h"
#include "js/Array.h"
#include "js/JSON.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTING_ADDREF(SessionStoreScrollData)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SessionStoreScrollData)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK_PTR(SessionStoreScrollData,
                                               mChildren)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SessionStoreScrollData)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsISupports* SessionStoreScrollData::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
  ;
}

JSObject* SessionStoreScrollData::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SessionStoreScrollData_Binding::Wrap(aCx, this, aGivenProto);
}

void SessionStoreScrollData::GetScroll(nsACString& aScroll) const {
  int scrollX = nsPresContext::AppUnitsToIntCSSPixels(mScroll.x);
  int scrollY = nsPresContext::AppUnitsToIntCSSPixels(mScroll.y);

  if ((scrollX != 0) || (scrollY != 0)) {
    aScroll = nsPrintfCString("%d,%d", scrollX, scrollY);
  } else {
    aScroll.SetIsVoid(true);
  }
}

SessionStoreScrollData::ChildrenArray& SessionStoreScrollData::Children() {
  return mChildren;
}

void SessionStoreScrollData::GetChildren(
    Nullable<ChildrenArray>& aChildren) const {
  if (!mChildren.IsEmpty()) {
    aChildren.SetValue() = mChildren.Clone();
  } else {
    aChildren.SetNull();
  }
}

void SessionStoreScrollData::ToJSON(JSContext* aCx,
                                    JS::MutableHandle<JSObject*> aRetval) {
  JS::Rooted<JSObject*> self(aCx);
  {
    JS::Rooted<JS::Value> value(aCx);
    if (!GetOrCreateDOMReflector(aCx, this, &value)) {
      return;
    }

    self.set(value.toObjectOrNull());
  }

  JS::Rooted<JSObject*> result(aCx, JS_NewPlainObject(aCx));

  if (!IsEmpty()) {
    if (HasData(mScroll)) {
      if (!SessionStoreUtils::CopyProperty(aCx, result, self, u"scroll"_ns)) {
        return;
      }
    }

    SessionStoreUtils::CopyChildren(aCx, result, mChildren);
  }

  aRetval.set(result);
}

void SessionStoreScrollData::Update(const CollectedType& aUpdate) {
  SessionStoreScrollData_Binding::ClearCachedScrollValue(this);
  mScroll = aUpdate;
}

void SessionStoreScrollData::ClearCachedChildren() {
  SessionStoreScrollData_Binding::ClearCachedChildrenValue(this);
}

/* static */
bool SessionStoreScrollData::HasData(const CollectedType& aPoint) {
  int scrollX = nsPresContext::AppUnitsToIntCSSPixels(aPoint.x);
  int scrollY = nsPresContext::AppUnitsToIntCSSPixels(aPoint.y);
  return scrollX != 0 || scrollY != 0;
}

bool SessionStoreScrollData::IsEmpty() const {
  return !HasData(mScroll) && mChildren.IsEmpty();
}
