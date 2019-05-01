/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGDKErrorHandler.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "nsDebug.h"
#include "nsString.h"
#include "nsX11ErrorHandler.h"

#include "prenv.h"

/* See https://bugzilla.gnome.org/show_bug.cgi?id=629608#c8
 *
 * GDK implements X11 error traps to ignore X11 errors.
 * Unfortunatelly We don't know which X11 events can be ignored
 * so we have to utilize the Gdk error handler to avoid
 * false alarms in Gtk3.
 */
static void GdkErrorHandler(const gchar* log_domain, GLogLevelFlags log_level,
                            const gchar* message, gpointer user_data) {
  if (strstr(message, "X Window System error")) {
    XErrorEvent event;
    nsDependentCString buffer(message);
    char* endptr;

    /* Parse Gdk X Window error message which has this format:
     * (Details: serial XXXX error_code XXXX request_code XXXX (XXXX) minor_code
     * XXXX)
     */
    NS_NAMED_LITERAL_CSTRING(serialString, "(Details: serial ");
    int32_t start = buffer.Find(serialString);
    if (start == kNotFound) {
      MOZ_CRASH_UNSAFE(message);
    }

    start += serialString.Length();
    errno = 0;
    event.serial = strtol(buffer.BeginReading() + start, &endptr, 10);
    if (errno) {
      MOZ_CRASH_UNSAFE(message);
    }

    NS_NAMED_LITERAL_CSTRING(errorCodeString, " error_code ");
    if (!StringBeginsWith(Substring(endptr, buffer.EndReading()),
                          errorCodeString)) {
      MOZ_CRASH_UNSAFE(message);
    }

    errno = 0;
    event.error_code = strtol(endptr + errorCodeString.Length(), &endptr, 10);
    if (errno) {
      MOZ_CRASH_UNSAFE(message);
    }

    NS_NAMED_LITERAL_CSTRING(requestCodeString, " request_code ");
    if (!StringBeginsWith(Substring(endptr, buffer.EndReading()),
                          requestCodeString)) {
      MOZ_CRASH_UNSAFE(message);
    }

    errno = 0;
    event.request_code =
        strtol(endptr + requestCodeString.Length(), &endptr, 10);
    if (errno) {
      MOZ_CRASH_UNSAFE(message);
    }

    NS_NAMED_LITERAL_CSTRING(minorCodeString, " minor_code ");
    start = buffer.Find(minorCodeString, /* aIgnoreCase = */ false,
                        endptr - buffer.BeginReading());
    if (!start) {
      MOZ_CRASH_UNSAFE(message);
    }

    errno = 0;
    event.minor_code = strtol(
        buffer.BeginReading() + start + minorCodeString.Length(), nullptr, 10);
    if (errno) {
      MOZ_CRASH_UNSAFE(message);
    }

    event.display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    // Gdk does not provide resource ID
    event.resourceid = 0;

    X11Error(event.display, &event);
  } else {
    g_log_default_handler(log_domain, log_level, message, user_data);
    MOZ_CRASH_UNSAFE(message);
  }
}

void InstallGdkErrorHandler() {
  g_log_set_handler("Gdk",
                    (GLogLevelFlags)(G_LOG_LEVEL_ERROR | G_LOG_FLAG_FATAL |
                                     G_LOG_FLAG_RECURSION),
                    GdkErrorHandler, nullptr);
  if (PR_GetEnv("MOZ_X_SYNC")) {
    XSynchronize(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), X11True);
  }
}
