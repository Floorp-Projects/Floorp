/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeCocoa.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
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
#include "nsNativeBasicTheme.h"
#include "nsNativeThemeColors.h"
#include "nsIScrollableFrame.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EventStates.h"
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
void CUIDraw(CUIRendererRef r, CGRect rect, CGContextRef ctx, CFDictionaryRef options,
             CFDictionaryRef* result);
}

// Workaround for NSCell control tint drawing
// Without this workaround, NSCells are always drawn with the clear control tint
// as long as they're not attached to an NSControl which is a subview of an active window.
// XXXmstange Why doesn't Webkit need this?
@implementation NSCell (ControlTintWorkaround)
- (int)_realControlTint {
  return [self controlTint];
}
@end

// The purpose of this class is to provide objects that can be used when drawing
// NSCells using drawWithFrame:inView: without causing any harm. The only
// messages that will be sent to such an object are "isFlipped" and
// "currentEditor": isFlipped needs to return YES in order to avoid drawing bugs
// on 10.4 (see bug 465069); currentEditor (which isn't even a method of
// NSView) will be called when drawing search fields, and we only provide it in
// order to prevent "unrecognized selector" exceptions.
// There's no need to pass the actual NSView that we're drawing into to
// drawWithFrame:inView:. What's more, doing so even causes unnecessary
// invalidations as soon as we draw a focusring!
@interface CellDrawView : NSView

@end

@implementation CellDrawView

- (BOOL)isFlipped {
  return YES;
}

- (NSText*)currentEditor {
  return nil;
}

@end

// These two classes don't actually add any behavior over NSButtonCell. Their
// purpose is to make it easy to distinguish NSCell objects that are used for
// drawing radio buttons / checkboxes from other cell types.
// The class names are made up, there are no classes with these names in AppKit.
// The reason we need them is that calling [cell setButtonType:NSRadioButton]
// doesn't leave an easy-to-check "marker" on the cell object - there is no
// -[NSButtonCell buttonType] method.
@interface RadioButtonCell : NSButtonCell
@end

@implementation RadioButtonCell
@end

@interface CheckboxCell : NSButtonCell
@end

@implementation CheckboxCell
@end

static void DrawFocusRingForCellIfNeeded(NSCell* aCell, NSRect aWithFrame, NSView* aInView) {
  if ([aCell showsFirstResponder]) {
    CGContextRef cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
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
    CGContextBeginTransparencyLayerWithRect(cgContext, NSRectToCGRect(aWithFrame), 0);
    [aCell drawFocusRingMaskWithFrame:aWithFrame inView:aInView];
    CGContextEndTransparencyLayer(cgContext);

    CGContextRestoreGState(cgContext);
  }
}

static bool FocusIsDrawnByDrawWithFrame(NSCell* aCell) {
#if defined(MAC_OS_X_VERSION_10_8) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_8
  // When building with the 10.8 SDK or higher, focus rings don't draw as part
  // of -[NSCell drawWithFrame:inView:] and must be drawn by a separate call
  // to -[NSCell drawFocusRingMaskWithFrame:inView:]; .
  // See the NSButtonCell section under
  // https://developer.apple.com/library/mac/releasenotes/AppKit/RN-AppKitOlderNotes/#X10_8Notes
  return false;
#else
  // On 10.10, whether the focus ring is drawn as part of
  // -[NSCell drawWithFrame:inView:] depends on the cell type.
  // Radio buttons and checkboxes draw their own focus rings, other cell
  // types need -[NSCell drawFocusRingMaskWithFrame:inView:].
  return
      [aCell isKindOfClass:[RadioButtonCell class]] || [aCell isKindOfClass:[CheckboxCell class]];
#endif
}

static void DrawCellIncludingFocusRing(NSCell* aCell, NSRect aWithFrame, NSView* aInView) {
  [aCell drawWithFrame:aWithFrame inView:aInView];

  if (!FocusIsDrawnByDrawWithFrame(aCell)) {
    DrawFocusRingForCellIfNeeded(aCell, aWithFrame, aInView);
  }
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
  CGContext* cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

  HIThemeTrackDrawInfo tdi;

  tdi.version = 0;
  tdi.min = 0;

  tdi.value = INT32_MAX * (mValue / mMax);
  tdi.max = INT32_MAX;
  tdi.bounds = NSRectToCGRect(cellFrame);
  tdi.attributes = mIsHorizontal ? kThemeTrackHorizontal : 0;
  tdi.enableState =
      [self controlTint] == NSClearControlTint ? kThemeTrackInactive : kThemeTrackActive;

  NSControlSize size = [self controlSize];
  if (size == NSControlSizeRegular) {
    tdi.kind = mIsIndeterminate ? kThemeLargeIndeterminateBar : kThemeLargeProgressBar;
  } else {
    NS_ASSERTION(size == NSControlSizeSmall,
                 "We shouldn't have another size than small and regular for the moment");
    tdi.kind = mIsIndeterminate ? kThemeMediumIndeterminateBar : kThemeMediumProgressBar;
  }

  int32_t stepsPerSecond = mIsIndeterminate ? 60 : 30;
  int32_t milliSecondsPerStep = 1000 / stepsPerSecond;
  tdi.trackInfo.progress.phase =
      uint8_t(PR_IntervalToMilliseconds(PR_IntervalNow()) / milliSecondsPerStep);

  HIThemeDrawTrack(&tdi, NULL, cgContext, kHIThemeOrientationNormal);
}

@end

@interface SearchFieldCellWithFocusRing : NSSearchFieldCell {
}
@end

// Workaround for Bug 542048
// On 64-bit, NSSearchFieldCells don't draw focus rings.
@implementation SearchFieldCellWithFocusRing

- (void)drawWithFrame:(NSRect)rect inView:(NSView*)controlView {
  [super drawWithFrame:rect inView:controlView];

  if (FocusIsDrawnByDrawWithFrame(self)) {
    // For some reason, -[NSSearchFieldCell drawWithFrame:inView] doesn't draw a
    // focus ring in 64 bit mode, no matter what SDK is used or what OS X version
    // we're running on. But if FocusIsDrawnByDrawWithFrame(self), then our
    // caller expects us to draw a focus ring. So we just do that here.
    DrawFocusRingForCellIfNeeded(self, rect, controlView);
  }
}

- (void)drawFocusRingMaskWithFrame:(NSRect)rect inView:(NSView*)controlView {
  // By default this draws nothing. I don't know why.
  // We just draw the search field again. It's a great mask shape for its own
  // focus ring.
  [super drawWithFrame:rect inView:controlView];
}

@end

@interface ToolbarSearchFieldCellWithFocusRing : SearchFieldCellWithFocusRing
@end

@implementation ToolbarSearchFieldCellWithFocusRing

- (BOOL)_isToolbarMode {
  // This function is called during -[NSSearchFieldCell drawWithFrame:inView:].
  // On earlier macOS versions, returning YES from it selects the style
  // that's appropriate for search fields inside toolbars. On Big Sur,
  // returning YES causes the search field to be drawn incorrectly, with
  // the toolbar gradient appearing as the field background.
  if (nsCocoaFeatures::OnBigSurOrLater()) {
    return NO;
  }
  return YES;
}

@end

#define HITHEME_ORIENTATION kHIThemeOrientationNormal

static CGFloat kMaxFocusRingWidth = 0;  // initialized by the nsNativeThemeCocoa constructor

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

static NSWindow* NativeWindowForFrame(nsIFrame* aFrame, nsIWidget** aTopLevelWidget = NULL) {
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
  NSButton* minimizeButton = [window standardWindowButton:NSWindowMiniaturizeButton];
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
  if (topLevelWidget->WindowType() == eWindowType_popup) return YES;
  if ([win isSheet]) return [win isKeyWindow];
  return [win isMainWindow] && ![win attachedSheet];
}

// Toolbar controls and content controls respond to different window
// activeness states.
static BOOL IsActive(nsIFrame* aFrame, BOOL aIsToolbarControl) {
  if (aIsToolbarControl) return [NativeWindowForFrame(aFrame) isMainWindow];
  return FrameIsInActiveWindow(aFrame);
}

static bool IsInSourceList(nsIFrame* aFrame) {
  for (nsIFrame* frame = aFrame->GetParent(); frame;
       frame = nsLayoutUtils::GetCrossDocParentFrame(frame)) {
    if (frame->StyleDisplay()->EffectiveAppearance() == StyleAppearance::MozMacSourceList) {
      return true;
    }
  }
  return false;
}

NS_IMPL_ISUPPORTS_INHERITED(nsNativeThemeCocoa, nsNativeTheme, nsITheme)

nsNativeThemeCocoa::nsNativeThemeCocoa() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  kMaxFocusRingWidth = 7;

  // provide a local autorelease pool, as this is called during startup
  // before the main event-loop pool is in place
  nsAutoreleasePool pool;

  mDisclosureButtonCell = [[NSButtonCell alloc] initTextCell:@""];
  [mDisclosureButtonCell setBezelStyle:NSRoundedDisclosureBezelStyle];
  [mDisclosureButtonCell setButtonType:NSPushOnPushOffButton];
  [mDisclosureButtonCell setHighlightsBy:NSPushInCellMask];

  mHelpButtonCell = [[NSButtonCell alloc] initTextCell:@""];
  [mHelpButtonCell setBezelStyle:NSHelpButtonBezelStyle];
  [mHelpButtonCell setButtonType:NSMomentaryPushInButton];
  [mHelpButtonCell setHighlightsBy:NSPushInCellMask];

  mPushButtonCell = [[NSButtonCell alloc] initTextCell:@""];
  [mPushButtonCell setButtonType:NSMomentaryPushInButton];
  [mPushButtonCell setHighlightsBy:NSPushInCellMask];

  mRadioButtonCell = [[RadioButtonCell alloc] initTextCell:@""];
  [mRadioButtonCell setButtonType:NSRadioButton];

  mCheckboxCell = [[CheckboxCell alloc] initTextCell:@""];
  [mCheckboxCell setButtonType:NSSwitchButton];
  [mCheckboxCell setAllowsMixedState:YES];

  mSearchFieldCell = [[SearchFieldCellWithFocusRing alloc] initTextCell:@""];
  [mSearchFieldCell setBezelStyle:NSTextFieldRoundedBezel];
  [mSearchFieldCell setBezeled:YES];
  [mSearchFieldCell setEditable:YES];
  [mSearchFieldCell setFocusRingType:NSFocusRingTypeExterior];

  mToolbarSearchFieldCell = [[ToolbarSearchFieldCellWithFocusRing alloc] initTextCell:@""];
  [mToolbarSearchFieldCell setBezelStyle:NSTextFieldRoundedBezel];
  [mToolbarSearchFieldCell setBezeled:YES];
  [mToolbarSearchFieldCell setEditable:YES];
  [mToolbarSearchFieldCell setFocusRingType:NSFocusRingTypeExterior];

  mDropdownCell = [[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO];

  mComboBoxCell = [[NSComboBoxCell alloc] initTextCell:@""];
  [mComboBoxCell setBezeled:YES];
  [mComboBoxCell setEditable:YES];
  [mComboBoxCell setFocusRingType:NSFocusRingTypeExterior];

  mProgressBarCell = [[NSProgressBarCell alloc] init];

  mMeterBarCell = [[NSLevelIndicatorCell alloc]
      initWithLevelIndicatorStyle:NSContinuousCapacityLevelIndicatorStyle];

  mCellDrawView = [[CellDrawView alloc] init];

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
  [mSearchFieldCell release];
  [mToolbarSearchFieldCell release];
  [mDropdownCell release];
  [mComboBoxCell release];
  [mCellDrawView release];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Limit on the area of the target rect (in pixels^2) in
// DrawCellWithScaling() and DrawButton() and above which we
// don't draw the object into a bitmap buffer.  This is to avoid crashes in
// [NSGraphicsContext graphicsContextWithGraphicsPort:flipped:] and
// CGContextDrawImage(), and also to avoid very poor drawing performance in
// CGContextDrawImage() when it scales the bitmap (particularly if xscale or
// yscale is less than but near 1 -- e.g. 0.9).  This value was determined
// by trial and error, on OS X 10.4.11 and 10.5.4, and on systems with
// different amounts of RAM.
#define BITMAP_MAX_AREA 500000

static int GetBackingScaleFactorForRendering(CGContextRef cgContext) {
  CGAffineTransform ctm = CGContextGetUserSpaceToDeviceSpaceTransform(cgContext);
  CGRect transformedUserSpacePixel = CGRectApplyAffineTransform(CGRectMake(0, 0, 1, 1), ctm);
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
static void DrawCellWithScaling(NSCell* cell, CGContextRef cgContext, const HIRect& destRect,
                                NSControlSize controlSize, NSSize naturalSize, NSSize minimumSize,
                                const float marginSet[][3][4], NSView* view,
                                BOOL mirrorHorizontal) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NSRect drawRect =
      NSMakeRect(destRect.origin.x, destRect.origin.y, destRect.size.width, destRect.size.height);

  if (naturalSize.width != 0.0f) drawRect.size.width = naturalSize.width;
  if (naturalSize.height != 0.0f) drawRect.size.height = naturalSize.height;

  // Keep aspect ratio when scaling if one dimension is free.
  if (naturalSize.width == 0.0f && naturalSize.height != 0.0f)
    drawRect.size.width = destRect.size.width * naturalSize.height / destRect.size.height;
  if (naturalSize.height == 0.0f && naturalSize.width != 0.0f)
    drawRect.size.height = destRect.size.height * naturalSize.width / destRect.size.width;

  // Honor minimum sizes.
  if (drawRect.size.width < minimumSize.width) drawRect.size.width = minimumSize.width;
  if (drawRect.size.height < minimumSize.height) drawRect.size.height = minimumSize.height;

  [NSGraphicsContext saveGraphicsState];

  // Only skip the buffer if the area of our cell (in pixels^2) is too large.
  if (drawRect.size.width * drawRect.size.height > BITMAP_MAX_AREA) {
    // Inflate the rect Gecko gave us by the margin for the control.
    InflateControlRect(&drawRect, controlSize, marginSet);

    NSGraphicsContext* savedContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext
        setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:cgContext
                                                                     flipped:YES]];

    DrawCellIncludingFocusRing(cell, drawRect, view);

    [NSGraphicsContext setCurrentContext:savedContext];
  } else {
    float w = ceil(drawRect.size.width);
    float h = ceil(drawRect.size.height);
    NSRect tmpRect = NSMakeRect(kMaxFocusRingWidth, kMaxFocusRingWidth, w, h);

    // inflate to figure out the frame we need to tell NSCell to draw in, to get something that's
    // 0,0,w,h
    InflateControlRect(&tmpRect, controlSize, marginSet);

    // and then, expand by kMaxFocusRingWidth size to make sure we can capture any focus ring
    w += kMaxFocusRingWidth * 2.0;
    h += kMaxFocusRingWidth * 2.0;

    int backingScaleFactor = GetBackingScaleFactorForRendering(cgContext);
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        NULL, (int)w * backingScaleFactor, (int)h * backingScaleFactor, 8,
        (int)w * backingScaleFactor * 4, rgb, kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(rgb);

    // We need to flip the image twice in order to avoid drawing bugs on 10.4, see bug 465069.
    // This is the first flip transform, applied to cgContext.
    CGContextScaleCTM(cgContext, 1.0f, -1.0f);
    CGContextTranslateCTM(cgContext, 0.0f, -(2.0 * destRect.origin.y + destRect.size.height));
    if (mirrorHorizontal) {
      CGContextScaleCTM(cgContext, -1.0f, 1.0f);
      CGContextTranslateCTM(cgContext, -(2.0 * destRect.origin.x + destRect.size.width), 0.0f);
    }

    NSGraphicsContext* savedContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:ctx
                                                                                    flipped:YES]];

    CGContextScaleCTM(ctx, backingScaleFactor, backingScaleFactor);

    // Set the context's "base transform" to in order to get correctly-sized focus rings.
    CGContextSetBaseCTM(ctx, CGAffineTransformMakeScale(backingScaleFactor, backingScaleFactor));

    // This is the second flip transform, applied to ctx.
    CGContextScaleCTM(ctx, 1.0f, -1.0f);
    CGContextTranslateCTM(ctx, 0.0f, -(2.0 * tmpRect.origin.y + tmpRect.size.height));

    DrawCellIncludingFocusRing(cell, tmpRect, view);

    [NSGraphicsContext setCurrentContext:savedContext];

    CGImageRef img = CGBitmapContextCreateImage(ctx);

    // Drop the image into the original destination rectangle, scaling to fit
    // Only scale kMaxFocusRingWidth by xscale/yscale when the resulting rect
    // doesn't extend beyond the overflow rect
    float xscale = destRect.size.width / drawRect.size.width;
    float yscale = destRect.size.height / drawRect.size.height;
    float scaledFocusRingX = xscale < 1.0f ? kMaxFocusRingWidth * xscale : kMaxFocusRingWidth;
    float scaledFocusRingY = yscale < 1.0f ? kMaxFocusRingWidth * yscale : kMaxFocusRingWidth;
    CGContextDrawImage(
        cgContext,
        CGRectMake(destRect.origin.x - scaledFocusRingX, destRect.origin.y - scaledFocusRingY,
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
  // with the first dimension being the OS version ([0] 10.6-10.9, [1] 10.10 and above),
  // the second being the control size (mini, small, regular), and the third
  // being the 4 margin values (left, top, right, bottom).
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
static NSControlSize FindControlSize(CGFloat size, const CGFloat* sizes, CGFloat tolerance) {
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
static void DrawCellWithSnapping(NSCell* cell, CGContextRef cgContext, const HIRect& destRect,
                                 const CellRenderSettings settings, float verticalAlignFactor,
                                 NSView* view, BOOL mirrorHorizontal, float snapTolerance = 2.0f) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  const float rectWidth = destRect.size.width, rectHeight = destRect.size.height;
  const NSSize* sizes = settings.naturalSizes;
  const NSSize miniSize = sizes[EnumSizeForCocoaSize(NSControlSizeMini)];
  const NSSize smallSize = sizes[EnumSizeForCocoaSize(NSControlSizeSmall)];
  const NSSize regularSize = sizes[EnumSizeForCocoaSize(NSControlSizeRegular)];

  HIRect drawRect = destRect;

  CGFloat controlWidths[3] = {miniSize.width, smallSize.width, regularSize.width};
  NSControlSize controlSizeX = FindControlSize(rectWidth, controlWidths, snapTolerance);
  CGFloat controlHeights[3] = {miniSize.height, smallSize.height, regularSize.height};
  NSControlSize controlSizeY = FindControlSize(rectHeight, controlHeights, snapTolerance);

  NSControlSize controlSize = NSControlSizeRegular;
  size_t sizeIndex = 0;

  // At some sizes, don't scale but snap.
  const NSControlSize smallerControlSize =
      EnumSizeForCocoaSize(controlSizeX) < EnumSizeForCocoaSize(controlSizeY) ? controlSizeX
                                                                              : controlSizeY;
  const size_t smallerControlSizeIndex = EnumSizeForCocoaSize(smallerControlSize);
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
      drawRect.origin.x += ceil((destRect.size.width - sizes[sizeIndex].width) / 2);
      drawRect.size.width = sizes[sizeIndex].width;
    }
    if (sizes[sizeIndex].height) {
      drawRect.origin.y +=
          floor((destRect.size.height - sizes[sizeIndex].height) * verticalAlignFactor);
      drawRect.size.height = sizes[sizeIndex].height;
    }
  } else {
    // Use the larger control size.
    controlSize = EnumSizeForCocoaSize(controlSizeX) > EnumSizeForCocoaSize(controlSizeY)
                      ? controlSizeX
                      : controlSizeY;
    sizeIndex = EnumSizeForCocoaSize(controlSize);
  }

  [cell setControlSize:controlSize];

  MOZ_ASSERT(sizeIndex < ArrayLength(settings.minimumSizes));
  const NSSize minimumSize = settings.minimumSizes[sizeIndex];
  DrawCellWithScaling(cell, cgContext, drawRect, controlSize, sizes[sizeIndex], minimumSize,
                      settings.margins, view, mirrorHorizontal);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

@interface NSWindow (CoreUIRendererPrivate)
+ (CUIRendererRef)coreUIRenderer;
@end

static id GetAquaAppearance() {
  Class NSAppearanceClass = NSClassFromString(@"NSAppearance");
  if (NSAppearanceClass && [NSAppearanceClass respondsToSelector:@selector(appearanceNamed:)]) {
    return [NSAppearanceClass performSelector:@selector(appearanceNamed:)
                                   withObject:@"NSAppearanceNameAqua"];
  }
  return nil;
}

@interface NSObject (NSAppearanceCoreUIRendering)
- (void)_drawInRect:(CGRect)rect context:(CGContextRef)cgContext options:(id)options;
@end

static void RenderWithCoreUI(CGRect aRect, CGContextRef cgContext, NSDictionary* aOptions,
                             bool aSkipAreaCheck = false) {
  id appearance = GetAquaAppearance();

  if (!aSkipAreaCheck && aRect.size.width * aRect.size.height > BITMAP_MAX_AREA) {
    return;
  }

  if (appearance && [appearance respondsToSelector:@selector(_drawInRect:context:options:)]) {
    // Render through NSAppearance on Mac OS 10.10 and up. This will call
    // CUIDraw with a CoreUI renderer that will give us the correct 10.10
    // style. Calling CUIDraw directly with [NSWindow coreUIRenderer] still
    // renders 10.9-style widgets on 10.10.
    [appearance _drawInRect:aRect context:cgContext options:aOptions];
  } else {
    // 10.9 and below
    CUIRendererRef renderer =
        [NSWindow respondsToSelector:@selector(coreUIRenderer)] ? [NSWindow coreUIRenderer] : nil;
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

static void ApplyControlParamsToNSCell(nsNativeThemeCocoa::ControlParams aControlParams,
                                       NSCell* aCell) {
  [aCell setEnabled:!aControlParams.disabled];
  [aCell setShowsFirstResponder:(aControlParams.focused && !aControlParams.disabled &&
                                 aControlParams.insideActiveWindow)];
  [aCell setHighlighted:aControlParams.pressed];
}

// These are the sizes that Gecko needs to request to draw if it wants
// to get a standard-sized Aqua radio button drawn. Note that the rects
// that draw these are actually a little bigger.
static const CellRenderSettings radioSettings = {{
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

static const CellRenderSettings checkboxSettings = {{
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

static NSCellStateValue CellStateForCheckboxOrRadioState(
    nsNativeThemeCocoa::CheckboxOrRadioState aState) {
  switch (aState) {
    case nsNativeThemeCocoa::CheckboxOrRadioState::eOff:
      return NSOffState;
    case nsNativeThemeCocoa::CheckboxOrRadioState::eOn:
      return NSOnState;
    case nsNativeThemeCocoa::CheckboxOrRadioState::eIndeterminate:
      return NSMixedState;
  }
}

void nsNativeThemeCocoa::DrawCheckboxOrRadio(CGContextRef cgContext, bool inCheckbox,
                                             const HIRect& inBoxRect,
                                             const CheckboxOrRadioParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NSButtonCell* cell = inCheckbox ? mCheckboxCell : mRadioButtonCell;
  ApplyControlParamsToNSCell(aParams.controlParams, cell);

  [cell setState:CellStateForCheckboxOrRadioState(aParams.state)];
  [cell setControlTint:(aParams.controlParams.insideActiveWindow ? [NSColor currentControlTint]
                                                                 : NSClearControlTint)];

  // Ensure that the control is square.
  float length = std::min(inBoxRect.size.width, inBoxRect.size.height);
  HIRect drawRect = CGRectMake(inBoxRect.origin.x + (int)((inBoxRect.size.width - length) / 2.0f),
                               inBoxRect.origin.y + (int)((inBoxRect.size.height - length) / 2.0f),
                               length, length);

  DrawCellWithSnapping(cell, cgContext, drawRect, inCheckbox ? checkboxSettings : radioSettings,
                       aParams.verticalAlignFactor, mCellDrawView, NO);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static const CellRenderSettings searchFieldSettings = {{
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

  if (content->IsAnyOfXULElements(nsGkAtoms::toolbar, nsGkAtoms::toolbox, nsGkAtoms::statusbar)) {
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

nsNativeThemeCocoa::SearchFieldParams nsNativeThemeCocoa::ComputeSearchFieldParams(
    nsIFrame* aFrame, EventStates aEventState) {
  SearchFieldParams params;
  params.insideToolbar = IsInsideToolbar(aFrame);
  params.disabled = IsDisabled(aFrame, aEventState);
  params.focused = IsFocused(aFrame);
  params.rtl = IsFrameRTL(aFrame);
  params.verticalAlignFactor = VerticalAlignFactor(aFrame);
  return params;
}

void nsNativeThemeCocoa::DrawSearchField(CGContextRef cgContext, const HIRect& inBoxRect,
                                         const SearchFieldParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NSSearchFieldCell* cell = aParams.insideToolbar ? mToolbarSearchFieldCell : mSearchFieldCell;
  [cell setEnabled:!aParams.disabled];
  [cell setShowsFirstResponder:aParams.focused];

  // When using the 10.11 SDK, the default string will be shown if we don't
  // set the placeholder string.
  [cell setPlaceholderString:@""];

  DrawCellWithSnapping(cell, cgContext, inBoxRect, searchFieldSettings, aParams.verticalAlignFactor,
                       mCellDrawView, aParams.rtl);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static const NSSize kCheckmarkSize = NSMakeSize(11, 11);
static const NSSize kMenuarrowSize = NSMakeSize(9, 10);
static const NSSize kMenuScrollArrowSize = NSMakeSize(10, 8);
static NSString* kCheckmarkImage = @"MenuOnState";
static NSString* kMenuarrowRightImage = @"MenuSubmenu";
static NSString* kMenuarrowLeftImage = @"MenuSubmenuLeft";
static NSString* kMenuDownScrollArrowImage = @"MenuScrollDown";
static NSString* kMenuUpScrollArrowImage = @"MenuScrollUp";
static const CGFloat kMenuIconIndent = 6.0f;

NSString* nsNativeThemeCocoa::GetMenuIconName(const MenuIconParams& aParams) {
  switch (aParams.icon) {
    case MenuIcon::eCheckmark:
      return kCheckmarkImage;
    case MenuIcon::eMenuArrow:
      return aParams.rtl ? kMenuarrowLeftImage : kMenuarrowRightImage;
    case MenuIcon::eMenuDownScrollArrow:
      return kMenuDownScrollArrowImage;
    case MenuIcon::eMenuUpScrollArrow:
      return kMenuUpScrollArrowImage;
  }
}

NSSize nsNativeThemeCocoa::GetMenuIconSize(MenuIcon aIcon) {
  switch (aIcon) {
    case MenuIcon::eCheckmark:
      return kCheckmarkSize;
    case MenuIcon::eMenuArrow:
      return kMenuarrowSize;
    case MenuIcon::eMenuDownScrollArrow:
    case MenuIcon::eMenuUpScrollArrow:
      return kMenuScrollArrowSize;
  }
}

nsNativeThemeCocoa::MenuIconParams nsNativeThemeCocoa::ComputeMenuIconParams(
    nsIFrame* aFrame, EventStates aEventState, MenuIcon aIcon) {
  bool isDisabled = IsDisabled(aFrame, aEventState);

  MenuIconParams params;
  params.icon = aIcon;
  params.disabled = isDisabled;
  params.insideActiveMenuItem = !isDisabled && CheckBooleanAttr(aFrame, nsGkAtoms::menuactive);
  params.centerHorizontally = true;
  params.rtl = IsFrameRTL(aFrame);
  return params;
}

void nsNativeThemeCocoa::DrawMenuIcon(CGContextRef cgContext, const CGRect& aRect,
                                      const MenuIconParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NSSize size = GetMenuIconSize(aParams.icon);

  // Adjust size and position of our drawRect.
  CGFloat paddingX = std::max(CGFloat(0.0), aRect.size.width - size.width);
  CGFloat paddingY = std::max(CGFloat(0.0), aRect.size.height - size.height);
  CGFloat paddingStartX = std::min(paddingX, kMenuIconIndent);
  CGFloat paddingEndX = std::max(CGFloat(0.0), paddingX - kMenuIconIndent);
  CGRect drawRect = CGRectMake(aRect.origin.x + (aParams.centerHorizontally ? ceil(paddingX / 2)
                                                 : aParams.rtl              ? paddingEndX
                                                                            : paddingStartX),
                               aRect.origin.y + ceil(paddingY / 2), size.width, size.height);

  NSString* state =
      aParams.disabled ? @"disabled" : (aParams.insideActiveMenuItem ? @"pressed" : @"normal");

  NSString* imageName = GetMenuIconName(aParams);

  RenderWithCoreUI(
      drawRect, cgContext,
      [NSDictionary dictionaryWithObjectsAndKeys:@"kCUIBackgroundTypeMenu", @"backgroundTypeKey",
                                                 imageName, @"imageNameKey", state, @"state",
                                                 @"image", @"widget", [NSNumber numberWithBool:YES],
                                                 @"is.flipped", nil]);

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, drawRect);
#endif

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

nsNativeThemeCocoa::MenuItemParams nsNativeThemeCocoa::ComputeMenuItemParams(
    nsIFrame* aFrame, EventStates aEventState, bool aIsChecked) {
  bool isDisabled = IsDisabled(aFrame, aEventState);

  MenuItemParams params;
  params.checked = aIsChecked;
  params.disabled = isDisabled;
  params.selected = !isDisabled && CheckBooleanAttr(aFrame, nsGkAtoms::menuactive);
  params.rtl = IsFrameRTL(aFrame);
  return params;
}

static void SetCGContextFillColor(CGContextRef cgContext, const sRGBColor& aColor) {
  DeviceColor color = ToDeviceColor(aColor);
  CGContextSetRGBFillColor(cgContext, color.r, color.g, color.b, color.a);
}

void nsNativeThemeCocoa::DrawMenuItem(CGContextRef cgContext, const CGRect& inBoxRect,
                                      const MenuItemParams& aParams) {
  if (aParams.checked) {
    MenuIconParams params;
    params.disabled = aParams.disabled;
    params.insideActiveMenuItem = aParams.selected;
    params.rtl = aParams.rtl;
    params.icon = MenuIcon::eCheckmark;
    DrawMenuIcon(cgContext, inBoxRect, params);
  }
}

void nsNativeThemeCocoa::DrawMenuSeparator(CGContextRef cgContext, const CGRect& inBoxRect,
                                           const MenuItemParams& aParams) {
  // Workaround for visual artifacts issues with
  // HIThemeDrawMenuSeparator on macOS Big Sur.
  if (nsCocoaFeatures::OnBigSurOrLater()) {
    CGRect separatorRect = inBoxRect;
    separatorRect.size.height = 1;
    separatorRect.size.width -= 42;
    separatorRect.origin.x += 21;
    // Use transparent black with an alpha similar to the native separator.
    // The values 231 (menu background) and 205 (separator color) have been
    // sampled from a window screenshot of a native context menu.
    CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.0, (231 - 205) / 231.0);
    CGContextFillRect(cgContext, separatorRect);
    return;
  }

  ThemeMenuState menuState;
  if (aParams.disabled) {
    menuState = kThemeMenuDisabled;
  } else {
    menuState = aParams.selected ? kThemeMenuSelected : kThemeMenuActive;
  }

  HIThemeMenuItemDrawInfo midi = {0, kThemeMenuItemPlain, menuState};
  HIThemeDrawMenuSeparator(&inBoxRect, &inBoxRect, &midi, cgContext, HITHEME_ORIENTATION);
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
    nsIFrame* aFrame, EventStates aEventState) {
  ControlParams params;
  params.disabled = IsDisabled(aFrame, aEventState);
  params.insideActiveWindow = FrameIsInActiveWindow(aFrame);
  params.pressed = aEventState.HasAllStates(NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_HOVER);
  params.focused = aEventState.HasState(NS_EVENT_STATE_FOCUS) &&
                   (aEventState.HasState(NS_EVENT_STATE_FOCUSRING) ||
                    ShouldUnconditionallyDrawFocusRingIfFocused(aFrame));
  params.rtl = IsFrameRTL(aFrame);
  return params;
}

static const NSSize kHelpButtonSize = NSMakeSize(20, 20);
static const NSSize kDisclosureButtonSize = NSMakeSize(21, 21);

static const CellRenderSettings pushButtonSettings = {{
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
// Rounded buttons look bad if drawn at a height greater than 26, so at that point
// we switch over to doing square buttons which looks fine at any size.
#define DO_SQUARE_BUTTON_HEIGHT 26

void nsNativeThemeCocoa::DrawRoundedBezelPushButton(CGContextRef cgContext, const HIRect& inBoxRect,
                                                    ControlParams aControlParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  ApplyControlParamsToNSCell(aControlParams, mPushButtonCell);
  [mPushButtonCell setBezelStyle:NSRoundedBezelStyle];
  DrawCellWithSnapping(mPushButtonCell, cgContext, inBoxRect, pushButtonSettings, 0.5f,
                       mCellDrawView, aControlParams.rtl, 1.0f);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawSquareBezelPushButton(CGContextRef cgContext, const HIRect& inBoxRect,
                                                   ControlParams aControlParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  ApplyControlParamsToNSCell(aControlParams, mPushButtonCell);
  [mPushButtonCell setBezelStyle:NSShadowlessSquareBezelStyle];
  DrawCellWithScaling(mPushButtonCell, cgContext, inBoxRect, NSControlSizeRegular, NSZeroSize,
                      NSMakeSize(14, 0), NULL, mCellDrawView, aControlParams.rtl);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawHelpButton(CGContextRef cgContext, const HIRect& inBoxRect,
                                        ControlParams aControlParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  ApplyControlParamsToNSCell(aControlParams, mHelpButtonCell);
  DrawCellWithScaling(mHelpButtonCell, cgContext, inBoxRect, NSControlSizeRegular, NSZeroSize,
                      kHelpButtonSize, NULL, mCellDrawView,
                      false);  // Don't mirror icon in RTL.

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawDisclosureButton(CGContextRef cgContext, const HIRect& inBoxRect,
                                              ControlParams aControlParams,
                                              NSCellStateValue aCellState) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  ApplyControlParamsToNSCell(aControlParams, mDisclosureButtonCell);
  [mDisclosureButtonCell setState:aCellState];
  DrawCellWithScaling(mDisclosureButtonCell, cgContext, inBoxRect, NSControlSizeRegular, NSZeroSize,
                      kDisclosureButtonSize, NULL, mCellDrawView,
                      false);  // Don't mirror icon in RTL.

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawFocusOutline(CGContextRef cgContext, const HIRect& inBoxRect) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;
  NSGraphicsContext* savedContext = [NSGraphicsContext currentContext];
  [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:cgContext
                                                                                  flipped:YES]];
  CGContextSaveGState(cgContext);
  NSSetFocusRingStyle(NSFocusRingOnly);
  NSRectFill(NSRectFromCGRect(inBoxRect));
  CGContextRestoreGState(cgContext);
  [NSGraphicsContext setCurrentContext:savedContext];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

typedef void (*RenderHIThemeControlFunction)(CGContextRef cgContext, const HIRect& aRenderRect,
                                             void* aData);

static void RenderTransformedHIThemeControl(CGContextRef aCGContext, const HIRect& aRect,
                                            RenderHIThemeControlFunction aFunc, void* aData,
                                            BOOL mirrorHorizontally = NO) {
  CGAffineTransform savedCTM = CGContextGetCTM(aCGContext);
  CGContextTranslateCTM(aCGContext, aRect.origin.x, aRect.origin.y);

  bool drawDirect;
  HIRect drawRect = aRect;
  drawRect.origin = CGPointZero;

  if (!mirrorHorizontally && savedCTM.a == 1.0f && savedCTM.b == 0.0f && savedCTM.c == 0.0f &&
      (savedCTM.d == 1.0f || savedCTM.d == -1.0f)) {
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
        NULL, w * backingScaleFactor, h * backingScaleFactor, 8, w * backingScaleFactor * 4,
        colorSpace, kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(colorSpace);

    CGContextScaleCTM(bitmapctx, backingScaleFactor, backingScaleFactor);
    CGContextTranslateCTM(bitmapctx, kMaxFocusRingWidth, kMaxFocusRingWidth);

    // Set the context's "base transform" to in order to get correctly-sized focus rings.
    CGContextSetBaseCTM(bitmapctx,
                        CGAffineTransformMakeScale(backingScaleFactor, backingScaleFactor));

    // HITheme always wants to draw into a flipped context, or things
    // get confused.
    CGContextTranslateCTM(bitmapctx, 0.0f, aRect.size.height);
    CGContextScaleCTM(bitmapctx, 1.0f, -1.0f);

    aFunc(bitmapctx, drawRect, aData);

    CGImageRef bitmap = CGBitmapContextCreateImage(bitmapctx);

    CGAffineTransform ctm = CGContextGetCTM(aCGContext);

    // We need to unflip, so that we can do a DrawImage without getting a flipped image.
    CGContextTranslateCTM(aCGContext, 0.0f, aRect.size.height);
    CGContextScaleCTM(aCGContext, 1.0f, -1.0f);

    if (mirrorHorizontally) {
      CGContextTranslateCTM(aCGContext, aRect.size.width, 0);
      CGContextScaleCTM(aCGContext, -1.0f, 1.0f);
    }

    HIRect inflatedDrawRect = CGRectMake(-kMaxFocusRingWidth, -kMaxFocusRingWidth, w, h);
    CGContextDrawImage(aCGContext, inflatedDrawRect, bitmap);

    CGContextSetCTM(aCGContext, ctm);

    CGImageRelease(bitmap);
    CGContextRelease(bitmapctx);
  }

  CGContextSetCTM(aCGContext, savedCTM);
}

static void RenderButton(CGContextRef cgContext, const HIRect& aRenderRect, void* aData) {
  HIThemeButtonDrawInfo* bdi = (HIThemeButtonDrawInfo*)aData;
  HIThemeDrawButton(&aRenderRect, bdi, cgContext, kHIThemeOrientationNormal, NULL);
}

static ThemeDrawState ToThemeDrawState(const nsNativeThemeCocoa::ControlParams& aParams) {
  if (aParams.disabled) {
    return kThemeStateUnavailable;
  }
  if (aParams.pressed) {
    return kThemeStatePressed;
  }
  return kThemeStateActive;
}

void nsNativeThemeCocoa::DrawHIThemeButton(CGContextRef cgContext, const HIRect& aRect,
                                           ThemeButtonKind aKind, ThemeButtonValue aValue,
                                           ThemeDrawState aState, ThemeButtonAdornment aAdornment,
                                           const ControlParams& aParams) {
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

  if ((aAdornment & kThemeAdornmentDefault) && !aParams.disabled) {
    bdi.animation.time.start = 0;
    bdi.animation.time.current = CFAbsoluteTimeGetCurrent();
  }

  RenderTransformedHIThemeControl(cgContext, aRect, RenderButton, &bdi, aParams.rtl);

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawButton(CGContextRef cgContext, const HIRect& inBoxRect,
                                    const ButtonParams& aParams) {
  ControlParams controlParams = aParams.controlParams;

  switch (aParams.button) {
    case ButtonType::eRegularPushButton:
    case ButtonType::eDefaultPushButton: {
      ThemeButtonAdornment adornment = aParams.button == ButtonType::eDefaultPushButton
                                           ? kThemeAdornmentDefault
                                           : kThemeAdornmentNone;
      HIRect drawFrame = inBoxRect;
      drawFrame.size.height -= 2;
      if (inBoxRect.size.height >= pushButtonSettings.naturalSizes[regularControlSize].height) {
        DrawHIThemeButton(cgContext, drawFrame, kThemePushButton, kThemeButtonOff,
                          ToThemeDrawState(controlParams), adornment, controlParams);
        return;
      }
      if (inBoxRect.size.height >= pushButtonSettings.naturalSizes[smallControlSize].height) {
        drawFrame.origin.y -= 1;
        drawFrame.origin.x += 1;
        drawFrame.size.width -= 2;
        DrawHIThemeButton(cgContext, drawFrame, kThemePushButtonSmall, kThemeButtonOff,
                          ToThemeDrawState(controlParams), adornment, controlParams);
        return;
      }
      DrawHIThemeButton(cgContext, drawFrame, kThemePushButtonMini, kThemeButtonOff,
                        ToThemeDrawState(controlParams), adornment, controlParams);
      return;
    }
    case ButtonType::eRegularBevelButton:
    case ButtonType::eDefaultBevelButton: {
      ThemeButtonAdornment adornment = aParams.button == ButtonType::eDefaultBevelButton
                                           ? kThemeAdornmentDefault
                                           : kThemeAdornmentNone;
      DrawHIThemeButton(cgContext, inBoxRect, kThemeMediumBevelButton, kThemeButtonOff,
                        ToThemeDrawState(controlParams), adornment, controlParams);
      return;
    }
    case ButtonType::eRoundedBezelPushButton:
      DrawRoundedBezelPushButton(cgContext, inBoxRect, controlParams);
      return;
    case ButtonType::eSquareBezelPushButton:
      DrawSquareBezelPushButton(cgContext, inBoxRect, controlParams);
      return;
    case ButtonType::eArrowButton:
      DrawHIThemeButton(cgContext, inBoxRect, kThemeArrowButton, kThemeButtonOn,
                        kThemeStateUnavailable, kThemeAdornmentArrowDownArrow, controlParams);
      return;
    case ButtonType::eHelpButton:
      DrawHelpButton(cgContext, inBoxRect, controlParams);
      return;
    case ButtonType::eTreeTwistyPointingRight:
      DrawHIThemeButton(cgContext, inBoxRect, kThemeDisclosureButton, kThemeDisclosureRight,
                        ToThemeDrawState(controlParams), kThemeAdornmentNone, controlParams);
      return;
    case ButtonType::eTreeTwistyPointingDown:
      DrawHIThemeButton(cgContext, inBoxRect, kThemeDisclosureButton, kThemeDisclosureDown,
                        ToThemeDrawState(controlParams), kThemeAdornmentNone, controlParams);
      return;
    case ButtonType::eDisclosureButtonClosed:
      DrawDisclosureButton(cgContext, inBoxRect, controlParams, NSOffState);
      return;
    case ButtonType::eDisclosureButtonOpen:
      DrawDisclosureButton(cgContext, inBoxRect, controlParams, NSOnState);
      return;
  }
}

nsNativeThemeCocoa::TreeHeaderCellParams nsNativeThemeCocoa::ComputeTreeHeaderCellParams(
    nsIFrame* aFrame, EventStates aEventState) {
  TreeHeaderCellParams params;
  params.controlParams = ComputeControlParams(aFrame, aEventState);
  params.sortDirection = GetTreeSortDirection(aFrame);
  params.lastTreeHeaderCell = IsLastTreeHeaderCell(aFrame);
  return params;
}

void nsNativeThemeCocoa::DrawTreeHeaderCell(CGContextRef cgContext, const HIRect& inBoxRect,
                                            const TreeHeaderCellParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  HIThemeButtonDrawInfo bdi;
  bdi.version = 0;
  bdi.kind = kThemeListHeaderButton;
  bdi.value = kThemeButtonOff;
  bdi.adornment = kThemeAdornmentNone;

  switch (aParams.sortDirection) {
    case eTreeSortDirection_Natural:
      break;
    case eTreeSortDirection_Ascending:
      bdi.value = kThemeButtonOn;
      bdi.adornment = kThemeAdornmentHeaderButtonSortUp;
      break;
    case eTreeSortDirection_Descending:
      bdi.value = kThemeButtonOn;
      break;
  }

  if (aParams.controlParams.disabled) {
    bdi.state = kThemeStateUnavailable;
  } else if (aParams.controlParams.pressed) {
    bdi.state = kThemeStatePressed;
  } else if (!aParams.controlParams.insideActiveWindow) {
    bdi.state = kThemeStateInactive;
  } else {
    bdi.state = kThemeStateActive;
  }

  CGContextClipToRect(cgContext, inBoxRect);

  HIRect drawFrame = inBoxRect;
  // Always remove the top border.
  drawFrame.origin.y -= 1;
  drawFrame.size.height += 1;
  // Remove the left border in LTR mode and the right border in RTL mode.
  drawFrame.size.width += 1;
  if (aParams.lastTreeHeaderCell) {
    drawFrame.size.width += 1;  // Also remove the other border.
  }
  if (!aParams.controlParams.rtl || aParams.lastTreeHeaderCell) {
    drawFrame.origin.x -= 1;
  }

  RenderTransformedHIThemeControl(cgContext, drawFrame, RenderButton, &bdi,
                                  aParams.controlParams.rtl);

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static const CellRenderSettings dropdownSettings = {{
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

static const CellRenderSettings editableMenulistSettings = {{
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

void nsNativeThemeCocoa::DrawDropdown(CGContextRef cgContext, const HIRect& inBoxRect,
                                      const DropdownParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mDropdownCell setPullsDown:aParams.pullsDown];
  NSCell* cell = aParams.editable ? (NSCell*)mComboBoxCell : (NSCell*)mDropdownCell;

  ApplyControlParamsToNSCell(aParams.controlParams, cell);

  if (aParams.controlParams.insideActiveWindow) {
    [cell setControlTint:[NSColor currentControlTint]];
  } else {
    [cell setControlTint:NSClearControlTint];
  }

  const CellRenderSettings& settings =
      aParams.editable ? editableMenulistSettings : dropdownSettings;
  DrawCellWithSnapping(cell, cgContext, inBoxRect, settings, 0.5f, mCellDrawView,
                       aParams.controlParams.rtl);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static const CellRenderSettings spinnerSettings = {
    {
        NSMakeSize(11, 16),  // mini (width trimmed by 2px to reduce blank border)
        NSMakeSize(15, 22),  // small
        NSMakeSize(19, 27)   // regular
    },
    {
        NSMakeSize(11, 16),  // mini (width trimmed by 2px to reduce blank border)
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

HIThemeButtonDrawInfo nsNativeThemeCocoa::SpinButtonDrawInfo(ThemeButtonKind aKind,
                                                             const SpinButtonParams& aParams) {
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

void nsNativeThemeCocoa::DrawSpinButtons(CGContextRef cgContext, const HIRect& inBoxRect,
                                         const SpinButtonParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  HIThemeButtonDrawInfo bdi = SpinButtonDrawInfo(kThemeIncDecButton, aParams);
  HIThemeDrawButton(&inBoxRect, &bdi, cgContext, HITHEME_ORIENTATION, NULL);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawSpinButton(CGContextRef cgContext, const HIRect& inBoxRect,
                                        SpinButton aDrawnButton, const SpinButtonParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  HIThemeButtonDrawInfo bdi = SpinButtonDrawInfo(kThemeIncDecButtonMini, aParams);

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

void nsNativeThemeCocoa::DrawTextBox(CGContextRef cgContext, const HIRect& inBoxRect,
                                     TextBoxParams aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  SetCGContextFillColor(cgContext, sRGBColor(1.0, 1.0, 1.0, 1.0));
  CGContextFillRect(cgContext, inBoxRect);

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  if (aParams.borderless) {
    return;
  }

  HIThemeFrameDrawInfo fdi;
  fdi.version = 0;
  fdi.kind = kHIThemeFrameTextFieldSquare;

  // We don't ever set an inactive state for this because it doesn't
  // look right (see other apps).
  fdi.state = aParams.disabled ? kThemeStateUnavailable : kThemeStateActive;
  fdi.isFocused = aParams.focused;

  // HIThemeDrawFrame takes the rect for the content area of the frame, not
  // the bounding rect for the frame. Here we reduce the size of the rect we
  // will pass to make it the size of the content.
  HIRect drawRect = inBoxRect;
  SInt32 frameOutset = 0;
  ::GetThemeMetric(kThemeMetricEditTextFrameOutset, &frameOutset);
  drawRect.origin.x += frameOutset;
  drawRect.origin.y += frameOutset;
  drawRect.size.width -= frameOutset * 2;
  drawRect.size.height -= frameOutset * 2;

  HIThemeDrawFrame(&drawRect, &fdi, cgContext, HITHEME_ORIENTATION);

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
    nsIFrame* aFrame, EventStates aEventState, bool aIsHorizontal) {
  ProgressParams params;
  params.value = GetProgressValue(aFrame);
  params.max = GetProgressMaxValue(aFrame);
  params.verticalAlignFactor = VerticalAlignFactor(aFrame);
  params.insideActiveWindow = FrameIsInActiveWindow(aFrame);
  params.indeterminate = IsIndeterminateProgress(aFrame, aEventState);
  params.horizontal = aIsHorizontal;
  params.rtl = IsFrameRTL(aFrame);
  return params;
}

void nsNativeThemeCocoa::DrawProgress(CGContextRef cgContext, const HIRect& inBoxRect,
                                      const ProgressParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NSProgressBarCell* cell = mProgressBarCell;

  [cell setValue:aParams.value];
  [cell setMax:aParams.max];
  [cell setIndeterminate:aParams.indeterminate];
  [cell setHorizontal:aParams.horizontal];
  [cell setControlTint:(aParams.insideActiveWindow ? [NSColor currentControlTint]
                                                   : NSClearControlTint)];

  DrawCellWithSnapping(cell, cgContext, inBoxRect,
                       progressSettings[aParams.horizontal][aParams.indeterminate],
                       aParams.verticalAlignFactor, mCellDrawView, aParams.rtl);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static const CellRenderSettings meterSetting = {{
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

nsNativeThemeCocoa::MeterParams nsNativeThemeCocoa::ComputeMeterParams(nsIFrame* aFrame) {
  nsIContent* content = aFrame->GetContent();
  if (!(content && content->IsHTMLElement(nsGkAtoms::meter))) {
    return MeterParams();
  }

  HTMLMeterElement* meterElement = static_cast<HTMLMeterElement*>(content);
  MeterParams params;
  params.value = meterElement->Value();
  params.min = meterElement->Min();
  params.max = meterElement->Max();
  EventStates states = meterElement->State();
  if (states.HasState(NS_EVENT_STATE_SUB_OPTIMUM)) {
    params.optimumState = OptimumState::eSubOptimum;
  } else if (states.HasState(NS_EVENT_STATE_SUB_SUB_OPTIMUM)) {
    params.optimumState = OptimumState::eSubSubOptimum;
  }
  params.horizontal = !IsVerticalMeter(aFrame);
  params.verticalAlignFactor = VerticalAlignFactor(aFrame);
  params.rtl = IsFrameRTL(aFrame);

  return params;
}

void nsNativeThemeCocoa::DrawMeter(CGContextRef cgContext, const HIRect& inBoxRect,
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
    CGContextTranslateCTM(cgContext, -CGRectGetMidX(rect), -CGRectGetMidY(rect));
  }

  DrawCellWithSnapping(cell, cgContext, rect, meterSetting, aParams.verticalAlignFactor,
                       mCellDrawView, !vertical && aParams.rtl);

  CGContextRestoreGState(cgContext);

  NS_OBJC_END_TRY_IGNORE_BLOCK
}

void nsNativeThemeCocoa::DrawTabPanel(CGContextRef cgContext, const HIRect& inBoxRect,
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

Maybe<nsNativeThemeCocoa::ScaleParams> nsNativeThemeCocoa::ComputeHTMLScaleParams(
    nsIFrame* aFrame, EventStates aEventState) {
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
  params.focused = aEventState.HasState(NS_EVENT_STATE_FOCUSRING);
  params.disabled = IsDisabled(aFrame, aEventState);
  params.horizontal = isHorizontal;
  return Some(params);
}

void nsNativeThemeCocoa::DrawScale(CGContextRef cgContext, const HIRect& inBoxRect,
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
    tdi.enableState = aParams.insideActiveWindow ? kThemeTrackActive : kThemeTrackInactive;
  }
  tdi.trackInfo.slider.thumbDir = kThemeThumbPlain;
  tdi.trackInfo.slider.pressState = 0;

  HIThemeDrawTrack(&tdi, NULL, cgContext, HITHEME_ORIENTATION);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

nsIFrame* nsNativeThemeCocoa::SeparatorResponsibility(nsIFrame* aBefore, nsIFrame* aAfter) {
  // Usually a separator is drawn by the segment to the right of the
  // separator, but pressed and selected segments have higher priority.
  if (!aBefore || !aAfter) return nullptr;
  if (IsSelectedButton(aAfter)) return aAfter;
  if (IsSelectedButton(aBefore) || IsPressedButton(aBefore)) return aBefore;
  return aAfter;
}

static CGRect SeparatorAdjustedRect(CGRect aRect, nsNativeThemeCocoa::SegmentParams aParams) {
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

static const SegmentedControlRenderSettings tabRenderSettings = {tabHeights, @"tab"};

static const CGFloat toolbarButtonHeights[3] = {15, 18, 22};

static const SegmentedControlRenderSettings toolbarButtonRenderSettings = {
    toolbarButtonHeights, @"kCUIWidgetButtonSegmentedSCurve"};

nsNativeThemeCocoa::SegmentParams nsNativeThemeCocoa::ComputeSegmentParams(
    nsIFrame* aFrame, EventStates aEventState, SegmentType aSegmentType) {
  SegmentParams params;
  params.segmentType = aSegmentType;
  params.insideActiveWindow = FrameIsInActiveWindow(aFrame);
  params.pressed = IsPressedButton(aFrame);
  params.selected = IsSelectedButton(aFrame);
  params.focused = aEventState.HasState(NS_EVENT_STATE_FOCUSRING);
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

void nsNativeThemeCocoa::DrawSegmentBackground(CGContextRef cgContext, const HIRect& inBoxRect,
                                               const SegmentParams& aParams) {
  // On earlier macOS versions, the segment background is automatically
  // drawn correctly and this method should not be used. ASSERT here
  // to catch unnecessary usage, but the method implementation is not
  // dependent on Big Sur in any way.
  MOZ_ASSERT(nsCocoaFeatures::OnBigSurOrLater());

  // Use colors resembling 10.15.
  if (aParams.selected) {
    DeviceColor color = ToDeviceColor(mozilla::gfx::sRGBColor::FromU8(93, 93, 93, 255));
    CGContextSetRGBFillColor(cgContext, color.r, color.g, color.b, color.a);
  } else {
    DeviceColor color = ToDeviceColor(mozilla::gfx::sRGBColor::FromU8(247, 247, 247, 255));
    CGContextSetRGBFillColor(cgContext, color.r, color.g, color.b, color.a);
  }

  // Create a rect for the background fill.
  CGRect bgRect = inBoxRect;
  bgRect.size.height -= 3.0;
  bgRect.size.width -= 4.0;
  bgRect.origin.x += 2.0;
  bgRect.origin.y += 1.0;

  // Round the corners unless the button is a middle button. Buttons in
  // a grouping but on the edge will have the inner edge filled below.
  if (aParams.atLeftEnd || aParams.atRightEnd) {
    CGPathRef path = CGPathCreateWithRoundedRect(bgRect, 5, 4, nullptr);
    CGContextAddPath(cgContext, path);
    CGPathRelease(path);
    CGContextClosePath(cgContext);
    CGContextFillPath(cgContext);
  }

  // Handle buttons grouped together where either or both of
  // the side edges do not have curved corners.
  if (!aParams.atLeftEnd && aParams.atRightEnd) {
    // Shift the rect left to draw the left side of the
    // rect with right angle corners leaving the right side
    // to have rounded corners drawn with the curve above.
    // For example, the left side of the forward button in
    // the Library window.
    CGRect leftRectEdge = bgRect;
    leftRectEdge.size.width -= 10;
    leftRectEdge.origin.x -= 2;
    CGContextFillRect(cgContext, leftRectEdge);
  } else if (aParams.atLeftEnd && !aParams.atRightEnd) {
    // Shift the rect right to draw the right side of the
    // rect with right angle corners leaving the left side
    // to have rounded corners drawn with the curve above.
    // For example, the right side of the back button in
    // the Library window.
    CGRect rightRectEdge = bgRect;
    rightRectEdge.size.width -= 10;
    rightRectEdge.origin.x += 12;
    CGContextFillRect(cgContext, rightRectEdge);
  } else if (!aParams.atLeftEnd && !aParams.atRightEnd) {
    // The middle button in a group of buttons. Widen the
    // background rect to meet adjacent buttons seamlessly.
    CGRect middleRect = bgRect;
    middleRect.size.width += 4;
    middleRect.origin.x -= 2;
    CGContextFillRect(cgContext, middleRect);
  }
}

void nsNativeThemeCocoa::DrawSegment(CGContextRef cgContext, const HIRect& inBoxRect,
                                     const SegmentParams& aParams) {
  SegmentedControlRenderSettings renderSettings = RenderSettingsForSegmentType(aParams.segmentType);

  // On Big Sur, manually draw the background of the buttons to workaround a
  // change in Big Sur where the backround is filled with the toolbar gradient.
  if (nsCocoaFeatures::OnBigSurOrLater() &&
      (aParams.segmentType == nsNativeThemeCocoa::SegmentType::eToolbarButton)) {
    DrawSegmentBackground(cgContext, inBoxRect, aParams);
  }

  NSControlSize controlSize = FindControlSize(inBoxRect.size.height, renderSettings.heights, 4.0f);
  CGRect drawRect = SeparatorAdjustedRect(inBoxRect, aParams);

  NSDictionary* dict = @{
    @"widget" : renderSettings.widgetName,
    @"kCUIPresentationStateKey" : (aParams.insideActiveWindow ? @"kCUIPresentationStateActiveKey"
                                                              : @"kCUIPresentationStateInactive"),
    @"kCUIPositionKey" : ToolbarButtonPosition(aParams.atLeftEnd, aParams.atRightEnd),
    @"kCUISegmentLeadingSeparatorKey" : [NSNumber numberWithBool:aParams.drawsLeftSeparator],
    @"kCUISegmentTrailingSeparatorKey" : [NSNumber numberWithBool:aParams.drawsRightSeparator],
    @"value" : [NSNumber numberWithBool:aParams.selected],
    @"state" :
        (aParams.pressed ? @"pressed" : (aParams.insideActiveWindow ? @"normal" : @"inactive")),
    @"focus" : [NSNumber numberWithBool:aParams.focused],
    @"size" : CUIControlSizeForCocoaSize(controlSize),
    @"is.flipped" : [NSNumber numberWithBool:YES],
    @"direction" : @"up"
  };

  RenderWithCoreUI(drawRect, cgContext, dict);
}

void nsNativeThemeCocoa::DrawToolbar(CGContextRef cgContext, const CGRect& inBoxRect,
                                     bool aIsMain) {
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
  DrawNativeGreyColorInRect(cgContext, toolbarBottomBorderGrey, drawRect, aIsMain);
}

static bool ToolbarCanBeUnified(const gfx::Rect& aRect, NSWindow* aWindow) {
  if (![aWindow isKindOfClass:[ToolbarWindow class]]) return false;

  ToolbarWindow* win = (ToolbarWindow*)aWindow;
  float unifiedToolbarHeight = [win unifiedToolbarHeight];
  return aRect.X() == 0 && aRect.Width() >= [win frame].size.width &&
         aRect.YMost() <= unifiedToolbarHeight;
}

// By default, kCUIWidgetWindowFrame drawing draws rounded corners in the
// upper corners. Depending on the context type, it fills the background in
// the corners with black or leaves it transparent. Unfortunately, this corner
// rounding interacts poorly with the window corner masking we apply during
// titlebar drawing and results in small remnants of the corner background
// appearing at the rounded edge.
// So we draw square corners.
static void DrawNativeTitlebarToolbarWithSquareCorners(CGContextRef aContext, const CGRect& aRect,
                                                       CGFloat aUnifiedHeight, BOOL aIsMain,
                                                       BOOL aIsFlipped) {
  // We extend the draw rect horizontally and clip away the rounded corners.
  const CGFloat extendHorizontal = 10;
  CGRect drawRect = CGRectInset(aRect, -extendHorizontal, 0);
  CGContextSaveGState(aContext);
  CGContextClipToRect(aContext, aRect);

  RenderWithCoreUI(
      drawRect, aContext,
      [NSDictionary
          dictionaryWithObjectsAndKeys:@"kCUIWidgetWindowFrame", @"widget", @"regularwin",
                                       @"windowtype", (aIsMain ? @"normal" : @"inactive"), @"state",
                                       [NSNumber numberWithDouble:aUnifiedHeight],
                                       @"kCUIWindowFrameUnifiedTitleBarHeightKey",
                                       [NSNumber numberWithBool:YES],
                                       @"kCUIWindowFrameDrawTitleSeparatorKey",
                                       [NSNumber numberWithBool:aIsFlipped], @"is.flipped", nil]);

  CGContextRestoreGState(aContext);
}

void nsNativeThemeCocoa::DrawUnifiedToolbar(CGContextRef cgContext, const HIRect& inBoxRect,
                                            const UnifiedToolbarParams& aParams) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  CGContextSaveGState(cgContext);
  CGContextClipToRect(cgContext, inBoxRect);

  CGFloat titlebarHeight = aParams.unifiedHeight - inBoxRect.size.height;
  CGRect drawRect = CGRectMake(inBoxRect.origin.x, inBoxRect.origin.y - titlebarHeight,
                               inBoxRect.size.width, inBoxRect.size.height + titlebarHeight);
  DrawNativeTitlebarToolbarWithSquareCorners(cgContext, drawRect, aParams.unifiedHeight,
                                             aParams.isMain, YES);

  CGContextRestoreGState(cgContext);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawStatusBar(CGContextRef cgContext, const HIRect& inBoxRect,
                                       bool aIsMain) {
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
      [NSDictionary
          dictionaryWithObjectsAndKeys:@"kCUIWidgetWindowFrame", @"widget", @"regularwin",
                                       @"windowtype", (aIsMain ? @"normal" : @"inactive"), @"state",
                                       [NSNumber numberWithInt:inBoxRect.size.height],
                                       @"kCUIWindowFrameBottomBarHeightKey",
                                       [NSNumber numberWithBool:YES],
                                       @"kCUIWindowFrameDrawBottomBarSeparatorKey",
                                       [NSNumber numberWithBool:YES], @"is.flipped", nil]);

  CGContextRestoreGState(cgContext);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsNativeThemeCocoa::DrawNativeTitlebar(CGContextRef aContext, CGRect aTitlebarRect,
                                            CGFloat aUnifiedHeight, BOOL aIsMain, BOOL aIsFlipped) {
  CGFloat unifiedHeight = std::max(aUnifiedHeight, aTitlebarRect.size.height);
  DrawNativeTitlebarToolbarWithSquareCorners(aContext, aTitlebarRect, unifiedHeight, aIsMain,
                                             aIsFlipped);
}

void nsNativeThemeCocoa::DrawNativeTitlebar(CGContextRef aContext, CGRect aTitlebarRect,
                                            const UnifiedToolbarParams& aParams) {
  DrawNativeTitlebar(aContext, aTitlebarRect, aParams.unifiedHeight, aParams.isMain, YES);
}

static void RenderResizer(CGContextRef cgContext, const HIRect& aRenderRect, void* aData) {
  HIThemeGrowBoxDrawInfo* drawInfo = (HIThemeGrowBoxDrawInfo*)aData;
  HIThemeDrawGrowBox(&CGPointZero, drawInfo, cgContext, kHIThemeOrientationNormal);
}

void nsNativeThemeCocoa::DrawResizer(CGContextRef cgContext, const HIRect& aRect, bool aIsRTL) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  HIThemeGrowBoxDrawInfo drawInfo;
  drawInfo.version = 0;
  drawInfo.state = kThemeStateActive;
  drawInfo.kind = kHIThemeGrowBoxKindNormal;
  drawInfo.direction = kThemeGrowRight | kThemeGrowDown;
  drawInfo.size = kHIThemeGrowBoxSizeNormal;

  RenderTransformedHIThemeControl(cgContext, aRect, RenderResizer, &drawInfo, aIsRTL);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static const sRGBColor kMultilineTextFieldTopBorderColor(0.4510, 0.4510, 0.4510, 1.0);
static const sRGBColor kMultilineTextFieldSidesAndBottomBorderColor(0.6, 0.6, 0.6, 1.0);
static const sRGBColor kListboxTopBorderColor(0.557, 0.557, 0.557, 1.0);
static const sRGBColor kListBoxSidesAndBottomBorderColor(0.745, 0.745, 0.745, 1.0);

void nsNativeThemeCocoa::DrawMultilineTextField(CGContextRef cgContext, const CGRect& inBoxRect,
                                                bool aIsFocused) {
  SetCGContextFillColor(cgContext, sRGBColor(1.0, 1.0, 1.0, 1.0));

  CGContextFillRect(cgContext, inBoxRect);

  float x = inBoxRect.origin.x, y = inBoxRect.origin.y;
  float w = inBoxRect.size.width, h = inBoxRect.size.height;
  SetCGContextFillColor(cgContext, kMultilineTextFieldTopBorderColor);
  CGContextFillRect(cgContext, CGRectMake(x, y, w, 1));
  SetCGContextFillColor(cgContext, kMultilineTextFieldSidesAndBottomBorderColor);
  CGContextFillRect(cgContext, CGRectMake(x, y + 1, 1, h - 1));
  CGContextFillRect(cgContext, CGRectMake(x + w - 1, y + 1, 1, h - 1));
  CGContextFillRect(cgContext, CGRectMake(x + 1, y + h - 1, w - 2, 1));

  if (aIsFocused) {
    NSGraphicsContext* savedContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext
        setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:cgContext
                                                                     flipped:YES]];
    CGContextSaveGState(cgContext);
    NSSetFocusRingStyle(NSFocusRingOnly);
    NSRectFill(NSRectFromCGRect(inBoxRect));
    CGContextRestoreGState(cgContext);
    [NSGraphicsContext setCurrentContext:savedContext];
  }
}

void nsNativeThemeCocoa::DrawSourceListSelection(CGContextRef aContext, const CGRect& aRect,
                                                 bool aWindowIsActive, bool aSelectionIsActive) {
  NSColor* fillColor;
  if (aSelectionIsActive) {
    // Active selection, blue or graphite.
    fillColor = ControlAccentColor();
  } else {
    // Inactive selection, gray.
    if (aWindowIsActive) {
      fillColor = [NSColor colorWithWhite:0.871 alpha:1.0];
    } else {
      fillColor = [NSColor colorWithWhite:0.808 alpha:1.0];
    }
  }
  CGContextSetFillColorWithColor(aContext, [fillColor CGColor]);
  CGContextFillRect(aContext, aRect);
}

static bool IsHiDPIContext(nsDeviceContext* aContext) {
  return AppUnitsPerCSSPixel() >= 2 * aContext->AppUnitsPerDevPixelAtUnitFullZoom();
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

  EventStates eventState = GetContentState(aFrame, aAppearance);

  switch (aAppearance) {
    case StyleAppearance::Menupopup:
      return Nothing();

    case StyleAppearance::Menuarrow:
      return Some(
          WidgetInfo::MenuIcon(ComputeMenuIconParams(aFrame, eventState, MenuIcon::eMenuArrow)));

    case StyleAppearance::Menuitem:
    case StyleAppearance::Checkmenuitem:
      return Some(WidgetInfo::MenuItem(ComputeMenuItemParams(
          aFrame, eventState, aAppearance == StyleAppearance::Checkmenuitem)));

    case StyleAppearance::Menuseparator:
      return Some(WidgetInfo::MenuSeparator(ComputeMenuItemParams(aFrame, eventState, false)));

    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown: {
      MenuIcon icon = aAppearance == StyleAppearance::ButtonArrowUp
                          ? MenuIcon::eMenuUpScrollArrow
                          : MenuIcon::eMenuDownScrollArrow;
      return Some(WidgetInfo::MenuIcon(ComputeMenuIconParams(aFrame, eventState, icon)));
    }

    case StyleAppearance::Tooltip:
      return Nothing();

    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio: {
      bool isCheckbox = (aAppearance == StyleAppearance::Checkbox);

      CheckboxOrRadioParams params;
      params.state = CheckboxOrRadioState::eOff;
      if (isCheckbox && GetIndeterminate(aFrame)) {
        params.state = CheckboxOrRadioState::eIndeterminate;
      } else if (GetCheckedOrSelected(aFrame, !isCheckbox)) {
        params.state = CheckboxOrRadioState::eOn;
      }
      params.controlParams = ComputeControlParams(aFrame, eventState);
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
        EventStates docState = aFrame->GetContent()->OwnerDoc()->GetDocumentState();
        bool isInActiveWindow = !docState.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
        bool hasDefaultButtonLook = isInActiveWindow && !eventState.HasState(NS_EVENT_STATE_ACTIVE);
        ButtonType buttonType =
            hasDefaultButtonLook ? ButtonType::eDefaultPushButton : ButtonType::eRegularPushButton;
        ControlParams params = ComputeControlParams(aFrame, eventState);
        params.insideActiveWindow = isInActiveWindow;
        return Some(WidgetInfo::Button(ButtonParams{params, buttonType}));
      }
      if (IsButtonTypeMenu(aFrame)) {
        ControlParams controlParams = ComputeControlParams(aFrame, eventState);
        controlParams.focused = controlParams.focused || IsFocused(aFrame);
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
        // This comparison is done based on the height that is calculated without
        // the top, because the snapped height can be affected by the top of the
        // rect and that may result in different height depending on the top value.
        return Some(WidgetInfo::Button(ButtonParams{ComputeControlParams(aFrame, eventState),
                                                    ButtonType::eSquareBezelPushButton}));
      }
      return Some(WidgetInfo::Button(ButtonParams{ComputeControlParams(aFrame, eventState),
                                                  ButtonType::eRoundedBezelPushButton}));

    case StyleAppearance::FocusOutline:
      return Some(WidgetInfo::FocusOutline());

    case StyleAppearance::MozMacHelpButton:
      return Some(WidgetInfo::Button(
          ButtonParams{ComputeControlParams(aFrame, eventState), ButtonType::eHelpButton}));

    case StyleAppearance::MozMacDisclosureButtonOpen:
    case StyleAppearance::MozMacDisclosureButtonClosed: {
      ButtonType buttonType = (aAppearance == StyleAppearance::MozMacDisclosureButtonClosed)
                                  ? ButtonType::eDisclosureButtonClosed
                                  : ButtonType::eDisclosureButtonOpen;
      return Some(
          WidgetInfo::Button(ButtonParams{ComputeControlParams(aFrame, eventState), buttonType}));
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
        if (content->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::state, u"up"_ns,
                                              eCaseMatters)) {
          params.pressedButton = Some(SpinButton::eUp);
        } else if (content->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::state,
                                                     u"down"_ns, eCaseMatters)) {
          params.pressedButton = Some(SpinButton::eDown);
        }
      }
      params.disabled = IsDisabled(aFrame, eventState);
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
        params.disabled = IsDisabled(aFrame, eventState);
        params.insideActiveWindow = FrameIsInActiveWindow(aFrame);
        if (aAppearance == StyleAppearance::SpinnerUpbutton) {
          return Some(WidgetInfo::SpinButtonUp(params));
        }
        return Some(WidgetInfo::SpinButtonDown(params));
      }
    } break;

    case StyleAppearance::Toolbarbutton: {
      SegmentParams params = ComputeSegmentParams(aFrame, eventState, SegmentType::eToolbarButton);
      params.insideActiveWindow = [NativeWindowForFrame(aFrame) isMainWindow];
      return Some(WidgetInfo::Segment(params));
    }

    case StyleAppearance::Separator:
      return Some(WidgetInfo::Separator());

    case StyleAppearance::Toolbar: {
      NSWindow* win = NativeWindowForFrame(aFrame);
      bool isMain = [win isMainWindow];
      if (ToolbarCanBeUnified(nativeWidgetRect, win)) {
        float unifiedHeight =
            std::max(float([(ToolbarWindow*)win unifiedToolbarHeight]), nativeWidgetRect.Height());
        return Some(WidgetInfo::UnifiedToolbar(UnifiedToolbarParams{unifiedHeight, isMain}));
      }
      return Some(WidgetInfo::Toolbar(isMain));
    }

    case StyleAppearance::MozWindowTitlebar: {
      NSWindow* win = NativeWindowForFrame(aFrame);
      bool isMain = [win isMainWindow];
      float unifiedToolbarHeight = [win isKindOfClass:[ToolbarWindow class]]
                                       ? [(ToolbarWindow*)win unifiedToolbarHeight]
                                       : nativeWidgetRect.Height();
      return Some(WidgetInfo::NativeTitlebar(UnifiedToolbarParams{unifiedToolbarHeight, isMain}));
    }

    case StyleAppearance::Statusbar:
      return Some(WidgetInfo::StatusBar(IsActive(aFrame, YES)));

    case StyleAppearance::MenulistButton:
    case StyleAppearance::Menulist: {
      ControlParams controlParams = ComputeControlParams(aFrame, eventState);
      controlParams.focused = controlParams.focused || IsFocused(aFrame);
      controlParams.pressed = IsOpenButton(aFrame);
      DropdownParams params;
      params.controlParams = controlParams;
      params.pullsDown = false;
      params.editable = false;
      return Some(WidgetInfo::Dropdown(params));
    }

    case StyleAppearance::MozMenulistArrowButton:
      return Some(WidgetInfo::Button(
          ButtonParams{ComputeControlParams(aFrame, eventState), ButtonType::eArrowButton}));

    case StyleAppearance::Groupbox:
      return Some(WidgetInfo::GroupBox());

    case StyleAppearance::Textfield:
    case StyleAppearance::NumberInput: {
      // See ShouldUnconditionallyDrawFocusRingIfFocused.
      bool isFocused = eventState.HasState(NS_EVENT_STATE_FOCUS);
      // XUL textboxes set the native appearance on the containing box, while
      // concrete focus is set on the html:input element within it. We can
      // though, check the focused attribute of xul textboxes in this case.
      // On Mac, focus rings are always shown for textboxes, so we do not need
      // to check the window's focus ring state here
      if (aFrame->GetContent()->IsXULElement() && IsFocused(aFrame)) {
        isFocused = true;
      }

      bool isDisabled = IsDisabled(aFrame, eventState) || IsReadOnly(aFrame);
      return Some(
          WidgetInfo::TextBox(TextBoxParams{isDisabled, isFocused, /* borderless = */ false}));
    }

    case StyleAppearance::Searchfield:
      return Some(WidgetInfo::SearchField(ComputeSearchFieldParams(aFrame, eventState)));

    case StyleAppearance::ProgressBar: {
      if (IsIndeterminateProgress(aFrame, eventState)) {
        if (!QueueAnimatedContentForRefresh(aFrame->GetContent(), 30)) {
          NS_WARNING("Unable to animate progressbar!");
        }
      }
      return Some(WidgetInfo::ProgressBar(
          ComputeProgressParams(aFrame, eventState, !IsVerticalProgress(aFrame))));
    }

    case StyleAppearance::Meter:
      return Some(WidgetInfo::Meter(ComputeMeterParams(aFrame)));

    case StyleAppearance::Progresschunk:
    case StyleAppearance::Meterchunk:
      // Do nothing: progress and meter bars cases will draw chunks.
      break;

    case StyleAppearance::Treetwisty:
      return Some(WidgetInfo::Button(ButtonParams{ComputeControlParams(aFrame, eventState),
                                                  ButtonType::eTreeTwistyPointingRight}));

    case StyleAppearance::Treetwistyopen:
      return Some(WidgetInfo::Button(ButtonParams{ComputeControlParams(aFrame, eventState),
                                                  ButtonType::eTreeTwistyPointingDown}));

    case StyleAppearance::Treeheadercell:
      return Some(WidgetInfo::TreeHeaderCell(ComputeTreeHeaderCellParams(aFrame, eventState)));

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
      Maybe<ScaleParams> params = ComputeHTMLScaleParams(aFrame, eventState);
      if (params) {
        return Some(WidgetInfo::Scale(*params));
      }
      break;
    }

    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonRight:
      break;

    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::Scrollcorner: {
      bool isHorizontal = aAppearance == StyleAppearance::ScrollbarthumbHorizontal ||
                          aAppearance == StyleAppearance::ScrollbartrackHorizontal;
      ScrollbarParams params = ScrollbarDrawingMac::ComputeScrollbarParams(
          aFrame, *nsLayoutUtils::StyleForScrollbar(aFrame), isHorizontal);
      switch (aAppearance) {
        case StyleAppearance::ScrollbarthumbVertical:
        case StyleAppearance::ScrollbarthumbHorizontal:
          return Some(WidgetInfo::ScrollbarThumb(params));
        case StyleAppearance::ScrollbartrackHorizontal:
        case StyleAppearance::ScrollbartrackVertical:
          return Some(WidgetInfo::ScrollbarTrack(params));
        case StyleAppearance::Scrollcorner:
          return Some(WidgetInfo::ScrollCorner(params));
        default:
          MOZ_CRASH("unexpected aAppearance");
      }
      break;
    }

    case StyleAppearance::Textarea:
      return Some(WidgetInfo::MultilineTextField(eventState.HasState(NS_EVENT_STATE_FOCUS)));

    case StyleAppearance::Listbox:
      return Some(WidgetInfo::ListBox());

    case StyleAppearance::MozMacSourceList: {
      return Nothing();
    }

    case StyleAppearance::MozMacSourceListSelection:
    case StyleAppearance::MozMacActiveSourceListSelection: {
      // We only support vibrancy for source list selections if we're inside
      // a source list, because we need the background to be transparent.
      if (IsInSourceList(aFrame)) {
        return Nothing();
      }
      bool isInActiveWindow = FrameIsInActiveWindow(aFrame);
      if (aAppearance == StyleAppearance::MozMacActiveSourceListSelection) {
        return Some(WidgetInfo::ActiveSourceListSelection(isInActiveWindow));
      }
      return Some(WidgetInfo::InactiveSourceListSelection(isInActiveWindow));
    }

    case StyleAppearance::Tab: {
      SegmentParams params = ComputeSegmentParams(aFrame, eventState, SegmentType::eTab);
      params.pressed = params.pressed && !params.selected;
      return Some(WidgetInfo::Segment(params));
    }

    case StyleAppearance::Tabpanels:
      return Some(WidgetInfo::TabPanel(FrameIsInActiveWindow(aFrame)));

    case StyleAppearance::Resizer:
      return Some(WidgetInfo::Resizer(IsFrameRTL(aFrame)));

    default:
      break;
  }

  return Nothing();

  NS_OBJC_END_TRY_BLOCK_RETURN(Nothing());
}

NS_IMETHODIMP
nsNativeThemeCocoa::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                         StyleAppearance aAppearance, const nsRect& aRect,
                                         const nsRect& aDirtyRect) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  Maybe<WidgetInfo> widgetInfo = ComputeWidgetInfo(aFrame, aAppearance, aRect);

  if (!widgetInfo) {
    return NS_OK;
  }

  int32_t p2a = aFrame->PresContext()->AppUnitsPerDevPixel();

  gfx::Rect nativeWidgetRect = NSRectToRect(aRect, p2a);
  nativeWidgetRect.Round();

  bool hidpi = IsHiDPIContext(aFrame->PresContext()->DeviceContext());

  RenderWidget(*widgetInfo, *aContext->GetDrawTarget(), nativeWidgetRect,
               NSRectToRect(aDirtyRect, p2a), hidpi ? 2.0f : 1.0f);

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

void nsNativeThemeCocoa::RenderWidget(const WidgetInfo& aWidgetInfo, DrawTarget& aDrawTarget,
                                      const gfx::Rect& aWidgetRect, const gfx::Rect& aDirtyRect,
                                      float aScale) {
  AutoRestoreTransform autoRestoreTransform(&aDrawTarget);

  gfx::Rect dirtyRect = aDirtyRect;
  gfx::Rect widgetRect = aWidgetRect;
  dirtyRect.Scale(1.0f / aScale);
  widgetRect.Scale(1.0f / aScale);
  aDrawTarget.SetTransform(aDrawTarget.GetTransform().PreScale(aScale, aScale));

  const Widget widget = aWidgetInfo.Widget();

  // Some widgets render using DrawTarget, and some using CGContext.
  switch (widget) {
    case Widget::eColorFill: {
      sRGBColor color = aWidgetInfo.Params<sRGBColor>();
      aDrawTarget.FillRect(widgetRect, ColorPattern(ToDeviceColor(color)));
      break;
    }
    case Widget::eScrollbarThumb: {
      ScrollbarParams params = aWidgetInfo.Params<ScrollbarParams>();
      ScrollbarDrawingMac::DrawScrollbarThumb(aDrawTarget, widgetRect, params);
      break;
    }
    case Widget::eScrollbarTrack: {
      ScrollbarParams params = aWidgetInfo.Params<ScrollbarParams>();
      ScrollbarDrawingMac::DrawScrollbarTrack(aDrawTarget, widgetRect, params);
      break;
    }
    case Widget::eScrollCorner: {
      ScrollbarParams params = aWidgetInfo.Params<ScrollbarParams>();
      ScrollbarDrawingMac::DrawScrollCorner(aDrawTarget, widgetRect, params);
      break;
    }
    default: {
      // The remaining widgets require a CGContext.
      CGRect macRect =
          CGRectMake(widgetRect.X(), widgetRect.Y(), widgetRect.Width(), widgetRect.Height());

      gfxQuartzNativeDrawing nativeDrawing(aDrawTarget, dirtyRect);

      CGContextRef cgContext = nativeDrawing.BeginNativeDrawing();
      if (cgContext == nullptr) {
        // The Quartz surface handles 0x0 surfaces by internally
        // making all operations no-ops; there's no cgcontext created for them.
        // Unfortunately, this means that callers that want to render
        // directly to the CGContext need to be aware of this quirk.
        return;
      }

      // Set the context's "base transform" to in order to get correctly-sized focus rings.
      CGContextSetBaseCTM(cgContext, CGAffineTransformMakeScale(aScale, aScale));

      switch (widget) {
        case Widget::eColorFill:
        case Widget::eScrollbarThumb:
        case Widget::eScrollbarTrack:
        case Widget::eScrollCorner: {
          MOZ_CRASH("already handled in outer switch");
          break;
        }
        case Widget::eMenuIcon: {
          MenuIconParams params = aWidgetInfo.Params<MenuIconParams>();
          DrawMenuIcon(cgContext, macRect, params);
          break;
        }
        case Widget::eMenuItem: {
          MenuItemParams params = aWidgetInfo.Params<MenuItemParams>();
          DrawMenuItem(cgContext, macRect, params);
          break;
        }
        case Widget::eMenuSeparator: {
          MenuItemParams params = aWidgetInfo.Params<MenuItemParams>();
          DrawMenuSeparator(cgContext, macRect, params);
          break;
        }
        case Widget::eCheckbox: {
          CheckboxOrRadioParams params = aWidgetInfo.Params<CheckboxOrRadioParams>();
          DrawCheckboxOrRadio(cgContext, true, macRect, params);
          break;
        }
        case Widget::eRadio: {
          CheckboxOrRadioParams params = aWidgetInfo.Params<CheckboxOrRadioParams>();
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
        case Widget::eFocusOutline: {
          DrawFocusOutline(cgContext, macRect);
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
        case Widget::eUnifiedToolbar: {
          UnifiedToolbarParams params = aWidgetInfo.Params<UnifiedToolbarParams>();
          DrawUnifiedToolbar(cgContext, macRect, params);
          break;
        }
        case Widget::eToolbar: {
          bool isMain = aWidgetInfo.Params<bool>();
          DrawToolbar(cgContext, macRect, isMain);
          break;
        }
        case Widget::eNativeTitlebar: {
          UnifiedToolbarParams params = aWidgetInfo.Params<UnifiedToolbarParams>();
          DrawNativeTitlebar(cgContext, macRect, params);
          break;
        }
        case Widget::eStatusBar: {
          bool isMain = aWidgetInfo.Params<bool>();
          DrawStatusBar(cgContext, macRect, isMain);
          break;
        }
        case Widget::eGroupBox: {
          HIThemeGroupBoxDrawInfo gdi = {0, kThemeStateActive, kHIThemeGroupBoxKindPrimary};
          HIThemeDrawGroupBox(&macRect, &gdi, cgContext, HITHEME_ORIENTATION);
          break;
        }
        case Widget::eTextBox: {
          TextBoxParams params = aWidgetInfo.Params<TextBoxParams>();
          DrawTextBox(cgContext, macRect, params);
          break;
        }
        case Widget::eSearchField: {
          SearchFieldParams params = aWidgetInfo.Params<SearchFieldParams>();
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
          TreeHeaderCellParams params = aWidgetInfo.Params<TreeHeaderCellParams>();
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
          // We have to draw this by hand because kHIThemeFrameListBox drawing
          // is buggy on 10.5, see bug 579259.
          SetCGContextFillColor(cgContext, sRGBColor(1.0, 1.0, 1.0, 1.0));
          CGContextFillRect(cgContext, macRect);

          float x = macRect.origin.x, y = macRect.origin.y;
          float w = macRect.size.width, h = macRect.size.height;
          SetCGContextFillColor(cgContext, kListboxTopBorderColor);
          CGContextFillRect(cgContext, CGRectMake(x, y, w, 1));
          SetCGContextFillColor(cgContext, kListBoxSidesAndBottomBorderColor);
          CGContextFillRect(cgContext, CGRectMake(x, y + 1, 1, h - 1));
          CGContextFillRect(cgContext, CGRectMake(x + w - 1, y + 1, 1, h - 1));
          CGContextFillRect(cgContext, CGRectMake(x + 1, y + h - 1, w - 2, 1));
          break;
        }
        case Widget::eActiveSourceListSelection:
        case Widget::eInactiveSourceListSelection: {
          bool isInActiveWindow = aWidgetInfo.Params<bool>();
          bool isActiveSelection = aWidgetInfo.Widget() == Widget::eActiveSourceListSelection;
          DrawSourceListSelection(cgContext, macRect, isInActiveWindow, isActiveSelection);
          break;
        }
        case Widget::eTabPanel: {
          bool isInsideActiveWindow = aWidgetInfo.Params<bool>();
          DrawTabPanel(cgContext, macRect, isInsideActiveWindow);
          break;
        }
        case Widget::eResizer: {
          bool isRTL = aWidgetInfo.Params<bool>();
          DrawResizer(cgContext, macRect, isRTL);
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
    mozilla::wr::DisplayListBuilder& aBuilder, mozilla::wr::IpcResourceUpdateQueue& aResources,
    const mozilla::layers::StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager, nsIFrame* aFrame,
    StyleAppearance aAppearance, const nsRect& aRect) {
  nsPresContext* presContext = aFrame->PresContext();
  wr::LayoutRect bounds =
      wr::ToLayoutRect(LayoutDeviceRect::FromAppUnits(aRect, presContext->AppUnitsPerDevPixel()));

  EventStates eventState = GetContentState(aFrame, aAppearance);

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
    case StyleAppearance::Menuarrow:
    case StyleAppearance::Menuitem:
    case StyleAppearance::Checkmenuitem:
    case StyleAppearance::Menuseparator:
    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown:
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
    case StyleAppearance::Button:
    case StyleAppearance::FocusOutline:
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
    case StyleAppearance::Groupbox:
    case StyleAppearance::Textfield:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Searchfield:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Meter:
    case StyleAppearance::Treetwisty:
    case StyleAppearance::Treetwistyopen:
    case StyleAppearance::Treeheadercell:
    case StyleAppearance::Treeitem:
    case StyleAppearance::Treeview:
    case StyleAppearance::Range:
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbarthumbHorizontal:
      return false;

    case StyleAppearance::Scrollcorner:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical: {
      const ComputedStyle& style = *nsLayoutUtils::StyleForScrollbar(aFrame);
      ScrollbarParams params = ScrollbarDrawingMac::ComputeScrollbarParams(
          aFrame, style, aAppearance == StyleAppearance::ScrollbartrackHorizontal);
      if (params.overlay && !params.rolledOver) {
        // There is no scrollbar track, draw nothing and return true.
        return true;
      }
      // There is a scrollbar track and it needs to be drawn using fallback.
      return false;
    }

    case StyleAppearance::Textarea: {
      if (eventState.HasState(NS_EVENT_STATE_FOCUS)) {
        // We can't draw the focus ring using webrender, so fall back to regular
        // drawing if we're focused.
        return false;
      }

      // White background
      aBuilder.PushRect(bounds, bounds, true,
                        wr::ToColorF(ToDeviceColor(sRGBColor::OpaqueWhite())));

      wr::BorderSide side[4] = {
          wr::ToBorderSide(ToDeviceColor(kMultilineTextFieldTopBorderColor),
                           StyleBorderStyle::Solid),
          wr::ToBorderSide(ToDeviceColor(kMultilineTextFieldSidesAndBottomBorderColor),
                           StyleBorderStyle::Solid),
          wr::ToBorderSide(ToDeviceColor(kMultilineTextFieldSidesAndBottomBorderColor),
                           StyleBorderStyle::Solid),
          wr::ToBorderSide(ToDeviceColor(kMultilineTextFieldSidesAndBottomBorderColor),
                           StyleBorderStyle::Solid),
      };

      wr::BorderRadius borderRadius = wr::EmptyBorderRadius();
      float borderWidth = presContext->CSSPixelsToDevPixels(1.0f);
      wr::LayoutSideOffsets borderWidths =
          wr::ToBorderWidths(borderWidth, borderWidth, borderWidth, borderWidth);

      mozilla::Range<const wr::BorderSide> wrsides(side, 4);
      aBuilder.PushBorder(bounds, bounds, true, borderWidths, wrsides, borderRadius);

      return true;
    }

    case StyleAppearance::Listbox: {
      // White background
      aBuilder.PushRect(bounds, bounds, true,
                        wr::ToColorF(ToDeviceColor(sRGBColor::OpaqueWhite())));

      wr::BorderSide side[4] = {
          wr::ToBorderSide(ToDeviceColor(kListboxTopBorderColor), StyleBorderStyle::Solid),
          wr::ToBorderSide(ToDeviceColor(kListBoxSidesAndBottomBorderColor),
                           StyleBorderStyle::Solid),
          wr::ToBorderSide(ToDeviceColor(kListBoxSidesAndBottomBorderColor),
                           StyleBorderStyle::Solid),
          wr::ToBorderSide(ToDeviceColor(kListBoxSidesAndBottomBorderColor),
                           StyleBorderStyle::Solid),
      };

      wr::BorderRadius borderRadius = wr::EmptyBorderRadius();
      float borderWidth = presContext->CSSPixelsToDevPixels(1.0f);
      wr::LayoutSideOffsets borderWidths =
          wr::ToBorderWidths(borderWidth, borderWidth, borderWidth, borderWidth);

      mozilla::Range<const wr::BorderSide> wrsides(side, 4);
      aBuilder.PushBorder(bounds, bounds, true, borderWidths, wrsides, borderRadius);
      return true;
    }

    case StyleAppearance::Tab:
    case StyleAppearance::Tabpanels:
    case StyleAppearance::Resizer:
      return false;

    default:
      return true;
  }
}

LayoutDeviceIntMargin nsNativeThemeCocoa::DirectionAwareMargin(const LayoutDeviceIntMargin& aMargin,
                                                               nsIFrame* aFrame) {
  // Assuming aMargin was originally specified for a horizontal LTR context,
  // reinterpret the values as logical, and then map to physical coords
  // according to aFrame's actual writing mode.
  WritingMode wm = aFrame->GetWritingMode();
  nsMargin m = LogicalMargin(wm, aMargin.top, aMargin.right, aMargin.bottom, aMargin.left)
                   .GetPhysicalMargin(wm);
  return LayoutDeviceIntMargin(m.top, m.right, m.bottom, m.left);
}

static const LayoutDeviceIntMargin kAquaDropdownBorder(1, 22, 2, 5);
static const LayoutDeviceIntMargin kAquaComboboxBorder(3, 20, 3, 4);
static const LayoutDeviceIntMargin kAquaSearchfieldBorder(3, 5, 2, 19);
static const LayoutDeviceIntMargin kAquaSearchfieldBorderBigSur(5, 5, 4, 26);

LayoutDeviceIntMargin nsNativeThemeCocoa::GetWidgetBorder(nsDeviceContext* aContext,
                                                          nsIFrame* aFrame,
                                                          StyleAppearance aAppearance) {
  LayoutDeviceIntMargin result;

  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  switch (aAppearance) {
    case StyleAppearance::Button: {
      if (IsButtonTypeMenu(aFrame)) {
        result = DirectionAwareMargin(kAquaDropdownBorder, aFrame);
      } else {
        result = DirectionAwareMargin(LayoutDeviceIntMargin(1, 7, 3, 7), aFrame);
      }
      break;
    }

    case StyleAppearance::Toolbarbutton: {
      result = DirectionAwareMargin(LayoutDeviceIntMargin(1, 4, 1, 4), aFrame);
      break;
    }

    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio: {
      // nsCheckboxRadioFrame::GetIntrinsicWidth and nsCheckboxRadioFrame::GetIntrinsicHeight
      // assume a border width of 2px.
      result.SizeTo(2, 2, 2, 2);
      break;
    }

    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MozMenulistArrowButton:
      result = DirectionAwareMargin(kAquaDropdownBorder, aFrame);
      break;

    case StyleAppearance::Menuarrow:
      if (nsCocoaFeatures::OnBigSurOrLater()) {
        result.SizeTo(0, 0, 0, 28);
      }
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
      auto border = nsCocoaFeatures::OnBigSurOrLater() ? kAquaSearchfieldBorderBigSur
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

    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical: {
      bool isHorizontal = (aAppearance == StyleAppearance::ScrollbartrackHorizontal);
      if (nsLookAndFeel::UseOverlayScrollbars()) {
        // Leave a bit of space at the start and the end on all OS X versions.
        if (isHorizontal) {
          result.left = 1;
          result.right = 1;
        } else {
          result.top = 1;
          result.bottom = 1;
        }
      }

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

// Return false here to indicate that CSS padding values should be used. There is
// no reason to make a distinction between padding and border values, just specify
// whatever values you want in GetWidgetBorder and only use this to return true
// if you want to override CSS padding values.
bool nsNativeThemeCocoa::GetWidgetPadding(nsDeviceContext* aContext, nsIFrame* aFrame,
                                          StyleAppearance aAppearance,
                                          LayoutDeviceIntMargin* aResult) {
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

    case StyleAppearance::Menuarrow:
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

bool nsNativeThemeCocoa::GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                                           StyleAppearance aAppearance, nsRect* aOverflowRect) {
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
    case StyleAppearance::Tab:
    case StyleAppearance::FocusOutline: {
      overflow.SizeTo(kMaxFocusRingWidth, kMaxFocusRingWidth, kMaxFocusRingWidth,
                      kMaxFocusRingWidth);
      break;
    }
    case StyleAppearance::ProgressBar: {
      // Progress bars draw a 2 pixel white shadow under their progress indicators.
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
    aOverflowRect->Inflate(nsMargin(
        NSIntPixelsToAppUnits(overflow.top, p2a), NSIntPixelsToAppUnits(overflow.right, p2a),
        NSIntPixelsToAppUnits(overflow.bottom, p2a), NSIntPixelsToAppUnits(overflow.left, p2a)));
    return true;
  }

  return false;
}

auto nsNativeThemeCocoa::GetScrollbarSizes(nsPresContext* aPresContext, StyleScrollbarWidth aWidth,
                                           Overlay aOverlay) -> ScrollbarSizes {
  auto size = ScrollbarDrawingMac::GetScrollbarSize(aWidth, aOverlay == Overlay::Yes);
  if (IsHiDPIContext(aPresContext->DeviceContext())) {
    size *= 2;
  }
  return {int32_t(size), int32_t(size)};
}

NS_IMETHODIMP
nsNativeThemeCocoa::GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                         StyleAppearance aAppearance, LayoutDeviceIntSize* aResult,
                                         bool* aIsOverridable) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  aResult->SizeTo(0, 0);
  *aIsOverridable = true;

  switch (aAppearance) {
    case StyleAppearance::Button: {
      aResult->SizeTo(pushButtonSettings.minimumSizes[miniControlSize].width,
                      pushButtonSettings.naturalSizes[miniControlSize].height);
      break;
    }

    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown: {
      aResult->SizeTo(kMenuScrollArrowSize.width, kMenuScrollArrowSize.height);
      *aIsOverridable = false;
      break;
    }

    case StyleAppearance::Menuarrow: {
      aResult->SizeTo(kMenuarrowSize.width, kMenuarrowSize.height);
      *aIsOverridable = false;
      break;
    }

    case StyleAppearance::MozMacDisclosureButtonOpen:
    case StyleAppearance::MozMacDisclosureButtonClosed: {
      aResult->SizeTo(kDisclosureButtonSize.width, kDisclosureButtonSize.height);
      *aIsOverridable = false;
      break;
    }

    case StyleAppearance::MozMacHelpButton: {
      aResult->SizeTo(kHelpButtonSize.width, kHelpButtonSize.height);
      *aIsOverridable = false;
      break;
    }

    case StyleAppearance::Toolbarbutton: {
      aResult->SizeTo(0, toolbarButtonHeights[miniControlSize]);
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
        NSSize size = spinnerSettings.minimumSizes[EnumSizeForCocoaSize(NSControlSizeMini)];
        buttonWidth = size.width;
        buttonHeight = size.height;
        if (aAppearance != StyleAppearance::Spinner) {
          // the buttons are half the height of the spinner
          buttonHeight /= 2;
        }
      }
      aResult->SizeTo(buttonWidth, buttonHeight);
      *aIsOverridable = true;
      break;
    }

    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton: {
      SInt32 popupHeight = 0;
      ::GetThemeMetric(kThemeMetricPopupButtonHeight, &popupHeight);
      aResult->SizeTo(0, popupHeight);
      break;
    }

    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Searchfield: {
      // at minimum, we should be tall enough for 9pt text.
      // I'm using hardcoded values here because the appearance manager
      // values for the frame size are incorrect.
      aResult->SizeTo(0, (2 + 2) /* top */ + 9 + (1 + 1) /* bottom */);
      break;
    }

    case StyleAppearance::MozWindowButtonBox: {
      NSSize size = WindowButtonsSize(aFrame);
      aResult->SizeTo(size.width, size.height);
      *aIsOverridable = false;
      break;
    }

    case StyleAppearance::ProgressBar: {
      SInt32 barHeight = 0;
      ::GetThemeMetric(kThemeMetricNormalProgressBarThickness, &barHeight);
      aResult->SizeTo(0, barHeight);
      break;
    }

    case StyleAppearance::Separator: {
      aResult->SizeTo(1, 1);
      break;
    }

    case StyleAppearance::Treetwisty:
    case StyleAppearance::Treetwistyopen: {
      SInt32 twistyHeight = 0, twistyWidth = 0;
      ::GetThemeMetric(kThemeMetricDisclosureButtonWidth, &twistyWidth);
      ::GetThemeMetric(kThemeMetricDisclosureButtonHeight, &twistyHeight);
      aResult->SizeTo(twistyWidth, twistyHeight);
      *aIsOverridable = false;
      break;
    }

    case StyleAppearance::Treeheader:
    case StyleAppearance::Treeheadercell: {
      SInt32 headerHeight = 0;
      ::GetThemeMetric(kThemeMetricListHeaderHeight, &headerHeight);
      aResult->SizeTo(0, headerHeight - 1);  // We don't need the top border.
      break;
    }

    case StyleAppearance::Tab: {
      aResult->SizeTo(0, tabHeights[miniControlSize]);
      break;
    }

    case StyleAppearance::RangeThumb: {
      SInt32 width = 0;
      SInt32 height = 0;
      ::GetThemeMetric(kThemeMetricSliderMinThumbWidth, &width);
      ::GetThemeMetric(kThemeMetricSliderMinThumbHeight, &height);
      aResult->SizeTo(width, height);
      *aIsOverridable = false;
      break;
    }

    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight: {
      *aIsOverridable = false;
      *aResult = ScrollbarDrawingMac::GetMinimumWidgetSize(aAppearance, aFrame, 1.0f);
      break;
    }

    case StyleAppearance::MozMenulistArrowButton:
      *aResult = ScrollbarDrawingMac::GetMinimumWidgetSize(aAppearance, aFrame, 1.0f);
      break;

    case StyleAppearance::Resizer: {
      HIThemeGrowBoxDrawInfo drawInfo;
      drawInfo.version = 0;
      drawInfo.state = kThemeStateActive;
      drawInfo.kind = kHIThemeGrowBoxKindNormal;
      drawInfo.direction = kThemeGrowRight | kThemeGrowDown;
      drawInfo.size = kHIThemeGrowBoxSizeNormal;
      HIPoint pnt = {0, 0};
      HIRect bounds;
      HIThemeGetGrowBoxBounds(&pnt, &drawInfo, &bounds);
      aResult->SizeTo(bounds.size.width, bounds.size.height);
      *aIsOverridable = false;
    }
    default:
      break;
  }

  if (IsHiDPIContext(aPresContext->DeviceContext())) {
    *aResult = *aResult * 2;
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsNativeThemeCocoa::WidgetStateChanged(nsIFrame* aFrame, StyleAppearance aAppearance,
                                       nsAtom* aAttribute, bool* aShouldRepaint,
                                       const nsAttrValue* aOldValue) {
  // Some widget types just never change state.
  switch (aAppearance) {
    case StyleAppearance::MozWindowTitlebar:
    case StyleAppearance::Toolbox:
    case StyleAppearance::Toolbar:
    case StyleAppearance::Statusbar:
    case StyleAppearance::Statusbarpanel:
    case StyleAppearance::Resizerpanel:
    case StyleAppearance::Tooltip:
    case StyleAppearance::Tabpanels:
    case StyleAppearance::Tabpanel:
    case StyleAppearance::Dialog:
    case StyleAppearance::Menupopup:
    case StyleAppearance::Groupbox:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Meter:
    case StyleAppearance::Meterchunk:
    case StyleAppearance::MozMacVibrancyLight:
    case StyleAppearance::MozMacVibrancyDark:
    case StyleAppearance::MozMacVibrantTitlebarLight:
    case StyleAppearance::MozMacVibrantTitlebarDark:
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
        aAttribute == nsGkAtoms::selected || aAttribute == nsGkAtoms::visuallyselected ||
        aAttribute == nsGkAtoms::menuactive || aAttribute == nsGkAtoms::sortDirection ||
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

bool nsNativeThemeCocoa::ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame,
                                             StyleAppearance aAppearance) {
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
    case StyleAppearance::MenulistText:
      if (aFrame && aFrame->GetWritingMode().IsVertical()) {
        return false;
      }
      [[fallthrough]];

    case StyleAppearance::Listbox:
    case StyleAppearance::Dialog:
    case StyleAppearance::Window:
    case StyleAppearance::MozWindowButtonBox:
    case StyleAppearance::MozWindowTitlebar:
    case StyleAppearance::Checkmenuitem:
    case StyleAppearance::Menupopup:
    case StyleAppearance::Menuarrow:
    case StyleAppearance::Menuitem:
    case StyleAppearance::Menuseparator:
    case StyleAppearance::Tooltip:

    case StyleAppearance::Checkbox:
    case StyleAppearance::CheckboxContainer:
    case StyleAppearance::Radio:
    case StyleAppearance::RadioContainer:
    case StyleAppearance::Groupbox:
    case StyleAppearance::MozMacHelpButton:
    case StyleAppearance::MozMacDisclosureButtonOpen:
    case StyleAppearance::MozMacDisclosureButtonClosed:
    case StyleAppearance::Button:
    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown:
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
    // case StyleAppearance::Toolbarbutton:
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
    case StyleAppearance::MozMacSourceList:
    case StyleAppearance::MozMacSourceListSelection:
    case StyleAppearance::MozMacActiveSourceListSelection:

    case StyleAppearance::Range:

    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::ScrollbartrackHorizontal:
      return !IsWidgetStyled(aPresContext, aFrame, aAppearance);

    case StyleAppearance::Scrollcorner:
      return true;

    case StyleAppearance::Resizer: {
      nsIFrame* parentFrame = aFrame->GetParent();
      if (!parentFrame || !parentFrame->IsScrollFrame()) return true;

      // Note that IsWidgetStyled is not called for resizers on Mac. This is
      // because for scrollable containers, the native resizer looks better
      // when (non-overlay) scrollbars are present even when the style is
      // overriden, and the custom transparent resizer looks better when
      // scrollbars are not present.
      nsIScrollableFrame* scrollFrame = do_QueryFrame(parentFrame);
      return (!nsLookAndFeel::UseOverlayScrollbars() && scrollFrame &&
              (!scrollFrame->GetScrollbarVisibility().isEmpty()));
    }

    case StyleAppearance::FocusOutline:
      return true;

    case StyleAppearance::MozMacVibrancyLight:
    case StyleAppearance::MozMacVibrancyDark:
    case StyleAppearance::MozMacVibrantTitlebarLight:
    case StyleAppearance::MozMacVibrantTitlebarDark:
      return true;
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

bool nsNativeThemeCocoa::ThemeDrawsFocusForWidget(StyleAppearance aAppearance) {
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

bool nsNativeThemeCocoa::WidgetAppearanceDependsOnWindowFocus(StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Dialog:
    case StyleAppearance::Groupbox:
    case StyleAppearance::Tabpanels:
    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown:
    case StyleAppearance::Checkmenuitem:
    case StyleAppearance::Menupopup:
    case StyleAppearance::Menuarrow:
    case StyleAppearance::Menuitem:
    case StyleAppearance::Menuseparator:
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
    case StyleAppearance::Resizer:
      return false;
    default:
      return true;
  }
}

bool nsNativeThemeCocoa::IsWindowSheet(nsIFrame* aFrame) {
  NSWindow* win = NativeWindowForFrame(aFrame);
  id winDelegate = [win delegate];
  nsIWidget* widget = [(WindowDelegate*)winDelegate geckoWidget];
  if (!widget) {
    return false;
  }
  return (widget->WindowType() == eWindowType_sheet);
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
    case StyleAppearance::MozMacVibrancyLight:
      return eThemeGeometryTypeVibrancyLight;
    case StyleAppearance::MozMacVibrancyDark:
      return eThemeGeometryTypeVibrancyDark;
    case StyleAppearance::MozMacVibrantTitlebarLight:
      return eThemeGeometryTypeVibrantTitlebarLight;
    case StyleAppearance::MozMacVibrantTitlebarDark:
      return eThemeGeometryTypeVibrantTitlebarDark;
    case StyleAppearance::Tooltip:
      return eThemeGeometryTypeTooltip;
    case StyleAppearance::Menupopup:
      return eThemeGeometryTypeMenu;
    case StyleAppearance::Menuitem:
    case StyleAppearance::Checkmenuitem: {
      EventStates eventState = GetContentState(aFrame, aAppearance);
      bool isDisabled = IsDisabled(aFrame, eventState);
      bool isSelected = !isDisabled && CheckBooleanAttr(aFrame, nsGkAtoms::menuactive);
      return isSelected ? eThemeGeometryTypeHighlightedMenuItem : eThemeGeometryTypeMenu;
    }
    case StyleAppearance::Dialog:
      return IsWindowSheet(aFrame) ? eThemeGeometryTypeSheet : eThemeGeometryTypeUnknown;
    case StyleAppearance::MozMacSourceList:
      return eThemeGeometryTypeSourceList;
    case StyleAppearance::MozMacSourceListSelection:
      return IsInSourceList(aFrame) ? eThemeGeometryTypeSourceListSelection
                                    : eThemeGeometryTypeUnknown;
    case StyleAppearance::MozMacActiveSourceListSelection:
      return IsInSourceList(aFrame) ? eThemeGeometryTypeActiveSourceListSelection
                                    : eThemeGeometryTypeUnknown;
    default:
      return eThemeGeometryTypeUnknown;
  }
}

nsITheme::Transparency nsNativeThemeCocoa::GetWidgetTransparency(nsIFrame* aFrame,
                                                                 StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Menupopup:
    case StyleAppearance::Tooltip:
    case StyleAppearance::Dialog:
      return eTransparent;

    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::Scrollcorner: {
      // We don't use custom scrollbars when using overlay scrollbars.
      if (nsLookAndFeel::UseOverlayScrollbars()) {
        return eTransparent;
      }
      const nsStyleUI* ui = nsLayoutUtils::StyleForScrollbar(aFrame)->StyleUI();
      if (!ui->mScrollbarColor.IsAuto() &&
          ui->mScrollbarColor.AsColors().track.MaybeTransparent()) {
        return eTransparent;
      }
      return eOpaque;
    }

    case StyleAppearance::Statusbar:
      // Knowing that scrollbars and statusbars are opaque improves
      // performance, because we create layers for them.
      return eOpaque;

    case StyleAppearance::Toolbar:
      return eOpaque;

    default:
      return eUnknownTransparency;
  }
}

already_AddRefed<nsITheme> do_GetNativeThemeDoNotUseDirectly() {
  static nsCOMPtr<nsITheme> inst;

  if (!inst) {
    inst = new nsNativeThemeCocoa();
    ClearOnShutdown(&inst);
  }

  return do_AddRef(inst);
}
