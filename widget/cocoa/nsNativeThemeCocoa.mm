/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeCocoa.h"
#include "nsObjCExceptions.h"
#include "nsRangeFrame.h"
#include "nsRenderingContext.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsThemeConstants.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIAtom.h"
#include "nsEventStates.h"
#include "nsINameSpaceManager.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsCocoaFeatures.h"
#include "nsCocoaWindow.h"
#include "nsNativeThemeColors.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMHTMLMeterElement.h"
#include "mozilla/dom/Element.h"
#include "nsLookAndFeel.h"

#include "gfxContext.h"
#include "gfxQuartzSurface.h"
#include "gfxQuartzNativeDrawing.h"
#include <algorithm>

#define DRAW_IN_FRAME_DEBUG 0
#define SCROLLBARS_VISUAL_DEBUG 0

// private Quartz routines needed here
extern "C" {
  CG_EXTERN void CGContextSetCTM(CGContextRef, CGAffineTransform);
}

// Workaround for NSCell control tint drawing
// Without this workaround, NSCells are always drawn with the clear control tint
// as long as they're not attached to an NSControl which is a subview of an active window.
// XXXmstange Why doesn't Webkit need this?
@implementation NSCell (ControlTintWorkaround)
- (int)_realControlTint { return [self controlTint]; }
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

@end;

@implementation CellDrawView

- (BOOL)isFlipped
{
  return YES;
}

- (NSText*)currentEditor
{
  return nil;
}

@end

/**
 * NSProgressBarCell is used to draw progress bars of any size.
 */
@interface NSProgressBarCell : NSCell
{
    /*All instance variables are private*/
    double mValue;
    double mMax;
    bool   mIsIndeterminate;
    bool   mIsHorizontal;
}

- (void)setValue:(double)value;
- (double)value;
- (void)setMax:(double)max;
- (double)max;
- (void)setIndeterminate:(bool)aIndeterminate;
- (bool)isIndeterminate;
- (void)setHorizontal:(bool)aIsHorizontal;
- (bool)isHorizontal;
- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView;
@end

@implementation NSProgressBarCell

- (void)setMax:(double)aMax
{
  mMax = aMax;
}

- (double)max
{
  return mMax;
}

- (void)setValue:(double)aValue
{
  mValue = aValue;
}

- (double)value
{
  return mValue;
}

- (void)setIndeterminate:(bool)aIndeterminate
{
  mIsIndeterminate = aIndeterminate;
}

- (bool)isIndeterminate
{
  return mIsIndeterminate;
}

- (void)setHorizontal:(bool)aIsHorizontal
{
  mIsHorizontal = aIsHorizontal;
}

- (bool)isHorizontal
{
  return mIsHorizontal;
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
  CGContext* cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

  HIThemeTrackDrawInfo tdi;

  tdi.version = 0;
  tdi.min = 0;

  tdi.value = INT32_MAX * (mValue / mMax);
  tdi.max = INT32_MAX;
  tdi.bounds = NSRectToCGRect(cellFrame);
  tdi.attributes = mIsHorizontal ? kThemeTrackHorizontal : 0;
  tdi.enableState = [self controlTint] == NSClearControlTint ? kThemeTrackInactive
                                                             : kThemeTrackActive;

  NSControlSize size = [self controlSize];
  if (size == NSRegularControlSize) {
    tdi.kind = mIsIndeterminate ? kThemeLargeIndeterminateBar
                                : kThemeLargeProgressBar;
  } else {
    NS_ASSERTION(size == NSSmallControlSize,
                 "We shouldn't have another size than small and regular for the moment");
    tdi.kind = mIsIndeterminate ? kThemeMediumIndeterminateBar
                                : kThemeMediumProgressBar;
  }

  int32_t stepsPerSecond = mIsIndeterminate ? 60 : 30;
  int32_t milliSecondsPerStep = 1000 / stepsPerSecond;
  tdi.trackInfo.progress.phase = uint8_t(PR_IntervalToMilliseconds(PR_IntervalNow()) /
                                         milliSecondsPerStep);

  HIThemeDrawTrack(&tdi, NULL, cgContext, kHIThemeOrientationNormal);
}

@end

@interface ContextAwareSearchFieldCell : NSSearchFieldCell
{
  nsIFrame* mContext;
}

// setContext: stores the searchfield nsIFrame so that it can be consulted
// during painting. Please reset this by calling setContext:nullptr as soon as
// you're done with painting because we don't want to keep a dangling pointer.
- (void)setContext:(nsIFrame*)aContext;
@end

@implementation ContextAwareSearchFieldCell

- (id)initTextCell:(NSString*)aString
{
  if ((self = [super initTextCell:aString])) {
    mContext = nullptr;
  }
  return self;
}

- (void)setContext:(nsIFrame*)aContext
{
  mContext = aContext;
}

static BOOL IsToolbarStyleContainer(nsIFrame* aFrame)
{
  nsIContent* content = aFrame->GetContent();
  if (!content)
    return NO;

  if (content->Tag() == nsGkAtoms::toolbar ||
      content->Tag() == nsGkAtoms::toolbox ||
      content->Tag() == nsGkAtoms::statusbar)
    return YES;

  switch (aFrame->StyleDisplay()->mAppearance) {
    case NS_THEME_TOOLBAR:
    case NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR:
    case NS_THEME_STATUSBAR:
      return YES;
    default:
      return NO;
  }
}

- (BOOL)_isToolbarMode
{
  // On 10.7, searchfields have two different styles, depending on whether
  // the searchfield is on top of of window chrome. This function is called on
  // 10.7 during drawing in order to determine which style to use.
  for (nsIFrame* frame = mContext; frame; frame = frame->GetParent()) {
    if (IsToolbarStyleContainer(frame)) {
      return YES;
    }
  }
  return NO;
}

@end

// Workaround for Bug 542048
// On 64-bit, NSSearchFieldCells don't draw focus rings.
#if defined(__x86_64__)

static void DrawFocusRing(NSRect rect, float radius)
{
  NSSetFocusRingStyle(NSFocusRingOnly);
  NSBezierPath* path = [NSBezierPath bezierPath];
  rect = NSInsetRect(rect, radius, radius);
  [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMinX(rect), NSMinY(rect)) radius:radius startAngle:180.0 endAngle:270.0];
  [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMaxX(rect), NSMinY(rect)) radius:radius startAngle:270.0 endAngle:360.0];
  [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMaxX(rect), NSMaxY(rect)) radius:radius startAngle:  0.0 endAngle: 90.0];
  [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMinX(rect), NSMaxY(rect)) radius:radius startAngle: 90.0 endAngle:180.0];
  [path closePath];
  [path fill];
}

@interface SearchFieldCellWithFocusRing : ContextAwareSearchFieldCell {} @end

@implementation SearchFieldCellWithFocusRing

- (void)drawWithFrame:(NSRect)rect inView:(NSView*)controlView
{
  [super drawWithFrame:rect inView:controlView];
  if ([self showsFirstResponder]) {
    DrawFocusRing(rect, NSHeight(rect) / 2);
  }
}

@end
  
#endif

// Copied from nsLookAndFeel.h
// Apple hasn't defined a constant for scollbars with two arrows on each end, so we'll use this one.
static const int kThemeScrollBarArrowsBoth = 2;

#define HITHEME_ORIENTATION kHIThemeOrientationNormal
#define MAX_FOCUS_RING_WIDTH 4

// These enums are for indexing into the margin array.
enum {
  leopardOS = 0
};

enum {
  miniControlSize,
  smallControlSize,
  regularControlSize
};

enum {
  leftMargin,
  topMargin,
  rightMargin,
  bottomMargin
};

static int EnumSizeForCocoaSize(NSControlSize cocoaControlSize) {
  if (cocoaControlSize == NSMiniControlSize)
    return miniControlSize;
  else if (cocoaControlSize == NSSmallControlSize)
    return smallControlSize;
  else
    return regularControlSize;
}

static NSControlSize CocoaSizeForEnum(int32_t enumControlSize) {
  if (enumControlSize == miniControlSize)
    return NSMiniControlSize;
  else if (enumControlSize == smallControlSize)
    return NSSmallControlSize;
  else
    return NSRegularControlSize;
}

static NSString* CUIControlSizeForCocoaSize(NSControlSize aControlSize)
{
  if (aControlSize == NSRegularControlSize)
    return @"regular";
  else if (aControlSize == NSSmallControlSize)
    return @"small";
  else
    return @"mini";
}

static void InflateControlRect(NSRect* rect, NSControlSize cocoaControlSize, const float marginSet[][3][4])
{
  if (!marginSet)
    return;

  static int osIndex = leopardOS;
  int controlSize = EnumSizeForCocoaSize(cocoaControlSize);
  const float* buttonMargins = marginSet[osIndex][controlSize];
  rect->origin.x -= buttonMargins[leftMargin];
  rect->origin.y -= buttonMargins[bottomMargin];
  rect->size.width += buttonMargins[leftMargin] + buttonMargins[rightMargin];
  rect->size.height += buttonMargins[bottomMargin] + buttonMargins[topMargin];
}

static NSWindow* NativeWindowForFrame(nsIFrame* aFrame,
                                      nsIWidget** aTopLevelWidget = NULL)
{
  if (!aFrame)
    return nil;  

  nsIWidget* widget = aFrame->GetNearestWidget();
  if (!widget)
    return nil;

  nsIWidget* topLevelWidget = widget->GetTopLevelWidget();
  if (aTopLevelWidget)
    *aTopLevelWidget = topLevelWidget;

  return (NSWindow*)topLevelWidget->GetNativeData(NS_NATIVE_WINDOW);
}

static BOOL FrameIsInActiveWindow(nsIFrame* aFrame)
{
  nsIWidget* topLevelWidget = NULL;
  NSWindow* win = NativeWindowForFrame(aFrame, &topLevelWidget);
  if (!topLevelWidget || !win)
    return YES;

  // XUL popups, e.g. the toolbar customization popup, can't become key windows,
  // but controls in these windows should still get the active look.
  nsWindowType windowType;
  topLevelWidget->GetWindowType(windowType);
  if (windowType == eWindowType_popup)
    return YES;
  if ([win isSheet])
    return [win isKeyWindow];
  return [win isMainWindow] && ![win attachedSheet];
}

// Toolbar controls and content controls respond to different window
// activeness states.
static BOOL IsActive(nsIFrame* aFrame, BOOL aIsToolbarControl)
{
  if (aIsToolbarControl)
    return [NativeWindowForFrame(aFrame) isMainWindow];
  return FrameIsInActiveWindow(aFrame);
}

NS_IMPL_ISUPPORTS_INHERITED1(nsNativeThemeCocoa, nsNativeTheme, nsITheme)


nsNativeThemeCocoa::nsNativeThemeCocoa()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // provide a local autorelease pool, as this is called during startup
  // before the main event-loop pool is in place
  nsAutoreleasePool pool;

  mPushButtonCell = [[NSButtonCell alloc] initTextCell:nil];
  [mPushButtonCell setButtonType:NSMomentaryPushInButton];
  [mPushButtonCell setHighlightsBy:NSPushInCellMask];

  mRadioButtonCell = [[NSButtonCell alloc] initTextCell:nil];
  [mRadioButtonCell setButtonType:NSRadioButton];

  mCheckboxCell = [[NSButtonCell alloc] initTextCell:nil];
  [mCheckboxCell setButtonType:NSSwitchButton];
  [mCheckboxCell setAllowsMixedState:YES];

#if defined(__x86_64__)
  mSearchFieldCell = [[SearchFieldCellWithFocusRing alloc] initTextCell:@""];
#else
  mSearchFieldCell = [[ContextAwareSearchFieldCell alloc] initTextCell:@""];
#endif
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
                    initWithLevelIndicatorStyle:NSContinuousCapacityLevelIndicatorStyle];

  mCellDrawView = [[CellDrawView alloc] init];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsNativeThemeCocoa::~nsNativeThemeCocoa()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mMeterBarCell release];
  [mProgressBarCell release];
  [mPushButtonCell release];
  [mRadioButtonCell release];
  [mCheckboxCell release];
  [mSearchFieldCell release];
  [mDropdownCell release];
  [mComboBoxCell release];
  [mCellDrawView release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Limit on the area of the target rect (in pixels^2) in
// DrawCellWithScaling(), DrawButton() and DrawScrollbar(), above which we
// don't draw the object into a bitmap buffer.  This is to avoid crashes in
// [NSGraphicsContext graphicsContextWithGraphicsPort:flipped:] and
// CGContextDrawImage(), and also to avoid very poor drawing performance in
// CGContextDrawImage() when it scales the bitmap (particularly if xscale or
// yscale is less than but near 1 -- e.g. 0.9).  This value was determined
// by trial and error, on OS X 10.4.11 and 10.5.4, and on systems with
// different amounts of RAM.
#define BITMAP_MAX_AREA 500000

static int
GetBackingScaleFactorForRendering(CGContextRef cgContext)
{
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
static void DrawCellWithScaling(NSCell *cell,
                                CGContextRef cgContext,
                                const HIRect& destRect,
                                NSControlSize controlSize,
                                NSSize naturalSize,
                                NSSize minimumSize,
                                const float marginSet[][3][4],
                                NSView* view,
                                BOOL mirrorHorizontal)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSRect drawRect = NSMakeRect(destRect.origin.x, destRect.origin.y, destRect.size.width, destRect.size.height);

  if (naturalSize.width != 0.0f)
    drawRect.size.width = naturalSize.width;
  if (naturalSize.height != 0.0f)
    drawRect.size.height = naturalSize.height;

  // Keep aspect ratio when scaling if one dimension is free.
  if (naturalSize.width == 0.0f && naturalSize.height != 0.0f)
    drawRect.size.width = destRect.size.width * naturalSize.height / destRect.size.height;
  if (naturalSize.height == 0.0f && naturalSize.width != 0.0f)
    drawRect.size.height = destRect.size.height * naturalSize.width / destRect.size.width;

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
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:YES]];

    [cell drawWithFrame:drawRect inView:view];

    [NSGraphicsContext setCurrentContext:savedContext];
  }
  else {
    float w = ceil(drawRect.size.width);
    float h = ceil(drawRect.size.height);
    NSRect tmpRect = NSMakeRect(MAX_FOCUS_RING_WIDTH, MAX_FOCUS_RING_WIDTH, w, h);

    // inflate to figure out the frame we need to tell NSCell to draw in, to get something that's 0,0,w,h
    InflateControlRect(&tmpRect, controlSize, marginSet);

    // and then, expand by MAX_FOCUS_RING_WIDTH size to make sure we can capture any focus ring
    w += MAX_FOCUS_RING_WIDTH * 2.0;
    h += MAX_FOCUS_RING_WIDTH * 2.0;

    int backingScaleFactor = GetBackingScaleFactorForRendering(cgContext);
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(NULL,
                                             (int) w * backingScaleFactor, (int) h * backingScaleFactor,
                                             8, (int) w * backingScaleFactor * 4,
                                             rgb, kCGImageAlphaPremultipliedFirst);
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
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:ctx flipped:YES]];

    CGContextScaleCTM(ctx, backingScaleFactor, backingScaleFactor);

    // This is the second flip transform, applied to ctx.
    CGContextScaleCTM(ctx, 1.0f, -1.0f);
    CGContextTranslateCTM(ctx, 0.0f, -(2.0 * tmpRect.origin.y + tmpRect.size.height));

    [cell drawWithFrame:tmpRect inView:view];

    [NSGraphicsContext setCurrentContext:savedContext];

    CGImageRef img = CGBitmapContextCreateImage(ctx);

    // Drop the image into the original destination rectangle, scaling to fit
    // Only scale MAX_FOCUS_RING_WIDTH by xscale/yscale when the resulting rect
    // doesn't extend beyond the overflow rect
    float xscale = destRect.size.width / drawRect.size.width;
    float yscale = destRect.size.height / drawRect.size.height;
    float scaledFocusRingX = xscale < 1.0f ? MAX_FOCUS_RING_WIDTH * xscale : MAX_FOCUS_RING_WIDTH;
    float scaledFocusRingY = yscale < 1.0f ? MAX_FOCUS_RING_WIDTH * yscale : MAX_FOCUS_RING_WIDTH;
    CGContextDrawImage(cgContext, CGRectMake(destRect.origin.x - scaledFocusRingX,
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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

struct CellRenderSettings {
  // The natural dimensions of the control.
  // If a control has no natural dimensions in either/both axes, set to 0.0f.
  NSSize naturalSizes[3];

  // The minimum dimensions of the control.
  // If a control has no minimum dimensions in either/both axes, set to 0.0f.
  NSSize minimumSizes[3];

  // A three-dimensional array,
  // with the first dimension being the OS version (only Leopard for the moment),
  // the second being the control size (mini, small, regular), and the third
  // being the 4 margin values (left, top, right, bottom).
  float margins[1][3][4];
};

/*
 * This is a helper method that returns the required NSControlSize given a size
 * and the size of the three controls plus a tolerance.
 * size - The width or the height of the element to draw.
 * sizes - An array with the all the width/height of the element for its
 *         different sizes.
 * tolerance - The tolerance as passed to DrawCellWithSnapping.
 * NOTE: returns NSRegularControlSize if all values in 'sizes' are zero.
 */
static NSControlSize FindControlSize(CGFloat size, const CGFloat* sizes, CGFloat tolerance)
{
  for (uint32_t i = miniControlSize; i <= regularControlSize; ++i) {
    if (sizes[i] == 0) {
      continue;
    }

    CGFloat next = 0;
    // Find next value.
    for (uint32_t j = i+1; j <= regularControlSize; ++j) {
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
static void DrawCellWithSnapping(NSCell *cell,
                                 CGContextRef cgContext,
                                 const HIRect& destRect,
                                 const CellRenderSettings settings,
                                 float verticalAlignFactor,
                                 NSView* view,
                                 BOOL mirrorHorizontal,
                                 float snapTolerance = 2.0f)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  const float rectWidth = destRect.size.width, rectHeight = destRect.size.height;
  const NSSize *sizes = settings.naturalSizes;
  const NSSize miniSize = sizes[EnumSizeForCocoaSize(NSMiniControlSize)];
  const NSSize smallSize = sizes[EnumSizeForCocoaSize(NSSmallControlSize)];
  const NSSize regularSize = sizes[EnumSizeForCocoaSize(NSRegularControlSize)];

  HIRect drawRect = destRect;

  CGFloat controlWidths[3] = { miniSize.width, smallSize.width, regularSize.width };
  NSControlSize controlSizeX = FindControlSize(rectWidth, controlWidths, snapTolerance);
  CGFloat controlHeights[3] = { miniSize.height, smallSize.height, regularSize.height };
  NSControlSize controlSizeY = FindControlSize(rectHeight, controlHeights, snapTolerance);

  NSControlSize controlSize = NSRegularControlSize;
  int sizeIndex = 0;

  // At some sizes, don't scale but snap.
  const NSControlSize smallerControlSize =
    EnumSizeForCocoaSize(controlSizeX) < EnumSizeForCocoaSize(controlSizeY) ?
    controlSizeX : controlSizeY;
  const int smallerControlSizeIndex = EnumSizeForCocoaSize(smallerControlSize);
  const NSSize size = sizes[smallerControlSizeIndex];
  float diffWidth = size.width ? rectWidth - size.width : 0.0f;
  float diffHeight = size.height ? rectHeight - size.height : 0.0f;
  if (diffWidth >= 0.0f && diffHeight >= 0.0f &&
      diffWidth <= snapTolerance && diffHeight <= snapTolerance) {
    // Snap to the smaller control size.
    controlSize = smallerControlSize;
    sizeIndex = smallerControlSizeIndex;
    // Resize and center the drawRect.
    if (sizes[sizeIndex].width) {
      drawRect.origin.x += ceil((destRect.size.width - sizes[sizeIndex].width) / 2);
      drawRect.size.width = sizes[sizeIndex].width;
    }
    if (sizes[sizeIndex].height) {
      drawRect.origin.y += floor((destRect.size.height - sizes[sizeIndex].height) * verticalAlignFactor);
      drawRect.size.height = sizes[sizeIndex].height;
    }
  } else {
    // Use the larger control size.
    controlSize = EnumSizeForCocoaSize(controlSizeX) > EnumSizeForCocoaSize(controlSizeY) ?
                  controlSizeX : controlSizeY;
    sizeIndex = EnumSizeForCocoaSize(controlSize);
   }

  [cell setControlSize:controlSize];

  NSSize minimumSize = settings.minimumSizes ? settings.minimumSizes[sizeIndex] : NSZeroSize;
  DrawCellWithScaling(cell, cgContext, drawRect, controlSize, sizes[sizeIndex],
                      minimumSize, settings.margins, view, mirrorHorizontal);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static float VerticalAlignFactor(nsIFrame *aFrame)
{
  if (!aFrame)
    return 0.5f; // default: center

  const nsStyleCoord& va = aFrame->StyleTextReset()->mVerticalAlign;
  uint8_t intval = (va.GetUnit() == eStyleUnit_Enumerated)
                     ? va.GetIntValue()
                     : NS_STYLE_VERTICAL_ALIGN_MIDDLE;
  switch (intval) {
    case NS_STYLE_VERTICAL_ALIGN_TOP:
    case NS_STYLE_VERTICAL_ALIGN_TEXT_TOP:
      return 0.0f;

    case NS_STYLE_VERTICAL_ALIGN_SUB:
    case NS_STYLE_VERTICAL_ALIGN_SUPER:
    case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
    case NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE:
      return 0.5f;

    case NS_STYLE_VERTICAL_ALIGN_BASELINE:
    case NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM:
    case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
      return 1.0f;

    default:
      NS_NOTREACHED("invalid vertical-align");
      return 0.5f;
  }
}

// These are the sizes that Gecko needs to request to draw if it wants
// to get a standard-sized Aqua radio button drawn. Note that the rects
// that draw these are actually a little bigger.
static const CellRenderSettings radioSettings = {
  {
    NSMakeSize(11, 11), // mini
    NSMakeSize(13, 13), // small
    NSMakeSize(16, 16)  // regular
  },
  {
    NSZeroSize, NSZeroSize, NSZeroSize
  },
  {
    { // Leopard
      {0, 0, 0, 0},     // mini
      {0, 1, 1, 1},     // small
      {0, 0, 0, 0}      // regular
    }
  }
};

static const CellRenderSettings checkboxSettings = {
  {
    NSMakeSize(11, 11), // mini
    NSMakeSize(13, 13), // small
    NSMakeSize(16, 16)  // regular
  },
  {
    NSZeroSize, NSZeroSize, NSZeroSize
  },
  {
    { // Leopard
      {0, 1, 0, 0},     // mini
      {0, 1, 0, 1},     // small
      {0, 1, 0, 1}      // regular
    }
  }
};

void
nsNativeThemeCocoa::DrawCheckboxOrRadio(CGContextRef cgContext, bool inCheckbox,
                                        const HIRect& inBoxRect, bool inSelected,
                                        nsEventStates inState, nsIFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSButtonCell *cell = inCheckbox ? mCheckboxCell : mRadioButtonCell;
  NSCellStateValue state = inSelected ? NSOnState : NSOffState;

  // Check if we have an indeterminate checkbox
  if (inCheckbox && GetIndeterminate(aFrame))
    state = NSMixedState;

  [cell setEnabled:!IsDisabled(aFrame, inState)];
  [cell setShowsFirstResponder:inState.HasState(NS_EVENT_STATE_FOCUS)];
  [cell setState:state];
  [cell setHighlighted:inState.HasAllStates(NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_HOVER)];
  [cell setControlTint:(FrameIsInActiveWindow(aFrame) ? [NSColor currentControlTint] : NSClearControlTint)];

  // Ensure that the control is square.
  float length = std::min(inBoxRect.size.width, inBoxRect.size.height);
  HIRect drawRect = CGRectMake(inBoxRect.origin.x + (int)((inBoxRect.size.width - length) / 2.0f),
                               inBoxRect.origin.y + (int)((inBoxRect.size.height - length) / 2.0f),
                               length, length);

  DrawCellWithSnapping(cell, cgContext, drawRect,
                       inCheckbox ? checkboxSettings : radioSettings,
                       VerticalAlignFactor(aFrame), mCellDrawView, NO);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static const CellRenderSettings searchFieldSettings = {
  {
    NSMakeSize(0, 16), // mini
    NSMakeSize(0, 19), // small
    NSMakeSize(0, 22)  // regular
  },
  {
    NSMakeSize(32, 0), // mini
    NSMakeSize(38, 0), // small
    NSMakeSize(44, 0)  // regular
  },
  {
    { // Leopard
      {0, 0, 0, 0},     // mini
      {0, 0, 0, 0},     // small
      {0, 0, 0, 0}      // regular
    }
  }
};

void
nsNativeThemeCocoa::DrawSearchField(CGContextRef cgContext, const HIRect& inBoxRect,
                                    nsIFrame* aFrame, nsEventStates inState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  ContextAwareSearchFieldCell* cell = mSearchFieldCell;
  [cell setContext:aFrame];
  [cell setEnabled:!IsDisabled(aFrame, inState)];
  // NOTE: this could probably use inState
  [cell setShowsFirstResponder:IsFocused(aFrame)];

  DrawCellWithSnapping(cell, cgContext, inBoxRect, searchFieldSettings,
                       VerticalAlignFactor(aFrame), mCellDrawView,
                       IsFrameRTL(aFrame));

  [cell setContext:nullptr];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static const CellRenderSettings pushButtonSettings = {
  {
    NSMakeSize(0, 16), // mini
    NSMakeSize(0, 19), // small
    NSMakeSize(0, 22)  // regular
  },
  {
    NSMakeSize(18, 0), // mini
    NSMakeSize(26, 0), // small
    NSMakeSize(30, 0)  // regular
  },
  {
    { // Leopard
      {0, 0, 0, 0},    // mini
      {4, 0, 4, 1},    // small
      {5, 0, 5, 2}     // regular
    }
  }
};

// The height at which we start doing square buttons instead of rounded buttons
// Rounded buttons look bad if drawn at a height greater than 26, so at that point
// we switch over to doing square buttons which looks fine at any size.
#define DO_SQUARE_BUTTON_HEIGHT 26

void
nsNativeThemeCocoa::DrawPushButton(CGContextRef cgContext, const HIRect& inBoxRect,
                                   nsEventStates inState, nsIFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  BOOL isActive = FrameIsInActiveWindow(aFrame);
  BOOL isDisabled = IsDisabled(aFrame, inState);

  [mPushButtonCell setEnabled:!isDisabled];
  [mPushButtonCell setHighlighted:isActive &&
                                  inState.HasAllStates(NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_HOVER)];
  [mPushButtonCell setShowsFirstResponder:inState.HasState(NS_EVENT_STATE_FOCUS) && !isDisabled && isActive];

  // If the button is tall enough, draw the square button style so that buttons with
  // non-standard content look good. Otherwise draw normal rounded aqua buttons.
  if (inBoxRect.size.height > DO_SQUARE_BUTTON_HEIGHT) {
    [mPushButtonCell setBezelStyle:NSShadowlessSquareBezelStyle];
    DrawCellWithScaling(mPushButtonCell, cgContext, inBoxRect, NSRegularControlSize,
                        NSZeroSize, NSMakeSize(14, 0), NULL,
                        mCellDrawView, IsFrameRTL(aFrame));
  } else {
    [mPushButtonCell setBezelStyle:NSRoundedBezelStyle];

    DrawCellWithSnapping(mPushButtonCell, cgContext, inBoxRect, pushButtonSettings,
                         0.5f, mCellDrawView, IsFrameRTL(aFrame), 1.0f);
  }

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

typedef void (*RenderHIThemeControlFunction)(CGContextRef cgContext, const HIRect& aRenderRect, void* aData);

static void
RenderTransformedHIThemeControl(CGContextRef aCGContext, const HIRect& aRect,
                                RenderHIThemeControlFunction aFunc, void* aData,
                                BOOL mirrorHorizontally = NO)
{
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
    int w = ceil(drawRect.size.width) + 2 * MAX_FOCUS_RING_WIDTH;
    int h = ceil(drawRect.size.height) + 2 * MAX_FOCUS_RING_WIDTH;

    int backingScaleFactor = GetBackingScaleFactorForRendering(aCGContext);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmapctx = CGBitmapContextCreate(NULL,
                                                   w * backingScaleFactor,
                                                   h * backingScaleFactor,
                                                   8,
                                                   w * backingScaleFactor * 4,
                                                   colorSpace,
                                                   kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(colorSpace);

    CGContextScaleCTM(bitmapctx, backingScaleFactor, backingScaleFactor);
    CGContextTranslateCTM(bitmapctx, MAX_FOCUS_RING_WIDTH, MAX_FOCUS_RING_WIDTH);

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

    HIRect inflatedDrawRect = CGRectMake(-MAX_FOCUS_RING_WIDTH, -MAX_FOCUS_RING_WIDTH, w, h);
    CGContextDrawImage(aCGContext, inflatedDrawRect, bitmap);

    CGContextSetCTM(aCGContext, ctm);

    CGImageRelease(bitmap);
    CGContextRelease(bitmapctx);
  }

  CGContextSetCTM(aCGContext, savedCTM);
}

static void
RenderButton(CGContextRef cgContext, const HIRect& aRenderRect, void* aData)
{
  HIThemeButtonDrawInfo* bdi = (HIThemeButtonDrawInfo*)aData;
  HIThemeDrawButton(&aRenderRect, bdi, cgContext, kHIThemeOrientationNormal, NULL);
}

void
nsNativeThemeCocoa::DrawButton(CGContextRef cgContext, ThemeButtonKind inKind,
                               const HIRect& inBoxRect, bool inIsDefault,
                               ThemeButtonValue inValue, ThemeButtonAdornment inAdornment,
                               nsEventStates inState, nsIFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  BOOL isActive = FrameIsInActiveWindow(aFrame);
  BOOL isDisabled = IsDisabled(aFrame, inState);

  HIThemeButtonDrawInfo bdi;
  bdi.version = 0;
  bdi.kind = inKind;
  bdi.value = inValue;
  bdi.adornment = inAdornment;

  if (isDisabled) {
    bdi.state = kThemeStateUnavailable;
  }
  else if (inState.HasAllStates(NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_HOVER)) {
    bdi.state = kThemeStatePressed;
  }
  else {
    if (inKind == kThemeArrowButton)
      bdi.state = kThemeStateUnavailable; // these are always drawn as unavailable
    else if (!isActive && inKind == kThemeListHeaderButton)
      bdi.state = kThemeStateInactive;
    else
      bdi.state = kThemeStateActive;
  }

  if (inState.HasState(NS_EVENT_STATE_FOCUS) && isActive)
    bdi.adornment |= kThemeAdornmentFocus;

  if (inIsDefault && !isDisabled && isActive &&
      !inState.HasState(NS_EVENT_STATE_ACTIVE)) {
    bdi.adornment |= kThemeAdornmentDefault;
    bdi.animation.time.start = 0;
    bdi.animation.time.current = CFAbsoluteTimeGetCurrent();
  }

  HIRect drawFrame = inBoxRect;

  if (inKind == kThemePushButton) {
    drawFrame.size.height -= 2;
    if (inBoxRect.size.height < pushButtonSettings.naturalSizes[smallControlSize].height) {
      bdi.kind = kThemePushButtonMini;
    }
    else if (inBoxRect.size.height < pushButtonSettings.naturalSizes[regularControlSize].height) {
      bdi.kind = kThemePushButtonSmall;
      drawFrame.origin.y -= 1;
      drawFrame.origin.x += 1;
      drawFrame.size.width -= 2;
    }
  }
  else if (inKind == kThemeListHeaderButton) {
    CGContextClipToRect(cgContext, inBoxRect);
    // Always remove the top border.
    drawFrame.origin.y -= 1;
    drawFrame.size.height += 1;
    // Remove the left border in LTR mode and the right border in RTL mode.
    drawFrame.size.width += 1;
    bool isLast = IsLastTreeHeaderCell(aFrame);
    if (isLast)
      drawFrame.size.width += 1; // Also remove the other border.
    if (!IsFrameRTL(aFrame) || isLast)
      drawFrame.origin.x -= 1;
  }

  RenderTransformedHIThemeControl(cgContext, drawFrame, RenderButton, &bdi,
                                  IsFrameRTL(aFrame));

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static const CellRenderSettings dropdownSettings = {
  {
    NSMakeSize(0, 16), // mini
    NSMakeSize(0, 19), // small
    NSMakeSize(0, 22)  // regular
  },
  {
    NSMakeSize(18, 0), // mini
    NSMakeSize(38, 0), // small
    NSMakeSize(44, 0)  // regular
  },
  {
    { // Leopard
      {1, 1, 2, 1},    // mini
      {3, 0, 3, 1},    // small
      {3, 0, 3, 0}     // regular
    }
  }
};

static const CellRenderSettings editableMenulistSettings = {
  {
    NSMakeSize(0, 15), // mini
    NSMakeSize(0, 18), // small
    NSMakeSize(0, 21)  // regular
  },
  {
    NSMakeSize(18, 0), // mini
    NSMakeSize(38, 0), // small
    NSMakeSize(44, 0)  // regular
  },
  {
    { // Leopard
      {0, 0, 2, 2},    // mini
      {0, 0, 3, 2},    // small
      {0, 1, 3, 3}     // regular
    }
  }
};

void
nsNativeThemeCocoa::DrawDropdown(CGContextRef cgContext, const HIRect& inBoxRect,
                                 nsEventStates inState, uint8_t aWidgetType,
                                 nsIFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mDropdownCell setPullsDown:(aWidgetType == NS_THEME_BUTTON)];

  BOOL isEditable = (aWidgetType == NS_THEME_DROPDOWN_TEXTFIELD);
  NSCell* cell = isEditable ? (NSCell*)mComboBoxCell : (NSCell*)mDropdownCell;

  [cell setEnabled:!IsDisabled(aFrame, inState)];
  [cell setShowsFirstResponder:(IsFocused(aFrame) || inState.HasState(NS_EVENT_STATE_FOCUS))];
  [cell setHighlighted:IsOpenButton(aFrame)];
  [cell setControlTint:(FrameIsInActiveWindow(aFrame) ? [NSColor currentControlTint] : NSClearControlTint)];

  const CellRenderSettings& settings = isEditable ? editableMenulistSettings : dropdownSettings;
  DrawCellWithSnapping(cell, cgContext, inBoxRect, settings,
                       0.5f, mCellDrawView, IsFrameRTL(aFrame));

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsNativeThemeCocoa::DrawSpinButtons(CGContextRef cgContext, ThemeButtonKind inKind,
                                    const HIRect& inBoxRect, ThemeDrawState inDrawState,
                                    ThemeButtonAdornment inAdornment,
                                    nsEventStates inState, nsIFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeButtonDrawInfo bdi;
  bdi.version = 0;
  bdi.kind = inKind;
  bdi.value = kThemeButtonOff;
  bdi.adornment = inAdornment;

  if (IsDisabled(aFrame, inState))
    bdi.state = kThemeStateUnavailable;
  else
    bdi.state = FrameIsInActiveWindow(aFrame) ? inDrawState : kThemeStateActive;

  HIThemeDrawButton(&inBoxRect, &bdi, cgContext, HITHEME_ORIENTATION, NULL);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsNativeThemeCocoa::DrawFrame(CGContextRef cgContext, HIThemeFrameKind inKind,
                              const HIRect& inBoxRect, bool inDisabled,
                              nsEventStates inState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeFrameDrawInfo fdi;
  fdi.version = 0;
  fdi.kind = inKind;

  // We don't ever set an inactive state for this because it doesn't
  // look right (see other apps).
  fdi.state = inDisabled ? kThemeStateUnavailable : kThemeStateActive;

  // for some reason focus rings on listboxes draw incorrectly
  if (inKind == kHIThemeFrameListBox)
    fdi.isFocused = 0;
  else
    fdi.isFocused = inState.HasState(NS_EVENT_STATE_FOCUS);

  // HIThemeDrawFrame takes the rect for the content area of the frame, not
  // the bounding rect for the frame. Here we reduce the size of the rect we
  // will pass to make it the size of the content.
  HIRect drawRect = inBoxRect;
  if (inKind == kHIThemeFrameTextFieldSquare) {
    SInt32 frameOutset = 0;
    ::GetThemeMetric(kThemeMetricEditTextFrameOutset, &frameOutset);
    drawRect.origin.x += frameOutset;
    drawRect.origin.y += frameOutset;
    drawRect.size.width -= frameOutset * 2;
    drawRect.size.height -= frameOutset * 2;
  }
  else if (inKind == kHIThemeFrameListBox) {
    SInt32 frameOutset = 0;
    ::GetThemeMetric(kThemeMetricListBoxFrameOutset, &frameOutset);
    drawRect.origin.x += frameOutset;
    drawRect.origin.y += frameOutset;
    drawRect.size.width -= frameOutset * 2;
    drawRect.size.height -= frameOutset * 2;
  }

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  HIThemeDrawFrame(&drawRect, &fdi, cgContext, HITHEME_ORIENTATION);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static const CellRenderSettings progressSettings[2][2] = {
  // Vertical progress bar.
  {
    // Determined settings.
    {
      {
        NSZeroSize, // mini
        NSMakeSize(10, 0), // small
        NSMakeSize(16, 0)  // regular
      },
      {
        NSZeroSize, NSZeroSize, NSZeroSize
      },
      {
        { // Leopard
          {0, 0, 0, 0},     // mini
          {1, 1, 1, 1},     // small
          {1, 1, 1, 1}      // regular
        }
      }
    },
    // There is no horizontal margin in regular undetermined size.
    {
      {
        NSZeroSize, // mini
        NSMakeSize(10, 0), // small
        NSMakeSize(16, 0)  // regular
      },
      {
        NSZeroSize, NSZeroSize, NSZeroSize
      },
      {
        { // Leopard
          {0, 0, 0, 0},     // mini
          {1, 1, 1, 1},     // small
          {1, 0, 1, 0}      // regular
        }
      }
    }
  },
  // Horizontal progress bar.
  {
    // Determined settings.
    {
      {
        NSZeroSize, // mini
        NSMakeSize(0, 10), // small
        NSMakeSize(0, 16)  // regular
      },
      {
        NSZeroSize, NSZeroSize, NSZeroSize
      },
      {
        { // Leopard
          {0, 0, 0, 0},     // mini
          {1, 1, 1, 1},     // small
          {1, 1, 1, 1}      // regular
        }
      }
    },
    // There is no horizontal margin in regular undetermined size.
    {
      {
        NSZeroSize, // mini
        NSMakeSize(0, 10), // small
        NSMakeSize(0, 16)  // regular
      },
      {
        NSZeroSize, NSZeroSize, NSZeroSize
      },
      {
        { // Leopard
          {0, 0, 0, 0},     // mini
          {1, 1, 1, 1},     // small
          {0, 1, 0, 1}      // regular
        }
      }
    }
  }
};

void
nsNativeThemeCocoa::DrawProgress(CGContextRef cgContext, const HIRect& inBoxRect,
                                 bool inIsIndeterminate, bool inIsHorizontal,
                                 double inValue, double inMaxValue,
                                 nsIFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSProgressBarCell* cell = mProgressBarCell;

  [cell setValue:inValue];
  [cell setMax:inMaxValue];
  [cell setIndeterminate:inIsIndeterminate];
  [cell setHorizontal:inIsHorizontal];
  [cell setControlTint:(FrameIsInActiveWindow(aFrame) ? [NSColor currentControlTint]
                                                      : NSClearControlTint)];

  DrawCellWithSnapping(cell, cgContext, inBoxRect,
                       progressSettings[inIsHorizontal][inIsIndeterminate],
                       VerticalAlignFactor(aFrame), mCellDrawView,
                       IsFrameRTL(aFrame));

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static const CellRenderSettings meterSetting = {
  {
    NSMakeSize(0, 16), // mini
    NSMakeSize(0, 16), // small
    NSMakeSize(0, 16)  // regular
  },
  {
    NSZeroSize, NSZeroSize, NSZeroSize
  },
  {
    { // Leopard
      {1, 1, 1, 1},     // mini
      {1, 1, 1, 1},     // small
      {1, 1, 1, 1}      // regular
    }
  }
};

void
nsNativeThemeCocoa::DrawMeter(CGContextRef cgContext, const HIRect& inBoxRect,
                              nsIFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK

  NS_PRECONDITION(aFrame, "aFrame should not be null here!");

  nsCOMPtr<nsIDOMHTMLMeterElement> meterElement =
    do_QueryInterface(aFrame->GetContent());

  // When using -moz-meterbar on an non meter element, we will not be able to
  // get all the needed information so we just draw an empty meter.
  if (!meterElement) {
    DrawCellWithSnapping(mMeterBarCell, cgContext, inBoxRect,
                         meterSetting, VerticalAlignFactor(aFrame),
                         mCellDrawView, IsFrameRTL(aFrame));
    return;
  }

  double value;
  double min;
  double max;
  double low;
  double high;
  double optimum;

  // NOTE: if we were allowed to static_cast to HTMLMeterElement we would be
  // able to use nicer getters...
  meterElement->GetValue(&value);
  meterElement->GetMin(&min);
  meterElement->GetMax(&max);
  meterElement->GetLow(&low);
  meterElement->GetHigh(&high);
  meterElement->GetOptimum(&optimum);

  NSLevelIndicatorCell* cell = mMeterBarCell;

  [cell setMinValue:min];
  [cell setMaxValue:max];
  [cell setDoubleValue:value];

  /**
   * The way HTML and Cocoa defines the meter/indicator widget are different.
   * So, we are going to use a trick to get the Cocoa widget showing what we
   * are expecting: we set the warningValue or criticalValue to the current
   * value when we want to have the widget to be in the warning or critical
   * state.
   */
  nsEventStates states = aFrame->GetContent()->AsElement()->State();

  // Reset previously set warning and critical values.
  [cell setWarningValue:max+1];
  [cell setCriticalValue:max+1];

  if (states.HasState(NS_EVENT_STATE_SUB_OPTIMUM)) {
    [cell setWarningValue:value];
  } else if (states.HasState(NS_EVENT_STATE_SUB_SUB_OPTIMUM)) {
    [cell setCriticalValue:value];
  }

  HIRect rect = CGRectStandardize(inBoxRect);
  BOOL vertical = IsVerticalMeter(aFrame);

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

  DrawCellWithSnapping(cell, cgContext, rect,
                       meterSetting, VerticalAlignFactor(aFrame),
                       mCellDrawView, !vertical && IsFrameRTL(aFrame));

  CGContextRestoreGState(cgContext);

  NS_OBJC_END_TRY_ABORT_BLOCK
}

void
nsNativeThemeCocoa::DrawTabPanel(CGContextRef cgContext, const HIRect& inBoxRect,
                                 nsIFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeTabPaneDrawInfo tpdi;

  tpdi.version = 1;
  tpdi.state = FrameIsInActiveWindow(aFrame) ? kThemeStateActive : kThemeStateInactive;
  tpdi.direction = kThemeTabNorth;
  tpdi.size = kHIThemeTabSizeNormal;
  tpdi.kind = kHIThemeTabKindNormal;

  HIThemeDrawTabPane(&inBoxRect, &tpdi, cgContext, HITHEME_ORIENTATION);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsNativeThemeCocoa::DrawScale(CGContextRef cgContext, const HIRect& inBoxRect,
                              nsEventStates inState, bool inIsVertical,
                              bool inIsReverse, int32_t inCurrentValue,
                              int32_t inMinValue, int32_t inMaxValue,
                              nsIFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeTrackDrawInfo tdi;

  tdi.version = 0;
  tdi.kind = kThemeMediumSlider;
  tdi.bounds = inBoxRect;
  tdi.min = inMinValue;
  tdi.max = inMaxValue;
  tdi.value = inCurrentValue;
  tdi.attributes = kThemeTrackShowThumb;
  if (!inIsVertical)
    tdi.attributes |= kThemeTrackHorizontal;
  if (inIsReverse)
    tdi.attributes |= kThemeTrackRightToLeft;
  if (inState.HasState(NS_EVENT_STATE_FOCUS))
    tdi.attributes |= kThemeTrackHasFocus;
  if (IsDisabled(aFrame, inState))
    tdi.enableState = kThemeTrackDisabled;
  else
    tdi.enableState = FrameIsInActiveWindow(aFrame) ? kThemeTrackActive : kThemeTrackInactive;
  tdi.trackInfo.slider.thumbDir = kThemeThumbPlain;
  tdi.trackInfo.slider.pressState = 0;

  HIThemeDrawTrack(&tdi, NULL, cgContext, HITHEME_ORIENTATION);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsIFrame*
nsNativeThemeCocoa::SeparatorResponsibility(nsIFrame* aBefore, nsIFrame* aAfter)
{
  // Usually a separator is drawn by the segment to the right of the
  // separator, but pressed and selected segments have higher priority.
  if (!aBefore || !aAfter)
    return nullptr;
  if (IsSelectedButton(aAfter))
    return aAfter;
  if (IsSelectedButton(aBefore) || IsPressedButton(aBefore))
    return aBefore;
  return aAfter;
}

CGRect
nsNativeThemeCocoa::SeparatorAdjustedRect(CGRect aRect, nsIFrame* aLeft,
                                          nsIFrame* aCurrent, nsIFrame* aRight)
{
  // A separator between two segments should always be located in the leftmost
  // pixel column of the segment to the right of the separator, regardless of
  // who ends up drawing it.
  // CoreUI draws the separators inside the drawing rect.
  if (aLeft && SeparatorResponsibility(aLeft, aCurrent) == aLeft) {
    // The left button draws the separator, so we need to make room for it.
    aRect.origin.x += 1;
    aRect.size.width -= 1;
  }
  if (SeparatorResponsibility(aCurrent, aRight) == aCurrent) {
    // We draw the right separator, so we need to extend the draw rect into the
    // segment to our right.
    aRect.size.width += 1;
  }
  return aRect;
}

static NSString* ToolbarButtonPosition(BOOL aIsFirst, BOOL aIsLast)
{
  if (aIsFirst) {
    if (aIsLast)
      return @"kCUISegmentPositionOnly";
    return @"kCUISegmentPositionFirst";
  }
  if (aIsLast)
    return @"kCUISegmentPositionLast";
  return @"kCUISegmentPositionMiddle";
}

struct SegmentedControlRenderSettings {
  const CGFloat* heights;
  const NSString* widgetName;
  const BOOL ignoresPressedWhenSelected;
  const BOOL isToolbarControl;
};

static const CGFloat tabHeights[3] = { 17, 20, 23 };

static const SegmentedControlRenderSettings tabRenderSettings = {
  tabHeights, @"tab", YES, NO
};

static const CGFloat toolbarButtonHeights[3] = { 15, 18, 22 };

static const SegmentedControlRenderSettings toolbarButtonRenderSettings = {
  toolbarButtonHeights, @"kCUIWidgetButtonSegmentedSCurve", NO, YES
};

void
nsNativeThemeCocoa::DrawSegment(CGContextRef cgContext, const HIRect& inBoxRect,
                                nsEventStates inState, nsIFrame* aFrame,
                                const SegmentedControlRenderSettings& aSettings)
{
  BOOL isActive = IsActive(aFrame, aSettings.isToolbarControl);
  BOOL isFocused = inState.HasState(NS_EVENT_STATE_FOCUS);
  BOOL isSelected = IsSelectedButton(aFrame);
  BOOL isPressed = IsPressedButton(aFrame);
  if (isSelected && aSettings.ignoresPressedWhenSelected) {
    isPressed = NO;
  }

  BOOL isRTL = IsFrameRTL(aFrame);
  nsIFrame* left = GetAdjacentSiblingFrameWithSameAppearance(aFrame, isRTL);
  nsIFrame* right = GetAdjacentSiblingFrameWithSameAppearance(aFrame, !isRTL);
  CGRect drawRect = SeparatorAdjustedRect(inBoxRect, left, aFrame, right);
  if (drawRect.size.width * drawRect.size.height > CUIDRAW_MAX_AREA) {
    return;
  }
  BOOL drawLeftSeparator = SeparatorResponsibility(left, aFrame) == aFrame;
  BOOL drawRightSeparator = SeparatorResponsibility(aFrame, right) == aFrame;
  NSControlSize controlSize = FindControlSize(drawRect.size.height, aSettings.heights, 4.0f);

  CUIDraw([NSWindow coreUIRenderer], drawRect, cgContext,
          (CFDictionaryRef)[NSDictionary dictionaryWithObjectsAndKeys:
            aSettings.widgetName, @"widget",
            ToolbarButtonPosition(!left, !right), @"kCUIPositionKey",
            [NSNumber numberWithBool:drawLeftSeparator], @"kCUISegmentLeadingSeparatorKey",
            [NSNumber numberWithBool:drawRightSeparator], @"kCUISegmentTrailingSeparatorKey",
            [NSNumber numberWithBool:isSelected], @"value",
            (isPressed ? @"pressed" : (isActive ? @"normal" : @"inactive")), @"state",
            [NSNumber numberWithBool:isFocused], @"focus",
            CUIControlSizeForCocoaSize(controlSize), @"size",
            [NSNumber numberWithBool:YES], @"is.flipped",
            @"up", @"direction",
            nil],
          nil);
}

static inline UInt8
ConvertToPressState(nsEventStates aButtonState, UInt8 aPressState)
{
  // If the button is pressed, return the press state passed in. Otherwise, return 0.
  return aButtonState.HasAllStates(NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_HOVER) ? aPressState : 0;
}

void 
nsNativeThemeCocoa::GetScrollbarPressStates(nsIFrame *aFrame, nsEventStates aButtonStates[])
{
  static nsIContent::AttrValuesArray attributeValues[] = {
    &nsGkAtoms::scrollbarUpTop,
    &nsGkAtoms::scrollbarDownTop,
    &nsGkAtoms::scrollbarUpBottom,
    &nsGkAtoms::scrollbarDownBottom,
    nullptr
  };

  // Get the state of any scrollbar buttons in our child frames
  for (nsIFrame *childFrame = aFrame->GetFirstPrincipalChild(); 
       childFrame;
       childFrame = childFrame->GetNextSibling()) {

    nsIContent *childContent = childFrame->GetContent();
    if (!childContent) continue;
    int32_t attrIndex = childContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::sbattr, 
                                                      attributeValues, eCaseMatters);
    if (attrIndex < 0) continue;

    aButtonStates[attrIndex] = GetContentState(childFrame, NS_THEME_BUTTON);
  }
}

// Both of the following sets of numbers were derived by loading the testcase in
// bmo bug 380185 in Safari and observing its behavior for various heights of scrollbar.
// These magic numbers are the minimum sizes we can draw a scrollbar and still 
// have room for everything to display, including the thumb
#define MIN_SCROLLBAR_SIZE_WITH_THUMB 61
#define MIN_SMALL_SCROLLBAR_SIZE_WITH_THUMB 49
// And these are the minimum sizes if we don't draw the thumb
#define MIN_SCROLLBAR_SIZE 56
#define MIN_SMALL_SCROLLBAR_SIZE 46

void
nsNativeThemeCocoa::GetScrollbarDrawInfo(HIThemeTrackDrawInfo& aTdi, nsIFrame *aFrame, 
                                         const CGSize& aSize, bool aShouldGetButtonStates)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  int32_t curpos = CheckIntAttr(aFrame, nsGkAtoms::curpos, 0);
  int32_t minpos = CheckIntAttr(aFrame, nsGkAtoms::minpos, 0);
  int32_t maxpos = CheckIntAttr(aFrame, nsGkAtoms::maxpos, 100);
  int32_t thumbSize = CheckIntAttr(aFrame, nsGkAtoms::pageincrement, 10);

  bool isHorizontal = aFrame->GetContent()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::orient, 
                                                          nsGkAtoms::horizontal, eCaseMatters);
  bool isSmall = aFrame->StyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL;

  aTdi.version = 0;
  aTdi.kind = isSmall ? kThemeSmallScrollBar : kThemeMediumScrollBar;
  aTdi.bounds.origin = CGPointZero;
  aTdi.bounds.size = aSize;
  aTdi.min = minpos;
  aTdi.max = maxpos;
  aTdi.value = curpos;
  aTdi.attributes = 0;
  aTdi.enableState = kThemeTrackActive;
  if (isHorizontal)
    aTdi.attributes |= kThemeTrackHorizontal;

  aTdi.trackInfo.scrollbar.viewsize = (SInt32)thumbSize;

  // This should be done early on so things like "kThemeTrackNothingToScroll" can
  // override the active enable state.
  aTdi.enableState = FrameIsInActiveWindow(aFrame) ? kThemeTrackActive : kThemeTrackInactive;

  /* Only display features if we have enough room for them.
   * Gecko still maintains the scrollbar info; this is just a visual issue (bug 380185).
   */
  int32_t longSideLength = (int32_t)(isHorizontal ? (aSize.width) : (aSize.height));
  if (longSideLength >= (isSmall ? MIN_SMALL_SCROLLBAR_SIZE_WITH_THUMB : MIN_SCROLLBAR_SIZE_WITH_THUMB)) {
    aTdi.attributes |= kThemeTrackShowThumb;
  }
  else if (longSideLength < (isSmall ? MIN_SMALL_SCROLLBAR_SIZE : MIN_SCROLLBAR_SIZE)) {
    aTdi.enableState = kThemeTrackNothingToScroll;
    return;
  }

  aTdi.trackInfo.scrollbar.pressState = 0;

  // Only go get these scrollbar button states if we need it. For example,
  // there's no reason to look up scrollbar button states when we're only
  // creating a TrackDrawInfo to determine the size of the thumb. There's
  // also no reason to do this on Lion or later, whose scrollbars have no
  // arrow buttons.
  if (aShouldGetButtonStates && !nsCocoaFeatures::OnLionOrLater()) {
    nsEventStates buttonStates[4];
    GetScrollbarPressStates(aFrame, buttonStates);
    NSString *buttonPlacement = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleScrollBarVariant"];
    // It seems that unless all four buttons are showing, kThemeTopOutsideArrowPressed is the correct constant for
    // the up scrollbar button.
    if ([buttonPlacement isEqualToString:@"DoubleBoth"]) {
      aTdi.trackInfo.scrollbar.pressState = ConvertToPressState(buttonStates[0], kThemeTopOutsideArrowPressed) |
                                            ConvertToPressState(buttonStates[1], kThemeTopInsideArrowPressed) |
                                            ConvertToPressState(buttonStates[2], kThemeBottomInsideArrowPressed) |
                                            ConvertToPressState(buttonStates[3], kThemeBottomOutsideArrowPressed);
    } else {
      aTdi.trackInfo.scrollbar.pressState = ConvertToPressState(buttonStates[0], kThemeTopOutsideArrowPressed) |
                                            ConvertToPressState(buttonStates[1], kThemeBottomOutsideArrowPressed) |
                                            ConvertToPressState(buttonStates[2], kThemeTopOutsideArrowPressed) |
                                            ConvertToPressState(buttonStates[3], kThemeBottomOutsideArrowPressed);
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static void
RenderScrollbar(CGContextRef cgContext, const HIRect& aRenderRect, void* aData)
{
  HIThemeTrackDrawInfo* tdi = (HIThemeTrackDrawInfo*)aData;
  HIThemeDrawTrack(tdi, NULL, cgContext, HITHEME_ORIENTATION);
}

void
nsNativeThemeCocoa::DrawScrollbar(CGContextRef aCGContext, const HIRect& aBoxRect, nsIFrame *aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeTrackDrawInfo tdi;
  GetScrollbarDrawInfo(tdi, aFrame, aBoxRect.size, true); // True means we want the press states
  RenderTransformedHIThemeControl(aCGContext, aBoxRect, RenderScrollbar, &tdi);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsIFrame*
nsNativeThemeCocoa::GetParentScrollbarFrame(nsIFrame *aFrame)
{
  // Walk our parents to find a scrollbar frame
  nsIFrame *scrollbarFrame = aFrame;
  do {
    if (scrollbarFrame->GetType() == nsGkAtoms::scrollbarFrame) break;
  } while ((scrollbarFrame = scrollbarFrame->GetParent()));
  
  // We return null if we can't find a parent scrollbar frame
  return scrollbarFrame;
}

static bool
ToolbarCanBeUnified(CGContextRef cgContext, const HIRect& inBoxRect, NSWindow* aWindow)
{
  if (![aWindow isKindOfClass:[ToolbarWindow class]] ||
      [(ToolbarWindow*)aWindow drawsContentsIntoWindowFrame])
    return false;

  float unifiedToolbarHeight = [(ToolbarWindow*)aWindow unifiedToolbarHeight];
  CGAffineTransform ctm = CGContextGetUserSpaceToDeviceSpaceTransform(cgContext);
  CGRect deviceRect = CGRectApplyAffineTransform(inBoxRect, ctm);
  return inBoxRect.origin.x == 0 &&
         deviceRect.size.width >= [aWindow frame].size.width &&
         inBoxRect.origin.y <= 0.0 &&
         floor(inBoxRect.origin.y + inBoxRect.size.height) <= unifiedToolbarHeight;
}

void
nsNativeThemeCocoa::DrawUnifiedToolbar(CGContextRef cgContext, const HIRect& inBoxRect,
                                       NSWindow* aWindow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  float titlebarHeight = [(ToolbarWindow*)aWindow titlebarHeight];
  float unifiedHeight = titlebarHeight + inBoxRect.size.height;

  BOOL isMain = [aWindow isMainWindow];

  CGContextSaveGState(cgContext);
  CGContextClipToRect(cgContext, inBoxRect);

  CGRect drawRect = CGRectOffset(inBoxRect, 0, -titlebarHeight);
  if (drawRect.size.width * drawRect.size.height <= CUIDRAW_MAX_AREA) {
    CUIDraw([NSWindow coreUIRenderer], drawRect, cgContext,
            (CFDictionaryRef)[NSDictionary dictionaryWithObjectsAndKeys:
              @"kCUIWidgetWindowFrame", @"widget",
              @"regularwin", @"windowtype",
              (isMain ? @"normal" : @"inactive"), @"state",
              [NSNumber numberWithInt:unifiedHeight], @"kCUIWindowFrameUnifiedTitleBarHeightKey",
              [NSNumber numberWithBool:YES], @"kCUIWindowFrameDrawTitleSeparatorKey",
              [NSNumber numberWithBool:YES], @"is.flipped",
              nil],
            nil);
  }

  CGContextRestoreGState(cgContext);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsNativeThemeCocoa::DrawStatusBar(CGContextRef cgContext, const HIRect& inBoxRect,
                                  nsIFrame *aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (inBoxRect.size.height < 2.0f)
    return;

  CGContextSaveGState(cgContext);
  CGContextClipToRect(cgContext, inBoxRect);

  // kCUIWidgetWindowFrame draws a complete window frame with both title bar
  // and bottom bar. We only want the bottom bar, so we extend the draw rect
  // upwards to make space for the title bar, and then we clip it away.
  CGRect drawRect = inBoxRect;
  const int extendUpwards = 40;
  drawRect.origin.y -= extendUpwards;
  drawRect.size.height += extendUpwards;
  if (drawRect.size.width * drawRect.size.height <= CUIDRAW_MAX_AREA) {
    CUIDraw([NSWindow coreUIRenderer], drawRect, cgContext,
            (CFDictionaryRef)[NSDictionary dictionaryWithObjectsAndKeys:
              @"kCUIWidgetWindowFrame", @"widget",
              @"regularwin", @"windowtype",
              (IsActive(aFrame, YES) ? @"normal" : @"inactive"), @"state",
              [NSNumber numberWithInt:inBoxRect.size.height], @"kCUIWindowFrameBottomBarHeightKey",
              [NSNumber numberWithBool:YES], @"kCUIWindowFrameDrawBottomBarSeparatorKey",
              [NSNumber numberWithBool:YES], @"is.flipped",
              nil],
            nil);
  }

  CGContextRestoreGState(cgContext);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static void
RenderResizer(CGContextRef cgContext, const HIRect& aRenderRect, void* aData)
{
  HIThemeGrowBoxDrawInfo* drawInfo = (HIThemeGrowBoxDrawInfo*)aData;
  HIThemeDrawGrowBox(&CGPointZero, drawInfo, cgContext, kHIThemeOrientationNormal);
}

void
nsNativeThemeCocoa::DrawResizer(CGContextRef cgContext, const HIRect& aRect,
                                nsIFrame *aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeGrowBoxDrawInfo drawInfo;
  drawInfo.version = 0;
  drawInfo.state = kThemeStateActive;
  drawInfo.kind = kHIThemeGrowBoxKindNormal;
  drawInfo.direction = kThemeGrowRight | kThemeGrowDown;
  drawInfo.size = kHIThemeGrowBoxSizeNormal;

  RenderTransformedHIThemeControl(cgContext, aRect, RenderResizer, &drawInfo,
                                  IsFrameRTL(aFrame));

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static bool
IsHiDPIContext(nsDeviceContext* aContext)
{
  return nsPresContext::AppUnitsPerCSSPixel() >=
    2 * aContext->UnscaledAppUnitsPerDevPixel();
}

NS_IMETHODIMP
nsNativeThemeCocoa::DrawWidgetBackground(nsRenderingContext* aContext,
                                         nsIFrame* aFrame,
                                         uint8_t aWidgetType,
                                         const nsRect& aRect,
                                         const nsRect& aDirtyRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // setup to draw into the correct port
  int32_t p2a = aContext->AppUnitsPerDevPixel();

  gfxRect nativeDirtyRect(aDirtyRect.x, aDirtyRect.y,
                          aDirtyRect.width, aDirtyRect.height);
  gfxRect nativeWidgetRect(aRect.x, aRect.y, aRect.width, aRect.height);
  nativeWidgetRect.ScaleInverse(gfxFloat(p2a));
  nativeDirtyRect.ScaleInverse(gfxFloat(p2a));
  nativeWidgetRect.Round();
  if (nativeWidgetRect.IsEmpty())
    return NS_OK; // Don't attempt to draw invisible widgets.

  gfxContext* thebesCtx = aContext->ThebesContext();
  if (!thebesCtx)
    return NS_ERROR_FAILURE;

  gfxContextMatrixAutoSaveRestore save(thebesCtx);

  bool hidpi = IsHiDPIContext(aContext->DeviceContext());
  if (hidpi) {
    // Use high-resolution drawing.
    nativeWidgetRect.ScaleInverse(2.0f);
    nativeDirtyRect.ScaleInverse(2.0f);
    thebesCtx->Scale(2.0f, 2.0f);
  }

  gfxQuartzNativeDrawing nativeDrawing(thebesCtx, nativeDirtyRect,
                                       hidpi ? 2.0f : 1.0f);

  CGContextRef cgContext = nativeDrawing.BeginNativeDrawing();
  if (cgContext == nullptr) {
    // The Quartz surface handles 0x0 surfaces by internally
    // making all operations no-ops; there's no cgcontext created for them.
    // Unfortunately, this means that callers that want to render
    // directly to the CGContext need to be aware of this quirk.
    return NS_OK;
  }

#if 0
  if (1 /*aWidgetType == NS_THEME_TEXTFIELD*/) {
    fprintf(stderr, "Native theme drawing widget %d [%p] dis:%d in rect [%d %d %d %d]\n",
            aWidgetType, aFrame, IsDisabled(aFrame), aRect.x, aRect.y, aRect.width, aRect.height);
    fprintf(stderr, "Cairo matrix: [%f %f %f %f %f %f]\n",
            mat.xx, mat.yx, mat.xy, mat.yy, mat.x0, mat.y0);
    fprintf(stderr, "Native theme xform[0]: [%f %f %f %f %f %f]\n",
            mm0.a, mm0.b, mm0.c, mm0.d, mm0.tx, mm0.ty);
    CGAffineTransform mm = CGContextGetCTM(cgContext);
    fprintf(stderr, "Native theme xform[1]: [%f %f %f %f %f %f]\n",
            mm.a, mm.b, mm.c, mm.d, mm.tx, mm.ty);
  }
#endif

  CGRect macRect = CGRectMake(nativeWidgetRect.X(), nativeWidgetRect.Y(),
                              nativeWidgetRect.Width(), nativeWidgetRect.Height());

#if 0
  fprintf(stderr, "    --> macRect %f %f %f %f\n",
          macRect.origin.x, macRect.origin.y, macRect.size.width, macRect.size.height);
  CGRect bounds = CGContextGetClipBoundingBox(cgContext);
  fprintf(stderr, "    --> clip bounds: %f %f %f %f\n",
          bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height);

  //CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 1.0, 0.1);
  //CGContextFillRect(cgContext, bounds);
#endif

  nsEventStates eventState = GetContentState(aFrame, aWidgetType);

  switch (aWidgetType) {
    case NS_THEME_DIALOG: {
      HIThemeSetFill(kThemeBrushDialogBackgroundActive, NULL, cgContext, HITHEME_ORIENTATION);
      CGContextFillRect(cgContext, macRect);
    }
      break;

    case NS_THEME_MENUPOPUP: {
      HIThemeMenuDrawInfo mdi;
      memset(&mdi, 0, sizeof(mdi));
      mdi.version = 0;
      mdi.menuType = IsDisabled(aFrame, eventState) ?
                       static_cast<ThemeMenuType>(kThemeMenuTypeInactive) :
                       static_cast<ThemeMenuType>(kThemeMenuTypePopUp);

      bool isLeftOfParent = false;
      if (IsSubmenu(aFrame, &isLeftOfParent) && !isLeftOfParent) {
        mdi.menuType = kThemeMenuTypeHierarchical;
      }
      
      // The rounded corners draw outside the frame.
      CGRect deflatedRect = CGRectMake(macRect.origin.x, macRect.origin.y + 4,
                                       macRect.size.width, macRect.size.height - 8);
      HIThemeDrawMenuBackground(&deflatedRect, &mdi, cgContext, HITHEME_ORIENTATION);
    }
      break;

    case NS_THEME_MENUITEM: {
      if (thebesCtx->OriginalSurface()->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA) {
        // Clear the background to get correct transparency.
        CGContextClearRect(cgContext, macRect);
      }

      // maybe use kThemeMenuItemHierBackground or PopUpBackground instead of just Plain?
      HIThemeMenuItemDrawInfo drawInfo;
      memset(&drawInfo, 0, sizeof(drawInfo));
      drawInfo.version = 0;
      drawInfo.itemType = kThemeMenuItemPlain;
      drawInfo.state = (IsDisabled(aFrame, eventState) ?
                         static_cast<ThemeMenuState>(kThemeMenuDisabled) :
                         CheckBooleanAttr(aFrame, nsGkAtoms::menuactive) ?
                           static_cast<ThemeMenuState>(kThemeMenuSelected) :
                           static_cast<ThemeMenuState>(kThemeMenuActive));

      // XXX pass in the menu rect instead of always using the item rect
      HIRect ignored;
      HIThemeDrawMenuItem(&macRect, &macRect, &drawInfo, cgContext, HITHEME_ORIENTATION, &ignored);
    }
      break;

    case NS_THEME_MENUSEPARATOR: {
      ThemeMenuState menuState;
      if (IsDisabled(aFrame, eventState)) {
        menuState = kThemeMenuDisabled;
      }
      else {
        menuState = CheckBooleanAttr(aFrame, nsGkAtoms::menuactive) ?
                    kThemeMenuSelected : kThemeMenuActive;
      }

      HIThemeMenuItemDrawInfo midi = { 0, kThemeMenuItemPlain, menuState };
      HIThemeDrawMenuSeparator(&macRect, &macRect, &midi, cgContext, HITHEME_ORIENTATION);
    }
      break;

    case NS_THEME_TOOLTIP:
      CGContextSetRGBFillColor(cgContext, 0.996, 1.000, 0.792, 0.950);
      CGContextFillRect(cgContext, macRect);
      break;

    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO: {
      bool isCheckbox = (aWidgetType == NS_THEME_CHECKBOX);
      DrawCheckboxOrRadio(cgContext, isCheckbox, macRect, GetCheckedOrSelected(aFrame, !isCheckbox),
                          eventState, aFrame);
    }
      break;

    case NS_THEME_BUTTON:
      if (IsDefaultButton(aFrame)) {
        if (!IsDisabled(aFrame, eventState) && FrameIsInActiveWindow(aFrame) &&
            !QueueAnimatedContentForRefresh(aFrame->GetContent(), 10)) {
          NS_WARNING("Unable to animate button!");
        }
        DrawButton(cgContext, kThemePushButton, macRect, true,
                   kThemeButtonOff, kThemeAdornmentNone, eventState, aFrame);
      } else if (IsButtonTypeMenu(aFrame)) {
        DrawDropdown(cgContext, macRect, eventState, aWidgetType, aFrame);
      } else {
        DrawPushButton(cgContext, macRect, eventState, aFrame);
      }
      break;

    case NS_THEME_BUTTON_BEVEL:
      DrawButton(cgContext, kThemeMediumBevelButton, macRect,
                 IsDefaultButton(aFrame), kThemeButtonOff, kThemeAdornmentNone,
                 eventState, aFrame);
      break;

    case NS_THEME_SPINNER: {
      ThemeDrawState state = kThemeStateActive;
      nsIContent* content = aFrame->GetContent();
      if (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::state,
                               NS_LITERAL_STRING("up"), eCaseMatters)) {
        state = kThemeStatePressedUp;
      }
      else if (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::state,
                                    NS_LITERAL_STRING("down"), eCaseMatters)) {
        state = kThemeStatePressedDown;
      }

      DrawSpinButtons(cgContext, kThemeIncDecButton, macRect, state,
                      kThemeAdornmentNone, eventState, aFrame);
    }
      break;

    case NS_THEME_TOOLBAR_BUTTON:
      DrawSegment(cgContext, macRect, eventState, aFrame, toolbarButtonRenderSettings);
      break;

    case NS_THEME_TOOLBAR_SEPARATOR: {
      HIThemeSeparatorDrawInfo sdi = { 0, kThemeStateActive };
      HIThemeDrawSeparator(&macRect, &sdi, cgContext, HITHEME_ORIENTATION);
    }
      break;

    case NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR:
    case NS_THEME_TOOLBAR: {
      NSWindow* win = NativeWindowForFrame(aFrame);
      if (ToolbarCanBeUnified(cgContext, macRect, win)) {
        DrawUnifiedToolbar(cgContext, macRect, win);
        break;
      }
      BOOL isMain = [win isMainWindow];
      CGRect drawRect = macRect;

      // top border
      drawRect.size.height = 1.0f;
      DrawNativeGreyColorInRect(cgContext, toolbarTopBorderGrey, drawRect, isMain);

      // background
      drawRect.origin.y += drawRect.size.height;
      drawRect.size.height = macRect.size.height - 2.0f;
      DrawNativeGreyColorInRect(cgContext, toolbarFillGrey, drawRect, isMain);

      // bottom border
      drawRect.origin.y += drawRect.size.height;
      drawRect.size.height = 1.0f;
      DrawNativeGreyColorInRect(cgContext, toolbarBottomBorderGrey, drawRect, isMain);
    }
      break;

    case NS_THEME_TOOLBOX: {
      HIThemeHeaderDrawInfo hdi = { 0, kThemeStateActive, kHIThemeHeaderKindWindow };
      HIThemeDrawHeader(&macRect, &hdi, cgContext, HITHEME_ORIENTATION);
    }
      break;

    case NS_THEME_STATUSBAR: 
      DrawStatusBar(cgContext, macRect, aFrame);
      break;

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD:
      DrawDropdown(cgContext, macRect, eventState, aWidgetType, aFrame);
      break;

    case NS_THEME_DROPDOWN_BUTTON:
      DrawButton(cgContext, kThemeArrowButton, macRect, false, kThemeButtonOn,
                 kThemeAdornmentArrowDownArrow, eventState, aFrame);
      break;

    case NS_THEME_GROUPBOX: {
      HIThemeGroupBoxDrawInfo gdi = { 0, kThemeStateActive, kHIThemeGroupBoxKindPrimary };
      HIThemeDrawGroupBox(&macRect, &gdi, cgContext, HITHEME_ORIENTATION);
      break;
    }

    case NS_THEME_TEXTFIELD:
      // HIThemeSetFill is not available on 10.3
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      CGContextFillRect(cgContext, macRect);

      // XUL textboxes set the native appearance on the containing box, while
      // concrete focus is set on the html:input element within it. We can
      // though, check the focused attribute of xul textboxes in this case.
      // On Mac, focus rings are always shown for textboxes, so we do not need
      // to check the window's focus ring state here
      if (aFrame->GetContent()->IsXUL() && IsFocused(aFrame)) {
        eventState |= NS_EVENT_STATE_FOCUS;
      }

      DrawFrame(cgContext, kHIThemeFrameTextFieldSquare, macRect,
                IsDisabled(aFrame, eventState) || IsReadOnly(aFrame), eventState);
      break;
      
    case NS_THEME_SEARCHFIELD:
      DrawSearchField(cgContext, macRect, aFrame, eventState);
      break;

    case NS_THEME_PROGRESSBAR:
      if (!QueueAnimatedContentForRefresh(aFrame->GetContent(), 30)) {
        NS_WARNING("Unable to animate progressbar!");
      }
      DrawProgress(cgContext, macRect, IsIndeterminateProgress(aFrame, eventState),
                   aFrame->StyleDisplay()->mOrient != NS_STYLE_ORIENT_VERTICAL,
		   GetProgressValue(aFrame), GetProgressMaxValue(aFrame), aFrame);
      break;

    case NS_THEME_PROGRESSBAR_VERTICAL:
      DrawProgress(cgContext, macRect, IsIndeterminateProgress(aFrame, eventState),
                   false, GetProgressValue(aFrame),
                   GetProgressMaxValue(aFrame), aFrame);
      break;

    case NS_THEME_METERBAR:
      DrawMeter(cgContext, macRect, aFrame);
      break;

    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_METERBAR_CHUNK:
      // Do nothing: progress and meter bars cases will draw chunks.
      break;

    case NS_THEME_TREEVIEW_TWISTY:
      DrawButton(cgContext, kThemeDisclosureButton, macRect, false,
                 kThemeDisclosureRight, kThemeAdornmentNone, eventState, aFrame);
      break;

    case NS_THEME_TREEVIEW_TWISTY_OPEN:
      DrawButton(cgContext, kThemeDisclosureButton, macRect, false,
                 kThemeDisclosureDown, kThemeAdornmentNone, eventState, aFrame);
      break;

    case NS_THEME_TREEVIEW_HEADER_CELL: {
      TreeSortDirection sortDirection = GetTreeSortDirection(aFrame);
      DrawButton(cgContext, kThemeListHeaderButton, macRect, false,
                 sortDirection == eTreeSortDirection_Natural ? kThemeButtonOff : kThemeButtonOn,
                 sortDirection == eTreeSortDirection_Ascending ?
                 kThemeAdornmentHeaderButtonSortUp : kThemeAdornmentNone, eventState, aFrame);
    }
      break;

    case NS_THEME_TREEVIEW_TREEITEM:
    case NS_THEME_TREEVIEW:
      // HIThemeSetFill is not available on 10.3
      // HIThemeSetFill(kThemeBrushWhite, NULL, cgContext, HITHEME_ORIENTATION);
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      CGContextFillRect(cgContext, macRect);
      break;

    case NS_THEME_TREEVIEW_HEADER:
      // do nothing, taken care of by individual header cells
    case NS_THEME_TREEVIEW_HEADER_SORTARROW:
      // do nothing, taken care of by treeview header
    case NS_THEME_TREEVIEW_LINE:
      // do nothing, these lines don't exist on macos
      break;

    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL: {
      int32_t curpos = CheckIntAttr(aFrame, nsGkAtoms::curpos, 0);
      int32_t minpos = CheckIntAttr(aFrame, nsGkAtoms::minpos, 0);
      int32_t maxpos = CheckIntAttr(aFrame, nsGkAtoms::maxpos, 100);
      if (!maxpos)
        maxpos = 100;

      bool reverse = aFrame->GetContent()->
        AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                    NS_LITERAL_STRING("reverse"), eCaseMatters);
      DrawScale(cgContext, macRect, eventState,
                (aWidgetType == NS_THEME_SCALE_VERTICAL), reverse,
                curpos, minpos, maxpos, aFrame);
    }
      break;

    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
      // do nothing, drawn by scale
      break;

    case NS_THEME_RANGE: {
      nsRangeFrame *rangeFrame = do_QueryFrame(aFrame);
      if (!rangeFrame) {
        break;
      }
      // DrawScale requires integer min, max and value. This is purely for
      // drawing, so we normalize to a range 0-1000 here.
      int32_t value = int32_t(rangeFrame->GetValueAsFractionOfRange() * 1000);
      int32_t min = 0;
      int32_t max = 1000;
      bool isVertical = !IsRangeHorizontal(aFrame);
      bool reverseDir =
        isVertical ||
        rangeFrame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
      DrawScale(cgContext, macRect, eventState, isVertical, reverseDir,
                value, min, max, aFrame);
      break;
    }

    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBAR:
      if (nsLookAndFeel::GetInt(
            nsLookAndFeel::eIntID_UseOverlayScrollbars) == 0) {
        DrawScrollbar(cgContext, macRect, aFrame);
      }
      break;
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
      if (nsLookAndFeel::GetInt(
            nsLookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
        BOOL isHorizontal = (aWidgetType == NS_THEME_SCROLLBAR_THUMB_HORIZONTAL);
        BOOL isRolledOver = CheckBooleanAttr(GetParentScrollbarFrame(aFrame),
                                             nsGkAtoms::hover);
        if (!isRolledOver) {
          if (isHorizontal) {
            macRect.origin.y += 4;
            macRect.size.height -= 4;
          } else {
            macRect.origin.x += 4;
            macRect.size.width -= 4;
          }
        }
        const BOOL isOnTopOfDarkBackground = IsDarkBackground(aFrame);
        CUIDraw([NSWindow coreUIRenderer], macRect, cgContext,
                (CFDictionaryRef)[NSDictionary dictionaryWithObjectsAndKeys:
                  @"kCUIWidgetOverlayScrollBar", @"widget",
                  @"regular", @"size",
                  (isRolledOver ? @"rollover" : @""), @"state",
                  (isHorizontal ? @"kCUIOrientHorizontal" : @"kCUIOrientVertical"), @"kCUIOrientationKey",
                  (isOnTopOfDarkBackground ? @"kCUIVariantWhite" : @""), @"kCUIVariantKey",
                  [NSNumber numberWithBool:YES], @"indiconly",
                  [NSNumber numberWithBool:YES], @"kCUIThumbProportionKey",
                  [NSNumber numberWithBool:YES], @"is.flipped",
                  nil],
                nil);
      }
      break;
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
#if SCROLLBARS_VISUAL_DEBUG
      CGContextSetRGBFillColor(cgContext, 1.0, 0, 0, 0.6);
      CGContextFillRect(cgContext, macRect);
#endif
    break;
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
#if SCROLLBARS_VISUAL_DEBUG
      CGContextSetRGBFillColor(cgContext, 0, 1.0, 0, 0.6);
      CGContextFillRect(cgContext, macRect);
#endif
    break;
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
      if (nsLookAndFeel::GetInt(
            nsLookAndFeel::eIntID_UseOverlayScrollbars) != 0 &&
          CheckBooleanAttr(GetParentScrollbarFrame(aFrame), nsGkAtoms::hover)) {
        BOOL isHorizontal = (aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL);
        const BOOL isOnTopOfDarkBackground = IsDarkBackground(aFrame);
        CUIDraw([NSWindow coreUIRenderer], macRect, cgContext,
                (CFDictionaryRef)[NSDictionary dictionaryWithObjectsAndKeys:
                  @"kCUIWidgetOverlayScrollBar", @"widget",
                  @"regular", @"size",
                  (isHorizontal ? @"kCUIOrientHorizontal" : @"kCUIOrientVertical"), @"kCUIOrientationKey",
                  (isOnTopOfDarkBackground ? @"kCUIVariantWhite" : @""), @"kCUIVariantKey",
                  [NSNumber numberWithBool:YES], @"noindicator",
                  [NSNumber numberWithBool:YES], @"kCUIThumbProportionKey",
                  [NSNumber numberWithBool:YES], @"is.flipped",
                  nil],
                nil);
      }
      break;

    case NS_THEME_TEXTFIELD_MULTILINE: {
      // we have to draw this by hand because there is no HITheme value for it
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      
      CGContextFillRect(cgContext, macRect);

      CGContextSetLineWidth(cgContext, 1.0);
      CGContextSetShouldAntialias(cgContext, false);

      // stroke everything but the top line of the text area
      CGContextSetRGBStrokeColor(cgContext, 0.6, 0.6, 0.6, 1.0);
      CGContextBeginPath(cgContext);
      CGContextMoveToPoint(cgContext, macRect.origin.x, macRect.origin.y + 1);
      CGContextAddLineToPoint(cgContext, macRect.origin.x, macRect.origin.y + macRect.size.height);
      CGContextAddLineToPoint(cgContext, macRect.origin.x + macRect.size.width - 1, macRect.origin.y + macRect.size.height);
      CGContextAddLineToPoint(cgContext, macRect.origin.x + macRect.size.width - 1, macRect.origin.y + 1);
      CGContextStrokePath(cgContext);

      // stroke the line across the top of the text area
      CGContextSetRGBStrokeColor(cgContext, 0.4510, 0.4510, 0.4510, 1.0);
      CGContextBeginPath(cgContext);
      CGContextMoveToPoint(cgContext, macRect.origin.x, macRect.origin.y + 1);
      CGContextAddLineToPoint(cgContext, macRect.origin.x + macRect.size.width - 1, macRect.origin.y + 1);
      CGContextStrokePath(cgContext);

      // draw a focus ring
      if (eventState.HasState(NS_EVENT_STATE_FOCUS)) {
        // We need to bring the rectangle in by 1 pixel on each side.
        CGRect cgr = CGRectMake(macRect.origin.x + 1,
                                macRect.origin.y + 1,
                                macRect.size.width - 2,
                                macRect.size.height - 2);
        HIThemeDrawFocusRect(&cgr, true, cgContext, kHIThemeOrientationNormal);
      }
    }
      break;

    case NS_THEME_LISTBOX: {
      // We have to draw this by hand because kHIThemeFrameListBox drawing
      // is buggy on 10.5, see bug 579259.
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      CGContextFillRect(cgContext, macRect);

      // #8E8E8E for the top border, #BEBEBE for the rest.
      float x = macRect.origin.x, y = macRect.origin.y;
      float w = macRect.size.width, h = macRect.size.height;
      CGContextSetRGBFillColor(cgContext, 0.557, 0.557, 0.557, 1.0);
      CGContextFillRect(cgContext, CGRectMake(x, y, w, 1));
      CGContextSetRGBFillColor(cgContext, 0.745, 0.745, 0.745, 1.0);
      CGContextFillRect(cgContext, CGRectMake(x, y + 1, 1, h - 1));
      CGContextFillRect(cgContext, CGRectMake(x + w - 1, y + 1, 1, h - 1));
      CGContextFillRect(cgContext, CGRectMake(x + 1, y + h - 1, w - 2, 1));
    }
      break;
    
    case NS_THEME_TAB:
      DrawSegment(cgContext, macRect, eventState, aFrame, tabRenderSettings);
      break;

    case NS_THEME_TAB_PANELS:
      DrawTabPanel(cgContext, macRect, aFrame);
      break;

    case NS_THEME_RESIZER:
      DrawResizer(cgContext, macRect, aFrame);
      break;
  }

  nativeDrawing.EndNativeDrawing();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsIntMargin
nsNativeThemeCocoa::RTLAwareMargin(const nsIntMargin& aMargin, nsIFrame* aFrame)
{
  if (IsFrameRTL(aFrame)) {
    // Return a copy of aMargin w/ right & left reversed:
    return nsIntMargin(aMargin.top, aMargin.left,
                       aMargin.bottom, aMargin.right);
  }

  return aMargin;
}

static const nsIntMargin kAquaDropdownBorder(1, 22, 2, 5);
static const nsIntMargin kAquaComboboxBorder(3, 20, 3, 4);
static const nsIntMargin kAquaSearchfieldBorder(3, 5, 2, 19);

NS_IMETHODIMP
nsNativeThemeCocoa::GetWidgetBorder(nsDeviceContext* aContext, 
                                    nsIFrame* aFrame,
                                    uint8_t aWidgetType,
                                    nsIntMargin* aResult)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  aResult->SizeTo(0, 0, 0, 0);

  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    {
      if (IsButtonTypeMenu(aFrame)) {
        *aResult = RTLAwareMargin(kAquaDropdownBorder, aFrame);
      } else {
        aResult->SizeTo(1, 7, 3, 7);
      }
      break;
    }

    case NS_THEME_TOOLBAR_BUTTON:
    {
      aResult->SizeTo(1, 4, 1, 4);
      break;
    }

    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
    {
      // nsFormControlFrame::GetIntrinsicWidth and nsFormControlFrame::GetIntrinsicHeight
      // assume a border width of 2px.
      aResult->SizeTo(2, 2, 2, 2);
      break;
    }

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
      *aResult = RTLAwareMargin(kAquaDropdownBorder, aFrame);
      break;

    case NS_THEME_DROPDOWN_TEXTFIELD:
      *aResult = RTLAwareMargin(kAquaComboboxBorder, aFrame);
      break;

    case NS_THEME_TEXTFIELD:
    {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricEditTextFrameOutset, &frameOutset);

      SInt32 textPadding = 0;
      ::GetThemeMetric(kThemeMetricEditTextWhitespace, &textPadding);

      frameOutset += textPadding;

      aResult->SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
      break;
    }

    case NS_THEME_TEXTFIELD_MULTILINE:
      aResult->SizeTo(1, 1, 1, 1);
      break;

    case NS_THEME_SEARCHFIELD:
      *aResult = RTLAwareMargin(kAquaSearchfieldBorder, aFrame);
      break;

    case NS_THEME_LISTBOX:
    {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricListBoxFrameOutset, &frameOutset);
      aResult->SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
      break;
    }

    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    {
      bool isHorizontal = (aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL);

      // On Lion and later, scrollbars have no arrows.
      if (!nsCocoaFeatures::OnLionOrLater()) {
        // There's only an endcap to worry about when both arrows are on the bottom
        NSString *buttonPlacement = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleScrollBarVariant"];
        if (!buttonPlacement || [buttonPlacement isEqualToString:@"DoubleMax"]) {
          nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
          if (!scrollbarFrame) return NS_ERROR_FAILURE;
          bool isSmall = (scrollbarFrame->StyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL);

          // There isn't a metric for this, so just hardcode a best guess at the value.
          // This value is even less exact due to the fact that the endcap is partially concave.
          int32_t endcapSize = isSmall ? 5 : 6;

          if (isHorizontal)
            aResult->SizeTo(0, 0, 0, endcapSize);
          else
            aResult->SizeTo(endcapSize, 0, 0, 0);
        }
      }

      if (nsLookAndFeel::GetInt(
            nsLookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
        if (isHorizontal) {
          aResult->SizeTo(2, 1, 1, 1);
        } else {
          aResult->SizeTo(1, 1, 1, 2);
        }
      }

      break;
    }

    case NS_THEME_STATUSBAR:
      aResult->SizeTo(1, 0, 0, 0);
      break;
  }

  if (IsHiDPIContext(aContext)) {
    *aResult = *aResult + *aResult; // doubled
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Return false here to indicate that CSS padding values should be used. There is
// no reason to make a distinction between padding and border values, just specify
// whatever values you want in GetWidgetBorder and only use this to return true
// if you want to override CSS padding values.
bool
nsNativeThemeCocoa::GetWidgetPadding(nsDeviceContext* aContext, 
                                     nsIFrame* aFrame,
                                     uint8_t aWidgetType,
                                     nsIntMargin* aResult)
{
  // We don't want CSS padding being used for certain widgets.
  // See bug 381639 for an example of why.
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    // Radios and checkboxes return a fixed size in GetMinimumWidgetSize
    // and have a meaningful baseline, so they can't have
    // author-specified padding.
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
      aResult->SizeTo(0, 0, 0, 0);
      return true;
  }
  return false;
}

bool
nsNativeThemeCocoa::GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                                      uint8_t aWidgetType, nsRect* aOverflowRect)
{
  int32_t p2a = aContext->AppUnitsPerDevPixel();
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_SEARCHFIELD:
    case NS_THEME_LISTBOX:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
    case NS_THEME_TAB:
    {
      // We assume that the above widgets can draw a focus ring that will be less than
      // or equal to 4 pixels thick.
      nsIntMargin extraSize = nsIntMargin(MAX_FOCUS_RING_WIDTH,
                                          MAX_FOCUS_RING_WIDTH,
                                          MAX_FOCUS_RING_WIDTH,
                                          MAX_FOCUS_RING_WIDTH);
      nsMargin m(NSIntPixelsToAppUnits(extraSize.top, p2a),
                 NSIntPixelsToAppUnits(extraSize.right, p2a),
                 NSIntPixelsToAppUnits(extraSize.bottom, p2a),
                 NSIntPixelsToAppUnits(extraSize.left, p2a));
      aOverflowRect->Inflate(m);
      return true;
    }
    case NS_THEME_PROGRESSBAR:
    {
      // Progress bars draw a 2 pixel white shadow under their progress indicators
      nsMargin m(0, 0, NSIntPixelsToAppUnits(2, p2a), 0);
      aOverflowRect->Inflate(m);
      return true;
    }
  }

  return false;
}

static const int32_t kRegularScrollbarThumbMinSize = 26;
static const int32_t kSmallScrollbarThumbMinSize = 19;

NS_IMETHODIMP
nsNativeThemeCocoa::GetMinimumWidgetSize(nsRenderingContext* aContext,
                                         nsIFrame* aFrame,
                                         uint8_t aWidgetType,
                                         nsIntSize* aResult,
                                         bool* aIsOverridable)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  aResult->SizeTo(0,0);
  *aIsOverridable = true;

  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    {
      aResult->SizeTo(pushButtonSettings.minimumSizes[miniControlSize].width,
                      pushButtonSettings.naturalSizes[miniControlSize].height);
      break;
    }

    case NS_THEME_TOOLBAR_BUTTON:
    {
      aResult->SizeTo(0, toolbarButtonHeights[miniControlSize]);
      break;
    }

    case NS_THEME_SPINNER:
    {
      SInt32 buttonHeight = 0, buttonWidth = 0;
      ::GetThemeMetric(kThemeMetricLittleArrowsWidth, &buttonWidth);
      ::GetThemeMetric(kThemeMetricLittleArrowsHeight, &buttonHeight);
      aResult->SizeTo(buttonWidth, buttonHeight);
      *aIsOverridable = false;
      break;
    }

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    {
      SInt32 popupHeight = 0;
      ::GetThemeMetric(kThemeMetricPopupButtonHeight, &popupHeight);
      aResult->SizeTo(0, popupHeight);
      break;
    }
 
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_SEARCHFIELD:
    {
      // at minimum, we should be tall enough for 9pt text.
      // I'm using hardcoded values here because the appearance manager
      // values for the frame size are incorrect.
      aResult->SizeTo(0, (2 + 2) /* top */ + 9 + (1 + 1) /* bottom */);
      break;
    }
      
    case NS_THEME_PROGRESSBAR:
    {
      SInt32 barHeight = 0;
      ::GetThemeMetric(kThemeMetricNormalProgressBarThickness, &barHeight);
      aResult->SizeTo(0, barHeight);
      break;
    }

    case NS_THEME_TREEVIEW_TWISTY:
    case NS_THEME_TREEVIEW_TWISTY_OPEN:   
    {
      SInt32 twistyHeight = 0, twistyWidth = 0;
      ::GetThemeMetric(kThemeMetricDisclosureButtonWidth, &twistyWidth);
      ::GetThemeMetric(kThemeMetricDisclosureButtonHeight, &twistyHeight);
      aResult->SizeTo(twistyWidth, twistyHeight);
      *aIsOverridable = false;
      break;
    }
    
    case NS_THEME_TREEVIEW_HEADER:
    case NS_THEME_TREEVIEW_HEADER_CELL:
    {
      SInt32 headerHeight = 0;
      ::GetThemeMetric(kThemeMetricListHeaderHeight, &headerHeight);
      aResult->SizeTo(0, headerHeight - 1); // We don't need the top border.
      break;
    }

    case NS_THEME_TAB:
    {
      aResult->SizeTo(0, tabHeights[miniControlSize]);
      break;
    }

    case NS_THEME_RANGE:
    {
      // The Mac Appearance Manager API (the old API we're currently using)
      // doesn't define constants to obtain a minimum size for sliders. We use
      // the "thickness" of a slider that has default dimensions for both the
      // minimum width and height to get something sane and so that paint
      // invalidation works.
      SInt32 size = 0;
      if (IsRangeHorizontal(aFrame)) {
        ::GetThemeMetric(kThemeMetricHSliderHeight, &size);
      } else {
        ::GetThemeMetric(kThemeMetricVSliderWidth, &size);
      }
      aResult->SizeTo(size, size);
      *aIsOverridable = true;
      break;
    }

    case NS_THEME_RANGE_THUMB:
    {
      SInt32 width = 0;
      SInt32 height = 0;
      ::GetThemeMetric(kThemeMetricSliderMinThumbWidth, &width);
      ::GetThemeMetric(kThemeMetricSliderMinThumbHeight, &height);
      aResult->SizeTo(width, height);
      *aIsOverridable = false;
      break;
    }

    case NS_THEME_SCALE_HORIZONTAL:
    {
      SInt32 scaleHeight = 0;
      ::GetThemeMetric(kThemeMetricHSliderHeight, &scaleHeight);
      aResult->SizeTo(scaleHeight, scaleHeight);
      *aIsOverridable = false;
      break;
    }

    case NS_THEME_SCALE_VERTICAL:
    {
      SInt32 scaleWidth = 0;
      ::GetThemeMetric(kThemeMetricVSliderWidth, &scaleWidth);
      aResult->SizeTo(scaleWidth, scaleWidth);
      *aIsOverridable = false;
      break;
    }

    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    {
      // Find our parent scrollbar frame in order to find out whether we're in
      // a small or a large scrollbar.
      nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
      if (!scrollbarFrame)
        return NS_ERROR_FAILURE;

      bool isSmall = (scrollbarFrame->StyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL);
      bool isHorizontal = (aWidgetType == NS_THEME_SCROLLBAR_THUMB_HORIZONTAL);
      int32_t& minSize = isHorizontal ? aResult->width : aResult->height;
      minSize = isSmall ? kSmallScrollbarThumbMinSize : kRegularScrollbarThumbMinSize;
      break;
    }

    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    {
      *aIsOverridable = false;

      if (nsLookAndFeel::GetInt(
            nsLookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
        aResult->SizeTo(16, 16);
        break;
      }
      // Intentional fallthrough to next case.
    }

    case NS_THEME_SCROLLBAR_NON_DISAPPEARING:
    {
      // yeah, i know i'm cheating a little here, but i figure that it
      // really doesn't matter if the scrollbar is vertical or horizontal
      // and the width metric is a really good metric for every piece
      // of the scrollbar.

      nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
      if (!scrollbarFrame) return NS_ERROR_FAILURE;

      int32_t themeMetric = (scrollbarFrame->StyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL) ?
                            kThemeMetricSmallScrollBarWidth :
                            kThemeMetricScrollBarWidth;
      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(themeMetric, &scrollbarWidth);
      aResult->SizeTo(scrollbarWidth, scrollbarWidth);
      break;
    }

    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    {
      nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
      if (!scrollbarFrame) return NS_ERROR_FAILURE;

      // Since there is no NS_THEME_SCROLLBAR_BUTTON_UP_SMALL we need to ask the parent what appearance style it has.
      int32_t themeMetric = (scrollbarFrame->StyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL) ?
                            kThemeMetricSmallScrollBarWidth :
                            kThemeMetricScrollBarWidth;
      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(themeMetric, &scrollbarWidth);

      // It seems that for both sizes of scrollbar, the buttons are one pixel "longer".
      if (aWidgetType == NS_THEME_SCROLLBAR_BUTTON_LEFT || aWidgetType == NS_THEME_SCROLLBAR_BUTTON_RIGHT)
        aResult->SizeTo(scrollbarWidth+1, scrollbarWidth);
      else
        aResult->SizeTo(scrollbarWidth, scrollbarWidth+1);
 
      *aIsOverridable = false;
      break;
    }
    case NS_THEME_RESIZER:
    {
      HIThemeGrowBoxDrawInfo drawInfo;
      drawInfo.version = 0;
      drawInfo.state = kThemeStateActive;
      drawInfo.kind = kHIThemeGrowBoxKindNormal;
      drawInfo.direction = kThemeGrowRight | kThemeGrowDown;
      drawInfo.size = kHIThemeGrowBoxSizeNormal;
      HIPoint pnt = { 0, 0 };
      HIRect bounds;
      HIThemeGetGrowBoxBounds(&pnt, &drawInfo, &bounds);
      aResult->SizeTo(bounds.size.width, bounds.size.height);
      *aIsOverridable = false;
    }
  }

  if (IsHiDPIContext(aContext->DeviceContext())) {
    *aResult = *aResult * 2;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsNativeThemeCocoa::WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType, 
                                     nsIAtom* aAttribute, bool* aShouldRepaint)
{
  // Some widget types just never change state.
  switch (aWidgetType) {
    case NS_THEME_TOOLBOX:
    case NS_THEME_TOOLBAR:
    case NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_TAB_PANELS:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_DIALOG:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_GROUPBOX:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_METERBAR:
    case NS_THEME_METERBAR_CHUNK:
      *aShouldRepaint = false;
      return NS_OK;
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
    if (aAttribute == nsGkAtoms::disabled ||
        aAttribute == nsGkAtoms::checked ||
        aAttribute == nsGkAtoms::selected ||
        aAttribute == nsGkAtoms::menuactive ||
        aAttribute == nsGkAtoms::sortDirection ||
        aAttribute == nsGkAtoms::focused ||
        aAttribute == nsGkAtoms::_default ||
        aAttribute == nsGkAtoms::open ||
        aAttribute == nsGkAtoms::hover)
      *aShouldRepaint = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeCocoa::ThemeChanged()
{
  // This is unimplemented because we don't care if gecko changes its theme
  // and Mac OS X doesn't have themes.
  return NS_OK;
}

bool 
nsNativeThemeCocoa::ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame,
                                      uint8_t aWidgetType)
{
  // We don't have CSS set up to render non-native scrollbars on Mac OS X so we
  // render natively even if native theme support is disabled.
  if (aWidgetType != NS_THEME_SCROLLBAR &&
      aPresContext && !aPresContext->PresShell()->IsThemeSupportEnabled())
    return false;

  // if this is a dropdown button in a combobox the answer is always no
  if (aWidgetType == NS_THEME_DROPDOWN_BUTTON) {
    nsIFrame* parentFrame = aFrame->GetParent();
    if (parentFrame && (parentFrame->GetType() == nsGkAtoms::comboboxControlFrame))
      return false;
  }

  switch (aWidgetType) {
    case NS_THEME_LISTBOX:

    case NS_THEME_DIALOG:
    case NS_THEME_WINDOW:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUITEM:
    case NS_THEME_MENUSEPARATOR:
    case NS_THEME_TOOLTIP:
    
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_CONTAINER:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_CONTAINER:
    case NS_THEME_GROUPBOX:
    case NS_THEME_BUTTON:
    case NS_THEME_BUTTON_BEVEL:
    case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_SPINNER:
    case NS_THEME_TOOLBAR:
    case NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR:
    case NS_THEME_STATUSBAR:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_SEARCHFIELD:
    //case NS_THEME_TOOLBOX:
    //case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_METERBAR:
    case NS_THEME_METERBAR_CHUNK:
    case NS_THEME_TOOLBAR_SEPARATOR:
    
    case NS_THEME_TAB_PANELS:
    case NS_THEME_TAB:
    
    case NS_THEME_TREEVIEW_TWISTY:
    case NS_THEME_TREEVIEW_TWISTY_OPEN:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREEVIEW_HEADER:
    case NS_THEME_TREEVIEW_HEADER_CELL:
    case NS_THEME_TREEVIEW_HEADER_SORTARROW:
    case NS_THEME_TREEVIEW_TREEITEM:
    case NS_THEME_TREEVIEW_LINE:

    case NS_THEME_RANGE:

    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:

    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_NON_DISAPPEARING:

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_DROPDOWN_TEXT:
    case NS_THEME_DROPDOWN_TEXTFIELD:
      return !IsWidgetStyled(aPresContext, aFrame, aWidgetType);
      break;

    case NS_THEME_RESIZER:
    {
      nsIFrame* parentFrame = aFrame->GetParent();
      if (!parentFrame || parentFrame->GetType() != nsGkAtoms::scrollFrame)
        return true;

      // Note that IsWidgetStyled is not called for resizers on Mac. This is
      // because for scrollable containers, the native resizer looks better
      // when (non-overlay) scrollbars are present even when the style is
      // overriden, and the custom transparent resizer looks better when
      // scrollbars are not present.
      nsIScrollableFrame* scrollFrame = do_QueryFrame(parentFrame);
      return (nsLookAndFeel::GetInt(
                nsLookAndFeel::eIntID_UseOverlayScrollbars) == 0 &&
              scrollFrame && scrollFrame->GetScrollbarVisibility());
      break;
    }
  }

  return false;
}

bool
nsNativeThemeCocoa::WidgetIsContainer(uint8_t aWidgetType)
{
  // flesh this out at some point
  switch (aWidgetType) {
   case NS_THEME_DROPDOWN_BUTTON:
   case NS_THEME_RADIO:
   case NS_THEME_CHECKBOX:
   case NS_THEME_PROGRESSBAR:
   case NS_THEME_METERBAR:
   case NS_THEME_RANGE:
    return false;
    break;
  }
  return true;
}

bool
nsNativeThemeCocoa::ThemeDrawsFocusForWidget(uint8_t aWidgetType)
{
  if (aWidgetType == NS_THEME_DROPDOWN ||
      aWidgetType == NS_THEME_DROPDOWN_TEXTFIELD ||
      aWidgetType == NS_THEME_BUTTON ||
      aWidgetType == NS_THEME_RADIO ||
      aWidgetType == NS_THEME_RANGE ||
      aWidgetType == NS_THEME_CHECKBOX)
    return true;

  return false;
}

bool
nsNativeThemeCocoa::ThemeNeedsComboboxDropmarker()
{
  return false;
}

nsITheme::Transparency
nsNativeThemeCocoa::GetWidgetTransparency(nsIFrame* aFrame, uint8_t aWidgetType)
{
  switch (aWidgetType) {
  case NS_THEME_MENUPOPUP:
  case NS_THEME_TOOLTIP:
    return eTransparent;

  case NS_THEME_SCROLLBAR_SMALL:
  case NS_THEME_SCROLLBAR:
    return nsLookAndFeel::GetInt(
             nsLookAndFeel::eIntID_UseOverlayScrollbars) != 0 ?
           eTransparent : eOpaque;

  case NS_THEME_STATUSBAR:
    // Knowing that scrollbars and statusbars are opaque improves
    // performance, because we create layers for them.
    return eOpaque;

  default:
    return eUnknownTransparency;
  }
}
