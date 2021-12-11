/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowMap.h"
#include "nsObjCExceptions.h"
#include "nsChildView.h"
#include "nsCocoaWindow.h"

@interface WindowDataMap (Private)

- (NSString*)keyForWindow:(NSWindow*)inWindow;

@end

@interface TopLevelWindowData (Private)

- (void)windowResignedKey:(NSNotification*)inNotification;
- (void)windowBecameKey:(NSNotification*)inNotification;
- (void)windowWillClose:(NSNotification*)inNotification;

@end

#pragma mark -

@implementation WindowDataMap

+ (WindowDataMap*)sharedWindowDataMap {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  static WindowDataMap* sWindowMap = nil;
  if (!sWindowMap) sWindowMap = [[WindowDataMap alloc] init];

  return sWindowMap;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (id)init {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ((self = [super init])) {
    mWindowMap = [[NSMutableDictionary alloc] initWithCapacity:10];
  }
  return self;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mWindowMap release];
  [super dealloc];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)ensureDataForWindow:(NSWindow*)inWindow {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!inWindow || [self dataForWindow:inWindow]) return;

  TopLevelWindowData* windowData = [[TopLevelWindowData alloc] initWithWindow:inWindow];
  [self setData:windowData forWindow:inWindow];  // takes ownership
  [windowData release];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (id)dataForWindow:(NSWindow*)inWindow {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return [mWindowMap objectForKey:[self keyForWindow:inWindow]];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)setData:(id)inData forWindow:(NSWindow*)inWindow {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mWindowMap setObject:inData forKey:[self keyForWindow:inWindow]];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)removeDataForWindow:(NSWindow*)inWindow {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mWindowMap removeObjectForKey:[self keyForWindow:inWindow]];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (NSString*)keyForWindow:(NSWindow*)inWindow {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return [NSString stringWithFormat:@"%p", inWindow];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

@end

//  TopLevelWindowData
//
//  This class holds data about top-level windows. We can't use a window
//  delegate, because an embedder may already have one.

@implementation TopLevelWindowData

- (id)initWithWindow:(NSWindow*)inWindow {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ((self = [super init])) {
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowBecameKey:)
                                                 name:NSWindowDidBecomeKeyNotification
                                               object:inWindow];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowResignedKey:)
                                                 name:NSWindowDidResignKeyNotification
                                               object:inWindow];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowBecameMain:)
                                                 name:NSWindowDidBecomeMainNotification
                                               object:inWindow];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowResignedMain:)
                                                 name:NSWindowDidResignMainNotification
                                               object:inWindow];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowWillClose:)
                                                 name:NSWindowWillCloseNotification
                                               object:inWindow];
  }
  return self;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// As best I can tell, if the notification's object has a corresponding
// top-level widget (an nsCocoaWindow object), it has a delegate (set in
// nsCocoaWindow::StandardCreate()) of class WindowDelegate, and otherwise
// not (Camino didn't use top-level widgets (nsCocoaWindow objects) --
// only child widgets (nsChildView objects)).  (The notification is sent
// to windowBecameKey: or windowBecameMain: below.)
//
// For use with clients that (like Firefox) do use top-level widgets (and
// have NSWindow delegates of class WindowDelegate).
+ (void)activateInWindow:(NSWindow*)aWindow {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  WindowDelegate* delegate = (WindowDelegate*)[aWindow delegate];
  if (!delegate || ![delegate isKindOfClass:[WindowDelegate class]]) return;

  if ([delegate toplevelActiveState]) return;
  [delegate sendToplevelActivateEvents];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// See comments above activateInWindow:
//
// If we're using top-level widgets (nsCocoaWindow objects), we send them
// NS_DEACTIVATE events (which propagate to child widgets (nsChildView
// objects) via nsWebShellWindow::HandleEvent()).
//
// For use with clients that (like Firefox) do use top-level widgets (and
// have NSWindow delegates of class WindowDelegate).
+ (void)deactivateInWindow:(NSWindow*)aWindow {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  WindowDelegate* delegate = (WindowDelegate*)[aWindow delegate];
  if (!delegate || ![delegate isKindOfClass:[WindowDelegate class]]) return;

  if (![delegate toplevelActiveState]) return;
  [delegate sendToplevelDeactivateEvents];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// For use with clients that (like Camino) don't use top-level widgets (and
// don't have NSWindow delegates of class WindowDelegate).
+ (void)activateInWindowViews:(NSWindow*)aWindow {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  id firstResponder = [aWindow firstResponder];
  if ([firstResponder isKindOfClass:[ChildView class]]) [firstResponder viewsWindowDidBecomeKey];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// For use with clients that (like Camino) don't use top-level widgets (and
// don't have NSWindow delegates of class WindowDelegate).
+ (void)deactivateInWindowViews:(NSWindow*)aWindow {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  id firstResponder = [aWindow firstResponder];
  if ([firstResponder isKindOfClass:[ChildView class]]) [firstResponder viewsWindowDidResignKey];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// We make certain exceptions for top-level windows in non-embedders (see
// comment above windowBecameMain below).  And we need (elsewhere) to guard
// against sending duplicate events.  But in general the NS_ACTIVATE event
// should be sent when a native window becomes key, and the NS_DEACTIVATE
// event should be sent when it resignes key.
- (void)windowBecameKey:(NSNotification*)inNotification {
  NSWindow* window = (NSWindow*)[inNotification object];

  id delegate = [window delegate];
  if (!delegate || ![delegate isKindOfClass:[WindowDelegate class]]) {
    [TopLevelWindowData activateInWindowViews:window];
  } else if ([window isSheet]) {
    [TopLevelWindowData activateInWindow:window];
  }
}

- (void)windowResignedKey:(NSNotification*)inNotification {
  NSWindow* window = (NSWindow*)[inNotification object];

  id delegate = [window delegate];
  if (!delegate || ![delegate isKindOfClass:[WindowDelegate class]]) {
    [TopLevelWindowData deactivateInWindowViews:window];
  } else if ([window isSheet]) {
    [TopLevelWindowData deactivateInWindow:window];
  }
}

// The appearance of a top-level window depends on its main state (not its key
// state).  So (for non-embedders) we need to ensure that a top-level window
// is main when an NS_ACTIVATE event is sent to Gecko for it.
- (void)windowBecameMain:(NSNotification*)inNotification {
  NSWindow* window = (NSWindow*)[inNotification object];

  id delegate = [window delegate];
  // Don't send events to a top-level window that has a sheet open above it --
  // as far as Gecko is concerned, it's inactive, and stays so until the sheet
  // closes.
  if (delegate && [delegate isKindOfClass:[WindowDelegate class]] && ![window attachedSheet])
    [TopLevelWindowData activateInWindow:window];
}

- (void)windowResignedMain:(NSNotification*)inNotification {
  NSWindow* window = (NSWindow*)[inNotification object];

  id delegate = [window delegate];
  if (delegate && [delegate isKindOfClass:[WindowDelegate class]] && ![window attachedSheet])
    [TopLevelWindowData deactivateInWindow:window];
}

- (void)windowWillClose:(NSNotification*)inNotification {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // postpone our destruction
  [[self retain] autorelease];

  // remove ourselves from the window map (which owns us)
  [[WindowDataMap sharedWindowDataMap] removeDataForWindow:[inNotification object]];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

@end
