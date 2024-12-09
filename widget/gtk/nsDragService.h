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
#include "GUniquePtr.h"

class nsICookieJarSettings;
class nsWindow;

namespace mozilla {
namespace gfx {
class SourceSurface;
}
}  // namespace mozilla
class DragData final {
 public:
  NS_INLINE_DECL_REFCOUNTING(DragData)

  explicit DragData(GdkAtom aDataFlavor, const void* aData, uint32_t aDataLen)
      : mDataFlavor(aDataFlavor),
        mDragDataLen(aDataLen),
        mDragData(moz_xmemdup(aData, aDataLen)) {
    // kURLMime (text/x-moz-url) is received as UTF16 raw data as
    // Gtk doesn't recognize it as URI format. We need to flip it to URI
    // format.
    if (IsURIFlavor()) {
      ConvertToMozURIList();
    }
  }
  explicit DragData(GdkAtom aDataFlavor, gchar** aDragUris);

  GdkAtom GetFlavor() const { return mDataFlavor; }

  // Try to convert text/uri-list or _NETSCAPE_URL MIME to x-moz-url MIME type
  // which is used internally.
  RefPtr<DragData> ConvertToMozURL() const;

  // Try to convert text/uri-list MIME to application/x-moz-file MIME type.
  RefPtr<DragData> ConvertToFile() const;

  bool Export(nsITransferable* aTransferable, uint32_t aItemIndex);

  bool IsImageFlavor() const;
  bool IsFileFlavor() const;
  bool IsTextFlavor() const;
  bool IsURIFlavor() const;

  int GetURIsNum() const;

  bool IsDataValid() const;

#ifdef MOZ_LOGGING
  void Print() const;
#endif

 private:
  explicit DragData(GdkAtom aDataFlavor) : mDataFlavor(aDataFlavor) {}
  ~DragData() = default;

  void ConvertToMozURIList();

  GdkAtom mDataFlavor = nullptr;

  bool mAsURIData = false;

  // In a rare case we export
  bool mDragDataDOMEndings = false;

  // Data obtained from Gtk
  uint32_t mDragDataLen = 0;
  mozilla::UniqueFreePtr<void> mDragData;
  mozilla::GUniquePtr<gchar*> mDragUris;

  // Data which can be passed to transferable. In some cases we can use Gtk data
  // directly but in most cases we need to do UTF8/UTF16 conversion
  // and perform line break;
  nsString mData;
  nsTArray<nsString> mUris;
};

/**
 * GTK native nsIDragSession implementation
 */
class nsDragSession : public nsBaseDragSession, public nsIObserver {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER

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

  MOZ_CAN_RUN_SCRIPT nsresult
  EndDragSessionImpl(bool aDoneDrag, uint32_t aKeyModifiers) override;

  class AutoEventLoop {
    RefPtr<nsDragSession> mSession;

   public:
    explicit AutoEventLoop(RefPtr<nsDragSession> aSession)
        : mSession(std::move(aSession)) {
      nsDragSession::sEventLoopDepth++;
    }
    ~AutoEventLoop() { nsDragSession::sEventLoopDepth--; }
  };

  static int GetLoopDepth() { return sEventLoopDepth; };

 protected:
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

  void ReplyToDragMotion(GdkDragContext* aDragContext, guint aTime);
  void ReplyToDragMotion();

  void GetDragFlavors(nsTArray<nsCString>& aFlavors);
  // this will get the native data from the last target given a
  // specific flavor
  void GetTargetDragData(GdkAtom aFlavor, nsTArray<nsCString>& aDropFlavors,
                         bool aResetTargetData = true);
  // this will reset all of the target vars
  void TargetResetData(void);

  // is the current target drag context contain a list?
  bool IsTargetContextList(void);

  // Ensure our data cache belongs to aDragContext and clear the cache if
  // aDragContext is different than mCachedDragContext.
  void EnsureCachedDataValidForContext(GdkDragContext* aDragContext);

  static gboolean TaskRemoveTempFiles(gpointer data);

  bool RemoveTempFiles();

  // Where the drag begins. We need to keep it open on Wayland.
  RefPtr<nsWindow> mSourceWindow;

  // mTargetWindow and mTargetWindowPoint record the position of the last
  // eDragTaskMotion or eDragTaskDrop task that was run or is still running.
  // mTargetWindow is cleared once the drag has completed or left.
  RefPtr<nsWindow> mTargetWindow;

  // our source data items
  nsCOMPtr<nsIArray> mSourceDataItems;

  // mTargetWidget and mTargetDragContext are set only while dispatching
  // motion or drop events.  mTime records the corresponding timestamp.
  RefPtr<GtkWidget> mTargetWidget;
  RefPtr<GdkDragContext> mTargetDragContext;

  // last data received and its length
  void* mTargetDragData = nullptr;
  uint32_t mTargetDragDataLen = 0;

  // have we received our drag data?
  bool mTargetDragDataReceived = false;

  // When we route D'n'D request to child process
  // (by EventStateManager::DispatchCrossProcessEvent)
  // we save GdkDragContext to mTargetDragContextForRemote.
  // When we get a reply from child process we use
  // the stored GdkDragContext to send reply to OS.
  //
  // We need to store GdkDragContext because mTargetDragContext is cleared
  // after every D'n'D event.
  RefPtr<GdkDragContext> mTargetDragContextForRemote;
  guint mTargetTime;

  mozilla::GUniquePtr<gchar*> mTargetDragUris = nullptr;

  nsTHashMap<nsCStringHashKey, mozilla::GUniquePtr<gchar*>> mCachedUris;

  // We cache all data for the current drag context,
  // because waiting for the data in GetTargetDragData can be very slow.
  nsTHashMap<nsCStringHashKey, nsTArray<uint8_t>> mCachedData;

  DragTask mScheduledTask = eDragTaskNone;
  bool mScheduledTaskIsRunning = false;

  // mPendingWindow, mPendingWindowPoint, mPendingDragContext, and
  // mPendingTime, carry information from the GTK signal that will be used
  // when the scheduled task is run.  mPendingWindow and mPendingDragContext
  // will be nullptr if the scheduled task is eDragTaskLeave.
  RefPtr<nsWindow> mPendingWindow;
  mozilla::LayoutDeviceIntPoint mPendingWindowPoint;
  RefPtr<GdkDragContext> mPendingDragContext;

  guint mPendingTime;

  // mTaskSource is the GSource id for the task that is either scheduled
  // or currently running.  It is 0 if no task is scheduled or running.
  guint mTaskSource = 0;

  // stores all temporary files
  nsCOMArray<nsIFile> mTemporaryFiles;
  // timer to trigger deletion of temporary files
  guint mTempFileTimerID;
  // the url of the temporary file that has been created in the current drag
  // session
  nsTArray<nsCString> mTempFileUrls;

  // mCachedData are tied to mCachedDragContext. mCachedDragContext is not
  // ref counted and may be already deleted on Gtk side.
  // We used it for mCachedData invalidation only and can't be used for
  // any D&D operation.
  uintptr_t mCachedDragContext = 0;

  // How deep we're nested in event loops
  static int sEventLoopDepth;

  // is it OK to drop on us?
  bool mCanDrop = false;

 public:
  static GdkAtom sJPEGImageMimeAtom;
  static GdkAtom sJPGImageMimeAtom;
  static GdkAtom sPNGImageMimeAtom;
  static GdkAtom sGIFImageMimeAtom;
  static GdkAtom sCustomTypesMimeAtom;
  static GdkAtom sURLMimeAtom;
  static GdkAtom sRTFMimeAtom;
  static GdkAtom sTextMimeAtom;
  static GdkAtom sMozUrlTypeAtom;
  static GdkAtom sMimeListTypeAtom;
  static GdkAtom sTextUriListTypeAtom;
  static GdkAtom sTextPlainUTF8TypeAtom;
  static GdkAtom sXdndDirectSaveTypeAtom;
  static GdkAtom sTabDropTypeAtom;
  static GdkAtom sFileMimeAtom;
  static GdkAtom sPortalFileAtom;
  static GdkAtom sPortalFileTransferAtom;
  static GdkAtom sFilePromiseURLMimeAtom;
  static GdkAtom sFilePromiseMimeAtom;
  static GdkAtom sNativeImageMimeAtom;

  nsDragSession();

  // nsBaseDragSession
  MOZ_CAN_RUN_SCRIPT virtual nsresult InvokeDragSessionImpl(
      nsIWidget* aWidget, nsIArray* anArrayTransferables,
      const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
      uint32_t aActionType) override;

  // nsIDragSession
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD InvokeDragSession(
      nsIWidget* aWidget, nsINode* aDOMNode, nsIPrincipal* aPrincipal,
      nsIContentSecurityPolicy* aCsp, nsICookieJarSettings* aCookieJarSettings,
      nsIArray* anArrayTransferables, uint32_t aActionType,
      nsContentPolicyType aContentPolicyType) override;

  // Methods called from nsWindow to handle responding to GTK drag
  // destination signals

  void TargetDataReceived(GtkWidget* aWidget, GdkDragContext* aContext, gint aX,
                          gint aY, GtkSelectionData* aSelection_data,
                          guint aInfo, guint32 aTime);

  gboolean ScheduleMotionEvent(nsWindow* aWindow, GdkDragContext* aDragContext,
                               mozilla::LayoutDeviceIntPoint aWindowPoint,
                               guint aTime);
  void ScheduleLeaveEvent();
  gboolean ScheduleDropEvent(nsWindow* aWindow, GdkDragContext* aDragContext,
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
  bool SourceDataGetText(nsITransferable* aItem, const nsACString& aMIMEType,
                         bool aNeedToDoConversionToPlainText,
                         GtkSelectionData* aSelectionData);
  void SourceDataGetImage(nsITransferable* aItem,
                          GtkSelectionData* aSelectionData);
  void SourceDataGetXDND(nsITransferable* aItem, GdkDragContext* aContext,
                         GtkSelectionData* aSelectionData);
  void SourceDataGetUriList(GdkDragContext* aContext,
                            GtkSelectionData* aSelectionData,
                            uint32_t aDragItems);
  bool SourceDataAppendURLFileItem(nsACString& aURI, nsITransferable* aItem);
  bool SourceDataAppendURLItem(nsITransferable* aItem, bool aExternalDrop,
                               nsACString& aURI);

  void SourceBeginDrag(GdkDragContext* aContext);

  // set the drag icon during drag-begin
  void SetDragIcon(GdkDragContext* aContext);

 protected:
  virtual ~nsDragSession();

 private:
  // target/destination side vars
  // These variables keep track of the state of the current drag.

  // mCachedDragData/mCachedDragFlavors are tied to mCachedDragContext.
  // mCachedDragContext is not ref counted and may be already deleted
  // on Gtk side.
  // We used it for mCachedDragData/mCachedDragFlavors invalidation
  // only and can't be used for any D&D operation.
  nsTHashMap<void*, RefPtr<DragData>> mCachedDragData;
  nsTArray<GdkAtom> mCachedDragFlavors;

  void SetCachedDragContext(GdkDragContext* aDragContext);

  mozilla::LayoutDeviceIntPoint mTargetWindowPoint;

  int mWaitingForDragDataRequests = 0;

  bool IsDragFlavorAvailable(GdkAtom aRequestedFlavor);

  // this will get the native data from the last target given a
  // specific flavor
  RefPtr<DragData> GetDragData(GdkAtom aRequestedFlavor);

  // source side vars

  // the source of our drags
  GtkWidget* mHiddenWidget;

  // get a list of the sources in gtk's format
  GtkTargetList* GetSourceList(void);

  // attempts to create a semi-transparent drag image. Returns TRUE if
  // successful, FALSE if not
  bool SetAlphaPixmap(mozilla::gfx::SourceSurface* aPixbuf,
                      GdkDragContext* aContext, int32_t aXOffset,
                      int32_t aYOffset,
                      const mozilla::LayoutDeviceIntRect& dragRect);

  gboolean Schedule(DragTask aTask, nsWindow* aWindow,
                    GdkDragContext* aDragContext,
                    mozilla::LayoutDeviceIntPoint aWindowPoint, guint aTime);

  // Callback for g_idle_add_full() to run mScheduledTask.
  MOZ_CAN_RUN_SCRIPT static gboolean TaskDispatchCallback(gpointer data);
  MOZ_CAN_RUN_SCRIPT gboolean RunScheduledTask();
  MOZ_CAN_RUN_SCRIPT void DispatchMotionEvents();
  void UpdateDragAction(GdkDragContext* aDragContext);
  void UpdateDragAction();

#ifdef MOZ_LOGGING
  const char* GetDragServiceTaskName(DragTask aTask);
#endif
  gboolean DispatchDropEvent();
  static uint32_t GetCurrentModifiers();

  nsresult CreateTempFile(nsITransferable* aItem, nsACString& aURI);
};

/**
 * Native GTK nsIDragService implementation
 */
class nsDragService : public nsBaseDragService {
 public:
  static already_AddRefed<nsDragService> GetInstance();
  nsIDragSession* StartDragSession(nsISupports* aWidgetProvider) override;

 protected:
  already_AddRefed<nsIDragSession> CreateDragSession() override;
};

#endif  // nsDragService_h__
