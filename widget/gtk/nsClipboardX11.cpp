/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
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

static GdkFilterReturn selection_request_filter(GdkXEvent* gdk_xevent,
                                                GdkEvent* event,
                                                gpointer data) {
  XEvent* xevent = static_cast<XEvent*>(gdk_xevent);
  if (xevent->xany.type == SelectionRequest) {
    if (xevent->xselectionrequest.requestor == X11None)
      return GDK_FILTER_REMOVE;

    GdkDisplay* display =
        gdk_x11_lookup_xdisplay(xevent->xselectionrequest.display);
    if (!display) return GDK_FILTER_REMOVE;

    GdkWindow* window = gdk_x11_window_foreign_new_for_display(
        display, xevent->xselectionrequest.requestor);
    if (!window) return GDK_FILTER_REMOVE;

    g_object_unref(window);
  }
  return GDK_FILTER_CONTINUE;
}

nsRetrievalContextX11::nsRetrievalContextX11()
    : mState(INITIAL),
      mClipboardRequestNumber(0),
      mClipboardData(nullptr),
      mClipboardDataLength(0),
      mTargetMIMEType(gdk_atom_intern("TARGETS", FALSE)) {
  // A custom event filter to workaround attempting to dereference a null
  // selection requestor in GTK3 versions before 3.11.3. See bug 1178799.
#if defined(MOZ_X11)
  if (gtk_check_version(3, 11, 3))
    gdk_window_add_filter(nullptr, selection_request_filter, nullptr);
#endif
}

nsRetrievalContextX11::~nsRetrievalContextX11() {
  gdk_window_remove_filter(nullptr, selection_request_filter, nullptr);
}

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

bool nsRetrievalContextX11::WaitForX11Content() {
  if (mState == COMPLETED) {  // the request completed synchronously
    return true;
  }

  GdkDisplay* gdkDisplay = gdk_display_get_default();
  // gdk_display_get_default() returns null on headless
  if (gdkDisplay && GDK_IS_X11_DISPLAY(gdkDisplay)) {
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

        if (mState == COMPLETED) {
          return true;
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
  mState = TIMED_OUT;
  return false;
}

// Call this when data has been retrieved.
void nsRetrievalContextX11::Complete(ClipboardDataType aDataType,
                                     const void* aData,
                                     int aDataRequestNumber) {
  LOGCLIP(("nsRetrievalContextX11::Complete\n"));

  if (mClipboardRequestNumber != aDataRequestNumber) {
    NS_WARNING(
        "nsRetrievalContextX11::Complete() got obsoleted clipboard data.");
    return;
  }

  if (mState == INITIAL) {
    mState = COMPLETED;

    MOZ_ASSERT(mClipboardData == nullptr && mClipboardDataLength == 0,
               "We're leaking clipboard data!");

    switch (aDataType) {
      case CLIPBOARD_TEXT: {
        const char* text = static_cast<const char*>(aData);
        if (text) {
          mClipboardDataLength = sizeof(char) * (strlen(text) + 1);
          mClipboardData = moz_xmalloc(mClipboardDataLength);
          memcpy(mClipboardData, text, mClipboardDataLength);
        }
      } break;
      case CLIPBOARD_TARGETS: {
        const GtkSelectionData* selection =
            static_cast<const GtkSelectionData*>(aData);

        gint n_targets = 0;
        GdkAtom* targets = nullptr;

        if (!gtk_selection_data_get_targets(selection, &targets, &n_targets) ||
            !n_targets) {
          return;
        }

        mClipboardData = targets;
        mClipboardDataLength = n_targets;
      } break;
      case CLIPBOARD_DATA: {
        const GtkSelectionData* selection =
            static_cast<const GtkSelectionData*>(aData);

        gint dataLength = gtk_selection_data_get_length(selection);
        if (dataLength > 0) {
          mClipboardDataLength = dataLength;
          mClipboardData = moz_xmalloc(dataLength);
          memcpy(mClipboardData, gtk_selection_data_get_data(selection),
                 dataLength);
        }
      } break;
    }
  } else {
    // Already timed out
    MOZ_ASSERT(mState == TIMED_OUT);
  }
}

static void clipboard_contents_received(GtkClipboard* clipboard,
                                        GtkSelectionData* selection_data,
                                        gpointer data) {
  int whichClipboard = GetGeckoClipboardType(clipboard);
  LOGCLIP(("clipboard_contents_received (%s) callback\n",
           whichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                              : "clipboard"));

  ClipboardRequestHandler* handler =
      static_cast<ClipboardRequestHandler*>(data);
  handler->Complete(selection_data);
  delete handler;
}

static void clipboard_text_received(GtkClipboard* clipboard, const gchar* text,
                                    gpointer data) {
  int whichClipboard = GetGeckoClipboardType(clipboard);
  LOGCLIP(("clipboard_text_received (%s) callback\n",
           whichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                              : "clipboard"));

  ClipboardRequestHandler* handler =
      static_cast<ClipboardRequestHandler*>(data);
  handler->Complete(text);
  delete handler;
}

bool nsRetrievalContextX11::WaitForClipboardData(ClipboardDataType aDataType,
                                                 GtkClipboard* clipboard,
                                                 const char* aMimeType) {
  LOGCLIP(("nsRetrievalContextX11::WaitForClipboardData\n"));

  mState = INITIAL;
  NS_ASSERTION(!mClipboardData, "Leaking clipboard content!");

  // Call ClipboardRequestHandler() with unique clipboard request number.
  // The request number pairs gtk_clipboard_request_contents() data request
  // with clipboard_contents_received() callback where the data
  // is provided by Gtk.
  mClipboardRequestNumber++;
  ClipboardRequestHandler* handler =
      new ClipboardRequestHandler(this, aDataType, mClipboardRequestNumber);

  switch (aDataType) {
    case CLIPBOARD_DATA:
      gtk_clipboard_request_contents(clipboard,
                                     gdk_atom_intern(aMimeType, FALSE),
                                     clipboard_contents_received, handler);
      break;
    case CLIPBOARD_TEXT:
      gtk_clipboard_request_text(clipboard, clipboard_text_received, handler);
      break;
    case CLIPBOARD_TARGETS:
      gtk_clipboard_request_contents(clipboard, mTargetMIMEType,
                                     clipboard_contents_received, handler);
      break;
  }

  return WaitForX11Content();
}

GdkAtom* nsRetrievalContextX11::GetTargets(int32_t aWhichClipboard,
                                           int* aTargetNums) {
  LOGCLIP(("nsRetrievalContextX11::GetTargets(%s)\n",
           aWhichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                               : "clipboard"));

  GtkClipboard* clipboard =
      gtk_clipboard_get(GetSelectionAtom(aWhichClipboard));

  if (!WaitForClipboardData(CLIPBOARD_TARGETS, clipboard)) {
    LOGCLIP(("    WaitForClipboardData() failed!\n"));
    return nullptr;
  }

  *aTargetNums = mClipboardDataLength;
  GdkAtom* targets = static_cast<GdkAtom*>(mClipboardData);

  // We don't hold the target list internally but we transfer the ownership.
  mClipboardData = nullptr;
  mClipboardDataLength = 0;

  LOGCLIP(("    returned %d targets\n", *aTargetNums));
  return targets;
}

const char* nsRetrievalContextX11::GetClipboardData(const char* aMimeType,
                                                    int32_t aWhichClipboard,
                                                    uint32_t* aContentLength) {
  LOGCLIP(("nsRetrievalContextX11::GetClipboardData(%s)\n",
           aWhichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                               : "clipboard"));

  GtkClipboard* clipboard;
  clipboard = gtk_clipboard_get(GetSelectionAtom(aWhichClipboard));

  if (!WaitForClipboardData(CLIPBOARD_DATA, clipboard, aMimeType))
    return nullptr;

  *aContentLength = mClipboardDataLength;
  return static_cast<const char*>(mClipboardData);
}

const char* nsRetrievalContextX11::GetClipboardText(int32_t aWhichClipboard) {
  LOGCLIP(("nsRetrievalContextX11::GetClipboardText(%s)\n",
           aWhichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                               : "clipboard"));

  GtkClipboard* clipboard;
  clipboard = gtk_clipboard_get(GetSelectionAtom(aWhichClipboard));

  if (!WaitForClipboardData(CLIPBOARD_TEXT, clipboard)) return nullptr;

  return static_cast<const char*>(mClipboardData);
}

void nsRetrievalContextX11::ReleaseClipboardData(const char* aClipboardData) {
  LOGCLIP(("nsRetrievalContextX11::ReleaseClipboardData\n"));
  NS_ASSERTION(aClipboardData == mClipboardData,
               "Releasing unknown clipboard data!");
  free((void*)aClipboardData);

  mClipboardData = nullptr;
  mClipboardDataLength = 0;
}
