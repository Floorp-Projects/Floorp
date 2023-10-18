/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCocoaWindow_h_
#define nsCocoaWindow_h_

#undef DARWIN

#import <Cocoa/Cocoa.h>

#include "mozilla/StaticMutex.h"
#include "mozilla/RefPtr.h"
#include "nsBaseWidget.h"
#include "nsPIWidgetCocoa.h"
#include "nsCocoaUtils.h"
#include "nsTouchBar.h"
#include <dlfcn.h>
#include <queue>

class nsCocoaWindow;
class nsChildView;
class nsMenuBarX;
@class ChildView;

namespace mozilla {
enum class NativeKeyBindingsType : uint8_t;
}  // namespace mozilla

typedef struct _nsCocoaWindowList {
  _nsCocoaWindowList() : prev(nullptr), window(nullptr) {}
  struct _nsCocoaWindowList* prev;
  nsCocoaWindow* window;  // Weak
} nsCocoaWindowList;

// NSWindow subclass that is the base class for all of our own window classes.
// Among other things, this class handles the storage of those settings that
// need to be persisted across window destruction and reconstruction, i.e. when
// switching to and from fullscreen mode.
// We don't save shadow, transparency mode or background color because it's not
// worth the hassle - Gecko will reset them anyway as soon as the window is
// resized.
@interface BaseWindow : NSWindow {
  // Data Storage
  NSMutableDictionary* mState;
  BOOL mDrawsIntoWindowFrame;

  // Invalidation disabling
  BOOL mDisabledNeedsDisplay;

  NSTrackingArea* mTrackingArea;

  NSRect mDirtyRect;

  BOOL mBeingShown;
  BOOL mDrawTitle;
  BOOL mUseMenuStyle;
  BOOL mIsAnimationSuppressed;

  nsTouchBar* mTouchBar;
}

- (void)importState:(NSDictionary*)aState;
- (NSMutableDictionary*)exportState;
- (void)setDrawsContentsIntoWindowFrame:(BOOL)aState;
- (BOOL)drawsContentsIntoWindowFrame;

// These two methods are like contentRectForFrameRect and
// frameRectForContentRect, but they deal with the rect of the window's "main
// ChildView" instead of the rect of the window's content view. The two are
// sometimes sized differently: The window's content view always covers the
// entire window, whereas the ChildView only covers the full window when
// drawsContentsIntoWindowFrame is YES. When drawsContentsIntoWindowFrame is NO,
// there's a titlebar-sized gap above the ChildView within the content view.
- (NSRect)childViewRectForFrameRect:(NSRect)aFrameRect;
- (NSRect)frameRectForChildViewRect:(NSRect)aChildViewRect;

- (void)mouseEntered:(NSEvent*)aEvent;
- (void)mouseExited:(NSEvent*)aEvent;
- (void)mouseMoved:(NSEvent*)aEvent;
- (void)updateTrackingArea;
- (NSView*)trackingAreaView;

- (void)setBeingShown:(BOOL)aValue;
- (BOOL)isBeingShown;
- (BOOL)isVisibleOrBeingShown;

- (void)setIsAnimationSuppressed:(BOOL)aValue;
- (BOOL)isAnimationSuppressed;

// Returns an autoreleased NSArray containing the NSViews that we consider the
// "contents" of this window. All views in the returned array are subviews of
// this window's content view. However, the array may not include all of the
// content view's subviews; concretely, the ToolbarWindow implementation will
// exclude its MOZTitlebarView from the array that is returned here.
// In the vast majority of cases, the array will only have a single element:
// this window's mainChildView.
- (NSArray<NSView*>*)contentViewContents;

- (ChildView*)mainChildView;

- (void)setWantsTitleDrawn:(BOOL)aDrawTitle;
- (BOOL)wantsTitleDrawn;

- (void)disableSetNeedsDisplay;
- (void)enableSetNeedsDisplay;

- (NSRect)getAndResetNativeDirtyRect;

- (void)setUseMenuStyle:(BOOL)aValue;
@property(nonatomic) mozilla::StyleWindowShadow shadowStyle;

- (void)releaseJSObjects;

@end

@interface NSWindow (Undocumented)

// If a window has been explicitly removed from the "window cache" (to
// deactivate it), it's sometimes necessary to "reset" it to reactivate it
// (and put it back in the "window cache").  One way to do this, which Apple
// often uses, is to set the "window number" to '-1' and then back to its
// original value.
- (void)_setWindowNumber:(NSInteger)aNumber;

- (BOOL)bottomCornerRounded;

// Present in the same form on OS X since at least OS X 10.5.
- (NSRect)contentRectForFrameRect:(NSRect)windowFrame
                        styleMask:(NSUInteger)windowStyle;
- (NSRect)frameRectForContentRect:(NSRect)windowContentRect
                        styleMask:(NSUInteger)windowStyle;

// Present since at least OS X 10.5.  The OS calls this method on NSWindow
// (and its subclasses) to find out which NSFrameView subclass to instantiate
// to create its "frame view".
+ (Class)frameViewClassForStyleMask:(NSUInteger)styleMask;

@end

@interface PopupWindow : BaseWindow {
 @private
  BOOL mIsContextMenu;
}

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)styleMask
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation;
- (BOOL)isContextMenu;
- (void)setIsContextMenu:(BOOL)flag;
- (BOOL)canBecomeMainWindow;

@end

@interface BorderlessWindow : BaseWindow {
}

- (BOOL)canBecomeKeyWindow;
- (BOOL)canBecomeMainWindow;

@end

@interface WindowDelegate : NSObject <NSWindowDelegate> {
  nsCocoaWindow* mGeckoWindow;  // [WEAK] (we are owned by the window)
  // Used to avoid duplication when we send NS_ACTIVATE and
  // NS_DEACTIVATE to Gecko for toplevel widgets.  Starts out
  // false.
  bool mToplevelActiveState;
  BOOL mHasEverBeenZoomed;
}
+ (void)paintMenubarForWindow:(NSWindow*)aWindow;
- (id)initWithGeckoWindow:(nsCocoaWindow*)geckoWind;
- (void)windowDidResize:(NSNotification*)aNotification;
- (nsCocoaWindow*)geckoWidget;
- (bool)toplevelActiveState;
- (void)sendToplevelActivateEvents;
- (void)sendToplevelDeactivateEvents;
@end

@interface MOZTitlebarView : NSVisualEffectView
@end

@interface FullscreenTitlebarTracker : NSTitlebarAccessoryViewController
- (FullscreenTitlebarTracker*)init;
@end

// NSWindow subclass for handling windows with toolbars.
@interface ToolbarWindow : BaseWindow {
  // This window's titlebar view, if present.
  // Will be nil if the window has neither a titlebar nor a unified toolbar.
  // This view is a subview of the window's content view and gets created and
  // destroyed by updateTitlebarView.
  MOZTitlebarView* mTitlebarView;  // [STRONG]
  // mFullscreenTitlebarTracker attaches an invisible rectangle to the system
  // title bar. This allows us to detect when the title bar is showing in
  // fullscreen.
  FullscreenTitlebarTracker* mFullscreenTitlebarTracker;

  CGFloat mUnifiedToolbarHeight;
  CGFloat mSheetAttachmentPosition;
  CGFloat mMenuBarHeight;
  /* Store the height of the titlebar when this window is initialized. The
     titlebarHeight getter returns 0 when in fullscreen, which is not useful in
     some cases. */
  CGFloat mInitialTitlebarHeight;
  NSRect mWindowButtonsRect;
}
- (void)setUnifiedToolbarHeight:(CGFloat)aHeight;
- (CGFloat)unifiedToolbarHeight;
- (CGFloat)titlebarHeight;
- (NSRect)titlebarRect;
- (void)setTitlebarNeedsDisplay;
- (void)setDrawsContentsIntoWindowFrame:(BOOL)aState;
- (void)setSheetAttachmentPosition:(CGFloat)aY;
- (CGFloat)sheetAttachmentPosition;
- (void)placeWindowButtons:(NSRect)aRect;
- (NSRect)windowButtonsRect;
- (void)windowMainStateChanged;
@end

class nsCocoaWindow final : public nsBaseWidget, public nsPIWidgetCocoa {
 private:
  typedef nsBaseWidget Inherited;

 public:
  nsCocoaWindow();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSPIWIDGETCOCOA;  // semicolon for clang-format bug 1629756

  [[nodiscard]] virtual nsresult Create(nsIWidget* aParent,
                                        nsNativeWidget aNativeParent,
                                        const DesktopIntRect& aRect,
                                        InitData* = nullptr) override;

  [[nodiscard]] virtual nsresult Create(nsIWidget* aParent,
                                        nsNativeWidget aNativeParent,
                                        const LayoutDeviceIntRect& aRect,
                                        InitData* = nullptr) override;

  virtual void Destroy() override;

  virtual void Show(bool aState) override;
  virtual bool NeedsRecreateToReshow() override;

  virtual nsIWidget* GetSheetWindowParent(void) override;
  virtual void Enable(bool aState) override;
  virtual bool IsEnabled() const override;
  virtual void SetModal(bool aState) override;
  virtual void SetFakeModal(bool aState) override;
  virtual bool IsRunningAppModal() override;
  virtual bool IsVisible() const override;
  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
  virtual LayoutDeviceIntPoint GetClientOffset() override;
  virtual LayoutDeviceIntMargin ClientToWindowMargin() override;

  virtual void* GetNativeData(uint32_t aDataType) override;

  virtual void ConstrainPosition(DesktopIntPoint&) override;
  virtual void SetSizeConstraints(const SizeConstraints& aConstraints) override;
  virtual void Move(double aX, double aY) override;
  virtual nsSizeMode SizeMode() override { return mSizeMode; }
  virtual void SetSizeMode(nsSizeMode aMode) override;
  virtual void GetWorkspaceID(nsAString& workspaceID) override;
  virtual void MoveToWorkspace(const nsAString& workspaceID) override;
  virtual void SuppressAnimation(bool aSuppress) override;
  virtual void HideWindowChrome(bool aShouldHide) override;

  virtual bool PrepareForFullscreenTransition(nsISupports** aData) override;
  virtual void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                           uint16_t aDuration,
                                           nsISupports* aData,
                                           nsIRunnable* aCallback) override;
  virtual void CleanupFullscreenTransition() override;
  nsresult MakeFullScreen(bool aFullScreen) final;
  nsresult MakeFullScreenWithNativeTransition(bool aFullScreen) final;
  NSAnimation* FullscreenTransitionAnimation() const {
    return mFullscreenTransitionAnimation;
  }
  void ReleaseFullscreenTransitionAnimation() {
    MOZ_ASSERT(mFullscreenTransitionAnimation,
               "Should only be called when there is animation");
    [mFullscreenTransitionAnimation release];
    mFullscreenTransitionAnimation = nil;
  }

  virtual void Resize(double aWidth, double aHeight, bool aRepaint) override;
  virtual void Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) override;
  NSRect GetClientCocoaRect();
  virtual LayoutDeviceIntRect GetClientBounds() override;
  virtual LayoutDeviceIntRect GetScreenBounds() override;
  void ReportMoveEvent();
  void ReportSizeEvent();
  virtual void SetCursor(const Cursor&) override;

  CGFloat BackingScaleFactor();
  void BackingScaleFactorChanged();
  virtual double GetDefaultScaleInternal() override;
  virtual int32_t RoundsWidgetCoordinatesTo() override;

  mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() final {
    return mozilla::DesktopToLayoutDeviceScale(BackingScaleFactor());
  }

  virtual nsresult SetTitle(const nsAString& aTitle) override;

  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override;
  virtual WindowRenderer* GetWindowRenderer() override;
  virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) override;
  virtual void CaptureRollupEvents(bool aDoCapture) override;
  [[nodiscard]] virtual nsresult GetAttention(int32_t aCycleCount) override;
  virtual bool HasPendingInputEvent() override;
  virtual TransparencyMode GetTransparencyMode() override;
  virtual void SetTransparencyMode(TransparencyMode aMode) override;
  virtual void SetWindowShadowStyle(mozilla::StyleWindowShadow aStyle) override;
  virtual void SetWindowOpacity(float aOpacity) override;
  virtual void SetWindowTransform(
      const mozilla::gfx::Matrix& aTransform) override;
  virtual void SetInputRegion(const InputRegion&) override;
  virtual void SetColorScheme(
      const mozilla::Maybe<mozilla::ColorScheme>&) override;
  virtual void SetShowsToolbarButton(bool aShow) override;
  virtual void SetSupportsNativeFullscreen(bool aShow) override;
  virtual void SetWindowAnimationType(WindowAnimationType aType) override;
  virtual void SetDrawsTitle(bool aDrawTitle) override;
  virtual nsresult SetNonClientMargins(const LayoutDeviceIntMargin&) override;
  void SetDrawsInTitlebar(bool aState);
  virtual void UpdateThemeGeometries(
      const nsTArray<ThemeGeometry>& aThemeGeometries) override;
  virtual nsresult SynthesizeNativeMouseEvent(
      LayoutDeviceIntPoint aPoint, NativeMouseMessage aNativeMessage,
      mozilla::MouseButton aButton, nsIWidget::Modifiers aModifierFlags,
      nsIObserver* aObserver) override;
  virtual nsresult SynthesizeNativeMouseScrollEvent(
      LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage, double aDeltaX,
      double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
      uint32_t aAdditionalFlags, nsIObserver* aObserver) override;
  virtual void LockAspectRatio(bool aShouldLock) override;

  void DispatchSizeModeEvent();
  void DispatchOcclusionEvent();

  // be notified that a some form of drag event needs to go into Gecko
  virtual bool DragEvent(unsigned int aMessage,
                         mozilla::gfx::Point aMouseGlobal,
                         UInt16 aKeyModifiers);

  bool HasModalDescendents() { return mNumModalDescendents > 0; }
  NSWindow* GetCocoaWindow() { return mWindow; }

  void SetMenuBar(RefPtr<nsMenuBarX>&& aMenuBar);
  nsMenuBarX* GetMenuBar();

  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override;
  virtual InputContext GetInputContext() override { return mInputContext; }
  MOZ_CAN_RUN_SCRIPT virtual bool GetEditCommands(
      mozilla::NativeKeyBindingsType aType,
      const mozilla::WidgetKeyboardEvent& aEvent,
      nsTArray<mozilla::CommandInt>& aCommands) override;

  void SetPopupWindowLevel();

  bool InFullScreenMode() const { return mInFullScreenMode; }

  void PauseOrResumeCompositor(bool aPause) override;

  bool AsyncPanZoomEnabled() const override;

  bool StartAsyncAutoscroll(const ScreenPoint& aAnchorLocation,
                            const ScrollableLayerGuid& aGuid) override;
  void StopAsyncAutoscroll(const ScrollableLayerGuid& aGuid) override;

  // Class method versions of NSWindow/Delegate callbacks which need to
  // access object state.
  void CocoaWindowWillEnterFullscreen(bool aFullscreen);
  void CocoaWindowDidEnterFullscreen(bool aFullscreen);
  void CocoaWindowDidResize();
  void CocoaSendToplevelActivateEvents();
  void CocoaSendToplevelDeactivateEvents();

  enum class TransitionType {
    Windowed,
    Fullscreen,
    EmulatedFullscreen,
    Miniaturize,
    Deminiaturize,
    Zoom,
  };
  void FinishCurrentTransition();
  void FinishCurrentTransitionIfMatching(const TransitionType& aTransition);

  // Called when something has happened that might cause us to update our
  // fullscreen state. Returns true if we updated state. We'll call this
  // on window resize, and we'll call it when we enter or exit fullscreen,
  // since fullscreen to-and-from zoomed windows won't necessarily trigger
  // a resize.
  bool HandleUpdateFullscreenOnResize();

 protected:
  virtual ~nsCocoaWindow();

  nsresult CreateNativeWindow(const NSRect& aRect, BorderStyle aBorderStyle,
                              bool aRectIsFrameRect, bool aIsPrivateBrowsing);
  nsresult CreatePopupContentView(const LayoutDeviceIntRect& aRect, InitData*);
  void DestroyNativeWindow();
  void UpdateBounds();
  int32_t GetWorkspaceID();

  void DoResize(double aX, double aY, double aWidth, double aHeight,
                bool aRepaint, bool aConstrainToCurrentScreen);

  void UpdateFullscreenState(bool aFullScreen, bool aNativeMode);
  nsresult DoMakeFullScreen(bool aFullScreen, bool aUseSystemTransition);

  virtual already_AddRefed<nsIWidget> AllocateChildPopupWidget() override {
    return nsIWidget::CreateTopLevelWindow();
  }

  nsIWidget* mParent;        // if we're a popup, this is our parent [WEAK]
  nsIWidget* mAncestorLink;  // link to traverse ancestors [WEAK]
  BaseWindow* mWindow;       // our cocoa window [STRONG]
  WindowDelegate*
      mDelegate;  // our delegate for processing window msgs [STRONG]
  RefPtr<nsMenuBarX> mMenuBar;
  NSWindow* mSheetWindowParent;  // if this is a sheet, this is the NSWindow
                                 // it's attached to
  nsChildView*
      mPopupContentView;  // if this is a popup, this is its content widget
  // if this is a toplevel window, and there is any ongoing fullscreen
  // transition, it is the animation object.
  NSAnimation* mFullscreenTransitionAnimation;
  mozilla::StyleWindowShadow mShadowStyle;

  CGFloat mBackingScaleFactor;
  CGFloat mAspectRatio;

  WindowAnimationType mAnimationType;

  bool mWindowMadeHere;  // true if we created the window, false for embedding
  bool mSheetNeedsShow;  // if this is a sheet, are we waiting to be shown?
                         // this is used for sibling sheet contention only
  nsSizeMode mSizeMode;
  bool mInFullScreenMode;
  // Whether we are currently using native fullscreen. It could be false because
  // we are in the emulated fullscreen where we do not use the native
  // fullscreen.
  bool mInNativeFullScreenMode;

  mozilla::Maybe<TransitionType> mTransitionCurrent;
  std::queue<TransitionType> mTransitionsPending;

  // Sometimes we add a transition that wasn't requested by a caller. We do this
  // to manage transitions between states that otherwise would be rejected by
  // Cocoa. When we do this, it's useful to know when we are handling an added
  // transition because we don't want to send size mode events when they
  // execute.
  bool mIsTransitionCurrentAdded = false;

  // Whether we are treating the next resize as the start of a fullscreen
  // transition. If we are, which direction are we going: Fullscreen or
  // Windowed.
  mozilla::Maybe<TransitionType> mUpdateFullscreenOnResize;

  bool IsInTransition() { return mTransitionCurrent.isSome(); }
  void QueueTransition(const TransitionType& aTransition);
  void ProcessTransitions();

  bool mInProcessTransitions = false;
  bool mInLocalRunLoop = false;

  // While running an emulated fullscreen transition, we want to suppress
  // sending size mode events due to window resizing. We fix it up at the end
  // when the transition is complete.
  bool mSuppressSizeModeEvents = false;

  // Ignore occlusion events caused by displaying the temporary fullscreen
  // window during the fullscreen transition animation because only focused
  // contexts are permitted to enter DOM fullscreen.
  int mIgnoreOcclusionCount;

  // Set to true when a native fullscreen transition is initiated -- either to
  // or from fullscreen -- and set to false when it is complete. During this
  // period, we presume the window is visible, which prevents us from sending
  // unnecessary OcclusionStateChanged events.
  bool mHasStartedNativeFullscreen;

  bool mWaitingOnFinishCurrentTransition = false;

  bool mModal;
  bool mFakeModal;

  bool mIsAnimationSuppressed;

  bool mInReportMoveEvent;  // true if in a call to ReportMoveEvent().
  bool mInResize;           // true if in a call to DoResize().
  bool mWindowTransformIsIdentity;
  bool mAlwaysOnTop;
  bool mAspectRatioLocked;

  int32_t mNumModalDescendents;
  InputContext mInputContext;
  NSWindowAnimationBehavior mWindowAnimationBehavior;

 private:
  // This is class state for tracking which nsCocoaWindow, if any, is in the
  // middle of a native fullscreen transition.
  static mozilla::StaticDataMutex<nsCocoaWindow*> sWindowInNativeTransition;

  // This function returns true if the caller has been able to claim the sole
  // permission to start a native transition. It must be followed by a call
  // to EndOurNativeTransition() when the native transition is complete.
  bool CanStartNativeTransition();
  bool WeAreInNativeTransition();
  void EndOurNativeTransition();

  // true if Show() has been called.
  bool mWasShown;
};

#endif  // nsCocoaWindow_h_
