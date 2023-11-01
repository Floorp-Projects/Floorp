/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeCocoa.h"
#include <objc/NSObjCRuntime.h>

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/PathHelpers.h"
#include "nsChildView.h"
#include "nsDeviceContext.h"
#include "nsLayoutUtils.h"
#include "nsObjCExceptions.h"
#include "nsNumberControlFrame.h"
#include "nsRangeFrame.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsIFrame.h"
#include "nsAtom.h"
#include "nsNameSpaceManager.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsCocoaFeatures.h"
#include "nsCocoaWindow.h"
#include "nsNativeThemeColors.h"
#include "nsIScrollableFrame.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Range.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLMeterElement.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsLookAndFeel.h"
#include "MacThemeGeometryType.h"
#include "VibrancyManager.h"

#include "gfxContext.h"
#include "gfxQuartzSurface.h"
#include "gfxQuartzNativeDrawing.h"
#include "gfxUtils.h"  // for ToDeviceColor
#include <algorithm>

using namespace mozilla;
using namespace mozilla::gfx;
using mozilla::dom::HTMLMeterElement;

#define DRAW_IN_FRAME_DEBUG 0
#define SCROLLBARS_VISUAL_DEBUG 0

// private Quartz routines needed here
extern "C" {
CG_EXTERN void CGContextSetCTM(CGContextRef, CGAffineTransform);
CG_EXTERN void CGContextSetBaseCTM(CGContextRef, CGAffineTransform);
typedef CFTypeRef CUIRendererRef;
void CUIDraw(CUIRendererRef r, CGRect rect, CGContextRef ctx,
             CFDictionaryRef options, CFDictionaryRef* result);
}

// Workaround for NSCell control tint drawing
// Without this workaround, NSCells are always drawn with the clear control tint
// as long as they're not attached to an NSControl which is a subview of an
// active window.
// XXXmstange Why doesn't Webkit need this?
@implementation NSCell (ControlTintWorkaround)
- (int)_realControlTint {
  return [self controlTint];
}
@end

// This is the window for our MOZCellDrawView. When an NSCell is drawn, some
// NSCell implementations look at the draw view's window to determine whether
// the cell should draw with the active look.
@interface MOZCellDrawWindow : NSWindow
@property BOOL cellsShouldLookActive;
@end

@implementation MOZCellDrawWindow

// Override three different methods, for good measure. The NSCell implementation
// could call any one of them.
- (BOOL)_hasActiveAppearance {
  return self.cellsShouldLookActive;
}
- (BOOL)hasKeyAppearance {
  return self.cellsShouldLookActive;
}
- (BOOL)_hasKeyAppearance {
  return self.cellsShouldLookActive;
}

@end

// The purpose of this class is to provide objects that can be used when drawing
// NSCells using drawWithFrame:inView: without causing any harm. Only a small
// number of methods are called on the draw view, among those "isFlipped" and
// "currentEditor": isFlipped needs to return YES in order to avoid drawing bugs
// on 10.4 (see bug 465069); currentEditor (which isn't even a method of
// NSView) will be called when drawing search fields, and we only provide it in
// order to prevent "unrecognized selector" exceptions.
// There's no need to pass the actual NSView that we're drawing into to
// drawWithFrame:inView:. What's more, doing so even causes unnecessary
// invalidations as soon as we draw a focusring!
// This class needs to be an NSControl so that NSTextFieldCell (and
// NSSearchFieldCell, which is a subclass of NSTextFieldCell) draws a focus
// ring.
@interface MOZCellDrawView : NSControl
// Called by NSTreeHeaderCell during drawing.
@property BOOL _drawingEndSeparator;
@end

@implementation MOZCellDrawView

- (BOOL)isFlipped {
  return YES;
}

- (NSText*)currentEditor {
  return nil;
}

@end

static void DrawFocusRingForCellIfNeeded(NSCell* aCell, NSRect aWithFrame,
                                         NSView* aInView) {
  if ([aCell showsFirstResponder]) {
    CGContextRef cgContext = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(cgContext);

    // It's important to set the focus ring style before we enter the
    // transparency layer so that the transparency layer only contains
    // the normal button mask without the focus ring, and the conversion
    // to the focus ring shape happens only when the transparency layer is
    // ended.
    NSSetFocusRingStyle(NSFocusRingOnly);

    // We need to draw the whole button into a transparency layer because
    // many button types are composed of multiple parts, and if these parts
    // were drawn while the focus ring style was active, each individual part
    // would produce a focus ring for itself. But we only want one focus ring
    // for the whole button. The transparency layer is a way to merge the
    // individual button parts together before the focus ring shape is
    // calculated.
    CGContextBeginTransparencyLayerWithRect(cgContext,
                                            NSRectToCGRect(aWithFrame), 0);
    [aCell drawFocusRingMaskWithFrame:aWithFrame inView:aInView];
    CGContextEndTransparencyLayer(cgContext);

    CGContextRestoreGState(cgContext);
  }
}

static void DrawCellIncludingFocusRing(NSCell* aCell, NSRect aWithFrame,
                                       NSView* aInView) {
  [aCell drawWithFrame:aWithFrame inView:aInView];
  DrawFocusRingForCellIfNeeded(aCell, aWithFrame, aInView);
}

/**
 * NSProgressBarCell is used to draw progress bars of any size.
 */
@interface NSProgressBarCell : NSCell {
  /*All instance variables are private*/
  double mValue;
  double mMax;
  bool mIsIndeterminate;
  bool mIsHorizontal;
}

- (void)setValue:(double)value;
- (double)value;
- (void)setMax:(double)max;
- (double)max;
- (void)setIndeterminate:(bool)aIndeterminate;
- (bool)isIndeterminate;
- (void)setHorizontal:(bool)aIsHorizontal;
- (bool)isHorizontal;
- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView;
@end

@implementation NSProgressBarCell

- (void)setMax:(double)aMax {
  mMax = aMax;
}

- (double)max {
  return mMax;
}

- (void)setValue:(double)aValue {
  mValue = aValue;
}

- (double)value {
  return mValue;
}

- (void)setIndeterminate:(bool)aIndeterminate {
  mIsIndeterminate = aIndeterminate;
}

- (bool)isIndeterminate {
  return mIsIndeterminate;
}

- (void)setHorizontal:(bool)aIsHorizontal {
  mIsHorizontal = aIsHorizontal;
}

- (bool)isHorizontal {
  return mIsHorizontal;
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  CGContext* cgContext = [[NSGraphicsContext currentContext] CGContext];

  HIThemeTrackDrawInfo tdi;

  tdi.version = 0;
  tdi.min = 0;

  tdi.value = INT32_MAX * (mValue / mMax);
  tdi.max = INT32_MAX;
  tdi.bounds = NSRectToCGRect(cellFrame);
  tdi.attributes = mIsHorizontal ? kThemeTrackHorizontal : 0;
  tdi.enableState = [self controlTint] == NSClearControlTint
                        ? kThemeTrackInactive
                        : kThemeTrackActive;

  NSControlSize size = [self controlSize];
  if (size == NSControlSizeRegular) {
    tdi.kind =
        mIsIndeterminate ? kThemeLargeIndeterminateBar : kThemeLargeProgressBar;
  } else {
    NS_ASSERTION(
        size == NSControlSizeSmall,
        "We shouldn't have another size than small and regular for the moment");
    tdi.kind = mIsIndeterminate ? kThemeMediumIndeterminateBar
                                : kThemeMediumProgressBar;
  }

  int32_t stepsPerSecond = mIsIndeterminate ? 60 : 30;
  int32_t milliSecondsPerStep = 1000 / stepsPerSecond;
  tdi.trackInfo.progress.phase = uint8_t(
      PR_IntervalToMilliseconds(PR_IntervalNow()) / milliSecondsPerStep);

  HIThemeDrawTrack(&tdi, NULL, cgContext, kHIThemeOrientationNormal);
}

@end

@interface MOZSearchFieldCell : NSSearchFieldCell
@property BOOL shouldUseToolbarStyle;
@end

@implementation MOZSearchFieldCell

- (instancetype)init {
  // We would like to render a search field which has the magnifying glass icon
  // at the start of the search field, and no cancel button. On 10.12 and 10.13,
  // empty search fields render the magnifying glass icon in the middle of the
  // field. So in order to get the icon to show at the start of the field, we
  // need to give the field some content. We achieve this with a single space
  // character.
  self = [super initTextCell:@" "];

  // However, because the field is now non-empty, by default it shows a cancel
  // button. To hide the cancel button, override it with a custom NSButtonCell
  // which renders nothing.
  NSButtonCell* invisibleCell = [[NSButtonCell alloc] initImageCell:nil];
  invisibleCell.bezeled = NO;
  invisibleCell.bordered = NO;
  self.cancelButtonCell = invisibleCell;
  [invisibleCell release];

  return self;
}

- (BOOL)_isToolbarMode {
  return self.shouldUseToolbarStyle;
}

@end

#define HITHEME_ORIENTATION kHIThemeOrientationNormal

static CGFloat kMaxFocusRingWidth =
    0;  // initialized by the nsNativeThemeCocoa constructor

// These enums are for indexing into the margin array.
enum {
  leopardOSorlater = 0,  // 10.6 - 10.9
  yosemiteOSorlater = 1  // 10.10+
};

enum { miniControlSize, smallControlSize, regularControlSize };

enum { leftMargin, topMargin, rightMargin, bottomMargin };

static size_t EnumSizeForCocoaSize(NSControlSize cocoaControlSize) {
  if (cocoaControlSize == NSControlSizeMini)
    return miniControlSize;
  else if (cocoaControlSize == NSControlSizeSmall)
    return smallControlSize;
  else
    return regularControlSize;
}

static NSControlSize CocoaSizeForEnum(int32_t enumControlSize) {
  if (enumControlSize == miniControlSize)
    return NSControlSizeMini;
  else if (enumControlSize == smallControlSize)
    return NSControlSizeSmall;
  else
    return NSControlSizeRegular;
}

static NSString* CUIControlSizeForCocoaSize(NSControlSize aControlSize) {
  if (aControlSize == NSControlSizeRegular)
    return @"regular";
  else if (aControlSize == NSControlSizeSmall)
    return @"small";
  else
    return @"mini";
}

static void InflateControlRect(NSRect* rect, NSControlSize cocoaControlSize,
                               const float marginSet[][3][4]) {
  if (!marginSet) return;

  static int osIndex = yosemiteOSorlater;
  size_t controlSize = EnumSizeForCocoaSize(cocoaControlSize);
  const float* buttonMargins = marginSet[osIndex][controlSize];
  rect->origin.x -= buttonMargins[leftMargin];
  rect->origin.y -= buttonMargins[bottomMargin];
  rect->size.width += buttonMargins[leftMargin] + buttonMargins[rightMargin];
  rect->size.height += buttonMargins[bottomMargin] + buttonMargins[topMargin];
}

static NSWindow* NativeWindowForFrame(nsIFrame* aFrame,
                                      nsIWidget** aTopLevelWidget = NULL) {
  if (!aFrame) return nil;

  nsIWidget* widget = aFrame->GetNearestWidget();
  if (!widget) return nil;

  nsIWidget* topLevelWidget = widget->GetTopLevelWidget();
  if (aTopLevelWidget) *aTopLevelWidget = topLevelWidget;

  return (NSWindow*)topLevelWidget->GetNativeData(NS_NATIVE_WINDOW);
}

static NSSize WindowButtonsSize(nsIFrame* aFrame) {
  NSWindow* window = NativeWindowForFrame(aFrame);
  if (!window) {
    // Return fallback values.
    return NSMakeSize(54, 16);
  }

  NSRect buttonBox = NSZeroRect;
  NSButton* closeButton = [window standardWindowButton:NSWindowCloseButton];
  if (closeButton) {
    buttonBox = NSUnionRect(buttonBox, [closeButton frame]);
  }
  NSButton* minimizeButton =
      [window standardWindowButton:NSWindowMiniaturizeButton];
  if (minimizeButton) {
    buttonBox = NSUnionRect(buttonBox, [minimizeButton frame]);
  }
  NSButton* zoomButton = [window standardWindowButton:NSWindowZoomButton];
  if (zoomButton) {
    buttonBox = NSUnionRect(buttonBox, [zoomButton frame]);
  }
  return buttonBox.size;
}

static BOOL FrameIsInActiveWindow(nsIFrame* aFrame) {
  nsIWidget* topLevelWidget = NULL;
  NSWindow* win = NativeWindowForFrame(aFrame, &topLevelWidget);
  if (!topLevelWidget || !win) return YES;

  // XUL popups, e.g. the toolbar customization popup, can't become key windows,
  // but controls in these windows should still get the active look.
  if (topLevelWidget->GetWindowType() == widget::WindowType::Popup) {
    return YES;
  }
  if ([win isSheet]) {
    return [win isKeyWindow];
  }
  return [win isMainWindow] && ![win attachedSheet];
}

// Toolbar controls and content controls respond to different window
// activeness states.
static BOOL IsActive(nsIFrame* aFrame, BOOL aIsToolbarControl) {
  if (aIsToolbarControl) return [NativeWindowForFrame(aFrame) isMainWindow];
  return FrameIsInActiveWindow(aFrame);
}

NS_IMPL_ISUPPORTS_INHERITED(nsNativeThemeCocoa, nsNativeTheme, nsITheme)

nsNativeThemeCocoa::nsNativeThemeCocoa() : ThemeCocoa(ScrollbarStyle()) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  kMaxFocusRingWidth = 7;

  // provide a local autorelease pool, as this is called during startup
  // before the main event-loop pool is in place
  nsAutoreleasePool pool;

  mDisclosureButtonCell = [[NSButtonCell alloc] initTextCell:@""];
  [mDisclosureButtonCell setBezelStyle:NSBezelStyleRoundedDisclosure];
  [mDisclosureButtonCell setButtonType:NSButtonTypePushOnPushOff];
  [mDisclosureButtonCell setHighlightsBy:NSPushInCellMask];

  mHelpButtonCell = [[NSButtonCell alloc] initTextCell:@""];
  [mHelpButtonCell setBezelStyle:NSBezelStyleHelpButton];
  [mHelpButtonCell setButtonType:NSButtonTypeMomentaryPushIn];
  [mHelpButtonCell setHighlightsBy:NSPushInCellMask];

  mPushButtonCell = [[NSButtonCell alloc] initTextCell:@""];
  [mPushButtonCell setButtonType:NSButtonTypeMomentaryPushIn];
  [mPushButtonCell setHighlightsBy:NSPushInCellMask];

  mRadioButtonCell = [[NSButtonCell alloc] initTextCell:@""];
  [mRadioButtonCell setButtonType:NSButtonTypeRadio];

  mCheckboxCell = [[NSButtonCell alloc] initTextCell:@""];
  [mCheckboxCell setButtonType:NSButtonTypeSwitch];
  [mCheckboxCell setAllowsMixedState:YES];

  mTextFieldCell = [[NSTextFieldCell alloc] initTextCell:@""];
  [mTextFieldCell setBezeled:YES];
  [mTextFieldCell setEditable:YES];
  [mTextFieldCell setFocusRingType:NSFocusRingTypeExterior];

  mSearchFieldCell = [[MOZSearchFieldCell alloc] init];
  [mSearchFieldCell setBezelStyle:NSTextFieldRoundedBezel];
  [mSearchFieldCell setBezeled:YES];
  [mSearchFieldCell setEditable:YES];
  [mSearchFieldCell setFocusRingType:NSFocusRingTypeExterior];

  mDropdownCell = [[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO];

  mComboBoxCell = [[NSComboBoxCell alloc] initTextCell:@""];
  [mComboBoxCell setBezeled:YES];
  [mComboBoxCell setEditable:YES];
  [mComboBoxCell setFocusRingType:NSFocusRingTypeExterior];

  mProgressBarCell = [[NSProgressBarCell alloc] init];

  mMeterBarCell = [[NSLevelIndicatorCell alloc]
      initWithLevelIndicatorStyle:NSLevelIndicatorStyleContinuousCapacity];

  mTreeHeaderCell = [[NSTableHeaderCell alloc] init];

  mCellDrawView = [[MOZCellDrawView alloc] init];

  if (XRE_IsParentProcess()) {
    // Put the cell draw view into a window that is never shown.
    // This allows us to convince some NSCell implementations (such as
    // NSButtonCell for default buttons) to draw with the active appearance.
    // Another benefit of putting the draw view in a window is the fact that it
    // lets NSTextFieldCell (and its subclass NSSearchFieldCell) inherit the
    // current NSApplication effectiveAppearance automatically, so the field
    // adapts to Dark Mode correctly. We don't create this window when the
    // native theme is used in the content process because NSWindow creation
    // runs into the sandbox and because we never run default buttons in content
    // processes anyway.
    mCellDrawWindow = [[MOZCellDrawWindow alloc]
        initWithContentRect:NSZeroRect
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [mCellDrawWindow.contentView addSubview:mCellDrawView];
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

nsNativeThemeCocoa::~nsNativeThemeCocoa() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mMeterBarCell release];
  [mProgressBarCell release];
  [mDisclosureButtonCell release];
  [mHelpButtonCell release];
  [mPushButtonCell release];
  [mRadioButtonCell release];
  [mCheckboxCell release];
  [mTextFieldCell release];
  [mSearchFieldCell release];
  [mDropdownCell release];
  [mComboBoxCell release];
  [mTreeHeaderCell release];
  [mCellDrawWindow release];
  [mCellDrawView release];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Limit on the area of the target rect (in pixels^2) in
// DrawCellWithScaling() and DrawButton() and above which we
// don't draw the object into a bitmap buffer.  This is to avoid crashes in
// [NSGraphicsContext graphicsContextWithCGContext:flipped:] and
// CGContextDrawImage(), and also to avoid very poor drawing performance in
// CGContextDrawImage() when it scales the bitmap (particularly if xscale or
// yscale is less than but near 1 -- e.g. 0.9).  This value was determined
// by trial and error, on OS X 10.4.11 and 10.5.4, and on systems with
// different amounts of RAM.
#define BITMAP_MAX_AREA 500000

static int GetBackingScaleFactorForRendering(CGContextRef cgContext) {
  CGAffineTransform ctm =
      CGContextGetUserSpaceToDeviceSpaceTransform(cgContext);
  CGRect transformedUserSpacePixel =
      CGRectApplyAffineTransform(CGRectMake(0, 0, 1, 1), ctm);
  float maxScale = std::max(fabs(transformedUserSpacePixel.size.width),
                            fabs(transformedUserSpacePixel.size.height));
  return maxScale > 1.0 ? 2 : 1;
}

/*
 * Draw the given NSCell into the given cgContext.
 *
 * destRect - the size and position of the resulting control rectangle
 * controlSize - the NSControlSize which will be given to the NSCell before
 *  asking it to render
 * naturalSize - The natural dimensions of this control.
 *  If the control rect size is not equal to either of these, a scale
 *  will be applied to the context so that rendering the control at the
 *  natural size will result in it filling the destRect space.
 *  If a control has no natural dimensions in either/both axes, pass 0.0f.
 * minimumSize - The minimum dimensions of this control.
 *  If the control rect size is less than the minimum for a given axis,
 *  a scale will be applied to the context so that the minimum is used
 *  for drawing.  If a control has no minimum dimensions in either/both
 *  axes, pass 0.0f.
 * marginSet - an array of margins; a multidimensional array of [2][3][4],
 *  with the first dimension being the OS version (Tiger or Leopard),
 *  the second being the control size (mini, small, regular), and the third
 *  being the 4 margin values (left, top, right, bottom).
 * view - The NSView that we're drawing into. As far as I can tell, it doesn't
 *  matter if this is really the right view; it just has to return YES when
 *  asked for isFlipped. Otherwise we'll get drawing bugs on 10.4.
 * mirrorHorizontal - whether to mirror the cell horizontally
 */
static void DrawCellWithScaling(NSCell* cell, CGContextRef cgContext,
                                const HIRect& destRect,
                                NSControlSize controlSize, NSSize naturalSize,
                                NSSize minimumSize,
                                const float marginSet[][3][4], NSView* view,
                                BOOL mirrorHorizontal) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NSRect drawRect = NSMakeRect(destRect.origin.x, destRect.origin.y,
                               destRect.size.width, destRect.size.height);

  if (naturalSize.width != 0.0f) drawRect.size.width = naturalSize.width;
  if (naturalSize.height != 0.0f) drawRect.size.height = naturalSize.height;

  // Keep aspect ratio when scaling if one dimension is free.
  if (naturalSize.width == 0.0f && naturalSize.height != 0.0f)
    drawRect.size.width =
        destRect.size.width * naturalSize.height / destRect.size.height;
  if (naturalSize.height == 0.0f && naturalSize.width != 0.0f)
    drawRect.size.height =
        destRect.size.height * naturalSize.width / destRect.size.width;

  // Honor minimum sizes.
  if (drawRect.size.width < minimumSize.width)
    drawRect.size.width = minimumSize.width;
  if (drawRect.size.height < minimumSize.height)
    drawRect.size.height = minimumSize.height;

  [NSGraphicsContext saveGraphicsState];

  // Only skip the buffer if the area of our cell (in pixels^2) is too large.
  if (drawRect.size.width * drawRect.size.height > BITMAP_MAX_AREA) {
    // Inflate the rect Gecko gave us by the margin for the control.
    InflateControlRect(&drawRect, controlSize, marginSet);

    NSGraphicsContext* savedContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext
        setCurrentContext:[NSGraphicsContext
                              graphicsContextWithCGContext:cgContext
                                                   flipped:YES]];

    DrawCellIncludingFocusRing(cell, drawRect, view);

    [NSGraphicsContext setCurrentContext:savedContext];
  } else {
    float w = ceil(drawRect.size.width);
    float h = ceil(drawRect.size.height);
    NSRect tmpRect = NSMakeRect(kMaxFocusRingWidth, kMaxFocusRingWidth, w, h);

    // inflate to figure out the frame we need to tell NSCell to draw in, to get
    // something that's 0,0,w,h
    InflateControlRect(&tmpRect, controlSize, marginSet);

    // and then, expand by kMaxFocusRingWidth size to make sure we can capture
    // any focus ring
    w += kMaxFocusRingWidth * 2.0;
    h += kMaxFocusRingWidth * 2.0;

    int backingScaleFactor = GetBackingScaleFactorForRendering(cgContext);
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        NULL, (int)w * backingScaleFactor, (int)h * backingScaleFactor, 8,
        (int)w * backingScaleFactor * 4, rgb, kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(rgb);

    // We need to flip the image twice in order to avoid drawing bugs on 10.4,
    // see bug 465069. This is the first flip transform, applied to cgContext.
    CGContextScaleCTM(cgContext, 1.0f, -1.0f);
    CGContextTranslateCTM(cgContext, 0.0f,
                          -(2.0 * destRect.origin.y + destRect.size.height));
    if (mirrorHorizontal) {
      CGContextScaleCTM(cgContext, -1.0f, 1.0f);
      CGContextTranslateCTM(
          cgContext, -(2.0 * destRect.origin.x + destRect.size.width), 0.0f);
    }

    NSGraphicsContext* savedContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext
        setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:ctx
                                                                  flipped:YES]];

    CGContextScaleCTM(ctx, backingScaleFactor, backingScaleFactor);

    // Set the context's "base transform" to in order to get correctly-sized
    // focus rings.
    CGContextSetBaseCTM(ctx, CGAffineTransformMakeScale(backingScaleFactor,
                                                        backingScaleFactor));

    // This is the second flip transform, applied to ctx.
    CGContextScaleCTM(ctx, 1.0f, -1.0f);
    CGContextTranslateCTM(ctx, 0.0f,
                          -(2.0 * tmpRect.origin.y + tmpRect.size.height));

    DrawCellIncludingFocusRing(cell, tmpRect, view);

    [NSGraphicsContext setCurrentContext:savedContext];

    CGImageRef img = CGBitmapContextCreateImage(ctx);

    // Drop the image into the original destination rectangle, scaling to fit
    // Only scale kMaxFocusRingWidth by xscale/yscale when the resulting rect
    // doesn't extend beyond the overflow rect
    float xscale = destRect.size.width / drawRect.size.width;
    float yscale = destRect.size.height / drawRect.size.height;
    float scaledFocusRingX =
        xscale < 1.0f ? kMaxFocusRingWidth * xscale : kMaxFocusRingWidth;
    float scaledFocusRingY =
        yscale < 1.0f ? kMaxFocusRingWidth * yscale : kMaxFocusRingWidth;
    CGContextDrawImage(cgContext,
                       CGRectMake(destRect.origin.x - scaledFocusRingX,
                                  destRect.origin.y - scaledFocusRingY,
                                  destRect.size.width + scaledFocusRingX * 2,
                                  destRect.size.height + scaledFocusRingY * 2),
                       img);

    CGImageRelease(img);
    CGContextRelease(ctx);
  }

  [NSGraphicsContext restoreGraphicsState];

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, destRect);
#endif

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

struct CellRenderSettings {
  // The natural dimensions of the control.
  // If a control has no natural dimensions in either/both axes, set to 0.0f.
  NSSize naturalSizes[3];

  // The minimum dimensions of the control.
  // If a control has no minimum dimensions in either/both axes, set to 0.0f.
  NSSize minimumSizes[3];

  // A three-dimensional array,
  // with the first dimension being the OS version ([0] 10.6-10.9, [1] 10.10 and
  // above), the second being the control size (mini, small, regular), and the
  // third being the 4 margin values (left, top, right, bottom).
  float margins[2][3][4];
};

/*
 * This is a helper method that returns the required NSControlSize given a size
 * and the size of the three controls plus a tolerance.
 * size - The width or the height of the element to draw.
 * sizes - An array with the all the width/height of the element for its
 *         different sizes.
 * tolerance - The tolerance as passed to DrawCellWithSnapping.
 * NOTE: returns NSControlSizeRegular if all values in 'sizes' are zero.
 */
static NSControlSize FindControlSize(CGFloat size, const CGFloat* sizes,
                                     CGFloat tolerance) {
  for (uint32_t i = miniControlSize; i <= regularControlSize; ++i) {
    if (sizes[i] == 0) {
      continue;
    }

    CGFloat next = 0;
    // Find next value.
    for (uint32_t j = i + 1; j <= regularControlSize; ++j) {
      if (sizes[j] != 0) {
        next = sizes[j];
        break;
      }
    }

    // If it's the latest value, we pick it.
    if (next == 0) {
      return CocoaSizeForEnum(i);
    }

    if (size <= sizes[i] + tolerance && size < next) {
      return CocoaSizeForEnum(i);
    }
  }

  // If we are here, that means sizes[] was an array with only empty values
  // or the algorithm above is wrong.
  // The former can happen but the later would be wrong.
  NS_ASSERTION(sizes[0] == 0 && sizes[1] == 0 && sizes[2] == 0,
               "We found no control! We shouldn't be there!");
  return CocoaSizeForEnum(regularControlSize);
}

/*
 * Draw the given NSCell into the given cgContext with a nice control size.
 *
 * This function is similar to DrawCellWithScaling, but it decides what
 * control size to use based on the destRect's size.
 * Scaling is only applied when the difference between the destRect's size
 * and the next smaller natural size is greater than snapTolerance. Otherwise
 * it snaps to the next smaller control size without scaling because unscaled
 * controls look nicer.
 */
static void DrawCellWithSnapping(NSCell* cell, CGContextRef cgContext,
                                 const HIRect& destRect,
                                 const CellRenderSettings settings,
                                 float verticalAlignFactor, NSView* view,
                                 BOOL mirrorHorizontal,
                                 float snapTolerance = 2.0f) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  const float rectWidth = destRect.size.width,
              rectHeight = destRect.size.height;
  const NSSize* sizes = settings.naturalSizes;
  const NSSize miniSize = sizes[EnumSizeForCocoaSize(NSControlSizeMini)];
  const NSSize smallSize = sizes[EnumSizeForCocoaSize(NSControlSizeSmall)];
  const NSSize regularSize = sizes[EnumSizeForCocoaSize(NSControlSizeRegular)];

  HIRect drawRect = destRect;

  CGFloat controlWidths[3] = {miniSize.width, smallSize.width,
                              regularSize.width};
  NSControlSize controlSizeX =
      FindControlSize(rectWidth, controlWidths, snapTolerance);
  CGFloat controlHeights[3] = {miniSize.height, smallSize.height,
                               regularSize.height};
  NSControlSize controlSizeY =
      FindControlSize(rectHeight, controlHeights, snapTolerance);

  NSControlSize controlSize = NSControlSizeRegular;
  size_t sizeIndex = 0;

  // At some sizes, don't scale but snap.
  const NSControlSize smallerControlSize =
      EnumSizeForCocoaSize(controlSizeX) < EnumSizeForCocoaSize(controlSizeY)
          ? controlSizeX
          : controlSizeY;
  const size_t smallerControlSizeIndex =
      EnumSizeForCocoaSize(smallerControlSize);
  const NSSize size = sizes[smallerControlSizeIndex];
  float diffWidth = size.width ? rectWidth - size.width : 0.0f;
  float diffHeight = size.height ? rectHeight - size.height : 0.0f;
  if (diffWidth >= 0.0f && diffHeight >= 0.0f && diffWidth <= snapTolerance &&
      diffHeight <= snapTolerance) {
    // Snap to the smaller control size.
    controlSize = smallerControlSize;
    sizeIndex = smallerControlSizeIndex;
    MOZ_ASSERT(sizeIndex < ArrayLength(settings.naturalSizes));

    // Resize and center the drawRect.
    if (sizes[sizeIndex].width) {
      drawRect.origin.x +=
          ceil((destRect.size.width - sizes[sizeIndex].width) / 2);
      drawRect.size.width = sizes[sizeIndex].width;
    }
    if (sizes[sizeIndex].height) {
      drawRect.origin.y +=
          floor((destRect.size.height - sizes[sizeIndex].height) *
                verticalAlignFactor);
      drawRect.size.height = sizes[sizeIndex].height;
    }
  } else {
    // Use the larger control size.
    controlSize =
        EnumSizeForCocoaSize(controlSizeX) > EnumSizeForCocoaSize(controlSizeY)
            ? controlSizeX
            : controlSizeY;
    sizeIndex = EnumSizeForCocoaSize(controlSize);
  }

  [cell setControlSize:controlSize];

  MOZ_ASSERT(sizeIndex < ArrayLength(settings.minimumSizes));
  const NSSize minimumSize = settings.minimumSizes[sizeIndex];
  DrawCellWithScaling(cell, cgContext, drawRect, controlSize, sizes[sizeIndex],
                      minimumSize, settings.margins, view, mirrorHorizontal);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

@interface NSWindow (CoreUIRendererPrivate)
+ (CUIRendererRef)coreUIRenderer;
@end

@interface NSObject (NSAppearanceCoreUIRendering)
- (void)_drawInRect:(CGRect)rect
            context:(CGContextRef)cgContext
            options:(id)options;
@end

static void RenderWithCoreUI(CGRect aRect, CGContextRef cgContext,
                             NSDictionary* aOptions,
                             bool aSkipAreaCheck = false) {
  if (!aSkipAreaCheck &&
      aRect.size.width * aRect.size.height > BITMAP_MAX_AREA) {
    return;
  }

  NSAppearance* appearance = NSAppearance.currentAppearance;
  if (appearance &&
      [appearance respondsToSelector:@selector(_drawInRect:context:options:)]) {
    // Render through NSAppearance on Mac OS 10.10 and up. This will call
    // CUIDraw with a CoreUI renderer that will give us the correct 10.10
    // style. Calling CUIDraw directly with [NSWindow coreUIRenderer] still
    // renders 10.9-style widgets on 10.10.
    [appearance _drawInRect:aRect context:cgContext options:aOptions];
  } else {
    // 10.9 and below
    CUIRendererRef renderer =
        [NSWindow respondsToSelector:@selector(coreUIRenderer)]
            ? [NSWindow coreUIRenderer]
            : nil;
    CUIDraw(renderer, aRect, cgContext, (CFDictionaryRef)aOptions, NULL);
  }
}

static float VerticalAlignFactor(nsIFrame* aFrame) {
  if (!aFrame) return 0.5f;  // default: center

  const auto& va = aFrame->StyleDisplay()->mVerticalAlign;
  auto kw = va.IsKeyword() ? va.AsKeyword() : StyleVerticalAlignKeyword::Middle;
  switch (kw) {
    case StyleVerticalAlignKeyword::Top:
    case StyleVerticalAlignKeyword::TextTop:
      return 0.0f;

    case StyleVerticalAlignKeyword::Sub:
    case StyleVerticalAlignKeyword::Super:
    case StyleVerticalAlignKeyword::Middle:
    case StyleVerticalAlignKeyword::MozMiddleWithBaseline:
      return 0.5f;

    case StyleVerticalAlignKeyword::Baseline:
    case StyleVerticalAlignKeyword::Bottom:
    case StyleVerticalAlignKeyword::TextBottom:
      return 1.0f;

    default:
      MOZ_ASSERT_UNREACHABLE("invalid vertical-align");
      return 0.5f;
  }
}

static void ApplyControlParamsToNSCell(
    nsNativeThemeCocoa::ControlParams aControlParams, NSCell* aCell) {
  [aCell setEnabled:!aControlParams.disabled];
  [aCell setShowsFirstResponder:(aControlParams.focused &&
                                 !aControlParams.disabled &&
                                 aControlParams.insideActiveWindow)];
  [aCell setHighlighted:aControlParams.pressed];
}

// These are the sizes that Gecko needs to request to draw if it wants
// to get a standard-sized Aqua radio button drawn. Note that the rects
// that draw these are actually a little bigger.
static const CellRenderSettings radioSettings = {
    {
        NSMakeSize(11, 11),  // mini
        NSMakeSize(13, 13),  // small
        NSMakeSize(16, 16)   // regular
    },
    {NSZeroSize, NSZeroSize, NSZeroSize},
    {{
         // Leopard
         {0, 0, 0, 0},  // mini
         {0, 1, 1, 1},  // small
         {0, 0, 0, 0}   // regular
     },
     {
         // Yosemite
         {0, 0, 0, 0},  // mini
         {1, 1, 1, 2},  // small
         {0, 0, 0, 0}   // regular
     }}};

static const CellRenderSettings checkboxSettings = {
    {
        NSMakeSize(11, 11),  // mini
        NSMakeSize(13, 13),  // small
        NSMakeSize(16, 16)   // regular
    },
    {NSZeroSize, NSZeroSize, NSZeroSize},
    {{
         // Leopard
         {0, 1, 0, 0},  // mini
         {0, 1, 0, 1},  // small
         {0, 1, 0, 1}   // regular
     },
     {
         // Yosemite
         {0, 1, 0, 0},  // mini
         {0, 1, 0, 1},  // small
         {0, 1, 0, 1}   // regular
     }}};

static NSControlStateValue CellStateForCheckboxOrRadioState(
    nsNativeThemeCocoa::CheckboxOrRadioState aState) {
  switch (aState) {
    case nsNativeThemeCocoa::CheckboxOrRadioState::eOff:
      return NSControlStateValueOff;
    case nsNativeThemeCocoa::CheckboxOrRadioState::eOn:
      return NSControlStateValueOn;
    case nsNativeThemeCocoa::CheckboxOrRadioState::eIndeterminate:
      return NSControlStateValueMixed;
  }
}

void nsNativeThemeCocoa::DrawCheckboxOrRadio(
    CGContextRef cgContext, bool inCheckbox, const HIRect& inBoxRect,
    const CheckboxOrRadioParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NSButtonCell* cell = inCheckbox ? mCheckboxCell : mRadioButtonCell;
  ApplyControlParamsToNSCell(aParams.controlParams, cell);

  [cell setState:CellStateForCheckboxOrRadioState(aParams.state)];
  [cell setControlTint:(aParams.controlParams.insideActiveWindow
                            ? [NSColor currentControlTint]
                            : NSClearControlTint)];

  // Ensure that the control is square.
  float length = std::min(inBoxRect.size.width, inBoxRect.size.height);
  HIRect drawRect = CGRectMake(
      inBoxRect.origin.x + (int)((inBoxRect.size.width - length) / 2.0f),
      inBoxRect.origin.y + (int)((inBoxRect.size.height - length) / 2.0f),
      length, length);

  if (mCellDrawWindow) {
    mCellDrawWindow.cellsShouldLookActive =
        aParams.controlParams.insideActiveWindow;
  }
  DrawCellWithSnapping(cell, cgContext, drawRect,
                       inCheckbox ? checkboxSettings : radioSettings,
                       aParams.verticalAlignFactor, mCellDrawView, NO);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static const CellRenderSettings searchFieldSettings = {
    {
        NSMakeSize(0, 16),  // mini
        NSMakeSize(0, 19),  // small
        NSMakeSize(0, 22)   // regular
    },
    {
        NSMakeSize(32, 0),  // mini
        NSMakeSize(38, 0),  // small
        NSMakeSize(44, 0)   // regular
    },
    {{
         // Leopard
         {0, 0, 0, 0},  // mini
         {0, 0, 0, 0},  // small
         {0, 0, 0, 0}   // regular
     },
     {
         // Yosemite
         {0, 0, 0, 0},  // mini
         {0, 0, 0, 0},  // small
         {0, 0, 0, 0}   // regular
     }}};

static bool IsToolbarStyleContainer(nsIFrame* aFrame) {
  nsIContent* content = aFrame->GetContent();
  if (!content) {
    return false;
  }

  if (content->IsAnyOfXULElements(nsGkAtoms::toolbar, nsGkAtoms::toolbox,
                                  nsGkAtoms::statusbar)) {
    return true;
  }

  switch (aFrame->StyleDisplay()->EffectiveAppearance()) {
    case StyleAppearance::Toolbar:
    case StyleAppearance::Statusbar:
      return true;
    default:
      return false;
  }
}

static bool IsInsideToolbar(nsIFrame* aFrame) {
  for (nsIFrame* frame = aFrame; frame; frame = frame->GetParent()) {
    if (IsToolbarStyleContainer(frame)) {
      return true;
    }
  }
  return false;
}

nsNativeThemeCocoa::TextFieldParams nsNativeThemeCocoa::ComputeTextFieldParams(
    nsIFrame* aFrame, ElementState aEventState) {
  TextFieldParams params;
  params.insideToolbar = IsInsideToolbar(aFrame);
  params.disabled = aEventState.HasState(ElementState::DISABLED);

  // See ShouldUnconditionallyDrawFocusRingIfFocused.
  params.focused = aEventState.HasState(ElementState::FOCUS);

  params.rtl = IsFrameRTL(aFrame);
  params.verticalAlignFactor = VerticalAlignFactor(aFrame);
  return params;
}

void nsNativeThemeCocoa::DrawTextField(CGContextRef cgContext,
                                       const HIRect& inBoxRect,
                                       const TextFieldParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NSTextFieldCell* cell = mTextFieldCell;
  [cell setEnabled:!aParams.disabled];
  [cell setShowsFirstResponder:aParams.focused];

  if (mCellDrawWindow) {
    mCellDrawWindow.cellsShouldLookActive =
        YES;  // TODO: propagate correct activeness state
  }
  DrawCellWithSnapping(cell, cgContext, inBoxRect, searchFieldSettings,
                       aParams.verticalAlignFactor, mCellDrawView, aParams.rtl);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawSearchField(CGContextRef cgContext,
                                         const HIRect& inBoxRect,
                                         const TextFieldParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  mSearchFieldCell.enabled = !aParams.disabled;
  mSearchFieldCell.showsFirstResponder = aParams.focused;
  mSearchFieldCell.placeholderString = @"";
  mSearchFieldCell.shouldUseToolbarStyle = aParams.insideToolbar;

  if (mCellDrawWindow) {
    mCellDrawWindow.cellsShouldLookActive =
        YES;  // TODO: propagate correct activeness state
  }
  DrawCellWithSnapping(mSearchFieldCell, cgContext, inBoxRect,
                       searchFieldSettings, aParams.verticalAlignFactor,
                       mCellDrawView, aParams.rtl);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static bool ShouldUnconditionallyDrawFocusRingIfFocused(nsIFrame* aFrame) {
  // Mac always draws focus rings for textboxes and lists.
  switch (aFrame->StyleDisplay()->EffectiveAppearance()) {
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Searchfield:
    case StyleAppearance::Listbox:
      return true;
    default:
      return false;
  }
}

nsNativeThemeCocoa::ControlParams nsNativeThemeCocoa::ComputeControlParams(
    nsIFrame* aFrame, ElementState aEventState) {
  ControlParams params;
  params.disabled = aEventState.HasState(ElementState::DISABLED);
  params.insideActiveWindow = FrameIsInActiveWindow(aFrame);
  params.pressed =
      aEventState.HasAllStates(ElementState::ACTIVE | ElementState::HOVER);
  params.focused = aEventState.HasState(ElementState::FOCUS) &&
                   (aEventState.HasState(ElementState::FOCUSRING) ||
                    ShouldUnconditionallyDrawFocusRingIfFocused(aFrame));
  params.rtl = IsFrameRTL(aFrame);
  return params;
}

static const NSSize kHelpButtonSize = NSMakeSize(20, 20);
static const NSSize kDisclosureButtonSize = NSMakeSize(21, 21);

static const CellRenderSettings pushButtonSettings = {
    {
        NSMakeSize(0, 16),  // mini
        NSMakeSize(0, 19),  // small
        NSMakeSize(0, 22)   // regular
    },
    {
        NSMakeSize(18, 0),  // mini
        NSMakeSize(26, 0),  // small
        NSMakeSize(30, 0)   // regular
    },
    {{
         // Leopard
         {0, 0, 0, 0},  // mini
         {4, 0, 4, 1},  // small
         {5, 0, 5, 2}   // regular
     },
     {
         // Yosemite
         {0, 0, 0, 0},  // mini
         {4, 0, 4, 1},  // small
         {5, 0, 5, 2}   // regular
     }}};

// The height at which we start doing square buttons instead of rounded buttons
// Rounded buttons look bad if drawn at a height greater than 26, so at that
// point we switch over to doing square buttons which looks fine at any size.
#define DO_SQUARE_BUTTON_HEIGHT 26

void nsNativeThemeCocoa::DrawPushButton(CGContextRef cgContext,
                                        const HIRect& inBoxRect,
                                        ButtonType aButtonType,
                                        ControlParams aControlParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  ApplyControlParamsToNSCell(aControlParams, mPushButtonCell);
  [mPushButtonCell setBezelStyle:NSBezelStyleRounded];
  mPushButtonCell.keyEquivalent =
      aButtonType == ButtonType::eDefaultPushButton ? @"\r" : @"";

  if (mCellDrawWindow) {
    mCellDrawWindow.cellsShouldLookActive = aControlParams.insideActiveWindow;
  }
  DrawCellWithSnapping(mPushButtonCell, cgContext, inBoxRect,
                       pushButtonSettings, 0.5f, mCellDrawView,
                       aControlParams.rtl, 1.0f);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawSquareBezelPushButton(
    CGContextRef cgContext, const HIRect& inBoxRect,
    ControlParams aControlParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  ApplyControlParamsToNSCell(aControlParams, mPushButtonCell);
  [mPushButtonCell setBezelStyle:NSBezelStyleShadowlessSquare];

  if (mCellDrawWindow) {
    mCellDrawWindow.cellsShouldLookActive = aControlParams.insideActiveWindow;
  }
  DrawCellWithScaling(mPushButtonCell, cgContext, inBoxRect,
                      NSControlSizeRegular, NSZeroSize, NSMakeSize(14, 0), NULL,
                      mCellDrawView, aControlParams.rtl);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawHelpButton(CGContextRef cgContext,
                                        const HIRect& inBoxRect,
                                        ControlParams aControlParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  ApplyControlParamsToNSCell(aControlParams, mHelpButtonCell);

  if (mCellDrawWindow) {
    mCellDrawWindow.cellsShouldLookActive = aControlParams.insideActiveWindow;
  }
  DrawCellWithScaling(mHelpButtonCell, cgContext, inBoxRect,
                      NSControlSizeRegular, NSZeroSize, kHelpButtonSize, NULL,
                      mCellDrawView,
                      false);  // Don't mirror icon in RTL.

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawDisclosureButton(CGContextRef cgContext,
                                              const HIRect& inBoxRect,
                                              ControlParams aControlParams,
                                              NSControlStateValue aCellState) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  ApplyControlParamsToNSCell(aControlParams, mDisclosureButtonCell);
  [mDisclosureButtonCell setState:aCellState];

  if (mCellDrawWindow) {
    mCellDrawWindow.cellsShouldLookActive = aControlParams.insideActiveWindow;
  }
  DrawCellWithScaling(mDisclosureButtonCell, cgContext, inBoxRect,
                      NSControlSizeRegular, NSZeroSize, kDisclosureButtonSize,
                      NULL, mCellDrawView,
                      false);  // Don't mirror icon in RTL.

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

typedef void (*RenderHIThemeControlFunction)(CGContextRef cgContext,
                                             const HIRect& aRenderRect,
                                             void* aData);

static void RenderTransformedHIThemeControl(CGContextRef aCGContext,
                                            const HIRect& aRect,
                                            RenderHIThemeControlFunction aFunc,
                                            void* aData,
                                            BOOL mirrorHorizontally = NO) {
  CGAffineTransform savedCTM = CGContextGetCTM(aCGContext);
  CGContextTranslateCTM(aCGContext, aRect.origin.x, aRect.origin.y);

  bool drawDirect;
  HIRect drawRect = aRect;
  drawRect.origin = CGPointZero;

  if (!mirrorHorizontally && savedCTM.a == 1.0f && savedCTM.b == 0.0f &&
      savedCTM.c == 0.0f && (savedCTM.d == 1.0f || savedCTM.d == -1.0f)) {
    drawDirect = TRUE;
  } else {
    drawDirect = FALSE;
  }

  // Fall back to no bitmap buffer if the area of our control (in pixels^2)
  // is too large.
  if (drawDirect || (aRect.size.width * aRect.size.height > BITMAP_MAX_AREA)) {
    aFunc(aCGContext, drawRect, aData);
  } else {
    // Inflate the buffer to capture focus rings.
    int w = ceil(drawRect.size.width) + 2 * kMaxFocusRingWidth;
    int h = ceil(drawRect.size.height) + 2 * kMaxFocusRingWidth;

    int backingScaleFactor = GetBackingScaleFactorForRendering(aCGContext);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmapctx = CGBitmapContextCreate(
        NULL, w * backingScaleFactor, h * backingScaleFactor, 8,
        w * backingScaleFactor * 4, colorSpace,
        kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(colorSpace);

    CGContextScaleCTM(bitmapctx, backingScaleFactor, backingScaleFactor);
    CGContextTranslateCTM(bitmapctx, kMaxFocusRingWidth, kMaxFocusRingWidth);

    // Set the context's "base transform" to in order to get correctly-sized
    // focus rings.
    CGContextSetBaseCTM(bitmapctx, CGAffineTransformMakeScale(
                                       backingScaleFactor, backingScaleFactor));

    // HITheme always wants to draw into a flipped context, or things
    // get confused.
    CGContextTranslateCTM(bitmapctx, 0.0f, aRect.size.height);
    CGContextScaleCTM(bitmapctx, 1.0f, -1.0f);

    aFunc(bitmapctx, drawRect, aData);

    CGImageRef bitmap = CGBitmapContextCreateImage(bitmapctx);

    CGAffineTransform ctm = CGContextGetCTM(aCGContext);

    // We need to unflip, so that we can do a DrawImage without getting a
    // flipped image.
    CGContextTranslateCTM(aCGContext, 0.0f, aRect.size.height);
    CGContextScaleCTM(aCGContext, 1.0f, -1.0f);

    if (mirrorHorizontally) {
      CGContextTranslateCTM(aCGContext, aRect.size.width, 0);
      CGContextScaleCTM(aCGContext, -1.0f, 1.0f);
    }

    HIRect inflatedDrawRect =
        CGRectMake(-kMaxFocusRingWidth, -kMaxFocusRingWidth, w, h);
    CGContextDrawImage(aCGContext, inflatedDrawRect, bitmap);

    CGContextSetCTM(aCGContext, ctm);

    CGImageRelease(bitmap);
    CGContextRelease(bitmapctx);
  }

  CGContextSetCTM(aCGContext, savedCTM);
}

static void RenderButton(CGContextRef cgContext, const HIRect& aRenderRect,
                         void* aData) {
  HIThemeButtonDrawInfo* bdi = (HIThemeButtonDrawInfo*)aData;
  HIThemeDrawButton(&aRenderRect, bdi, cgContext, kHIThemeOrientationNormal,
                    NULL);
}

static ThemeDrawState ToThemeDrawState(
    const nsNativeThemeCocoa::ControlParams& aParams) {
  if (aParams.disabled) {
    return kThemeStateUnavailable;
  }
  if (aParams.pressed) {
    return kThemeStatePressed;
  }
  return kThemeStateActive;
}

void nsNativeThemeCocoa::DrawHIThemeButton(
    CGContextRef cgContext, const HIRect& aRect, ThemeButtonKind aKind,
    ThemeButtonValue aValue, ThemeDrawState aState,
    ThemeButtonAdornment aAdornment, const ControlParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  HIThemeButtonDrawInfo bdi;
  bdi.version = 0;
  bdi.kind = aKind;
  bdi.value = aValue;
  bdi.state = aState;
  bdi.adornment = aAdornment;

  if (aParams.focused && aParams.insideActiveWindow) {
    bdi.adornment |= kThemeAdornmentFocus;
  }

  RenderTransformedHIThemeControl(cgContext, aRect, RenderButton, &bdi,
                                  aParams.rtl);

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawButton(CGContextRef cgContext,
                                    const HIRect& inBoxRect,
                                    const ButtonParams& aParams) {
  ControlParams controlParams = aParams.controlParams;

  switch (aParams.button) {
    case ButtonType::eRegularPushButton:
    case ButtonType::eDefaultPushButton:
      DrawPushButton(cgContext, inBoxRect, aParams.button, controlParams);
      return;
    case ButtonType::eSquareBezelPushButton:
      DrawSquareBezelPushButton(cgContext, inBoxRect, controlParams);
      return;
    case ButtonType::eArrowButton:
      DrawHIThemeButton(cgContext, inBoxRect, kThemeArrowButton, kThemeButtonOn,
                        kThemeStateUnavailable, kThemeAdornmentArrowDownArrow,
                        controlParams);
      return;
    case ButtonType::eHelpButton:
      DrawHelpButton(cgContext, inBoxRect, controlParams);
      return;
    case ButtonType::eTreeTwistyPointingRight:
      DrawHIThemeButton(cgContext, inBoxRect, kThemeDisclosureButton,
                        kThemeDisclosureRight, ToThemeDrawState(controlParams),
                        kThemeAdornmentNone, controlParams);
      return;
    case ButtonType::eTreeTwistyPointingDown:
      DrawHIThemeButton(cgContext, inBoxRect, kThemeDisclosureButton,
                        kThemeDisclosureDown, ToThemeDrawState(controlParams),
                        kThemeAdornmentNone, controlParams);
      return;
    case ButtonType::eDisclosureButtonClosed:
      DrawDisclosureButton(cgContext, inBoxRect, controlParams,
                           NSControlStateValueOff);
      return;
    case ButtonType::eDisclosureButtonOpen:
      DrawDisclosureButton(cgContext, inBoxRect, controlParams,
                           NSControlStateValueOn);
      return;
  }
}

nsNativeThemeCocoa::TreeHeaderCellParams
nsNativeThemeCocoa::ComputeTreeHeaderCellParams(nsIFrame* aFrame,
                                                ElementState aEventState) {
  TreeHeaderCellParams params;
  params.controlParams = ComputeControlParams(aFrame, aEventState);
  params.sortDirection = GetTreeSortDirection(aFrame);
  params.lastTreeHeaderCell = IsLastTreeHeaderCell(aFrame);
  return params;
}

@interface NSTableHeaderCell (NSTableHeaderCell_setSortable)
// This method has been present in the same form since at least macOS 10.4.
- (void)_setSortable:(BOOL)arg1
    showSortIndicator:(BOOL)arg2
            ascending:(BOOL)arg3
             priority:(NSInteger)arg4
     highlightForSort:(BOOL)arg5;
@end

void nsNativeThemeCocoa::DrawTreeHeaderCell(
    CGContextRef cgContext, const HIRect& inBoxRect,
    const TreeHeaderCellParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Without clearing the cell's title, it takes on a default value of "Field",
  // which is displayed underneath the title set in the front-end.
  NSCell* cell = (NSCell*)mTreeHeaderCell;
  cell.title = @"";

  if ([mTreeHeaderCell
          respondsToSelector:@selector
          (_setSortable:
              showSortIndicator:ascending:priority:highlightForSort:)]) {
    switch (aParams.sortDirection) {
      case eTreeSortDirection_Ascending:
        [mTreeHeaderCell _setSortable:YES
                    showSortIndicator:YES
                            ascending:YES
                             priority:0
                     highlightForSort:YES];
        break;
      case eTreeSortDirection_Descending:
        [mTreeHeaderCell _setSortable:YES
                    showSortIndicator:YES
                            ascending:NO
                             priority:0
                     highlightForSort:YES];
        break;
      default:
        // eTreeSortDirection_Natural
        [mTreeHeaderCell _setSortable:YES
                    showSortIndicator:NO
                            ascending:YES
                             priority:0
                     highlightForSort:NO];
        break;
    }
  }

  mTreeHeaderCell.enabled = !aParams.controlParams.disabled;
  mTreeHeaderCell.state =
      (mTreeHeaderCell.enabled && aParams.controlParams.pressed)
          ? NSControlStateValueOn
          : NSControlStateValueOff;

  mCellDrawView._drawingEndSeparator = !aParams.lastTreeHeaderCell;

  NSGraphicsContext* savedContext = NSGraphicsContext.currentContext;
  NSGraphicsContext.currentContext =
      [NSGraphicsContext graphicsContextWithCGContext:cgContext flipped:YES];
  DrawCellIncludingFocusRing(mTreeHeaderCell, inBoxRect, mCellDrawView);
  NSGraphicsContext.currentContext = savedContext;

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static const CellRenderSettings dropdownSettings = {
    {
        NSMakeSize(0, 16),  // mini
        NSMakeSize(0, 19),  // small
        NSMakeSize(0, 22)   // regular
    },
    {
        NSMakeSize(18, 0),  // mini
        NSMakeSize(38, 0),  // small
        NSMakeSize(44, 0)   // regular
    },
    {{
         // Leopard
         {1, 1, 2, 1},  // mini
         {3, 0, 3, 1},  // small
         {3, 0, 3, 0}   // regular
     },
     {
         // Yosemite
         {1, 1, 2, 1},  // mini
         {3, 0, 3, 1},  // small
         {3, 0, 3, 0}   // regular
     }}};

static const CellRenderSettings editableMenulistSettings = {
    {
        NSMakeSize(0, 15),  // mini
        NSMakeSize(0, 18),  // small
        NSMakeSize(0, 21)   // regular
    },
    {
        NSMakeSize(18, 0),  // mini
        NSMakeSize(38, 0),  // small
        NSMakeSize(44, 0)   // regular
    },
    {{
         // Leopard
         {0, 0, 2, 2},  // mini
         {0, 0, 3, 2},  // small
         {0, 1, 3, 3}   // regular
     },
     {
         // Yosemite
         {0, 0, 2, 2},  // mini
         {0, 0, 3, 2},  // small
         {0, 1, 3, 3}   // regular
     }}};

void nsNativeThemeCocoa::DrawDropdown(CGContextRef cgContext,
                                      const HIRect& inBoxRect,
                                      const DropdownParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mDropdownCell setPullsDown:aParams.pullsDown];
  NSCell* cell =
      aParams.editable ? (NSCell*)mComboBoxCell : (NSCell*)mDropdownCell;

  ApplyControlParamsToNSCell(aParams.controlParams, cell);

  if (aParams.controlParams.insideActiveWindow) {
    [cell setControlTint:[NSColor currentControlTint]];
  } else {
    [cell setControlTint:NSClearControlTint];
  }

  const CellRenderSettings& settings =
      aParams.editable ? editableMenulistSettings : dropdownSettings;

  if (mCellDrawWindow) {
    mCellDrawWindow.cellsShouldLookActive =
        aParams.controlParams.insideActiveWindow;
  }
  DrawCellWithSnapping(cell, cgContext, inBoxRect, settings, 0.5f,
                       mCellDrawView, aParams.controlParams.rtl);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static const CellRenderSettings spinnerSettings = {
    {
        NSMakeSize(11,
                   16),  // mini (width trimmed by 2px to reduce blank border)
        NSMakeSize(15, 22),  // small
        NSMakeSize(19, 27)   // regular
    },
    {
        NSMakeSize(11,
                   16),  // mini (width trimmed by 2px to reduce blank border)
        NSMakeSize(15, 22),  // small
        NSMakeSize(19, 27)   // regular
    },
    {{
         // Leopard
         {0, 0, 0, 0},  // mini
         {0, 0, 0, 0},  // small
         {0, 0, 0, 0}   // regular
     },
     {
         // Yosemite
         {0, 0, 0, 0},  // mini
         {0, 0, 0, 0},  // small
         {0, 0, 0, 0}   // regular
     }}};

HIThemeButtonDrawInfo nsNativeThemeCocoa::SpinButtonDrawInfo(
    ThemeButtonKind aKind, const SpinButtonParams& aParams) {
  HIThemeButtonDrawInfo bdi;
  bdi.version = 0;
  bdi.kind = aKind;
  bdi.value = kThemeButtonOff;
  bdi.adornment = kThemeAdornmentNone;

  if (aParams.disabled) {
    bdi.state = kThemeStateUnavailable;
  } else if (aParams.insideActiveWindow && aParams.pressedButton) {
    if (*aParams.pressedButton == SpinButton::eUp) {
      bdi.state = kThemeStatePressedUp;
    } else {
      bdi.state = kThemeStatePressedDown;
    }
  } else {
    bdi.state = kThemeStateActive;
  }

  return bdi;
}

void nsNativeThemeCocoa::DrawSpinButtons(CGContextRef cgContext,
                                         const HIRect& inBoxRect,
                                         const SpinButtonParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  HIThemeButtonDrawInfo bdi = SpinButtonDrawInfo(kThemeIncDecButton, aParams);
  HIThemeDrawButton(&inBoxRect, &bdi, cgContext, HITHEME_ORIENTATION, NULL);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawSpinButton(CGContextRef cgContext,
                                        const HIRect& inBoxRect,
                                        SpinButton aDrawnButton,
                                        const SpinButtonParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  HIThemeButtonDrawInfo bdi =
      SpinButtonDrawInfo(kThemeIncDecButtonMini, aParams);

  // Cocoa only allows kThemeIncDecButton to paint the up and down spin buttons
  // together as a single unit (presumably because when one button is active,
  // the appearance of both changes (in different ways)). Here we have to paint
  // both buttons, using clip to hide the one we don't want to paint.
  HIRect drawRect = inBoxRect;
  drawRect.size.height *= 2;
  if (aDrawnButton == SpinButton::eDown) {
    drawRect.origin.y -= inBoxRect.size.height;
  }

  // Shift the drawing a little to the left, since cocoa paints with more
  // blank space around the visual buttons than we'd like:
  drawRect.origin.x -= 1;

  CGContextSaveGState(cgContext);
  CGContextClipToRect(cgContext, inBoxRect);

  HIThemeDrawButton(&drawRect, &bdi, cgContext, HITHEME_ORIENTATION, NULL);

  CGContextRestoreGState(cgContext);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static const CellRenderSettings progressSettings[2][2] = {
    // Vertical progress bar.
    {// Determined settings.
     {{
          NSZeroSize,         // mini
          NSMakeSize(10, 0),  // small
          NSMakeSize(16, 0)   // regular
      },
      {NSZeroSize, NSZeroSize, NSZeroSize},
      {{
          // Leopard
          {0, 0, 0, 0},  // mini
          {1, 1, 1, 1},  // small
          {1, 1, 1, 1}   // regular
      }}},
     // There is no horizontal margin in regular undetermined size.
     {{
          NSZeroSize,         // mini
          NSMakeSize(10, 0),  // small
          NSMakeSize(16, 0)   // regular
      },
      {NSZeroSize, NSZeroSize, NSZeroSize},
      {{
           // Leopard
           {0, 0, 0, 0},  // mini
           {1, 1, 1, 1},  // small
           {1, 0, 1, 0}   // regular
       },
       {
           // Yosemite
           {0, 0, 0, 0},  // mini
           {1, 1, 1, 1},  // small
           {1, 0, 1, 0}   // regular
       }}}},
    // Horizontal progress bar.
    {// Determined settings.
     {{
          NSZeroSize,         // mini
          NSMakeSize(0, 10),  // small
          NSMakeSize(0, 16)   // regular
      },
      {NSZeroSize, NSZeroSize, NSZeroSize},
      {{
           // Leopard
           {0, 0, 0, 0},  // mini
           {1, 1, 1, 1},  // small
           {1, 1, 1, 1}   // regular
       },
       {
           // Yosemite
           {0, 0, 0, 0},  // mini
           {1, 1, 1, 1},  // small
           {1, 1, 1, 1}   // regular
       }}},
     // There is no horizontal margin in regular undetermined size.
     {{
          NSZeroSize,         // mini
          NSMakeSize(0, 10),  // small
          NSMakeSize(0, 16)   // regular
      },
      {NSZeroSize, NSZeroSize, NSZeroSize},
      {{
           // Leopard
           {0, 0, 0, 0},  // mini
           {1, 1, 1, 1},  // small
           {0, 1, 0, 1}   // regular
       },
       {
           // Yosemite
           {0, 0, 0, 0},  // mini
           {1, 1, 1, 1},  // small
           {0, 1, 0, 1}   // regular
       }}}}};

nsNativeThemeCocoa::ProgressParams nsNativeThemeCocoa::ComputeProgressParams(
    nsIFrame* aFrame, ElementState aEventState, bool aIsHorizontal) {
  ProgressParams params;
  params.value = GetProgressValue(aFrame);
  params.max = GetProgressMaxValue(aFrame);
  params.verticalAlignFactor = VerticalAlignFactor(aFrame);
  params.insideActiveWindow = FrameIsInActiveWindow(aFrame);
  params.indeterminate = aEventState.HasState(ElementState::INDETERMINATE);
  params.horizontal = aIsHorizontal;
  params.rtl = IsFrameRTL(aFrame);
  return params;
}

void nsNativeThemeCocoa::DrawProgress(CGContextRef cgContext,
                                      const HIRect& inBoxRect,
                                      const ProgressParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NSProgressBarCell* cell = mProgressBarCell;

  [cell setValue:aParams.value];
  [cell setMax:aParams.max];
  [cell setIndeterminate:aParams.indeterminate];
  [cell setHorizontal:aParams.horizontal];
  [cell
      setControlTint:(aParams.insideActiveWindow ? [NSColor currentControlTint]
                                                 : NSClearControlTint)];

  if (mCellDrawWindow) {
    mCellDrawWindow.cellsShouldLookActive = aParams.insideActiveWindow;
  }
  DrawCellWithSnapping(
      cell, cgContext, inBoxRect,
      progressSettings[aParams.horizontal][aParams.indeterminate],
      aParams.verticalAlignFactor, mCellDrawView, aParams.rtl);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static const CellRenderSettings meterSetting = {
    {
        NSMakeSize(0, 16),  // mini
        NSMakeSize(0, 16),  // small
        NSMakeSize(0, 16)   // regular
    },
    {NSZeroSize, NSZeroSize, NSZeroSize},
    {{
         // Leopard
         {1, 1, 1, 1},  // mini
         {1, 1, 1, 1},  // small
         {1, 1, 1, 1}   // regular
     },
     {
         // Yosemite
         {1, 1, 1, 1},  // mini
         {1, 1, 1, 1},  // small
         {1, 1, 1, 1}   // regular
     }}};

nsNativeThemeCocoa::MeterParams nsNativeThemeCocoa::ComputeMeterParams(
    nsIFrame* aFrame) {
  nsIContent* content = aFrame->GetContent();
  if (!(content && content->IsHTMLElement(nsGkAtoms::meter))) {
    return MeterParams();
  }

  HTMLMeterElement* meterElement = static_cast<HTMLMeterElement*>(content);
  MeterParams params;
  params.value = meterElement->Value();
  params.min = meterElement->Min();
  params.max = meterElement->Max();
  ElementState states = meterElement->State();
  if (states.HasState(ElementState::SUB_OPTIMUM)) {
    params.optimumState = OptimumState::eSubOptimum;
  } else if (states.HasState(ElementState::SUB_SUB_OPTIMUM)) {
    params.optimumState = OptimumState::eSubSubOptimum;
  }
  params.horizontal = !IsVerticalMeter(aFrame);
  params.verticalAlignFactor = VerticalAlignFactor(aFrame);
  params.rtl = IsFrameRTL(aFrame);

  return params;
}

void nsNativeThemeCocoa::DrawMeter(CGContextRef cgContext,
                                   const HIRect& inBoxRect,
                                   const MeterParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK

  NSLevelIndicatorCell* cell = mMeterBarCell;

  [cell setMinValue:aParams.min];
  [cell setMaxValue:aParams.max];
  [cell setDoubleValue:aParams.value];

  /**
   * The way HTML and Cocoa defines the meter/indicator widget are different.
   * So, we are going to use a trick to get the Cocoa widget showing what we
   * are expecting: we set the warningValue or criticalValue to the current
   * value when we want to have the widget to be in the warning or critical
   * state.
   */
  switch (aParams.optimumState) {
    case OptimumState::eOptimum:
      [cell setWarningValue:aParams.max + 1];
      [cell setCriticalValue:aParams.max + 1];
      break;
    case OptimumState::eSubOptimum:
      [cell setWarningValue:aParams.value];
      [cell setCriticalValue:aParams.max + 1];
      break;
    case OptimumState::eSubSubOptimum:
      [cell setWarningValue:aParams.max + 1];
      [cell setCriticalValue:aParams.value];
      break;
  }

  HIRect rect = CGRectStandardize(inBoxRect);
  BOOL vertical = !aParams.horizontal;

  CGContextSaveGState(cgContext);

  if (vertical) {
    /**
     * Cocoa doesn't provide a vertical meter bar so to show one, we have to
     * show a rotated horizontal meter bar.
     * Given that we want to show a vertical meter bar, we assume that the rect
     * has vertical dimensions but we can't correctly draw a meter widget inside
     * such a rectangle so we need to inverse width and height (and re-position)
     * to get a rectangle with horizontal dimensions.
     * Finally, we want to show a vertical meter so we want to rotate the result
     * so it is vertical. We do that by changing the context.
     */
    CGFloat tmp = rect.size.width;
    rect.size.width = rect.size.height;
    rect.size.height = tmp;
    rect.origin.x += rect.size.height / 2.f - rect.size.width / 2.f;
    rect.origin.y += rect.size.width / 2.f - rect.size.height / 2.f;

    CGContextTranslateCTM(cgContext, CGRectGetMidX(rect), CGRectGetMidY(rect));
    CGContextRotateCTM(cgContext, -M_PI / 2.f);
    CGContextTranslateCTM(cgContext, -CGRectGetMidX(rect),
                          -CGRectGetMidY(rect));
  }

  if (mCellDrawWindow) {
    mCellDrawWindow.cellsShouldLookActive =
        YES;  // TODO: propagate correct activeness state
  }
  DrawCellWithSnapping(cell, cgContext, rect, meterSetting,
                       aParams.verticalAlignFactor, mCellDrawView,
                       !vertical && aParams.rtl);

  CGContextRestoreGState(cgContext);

  NS_OBJC_END_TRY_IGNORE_BLOCK
}

void nsNativeThemeCocoa::DrawTabPanel(CGContextRef cgContext,
                                      const HIRect& inBoxRect,
                                      bool aIsInsideActiveWindow) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  HIThemeTabPaneDrawInfo tpdi;

  tpdi.version = 1;
  tpdi.state = aIsInsideActiveWindow ? kThemeStateActive : kThemeStateInactive;
  tpdi.direction = kThemeTabNorth;
  tpdi.size = kHIThemeTabSizeNormal;
  tpdi.kind = kHIThemeTabKindNormal;

  HIThemeDrawTabPane(&inBoxRect, &tpdi, cgContext, HITHEME_ORIENTATION);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

Maybe<nsNativeThemeCocoa::ScaleParams>
nsNativeThemeCocoa::ComputeHTMLScaleParams(nsIFrame* aFrame,
                                           ElementState aEventState) {
  nsRangeFrame* rangeFrame = do_QueryFrame(aFrame);
  if (!rangeFrame) {
    return Nothing();
  }

  bool isHorizontal = IsRangeHorizontal(aFrame);

  // ScaleParams requires integer min, max and value. This is purely for
  // drawing, so we normalize to a range 0-1000 here.
  ScaleParams params;
  params.value = int32_t(rangeFrame->GetValueAsFractionOfRange() * 1000);
  params.min = 0;
  params.max = 1000;
  params.reverse = !isHorizontal || rangeFrame->IsRightToLeft();
  params.insideActiveWindow = FrameIsInActiveWindow(aFrame);
  params.focused = aEventState.HasState(ElementState::FOCUSRING);
  params.disabled = aEventState.HasState(ElementState::DISABLED);
  params.horizontal = isHorizontal;
  return Some(params);
}

void nsNativeThemeCocoa::DrawScale(CGContextRef cgContext,
                                   const HIRect& inBoxRect,
                                   const ScaleParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  HIThemeTrackDrawInfo tdi;

  tdi.version = 0;
  tdi.kind = kThemeMediumSlider;
  tdi.bounds = inBoxRect;
  tdi.min = aParams.min;
  tdi.max = aParams.max;
  tdi.value = aParams.value;
  tdi.attributes = kThemeTrackShowThumb;
  if (aParams.horizontal) {
    tdi.attributes |= kThemeTrackHorizontal;
  }
  if (aParams.reverse) {
    tdi.attributes |= kThemeTrackRightToLeft;
  }
  if (aParams.focused) {
    tdi.attributes |= kThemeTrackHasFocus;
  }
  if (aParams.disabled) {
    tdi.enableState = kThemeTrackDisabled;
  } else {
    tdi.enableState =
        aParams.insideActiveWindow ? kThemeTrackActive : kThemeTrackInactive;
  }
  tdi.trackInfo.slider.thumbDir = kThemeThumbPlain;
  tdi.trackInfo.slider.pressState = 0;

  HIThemeDrawTrack(&tdi, NULL, cgContext, HITHEME_ORIENTATION);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

nsIFrame* nsNativeThemeCocoa::SeparatorResponsibility(nsIFrame* aBefore,
                                                      nsIFrame* aAfter) {
  // Usually a separator is drawn by the segment to the right of the
  // separator, but pressed and selected segments have higher priority.
  if (!aBefore || !aAfter) return nullptr;
  if (IsSelectedButton(aAfter)) return aAfter;
  if (IsSelectedButton(aBefore) || IsPressedButton(aBefore)) return aBefore;
  return aAfter;
}

static CGRect SeparatorAdjustedRect(CGRect aRect,
                                    nsNativeThemeCocoa::SegmentParams aParams) {
  // A separator between two segments should always be located in the leftmost
  // pixel column of the segment to the right of the separator, regardless of
  // who ends up drawing it.
  // CoreUI draws the separators inside the drawing rect.
  if (!aParams.atLeftEnd && !aParams.drawsLeftSeparator) {
    // The segment to the left of us draws the separator, so we need to make
    // room for it.
    aRect.origin.x += 1;
    aRect.size.width -= 1;
  }
  if (aParams.drawsRightSeparator) {
    // We draw the right separator, so we need to extend the draw rect into the
    // segment to our right.
    aRect.size.width += 1;
  }
  return aRect;
}

static NSString* ToolbarButtonPosition(BOOL aIsFirst, BOOL aIsLast) {
  if (aIsFirst) {
    if (aIsLast) return @"kCUISegmentPositionOnly";
    return @"kCUISegmentPositionFirst";
  }
  if (aIsLast) return @"kCUISegmentPositionLast";
  return @"kCUISegmentPositionMiddle";
}

struct SegmentedControlRenderSettings {
  const CGFloat* heights;
  const NSString* widgetName;
};

static const CGFloat tabHeights[3] = {17, 20, 23};

static const SegmentedControlRenderSettings tabRenderSettings = {tabHeights,
                                                                 @"tab"};

static const CGFloat toolbarButtonHeights[3] = {15, 18, 22};

static const SegmentedControlRenderSettings toolbarButtonRenderSettings = {
    toolbarButtonHeights, @"kCUIWidgetButtonSegmentedSCurve"};

nsNativeThemeCocoa::SegmentParams nsNativeThemeCocoa::ComputeSegmentParams(
    nsIFrame* aFrame, ElementState aEventState, SegmentType aSegmentType) {
  SegmentParams params;
  params.segmentType = aSegmentType;
  params.insideActiveWindow = FrameIsInActiveWindow(aFrame);
  params.pressed = IsPressedButton(aFrame);
  params.selected = IsSelectedButton(aFrame);
  params.focused = aEventState.HasState(ElementState::FOCUSRING);
  bool isRTL = IsFrameRTL(aFrame);
  nsIFrame* left = GetAdjacentSiblingFrameWithSameAppearance(aFrame, isRTL);
  nsIFrame* right = GetAdjacentSiblingFrameWithSameAppearance(aFrame, !isRTL);
  params.atLeftEnd = !left;
  params.atRightEnd = !right;
  params.drawsLeftSeparator = SeparatorResponsibility(left, aFrame) == aFrame;
  params.drawsRightSeparator = SeparatorResponsibility(aFrame, right) == aFrame;
  params.rtl = isRTL;
  return params;
}

static SegmentedControlRenderSettings RenderSettingsForSegmentType(
    nsNativeThemeCocoa::SegmentType aSegmentType) {
  switch (aSegmentType) {
    case nsNativeThemeCocoa::SegmentType::eToolbarButton:
      return toolbarButtonRenderSettings;
    case nsNativeThemeCocoa::SegmentType::eTab:
      return tabRenderSettings;
  }
}

void nsNativeThemeCocoa::DrawSegment(CGContextRef cgContext,
                                     const HIRect& inBoxRect,
                                     const SegmentParams& aParams) {
  SegmentedControlRenderSettings renderSettings =
      RenderSettingsForSegmentType(aParams.segmentType);
  NSControlSize controlSize =
      FindControlSize(inBoxRect.size.height, renderSettings.heights, 4.0f);
  CGRect drawRect = SeparatorAdjustedRect(inBoxRect, aParams);

  NSDictionary* dict = @{
    @"widget" : renderSettings.widgetName,
    @"kCUIPresentationStateKey" :
        (aParams.insideActiveWindow ? @"kCUIPresentationStateActiveKey"
                                    : @"kCUIPresentationStateInactive"),
    @"kCUIPositionKey" :
        ToolbarButtonPosition(aParams.atLeftEnd, aParams.atRightEnd),
    @"kCUISegmentLeadingSeparatorKey" :
        [NSNumber numberWithBool:aParams.drawsLeftSeparator],
    @"kCUISegmentTrailingSeparatorKey" :
        [NSNumber numberWithBool:aParams.drawsRightSeparator],
    @"value" : [NSNumber numberWithBool:aParams.selected],
    @"state" : (aParams.pressed
                    ? @"pressed"
                    : (aParams.insideActiveWindow ? @"normal" : @"inactive")),
    @"focus" : [NSNumber numberWithBool:aParams.focused],
    @"size" : CUIControlSizeForCocoaSize(controlSize),
    @"is.flipped" : [NSNumber numberWithBool:YES],
    @"direction" : @"up"
  };

  RenderWithCoreUI(drawRect, cgContext, dict);
}

void nsNativeThemeCocoa::DrawToolbar(CGContextRef cgContext,
                                     const CGRect& inBoxRect, bool aIsMain) {
  CGRect drawRect = inBoxRect;

  // top border
  drawRect.size.height = 1.0f;
  DrawNativeGreyColorInRect(cgContext, toolbarTopBorderGrey, drawRect, aIsMain);

  // background
  drawRect.origin.y += drawRect.size.height;
  drawRect.size.height = inBoxRect.size.height - 2.0f;
  DrawNativeGreyColorInRect(cgContext, toolbarFillGrey, drawRect, aIsMain);

  // bottom border
  drawRect.origin.y += drawRect.size.height;
  drawRect.size.height = 1.0f;
  DrawNativeGreyColorInRect(cgContext, toolbarBottomBorderGrey, drawRect,
                            aIsMain);
}

static bool ToolbarCanBeUnified(const gfx::Rect& aRect, NSWindow* aWindow) {
  if (![aWindow isKindOfClass:[ToolbarWindow class]]) return false;

  ToolbarWindow* win = (ToolbarWindow*)aWindow;
  float unifiedToolbarHeight = [win unifiedToolbarHeight];
  return aRect.X() == 0 && aRect.Width() >= [win frame].size.width &&
         aRect.YMost() <= unifiedToolbarHeight;
}

void nsNativeThemeCocoa::DrawStatusBar(CGContextRef cgContext,
                                       const HIRect& inBoxRect, bool aIsMain) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (inBoxRect.size.height < 2.0f) return;

  CGContextSaveGState(cgContext);
  CGContextClipToRect(cgContext, inBoxRect);

  // kCUIWidgetWindowFrame draws a complete window frame with both title bar
  // and bottom bar. We only want the bottom bar, so we extend the draw rect
  // upwards to make space for the title bar, and then we clip it away.
  CGRect drawRect = inBoxRect;
  const int extendUpwards = 40;
  drawRect.origin.y -= extendUpwards;
  drawRect.size.height += extendUpwards;
  RenderWithCoreUI(
      drawRect, cgContext,
      [NSDictionary dictionaryWithObjectsAndKeys:
                        @"kCUIWidgetWindowFrame", @"widget", @"regularwin",
                        @"windowtype", (aIsMain ? @"normal" : @"inactive"),
                        @"state",
                        [NSNumber numberWithInt:inBoxRect.size.height],
                        @"kCUIWindowFrameBottomBarHeightKey",
                        [NSNumber numberWithBool:YES],
                        @"kCUIWindowFrameDrawBottomBarSeparatorKey",
                        [NSNumber numberWithBool:YES], @"is.flipped", nil]);

  CGContextRestoreGState(cgContext);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawMultilineTextField(CGContextRef cgContext,
                                                const CGRect& inBoxRect,
                                                bool aIsFocused) {
  mTextFieldCell.enabled = YES;
  mTextFieldCell.showsFirstResponder = aIsFocused;

  if (mCellDrawWindow) {
    mCellDrawWindow.cellsShouldLookActive = YES;
  }

  // DrawCellIncludingFocusRing draws into the current NSGraphicsContext, so do
  // the usual save+restore dance.
  NSGraphicsContext* savedContext = NSGraphicsContext.currentContext;
  NSGraphicsContext.currentContext =
      [NSGraphicsContext graphicsContextWithCGContext:cgContext flipped:YES];
  DrawCellIncludingFocusRing(mTextFieldCell, inBoxRect, mCellDrawView);
  NSGraphicsContext.currentContext = savedContext;
}

static bool IsHiDPIContext(nsDeviceContext* aContext) {
  return AppUnitsPerCSSPixel() >=
         2 * aContext->AppUnitsPerDevPixelAtUnitFullZoom();
}

Maybe<nsNativeThemeCocoa::WidgetInfo> nsNativeThemeCocoa::ComputeWidgetInfo(
    nsIFrame* aFrame, StyleAppearance aAppearance, const nsRect& aRect) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // setup to draw into the correct port
  int32_t p2a = aFrame->PresContext()->AppUnitsPerDevPixel();

  gfx::Rect nativeWidgetRect(aRect.x, aRect.y, aRect.width, aRect.height);
  nativeWidgetRect.Scale(1.0 / gfxFloat(p2a));
  float originalHeight = nativeWidgetRect.Height();
  nativeWidgetRect.Round();
  if (nativeWidgetRect.IsEmpty()) {
    return Nothing();  // Don't attempt to draw invisible widgets.
  }

  bool hidpi = IsHiDPIContext(aFrame->PresContext()->DeviceContext());
  if (hidpi) {
    // Use high-resolution drawing.
    nativeWidgetRect.Scale(0.5f);
    originalHeight *= 0.5f;
  }

  ElementState elementState = GetContentState(aFrame, aAppearance);

  switch (aAppearance) {
    case StyleAppearance::Menupopup:
      return Nothing();

    case StyleAppearance::Tooltip:
      return Nothing();

    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio: {
      bool isCheckbox = aAppearance == StyleAppearance::Checkbox;

      CheckboxOrRadioParams params;
      params.state = CheckboxOrRadioState::eOff;
      if (isCheckbox && elementState.HasState(ElementState::INDETERMINATE)) {
        params.state = CheckboxOrRadioState::eIndeterminate;
      } else if (elementState.HasState(ElementState::CHECKED)) {
        params.state = CheckboxOrRadioState::eOn;
      }
      params.controlParams = ComputeControlParams(aFrame, elementState);
      params.verticalAlignFactor = VerticalAlignFactor(aFrame);
      if (isCheckbox) {
        return Some(WidgetInfo::Checkbox(params));
      }
      return Some(WidgetInfo::Radio(params));
    }

    case StyleAppearance::Button:
      if (IsDefaultButton(aFrame)) {
        // Check whether the default button is in a document that does not
        // match the :-moz-window-inactive pseudoclass. This activeness check
        // is different from the other "active window" checks in this file
        // because we absolutely need the button's default button appearance to
        // be in sync with its text color, and the text color is changed by
        // such a :-moz-window-inactive rule. (That's because on 10.10 and up,
        // default buttons in active windows have blue background and white
        // text, and default buttons in inactive windows have white background
        // and black text.)
        DocumentState docState = aFrame->PresContext()->Document()->State();
        ControlParams params = ComputeControlParams(aFrame, elementState);
        params.insideActiveWindow =
            !docState.HasState(DocumentState::WINDOW_INACTIVE);
        return Some(WidgetInfo::Button(
            ButtonParams{params, ButtonType::eDefaultPushButton}));
      }
      if (IsButtonTypeMenu(aFrame)) {
        ControlParams controlParams =
            ComputeControlParams(aFrame, elementState);
        controlParams.pressed = IsOpenButton(aFrame);
        DropdownParams params;
        params.controlParams = controlParams;
        params.pullsDown = true;
        params.editable = false;
        return Some(WidgetInfo::Dropdown(params));
      }
      if (originalHeight > DO_SQUARE_BUTTON_HEIGHT) {
        // If the button is tall enough, draw the square button style so that
        // buttons with non-standard content look good. Otherwise draw normal
        // rounded aqua buttons.
        // This comparison is done based on the height that is calculated
        // without the top, because the snapped height can be affected by the
        // top of the rect and that may result in different height depending on
        // the top value.
        return Some(WidgetInfo::Button(
            ButtonParams{ComputeControlParams(aFrame, elementState),
                         ButtonType::eSquareBezelPushButton}));
      }
      return Some(WidgetInfo::Button(
          ButtonParams{ComputeControlParams(aFrame, elementState),
                       ButtonType::eRegularPushButton}));

    case StyleAppearance::MozMacHelpButton:
      return Some(WidgetInfo::Button(
          ButtonParams{ComputeControlParams(aFrame, elementState),
                       ButtonType::eHelpButton}));

    case StyleAppearance::MozMacDisclosureButtonOpen:
    case StyleAppearance::MozMacDisclosureButtonClosed: {
      ButtonType buttonType =
          (aAppearance == StyleAppearance::MozMacDisclosureButtonClosed)
              ? ButtonType::eDisclosureButtonClosed
              : ButtonType::eDisclosureButtonOpen;
      return Some(WidgetInfo::Button(ButtonParams{
          ComputeControlParams(aFrame, elementState), buttonType}));
    }

    case StyleAppearance::Spinner: {
      bool isSpinner = (aAppearance == StyleAppearance::Spinner);
      nsIContent* content = aFrame->GetContent();
      if (isSpinner && content->IsHTMLElement()) {
        // In HTML the theming for the spin buttons is drawn individually into
        // their own backgrounds instead of being drawn into the background of
        // their spinner parent as it is for XUL.
        break;
      }
      SpinButtonParams params;
      if (content->IsElement()) {
        if (content->AsElement()->AttrValueIs(
                kNameSpaceID_None, nsGkAtoms::state, u"up"_ns, eCaseMatters)) {
          params.pressedButton = Some(SpinButton::eUp);
        } else if (content->AsElement()->AttrValueIs(
                       kNameSpaceID_None, nsGkAtoms::state, u"down"_ns,
                       eCaseMatters)) {
          params.pressedButton = Some(SpinButton::eDown);
        }
      }
      params.disabled = elementState.HasState(ElementState::DISABLED);
      params.insideActiveWindow = FrameIsInActiveWindow(aFrame);

      return Some(WidgetInfo::SpinButtons(params));
    }

    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton: {
      nsNumberControlFrame* numberControlFrame =
          nsNumberControlFrame::GetNumberControlFrameForSpinButton(aFrame);
      if (numberControlFrame) {
        SpinButtonParams params;
        if (numberControlFrame->SpinnerUpButtonIsDepressed()) {
          params.pressedButton = Some(SpinButton::eUp);
        } else if (numberControlFrame->SpinnerDownButtonIsDepressed()) {
          params.pressedButton = Some(SpinButton::eDown);
        }
        params.disabled = elementState.HasState(ElementState::DISABLED);
        params.insideActiveWindow = FrameIsInActiveWindow(aFrame);
        if (aAppearance == StyleAppearance::SpinnerUpbutton) {
          return Some(WidgetInfo::SpinButtonUp(params));
        }
        return Some(WidgetInfo::SpinButtonDown(params));
      }
    } break;

    case StyleAppearance::Toolbarbutton: {
      SegmentParams params = ComputeSegmentParams(aFrame, elementState,
                                                  SegmentType::eToolbarButton);
      params.insideActiveWindow = [NativeWindowForFrame(aFrame) isMainWindow];
      return Some(WidgetInfo::Segment(params));
    }

    case StyleAppearance::Separator:
      return Some(WidgetInfo::Separator());

    case StyleAppearance::Toolbar: {
      NSWindow* win = NativeWindowForFrame(aFrame);
      bool isMain = [win isMainWindow];
      if (ToolbarCanBeUnified(nativeWidgetRect, win)) {
        // Unified toolbars are drawn similar to vibrancy; we communicate their
        // extents via the theme geometry mechanism and then place native views
        // under Gecko's rendering. So Gecko just needs to be transparent in the
        // place where the toolbar should be visible.
        return Nothing();
      }
      return Some(WidgetInfo::Toolbar(isMain));
    }

    case StyleAppearance::MozWindowTitlebar: {
      return Nothing();
    }

    case StyleAppearance::Statusbar:
      return Some(WidgetInfo::StatusBar(IsActive(aFrame, YES)));

    case StyleAppearance::MenulistButton:
    case StyleAppearance::Menulist: {
      ControlParams controlParams = ComputeControlParams(aFrame, elementState);
      controlParams.pressed = IsOpenButton(aFrame);
      DropdownParams params;
      params.controlParams = controlParams;
      params.pullsDown = false;
      params.editable = false;
      return Some(WidgetInfo::Dropdown(params));
    }

    case StyleAppearance::MozMenulistArrowButton:
      return Some(WidgetInfo::Button(
          ButtonParams{ComputeControlParams(aFrame, elementState),
                       ButtonType::eArrowButton}));

    case StyleAppearance::Textfield:
    case StyleAppearance::NumberInput:
      return Some(
          WidgetInfo::TextField(ComputeTextFieldParams(aFrame, elementState)));

    case StyleAppearance::Searchfield:
      return Some(WidgetInfo::SearchField(
          ComputeTextFieldParams(aFrame, elementState)));

    case StyleAppearance::ProgressBar: {
      if (elementState.HasState(ElementState::INDETERMINATE)) {
        if (!QueueAnimatedContentForRefresh(aFrame->GetContent(), 30)) {
          NS_WARNING("Unable to animate progressbar!");
        }
      }
      return Some(WidgetInfo::ProgressBar(ComputeProgressParams(
          aFrame, elementState, !IsVerticalProgress(aFrame))));
    }

    case StyleAppearance::Meter:
      return Some(WidgetInfo::Meter(ComputeMeterParams(aFrame)));

    case StyleAppearance::Progresschunk:
    case StyleAppearance::Meterchunk:
      // Do nothing: progress and meter bars cases will draw chunks.
      break;

    case StyleAppearance::Treetwisty:
      return Some(WidgetInfo::Button(
          ButtonParams{ComputeControlParams(aFrame, elementState),
                       ButtonType::eTreeTwistyPointingRight}));

    case StyleAppearance::Treetwistyopen:
      return Some(WidgetInfo::Button(
          ButtonParams{ComputeControlParams(aFrame, elementState),
                       ButtonType::eTreeTwistyPointingDown}));

    case StyleAppearance::Treeheadercell:
      return Some(WidgetInfo::TreeHeaderCell(
          ComputeTreeHeaderCellParams(aFrame, elementState)));

    case StyleAppearance::Treeitem:
    case StyleAppearance::Treeview:
      return Some(WidgetInfo::ColorFill(sRGBColor(1.0, 1.0, 1.0, 1.0)));

    case StyleAppearance::Treeheader:
      // do nothing, taken care of by individual header cells
    case StyleAppearance::Treeheadersortarrow:
      // do nothing, taken care of by treeview header
    case StyleAppearance::Treeline:
      // do nothing, these lines don't exist on macos
      break;

    case StyleAppearance::Range: {
      Maybe<ScaleParams> params = ComputeHTMLScaleParams(aFrame, elementState);
      if (params) {
        return Some(WidgetInfo::Scale(*params));
      }
      break;
    }

    case StyleAppearance::Textarea:
      return Some(WidgetInfo::MultilineTextField(
          elementState.HasState(ElementState::FOCUS)));

    case StyleAppearance::Listbox:
      return Some(WidgetInfo::ListBox());

    case StyleAppearance::Tab: {
      SegmentParams params =
          ComputeSegmentParams(aFrame, elementState, SegmentType::eTab);
      params.pressed = params.pressed && !params.selected;
      return Some(WidgetInfo::Segment(params));
    }

    case StyleAppearance::Tabpanels:
      return Some(WidgetInfo::TabPanel(FrameIsInActiveWindow(aFrame)));

    default:
      break;
  }

  return Nothing();

  NS_OBJC_END_TRY_BLOCK_RETURN(Nothing());
}

NS_IMETHODIMP
nsNativeThemeCocoa::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         const nsRect& aRect,
                                         const nsRect& aDirtyRect,
                                         DrawOverflow aDrawOverflow) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return ThemeCocoa::DrawWidgetBackground(aContext, aFrame, aAppearance,
                                            aRect, aDirtyRect, aDrawOverflow);
  }

  Maybe<WidgetInfo> widgetInfo = ComputeWidgetInfo(aFrame, aAppearance, aRect);

  if (!widgetInfo) {
    return NS_OK;
  }

  int32_t p2a = aFrame->PresContext()->AppUnitsPerDevPixel();

  gfx::Rect nativeWidgetRect = NSRectToRect(aRect, p2a);
  nativeWidgetRect.Round();

  bool hidpi = IsHiDPIContext(aFrame->PresContext()->DeviceContext());

  auto colorScheme = LookAndFeel::ColorSchemeForFrame(aFrame);

  RenderWidget(*widgetInfo, colorScheme, *aContext->GetDrawTarget(),
               nativeWidgetRect, NSRectToRect(aDirtyRect, p2a),
               hidpi ? 2.0f : 1.0f);

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

void nsNativeThemeCocoa::RenderWidget(const WidgetInfo& aWidgetInfo,
                                      LookAndFeel::ColorScheme aScheme,
                                      DrawTarget& aDrawTarget,
                                      const gfx::Rect& aWidgetRect,
                                      const gfx::Rect& aDirtyRect,
                                      float aScale) {
  // Some of the drawing below uses NSAppearance.currentAppearance behind the
  // scenes. Set it to the appearance we want, the same way as
  // nsLookAndFeel::NativeGetColor.
  NSAppearance.currentAppearance = NSAppearanceForColorScheme(aScheme);

  // Also set the cell draw window's appearance; this is respected by
  // NSTextFieldCell (and its subclass NSSearchFieldCell).
  if (mCellDrawWindow) {
    mCellDrawWindow.appearance = NSAppearance.currentAppearance;
  }

  const Widget widget = aWidgetInfo.Widget();

  // Some widgets render using DrawTarget, and some using CGContext.
  switch (widget) {
    case Widget::eColorFill: {
      sRGBColor color = aWidgetInfo.Params<sRGBColor>();
      aDrawTarget.FillRect(aWidgetRect, ColorPattern(ToDeviceColor(color)));
      break;
    }
    default: {
      AutoRestoreTransform autoRestoreTransform(&aDrawTarget);
      gfx::Rect widgetRect = aWidgetRect;
      gfx::Rect dirtyRect = aDirtyRect;

      dirtyRect.Scale(1.0f / aScale);
      widgetRect.Scale(1.0f / aScale);
      aDrawTarget.SetTransform(
          aDrawTarget.GetTransform().PreScale(aScale, aScale));

      // The remaining widgets require a CGContext.
      CGRect macRect = CGRectMake(widgetRect.X(), widgetRect.Y(),
                                  widgetRect.Width(), widgetRect.Height());

      gfxQuartzNativeDrawing nativeDrawing(aDrawTarget, dirtyRect);

      CGContextRef cgContext = nativeDrawing.BeginNativeDrawing();
      if (cgContext == nullptr) {
        // The Quartz surface handles 0x0 surfaces by internally
        // making all operations no-ops; there's no cgcontext created for them.
        // Unfortunately, this means that callers that want to render
        // directly to the CGContext need to be aware of this quirk.
        return;
      }

      // Set the context's "base transform" to in order to get correctly-sized
      // focus rings.
      CGContextSetBaseCTM(cgContext,
                          CGAffineTransformMakeScale(aScale, aScale));

      switch (widget) {
        case Widget::eColorFill:
          MOZ_CRASH("already handled in outer switch");
          break;
        case Widget::eCheckbox: {
          CheckboxOrRadioParams params =
              aWidgetInfo.Params<CheckboxOrRadioParams>();
          DrawCheckboxOrRadio(cgContext, true, macRect, params);
          break;
        }
        case Widget::eRadio: {
          CheckboxOrRadioParams params =
              aWidgetInfo.Params<CheckboxOrRadioParams>();
          DrawCheckboxOrRadio(cgContext, false, macRect, params);
          break;
        }
        case Widget::eButton: {
          ButtonParams params = aWidgetInfo.Params<ButtonParams>();
          DrawButton(cgContext, macRect, params);
          break;
        }
        case Widget::eDropdown: {
          DropdownParams params = aWidgetInfo.Params<DropdownParams>();
          DrawDropdown(cgContext, macRect, params);
          break;
        }
        case Widget::eSpinButtons: {
          SpinButtonParams params = aWidgetInfo.Params<SpinButtonParams>();
          DrawSpinButtons(cgContext, macRect, params);
          break;
        }
        case Widget::eSpinButtonUp: {
          SpinButtonParams params = aWidgetInfo.Params<SpinButtonParams>();
          DrawSpinButton(cgContext, macRect, SpinButton::eUp, params);
          break;
        }
        case Widget::eSpinButtonDown: {
          SpinButtonParams params = aWidgetInfo.Params<SpinButtonParams>();
          DrawSpinButton(cgContext, macRect, SpinButton::eDown, params);
          break;
        }
        case Widget::eSegment: {
          SegmentParams params = aWidgetInfo.Params<SegmentParams>();
          DrawSegment(cgContext, macRect, params);
          break;
        }
        case Widget::eSeparator: {
          HIThemeSeparatorDrawInfo sdi = {0, kThemeStateActive};
          HIThemeDrawSeparator(&macRect, &sdi, cgContext, HITHEME_ORIENTATION);
          break;
        }
        case Widget::eToolbar: {
          bool isMain = aWidgetInfo.Params<bool>();
          DrawToolbar(cgContext, macRect, isMain);
          break;
        }
        case Widget::eStatusBar: {
          bool isMain = aWidgetInfo.Params<bool>();
          DrawStatusBar(cgContext, macRect, isMain);
          break;
        }
        case Widget::eGroupBox: {
          HIThemeGroupBoxDrawInfo gdi = {0, kThemeStateActive,
                                         kHIThemeGroupBoxKindPrimary};
          HIThemeDrawGroupBox(&macRect, &gdi, cgContext, HITHEME_ORIENTATION);
          break;
        }
        case Widget::eTextField: {
          TextFieldParams params = aWidgetInfo.Params<TextFieldParams>();
          DrawTextField(cgContext, macRect, params);
          break;
        }
        case Widget::eSearchField: {
          TextFieldParams params = aWidgetInfo.Params<TextFieldParams>();
          DrawSearchField(cgContext, macRect, params);
          break;
        }
        case Widget::eProgressBar: {
          ProgressParams params = aWidgetInfo.Params<ProgressParams>();
          DrawProgress(cgContext, macRect, params);
          break;
        }
        case Widget::eMeter: {
          MeterParams params = aWidgetInfo.Params<MeterParams>();
          DrawMeter(cgContext, macRect, params);
          break;
        }
        case Widget::eTreeHeaderCell: {
          TreeHeaderCellParams params =
              aWidgetInfo.Params<TreeHeaderCellParams>();
          DrawTreeHeaderCell(cgContext, macRect, params);
          break;
        }
        case Widget::eScale: {
          ScaleParams params = aWidgetInfo.Params<ScaleParams>();
          DrawScale(cgContext, macRect, params);
          break;
        }
        case Widget::eMultilineTextField: {
          bool isFocused = aWidgetInfo.Params<bool>();
          DrawMultilineTextField(cgContext, macRect, isFocused);
          break;
        }
        case Widget::eListBox: {
          // Fill the content with the control background color.
          CGContextSetFillColorWithColor(
              cgContext, [NSColor.controlBackgroundColor CGColor]);
          CGContextFillRect(cgContext, macRect);
          // Draw the frame using kCUIWidgetScrollViewFrame. This is what
          // NSScrollView uses in
          // -[NSScrollView drawRect:] if you give it a borderType of
          // NSBezelBorder.
          RenderWithCoreUI(
              macRect, cgContext, @{
                @"widget" : @"kCUIWidgetScrollViewFrame",
                @"kCUIIsFlippedKey" : @YES,
                @"kCUIVariantMetal" : @NO,
              });
          break;
        }
        case Widget::eTabPanel: {
          bool isInsideActiveWindow = aWidgetInfo.Params<bool>();
          DrawTabPanel(cgContext, macRect, isInsideActiveWindow);
          break;
        }
      }

      // Reset the base CTM.
      CGContextSetBaseCTM(cgContext, CGAffineTransformIdentity);

      nativeDrawing.EndNativeDrawing();
    }
  }
}

bool nsNativeThemeCocoa::CreateWebRenderCommandsForWidget(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const mozilla::layers::StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager, nsIFrame* aFrame,
    StyleAppearance aAppearance, const nsRect& aRect) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return ThemeCocoa::CreateWebRenderCommandsForWidget(
        aBuilder, aResources, aSc, aManager, aFrame, aAppearance, aRect);
  }

  // This list needs to stay consistent with the list in DrawWidgetBackground.
  // For every switch case in DrawWidgetBackground, there are three choices:
  //  - If the case in DrawWidgetBackground draws nothing for the given widget
  //    type, then don't list it here. We will hit the "default: return true;"
  //    case.
  //  - If the case in DrawWidgetBackground draws something simple for the given
  //    widget type, imitate that drawing using WebRender commands.
  //  - If the case in DrawWidgetBackground draws something complicated for the
  //    given widget type, return false here.
  switch (aAppearance) {
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
    case StyleAppearance::Button:
    case StyleAppearance::MozMacHelpButton:
    case StyleAppearance::MozMacDisclosureButtonOpen:
    case StyleAppearance::MozMacDisclosureButtonClosed:
    case StyleAppearance::Spinner:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::Separator:
    case StyleAppearance::Toolbar:
    case StyleAppearance::MozWindowTitlebar:
    case StyleAppearance::Statusbar:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MozMenulistArrowButton:
    case StyleAppearance::Textfield:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Searchfield:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Meter:
    case StyleAppearance::Treeheadercell:
    case StyleAppearance::Treetwisty:
    case StyleAppearance::Treetwistyopen:
    case StyleAppearance::Treeitem:
    case StyleAppearance::Treeview:
    case StyleAppearance::Range:
      return false;

    case StyleAppearance::Textarea:
    case StyleAppearance::Listbox:
    case StyleAppearance::Tab:
    case StyleAppearance::Tabpanels:
      return false;

    default:
      return true;
  }
}

LayoutDeviceIntMargin nsNativeThemeCocoa::DirectionAwareMargin(
    const LayoutDeviceIntMargin& aMargin, nsIFrame* aFrame) {
  // Assuming aMargin was originally specified for a horizontal LTR context,
  // reinterpret the values as logical, and then map to physical coords
  // according to aFrame's actual writing mode.
  WritingMode wm = aFrame->GetWritingMode();
  nsMargin m = LogicalMargin(wm, aMargin.top, aMargin.right, aMargin.bottom,
                             aMargin.left)
                   .GetPhysicalMargin(wm);
  return LayoutDeviceIntMargin(m.top, m.right, m.bottom, m.left);
}

static const LayoutDeviceIntMargin kAquaDropdownBorder(1, 22, 2, 5);
static const LayoutDeviceIntMargin kAquaComboboxBorder(3, 20, 3, 4);
static const LayoutDeviceIntMargin kAquaSearchfieldBorder(3, 5, 2, 19);
static const LayoutDeviceIntMargin kAquaSearchfieldBorderBigSur(5, 5, 4, 26);

LayoutDeviceIntMargin nsNativeThemeCocoa::GetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetWidgetBorder(aContext, aFrame, aAppearance);
  }

  LayoutDeviceIntMargin result;

  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;
  switch (aAppearance) {
    case StyleAppearance::Button: {
      if (IsButtonTypeMenu(aFrame)) {
        result = DirectionAwareMargin(kAquaDropdownBorder, aFrame);
      } else {
        result =
            DirectionAwareMargin(LayoutDeviceIntMargin(1, 7, 3, 7), aFrame);
      }
      break;
    }

    case StyleAppearance::Toolbarbutton: {
      result = DirectionAwareMargin(LayoutDeviceIntMargin(1, 4, 1, 4), aFrame);
      break;
    }

    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio: {
      // nsCheckboxRadioFrame::GetIntrinsicWidth and
      // nsCheckboxRadioFrame::GetIntrinsicHeight assume a border width of 2px.
      result.SizeTo(2, 2, 2, 2);
      break;
    }

    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MozMenulistArrowButton:
      result = DirectionAwareMargin(kAquaDropdownBorder, aFrame);
      break;

    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield: {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricEditTextFrameOutset, &frameOutset);

      SInt32 textPadding = 0;
      ::GetThemeMetric(kThemeMetricEditTextWhitespace, &textPadding);

      frameOutset += textPadding;

      result.SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
      break;
    }

    case StyleAppearance::Textarea:
      result.SizeTo(1, 1, 1, 1);
      break;

    case StyleAppearance::Searchfield: {
      auto border = nsCocoaFeatures::OnBigSurOrLater()
                        ? kAquaSearchfieldBorderBigSur
                        : kAquaSearchfieldBorder;
      result = DirectionAwareMargin(border, aFrame);
      break;
    }

    case StyleAppearance::Listbox: {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricListBoxFrameOutset, &frameOutset);
      result.SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
      break;
    }

    case StyleAppearance::Statusbar:
      result.SizeTo(1, 0, 0, 0);
      break;

    default:
      break;
  }

  if (IsHiDPIContext(aContext)) {
    result = result + result;  // doubled
  }

  NS_OBJC_END_TRY_BLOCK_RETURN(result);
}

// Return false here to indicate that CSS padding values should be used. There
// is no reason to make a distinction between padding and border values, just
// specify whatever values you want in GetWidgetBorder and only use this to
// return true if you want to override CSS padding values.
bool nsNativeThemeCocoa::GetWidgetPadding(nsDeviceContext* aContext,
                                          nsIFrame* aFrame,
                                          StyleAppearance aAppearance,
                                          LayoutDeviceIntMargin* aResult) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetWidgetPadding(aContext, aFrame, aAppearance, aResult);
  }

  // We don't want CSS padding being used for certain widgets.
  // See bug 381639 for an example of why.
  switch (aAppearance) {
    // Radios and checkboxes return a fixed size in GetMinimumWidgetSize
    // and have a meaningful baseline, so they can't have
    // author-specified padding.
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
      aResult->SizeTo(0, 0, 0, 0);
      return true;

    case StyleAppearance::Searchfield:
      if (nsCocoaFeatures::OnBigSurOrLater()) {
        return true;
      }
      break;

    default:
      break;
  }
  return false;
}

bool nsNativeThemeCocoa::GetWidgetOverflow(nsDeviceContext* aContext,
                                           nsIFrame* aFrame,
                                           StyleAppearance aAppearance,
                                           nsRect* aOverflowRect) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return ThemeCocoa::GetWidgetOverflow(aContext, aFrame, aAppearance,
                                         aOverflowRect);
  }
  nsIntMargin overflow;
  switch (aAppearance) {
    case StyleAppearance::Button:
    case StyleAppearance::MozMacDisclosureButtonOpen:
    case StyleAppearance::MozMacDisclosureButtonClosed:
    case StyleAppearance::MozMacHelpButton:
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Searchfield:
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MozMenulistArrowButton:
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
    case StyleAppearance::Tab: {
      overflow.SizeTo(static_cast<int32_t>(kMaxFocusRingWidth),
                      static_cast<int32_t>(kMaxFocusRingWidth),
                      static_cast<int32_t>(kMaxFocusRingWidth),
                      static_cast<int32_t>(kMaxFocusRingWidth));
      break;
    }
    case StyleAppearance::ProgressBar: {
      // Progress bars draw a 2 pixel white shadow under their progress
      // indicators.
      overflow.bottom = 2;
      break;
    }
    case StyleAppearance::Meter: {
      // Meter bars overflow their boxes by about 2 pixels.
      overflow.SizeTo(2, 2, 2, 2);
      break;
    }
    default:
      break;
  }

  if (IsHiDPIContext(aContext)) {
    // Double the number of device pixels.
    overflow += overflow;
  }

  if (overflow != nsIntMargin()) {
    int32_t p2a = aFrame->PresContext()->AppUnitsPerDevPixel();
    aOverflowRect->Inflate(nsMargin(NSIntPixelsToAppUnits(overflow.top, p2a),
                                    NSIntPixelsToAppUnits(overflow.right, p2a),
                                    NSIntPixelsToAppUnits(overflow.bottom, p2a),
                                    NSIntPixelsToAppUnits(overflow.left, p2a)));
    return true;
  }

  return false;
}

LayoutDeviceIntSize nsNativeThemeCocoa::GetMinimumWidgetSize(
    nsPresContext* aPresContext, nsIFrame* aFrame,
    StyleAppearance aAppearance) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return ThemeCocoa::GetMinimumWidgetSize(aPresContext, aFrame, aAppearance);
  }

  LayoutDeviceIntSize result;
  switch (aAppearance) {
    case StyleAppearance::Button: {
      result.SizeTo(pushButtonSettings.minimumSizes[miniControlSize].width,
                    pushButtonSettings.naturalSizes[miniControlSize].height);
      break;
    }

    case StyleAppearance::MozMacDisclosureButtonOpen:
    case StyleAppearance::MozMacDisclosureButtonClosed: {
      result.SizeTo(kDisclosureButtonSize.width, kDisclosureButtonSize.height);
      break;
    }

    case StyleAppearance::MozMacHelpButton: {
      result.SizeTo(kHelpButtonSize.width, kHelpButtonSize.height);
      break;
    }

    case StyleAppearance::Toolbarbutton: {
      result.SizeTo(0, toolbarButtonHeights[miniControlSize]);
      break;
    }

    case StyleAppearance::Spinner:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton: {
      SInt32 buttonHeight = 0, buttonWidth = 0;
      if (aFrame->GetContent()->IsXULElement()) {
        ::GetThemeMetric(kThemeMetricLittleArrowsWidth, &buttonWidth);
        ::GetThemeMetric(kThemeMetricLittleArrowsHeight, &buttonHeight);
      } else {
        NSSize size =
            spinnerSettings
                .minimumSizes[EnumSizeForCocoaSize(NSControlSizeMini)];
        buttonWidth = size.width;
        buttonHeight = size.height;
        if (aAppearance != StyleAppearance::Spinner) {
          // the buttons are half the height of the spinner
          buttonHeight /= 2;
        }
      }
      result.SizeTo(buttonWidth, buttonHeight);
      break;
    }

    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton: {
      SInt32 popupHeight = 0;
      ::GetThemeMetric(kThemeMetricPopupButtonHeight, &popupHeight);
      result.SizeTo(0, popupHeight);
      break;
    }

    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Searchfield: {
      // at minimum, we should be tall enough for 9pt text.
      // I'm using hardcoded values here because the appearance manager
      // values for the frame size are incorrect.
      result.SizeTo(0, (2 + 2) /* top */ + 9 + (1 + 1) /* bottom */);
      break;
    }

    case StyleAppearance::MozWindowButtonBox: {
      NSSize size = WindowButtonsSize(aFrame);
      result.SizeTo(size.width, size.height);
      break;
    }

    case StyleAppearance::ProgressBar: {
      SInt32 barHeight = 0;
      ::GetThemeMetric(kThemeMetricNormalProgressBarThickness, &barHeight);
      result.SizeTo(0, barHeight);
      break;
    }

    case StyleAppearance::Separator: {
      result.SizeTo(1, 1);
      break;
    }

    case StyleAppearance::Treetwisty:
    case StyleAppearance::Treetwistyopen: {
      SInt32 twistyHeight = 0, twistyWidth = 0;
      ::GetThemeMetric(kThemeMetricDisclosureButtonWidth, &twistyWidth);
      ::GetThemeMetric(kThemeMetricDisclosureButtonHeight, &twistyHeight);
      result.SizeTo(twistyWidth, twistyHeight);
      break;
    }

    case StyleAppearance::Treeheader:
    case StyleAppearance::Treeheadercell: {
      SInt32 headerHeight = 0;
      ::GetThemeMetric(kThemeMetricListHeaderHeight, &headerHeight);
      result.SizeTo(0, headerHeight);
      break;
    }

    case StyleAppearance::Tab: {
      result.SizeTo(0, tabHeights[miniControlSize]);
      break;
    }

    case StyleAppearance::RangeThumb: {
      SInt32 width = 0;
      SInt32 height = 0;
      ::GetThemeMetric(kThemeMetricSliderMinThumbWidth, &width);
      ::GetThemeMetric(kThemeMetricSliderMinThumbHeight, &height);
      result.SizeTo(width, height);
      break;
    }

    case StyleAppearance::MozMenulistArrowButton:
      return ThemeCocoa::GetMinimumWidgetSize(aPresContext, aFrame,
                                              aAppearance);

    default:
      break;
  }

  if (IsHiDPIContext(aPresContext->DeviceContext())) {
    result = result * 2;
  }

  return result;

  NS_OBJC_END_TRY_BLOCK_RETURN(LayoutDeviceIntSize());
}

NS_IMETHODIMP
nsNativeThemeCocoa::WidgetStateChanged(nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       nsAtom* aAttribute, bool* aShouldRepaint,
                                       const nsAttrValue* aOldValue) {
  // Some widget types just never change state.
  switch (aAppearance) {
    case StyleAppearance::MozWindowTitlebar:
    case StyleAppearance::Toolbox:
    case StyleAppearance::Toolbar:
    case StyleAppearance::Statusbar:
    case StyleAppearance::Tooltip:
    case StyleAppearance::Tabpanels:
    case StyleAppearance::Tabpanel:
    case StyleAppearance::Dialog:
    case StyleAppearance::Menupopup:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Meter:
    case StyleAppearance::Meterchunk:
      *aShouldRepaint = false;
      return NS_OK;
    default:
      break;
  }

  // XXXdwh Not sure what can really be done here.  Can at least guess for
  // specific widgets that they're highly unlikely to have certain states.
  // For example, a toolbar doesn't care about any states.
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = true;
  } else {
    // Check the attribute to see if it's relevant.
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = false;
    if (aAttribute == nsGkAtoms::disabled || aAttribute == nsGkAtoms::checked ||
        aAttribute == nsGkAtoms::selected ||
        aAttribute == nsGkAtoms::visuallyselected ||
        aAttribute == nsGkAtoms::menuactive ||
        aAttribute == nsGkAtoms::sortDirection ||
        aAttribute == nsGkAtoms::focused || aAttribute == nsGkAtoms::_default ||
        aAttribute == nsGkAtoms::open || aAttribute == nsGkAtoms::hover)
      *aShouldRepaint = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeCocoa::ThemeChanged() {
  // This is unimplemented because we don't care if gecko changes its theme
  // and macOS system appearance changes are handled by
  // nsLookAndFeel::SystemWantsDarkTheme.
  return NS_OK;
}

bool nsNativeThemeCocoa::ThemeSupportsWidget(nsPresContext* aPresContext,
                                             nsIFrame* aFrame,
                                             StyleAppearance aAppearance) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return ThemeCocoa::ThemeSupportsWidget(aPresContext, aFrame, aAppearance);
  }
  // if this is a dropdown button in a combobox the answer is always no
  if (aAppearance == StyleAppearance::MozMenulistArrowButton) {
    nsIFrame* parentFrame = aFrame->GetParent();
    if (parentFrame && parentFrame->IsComboboxControlFrame()) return false;
  }

  switch (aAppearance) {
    // Combobox dropdowns don't support native theming in vertical mode.
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MozMenulistArrowButton:
      if (aFrame && aFrame->GetWritingMode().IsVertical()) {
        return false;
      }
      [[fallthrough]];

    case StyleAppearance::Listbox:
    case StyleAppearance::Dialog:
    case StyleAppearance::MozWindowButtonBox:
    case StyleAppearance::MozWindowTitlebar:
    case StyleAppearance::Menupopup:
    case StyleAppearance::Tooltip:

    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
    case StyleAppearance::MozMacHelpButton:
    case StyleAppearance::MozMacDisclosureButtonOpen:
    case StyleAppearance::MozMacDisclosureButtonClosed:
    case StyleAppearance::Button:
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::Spinner:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
    case StyleAppearance::Toolbar:
    case StyleAppearance::Statusbar:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Searchfield:
    case StyleAppearance::Toolbox:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::Meter:
    case StyleAppearance::Meterchunk:
    case StyleAppearance::Separator:

    case StyleAppearance::Tabpanels:
    case StyleAppearance::Tab:

    case StyleAppearance::Treetwisty:
    case StyleAppearance::Treetwistyopen:
    case StyleAppearance::Treeview:
    case StyleAppearance::Treeheader:
    case StyleAppearance::Treeheadercell:
    case StyleAppearance::Treeheadersortarrow:
    case StyleAppearance::Treeitem:
    case StyleAppearance::Treeline:

    case StyleAppearance::Range:
      return !IsWidgetStyled(aPresContext, aFrame, aAppearance);

    default:
      break;
  }

  return false;
}

bool nsNativeThemeCocoa::WidgetIsContainer(StyleAppearance aAppearance) {
  // flesh this out at some point
  switch (aAppearance) {
    case StyleAppearance::MozMenulistArrowButton:
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Meter:
    case StyleAppearance::Range:
    case StyleAppearance::MozMacHelpButton:
    case StyleAppearance::MozMacDisclosureButtonOpen:
    case StyleAppearance::MozMacDisclosureButtonClosed:
      return false;
    default:
      break;
  }
  return true;
}

bool nsNativeThemeCocoa::ThemeDrawsFocusForWidget(nsIFrame*,
                                                  StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Textarea:
    case StyleAppearance::Textfield:
    case StyleAppearance::Searchfield:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Button:
    case StyleAppearance::MozMacHelpButton:
    case StyleAppearance::MozMacDisclosureButtonOpen:
    case StyleAppearance::MozMacDisclosureButtonClosed:
    case StyleAppearance::Radio:
    case StyleAppearance::Range:
    case StyleAppearance::Checkbox:
      return true;
    default:
      return false;
  }
}

bool nsNativeThemeCocoa::ThemeNeedsComboboxDropmarker() { return false; }

bool nsNativeThemeCocoa::WidgetAppearanceDependsOnWindowFocus(
    StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Dialog:
    case StyleAppearance::Tabpanels:
    case StyleAppearance::Menupopup:
    case StyleAppearance::Tooltip:
    case StyleAppearance::Spinner:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
    case StyleAppearance::Separator:
    case StyleAppearance::Toolbox:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Treeview:
    case StyleAppearance::Treeline:
    case StyleAppearance::Textarea:
    case StyleAppearance::Listbox:
      return false;
    default:
      return true;
  }
}

nsITheme::ThemeGeometryType nsNativeThemeCocoa::ThemeGeometryTypeForWidget(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::MozWindowTitlebar:
      return eThemeGeometryTypeTitlebar;
    case StyleAppearance::Toolbar:
      return eThemeGeometryTypeToolbar;
    case StyleAppearance::Toolbox:
      return eThemeGeometryTypeToolbox;
    case StyleAppearance::MozWindowButtonBox:
      return eThemeGeometryTypeWindowButtons;
    case StyleAppearance::Tooltip:
      return eThemeGeometryTypeTooltip;
    case StyleAppearance::Menupopup:
      return eThemeGeometryTypeMenu;
    default:
      return eThemeGeometryTypeUnknown;
  }
}

nsITheme::Transparency nsNativeThemeCocoa::GetWidgetTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (IsWidgetScrollbarPart(aAppearance)) {
    return ThemeCocoa::GetWidgetTransparency(aFrame, aAppearance);
  }

  switch (aAppearance) {
    case StyleAppearance::Menupopup:
    case StyleAppearance::Tooltip:
    case StyleAppearance::Dialog:
    case StyleAppearance::Toolbar:
      return eTransparent;

    case StyleAppearance::Statusbar:
      // Knowing that scrollbars and statusbars are opaque improves
      // performance, because we create layers for them.
      return eOpaque;

    default:
      return eUnknownTransparency;
  }
}

already_AddRefed<widget::Theme> do_CreateNativeThemeDoNotUseDirectly() {
  return do_AddRef(new nsNativeThemeCocoa());
}
