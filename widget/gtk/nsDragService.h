/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"
#include "nsIObserver.h"
#include "nsAutoRef.h"
#include <gtk/gtk.h>

class nsWindow;

#ifndef HAVE_NSGOBJECTREFTRAITS
#define HAVE_NSGOBJECTREFTRAITS
template <class T>
class nsGObjectRefTraits : public nsPointerRefTraits<T> {
public:
    static void Release(T *aPtr) { g_object_unref(aPtr); }
    static void AddRef(T *aPtr) { g_object_ref(aPtr); }
};
#endif

#ifndef HAVE_NSAUTOREFTRAITS_GTKWIDGET
#define HAVE_NSAUTOREFTRAITS_GTKWIDGET
template <>
class nsAutoRefTraits<GtkWidget> : public nsGObjectRefTraits<GtkWidget> { };
#endif

#ifndef HAVE_NSAUTOREFTRAITS_GDKDRAGCONTEXT
#define HAVE_NSAUTOREFTRAITS_GDKDRAGCONTEXT
template <>
class nsAutoRefTraits<GdkDragContext> :
    public nsGObjectRefTraits<GdkDragContext> { };
#endif

/**
 * Native GTK DragService wrapper
 */

class nsDragService : public nsBaseDragService,
                      public nsIObserver
{
public:
    nsDragService();
    virtual ~nsDragService();

    NS_DECL_ISUPPORTS_INHERITED

    NS_DECL_NSIOBSERVER

    // nsIDragService
    NS_IMETHOD InvokeDragSession (nsIDOMNode *aDOMNode,
                                  nsISupportsArray * anArrayTransferables,
                                  nsIScriptableRegion * aRegion,
                                  uint32_t aActionType);
    NS_IMETHOD StartDragSession();
    NS_IMETHOD EndDragSession(bool aDoneDrag);

    // nsIDragSession
    NS_IMETHOD SetCanDrop            (bool             aCanDrop);
    NS_IMETHOD GetCanDrop            (bool            *aCanDrop);
    NS_IMETHOD GetNumDropItems       (uint32_t * aNumItems);
    NS_IMETHOD GetData               (nsITransferable * aTransferable,
                                      uint32_t aItemIndex);
    NS_IMETHOD IsDataFlavorSupported (const char *aDataFlavor, bool *_retval);

    // Methods called from nsWindow to handle responding to GTK drag
    // destination signals

    static nsDragService* GetInstance();

    void TargetDataReceived          (GtkWidget         *aWidget,
                                      GdkDragContext    *aContext,
                                      gint               aX,
                                      gint               aY,
                                      GtkSelectionData  *aSelection_data,
                                      guint              aInfo,
                                      guint32            aTime);

    gboolean ScheduleMotionEvent(nsWindow *aWindow,
                                 GdkDragContext *aDragContext,
                                 nsIntPoint aWindowPoint,
                                 guint aTime);
    void ScheduleLeaveEvent();
    gboolean ScheduleDropEvent(nsWindow *aWindow,
                               GdkDragContext *aDragContext,
                               nsIntPoint aWindowPoint,
                               guint aTime);

    nsWindow* GetMostRecentDestWindow()
    {
        return mScheduledTask == eDragTaskNone ? mTargetWindow
            : mPendingWindow;
    }

    //  END PUBLIC API

    // These methods are public only so that they can be called from functions
    // with C calling conventions.  They are called for drags started with the
    // invisible widget.
    void           SourceEndDragSession(GdkDragContext *aContext,
                                        gint            aResult);
    void           SourceDataGet(GtkWidget        *widget,
                                 GdkDragContext   *context,
                                 GtkSelectionData *selection_data,
                                 guint32           aTime);

    // set the drag icon during drag-begin
    void SetDragIcon(GdkDragContext* aContext);

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
    nsRefPtr<nsWindow> mPendingWindow;
    nsIntPoint mPendingWindowPoint;
    nsCountedRef<GdkDragContext> mPendingDragContext;
    guint mPendingTime;

    // mTargetWindow and mTargetWindowPoint record the position of the last
    // eDragTaskMotion or eDragTaskDrop task that was run or is still running.
    // mTargetWindow is cleared once the drag has completed or left.
    nsRefPtr<nsWindow> mTargetWindow;
    nsIntPoint mTargetWindowPoint;
    // mTargetWidget and mTargetDragContext are set only while dispatching
    // motion or drop events.  mTime records the corresponding timestamp.
    nsCountedRef<GtkWidget> mTargetWidget;
    nsCountedRef<GdkDragContext> mTargetDragContext;
    guint           mTargetTime;

    // is it OK to drop on us?
    bool            mCanDrop;

    // have we received our drag data?
    bool            mTargetDragDataReceived;
    // last data received and its length
    void           *mTargetDragData;
    uint32_t        mTargetDragDataLen;
    // is the current target drag context contain a list?
    bool           IsTargetContextList(void);
    // this will get the native data from the last target given a
    // specific flavor
    void           GetTargetDragData(GdkAtom aFlavor);
    // this will reset all of the target vars
    void           TargetResetData(void);

    // source side vars

    // the source of our drags
    GtkWidget     *mHiddenWidget;
    // our source data items
    nsCOMPtr<nsISupportsArray> mSourceDataItems;

    nsCOMPtr<nsIScriptableRegion> mSourceRegion;

    // get a list of the sources in gtk's format
    GtkTargetList *GetSourceList(void);

    // attempts to create a semi-transparent drag image. Returns TRUE if
    // successful, FALSE if not
    bool SetAlphaPixmap(gfxASurface     *aPixbuf,
                          GdkDragContext  *aContext,
                          int32_t          aXOffset,
                          int32_t          aYOffset,
                          const nsIntRect &dragRect);

    gboolean Schedule(DragTask aTask, nsWindow *aWindow,
                      GdkDragContext *aDragContext,
                      nsIntPoint aWindowPoint, guint aTime);

    // Callback for g_idle_add_full() to run mScheduledTask.
    static gboolean TaskDispatchCallback(gpointer data);
    gboolean RunScheduledTask();
    void UpdateDragAction();
    void DispatchMotionEvents();
    void ReplyToDragMotion();
    gboolean DispatchDropEvent();
};

#endif // nsDragService_h__

