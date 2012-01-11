/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Simon Fraser <smfr@smfr.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

@interface WindowDataMap : NSObject
{
@private
  NSMutableDictionary*    mWindowMap;   // dict of TopLevelWindowData keyed by address of NSWindow
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

@interface TopLevelWindowData : NSObject
{
@private
}

- (id)initWithWindow:(NSWindow*)inWindow;
+ (void)activateInWindow:(NSWindow*)aWindow;
+ (void)deactivateInWindow:(NSWindow*)aWindow;
+ (void)activateInWindowViews:(NSWindow*)aWindow;
+ (void)deactivateInWindowViews:(NSWindow*)aWindow;

@end

#endif // nsWindowMap_h_
