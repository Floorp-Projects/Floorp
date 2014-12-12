/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozView_h_
#define mozView_h_

#undef DARWIN
#import <Cocoa/Cocoa.h>
class nsIWidget;

namespace mozilla {
namespace widget{
class TextInputHandler;
} // namespace widget
} // namespace mozilla

// A protocol listing all the methods that an object which wants
// to live in gecko's widget hierarchy must implement. |nsChildView|
// makes assumptions that any NSView with which it comes in contact will
// implement this protocol.
@protocol mozView

  // aHandler is Gecko's default text input handler:  It implements the
  // NSTextInput protocol to handle key events.  Don't make aHandler a
  // strong reference -- that causes a memory leak.
- (void)installTextInputHandler:(mozilla::widget::TextInputHandler*)aHandler;
- (void)uninstallTextInputHandler;

  // access the nsIWidget associated with this view. DOES NOT ADDREF.
- (nsIWidget*)widget;

  // return a context menu for this view
- (NSMenu*)contextMenu;

  // Allows callers to do a delayed invalidate (e.g., if an invalidate
  // happens during drawing)
- (void)setNeedsPendingDisplay;
- (void)setNeedsPendingDisplayInRect:(NSRect)invalidRect;

  // called when our corresponding Gecko view goes away
- (void)widgetDestroyed;

- (BOOL)isDragInProgress;

  // Checks whether the view is first responder or not
- (BOOL)isFirstResponder;

  // Call when you dispatch an event which may cause to open context menu.
- (void)maybeInitContextMenuTracking;

@end

// An informal protocol implemented by the NSWindow of the host application.
// 
// It's used to prevent re-entrant calls to -makeKeyAndOrderFront: when gecko
// focus/activate events propagate out to the embedder's
// nsIEmbeddingSiteWindow::SetFocus implementation.
@interface NSObject(mozWindow)

- (BOOL)suppressMakeKeyFront;
- (void)setSuppressMakeKeyFront:(BOOL)inSuppress;

@end

#endif // mozView_h_
