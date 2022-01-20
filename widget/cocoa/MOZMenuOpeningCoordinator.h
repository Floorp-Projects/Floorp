/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZMenuOpeningCoordinator_h
#define MOZMenuOpeningCoordinator_h

#import <Cocoa/Cocoa.h>

#include "mozilla/RefPtr.h"

namespace mozilla {
class Runnable;
}

/*
 * MOZMenuOpeningCoordinator is a workaround for the fact that opening an NSMenu creates a nested
 * event loop. This event loop is only exited after the menu is closed. The caller of
 * NativeMenuMac::ShowAsContextMenu does not expect ShowAsContextMenu to create a nested event loop,
 * so we need to make sure to open the NSMenu asynchronously.
 */

@interface MOZMenuOpeningCoordinator : NSObject

+ (instancetype)sharedInstance;

// Queue aMenu for opening.
// The menu will open from a new event loop tick so that its nested event loop does not block the
// caller. If another menu's nested event loop is currently on the stack, we wait for that nested
// event loop to unwind before opening aMenu. Returns a handle that can be passed to
// cancelAsynchronousOpening:. Can only be called on the main thread.
- (NSInteger)asynchronouslyOpenMenu:(NSMenu*)aMenu
                   atScreenPosition:(NSPoint)aPosition
                            forView:(NSView*)aView;

// If the menu opening request for aHandle hasn't been processed yet, cancel it.
// Can only be called on the main thread.
- (void)cancelAsynchronousOpening:(NSInteger)aHandle;

// Run aRunnable once the nested event loop of the currently open menu has been exited.
// If no menu is currently open, post the runnable with NS_DispatchToCurrentThread.
- (void)runAfterMenuClosed:(RefPtr<mozilla::Runnable>&&)aRunnable;

// This field is a terrible workaround for a gnarly problem.
// It should be set to YES by the caller of -[NSMenu cancelTracking(WithoutAnimation)].
// This field gets checked by the native event loop code in nsAppShell.mm to avoid calling
// -[NSApplication nextEventMatchingMask:...] between the call to cancelTracking and the point at
// which the menu has finished closing and unwound from its tracking event loop, because such calls
// can interfere with menu closing and get us stuck in the menu event loop forever.
@property(class) BOOL needToUnwindForMenuClosing;

@end

#endif  // MOZMenuOpeningCoordinator_h
