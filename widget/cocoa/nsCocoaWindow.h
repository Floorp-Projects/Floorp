/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCocoaWindow_h_
#define nsCocoaWindow_h_

#undef DARWIN

#import <Cocoa/Cocoa.h>

#include "nsBaseWidget.h"
#include "nsPIWidgetCocoa.h"
#include "nsAutoPtr.h"
#include "nsCocoaUtils.h"

class nsCocoaWindow;
class nsChildView;
class nsMenuBarX;
@class ChildView;

// If we are using an SDK older than 10.7, define bits we need that are missing
// from it.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

enum {
    NSWindowAnimationBehaviorDefault = 0,
    NSWindowAnimationBehaviorNone = 2,
    NSWindowAnimationBehaviorDocumentWindow = 3,
    NSWindowAnimationBehaviorUtilityWindow = 4,
    NSWindowAnimationBehaviorAlertPanel = 5,
    NSWindowCollectionBehaviorFullScreenPrimary = 128, // 1 << 7
};

typedef NSInteger NSWindowAnimationBehavior;

@interface NSWindow (LionWindowFeatures)
- (void)setAnimationBehavior:(NSWindowAnimationBehavior)newAnimationBehavior;
- (void)toggleFullScreen:(id)sender;
@end

#endif

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

  BOOL mBeingShown;
  BOOL mDrawTitle;
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

- (void)disableSetNeedsDisplay;
- (void)enableSetNeedsDisplay;

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

#if defined( MAC_OS_X_VERSION_10_6 ) && ( MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6 )
@interface WindowDelegate : NSObject <NSWindowDelegate>
#else
@interface WindowDelegate : NSObject
#endif
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
- (void)placeWindowButtons:(NSRect)aRect;
- (void)placeFullScreenButton:(NSRect)aRect;
- (NSPoint)windowButtonsPositionWithDefaultPosition:(NSPoint)aDefaultPosition;
- (NSPoint)fullScreenButtonPositionWithDefaultPosition:(NSPoint)aDefaultPosition;
@end

class nsCocoaWindow : public nsBaseWidget, public nsPIWidgetCocoa
{
private:
  
  typedef nsBaseWidget Inherited;

public:

    nsCocoaWindow();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSPIWIDGETCOCOA
      
    NS_IMETHOD              Create(nsIWidget* aParent,
                                   nsNativeWidget aNativeParent,
                                   const nsIntRect &aRect,
                                   nsDeviceContext *aContext,
                                   nsWidgetInitData *aInitData = nullptr);

    NS_IMETHOD              Destroy();

    NS_IMETHOD              Show(bool aState);
    virtual nsIWidget*      GetSheetWindowParent(void);
    NS_IMETHOD              Enable(bool aState);
    virtual bool            IsEnabled() const;
    NS_IMETHOD              SetModal(bool aState);
    virtual bool            IsVisible() const;
    NS_IMETHOD              SetFocus(bool aState=false);
    virtual nsIntPoint WidgetToScreenOffset();
    virtual nsIntPoint GetClientOffset();
    virtual nsIntSize ClientToWindowSize(const nsIntSize& aClientSize);

    virtual void* GetNativeData(uint32_t aDataType) ;

    NS_IMETHOD              ConstrainPosition(bool aAllowSlop,
                                              int32_t *aX, int32_t *aY);
    virtual void            SetSizeConstraints(const SizeConstraints& aConstraints);
    NS_IMETHOD              Move(double aX, double aY);
    NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                        nsIWidget *aWidget, bool aActivate);
    NS_IMETHOD              SetSizeMode(int32_t aMode);
    NS_IMETHOD              HideWindowChrome(bool aShouldHide);
    void                    EnteredFullScreen(bool aFullScreen);
    NS_IMETHOD              MakeFullScreen(bool aFullScreen);
    NS_IMETHOD              Resize(double aWidth, double aHeight, bool aRepaint);
    NS_IMETHOD              Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint);
    NS_IMETHOD              GetClientBounds(nsIntRect &aRect);
    NS_IMETHOD              GetScreenBounds(nsIntRect &aRect);
    void                    ReportMoveEvent();
    void                    ReportSizeEvent();
    NS_IMETHOD              SetCursor(nsCursor aCursor);
    NS_IMETHOD              SetCursor(imgIContainer* aCursor, uint32_t aHotspotX, uint32_t aHotspotY);

    CGFloat                 BackingScaleFactor();
    void                    BackingScaleFactorChanged();
    virtual double          GetDefaultScaleInternal();
    virtual int32_t         RoundsWidgetCoordinatesTo() MOZ_OVERRIDE;

    NS_IMETHOD              SetTitle(const nsAString& aTitle);

    NS_IMETHOD Invalidate(const nsIntRect &aRect);
    virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
    virtual LayerManager* GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                          LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                          bool* aAllowRetaining = nullptr);
    NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                             nsEventStatus& aStatus);
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener * aListener, bool aDoCapture);
    NS_IMETHOD GetAttention(int32_t aCycleCount);
    virtual bool HasPendingInputEvent();
    virtual nsTransparencyMode GetTransparencyMode();
    virtual void SetTransparencyMode(nsTransparencyMode aMode);
    NS_IMETHOD SetWindowShadowStyle(int32_t aStyle);
    virtual void SetShowsToolbarButton(bool aShow);
    virtual void SetShowsFullScreenButton(bool aShow);
    virtual void SetWindowAnimationType(WindowAnimationType aType);
    virtual void SetDrawsTitle(bool aDrawTitle);
    NS_IMETHOD SetNonClientMargins(nsIntMargin &margins);
    NS_IMETHOD SetWindowTitlebarColor(nscolor aColor, bool aActive);
    virtual void SetDrawsInTitlebar(bool aState);
    virtual nsresult SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                                uint32_t aNativeMessage,
                                                uint32_t aModifierFlags);

    void DispatchSizeModeEvent();

    // be notified that a some form of drag event needs to go into Gecko
    virtual bool DragEvent(unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers);

    bool HasModalDescendents() { return mNumModalDescendents > 0; }
    NSWindow *GetCocoaWindow() { return mWindow; }

    void SetMenuBar(nsMenuBarX* aMenuBar);
    nsMenuBarX *GetMenuBar();

    NS_IMETHOD NotifyIME(const IMENotification& aIMENotification) MOZ_OVERRIDE;
    NS_IMETHOD_(void) SetInputContext(
                        const InputContext& aContext,
                        const InputContextAction& aAction) MOZ_OVERRIDE;
    NS_IMETHOD_(InputContext) GetInputContext()
    {
      NSView* view = mWindow ? [mWindow contentView] : nil;
      if (view) {
        mInputContext.mNativeIMEContext = [view inputContext];
      }
      // If inputContext isn't available on this window, returns this window's
      // pointer since nullptr means the platform has only one context per
      // process.
      if (!mInputContext.mNativeIMEContext) {
        mInputContext.mNativeIMEContext = this;
      }
      return mInputContext;
    }
    NS_IMETHOD_(bool) ExecuteNativeKeyBinding(
                        NativeKeyBindingsType aType,
                        const mozilla::WidgetKeyboardEvent& aEvent,
                        DoCommandCallback aCallback,
                        void* aCallbackData) MOZ_OVERRIDE;

    void SetPopupWindowLevel();

    NS_IMETHOD         ReparentNativeWidget(nsIWidget* aNewParent);
protected:
  virtual ~nsCocoaWindow();

  nsresult             CreateNativeWindow(const NSRect &aRect,
                                          nsBorderStyle aBorderStyle,
                                          bool aRectIsFrameRect);
  nsresult             CreatePopupContentView(const nsIntRect &aRect,
                                              nsDeviceContext *aContext);
  void                 DestroyNativeWindow();
  void                 AdjustWindowShadow();
  void                 SetWindowBackgroundBlur();
  void                 UpdateBounds();

  nsresult             DoResize(double aX, double aY, double aWidth, double aHeight,
                                bool aRepaint, bool aConstrainToCurrentScreen);

  virtual already_AddRefed<nsIWidget>
  AllocateChildPopupWidget()
  {
    static NS_DEFINE_IID(kCPopUpCID, NS_POPUP_CID);
    nsCOMPtr<nsIWidget> widget = do_CreateInstance(kCPopUpCID);
    return widget.forget();
  }

  nsIWidget*           mParent;         // if we're a popup, this is our parent [WEAK]
  BaseWindow*          mWindow;         // our cocoa window [STRONG]
  WindowDelegate*      mDelegate;       // our delegate for processing window msgs [STRONG]
  nsRefPtr<nsMenuBarX> mMenuBar;
  NSWindow*            mSheetWindowParent; // if this is a sheet, this is the NSWindow it's attached to
  nsChildView*         mPopupContentView; // if this is a popup, this is its content widget
  int32_t              mShadowStyle;

  CGFloat              mBackingScaleFactor;

  WindowAnimationType  mAnimationType;

  bool                 mWindowMadeHere; // true if we created the window, false for embedding
  bool                 mSheetNeedsShow; // if this is a sheet, are we waiting to be shown?
                                        // this is used for sibling sheet contention only
  bool                 mFullScreen;
  bool                 mInFullScreenTransition; // true from the request to enter/exit fullscreen
                                                // (MakeFullScreen() call) to EnteredFullScreen()
  bool                 mModal;

  bool                 mUsesNativeFullScreen; // only true on Lion if SetShowsFullScreenButton(true);

  bool                 mIsAnimationSuppressed;

  bool                 mInReportMoveEvent; // true if in a call to ReportMoveEvent().
  bool                 mInResize; // true if in a call to DoResize().

  int32_t              mNumModalDescendents;
  InputContext         mInputContext;
};

#endif // nsCocoaWindow_h_
