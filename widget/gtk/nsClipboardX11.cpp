/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsArrayUtils.h"
#include "nsClipboard.h"
#include "nsClipboardX11.h"
#include "nsSupportsPrimitives.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsPrimitiveHelpers.h"
#include "nsImageToPixbuf.h"
#include "nsStringStream.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "WidgetUtilsGtk.h"

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

bool nsRetrievalContextX11::HasSelectionSupport(void) {
  // yeah, unix supports the selection clipboard on X11.
  return true;
}

nsRetrievalContextX11::nsRetrievalContextX11()
    : mTargetMIMEType(gdk_atom_intern("TARGETS", FALSE)) {}

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

ClipboardData nsRetrievalContextX11::WaitForX11Content() {
  if (mState == State::Completed) {  // the request completed synchronously
    return mClipboardData.extract();
  }

  GdkDisplay* gdkDisplay = gdk_display_get_default();
  // gdk_display_get_default() returns null on headless
  if (mozilla::widget::GdkIsX11Display(gdkDisplay)) {
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

        if (mState == State::Completed) {
          return mClipboardData.extract();
        }
      }

      TimeStamp now = TimeStamp::Now();
      int timeout = std::max<int>(
          0, kClipboardTimeout / 1000 - (now - start).ToMilliseconds());
      poll_result = poll(&pfd, 1, timeout);
    } while ((poll_result == 1 && (pfd.revents & (POLLHUP | POLLERR)) == 0) ||
             (poll_result == -1 && errno == EINTR));
  }
#ifdef DEBUG_CLIPBOARD
  printf("exceeded clipboard timeout\n");
#endif
  mState = State::TimedOut;
  return {};
}

// Call this when data has been retrieved.
void nsRetrievalContextX11::Complete(ClipboardDataType aDataType,
                                     const void* aData,
                                     int aDataRequestNumber) {
  LOGCLIP("nsRetrievalContextX11::Complete\n");

  if (mClipboardRequestNumber != aDataRequestNumber) {
    NS_WARNING(
        "nsRetrievalContextX11::Complete() got obsoleted clipboard data.");
    return;
  }

  if (mState != State::Initial) {
    // Already timed out
    MOZ_ASSERT(mState == State::TimedOut);
    return;
  }

  mState = State::Completed;

  MOZ_ASSERT(!mClipboardData, "We're leaking clipboard data!");
  mClipboardData = Some(ClipboardData());

  switch (aDataType) {
    case ClipboardDataType::Text: {
      LOGCLIP("  got text data %p\n", aData);
      auto* data = static_cast<const char*>(aData);
      mClipboardData->SetText(Span(data, strlen(data)));
    } break;
    case ClipboardDataType::Targets: {
      auto* selection = static_cast<const GtkSelectionData*>(aData);

      gint n_targets = 0;
      GdkAtom* targets = nullptr;

      if (!gtk_selection_data_get_targets(selection, &targets, &n_targets) ||
          !n_targets) {
        return;
      }

      LOGCLIP("  got %d targets\n", n_targets);

      mClipboardData->SetTargets(
          ClipboardTargets{GUniquePtr<GdkAtom>(targets), uint32_t(n_targets)});
    } break;
    case ClipboardDataType::Data: {
      auto* selection = static_cast<const GtkSelectionData*>(aData);
      mClipboardData->SetData(Span(gtk_selection_data_get_data(selection),
                                   gtk_selection_data_get_length(selection)));
#ifdef MOZ_LOGGING
      if (LOGCLIP_ENABLED()) {
        GdkAtom target = gtk_selection_data_get_target(selection);
        LOGCLIP("  got data %p len %lu MIME %s\n",
                mClipboardData->AsSpan().data(),
                mClipboardData->AsSpan().Length(),
                GUniquePtr<gchar>(gdk_atom_name(target)).get());
      }
#endif
    } break;
  }
}

static void clipboard_contents_received(GtkClipboard* clipboard,
                                        GtkSelectionData* selection_data,
                                        gpointer data) {
  int whichClipboard = GetGeckoClipboardType(clipboard);
  LOGCLIP("clipboard_contents_received (%s) callback\n",
          whichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                             : "clipboard");

  ClipboardRequestHandler* handler =
      static_cast<ClipboardRequestHandler*>(data);
  handler->Complete(selection_data);
  delete handler;
}

static void clipboard_text_received(GtkClipboard* clipboard, const gchar* text,
                                    gpointer data) {
  int whichClipboard = GetGeckoClipboardType(clipboard);
  LOGCLIP("clipboard_text_received (%s) callback\n",
          whichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                             : "clipboard");

  auto* handler = static_cast<ClipboardRequestHandler*>(data);
  handler->Complete(text);
  delete handler;
}

ClipboardData nsRetrievalContextX11::WaitForClipboardData(
    ClipboardDataType aDataType, int32_t aWhichClipboard,
    const char* aMimeType) {
  LOGCLIP("nsRetrievalContextX11::WaitForClipboardData, MIME %s\n", aMimeType);

  GtkClipboard* clipboard =
      gtk_clipboard_get(GetSelectionAtom(aWhichClipboard));

  mState = State::Initial;

  NS_ASSERTION(!mClipboardData, "Leaking clipboard content!");
  mClipboardData.reset();
  // Call ClipboardRequestHandler() with unique clipboard request number.
  // The request number pairs gtk_clipboard_request_contents() data request
  // with clipboard_contents_received() callback where the data
  // is provided by Gtk.
  mClipboardRequestNumber++;

  ClipboardRequestHandler* handler =
      new ClipboardRequestHandler(this, aDataType, mClipboardRequestNumber);

  switch (aDataType) {
    case ClipboardDataType::Data:
      LOGCLIP("  getting DATA MIME %s\n", aMimeType);
      gtk_clipboard_request_contents(clipboard,
                                     gdk_atom_intern(aMimeType, FALSE),
                                     clipboard_contents_received, handler);
      break;
    case ClipboardDataType::Text:
      LOGCLIP("  getting TEXT\n");
      gtk_clipboard_request_text(clipboard, clipboard_text_received, handler);
      break;
    case ClipboardDataType::Targets:
      LOGCLIP("  getting TARGETS\n");
      gtk_clipboard_request_contents(clipboard, mTargetMIMEType,
                                     clipboard_contents_received, handler);
      break;
  }

  return WaitForX11Content();
}

ClipboardTargets nsRetrievalContextX11::GetTargets(int32_t aWhichClipboard) {
  LOGCLIP("nsRetrievalContextX11::GetTargets(%s)\n",
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

  return WaitForClipboardData(ClipboardDataType::Text, aWhichClipboard).mData;
}
