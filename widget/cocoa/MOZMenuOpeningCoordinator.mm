/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Makes sure that the nested event loop for NSMenu tracking is situated as low
 * on the stack as possible, and that two NSMenu event loops are never nested.
 */

#include "MOZMenuOpeningCoordinator.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPrefs_widget.h"

#include "nsCocoaFeatures.h"
#include "nsCocoaUtils.h"
#include "nsDeque.h"
#include "nsMenuX.h"
#include "nsObjCExceptions.h"
#include "nsThreadUtils.h"
#include "SDKDeclarations.h"

static BOOL sNeedToUnwindForMenuClosing = NO;

@interface MOZMenuOpeningInfo : NSObject
@property NSInteger handle;
@property(retain) NSMenu* menu;
@property NSPoint position;
@property(retain) NSView* view;
@end

@implementation MOZMenuOpeningInfo
@end

@implementation MOZMenuOpeningCoordinator {
  // non-nil between asynchronouslyOpenMenu:atScreenPosition:forView: and the
  // time at at which it is unqueued in _runMenu.
  MOZMenuOpeningInfo* mPendingOpening;  // strong

  // Any runnables we want to run after the current menu event loop has been exited.
  // Only non-empty if mRunMenuIsOnTheStack is true.
  nsRefPtrDeque<mozilla::Runnable> mPendingAfterMenuCloseRunnables;

  // An incrementing counter
  NSInteger mLastHandle;

  // YES while _runMenu is on the stack
  BOOL mRunMenuIsOnTheStack;
}

+ (instancetype)sharedInstance {
  static MOZMenuOpeningCoordinator* sInstance = nil;
  if (!sInstance) {
    sInstance = [[MOZMenuOpeningCoordinator alloc] init];
    mozilla::RunOnShutdown([&]() {
      [sInstance release];
      sInstance = nil;
    });
  }
  return sInstance;
}

- (void)dealloc {
  MOZ_RELEASE_ASSERT(!mPendingOpening, "should be empty at shutdown");
  MOZ_RELEASE_ASSERT(mPendingAfterMenuCloseRunnables.GetSize() == 0, "should be empty at shutdown");
  [super dealloc];
}

- (NSInteger)asynchronouslyOpenMenu:(NSMenu*)aMenu
                   atScreenPosition:(NSPoint)aPosition
                            forView:(NSView*)aView {
  MOZ_RELEASE_ASSERT(!mPendingOpening,
                     "A menu is already waiting to open. Before opening the next one, either wait "
                     "for this one to open or cancel the request.");

  NSInteger handle = ++mLastHandle;

  MOZMenuOpeningInfo* info = [[MOZMenuOpeningInfo alloc] init];
  info.handle = handle;
  info.menu = aMenu;
  info.position = aPosition;
  info.view = aView;
  mPendingOpening = [info retain];
  [info release];

  if (!mRunMenuIsOnTheStack) {
    // Call _runMenu from the event loop, so that it doesn't block this call.
    [self performSelector:@selector(_runMenu) withObject:nil afterDelay:0.0];
  }

  return handle;
}

- (void)_runMenu {
  MOZ_RELEASE_ASSERT(!mRunMenuIsOnTheStack);

  mRunMenuIsOnTheStack = YES;

  while (mPendingOpening) {
    MOZMenuOpeningInfo* info = [mPendingOpening retain];
    [mPendingOpening release];
    mPendingOpening = nil;

    @try {
      [self _openMenu:info.menu atScreenPosition:info.position forView:info.view];
    } @catch (NSException* exception) {
      nsObjCExceptionLog(exception);
    }

    [info release];

    // We have exited _openMenu's nested event loop.
    MOZMenuOpeningCoordinator.needToUnwindForMenuClosing = NO;

    // Dispatch any pending "after menu close" runnables to the event loop.
    while (mPendingAfterMenuCloseRunnables.GetSize() != 0) {
      NS_DispatchToCurrentThread(mPendingAfterMenuCloseRunnables.PopFront());
    }
  }

  mRunMenuIsOnTheStack = NO;
}

- (void)cancelAsynchronousOpening:(NSInteger)aHandle {
  if (mPendingOpening && mPendingOpening.handle == aHandle) {
    [mPendingOpening release];
    mPendingOpening = nil;
  }
}

- (void)runAfterMenuClosed:(RefPtr<mozilla::Runnable>&&)aRunnable {
  MOZ_RELEASE_ASSERT(aRunnable);

  if (mRunMenuIsOnTheStack) {
    mPendingAfterMenuCloseRunnables.Push(aRunnable.forget());
  } else {
    NS_DispatchToCurrentThread(aRunnable.forget());
  }
}

- (void)_openMenu:(NSMenu*)aMenu atScreenPosition:(NSPoint)aPosition forView:(NSView*)aView {
  // There are multiple ways to display an NSMenu as a context menu.
  //
  //  1. We can return the NSMenu from -[ChildView menuForEvent:] and the NSView will open it for
  //     us.
  //  2. We can call +[NSMenu popUpContextMenu:withEvent:forView:] inside a mouseDown handler with a
  //     real mouse down event.
  //  3. We can call +[NSMenu popUpContextMenu:withEvent:forView:] at a later time, with a real
  //     mouse event that we stored earlier.
  //  4. We can call +[NSMenu popUpContextMenu:withEvent:forView:] at any time, with a synthetic
  //     mouse event that we create just for that purpose.
  //  5. We can call -[NSMenu popUpMenuPositioningItem:atLocation:inView:] and it just takes a
  //     position, not an event.
  //
  // 1-4 look the same, 5 looks different: 5 is made for use with NSPopUpButton, where the selected
  // item needs to be shown at a specific position. If a tall menu is opened with a position close
  // to the bottom edge of the screen, 5 results in a cropped menu with scroll arrows, even if the
  // entire menu would fit on the screen, due to the positioning constraint.
  // 1-2 only work if the menu contents are known synchronously during the call to menuForEvent or
  // during the mouseDown event handler.
  // NativeMenuMac::ShowAsContextMenu can be called at any time. It could be called during a
  // menuForEvent call (during a "contextmenu" event handler), or during a mouseDown handler, or at
  // a later time.
  // The code below uses option 4 as the preferred option because it's the simplest: It works in all
  // scenarios and it doesn't have the positioning drawbacks of option 5.

  if (aView) {
    NSWindow* window = aView.window;

    if (@available(macOS 10.14, *)) {
      if (window.effectiveAppearance != NSApp.effectiveAppearance) {
        // By default, NSMenu inherits its appearance from the opening NSEvent's window. But we
        // would like it to use the system appearance - if the system uses Dark Mode, we would like
        // context menus to be dark even if the window's appearance is Light.
#if !defined(MAC_OS_VERSION_11_0) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_11_0
        if (nsCocoaFeatures::OnBigSurOrLater()) {
#else
        if (@available(macOS 11.0, *)) {
#endif
          // On macOS Big Sur, we can achieve this by using -[NSMenu setAppearance:].
          [aMenu setAppearance:NSApp.effectiveAppearance];
        } else if (mozilla::StaticPrefs::
                       widget_macos_enable_pre_bigsur_workaround_for_dark_mode_context_menus()) {
          // On 10.14 and 10.15, there is no API to override the NSMenu appearance.
          // We use the following hack: We change the NSWindow appearance just long enough that
          // NSMenu opening picks it up, and then reset the NSWindow appearance to its old value.
          // Resetting it in the menu delegate's menuWillOpen method seems to achieve this without
          // any flashing effect.
          if ([aMenu.delegate isKindOfClass:[MenuDelegate class]]) {
            MenuDelegate* delegate = (MenuDelegate*)aMenu.delegate;

            // Store the old NSWindow appearance, override it with the system appearance, and then
            // reset it when the menu is open.
            NSAppearance* oldAppearance = window.appearance;
            window.appearance = NSApp.effectiveAppearance;
            [(MenuDelegate*)delegate runBlockWhenOpen:^() {
              window.appearance = oldAppearance;
            }];
          }
        }
      }
    }

    // Create a synthetic event at the right location and open the menu [option 4].
    NSPoint locationInWindow = nsCocoaUtils::ConvertPointFromScreen(window, aPosition);
    NSEvent* event = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                                        location:locationInWindow
                                   modifierFlags:0
                                       timestamp:NSProcessInfo.processInfo.systemUptime
                                    windowNumber:window.windowNumber
                                         context:nil
                                     eventNumber:0
                                      clickCount:1
                                        pressure:0.0f];
    [NSMenu popUpContextMenu:aMenu withEvent:event forView:aView];
  } else {
    // Open the menu using popUpMenuPositioningItem:atLocation:inView: [option 5].
    // This is not preferred, because it positions the menu differently from how a native context
    // menu would be positioned; it enforces aPosition for the top left corner even if this
    // means that the menu will be displayed in a clipped fashion with scroll arrows.
    [aMenu popUpMenuPositioningItem:nil atLocation:aPosition inView:nil];
  }
}

+ (void)setNeedToUnwindForMenuClosing:(BOOL)aValue {
  sNeedToUnwindForMenuClosing = aValue;
}

+ (BOOL)needToUnwindForMenuClosing {
  return sNeedToUnwindForMenuClosing;
}

@end
