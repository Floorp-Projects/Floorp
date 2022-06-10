/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncDBus.h"

#include <dbus/dbus-glib-lowlevel.h>
#include "mozilla/UniquePtrExtensions.h"

namespace mozilla::widget {

static void CreateProxyCallback(GObject*, GAsyncResult* aResult,
                                gpointer aUserData) {
  RefPtr<DBusProxyPromise::Private> promise =
      dont_AddRef(static_cast<DBusProxyPromise::Private*>(aUserData));
  GUniquePtr<GError> error;
  RefPtr<GDBusProxy> proxy = dont_AddRef(
      g_dbus_proxy_new_for_bus_finish(aResult, getter_Transfers(error)));
  if (proxy) {
    promise->Resolve(std::move(proxy), __func__);
  } else {
    promise->Reject(std::move(error), __func__);
  }
}

RefPtr<DBusProxyPromise> CreateDBusProxyForBus(
    GBusType aBusType, GDBusProxyFlags aFlags,
    GDBusInterfaceInfo* aInterfaceInfo, const char* aName,
    const char* aObjectPath, const char* aInterfaceName,
    GCancellable* aCancellable) {
  auto promise = MakeRefPtr<DBusProxyPromise::Private>(__func__);
  g_dbus_proxy_new_for_bus(aBusType, aFlags, aInterfaceInfo, aName, aObjectPath,
                           aInterfaceName, aCancellable, CreateProxyCallback,
                           do_AddRef(promise).take());
  return promise.forget();
}

static void ProxyCallCallback(GObject* aSourceObject, GAsyncResult* aResult,
                              gpointer aUserData) {
  RefPtr<DBusCallPromise::Private> promise =
      dont_AddRef(static_cast<DBusCallPromise::Private*>(aUserData));
  GUniquePtr<GError> error;
  RefPtr<GVariant> result = dont_AddRef(g_dbus_proxy_call_finish(
      G_DBUS_PROXY(aSourceObject), aResult, getter_Transfers(error)));
  if (result) {
    promise->Resolve(std::move(result), __func__);
  } else {
    promise->Reject(std::move(error), __func__);
  }
}

RefPtr<DBusCallPromise> DBusProxyCall(GDBusProxy* aProxy, const char* aMethod,
                                      GVariant* aArgs, GDBusCallFlags aFlags,
                                      gint aTimeout,
                                      GCancellable* aCancellable) {
  auto promise = MakeRefPtr<DBusCallPromise::Private>(__func__);
  g_dbus_proxy_call(aProxy, aMethod, aArgs, aFlags, aTimeout, aCancellable,
                    ProxyCallCallback, do_AddRef(promise).take());
  return promise.forget();
}

static void ProxyCallWithUnixFDListCallback(GObject* aSourceObject,
                                            GAsyncResult* aResult,
                                            gpointer aUserData) {
  RefPtr<DBusCallPromise::Private> promise =
      dont_AddRef(static_cast<DBusCallPromise::Private*>(aUserData));
  GUniquePtr<GError> error;
  GUnixFDList** aFDList = nullptr;
  RefPtr<GVariant> result =
      dont_AddRef(g_dbus_proxy_call_with_unix_fd_list_finish(
          G_DBUS_PROXY(aSourceObject), aFDList, aResult,
          getter_Transfers(error)));
  if (result) {
    promise->Resolve(std::move(result), __func__);
  } else {
    promise->Reject(std::move(error), __func__);
  }
}

RefPtr<DBusCallPromise> DBusProxyCallWithUnixFDList(
    GDBusProxy* aProxy, const char* aMethod, GVariant* aArgs,
    GDBusCallFlags aFlags, gint aTimeout, GUnixFDList* aFDList,
    GCancellable* aCancellable) {
  auto promise = MakeRefPtr<DBusCallPromise::Private>(__func__);
  g_dbus_proxy_call_with_unix_fd_list(
      aProxy, aMethod, aArgs, aFlags, aTimeout, aFDList, aCancellable,
      ProxyCallWithUnixFDListCallback, do_AddRef(promise).take());
  return promise.forget();
}

}  // namespace mozilla::widget
