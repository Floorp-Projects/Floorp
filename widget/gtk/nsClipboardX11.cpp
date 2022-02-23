/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "AsyncGtkClipboardRequest.h"
#include "nsClipboardX11.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/WidgetUtilsGtk.h"

#include <gtk/gtk.h>

// For manipulation of the X event queue
#include <X11/Xlib.h>
#include <poll.h>
#include <gdk/gdkx.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include "X11UndefineNone.h"

using namespace mozilla;

nsRetrievalContextX11::nsRetrievalContextX11() = default;

static void DispatchSelectionNotifyEvent(GtkWidget* widget, XEvent* xevent) {
  GdkEvent event = {};
  event.selection.type = GDK_SELECTION_NOTIFY;
  event.selection.window = gtk_widget_get_window(widget);
  event.selection.selection =
      gdk_x11_xatom_to_atom(xevent->xselection.selection);
  event.selection.target = gdk_x11_xatom_to_atom(xevent->xselection.target);
  event.selection.property = gdk_x11_xatom_to_atom(xevent->xselection.property);
  event.selection.time = xevent->xselection.time;

  gtk_widget_event(widget, &event);
}

static void DispatchPropertyNotifyEvent(GtkWidget* widget, XEvent* xevent) {
  GdkWindow* window = gtk_widget_get_window(widget);
  if ((gdk_window_get_events(window)) & GDK_PROPERTY_CHANGE_MASK) {
    GdkEvent event = {};
    event.property.type = GDK_PROPERTY_NOTIFY;
    event.property.window = window;
    event.property.atom = gdk_x11_xatom_to_atom(xevent->xproperty.atom);
    event.property.time = xevent->xproperty.time;
    event.property.state = xevent->xproperty.state;

    gtk_widget_event(widget, &event);
  }
}

struct checkEventContext {
  GtkWidget* cbWidget;
  Atom selAtom;
};

static Bool checkEventProc(Display* display, XEvent* event, XPointer arg) {
  checkEventContext* context = (checkEventContext*)arg;

  if (event->xany.type == SelectionNotify ||
      (event->xany.type == PropertyNotify &&
       event->xproperty.atom == context->selAtom)) {
    GdkWindow* cbWindow = gdk_x11_window_lookup_for_display(
        gdk_x11_lookup_xdisplay(display), event->xany.window);
    if (cbWindow) {
      GtkWidget* cbWidget = nullptr;
      gdk_window_get_user_data(cbWindow, (gpointer*)&cbWidget);
      if (cbWidget && GTK_IS_WIDGET(cbWidget)) {
        context->cbWidget = cbWidget;
        return X11True;
      }
    }
  }

  return X11False;
}

ClipboardData nsRetrievalContextX11::WaitForClipboardData(
    ClipboardDataType aDataType, int32_t aWhichClipboard,
    const char* aMimeType) {
  AsyncGtkClipboardRequest request(aDataType, aWhichClipboard, aMimeType);
  if (request.HasCompleted()) {
    // the request completed synchronously
    return request.TakeResult();
  }

  GdkDisplay* gdkDisplay = gdk_display_get_default();
  // gdk_display_get_default() returns null on headless
  if (widget::GdkIsX11Display(gdkDisplay)) {
    Display* xDisplay = GDK_DISPLAY_XDISPLAY(gdkDisplay);
    checkEventContext context;
    context.cbWidget = nullptr;
    context.selAtom =
        gdk_x11_atom_to_xatom(gdk_atom_intern("GDK_SELECTION", FALSE));

    // Send X events which are relevant to the ongoing selection retrieval
    // to the clipboard widget.  Wait until either the operation completes, or
    // we hit our timeout.  All other X events remain queued.

    int poll_result;

    struct pollfd pfd;
    pfd.fd = ConnectionNumber(xDisplay);
    pfd.events = POLLIN;
    TimeStamp start = TimeStamp::Now();

    do {
      XEvent xevent;

      while (XCheckIfEvent(xDisplay, &xevent, checkEventProc,
                           (XPointer)&context)) {
        if (xevent.xany.type == SelectionNotify)
          DispatchSelectionNotifyEvent(context.cbWidget, &xevent);
        else
          DispatchPropertyNotifyEvent(context.cbWidget, &xevent);

        if (request.HasCompleted()) {
          return request.TakeResult();
        }
      }

      TimeStamp now = TimeStamp::Now();
      int timeout = std::max<int>(
          0, kClipboardTimeout / 1000 - (now - start).ToMilliseconds());
      poll_result = poll(&pfd, 1, timeout);
    } while ((poll_result == 1 && (pfd.revents & (POLLHUP | POLLERR)) == 0) ||
             (poll_result == -1 && errno == EINTR));
  }

  LOGCLIP("exceeded clipboard timeout");
  return {};
}

ClipboardTargets nsRetrievalContextX11::GetTargetsImpl(
    int32_t aWhichClipboard) {
  LOGCLIP("nsRetrievalContextX11::GetTargetsImpl(%s)\n",
          aWhichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                              : "clipboard");
  return WaitForClipboardData(ClipboardDataType::Targets, aWhichClipboard)
      .ExtractTargets();
}

ClipboardData nsRetrievalContextX11::GetClipboardData(const char* aMimeType,
                                                      int32_t aWhichClipboard) {
  LOGCLIP("nsRetrievalContextX11::GetClipboardData(%s) MIME %s\n",
          aWhichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                              : "clipboard",
          aMimeType);

  return WaitForClipboardData(ClipboardDataType::Data, aWhichClipboard,
                              aMimeType);
}

GUniquePtr<char> nsRetrievalContextX11::GetClipboardText(
    int32_t aWhichClipboard) {
  LOGCLIP("nsRetrievalContextX11::GetClipboardText(%s)\n",
          aWhichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                              : "clipboard");

  return WaitForClipboardData(ClipboardDataType::Text, aWhichClipboard)
      .ExtractText();
}
