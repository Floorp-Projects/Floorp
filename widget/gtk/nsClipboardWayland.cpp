/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsArrayUtils.h"
#include "nsClipboard.h"
#include "nsClipboardWayland.h"
#include "nsSupportsPrimitives.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsPrimitiveHelpers.h"
#include "nsImageToPixbuf.h"
#include "nsStringStream.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsDragService.h"
#include "mozwayland/mozwayland.h"
#include "nsWaylandDisplay.h"

#include <gtk/gtk.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

using namespace mozilla;
using namespace mozilla::widget;

const char* nsRetrievalContextWayland::sTextMimeTypes[TEXT_MIME_TYPES_NUM] = {
    "text/plain;charset=utf-8", "UTF8_STRING", "COMPOUND_TEXT"};

void DataOffer::AddMIMEType(const char* aMimeType) {
  GdkAtom atom = gdk_atom_intern(aMimeType, FALSE);
  mTargetMIMETypes.AppendElement(atom);
}

GdkAtom* DataOffer::GetTargets(int* aTargetNum) {
  int length = mTargetMIMETypes.Length();
  if (!length) {
    *aTargetNum = 0;
    return nullptr;
  }

  GdkAtom* targetList =
      reinterpret_cast<GdkAtom*>(g_malloc(sizeof(GdkAtom) * length));
  for (int32_t j = 0; j < length; j++) {
    targetList[j] = mTargetMIMETypes[j];
  }

  *aTargetNum = length;
  return targetList;
}

bool DataOffer::HasTarget(const char* aMimeType) {
  int length = mTargetMIMETypes.Length();
  for (int32_t j = 0; j < length; j++) {
    if (mTargetMIMETypes[j] == gdk_atom_intern(aMimeType, FALSE)) {
      LOGCLIP(("DataOffer::HasTarget() we have mime %s\n", aMimeType));
      return true;
    }
  }
  LOGCLIP(("DataOffer::HasTarget() missing mime %s\n", aMimeType));
  return false;
}

char* DataOffer::GetData(wl_display* aDisplay, const char* aMimeType,
                         uint32_t* aContentLength) {
  LOGCLIP(("DataOffer::GetData() mime %s\n", aMimeType));

  int pipe_fd[2];
  if (pipe(pipe_fd) == -1) {
    return nullptr;
  }

  if (!RequestDataTransfer(aMimeType, pipe_fd[1])) {
    NS_WARNING("DataOffer::RequestDataTransfer() failed!");
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return nullptr;
  }

  close(pipe_fd[1]);
  wl_display_flush(aDisplay);

  GIOChannel* channel = g_io_channel_unix_new(pipe_fd[0]);
  GError* error = nullptr;
  char* clipboardData = nullptr;

  g_io_channel_set_encoding(channel, nullptr, &error);
  if (!error) {
    gsize length = 0;
    g_io_channel_read_to_end(channel, &clipboardData, &length, &error);
    if (length == 0) {
      // We don't have valid clipboard data although
      // g_io_channel_read_to_end() allocated clipboardData for us.
      // Release it now and return nullptr to indicate
      // we don't have reqested data flavour.
      g_free((void*)clipboardData);
      clipboardData = nullptr;
    }
    *aContentLength = length;
  }

  if (error) {
    NS_WARNING(
        nsPrintfCString("Unexpected error when reading clipboard data: %s",
                        error->message)
            .get());
    g_error_free(error);
  }

  g_io_channel_unref(channel);
  close(pipe_fd[0]);

  LOGCLIP(("  Got clipboard data length %d\n", *aContentLength));
  return clipboardData;
}

bool WaylandDataOffer::RequestDataTransfer(const char* aMimeType, int fd) {
  LOGCLIP(
      ("WaylandDataOffer::RequestDataTransfer MIME %s FD %d\n", aMimeType, fd));
  if (mWaylandDataOffer) {
    wl_data_offer_receive(mWaylandDataOffer, aMimeType, fd);
    return true;
  }

  return false;
}

static void data_offer_offer(void* data, struct wl_data_offer* wl_data_offer,
                             const char* type) {
  auto* offer = static_cast<DataOffer*>(data);
  LOGCLIP(("Data offer %p add MIME %s\n", wl_data_offer, type));
  offer->AddMIMEType(type);
}

/* Advertise all available drag and drop actions from source.
 * We don't use that but follow gdk_wayland_drag_context_commit_status()
 * from gdkdnd-wayland.c here.
 */
static void data_offer_source_actions(void* data,
                                      struct wl_data_offer* wl_data_offer,
                                      uint32_t source_actions) {}

/* Advertise recently selected drag and drop action by compositor, based
 * on source actions and user choice (key modifiers, etc.).
 */
static void data_offer_action(void* data, struct wl_data_offer* wl_data_offer,
                              uint32_t dnd_action) {}

/* wl_data_offer callback description:
 *
 * data_offer_offer - Is called for each MIME type available at wl_data_offer.
 * data_offer_source_actions - This event indicates the actions offered by
 *                             the data source.
 * data_offer_action - This event indicates the action selected by
 *                     the compositor after matching the source/destination
 *                     side actions.
 */
static const moz_wl_data_offer_listener data_offer_listener = {
    data_offer_offer, data_offer_source_actions, data_offer_action};

WaylandDataOffer::WaylandDataOffer(wl_data_offer* aWaylandDataOffer)
    : mWaylandDataOffer(aWaylandDataOffer) {
  wl_data_offer_add_listener(
      mWaylandDataOffer, (struct wl_data_offer_listener*)&data_offer_listener,
      this);
}

WaylandDataOffer::~WaylandDataOffer(void) {
  if (mWaylandDataOffer) {
    wl_data_offer_destroy(mWaylandDataOffer);
  }
}

bool PrimaryDataOffer::RequestDataTransfer(const char* aMimeType, int fd) {
  if (mPrimaryDataOfferGtk) {
    gtk_primary_selection_offer_receive(mPrimaryDataOfferGtk, aMimeType, fd);
    return true;
  }
  if (mPrimaryDataOfferZwpV1) {
    zwp_primary_selection_offer_v1_receive(mPrimaryDataOfferZwpV1, aMimeType,
                                           fd);
    return true;
  }
  return false;
}

static void primary_data_offer(
    void* data, gtk_primary_selection_offer* primary_selection_offer,
    const char* mime_type) {
  LOGCLIP(("Primary data offer %p add MIME %s\n", primary_selection_offer,
           mime_type));
  auto* offer = static_cast<DataOffer*>(data);
  offer->AddMIMEType(mime_type);
}

static void primary_data_offer(
    void* data, zwp_primary_selection_offer_v1* primary_selection_offer,
    const char* mime_type) {
  LOGCLIP(("Primary data offer %p add MIME %s\n", primary_selection_offer,
           mime_type));
  auto* offer = static_cast<DataOffer*>(data);
  offer->AddMIMEType(mime_type);
}

/* gtk_primary_selection_offer_listener callback description:
 *
 * primary_data_offer - Is called for each MIME type available at
 *                      gtk_primary_selection_offer.
 */
static const struct gtk_primary_selection_offer_listener
    primary_selection_offer_listener_gtk = {primary_data_offer};

static const struct zwp_primary_selection_offer_v1_listener
    primary_selection_offer_listener_zwp_v1 = {primary_data_offer};

PrimaryDataOffer::PrimaryDataOffer(
    gtk_primary_selection_offer* aPrimaryDataOffer)
    : mPrimaryDataOfferGtk(aPrimaryDataOffer), mPrimaryDataOfferZwpV1(nullptr) {
  gtk_primary_selection_offer_add_listener(
      aPrimaryDataOffer, &primary_selection_offer_listener_gtk, this);
}

PrimaryDataOffer::PrimaryDataOffer(
    zwp_primary_selection_offer_v1* aPrimaryDataOffer)
    : mPrimaryDataOfferGtk(nullptr), mPrimaryDataOfferZwpV1(aPrimaryDataOffer) {
  zwp_primary_selection_offer_v1_add_listener(
      aPrimaryDataOffer, &primary_selection_offer_listener_zwp_v1, this);
}

PrimaryDataOffer::~PrimaryDataOffer(void) {
  if (mPrimaryDataOfferGtk) {
    gtk_primary_selection_offer_destroy(mPrimaryDataOfferGtk);
  }
  if (mPrimaryDataOfferZwpV1) {
    zwp_primary_selection_offer_v1_destroy(mPrimaryDataOfferZwpV1);
  }
}

void nsRetrievalContextWayland::RegisterNewDataOffer(
    wl_data_offer* aWaylandDataOffer) {
  LOGCLIP(
      ("nsRetrievalContextWayland::RegisterNewDataOffer (wl_data_offer) %p\n",
       aWaylandDataOffer));

  DataOffer* dataOffer = static_cast<DataOffer*>(
      g_hash_table_lookup(mActiveOffers, aWaylandDataOffer));
  MOZ_ASSERT(
      dataOffer == nullptr,
      "Registered WaylandDataOffer already exists. Wayland protocol error?");

  if (!dataOffer) {
    dataOffer = new WaylandDataOffer(aWaylandDataOffer);
    g_hash_table_insert(mActiveOffers, aWaylandDataOffer, dataOffer);
  }
}

void nsRetrievalContextWayland::RegisterNewDataOffer(
    gtk_primary_selection_offer* aPrimaryDataOffer) {
  LOGCLIP(("nsRetrievalContextWayland::RegisterNewDataOffer (primary) %p\n",
           aPrimaryDataOffer));

  DataOffer* dataOffer = static_cast<DataOffer*>(
      g_hash_table_lookup(mActiveOffers, aPrimaryDataOffer));
  MOZ_ASSERT(
      dataOffer == nullptr,
      "Registered PrimaryDataOffer already exists. Wayland protocol error?");

  if (!dataOffer) {
    dataOffer = new PrimaryDataOffer(aPrimaryDataOffer);
    g_hash_table_insert(mActiveOffers, aPrimaryDataOffer, dataOffer);
  }
}

void nsRetrievalContextWayland::RegisterNewDataOffer(
    zwp_primary_selection_offer_v1* aPrimaryDataOffer) {
  LOGCLIP(("nsRetrievalContextWayland::RegisterNewDataOffer (primary ZWP) %p\n",
           aPrimaryDataOffer));

  DataOffer* dataOffer = static_cast<DataOffer*>(
      g_hash_table_lookup(mActiveOffers, aPrimaryDataOffer));
  MOZ_ASSERT(
      dataOffer == nullptr,
      "Registered PrimaryDataOffer already exists. Wayland protocol error?");

  if (!dataOffer) {
    dataOffer = new PrimaryDataOffer(aPrimaryDataOffer);
    g_hash_table_insert(mActiveOffers, aPrimaryDataOffer, dataOffer);
  }
}

void nsRetrievalContextWayland::SetClipboardDataOffer(
    wl_data_offer* aWaylandDataOffer) {
  LOGCLIP(
      ("nsRetrievalContextWayland::SetClipboardDataOffer (wl_data_offer) %p\n",
       aWaylandDataOffer));

  // Delete existing clipboard data offer
  mClipboardOffer = nullptr;

  // null aWaylandDataOffer indicates that our clipboard content
  // is no longer valid and should be release.
  if (aWaylandDataOffer != nullptr) {
    DataOffer* dataOffer = static_cast<DataOffer*>(
        g_hash_table_lookup(mActiveOffers, aWaylandDataOffer));
#ifdef MOZ_LOGGING
    if (!dataOffer) {
      LOGCLIP(("    We're missing stored clipboard data offer!\n"));
    }
#endif
    if (dataOffer) {
      g_hash_table_remove(mActiveOffers, aWaylandDataOffer);
      mClipboardOffer = WrapUnique(dataOffer);
    }
  }
}

void nsRetrievalContextWayland::SetPrimaryDataOffer(
    gtk_primary_selection_offer* aPrimaryDataOffer) {
  LOGCLIP(("nsRetrievalContextWayland::SetPrimaryDataOffer (primary) %p\n",
           aPrimaryDataOffer));

  // Release any primary offer we have.
  mPrimaryOffer = nullptr;

  // aPrimaryDataOffer can be null which means we lost
  // the mouse selection.
  if (aPrimaryDataOffer) {
    DataOffer* dataOffer = static_cast<DataOffer*>(
        g_hash_table_lookup(mActiveOffers, aPrimaryDataOffer));
#ifdef MOZ_LOGGING
    if (!dataOffer) {
      LOGCLIP(("    We're missing stored primary data offer!\n"));
    }
#endif
    if (dataOffer) {
      g_hash_table_remove(mActiveOffers, aPrimaryDataOffer);
      mPrimaryOffer = WrapUnique(dataOffer);
    }
  }
}

void nsRetrievalContextWayland::SetPrimaryDataOffer(
    zwp_primary_selection_offer_v1* aPrimaryDataOffer) {
  LOGCLIP(("nsRetrievalContextWayland::SetPrimaryDataOffer (primary ZWP)%p\n",
           aPrimaryDataOffer));

  // Release any primary offer we have.
  mPrimaryOffer = nullptr;

  // aPrimaryDataOffer can be null which means we lost
  // the mouse selection.
  if (aPrimaryDataOffer) {
    DataOffer* dataOffer = static_cast<DataOffer*>(
        g_hash_table_lookup(mActiveOffers, aPrimaryDataOffer));
#ifdef MOZ_LOGGING
    if (!dataOffer) {
      LOGCLIP(("    We're missing stored primary data offer!\n"));
    }
#endif
    if (dataOffer) {
      g_hash_table_remove(mActiveOffers, aPrimaryDataOffer);
      mPrimaryOffer = WrapUnique(dataOffer);
    }
  }
}

// We have a new fresh data content.
// We should attach listeners to it and save for further use.
static void data_device_data_offer(void* data,
                                   struct wl_data_device* data_device,
                                   struct wl_data_offer* offer) {
  LOGCLIP(("data_device_data_offer(), wl_data_offer %p\n", offer));
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->RegisterNewDataOffer(offer);
}

// The new fresh data content is clipboard.
static void data_device_selection(void* data,
                                  struct wl_data_device* wl_data_device,
                                  struct wl_data_offer* offer) {
  LOGCLIP(("data_device_selection(), set wl_data_offer %p\n", offer));
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->SetClipboardDataOffer(offer);
}

static void data_device_enter(void* data, struct wl_data_device* data_device,
                              uint32_t time, struct wl_surface* surface,
                              int32_t x_fixed, int32_t y_fixed,
                              struct wl_data_offer* offer) {}

static void data_device_leave(void* data, struct wl_data_device* data_device) {}
static void data_device_motion(void* data, struct wl_data_device* data_device,
                               uint32_t time, int32_t x_fixed,
                               int32_t y_fixed) {}
static void data_device_drop(void* data, struct wl_data_device* data_device) {}

/* wl_data_device callback description:
 *
 * data_device_data_offer - It's called when there's a new wl_data_offer
 *                          available. We need to attach wl_data_offer_listener
 *                          to it to get available MIME types.
 *
 * data_device_selection - It's called when the new wl_data_offer
 *                         is a clipboard content.
 */
static const struct wl_data_device_listener data_device_listener = {
    data_device_data_offer, data_device_enter, data_device_leave,
    data_device_motion,     data_device_drop,  data_device_selection};

static void primary_selection_data_offer(
    void* data, struct gtk_primary_selection_device* primary_selection_device,
    struct gtk_primary_selection_offer* primary_offer) {
  LOGCLIP(("primary_selection_data_offer()\n"));
  // create and add listener
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->RegisterNewDataOffer(primary_offer);
}

static void primary_selection_data_offer(
    void* data,
    struct zwp_primary_selection_device_v1* primary_selection_device,
    struct zwp_primary_selection_offer_v1* primary_offer) {
  LOGCLIP(("primary_selection_data_offer()\n"));
  // create and add listener
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->RegisterNewDataOffer(primary_offer);
}

static void primary_selection_selection(
    void* data, struct gtk_primary_selection_device* primary_selection_device,
    struct gtk_primary_selection_offer* primary_offer) {
  LOGCLIP(("primary_selection_selection()\n"));
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->SetPrimaryDataOffer(primary_offer);
}

static void primary_selection_selection(
    void* data,
    struct zwp_primary_selection_device_v1* primary_selection_device,
    struct zwp_primary_selection_offer_v1* primary_offer) {
  LOGCLIP(("primary_selection_selection()\n"));
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->SetPrimaryDataOffer(primary_offer);
}

/* gtk_primary_selection_device callback description:
 *
 * primary_selection_data_offer - It's called when there's a new
 *                          gtk_primary_selection_offer available.  We need to
 *                          attach gtk_primary_selection_offer_listener to it
 *                          to get available MIME types.
 *
 * primary_selection_selection - It's called when the new
 *                          gtk_primary_selection_offer is a primary selection
 *                          content. It can be also called with
 *                          gtk_primary_selection_offer = null which means
 *                          there's no primary selection.
 */
static const struct gtk_primary_selection_device_listener
    primary_selection_device_listener_gtk = {
        primary_selection_data_offer,
        primary_selection_selection,
};

static const struct zwp_primary_selection_device_v1_listener
    primary_selection_device_listener_zwp_v1 = {
        primary_selection_data_offer,
        primary_selection_selection,
};

bool nsRetrievalContextWayland::HasSelectionSupport(void) {
  return (mDisplay->GetPrimarySelectionDeviceManagerZwpV1() != nullptr ||
          mDisplay->GetPrimarySelectionDeviceManagerGtk() != nullptr);
}

nsRetrievalContextWayland::nsRetrievalContextWayland(void)
    : mInitialized(false),
      mDisplay(WaylandDisplayGet()),
      mActiveOffers(g_hash_table_new(NULL, NULL)),
      mClipboardOffer(nullptr),
      mPrimaryOffer(nullptr),
      mClipboardRequestNumber(0),
      mClipboardData(nullptr),
      mClipboardDataLength(0) {
  wl_data_device* dataDevice = wl_data_device_manager_get_data_device(
      mDisplay->GetDataDeviceManager(), mDisplay->GetSeat());
  wl_data_device_add_listener(dataDevice, &data_device_listener, this);

  if (mDisplay->GetPrimarySelectionDeviceManagerZwpV1()) {
    zwp_primary_selection_device_v1* primaryDataDevice =
        zwp_primary_selection_device_manager_v1_get_device(
            mDisplay->GetPrimarySelectionDeviceManagerZwpV1(),
            mDisplay->GetSeat());
    zwp_primary_selection_device_v1_add_listener(
        primaryDataDevice, &primary_selection_device_listener_zwp_v1, this);
  } else if (mDisplay->GetPrimarySelectionDeviceManagerGtk()) {
    gtk_primary_selection_device* primaryDataDevice =
        gtk_primary_selection_device_manager_get_device(
            mDisplay->GetPrimarySelectionDeviceManagerGtk(),
            mDisplay->GetSeat());
    gtk_primary_selection_device_add_listener(
        primaryDataDevice, &primary_selection_device_listener_gtk, this);
  }

  mInitialized = true;
}

static gboolean offer_hash_remove(gpointer wl_offer, gpointer aDataOffer,
                                  gpointer user_data) {
#ifdef DEBUG
  nsPrintfCString msg("nsRetrievalContextWayland(): leaked nsDataOffer %p\n",
                      aDataOffer);
  NS_WARNING(msg.get());
#endif
  delete static_cast<DataOffer*>(aDataOffer);
  return true;
}

nsRetrievalContextWayland::~nsRetrievalContextWayland(void) {
  g_hash_table_foreach_remove(mActiveOffers, offer_hash_remove, nullptr);
  g_hash_table_destroy(mActiveOffers);
}

struct FastTrackClipboard {
  FastTrackClipboard(ClipboardDataType aDataType, int aClipboardRequestNumber,
                     nsRetrievalContextWayland* aRetrievalContex)
      : mClipboardRequestNumber(aClipboardRequestNumber),
        mRetrievalContex(aRetrievalContex),
        mDataType(aDataType) {}
  int mClipboardRequestNumber;
  nsRetrievalContextWayland* mRetrievalContex;
  ClipboardDataType mDataType;
};

static void wayland_clipboard_contents_received(
    GtkClipboard* clipboard, GtkSelectionData* selection_data, gpointer data) {
  LOGCLIP(("wayland_clipboard_contents_received() selection_data = %p\n",
           selection_data));
  FastTrackClipboard* fastTrack = static_cast<FastTrackClipboard*>(data);
  fastTrack->mRetrievalContex->TransferFastTrackClipboard(
      fastTrack->mDataType, fastTrack->mClipboardRequestNumber, selection_data);
  delete fastTrack;
}

void nsRetrievalContextWayland::TransferFastTrackClipboard(
    ClipboardDataType aDataType, int aClipboardRequestNumber,
    GtkSelectionData* aSelectionData) {
  LOGCLIP(
      ("nsRetrievalContextWayland::TransferFastTrackClipboard(), "
       "aSelectionData = %p\n",
       aSelectionData));

  if (mClipboardRequestNumber != aClipboardRequestNumber) {
    LOGCLIP(("    request number does not match!\n"));
    NS_WARNING("Received obsoleted clipboard data!");
  }
  LOGCLIP(("    request number matches\n"));

  int dataLength = gtk_selection_data_get_length(aSelectionData);
  if (dataLength < 0) {
    LOGCLIP(
        ("    gtk_clipboard_request_contents() failed to get clipboard "
         "data!\n"));
    ReleaseClipboardData(mClipboardData);
    return;
  }

  switch (aDataType) {
    case CLIPBOARD_TARGETS: {
      LOGCLIP(("    fastracking %d bytes of clipboard targets.\n", dataLength));
      gint n_targets = 0;
      GdkAtom* targets = nullptr;

      if (!gtk_selection_data_get_targets(aSelectionData, &targets,
                                          &n_targets) ||
          !n_targets) {
        ReleaseClipboardData(mClipboardData);
      }

      mClipboardData = reinterpret_cast<char*>(targets);
      mClipboardDataLength = n_targets;
      break;
    }
    case CLIPBOARD_DATA:
    case CLIPBOARD_TEXT: {
      LOGCLIP(("    fastracking %d bytes of data.\n", dataLength));
      mClipboardDataLength = dataLength;
      if (dataLength > 0) {
        mClipboardData = reinterpret_cast<char*>(
            g_malloc(sizeof(char) * (mClipboardDataLength + 1)));
        memcpy(mClipboardData, gtk_selection_data_get_data(aSelectionData),
               sizeof(char) * mClipboardDataLength);
        mClipboardData[mClipboardDataLength] = '\0';
        LOGCLIP(("    done, mClipboardData = %p\n", mClipboardData));
      } else {
        ReleaseClipboardData(mClipboardData);
      }
    }
  }
}

GdkAtom* nsRetrievalContextWayland::GetTargets(int32_t aWhichClipboard,
                                               int* aTargetNum) {
  /* If actual clipboard data is owned by us we don't need to go
   * through Wayland but we ask Gtk+ to directly call data
   * getter callback nsClipboard::SelectionGetEvent().
   * see gtk_selection_convert() at gtk+/gtkselection.c.
   */
  GdkAtom selection = GetSelectionAtom(aWhichClipboard);
  if (gdk_selection_owner_get(selection)) {
    LOGCLIP(("  Asking for internal clipboard content.\n"));
    mClipboardRequestNumber++;
    gtk_clipboard_request_contents(
        gtk_clipboard_get(selection), gdk_atom_intern("TARGETS", FALSE),
        wayland_clipboard_contents_received,
        new FastTrackClipboard(CLIPBOARD_TARGETS, mClipboardRequestNumber,
                               this));
    *aTargetNum = mClipboardDataLength;
    GdkAtom* targets = static_cast<GdkAtom*>((void*)mClipboardData);
    // We don't hold the target list internally but we transfer the ownership.
    mClipboardData = nullptr;
    mClipboardDataLength = 0;
    return targets;
  }

  if (GetSelectionAtom(aWhichClipboard) == GDK_SELECTION_CLIPBOARD) {
    if (mClipboardOffer) {
      return mClipboardOffer->GetTargets(aTargetNum);
    }
  } else {
    if (mPrimaryOffer) {
      return mPrimaryOffer->GetTargets(aTargetNum);
    }
  }

  *aTargetNum = 0;
  return nullptr;
}

const char* nsRetrievalContextWayland::GetClipboardData(
    const char* aMimeType, int32_t aWhichClipboard, uint32_t* aContentLength) {
  NS_ASSERTION(mClipboardData == nullptr && mClipboardDataLength == 0,
               "Looks like we're leaking clipboard data here!");

  LOGCLIP(("nsRetrievalContextWayland::GetClipboardData [%p] mime %s\n", this,
           aMimeType));

  /* If actual clipboard data is owned by us we don't need to go
   * through Wayland but we ask Gtk+ to directly call data
   * getter callback nsClipboard::SelectionGetEvent().
   * see gtk_selection_convert() at gtk+/gtkselection.c.
   */
  GdkAtom selection = GetSelectionAtom(aWhichClipboard);
  if (gdk_selection_owner_get(selection)) {
    LOGCLIP(("  Asking for internal clipboard content.\n"));
    mClipboardRequestNumber++;
    gtk_clipboard_request_contents(
        gtk_clipboard_get(selection), gdk_atom_intern(aMimeType, FALSE),
        wayland_clipboard_contents_received,
        new FastTrackClipboard(CLIPBOARD_DATA, mClipboardRequestNumber, this));
  } else {
    LOGCLIP(("  Asking for remote clipboard content.\n"));
    const auto& dataOffer =
        (selection == GDK_SELECTION_PRIMARY) ? mPrimaryOffer : mClipboardOffer;
    if (!dataOffer) {
      // Something went wrong. We're requested to provide clipboard data
      // but we haven't got any from wayland.
      LOGCLIP(("  We're missing dataOffer! mClipboardData = null\n"));
      mClipboardData = nullptr;
      mClipboardDataLength = 0;
    } else {
      LOGCLIP(
          ("  Getting clipboard data from compositor, MIME %s\n", aMimeType));
      mClipboardData = dataOffer->GetData(mDisplay->GetDisplay(), aMimeType,
                                          &mClipboardDataLength);
      LOGCLIP(("  Got %d bytes of data, mClipboardData = %p\n",
               mClipboardDataLength, mClipboardData));
    }
  }

  *aContentLength = mClipboardDataLength;
  return reinterpret_cast<const char*>(mClipboardData);
}

const char* nsRetrievalContextWayland::GetClipboardText(
    int32_t aWhichClipboard) {
  GdkAtom selection = GetSelectionAtom(aWhichClipboard);

  LOGCLIP(("nsRetrievalContextWayland::GetClipboardText [%p], clipboard %s\n",
           this,
           (selection == GDK_SELECTION_PRIMARY) ? "Primary" : "Selection"));

  const auto& dataOffer =
      (selection == GDK_SELECTION_PRIMARY) ? mPrimaryOffer : mClipboardOffer;
  if (!dataOffer) {
    LOGCLIP(("  We're missing data offer!\n"));
    return nullptr;
  }

  for (unsigned int i = 0; i < TEXT_MIME_TYPES_NUM; i++) {
    if (dataOffer->HasTarget(sTextMimeTypes[i])) {
      LOGCLIP(("  We have %s MIME type in clipboard, ask for it.\n",
               sTextMimeTypes[i]));
      uint32_t unused;
      return GetClipboardData(sTextMimeTypes[i], aWhichClipboard, &unused);
    }
  }

  LOGCLIP(("  There isn't text MIME type in clipboard!\n"));
  return nullptr;
}

void nsRetrievalContextWayland::ReleaseClipboardData(
    const char* aClipboardData) {
  LOGCLIP(("nsRetrievalContextWayland::ReleaseClipboardData [%p]\n",
           aClipboardData));
  if (aClipboardData != mClipboardData) {
    NS_WARNING("Wayland clipboard: Releasing unknown clipboard data!");
  }
  g_free((void*)mClipboardData);
  mClipboardDataLength = 0;
  mClipboardData = nullptr;
}
