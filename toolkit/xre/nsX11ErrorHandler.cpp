/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org/>
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Karl Tomlinson <karlt+@karlt.net>
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

#include "nsX11ErrorHandler.h"

#ifdef MOZ_IPC
#include "mozilla/plugins/PluginThreadChild.h"
using mozilla::plugins::PluginThreadChild;
#endif

#include "prenv.h"
#include "nsXULAppAPI.h"
#include "nsExceptionHandler.h"
#include "nsDebug.h"

#include <X11/Xlib.h>
#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdkx.h>
#endif

#define BUFSIZE 2048 // What Xlib uses with XGetErrorDatabaseText

extern "C" {
static int
IgnoreError(Display *display, XErrorEvent *event) {
  return 0; // This return value is ignored.
}

static int
X11Error(Display *display, XErrorEvent *event) {
  nsCAutoString notes;
  char buffer[BUFSIZE];

  // Get an indication of how long ago the request that caused the error was
  // made.  Do this before querying extensions etc below.
  unsigned long age = NextRequest(display) - event->serial;

  // Ignore subsequent errors, which may get processed during the extension
  // queries below for example.
  XSetErrorHandler(IgnoreError);

  // Get a string to represent the request that caused the error.
  nsCAutoString message;
  if (event->request_code < 128) {
    // Core protocol request
    message.AppendInt(event->request_code);
  } else {
    // Extension request
    int nExts;
    char** extNames = XListExtensions(display, &nExts);
    if (extNames) {
      for (int i = 0; i < nExts; ++i) {
        int major_opcode, first_event, first_error;
        if (XQueryExtension(display, extNames[i],
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
  }

  if (message.IsEmpty()) {
    buffer[0] = '\0';
  } else {
    XGetErrorDatabaseText(display, "XRequest", message.get(), "",
                          buffer, sizeof(buffer));
  }

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
    CrashReporter::AppendAppNotesToCrashReport(notes);
    break;
#ifdef MOZ_IPC
  case GeckoProcessType_Plugin:
    if (CrashReporter::GetEnabled()) {
      // This is assuming that X operations are performed on the plugin
      // thread.  If plugins are using X on another thread, then we'll need to
      // handle that differently.
      PluginThreadChild::AppendNotesToCrashReport(notes);
    }
    break;
#endif
  default: 
    ; // crash report notes not supported.
  }
#endif

#ifdef DEBUG
  // The resource id is unlikely to be useful in a crash report without
  // context of other ids, but add it to the debug console output.
  notes.Append("; id=0x");
  notes.AppendInt(PRUint32(event->resourceid), 16);
#ifdef MOZ_WIDGET_GTK2
  // Actually, for requests where Xlib gets the reply synchronously,
  // MOZ_X_SYNC=1 will not be necessary, but we don't have a table to tell us
  // which requests get a synchronous reply.
  if (!PR_GetEnv("MOZ_X_SYNC")) {
    notes.Append("\nRe-running with MOZ_X_SYNC=1 in the environment may give a more helpful backtrace.");
  }
#endif
#endif

  NS_RUNTIMEABORT(notes.get());
  return 0; // not reached
}
}

void
InstallX11ErrorHandler()
{
  XSetErrorHandler(X11Error);

#ifdef MOZ_WIDGET_GTK2
  NS_ASSERTION(GDK_DISPLAY(), "No GDK display");
  if (PR_GetEnv("MOZ_X_SYNC")) {
    XSynchronize(GDK_DISPLAY(), True);
  }
#endif
}
