/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsArrayUtils.h"
#include "nsClipboard.h"
#include "nsClipboardWayland.h"
#include "nsIStorageStream.h"
#include "nsIBinaryOutputStream.h"
#include "nsSupportsPrimitives.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsPrimitiveHelpers.h"
#include "nsIServiceManager.h"
#include "nsImageToPixbuf.h"
#include "nsStringStream.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"

#include "imgIContainer.h"

#include <gtk/gtk.h>
#include <poll.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <errno.h>

#include "wayland/gtk-primary-selection-client-protocol.h"

const char*
nsRetrievalContextWayland::sTextMimeTypes[TEXT_MIME_TYPES_NUM] =
{
    "text/plain;charset=utf-8",
    "UTF8_STRING",
    "COMPOUND_TEXT"
};

void
DataOffer::AddMIMEType(const char *aMimeType)
{
    GdkAtom atom = gdk_atom_intern(aMimeType, FALSE);
    mTargetMIMETypes.AppendElement(atom);
}

GdkAtom*
DataOffer::GetTargets(int* aTargetNum)
{
    int length = mTargetMIMETypes.Length();
    if (!length) {
        *aTargetNum = 0;
        return nullptr;
    }

    GdkAtom* targetList = reinterpret_cast<GdkAtom*>(
        g_malloc(sizeof(GdkAtom)*length));
    for (int32_t j = 0; j < length; j++) {
        targetList[j] = mTargetMIMETypes[j];
    }

    *aTargetNum = length;
    return targetList;
}

bool
DataOffer::HasTarget(const char *aMimeType)
{
    int length = mTargetMIMETypes.Length();
    for (int32_t j = 0; j < length; j++) {
        if (mTargetMIMETypes[j] == gdk_atom_intern(aMimeType, FALSE))
            return true;
    }
    return false;
}

char*
DataOffer::GetData(wl_display* aDisplay, const char* aMimeType,
                   uint32_t* aContentLength)
{
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
        return nullptr;

    if (!RequestDataTransfer(aMimeType, pipe_fd[1])) {
        NS_WARNING("DataOffer::RequestDataTransfer() failed!");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return nullptr;
    }

    close(pipe_fd[1]);
    wl_display_flush(aDisplay);

    struct pollfd fds;
    fds.fd = pipe_fd[0];
    fds.events = POLLIN;

    // Choose some reasonable timeout here
    int ret = poll(&fds, 1, kClipboardTimeout / 1000);
    if (!ret || ret == -1) {
        close(pipe_fd[0]);
        return nullptr;
    }

    GIOChannel *channel = g_io_channel_unix_new(pipe_fd[0]);
    GError* error = nullptr;
    char* clipboardData;

    g_io_channel_set_encoding(channel, nullptr, &error);
    if (!error) {
        gsize length = 0;
        g_io_channel_read_to_end(channel, &clipboardData, &length, &error);
        if (length == 0) {
            // We don't have valid clipboard data although
            // g_io_channel_read_to_end() allocated clipboardData for us.
            // Release it now and return nullptr to indicate
            // we don't have reqested data flavour.
            g_free((void *)clipboardData);
            clipboardData = nullptr;
        }
        *aContentLength = length;
    }

    if (error) {
        NS_WARNING(
            nsPrintfCString("Unexpected error when reading clipboard data: %s",
                            error->message).get());
        g_error_free(error);
    }

    g_io_channel_unref(channel);
    close(pipe_fd[0]);

    return clipboardData;
}

bool
WaylandDataOffer::RequestDataTransfer(const char* aMimeType, int fd)
{
    if (mWaylandDataOffer) {
        wl_data_offer_receive(mWaylandDataOffer, aMimeType, fd);
        return true;
    }

    return false;
}

static void
data_offer_offer (void                 *data,
                  struct wl_data_offer *wl_data_offer,
                  const char           *type)
{
    auto *offer = static_cast<DataOffer*>(data);
    offer->AddMIMEType(type);
}

static void
data_offer_source_actions(void *data,
                          struct wl_data_offer *wl_data_offer,
                          uint32_t source_actions)
{
}

static void
data_offer_action(void *data,
                  struct wl_data_offer *wl_data_offer,
                  uint32_t dnd_action)
{
}

/* wl_data_offer callback description:
 *
 * data_offer_offer - Is called for each MIME type available at wl_data_offer.
 * data_offer_source_actions - Exposes all available D&D actions.
 * data_offer_action - Expose one actually selected D&D action.
 */
static const struct wl_data_offer_listener data_offer_listener = {
    data_offer_offer,
    data_offer_source_actions,
    data_offer_action
};

WaylandDataOffer::WaylandDataOffer(wl_data_offer* aWaylandDataOffer)
  : mWaylandDataOffer(aWaylandDataOffer)
{
    wl_data_offer_add_listener(mWaylandDataOffer, &data_offer_listener, this);
}

WaylandDataOffer::~WaylandDataOffer(void)
{
    if(mWaylandDataOffer) {
        wl_data_offer_destroy(mWaylandDataOffer);
    }
}

bool
PrimaryDataOffer::RequestDataTransfer(const char* aMimeType, int fd)
{
    if (mPrimaryDataOffer) {
        gtk_primary_selection_offer_receive(mPrimaryDataOffer, aMimeType, fd);
        return true;
    }
    return false;
}

static void
primary_data_offer(void *data,
                   gtk_primary_selection_offer *gtk_primary_selection_offer,
                   const char *mime_type)
{
    auto *offer = static_cast<DataOffer*>(data);
    offer->AddMIMEType(mime_type);
}

/* gtk_primary_selection_offer_listener callback description:
 *
 * primary_data_offer - Is called for each MIME type available at
 *                      gtk_primary_selection_offer.
 */
static const struct gtk_primary_selection_offer_listener
primary_selection_offer_listener = {
    primary_data_offer
};

PrimaryDataOffer::PrimaryDataOffer(gtk_primary_selection_offer* aPrimaryDataOffer)
  : mPrimaryDataOffer(aPrimaryDataOffer)
{
    gtk_primary_selection_offer_add_listener(aPrimaryDataOffer,
        &primary_selection_offer_listener, this);
}

PrimaryDataOffer::~PrimaryDataOffer(void)
{
    if(mPrimaryDataOffer) {
        gtk_primary_selection_offer_destroy(mPrimaryDataOffer);
    }
}

void
nsRetrievalContextWayland::RegisterDataOffer(wl_data_offer *aWaylandDataOffer)
{
  DataOffer* dataOffer =
      static_cast<DataOffer*>(g_hash_table_lookup(mActiveOffers,
                                                  aWaylandDataOffer));
  if (!dataOffer) {
      dataOffer = new WaylandDataOffer(aWaylandDataOffer);
      g_hash_table_insert(mActiveOffers, aWaylandDataOffer, dataOffer);
  }
}

void
nsRetrievalContextWayland::RegisterDataOffer(
    gtk_primary_selection_offer *aPrimaryDataOffer)
{
  DataOffer* dataOffer =
      static_cast<DataOffer*>(g_hash_table_lookup(mActiveOffers,
                                                  aPrimaryDataOffer));
  if (!dataOffer) {
      dataOffer = new PrimaryDataOffer(aPrimaryDataOffer);
      g_hash_table_insert(mActiveOffers, aPrimaryDataOffer, dataOffer);
  }
}

void
nsRetrievalContextWayland::SetClipboardDataOffer(wl_data_offer *aWaylandDataOffer)
{
    DataOffer* dataOffer =
        static_cast<DataOffer*>(g_hash_table_lookup(mActiveOffers,
                                                    aWaylandDataOffer));
    NS_ASSERTION(dataOffer, "We're missing clipboard data offer!");
    if (dataOffer) {
        g_hash_table_remove(mActiveOffers, aWaylandDataOffer);
        mClipboardOffer = dataOffer;
    }
}

void
nsRetrievalContextWayland::SetPrimaryDataOffer(
      gtk_primary_selection_offer *aPrimaryDataOffer)
{
    if (aPrimaryDataOffer == nullptr) {
        // Release any primary offer we have.
        mPrimaryOffer = nullptr;
    } else {
        DataOffer* dataOffer =
            static_cast<DataOffer*>(g_hash_table_lookup(mActiveOffers,
                                                        aPrimaryDataOffer));
        NS_ASSERTION(dataOffer, "We're missing primary data offer!");
        if (dataOffer) {
            g_hash_table_remove(mActiveOffers, aPrimaryDataOffer);
            mPrimaryOffer = dataOffer;
        }
    }
}

void
nsRetrievalContextWayland::ClearDataOffers(void)
{
    if (mClipboardOffer)
        mClipboardOffer = nullptr;
    if (mPrimaryOffer)
        mPrimaryOffer = nullptr;
}

// We have a new fresh data content.
// We should attach listeners to it and save for further use.
static void
data_device_data_offer (void                  *data,
                        struct wl_data_device *data_device,
                        struct wl_data_offer  *offer)
{
    nsRetrievalContextWayland *context =
        static_cast<nsRetrievalContextWayland*>(data);
    context->RegisterDataOffer(offer);
}

// The new fresh data content is clipboard.
static void
data_device_selection (void                  *data,
                       struct wl_data_device *wl_data_device,
                       struct wl_data_offer  *offer)
{
    nsRetrievalContextWayland *context =
        static_cast<nsRetrievalContextWayland*>(data);
    context->SetClipboardDataOffer(offer);
}

// The new fresh wayland data content is drag and drop.
static void
data_device_enter (void                  *data,
                   struct wl_data_device *data_device,
                   uint32_t               time,
                   struct wl_surface     *surface,
                   int32_t                x,
                   int32_t                y,
                   struct wl_data_offer  *offer)
{
}

static void
data_device_leave (void                  *data,
                   struct wl_data_device *data_device)
{
}

static void
data_device_motion (void                  *data,
                    struct wl_data_device *data_device,
                    uint32_t               time,
                    int32_t                x,
                    int32_t                y)
{
}

static void
data_device_drop (void                  *data,
                  struct wl_data_device *data_device)
{
}

/* wl_data_device callback description:
 *
 * data_device_data_offer - It's called when there's a new wl_data_offer
 *                          available. We need to attach wl_data_offer_listener
 *                          to it to get available MIME types.
 *
 * data_device_selection - It's called when the new wl_data_offer
 *                         is a clipboard content.
 *
 * data_device_enter - It's called when the new wl_data_offer is a drag & drop
 *                     content and it's tied to actual wl_surface.
 * data_device_leave - It's called when the wl_data_offer (drag & dop) is not
 *                     valid any more.
 * data_device_motion - It's called when the drag and drop selection moves across
 *                      wl_surface.
 * data_device_drop - It's called when D&D operation is sucessfully finished and
 *                    we can read the data from D&D.
 *                    It's generated only if we call wl_data_offer_accept() and
 *                    wl_data_offer_set_actions() from data_device_motion callback.
 */
static const struct wl_data_device_listener data_device_listener = {
    data_device_data_offer,
    data_device_enter,
    data_device_leave,
    data_device_motion,
    data_device_drop,
    data_device_selection
};

static void
primary_selection_data_offer (void                                *data,
                              struct gtk_primary_selection_device *gtk_primary_selection_device,
                              struct gtk_primary_selection_offer  *gtk_primary_offer)
{
    // create and add listener
    nsRetrievalContextWayland *context =
        static_cast<nsRetrievalContextWayland*>(data);
    context->RegisterDataOffer(gtk_primary_offer);
}

static void
primary_selection_selection (void                                *data,
                             struct gtk_primary_selection_device *gtk_primary_selection_device,
                             struct gtk_primary_selection_offer  *gtk_primary_offer)
{
    nsRetrievalContextWayland *context =
        static_cast<nsRetrievalContextWayland*>(data);
    context->SetPrimaryDataOffer(gtk_primary_offer);
}

static const struct
gtk_primary_selection_device_listener primary_selection_device_listener = {
    primary_selection_data_offer,
    primary_selection_selection,
};

bool
nsRetrievalContextWayland::HasSelectionSupport(void)
{
    return mPrimarySelectionDataDeviceManager != nullptr;
}

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
                       uint32_t format, int fd, uint32_t size)
{
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface,
                      struct wl_array *keys)
{
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface)
{
    // We lost focus so our clipboard data are outdated
    nsRetrievalContextWayland *context =
        static_cast<nsRetrievalContextWayland*>(data);

    context->ClearDataOffers();
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                    uint32_t serial, uint32_t time, uint32_t key,
                    uint32_t state)
{
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                          uint32_t serial, uint32_t mods_depressed,
                          uint32_t mods_latched, uint32_t mods_locked,
                          uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
};

void
nsRetrievalContextWayland::ConfigureKeyboard(wl_seat_capability caps)
{
  // ConfigureKeyboard() is called when wl_seat configuration is changed.
  // We look for the keyboard only, get it when is't available and release it
  // when it's lost (we don't have focus for instance).
  if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
      mKeyboard = wl_seat_get_keyboard(mSeat);
      wl_keyboard_add_listener(mKeyboard, &keyboard_listener, this);
  } else if (mKeyboard && !(caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
      wl_keyboard_destroy(mKeyboard);
      mKeyboard = nullptr;
  }
}

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
                         unsigned int caps)
{
    nsRetrievalContextWayland *context =
        static_cast<nsRetrievalContextWayland*>(data);
    context->ConfigureKeyboard((wl_seat_capability)caps);
}

static const struct wl_seat_listener seat_listener = {
      seat_handle_capabilities,
};

void
nsRetrievalContextWayland::InitDataDeviceManager(wl_registry *registry,
                                                 uint32_t id,
                                                 uint32_t version)
{
    int data_device_manager_version = MIN (version, 3);
    mDataDeviceManager = (wl_data_device_manager *)wl_registry_bind(registry, id,
        &wl_data_device_manager_interface, data_device_manager_version);
}

void
nsRetrievalContextWayland::InitPrimarySelectionDataDeviceManager(
  wl_registry *registry, uint32_t id)
{
    mPrimarySelectionDataDeviceManager =
        (gtk_primary_selection_device_manager *)wl_registry_bind(registry, id,
            &gtk_primary_selection_device_manager_interface, 1);
}

void
nsRetrievalContextWayland::InitSeat(wl_registry *registry,
                                    uint32_t id, uint32_t version,
                                    void *data)
{
    mSeat = (wl_seat*)wl_registry_bind(registry, id, &wl_seat_interface, 1);
    wl_seat_add_listener(mSeat, &seat_listener, data);
}

static void
gdk_registry_handle_global(void               *data,
                           struct wl_registry *registry,
                           uint32_t            id,
                           const char         *interface,
                           uint32_t            version)
{
    nsRetrievalContextWayland *context =
        static_cast<nsRetrievalContextWayland*>(data);

    if (strcmp (interface, "wl_data_device_manager") == 0) {
        context->InitDataDeviceManager(registry, id, version);
    } else if (strcmp(interface, "wl_seat") == 0) {
        context->InitSeat(registry, id, version, data);
    } else if (strcmp (interface, "gtk_primary_selection_device_manager") == 0) {
        context->InitPrimarySelectionDataDeviceManager(registry, id);
    }
}

static void
gdk_registry_handle_global_remove(void               *data,
                                 struct wl_registry *registry,
                                 uint32_t            id)
{
}

static const struct wl_registry_listener clipboard_registry_listener = {
    gdk_registry_handle_global,
    gdk_registry_handle_global_remove
};

nsRetrievalContextWayland::nsRetrievalContextWayland(void)
  : mInitialized(false)
  , mSeat(nullptr)
  , mDataDeviceManager(nullptr)
  , mPrimarySelectionDataDeviceManager(nullptr)
  , mKeyboard(nullptr)
  , mActiveOffers(g_hash_table_new(NULL, NULL))
  , mClipboardOffer(nullptr)
  , mPrimaryOffer(nullptr)
  , mClipboardRequestNumber(0)
  , mClipboardData(nullptr)
  , mClipboardDataLength(0)
{
    // Available as of GTK 3.8+
    static auto sGdkWaylandDisplayGetWlDisplay =
        (wl_display *(*)(GdkDisplay *))
        dlsym(RTLD_DEFAULT, "gdk_wayland_display_get_wl_display");

    mDisplay = sGdkWaylandDisplayGetWlDisplay(gdk_display_get_default());
    wl_registry_add_listener(wl_display_get_registry(mDisplay),
                             &clipboard_registry_listener, this);
    // Call wl_display_roundtrip() twice to make sure all
    // callbacks are processed.
    wl_display_roundtrip(mDisplay);
    wl_display_roundtrip(mDisplay);

    // mSeat/mDataDeviceManager should be set now by
    // gdk_registry_handle_global() as a response to
    // wl_registry_add_listener() call.
    if (!mDataDeviceManager || !mSeat)
        return;

    wl_data_device *dataDevice =
        wl_data_device_manager_get_data_device(mDataDeviceManager, mSeat);
    wl_data_device_add_listener(dataDevice, &data_device_listener, this);
    // We have to call wl_display_roundtrip() twice otherwise data_offer_listener
    // may not be processed because it's called from data_device_data_offer
    // callback.
    wl_display_roundtrip(mDisplay);
    wl_display_roundtrip(mDisplay);

    if (mPrimarySelectionDataDeviceManager) {
        gtk_primary_selection_device *primaryDataDevice =
            gtk_primary_selection_device_manager_get_device(mPrimarySelectionDataDeviceManager,
                                                            mSeat);
        gtk_primary_selection_device_add_listener(primaryDataDevice,
            &primary_selection_device_listener, this);
    }

    mInitialized = true;
}

nsRetrievalContextWayland::~nsRetrievalContextWayland(void)
{
    g_hash_table_destroy(mActiveOffers);
}

GdkAtom*
nsRetrievalContextWayland::GetTargets(int32_t aWhichClipboard,
                                      int* aTargetNum)
{
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

struct FastTrackClipboard
{
    FastTrackClipboard(int aClipboardRequestNumber,
                       nsRetrievalContextWayland* aRetrievalContex)
    : mClipboardRequestNumber(aClipboardRequestNumber)
    , mRetrievalContex(aRetrievalContex)
    {}

    int                        mClipboardRequestNumber;
    nsRetrievalContextWayland* mRetrievalContex;
};

static void
wayland_clipboard_contents_received(GtkClipboard     *clipboard,
                                    GtkSelectionData *selection_data,
                                    gpointer          data)
{
    FastTrackClipboard* fastTrack = static_cast<FastTrackClipboard*>(data);
    fastTrack->mRetrievalContex->TransferFastTrackClipboard(
        fastTrack->mClipboardRequestNumber, selection_data);
    delete fastTrack;
}

void
nsRetrievalContextWayland::TransferFastTrackClipboard(
    int aClipboardRequestNumber, GtkSelectionData *aSelectionData)
{
    if (mClipboardRequestNumber == aClipboardRequestNumber) {
        int dataLength = gtk_selection_data_get_length(aSelectionData);
        if (dataLength > 0) {
            mClipboardDataLength = dataLength;
            mClipboardData = reinterpret_cast<char*>(
                g_malloc(sizeof(char)*mClipboardDataLength));
            memcpy(mClipboardData, gtk_selection_data_get_data(aSelectionData),
                   sizeof(char)*mClipboardDataLength);
        }
    } else {
        NS_WARNING("Received obsoleted clipboard data!");
    }
}

const char*
nsRetrievalContextWayland::GetClipboardData(const char* aMimeType,
                                            int32_t aWhichClipboard,
                                            uint32_t* aContentLength)
{
    NS_ASSERTION(mClipboardData == nullptr && mClipboardDataLength == 0,
                 "Looks like we're leaking clipboard data here!");

    /* If actual clipboard data is owned by us we don't need to go
     * through Wayland but we ask Gtk+ to directly call data
     * getter callback nsClipboard::SelectionGetEvent().
     * see gtk_selection_convert() at gtk+/gtkselection.c.
     */
    GdkAtom selection = GetSelectionAtom(aWhichClipboard);
    if (gdk_selection_owner_get(selection)) {
        mClipboardRequestNumber++;
        gtk_clipboard_request_contents(gtk_clipboard_get(selection),
            gdk_atom_intern(aMimeType, FALSE),
            wayland_clipboard_contents_received,
            new FastTrackClipboard(mClipboardRequestNumber, this));
    } else {
        DataOffer* dataOffer = (selection == GDK_SELECTION_PRIMARY) ?
                                  mPrimaryOffer : mClipboardOffer;
        if (!dataOffer) {
            // Something went wrong. We're requested to provide clipboard data
            // but we haven't got any from wayland.
            NS_WARNING("Requested data without valid DataOffer!");
            mClipboardData = nullptr;
            mClipboardDataLength = 0;
        } else {
            mClipboardData = dataOffer->GetData(mDisplay,
                aMimeType, &mClipboardDataLength);
        }
    }

    *aContentLength = mClipboardDataLength;
    return reinterpret_cast<const char*>(mClipboardData);
}

const char*
nsRetrievalContextWayland::GetClipboardText(int32_t aWhichClipboard)
{
    GdkAtom selection = GetSelectionAtom(aWhichClipboard);
    DataOffer* dataOffer = (selection == GDK_SELECTION_PRIMARY) ?
                            mPrimaryOffer : mClipboardOffer;
    if (!dataOffer)
        return nullptr;

    for (unsigned int i = 0; i < sizeof(sTextMimeTypes); i++) {
        if (dataOffer->HasTarget(sTextMimeTypes[i])) {
            uint32_t unused;
            return GetClipboardData(sTextMimeTypes[i], aWhichClipboard,
                                    &unused);
        }
    }
    return nullptr;
}

void nsRetrievalContextWayland::ReleaseClipboardData(const char* aClipboardData)
{
    NS_ASSERTION(aClipboardData == mClipboardData,
        "Releasing unknown clipboard data!");
    g_free((void*)aClipboardData);

    mClipboardData = nullptr;
    mClipboardDataLength = 0;
}
