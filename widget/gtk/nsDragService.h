/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h__
#define nsDragService_h__

#include "mozilla/RefPtr.h"
#include "nsBaseDragService.h"
#include "nsIObserver.h"
#include "nsAutoRef.h"
#include <gtk/gtk.h>

class nsWindow;
class nsWaylandDragContext;

namespace mozilla {
namespace gfx {
class SourceSurface;
}
}  // namespace mozilla

#ifndef HAVE_NSGOBJECTREFTRAITS
#  define HAVE_NSGOBJECTREFTRAITS
template <class T>
class nsGObjectRefTraits : public nsPointerRefTraits<T> {
 public:
  static void Release(T* aPtr) { g_object_unref(aPtr); }
  static void AddRef(T* aPtr) { g_object_ref(aPtr); }
};
#endif

#ifndef HAVE_NSAUTOREFTRAITS_GTKWIDGET
#  define HAVE_NSAUTOREFTRAITS_GTKWIDGET
template <>
class nsAutoRefTraits<GtkWidget> : public nsGObjectRefTraits<GtkWidget> {};
#endif

#ifndef HAVE_NSAUTOREFTRAITS_GDKDRAGCONTEXT
#  define HAVE_NSAUTOREFTRAITS_GDKDRAGCONTEXT
template <>
class nsAutoRefTraits<GdkDragContext>
    : public nsGObjectRefTraits<GdkDragContext> {};
#endif

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
      nsIContentSecurityPolicy* aCsp, nsIArray* anArrayTransferables,
      uint32_t aActionType, nsContentPolicyType aContentPolicyType) override;
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

  NS_IMETHOD UpdateDragEffect() override;

  // Methods called from nsWindow to handle responding to GTK drag
  // destination signals

  static already_AddRefed<nsDragService> GetInstance();

  void TargetDataReceived(GtkWidget* aWidget, GdkDragContext* aContext, gint aX,
                          gint aY, GtkSelectionData* aSelection_data,
                          guint aInfo, guint32 aTime);

  gboolean ScheduleMotionEvent(nsWindow* aWindow, GdkDragContext* aDragContext,
                               nsWaylandDragContext* aPendingWaylandDragContext,
                               mozilla::LayoutDeviceIntPoint aWindowPoint,
                               guint aTime);
  void ScheduleLeaveEvent();
  gboolean ScheduleDropEvent(nsWindow* aWindow, GdkDragContext* aDragContext,
                             nsWaylandDragContext* aPendingWaylandDragContext,
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

  // target/destination side vars
  // These variables keep track of the state of the current drag.

  // mPendingWindow, mPendingWindowPoint, mPendingDragContext, and
  // mPendingTime, carry information from the GTK signal that will be used
  // when the scheduled task is run.  mPendingWindow and mPendingDragContext
  // will be nullptr if the scheduled task is eDragTaskLeave.
  RefPtr<nsWindow> mPendingWindow;
  mozilla::LayoutDeviceIntPoint mPendingWindowPoint;
  nsCountedRef<GdkDragContext> mPendingDragContext;
#ifdef MOZ_WAYLAND
  RefPtr<nsWaylandDragContext> mPendingWaylandDragContext;
#endif
  guint mPendingTime;

  // mTargetWindow and mTargetWindowPoint record the position of the last
  // eDragTaskMotion or eDragTaskDrop task that was run or is still running.
  // mTargetWindow is cleared once the drag has completed or left.
  RefPtr<nsWindow> mTargetWindow;
  mozilla::LayoutDeviceIntPoint mTargetWindowPoint;
  // mTargetWidget and mTargetDragContext are set only while dispatching
  // motion or drop events.  mTime records the corresponding timestamp.
  nsCountedRef<GtkWidget> mTargetWidget;
  nsCountedRef<GdkDragContext> mTargetDragContext;
#ifdef MOZ_WAYLAND
  RefPtr<nsWaylandDragContext> mTargetWaylandDragContext;
#endif
  // mTargetDragContextForRemote is set while waiting for a reply from
  // a child process.
  nsCountedRef<GdkDragContext> mTargetDragContextForRemote;
#ifdef MOZ_WAYLAND
  RefPtr<nsWaylandDragContext> mTargetWaylandDragContextForRemote;
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
  void GetTargetDragData(GdkAtom aFlavor);
  // this will reset all of the target vars
  void TargetResetData(void);

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
                    nsWaylandDragContext* aPendingWaylandDragContext,
                    mozilla::LayoutDeviceIntPoint aWindowPoint, guint aTime);

  // Callback for g_idle_add_full() to run mScheduledTask.
  MOZ_CAN_RUN_SCRIPT static gboolean TaskDispatchCallback(gpointer data);
  MOZ_CAN_RUN_SCRIPT gboolean RunScheduledTask();
  void UpdateDragAction();
  MOZ_CAN_RUN_SCRIPT void DispatchMotionEvents();
  void ReplyToDragMotion(GdkDragContext* aDragContext);
#ifdef MOZ_WAYLAND
  void ReplyToDragMotion(nsWaylandDragContext* aDragContext);
#endif
  gboolean DispatchDropEvent();
  static uint32_t GetCurrentModifiers();
};

#endif  // nsDragService_h__
