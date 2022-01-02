/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h__
#define nsDragService_h__

#include "mozilla/RefPtr.h"
#include "nsBaseDragService.h"
#include "nsCOMArray.h"
#include "nsIObserver.h"
#include <gtk/gtk.h>
#include "nsITimer.h"

class nsICookieJarSettings;
class nsWindow;

#ifdef MOZ_WAYLAND
class DataOffer;
#else
typedef nsISupports DataOffer;
#endif

namespace mozilla {
namespace gfx {
class SourceSurface;
}
}  // namespace mozilla

/**
 * Native GTK DragService wrapper
 */

class nsDragService final : public nsBaseDragService, public nsIObserver {
 public:
  nsDragService();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER

  // nsBaseDragService
  MOZ_CAN_RUN_SCRIPT virtual nsresult InvokeDragSessionImpl(
      nsIArray* anArrayTransferables,
      const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
      uint32_t aActionType) override;
  // nsIDragService
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD InvokeDragSession(
      nsINode* aDOMNode, nsIPrincipal* aPrincipal,
      nsIContentSecurityPolicy* aCsp, nsICookieJarSettings* aCookieJarSettings,
      nsIArray* anArrayTransferables, uint32_t aActionType,
      nsContentPolicyType aContentPolicyType) override;
  NS_IMETHOD StartDragSession() override;
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD EndDragSession(bool aDoneDrag,
                                               uint32_t aKeyModifiers) override;

  // nsIDragSession
  NS_IMETHOD SetCanDrop(bool aCanDrop) override;
  NS_IMETHOD GetCanDrop(bool* aCanDrop) override;
  NS_IMETHOD GetNumDropItems(uint32_t* aNumItems) override;
  NS_IMETHOD GetData(nsITransferable* aTransferable,
                     uint32_t aItemIndex) override;
  NS_IMETHOD IsDataFlavorSupported(const char* aDataFlavor,
                                   bool* _retval) override;

  // Update Drag&Drop state according child process state.
  // UpdateDragEffect() is called by IPC bridge when child process
  // accepts/denies D&D operation and uses stored
  // mTargetDragContextForRemote context.
  NS_IMETHOD UpdateDragEffect() override;

  // Methods called from nsWindow to handle responding to GTK drag
  // destination signals

  static already_AddRefed<nsDragService> GetInstance();

  void TargetDataReceived(GtkWidget* aWidget, GdkDragContext* aContext, gint aX,
                          gint aY, GtkSelectionData* aSelection_data,
                          guint aInfo, guint32 aTime);

  gboolean ScheduleMotionEvent(nsWindow* aWindow, GdkDragContext* aDragContext,
                               RefPtr<DataOffer> aPendingWaylandDataOffer,
                               mozilla::LayoutDeviceIntPoint aWindowPoint,
                               guint aTime);
  void ScheduleLeaveEvent();
  gboolean ScheduleDropEvent(nsWindow* aWindow, GdkDragContext* aDragContext,
                             RefPtr<DataOffer> aPendingWaylandDataOffer,
                             mozilla::LayoutDeviceIntPoint aWindowPoint,
                             guint aTime);

  nsWindow* GetMostRecentDestWindow() {
    return mScheduledTask == eDragTaskNone ? mTargetWindow : mPendingWindow;
  }

  //  END PUBLIC API

  // These methods are public only so that they can be called from functions
  // with C calling conventions.  They are called for drags started with the
  // invisible widget.
  void SourceEndDragSession(GdkDragContext* aContext, gint aResult);
  void SourceDataGet(GtkWidget* widget, GdkDragContext* context,
                     GtkSelectionData* selection_data, guint32 aTime);

  void SourceBeginDrag(GdkDragContext* aContext);

  // set the drag icon during drag-begin
  void SetDragIcon(GdkDragContext* aContext);

  // Reply to drag_motion event according to recent DragService state.
  // We need that on Wayland to reply immediately as it's requested
  // there (see Bug 1730203).
  void ReplyToDragMotion();

 protected:
  virtual ~nsDragService();

 private:
  // mScheduledTask indicates what signal has been received from GTK and
  // so what needs to be dispatched when the scheduled task is run.  It is
  // eDragTaskNone when there is no task scheduled (but the
  // previous task may still not have finished running).
  enum DragTask {
    eDragTaskNone,
    eDragTaskMotion,
    eDragTaskLeave,
    eDragTaskDrop,
    eDragTaskSourceEnd
  };
  DragTask mScheduledTask;
  // mTaskSource is the GSource id for the task that is either scheduled
  // or currently running.  It is 0 if no task is scheduled or running.
  guint mTaskSource;
  bool mScheduledTaskIsRunning;

  // target/destination side vars
  // These variables keep track of the state of the current drag.

  // mPendingWindow, mPendingWindowPoint, mPendingDragContext, and
  // mPendingTime, carry information from the GTK signal that will be used
  // when the scheduled task is run.  mPendingWindow and mPendingDragContext
  // will be nullptr if the scheduled task is eDragTaskLeave.
  RefPtr<nsWindow> mPendingWindow;
  mozilla::LayoutDeviceIntPoint mPendingWindowPoint;
  RefPtr<GdkDragContext> mPendingDragContext;

  // We cache all data for the current drag context,
  // because waiting for the data in GetTargetDragData can be very slow.
  nsTHashMap<nsCStringHashKey, nsTArray<uint8_t>> mCachedData;
  // mCachedData are tied to mCachedDragContext. mCachedDragContext is not
  // ref counted and may be already deleted on Gtk side.
  // We used it for mCachedData invalidation only and can't be used for
  // any D&D operation.
  uintptr_t mCachedDragContext;

#ifdef MOZ_WAYLAND
  RefPtr<DataOffer> mPendingWaylandDataOffer;
#endif
  guint mPendingTime;

  // mTargetWindow and mTargetWindowPoint record the position of the last
  // eDragTaskMotion or eDragTaskDrop task that was run or is still running.
  // mTargetWindow is cleared once the drag has completed or left.
  RefPtr<nsWindow> mTargetWindow;
  mozilla::LayoutDeviceIntPoint mTargetWindowPoint;
  // mTargetWidget and mTargetDragContext are set only while dispatching
  // motion or drop events.  mTime records the corresponding timestamp.
  RefPtr<GtkWidget> mTargetWidget;
  RefPtr<GdkDragContext> mTargetDragContext;
#ifdef MOZ_WAYLAND
  RefPtr<DataOffer> mTargetWaylandDataOffer;
#endif

  // When we route D'n'D request to child process
  // (by EventStateManager::DispatchCrossProcessEvent)
  // we save GdkDragContext to mTargetDragContextForRemote.
  // When we get a reply from child process we use
  // the stored GdkDragContext to send reply to OS.
  //
  // We need to store GdkDragContext because mTargetDragContext is cleared
  // after every D'n'D event.
  RefPtr<GdkDragContext> mTargetDragContextForRemote;
#ifdef MOZ_WAYLAND
  RefPtr<DataOffer> mTargetWaylandDataOfferForRemote;
#endif
  guint mTargetTime;

  // is it OK to drop on us?
  bool mCanDrop;

  // have we received our drag data?
  bool mTargetDragDataReceived;
  // last data received and its length
  void* mTargetDragData;
  uint32_t mTargetDragDataLen;
  // is the current target drag context contain a list?
  bool IsTargetContextList(void);
  // this will get the native data from the last target given a
  // specific flavor
  void GetTargetDragData(GdkAtom aFlavor, nsTArray<nsCString>& aDropFlavors);
  // this will reset all of the target vars
  void TargetResetData(void);
  // Ensure our data cache belongs to aDragContext and clear the cache if
  // aDragContext is different than mCachedDragContext.
  void EnsureCachedDataValidForContext(GdkDragContext* aDragContext);

  // source side vars

  // the source of our drags
  GtkWidget* mHiddenWidget;
  // our source data items
  nsCOMPtr<nsIArray> mSourceDataItems;

  // get a list of the sources in gtk's format
  GtkTargetList* GetSourceList(void);

  // attempts to create a semi-transparent drag image. Returns TRUE if
  // successful, FALSE if not
  bool SetAlphaPixmap(SourceSurface* aPixbuf, GdkDragContext* aContext,
                      int32_t aXOffset, int32_t aYOffset,
                      const mozilla::LayoutDeviceIntRect& dragRect);

  gboolean Schedule(DragTask aTask, nsWindow* aWindow,
                    GdkDragContext* aDragContext,
                    RefPtr<DataOffer> aPendingWaylandDataOffer,
                    mozilla::LayoutDeviceIntPoint aWindowPoint, guint aTime);

  // Callback for g_idle_add_full() to run mScheduledTask.
  MOZ_CAN_RUN_SCRIPT static gboolean TaskDispatchCallback(gpointer data);
  MOZ_CAN_RUN_SCRIPT gboolean RunScheduledTask();
  void UpdateDragAction();
  MOZ_CAN_RUN_SCRIPT void DispatchMotionEvents();
  void ReplyToDragMotion(GdkDragContext* aDragContext);
#ifdef MOZ_WAYLAND
  void ReplyToDragMotion(RefPtr<DataOffer> aDragContext);
#endif
#ifdef MOZ_LOGGING
  const char* GetDragServiceTaskName(nsDragService::DragTask aTask);
#endif
  void GetDragFlavors(nsTArray<nsCString>& aFlavors);
  gboolean DispatchDropEvent();
  static uint32_t GetCurrentModifiers();

  nsresult CreateTempFile(nsITransferable* aItem,
                          GtkSelectionData* aSelectionData);
  bool RemoveTempFiles();
  static gboolean TaskRemoveTempFiles(gpointer data);

  // the url of the temporary file that has been created in the current drag
  // session
  nsCString mTempFileUrl;
  // stores all temporary files
  nsCOMArray<nsIFile> mTemporaryFiles;
  // timer to trigger deletion of temporary files
  guint mTempFileTimerID;
};

#endif  // nsDragService_h__
