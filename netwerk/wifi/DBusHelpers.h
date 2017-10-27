/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DBusHelpers_h
#define mozilla_DBusHelpers_h

#include <dbus/dbus.h>
#include "mozilla/UniquePtr.h"

namespace mozilla {

template<>
struct RefPtrTraits<DBusMessage>
{
  static void AddRef(DBusMessage* aMessage) {
    MOZ_ASSERT(aMessage);
    dbus_message_ref(aMessage);
  }
  static void Release(DBusMessage* aMessage) {
    MOZ_ASSERT(aMessage);
    dbus_message_unref(aMessage);
  }
};

template<>
struct RefPtrTraits<DBusPendingCall>
{
  static void AddRef(DBusPendingCall* aPendingCall) {
    MOZ_ASSERT(aPendingCall);
    dbus_pending_call_ref(aPendingCall);
  }
  static void Release(DBusPendingCall* aPendingCall) {
    MOZ_ASSERT(aPendingCall);
    dbus_pending_call_unref(aPendingCall);
  }
};

/*
 * |RefPtrTraits<DBusConnection>| specializes |RefPtrTraits<>|
 * for managing |DBusConnection| with |RefPtr|.
 *
 * |RefPtrTraits<DBusConnection>| will _not_ close the DBus
 * connection upon the final unref. The caller is responsible
 * for closing the connection.
 */
template<>
struct RefPtrTraits<DBusConnection>
{
  static void AddRef(DBusConnection* aConnection) {
    MOZ_ASSERT(aConnection);
    dbus_connection_ref(aConnection);
  }
  static void Release(DBusConnection* aConnection) {
    MOZ_ASSERT(aConnection);
    dbus_connection_unref(aConnection);
  }
};

/*
 * |DBusConnectionDelete| is a deleter for managing instances
 * of |DBusConnection| in |UniquePtr|. Upon destruction, it
 * will close an open connection before unref'ing the data
 * structure.
 *
 * Do not use |UniquePtr| with shared DBus connections. For
 * shared connections, use |RefPtr|.
 */
class DBusConnectionDelete
{
public:
  constexpr DBusConnectionDelete()
  { }

  void operator()(DBusConnection* aConnection) const
  {
    MOZ_ASSERT(aConnection);
    if (dbus_connection_get_is_connected(aConnection)) {
      dbus_connection_close(aConnection);
    }
    dbus_connection_unref(aConnection);
  }
};

} // namespace mozilla

#endif // mozilla_DBusHelpers_h
