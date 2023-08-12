/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGTKRemoteServer.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "mozilla/ModuleUtils.h"
#include "nsAppShellCID.h"

#include "nsCOMPtr.h"

#include "nsGTKToolkit.h"

#ifdef MOZ_WAYLAND
#  include "mozilla/WidgetUtilsGtk.h"
#endif

nsresult nsGTKRemoteServer::Startup(const char* aAppName,
                                    const char* aProfileName) {
  NS_ASSERTION(aAppName, "Don't pass a null appname!");

  if (mServerWindow) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

#ifdef MOZ_WAYLAND
  if (mozilla::widget::GdkIsWaylandDisplay()) {
    return NS_ERROR_FAILURE;
  }
#endif

  XRemoteBaseStartup(aAppName, aProfileName);

  mServerWindow = gtk_invisible_new();
  gtk_widget_realize(mServerWindow);
  HandleCommandsFor(mServerWindow);

  return NS_OK;
}

void nsGTKRemoteServer::Shutdown() {
  if (!mServerWindow) {
    return;
  }

  gtk_widget_destroy(mServerWindow);
  mServerWindow = nullptr;
}

void nsGTKRemoteServer::HandleCommandsFor(GtkWidget* widget) {
  g_signal_connect(G_OBJECT(widget), "property_notify_event",
                   G_CALLBACK(HandlePropertyChange), this);

  gtk_widget_add_events(widget, GDK_PROPERTY_CHANGE_MASK);

  Window window = gdk_x11_window_get_xid(gtk_widget_get_window(widget));
  nsXRemoteServer::HandleCommandsFor(window);
}

gboolean nsGTKRemoteServer::HandlePropertyChange(GtkWidget* aWidget,
                                                 GdkEventProperty* pevent,
                                                 nsGTKRemoteServer* aThis) {
  if (pevent->state == GDK_PROPERTY_NEW_VALUE) {
    Atom changedAtom = gdk_x11_atom_to_xatom(pevent->atom);

    XID window = gdk_x11_window_get_xid(gtk_widget_get_window(aWidget));
    return aThis->HandleNewProperty(
        window, GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), pevent->time,
        changedAtom);
  }
  return FALSE;
}
