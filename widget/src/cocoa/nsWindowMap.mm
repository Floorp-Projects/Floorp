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
 *    Josh Aas <josh@mozilla.com>
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

#include "nsWindowMap.h"

#include "nsChildView.h"
#include "nsCocoaWindow.h"

@interface WindowDataMap(Private)

- (NSString*)keyForWindow:(NSWindow*)inWindow;

@end

@interface TopLevelWindowData(Private)

- (void)windowResignedKey:(NSNotification*)inNotification;
- (void)windowBecameKey:(NSNotification*)inNotification;
- (void)windowWillClose:(NSNotification*)inNotification;

@end

#pragma mark -

@implementation WindowDataMap

+ (WindowDataMap*)sharedWindowDataMap
{
  static WindowDataMap* sWindowMap = nil;
  if (!sWindowMap)
    sWindowMap = [[WindowDataMap alloc] init];

  return sWindowMap;
}

- (id)init
{
  if ((self = [super init])) {
    mWindowMap = [[NSMutableDictionary alloc] initWithCapacity:10];
  }
  return self;
}

- (void)dealloc
{
  [mWindowMap release];
  [super dealloc];
}

- (id)dataForWindow:(NSWindow*)inWindow
{
  return [mWindowMap objectForKey:[self keyForWindow:inWindow]];
}

- (void)setData:(id)inData forWindow:(NSWindow*)inWindow
{
  [mWindowMap setObject:inData forKey:[self keyForWindow:inWindow]];
}

- (void)removeDataForWindow:(NSWindow*)inWindow
{
  [mWindowMap removeObjectForKey:[self keyForWindow:inWindow]];
}

- (NSString*)keyForWindow:(NSWindow*)inWindow
{
  return [NSString stringWithFormat:@"%p", inWindow];
}

@end


//  TopLevelWindowData
// 
//  This class holds data about top-level windows. We can't use a window
//  delegate, because an embedder may already have one.

@implementation TopLevelWindowData

- (id)initWithWindow:(NSWindow*)inWindow
{
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
                                             selector:@selector(windowWillClose:)
                                                 name:NSWindowWillCloseNotification
                                               object:inWindow];
  }
  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)windowBecameKey:(NSNotification*)inNotification
{
  NSWindow* window = (NSWindow*)[inNotification object];
  id firstResponder = [window firstResponder];
  if ([firstResponder isKindOfClass:[ChildView class]]) {
    [firstResponder viewsWindowDidBecomeKey];
  }
  else {
    id delegate = [window delegate];
    if ([delegate respondsToSelector:@selector(sendFocusEvent:)]) {
      [delegate sendFocusEvent:NS_GOTFOCUS];
      [delegate sendFocusEvent:NS_ACTIVATE];
    }
  }
}

- (void)windowResignedKey:(NSNotification*)inNotification
{
  NSWindow* window = (NSWindow*)[inNotification object];
  id firstResponder = [window firstResponder];
  if ([firstResponder isKindOfClass:[ChildView class]]) {
    [firstResponder viewsWindowDidResignKey];
  }
  else {
    id delegate = [window delegate];
    if ([delegate respondsToSelector:@selector(sendFocusEvent:)]) {
      [delegate sendFocusEvent:NS_LOSTFOCUS];
      [delegate sendFocusEvent:NS_DEACTIVATE];
    }
  }
}

- (void)windowWillClose:(NSNotification*)inNotification
{
  // postpone our destruction
  [[self retain] autorelease];

  // remove ourselves from the window map (which owns us)
  [[WindowDataMap sharedWindowDataMap] removeDataForWindow:[inNotification object]];
}

@end
