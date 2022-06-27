/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsX11ErrorHandler.h"

#include "prenv.h"
#include "nsXULAppAPI.h"
#include "nsExceptionHandler.h"
#include "nsDebug.h"
#include "nsString.h"
#include "nsTArray.h"

#include "mozilla/X11Util.h"
#include <X11/Xlib.h>

#define BUFSIZE 2048  // What Xlib uses with XGetErrorDatabaseText

struct XExtension {
  nsCString name;
  int major_opcode;

  XExtension(const char* aName, int aCode) : name(aName), major_opcode(aCode) {}
};

static nsTArray<XExtension> sXExtensions;

// man XSetErrorHandler says "the error handler should not call any
// functions (directly or indirectly) on the display that will generate
// protocol requests or that will look for input events" so we query the
// extension list early to avoid problems.
static void QueryXExtensions(Display* aDisplay) {
  if (!aDisplay) {
    return;
  }
  int nExts = 0;
  char** extNames = XListExtensions(aDisplay, &nExts);
  if (!extNames) {
    return;
  }
  for (int i = 0; i < nExts; ++i) {
    int major_opcode, first_event, first_error;
    if (XQueryExtension(aDisplay, extNames[i], &major_opcode, &first_event,
                        &first_error)) {
      sXExtensions.EmplaceBack(extNames[i], major_opcode);
    }
  }
  XFreeExtensionList(extNames);
}

extern "C" {
int X11Error(Display* display, XErrorEvent* event) {
  // Get an indication of how long ago the request that caused the error was
  // made.
  unsigned long age = NextRequest(display) - event->serial;

  // Get a string to represent the request that caused the error.
  nsAutoCString message;
  if (event->request_code < 128) {
    // Core protocol request
    message.AppendInt(event->request_code);
  } else {
    // Extension request
    for (XExtension& ext : sXExtensions) {
      if (ext.major_opcode == event->request_code) {
        message.Append(ext.name);
        message.Append('.');
        message.AppendInt(event->minor_code);
        break;
      }
    }
  }

  char buffer[BUFSIZE];
  if (message.IsEmpty()) {
    buffer[0] = '\0';
  } else {
    XGetErrorDatabaseText(display, "XRequest", message.get(), "", buffer,
                          sizeof(buffer));
  }

  nsAutoCString notes;
  if (buffer[0]) {
    notes.Append(buffer);
  } else {
    notes.AppendLiteral("Request ");
    notes.AppendInt(event->request_code);
    notes.Append('.');
    notes.AppendInt(event->minor_code);
  }

  notes.AppendLiteral(": ");

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
    if (XSynchronize(display, X11True) == XSynchronize(display, X11False)) {
      notes.AppendLiteral("; sync");
    } else {
      notes.AppendLiteral("; ");
      notes.AppendInt(uint32_t(age));
      notes.AppendLiteral(" requests ago");
    }
  }

#ifdef DEBUG
  // The resource id is unlikely to be useful in a crash report without
  // context of other ids, but add it to the debug console output.
  notes.AppendLiteral("; id=0x");
  notes.AppendInt(uint32_t(event->resourceid), 16);
#  ifdef MOZ_X11
  // Actually, for requests where Xlib gets the reply synchronously,
  // MOZ_X_SYNC=1 will not be necessary, but we don't have a table to tell us
  // which requests get a synchronous reply.
  if (!PR_GetEnv("MOZ_X_SYNC")) {
    notes.AppendLiteral(
        "\nRe-running with MOZ_X_SYNC=1 in the environment may give a more "
        "helpful backtrace.");
  }
#  endif
#endif

  NS_WARNING(notes.get());
  return 0;
}
}

void InstallX11ErrorHandler() {
  XSetErrorHandler(X11Error);

  if (Display* display = mozilla::DefaultXDisplay()) {
    QueryXExtensions(display);
    if (PR_GetEnv("MOZ_X_SYNC")) {
      XSynchronize(display, X11True);
    }
  }
}
