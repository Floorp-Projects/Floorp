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
#include "nsNameSpaceManager.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsCocoaFeatures.h"
#include "nsCocoaWindow.h"
#include "nsNativeThemeColors.h"
#include "nsIScrollableFrame.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLMeterElement.h"
#include "nsLookAndFeel.h"
#include "VibrancyManager.h"

#include "gfxContext.h"
#include "gfxQuartzSurface.h"
#include "gfxQuartzNativeDrawing.h"
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
  void CUIDraw(CUIRendererRef r, CGRect rect, CGContextRef ctx, CFDictionaryRef options, CFDictionaryRef* result);
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

// These two classes don't actually add any behavior over NSButtonCell. Their
// purpose is to make it easy to distinguish NSCell objects that are used for
// drawing radio buttons / checkboxes from other cell types.
// The class names are made up, there are no classes with these names in AppKit.
// The reason we need them is that calling [cell setButtonType:NSRadioButton]
// doesn't leave an easy-to-check "marker" on the cell object - there is no
// -[NSButtonCell buttonType] method.
@interface RadioButtonCell : NSButtonCell
@end;
@implementation RadioButtonCell @end;
@interface CheckboxCell : NSButtonCell
@end;
@implementation CheckboxCell @end;

static void
DrawFocusRingForCellIfNeeded(NSCell* aCell, NSRect aWithFrame, NSView* aInView)
{
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

static bool
FocusIsDrawnByDrawWithFrame(NSCell* aCell)
{
#if defined(MAC_OS_X_VERSION_10_8) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_8
  // When building with the 10.8 SDK or higher, focus rings don't draw as part
  // of -[NSCell drawWithFrame:inView:] and must be drawn by a separate call
  // to -[NSCell drawFocusRingMaskWithFrame:inView:]; .
  // See the NSButtonCell section under
  // https://developer.apple.com/library/mac/releasenotes/AppKit/RN-AppKitOlderNotes/#X10_8Notes
  return false;
#else
  if (!nsCocoaFeatures::OnYosemiteOrLater()) {
    // When building with the 10.7 SDK or lower, focus rings always draw as
    // part of -[NSCell drawWithFrame:inView:] if the build is run on 10.9 or
    // lower.
    return true;
  }

  // On 10.10, whether the focus ring is drawn as part of
  // -[NSCell drawWithFrame:inView:] depends on the cell type.
  // Radio buttons and checkboxes draw their own focus rings, other cell
  // types need -[NSCell drawFocusRingMaskWithFrame:inView:].
  return [aCell isKindOfClass:[RadioButtonCell class]] ||
         [aCell isKindOfClass:[CheckboxCell class]];
#endif
}

static void
DrawCellIncludingFocusRing(NSCell* aCell, NSRect aWithFrame, NSView* aInView)
{
  [aCell drawWithFrame:aWithFrame inView:aInView];

  if (!FocusIsDrawnByDrawWithFrame(aCell)) {
    DrawFocusRingForCellIfNeeded(aCell, aWithFrame, aInView);
  }
}

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

  if (content->IsAnyOfXULElements(nsGkAtoms::toolbar,
                                  nsGkAtoms::toolbox,
                                  nsGkAtoms::statusbar))
    return YES;

  switch (aFrame->StyleDisplay()->mAppearance) {
    case NS_THEME_TOOLBAR:
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

@interface SearchFieldCellWithFocusRing : ContextAwareSearchFieldCell {} @end

@implementation SearchFieldCellWithFocusRing

- (void)drawWithFrame:(NSRect)rect inView:(NSView*)controlView
{
  [super drawWithFrame:rect inView:controlView];

  if (FocusIsDrawnByDrawWithFrame(self)) {
    // For some reason, -[NSSearchFieldCell drawWithFrame:inView] doesn't draw a
    // focus ring in 64 bit mode, no matter what SDK is used or what OS X version
    // we're running on. But if FocusIsDrawnByDrawWithFrame(self), then our
    // caller expects us to draw a focus ring. So we just do that here.
    DrawFocusRingForCellIfNeeded(self, rect, controlView);
  }
}

- (void)drawFocusRingMaskWithFrame:(NSRect)rect inView:(NSView*)controlView
{
  // By default this draws nothing. I don't know why.
  // We just draw the search field again. It's a great mask shape for its own
  // focus ring.
  [super drawWithFrame:rect inView:controlView];
}

@end

#endif

#define HITHEME_ORIENTATION kHIThemeOrientationNormal

static CGFloat kMaxFocusRingWidth = 0; // initialized by the nsNativeThemeCocoa constructor

// These enums are for indexing into the margin array.
enum {
  leopardOSorlater = 0, // 10.6 - 10.9
  yosemiteOSorlater = 1 // 10.10+
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

static size_t EnumSizeForCocoaSize(NSControlSize cocoaControlSize) {
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

  static int osIndex = nsCocoaFeatures::OnYosemiteOrLater() ?
                         yosemiteOSorlater : leopardOSorlater;
  size_t controlSize = EnumSizeForCocoaSize(cocoaControlSize);
  const float* buttonMargins = marginSet[osIndex][controlSize];
  rect->origin.x -= buttonMargins[leftMargin];
  rect->origin.y -= buttonMargins[bottomMargin];
  rect->size.width += buttonMargins[leftMargin] + buttonMargins[rightMargin];
  rect->size.height += buttonMargins[bottomMargin] + buttonMargins[topMargin];
}

static ChildView* ChildViewForFrame(nsIFrame* aFrame)
{
  if (!aFrame)
    return nil;

  nsIWidget* widget = aFrame->GetNearestWidget();
  if (!widget)
    return nil;

  NSView* view = (NSView*)widget->GetNativeData(NS_NATIVE_WIDGET);
  return [view isKindOfClass:[ChildView class]] ? (ChildView*)view : nil;
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

static NSSize
WindowButtonsSize(nsIFrame* aFrame)
{
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

static BOOL FrameIsInActiveWindow(nsIFrame* aFrame)
{
  nsIWidget* topLevelWidget = NULL;
  NSWindow* win = NativeWindowForFrame(aFrame, &topLevelWidget);
  if (!topLevelWidget || !win)
    return YES;

  // XUL popups, e.g. the toolbar customization popup, can't become key windows,
  // but controls in these windows should still get the active look.
  if (topLevelWidget->WindowType() == eWindowType_popup)
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

static bool IsInSourceList(nsIFrame* aFrame) {
  for (nsIFrame* frame = aFrame->GetParent(); frame; frame = frame->GetParent()) {
    if (frame->StyleDisplay()->mAppearance == NS_THEME_MAC_SOURCE_LIST) {
      return true;
    }
  }
  return false;
}

NS_IMPL_ISUPPORTS_INHERITED(nsNativeThemeCocoa, nsNativeTheme, nsITheme)

nsNativeThemeCocoa::nsNativeThemeCocoa()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  kMaxFocusRingWidth = nsCocoaFeatures::OnYosemiteOrLater() ? 7 : 4;

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
  [mDisclosureButtonCell release];
  [mHelpButtonCell release];
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
// DrawCellWithScaling() and DrawButton() and above which we
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

    DrawCellIncludingFocusRing(cell, drawRect, view);

    [NSGraphicsContext setCurrentContext:savedContext];
  }
  else {
    float w = ceil(drawRect.size.width);
    float h = ceil(drawRect.size.height);
    NSRect tmpRect = NSMakeRect(kMaxFocusRingWidth, kMaxFocusRingWidth, w, h);

    // inflate to figure out the frame we need to tell NSCell to draw in, to get something that's 0,0,w,h
    InflateControlRect(&tmpRect, controlSize, marginSet);

    // and then, expand by kMaxFocusRingWidth size to make sure we can capture any focus ring
    w += kMaxFocusRingWidth * 2.0;
    h += kMaxFocusRingWidth * 2.0;

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
  size_t sizeIndex = 0;

  // At some sizes, don't scale but snap.
  const NSControlSize smallerControlSize =
    EnumSizeForCocoaSize(controlSizeX) < EnumSizeForCocoaSize(controlSizeY) ?
    controlSizeX : controlSizeY;
  const size_t smallerControlSizeIndex = EnumSizeForCocoaSize(smallerControlSize);
  const NSSize size = sizes[smallerControlSizeIndex];
  float diffWidth = size.width ? rectWidth - size.width : 0.0f;
  float diffHeight = size.height ? rectHeight - size.height : 0.0f;
  if (diffWidth >= 0.0f && diffHeight >= 0.0f &&
      diffWidth <= snapTolerance && diffHeight <= snapTolerance) {
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

  MOZ_ASSERT(sizeIndex < ArrayLength(settings.minimumSizes));
  const NSSize minimumSize = settings.minimumSizes[sizeIndex];
  DrawCellWithScaling(cell, cgContext, drawRect, controlSize, sizes[sizeIndex],
                      minimumSize, settings.margins, view, mirrorHorizontal);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@interface NSWindow(CoreUIRendererPrivate)
+ (CUIRendererRef)coreUIRenderer;
@end

static id
GetAquaAppearance()
{
  // We only need NSAppearance on 10.10 and up.
  if (nsCocoaFeatures::OnYosemiteOrLater()) {
    Class NSAppearanceClass = NSClassFromString(@"NSAppearance");
    if (NSAppearanceClass &&
        [NSAppearanceClass respondsToSelector:@selector(appearanceNamed:)]) {
      return [NSAppearanceClass performSelector:@selector(appearanceNamed:)
                                     withObject:@"NSAppearanceNameAqua"];
    }
  }
  return nil;
}

@interface NSObject(NSAppearanceCoreUIRendering)
- (void)_drawInRect:(CGRect)rect context:(CGContextRef)cgContext options:(id)options;
@end

static void
RenderWithCoreUI(CGRect aRect, CGContextRef cgContext, NSDictionary* aOptions, bool aSkipAreaCheck = false)
{
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
    CUIRendererRef renderer = [NSWindow respondsToSelector:@selector(coreUIRenderer)]
      ? [NSWindow coreUIRenderer] : nil;
    CUIDraw(renderer, aRect, cgContext, (CFDictionaryRef)aOptions, NULL);
  }
}

static float VerticalAlignFactor(nsIFrame *aFrame)
{
  if (!aFrame)
    return 0.5f; // default: center

  const nsStyleCoord& va = aFrame->StyleDisplay()->mVerticalAlign;
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
    },
    { // Yosemite
      {0, 0, 0, 0},     // mini
      {1, 1, 1, 2},     // small
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
    },
    { // Yosemite
      {0, 1, 0, 0},     // mini
      {0, 1, 0, 1},     // small
      {0, 1, 0, 1}      // regular
    }
  }
};

void
nsNativeThemeCocoa::DrawCheckboxOrRadio(CGContextRef cgContext, bool inCheckbox,
                                        const HIRect& inBoxRect, bool inSelected,
                                        EventStates inState, nsIFrame* aFrame)
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
    },
    { // Yosemite
      {0, 0, 0, 0},     // mini
      {0, 0, 0, 0},     // small
      {0, 0, 0, 0}      // regular
    }
  }
};

void
nsNativeThemeCocoa::DrawSearchField(CGContextRef cgContext, const HIRect& inBoxRect,
                                    nsIFrame* aFrame, EventStates inState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  ContextAwareSearchFieldCell* cell = mSearchFieldCell;
  [cell setContext:aFrame];
  [cell setEnabled:!IsDisabled(aFrame, inState)];
  // NOTE: this could probably use inState
  [cell setShowsFirstResponder:IsFocused(aFrame)];

  // When using the 10.11 SDK, the default string will be shown if we don't
  // set the placeholder string.
  [cell setPlaceholderString:@""];

  DrawCellWithSnapping(cell, cgContext, inBoxRect, searchFieldSettings,
                       VerticalAlignFactor(aFrame), mCellDrawView,
                       IsFrameRTL(aFrame));

  [cell setContext:nullptr];

  NS_OBJC_END_TRY_ABORT_BLOCK;
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

void
nsNativeThemeCocoa::DrawMenuIcon(CGContextRef cgContext, const CGRect& aRect,
                                 EventStates inState, nsIFrame* aFrame,
                                 const NSSize& aIconSize, NSString* aImageName,
                                 bool aCenterHorizontally)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Adjust size and position of our drawRect.
  CGFloat paddingX = std::max(CGFloat(0.0), aRect.size.width - aIconSize.width);
  CGFloat paddingY = std::max(CGFloat(0.0), aRect.size.height - aIconSize.height);
  CGFloat paddingStartX = std::min(paddingX, kMenuIconIndent);
  CGFloat paddingEndX = std::max(CGFloat(0.0), paddingX - kMenuIconIndent);
  CGRect drawRect = CGRectMake(
    aRect.origin.x + (aCenterHorizontally ? ceil(paddingX / 2) :
                      IsFrameRTL(aFrame) ? paddingEndX : paddingStartX),
    aRect.origin.y + ceil(paddingY / 2),
    aIconSize.width, aIconSize.height);

  NSString* state = IsDisabled(aFrame, inState) ? @"disabled" :
    (CheckBooleanAttr(aFrame, nsGkAtoms::menuactive) ? @"pressed" : @"normal");

  NSString* imageName = aImageName;
  if (!nsCocoaFeatures::OnElCapitanOrLater()) {
    // Pre-10.11, image names are prefixed with "image."
    imageName = [@"image." stringByAppendingString:aImageName];
  }

  RenderWithCoreUI(drawRect, cgContext,
          [NSDictionary dictionaryWithObjectsAndKeys:
            @"kCUIBackgroundTypeMenu", @"backgroundTypeKey",
            imageName, @"imageNameKey",
            state, @"state",
            @"image", @"widget",
            [NSNumber numberWithBool:YES], @"is.flipped",
            nil]);

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, drawRect);
#endif

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static const NSSize kHelpButtonSize = NSMakeSize(20, 20);
static const NSSize kDisclosureButtonSize = NSMakeSize(21, 21);

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
    },
    { // Yosemite
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
                                   EventStates inState, uint8_t aWidgetType,
                                   nsIFrame* aFrame, float aOriginalHeight)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  BOOL isActive = FrameIsInActiveWindow(aFrame);
  BOOL isDisabled = IsDisabled(aFrame, inState);

  NSButtonCell* cell = (aWidgetType == NS_THEME_BUTTON) ? mPushButtonCell :
    (aWidgetType == NS_THEME_MAC_HELP_BUTTON) ? mHelpButtonCell : mDisclosureButtonCell;
  [cell setEnabled:!isDisabled];
  [cell setHighlighted:isActive &&
                       inState.HasAllStates(NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_HOVER)];
  [cell setShowsFirstResponder:inState.HasState(NS_EVENT_STATE_FOCUS) && !isDisabled && isActive];

  if (aWidgetType != NS_THEME_BUTTON) { // Help button or disclosure button.
    NSSize buttonSize = NSMakeSize(0, 0);
    if (aWidgetType == NS_THEME_MAC_HELP_BUTTON) {
      buttonSize = kHelpButtonSize;
    } else { // Disclosure button.
      buttonSize = kDisclosureButtonSize;
      [cell setState:(aWidgetType == NS_THEME_MAC_DISCLOSURE_BUTTON_CLOSED) ? NSOffState : NSOnState];
    }

    DrawCellWithScaling(cell, cgContext, inBoxRect, NSRegularControlSize,
                        NSZeroSize, buttonSize, NULL, mCellDrawView,
                        false); // Don't mirror icon in RTL.
  } else {
    // If the button is tall enough, draw the square button style so that
    // buttons with non-standard content look good. Otherwise draw normal
    // rounded aqua buttons.
    // This comparison is done based on the height that is calculated without
    // the top, because the snapped height can be affected by the top of the
    // rect and that may result in different height depending on the top value.
    if (aOriginalHeight > DO_SQUARE_BUTTON_HEIGHT) {
      [cell setBezelStyle:NSShadowlessSquareBezelStyle];
      DrawCellWithScaling(cell, cgContext, inBoxRect, NSRegularControlSize,
                          NSZeroSize, NSMakeSize(14, 0), NULL, mCellDrawView,
                          IsFrameRTL(aFrame));
    } else {
      [cell setBezelStyle:NSRoundedBezelStyle];
      DrawCellWithSnapping(cell, cgContext, inBoxRect, pushButtonSettings, 0.5f,
                           mCellDrawView, IsFrameRTL(aFrame), 1.0f);
    }
  }

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsNativeThemeCocoa::DrawFocusOutline(CGContextRef cgContext, const HIRect& inBoxRect,
                                     EventStates inState, uint8_t aWidgetType,
                                     nsIFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeFrameDrawInfo fdi;
  fdi.version = 0;
  fdi.kind = kHIThemeFrameTextFieldSquare;
  fdi.state = kThemeStateActive;
  fdi.isFocused = TRUE;

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  HIThemeDrawFrame(&inBoxRect, &fdi, cgContext, HITHEME_ORIENTATION);

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
    int w = ceil(drawRect.size.width) + 2 * kMaxFocusRingWidth;
    int h = ceil(drawRect.size.height) + 2 * kMaxFocusRingWidth;

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
    CGContextTranslateCTM(bitmapctx, kMaxFocusRingWidth, kMaxFocusRingWidth);

    // Set the context's "base transform" to in order to get correctly-sized focus rings.
    CGContextSetBaseCTM(bitmapctx, CGAffineTransformMakeScale(backingScaleFactor, backingScaleFactor));

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
                               EventStates inState, nsIFrame* aFrame)
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

  if (inIsDefault && !isDisabled &&
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
    },
    { // Yosemite
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
    },
    { // Yosemite
      {0, 0, 2, 2},    // mini
      {0, 0, 3, 2},    // small
      {0, 1, 3, 3}     // regular
    }
  }
};

void
nsNativeThemeCocoa::DrawDropdown(CGContextRef cgContext, const HIRect& inBoxRect,
                                 EventStates inState, uint8_t aWidgetType,
                                 nsIFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mDropdownCell setPullsDown:(aWidgetType == NS_THEME_BUTTON)];

  BOOL isEditable = (aWidgetType == NS_THEME_MENULIST_TEXTFIELD);
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

static const CellRenderSettings spinnerSettings = {
  {
    NSMakeSize(11, 16), // mini (width trimmed by 2px to reduce blank border)
    NSMakeSize(15, 22), // small
    NSMakeSize(19, 27)  // regular
  },
  {
    NSMakeSize(11, 16), // mini (width trimmed by 2px to reduce blank border)
    NSMakeSize(15, 22), // small
    NSMakeSize(19, 27)  // regular
  },
  {
    { // Leopard
      {0, 0, 0, 0},    // mini
      {0, 0, 0, 0},    // small
      {0, 0, 0, 0}     // regular
    },
    { // Yosemite
      {0, 0, 0, 0},    // mini
      {0, 0, 0, 0},    // small
      {0, 0, 0, 0}     // regular
    }
  }
};

void
nsNativeThemeCocoa::DrawSpinButtons(CGContextRef cgContext, ThemeButtonKind inKind,
                                    const HIRect& inBoxRect, ThemeDrawState inDrawState,
                                    ThemeButtonAdornment inAdornment,
                                    EventStates inState, nsIFrame* aFrame)
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
nsNativeThemeCocoa::DrawSpinButton(CGContextRef cgContext,
                                   ThemeButtonKind inKind,
                                   const HIRect& inBoxRect,
                                   ThemeDrawState inDrawState,
                                   ThemeButtonAdornment inAdornment,
                                   EventStates inState,
                                   nsIFrame* aFrame,
                                   uint8_t aWidgetType)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_ASSERT(aWidgetType == NS_THEME_SPINNER_UPBUTTON ||
             aWidgetType == NS_THEME_SPINNER_DOWNBUTTON);

  HIThemeButtonDrawInfo bdi;
  bdi.version = 0;
  bdi.kind = inKind;
  bdi.value = kThemeButtonOff;
  bdi.adornment = inAdornment;

  if (IsDisabled(aFrame, inState))
    bdi.state = kThemeStateUnavailable;
  else
    bdi.state = FrameIsInActiveWindow(aFrame) ? inDrawState : kThemeStateActive;

  // Cocoa only allows kThemeIncDecButton to paint the up and down spin buttons
  // together as a single unit (presumably because when one button is active,
  // the appearance of both changes (in different ways)). Here we have to paint
  // both buttons, using clip to hide the one we don't want to paint.
  HIRect drawRect = inBoxRect;
  drawRect.size.height *= 2;
  if (aWidgetType == NS_THEME_SPINNER_DOWNBUTTON) {
    drawRect.origin.y -= inBoxRect.size.height;
  }

  // Shift the drawing a little to the left, since cocoa paints with more
  // blank space around the visual buttons than we'd like:
  drawRect.origin.x -= 1;

  CGContextSaveGState(cgContext);
  CGContextClipToRect(cgContext, inBoxRect);

  HIThemeDrawButton(&drawRect, &bdi, cgContext, HITHEME_ORIENTATION, NULL);

  CGContextRestoreGState(cgContext);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsNativeThemeCocoa::DrawFrame(CGContextRef cgContext, HIThemeFrameKind inKind,
                              const HIRect& inBoxRect, bool inDisabled,
                              EventStates inState)
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
        },
        { // Yosemite
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
        },
        { // Yosemite
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
        },
        { // Yosemite
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
    },
    { // Yosemite
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

  // When using -moz-meterbar on an non meter element, we will not be able to
  // get all the needed information so we just draw an empty meter.
  nsIContent* content = aFrame->GetContent();
  if (!(content && content->IsHTMLElement(nsGkAtoms::meter))) {
    DrawCellWithSnapping(mMeterBarCell, cgContext, inBoxRect,
                         meterSetting, VerticalAlignFactor(aFrame),
                         mCellDrawView, IsFrameRTL(aFrame));
    return;
  }

  HTMLMeterElement* meterElement = static_cast<HTMLMeterElement*>(content);
  double value = meterElement->Value();
  double min = meterElement->Min();
  double max = meterElement->Max();

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
  EventStates states = aFrame->GetContent()->AsElement()->State();

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
                              EventStates inState, bool inIsVertical,
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
                                EventStates inState, nsIFrame* aFrame,
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
  BOOL drawLeftSeparator = SeparatorResponsibility(left, aFrame) == aFrame;
  BOOL drawRightSeparator = SeparatorResponsibility(aFrame, right) == aFrame;
  NSControlSize controlSize = FindControlSize(drawRect.size.height, aSettings.heights, 4.0f);

  RenderWithCoreUI(drawRect, cgContext, [NSDictionary dictionaryWithObjectsAndKeys:
            aSettings.widgetName, @"widget",
            (isActive ? @"kCUIPresentationStateActiveKey" : @"kCUIPresentationStateInactive"), @"kCUIPresentationStateKey",
            ToolbarButtonPosition(!left, !right), @"kCUIPositionKey",
            [NSNumber numberWithBool:drawLeftSeparator], @"kCUISegmentLeadingSeparatorKey",
            [NSNumber numberWithBool:drawRightSeparator], @"kCUISegmentTrailingSeparatorKey",
            [NSNumber numberWithBool:isSelected], @"value",
            (isPressed ? @"pressed" : (isActive ? @"normal" : @"inactive")), @"state",
            [NSNumber numberWithBool:isFocused], @"focus",
            CUIControlSizeForCocoaSize(controlSize), @"size",
            [NSNumber numberWithBool:YES], @"is.flipped",
            @"up", @"direction",
            nil]);
}

void
nsNativeThemeCocoa::GetScrollbarPressStates(nsIFrame* aFrame,
                                            EventStates aButtonStates[])
{
  static nsIContent::AttrValuesArray attributeValues[] = {
    &nsGkAtoms::scrollbarUpTop,
    &nsGkAtoms::scrollbarDownTop,
    &nsGkAtoms::scrollbarUpBottom,
    &nsGkAtoms::scrollbarDownBottom,
    nullptr
  };

  // Get the state of any scrollbar buttons in our child frames
  for (nsIFrame *childFrame : aFrame->PrincipalChildList()) {
    nsIContent *childContent = childFrame->GetContent();
    if (!childContent) continue;
    int32_t attrIndex = childContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::sbattr,
                                                      attributeValues, eCaseMatters);
    if (attrIndex < 0) continue;

    aButtonStates[attrIndex] = GetContentState(childFrame, NS_THEME_BUTTON);
  }
}

nsIFrame*
nsNativeThemeCocoa::GetParentScrollbarFrame(nsIFrame *aFrame)
{
  // Walk our parents to find a scrollbar frame
  nsIFrame* scrollbarFrame = aFrame;
  do {
    if (scrollbarFrame->IsScrollbarFrame()) break;
  } while ((scrollbarFrame = scrollbarFrame->GetParent()));

  // We return null if we can't find a parent scrollbar frame
  return scrollbarFrame;
}

static bool
ToolbarCanBeUnified(CGContextRef cgContext, const HIRect& inBoxRect, NSWindow* aWindow)
{
  if (![aWindow isKindOfClass:[ToolbarWindow class]])
    return false;

  ToolbarWindow* win = (ToolbarWindow*)aWindow;
  float unifiedToolbarHeight = [win unifiedToolbarHeight];
  return inBoxRect.origin.x == 0 &&
         inBoxRect.size.width >= [win frame].size.width &&
         CGRectGetMaxY(inBoxRect) <= unifiedToolbarHeight;
}

// By default, kCUIWidgetWindowFrame drawing draws rounded corners in the
// upper corners. Depending on the context type, it fills the background in
// the corners with black or leaves it transparent. Unfortunately, this corner
// rounding interacts poorly with the window corner masking we apply during
// titlebar drawing and results in small remnants of the corner background
// appearing at the rounded edge.
// So we draw square corners.
static void
DrawNativeTitlebarToolbarWithSquareCorners(CGContextRef aContext, const CGRect& aRect,
                                           CGFloat aUnifiedHeight, BOOL aIsMain, BOOL aIsFlipped)
{
  // We extend the draw rect horizontally and clip away the rounded corners.
  const CGFloat extendHorizontal = 10;
  CGRect drawRect = CGRectInset(aRect, -extendHorizontal, 0);
  CGContextSaveGState(aContext);
  CGContextClipToRect(aContext, aRect);

  RenderWithCoreUI(drawRect, aContext,
          [NSDictionary dictionaryWithObjectsAndKeys:
            @"kCUIWidgetWindowFrame", @"widget",
            @"regularwin", @"windowtype",
            (aIsMain ? @"normal" : @"inactive"), @"state",
            [NSNumber numberWithDouble:aUnifiedHeight], @"kCUIWindowFrameUnifiedTitleBarHeightKey",
            [NSNumber numberWithBool:YES], @"kCUIWindowFrameDrawTitleSeparatorKey",
            [NSNumber numberWithBool:aIsFlipped], @"is.flipped",
            nil]);

  CGContextRestoreGState(aContext);
}

void
nsNativeThemeCocoa::DrawUnifiedToolbar(CGContextRef cgContext, const HIRect& inBoxRect,
                                       NSWindow* aWindow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  CGContextSaveGState(cgContext);
  CGContextClipToRect(cgContext, inBoxRect);

  CGFloat unifiedHeight = std::max([(ToolbarWindow*)aWindow unifiedToolbarHeight],
                                   inBoxRect.size.height);
  BOOL isMain = [aWindow isMainWindow];
  CGFloat titlebarHeight = unifiedHeight - inBoxRect.size.height;
  CGRect drawRect = CGRectMake(inBoxRect.origin.x, inBoxRect.origin.y - titlebarHeight,
                               inBoxRect.size.width, inBoxRect.size.height + titlebarHeight);
  DrawNativeTitlebarToolbarWithSquareCorners(cgContext, drawRect, unifiedHeight, isMain, YES);

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
  RenderWithCoreUI(drawRect, cgContext,
          [NSDictionary dictionaryWithObjectsAndKeys:
            @"kCUIWidgetWindowFrame", @"widget",
            @"regularwin", @"windowtype",
            (IsActive(aFrame, YES) ? @"normal" : @"inactive"), @"state",
            [NSNumber numberWithInt:inBoxRect.size.height], @"kCUIWindowFrameBottomBarHeightKey",
            [NSNumber numberWithBool:YES], @"kCUIWindowFrameDrawBottomBarSeparatorKey",
            [NSNumber numberWithBool:YES], @"is.flipped",
            nil]);

  CGContextRestoreGState(cgContext);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsNativeThemeCocoa::DrawNativeTitlebar(CGContextRef aContext, CGRect aTitlebarRect,
                                       CGFloat aUnifiedHeight, BOOL aIsMain, BOOL aIsFlipped)
{
  CGFloat unifiedHeight = std::max(aUnifiedHeight, aTitlebarRect.size.height);
  DrawNativeTitlebarToolbarWithSquareCorners(aContext, aTitlebarRect, unifiedHeight, aIsMain, aIsFlipped);
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

static void
DrawVibrancyBackground(CGContextRef cgContext, CGRect inBoxRect,
                       nsIFrame* aFrame, nsITheme::ThemeGeometryType aThemeGeometryType,
                       int aCornerRadiusIfOpaque = 0)
{
  ChildView* childView = ChildViewForFrame(aFrame);
  if (childView) {
    NSRect rect = NSRectFromCGRect(inBoxRect);
    NSGraphicsContext* savedContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:YES]];
    [NSGraphicsContext saveGraphicsState];

    NSColor* fillColor = [childView vibrancyFillColorForThemeGeometryType:aThemeGeometryType];
    if ([fillColor alphaComponent] == 1.0 && aCornerRadiusIfOpaque > 0) {
      // The fillColor being opaque means that the system-wide pref "reduce
      // transparency" is set. In that scenario, we still go through all the
      // vibrancy rendering paths (VibrancyManager::SystemSupportsVibrancy()
      // will still return true), but the result just won't look "vibrant".
      // However, there's one unfortunate change of behavior that this pref
      // has: It stops the window server from applying window masks. We use
      // a window mask to get rounded corners on menus. So since the mask
      // doesn't work in "reduce vibrancy" mode, we need to do our own rounded
      // corner clipping here.
      [[NSBezierPath bezierPathWithRoundedRect:rect
                                       xRadius:aCornerRadiusIfOpaque
                                       yRadius:aCornerRadiusIfOpaque] addClip];
    }

    [fillColor set];
    NSRectFill(rect);

    [NSGraphicsContext restoreGraphicsState];
    [NSGraphicsContext setCurrentContext:savedContext];
  }
}

bool
nsNativeThemeCocoa::IsParentScrollbarRolledOver(nsIFrame* aFrame)
{
  nsIFrame* scrollbarFrame = GetParentScrollbarFrame(aFrame);
  return nsLookAndFeel::UseOverlayScrollbars()
    ? CheckBooleanAttr(scrollbarFrame, nsGkAtoms::hover)
    : GetContentState(scrollbarFrame, NS_THEME_NONE).HasState(NS_EVENT_STATE_HOVER);
}

static bool
IsHiDPIContext(nsPresContext* aContext)
{
  return nsPresContext::AppUnitsPerCSSPixel() >=
    2 * aContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom();
}

NS_IMETHODIMP
nsNativeThemeCocoa::DrawWidgetBackground(nsRenderingContext* aContext,
                                         nsIFrame* aFrame,
                                         uint8_t aWidgetType,
                                         const nsRect& aRect,
                                         const nsRect& aDirtyRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  DrawTarget& aDrawTarget = *aContext->GetDrawTarget();

  // setup to draw into the correct port
  int32_t p2a = aFrame->PresContext()->AppUnitsPerDevPixel();

  gfx::Rect nativeDirtyRect = NSRectToRect(aDirtyRect, p2a);
  gfxRect nativeWidgetRect(aRect.x, aRect.y, aRect.width, aRect.height);
  nativeWidgetRect.ScaleInverse(gfxFloat(p2a));
  float nativeWidgetHeight = round(nativeWidgetRect.Height());
  nativeWidgetRect.Round();
  if (nativeWidgetRect.IsEmpty())
    return NS_OK; // Don't attempt to draw invisible widgets.

  // Bug 1059031 -
  // Quartz theme drawing often adjusts drawing rects, so make
  // sure our surface is big enough for that.
  // For example, DrawCellWithSnapping snaps the drawing rect, and that can
  // result in us drawing outside of what we thought the bounds were.
  // The 5 is just a guess of how much margin we need to handle that.
  nativeDirtyRect.Inflate(5);

  AutoRestoreTransform autoRestoreTransform(&aDrawTarget);

  bool hidpi = IsHiDPIContext(aFrame->PresContext());
  if (hidpi) {
    // Use high-resolution drawing.
    nativeWidgetRect.Scale(0.5f);
    nativeWidgetHeight *= 0.5f;
    nativeDirtyRect.Scale(0.5f);
    aDrawTarget.SetTransform(aDrawTarget.GetTransform().PreScale(2.0f, 2.0f));
  }

  gfxQuartzNativeDrawing nativeDrawing(aDrawTarget, nativeDirtyRect);

  CGContextRef cgContext = nativeDrawing.BeginNativeDrawing();
  if (cgContext == nullptr) {
    // The Quartz surface handles 0x0 surfaces by internally
    // making all operations no-ops; there's no cgcontext created for them.
    // Unfortunately, this means that callers that want to render
    // directly to the CGContext need to be aware of this quirk.
    return NS_OK;
  }

  if (hidpi) {
    // Set the context's "base transform" to in order to get correctly-sized focus rings.
    CGContextSetBaseCTM(cgContext, CGAffineTransformMakeScale(2, 2));
  }

#if 0
  if (1 /*aWidgetType == NS_THEME_TEXTFIELD*/) {
    fprintf(stderr, "Native theme drawing widget %d [%p] dis:%d in rect [%d %d %d %d]\n",
            aWidgetType, aFrame, IsDisabled(aFrame), aRect.x, aRect.y, aRect.width, aRect.height);
    fprintf(stderr, "Cairo matrix: [%f %f %f %f %f %f]\n",
            mat._11, mat._12, mat._21, mat._22, mat._31, mat._32);
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

  EventStates eventState = GetContentState(aFrame, aWidgetType);

  switch (aWidgetType) {
    case NS_THEME_DIALOG: {
      if (IsWindowSheet(aFrame)) {
        if (VibrancyManager::SystemSupportsVibrancy()) {
          ThemeGeometryType type = ThemeGeometryTypeForWidget(aFrame, aWidgetType);
          DrawVibrancyBackground(cgContext, macRect, aFrame, type);
        } else {
          HIThemeSetFill(kThemeBrushSheetBackgroundTransparent, NULL, cgContext, HITHEME_ORIENTATION);
          CGContextFillRect(cgContext, macRect);
        }
      } else {
        HIThemeSetFill(kThemeBrushDialogBackgroundActive, NULL, cgContext, HITHEME_ORIENTATION);
        CGContextFillRect(cgContext, macRect);
      }

    }
      break;

    case NS_THEME_MENUPOPUP:
      if (VibrancyManager::SystemSupportsVibrancy()) {
        DrawVibrancyBackground(cgContext, macRect, aFrame, eThemeGeometryTypeMenu, 4);
      } else {
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

    case NS_THEME_MENUARROW: {
      bool isRTL = IsFrameRTL(aFrame);
      DrawMenuIcon(cgContext, macRect, eventState, aFrame, kMenuarrowSize,
                   isRTL ? kMenuarrowLeftImage : kMenuarrowRightImage, true);
    }
      break;

    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM: {
      if (VibrancyManager::SystemSupportsVibrancy()) {
        ThemeGeometryType type = ThemeGeometryTypeForWidget(aFrame, aWidgetType);
        DrawVibrancyBackground(cgContext, macRect, aFrame, type);
      } else {
        bool isDisabled = IsDisabled(aFrame, eventState);
        bool isSelected = !isDisabled && CheckBooleanAttr(aFrame, nsGkAtoms::menuactive);
        // maybe use kThemeMenuItemHierBackground or PopUpBackground instead of just Plain?
        HIThemeMenuItemDrawInfo drawInfo;
        memset(&drawInfo, 0, sizeof(drawInfo));
        drawInfo.version = 0;
        drawInfo.itemType = kThemeMenuItemPlain;
        drawInfo.state = (isDisabled ?
                           static_cast<ThemeMenuState>(kThemeMenuDisabled) :
                           isSelected ?
                             static_cast<ThemeMenuState>(kThemeMenuSelected) :
                             static_cast<ThemeMenuState>(kThemeMenuActive));

        // XXX pass in the menu rect instead of always using the item rect
        HIRect ignored;
        HIThemeDrawMenuItem(&macRect, &macRect, &drawInfo, cgContext, HITHEME_ORIENTATION, &ignored);
      }

      if (aWidgetType == NS_THEME_CHECKMENUITEM) {
        DrawMenuIcon(cgContext, macRect, eventState, aFrame, kCheckmarkSize, kCheckmarkImage, false);
      }
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

    case NS_THEME_BUTTON_ARROW_UP:
    case NS_THEME_BUTTON_ARROW_DOWN:
      DrawMenuIcon(cgContext, macRect, eventState, aFrame, kMenuScrollArrowSize,
                   aWidgetType == NS_THEME_BUTTON_ARROW_UP ?
                   kMenuUpScrollArrowImage : kMenuDownScrollArrowImage, true);
      break;

    case NS_THEME_TOOLTIP:
      if (VibrancyManager::SystemSupportsVibrancy()) {
        DrawVibrancyBackground(cgContext, macRect, aFrame, ThemeGeometryTypeForWidget(aFrame, aWidgetType));
      } else {
        CGContextSetRGBFillColor(cgContext, 0.996, 1.000, 0.792, 0.950);
        CGContextFillRect(cgContext, macRect);
      }
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
        if (!IsDisabled(aFrame, eventState) && isInActiveWindow &&
            !QueueAnimatedContentForRefresh(aFrame->GetContent(), 10)) {
          NS_WARNING("Unable to animate button!");
        }
        DrawButton(cgContext, kThemePushButton, macRect, isInActiveWindow,
                   kThemeButtonOff, kThemeAdornmentNone, eventState, aFrame);
      } else if (IsButtonTypeMenu(aFrame)) {
        DrawDropdown(cgContext, macRect, eventState, aWidgetType, aFrame);
      } else {
        DrawPushButton(cgContext, macRect, eventState, aWidgetType, aFrame,
                       nativeWidgetHeight);
      }
      break;

    case NS_THEME_FOCUS_OUTLINE:
      DrawFocusOutline(cgContext, macRect, eventState, aWidgetType, aFrame);
      break;

    case NS_THEME_MAC_HELP_BUTTON:
    case NS_THEME_MAC_DISCLOSURE_BUTTON_OPEN:
    case NS_THEME_MAC_DISCLOSURE_BUTTON_CLOSED:
      DrawPushButton(cgContext, macRect, eventState, aWidgetType, aFrame,
                     nativeWidgetHeight);
      break;

    case NS_THEME_BUTTON_BEVEL:
      DrawButton(cgContext, kThemeMediumBevelButton, macRect,
                 IsDefaultButton(aFrame), kThemeButtonOff, kThemeAdornmentNone,
                 eventState, aFrame);
      break;

    case NS_THEME_SPINNER: {
      nsIContent* content = aFrame->GetContent();
      if (content->IsHTMLElement()) {
        // In HTML the theming for the spin buttons is drawn individually into
        // their own backgrounds instead of being drawn into the background of
        // their spinner parent as it is for XUL.
        break;
      }
      ThemeDrawState state = kThemeStateActive;
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

    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON: {
      nsNumberControlFrame* numberControlFrame =
        nsNumberControlFrame::GetNumberControlFrameForSpinButton(aFrame);
      if (numberControlFrame) {
        ThemeDrawState state = kThemeStateActive;
        if (numberControlFrame->SpinnerUpButtonIsDepressed()) {
          state = kThemeStatePressedUp;
        } else if (numberControlFrame->SpinnerDownButtonIsDepressed()) {
          state = kThemeStatePressedDown;
        }
        DrawSpinButton(cgContext, kThemeIncDecButtonMini, macRect, state,
                       kThemeAdornmentNone, eventState, aFrame, aWidgetType);
      }
    }
      break;

    case NS_THEME_TOOLBARBUTTON:
      DrawSegment(cgContext, macRect, eventState, aFrame, toolbarButtonRenderSettings);
      break;

    case NS_THEME_SEPARATOR: {
      HIThemeSeparatorDrawInfo sdi = { 0, kThemeStateActive };
      HIThemeDrawSeparator(&macRect, &sdi, cgContext, HITHEME_ORIENTATION);
    }
      break;

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

    case NS_THEME_WINDOW_TITLEBAR: {
      NSWindow* win = NativeWindowForFrame(aFrame);
      BOOL isMain = [win isMainWindow];
      float unifiedToolbarHeight = [win isKindOfClass:[ToolbarWindow class]] ?
        [(ToolbarWindow*)win unifiedToolbarHeight] : macRect.size.height;
      DrawNativeTitlebar(cgContext, macRect, unifiedToolbarHeight, isMain, YES);
    }
      break;

    case NS_THEME_STATUSBAR:
      DrawStatusBar(cgContext, macRect, aFrame);
      break;

    case NS_THEME_MENULIST:
    case NS_THEME_MENULIST_TEXTFIELD:
      DrawDropdown(cgContext, macRect, eventState, aWidgetType, aFrame);
      break;

    case NS_THEME_MENULIST_BUTTON:
      DrawButton(cgContext, kThemeArrowButton, macRect, false, kThemeButtonOn,
                 kThemeAdornmentArrowDownArrow, eventState, aFrame);
      break;

    case NS_THEME_GROUPBOX: {
      HIThemeGroupBoxDrawInfo gdi = { 0, kThemeStateActive, kHIThemeGroupBoxKindPrimary };
      HIThemeDrawGroupBox(&macRect, &gdi, cgContext, HITHEME_ORIENTATION);
      break;
    }

    case NS_THEME_TEXTFIELD:
    case NS_THEME_NUMBER_INPUT:
      // HIThemeSetFill is not available on 10.3
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      CGContextFillRect(cgContext, macRect);

      // XUL textboxes set the native appearance on the containing box, while
      // concrete focus is set on the html:input element within it. We can
      // though, check the focused attribute of xul textboxes in this case.
      // On Mac, focus rings are always shown for textboxes, so we do not need
      // to check the window's focus ring state here
      if (aFrame->GetContent()->IsXULElement() && IsFocused(aFrame)) {
        eventState |= NS_EVENT_STATE_FOCUS;
      }

      DrawFrame(cgContext, kHIThemeFrameTextFieldSquare, macRect,
                IsDisabled(aFrame, eventState) || IsReadOnly(aFrame), eventState);
      break;

    case NS_THEME_SEARCHFIELD:
      DrawSearchField(cgContext, macRect, aFrame, eventState);
      break;

    case NS_THEME_PROGRESSBAR:
    {
      double value = GetProgressValue(aFrame);
      double maxValue = GetProgressMaxValue(aFrame);
      // Don't request repaints for scrollbars at 100% because those don't animate.
      if (value < maxValue) {
        if (!QueueAnimatedContentForRefresh(aFrame->GetContent(), 30)) {
          NS_WARNING("Unable to animate progressbar!");
        }
      }
      DrawProgress(cgContext, macRect, IsIndeterminateProgress(aFrame, eventState),
                   !IsVerticalProgress(aFrame),
                   value, maxValue, aFrame);
      break;
    }

    case NS_THEME_PROGRESSBAR_VERTICAL:
      DrawProgress(cgContext, macRect, IsIndeterminateProgress(aFrame, eventState),
                   false, GetProgressValue(aFrame),
                   GetProgressMaxValue(aFrame), aFrame);
      break;

    case NS_THEME_METERBAR:
      DrawMeter(cgContext, macRect, aFrame);
      break;

    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
    case NS_THEME_METERCHUNK:
      // Do nothing: progress and meter bars cases will draw chunks.
      break;

    case NS_THEME_TREETWISTY:
      DrawButton(cgContext, kThemeDisclosureButton, macRect, false,
                 kThemeDisclosureRight, kThemeAdornmentNone, eventState, aFrame);
      break;

    case NS_THEME_TREETWISTYOPEN:
      DrawButton(cgContext, kThemeDisclosureButton, macRect, false,
                 kThemeDisclosureDown, kThemeAdornmentNone, eventState, aFrame);
      break;

    case NS_THEME_TREEHEADERCELL: {
      TreeSortDirection sortDirection = GetTreeSortDirection(aFrame);
      DrawButton(cgContext, kThemeListHeaderButton, macRect, false,
                 sortDirection == eTreeSortDirection_Natural ? kThemeButtonOff : kThemeButtonOn,
                 sortDirection == eTreeSortDirection_Ascending ?
                 kThemeAdornmentHeaderButtonSortUp : kThemeAdornmentNone, eventState, aFrame);
    }
      break;

    case NS_THEME_TREEITEM:
    case NS_THEME_TREEVIEW:
      // HIThemeSetFill is not available on 10.3
      // HIThemeSetFill(kThemeBrushWhite, NULL, cgContext, HITHEME_ORIENTATION);
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      CGContextFillRect(cgContext, macRect);
      break;

    case NS_THEME_TREEHEADER:
      // do nothing, taken care of by individual header cells
    case NS_THEME_TREEHEADERSORTARROW:
      // do nothing, taken care of by treeview header
    case NS_THEME_TREELINE:
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

    case NS_THEME_SCALETHUMB_HORIZONTAL:
    case NS_THEME_SCALETHUMB_VERTICAL:
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
      bool reverseDir = isVertical || rangeFrame->IsRightToLeft();
      DrawScale(cgContext, macRect, eventState, isVertical, reverseDir,
                value, min, max, aFrame);
      break;
    }

    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBAR:
      break;
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL: {
      BOOL isOverlay = nsLookAndFeel::UseOverlayScrollbars();
      BOOL isHorizontal = (aWidgetType == NS_THEME_SCROLLBARTHUMB_HORIZONTAL);
      BOOL isRolledOver = IsParentScrollbarRolledOver(aFrame);
      nsIFrame* scrollbarFrame = GetParentScrollbarFrame(aFrame);
      bool isSmall = (scrollbarFrame && scrollbarFrame->StyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL);
      if (isOverlay && !isRolledOver) {
        if (isHorizontal) {
          macRect.origin.y += 4;
          macRect.size.height -= 4;
        } else {
          if (aFrame->StyleVisibility()->mDirection !=
              NS_STYLE_DIRECTION_RTL) {
            macRect.origin.x += 4;
          }
          macRect.size.width -= 4;
        }
      }
      const BOOL isOnTopOfDarkBackground = IsDarkBackground(aFrame);
      NSMutableDictionary* options = [NSMutableDictionary dictionaryWithObjectsAndKeys:
        (isOverlay ? @"kCUIWidgetOverlayScrollBar" : @"scrollbar"), @"widget",
        (isSmall ? @"small" : @"regular"), @"size",
        (isHorizontal ? @"kCUIOrientHorizontal" : @"kCUIOrientVertical"), @"kCUIOrientationKey",
        (isOnTopOfDarkBackground ? @"kCUIVariantWhite" : @""), @"kCUIVariantKey",
        [NSNumber numberWithBool:YES], @"indiconly",
        [NSNumber numberWithBool:YES], @"kCUIThumbProportionKey",
        [NSNumber numberWithBool:YES], @"is.flipped",
        nil];
      if (isRolledOver) {
        [options setObject:@"rollover" forKey:@"state"];
      }
      RenderWithCoreUI(macRect, cgContext, options, true);
    }
      break;

    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
#if SCROLLBARS_VISUAL_DEBUG
      CGContextSetRGBFillColor(cgContext, 1.0, 0, 0, 0.6);
      CGContextFillRect(cgContext, macRect);
#endif
    break;
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
#if SCROLLBARS_VISUAL_DEBUG
      CGContextSetRGBFillColor(cgContext, 0, 1.0, 0, 0.6);
      CGContextFillRect(cgContext, macRect);
#endif
    break;
    case NS_THEME_SCROLLBARTRACK_HORIZONTAL:
    case NS_THEME_SCROLLBARTRACK_VERTICAL: {
      BOOL isOverlay = nsLookAndFeel::UseOverlayScrollbars();
      if (!isOverlay || IsParentScrollbarRolledOver(aFrame)) {
        BOOL isHorizontal = (aWidgetType == NS_THEME_SCROLLBARTRACK_HORIZONTAL);
        nsIFrame* scrollbarFrame = GetParentScrollbarFrame(aFrame);
        bool isSmall = (scrollbarFrame && scrollbarFrame->StyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL);
        const BOOL isOnTopOfDarkBackground = IsDarkBackground(aFrame);
        RenderWithCoreUI(macRect, cgContext,
                [NSDictionary dictionaryWithObjectsAndKeys:
                  (isOverlay ? @"kCUIWidgetOverlayScrollBar" : @"scrollbar"), @"widget",
                  (isSmall ? @"small" : @"regular"), @"size",
                  (isHorizontal ? @"kCUIOrientHorizontal" : @"kCUIOrientVertical"), @"kCUIOrientationKey",
                  (isOnTopOfDarkBackground ? @"kCUIVariantWhite" : @""), @"kCUIVariantKey",
                  [NSNumber numberWithBool:YES], @"noindicator",
                  [NSNumber numberWithBool:YES], @"kCUIThumbProportionKey",
                  [NSNumber numberWithBool:YES], @"is.flipped",
                  nil],
                true);
      }
    }
      break;

    case NS_THEME_TEXTFIELD_MULTILINE: {
      // we have to draw this by hand because there is no HITheme value for it
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);

      CGContextFillRect(cgContext, macRect);

      // #737373 for the top border, #999999 for the rest.
      float x = macRect.origin.x, y = macRect.origin.y;
      float w = macRect.size.width, h = macRect.size.height;
      CGContextSetRGBFillColor(cgContext, 0.4510, 0.4510, 0.4510, 1.0);
      CGContextFillRect(cgContext, CGRectMake(x, y, w, 1));
      CGContextSetRGBFillColor(cgContext, 0.6, 0.6, 0.6, 1.0);
      CGContextFillRect(cgContext, CGRectMake(x, y + 1, 1, h - 1));
      CGContextFillRect(cgContext, CGRectMake(x + w - 1, y + 1, 1, h - 1));
      CGContextFillRect(cgContext, CGRectMake(x + 1, y + h - 1, w - 2, 1));

      // draw a focus ring
      if (eventState.HasState(NS_EVENT_STATE_FOCUS)) {
        NSGraphicsContext* savedContext = [NSGraphicsContext currentContext];
        [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:YES]];
        CGContextSaveGState(cgContext);
        NSSetFocusRingStyle(NSFocusRingOnly);
        NSRectFill(NSRectFromCGRect(macRect));
        CGContextRestoreGState(cgContext);
        [NSGraphicsContext setCurrentContext:savedContext];
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

    case NS_THEME_MAC_SOURCE_LIST: {
      if (VibrancyManager::SystemSupportsVibrancy()) {
        ThemeGeometryType type = ThemeGeometryTypeForWidget(aFrame, aWidgetType);
        DrawVibrancyBackground(cgContext, macRect, aFrame, type);
      } else {
        CGGradientRef backgroundGradient;
        CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
        CGFloat activeGradientColors[8] = { 0.9137, 0.9294, 0.9490, 1.0,
                                            0.8196, 0.8471, 0.8784, 1.0 };
        CGFloat inactiveGradientColors[8] = { 0.9686, 0.9686, 0.9686, 1.0,
                                              0.9216, 0.9216, 0.9216, 1.0 };
        CGPoint start = macRect.origin;
        CGPoint end = CGPointMake(macRect.origin.x,
                                  macRect.origin.y + macRect.size.height);
        BOOL isActive = FrameIsInActiveWindow(aFrame);
        backgroundGradient =
          CGGradientCreateWithColorComponents(rgb, isActive ? activeGradientColors
                                                            : inactiveGradientColors, NULL, 2);
        CGContextDrawLinearGradient(cgContext, backgroundGradient, start, end, 0);
        CGGradientRelease(backgroundGradient);
        CGColorSpaceRelease(rgb);
      }
    }
      break;

    case NS_THEME_MAC_SOURCE_LIST_SELECTION:
    case NS_THEME_MAC_ACTIVE_SOURCE_LIST_SELECTION: {
      // If we're in XUL tree, we need to rely on the source list's clear
      // background display item. If we cleared the background behind the
      // selections, the source list would not pick up the right font
      // smoothing background. So, to simplify a bit, we only support vibrancy
      // if we're in a source list.
      if (VibrancyManager::SystemSupportsVibrancy() && IsInSourceList(aFrame)) {
        ThemeGeometryType type = ThemeGeometryTypeForWidget(aFrame, aWidgetType);
        DrawVibrancyBackground(cgContext, macRect, aFrame, type);
      } else {
        BOOL isActiveSelection =
          aWidgetType == NS_THEME_MAC_ACTIVE_SOURCE_LIST_SELECTION;
        RenderWithCoreUI(macRect, cgContext,
          [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithBool:isActiveSelection], @"focus",
            [NSNumber numberWithBool:YES], @"is.flipped",
            @"kCUIVariantGradientSideBarSelection", @"kCUIVariantKey",
            (FrameIsInActiveWindow(aFrame) ? @"normal" : @"inactive"), @"state",
            @"gradient", @"widget",
            nil]);
      }
    }
      break;

    case NS_THEME_TAB:
      DrawSegment(cgContext, macRect, eventState, aFrame, tabRenderSettings);
      break;

    case NS_THEME_TABPANELS:
      DrawTabPanel(cgContext, macRect, aFrame);
      break;

    case NS_THEME_RESIZER:
      DrawResizer(cgContext, macRect, aFrame);
      break;

    case NS_THEME_MAC_VIBRANCY_LIGHT:
    case NS_THEME_MAC_VIBRANCY_DARK: {
      ThemeGeometryType type = ThemeGeometryTypeForWidget(aFrame, aWidgetType);
      DrawVibrancyBackground(cgContext, macRect, aFrame, type);
      break;
    }
  }

  if (hidpi) {
    // Reset the base CTM.
    CGContextSetBaseCTM(cgContext, CGAffineTransformIdentity);
  }

  nativeDrawing.EndNativeDrawing();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsIntMargin
nsNativeThemeCocoa::DirectionAwareMargin(const nsIntMargin& aMargin,
                                         nsIFrame* aFrame)
{
  // Assuming aMargin was originally specified for a horizontal LTR context,
  // reinterpret the values as logical, and then map to physical coords
  // according to aFrame's actual writing mode.
  WritingMode wm = aFrame->GetWritingMode();
  nsMargin m = LogicalMargin(wm, aMargin.top, aMargin.right, aMargin.bottom,
                             aMargin.left).GetPhysicalMargin(wm);
  return nsIntMargin(m.top, m.right, m.bottom, m.left);
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
        *aResult = DirectionAwareMargin(kAquaDropdownBorder, aFrame);
      } else {
        *aResult = DirectionAwareMargin(nsIntMargin(1, 7, 3, 7), aFrame);
      }
      break;
    }

    case NS_THEME_TOOLBARBUTTON:
    {
      *aResult = DirectionAwareMargin(nsIntMargin(1, 4, 1, 4), aFrame);
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

    case NS_THEME_MENULIST:
    case NS_THEME_MENULIST_BUTTON:
      *aResult = DirectionAwareMargin(kAquaDropdownBorder, aFrame);
      break;

    case NS_THEME_MENULIST_TEXTFIELD:
      *aResult = DirectionAwareMargin(kAquaComboboxBorder, aFrame);
      break;

    case NS_THEME_NUMBER_INPUT:
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
      *aResult = DirectionAwareMargin(kAquaSearchfieldBorder, aFrame);
      break;

    case NS_THEME_LISTBOX:
    {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricListBoxFrameOutset, &frameOutset);
      aResult->SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
      break;
    }

    case NS_THEME_SCROLLBARTRACK_HORIZONTAL:
    case NS_THEME_SCROLLBARTRACK_VERTICAL:
    {
      bool isHorizontal = (aWidgetType == NS_THEME_SCROLLBARTRACK_HORIZONTAL);
      if (nsLookAndFeel::UseOverlayScrollbars()) {
        if (!nsCocoaFeatures::OnYosemiteOrLater()) {
          // Pre-10.10, we have to center the thumb rect in the middle of the
          // scrollbar. Starting with 10.10, the expected rect for thumb
          // rendering is the full width of the scrollbar.
          if (isHorizontal) {
            aResult->top = 2;
            aResult->bottom = 1;
          } else {
            aResult->left = 2;
            aResult->right = 1;
          }
        }
        // Leave a bit of space at the start and the end on all OS X versions.
        if (isHorizontal) {
          aResult->left = 1;
          aResult->right = 1;
        } else {
          aResult->top = 1;
          aResult->bottom = 1;
        }
      }

      break;
    }

    case NS_THEME_STATUSBAR:
      aResult->SizeTo(1, 0, 0, 0);
      break;
  }

  if (IsHiDPIContext(aFrame->PresContext())) {
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
  int32_t p2a = aFrame->PresContext()->AppUnitsPerDevPixel();
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_MAC_DISCLOSURE_BUTTON_OPEN:
    case NS_THEME_MAC_DISCLOSURE_BUTTON_CLOSED:
    case NS_THEME_MAC_HELP_BUTTON:
    case NS_THEME_TOOLBARBUTTON:
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_SEARCHFIELD:
    case NS_THEME_LISTBOX:
    case NS_THEME_MENULIST:
    case NS_THEME_MENULIST_BUTTON:
    case NS_THEME_MENULIST_TEXTFIELD:
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
    case NS_THEME_TAB:
    {
      // We assume that the above widgets can draw a focus ring that will be less than
      // or equal to 4 pixels thick.
      nsIntMargin extraSize = nsIntMargin(kMaxFocusRingWidth,
                                          kMaxFocusRingWidth,
                                          kMaxFocusRingWidth,
                                          kMaxFocusRingWidth);
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
    case NS_THEME_FOCUS_OUTLINE:
    {
      aOverflowRect->Inflate(NSIntPixelsToAppUnits(2, p2a));
      return true;
    }
  }

  return false;
}

static const int32_t kRegularScrollbarThumbMinSize = 26;
static const int32_t kSmallScrollbarThumbMinSize = 26;

NS_IMETHODIMP
nsNativeThemeCocoa::GetMinimumWidgetSize(nsPresContext* aPresContext,
                                         nsIFrame* aFrame,
                                         uint8_t aWidgetType,
                                         LayoutDeviceIntSize* aResult,
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

    case NS_THEME_BUTTON_ARROW_UP:
    case NS_THEME_BUTTON_ARROW_DOWN:
    {
      aResult->SizeTo(kMenuScrollArrowSize.width, kMenuScrollArrowSize.height);
      *aIsOverridable = false;
      break;
    }

    case NS_THEME_MENUARROW:
    {
      aResult->SizeTo(kMenuarrowSize.width, kMenuarrowSize.height);
      *aIsOverridable = false;
      break;
    }

    case NS_THEME_MAC_DISCLOSURE_BUTTON_OPEN:
    case NS_THEME_MAC_DISCLOSURE_BUTTON_CLOSED:
    {
      aResult->SizeTo(kDisclosureButtonSize.width, kDisclosureButtonSize.height);
      *aIsOverridable = false;
      break;
    }

    case NS_THEME_MAC_HELP_BUTTON:
    {
      aResult->SizeTo(kHelpButtonSize.width, kHelpButtonSize.height);
      *aIsOverridable = false;
      break;
    }

    case NS_THEME_TOOLBARBUTTON:
    {
      aResult->SizeTo(0, toolbarButtonHeights[miniControlSize]);
      break;
    }

    case NS_THEME_SPINNER:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
    {
      SInt32 buttonHeight = 0, buttonWidth = 0;
      if (aFrame->GetContent()->IsXULElement()) {
        ::GetThemeMetric(kThemeMetricLittleArrowsWidth, &buttonWidth);
        ::GetThemeMetric(kThemeMetricLittleArrowsHeight, &buttonHeight);
      } else {
        NSSize size =
          spinnerSettings.minimumSizes[EnumSizeForCocoaSize(NSMiniControlSize)];
        buttonWidth = size.width;
        buttonHeight = size.height;
        if (aWidgetType != NS_THEME_SPINNER) {
          // the buttons are half the height of the spinner
          buttonHeight /= 2;
        }
      }
      aResult->SizeTo(buttonWidth, buttonHeight);
      *aIsOverridable = true;
      break;
    }

    case NS_THEME_MENULIST:
    case NS_THEME_MENULIST_BUTTON:
    {
      SInt32 popupHeight = 0;
      ::GetThemeMetric(kThemeMetricPopupButtonHeight, &popupHeight);
      aResult->SizeTo(0, popupHeight);
      break;
    }

    case NS_THEME_NUMBER_INPUT:
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

    case NS_THEME_WINDOW_BUTTON_BOX: {
      NSSize size = WindowButtonsSize(aFrame);
      aResult->SizeTo(size.width, size.height);
      *aIsOverridable = false;
      break;
    }

    case NS_THEME_MAC_FULLSCREEN_BUTTON: {
      if ([NativeWindowForFrame(aFrame) respondsToSelector:@selector(toggleFullScreen:)] &&
          !nsCocoaFeatures::OnYosemiteOrLater()) {
        // This value is hardcoded because it's needed before we can measure the
        // position and size of the fullscreen button.
        aResult->SizeTo(16, 17);
      }
      *aIsOverridable = false;
      break;
    }

    case NS_THEME_PROGRESSBAR:
    {
      SInt32 barHeight = 0;
      ::GetThemeMetric(kThemeMetricNormalProgressBarThickness, &barHeight);
      aResult->SizeTo(0, barHeight);
      break;
    }

    case NS_THEME_TREETWISTY:
    case NS_THEME_TREETWISTYOPEN:
    {
      SInt32 twistyHeight = 0, twistyWidth = 0;
      ::GetThemeMetric(kThemeMetricDisclosureButtonWidth, &twistyWidth);
      ::GetThemeMetric(kThemeMetricDisclosureButtonHeight, &twistyHeight);
      aResult->SizeTo(twistyWidth, twistyHeight);
      *aIsOverridable = false;
      break;
    }

    case NS_THEME_TREEHEADER:
    case NS_THEME_TREEHEADERCELL:
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

    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    {
      // Find our parent scrollbar frame in order to find out whether we're in
      // a small or a large scrollbar.
      nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
      if (!scrollbarFrame)
        return NS_ERROR_FAILURE;

      bool isSmall = (scrollbarFrame->StyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL);
      bool isHorizontal = (aWidgetType == NS_THEME_SCROLLBARTHUMB_HORIZONTAL);
      int32_t& minSize = isHorizontal ? aResult->width : aResult->height;
      minSize = isSmall ? kSmallScrollbarThumbMinSize : kRegularScrollbarThumbMinSize;
      break;
    }

    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBARTRACK_VERTICAL:
    case NS_THEME_SCROLLBARTRACK_HORIZONTAL:
    {
      *aIsOverridable = false;

      if (nsLookAndFeel::UseOverlayScrollbars()) {
        nsIFrame* scrollbarFrame = GetParentScrollbarFrame(aFrame);
        if (scrollbarFrame &&
            scrollbarFrame->StyleDisplay()->mAppearance ==
              NS_THEME_SCROLLBAR_SMALL) {
          aResult->SizeTo(14, 14);
        }
        else {
          aResult->SizeTo(16, 16);
        }
        break;
      }

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

    case NS_THEME_SCROLLBAR_NON_DISAPPEARING:
    {
      int32_t themeMetric = kThemeMetricScrollBarWidth;

      if (aFrame) {
        nsIFrame* scrollbarFrame = GetParentScrollbarFrame(aFrame);
        if (scrollbarFrame &&
            scrollbarFrame->StyleDisplay()->mAppearance ==
            NS_THEME_SCROLLBAR_SMALL) {
          // XXX We're interested in the width of non-disappearing scrollbars
          // to leave enough space for a dropmarker in non-native styled
          // comboboxes (bug 869314). It isn't clear to me if comboboxes can
          // ever have small scrollbars.
          themeMetric = kThemeMetricSmallScrollBarWidth;
        }
      }

      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(themeMetric, &scrollbarWidth);
      aResult->SizeTo(scrollbarWidth, scrollbarWidth);
      break;
    }

    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
    {
      nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
      if (!scrollbarFrame) return NS_ERROR_FAILURE;

      // Since there is no NS_THEME_SCROLLBARBUTTON_UP_SMALL we need to ask the parent what appearance style it has.
      int32_t themeMetric = (scrollbarFrame->StyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL) ?
                            kThemeMetricSmallScrollBarWidth :
                            kThemeMetricScrollBarWidth;
      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(themeMetric, &scrollbarWidth);

      // It seems that for both sizes of scrollbar, the buttons are one pixel "longer".
      if (aWidgetType == NS_THEME_SCROLLBARBUTTON_LEFT || aWidgetType == NS_THEME_SCROLLBARBUTTON_RIGHT)
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

  if (IsHiDPIContext(aPresContext)) {
    *aResult = *aResult * 2;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsNativeThemeCocoa::WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType,
                                       nsIAtom* aAttribute, bool* aShouldRepaint,
                                       const nsAttrValue* aOldValue)
{
  // Some widget types just never change state.
  switch (aWidgetType) {
    case NS_THEME_WINDOW_TITLEBAR:
    case NS_THEME_TOOLBOX:
    case NS_THEME_TOOLBAR:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBARPANEL:
    case NS_THEME_RESIZERPANEL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_TABPANELS:
    case NS_THEME_TABPANEL:
    case NS_THEME_DIALOG:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_GROUPBOX:
    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_METERBAR:
    case NS_THEME_METERCHUNK:
    case NS_THEME_MAC_VIBRANCY_LIGHT:
    case NS_THEME_MAC_VIBRANCY_DARK:
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
        aAttribute == nsGkAtoms::visuallyselected ||
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
  if (aWidgetType == NS_THEME_MENULIST_BUTTON) {
    nsIFrame* parentFrame = aFrame->GetParent();
    if (parentFrame && parentFrame->IsComboboxControlFrame())
      return false;
  }

  switch (aWidgetType) {
    // Combobox dropdowns don't support native theming in vertical mode.
    case NS_THEME_MENULIST:
    case NS_THEME_MENULIST_BUTTON:
    case NS_THEME_MENULIST_TEXT:
    case NS_THEME_MENULIST_TEXTFIELD:
      if (aFrame && aFrame->GetWritingMode().IsVertical()) {
        return false;
      }
      MOZ_FALLTHROUGH;

    case NS_THEME_LISTBOX:

    case NS_THEME_DIALOG:
    case NS_THEME_WINDOW:
    case NS_THEME_WINDOW_BUTTON_BOX:
    case NS_THEME_WINDOW_TITLEBAR:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUARROW:
    case NS_THEME_MENUITEM:
    case NS_THEME_MENUSEPARATOR:
    case NS_THEME_MAC_FULLSCREEN_BUTTON:
    case NS_THEME_TOOLTIP:

    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_CONTAINER:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_CONTAINER:
    case NS_THEME_GROUPBOX:
    case NS_THEME_MAC_HELP_BUTTON:
    case NS_THEME_MAC_DISCLOSURE_BUTTON_OPEN:
    case NS_THEME_MAC_DISCLOSURE_BUTTON_CLOSED:
    case NS_THEME_BUTTON:
    case NS_THEME_BUTTON_ARROW_UP:
    case NS_THEME_BUTTON_ARROW_DOWN:
    case NS_THEME_BUTTON_BEVEL:
    case NS_THEME_TOOLBARBUTTON:
    case NS_THEME_SPINNER:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
    case NS_THEME_TOOLBAR:
    case NS_THEME_STATUSBAR:
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_SEARCHFIELD:
    case NS_THEME_TOOLBOX:
    //case NS_THEME_TOOLBARBUTTON:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
    case NS_THEME_METERBAR:
    case NS_THEME_METERCHUNK:
    case NS_THEME_SEPARATOR:

    case NS_THEME_TABPANELS:
    case NS_THEME_TAB:

    case NS_THEME_TREETWISTY:
    case NS_THEME_TREETWISTYOPEN:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREEHEADER:
    case NS_THEME_TREEHEADERCELL:
    case NS_THEME_TREEHEADERSORTARROW:
    case NS_THEME_TREEITEM:
    case NS_THEME_TREELINE:
    case NS_THEME_MAC_SOURCE_LIST:
    case NS_THEME_MAC_SOURCE_LIST_SELECTION:
    case NS_THEME_MAC_ACTIVE_SOURCE_LIST_SELECTION:

    case NS_THEME_RANGE:

    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALETHUMB_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALETHUMB_VERTICAL:

    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    case NS_THEME_SCROLLBARTRACK_VERTICAL:
    case NS_THEME_SCROLLBARTRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_NON_DISAPPEARING:
      return !IsWidgetStyled(aPresContext, aFrame, aWidgetType);

    case NS_THEME_RESIZER:
    {
      nsIFrame* parentFrame = aFrame->GetParent();
      if (!parentFrame || !parentFrame->IsScrollFrame())
        return true;

      // Note that IsWidgetStyled is not called for resizers on Mac. This is
      // because for scrollable containers, the native resizer looks better
      // when (non-overlay) scrollbars are present even when the style is
      // overriden, and the custom transparent resizer looks better when
      // scrollbars are not present.
      nsIScrollableFrame* scrollFrame = do_QueryFrame(parentFrame);
      return (!nsLookAndFeel::UseOverlayScrollbars() &&
              scrollFrame && scrollFrame->GetScrollbarVisibility());
    }

    case NS_THEME_FOCUS_OUTLINE:
      return true;

    case NS_THEME_MAC_VIBRANCY_LIGHT:
    case NS_THEME_MAC_VIBRANCY_DARK:
      return VibrancyManager::SystemSupportsVibrancy();
  }

  return false;
}

bool
nsNativeThemeCocoa::WidgetIsContainer(uint8_t aWidgetType)
{
  // flesh this out at some point
  switch (aWidgetType) {
   case NS_THEME_MENULIST_BUTTON:
   case NS_THEME_RADIO:
   case NS_THEME_CHECKBOX:
   case NS_THEME_PROGRESSBAR:
   case NS_THEME_METERBAR:
   case NS_THEME_RANGE:
   case NS_THEME_MAC_HELP_BUTTON:
   case NS_THEME_MAC_DISCLOSURE_BUTTON_OPEN:
   case NS_THEME_MAC_DISCLOSURE_BUTTON_CLOSED:
    return false;
  }
  return true;
}

bool
nsNativeThemeCocoa::ThemeDrawsFocusForWidget(uint8_t aWidgetType)
{
  if (aWidgetType == NS_THEME_MENULIST ||
      aWidgetType == NS_THEME_MENULIST_TEXTFIELD ||
      aWidgetType == NS_THEME_BUTTON ||
      aWidgetType == NS_THEME_MAC_HELP_BUTTON ||
      aWidgetType == NS_THEME_MAC_DISCLOSURE_BUTTON_OPEN ||
      aWidgetType == NS_THEME_MAC_DISCLOSURE_BUTTON_CLOSED ||
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

bool
nsNativeThemeCocoa::WidgetAppearanceDependsOnWindowFocus(uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_DIALOG:
    case NS_THEME_GROUPBOX:
    case NS_THEME_TABPANELS:
    case NS_THEME_BUTTON_ARROW_UP:
    case NS_THEME_BUTTON_ARROW_DOWN:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUARROW:
    case NS_THEME_MENUITEM:
    case NS_THEME_MENUSEPARATOR:
    case NS_THEME_TOOLTIP:
    case NS_THEME_SPINNER:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
    case NS_THEME_SEPARATOR:
    case NS_THEME_TOOLBOX:
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREELINE:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_LISTBOX:
    case NS_THEME_RESIZER:
      return false;
    default:
      return true;
  }
}

bool
nsNativeThemeCocoa::IsWindowSheet(nsIFrame* aFrame)
{
  NSWindow* win = NativeWindowForFrame(aFrame);
  id winDelegate = [win delegate];
  nsIWidget* widget = [(WindowDelegate *)winDelegate geckoWidget];
  if (!widget) {
    return false;
  }
  return (widget->WindowType() == eWindowType_sheet);
}

bool
nsNativeThemeCocoa::NeedToClearBackgroundBehindWidget(nsIFrame* aFrame,
                                                      uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_MAC_SOURCE_LIST:
    // If we're in a XUL tree, we don't want to clear the background behind the
    // selections below, since that would make our source list to not pick up
    // the right font smoothing background. But since we don't call this method
    // in nsTreeBodyFrame::BuildDisplayList, we never get here.
    case NS_THEME_MAC_SOURCE_LIST_SELECTION:
    case NS_THEME_MAC_ACTIVE_SOURCE_LIST_SELECTION:
    case NS_THEME_MAC_VIBRANCY_LIGHT:
    case NS_THEME_MAC_VIBRANCY_DARK:
    case NS_THEME_TOOLTIP:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
      return true;
    case NS_THEME_DIALOG:
      return IsWindowSheet(aFrame);
    default:
      return false;
  }
}

static nscolor ConvertNSColor(NSColor* aColor)
{
  NSColor* deviceColor = [aColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
  return NS_RGBA((unsigned int)([deviceColor redComponent] * 255.0),
                 (unsigned int)([deviceColor greenComponent] * 255.0),
                 (unsigned int)([deviceColor blueComponent] * 255.0),
                 (unsigned int)([deviceColor alphaComponent] * 255.0));
}

bool
nsNativeThemeCocoa::WidgetProvidesFontSmoothingBackgroundColor(nsIFrame* aFrame,
                                                               uint8_t aWidgetType,
                                                               nscolor* aColor)
{
  switch (aWidgetType) {
    case NS_THEME_MAC_SOURCE_LIST:
    case NS_THEME_MAC_SOURCE_LIST_SELECTION:
    case NS_THEME_MAC_ACTIVE_SOURCE_LIST_SELECTION:
    case NS_THEME_MAC_VIBRANCY_LIGHT:
    case NS_THEME_MAC_VIBRANCY_DARK:
    case NS_THEME_TOOLTIP:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_DIALOG:
    {
      if ((aWidgetType == NS_THEME_DIALOG && !IsWindowSheet(aFrame)) ||
          ((aWidgetType == NS_THEME_MAC_SOURCE_LIST_SELECTION ||
            aWidgetType == NS_THEME_MAC_ACTIVE_SOURCE_LIST_SELECTION) &&
            !IsInSourceList(aFrame))) {
        return false;
      }
      ChildView* childView = ChildViewForFrame(aFrame);
      if (childView) {
        ThemeGeometryType type = ThemeGeometryTypeForWidget(aFrame, aWidgetType);
        NSColor* color = [childView vibrancyFontSmoothingBackgroundColorForThemeGeometryType:type];
        *aColor = ConvertNSColor(color);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

nsITheme::ThemeGeometryType
nsNativeThemeCocoa::ThemeGeometryTypeForWidget(nsIFrame* aFrame, uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_WINDOW_TITLEBAR:
      return eThemeGeometryTypeTitlebar;
    case NS_THEME_TOOLBAR:
      return eThemeGeometryTypeToolbar;
    case NS_THEME_TOOLBOX:
      return eThemeGeometryTypeToolbox;
    case NS_THEME_WINDOW_BUTTON_BOX:
      return eThemeGeometryTypeWindowButtons;
    case NS_THEME_MAC_FULLSCREEN_BUTTON:
      return eThemeGeometryTypeFullscreenButton;
    case NS_THEME_MAC_VIBRANCY_LIGHT:
      return eThemeGeometryTypeVibrancyLight;
    case NS_THEME_MAC_VIBRANCY_DARK:
      return eThemeGeometryTypeVibrancyDark;
    case NS_THEME_TOOLTIP:
      return eThemeGeometryTypeTooltip;
    case NS_THEME_MENUPOPUP:
      return eThemeGeometryTypeMenu;
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM: {
      EventStates eventState = GetContentState(aFrame, aWidgetType);
      bool isDisabled = IsDisabled(aFrame, eventState);
      bool isSelected = !isDisabled && CheckBooleanAttr(aFrame, nsGkAtoms::menuactive);
      return isSelected ? eThemeGeometryTypeHighlightedMenuItem : eThemeGeometryTypeMenu;
    }
    case NS_THEME_DIALOG:
      return IsWindowSheet(aFrame) ? eThemeGeometryTypeSheet : eThemeGeometryTypeUnknown;
    case NS_THEME_MAC_SOURCE_LIST:
      return eThemeGeometryTypeSourceList;
    case NS_THEME_MAC_SOURCE_LIST_SELECTION:
      return IsInSourceList(aFrame) ? eThemeGeometryTypeSourceListSelection
                                    : eThemeGeometryTypeUnknown;
    case NS_THEME_MAC_ACTIVE_SOURCE_LIST_SELECTION:
      return IsInSourceList(aFrame) ? eThemeGeometryTypeActiveSourceListSelection
                                    : eThemeGeometryTypeUnknown;
    default:
      return eThemeGeometryTypeUnknown;
  }
}

nsITheme::Transparency
nsNativeThemeCocoa::GetWidgetTransparency(nsIFrame* aFrame, uint8_t aWidgetType)
{
  switch (aWidgetType) {
  case NS_THEME_MENUPOPUP:
  case NS_THEME_TOOLTIP:
    return eTransparent;

  case NS_THEME_DIALOG:
    return IsWindowSheet(aFrame) ? eTransparent : eOpaque;

  case NS_THEME_SCROLLBAR_SMALL:
  case NS_THEME_SCROLLBAR:
    return nsLookAndFeel::UseOverlayScrollbars() ? eTransparent : eOpaque;

  case NS_THEME_STATUSBAR:
    // Knowing that scrollbars and statusbars are opaque improves
    // performance, because we create layers for them.
    return eOpaque;

  case NS_THEME_TOOLBAR:
    return eOpaque;

  default:
    return eUnknownTransparency;
  }
}
