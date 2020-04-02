/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppWindow_h__
#define mozilla_AppWindow_h__

// Local Includes
#include "nsChromeTreeOwner.h"
#include "nsContentTreeOwner.h"

// Helper classes
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "nsCOMArray.h"
#include "nsRect.h"
#include "Units.h"
#include "mozilla/Mutex.h"

// Interfaces needed
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIAppWindow.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIXULBrowserWindow.h"
#include "nsIWidgetListener.h"
#include "nsIRemoteTab.h"
#include "nsIWebProgressListener.h"
#include "nsITimer.h"

#ifndef MOZ_NEW_XULSTORE
#  include "nsIXULStore.h"
#endif

namespace mozilla {
namespace dom {
class Element;
}  // namespace dom
}  // namespace mozilla

class nsAtom;
class nsXULTooltipListener;
struct nsWidgetInitData;

namespace mozilla {
class PresShell;
class AppWindowTimerCallback;
}  // namespace mozilla

// AppWindow

#define NS_APPWINDOW_IMPL_CID                        \
  { /* 8eaec2f3-ed02-4be2-8e0f-342798477298 */       \
    0x8eaec2f3, 0xed02, 0x4be2, {                    \
      0x8e, 0x0f, 0x34, 0x27, 0x98, 0x47, 0x72, 0x98 \
    }                                                \
  }

class nsContentShellInfo;

namespace mozilla {

class AppWindow final : public nsIBaseWindow,
                        public nsIInterfaceRequestor,
                        public nsIAppWindow,
                        public nsSupportsWeakReference,
                        public nsIWebProgressListener {
  friend class ::nsChromeTreeOwner;
  friend class ::nsContentTreeOwner;

 public:
  // The implementation of non-refcounted nsIWidgetListener, which would hold a
  // strong reference on stack before calling AppWindow's
  // MOZ_CAN_RUN_SCRIPT methods.
  class WidgetListenerDelegate : public nsIWidgetListener {
   public:
    explicit WidgetListenerDelegate(AppWindow* aAppWindow)
        : mAppWindow(aAppWindow) {}

    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual nsIAppWindow* GetAppWindow() override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual mozilla::PresShell* GetPresShell() override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual bool WindowMoved(nsIWidget* aWidget, int32_t x, int32_t y) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual bool WindowResized(nsIWidget* aWidget, int32_t aWidth,
                               int32_t aHeight) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual bool RequestWindowClose(nsIWidget* aWidget) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void SizeModeChanged(nsSizeMode sizeMode) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void UIResolutionChanged() override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void FullscreenWillChange(bool aInFullscreen) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void FullscreenChanged(bool aInFullscreen) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void OcclusionStateChanged(bool aIsFullyOccluded) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void OSToolbarButtonPressed() override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual bool ZLevelChanged(bool aImmediate, nsWindowZ* aPlacement,
                               nsIWidget* aRequestBelow,
                               nsIWidget** aActualBelow) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void WindowActivated() override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void WindowDeactivated() override;

   private:
    // The lifetime of WidgetListenerDelegate is bound to AppWindow so
    // we just use a raw pointer here.
    AppWindow* mAppWindow;
  };

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIAPPWINDOW
  NS_DECL_NSIBASEWINDOW

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_APPWINDOW_IMPL_CID)

  void LockUntilChromeLoad() { mLockedUntilChromeLoad = true; }
  bool IsLocked() const { return mLockedUntilChromeLoad; }
  void IgnoreXULSizeMode(bool aEnable) { mIgnoreXULSizeMode = aEnable; }
  void WasRegistered() { mRegistered = true; }

  // AppWindow methods...
  nsresult Initialize(nsIAppWindow* aParent, nsIAppWindow* aOpener,
                      nsIURI* aUrl, int32_t aInitialWidth,
                      int32_t aInitialHeight, bool aIsHiddenWindow,
                      nsIRemoteTab* aOpeningTab,
                      mozIDOMWindowProxy* aOpenerWIndow,
                      nsWidgetInitData& widgetInitData);

  nsresult Toolbar();

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

  // nsIWidgetListener methods for WidgetListenerDelegate.
  nsIAppWindow* GetAppWindow() { return this; }
  mozilla::PresShell* GetPresShell();
  MOZ_CAN_RUN_SCRIPT
  bool WindowMoved(nsIWidget* aWidget, int32_t aX, int32_t aY);
  MOZ_CAN_RUN_SCRIPT
  bool WindowResized(nsIWidget* aWidget, int32_t aWidth, int32_t aHeight);
  MOZ_CAN_RUN_SCRIPT bool RequestWindowClose(nsIWidget* aWidget);
  MOZ_CAN_RUN_SCRIPT void SizeModeChanged(nsSizeMode aSizeMode);
  MOZ_CAN_RUN_SCRIPT void UIResolutionChanged();
  MOZ_CAN_RUN_SCRIPT void FullscreenWillChange(bool aInFullscreen);
  MOZ_CAN_RUN_SCRIPT void FullscreenChanged(bool aInFullscreen);
  MOZ_CAN_RUN_SCRIPT void OcclusionStateChanged(bool aIsFullyOccluded);
  MOZ_CAN_RUN_SCRIPT void OSToolbarButtonPressed();
  MOZ_CAN_RUN_SCRIPT
  bool ZLevelChanged(bool aImmediate, nsWindowZ* aPlacement,
                     nsIWidget* aRequestBelow, nsIWidget** aActualBelow);
  MOZ_CAN_RUN_SCRIPT void WindowActivated();
  MOZ_CAN_RUN_SCRIPT void WindowDeactivated();

  explicit AppWindow(uint32_t aChromeFlags);

 protected:
  enum persistentAttributes {
    PAD_MISC = 0x1,
    PAD_POSITION = 0x2,
    PAD_SIZE = 0x4
  };

  virtual ~AppWindow();

  friend class mozilla::AppWindowTimerCallback;

  bool ExecuteCloseHandler();
  void ConstrainToOpenerScreen(int32_t* aX, int32_t* aY);

  void SetPersistenceTimer(uint32_t aDirtyFlags);
  void FirePersistenceTimer();

  NS_IMETHOD EnsureChromeTreeOwner();
  NS_IMETHOD EnsureContentTreeOwner();
  NS_IMETHOD EnsurePrimaryContentTreeOwner();
  NS_IMETHOD EnsurePrompter();
  NS_IMETHOD EnsureAuthPrompter();
  NS_IMETHOD ForceRoundedDimensions();
  NS_IMETHOD GetAvailScreenSize(int32_t* aAvailWidth, int32_t* aAvailHeight);

  void FinishFullscreenChange(bool aInFullscreen);

  void ApplyChromeFlags();
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void SizeShell();
  void OnChromeLoaded();
  void StaggerPosition(int32_t& aRequestedX, int32_t& aRequestedY,
                       int32_t aSpecWidth, int32_t aSpecHeight);
  bool LoadPositionFromXUL(int32_t aSpecWidth, int32_t aSpecHeight);
  bool LoadSizeFromXUL(int32_t& aSpecWidth, int32_t& aSpecHeight);
  void SetSpecifiedSize(int32_t aSpecWidth, int32_t aSpecHeight);
  bool UpdateWindowStateFromMiscXULAttributes();
  void SyncAttributesToWidget();
  NS_IMETHOD SavePersistentAttributes();

  bool NeedsTooltipListener();
  void AddTooltipSupport();
  void RemoveTooltipSupport();

  NS_IMETHOD GetWindowDOMWindow(mozIDOMWindowProxy** aDOMWindow);
  dom::Element* GetWindowDOMElement() const;

  // See nsIDocShellTreeOwner for docs on next two methods
  nsresult ContentShellAdded(nsIDocShellTreeItem* aContentShell, bool aPrimary);
  nsresult ContentShellRemoved(nsIDocShellTreeItem* aContentShell);
  NS_IMETHOD GetPrimaryContentSize(int32_t* aWidth, int32_t* aHeight);
  NS_IMETHOD SetPrimaryContentSize(int32_t aWidth, int32_t aHeight);
  nsresult GetRootShellSize(int32_t* aWidth, int32_t* aHeight);
  nsresult SetRootShellSize(int32_t aWidth, int32_t aHeight);

  NS_IMETHOD SizeShellTo(nsIDocShellTreeItem* aShellItem, int32_t aCX,
                         int32_t aCY);
  NS_IMETHOD ExitModalLoop(nsresult aStatus);
  NS_IMETHOD CreateNewChromeWindow(int32_t aChromeFlags,
                                   nsIRemoteTab* aOpeningTab,
                                   mozIDOMWindowProxy* aOpenerWindow,
                                   nsIAppWindow** _retval);
  NS_IMETHOD CreateNewContentWindow(int32_t aChromeFlags,
                                    nsIRemoteTab* aOpeningTab,
                                    mozIDOMWindowProxy* aOpenerWindow,
                                    uint64_t aNextRemoteTabId,
                                    nsIAppWindow** _retval);
  NS_IMETHOD GetHasPrimaryContent(bool* aResult);

  void EnableParent(bool aEnable);
  bool ConstrainToZLevel(bool aImmediate, nsWindowZ* aPlacement,
                         nsIWidget* aReqBelow, nsIWidget** aActualBelow);
  void PlaceWindowLayersBehind(uint32_t aLowLevel, uint32_t aHighLevel,
                               nsIAppWindow* aBehind);
  void SetContentScrollbarVisibility(bool aVisible);
  void PersistentAttributesDirty(uint32_t aDirtyFlags);
  nsresult GetTabCount(uint32_t* aResult);

  void LoadPersistentWindowState();
  nsresult GetPersistentValue(const nsAtom* aAttr, nsAString& aValue);
  nsresult SetPersistentValue(const nsAtom* aAttr, const nsAString& aValue);

  // Enum for the current state of a fullscreen change.
  //
  // It is used to ensure that fullscreen change is issued after both
  // the window state change and the window size change at best effort.
  // This is needed because some platforms can't guarantee the order
  // between such two events.
  //
  // It's changed in the following way:
  // +---------------------------+--------------------------------------+
  // |                           |                                      |
  // |                           v                                      |
  // |                      NotChanging                                 |
  // |                           +                                      |
  // |                           | FullscreenWillChange                 |
  // |                           v                                      |
  // |        +-----------+ WillChange +------------------+             |
  // |        | WindowResized           FullscreenChanged |             |
  // |        v                                           v             |
  // |  WidgetResized                         WidgetEnteredFullscreen   |
  // |        +                              or WidgetExitedFullscreen  |
  // |        | FullscreenChanged                         +             |
  // |        v                          WindowResized or |             |
  // +--------+                          delayed dispatch |             |
  //                                                      v             |
  //                                                      +-------------+
  //
  // The delayed dispatch serves as timeout, which is necessary because it's
  // not even guaranteed that the widget will be resized at all.
  enum class FullscreenChangeState : uint8_t {
    // No current fullscreen change. Any previous change has finished.
    NotChanging,
    // Indicate there is going to be a fullscreen change.
    WillChange,
    // The widget has been resized since WillChange.
    WidgetResized,
    // The widget has entered fullscreen state since WillChange.
    WidgetEnteredFullscreen,
    // The widget has exited fullscreen state since WillChange.
    WidgetExitedFullscreen,
  };

  nsChromeTreeOwner* mChromeTreeOwner;
  nsContentTreeOwner* mContentTreeOwner;
  nsContentTreeOwner* mPrimaryContentTreeOwner;
  nsCOMPtr<nsIWidget> mWindow;
  nsCOMPtr<nsIDocShell> mDocShell;
  nsCOMPtr<nsPIDOMWindowOuter> mDOMWindow;
  nsWeakPtr mParentWindow;
  nsCOMPtr<nsIPrompt> mPrompter;
  nsCOMPtr<nsIAuthPrompt> mAuthPrompter;
  nsCOMPtr<nsIXULBrowserWindow> mXULBrowserWindow;
  nsCOMPtr<nsIDocShellTreeItem> mPrimaryContentShell;
  nsresult mModalStatus;
  FullscreenChangeState mFullscreenChangeState;
  bool mContinueModalLoop;
  bool mDebuting;            // being made visible right now
  bool mChromeLoaded;        // True when chrome has loaded
  bool mSizingShellFromXUL;  // true when in SizeShell()
  bool mShowAfterLoad;
  bool mIntrinsicallySized;
  bool mCenterAfterLoad;
  bool mIsHiddenWindow;
  bool mLockedUntilChromeLoad;
  bool mIgnoreXULSize;
  bool mIgnoreXULPosition;
  bool mChromeFlagsFrozen;
  bool mIgnoreXULSizeMode;
  // mDestroying is used to prevent reentry into into Destroy(), which can
  // otherwise happen due to script running as we tear down various things.
  bool mDestroying;
  bool mRegistered;
  uint32_t mPersistentAttributesDirty;  // persistentAttributes
  uint32_t mPersistentAttributesMask;
  uint32_t mChromeFlags;
  uint64_t mNextRemoteTabId;
  nsString mTitle;
  nsIntRect mOpenerScreenRect;  // the screen rect of the opener

  nsCOMPtr<nsIRemoteTab> mPrimaryBrowserParent;

  nsCOMPtr<nsITimer> mSPTimer;
  mozilla::Mutex mSPTimerLock;
  WidgetListenerDelegate mWidgetListenerDelegate;

 private:
  // GetPrimaryBrowserParentSize is called from xpidl methods and we don't have
  // a good way to annotate those with MOZ_CAN_RUN_SCRIPT yet.  It takes no
  // refcounted args other than "this", and the "this" uses seem ok.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  GetPrimaryRemoteTabSize(int32_t* aWidth, int32_t* aHeight);
  nsresult GetPrimaryContentShellSize(int32_t* aWidth, int32_t* aHeight);
  nsresult SetPrimaryRemoteTabSize(int32_t aWidth, int32_t aHeight);
#ifndef MOZ_NEW_XULSTORE
  nsCOMPtr<nsIXULStore> mLocalStore;
#endif
};

NS_DEFINE_STATIC_IID_ACCESSOR(AppWindow, NS_APPWINDOW_IMPL_CID)

}  // namespace mozilla

#endif /* mozilla_AppWindow_h__ */
