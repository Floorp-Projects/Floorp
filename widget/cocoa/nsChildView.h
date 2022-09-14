/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChildView_h_
#define nsChildView_h_

// formal protocols
#include "mozView.h"
#ifdef ACCESSIBILITY
#  include "mozilla/a11y/LocalAccessible.h"
#  include "mozAccessibleProtocol.h"
#endif

#include "nsISupports.h"
#include "nsBaseWidget.h"
#include "nsIWeakReferenceUtils.h"
#include "TextInputHandler.h"
#include "nsCocoaUtils.h"
#include "gfxQuartzSurface.h"
#include "GLContextTypes.h"
#include "mozilla/DataMutex.h"
#include "mozilla/Mutex.h"
#include "nsRegion.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/webrender/WebRenderTypes.h"

#include "nsString.h"
#include "nsIDragService.h"
#include "ViewRegion.h"
#include "CFTypeRefPtr.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <AppKit/NSOpenGL.h>

class nsChildView;
class nsCocoaWindow;

namespace {
class GLPresenter;
}  // namespace

namespace mozilla {
enum class NativeKeyBindingsType : uint8_t;

class InputData;
class PanGestureInput;
class VibrancyManager;
namespace layers {
class GLManager;
class IAPZCTreeManager;
class NativeLayerRootCA;
class NativeLayerCA;
}  // namespace layers
namespace widget {
class WidgetRenderingContext;
}  // namespace widget
}  // namespace mozilla

@class PixelHostingView;

@interface NSEvent (Undocumented)

// Return Cocoa event's corresponding Carbon event.  Not initialized (on
// synthetic events) until the OS actually "sends" the event.  This method
// has been present in the same form since at least OS X 10.2.8.
- (EventRef)_eventRef;

// stage From 10.10.3 for force touch event
@property(readonly) NSInteger stage;

@end

@interface NSView (Undocumented)

// Undocumented method of one or more of NSFrameView's subclasses.  Called
// when one or more of the titlebar buttons needs to be repositioned, to
// disappear, or to reappear (say if the window's style changes).  If
// 'redisplay' is true, the entire titlebar (the window's top 22 pixels) is
// marked as needing redisplay.  This method has been present in the same
// format since at least OS X 10.5.
- (void)_tileTitlebarAndRedisplay:(BOOL)redisplay;

// The following undocumented methods are used to work around bug 1069658,
// which is an Apple bug or design flaw that effects Yosemite.  None of them
// were present prior to Yosemite (OS X 10.10).
- (NSView*)titlebarView;                   // Method of NSThemeFrame
- (NSView*)titlebarContainerView;          // Method of NSThemeFrame
- (BOOL)transparent;                       // Method of NSTitlebarView and NSTitlebarContainerView
- (void)setTransparent:(BOOL)transparent;  // Method of NSTitlebarView and
                                           // NSTitlebarContainerView

// Available since 10.7.4:
- (void)viewDidChangeBackingProperties;
@end

@interface ChildView : NSView <
#ifdef ACCESSIBILITY
                           mozAccessible,
#endif
                           mozView,
                           NSTextInputClient,
                           NSDraggingSource,
                           NSDraggingDestination,
                           NSPasteboardItemDataProvider> {
 @private
  // the nsChildView that created the view. It retains this NSView, so
  // the link back to it must be weak.
  nsChildView* mGeckoChild;

  // Text input handler for mGeckoChild and us.  Note that this is a weak
  // reference.  Ideally, this should be a strong reference but a ChildView
  // object can live longer than the mGeckoChild that owns it.  And if
  // mTextInputHandler were a strong reference, this would make it difficult
  // for Gecko's leak detector to detect leaked TextInputHandler objects.
  // This is initialized by [mozView installTextInputHandler:aHandler] and
  // cleared by [mozView uninstallTextInputHandler].
  mozilla::widget::TextInputHandler* mTextInputHandler;  // [WEAK]

  // when mouseDown: is called, we store its event here (strong)
  NSEvent* mLastMouseDownEvent;

  // Needed for IME support in e10s mode.  Strong.
  NSEvent* mLastKeyDownEvent;

  // Whether the last mouse down event was blocked from Gecko.
  BOOL mBlockedLastMouseDown;

  // when acceptsFirstMouse: is called, we store the event here (strong)
  NSEvent* mClickThroughMouseDownEvent;

  // WheelStart/Stop events should always come in pairs. This BOOL records the
  // last received event so that, when we receive one of the events, we make sure
  // to send its pair event first, in case we didn't yet for any reason.
  BOOL mExpectingWheelStop;

  // Whether we're inside updateRootCALayer at the moment.
  BOOL mIsUpdatingLayer;

  // Holds our drag service across multiple drag calls. The reference to the
  // service is obtained when the mouse enters the view and is released when
  // the mouse exits or there is a drop. This prevents us from having to
  // re-establish the connection to the service manager many times per second
  // when handling |draggingUpdated:| messages.
  nsIDragService* mDragService;

  // Gestures support
  //
  // mGestureState is used to detect when Cocoa has called both
  // magnifyWithEvent and rotateWithEvent within the same
  // beginGestureWithEvent and endGestureWithEvent sequence. We
  // discard the spurious gesture event so as not to confuse Gecko.
  //
  // mCumulativeRotation keeps track of the total amount of rotation
  // performed during a rotate gesture so we can send that value with
  // the final MozRotateGesture event.
  enum {
    eGestureState_None,
    eGestureState_StartGesture,
    eGestureState_MagnifyGesture,
    eGestureState_RotateGesture
  } mGestureState;
  float mCumulativeRotation;

#ifdef __LP64__
  // Support for fluid swipe tracking.
  BOOL* mCancelSwipeAnimation;
#endif

  // Whether this uses off-main-thread compositing.
  BOOL mUsingOMTCompositor;

  // Subviews of self, which act as container views for vibrancy views and
  // non-draggable views.
  NSView* mVibrancyViewsContainer;      // [STRONG]
  NSView* mNonDraggableViewsContainer;  // [STRONG]

  // The layer-backed view that hosts our drawing. Always non-null.
  // This is a subview of self so that it can be ordered on top of mVibrancyViewsContainer.
  PixelHostingView* mPixelHostingView;

  // The CALayer that wraps Gecko's rendered contents. It's a sublayer of
  // mPixelHostingView's backing layer. Always non-null.
  CALayer* mRootCALayer;  // [STRONG]

  // Last pressure stage by trackpad's force click
  NSInteger mLastPressureStage;
}

// class initialization
+ (void)initialize;

+ (void)registerViewForDraggedTypes:(NSView*)aView;

// these are sent to the first responder when the window key status changes
- (void)viewsWindowDidBecomeKey;
- (void)viewsWindowDidResignKey;

// Stop NSView hierarchy being changed during [ChildView drawRect:]
- (void)delayedTearDown;

- (void)handleMouseMoved:(NSEvent*)aEvent;

- (void)sendMouseEnterOrExitEvent:(NSEvent*)aEvent
                            enter:(BOOL)aEnter
                         exitFrom:(mozilla::WidgetMouseEvent::ExitFrom)aExitFrom;

// Call this during operations that will likely trigger a main thread
// CoreAnimation paint of the window, during which Gecko should do its own
// painting and present the results atomically with that main thread transaction.
// This method will suspend off-thread window updates so that the upcoming paint
// can be atomic, and mark the layer as needing display so that
// HandleMainThreadCATransaction gets called and Gecko gets a chance to paint.
- (void)ensureNextCompositeIsAtomicWithMainThreadPaint;

- (NSView*)vibrancyViewsContainer;
- (NSView*)nonDraggableViewsContainer;
- (NSView*)pixelHostingView;

- (BOOL)isCoveringTitlebar;

- (void)viewWillStartLiveResize;
- (void)viewDidEndLiveResize;

/*
 * Gestures support
 *
 * The prototypes swipeWithEvent, beginGestureWithEvent, smartMagnifyWithEvent,
 * rotateWithEvent and endGestureWithEvent were obtained from the following
 * links:
 * https://developer.apple.com/library/mac/#documentation/Cocoa/Reference/ApplicationKit/Classes/NSResponder_Class/Reference/Reference.html
 * https://developer.apple.com/library/mac/#releasenotes/Cocoa/AppKit.html
 */
- (void)swipeWithEvent:(NSEvent*)anEvent;
- (void)beginGestureWithEvent:(NSEvent*)anEvent;
- (void)magnifyWithEvent:(NSEvent*)anEvent;
- (void)smartMagnifyWithEvent:(NSEvent*)anEvent;
- (void)rotateWithEvent:(NSEvent*)anEvent;
- (void)endGestureWithEvent:(NSEvent*)anEvent;

- (void)scrollWheel:(NSEvent*)anEvent;

- (void)setUsingOMTCompositor:(BOOL)aUseOMTC;

- (NSEvent*)lastKeyDownEvent;

+ (uint32_t)sUniqueKeyEventId;

+ (NSMutableDictionary*)sNativeKeyEventsMap;
@end

class ChildViewMouseTracker {
 public:
  static void MouseMoved(NSEvent* aEvent);
  static void MouseScrolled(NSEvent* aEvent);
  static void OnDestroyView(ChildView* aView);
  static void OnDestroyWindow(NSWindow* aWindow);
  static BOOL WindowAcceptsEvent(NSWindow* aWindow, NSEvent* aEvent, ChildView* aView,
                                 BOOL isClickThrough = NO);
  static void MouseExitedWindow(NSEvent* aEvent);
  static void MouseEnteredWindow(NSEvent* aEvent);
  static void NativeMenuOpened();
  static void NativeMenuClosed();
  static void ReEvaluateMouseEnterState(NSEvent* aEvent = nil, ChildView* aOldView = nil);
  static void ResendLastMouseMoveEvent();
  static ChildView* ViewForEvent(NSEvent* aEvent);

  static ChildView* sLastMouseEventView;
  static NSEvent* sLastMouseMoveEvent;
  static NSWindow* sWindowUnderMouse;
  static NSPoint sLastScrollEventScreenLocation;
};

//-------------------------------------------------------------------------
//
// nsChildView
//
//-------------------------------------------------------------------------

class nsChildView final : public nsBaseWidget {
 private:
  typedef nsBaseWidget Inherited;
  typedef mozilla::layers::IAPZCTreeManager IAPZCTreeManager;

 public:
  nsChildView();

  // nsIWidget interface
  [[nodiscard]] virtual nsresult Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                                        const LayoutDeviceIntRect& aRect,
                                        nsWidgetInitData* aInitData = nullptr) override;

  virtual void Destroy() override;

  virtual void Show(bool aState) override;
  virtual bool IsVisible() const override;

  virtual void SetParent(nsIWidget* aNewParent) override;
  virtual nsIWidget* GetParent(void) override;
  virtual float GetDPI() override;

  virtual void Move(double aX, double aY) override;
  virtual void Resize(double aWidth, double aHeight, bool aRepaint) override;
  virtual void Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint) override;

  virtual void Enable(bool aState) override;
  virtual bool IsEnabled() const override;

  virtual nsSizeMode SizeMode() override { return mSizeMode; }
  virtual void SetSizeMode(nsSizeMode aMode) override { mSizeMode = aMode; }

  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  virtual LayoutDeviceIntRect GetBounds() override;
  virtual LayoutDeviceIntRect GetClientBounds() override;
  virtual LayoutDeviceIntRect GetScreenBounds() override;

  // Refresh mBounds with up-to-date values from [mView frame].
  // Only called if this nsChildView is the popup content view of a popup window.
  // For popup windows, the nsIWidget interface to Gecko is provided by
  // nsCocoaWindow, not by nsChildView. So nsCocoaWindow manages resize requests
  // from Gecko, fires resize events, and resizes the native NSWindow and NSView.
  void UpdateBoundsFromView();

  // Returns the "backing scale factor" of the view's window, which is the
  // ratio of pixels in the window's backing store to Cocoa points. Prior to
  // HiDPI support in OS X 10.7, this was always 1.0, but in HiDPI mode it
  // will be 2.0 (and might potentially other values as screen resolutions
  // evolve). This gives the relationship between what Gecko calls "device
  // pixels" and the Cocoa "points" coordinate system.
  CGFloat BackingScaleFactor() const;

  mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() final {
    return mozilla::DesktopToLayoutDeviceScale(BackingScaleFactor());
  }

  // Call if the window's backing scale factor changes - i.e., it is moved
  // between HiDPI and non-HiDPI screens
  void BackingScaleFactorChanged();

  virtual double GetDefaultScaleInternal() override;

  virtual int32_t RoundsWidgetCoordinatesTo() override;

  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override;
  void EnsureContentLayerForMainThreadPainting();

  virtual void* GetNativeData(uint32_t aDataType) override;
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
  virtual bool ShowsResizeIndicator(LayoutDeviceIntRect* aResizerRect) override { return false; }

  virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent, nsEventStatus& aStatus) override;

  virtual bool WidgetTypeSupportsAcceleration() override;
  virtual bool ShouldUseOffMainThreadCompositing() override;

  virtual void SetCursor(const Cursor&) override;

  virtual nsresult SetTitle(const nsAString& title) override;

  [[nodiscard]] virtual nsresult GetAttention(int32_t aCycleCount) override;

  virtual bool HasPendingInputEvent() override;

  bool SendEventToNativeMenuSystem(NSEvent* aEvent);
  virtual void PostHandleKeyEvent(mozilla::WidgetKeyboardEvent* aEvent) override;
  virtual nsresult ActivateNativeMenuItemAt(const nsAString& indexString) override;
  virtual nsresult ForceUpdateNativeMenuAt(const nsAString& indexString) override;
  [[nodiscard]] virtual nsresult GetSelectionAsPlaintext(nsAString& aResult) override;

  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override;
  virtual InputContext GetInputContext() override;
  virtual TextEventDispatcherListener* GetNativeTextEventDispatcherListener() override;
  [[nodiscard]] virtual nsresult AttachNativeKeyEvent(
      mozilla::WidgetKeyboardEvent& aEvent) override;
  MOZ_CAN_RUN_SCRIPT virtual bool GetEditCommands(
      mozilla::NativeKeyBindingsType aType, const mozilla::WidgetKeyboardEvent& aEvent,
      nsTArray<mozilla::CommandInt>& aCommands) override;

  virtual void SuppressAnimation(bool aSuppress) override;

  virtual nsresult SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout, int32_t aNativeKeyCode,
                                            uint32_t aModifierFlags, const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters,
                                            nsIObserver* aObserver) override;

  virtual nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                              NativeMouseMessage aNativeMessage,
                                              mozilla::MouseButton aButton,
                                              nsIWidget::Modifiers aModifierFlags,
                                              nsIObserver* aObserver) override;

  virtual nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                             nsIObserver* aObserver) override {
    return SynthesizeNativeMouseEvent(aPoint, NativeMouseMessage::Move,
                                      mozilla::MouseButton::eNotPressed,
                                      nsIWidget::Modifiers::NO_MODIFIERS, aObserver);
  }
  virtual nsresult SynthesizeNativeMouseScrollEvent(LayoutDeviceIntPoint aPoint,
                                                    uint32_t aNativeMessage, double aDeltaX,
                                                    double aDeltaY, double aDeltaZ,
                                                    uint32_t aModifierFlags,
                                                    uint32_t aAdditionalFlags,
                                                    nsIObserver* aObserver) override;
  virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId, TouchPointerState aPointerState,
                                              LayoutDeviceIntPoint aPoint, double aPointerPressure,
                                              uint32_t aPointerOrientation,
                                              nsIObserver* aObserver) override;

  virtual nsresult SynthesizeNativeTouchpadDoubleTap(LayoutDeviceIntPoint aPoint,
                                                     uint32_t aModifierFlags) override;

  // Mac specific methods
  void WillPaintWindow();
  bool PaintWindow(LayoutDeviceIntRegion aRegion);
  bool PaintWindowInDrawTarget(mozilla::gfx::DrawTarget* aDT, const LayoutDeviceIntRegion& aRegion,
                               const mozilla::gfx::IntSize& aSurfaceSize);

  void PaintWindowInContentLayer();
  void HandleMainThreadCATransaction();

#ifdef ACCESSIBILITY
  already_AddRefed<mozilla::a11y::LocalAccessible> GetDocumentAccessible();
#endif

  virtual void CreateCompositor() override;

  virtual bool WidgetPaintsBackground() override { return true; }

  virtual bool PreRender(mozilla::widget::WidgetRenderingContext* aContext) override;
  virtual void PostRender(mozilla::widget::WidgetRenderingContext* aContext) override;
  virtual RefPtr<mozilla::layers::NativeLayerRoot> GetNativeLayerRoot() override;

  virtual void UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries) override;

  virtual void UpdateWindowDraggingRegion(const LayoutDeviceIntRegion& aRegion) override;
  LayoutDeviceIntRegion GetNonDraggableRegion() { return mNonDraggableRegion.Region(); }

  virtual void LookUpDictionary(const nsAString& aText,
                                const nsTArray<mozilla::FontRange>& aFontRangeArray,
                                const bool aIsVertical,
                                const LayoutDeviceIntPoint& aPoint) override;

  void ResetParent();

  static bool DoHasPendingInputEvent();
  static uint32_t GetCurrentInputEventCount();
  static void UpdateCurrentInputEventCount();

  NSView<mozView>* GetEditorView();

  nsCocoaWindow* GetAppWindowWidget() const;

  virtual void ReparentNativeWidget(nsIWidget* aNewParent) override;

  mozilla::widget::TextInputHandler* GetTextInputHandler() { return mTextInputHandler; }

  // unit conversion convenience functions
  int32_t CocoaPointsToDevPixels(CGFloat aPts) const {
    return nsCocoaUtils::CocoaPointsToDevPixels(aPts, BackingScaleFactor());
  }
  LayoutDeviceIntPoint CocoaPointsToDevPixels(const NSPoint& aPt) const {
    return nsCocoaUtils::CocoaPointsToDevPixels(aPt, BackingScaleFactor());
  }
  LayoutDeviceIntPoint CocoaPointsToDevPixelsRoundDown(const NSPoint& aPt) const {
    return nsCocoaUtils::CocoaPointsToDevPixelsRoundDown(aPt, BackingScaleFactor());
  }
  LayoutDeviceIntRect CocoaPointsToDevPixels(const NSRect& aRect) const {
    return nsCocoaUtils::CocoaPointsToDevPixels(aRect, BackingScaleFactor());
  }
  CGFloat DevPixelsToCocoaPoints(int32_t aPixels) const {
    return nsCocoaUtils::DevPixelsToCocoaPoints(aPixels, BackingScaleFactor());
  }
  NSRect DevPixelsToCocoaPoints(const LayoutDeviceIntRect& aRect) const {
    return nsCocoaUtils::DevPixelsToCocoaPoints(aRect, BackingScaleFactor());
  }

  virtual LayoutDeviceIntPoint GetClientOffset() override;

  void DispatchAPZWheelInputEvent(mozilla::InputData& aEvent, bool aCanTriggerSwipe);
  nsEventStatus DispatchAPZInputEvent(mozilla::InputData& aEvent);

  void DispatchDoubleTapGesture(mozilla::TimeStamp aEventTimeStamp,
                                LayoutDeviceIntPoint aScreenPosition,
                                mozilla::Modifiers aModifiers);

  // Called when the main thread enters a phase during which visual changes
  // are imminent and any layer updates on the compositor thread would interfere
  // with visual atomicity.
  // "Async" CATransactions are CATransactions which happen on a thread that's
  // not the main thread.
  void SuspendAsyncCATransactions();

  // Called when we know that the current main thread paint will be completed once
  // the main thread goes back to the event loop.
  void MaybeScheduleUnsuspendAsyncCATransactions();

  // Called from the runnable dispatched by MaybeScheduleUnsuspendAsyncCATransactions().
  // At this point we know that the main thread is done handling the visual change
  // (such as a window resize) and we can start modifying CALayers from the
  // compositor thread again.
  void UnsuspendAsyncCATransactions();

  // Called by nsCocoaWindow when the window's fullscreen state changes.
  void UpdateFullscreen(bool aFullscreen);

  // test only.
  virtual nsresult SetHiDPIMode(bool aHiDPI) override;
  virtual nsresult RestoreHiDPIMode() override;

 protected:
  virtual ~nsChildView();

  void ReportMoveEvent();
  void ReportSizeEvent();

  void TearDownView();

  virtual already_AddRefed<nsIWidget> AllocateChildPopupWidget() override {
    return nsIWidget::CreateTopLevelWindow();
  }

  void ConfigureAPZCTreeManager() override;
  void ConfigureAPZControllerThread() override;

  void UpdateVibrancy(const nsTArray<ThemeGeometry>& aThemeGeometries);
  mozilla::VibrancyManager& EnsureVibrancyManager();

  nsIWidget* GetWidgetForListenerEvents();

 protected:
  ChildView* mView;  // my parallel cocoa view, [STRONG]
  RefPtr<mozilla::widget::TextInputHandler> mTextInputHandler;
  InputContext mInputContext;

  NSView* mParentView;
  nsIWidget* mParentWidget;

#ifdef ACCESSIBILITY
  // weak ref to this childview's associated mozAccessible for speed reasons
  // (we get queried for it *a lot* but don't want to own it)
  nsWeakPtr mAccessible;
#endif

  // Held while the compositor (or WR renderer) thread is compositing.
  // Protects from tearing down the view during compositing and from presenting
  // half-composited layers to the screen.
  mozilla::Mutex mCompositingLock MOZ_UNANNOTATED;

  mozilla::ViewRegion mNonDraggableRegion;

  // Cached value of [mView backingScaleFactor], to avoid sending two obj-c
  // messages (respondsToSelector, backingScaleFactor) every time we need to
  // use it.
  // ** We'll need to reinitialize this if the backing resolution changes. **
  mutable CGFloat mBackingScaleFactor;

  bool mVisible;
  nsSizeMode mSizeMode;
  bool mDrawing;
  bool mIsDispatchPaint;  // Is a paint event being dispatched

  RefPtr<mozilla::layers::NativeLayerRootCA> mNativeLayerRoot;

  // In BasicLayers mode, this is the CoreAnimation layer that contains the
  // rendering from Gecko. It is a sublayer of mNativeLayerRoot's underlying
  // wrapper layer.
  // Lazily created by EnsureContentLayerForMainThreadPainting().
  RefPtr<mozilla::layers::NativeLayerCA> mContentLayer;
  RefPtr<mozilla::layers::SurfacePoolHandle> mPoolHandle;

  // In BasicLayers mode, this is the invalid region of mContentLayer.
  LayoutDeviceIntRegion mContentLayerInvalidRegion;

  mozilla::UniquePtr<mozilla::VibrancyManager> mVibrancyManager;

  RefPtr<mozilla::CancelableRunnable> mUnsuspendAsyncCATransactionsRunnable;

  static uint32_t sLastInputEventCount;

  // This is used by SynthesizeNativeTouchPoint to maintain state between
  // multiple synthesized points
  mozilla::UniquePtr<mozilla::MultiTouchInput> mSynthesizedTouchInput;
};

#endif  // nsChildView_h_
