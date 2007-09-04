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

// As best I can tell, if the notification's object has a corresponding
// top-level widget (an nsCocoaWindow object), it has a delegate (set in
// nsCocoaWindow::StandardCreate()) that responds to sendFocusEvent:, and
// otherwise not (Camino doesn't use top-level widgets (nsCocoaWindow
// objects) -- only child widgets (nsChildView objects)).
//
// If we're using top-level widgets, we need to send them both kinds of
// focus event (NS_GOTFOCUS and NS_ACTIVATE, which by convention are sent in
// that order) -- otherwise text input can (under unusual circumstances) stop
// working in the currently focused child widget (see bmo bug 354768).
//
// When we send focus events to a top-level widget, they get propagated
// (via nsWebShellWindow::HandleEvent(), indirectly) to a child widget (an
// nsChildView object) -- so in principle we shouldn't have to send them to
// child widgets here.  But I've found that, unless I also send at least an
// NS_GOTFOCUS event directly to the currently focused child widget, it's
// easy to get blinking I-bar cursors in multiple text input fields
// (particularly if one of them is the Google search box).  On other platforms
// (e.g. Windows and GTK2), NS_ACTIVATE events are only sent (directly) to
// top-level widgets -- so we do the same here.  Not sending them directly
// to child widgets also avoids "win is null" assertions on debug builds
// (see bug 354768 comments 55 and 58).
- (void)windowBecameKey:(NSNotification*)inNotification
{
  NSWindow* window = (NSWindow*)[inNotification object];
  id delegate = [window delegate];
  if (delegate && [delegate respondsToSelector:@selector(sendFocusEvent:)]) {
    [delegate sendFocusEvent:NS_GOTFOCUS];
    [delegate sendFocusEvent:NS_ACTIVATE];
    id firstResponder = [window firstResponder];
    if ([firstResponder isKindOfClass:[ChildView class]]) {
      BOOL isMozWindow = [window respondsToSelector:@selector(setSuppressMakeKeyFront:)];
      if (isMozWindow)
        [window setSuppressMakeKeyFront:YES];
      [firstResponder sendFocusEvent:NS_GOTFOCUS];
      if (isMozWindow)
        [window setSuppressMakeKeyFront:NO];
    }
  } else {
    id firstResponder = [window firstResponder];
    if ([firstResponder isKindOfClass:[ChildView class]])
      [firstResponder viewsWindowDidBecomeKey];
  }
}

// See comments above windowBecameKey:
//
// If we're using top-level widgets (nsCocoaWindow objects), we send them
// NS_DEACTIVATE events (which propagate to child widgets (nsChildView
// objects) via nsWebShellWindow::HandleEvent()).  Sending NS_LOSTFOCUS
// events to top-level widgets currently has no effect (nsWebShellWindow::
// HandleEvent(), which processes focus events sent to top-level widgets,
// doesn't have a section for NS_LOSTFOCUS).  But on general principles we
// send them anyway.
//
// On other platforms (e.g. Windows and GTK2), NS_DEACTIVATE events are only
// sent (directly) to top-level widgets.  And (as noted above) these events
// propagate to child widgets when they're sent to top-level widgets.  But if
// we don't send them again, blinking I-bar cursors can appear in multiple
// text input fields.  Since we also need to send NS_LOSTFOCUS events and
// call nsTSMManager::CommitIME(), we just always call through to ChildView
// viewsWindowDidResignKey (whether or not we're using top-level widgets).
- (void)windowResignedKey:(NSNotification*)inNotification
{
  NSWindow* window = (NSWindow*)[inNotification object];
  id delegate = [window delegate];
  if (delegate && [delegate respondsToSelector:@selector(sendFocusEvent:)]) {
    [delegate sendFocusEvent:NS_DEACTIVATE];
    [delegate sendFocusEvent:NS_LOSTFOCUS];
  }
  id firstResponder = [window firstResponder];
  if ([firstResponder isKindOfClass:[ChildView class]])
    [firstResponder viewsWindowDidResignKey];
}

- (void)windowWillClose:(NSNotification*)inNotification
{
  // postpone our destruction
  [[self retain] autorelease];

  // remove ourselves from the window map (which owns us)
  [[WindowDataMap sharedWindowDataMap] removeDataForWindow:[inNotification object]];
}

@end
