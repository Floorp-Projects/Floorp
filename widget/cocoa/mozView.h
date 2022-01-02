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
namespace widget {
class TextInputHandler;
}  // namespace widget
}  // namespace mozilla

// A protocol with some of the methods that ChildView implements. In the distant
// past, this protocol was used by embedders: They would create their own NSView
// subclass, implement mozView on it, and then embed a Gecko ChildView by adding
// it as a subview of this view. This scenario no longer exists.
// Now this protocol is mostly just used by TextInputHandler and mozAccessible
// in order to communicate with ChildView without seeing the entire ChildView
// interface definition.
@protocol mozView

// aHandler is Gecko's default text input handler:  It implements the
// NSTextInput protocol to handle key events.  Don't make aHandler a
// strong reference -- that causes a memory leak.
- (void)installTextInputHandler:(mozilla::widget::TextInputHandler*)aHandler;
- (void)uninstallTextInputHandler;

// access the nsIWidget associated with this view. DOES NOT ADDREF.
- (nsIWidget*)widget;

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
@interface NSObject (mozWindow)

- (BOOL)suppressMakeKeyFront;
- (void)setSuppressMakeKeyFront:(BOOL)inSuppress;

@end

#endif  // mozView_h_
