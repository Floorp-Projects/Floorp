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
#include "nsWindow.h"
#include "mozilla/ScopeExit.h"
#include "nsThreadUtils.h"

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

static inline GdkDragAction wl_to_gdk_actions(uint32_t dnd_actions) {
  GdkDragAction actions = GdkDragAction(0);

  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    actions = GdkDragAction(actions | GDK_ACTION_COPY);
  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    actions = GdkDragAction(actions | GDK_ACTION_MOVE);

  return actions;
}

static inline uint32_t gdk_to_wl_actions(GdkDragAction action) {
  uint32_t dnd_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

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
                                      uint32_t source_actions) {
  auto* dragContext = static_cast<DataOffer*>(data);
  dragContext->SetAvailableDragActions(source_actions);
}

/* Advertise recently selected drag and drop action by compositor, based
 * on source actions and user choice (key modifiers, etc.).
 */
static void data_offer_action(void* data, struct wl_data_offer* wl_data_offer,
                              uint32_t dnd_action) {
  auto* dropContext = static_cast<DataOffer*>(data);
  dropContext->SetSelectedDragAction(dnd_action);

  if (dropContext->GetWidget()) {
    uint32_t time;
    nscoord x, y;
    dropContext->GetLastDropInfo(&time, &x, &y);
    WindowDragMotionHandler(dropContext->GetWidget(), nullptr, dropContext, x,
                            y, time);
  }
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

DataOffer::DataOffer(wl_data_offer* aDataOffer)
    : mWaylandDataOffer(aDataOffer),
      mMutex("DataOffer"),
      mAsyncContentLength(),
      mAsyncContentData(),
      mGetterFinished(),
      mSelectedDragAction(),
      mAvailableDragActions(),
      mTime(),
      mGtkWidget(),
      mX(),
      mY() {
  if (mWaylandDataOffer) {
    wl_data_offer_add_listener(
        mWaylandDataOffer, (struct wl_data_offer_listener*)&data_offer_listener,
        this);
  }
}

DataOffer::~DataOffer() {
  g_clear_pointer(&mWaylandDataOffer, wl_data_offer_destroy);

  // Async transfer was finished after time limit. In such case release the
  // clipboard data.
  if (mGetterFinished && mAsyncContentLength && mAsyncContentData) {
    g_free((void*)mAsyncContentData);
  }
}

bool DataOffer::RequestDataTransfer(const char* aMimeType, int fd) {
  LOGCLIP(("DataOffer::RequestDataTransfer MIME %s FD %d\n", aMimeType, fd));
  if (mWaylandDataOffer) {
    wl_data_offer_receive(mWaylandDataOffer, aMimeType, fd);
    return true;
  }

  return false;
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
    if (mTargetMIMETypes[j] == gdk_atom_intern(aMimeType, FALSE)) {
      LOGCLIP(("DataOffer::HasTarget() we have mime %s\n", aMimeType));
      return true;
    }
  }
  LOGCLIP(("DataOffer::HasTarget() missing mime %s\n", aMimeType));
  return false;
}

static bool MakeFdNonBlocking(int fd) {
  return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) != -1;
}

char* DataOffer::GetDataInternal(const char* aMimeType,
                                 uint32_t* aContentLength) {
  LOGCLIP(("GetDataInternal() mime %s\n", aMimeType));

  int pipe_fd[2];
  if (pipe(pipe_fd) == -1) {
    return nullptr;
  }

  GIOChannel* channel = nullptr;

  auto free = mozilla::MakeScopeExit([&] {
    if (channel) {
      g_io_channel_unref(channel);
    }
    if (pipe_fd[0] >= 0) {
      close(pipe_fd[0]);
    }
    if (pipe_fd[1] >= 0) {
      close(pipe_fd[1]);
    }
  });

  if (!MakeFdNonBlocking(pipe_fd[0]) || !MakeFdNonBlocking(pipe_fd[1])) {
    return nullptr;
  }

  if (!RequestDataTransfer(aMimeType, pipe_fd[1])) {
    NS_WARNING("DataOffer::RequestDataTransfer() failed!");
    return nullptr;
  }

  close(pipe_fd[1]);
  pipe_fd[1] = -1;

  // Flush wl_display connection to get clipboard data uploaded from display to
  // our pipe.
  wl_display_flush(WaylandDisplayGet()->GetDisplay());

  channel = g_io_channel_unix_new(pipe_fd[0]);
  GError* error = nullptr;
  char* clipboardData = nullptr;

  GIOStatus ret;
  ret = g_io_channel_set_encoding(channel, nullptr, &error);
  g_clear_pointer(&error, g_error_free);
  if (ret != G_IO_STATUS_NORMAL) {
    NS_WARNING("g_io_channel_set_encoding failed!");
    return nullptr;
  }

  const PRTime entryTime = PR_Now();
  gsize len;
  while (1) {
    LOGCLIP(("reading data...\n"));
    ret = g_io_channel_read_to_end(channel, &clipboardData, &len, &error);
    if (ret == G_IO_STATUS_NORMAL) {
      break;
    }
    if (ret == G_IO_STATUS_AGAIN) {
      wl_display_flush(WaylandDisplayGet()->GetDisplay());
      // check the number of iterations
      PR_Sleep(20 * PR_TicksPerSecond() / 1000); /* sleep for 20 ms/iteration */
      if (PR_Now() - entryTime > kClipboardTimeout) {
        break;
      }
    } else {  // G_IO_STATUS_ERROR
      if (error) {
        NS_WARNING(
            nsPrintfCString("Unexpected error when reading clipboard data: %s",
                            error->message)
                .get());
        g_error_free(error);
      }
      return nullptr;
    }
  }
  *aContentLength = len;

  if (*aContentLength == 0) {
    // We don't have valid clipboard data although
    // g_io_channel_read_to_end() allocated clipboardData for us.
    // Release it now and return nullptr to indicate
    // we don't have reqested data flavour.
    g_free((void*)clipboardData);
    clipboardData = nullptr;
  }

  LOGCLIP(("  Got clipboard data length %d\n", *aContentLength));
  return clipboardData;
}

void DataOffer::GetDataAsyncInternal(const char* aMimeType) {
  mAsyncContentData = GetDataInternal(aMimeType, &mAsyncContentLength);
  mGetterFinished = true;
}

char* DataOffer::GetData(const char* aMimeType, uint32_t* aContentLength) {
  LOGCLIP(("DataOffer::GetData() mime %s\n", aMimeType));

  if (!HasTarget(aMimeType)) {
    LOGCLIP(("  Failed: DataOffer does not contain %s MIME!\n", aMimeType));
    return nullptr;
  }

  return GetDataInternal(aMimeType, aContentLength);
}

char* DataOffer::GetDataAsync(const char* aMimeType, uint32_t* aContentLength) {
  LOGCLIP(("DataOffer::GetDataAsync() mime %s\n", aMimeType));

  if (!HasTarget(aMimeType)) {
    LOGCLIP(("  Failed: DataOffer does not contain %s MIME!\n", aMimeType));
    return nullptr;
  }

  if (!mMutex.TryLock()) {
    LOGCLIP(("  Failed: DataOffer is already used!\n"));
    return nullptr;
  }
  auto unlock = mozilla::MakeScopeExit([&] {
    // If async getter was sucessful transfer ownership of the clipboard data
    // to caller.
    if (mGetterFinished) {
      mAsyncContentLength = 0;
      mAsyncContentData = nullptr;
    } else {
      // Remove offers for failed transfers
      LOGCLIP(("  data offer was not finished in time, clearing\n"));
      g_clear_pointer(&mWaylandDataOffer, wl_data_offer_destroy);
    }
    mMutex.Unlock();
  });

  mAsyncContentLength = 0;
  mAsyncContentData = nullptr;
  mGetterFinished = false;

  RefPtr<DataOffer> offer(this);
  NS_DispatchBackgroundTask(
      NS_NewRunnableFunction("DataOffer::GetDataInternal",
                             [offer, aMimeType]() -> void {
                               offer->GetDataAsyncInternal(aMimeType);
                             }),
      nsIEventTarget::NS_DISPATCH_NORMAL);

  PRTime entryTime = PR_Now();
  while (!mGetterFinished) {
    // check the number of iterations
    LOGCLIP(("doing iteration...\n"));
    PR_Sleep(20 * PR_TicksPerSecond() / 1000); /* sleep for 20 ms/iteration */
    if (PR_Now() - entryTime > kClipboardTimeout) {
      break;
    }
    gtk_main_iteration();
  }

  if (!mGetterFinished) {
    LOGCLIP(("  failed to get async clipboard data in time limit\n"));
    *aContentLength = 0;
    return nullptr;
  }

  LOGCLIP(("  ineration over, got data %p len %d\n", mAsyncContentData,
           mAsyncContentLength));
  *aContentLength = mAsyncContentLength;
  return mAsyncContentData;
}

void DataOffer::DragOfferAccept(const char* aMimeType) {
  LOGDRAG(("DataOffer::DragOfferAccept MIME %s mTime %d\n", aMimeType, mTime));
  if (!HasTarget(aMimeType)) {
    LOGCLIP(("  DataOffer: DataOffer does not contain %s MIME!\n", aMimeType));
    return;
  }
  wl_data_offer_accept(mWaylandDataOffer, mTime, aMimeType);
}

/* We follow logic of gdk_wayland_drag_context_commit_status()/gdkdnd-wayland.c
 * here.
 */
void DataOffer::SetDragStatus(GdkDragAction aPreferredAction) {
  uint32_t preferredAction = gdk_to_wl_actions(aPreferredAction);
  uint32_t allActions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

  LOGDRAG(("DataOffer::SetDragStatus aPreferredAction %d\n", aPreferredAction));

  /* We only don't choose a preferred action if we don't accept any.
   * If we do accept any, it is currently alway copy and move
   */
  if (preferredAction != WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE) {
    allActions = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
                 WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
  }

  wl_data_offer_set_actions(mWaylandDataOffer, allActions, preferredAction);

  /* Workaround Wayland D&D architecture here. To get the data_device_drop()
     signal (which routes to nsDragService::GetData() call) we need to
     accept at least one mime type before data_device_leave().

     Real wl_data_offer_accept() for actualy requested data mime type is
     called from nsDragService::GetData().
  */
  if (mTargetMIMETypes[0]) {
    wl_data_offer_accept(mWaylandDataOffer, mTime,
                         gdk_atom_name(mTargetMIMETypes[0]));
  }
}

void DataOffer::SetSelectedDragAction(uint32_t aWaylandAction) {
  mSelectedDragAction = aWaylandAction;
}

GdkDragAction DataOffer::GetSelectedDragAction() {
  return wl_to_gdk_actions(mSelectedDragAction);
}

void DataOffer::SetAvailableDragActions(uint32_t aWaylandActions) {
  mAvailableDragActions = aWaylandActions;
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
    : DataOffer(nullptr),
      mPrimaryDataOfferGtk(aPrimaryDataOffer),
      mPrimaryDataOfferZwpV1(nullptr) {
  gtk_primary_selection_offer_add_listener(
      aPrimaryDataOffer, &primary_selection_offer_listener_gtk, this);
}

PrimaryDataOffer::PrimaryDataOffer(
    zwp_primary_selection_offer_v1* aPrimaryDataOffer)
    : DataOffer(nullptr),
      mPrimaryDataOfferGtk(nullptr),
      mPrimaryDataOfferZwpV1(aPrimaryDataOffer) {
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

void DataOffer::DropDataEnter(GtkWidget* aGtkWidget, uint32_t aTime, nscoord aX,
                              nscoord aY) {
  mTime = aTime;
  mGtkWidget = aGtkWidget;
  mX = aX;
  mY = aY;
}

void DataOffer::DropMotion(uint32_t aTime, nscoord aX, nscoord aY) {
  mTime = aTime;
  mX = aX;
  mY = aY;
}

void DataOffer::GetLastDropInfo(uint32_t* aTime, nscoord* aX, nscoord* aY) {
  *aTime = mTime;
  *aX = mX;
  *aY = mY;
}

GdkDragAction DataOffer::GetAvailableDragActions() {
  GdkDragAction gdkAction = GetSelectedDragAction();

  // We emulate gdk_drag_context_get_actions() here.
  if (!gdkAction) {
    gdkAction = wl_to_gdk_actions(mAvailableDragActions);
  }

  return gdkAction;
}

GList* DataOffer::GetDragTargets() {
  int targetNums;
  GdkAtom* atoms = GetTargets(&targetNums);

  GList* targetList = nullptr;
  for (int i = 0; i < targetNums; i++) {
    targetList = g_list_append(targetList, GDK_ATOM_TO_POINTER(atoms[i]));
  }

  return targetList;
}

char* DataOffer::GetDragData(const char* aMimeType, uint32_t* aContentLength) {
  LOGDRAG(("DataOffer::GetData %s\n", aMimeType));
  if (!HasTarget(aMimeType)) {
    return nullptr;
  }
  DragOfferAccept(aMimeType);
  return GetDataAsync(aMimeType, aContentLength);
}

RefPtr<DataOffer> nsRetrievalContextWayland::FindActiveOffer(
    wl_data_offer* aDataOffer, bool aRemove) {
  const int len = mActiveOffers.Length();
  for (int i = 0; i < len; i++) {
    if (mActiveOffers[i] && mActiveOffers[i]->MatchesOffer(aDataOffer)) {
      RefPtr<DataOffer> ret = mActiveOffers[i];
      if (aRemove) {
        mActiveOffers[i] = nullptr;
      }
      return ret;
    }
  }
  return nullptr;
}

void nsRetrievalContextWayland::InsertOffer(RefPtr<DataOffer> aDataOffer) {
  const int len = mActiveOffers.Length();
  for (int i = 0; i < len; i++) {
    if (!mActiveOffers[i]) {
      mActiveOffers[i] = aDataOffer;
      return;
    }
  }
  mActiveOffers.AppendElement(aDataOffer);
}

void nsRetrievalContextWayland::RegisterNewDataOffer(
    wl_data_offer* aDataOffer) {
  LOGCLIP(
      ("nsRetrievalContextWayland::RegisterNewDataOffer (wl_data_offer) %p\n",
       aDataOffer));

  if (FindActiveOffer(aDataOffer)) {
    LOGCLIP(("  offer already exists, protocol error?\n"));
    return;
  }

  InsertOffer(new DataOffer(aDataOffer));
}

void nsRetrievalContextWayland::RegisterNewDataOffer(
    gtk_primary_selection_offer* aPrimaryDataOffer) {
  LOGCLIP(("nsRetrievalContextWayland::RegisterNewDataOffer (primary) %p\n",
           aPrimaryDataOffer));

  if (FindActiveOffer((wl_data_offer*)aPrimaryDataOffer)) {
    LOGCLIP(("  offer already exists, protocol error?\n"));
    return;
  }

  InsertOffer(new PrimaryDataOffer(aPrimaryDataOffer));
}

void nsRetrievalContextWayland::RegisterNewDataOffer(
    zwp_primary_selection_offer_v1* aPrimaryDataOffer) {
  LOGCLIP(("nsRetrievalContextWayland::RegisterNewDataOffer (primary ZWP) %p\n",
           aPrimaryDataOffer));

  if (FindActiveOffer((wl_data_offer*)aPrimaryDataOffer)) {
    LOGCLIP(("  offer already exists, protocol error?\n"));
    return;
  }

  InsertOffer(new PrimaryDataOffer(aPrimaryDataOffer));
}

void nsRetrievalContextWayland::SetClipboardDataOffer(
    wl_data_offer* aDataOffer) {
  LOGCLIP(
      ("nsRetrievalContextWayland::SetClipboardDataOffer (wl_data_offer) %p\n",
       aDataOffer));

  // Delete existing clipboard data offer
  mClipboardOffer = nullptr;

  // null aDataOffer indicates that our clipboard content
  // is no longer valid and should be release.
  if (aDataOffer) {
    mClipboardOffer = FindActiveOffer(aDataOffer, /* remove */ true);
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
    mPrimaryOffer =
        FindActiveOffer((wl_data_offer*)aPrimaryDataOffer, /* remove */ true);
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
    mPrimaryOffer =
        FindActiveOffer((wl_data_offer*)aPrimaryDataOffer, /* remove */ true);
  }
}

void nsRetrievalContextWayland::AddDragAndDropDataOffer(
    wl_data_offer* aDropDataOffer) {
  LOGDRAG(("nsRetrievalContextWayland::AddDragAndDropDataOffer %p\n",
           aDropDataOffer));

  // Remove any existing D&D contexts.
  mDragContext = nullptr;
  if (aDropDataOffer) {
    mDragContext = FindActiveOffer(aDropDataOffer, /* remove */ true);
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

// The new fresh wayland data content is drag and drop.
static void data_device_enter(void* data, struct wl_data_device* data_device,
                              uint32_t time, struct wl_surface* surface,
                              int32_t x_fixed, int32_t y_fixed,
                              struct wl_data_offer* offer) {
  LOGDRAG(("nsWindow data_device_enter"));
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->AddDragAndDropDataOffer(offer);

  RefPtr<DataOffer> dragContext = context->GetDragContext();
  if (dragContext) {
    GtkWidget* gtkWidget = get_gtk_widget_for_wl_surface(surface);
    if (!gtkWidget) {
      NS_WARNING("DragAndDrop: Unable to get GtkWidget for wl_surface!");
      return;
    }

    LOGDRAG(
        ("nsWindow data_device_enter for GtkWidget %p\n", (void*)gtkWidget));
    dragContext->DropDataEnter(gtkWidget, time, wl_fixed_to_int(x_fixed),
                               wl_fixed_to_int(y_fixed));
  }
}

static void data_device_leave(void* data, struct wl_data_device* data_device) {
  LOGDRAG(("nsWindow data_device_leave"));
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);

  RefPtr<DataOffer> dropContext = context->GetDragContext();
  if (dropContext) {
    WindowDragLeaveHandler(dropContext->GetWidget());

    LOGDRAG(("nsWindow data_device_leave for GtkWidget %p\n",
             (void*)dropContext->GetWidget()));
    context->ClearDragAndDropDataOffer();
  }
}

static void data_device_motion(void* data, struct wl_data_device* data_device,
                               uint32_t time, int32_t x_fixed,
                               int32_t y_fixed) {
  LOGDRAG(("nsWindow data_device_motion"));
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);

  RefPtr<DataOffer> dropContext = context->GetDragContext();
  if (dropContext) {
    nscoord x = wl_fixed_to_int(x_fixed);
    nscoord y = wl_fixed_to_int(y_fixed);
    dropContext->DropMotion(time, x, y);

    LOGDRAG(("nsWindow data_device_motion for GtkWidget %p\n",
             (void*)dropContext->GetWidget()));
    WindowDragMotionHandler(dropContext->GetWidget(), nullptr, dropContext, x,
                            y, time);
  }
}

static void data_device_drop(void* data, struct wl_data_device* data_device) {
  LOGDRAG(("nsWindow data_device_drop"));
  nsRetrievalContextWayland* context =
      static_cast<nsRetrievalContextWayland*>(data);

  RefPtr<DataOffer> dropContext = context->GetDragContext();
  if (dropContext) {
    uint32_t time;
    nscoord x, y;
    dropContext->GetLastDropInfo(&time, &x, &y);

    LOGDRAG(("nsWindow data_device_drop GtkWidget %p\n",
             (void*)dropContext->GetWidget()));
    WindowDragDropHandler(dropContext->GetWidget(), nullptr, dropContext, x, y,
                          time);
  }
}

/* wl_data_device callback description:
 *
 * data_device_data_offer - It's called when there's a new wl_data_offer
 *                          available. We need to attach wl_data_offer_listener
 *                          to it to get available MIME types.
 *
 * data_device_selection - It's called when the new wl_data_offer
 *                         is a clipboard content.
 * data_device_enter - It's called when the new wl_data_offer is a drag & drop
 *                     content and it's tied to actual wl_surface.
 *
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

void nsRetrievalContextWayland::ClearDragAndDropDataOffer(void) {
  LOGDRAG(("nsRetrievalContextWayland::ClearDragAndDropDataOffer()\n"));
  mDragContext = nullptr;
}

nsRetrievalContextWayland::nsRetrievalContextWayland(void)
    : mDisplay(WaylandDisplayGet()),
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
}

nsRetrievalContextWayland::~nsRetrievalContextWayland(void) {}

struct FastTrackClipboard {
  FastTrackClipboard(ClipboardDataType aDataType, int aClipboardRequestNumber,
                     RefPtr<nsRetrievalContextWayland> aRetrievalContex)
      : mClipboardRequestNumber(aClipboardRequestNumber),
        mRetrievalContex(std::move(aRetrievalContex)),
        mDataType(aDataType) {}
  int mClipboardRequestNumber;
  RefPtr<nsRetrievalContextWayland> mRetrievalContex;
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
    RefPtr<DataOffer> dataOffer =
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
      mClipboardData = dataOffer->GetData(aMimeType, &mClipboardDataLength);
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
