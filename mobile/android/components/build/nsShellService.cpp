/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if (MOZ_PLATFORM_MAEMO == 5)
#include <dbus/dbus.h>
#endif

#ifdef MOZ_WIDGET_QT
#include <QtGui/QApplication>
#include <QtGui/QWidget>
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

#include "nsShellService.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS1(nsShellService, nsIShellService)

NS_IMETHODIMP
nsShellService::SwitchTask()
{
#if (MOZ_PLATFORM_MAEMO == 5)
  DBusError error;
  dbus_error_init(&error);

  DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &error);

  DBusMessage *msg = dbus_message_new_signal("/com/nokia/hildon_desktop",
                                             "com.nokia.hildon_desktop",
                                             "exit_app_view");

  if (msg) {
      dbus_connection_send(conn, msg, NULL);
      dbus_message_unref(msg);
      dbus_connection_flush(conn);
  }
  return NS_OK;
#elif MOZ_WIDGET_QT
  QWidget * window = QApplication::activeWindow();
  if (window)
      window->showMinimized();
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsShellService::CreateShortcut(const nsAString& aTitle, const nsAString& aURI, const nsAString& aIconData, const nsAString& aIntent)
{
  if (!aTitle.Length() || !aURI.Length() || !aIconData.Length())
    return NS_ERROR_FAILURE;

#if MOZ_WIDGET_ANDROID
  mozilla::AndroidBridge::Bridge()->CreateShortcut(aTitle, aURI, aIconData, aIntent);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}
