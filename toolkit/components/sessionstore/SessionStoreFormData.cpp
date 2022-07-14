/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SessionStoreFormData.h"

#include "mozilla/Assertions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BrowserSessionStoreBinding.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/sessionstore/SessionStoreTypes.h"

#include "nsContentUtils.h"
#include "js/JSON.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTING_ADDREF(SessionStoreFormData)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SessionStoreFormData)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK_PTR(SessionStoreFormData, mChildren)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SessionStoreFormData)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsISupports* SessionStoreFormData::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject* SessionStoreFormData::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return SessionStoreFormData_Binding::Wrap(aCx, this, aGivenProto);
}

void SessionStoreFormData::GetUrl(nsACString& aUrl) const {
  if (mUrl.IsEmpty()) {
    aUrl.SetIsVoid(true);
  } else {
    aUrl = mUrl;
  }
}

void SessionStoreFormData::GetId(
    JSContext* aCx,
    Nullable<Record<nsString, OwningStringOrBooleanOrObject>>& aId) {
  if (mId.IsEmpty() ||
      NS_FAILED(SessionStoreUtils::ConstructFormDataValues(
          aCx, mId, aId.SetValue().Entries(), mParseSessionData))) {
    aId.SetNull();
  }

  // SessionStoreFormData.id is now stored in a slot, so we can free our
  // internal state.
  mId.Clear();
}

void SessionStoreFormData::GetXpath(
    JSContext* aCx,
    Nullable<Record<nsString, OwningStringOrBooleanOrObject>>& aXpath) {
  if (mXpath.IsEmpty() ||
      NS_FAILED(SessionStoreUtils::ConstructFormDataValues(
          aCx, mXpath, aXpath.SetValue().Entries(), mParseSessionData))) {
    aXpath.SetNull();
  }

  // SessionStoreFormData.xpath is now stored in a slot, so we can free our
  // internal state.
  mXpath.Clear();
}

void SessionStoreFormData::GetInnerHTML(nsAString& aInnerHTML) {
  if (mInnerHTML.IsEmpty()) {
    SetDOMStringToNull(aInnerHTML);
  } else {
    aInnerHTML = mInnerHTML;
  }

  // SessionStoreFormData.innerHTML is now stored in a slot, so we can free our
  // internal state.
  mInnerHTML.SetIsVoid(true);
}

SessionStoreFormData::ChildrenArray& SessionStoreFormData::Children() {
  return mChildren;
}

void SessionStoreFormData::GetChildren(
    Nullable<ChildrenArray>& aChildren) const {
  if (!mChildren.IsEmpty()) {
    aChildren.SetValue() = mChildren.Clone();
  } else {
    aChildren.SetNull();
  }
}

void SessionStoreFormData::ToJSON(JSContext* aCx,
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
    for (const auto& name :
         {u"url"_ns, u"id"_ns, u"xpath"_ns, u"innerHTML"_ns}) {
      if (!SessionStoreUtils::CopyProperty(aCx, result, self, name)) {
        return;
      }
    }

    SessionStoreUtils::CopyChildren(aCx, result, mChildren);
  }

  aRetval.set(result);
}

void SessionStoreFormData::Update(const CollectedType& aFormData) {
  SessionStoreFormData_Binding::ClearCachedUrlValue(this);
  SessionStoreFormData_Binding::ClearCachedIdValue(this);
  SessionStoreFormData_Binding::ClearCachedXpathValue(this);
  SessionStoreFormData_Binding::ClearCachedInnerHTMLValue(this);

  if (!aFormData.hasData()) {
    mParseSessionData = false;
    mHasData = false;
    mUrl = ""_ns;
    mId.Clear();
    mXpath.Clear();
    mInnerHTML = u""_ns;

    return;
  }

  mHasData = true;

  mUrl = aFormData.uri();
  // We want to avoid saving data for about:sessionrestore as a string.
  // Since it's stored in the form as stringified JSON, stringifying
  // further causes an explosion of escape characters. cf. bug 467409
  mParseSessionData =
      mUrl == "about:sessionrestore"_ns || mUrl == "about:welcomeback"_ns;

  mInnerHTML = aFormData.innerHTML();

  mId.Assign(aFormData.id());
  mXpath.Assign(aFormData.xpath());
}

void SessionStoreFormData::ClearCachedChildren() {
  SessionStoreFormData_Binding::ClearCachedChildrenValue(this);
}

/* static */ bool SessionStoreFormData::HasData(
    const CollectedType& aFormData) {
  return aFormData.hasData();
}

bool SessionStoreFormData::IsEmpty() const {
  return !mHasData && mChildren.IsEmpty();
}
