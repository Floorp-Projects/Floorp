/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Minimo Tel Protocol Handler.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>
 *  Nino D'Aversa <ninodaversa@gmail.com>
 *  Alex Pakhotin <alexp@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
