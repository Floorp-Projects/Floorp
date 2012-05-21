/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsX11ErrorHandler.h"

#include "prenv.h"
#include "nsXULAppAPI.h"
#include "nsExceptionHandler.h"
#include "nsDebug.h"

#include "mozilla/X11Util.h"
#include <X11/Xlib.h>

#define BUFSIZE 2048 // What Xlib uses with XGetErrorDatabaseText

extern "C" {
static int
X11Error(Display *display, XErrorEvent *event) {
  // Get an indication of how long ago the request that caused the error was
  // made.
  unsigned long age = NextRequest(display) - event->serial;

  // Get a string to represent the request that caused the error.
  nsCAutoString message;
  if (event->request_code < 128) {
    // Core protocol request
    message.AppendInt(event->request_code);
  } else {
    // Extension request

    // man XSetErrorHandler says "the error handler should not call any
    // functions (directly or indirectly) on the display that will generate
    // protocol requests or that will look for input events" so we use another
    // temporary Display to request extension information.  This assumes on
    // the DISPLAY environment variable has been set and matches what was used
    // to open |display|.
    Display *tmpDisplay = XOpenDisplay(NULL);
    if (tmpDisplay) {
      int nExts;
      char** extNames = XListExtensions(tmpDisplay, &nExts);
      int first_error;
      if (extNames) {
        for (int i = 0; i < nExts; ++i) {
          int major_opcode, first_event;
          if (XQueryExtension(tmpDisplay, extNames[i],
                              &major_opcode, &first_event, &first_error)
              && major_opcode == event->request_code) {
            message.Append(extNames[i]);
            message.Append('.');
            message.AppendInt(event->minor_code);
            break;
          }
        }

        XFreeExtensionList(extNames);
      }
      XCloseDisplay(tmpDisplay);

#ifdef MOZ_WIDGET_GTK2
      // GDK2 calls XCloseDevice the devices that it opened on startup, but
      // the XI protocol no longer ensures that the devices will still exist.
      // If they have been removed, then a BadDevice error results.  Ignore
      // this error.
      if (message.EqualsLiteral("XInputExtension.4") &&
          event->error_code == first_error + 0) {
        return 0;
      }
#endif
    }
  }

  char buffer[BUFSIZE];
  if (message.IsEmpty()) {
    buffer[0] = '\0';
  } else {
    XGetErrorDatabaseText(display, "XRequest", message.get(), "",
                          buffer, sizeof(buffer));
  }

  nsCAutoString notes;
  if (buffer[0]) {
    notes.Append(buffer);
  } else {
    notes.Append("Request ");
    notes.AppendInt(event->request_code);
    notes.Append('.');
    notes.AppendInt(event->minor_code);
  }

  notes.Append(": ");

  // Get a string to describe the error.
  XGetErrorText(display, event->error_code, buffer, sizeof(buffer));
  notes.Append(buffer);

  // For requests where Xlib gets the reply synchronously, |age| will be 1
  // and the stack will include the function making the request.  For
  // asynchronous requests, the current stack will often be unrelated to the
  // point of making the request, even if |age| is 1, but sometimes this may
  // help us count back to the point of the request.  With XSynchronize on,
  // the stack will include the function making the request, even though
  // |age| will be 2 for asynchronous requests because XSynchronize is
  // implemented by an empty request from an XSync, which has not yet been
  // processed.
  if (age > 1) {
    // XSynchronize returns the previous "after function".  If a second
    // XSynchronize call returns the same function after an enable call then
    // synchronization must have already been enabled.
    if (XSynchronize(display, True) == XSynchronize(display, False)) {
      notes.Append("; sync");
    } else {
      notes.Append("; ");
      notes.AppendInt(PRUint32(age));
      notes.Append(" requests ago");
    }
  }

#ifdef MOZ_CRASHREPORTER
  switch (XRE_GetProcessType()) {
  case GeckoProcessType_Default:
  case GeckoProcessType_Plugin:
  case GeckoProcessType_Content:
    CrashReporter::AppendAppNotesToCrashReport(notes);
    break;
  default: 
    ; // crash report notes not supported.
  }
#endif

#ifdef DEBUG
  // The resource id is unlikely to be useful in a crash report without
  // context of other ids, but add it to the debug console output.
  notes.Append("; id=0x");
  notes.AppendInt(PRUint32(event->resourceid), 16);
#ifdef MOZ_X11
  // Actually, for requests where Xlib gets the reply synchronously,
  // MOZ_X_SYNC=1 will not be necessary, but we don't have a table to tell us
  // which requests get a synchronous reply.
  if (!PR_GetEnv("MOZ_X_SYNC")) {
    notes.Append("\nRe-running with MOZ_X_SYNC=1 in the environment may give a more helpful backtrace.");
  }
#endif
#endif

#ifdef MOZ_WIDGET_QT
  // We should not abort here if MOZ_X_SYNC is not set
  // until http://bugreports.qt.nokia.com/browse/QTBUG-4042
  // not fixed, just print error value
  if (!PR_GetEnv("MOZ_X_SYNC")) {
    fprintf(stderr, "XError: %s\n", notes.get());
    return 0; // temporary workaround for bug 161472
  }
#endif

  NS_RUNTIMEABORT(notes.get());
  return 0; // not reached
}
}

void
InstallX11ErrorHandler()
{
  XSetErrorHandler(X11Error);

  Display *display = mozilla::DefaultXDisplay();
  NS_ASSERTION(display, "No X display");
  if (PR_GetEnv("MOZ_X_SYNC")) {
    XSynchronize(display, True);
  }
}
