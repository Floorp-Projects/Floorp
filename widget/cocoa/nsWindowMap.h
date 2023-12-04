/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowMap_h_
#define nsWindowMap_h_

#import <Cocoa/Cocoa.h>

//  WindowDataMap
//
//  In both mozilla and embedding apps, we need to have a place to put
//  per-top-level-window logic and data, to handle such things as IME
//  commit when the window gains/loses focus. We can't use a window
//  delegate, because an embeddor probably already has one. Nor can we
//  subclass NSWindow, again because we can't impose that burden on the
//  embeddor.
//
//  So we have a global map of NSWindow -> TopLevelWindowData, and set
//  up TopLevelWindowData as a notification observer etc.

@interface WindowDataMap : NSObject {
 @private
  NSMutableDictionary*
      mWindowMap;  // dict of TopLevelWindowData keyed by address of NSWindow
}

+ (WindowDataMap*)sharedWindowDataMap;

- (void)ensureDataForWindow:(NSWindow*)inWindow;
- (id)dataForWindow:(NSWindow*)inWindow;

// set data for a given window. inData is retained (and any previously set data
// is released).
- (void)setData:(id)inData forWindow:(NSWindow*)inWindow;

// remove the data for the given window. the data is released.
- (void)removeDataForWindow:(NSWindow*)inWindow;

@end

@class ChildView;

//  TopLevelWindowData
//
//  Class to hold per-window data, and handle window state changes.

@interface TopLevelWindowData : NSObject {
 @private
}

  - (id)initWithWindow:(NSWindow*)inWindow;
  + (void)activateInWindow:(NSWindow*)aWindow;
  + (void)deactivateInWindow:(NSWindow*)aWindow;
  + (void)activateInWindowViews:(NSWindow*)aWindow;
  + (void)deactivateInWindowViews:(NSWindow*)aWindow;

  @end

#endif  // nsWindowMap_h_
