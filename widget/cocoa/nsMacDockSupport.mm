/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsComponentManagerUtils.h"
#include "nsMacDockSupport.h"
#include "nsObjCExceptions.h"
#include "nsNativeThemeColors.h"

NS_IMPL_ISUPPORTS(nsMacDockSupport, nsIMacDockSupport, nsITaskbarProgress)

// This view is used in the dock tile when we're downloading a file.
// It draws a progress bar that looks similar to the native progress bar on
// 10.12. This style of progress bar is not animated, unlike the pre-10.10
// progress bar look which had to redrawn multiple times per second.
@interface MOZProgressDockOverlayView : NSView {
  double mFractionValue;
}
@property double fractionValue;

@end

@implementation MOZProgressDockOverlayView

@synthesize fractionValue = mFractionValue;

- (void)drawRect:(NSRect)aRect {
  // Erase the background behind this view, i.e. cut a rectangle hole in the icon.
  [[NSColor clearColor] set];
  NSRectFill(self.bounds);

  // Split the height of this view into four quarters. The middle two quarters
  // will be covered by the actual progress bar.
  CGFloat radius = self.bounds.size.height / 4;
  NSRect barBounds = NSInsetRect(self.bounds, 0, radius);

  NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:barBounds
                                                       xRadius:radius
                                                       yRadius:radius];

  // Draw a grayish background first.
  [[NSColor colorWithDeviceWhite:0 alpha:0.1] setFill];
  [path fill];

  // Draw a fill in the control accent color for the progress part.
  NSRect progressFillRect = self.bounds;
  progressFillRect.size.width *= mFractionValue;
  [NSGraphicsContext saveGraphicsState];
  [NSBezierPath clipRect:progressFillRect];
  [ControlAccentColor() setFill];
  [path fill];
  [NSGraphicsContext restoreGraphicsState];

  // Add a shadowy stroke on top.
  [NSGraphicsContext saveGraphicsState];
  [path addClip];
  [[NSColor colorWithDeviceWhite:0 alpha:0.2] setStroke];
  path.lineWidth = barBounds.size.height / 10;
  [path stroke];
  [NSGraphicsContext restoreGraphicsState];
}

@end

nsMacDockSupport::nsMacDockSupport()
    : mDockTileWrapperView(nil),
      mProgressDockOverlayView(nil),
      mProgressState(STATE_NO_PROGRESS),
      mProgressFraction(0.0) {}

nsMacDockSupport::~nsMacDockSupport() {
  if (mDockTileWrapperView) {
    [mDockTileWrapperView release];
    mDockTileWrapperView = nil;
  }
  if (mProgressDockOverlayView) {
    [mProgressDockOverlayView release];
    mProgressDockOverlayView = nil;
  }
}

NS_IMETHODIMP
nsMacDockSupport::GetDockMenu(nsIStandaloneNativeMenu** aDockMenu) {
  nsCOMPtr<nsIStandaloneNativeMenu> dockMenu(mDockMenu);
  dockMenu.forget(aDockMenu);
  return NS_OK;
}

NS_IMETHODIMP
nsMacDockSupport::SetDockMenu(nsIStandaloneNativeMenu* aDockMenu) {
  mDockMenu = aDockMenu;
  return NS_OK;
}

NS_IMETHODIMP
nsMacDockSupport::ActivateApplication(bool aIgnoreOtherApplications) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [[NSApplication sharedApplication] activateIgnoringOtherApps:aIgnoreOtherApplications];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsMacDockSupport::SetBadgeText(const nsAString& aBadgeText) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSDockTile* tile = [[NSApplication sharedApplication] dockTile];
  mBadgeText = aBadgeText;
  if (aBadgeText.IsEmpty())
    [tile setBadgeLabel:nil];
  else
    [tile setBadgeLabel:[NSString
                            stringWithCharacters:reinterpret_cast<const unichar*>(mBadgeText.get())
                                          length:mBadgeText.Length()]];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsMacDockSupport::GetBadgeText(nsAString& aBadgeText) {
  aBadgeText = mBadgeText;
  return NS_OK;
}

NS_IMETHODIMP
nsMacDockSupport::SetProgressState(nsTaskbarProgressState aState, uint64_t aCurrentValue,
                                   uint64_t aMaxValue) {
  NS_ENSURE_ARG_RANGE(aState, 0, STATE_PAUSED);
  if (aState == STATE_NO_PROGRESS || aState == STATE_INDETERMINATE) {
    NS_ENSURE_TRUE(aCurrentValue == 0, NS_ERROR_INVALID_ARG);
    NS_ENSURE_TRUE(aMaxValue == 0, NS_ERROR_INVALID_ARG);
  }
  if (aCurrentValue > aMaxValue) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  mProgressState = aState;
  if (aMaxValue == 0) {
    mProgressFraction = 0;
  } else {
    mProgressFraction = (double)aCurrentValue / aMaxValue;
  }

  return UpdateDockTile();
}

nsresult nsMacDockSupport::UpdateDockTile() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (mProgressState == STATE_NORMAL || mProgressState == STATE_INDETERMINATE) {
    if (!mDockTileWrapperView) {
      // Create the following NSView hierarchy:
      // * mDockTileWrapperView (NSView)
      //    * imageView (NSImageView) <- has the application icon
      //    * mProgressDockOverlayView (MOZProgressDockOverlayView) <- draws the progress bar

      mDockTileWrapperView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 32, 32)];
      mDockTileWrapperView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

      NSImageView* imageView = [[NSImageView alloc] initWithFrame:[mDockTileWrapperView bounds]];
      imageView.image = [NSImage imageNamed:@"NSApplicationIcon"];
      imageView.imageScaling = NSImageScaleAxesIndependently;
      imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
      [mDockTileWrapperView addSubview:imageView];

      mProgressDockOverlayView =
          [[MOZProgressDockOverlayView alloc] initWithFrame:NSMakeRect(1, 3, 30, 4)];
      mProgressDockOverlayView.autoresizingMask = NSViewMinXMargin | NSViewWidthSizable |
                                                  NSViewMaxXMargin | NSViewMinYMargin |
                                                  NSViewHeightSizable | NSViewMaxYMargin;
      [mDockTileWrapperView addSubview:mProgressDockOverlayView];
    }
    if (NSApp.dockTile.contentView != mDockTileWrapperView) {
      NSApp.dockTile.contentView = mDockTileWrapperView;
    }

    if (mProgressState == STATE_NORMAL) {
      mProgressDockOverlayView.fractionValue = mProgressFraction;
    } else {
      // Indeterminate states are rare. Just fill the entire progress bar in
      // that case.
      mProgressDockOverlayView.fractionValue = 1.0;
    }
    [NSApp.dockTile display];
  } else if (NSApp.dockTile.contentView) {
    NSApp.dockTile.contentView = nil;
    [NSApp.dockTile display];
  }

  return NS_OK;
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
