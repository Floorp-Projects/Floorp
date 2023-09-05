/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <signal.h>

#include "nsCocoaUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsMacDockSupport.h"
#include "nsObjCExceptions.h"
#include "nsNativeThemeColors.h"
#include "nsString.h"

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
  // Erase the background behind this view, i.e. cut a rectangle hole in the
  // icon.
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
  [[NSColor controlAccentColor] setFill];
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
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  [[NSApplication sharedApplication]
      activateIgnoringOtherApps:aIgnoreOtherApplications];
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsMacDockSupport::SetBadgeText(const nsAString& aBadgeText) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSDockTile* tile = [[NSApplication sharedApplication] dockTile];
  mBadgeText = aBadgeText;
  if (aBadgeText.IsEmpty())
    [tile setBadgeLabel:nil];
  else
    [tile
        setBadgeLabel:[NSString
                          stringWithCharacters:reinterpret_cast<const unichar*>(
                                                   mBadgeText.get())
                                        length:mBadgeText.Length()]];
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsMacDockSupport::GetBadgeText(nsAString& aBadgeText) {
  aBadgeText = mBadgeText;
  return NS_OK;
}

NS_IMETHODIMP
nsMacDockSupport::SetProgressState(nsTaskbarProgressState aState,
                                   uint64_t aCurrentValue, uint64_t aMaxValue) {
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
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (mProgressState == STATE_NORMAL || mProgressState == STATE_INDETERMINATE) {
    if (!mDockTileWrapperView) {
      // Create the following NSView hierarchy:
      // * mDockTileWrapperView (NSView)
      //    * imageView (NSImageView) <- has the application icon
      //    * mProgressDockOverlayView (MOZProgressDockOverlayView) <- draws the
      //    progress bar

      mDockTileWrapperView =
          [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 32, 32)];
      mDockTileWrapperView.autoresizingMask =
          NSViewWidthSizable | NSViewHeightSizable;

      NSImageView* imageView =
          [[NSImageView alloc] initWithFrame:[mDockTileWrapperView bounds]];
      imageView.image = [NSImage imageNamed:@"NSApplicationIcon"];
      imageView.imageScaling = NSImageScaleAxesIndependently;
      imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
      [mDockTileWrapperView addSubview:imageView];

      mProgressDockOverlayView = [[MOZProgressDockOverlayView alloc]
          initWithFrame:NSMakeRect(1, 3, 30, 4)];
      mProgressDockOverlayView.autoresizingMask =
          NSViewMinXMargin | NSViewWidthSizable | NSViewMaxXMargin |
          NSViewMinYMargin | NSViewHeightSizable | NSViewMaxYMargin;
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

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

extern "C" {
// Private CFURL API used by the Dock.
CFPropertyListRef _CFURLCopyPropertyListRepresentation(CFURLRef url);
CFURLRef _CFURLCreateFromPropertyListRepresentation(
    CFAllocatorRef alloc, CFPropertyListRef pListRepresentation);
}  // extern "C"

namespace {

const NSArray* const browserAppNames = [NSArray
    arrayWithObjects:@"Firefox.app", @"Firefox Beta.app",
                     @"Firefox Nightly.app", @"Safari.app", @"WebKit.app",
                     @"Google Chrome.app", @"Google Chrome Canary.app",
                     @"Chromium.app", @"Opera.app", nil];

constexpr NSString* const kDockDomainName = @"com.apple.dock";
// See https://developer.apple.com/documentation/devicemanagement/dock
constexpr NSString* const kDockPersistentAppsKey = @"persistent-apps";
// See
// https://developer.apple.com/documentation/devicemanagement/dock/staticitem
constexpr NSString* const kDockTileDataKey = @"tile-data";
constexpr NSString* const kDockFileDataKey = @"file-data";

NSArray* GetPersistentAppsFromDockPlist(NSDictionary* aDockPlist) {
  if (!aDockPlist) {
    return nil;
  }
  NSArray* persistentApps = [aDockPlist objectForKey:kDockPersistentAppsKey];
  if (![persistentApps isKindOfClass:[NSArray class]]) {
    return nil;
  }
  return persistentApps;
}

NSString* GetPathForApp(NSDictionary* aPersistantApp) {
  if (![aPersistantApp isKindOfClass:[NSDictionary class]]) {
    return nil;
  }
  NSDictionary* tileData = aPersistantApp[kDockTileDataKey];
  if (![tileData isKindOfClass:[NSDictionary class]]) {
    return nil;
  }
  NSDictionary* fileData = tileData[kDockFileDataKey];
  if (![fileData isKindOfClass:[NSDictionary class]]) {
    // Some special tiles may not have DockFileData but we can ignore those.
    return nil;
  }
  NSURL* url = CFBridgingRelease(
      _CFURLCreateFromPropertyListRepresentation(NULL, fileData));
  if (!url) {
    return nil;
  }
  return [url isFileURL] ? [url path] : nullptr;
}

// The only reliable way to get our changes to take effect seems to be to use
// `kill`.
void RefreshDock(NSDictionary* aDockPlist) {
  [[NSUserDefaults standardUserDefaults] setPersistentDomain:aDockPlist
                                                     forName:kDockDomainName];
  NSRunningApplication* dockApp = [[NSRunningApplication
      runningApplicationsWithBundleIdentifier:@"com.apple.dock"] firstObject];
  if (!dockApp) {
    return;
  }
  pid_t pid = [dockApp processIdentifier];
  if (pid > 0) {
    kill(pid, SIGTERM);
  }
}

}  // namespace

nsresult nsMacDockSupport::GetIsAppInDock(bool* aIsInDock) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  *aIsInDock = false;

  NSDictionary* dockPlist = [[NSUserDefaults standardUserDefaults]
      persistentDomainForName:kDockDomainName];
  if (!dockPlist) {
    return NS_ERROR_FAILURE;
  }

  NSArray* persistentApps = GetPersistentAppsFromDockPlist(dockPlist);
  if (!persistentApps) {
    return NS_ERROR_FAILURE;
  }

  NSString* appPath = [[NSBundle mainBundle] bundlePath];

  for (id app in persistentApps) {
    NSString* persistentAppPath = GetPathForApp(app);
    if (persistentAppPath && [appPath isEqual:persistentAppPath]) {
      *aIsInDock = true;
      break;
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsMacDockSupport::EnsureAppIsPinnedToDock(
    const nsAString& aAppPath, const nsAString& aAppToReplacePath,
    bool* aIsInDock) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  MOZ_ASSERT(aAppPath != aAppToReplacePath || !aAppPath.IsEmpty());

  *aIsInDock = false;

  NSString* appPath = !aAppPath.IsEmpty() ? nsCocoaUtils::ToNSString(aAppPath)
                                          : [[NSBundle mainBundle] bundlePath];
  NSString* appToReplacePath = nsCocoaUtils::ToNSString(aAppToReplacePath);

  NSMutableDictionary* dockPlist = [NSMutableDictionary
      dictionaryWithDictionary:[[NSUserDefaults standardUserDefaults]
                                   persistentDomainForName:kDockDomainName]];
  if (!dockPlist) {
    return NS_ERROR_FAILURE;
  }

  NSMutableArray* persistentApps =
      [NSMutableArray arrayWithArray:GetPersistentAppsFromDockPlist(dockPlist)];
  if (!persistentApps) {
    return NS_ERROR_FAILURE;
  }

  // See the comment for this method in the .idl file for the strategy that we
  // use here to determine where to pin the app.
  NSUInteger preexistingAppIndex = NSNotFound;  // full path matches
  NSUInteger sameNameAppIndex = NSNotFound;     // app name matches only
  NSUInteger toReplaceAppIndex = NSNotFound;
  NSUInteger lastBrowserAppIndex = NSNotFound;
  for (NSUInteger index = 0; index < [persistentApps count]; ++index) {
    NSString* persistentAppPath =
        GetPathForApp([persistentApps objectAtIndex:index]);

    if ([persistentAppPath isEqualToString:appPath]) {
      preexistingAppIndex = index;
    } else if (appToReplacePath &&
               [persistentAppPath isEqualToString:appToReplacePath]) {
      toReplaceAppIndex = index;
    } else {
      NSString* appName = [appPath lastPathComponent];
      NSString* persistentAppName = [persistentAppPath lastPathComponent];

      if ([persistentAppName isEqual:appName]) {
        if ([appToReplacePath hasPrefix:@"/private/var/folders/"] &&
            [appToReplacePath containsString:@"/AppTranslocation/"] &&
            [persistentAppPath hasPrefix:@"/Volumes/"]) {
          // This is a special case when an app with the same name was
          // previously dragged and pinned from a quarantined DMG straight to
          // the Dock and an attempt is now made to pin the same named app to
          // the Dock. In this case we want to replace the currently pinned app
          // icon.
          toReplaceAppIndex = index;
        } else {
          sameNameAppIndex = index;
        }
      } else {
        if ([browserAppNames containsObject:persistentAppName]) {
          lastBrowserAppIndex = index;
        }
      }
    }
  }

  // Special cases where we're not going to add a new Dock tile:
  if (preexistingAppIndex != NSNotFound) {
    if (toReplaceAppIndex != NSNotFound) {
      [persistentApps removeObjectAtIndex:toReplaceAppIndex];
      [dockPlist setObject:persistentApps forKey:kDockPersistentAppsKey];
      RefreshDock(dockPlist);
    }
    *aIsInDock = true;
    return NS_OK;
  }

  // Create new tile:
  NSDictionary* newDockTile = nullptr;
  {
    NSURL* appUrl = [NSURL fileURLWithPath:appPath isDirectory:YES];
    NSDictionary* dict = CFBridgingRelease(
        _CFURLCopyPropertyListRepresentation((__bridge CFURLRef)appUrl));
    if (!dict) {
      return NS_ERROR_FAILURE;
    }
    NSDictionary* dockTileData =
        [NSDictionary dictionaryWithObject:dict forKey:kDockFileDataKey];
    if (dockTileData) {
      newDockTile = [NSDictionary dictionaryWithObject:dockTileData
                                                forKey:kDockTileDataKey];
    }
    if (!newDockTile) {
      return NS_ERROR_FAILURE;
    }
  }

  // Update the Dock:
  if (toReplaceAppIndex != NSNotFound) {
    [persistentApps replaceObjectAtIndex:toReplaceAppIndex
                              withObject:newDockTile];
  } else {
    NSUInteger index;
    if (sameNameAppIndex != NSNotFound) {
      index = sameNameAppIndex + 1;
    } else if (lastBrowserAppIndex != NSNotFound) {
      index = lastBrowserAppIndex + 1;
    } else {
      index = [persistentApps count];
    }
    [persistentApps insertObject:newDockTile atIndex:index];
  }
  [dockPlist setObject:persistentApps forKey:kDockPersistentAppsKey];
  RefreshDock(dockPlist);

  *aIsInDock = true;
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}
