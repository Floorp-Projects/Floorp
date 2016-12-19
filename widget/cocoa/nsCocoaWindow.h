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
#include "nsPIWidgetCocoa.h"
#include "nsCocoaUtils.h"

class nsCocoaWindow;
class nsChildView;
class nsMenuBarX;
@class ChildView;

typedef struct _nsCocoaWindowList {
  _nsCocoaWindowList() : prev(nullptr), window(nullptr) {}
  struct _nsCocoaWindowList *prev;
  nsCocoaWindow *window; // Weak
} nsCocoaWindowList;

// NSWindow subclass that is the base class for all of our own window classes.
// Among other things, this class handles the storage of those settings that
// need to be persisted across window destruction and reconstruction, i.e. when
// switching to and from fullscreen mode.
// We don't save shadow, transparency mode or background color because it's not
// worth the hassle - Gecko will reset them anyway as soon as the window is
// resized.
@interface BaseWindow : NSWindow
{
  // Data Storage
  NSMutableDictionary* mState;
  BOOL mDrawsIntoWindowFrame;
  NSColor* mActiveTitlebarColor;
  NSColor* mInactiveTitlebarColor;

  // Shadow
  BOOL mScheduledShadowInvalidation;

  // Invalidation disabling
  BOOL mDisabledNeedsDisplay;

  // DPI cache. Getting the physical screen size (CGDisplayScreenSize)
  // is ridiculously slow, so we cache it in the toplevel window for all
  // descendants to use.
  float mDPI;

  NSTrackingArea* mTrackingArea;

  NSRect mDirtyRect;

  BOOL mBeingShown;
  BOOL mDrawTitle;
  BOOL mBrightTitlebarForeground;
  BOOL mUseMenuStyle;
}

- (void)importState:(NSDictionary*)aState;
- (NSMutableDictionary*)exportState;
- (void)setDrawsContentsIntoWindowFrame:(BOOL)aState;
- (BOOL)drawsContentsIntoWindowFrame;
- (void)setTitlebarColor:(NSColor*)aColor forActiveWindow:(BOOL)aActive;
- (NSColor*)titlebarColorForActiveWindow:(BOOL)aActive;

- (void)deferredInvalidateShadow;
- (void)invalidateShadow;
- (float)getDPI;

- (void)mouseEntered:(NSEvent*)aEvent;
- (void)mouseExited:(NSEvent*)aEvent;
- (void)mouseMoved:(NSEvent*)aEvent;
- (void)updateTrackingArea;
- (NSView*)trackingAreaView;

- (void)setBeingShown:(BOOL)aValue;
- (BOOL)isBeingShown;
- (BOOL)isVisibleOrBeingShown;

- (ChildView*)mainChildView;

- (NSArray*)titlebarControls;

- (void)setWantsTitleDrawn:(BOOL)aDrawTitle;
- (BOOL)wantsTitleDrawn;

- (void)setUseBrightTitlebarForeground:(BOOL)aBrightForeground;
- (BOOL)useBrightTitlebarForeground;

- (void)disableSetNeedsDisplay;
- (void)enableSetNeedsDisplay;

- (NSRect)getAndResetNativeDirtyRect;

- (void)setUseMenuStyle:(BOOL)aValue;

@end

@interface NSWindow (Undocumented)

// If a window has been explicitly removed from the "window cache" (to
// deactivate it), it's sometimes necessary to "reset" it to reactivate it
// (and put it back in the "window cache").  One way to do this, which Apple
// often uses, is to set the "window number" to '-1' and then back to its
// original value.
- (void)_setWindowNumber:(NSInteger)aNumber;

// If we set the window's stylemask to be textured, the corners on the bottom of
// the window are rounded by default. We use this private method to make
// the corners square again, a la Safari. Starting with 10.7, all windows have
// rounded bottom corners, so this call doesn't have any effect there.
- (void)setBottomCornerRounded:(BOOL)rounded;
- (BOOL)bottomCornerRounded;

// Present in the same form on OS X since at least OS X 10.5.
- (NSRect)contentRectForFrameRect:(NSRect)windowFrame styleMask:(NSUInteger)windowStyle;
- (NSRect)frameRectForContentRect:(NSRect)windowContentRect styleMask:(NSUInteger)windowStyle;

// Present since at least OS X 10.5.  The OS calls this method on NSWindow
// (and its subclasses) to find out which NSFrameView subclass to instantiate
// to create its "frame view".
+ (Class)frameViewClassForStyleMask:(NSUInteger)styleMask;

@end

@interface PopupWindow : BaseWindow
{
@private
  BOOL mIsContextMenu;
}

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask
      backing:(NSBackingStoreType)bufferingType defer:(BOOL)deferCreation;
- (BOOL)isContextMenu;
- (void)setIsContextMenu:(BOOL)flag;
- (BOOL)canBecomeMainWindow;

@end

@interface BorderlessWindow : BaseWindow
{
}

- (BOOL)canBecomeKeyWindow;
- (BOOL)canBecomeMainWindow;

@end

@interface WindowDelegate : NSObject <NSWindowDelegate>
{
  nsCocoaWindow* mGeckoWindow; // [WEAK] (we are owned by the window)
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

@class ToolbarWindow;

// NSColor subclass that allows us to draw separate colors both in the titlebar
// and for background of the window.
@interface TitlebarAndBackgroundColor : NSColor
{
  ToolbarWindow *mWindow; // [WEAK] (we are owned by the window)
}

- (id)initWithWindow:(ToolbarWindow*)aWindow;

@end

// NSWindow subclass for handling windows with toolbars.
@interface ToolbarWindow : BaseWindow
{
  TitlebarAndBackgroundColor *mColor;
  CGFloat mUnifiedToolbarHeight;
  NSColor *mBackgroundColor;
  NSView *mTitlebarView; // strong
  NSRect mWindowButtonsRect;
  NSRect mFullScreenButtonRect;
}
// Pass nil here to get the default appearance.
- (void)setTitlebarColor:(NSColor*)aColor forActiveWindow:(BOOL)aActive;
- (void)setUnifiedToolbarHeight:(CGFloat)aHeight;
- (CGFloat)unifiedToolbarHeight;
- (CGFloat)titlebarHeight;
- (NSRect)titlebarRect;
- (void)setTitlebarNeedsDisplayInRect:(NSRect)aRect sync:(BOOL)aSync;
- (void)setTitlebarNeedsDisplayInRect:(NSRect)aRect;
- (void)setDrawsContentsIntoWindowFrame:(BOOL)aState;
- (void)setSheetAttachmentPosition:(CGFloat)aY;
- (void)placeWindowButtons:(NSRect)aRect;
- (void)placeFullScreenButton:(NSRect)aRect;
- (NSPoint)windowButtonsPositionWithDefaultPosition:(NSPoint)aDefaultPosition;
- (NSPoint)fullScreenButtonPositionWithDefaultPosition:(NSPoint)aDefaultPosition;
- (void)setTemporaryBackgroundColor;
- (void)restoreBackgroundColor;
@end

class nsCocoaWindow : public nsBaseWidget, public nsPIWidgetCocoa
{
private:
  typedef nsBaseWidget Inherited;

public:

    nsCocoaWindow();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSPIWIDGETCOCOA

    virtual MOZ_MUST_USE nsresult Create(nsIWidget* aParent,
                                         nsNativeWidget aNativeParent,
                                         const DesktopIntRect& aRect,
                                         nsWidgetInitData* aInitData = nullptr)
                                         override;

    virtual MOZ_MUST_USE nsresult Create(nsIWidget* aParent,
                                         nsNativeWidget aNativeParent,
                                         const LayoutDeviceIntRect& aRect,
                                         nsWidgetInitData* aInitData = nullptr)
                                         override;

    virtual void            Destroy() override;

    NS_IMETHOD              Show(bool aState) override;
    virtual nsIWidget*      GetSheetWindowParent(void) override;
    NS_IMETHOD              Enable(bool aState) override;
    virtual bool            IsEnabled() const override;
    virtual void            SetModal(bool aState) override;
    virtual void            SetFakeModal(bool aState) override;
    virtual bool            IsRunningAppModal() override;
    virtual bool            IsVisible() const override;
    NS_IMETHOD              SetFocus(bool aState=false) override;
    virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
    virtual LayoutDeviceIntPoint GetClientOffset() override;
    virtual LayoutDeviceIntSize
    ClientToWindowSize(const LayoutDeviceIntSize& aClientSize) override;

    virtual void* GetNativeData(uint32_t aDataType) override;

    virtual void            ConstrainPosition(bool aAllowSlop,
                                              int32_t *aX, int32_t *aY) override;
    virtual void            SetSizeConstraints(const SizeConstraints& aConstraints) override;
    virtual void            Move(double aX, double aY) override;
    virtual void            SetSizeMode(nsSizeMode aMode) override;
    virtual void            HideWindowChrome(bool aShouldHide) override;

    void EnteredFullScreen(bool aFullScreen, bool aNativeMode = true);
    virtual bool PrepareForFullscreenTransition(nsISupports** aData) override;
    virtual void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                             uint16_t aDuration,
                                             nsISupports* aData,
                                             nsIRunnable* aCallback) override;
    virtual nsresult MakeFullScreen(
      bool aFullScreen, nsIScreen* aTargetScreen = nullptr) override final;
    NS_IMETHOD MakeFullScreenWithNativeTransition(
      bool aFullScreen, nsIScreen* aTargetScreen = nullptr) override final;
    NSAnimation* FullscreenTransitionAnimation() const { return mFullscreenTransitionAnimation; }
    void ReleaseFullscreenTransitionAnimation()
    {
      MOZ_ASSERT(mFullscreenTransitionAnimation,
                 "Should only be called when there is animation");
      [mFullscreenTransitionAnimation release];
      mFullscreenTransitionAnimation = nil;
    }

    virtual void            Resize(double aWidth, double aHeight, bool aRepaint) override;
    virtual void            Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint) override;
    virtual LayoutDeviceIntRect GetClientBounds() override;
    virtual LayoutDeviceIntRect GetScreenBounds() override;
    void                    ReportMoveEvent();
    void                    ReportSizeEvent();
    NS_IMETHOD              SetCursor(nsCursor aCursor) override;
    NS_IMETHOD              SetCursor(imgIContainer* aCursor, uint32_t aHotspotX, uint32_t aHotspotY) override;

    CGFloat                 BackingScaleFactor();
    void                    BackingScaleFactorChanged();
    virtual double          GetDefaultScaleInternal() override;
    virtual int32_t         RoundsWidgetCoordinatesTo() override;

    mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() final {
      return mozilla::DesktopToLayoutDeviceScale(BackingScaleFactor());
    }

    NS_IMETHOD              SetTitle(const nsAString& aTitle) override;

    NS_IMETHOD Invalidate(const LayoutDeviceIntRect& aRect) override;
    virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override;
    virtual LayerManager* GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                          LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;
    NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                             nsEventStatus& aStatus) override;
    virtual void CaptureRollupEvents(nsIRollupListener * aListener,
                                     bool aDoCapture) override;
    virtual MOZ_MUST_USE nsresult GetAttention(int32_t aCycleCount) override;
    virtual bool HasPendingInputEvent() override;
    virtual nsTransparencyMode GetTransparencyMode() override;
    virtual void SetTransparencyMode(nsTransparencyMode aMode) override;
    virtual void SetWindowShadowStyle(int32_t aStyle) override;
    virtual void SetShowsToolbarButton(bool aShow) override;
    virtual void SetShowsFullScreenButton(bool aShow) override;
    virtual void SetWindowAnimationType(WindowAnimationType aType) override;
    virtual void SetDrawsTitle(bool aDrawTitle) override;
    virtual void SetUseBrightTitlebarForeground(bool aBrightForeground) override;
    NS_IMETHOD SetNonClientMargins(LayoutDeviceIntMargin& aMargins) override;
    virtual void SetWindowTitlebarColor(nscolor aColor, bool aActive) override;
    virtual void SetDrawsInTitlebar(bool aState) override;
    virtual void UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries) override;
    virtual nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                                uint32_t aNativeMessage,
                                                uint32_t aModifierFlags,
                                                nsIObserver* aObserver) override;

    void DispatchSizeModeEvent();

    // be notified that a some form of drag event needs to go into Gecko
    virtual bool DragEvent(unsigned int aMessage, mozilla::gfx::Point aMouseGlobal, UInt16 aKeyModifiers);

    bool HasModalDescendents() { return mNumModalDescendents > 0; }
    NSWindow *GetCocoaWindow() { return mWindow; }

    void SetMenuBar(nsMenuBarX* aMenuBar);
    nsMenuBarX *GetMenuBar();

    NS_IMETHOD_(void) SetInputContext(
                        const InputContext& aContext,
                        const InputContextAction& aAction) override;
    NS_IMETHOD_(InputContext) GetInputContext() override
    {
      return mInputContext;
    }
    NS_IMETHOD_(bool) ExecuteNativeKeyBinding(
                        NativeKeyBindingsType aType,
                        const mozilla::WidgetKeyboardEvent& aEvent,
                        DoCommandCallback aCallback,
                        void* aCallbackData) override;

    void SetPopupWindowLevel();

protected:
  virtual ~nsCocoaWindow();

  nsresult             CreateNativeWindow(const NSRect &aRect,
                                          nsBorderStyle aBorderStyle,
                                          bool aRectIsFrameRect);
  nsresult             CreatePopupContentView(const LayoutDeviceIntRect &aRect);
  void                 DestroyNativeWindow();
  void                 AdjustWindowShadow();
  void                 SetWindowBackgroundBlur();
  void                 UpdateBounds();

  void                 DoResize(double aX, double aY, double aWidth, double aHeight,
                                bool aRepaint, bool aConstrainToCurrentScreen);

  inline bool ShouldToggleNativeFullscreen(bool aFullScreen,
                                           bool aUseSystemTransition);
  nsresult DoMakeFullScreen(bool aFullScreen, bool aUseSystemTransition);

  virtual already_AddRefed<nsIWidget>
  AllocateChildPopupWidget() override
  {
    static NS_DEFINE_IID(kCPopUpCID, NS_POPUP_CID);
    nsCOMPtr<nsIWidget> widget = do_CreateInstance(kCPopUpCID);
    return widget.forget();
  }

  nsIWidget*           mParent;         // if we're a popup, this is our parent [WEAK]
  nsIWidget*           mAncestorLink;   // link to traverse ancestors [WEAK]
  BaseWindow*          mWindow;         // our cocoa window [STRONG]
  WindowDelegate*      mDelegate;       // our delegate for processing window msgs [STRONG]
  RefPtr<nsMenuBarX> mMenuBar;
  NSWindow*            mSheetWindowParent; // if this is a sheet, this is the NSWindow it's attached to
  nsChildView*         mPopupContentView; // if this is a popup, this is its content widget
  // if this is a toplevel window, and there is any ongoing fullscreen
  // transition, it is the animation object.
  NSAnimation*         mFullscreenTransitionAnimation;
  int32_t              mShadowStyle;

  CGFloat              mBackingScaleFactor;

  WindowAnimationType  mAnimationType;

  bool                 mWindowMadeHere; // true if we created the window, false for embedding
  bool                 mSheetNeedsShow; // if this is a sheet, are we waiting to be shown?
                                        // this is used for sibling sheet contention only
  bool                 mInFullScreenMode;
  bool                 mInFullScreenTransition; // true from the request to enter/exit fullscreen
                                                // (MakeFullScreen() call) to EnteredFullScreen()
  bool                 mModal;
  bool                 mFakeModal;

  // Only true on 10.7+ if SetShowsFullScreenButton(true) is called.
  bool                 mSupportsNativeFullScreen;
  // Whether we are currently using native fullscreen. It could be false because
  // we are in the DOM fullscreen where we do not use the native fullscreen.
  bool                 mInNativeFullScreenMode;

  bool                 mIsAnimationSuppressed;

  bool                 mInReportMoveEvent; // true if in a call to ReportMoveEvent().
  bool                 mInResize; // true if in a call to DoResize().

  int32_t              mNumModalDescendents;
  InputContext         mInputContext;
};

#endif // nsCocoaWindow_h_
