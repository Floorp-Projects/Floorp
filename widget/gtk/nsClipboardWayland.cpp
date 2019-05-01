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
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsDragService.h"
#include "mozwayland/mozwayland.h"
#include "nsWaylandDisplay.h"

#include "imgIContainer.h"

#include <gtk/gtk.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

const char* nsRetrievalContextWayland::sTextMimeTypes[TEXT_MIME_TYPES_NUM] = {
    "text/plain;charset=utf-8", "UTF8_STRING", "COMPOUND_TEXT"};

static inline GdkDragAction wl_to_gdk_actions(uint32_t dnd_actions) {
  GdkDragAction actions = GdkDragAction(0);

  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    actions = GdkDragAction(actions | GDK_ACTION_COPY);
  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    actions = GdkDragAction(actions | GDK_ACTION_MOVE);

  return actions;
}

static inline uint32_t gdk_to_wl_actions(GdkDragAction action) {
  uint32_t dnd_actions = 0;

  if (action & (GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_PRIVATE))
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  if (action & GDK_ACTION_MOVE)
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;

  return dnd_actions;
}

static GtkWidget* get_gtk_widget_for_wl_surface(struct wl_surface* surface) {
  GdkWindow* gdkParentWindow =
      static_cast<GdkWindow*>(wl_surface_get_user_data(surface));

  gpointer user_data = nullptr;
  gdk_window_get_user_data(gdkParentWindow, &user_data);

  return GTK_WIDGET(user_data);
}

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
    if (mTargetMIMETypes[j] == gdk_atom_intern(aMimeType, FALSE)) return true;
  }
  return false;
}

char* DataOffer::GetData(wl_display* aDisplay, const char* aMimeType,
                         uint32_t* aContentLength) {
  int pipe_fd[2];
  if (pipe(pipe_fd) == -1) return nullptr;

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

  return clipboardData;
}

bool WaylandDataOffer::RequestDataTransfer(const char* aMimeType, int fd) {
  if (mWaylandDataOffer) {
    wl_data_offer_receive(mWaylandDataOffer, aMimeType, fd);
    return true;
  }

  return false;
}

void WaylandDataOffer::DragOfferAccept(const char* aMimeType, uint32_t aTime) {
  wl_data_offer_accept(mWaylandDataOffer, aTime, aMimeType);
}

/* We follow logic of gdk_wayland_drag_context_commit_status()/gdkdnd-wayland.c
 * here.
 */
void WaylandDataOffer::SetDragStatus(GdkDragAction aAction, uint32_t aTime) {
  uint32_t dnd_actions = gdk_to_wl_actions(aAction);
  uint32_t all_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
                         WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;

  wl_data_offer_set_actions(mWaylandDataOffer, all_actions, dnd_actions);

  /* Workaround Wayland D&D architecture here. To get the data_device_drop()
     signal (which routes to nsDragService::GetData() call) we need to
     accept at least one mime type before data_device_leave().

     Real wl_data_offer_accept() for actualy requested data mime type is
     called from nsDragService::GetData().
  */
  if (mTargetMIMETypes[0]) {
    wl_data_offer_accept(mWaylandDataOffer, aTime,
                         gdk_atom_name(mTargetMIMETypes[0]));
  }
}

void WaylandDataOffer::SetSelectedDragAction(uint32_t aWaylandAction) {
  mSelectedDragAction = aWaylandAction;
}

GdkDragAction WaylandDataOffer::GetSelectedDragAction() {
  return wl_to_gdk_actions(mSelectedDragAction);
}

void WaylandDataOffer::SetAvailableDragActions(uint32_t aWaylandActions) {
  mAvailableDragAction = aWaylandActions;
}

GdkDragAction WaylandDataOffer::GetAvailableDragActions() {
  return wl_to_gdk_actions(mAvailableDragAction);
}

static void data_offer_offer(void* data, struct wl_data_offer* wl_data_offer,
                             const char* type) {
  auto* offer = static_cast<DataOffer*>(data);
  offer->AddMIMEType(type);
}

/* Advertise all available drag and drop actions from source.
 * We don't use that but follow gdk_wayland_drag_context_commit_status()
 * from gdkdnd-wayland.c here.
 */
static void data_offer_source_actions(void* data,
                                      struct wl_data_offer* wl_data_offer,
                                      uint32_t source_actions) {
  auto* offer = static_cast<WaylandDataOffer*>(data);
  offer->SetAvailableDragActions(source_actions);
}

/* Advertise recently selected drag and drop action by compositor, based
 * on source actions and user choice (key modifiers, etc.).
 */
static void data_offer_action(void* data, struct wl_data_offer* wl_data_offer,
                              uint32_t dnd_action) {
  auto* offer = static_cast<WaylandDataOffer*>(data);
  offer->SetSelectedDragAction(dnd_action);
}

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
  if (mPrimaryDataOffer) {
    gtk_primary_selection_offer_receive(mPrimaryDataOffer, aMimeType, fd);
    return true;
  }
  return false;
}

static void primary_data_offer(
    void* data, gtk_primary_selection_offer* gtk_primary_selection_offer,
    const char* mime_type) {
  auto* offer = static_cast<DataOffer*>(data);
  offer->AddMIMEType(mime_type);
}

/* gtk_primary_selection_offer_listener callback description:
 *
 * primary_data_offer - Is called for each MIME type available at
 *                      gtk_primary_selection_offer.
 */
static const struct gtk_primary_selection_offer_listener
    primary_selection_offer_listener = {primary_data_offer};

PrimaryDataOffer::PrimaryDataOffer(
    gtk_primary_selection_offer* aPrimaryDataOffer)
    : mPrimaryDataOffer(aPrimaryDataOffer) {
  gtk_primary_selection_offer_add_listener(
      aPrimaryDataOffer, &primary_selection_offer_listener, this);
}

PrimaryDataOffer::~PrimaryDataOffer(void) {
  if (mPrimaryDataOffer) {
    gtk_primary_selection_offer_destroy(mPrimaryDataOffer);
  }
}

NS_IMPL_ISUPPORTS(nsWaylandDragContext, nsISupports);

nsWaylandDragContext::nsWaylandDragContext(WaylandDataOffer* aDataOffer,
                                           wl_display* aDisplay)
    : mDataOffer(aDataOffer),
      mDisplay(aDisplay),
      mTime(0),
      mGtkWidget(nullptr),
      mX(0),
      mY(0) {}

void nsWaylandDragContext::DropDataEnter(GtkWidget* aGtkWidget, uint32_t aTime,
                                         nscoord aX, nscoord aY) {
  mTime = aTime;
  mGtkWidget = aGtkWidget;
  mX = aX;
  mY = aY;
}

void nsWaylandDragContext::DropMotion(uint32_t aTime, nscoord aX, nscoord aY) {
  mTime = aTime;
  mX = aX;
  mY = aY;
}

void nsWaylandDragContext::GetLastDropInfo(uint32_t* aTime, nscoord* aX,
                                           nscoord* aY) {
  *aTime = mTime;
  *aX = mX;
  *aY = mY;
}

void nsWaylandDragContext::SetDragStatus(GdkDragAction aAction) {
  mDataOffer->SetDragStatus(aAction, mTime);
}

GdkDragAction nsWaylandDragContext::GetSelectedDragAction() {
  GdkDragAction gdkAction = mDataOffer->GetSelectedDragAction();

  // We emulate gdk_drag_context_get_actions() here.
  if (!gdkAction) {
    gdkAction = mDataOffer->GetAvailableDragActions();
  }

  return gdkAction;
}

GList* nsWaylandDragContext::GetTargets() {
  int targetNums;
  GdkAtom* atoms = mDataOffer->GetTargets(&targetNums);

  GList* targetList = nullptr;
  for (int i = 0; i < targetNums; i++) {
    targetList = g_list_append(targetList, GDK_ATOM_TO_POINTER(atoms[i]));
  }

  return targetList;
}

char* nsWaylandDragContext::GetData(const char* aMimeType,
                                    uint32_t* aContentLength) {
  mDataOffer->DragOfferAccept(aMimeType, mTime);
  return mDataOffer->GetData(mDisplay, aMimeType, aContentLength);
}

void nsRetrievalContextWayland::RegisterNewDataOffer(
    wl_data_offer* aWaylandDataOffer) {
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
  // Delete existing clipboard data offer
  mClipboardOffer = nullptr;

  // null aWaylandDataOffer indicates that our clipboard content
  // is no longer valid and should be release.
  if (aWaylandDataOffer != nullptr) {
    DataOffer* dataOffer = static_cast<DataOffer*>(
        g_hash_table_lookup(mActiveOffers, aWaylandDataOffer));
    NS_ASSERTION(dataOffer, "We're missing stored clipboard data offer!");
    if (dataOffer) {
      g_hash_table_remove(mActiveOffers, aWaylandDataOffer);
      mClipboardOffer = dataOffer;
    }
  }
}

void nsRetrievalContextWayland::SetPrimaryDataOffer(
    gtk_primary_selection_offer* aPrimaryDataOffer) {
  // Release any primary offer we have.
  mPrimaryOffer = nullptr;

  // aPrimaryDataOffer can be null which means we lost
  // the mouse selection.
  if (aPrimaryDataOffer) {
    DataOffer* dataOffer = static_cast<DataOffer*>(
        g_hash_table_lookup(mActiveOffers, aPrimaryDataOffer));
    NS_ASSERTION(dataOffer, "We're missing primary data offer!");
    if (dataOffer) {
      g_hash_table_remove(mActiveOffers, aPrimaryDataOffer);
      mPrimaryOffer = dataOffer;
    }
  }
}

void nsRetrievalContextWayland::AddDragAndDropDataOffer(
    wl_data_offer* aDropDataOffer) {
  // Remove any existing D&D contexts.
  mDragContext = nullptr;

  WaylandDataOffer* dataOffer = static_cast<WaylandDataOffer*>(
      g_hash_table_lookup(mActiveOffers, aDropDataOffer));
  NS_ASSERTION(dataOffer, "We're missing drag and drop data offer!");
  if (dataOffer) {
    g_hash_table_remove(mActiveOffers, aDropDataOffer);
    mDragContext = new nsWaylandDragContext(dataOffer, mDisplay->GetDisplay());
  }
}

nsWaylandDragContext* nsRetrievalContextWayland::GetDragContext(void) {
  return mDragContext;
}

void nsRetrievalContextWayland::ClearDragAndDropDataOffer(void) {
  mDragContext = nullptr;
}

// We have a new fresh data content.
// We should attach listeners to it and save for further use.
static void data_device_data_offer(void* data,
                                   struct wl_data_device* data_device,
                                   struct wl_data_offer* offer) {
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->RegisterNewDataOffer(offer);
}

// The new fresh data content is clipboard.
static void data_device_selection(void* data,
                                  struct wl_data_device* wl_data_device,
                                  struct wl_data_offer* offer) {
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->SetClipboardDataOffer(offer);
}

// The new fresh wayland data content is drag and drop.
static void data_device_enter(void* data, struct wl_data_device* data_device,
                              uint32_t time, struct wl_surface* surface,
                              int32_t x_fixed, int32_t y_fixed,
                              struct wl_data_offer* offer) {
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->AddDragAndDropDataOffer(offer);

  nsWaylandDragContext* dragContext = context->GetDragContext();

  GtkWidget* gtkWidget = get_gtk_widget_for_wl_surface(surface);
  if (!gtkWidget) {
    NS_WARNING("DragAndDrop: Unable to get GtkWidget for wl_surface!");
    return;
  }

  LOGDRAG(("nsWindow data_device_enter for GtkWidget %p\n", (void*)gtkWidget));

  dragContext->DropDataEnter(gtkWidget, time, wl_fixed_to_int(x_fixed),
                             wl_fixed_to_int(y_fixed));
}

static void data_device_leave(void* data, struct wl_data_device* data_device) {
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);

  nsWaylandDragContext* dropContext = context->GetDragContext();
  WindowDragLeaveHandler(dropContext->GetWidget());

  context->ClearDragAndDropDataOffer();
}

static void data_device_motion(void* data, struct wl_data_device* data_device,
                               uint32_t time, int32_t x_fixed,
                               int32_t y_fixed) {
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);

  nsWaylandDragContext* dropContext = context->GetDragContext();

  nscoord x = wl_fixed_to_int(x_fixed);
  nscoord y = wl_fixed_to_int(y_fixed);
  dropContext->DropMotion(time, x, y);

  WindowDragMotionHandler(dropContext->GetWidget(), nullptr, dropContext, x, y,
                          time);
}

static void data_device_drop(void* data, struct wl_data_device* data_device) {
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);

  nsWaylandDragContext* dropContext = context->GetDragContext();

  uint32_t time;
  nscoord x, y;
  dropContext->GetLastDropInfo(&time, &x, &y);

  WindowDragDropHandler(dropContext->GetWidget(), nullptr, dropContext, x, y,
                        time);
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
 * data_device_motion - It's called when the drag and drop selection moves
 *                      across wl_surface.
 * data_device_drop - It's called when D&D operation is sucessfully finished
 *                    and we can read the data from D&D.
 *                    It's generated only if we call wl_data_offer_accept() and
 *                    wl_data_offer_set_actions() from data_device_motion
 *                    callback.
 */
static const struct wl_data_device_listener data_device_listener = {
    data_device_data_offer, data_device_enter, data_device_leave,
    data_device_motion,     data_device_drop,  data_device_selection};

static void primary_selection_data_offer(
    void* data,
    struct gtk_primary_selection_device* gtk_primary_selection_device,
    struct gtk_primary_selection_offer* gtk_primary_offer) {
  // create and add listener
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->RegisterNewDataOffer(gtk_primary_offer);
}

static void primary_selection_selection(
    void* data,
    struct gtk_primary_selection_device* gtk_primary_selection_device,
    struct gtk_primary_selection_offer* gtk_primary_offer) {
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->SetPrimaryDataOffer(gtk_primary_offer);
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
    primary_selection_device_listener = {
        primary_selection_data_offer,
        primary_selection_selection,
};

bool nsRetrievalContextWayland::HasSelectionSupport(void) {
  return mDisplay->GetPrimarySelectionDeviceManager() != nullptr;
}

nsRetrievalContextWayland::nsRetrievalContextWayland(void)
    : mInitialized(false),
      mDisplay(WaylandDisplayGet()),
      mActiveOffers(g_hash_table_new(NULL, NULL)),
      mClipboardOffer(nullptr),
      mPrimaryOffer(nullptr),
      mDragContext(nullptr),
      mClipboardRequestNumber(0),
      mClipboardData(nullptr),
      mClipboardDataLength(0) {
  wl_data_device* dataDevice = wl_data_device_manager_get_data_device(
      mDisplay->GetDataDeviceManager(), mDisplay->GetSeat());
  wl_data_device_add_listener(dataDevice, &data_device_listener, this);

  gtk_primary_selection_device_manager* manager =
      mDisplay->GetPrimarySelectionDeviceManager();
  if (manager) {
    gtk_primary_selection_device* primaryDataDevice =
        gtk_primary_selection_device_manager_get_device(manager,
                                                        mDisplay->GetSeat());
    gtk_primary_selection_device_add_listener(
        primaryDataDevice, &primary_selection_device_listener, this);
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

GdkAtom* nsRetrievalContextWayland::GetTargets(int32_t aWhichClipboard,
                                               int* aTargetNum) {
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

struct FastTrackClipboard {
  FastTrackClipboard(int aClipboardRequestNumber,
                     nsRetrievalContextWayland* aRetrievalContex)
      : mClipboardRequestNumber(aClipboardRequestNumber),
        mRetrievalContex(aRetrievalContex) {}

  int mClipboardRequestNumber;
  nsRetrievalContextWayland* mRetrievalContex;
};

static void wayland_clipboard_contents_received(
    GtkClipboard* clipboard, GtkSelectionData* selection_data, gpointer data) {
  FastTrackClipboard* fastTrack = static_cast<FastTrackClipboard*>(data);
  fastTrack->mRetrievalContex->TransferFastTrackClipboard(
      fastTrack->mClipboardRequestNumber, selection_data);
  delete fastTrack;
}

void nsRetrievalContextWayland::TransferFastTrackClipboard(
    int aClipboardRequestNumber, GtkSelectionData* aSelectionData) {
  if (mClipboardRequestNumber == aClipboardRequestNumber) {
    int dataLength = gtk_selection_data_get_length(aSelectionData);
    if (dataLength > 0) {
      mClipboardDataLength = dataLength;
      mClipboardData = reinterpret_cast<char*>(
          g_malloc(sizeof(char) * (mClipboardDataLength + 1)));
      memcpy(mClipboardData, gtk_selection_data_get_data(aSelectionData),
             sizeof(char) * mClipboardDataLength);
      mClipboardData[mClipboardDataLength] = '\0';
    }
  } else {
    NS_WARNING("Received obsoleted clipboard data!");
  }
}

const char* nsRetrievalContextWayland::GetClipboardData(
    const char* aMimeType, int32_t aWhichClipboard, uint32_t* aContentLength) {
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
    gtk_clipboard_request_contents(
        gtk_clipboard_get(selection), gdk_atom_intern(aMimeType, FALSE),
        wayland_clipboard_contents_received,
        new FastTrackClipboard(mClipboardRequestNumber, this));
  } else {
    DataOffer* dataOffer =
        (selection == GDK_SELECTION_PRIMARY) ? mPrimaryOffer : mClipboardOffer;
    if (!dataOffer) {
      // Something went wrong. We're requested to provide clipboard data
      // but we haven't got any from wayland.
      NS_WARNING("Requested data without valid DataOffer!");
      mClipboardData = nullptr;
      mClipboardDataLength = 0;
    } else {
      mClipboardData = dataOffer->GetData(mDisplay->GetDisplay(), aMimeType,
                                          &mClipboardDataLength);
    }
  }

  *aContentLength = mClipboardDataLength;
  return reinterpret_cast<const char*>(mClipboardData);
}

const char* nsRetrievalContextWayland::GetClipboardText(
    int32_t aWhichClipboard) {
  GdkAtom selection = GetSelectionAtom(aWhichClipboard);
  DataOffer* dataOffer =
      (selection == GDK_SELECTION_PRIMARY) ? mPrimaryOffer : mClipboardOffer;
  if (!dataOffer) return nullptr;

  for (unsigned int i = 0; i < TEXT_MIME_TYPES_NUM; i++) {
    if (dataOffer->HasTarget(sTextMimeTypes[i])) {
      uint32_t unused;
      return GetClipboardData(sTextMimeTypes[i], aWhichClipboard, &unused);
    }
  }
  return nullptr;
}

void nsRetrievalContextWayland::ReleaseClipboardData(
    const char* aClipboardData) {
  NS_ASSERTION(aClipboardData == mClipboardData,
               "Releasing unknown clipboard data!");
  g_free((void*)aClipboardData);

  mClipboardData = nullptr;
  mClipboardDataLength = 0;
}
