/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_AsyncDBus_h
#define mozilla_widget_AsyncDBus_h

#include "mozilla/GRefPtr.h"
#include "mozilla/GUniquePtr.h"
#include "mozilla/MozPromise.h"

namespace mozilla::widget {

using DBusProxyPromise = MozPromise<RefPtr<GDBusProxy>, GUniquePtr<GError>,
                                    /* IsExclusive = */ true>;

using DBusCallPromise = MozPromise<RefPtr<GVariant>, GUniquePtr<GError>,
                                   /* IsExclusive = */ true>;

RefPtr<DBusProxyPromise> CreateDBusProxyForBus(
    GBusType aBusType, GDBusProxyFlags aFlags,
    GDBusInterfaceInfo* aInterfaceInfo, const char* aName,
    const char* aObjectPath, const char* aInterfaceName,
    GCancellable* aCancellable = nullptr);

RefPtr<DBusCallPromise> DBusProxyCall(GDBusProxy*, const char* aMethod,
                                      GVariant* aArgs, GDBusCallFlags,
                                      gint aTimeout = -1,
                                      GCancellable* = nullptr);

RefPtr<DBusCallPromise> DBusProxyCallWithUnixFDList(
    GDBusProxy*, const char* aMethod, GVariant* aArgs, GDBusCallFlags,
    gint aTimeout = -1, GUnixFDList* = nullptr, GCancellable* = nullptr);

}  // namespace mozilla::widget

#endif
