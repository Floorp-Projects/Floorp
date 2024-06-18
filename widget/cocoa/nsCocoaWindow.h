/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCocoaWindow_h_
#define nsCocoaWindow_h_

#undef DARWIN

#import <Cocoa/Cocoa.h>

#include "mozilla/RefPtr.h"
#include "nsBaseWidget.h"
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
  NSView* mViewWithTrackingArea;

  NSRect mDirtyRect;

  BOOL mBeingShown;
  BOOL mDrawTitle;
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

- (void)setEffectViewWrapperForStyle:(mozilla::WindowShadow)aStyle;
@property(nonatomic) mozilla::WindowShadow shadowStyle;

- (void)releaseJSObjects;

@end

@interface NSWindow (Undocumented)
- (NSDictionary*)shadowParameters;

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

@interface FullscreenTitlebarTracker : NSTitlebarAccessoryViewController
- (FullscreenTitlebarTracker*)init;
@end

// NSWindow subclass for handling windows with toolbars.
@interface ToolbarWindow : BaseWindow {
  // mFullscreenTitlebarTracker attaches an invisible rectangle to the system
  // title bar. This allows us to detect when the title bar is showing in
  // fullscreen.
  FullscreenTitlebarTracker* mFullscreenTitlebarTracker;

  CGFloat mMenuBarHeight;
  NSRect mWindowButtonsRect;
}
- (void)setDrawsContentsIntoWindowFrame:(BOOL)aState;
- (void)placeWindowButtons:(NSRect)aRect;
- (NSRect)windowButtonsRect;
- (void)windowMainStateChanged;
@end

class nsCocoaWindow final : public nsBaseWidget {
 private:
  typedef nsBaseWidget Inherited;

 public:
  nsCocoaWindow();

  [[nodiscard]] nsresult Create(nsIWidget* aParent,
                                nsNativeWidget aNativeParent,
                                const DesktopIntRect& aRect,
                                InitData* = nullptr) override;

  [[nodiscard]] nsresult Create(nsIWidget* aParent,
                                nsNativeWidget aNativeParent,
                                const LayoutDeviceIntRect& aRect,
                                InitData* = nullptr) override;

  void Destroy() override;

  void Show(bool aState) override;
  bool NeedsRecreateToReshow() override;

  void Enable(bool aState) override;
  bool IsEnabled() const override;
  void SetModal(bool aState) override;
  bool IsRunningAppModal() override;
  bool IsVisible() const override;
  void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  LayoutDeviceIntPoint WidgetToScreenOffset() override;
  LayoutDeviceIntPoint GetClientOffset() override;
  LayoutDeviceIntMargin ClientToWindowMargin() override;

  void* GetNativeData(uint32_t aDataType) override;

  void ConstrainPosition(DesktopIntPoint&) override;
  void SetSizeConstraints(const SizeConstraints& aConstraints) override;
  void Move(double aX, double aY) override;
  nsSizeMode SizeMode() override { return mSizeMode; }
  void SetSizeMode(nsSizeMode aMode) override;
  void GetWorkspaceID(nsAString& workspaceID) override;
  void MoveToWorkspace(const nsAString& workspaceID) override;
  void SuppressAnimation(bool aSuppress) override;
  void HideWindowChrome(bool aShouldHide) override;

  bool PrepareForFullscreenTransition(nsISupports** aData) override;
  void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                   uint16_t aDuration, nsISupports* aData,
                                   nsIRunnable* aCallback) override;
  void CleanupFullscreenTransition() override;
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

  void Resize(double aWidth, double aHeight, bool aRepaint) override;
  void Resize(double aX, double aY, double aWidth, double aHeight,
              bool aRepaint) override;
  NSRect GetClientCocoaRect();
  LayoutDeviceIntRect GetClientBounds() override;
  LayoutDeviceIntRect GetScreenBounds() override;
  void ReportMoveEvent();
  void ReportSizeEvent();
  void SetCursor(const Cursor&) override;

  CGFloat BackingScaleFactor();
  void BackingScaleFactorChanged();
  double GetDefaultScaleInternal() override;
  int32_t RoundsWidgetCoordinatesTo() override;

  mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() final {
    return mozilla::DesktopToLayoutDeviceScale(BackingScaleFactor());
  }

  nsresult SetTitle(const nsAString& aTitle) override;

  void Invalidate(const LayoutDeviceIntRect& aRect) override;
  WindowRenderer* GetWindowRenderer() override;
  nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus& aStatus) override;
  void CaptureRollupEvents(bool aDoCapture) override;
  [[nodiscard]] nsresult GetAttention(int32_t aCycleCount) override;
  bool HasPendingInputEvent() override;
  TransparencyMode GetTransparencyMode() override;
  void SetTransparencyMode(TransparencyMode aMode) override;
  void SetWindowShadowStyle(mozilla::WindowShadow aStyle) override;
  void SetWindowOpacity(float aOpacity) override;
  void SetWindowTransform(const mozilla::gfx::Matrix& aTransform) override;
  void SetInputRegion(const InputRegion&) override;
  void SetColorScheme(const mozilla::Maybe<mozilla::ColorScheme>&) override;
  void SetShowsToolbarButton(bool aShow) override;
  bool GetSupportsNativeFullscreen();
  void SetSupportsNativeFullscreen(bool aShow) override;
  void SetWindowAnimationType(WindowAnimationType aType) override;
  void SetDrawsTitle(bool aDrawTitle) override;
  nsresult SetNonClientMargins(const LayoutDeviceIntMargin&) override;
  void SetDrawsInTitlebar(bool aState);
  void UpdateThemeGeometries(
      const nsTArray<ThemeGeometry>& aThemeGeometries) override;
  nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                      NativeMouseMessage aNativeMessage,
                                      mozilla::MouseButton aButton,
                                      nsIWidget::Modifiers aModifierFlags,
                                      nsIObserver* aObserver) override;
  nsresult SynthesizeNativeMouseScrollEvent(
      LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage, double aDeltaX,
      double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
      uint32_t aAdditionalFlags, nsIObserver* aObserver) override;
  void LockAspectRatio(bool aShouldLock) override;

  void DispatchSizeModeEvent();
  void DispatchOcclusionEvent();

  // be notified that a some form of drag event needs to go into Gecko
  bool DragEvent(unsigned int aMessage, mozilla::gfx::Point aMouseGlobal,
                 UInt16 aKeyModifiers);

  bool HasModalDescendants() const { return mNumModalDescendants > 0; }
  bool IsModal() const { return mModal; }

  NSWindow* GetCocoaWindow() { return mWindow; }

  void SetMenuBar(RefPtr<nsMenuBarX>&& aMenuBar);
  nsMenuBarX* GetMenuBar();

  void SetInputContext(const InputContext& aContext,
                       const InputContextAction& aAction) override;
  InputContext GetInputContext() override { return mInputContext; }
  MOZ_CAN_RUN_SCRIPT bool GetEditCommands(
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
  void SendSetZLevelEvent();

  void DoResize(double aX, double aY, double aWidth, double aHeight,
                bool aRepaint, bool aConstrainToCurrentScreen);

  void UpdateFullscreenState(bool aFullScreen, bool aNativeMode);
  nsresult DoMakeFullScreen(bool aFullScreen, bool aUseSystemTransition);

  already_AddRefed<nsIWidget> AllocateChildPopupWidget() override {
    return nsIWidget::CreateTopLevelWindow();
  }

  nsIWidget* mParent;        // if we're a popup, this is our parent [WEAK]
  nsIWidget* mAncestorLink;  // link to traverse ancestors [WEAK]
  BaseWindow* mWindow;       // our cocoa window [STRONG]
  WindowDelegate*
      mDelegate;  // our delegate for processing window msgs [STRONG]
  RefPtr<nsMenuBarX> mMenuBar;
  nsChildView*
      mPopupContentView;  // if this is a popup, this is its content widget
  // if this is a toplevel window, and there is any ongoing fullscreen
  // transition, it is the animation object.
  NSAnimation* mFullscreenTransitionAnimation;
  mozilla::WindowShadow mShadowStyle;

  CGFloat mBackingScaleFactor;
  CGFloat mAspectRatio;

  WindowAnimationType mAnimationType;

  bool mWindowMadeHere;  // true if we created the window, false for embedding
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

  // Call this to stop all transition processing, which is useful during
  // window closing and shutdown.
  void CancelAllTransitions();

  bool mInProcessTransitions = false;

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

  bool mModal = false;
  bool mIsAnimationSuppressed = false;

  bool mInReportMoveEvent = false;  // true if in a call to ReportMoveEvent().
  bool mInResize = false;           // true if in a call to DoResize().
  bool mWindowTransformIsIdentity = true;
  bool mAlwaysOnTop = false;
  bool mAspectRatioLocked = false;
  bool mIsAlert = false;  // True if this is an non-native alert window.
  bool mWasShown = false;

  int32_t mNumModalDescendants = 0;
  InputContext mInputContext;
  NSWindowAnimationBehavior mWindowAnimationBehavior;

 private:
  // This is class state for tracking which nsCocoaWindow, if any, is in the
  // middle of a native fullscreen transition.
  static nsCocoaWindow* sWindowInNativeTransition;

  // This function returns true if the caller has been able to claim the sole
  // permission to start a native transition. It must be followed by a call
  // to EndOurNativeTransition() when the native transition is complete.
  bool CanStartNativeTransition();
  void EndOurNativeTransition();
};

#endif  // nsCocoaWindow_h_
