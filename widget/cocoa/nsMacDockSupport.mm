/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsComponentManagerUtils.h"
#include "nsMacDockSupport.h"
#include "nsObjCExceptions.h"

NS_IMPL_ISUPPORTS(nsMacDockSupport, nsIMacDockSupport, nsITaskbarProgress)

nsMacDockSupport::nsMacDockSupport()
: mAppIcon(nil)
, mProgressBackground(nil)
, mProgressState(STATE_NO_PROGRESS)
, mProgressFraction(0.0)
{
  mProgressTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
}

nsMacDockSupport::~nsMacDockSupport()
{
  if (mAppIcon) {
    [mAppIcon release];
    mAppIcon = nil;
  }
  if (mProgressBackground) {
    [mProgressBackground release];
    mProgressBackground = nil;
  }
  if (mProgressTimer) {
    mProgressTimer->Cancel();
    mProgressTimer = nullptr;
  }
}

NS_IMETHODIMP
nsMacDockSupport::GetDockMenu(nsIStandaloneNativeMenu ** aDockMenu)
{
  nsCOMPtr<nsIStandaloneNativeMenu> dockMenu(mDockMenu);
  dockMenu.forget(aDockMenu);
  return NS_OK;
}

NS_IMETHODIMP
nsMacDockSupport::SetDockMenu(nsIStandaloneNativeMenu * aDockMenu)
{
  mDockMenu = aDockMenu;
  return NS_OK;
}

NS_IMETHODIMP
nsMacDockSupport::ActivateApplication(bool aIgnoreOtherApplications)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [[NSApplication sharedApplication] activateIgnoringOtherApps:aIgnoreOtherApplications];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsMacDockSupport::SetBadgeText(const nsAString& aBadgeText)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSDockTile *tile = [[NSApplication sharedApplication] dockTile];
  mBadgeText = aBadgeText;
  if (aBadgeText.IsEmpty())
    [tile setBadgeLabel: nil];
  else
    [tile setBadgeLabel:[NSString stringWithCharacters:reinterpret_cast<const unichar*>(mBadgeText.get())
                                                length:mBadgeText.Length()]];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsMacDockSupport::GetBadgeText(nsAString& aBadgeText)
{
  aBadgeText = mBadgeText;
  return NS_OK;
}

NS_IMETHODIMP
nsMacDockSupport::SetProgressState(nsTaskbarProgressState aState,
                                   uint64_t aCurrentValue,
                                   uint64_t aMaxValue)
{
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

  if (mProgressState == STATE_NORMAL || mProgressState == STATE_INDETERMINATE) {
    int perSecond = 8; // Empirically determined, see bug 848792 
    mProgressTimer->InitWithNamedFuncCallback(RedrawIconCallback, this, 1000 / perSecond,
                                              nsITimer::TYPE_REPEATING_SLACK,
                                              "nsMacDockSupport::RedrawIconCallback");
    return NS_OK;
  } else {
    mProgressTimer->Cancel();
    return RedrawIcon();
  }
}

// static
void nsMacDockSupport::RedrawIconCallback(nsITimer* aTimer, void* aClosure)
{
  static_cast<nsMacDockSupport*>(aClosure)->RedrawIcon();
}

// Return whether to draw progress
bool nsMacDockSupport::InitProgress()
{
  if (mProgressState != STATE_NORMAL && mProgressState != STATE_INDETERMINATE) {
    return false;
  }

  if (!mAppIcon) {
    mProgressTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    mAppIcon = [[NSImage imageNamed:@"NSApplicationIcon"] retain];
    mProgressBackground = [mAppIcon copyWithZone:nil];
    mTheme = new nsNativeThemeCocoa();

    NSSize sz = [mProgressBackground size];
    mProgressBounds = CGRectMake(sz.width * 1/32, sz.height * 3/32,
                                 sz.width * 30/32, sz.height * 4/32);
    [mProgressBackground lockFocus];
    [[NSColor whiteColor] set];
    NSRectFill(NSRectFromCGRect(mProgressBounds));
    [mProgressBackground unlockFocus];
  }
  return true;
}

nsresult
nsMacDockSupport::RedrawIcon()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (InitProgress()) {
    // TODO: - Implement ERROR and PAUSED states?
    NSImage *icon = [mProgressBackground copyWithZone:nil];
    bool isIndeterminate = (mProgressState != STATE_NORMAL);

    [icon lockFocus];
    CGContextRef ctx = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    mTheme->DrawProgress(ctx, mProgressBounds, isIndeterminate,
      true, mProgressFraction, 1.0, NULL);
    [icon unlockFocus];
    [NSApp setApplicationIconImage:icon];
    [icon release];
  } else {
    [NSApp setApplicationIconImage:mAppIcon];
  }

  return NS_OK;
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
