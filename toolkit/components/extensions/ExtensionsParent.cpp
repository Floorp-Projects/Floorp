/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "extIWebNavigation.h"
#include "mozilla/extensions/ExtensionsParent.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/RefPtr.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"  // JS_SetProperty
#include "nsImportModule.h"
#include "xpcpublic.h"

namespace mozilla {
namespace extensions {

ExtensionsParent::ExtensionsParent() {}
ExtensionsParent::~ExtensionsParent() {}

extIWebNavigation* ExtensionsParent::WebNavigation() {
  if (!mWebNavigation) {
    mWebNavigation = do_ImportModule("resource://gre/modules/WebNavigation.jsm",
                                     "WebNavigationManager");
  }
  return mWebNavigation;
}

void ExtensionsParent::ActorDestroy(ActorDestroyReason aWhy) {}

static inline JS::Handle<JS::Value> ToJSBoolean(bool aValue) {
  return aValue ? JS::TrueHandleValue : JS::FalseHandleValue;
}

JS::Value FrameTransitionDataToJSValue(const FrameTransitionData& aData) {
  JS::Rooted<JS::Value> ret(dom::RootingCx(), JS::UndefinedValue());
  {
    dom::AutoJSAPI jsapi;
    MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
    JSContext* cx = jsapi.cx();

    JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));
    if (obj &&
        JS_SetProperty(cx, obj, "forward_back",
                       ToJSBoolean(aData.forwardBack())) &&
        JS_SetProperty(cx, obj, "form_submit",
                       ToJSBoolean(aData.formSubmit())) &&
        JS_SetProperty(cx, obj, "reload", ToJSBoolean(aData.reload())) &&
        JS_SetProperty(cx, obj, "server_redirect",
                       ToJSBoolean(aData.serverRedirect())) &&
        JS_SetProperty(cx, obj, "client_redirect",
                       ToJSBoolean(aData.clientRedirect()))) {
      ret.setObject(*obj);
    }
  }
  return ret;
}

ipc::IPCResult ExtensionsParent::RecvDocumentChange(
    MaybeDiscardedBrowsingContext&& aBC, FrameTransitionData&& aTransitionData,
    nsIURI* aLocation) {
  if (aBC.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  JS::Rooted<JS::Value> transitionData(
      dom::RootingCx(), FrameTransitionDataToJSValue(aTransitionData));

  WebNavigation()->OnDocumentChange(aBC.get(), transitionData, aLocation);
  return IPC_OK();
}

ipc::IPCResult ExtensionsParent::RecvHistoryChange(
    MaybeDiscardedBrowsingContext&& aBC, FrameTransitionData&& aTransitionData,
    nsIURI* aLocation, bool aIsHistoryStateUpdated,
    bool aIsReferenceFragmentUpdated) {
  if (aBC.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  JS::Rooted<JS::Value> transitionData(
      dom::RootingCx(), FrameTransitionDataToJSValue(aTransitionData));

  WebNavigation()->OnHistoryChange(aBC.get(), transitionData, aLocation,
                                   aIsHistoryStateUpdated,
                                   aIsReferenceFragmentUpdated);
  return IPC_OK();
}

ipc::IPCResult ExtensionsParent::RecvStateChange(
    MaybeDiscardedBrowsingContext&& aBC, nsIURI* aRequestURI, nsresult aStatus,
    uint32_t aStateFlags) {
  if (aBC.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  WebNavigation()->OnStateChange(aBC.get(), aRequestURI, aStatus, aStateFlags);
  return IPC_OK();
}

ipc::IPCResult ExtensionsParent::RecvCreatedNavigationTarget(
    MaybeDiscardedBrowsingContext&& aBC,
    MaybeDiscardedBrowsingContext&& aSourceBC, const nsCString& aURL) {
  if (aBC.IsNullOrDiscarded() || aSourceBC.IsNull()) {
    return IPC_OK();
  }

  WebNavigation()->OnCreatedNavigationTarget(
      aBC.get(), aSourceBC.GetMaybeDiscarded(), aURL);
  return IPC_OK();
}

ipc::IPCResult ExtensionsParent::RecvDOMContentLoaded(
    MaybeDiscardedBrowsingContext&& aBC, nsIURI* aDocumentURI) {
  if (aBC.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  WebNavigation()->OnDOMContentLoaded(aBC.get(), aDocumentURI);
  return IPC_OK();
}

}  // namespace extensions
}  // namespace mozilla
